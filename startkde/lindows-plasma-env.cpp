/*
    SPDX-FileCopyrightText: 2026 Lindows Authors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "lindows_drm_probe.h"
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    if (shouldRouteToSoftwareQuick()) {
        std::cout << "QT_QUICK_BACKEND=software\n";
        std::cout << "QSG_RENDER_LOOP=basic\n";
        std::cout << "GSK_RENDERER=cairo\n";
        std::cout << "LIBGL_ALWAYS_SOFTWARE=1\n";
        std::cout << "QT_WAYLAND_DISABLE_HW_INTEGRATION=1\n";
        std::cout << "QT_WAYLAND_HARDWARE_INTEGRATION=shm\n";
        std::cout << "QT_XCB_GL_INTEGRATION=none\n";
        std::cout << "LINDOWS_SW_ROUTED=1\n";
    }
    std::cout << "QML_DISK_CACHE=aot,bytecode\n";
    const QString qmlCacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/qmlcache");
    QDir().mkpath(qmlCacheDir);
    std::cout << "QML_DISK_CACHE_PATH=" << qPrintable(qmlCacheDir) << "\n";
    return 0;
}
