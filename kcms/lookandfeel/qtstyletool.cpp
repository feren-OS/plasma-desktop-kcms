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

#include "lookandfeelsettings.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    const char version[] = "1.0";

    // About data
    KAboutData aboutData("qtstyletool", i18n("Application Style tool"), version, i18n("Command line tool to apply a Qt Application Style."), KAboutLicense::GPL, i18n("Copyright 2017, Marco Martin, 2019, The Feren OS Dev"));
    aboutData.addAuthor(i18n("The Feren OS Dev"), i18n("Maintainer"), QStringLiteral("ferenosdev@outlook.com"));
    aboutData.setDesktopFileName("org.feren.qtstyletool");
    KAboutData::setApplicationData(aboutData);

    const static auto _a = QStringLiteral("apply");

    QCommandLineOption _apply = QCommandLineOption(QStringList() << QStringLiteral("a") << _a,
                                i18n("Apply an Application Style"), i18n("packagename"));
    QCommandLineParser parser;
    parser.addOption(_apply);
    aboutData.setupCommandLine(&parser);

    parser.process(app);
    aboutData.processCommandLine(&parser);

    if (!parser.isSet(_apply)) {
        parser.showHelp();
    }
    
    if (parser.isSet(_apply)) {
        KCMLookandFeel *kcm = new KCMLookandFeel(nullptr, QVariantList());
        kcm->load();
        kcm->setWidgetStyle(parser.value(_apply));
        delete kcm;
    }

    return 0;
}

