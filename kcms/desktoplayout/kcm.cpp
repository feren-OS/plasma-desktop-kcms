/* This file is part of the KDE Project
   Copyright (c) 2014 Marco Martin <mart@kde.org>
   Copyright (c) 2014 Vishesh Handa <me@vhanda.in>
   Copyright (c) 2019 Cyril Rossi <cyril.rossi@enioka.com>

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
    about->addAuthor(i18n("Marco Martin"), QString(), QStringLiteral("mart@kde.org"));
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
        KConfigGroup cg(conf, "kdeglobals");
        cg = KConfigGroup(&cg, "KDE");
        
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

    m_configGroup.sync();
    m_package.setPath(m_settings->lookAndFeelPackage());
}
