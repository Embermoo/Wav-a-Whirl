// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Embermoo

#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("Wav-a-Whirl");
    QCoreApplication::setApplicationName("Wav-a-Whirl");
    MainWindow w;
    w.show();
    return QCoreApplication::exec();
}
