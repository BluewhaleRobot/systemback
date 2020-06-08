/*
 * Copyright(C) 2014-2016, Krisztián Kende <nemh@freemail.hu>
 *
 * This file is part of the Systemback.
 *
 * The Systemback is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 3 of the License, or (at your option) any later
 * version.
 *
 * The Systemback is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with the
 * Systemback. If not, see <http://www.gnu.org/licenses>.
 */

#include "sbsustart.hpp"
#include <QTimer>

uint sustart::uid(getuid());

int main(int argc, char *argv[])
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 3, 0)
    if(sustart::uid && setuid(0) && sustart::uid != geteuid())
    {
        QStr arg1(argv[1]);

        if(! sb::like(arg1, {"_systemback_", "_scheduler_"}))
        {
            sb::error("\n Missing, wrong or too much argument(s).\n\n");
            return 2;
        }

        QStr emsg("Cannot start Systemback " % QStr(arg1 == "systemback" ? "graphical user interface" : "scheduler daemon") % "!\n\nUnable to get root permissions.");

        if(seteuid(sustart::uid))
            sb::error("\n " % emsg.replace("\n\n", "\n\n ") % "\n\n");
        else
            sb::exec((sb::execsrch("zenity") ? "zenity --title=Systemback --error --text=\"" : "kdialog --title=Systemback --error=\"") % emsg % '\"', sb::Bckgrnd);

        return 1;
    }
#endif

    QCoreApplication a(argc, argv);
    if(sb::dbglev) sb::dbglev = sb::Nodbg;
    sb::ldtltr();
    sustart s;

    QTimer::singleShot(0, &s,
#if QT_VERSION < QT_VERSION_CHECK(5, 4, 0)
        SLOT(main())
#else
        &sustart::main
#endif
        );

    uchar rv(a.exec());
    return rv;
}
