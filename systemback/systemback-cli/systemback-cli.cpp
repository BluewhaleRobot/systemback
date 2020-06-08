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

#include "systemback-cli.hpp"
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <ncursesw/ncurses.h>

#ifdef timeout
#undef timeout
#endif

#ifdef instr
#undef instr
#endif

void systemback::main()
{
    auto help([] {
            return tr("Usage: systemback-cli [option]\n\n"
                " Options:\n\n"
                "  -n, --newbackup          create a new restore point\n\n"
                "  -s, --storagedir <path>  get or set the restore points storage directory path\n\n"
                "  -u, --upgrade            upgrade the current system\n"
                "                           remove the unnecessary files and packages\n\n"
                "  -v, --version            output the Systemback version number\n\n"
                "  -h, --help               show this help");
        });

    uchar rv([&] {
            QSL args(qApp->arguments());

            if(args.count() == 1 || [&] {
                    if(sb::like(args.at(1), {"_-h_", "_--help_"}))
                        sb::print("\n " % help() % "\n\n");
                    else if(sb::like(args.at(1), {"_-v_", "_--version_"}))
                        sb::print("\n " % sb::appver() % "\n\n");
                    else
                        return true;

                    return false;
                }()) return sb::isfile("/cdrom/casper/filesystem.squashfs") || sb::isfile("/lib/live/mount/medium/live/filesystem.squashfs") ? 2
                    : getuid() + getgid() ? 3
                    : ! sb::lock(sb::Sblock) ? 4
                    : ! sb::lock(sb::Dpkglock) ? 5
                    : ! sb::lock(sb::Aptlock) ? 6
                    : [&] {
                            auto startui([this](bool crtrpt = false) -> uchar {
                                    if(! (isatty(fileno(stdin)) && isatty(fileno(stdout)) && isatty(fileno(stderr)))) return 255;
                                    initscr();

                                    uchar crv(! has_colors() ? 7
                                        : LINES < 24 || COLS < 80 ? 8
                                        : [crtrpt, this]() -> uchar {
                                                noecho(),
                                                raw(),
                                                curs_set(0),
                                                attron(A_BOLD),
                                                start_color(),
                                                assume_default_colors(COLOR_BLUE, COLOR_BLACK),
                                                init_pair(1, COLOR_WHITE, COLOR_BLACK),
                                                init_pair(2, COLOR_BLUE, COLOR_BLACK),
                                                init_pair(3, COLOR_GREEN, COLOR_BLACK),
                                                init_pair(4, COLOR_YELLOW, COLOR_BLACK),
                                                init_pair(5, COLOR_RED, COLOR_BLACK),
                                                sbtxt = bstr("Systemback " % tr("basic restore UI")), blgn = COLS / 2 - 6 - tr("basic restore UI").length() / 2;
                                                if(! crtrpt) return clistart();
                                                sb::pupgrade();
                                                return newrpnt() ? 0 : sb::dfree(sb::sdir[1]) < 104857600 ? 12 : 13;
                                            }());

                                    endwin();
                                    return crv;
                                });

                            return args.count() == 1 ? startui()
                                : sb::like(args.at(1), {"_-n_", "_--newrestorepoint_"}) ? sb::isdir(sb::sdir[1]) && sb::access(sb::sdir[1], sb::Write) ? startui(true) : 14
                                : sb::like(args.at(1), {"_-s_", "_--storagedir_"}) ? storagedir(args)
                                : sb::like(args.at(1), {"_-u_", "_--upgrade_"}) ? [] {
                                        sb::unlock(sb::Dpkglock), sb::unlock(sb::Aptlock),
                                        sb::supgrade();
                                        return 0;
                                    }() : 1;
                        }();

            return 0;
        }());

    if(! sb::like(rv, {0, 255})) sb::error("\n " % [=]() -> QStr {
            auto dbg([](cQStr &txt) {
                    if(! sb::eout.isEmpty()) sb::crtfile("/tmp/systemback-cli_stderr", QStr((sb::dbglev == sb::sb::Cextdbg ? sb::dbginf() : nullptr) % sb::eout).trimmed().replace("\n\n\n", "\n\n").replace("\n ", "\n") % '\n');
                    return txt;
                });

            switch(rv) {
            case 1:
                return help();
            case 2:
                return tr("The Systemback command line interface cannot be used on a Live system!");
            case 3:
                return tr("Root privileges are required for running the Systemback!");
            case 4:
                return tr("An another Systemback process is currently running, please wait until it finishes.");
            case 5:
                return tr("Unable to get exclusive lock!") % "\n\n " % tr("First, close all package manager.");
            case 6:
                return tr("The re-synchronization of package index files currently in progress, please wait until it finishes.");
            case 7:
                return tr("This stupid terminal does not support color!");
            case 8:
                return tr("This terminal is too small!") % " (< 80x24)";
            case 9:
                return tr("The specified storage directory path has not been set!");
            case 10:
                return tr("The restoration is aborted!");
            case 11:
                return dbg(tr("The restoration is completed, but an error occurred while reinstalling the GRUB!"));
            case 12:
                return dbg(tr("The restore point creation is aborted!") % "\n\n " % tr("Not enough free disk space to complete the process."));
            case 13:
                return dbg(tr("The restore point creation is aborted!") % "\n\n " % tr("There has been critical changes in the file system during this operation."));
            case 14:
                return dbg(tr("The restore points storage directory is not available or not writable!"));
            default:
                return dbg(tr("The restore point deletion is aborted!") % "\n\n " % tr("An error occurred while during the process."));
            }
        }() % "\n\n");

    qApp->exit(rv);
}

uchar systemback::clistart()
{
    mvprintw(0, blgn, sbtxt),
    attron(COLOR_PAIR(1)),
    printw(bstr("\n\n " % tr("Available restore point(s):") % "\n\n")),
    sb::pupgrade(),
    attron(COLOR_PAIR(3));
    if(! sb::pnames[0].isEmpty()) printw(bstr("  1 ─ " % sb::left(sb::pnames[0], COLS - 7) % '\n'));
    if(! sb::pnames[1].isEmpty()) printw(bstr("  2 ─ " % sb::left(sb::pnames[1], COLS - 7) % '\n'));
    if(sb::pnumber == 3) attron(COLOR_PAIR(5));
    if(! sb::pnames[2].isEmpty()) printw(bstr("  3 ─ " % sb::left(sb::pnames[2], COLS - 7) % '\n'));
    if(sb::pnumber == 4) attron(COLOR_PAIR(5));
    if(! sb::pnames[3].isEmpty()) printw(bstr("  4 ─ " % sb::left(sb::pnames[3], COLS - 7) % '\n'));
    if(sb::pnumber == 5) attron(COLOR_PAIR(5));
    if(! sb::pnames[4].isEmpty()) printw(bstr("  5 ─ " % sb::left(sb::pnames[4], COLS - 7) % '\n'));
    if(sb::pnumber == 6) attron(COLOR_PAIR(5));
    if(! sb::pnames[5].isEmpty()) printw(bstr("  6 ─ " % sb::left(sb::pnames[5], COLS - 7) % '\n'));
    if(sb::pnumber == 7) attron(COLOR_PAIR(5));
    if(! sb::pnames[6].isEmpty()) printw(bstr("  7 ─ " % sb::left(sb::pnames[6], COLS - 7) % '\n'));
    if(sb::pnumber == 8) attron(COLOR_PAIR(5));
    if(! sb::pnames[7].isEmpty()) printw(bstr("  8 ─ " % sb::left(sb::pnames[7], COLS - 7) % '\n'));
    if(sb::pnumber == 9) attron(COLOR_PAIR(5));
    if(! sb::pnames[8].isEmpty()) printw(bstr("  9 ─ " % sb::left(sb::pnames[8], COLS - 7) % '\n'));
    if(sb::pnumber == 10) attron(COLOR_PAIR(5));
    if(! sb::pnames[9].isEmpty()) printw(bstr("  A ─ " % sb::left(sb::pnames[9], COLS - 7) % '\n'));
    attron(COLOR_PAIR(3));
    if(! sb::pnames[10].isEmpty()) printw(bstr("  B ─ " % sb::left(sb::pnames[10], COLS - 7) % '\n'));
    if(! sb::pnames[11].isEmpty()) printw(bstr("  C ─ " % sb::left(sb::pnames[11], COLS - 7) % '\n'));
    if(! sb::pnames[12].isEmpty()) printw(bstr("  D ─ " % sb::left(sb::pnames[12], COLS - 7) % '\n'));
    if(! sb::pnames[13].isEmpty()) printw(bstr("  E ─ " % sb::left(sb::pnames[13], COLS - 7) % '\n'));
    if(! sb::pnames[14].isEmpty()) printw(bstr("  F ─ " % sb::left(sb::pnames[14], COLS - 7) % '\n'));
    printw(bstr("\n G ─ " % tr("Create new") % "\n Q ─ " % tr("Quit") % '\n')),
    attron(COLOR_PAIR(2)),
    mvprintw(LINES - 1, COLS - 13, "Kendek, GPLv3"),
    refresh();
    if(! sb::eout.isEmpty()) sb::eout.clear();
    if(! pname.isEmpty()) pname.clear();

    do {
        int gtch(getch());

        switch(gtch) {
        case '1' ... '9':
        {
            QStr cstr(gtch);
            cpoint = "S0" % cstr;
            uchar num(cstr.toUShort() - 1);
            if(! sb::pnames[num].isEmpty()) pname = sb::pnames[num];
            break;
        }
        case 'a':
        case 'A':
            cpoint = "S10";
            if(! sb::pnames[9].isEmpty()) pname = sb::pnames[9];
            break;
        case 'b':
        case 'B':
            cpoint = "H01";
            if(! sb::pnames[10].isEmpty()) pname = sb::pnames[10];
            break;
        case 'c':
        case 'C':
            cpoint = "H02";
            if(! sb::pnames[11].isEmpty()) pname = sb::pnames[11];
            break;
        case 'd':
        case 'D':
            cpoint = "H03";
            if(! sb::pnames[12].isEmpty()) pname = sb::pnames[12];
            break;
        case 'e':
        case 'E':
            cpoint = "H04";
            if(! sb::pnames[13].isEmpty()) pname = sb::pnames[13];
            break;
        case 'f':
        case 'F':
            cpoint = "H05";
            if(! sb::pnames[14].isEmpty()) pname = sb::pnames[14];
            break;
        case 'g':
        case 'G':
            if(! newrpnt()) return sb::dfree(sb::sdir[1]) < 104857600 ? 12 : 13;
            clear();
            return clistart();
        case 'q':
        case 'Q':
            return 0;
        }
    } while(pname.isEmpty());

    clear(),
    mvprintw(0, blgn, sbtxt),
    attron(COLOR_PAIR(1)),
    printw(bstr("\n\n " % tr("Selected restore point:"))),
    attron(COLOR_PAIR(4)),
    printw(bstr("\n\n  " % sb::left(pname, COLS - 3))),
    attron(COLOR_PAIR(3)),
    printw(bstr("\n\n 1 ─ " % tr("Delete") % "\n 2 ─ " % tr("System restore") % " ▸\n B ─ ◂ " % tr("Back"))),
    attron(COLOR_PAIR(2)),
    mvprintw(LINES - 1, COLS - 13, "Kendek, GPLv3"),
    refresh();

    forever
        switch(getch()) {
        case '1':
            pset(2), progress(Start);

            if(! (sb::rename(sb::sdir[1] % '/' % cpoint % '_' % pname, sb::sdir[1] % "/.DELETED_" % pname) && sb::remove(sb::sdir[1] % "/.DELETED_" % pname)))
            {
                progress(Stop);
                return 15;
            }

            emptycache(),
            progress(Stop),
            clear();
            return clistart();
        case '2':
            clear();
            return restore();
        case 'b':
        case 'B':
            clear();
            return clistart();
        }
}

uchar systemback::storagedir(cQSL &args)
{
    if(args.count() == 2)
        sb::print("\n " % sb::sdir[0] % "\n\n");
    else
    {
        QStr ndir;

        {
            QStr cpath, idir(args.at(2));

            if(args.count() > 3)
                for(uchar a(3) ; a < args.count() ; ++a) idir.append(' ' % args.at(a));

            QSL excl{"*/Systemback_", "*/Systemback/*", "*/_", "_/bin_", "_/bin/*", "_/boot_", "_/boot/*", "_/cdrom_", "_/cdrom/*", "_/dev_", "_/dev/*", "_/etc_", "_/etc/*", "_/lib_", "_/lib/*", "_/lib32_", "_/lib32/*", "_/lib64_", "_/lib64/*", "_/opt_", "_/opt/*", "_/proc_", "_/proc/*", "_/root_", "_/root/*", "_/run_", "_/run/*", "_/sbin_", "_/sbin/*", "_/selinux_", "_/selinux/*", "_/snap_", "_/snap/*", "_/srv_", "_/srv/*_", "_/sys_", "_/sys/*", "_/tmp_", "_/tmp/*", "_/usr_", "_/usr/*", "_/var_", "_/var/*"};
            if(sb::like(ndir = QDir::cleanPath(idir), excl) || sb::like(cpath = QDir(idir).canonicalPath(), excl) || sb::like(sb::fload("/etc/passwd"), {"*:" % idir % ":*","*:" % ndir % ":*", "*:" % cpath % ":*"}) || ! sb::islnxfs(cpath)) return 9;
        }

        if(sb::sdir[0] != ndir)
        {
            if(sb::isdir(sb::sdir[1]))
            {
                QSL dlst(QDir(sb::sdir[1]).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot));

                if(! dlst.count())
                    rmdir(bstr(sb::sdir[1]));
                else if(dlst.count() == 1 && sb::isfile(sb::sdir[1] % "/.sbschedule"))
                    sb::remove(sb::sdir[1]);
            }

            sb::sdir[0] = ndir, sb::sdir[1] = sb::sdir[0] % "/Systemback", sb::ismpnt = ! sb::issmfs(sb::sdir[0], sb::sdir[0].count('/') == 1 ? "/" : sb::left(sb::sdir[0], sb::rinstr(sb::sdir[0], "/") - 1));
            if(! sb::cfgwrite()) return 9;
        }

        if(! (sb::isdir(sb::sdir[1]) || sb::crtdir(sb::sdir[1]))) sb::rename(sb::sdir[1], sb::sdir[1] % '_' % sb::rndstr()),
                                                                  sb::crtdir(sb::sdir[1]);

        if(! sb::isfile(sb::sdir[1] % "/.sbschedule")) sb::crtfile(sb::sdir[1] % "/.sbschedule");
        sb::print("\n " % twrp(tr("The specified storage directory path is set.")) % "\n\n");
    }

    return 0;
}

void systemback::emptycache()
{
    pset(1),
    sb::fssync();
    if(sb::ecache) sb::crtfile("/proc/sys/vm/drop_caches", "3");
}

bool systemback::newrpnt()
{
    auto end([this](bool rv = true) {
            progress(Stop);
            return rv;
        });

    progress(Start);

    for(cQStr &item : QDir(sb::sdir[1]).entryList(QDir::Dirs | QDir::Hidden | QDir::NoSymLinks | QDir::NoDotAndDotDot))
        if(sb::like(item, {"_.DELETED_*", "_.S00_*"}))
        {
            if(prun.type != 3) pset(3);
            if(! sb::remove(sb::sdir[1] % '/' % item)) return end(false);
        }

    for(uchar a(9) ; a > 1 ; --a)
        if(! sb::pnames[a].isEmpty() && (a == 9 || a > 2 ? sb::pnumber < a + 2 : sb::pnumber == 3))
        {
            if(prun.type != 4) pset(4);
            if(! (sb::rename(sb::sdir[1] % (a < 9 ? QStr("/S0" % QStr::number(a + 1)) : "/S10") % '_' % sb::pnames[a], sb::sdir[1] % "/.DELETED_" % sb::pnames[a]) && sb::remove(sb::sdir[1] % "/.DELETED_" % sb::pnames[a]))) return end(false);
        }

    pset(5);
    QStr dtime(QDateTime().currentDateTime().toString("yyyy-MM-dd,hh.mm.ss"));
    if(! sb::crtrpoint(dtime)) return end(false);

    for(uchar a(0) ; a < 9 && sb::isdir(sb::sdir[1] % "/S0" % QStr::number(a + 1) % '_' % sb::pnames[a]) ; ++a)
        if(! sb::rename(sb::sdir[1] % "/S0" % QStr::number(a + 1) % '_' % sb::pnames[a], sb::sdir[1] % (a < 8 ? "/S0" : "/S") % QStr::number(a + 2) % '_' % sb::pnames[a])) return end(false);

    if(! sb::rename(sb::sdir[1] % "/.S00_" % dtime, sb::sdir[1] % "/S01_" % dtime)) return end(false);
    sb::crtfile(sb::sdir[1] % "/.sbschedule"),
    emptycache();
    return end();
}

uchar systemback::restore()
{
    mvprintw(0, blgn, sbtxt),
    attron(COLOR_PAIR(1));
    uchar mthd(0), fsave(0), greinst(0);

    {
        bstr rtxt[3]{bstr("\n\n " % tr("Restore with the following restore point:")), bstr("\n\n  " % pname), bstr("\n\n " % tr("Restore with the following restore method:"))};
        printw(rtxt[0]),
        attron(COLOR_PAIR(4)),
        printw(rtxt[1]),
        attron(COLOR_PAIR(1)),
        printw(rtxt[2]),
        attron(COLOR_PAIR(3)),
        printw(bstr("\n\n  1 ─ " % tr("Full restore") % "\n  2 ─ " % tr("System files restore"))),
        attron(COLOR_PAIR(1)),
        printw(bstr("\n\n  " % tr("Users configuration files restore"))),
        attron(COLOR_PAIR(3)),
        printw(bstr("\n\n   3 ─ " % tr("Complete configuration files restore") % "\n   4 ─ " % tr("Keep newly installed configuration files") % "\n\n C ─ " % tr("Cancel"))),
        attron(COLOR_PAIR(2)),
        mvprintw(LINES - 1, COLS - 13, "Kendek, GPLv3"),
        refresh();

        do {
            int gtch(getch());

            switch(gtch) {
            case 'c':
            case 'C':
                clear();
                return clistart();
            case '1' ... '4':
                mthd = QStr(gtch).toUShort();
            }
        } while(! mthd);

        clear(),
        mvprintw(0, blgn, sbtxt),
        attron(COLOR_PAIR(1)),
        printw(rtxt[0]),
        attron(COLOR_PAIR(4)),
        printw(rtxt[1]),
        attron(COLOR_PAIR(1)),
        printw(rtxt[2]),
        attron(COLOR_PAIR(4));

        printw(bstr("\n\n  " % [mthd] {
                switch(mthd) {
                case 1:
                    return tr("Full restore");
                case 2:
                    return tr("System files restore");
                case 3:
                    return tr("Complete configuration files restore");
                default:
                    return tr("Configuration files restore");
                }
            }()));

        attron(COLOR_PAIR(3));

        if(mthd < 3)
        {
            if(sb::isfile("/etc/fstab"))
            {
                printw(bstr("\n\n " % tr("You want to keep the current fstab file?") % ' ' % tr("(Y/N)"))),
                attron(COLOR_PAIR(2)),
                mvprintw(LINES - 1, COLS - 13, "Kendek, GPLv3"),
                refresh();

                do {
                    QChar gtch(getch());

                    if(sb::like(gtch.toUpper(), {"_Y_", '_' % yn[0] % '_'}))
                        fsave = 1;
                    else if(sb::like(gtch.toUpper(), {"_N_", '_' % yn[1] % '_'}))
                        fsave = 2;
                } while(! fsave);

                clear(),
                mvprintw(0, blgn, sbtxt),
                attron(COLOR_PAIR(1)),
                printw(rtxt[0]),
                attron(COLOR_PAIR(4)),
                printw(rtxt[1]),
                attron(COLOR_PAIR(1)),
                printw(rtxt[2]),
                attron(COLOR_PAIR(4)),
                printw(bstr("\n\n  " % (mthd == 1 ? tr("Full restore") : tr("System files restore")) % "\n\n " % tr("You want to keep the current fstab file?") % ' ' % tr("(Y/N)") % ' ' % yn[fsave == 1 ? 0 : 1])),
                attron(COLOR_PAIR(3));

                if(sb::execsrch("update-grub2", sb::sdir[1] % '/' % cpoint % '_' % pname))
                {
                    printw(bstr("\n\n " % tr("Reinstall the GRUB 2 bootloader?") % ' ' % tr("(Y/N)"))),
                    attron(COLOR_PAIR(2)),
                    mvprintw(LINES - 1, COLS - 13, "Kendek, GPLv3"),
                    refresh();

                    do {
                        QChar gtch(getch());

                        if(sb::like(gtch.toUpper(), {"_Y_", '_' % yn[0] % '_'}))
                            greinst = 1;
                        else if(sb::like(gtch.toUpper(), {"_N_", '_' % yn[1] % '_'}))
                            greinst = 2;
                    } while(! greinst);

                    clear(),
                    mvprintw(0, blgn, sbtxt),
                    attron(COLOR_PAIR(1)),
                    printw(rtxt[0]),
                    attron(COLOR_PAIR(4)),
                    printw(rtxt[1]),
                    attron(COLOR_PAIR(1)),
                    printw(rtxt[2]),
                    attron(COLOR_PAIR(4)),
                    printw(bstr("\n\n  " % (mthd == 1 ? tr("Full restore") : tr("System files restore")) % "\n\n " % tr("You want to keep the current fstab file?") % ' ' % tr("(Y/N)") % ' ' % yn[fsave == 1 ? 0 : 1] % "\n\n " % tr("Reinstall the GRUB 2 bootloader?") % ' ' % tr("(Y/N)") % ' ' % yn[greinst == 1 ? 0 : 1]));
                }
            }
            else if(sb::execsrch("update-grub2", sb::sdir[1] % '/' % cpoint % '_' % pname))
            {
                printw(bstr("\n\n " % tr("Reinstall the GRUB 2 bootloader?") % ' ' % tr("(Y/N)"))),
                attron(COLOR_PAIR(2)),
                mvprintw(LINES - 1, COLS - 13, "Kendek, GPLv3"),
                refresh();

                do {
                    QChar gtch(getch());

                    if(sb::like(gtch.toUpper(), {"_Y_", '_' % yn[0] % '_'}))
                        greinst = 1;
                    else if(sb::like(gtch.toUpper(), {"_N_", '_' % yn[1] % '_'}))
                        greinst = 2;
                } while(! greinst);

                clear(),
                mvprintw(0, blgn, sbtxt),
                attron(COLOR_PAIR(1)),
                printw(rtxt[0]),
                attron(COLOR_PAIR(4)),
                printw(rtxt[1]),
                attron(COLOR_PAIR(1)),
                printw(rtxt[2]),
                attron(COLOR_PAIR(4)),
                printw(bstr("\n\n  " % (mthd == 1 ? tr("Full restore") : tr("System files restore")) % "\n\n " % tr("Reinstall the GRUB 2 bootloader?") % ' ' % tr("(Y/N)") % ' ' % yn[greinst == 1 ? 0 : 1]));
            }
        }
    }

    attron(COLOR_PAIR(3)),
    printw(bstr("\n\n " % tr("Start the restore?") % ' ' % tr("(Y/N)"))),
    attron(COLOR_PAIR(2)),
    mvprintw(LINES - 1, COLS - 13, "Kendek, GPLv3"),
    refresh();
    bool rstart(false);

    do {
        QChar gtch(getch());

        if(sb::like(gtch.toUpper(), {"_Y_", '_' % yn[0] % '_'}))
            rstart = true;
        else if(sb::like(gtch.toUpper(), {"_N_", '_' % yn[1] % '_'}))
            return 10;
    } while(! rstart);

    pset(mthd + 5),
    progress(Start);
    bool sfstab(fsave == 1);
    sb::srestore(mthd, nullptr, sb::sdir[1] % '/' % cpoint % '_' % pname, nullptr, sfstab);
    { bool err(greinst == 1 && sb::exec("sh -c \"update-grub ; grub-install --force " % sb::gdetect() % '\"', sb::Silent));
    progress(Stop);
    if(err) return 11; }
    clear(),
    mvprintw(0, blgn, sbtxt),
    attron(COLOR_PAIR(1));

    printw(bstr("\n\n " % twrp([mthd] {
            switch(mthd) {
            case 1:
                return tr("The full system restoration is completed.");
            case 2:
                return tr("The system files restoration are completed.");
            case 3:
                return tr("The users configuration files full restoration are completed.");
            default:
                return tr("The users configuration files restoration are completed.");
            }
        }())));

    attron(COLOR_PAIR(3)),
    printw(bstr("\n\n " % twrp(mthd < 3 ? tr("Press 'ENTER' key to reboot the computer, or 'Q' to quit.") : tr("Press 'ENTER' key to quit.")))),
    attron(COLOR_PAIR(2)),
    mvprintw(LINES - 1, COLS - 13, "Kendek, GPLv3"),
    refresh();

    forever
        switch(getch()) {
        case '\n':
            if(mthd < 3) sb::exec(sb::execsrch("reboot") ? "reboot" : "systemctl reboot", sb::Bckgrnd);
            return 0;
        case 'q':
        case 'Q':
            if(mthd < 3) return 0;
        }
}

void systemback::pset(uchar type)
{
    prun.txt = [type]() -> QStr {
            switch(type) {
            case 1:
                return sb::ecache ? tr("Emptying cache") : tr("Flushing filesystem buffers");
            case 2:
                return tr("Deleting restore point");
            case 3:
                return tr("Deleting incomplete restore point");
            case 4:
                return tr("Deleting old restore point(s)");
            case 5:
                return tr("Creating restore point");
            case 6:
                return tr("Restoring the full system");
            case 7:
                return tr("Restoring the system files");
            default:
                return tr("Restoring the users configuration files");
            }
        }();

    prun.type = type;
}

void systemback::progress(uchar status)
{
    switch(status) {
    case Start:
        connect(ptimer = new QTimer, SIGNAL(timeout()), this, SLOT(progress())),
        QTimer::singleShot(0, this, SLOT(progress())),
        ptimer->start(2000);
        if(sb::dbglev == sb::Nulldbg) sb::dbglev = sb::Errdbg;
        return;
    case Inprog:
        for(uchar a(0) ; a < 4 ; ++a)
        {
            switch(prun.type) {
            case 5 ... 9:
            {
                schar cperc(sb::Progress);

                if(cperc == -1)
                    prun.pbar = prun.pbar == " (?%)" ? " ( %)" : " (?%)";
                else if(cperc > 99)
                {
                    if(prun.cperc < 100) prun.cperc = 100, prun.pbar = " (100%)";
                }
                else if(prun.cperc < cperc)
                    prun.pbar = " (" % QStr::number(prun.cperc = cperc) % "%)";
                else if(! (prun.cperc || prun.pbar == " (0%)"))
                    prun.pbar = " (0%)";
                else if(sb::like(99, {cperc, prun.cperc}, true))
                    prun.pbar = " (100%)", prun.cperc = 100;

                break;
            }
            default:
                if(! prun.pbar.isEmpty()) prun.pbar.clear();
            }

            if(! ptimer) return;
            clear(),
            attron(COLOR_PAIR(2)),
            mvprintw(0, blgn, sbtxt),
            attron(COLOR_PAIR(1));

            mvprintw(LINES / 2 - 1, COLS / 2 - (prun.txt.length() + prun.pbar.length() + 4) / 2, bstr(prun.txt % prun.pbar % [a] {
                    switch(a) {
                    case 0:
                        return "    ";
                    case 1:
                        return " .  ";
                    case 2:
                        return " .. ";
                    default:
                        return " ...";
                    }
                }()));

            attron(COLOR_PAIR(2)),
            mvprintw(LINES - 1, COLS - 13, "Kendek, GPLv3"),
            refresh();
            if(a < 3) sb::delay(500);
        }

        break;
    case Stop:
        delete ptimer, ptimer = nullptr;
        prun.txt.clear();
        if(! prun.pbar.isEmpty()) prun.pbar.clear();
        if(prun.cperc) prun.cperc = 0;
        if(sb::Progress > -1) sb::Progress = -1;
    }
}
