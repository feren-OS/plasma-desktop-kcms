/*
 *   Copyright 2017 Marco Martin <mart@kde.org>
 *   Copyright 2020 Dominic Hayes <ferenosdev@outlook.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as
 *   published by the Free Software Foundation; either version 2,
 *   or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "kcm.h"

#include <iostream>

#include <qcommandlineparser.h>
#include <QApplication>
#include <QDebug>

// Frameworks
#include <KAboutData>
#include <klocalizedstring.h>

#include <KPackage/Package>
#include <KPackage/PackageLoader>

#include "desktoplayoutsettings.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    const char version[] = "1.0";

    // About data
    KAboutData aboutData("desktoplayouttool", i18n("Desktop Layout Tool"), version, i18n("Command line tool to apply desktop layout packages for changing the desktop layout."), KAboutLicense::GPL, i18n("Copyright 2020, Dominic Hayes"));
    aboutData.addAuthor(i18n("Dominic Hayes"), i18n("Maintainer"), QStringLiteral("ferenosdev@outlook.com"));
    aboutData.setDesktopFileName("org.feren.plasma.desktoplayouttool");
    KAboutData::setApplicationData(aboutData);

    const static auto _l = QStringLiteral("list");
    const static auto _a = QStringLiteral("apply");

    QCommandLineOption _list = QCommandLineOption(QStringList() << QStringLiteral("l") << _l,
                               i18n("List available desktop layout packages"));
    QCommandLineOption _apply = QCommandLineOption(QStringList() << QStringLiteral("a") << _a,
                                i18n("Apply a desktop layout package"), i18n("packagename"));

    QCommandLineParser parser;
    parser.addOption(_list);
    parser.addOption(_apply);
    aboutData.setupCommandLine(&parser);

    parser.process(app);
    aboutData.processCommandLine(&parser);

    if (!parser.isSet(_list) && !parser.isSet(_apply)) {
        parser.showHelp();
    }
    
    if (parser.isSet(_list)) {
        QList<KPluginMetaData> pkgs = KPackage::PackageLoader::self()->listPackages("Plasma/LookAndFeel");

        for (const KPluginMetaData &data : pkgs) {
            std::cout << data.pluginId().toStdString() << std::endl;
        }

    } else if (parser.isSet(_apply)) {
        KPackage::Package p = KPackage::PackageLoader::self()->loadPackage("Plasma/LookAndFeel");
        p.setPath(parser.value(_apply));

        //can't use package.isValid as lnf packages always fallback, even when not existing
        if (p.metadata().pluginId() != parser.value(_apply)) {
            std::cout << "Unable to find the theme named " << parser.value(_apply).toStdString() << std::endl;
            return 1;
        }

        KCMDesktopLayout *kcm = new KCMDesktopLayout(nullptr, QVariantList());
        kcm->load();
        kcm->desktopLayoutSettings()->setLookAndFeelPackage(parser.value(_apply));
        kcm->save();
        delete kcm;
    }

    return 0;
}

