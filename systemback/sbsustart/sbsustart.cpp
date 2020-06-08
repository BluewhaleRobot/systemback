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
#include <QProcess>

void sustart::main()
{
    bool silent;

    {
        QSL args(qApp->arguments());

        uchar mode([&] {
                switch(args.count()) {
                case 1:
                    if(args.at(0).endsWith("systemback-sustart"))
                    {
                        silent = false;
                        return Systemback;
                    }

                    break;
                case 2 ... 3:
                    silent = true;

                    if(args.at(1) == "systemback")
                        return Systemback;
                    else if(args.at(1) == "finstall")
                        return Finstall;
                    else if(args.at(1) == "scheduler")
                        return Scheduler;
                }

                return Unknown;
            }()), rv(mode ? [&, mode] {
                    QStr uname, usrhm;

                    if(! uid)
                        uname = "root", usrhm = "/root";
                    else
                    {
                        QFile file("/etc/passwd");

                        if(sb::fopen(file))
                            while(! file.atEnd())
                            {
                                QStr line(file.readLine().trimmed());

                                if(line.contains("x:" % QStr::number(uid) % ':'))
                                {
                                    QSL uslst(line.split(':'));
                                    uname = uslst.at(0), usrhm = uslst.at(5);
                                    break;
                                }
                            }

                        if(uname.isEmpty() || usrhm.isEmpty()) return 3;
                    }

                    bool uidinr(getuid()), gidinr(getgid());

                    if(uidinr || gidinr)
                    {
                        if((uidinr && setuid(0)) || (gidinr && setgid(0))) return 3;

                        auto clrenv([](cQBA &uhm, cQStr &xpath = nullptr) {
                                QSL excl{"_DISPLAY_", "_PATH_", "_LANG_", "_XAUTHORITY_", "_DBGLEV_"};

                                for(cQStr &cvar : QProcess::systemEnvironment())
                                {
                                    QStr var(sb::left(cvar, sb::instr(cvar, "=") - 1));
                                    if(! (sb::like(var, excl) || qunsetenv(bstr(var)))) return false;
                                }

                                if(! (qputenv("USER", "root") && qputenv("HOME", uhm) && qputenv("LOGNAME", "root") && qputenv("SHELL", "/bin/bash") && (xpath.isEmpty() || qputenv("XAUTHORITY", xpath.toUtf8())))) return false;
                                return true;
                            });

                        if(mode == Scheduler)
                        {
                            if(! clrenv(usrhm.toUtf8())) return 3;
                            cmd = new QStr("sbscheduler " % uname);
                        }
                        else
                        {
                            QStr xauth("/tmp/sbXauthority-" % sb::rndstr());
                            if((qEnvironmentVariableIsEmpty("XAUTHORITY") || ! QFile(qgetenv("XAUTHORITY")).copy(xauth)) && ! ((sb::isfile("/home/" % uname % "/.Xauthority") && QFile("/home/" % uname % "/.Xauthority").copy(xauth)) || (sb::isfile(usrhm % "/.Xauthority") && QFile(usrhm % "/.Xauthority").copy(xauth)))) return 4;
                            if(! clrenv("/root", xauth)) return 3;
                            cmd = new QStr("systemback " % (mode == Finstall ? QStr("finstall ") : "authorization " % uname));
                        }
                    }
                    else
                        cmd = new QStr(mode == Scheduler ? [&]() -> QStr {
                                qputenv("HOME", usrhm.toUtf8());
                                return "sbscheduler " % uname;
                            }() : "systemback" % QStr(mode == Finstall ? " finstall" : nullptr));

                    return 0;
                }() : 2);

        if(rv)
        {
            if(rv == 2)
                sb::error("\n " % sb::tr("Missing, wrong or too much argument(s).") % "\n\n");
            else
            {
                QStr emsg((mode == Scheduler ? sb::tr("Cannot start the Systemback scheduler daemon!") : sb::tr("Cannot start the Systemback graphical user interface!")) % "\n\n" % (rv == 3 ? sb::tr("Unable to get root permissions.") : sb::tr("Unable to connect to the X server.")));

#if QT_VERSION < QT_VERSION_CHECK(5, 3, 0)
                if(uid != geteuid() && seteuid(uid))
                    sb::error("\n " % emsg.replace("\n\n", "\n\n ") % "\n\n");
                else
#endif
                sb::exec((sb::execsrch("zenity") ? "zenity --title=Systemback --error --text=\"" : "kdialog --title=Systemback --error=\"") % emsg % '\"', sb::Bckgrnd);
            }

            return qApp->exit(rv);
        }

        if(args.count() == 3 && args.at(2) == "gtk+") qputenv("QT_STYLE_OVERRIDE", "gtk+");
    }

    qApp->exit(silent ? sb::exec(*cmd, sb::Silent | sb::Wait, "DBGLEV=0") : sb::exec(*cmd, sb::Wait));
}
