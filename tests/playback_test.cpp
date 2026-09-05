// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Embermoo

#include <QMainWindow>
#include <QElapsedTimer>
#include <QStringList>
#include <QTimer>
#define private public
#include "mainwindow.h"
#undef private
#include <QApplication>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDataStream>
#include <QSpinBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QSlider>
#include <QLabel>
#include <QDir>
#include <QCheckBox>
#include <QSignalSpy>
#include <cmath>
#include <cstdio>
#include <QSettings>
#include <QFontDatabase>
#include <QLineEdit>
#include "audionormalizer.h"
#include <stdexcept>
static void check(bool ok, const char *message) {
    if (!ok) throw std::runtime_error(message);
}
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QTemporaryDir settingsFolder;
    QCoreApplication::setOrganizationName("Wav-a-Whirl-tests");
    QCoreApplication::setApplicationName("playback-test");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsFolder.path());
#ifdef Q_OS_WIN
    const int fontId = QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
    if (fontId >= 0) app.setFont(QFont(QFontDatabase::applicationFontFamilies(fontId).first(), 9));
#endif
    QTemporaryDir folder;
    QFile wav(folder.filePath(QString::fromUtf8("音声.wav")));
    check(wav.open(QIODevice::WriteOnly), "fixture open");
    QDataStream out(&wav);
    out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData("RIFF",4); out << quint32(32036);
    out.writeRawData("WAVEfmt ",8); out << quint32(16) << quint16(1) << quint16(1)
        << quint32(8000) << quint32(16000) << quint16(2) << quint16(16);
    out.writeRawData("data",4); out << quint32(32000);
    out.writeRawData(QByteArray(32000,0).constData(),32000);
    wav.close();
    try {
        MainWindow w;
        w.player->audioOutput()->setMuted(true);
        w.selectedFolder = folder.path();
        w.findChild<QSpinBox*>("duration")->setValue(1);
        w.findChild<QSpinBox*>("minDelay")->setValue(2);
        w.findChild<QSpinBox*>("maxDelay")->setValue(2);
        w.play();
        QTest::qWait(300);
        check(w.player->playbackState() == QMediaPlayer::PlayingState, "Unicode WAV playback");
        auto *volume = w.findChild<QSlider*>("volumeSlider");
        check(volume && volume->isEnabled() && volume->value() == 100, "volume available during playback");
        volume->setValue(0);
        check(w.player->audioOutput()->volume() == 0, "zero volume silences output");
        volume->setValue(50);
        check(w.player->audioOutput()->volume() > 0 && w.player->audioOutput()->volume() < 1, "intermediate volume reaches output");
        check(w.findChild<QLabel*>("volumeLabel")->text() == "Volume: 50%", "volume percentage display");
        w.pause();
        volume->setValue(100);
        check(volume->isEnabled() && w.player->audioOutput()->volume() == 1, "volume available while paused");
        auto remaining = w.sessionRemaining;
        auto position = w.player->position();
        QTest::qWait(150);
        check(w.sessionRemaining == remaining, "paused session clock");
        check(w.player->position() == position, "paused clip");
        w.resume();
        QTest::qWait(150);
        check(w.sessionRemaining < remaining, "resumed clock");
        w.findChild<QPushButton*>("stopButton")->click();
        check(w.phase == MainWindow::Phase::Idle && w.player->playbackState() == QMediaPlayer::StoppedState, "hard stop");
        w.play();
        QTest::qWait(2200);
        check(w.phase == MainWindow::Phase::Delay, "delay after clip end");
        w.pause();
        auto delay = w.delayRemaining;
        QTest::qWait(150);
        check(w.delayRemaining == delay, "paused delay");
        w.resume();
        w.sessionRemaining = 100;
        QTest::qWait(200);
        check(w.phase == MainWindow::Phase::Idle, "expiry during delay");
        w.play();
        w.sessionRemaining = 100;
        QTest::qWait(200);
        check(w.phase == MainWindow::Phase::Idle, "expiry during clip");
        w.play();
        check(w.phase == MainWindow::Phase::Clip, "restart after expiry");
        w.finishSession("test");
        w.findChild<QSpinBox*>("minDelay")->setValue(0);
        w.findChild<QSpinBox*>("maxDelay")->setValue(0);
        auto *log = w.findChild<QPlainTextEdit*>("outputDisplay");
        log->clear();
        w.play();
        QTest::qWait(4500);
        check(log->toPlainText().count("Playing: ") >= 3, "zero delay repeats completed clips");
        check(!log->toPlainText().contains("Failed to play:"), "zero delay playback succeeds");
        check(!log->toPlainText().contains("Time remaining:"), "countdown absent from log");
        check(w.statusBar()->currentMessage().startsWith("Time remaining:"), "status countdown remains");
        w.pause();
        remaining = w.sessionRemaining;
        QTest::qWait(150);
        check(w.sessionRemaining == remaining, "zero delay pause");
        w.resume();
        w.findChild<QPushButton*>("stopButton")->click();
        check(w.phase == MainWindow::Phase::Idle, "zero delay hard stop");
        QFile bad(folder.filePath("broken.wav"));
        check(bad.open(QIODevice::WriteOnly), "bad fixture open"); bad.write("invalid"); bad.close();
        w.files = {"broken.wav"};
        w.sessionRemaining = 10000;
        w.phase = MainWindow::Phase::Delay;
        w.clock.restart();
        w.timer.start();
        w.playNext();
        QTest::qWait(500);
        check(w.phase == MainWindow::Phase::Delay, "invalid media waits before retry");
        check(w.delayRemaining > 1000, "zero delay failure has retry backoff");
        w.finishSession("test");
        w.selectedFolder = QFINDTESTDATA("audio");
        check(!w.selectedFolder.isEmpty(), "encoded fixtures located");
        const auto audioFiles = w.discoverFiles();
        check(audioFiles.size() == 7 && audioFiles.contains("sample.Mp3"), "common formats and mixed case discovered");
        for (const auto &audioFile : audioFiles) {
            log->clear();
            w.files = {audioFile};
            w.sessionRemaining = 10000;
            w.phase = MainWindow::Phase::Delay;
            w.clock.restart();
            w.timer.start();
            w.playNext();
            QTest::qWait(350);
            check(w.player->playbackState() == QMediaPlayer::PlayingState, qPrintable("Playback: " + audioFile));
            check(!log->toPlainText().contains("Failed to play:"), "encoded audio has no playback error");
            w.finishSession("test");
        }
        w.show();
        QTemporaryDir tones;
        auto writeTone = [&](const QString &name, double amplitude) {
            QFile file(tones.filePath(name));
            check(file.open(QIODevice::WriteOnly), "tone fixture");
            QDataStream data(&file);
            data.setByteOrder(QDataStream::LittleEndian);
            data.writeRawData("RIFF", 4); data << quint32(32036);
            data.writeRawData("WAVEfmt ", 8); data << quint32(16) << quint16(1) << quint16(1)
                << quint32(8000) << quint32(16000) << quint16(2) << quint16(16);
            data.writeRawData("data", 4); data << quint32(32000);
            for (int i = 0; i < 16000; ++i)
                data << qint16(amplitude * 32767 * std::sin(i * 0.3455751919));
        };
        writeTone("loud.wav", 0.8);
        writeTone("quiet.wav", 0.2);
        writeTone("silent.wav", 0);
        AudioNormalizer analyzer;
        QSignalSpy result(&analyzer, &AudioNormalizer::completed);
        auto gainFor = [&](const QString &name) {
            result.clear();
            analyzer.analyze(tones.filePath(name));
            if (result.isEmpty()) check(result.wait(5000), "normalizer completes");
            check(result.first().at(1).toBool(), "normalizer decodes");
            return result.first().at(0).toFloat();
        };
        const float loudGain = gainFor("loud.wav");
        const float quietGain = gainFor("quiet.wav");
        check(loudGain > 0 && loudGain < quietGain && quietGain <= 1, "attenuation only");
        check(std::abs(loudGain * 0.8 - quietGain * 0.2) < 0.002, "different levels balanced");
        check(gainFor("silent.wav") == 1, "silence not amplified");
        check(gainFor("loud.wav") == loudGain, "cached result");
        writeTone("loud.wav", 0.4);
        check(gainFor("loud.wav") > loudGain, "modified file reanalyzed");
        result.clear();
        analyzer.analyze(tones.filePath("missing.wav"));
        if (result.isEmpty()) check(result.wait(5000), "analysis failure completes");
        check(!result.first().at(1).toBool() && result.first().at(0).toFloat() == 1, "failed analysis falls back");
        for (const auto &audioFile : audioFiles) {
            result.clear();
            analyzer.analyze(QDir(w.selectedFolder).absoluteFilePath(audioFile));
            if (result.isEmpty()) check(result.wait(5000), "compressed analysis completes");
            check(result.first().at(1).toBool(), qPrintable("Analysis: " + audioFile));
        }
        writeTone("below-target-loud.wav", 0.04);
        writeTone("below-target-soft.wav", 0.01);
        QSignalSpy prepared(&analyzer, &AudioNormalizer::prepared);
        analyzer.prepare({tones.filePath("below-target-loud.wav"), tones.filePath("below-target-soft.wav"), tones.filePath("silent.wav")});
        if (prepared.isEmpty()) check(prepared.wait(5000), "folder analysis completes");
        const float belowGain = gainFor("below-target-loud.wav");
        check(std::abs(belowGain - 0.25f) < 0.002, "below-target clips now balanced");
        check(gainFor("below-target-soft.wav") == 1, "quietest clip not boosted");
        const QString userTestFolder = qEnvironmentVariable("WAV_NORMALIZATION_TEST_FOLDER");
        if (!userTestFolder.isEmpty()) {
            const QString loud = QDir(userTestFolder).filePath("Recording loud.flac");
            const QString soft = QDir(userTestFolder).filePath("Recording soft.flac");
            prepared.clear();
            analyzer.prepare({loud, soft});
            if (prepared.isEmpty()) check(prepared.wait(15000), "user folder analysis completes");
            result.clear();
            analyzer.analyze(loud);
            if (result.isEmpty()) check(result.wait(15000), "user loud analysis");
            check(result.first().at(1).toBool(), "user loud decoded");
            const float actualGain = result.first().at(0).toFloat();
            check(actualGain > 0.19 && actualGain < 0.21, "user loud clip reduced by measured 14 dB");
            result.clear();
            analyzer.analyze(soft);
            if (result.isEmpty()) check(result.wait(15000), "user soft analysis");
            check(result.first().at(1).toBool() && result.first().at(0).toFloat() == 1, "user soft unchanged");
        }
        w.selectedFolder = tones.path();
        w.findChild<QCheckBox*>("normalizeCheck")->setChecked(true);
        w.play();
        w.pause();
        check(QTest::qWaitFor([&] { return w.folderReady; }, 5000), "paused folder preparation completes");
        check(w.paused && w.phase == MainWindow::Phase::Preparing, "analysis cannot start playback while paused");
        w.resume();
        QTest::qWait(300);
        check(w.phase == MainWindow::Phase::Clip, "normalized playback resumes");
        volume->setValue(50);
        check(std::abs(w.player->audioOutput()->volume() - 0.25f * w.clipGain) < 0.001, "slider combines with balancing");
        w.finishSession("test");
        w.play();
        w.finishSession("test");
        QTest::qWait(200);
        check(w.phase == MainWindow::Phase::Idle, "stop cancels analysis");
        QTest::qWait(50);
        check(w.grab().save("volume-preview.png"), "UI preview saved");
        w.findChild<QSpinBox*>("duration")->setValue(7);
        w.findChild<QSpinBox*>("minDelay")->setValue(1);
        w.findChild<QSpinBox*>("maxDelay")->setValue(9);
        w.findChild<QCheckBox*>("hideCheck")->setChecked(true);
        w.resize(720, 500);
        w.writeSettings();
        {
            MainWindow restored;
            check(restored.selectedFolder == w.selectedFolder, "folder restored");
            check(restored.findChild<QSpinBox*>("duration")->value() == 7, "duration restored");
            check(restored.findChild<QSpinBox*>("minDelay")->value() == 1 && restored.findChild<QSpinBox*>("maxDelay")->value() == 9, "delay range restored");
            check(restored.findChild<QSlider*>("volumeSlider")->value() == 50 && restored.player->audioOutput()->volume() == 0.25f, "volume restored to output");
            check(restored.findChild<QCheckBox*>("hideCheck")->isChecked() && restored.findChild<QCheckBox*>("normalizeCheck")->isChecked(), "options restored");
            check(restored.phase == MainWindow::Phase::Idle, "restoring does not autoplay");
            check(restored.size() == QSize(720, 500), "window size restored");
            restored.show();
            restored.resize(560, 420);
            QTest::qWait(50);
            check(restored.grab().save("layout-default.png"), "default layout preview");
            const auto smallLog = restored.findChild<QPlainTextEdit*>("outputDisplay")->size();
            restored.resize(900, 650);
            QTest::qWait(50);
            const auto largeLog = restored.findChild<QPlainTextEdit*>("outputDisplay")->size();
            check(largeLog.width() > smallLog.width() && largeLog.height() > smallLog.height(), "log expands with window");
            QFont larger = restored.font(); larger.setPointSize(18); restored.setFont(larger);
            restored.resize(restored.minimumSizeHint());
            QTest::qWait(50);
            for (auto *label : restored.findChildren<QLabel*>()) {
                check(label->width() >= label->fontMetrics().horizontalAdvance(label->text()), "large text fits labels");
            }
            check(restored.grab().save("layout-large-text.png"), "large font preview");
        }
        QSettings settings;
        settings.setValue("audioFolder", folder.filePath("missing-folder"));
        settings.setValue("volumePercent", 900);
        {
            MainWindow missing;
            check(missing.selectedFolder.isEmpty(), "missing folder cleared");
            check(missing.findChild<QSlider*>("volumeSlider")->value() == 100, "saved volume clamped");
        }
        w.play(); // Destruction while active must stop safely.
    } catch (const std::exception &error) {
        std::fprintf(stderr, "Test failed: %s\n", error.what());
        return 1;
    }
    qInfo("Playback regression checks passed");
    return 0;
}
