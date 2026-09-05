#include "mainwindow.h"
#include "audionormalizer.h"
#include "./ui_mainwindow.h"
#include <QAudioOutput>
#include <QMediaPlayer>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QRandomGenerator>
#include <QSettings>
#include <QMessageBox>
#include <random>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), player(new QMediaPlayer(this))
{
    ui->setupUi(this);
    player->setAudioOutput(new QAudioOutput(this));
    normalizer = new AudioNormalizer(this);
    connect(normalizer, &AudioNormalizer::progress, this, [this](int done, int total) {
        ui->statusbar->showMessage(QString("Analyzing volume: %1/%2 clips").arg(done).arg(total));
    });
    connect(normalizer, &AudioNormalizer::prepared, this, [this] {
        if (phase != Phase::Preparing) return;
        folderReady = true;
        if (!paused) {
            phase = Phase::Delay;
            clock.restart();
            tick();
        }
    });
    connect(normalizer, &AudioNormalizer::completed, this, [this](float gain, bool success) {
        if (phase != Phase::Analyzing) return;
        clipGain = gain;
        analysisReady = true;
        if (!success) ui->outputDisplay->appendPlainText("Volume analysis unavailable; playing at original level.");
        if (!paused) startClip();
    });
    connect(ui->volumeSlider, &QSlider::valueChanged, this, [this](int value) {
        applyVolume();
        ui->volumeLabel->setText("Volume: " + QString::number(value) + "%");
    });
    ui->outputDisplay->setReadOnly(true);
    connect(ui->premiumButton, &QPushButton::clicked, this, [this] {
        if (auto *existing = findChild<QMessageBox*>("premiumMessage")) {
            existing->raise();
            existing->activateWindow();
            return;
        }
        auto *message = new QMessageBox(QMessageBox::Information, "You're already Premium!",
            "Congratulations! You already have every feature. Please enjoy being premium for free.",
            QMessageBox::Ok, this);
        message->setObjectName("premiumMessage");
        message->setAttribute(Qt::WA_DeleteOnClose);
        message->setModal(false);
        message->show();
    });
    connect(ui->selectFolder, &QPushButton::clicked, this, &MainWindow::selectFolder);
    connect(ui->playButton, &QPushButton::clicked, this, &MainWindow::play);
    connect(ui->pauseButton, &QPushButton::clicked, this, &MainWindow::pause);
    connect(ui->resumeButton, &QPushButton::clicked, this, &MainWindow::resume);
    connect(ui->stopButton, &QPushButton::clicked, this, [this] {
        if (phase != Phase::Idle) finishSession("Stopped.");
    });
    timer.setInterval(50);
    timer.setTimerType(Qt::PreciseTimer);
    connect(&timer, &QTimer::timeout, this, &MainWindow::tick);
    connect(player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia) finishClip();
    });
    connect(player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString &) {
        if (phase != Phase::Clip) return;
        // Backend error text may expose a filename in Spoiler mode.
        ui->outputDisplay->appendPlainText("Failed to play: " +
            (ui->hideCheck->isChecked() ? QString("[Hidden]") : player->source().fileName()));
        finishClip(true);
    });
    readSettings();
    updateControls();
}
MainWindow::~MainWindow()
{
    writeSettings();
    phase = Phase::Idle;
    timer.stop();
    normalizer->cancel();
    player->disconnect(this);
    player->stop();
    delete ui;
}
QStringList MainWindow::discoverFiles() const
{
    const QStringList extensions = {"wav", "mp3", "flac", "ogg", "oga", "opus", "m4a", "aac", "aif", "aiff"};
    QStringList result;
    const auto entries = QDir(selectedFolder).entryList(QDir::Files | QDir::Readable, QDir::Name);
    for (const auto &file : entries) {
        if (extensions.contains(QFileInfo(file).suffix().toLower())) result.append(file);
    }
    return result;
}
void MainWindow::selectFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(this, "Select Folder with Audio Files");
    if (folder.isEmpty()) return;
    selectedFolder = folder;
    ui->folderDisplay->setText(QDir::toNativeSeparators(folder));
    ui->folderDisplay->setToolTip(folder);
    ui->outputDisplay->appendPlainText("Found " + QString::number(discoverFiles().size()) + " audio files");
}
void MainWindow::readSettings()
{
    QSettings settings;
    ui->duration->setValue(settings.value("durationMinutes", 0).toInt());
    ui->minDelay->setValue(settings.value("minDelaySeconds", 0).toInt());
    ui->maxDelay->setValue(qMax(ui->minDelay->value(), settings.value("maxDelaySeconds", 0).toInt()));
    ui->volumeSlider->setValue(settings.value("volumePercent", 100).toInt());
    ui->hideCheck->setChecked(settings.value("spoilerMode", false).toBool());
    ui->normalizeCheck->setChecked(settings.value("balanceVolume", false).toBool());
    applyVolume();
    const QString folder = settings.value("audioFolder").toString();
    if (!folder.isEmpty() && QDir(folder).exists()) {
        selectedFolder = folder;
        ui->folderDisplay->setText(QDir::toNativeSeparators(folder));
        ui->folderDisplay->setToolTip(folder);
    } else if (!folder.isEmpty()) {
        ui->outputDisplay->appendPlainText("Saved folder is unavailable. Please select a folder.");
    }
    const auto geometry = settings.value("windowGeometry").toByteArray();
    if (!geometry.isEmpty()) restoreGeometry(geometry);
}
void MainWindow::writeSettings()
{
    QSettings settings;
    settings.setValue("durationMinutes", ui->duration->value());
    settings.setValue("minDelaySeconds", ui->minDelay->value());
    settings.setValue("maxDelaySeconds", ui->maxDelay->value());
    settings.setValue("volumePercent", ui->volumeSlider->value());
    settings.setValue("spoilerMode", ui->hideCheck->isChecked());
    settings.setValue("balanceVolume", ui->normalizeCheck->isChecked());
    settings.setValue("audioFolder", selectedFolder);
    settings.setValue("windowGeometry", saveGeometry());
}
void MainWindow::play()
{
    if (phase != Phase::Idle) return;
    if (selectedFolder.isEmpty()) {
        ui->outputDisplay->appendPlainText("Please select a folder first!");
        return;
    }
    files = discoverFiles();
    ui->outputDisplay->appendPlainText("Found " + QString::number(files.size()) + " audio files");
    if (files.isEmpty()) {
        ui->outputDisplay->appendPlainText("No supported audio files found!");
        return;
    }
    minDelay = ui->minDelay->value();
    maxDelay = ui->maxDelay->value();
    if (ui->duration->value() < 1) {
        ui->outputDisplay->appendPlainText("Duration must be at least 1 minute!");
        return;
    }
    if (minDelay < 0 || maxDelay < 0 || minDelay > maxDelay) {
        ui->outputDisplay->appendPlainText("Delays must be zero or greater, with min no greater than max!");
        return;
    }
    sessionRemaining = qint64(ui->duration->value()) * 60000;
    paused = false;
    phase = Phase::Delay;
    delayRemaining = 0;
    clock.start();
    timer.start();
    if (ui->normalizeCheck->isChecked()) {
        folderReady = false;
        phase = Phase::Preparing;
        QStringList paths;
        for (const auto &file : files) paths.append(QDir(selectedFolder).absoluteFilePath(file));
        normalizer->prepare(paths);
    } else {
        tick(); // First clip starts immediately when balancing is disabled.
    }
    updateControls();
}
void MainWindow::tick()
{
    if (phase == Phase::Idle || paused) return;
    const qint64 elapsed = clock.restart();
    if (phase == Phase::Analyzing || phase == Phase::Preparing) return;
    sessionRemaining = qMax(qint64(0), sessionRemaining - elapsed);
    if (phase == Phase::Delay) delayRemaining -= elapsed;
    const int seconds = int((sessionRemaining + 999) / 1000);
    ui->statusbar->showMessage("Time remaining: " + QString::number(seconds) + " seconds");
    if (sessionRemaining == 0) {
        finishSession("Done playing!");
        return;
    }
    if (phase == Phase::Delay && delayRemaining <= 0) playNext();
}
void MainWindow::playNext()
{
    auto &random = *QRandomGenerator::global();
    // Inclusive, uniform ranges; each selection is independent, so repeats are allowed.
    const int delay = std::uniform_int_distribution<int>(minDelay, maxDelay)(random);
    const auto index = std::uniform_int_distribution<qsizetype>(0, files.size() - 1)(random);
    const QString file = files.at(index);
    delayRemaining = qint64(delay) * 1000;
    pendingFile = file;
    clipGain = 1.0f;
    analysisReady = false;
    if (ui->normalizeCheck->isChecked()) {
        phase = Phase::Analyzing;
        ui->statusbar->showMessage("Balancing clip volume...");
        normalizer->analyze(QDir(selectedFolder).absoluteFilePath(file));
    } else {
        startClip();
    }
}
void MainWindow::applyVolume()
{
    const float level = ui->volumeSlider->value() / 100.0f;
    player->audioOutput()->setVolume(level * level * clipGain);
}
void MainWindow::startClip()
{
    phase = Phase::Clip;
    clock.restart();
    applyVolume();
    const bool hidden = ui->hideCheck->isChecked();
    ui->outputDisplay->appendPlainText("Playing: " + (hidden ? QString("[Hidden]") : pendingFile) +
        " (delay: " + (hidden ? QString("[Hidden]") : QString::number(delayRemaining / 1000) + "s") + ")");
    player->setSource(QUrl::fromLocalFile(QDir(selectedFolder).absoluteFilePath(pendingFile)));
    if (phase == Phase::Clip) player->play();
}
void MainWindow::finishClip(bool failed)
{
    if (phase != Phase::Clip) return;
    tick();
    if (phase == Phase::Idle) return;
    phase = Phase::Delay;
    if (failed) delayRemaining = qMax(delayRemaining, qint64(2000));
    player->stop();
    // Let the next timer tick start playback, avoiding recursive backend callbacks.
    // Failed clips always wait at least two seconds, even with a zero delay.
}
void MainWindow::pause()
{
    if (phase == Phase::Idle || paused) return;
    tick();
    if (phase == Phase::Idle) return;
    paused = true;
    timer.stop();
    if (phase == Phase::Clip) player->pause();
    ui->outputDisplay->appendPlainText("Paused");
    updateControls();
}
void MainWindow::resume()
{
    if (phase == Phase::Idle || !paused) return;
    paused = false;
    clock.restart();
    timer.start();
    if (phase == Phase::Clip) player->play();
    ui->outputDisplay->appendPlainText("Resumed");
    if (phase == Phase::Analyzing && analysisReady) startClip();
    if (phase == Phase::Preparing && folderReady) {
        phase = Phase::Delay;
        tick();
    }
    updateControls();
}
void MainWindow::finishSession(const QString &message)
{
    phase = Phase::Idle;
    paused = false;
    timer.stop();
    player->stop();
    normalizer->cancel();
    clipGain = 1.0f;
    applyVolume();
    player->setSource(QUrl());
    ui->outputDisplay->appendPlainText(message);
    ui->statusbar->showMessage(message);
    updateControls();
}
void MainWindow::updateControls()
{
    const bool active = phase != Phase::Idle;
    ui->playButton->setEnabled(!active);
    ui->selectFolder->setEnabled(!active);
    ui->duration->setEnabled(!active);
    ui->minDelay->setEnabled(!active);
    ui->maxDelay->setEnabled(!active);
    ui->normalizeCheck->setEnabled(!active);
    ui->pauseButton->setEnabled(active && !paused);
    ui->resumeButton->setEnabled(active && paused);
    ui->stopButton->setEnabled(active);
}
