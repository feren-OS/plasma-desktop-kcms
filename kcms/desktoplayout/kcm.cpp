/* This file is part of the KDE Project
   Copyright (c) 2014 Marco Martin <mart@kde.org>
   Copyright (c) 2014 Vishesh Handa <me@vhanda.in>
   Copyright (c) 2019 Cyril Rossi <cyril.rossi@enioka.com>
   Copyright (c) 2020 Dominic Hayes <ferenosdev@outlook.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "kcm.h"
#include "config-workspace.h"
#include <klauncher_iface.h>

#include <KAboutData>
#include <KSharedConfig>
#include <KGlobalSettings>
#include <KIconLoader>
#include <KAutostart>
#include <KRun>
#include <KService>

#include <QDebug>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QProcess>
#include <QStandardItemModel>
#include <QX11Info>

#include <KLocalizedString>
#include <KPackage/PackageLoader>

#include <X11/Xlib.h>

#include "desktoplayoutsettings.h"

#ifdef HAVE_XFIXES
#  include <X11/extensions/Xfixes.h>
#endif

KCMDesktopLayout::KCMDesktopLayout(QObject *parent, const QVariantList &args)
    : KQuickAddons::ManagedConfigModule(parent, args)
    , m_settings(new DesktopLayoutSettings(this))
    , m_config(QStringLiteral("kdeglobals"))
    , m_configGroup(m_config.group("KDE"))
{
    qmlRegisterType<DesktopLayoutSettings>();
    qmlRegisterType<QStandardItemModel>();
    qmlRegisterType<KCMDesktopLayout>();

    KAboutData *about = new KAboutData(QStringLiteral("kcm_desktoplayout"), i18n("Desktop Layout"),
                                       QStringLiteral("0.1"), QString(), KAboutLicense::LGPL);
    about->addAuthor(i18n("Dominic Hayes"), QString(), QStringLiteral("ferenosdev@outlook.com"));
    setAboutData(about);
    setButtons(Apply | Default);

    m_model = new QStandardItemModel(this);
    QHash<int, QByteArray> roles = m_model->roleNames();
    roles[PluginNameRole] = "pluginName";
    roles[DescriptionRole] = "description";
    roles[ScreenshotRole] = "screenshot";
    roles[FullScreenPreviewRole] = "fullScreenPreview";
    m_model->setItemRoleNames(roles);
    loadModel();
}

KCMDesktopLayout::~KCMDesktopLayout()
{
}

void KCMDesktopLayout::reloadModel()
{
    loadModel();
}

QStandardItemModel *KCMDesktopLayout::desktopLayoutModel() const
{
    return m_model;
}

int KCMDesktopLayout::pluginIndex(const QString &pluginName) const
{
    const auto results = m_model->match(m_model->index(0, 0), PluginNameRole, pluginName);
    if (results.count() == 1) {
        return results.first().row();
    }

    return -1;
}

QList<KPackage::Package> KCMDesktopLayout::availablePackages(const QStringList &components)
{
    QList<KPackage::Package> packages;
    QStringList paths;
    const QStringList dataPaths = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);

    paths.reserve(dataPaths.count());
    for (const QString &path : dataPaths) {
        QDir dir(path + QStringLiteral("/plasma/look-and-feel"));
        paths << dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
    }

    for (const QString &path : paths) {
        KPackage::Package pkg = KPackage::PackageLoader::self()->loadPackage(QStringLiteral("Plasma/LookAndFeel"));
        pkg.setPath(path);
        pkg.setFallbackPackage(KPackage::Package());
        if (components.isEmpty()) {
            packages << pkg;
        } else {
            for (const auto &component : components) {
                if (!pkg.filePath(component.toUtf8()).isEmpty()) {
                    packages << pkg;
                    break;
                }
            }
        }
    }

    return packages;
}

DesktopLayoutSettings *KCMDesktopLayout::desktopLayoutSettings() const
{
    return m_settings;
}

void KCMDesktopLayout::loadModel()
{
    m_model->clear();

    const QList<KPackage::Package> pkgs = availablePackages({"defaults", "layouts"});
    for (const KPackage::Package &pkg : pkgs) {
        if (!pkg.metadata().isValid()) {
            continue;
        }
        QFileInfo check_file(pkg.filePath("layouts") + "/org.kde.plasma.desktop-layout.js");
        if (!check_file.isFile()) {
            continue;
        }
        QStandardItem *row = new QStandardItem(pkg.metadata().name());
        row->setData(pkg.metadata().pluginId(), PluginNameRole);
        row->setData(pkg.metadata().description(), DescriptionRole);
        row->setData(pkg.filePath("preview"), ScreenshotRole);
        row->setData(pkg.filePath("fullscreenpreview"), FullScreenPreviewRole);

        if (!pkg.filePath("defaults").isEmpty()) {
            KSharedConfigPtr conf = KSharedConfig::openConfig(pkg.filePath("defaults"));
            KConfigGroup cg(conf, "kdeglobals");
            cg = KConfigGroup(&cg, "General");
        }

        m_model->appendRow(row);
    }
    m_model->sort(0 /*column*/);

    //Model has been cleared so pretend the selected look and fell changed to force view update
    emit m_settings->lookAndFeelPackageChanged();
}

void KCMDesktopLayout::load()
{
    ManagedConfigModule::load();

    m_package = KPackage::PackageLoader::self()->loadPackage(QStringLiteral("Plasma/LookAndFeel"));
    m_package.setPath(m_settings->lookAndFeelPackage());
}

void KCMDesktopLayout::save()
{
    KPackage::Package package = KPackage::PackageLoader::self()->loadPackage(QStringLiteral("Plasma/LookAndFeel"));
    package.setPath(m_settings->lookAndFeelPackage());

    if (!package.isValid()) {
        return;
    }

    ManagedConfigModule::save();

    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.plasmashell"), QStringLiteral("/PlasmaShell"),
                                                   QStringLiteral("org.kde.PlasmaShell"), QStringLiteral("loadLookAndFeelDefaultLayout"));

    QList<QVariant> args;
    args << m_settings->lookAndFeelPackage();
    message.setArguments(args);

    QDBusConnection::sessionBus().call(message, QDBus::NoBlock);

    if (!package.filePath("defaults").isEmpty()) {
        KSharedConfigPtr conf = KSharedConfig::openConfig(package.filePath("defaults"));
        KConfigGroup cg(conf, "FerenThemer");
        cg = KConfigGroup(&cg, "Options");
        setFilesLayout(cg.readEntry("FilesStyle", QString()));
        // Cleanly quit Latte Dock
        std::system("/usr/bin/killall feren-latte-launch");
        QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.lattedock"), QStringLiteral("/MainApplication"),
                                                QStringLiteral("org.qtproject.Qt.QCoreApplication"), QStringLiteral("quit"));
        QDBusConnection::sessionBus().call(message, QDBus::NoBlock);
        if (cg.readEntry("MetaType", QString()) == "Latte") {
                std::system("/usr/bin/feren-theme-tool-plasma lattemeta");
        } else {
                std::system("/usr/bin/feren-theme-tool-plasma revertmeta");
        }
        cg = KConfigGroup(conf, "kwinrc");
        cg = KConfigGroup(&cg, "org.kde.kdecoration2");
        setWindowButtonsLayout(cg.readEntry("ButtonsOnLeft", QString()), cg.readEntry("ButtonsOnRight", QString()));
            
        cg = KConfigGroup(conf, "lattedockrc");
        cg = KConfigGroup(&cg, "UniversalSettings");
        setLatteLayout(cg.readEntry("currentLayout", QString()));
        
        cg = KConfigGroup(conf, "kwinrc");
        cg = KConfigGroup(&cg, "Windows");
        setBorderlessMaximised(cg.readEntry("BorderlessMaximizedWindows", QString()));
        
        cg = KConfigGroup(conf, "kdeglobals");
        cg = KConfigGroup(&cg, "KDE");
        
        // If the Desktop Layout is not org.feren.default and the Plasma Theme specified is 'feren', set it to 'feren-alt' instead - same for 'feren-light' -> 'feren-light-alt', and if it is org.feren.default switch it back if on -alt
        KConfig config2(QStringLiteral("plasmarc"));
        KConfigGroup cg2(&config2, "Theme");

        if ((cg2.readEntry("name", QString()) == "feren") && (m_settings->lookAndFeelPackage() != "org.feren.default")) {
            cg2.writeEntry("name", QString("feren-alt"));
        } else if ((cg2.readEntry("name", QString()) == "feren-light") && (m_settings->lookAndFeelPackage() != "org.feren.default")) {
            cg2.writeEntry("name", QString("feren-light-alt"));
        } else if ((cg2.readEntry("name", QString()) == "feren-alt") && (m_settings->lookAndFeelPackage() == "org.feren.default")) {
            cg2.writeEntry("name", QString("feren"));
        } else if ((cg2.readEntry("name", QString()) == "feren-light-alt") && (m_settings->lookAndFeelPackage() == "org.feren.default")) {
            cg2.writeEntry("name", QString("feren-light"));
        }
        cg2.sync();
        
        //autostart
        //remove all the old package to autostart
        {
            KSharedConfigPtr oldConf = KSharedConfig::openConfig(m_package.filePath("defaults"));
            cg = KConfigGroup(oldConf, QStringLiteral("Autostart"));
            const QStringList autostartServices = cg.readEntry("Services", QStringList());

            if (qEnvironmentVariableIsSet("KDE_FULL_SESSION")) {
                for (const QString &serviceFile : autostartServices) {
                    KService service(serviceFile + QStringLiteral(".desktop"));
                    KAutostart as(serviceFile);
                    as.setAutostarts(false);
                    //FIXME: quite ugly way to stop things, and what about non KDE things?
                    QProcess::startDetached(QStringLiteral("kquitapp5"), {QStringLiteral("--service"), service.property(QStringLiteral("X-DBUS-ServiceName")).toString()});
                }
            }
        }
        //Set all the stuff in the new lnf to autostart
        {
            cg = KConfigGroup(conf, QStringLiteral("Autostart"));
            const QStringList autostartServices = cg.readEntry("Services", QStringList());

            for (const QString &serviceFile : autostartServices) {
                KService service(serviceFile + QStringLiteral(".desktop"));
                KAutostart as(serviceFile);
                as.setCommand(service.exec());
                as.setAutostarts(true);
                if (qEnvironmentVariableIsSet("KDE_FULL_SESSION")) {
                    KRun::runApplication(service, {}, nullptr);
                }
            }
        }
    }

    // Reload KWin if something changed, but only once.
    QDBusMessage message2 = QDBusMessage::createSignal(QStringLiteral("/KWin"),
                                            QStringLiteral("org.kde.KWin"),
                                            QStringLiteral("reloadConfig"));
    QDBusConnection::sessionBus().send(message2);

    m_configGroup.sync();
    m_package.setPath(m_settings->lookAndFeelPackage());
}

void KCMDesktopLayout::setBorderlessMaximised(const QString &theme)
{
    
    KConfig kwinconfig(QString("kwinrc"));
    KConfigGroup cg(&kwinconfig, "Windows");
    
    if (theme.isEmpty()) {
        cg.writeEntry("BorderlessMaximizedWindows", "false");
        cg.sync();
        return;
    }
    
    cg.writeEntry("BorderlessMaximizedWindows", theme);
    cg.sync();
}

void KCMDesktopLayout::setLatteLayout(const QString &theme)
{
    
    if (theme.isEmpty()) {
        return;
    }
    
    if (QString(theme) != "None") {
        // Make sure Latte is not running as there seems to be inconsistencies with applying layouts
        std::system("/usr/bin/killall feren-latte-launch");
        std::system("/usr/bin/killall latte-dock");
        KConfig config(QString("lattedockrc"));
        KConfigGroup cg(&config, "UniversalSettings");
        //lastNonAssignedLayout also needs to be changed as otherwise Latte switches back to the layout that lastNonAssignedLayout is currently set to when loading up
        cg.writeEntry("currentLayout", theme);
        cg.writeEntry("lastNonAssignedLayout", theme);
        cg.sync();
        QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.lattedock"), QStringLiteral("/Latte"),
                                                    QStringLiteral("org.kde.LatteDock"), QStringLiteral("switchToLayout"));
 
         QList<QVariant> args;
         args << theme;
         message.setArguments(args);
 
         QDBusConnection::sessionBus().call(message, QDBus::NoBlock);
    } else {
        // Disable Latte Dock's autostarting
        KAutostart as("org.kde.latte-dock");
        as.setAutostarts(false);
        // Cleanly quit Latte Dock
        std::system("/usr/bin/killall feren-latte-launch");
        QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.lattedock"), QStringLiteral("/MainApplication"),
                                                    QStringLiteral("org.qtproject.Qt.QCoreApplication"), QStringLiteral("quit"));
        QDBusConnection::sessionBus().call(message, QDBus::NoBlock);
    }
}

void KCMDesktopLayout::setFilesLayout(const QString &theme)
{
    if (theme.isEmpty()) {
        return;
    }
    
    std::string selectedtheme = theme.toStdString();
    std::system(("/usr/bin/feren-theme-tool-plasma setnemo "+selectedtheme).c_str());
}

void KCMDesktopLayout::setWindowButtonsLayout(const QString &leftbtns, const QString &rightbtns)
{
    if (leftbtns.isEmpty() && rightbtns.isEmpty()) {
        return;
    }
    
    KConfig config(QStringLiteral("kwinrc"));
    KConfigGroup cg(&config, "org.kde.kdecoration2");
    cg.writeEntry("ButtonsOnLeft", leftbtns);
    cg.writeEntry("ButtonsOnRight", rightbtns);
    
    cg.sync();
    
}
