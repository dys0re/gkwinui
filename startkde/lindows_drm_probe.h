/*
    SPDX-FileCopyrightText: 2026 Lindows Authors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QStringList>

inline bool shouldRouteToSoftwareQuick()
{
    static int cachedResult = -1;
    if (cachedResult != -1) {
        return cachedResult == 1;
    }

    auto check = []() -> bool {
        // If user explicitly configured QT_QUICK_BACKEND, respect user choice
        if (!qEnvironmentVariableIsEmpty("QT_QUICK_BACKEND")) {
            return qgetenv("QT_QUICK_BACKEND") == "software";
        }

        // Explicit force flag
        if (qEnvironmentVariableIntValue("LINDOWS_FORCE_SW_QML") == 1) {
            return true;
        }

        // If KWin is in software compositing mode (QPainter)
        const QByteArray kwinCompose = qgetenv("KWIN_COMPOSE");
        if (kwinCompose == "Q" || kwinCompose == "software") {
            return true;
        }

        // Check DRM cards in /sys/class/drm
        const QDir drmDir(QStringLiteral("/sys/class/drm"));
        if (!drmDir.exists()) {
            return true;
        }

        const QStringList entries = drmDir.entryList({QStringLiteral("card[0-9]*")}, QDir::Dirs | QDir::System);
        QStringList primaryCards;
        for (const QString &c : entries) {
            if (!c.contains(QLatin1Char('-'))) {
                primaryCards.append(c);
            }
        }

        if (primaryCards.isEmpty()) {
            return true;
        }

        bool hasHardware3d = false;

        static const QStringList s_acceleratedDrivers = {
            QStringLiteral("i915"),
            QStringLiteral("xe"),
            QStringLiteral("amdgpu"),
            QStringLiteral("radeon"),
            QStringLiteral("nouveau"),
            QStringLiteral("msm"),
            QStringLiteral("panfrost"),
            QStringLiteral("lima"),
            QStringLiteral("etnaviv"),
            QStringLiteral("v3d"),
            QStringLiteral("vc4"),
            QStringLiteral("asahi"),
            QStringLiteral("nvidia"),
            QStringLiteral("nvidia-drm"),
        };

        static const QStringList s_unacceleratedDrivers = {
            QStringLiteral("vkms"),
            QStringLiteral("bochs-drm"),
            QStringLiteral("simpledrm"),
            QStringLiteral("udl"),
            QStringLiteral("hyperv_drm"),
            QStringLiteral("cirrus"),
            QStringLiteral("vboxvideo"),
            QStringLiteral("qxl"),
            QStringLiteral("mgag200"),
            QStringLiteral("ast"),
        };

        for (const QString &card : primaryCards) {
            const QString driverLink = QStringLiteral("/sys/class/drm/%1/device/driver").arg(card);
            QFileInfo driverInfo(driverLink);
            QString driverName;
            if (driverInfo.isSymLink()) {
                driverName = QFileInfo(driverInfo.symLinkTarget()).fileName();
            } else if (driverInfo.exists()) {
                driverName = driverInfo.canonicalFilePath().section(QLatin1Char('/'), -1);
            }

            if (s_acceleratedDrivers.contains(driverName)) {
                hasHardware3d = true;
                break;
            }

            if (s_unacceleratedDrivers.contains(driverName)) {
                continue;
            }

            if (driverName == QLatin1String("virtio_gpu") || driverName == QLatin1String("virtio-pci")) {
                // Check virtio features for VIRTIO_GPU_F_VIRGL (bit 0)
                bool virglFound = false;
                const QDir cardDevDir(QStringLiteral("/sys/class/drm/%1/device").arg(card));
                QStringList featureCandidates;
                const QString devFeatures = cardDevDir.filePath(QStringLiteral("features"));
                if (QFile::exists(devFeatures)) {
                    featureCandidates.append(devFeatures);
                }
                const QStringList virtioSubdirs = cardDevDir.entryList({QStringLiteral("virtio*")}, QDir::Dirs);
                for (const QString &vsub : virtioSubdirs) {
                    const QString fpath = cardDevDir.filePath(vsub + QStringLiteral("/features"));
                    if (QFile::exists(fpath)) {
                        featureCandidates.append(fpath);
                    }
                }
                const QDir virtioBus(QStringLiteral("/sys/bus/virtio/devices"));
                if (virtioBus.exists()) {
                    const QStringList vdevs = virtioBus.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                    for (const QString &vd : vdevs) {
                        const QString fpath = virtioBus.filePath(vd + QStringLiteral("/features"));
                        if (QFile::exists(fpath)) {
                            featureCandidates.append(fpath);
                        }
                    }
                }

                for (const QString &fpath : featureCandidates) {
                    QFile f(fpath);
                    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                        const QByteArray featStr = f.readAll().trimmed();
                        bool ok = false;
                        if (featStr.startsWith("0x") || featStr.startsWith("0X")) {
                            quint64 val = featStr.toULongLong(&ok, 16);
                            if (ok && (val & 0x1)) {
                                virglFound = true;
                                break;
                            }
                        } else {
                            quint64 val = featStr.toULongLong(&ok, 10);
                            if (ok && (val & 0x1)) {
                                virglFound = true;
                                break;
                            } else if (!featStr.isEmpty() && featStr.at(0) == '1') {
                                virglFound = true;
                                break;
                            }
                        }
                    }
                }

                if (virglFound) {
                    hasHardware3d = true;
                    break;
                } else {
                    continue;
                }
            }
        }

        return !hasHardware3d;
    };

    cachedResult = check() ? 1 : 0;
    return cachedResult == 1;
}

inline void applySoftwareQuickRoutingEnvironment()
{
    if (shouldRouteToSoftwareQuick()) {
        if (qEnvironmentVariableIsEmpty("QT_QUICK_BACKEND")) {
            qputenv("QT_QUICK_BACKEND", "software");
        }
        if (qEnvironmentVariableIsEmpty("QSG_RENDER_LOOP")) {
            qputenv("QSG_RENDER_LOOP", "basic");
        }
        if (qEnvironmentVariableIsEmpty("GSK_RENDERER")) {
            qputenv("GSK_RENDERER", "cairo");
        }
        if (qEnvironmentVariableIsEmpty("LIBGL_ALWAYS_SOFTWARE")) {
            qputenv("LIBGL_ALWAYS_SOFTWARE", "1");
        }
        if (qEnvironmentVariableIsEmpty("QT_WAYLAND_DISABLE_HW_INTEGRATION")) {
            qputenv("QT_WAYLAND_DISABLE_HW_INTEGRATION", "1");
        }
        if (qEnvironmentVariableIsEmpty("QT_WAYLAND_HARDWARE_INTEGRATION")) {
            qputenv("QT_WAYLAND_HARDWARE_INTEGRATION", "shm");
        }
        if (qEnvironmentVariableIsEmpty("QT_XCB_GL_INTEGRATION")) {
            qputenv("QT_XCB_GL_INTEGRATION", "none");
        }
        qputenv("LINDOWS_SW_ROUTED", "1");
    }
}
