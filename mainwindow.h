// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Embermoo

#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
#include <QElapsedTimer>
#include <QStringList>
#include <QTimer>
namespace Ui { class MainWindow; }
class QMediaPlayer;
class AudioNormalizer;
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
private:
    Ui::MainWindow *ui;
    QMediaPlayer *player;
    AudioNormalizer *normalizer;
    float clipGain = 1.0f;
    QString pendingFile;
    bool analysisReady = false;
    bool folderReady = false;
    void startClip();
    void applyVolume();
    void readSettings();
    void writeSettings();
    QTimer timer;
    QElapsedTimer clock;
    enum class Phase { Idle, Clip, Delay, Analyzing, Preparing };
    Phase phase = Phase::Idle;
    bool paused = false;
    QString selectedFolder;
    QStringList files;
    qint64 sessionRemaining = 0, delayRemaining = 0;
    int minDelay = 0, maxDelay = 0;
    void selectFolder();
    void play();
    void pause();
    void resume();
    void tick();
    void playNext();
    void finishClip(bool failed = false);
    void finishSession(const QString &message);
    void updateControls();
    QStringList discoverFiles() const;
};
#endif
