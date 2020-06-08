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

#include "sblib.hpp"
#include <QProcess>
#include <QTime>
#include <QDir>
#include <parted/parted.h>
#include <sys/resource.h>
#include <sys/sendfile.h>
#include <blkid/blkid.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/fs.h>
#include <dirent.h>
#include <utime.h>
#include <fcntl.h>

#ifdef C_MNT_LIB
#include "libmount.hpp"
#else
#include <libmount/libmount.h>
#endif

#ifdef bool
#undef bool
#endif

sb sb::SBThrd;
QTrn *sb::SBtr(nullptr);
QSL *sb::ThrdSlst;
QStr sb::ThrdStr[3], sb::eout, sb::sdir[3], sb::schdlr[2], sb::pnames[15], sb::lang, sb::style, sb::wsclng;
ullong sb::ThrdLng[]{0, 0};
int sb::sblock[4];
uchar sb::ThrdType, sb::ThrdChr, sb::dbglev, sb::pnumber(0), sb::ismpnt(sb::Empty), sb::schdle[]{sb::Empty, sb::Empty, sb::Empty, sb::Empty, sb::Empty, sb::Empty}, sb::waot(sb::Empty), sb::incrmtl(sb::Empty), sb::xzcmpr(sb::Empty), sb::autoiso(sb::Empty), sb::ecache(sb::Empty);
schar sb::Progress(-1);
bool sb::ThrdBool, sb::ExecKill(true), sb::ThrdKill(true), sb::ThrdRslt;

sb::sb()
{
    qputenv("PATH", "/usr/lib/systemback:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"),
    setlocale(LC_ALL, "C.UTF-8"), chdir("/"), umask(0);

    dbglev = qEnvironmentVariableIsEmpty("DBGLEV") ? Nulldbg : [] {
            bool ok;

            switch(qgetenv("DBGLEV").toUShort(&ok)) {
            case Errdbg:
                return Nulldbg;
            case Alldbg:
                return Alldbg;
            case Extdbg:
                return Extdbg;
            case Nodbg:
                if(ok) return Nodbg;
            default:
                return Falsedbg;
            }
        }();
}

sb::~sb()
{
    if(SBtr) delete SBtr;
}

void sb::ldtltr()
{
    QTrn *tltr(new QTrn);
    cfgread();

    if(lang == "auto")
    {
        if(QLocale::system().name() != "en_EN") tltr->load(QLocale::system(), "systemback", "_", "/usr/share/systemback/lang");
    }
    else if(lang != "en_EN")
        tltr->load("systemback_" % lang, "/usr/share/systemback/lang");

    if(tltr->isEmpty())
        delete tltr;
    else
        qApp->installTranslator(SBtr = tltr);

    switch(dbglev) {
    case Falsedbg:
        error("\n " % tr("The specified debug level is invalid!") % "\n\n " % tr("The default level (1) will be used.") % "\n\n"),
        dbglev = Nulldbg;
        break;
    case Extdbg:
        QTS(stderr) << (isatty(fileno(stderr)) ? "\033[1;31m" % dbginf() % "\033[0m" : right(dbginf().replace("\n ", "\n"), -1));
    }
}

void sb::print(cQStr &txt)
{
    QTS(stdout) << (isatty(fileno(stdout)) ? "\033[1m" % txt % "\033[0m" : QStr(txt).replace("\n ", "\n"));
}

bool sb::error(QStr txt, bool dbg)
{
    auto splt([](cQStr &stxt) {
            if(stxt.length() > 80)
            {
                QStr ftxt;
                QSL llst(stxt.split('\n'));

                for(uchar a(1) ; a < llst.count() ; ++a)
                {
                    cQStr &line(llst.at(a));
                    ftxt.append('\n' % (line.length() > 79 && ! like(line, {"*: /*", "*： /*"}) ? llst.value(a).replace(line.left(79).lastIndexOf(' '), 0, '\n') : line));
                }

                return ftxt;
            }
            else
                return stxt;
        });

    auto pfix([&txt]{ if(txt.contains("\n\n  ./")) txt.replace("\n\n  ./", "\n\n  /"); });
    if(! dbg) goto print;

    switch(dbglev) {
    case Errdbg:
    case Cextdbg:
        pfix(), eout.append(isatty(fileno(stderr)) ? splt(txt) : splt(txt).replace("\n ", "\n"));
        break;
    case Alldbg:
    case Extdbg:
        pfix();
print:
        QTS(stderr) << (isatty(fileno(stderr)) ? "\033[1;31m" % splt(txt) % "\033[0m" : splt(txt).replace("\n ", "\n"));
    }

    return false;
}

QStr sb::appver()
{
    QFile file(":version");
    fopen(file);
    QStr vrsn(qVersion());

    return file.readLine().trimmed() % "_Qt" % (vrsn == QT_VERSION_STR ? vrsn : vrsn % '(' % QT_VERSION_STR % ')') % '_' %
#ifdef __clang__
        "Clang" % QStr::number(__clang_major__) % '.' % QStr::number(__clang_minor__) % '.' % QStr::number(__clang_patchlevel__)
#elif defined(__INTEL_COMPILER) || ! defined(__GNUC__)
        "compiler?"
#elif defined(__GNUC__)
        "GCC" % QStr::number(__GNUC__) % '.' % QStr::number(__GNUC_MINOR__) % '.' % QStr::number(__GNUC_PATCHLEVEL__)
#endif
        % '_' %
#ifdef __amd64__
        "amd64";
#elif defined(__i386__)
        "i386";
#else
        "arch?";
#endif
}

QStr sb::dbginf()
{
    QStr txt("\n Systemback\n\n " % tr("Version:") % ' ' % appver() % "\n " % tr("Compilation date and time:") % ' ' % __DATE__ % ' ' % __TIME__ % "\n " % tr("Installed files:") % [] {
            QStr fls;
            QSL lst{"/etc/xdg/autostart/sbschedule-kde.desktop", "/etc/xdg/autostart/sbschedule.desktop", "/usr/bin/systemback", "/usr/bin/systemback-cli", "/usr/bin/systemback-sustart", "/usr/lib/systemback/libsystemback.so", "/usr/lib/systemback/libsystemback.so.1", "/usr/lib/systemback/libsystemback.so.1.0", "/usr/lib/systemback/libsystemback.so.1.0.0", "/usr/lib/systemback/sbscheduler", "/usr/lib/systemback/sbsustart", "/usr/lib/systemback/sbsysupgrade", "/usr/share/applications/systemback-kde.desktop", "/usr/share/applications/systemback.desktop", "/usr/share/systemback/efi-amd64.bootfiles"};
            uchar ind(tr("Installed files:").length() + 2);

            if(isdir("/usr/share/systemback/lang"))
                for(cQStr &file : QDir("/usr/share/systemback/lang").entryList(QDir::Files))
                    if(file.endsWith(".qm")) lst.append("/usr/share/systemback/lang/" % file);

            for(cQStr &file : lst)
                if(isfile(file)) fls.append((fls.isEmpty() ? " " : QStr('\n' % QStr(ind, ' '))) % file);

            return fls;
        }() % "\n " % tr("Operating system:") % []() -> QStr {
                QFile file("/etc/os-release");

                if(fopen(file))
                    while(! file.atEnd())
                    {
                        QStr cline(file.readLine().trimmed());
                        if(cline.startsWith("PRETTY_NAME=\"")) return ' ' % mid(cline, 14, cline.length() - 14);
                    }

                return nullptr;
            }() % "\n " % tr("Mounted filesystems:") % [] {
                    uchar ind(tr("Mounted filesystems:").length() + 2);
                    QStr mpts, spcs('\n' % QStr(ind++, ' '));
                    QBA mnts(fload("/proc/self/mounts"));
                    QTS in(&mnts, QIODevice::ReadOnly);

                    while(! in.atEnd())
                    {
                        QStr cline(in.readLine());

                        if(cline.startsWith("/dev/")) mpts.append((mpts.isEmpty() ? " " : spcs) % [&, ind] {
                                if((cline.contains("\\040") ? cline.replace("\\040", " ") : cline).length() > 81 - ind && isatty(fileno(stderr)))
                                {
                                    ushort clen(cline.length());
                                    uchar mlen(80 - ind);
                                    for(struct{ushort a; uchar b;} a{mlen, 0} ; a.a < clen ; a.a += mlen, ++a.b) cline.replace(a.a + a.b * ind, 0, spcs);
                                }

                                return cline;
                            }());
                    }

                    return mpts;
                }() % "\n " % tr("System language:") % ' ' % QLocale::system().name() % "\n " % tr("Translation:") % ' ' % (SBtr ? lang : QStr('-')) % "\n\n");

    return txt;
}

QStr sb::fdbg(cQStr &path1, cQStr &path2)
{
    switch(dbglev) {
    default:
        return "\n\n";
    case Extdbg:
    case Cextdbg:
        QStr path, txt;
        int cerrno(errno);

        for(uchar a(0) ; a < 2 && ! (a && path2.isEmpty()) ; ++a)
        {
            txt.append('\n'), path = a ? path2 : path1;
            bool cut(false);

            do {
                txt.append("\n " % (cut ? path = path.left(rinstr(path, "/") - 1) : path) % "\n  ");
                struct stat istat;

                if(lstat(bstr(path), &istat))
                    txt.append('-');
                else
                {
                    auto perm([&istat] {
                            QStr prm, rwx("rwxrwxrwx");
                            ushort ugo[]{S_IRUSR, S_IWUSR, S_IXUSR, S_IRGRP, S_IWGRP, S_IXGRP, S_IROTH, S_IWOTH, S_IXOTH};
                            for(uchar b(0) ; b < 9 ; ++b) prm.append((istat.st_mode & ugo[b]) ? rwx.at(b) : '-');
                            if(istat.st_mode & S_ISUID) prm.replace(2, 1, (prm.at(2) == '-' ? 'S' : 's'));
                            if(istat.st_mode & S_ISGID) prm.replace(5, 1, (prm.at(5) == '-' ? 'S' : 's'));
                            if(istat.st_mode & S_ISVTX) prm.replace(8, 1, (prm.at(8) == '-' ? 'T' : 't'));
                            return prm;
                        });

                    switch(istat.st_mode & S_IFMT) {
                    case S_IFREG:
                        txt.append("f " % perm() % ' ' % QStr::number(istat.st_nlink) % ' ' % QStr::number(istat.st_uid) % ' ' % QStr::number(istat.st_gid) % ' ' % QStr::number(istat.st_dev) % ' ' % hunit(istat.st_size).replace(' ', nullptr));
                        break;
                    case S_IFDIR:
                        txt.append("d " % perm() % " - " % QStr::number(istat.st_uid) % ' ' % QStr::number(istat.st_gid) % ' ' % QStr::number(istat.st_dev) % " -");
                        break;
                    case S_IFLNK:
                    {
                        QStr lpath(rlink(path, istat.st_size));

                        txt.append("l " % perm() % ' ' % QStr::number(istat.st_nlink) % ' ' % QStr::number(istat.st_uid) % ' ' % QStr::number(istat.st_gid) % ' ' % QStr::number(istat.st_dev) % " -\n  " % lpath % "\n   " % [&lpath]() -> QStr {
                                switch(stype(lpath)) {
                                case Notexist:
                                    return "-";
                                case Isfile:
                                    return "f";
                                case Isdir:
                                    return "d";
                                case Islink:
                                    return "l / " % [&lpath]() -> QChar {
                                            switch(stype(lpath, true)) {
                                            case Notexist:
                                                return '-';
                                            case Isfile:
                                                return 'f';
                                            case Isdir:
                                                return 'd';
                                            case Isblock:
                                                return 'b';
                                            default:
                                                return '?';
                                            }
                                        }();
                                case Isblock:
                                    return "b";
                                default:
                                    return "?";
                                }
                            }());

                        break;
                    }
                    case S_IFBLK:
                        txt.append("b " % perm() % " - " % QStr::number(istat.st_uid) % ' ' % QStr::number(istat.st_gid) % ' ' % QStr::number(istat.st_dev) % ' ' % hunit(devsize(path)).replace(' ', nullptr));
                        break;
                    default:
                        txt.append('?');
                    }
                }

                if(! cut) cut = true;
            } while(path.count('/') > 1);
        }

        return (txt.contains("\n ./") ? txt.replace("\n ./", "\n /") : txt) % "\n\n Errno: " % QStr::number(cerrno) % "\n\n";
    }
}

bool sb::fopen(QFile &file)
{
    return file.open(QIODevice::ReadOnly) ? true : error("\n " % tr("An error occurred while opening the following file:") % "\n\n  " % file.fileName() % fdbg(file.fileName()), true);
}

bool sb::like(cQStr &txt, cQSL &lst, uchar mode)
{
    switch(mode) {
    default:
        for(cQStr &stxt : lst)
            if(stxt.startsWith('*'))
            {
                if(stxt.endsWith('*'))
                {
                    if(txt.contains(stxt.mid(1, stxt.length() - 2))) return true;
                }
                else if(txt.endsWith(stxt.mid(1, stxt.length() - 2)))
                    return true;
            }
            else if(stxt.endsWith('*'))
            {
                if(txt.startsWith(stxt.mid(1, stxt.length() - 2))) return true;
            }
            else if(txt == stxt.mid(1, stxt.length() - 2))
                return true;

        return false;
    case All:
        for(cQStr &stxt : lst)
            if(stxt.startsWith('*'))
            {
                if(stxt.endsWith('*'))
                {
                    if(! txt.contains(stxt.mid(1, stxt.length() - 2))) return false;
                }
                else if(! txt.endsWith(stxt.mid(1, stxt.length() - 2)))
                    return false;
            }
            else if(stxt.endsWith('*'))
            {
                if(! txt.startsWith(stxt.mid(1, stxt.length() - 2))) return false;
            }
            else if(txt != stxt.mid(1, stxt.length() - 2))
                return false;

        return true;
    case Mixed:
        QSL alst, nlst;

        for(cQStr &stxt : lst)
            switch(stxt.at(0).toLatin1()) {
            case '+':
                alst.append(right(stxt, -1));
                break;
            case '-':
                nlst.append(right(stxt, -1));
                break;
            default:
                return false;
            }

        return like(txt, alst, All) && like(txt, nlst);
    }
}

QStr sb::rndstr(uchar vlen)
{
    QStr val, chrs("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz./");
    val.reserve(vlen), qsrand(QTime::currentTime().msecsSinceStartOfDay());
    uchar clen(vlen == 16 ? 64 : 62), num(255);

    do {
        uchar prev(num);
        while((num = qrand() % clen) == prev);
        val.append(chrs.at(num));
    } while(val.length() < vlen);

    return val;
}

bool sb::access(cQStr &path, uchar mode)
{
    switch(mode) {
    case Read:
        return QFileInfo(path).isReadable();
    case Write:
        return QFileInfo(path).isWritable();
    case Exec:
        return QFileInfo(path).isExecutable();
    default:
        return false;
    }
}

QStr sb::fload(cQStr &path, bool ascnt)
{
    QBA ba;
    { QFile file(path);
    if(! fopen(file)) return nullptr;
    ba = file.readAll(); }
    if(! ascnt || ba.isEmpty()) return ba;
    QSL lst;
    lst.reserve(100);
    QTS in(&ba, QIODevice::ReadOnly);
    while(! in.atEnd()) lst.append(in.readLine());
    QStr str;
    str.reserve(ba.size() + 1), ba.clear();
    for(ushort a(lst.count()) ; a ; --a) str.append(lst.at(a - 1) % '\n');
    return str;
}

QBA sb::fload(cQStr &path)
{
    QFile file(path);
    if(! fopen(file)) return nullptr;
    return file.readAll();
}

bool sb::islnxfs(cQStr &path)
{
    QTemporaryFile file(path % "/.sbdirtestfile_" % rndstr());
    return file.open() && file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther) && file.permissions() == 30548;
}

bool sb::cerr(uchar type, cQStr &str1, cQStr &str2)
{
    return error("\n " % [&, type]() -> QStr {
            switch(type) {
            case Crtdir:
                return tr("An error occurred while creating the following directory:");
            case Rmfile:
                return tr("An error occurred while removing the following file:");
            default:
                return tr("An error occurred while creating the following hard link:") % "\n\n  " % str2 % "\n\n " % tr("Reference file:");
            }
        }() % "\n\n  " % str1 % fdbg(str1, str2), true);
}

bool sb::crtfile(cQStr &path, cQStr &txt)
{
    auto err([&] { return error("\n " % tr("An error occurred while creating the following file:") % "\n\n  " % path % fdbg(path), true); });
    uchar otp(stype(path));
    if(! (like(otp, {Notexist, Isfile}) && isdir(left(path, rinstr(path, "/") - 1)))) return err();
    QFile file(path);
    if(! file.open(QFile::WriteOnly | QFile::Truncate) || file.write(txt.toUtf8()) == -1) return err();
    file.flush();
    return otp == Isfile || file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther) ? true : err();
}

bool sb::rename(cQStr &opath, cQStr &npath)
{
    return QFile::rename(opath, npath) ? true : error("\n " % tr("An error occurred while renaming the following item:") % "\n\n  " % opath % "\n\n " % tr("New path:") % "\n\n  " % npath % fdbg(opath, npath), true);
}

template<typename T1, typename T2> inline bool sb::crthlnk(const T1 &srclnk, const T2 &newlnk)
{
    return link(bstr(srclnk), bstr(newlnk)) ? cerr(Crthlnk, srclnk, newlnk) : true;
}

bool sb::lock(uchar type)
{
    return (sblock[type] = open([type] {
            switch(type) {
            case Sblock:
                return isdir("/run") ? "/run/systemback.lock" : "/var/run/systemback.lock";
            case Dpkglock:
                return "/var/lib/dpkg/lock";
            case Aptlock:
                return "/var/lib/apt/lists/lock";
            default:
                return isdir("/run") ? "/run/sbscheduler.lock" : "/var/run/sbscheduler.lock";
            }
        }(), O_RDWR | O_CREAT, 0644)) > -1 && ! lockf(sblock[type], F_TLOCK, 0);
}

void sb::unlock(uchar type)
{
    close(sblock[type]);
}

void sb::delay(ushort msec)
{
    QTime time;
    time.start();
    do msleep(10), qApp->processEvents();
    while(ushort(time.elapsed()) < msec);
}

void sb::thrdelay()
{
    while(SBThrd.isRunning()) msleep(10), qApp->processEvents();
}

bool sb::cfgwrite(cQStr &file)
{
    QStr cdir(file.startsWith("/.") ? isdir("/.sbsystemcopy" % sdir[0]) ? sdir[0] : "/home" : nullptr);

    return crtfile(file, "# Restore points settings\n#  storage_directory=<path>\n#  storage_dir_is_mount_point=[true/false]\n#  max_temporary_restore_points=[3-10]\n#  use_incremental_backup_method=[true/false]\n\n"
        "storage_directory=" % (cdir.isEmpty() ? sdir[0] : cdir) %
        "\nstorage_dir_is_mount_point=" % (cdir.isEmpty() ? ismpnt ? "true" : "false" : issmfs("/.sbsystemcopy" % cdir, cdir.count('/') == 1 ? "/.sbsystemcopy" : QStr("/.sbsystemcopy" % left(cdir, rinstr(cdir, "/") - 1))) ? "false" : "true") %
        "\nmax_temporary_restore_points=" % QStr::number(pnumber) %
        "\nuse_incremental_backup_method=" % (incrmtl ? "true" : "false") %
        "\n\n\n# Live system settings\n#  working_directory=<path>\n#  use_xz_compressor=[true/false]\n#  auto_iso_images=[true/false]\n\n"
        "working_directory=" % sdir[2] %
        "\nuse_xz_compressor=" % (xzcmpr ? "true" : "false") %
        "\nauto_iso_images=" % (autoiso ? "true" : "false") %
        "\n\n\n# Scheduler settigns\n#  enabled=[true/false]\n#  schedule=[0-7]:[0-23]:[0-59]:[10-99]\n#  silent=[true/false]\n#  window_position=[topleft/topright/center/bottomleft/bottomright]\n#  disable_starting_for_users=[false/everyone/:<username,list>]\n\n"
        "enabled=" % (cdir.isEmpty() && schdle[0] ? "true" : "false") %
        "\nschedule=" % QStr::number(schdle[1]) % ':' % QStr::number(schdle[2]) % ':' % QStr::number(schdle[3]) % ':' % QStr::number(schdle[4]) %
        "\nsilent=" % (schdle[5] ? "true" : "false") %
        "\nwindow_position=" % schdlr[0] %
        "\ndisable_starting_for_users=" % schdlr[1] %
        "\n\n\n# User interface settings\n#  language=[auto/<language_COUNTRY>]\n\n"
        "language=" % lang %
        "\n\n\n# Graphical user interface settings\n#  style=[auto/<name>]\n#  window_scaling_factor=[auto/1/1.5/2]\n#  always_on_top=[true/false]\n\n"
        "style=" % style %
        "\nwindow_scaling_factor=" % wsclng %
        "\nalways_on_top=" % (waot ? "true" : "false") %
        "\n\n\n# Host system settings\n#  disable_cache_emptying=[true/false]\n\n" %
        "disable_cache_emptying=" % (ecache ? "false" : "true") % '\n');
}

void sb::cfgread()
{
    if(! isdir("/etc/systemback"))
        crtdir("/etc/systemback");
    else if(isfile(cfgfile))
    {
        QFile file(cfgfile);

        if(fopen(file))
            while(! file.atEnd())
            {
                QStr cline(file.readLine().trimmed()), cval(right(cline, -instr(cline, "=")));

                if(! (cval.isEmpty() || cval.startsWith('#')))
                {
                    if(cline.startsWith("storage_directory="))
                        sdir[0] = cval;
                    else if(cline.startsWith("storage_dir_is_mount_point="))
                    {
                        if(cval == "true")
                            ismpnt = True;
                        else if(cval == "false")
                            ismpnt = False;
                    }
                    else if(cline.startsWith("max_temporary_restore_points="))
                        pnumber = cval.toUShort();
                    else if(cline.startsWith("use_incremental_backup_method="))
                    {
                        if(cval == "true")
                            incrmtl = True;
                        else if(cval == "false")
                            incrmtl = False;
                    }
                    else if(cline.startsWith("working_directory="))
                        sdir[2] = cval;
                    else if(cline.startsWith("use_xz_compressor="))
                    {
                        if(cval == "true")
                            xzcmpr = True;
                        else if(cval == "false")
                            xzcmpr = False;
                    }
                    else if(cline.startsWith("auto_iso_images="))
                    {
                        if(cval == "true")
                            autoiso = True;
                        else if(cval == "false")
                            autoiso = False;
                    }
                    else if(cline.startsWith("enabled="))
                    {
                        if(cval == "true")
                            schdle[0] = True;
                        else if(cval == "false")
                            schdle[0] = False;
                    }
                    else if(cline.startsWith("schedule="))
                    {
                        QSL vals(cval.split(':'));

                        if(vals.count() == 4)
                            for(uchar a(0) ; a < 4 ; ++a)
                            {
                                bool ok;
                                uchar num(vals.at(a).toUShort(&ok));
                                if(ok) schdle[a + 1] = num;
                            }
                    }
                    else if(cline.startsWith("silent="))
                    {
                        if(cval == "true")
                            schdle[5] = True;
                        else if(cval == "false")
                            schdle[5] = False;
                    }
                    else if(cline.startsWith("window_position="))
                        schdlr[0] = cval;
                    else if(cline.startsWith("disable_starting_for_users="))
                        schdlr[1] = cval;
                    else if(cline.startsWith("language="))
                        lang = cval;
                    else if(cline.startsWith("style="))
                        style = cval;
                    else if(cline.startsWith("window_scaling_factor="))
                        wsclng = cval;
                    else if(cline.startsWith("always_on_top="))
                    {
                        if(cval == "true")
                            waot = True;
                        else if(cval == "false")
                            waot = False;
                    }
                    else if(cline.startsWith("disable_cache_emptying="))
                    {
                        if(cval == "true")
                            ecache = False;
                        else if(cval == "false")
                            ecache = True;
                    }
                }
            }
    }

    bool cfgupdt(false);

    if(sdir[0].isEmpty())
    {
        sdir[0] = "/home", cfgupdt = true;
        if(ismpnt != Empty) ismpnt = Empty;
        if(! isdir("/home/Systemback")) crtdir("/home/Systemback");
        if(! isfile("/home/Systemback/.sbschedule")) crtfile("/home/Systemback/.sbschedule");
    }
    else
    {
        if(! isdir(sdir[0] % "/Systemback") && isdir(sdir[0]) && ! (ismpnt == True && issmfs(sdir[0], sdir[0].count('/') == 1 ? "/" : left(sdir[0], rinstr(sdir[0], "/") - 1))) && crtdir(sdir[0] % "/Systemback")) crtfile(sdir[0] % "/Systemback/.sbschedule");
        QStr cpath(QDir::cleanPath(sdir[0]));
        if(sdir[0] != cpath) sdir[0] = cpath, cfgupdt = true;
    }

    if(sdir[2].isEmpty())
    {
        sdir[2] = "/home";
        if(! cfgupdt) cfgupdt = true;
    }
    else
    {
        QStr cpath(QDir::cleanPath(sdir[2]));

        if(sdir[2] != cpath)
        {
            sdir[2] = cpath;
            if(! cfgupdt) cfgupdt = true;
        }
    }

    if(ismpnt == Empty)
    {
        QStr pdir(sdir[0].count('/') == 1 ? "/" : left(sdir[0], rinstr(sdir[0], "/") - 1));
        ismpnt = sdir[0] != pdir && isdir(pdir) && ! issmfs(sdir[0], pdir);
        if(! cfgupdt) cfgupdt = true;
    }

    if(incrmtl == Empty)
    {
        incrmtl = True;
        if(! cfgupdt) cfgupdt = true;
    }

    if(xzcmpr == Empty)
    {
        xzcmpr = False;
        if(! cfgupdt) cfgupdt = true;
    }

    if(autoiso == Empty)
    {
        autoiso = False;
        if(! cfgupdt) cfgupdt = true;
    }

    if(schdle[0] == Empty)
    {
        schdle[0] = False;
        if(! cfgupdt) cfgupdt = true;
    }

    if(schdle[1] > 7 || schdle[2] > 23 || schdle[3] > 59 || schdle[4] < 10 || schdle[4] > 99)
    {
        schdle[1] = 1, schdle[2] = schdle[3] = 0, schdle[4] = 10;
        if(! cfgupdt) cfgupdt = true;
    }
    else if(schdle[3] < 30 && ! (schdle[1] || schdle[2]))
    {
        schdle[3] = 30;
        if(! cfgupdt) cfgupdt = true;
    }

    if(schdle[5] == Empty)
    {
        schdle[5] = False;
        if(! cfgupdt) cfgupdt = true;
    }

    if(! like(schdlr[0], {"_topleft_", "_topright_", "_center_", "_bottomleft_", "_bottomright_"}))
    {
        schdlr[0] = "topright";
        if(! cfgupdt) cfgupdt = true;
    }

    if(! like(schdlr[1], {"_false_", "_everyone_", "_:*"}))
    {
        schdlr[1] = "false";
        if(! cfgupdt) cfgupdt = true;
    }

    if(pnumber < 3 || pnumber > 10)
    {
        pnumber = 5;
        if(! cfgupdt) cfgupdt = true;
    }

    if(lang.isEmpty() || ! (lang == "auto" || (lang.length() == 5 && lang.at(2) == '_' && lang.at(0).isLower() && lang.at(1).isLower() && lang.at(3).isUpper() && lang.at(4).isUpper())))
    {
        lang = "auto";
        if(! cfgupdt) cfgupdt = true;
    }

    if(style.isEmpty() || style.contains(' '))
    {
        style = "auto";
        if(! cfgupdt) cfgupdt = true;
    }

    if(! like(wsclng, {"_auto_", "_1_", "_1.5_", "_2_"}))
    {
        wsclng = "auto";
        if(! cfgupdt) cfgupdt = true;
    }

    if(waot == Empty)
    {
        waot = False;
        if(! cfgupdt) cfgupdt = true;
    }

    if(ecache == Empty)
    {
        ecache = True;
        if(! cfgupdt) cfgupdt = true;
    }

    sdir[1] = sdir[0] % "/Systemback";
    if(cfgupdt) cfgwrite();

    for(cQStr &file : {excfile, incfile})
        if(! isfile(file)) crtfile(file);
}

bool sb::execsrch(cQStr &fname, cQStr &ppath)
{
    for(cQStr &path : qgetenv("PATH").split(':'))
    {
        QStr fpath(ppath % path % '/' % fname);
        if(isfile(fpath)) return access(fpath, Exec);
    }

    return false;
}

uchar sb::exec(cQStr &cmd, uchar flag, cQStr &envv)
{
    auto exit([&cmd](uchar rv) -> uchar {
            if(! ExecKill && rv && ! like(cmd, {"_apt*", "_dpkg*", "_sbscheduler*"})) error("\n " % tr("An error occurred while executing the following command:") % "\n\n  " % cmd % "\n\n " % tr("Exit code:") % ' ' % QStr::number(rv) % "\n\n", true);
            return rv;
        });

    if(ExecKill) ExecKill = false;
    bool silent, bckgrnd, wait;
    uchar rprcnt;

    if(flag == Noflag)
        silent = bckgrnd = wait = false, rprcnt = 0;
    else
    {
        silent = flag != (flag & ~Silent), wait = flag != (flag & ~Wait);
        if((rprcnt = (bckgrnd = flag != (flag & ~Bckgrnd)) || wait || flag == (flag & ~Prgrss) ? 0 : cmd.startsWith("mksquashfs") ? 1 : cmd.startsWith("genisoimage") ? 2 : cmd.startsWith("tar -cf") ? 3 : cmd.startsWith("tar -xf") ? 4 : 0)) Progress = 0;
    }

    QProcess proc;

    if(silent)
        proc.setStandardOutputFile("/dev/null"), proc.setStandardErrorFile("/dev/null");
    else if(! rprcnt)
        proc.setProcessChannelMode(QProcess::ForwardedChannels), proc.setInputChannelMode(QProcess::ForwardedInputChannel);

    if(! envv.isEmpty())
    {
        QProcessEnvironment env(QProcessEnvironment::systemEnvironment());
        env.insert(left(envv, instr(envv, "=") - 1), right(envv, envv.length() - instr(envv, "="))), proc.setProcessEnvironment(env);
    }

    if(bckgrnd) return proc.startDetached(cmd) ? 0 : exit(255);
    proc.start(cmd, QProcess::ReadOnly);
    while(proc.state() == QProcess::Starting) msleep(10), qApp->processEvents();
    if(proc.error() == QProcess::FailedToStart) return exit(255);

    if(wait)
        proc.waitForFinished(-1);
    else
    {
        if(rprcnt == 1) setpriority(0, proc.pid(), 10);
        ullong inum(0);
        uchar cperc;

        while(proc.state() == QProcess::Running)
        {
            msleep(10), qApp->processEvents();
            if(ExecKill) proc.kill();

            switch(rprcnt) {
            case 1:
                if(Progress < (cperc = ((inum += proc.readAllStandardOutput().count('\n')) * 100 + 50) / ThrdLng[0])) Progress = cperc;
                QTS(stderr) << proc.readAllStandardError();

                if(dfree(sdir[2]) < 104857600)
                {
                    proc.kill();
                    return exit(255);
                }

                break;
            case 2:
            {
                QStr pout(proc.readAllStandardError());
                if(Progress < (cperc = mid(pout, rinstr(pout, "%") - 5, 2).toUShort())) Progress = cperc;
                break;
            }
            case 3:
                if(! ThrdLng[0])
                {
                    QBA itms;
                    QUCL itmst;
                    itms.reserve(10000), itmst.reserve(500);
                    ushort lcnt(0);
                    rodir(itms, itmst, sdir[2] % "/.sblivesystemcreate");

                    if(! itmst.isEmpty())
                    {
                        QTS in(&itms, QIODevice::ReadOnly);

                        while(! in.atEnd())
                        {
                            QStr item(in.readLine());
                            if(itmst.at(lcnt++) == Isfile) ThrdLng[0] += fsize(sdir[2] % "/.sblivesystemcreate/" % item);
                        }
                    }
                }
                else if(isfile(ThrdStr[0]) && Progress < (cperc = (fsize(ThrdStr[0]) * 100 + 50) / ThrdLng[0]))
                    Progress = cperc;

                break;
            case 4:
                QBA itms;
                QUCL itmst;
                itms.reserve(10000), itmst.reserve(500);
                ushort lcnt(0);
                rodir(itms, itmst, ThrdStr[0]);

                if(! itmst.isEmpty())
                {
                    QTS in(&itms, QIODevice::ReadOnly);
                    ullong size(0);

                    while(! in.atEnd())
                    {
                        QStr item(in.readLine());
                        if(itmst.at(lcnt++) == Isfile) size += fsize(ThrdStr[0] % '/' % item);
                    }

                    if(Progress < (cperc = (size * 100 + 50) / ThrdLng[0])) Progress = cperc;
                }
            }
        }
    }

    return exit(proc.exitStatus() == QProcess::CrashExit ? 255 : proc.exitCode());
}

uchar sb::exec(cQSL &cmds)
{
    uchar rv;

    for(cQStr &cmd : cmds)
        if((rv = exec(cmd))) return rv;

    return 0;
}

bool sb::mcheck(cQStr &item, cQStr &mnts)
{
    cQStr &itm(item.contains(' ') ? bstr(item).rplc(" ", "\\040") : item);

    if(! itm.startsWith("/dev/"))
        return itm.endsWith('/') && itm.length() > 1 ? like(mnts, {"* " % left(itm, -1) % " *", "* " % itm % "*"}) : mnts.contains(' ' % itm % ' ');
    else if(QStr('\n' % mnts).contains('\n' % itm % (itm.length() > (item.contains("mmc") ? 12 : 8) ? " " : nullptr)))
        return true;
    else
    {
        blkid_probe pr(blkid_new_probe_from_filename(bstr(itm)));
        cchar *val(nullptr);
        blkid_do_probe(pr), blkid_probe_lookup_value(pr, "UUID", &val, nullptr);
        QStr uuid(val);
        blkid_free_probe(pr);
        return ! uuid.isEmpty() && QStr('\n' % mnts).contains("\n/dev/disk/by-uuid/" % uuid % ' ');
    }
}

QStr sb::gdetect(cQStr rdir)
{
    QStr mnts(fload("/proc/self/mounts", true));
    QTS in(&mnts, QIODevice::ReadOnly);
    QSL incl[]{{"* " % rdir % " *", "* " % rdir % (rdir.endsWith('/') ? nullptr : "/") % "boot *"}, {"_/dev/sd*", "_/dev/hd*", "_/dev/vd*"}};

    while(! in.atEnd())
    {
        QStr cline(in.readLine());

        if(like(cline, incl[0]))
        {
            if(like(cline, incl[1]))
                return left(cline, 8);
            else if(cline.startsWith("/dev/mmcblk"))
                return left(cline, 12);
            else if(cline.startsWith("/dev/disk/by-uuid"))
            {
                QStr uid(right(left(cline, instr(cline, " ") - 1), -18));

                if(islink("/dev/disk/by-uuid/" % uid))
                {
                    QStr dev(QFile("/dev/disk/by-uuid/" % uid).readLink());
                    return left(dev, dev.contains("mmc") ? 12 : 8);
                }
            }

            break;
        }
    }

    error("\n " % tr("Failed to detect the device for installing the GRUB!") % "\n\n", true);
    return nullptr;
}

void sb::pupgrade()
{
    bool rerun;

    do {
        rerun = false;

        for(uchar a(0) ; a < 15 ; ++a)
            if(! pnames[a].isEmpty()) pnames[a].clear();

        if(isdir(sdir[1]) && access(sdir[1], Write))
        {
            for(cQStr &item : QDir(sdir[1]).entryList(QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot))
                if(! item.contains(' '))
                {
                    QStr pre(left(item, 4));

                    if(pre.at(1).isDigit() && pre.at(2).isDigit() && pre.at(3) == '_')
                    {
                        if(pre.at(0) == 'S')
                        {
                            if(pre.at(1) == '0' || mid(pre, 2, 2) == "10") pnames[mid(pre, 2, 2).toUShort() - 1] = right(item, -4);
                        }
                        else if(pre.at(0) == 'H' && pre.at(1) == '0' && like(pre.at(2), {"_1_", "_2_", "_3_", "_4_", "_5_"}))
                            pnames[9 + mid(pre, 3, 1).toUShort()] = right(item, -4);
                    }
                }

            for(uchar a(14) ; a ; --a)
                if(! (a == 10 || pnames[a].isEmpty()) && pnames[a - 1].isEmpty())
                {
                    rename(sdir[1] % (a > 10 ? QStr("/H0" % QStr::number(a - 9)) : (a < 9 ? "/S0" : "/S") % QStr::number(a + 1)) % '_' % pnames[a], sdir[1] % (a > 10 ? ("/H0" % QStr::number(a - 10)) : "/S0" % QStr::number(a)) % '_' % pnames[a]);
                    if(! rerun) rerun = true;
                }
        }
    } while(rerun);
}

void sb::supgrade()
{
    exec("apt-get update");

    QStr fyes([] {
            QProcess proc;
            proc.start("apt -v", QProcess::ReadOnly), proc.waitForFinished(-1);
            QStr sout(proc.readAllStandardOutput());
            return sout.length() < 7 || mid(sout, 5, 3).replace('.', nullptr).toUShort() < 12 ? "--force-yes" : "--allow-downgrades --allow-change-held-packages";
        }());

    forever
    {
        if(! exec({"apt-get install -fym " % fyes, "dpkg --configure -a", "apt-get dist-upgrade --no-install-recommends -ym " % fyes, "apt-get autoremove --purge -y"}))
        {
            QStr rklist;

            {
                QSL dlst(QDir("/boot").entryList(QDir::Files, QDir::Reversed));

                for(cQStr &item : dlst)
                    if(item.startsWith("vmlinuz-"))
                    {
                        QStr vmlinuz(right(item, -8)), kernel(left(vmlinuz, instr(vmlinuz, "-") - 1)), kver(mid(vmlinuz, kernel.length() + 2, instr(vmlinuz, "-", kernel.length() + 2) - kernel.length() - 2));

                        if(isnum(kver) && vmlinuz.startsWith(kernel % '-' % kver % '-') && ! rklist.contains(kernel % '-' % kver % "-*"))
                        {
                            for(ushort a(1) ; a < 101 ; ++a)
                            {
                                QStr subk(kernel % '-' % QStr::number(kver.toUShort() - a));

                                for(cQStr &ritem : dlst)
                                    if(ritem.startsWith("vmlinuz-" % subk % '-') && ! rklist.contains(' ' % subk % "-*")) rklist.append(' ' % subk % "-*");
                            }
                        }
                    }
            }

            uchar cproc(rklist.isEmpty() ? 0 : exec("apt-get autoremove --purge " % rklist));

            if(like(cproc, {0, 1}))
            {
                {
                    QProcess proc;
                    proc.start("dpkg -l", QProcess::ReadOnly), proc.waitForFinished(-1);
                    QBA sout(proc.readAllStandardOutput());
                    QStr iplist;
                    QTS in(&sout, QIODevice::ReadOnly);

                    while(! in.atEnd())
                    {
                        QStr cline(in.readLine());
                        if(cline.startsWith("rc")) iplist.append(' ' % mid(cline, 5, instr(cline, " ", 5) - 5));
                    }

                    if(! iplist.isEmpty()) exec("dpkg --purge " % iplist);
                }

                exec("apt-get clean");

                {
                    QSL dlst(QDir("/var/cache/apt").entryList(QDir::Files));

                    for(uchar a(0) ; a < dlst.count() ; ++a)
                    {
                        cQStr &item(dlst.at(a));
                        if(item.contains(".bin.")) rmfile("/var/cache/apt/" % item);
                    }
                }

                for(cQStr &item : QDir("/lib/modules").entryList(QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot))
                    if(! exist("/boot/vmlinuz-" % item)) QDir("/lib/modules/" % item).removeRecursively();

                break;
            }
        }
        else
            exec("dpkg --configure -a");

        exec({"tput reset", "tput civis"});

        for(uchar a(3) ; a ; --a) error("\n " % tr("An error occurred while upgrading the system!") % '\n'),
                                  print("\n " % tr("Restart upgrade ...") % ' ' % QStr::number(a)),
                                  sleep(1), exec("tput cup 0 0");

        exec("tput reset");
    }
}

ullong sb::devsize(cQStr &dev)
{
    ullong bsize;
    int odev;
    bool err;

    if(! (err = (odev = open(bstr(dev), O_RDONLY)) == -1))
    {
        if(ioctl(odev, BLKGETSIZE64, &bsize) == -1) err = true;
        close(odev);
    }

    return err ? 0 : bsize;
}

bool sb::copy(cQStr &srcfile, cQStr &newfile)
{
    if(! isfile(srcfile)) return error("\n " % tr("This file could not be copied because it does not exist:") % "\n\n  " % srcfile % fdbg(srcfile), true);
    ThrdType = Copy,
    ThrdStr[0] = srcfile,
    ThrdStr[1] = newfile,
    SBThrd.start(), thrdelay();
    return ThrdRslt;
}

QStr sb::ruuid(cQStr &part)
{
    ThrdType = Ruuid,
    ThrdStr[0] = part,
    SBThrd.start(), thrdelay();
    if(ThrdStr[1].isEmpty()) error("\n " % tr("The following partition has no UUID:") % "\n\n  " % part % "\n\n", true);
    return ThrdStr[1];
}

bool sb::srestore(uchar mthd, cQStr &usr, cQStr &srcdir, cQStr &trgt, bool sfstab)
{
    ThrdType = Srestore,
    ThrdChr = mthd,
    ThrdStr[0] = usr,
    ThrdStr[1] = srcdir,
    ThrdStr[2] = trgt,
    ThrdBool = sfstab,
    SBThrd.start(), thrdelay();
    return ThrdRslt;
}

bool sb::mkpart(cQStr &dev, ullong start, ullong len, uchar type)
{
    auto err([&dev] { return error("\n " % tr("An error occurred while creating a new partition on the following device:") % "\n\n  " % dev % fdbg(dev), true); });
    if(dev.length() > (dev.contains("mmc") ? 12 : 8) || stype(dev) != Isblock) return err();
    ThrdType = Mkpart,
    ThrdStr[0] = dev,
    ThrdLng[0] = start,
    ThrdLng[1] = len,
    ThrdChr = type,
    SBThrd.start(), thrdelay();
    return ThrdRslt ? true : err();
}

bool sb::mount(cQStr &dev, cQStr &mpoint, cQStr &moptns)
{
    auto err([&dev] { return error("\n " % tr("An error occurred while mounting the following partition/image:") % "\n\n  " % dev % fdbg(dev), true); });
#ifdef C_MNT_LIB
    if(moptns == "loop") return exec("mount -o loop " % dev % ' ' % mpoint) ? err() : true;
#endif
    ThrdType = Mount,
    ThrdStr[0] = dev,
    ThrdStr[1] = mpoint,
    ThrdStr[2] = moptns,
    SBThrd.start(), thrdelay();
    return ThrdRslt ? true : err();
}

bool sb::scopy(uchar mthd, cQStr &usr, cQStr &srcdir)
{
    ThrdType = Scopy,
    ThrdChr = mthd,
    ThrdStr[0] = usr,
    ThrdStr[1] = srcdir,
    SBThrd.start(), thrdelay();
    return ThrdRslt;
}

bool sb::crtrpoint(cQStr &pname)
{
    ThrdType = Crtrpoint,
    ThrdStr[0] = "/.S00_" % pname,
    SBThrd.start(), thrdelay();
    return ThrdRslt;
}

bool sb::setpflag(cQStr &part, cQStr &flags)
{
    auto err([&] { return error("\n " % tr("An error occurred while setting one or more flags on the following partition:") % "\n\n  " % part % "\n\n " % tr("Flag(s):") % ' ' % flags % fdbg(part), true); });
    { bool ismmc(part.contains("mmc"));
    if(part.length() < (ismmc ? 14 : 9) || stype(part) != Isblock || stype(left(part, (ismmc ? 12 : 8))) != Isblock) return err(); }
    ThrdType = Setpflag,
    ThrdStr[0] = part,
    ThrdStr[1] = flags,
    SBThrd.start(), thrdelay();
    return ThrdRslt ? true : err();
}

bool sb::lvprpr(bool iudata)
{
    ThrdType = Lvprpr,
    ThrdBool = iudata,
    SBThrd.start(), thrdelay();
    return ThrdRslt;
}

bool sb::mkptable(cQStr &dev, cQStr &type)
{
    auto err([&dev] { return error("\n " % tr("An error occurred while creating the partition table on the following device:") % "\n\n  " % dev % fdbg(dev), true); });
    if(dev.length() > (dev.contains("mmc") ? 12 : 8) || stype(dev) != Isblock) return err();
    ThrdType = Mkptable,
    ThrdStr[0] = dev,
    ThrdStr[1] = type,
    SBThrd.start(), thrdelay();
    return ThrdRslt ? true : err();
}

bool sb::remove(cQStr &path)
{
    ThrdType = Remove,
    ThrdStr[0] = path,
    SBThrd.start(), thrdelay();
    return ThrdRslt;
}

bool sb::umount(cQStr &dev)
{
    ThrdType = Umount,
    ThrdStr[0] = dev,
    SBThrd.start(), thrdelay();
    return ThrdRslt ? true : error("\n " % tr("An error occurred while unmounting the following partition/image/mount point:") % "\n\n  " % dev % fdbg(dev), true);
}

void sb::readlvdevs(QSL &strlst)
{
    ThrdType = Readlvdevs,
    ThrdSlst = &strlst,
    SBThrd.start(), thrdelay();
}

void sb::readprttns(QSL &strlst)
{
    ThrdType = Readprttns,
    ThrdSlst = &strlst,
    SBThrd.start(), thrdelay();
}

void sb::fssync()
{
    ThrdType = Sync,
    SBThrd.start(), thrdelay();
}

void sb::delpart(cQStr &part)
{
    if(stype(part) == Isblock)
    {
        ThrdType = Delpart,
        ThrdStr[0] = part,
        SBThrd.start(), thrdelay();
    }
}

inline QStr sb::rlink(cQStr &path, ushort blen)
{
    char rpath[blen];
    short rlen(readlink(bstr(path), rpath, blen));
    return rlen > 0 ? QStr(rpath).left(rlen) : nullptr;
}

uchar sb::fcomp(cQStr &file1, cQStr &file2)
{
    struct stat fstat[2];

    return stat(bstr(file1), &fstat[0]) || stat(bstr(file2), &fstat[1]) ? 0
        : fstat[0].st_size == fstat[1].st_size && fstat[0].st_mtim.tv_sec == fstat[1].st_mtim.tv_sec ? fstat[0].st_mode == fstat[1].st_mode && fstat[0].st_uid == fstat[1].st_uid && fstat[0].st_gid == fstat[1].st_gid ? 2 : 1 : 0;
}

bool sb::cpertime(cQStr &srcitem, cQStr &newitem, bool skel)
{
    auto err([&] { return error("\n " % tr("An error occurred while cloning the properties of the following item:") % "\n\n  " % srcitem % "\n\n " % tr("Target item:") % "\n\n  " % newitem % fdbg(srcitem, newitem), true); });
    struct stat istat[2];
    if(stat(bstr(srcitem), &istat[0])) return err();
    bstr nitem(newitem);
    if(stat(nitem, &istat[1])) return err();

    if(skel)
    {
        struct stat ustat;
        if(stat(bstr(left(newitem, instr(newitem, "/", 21) - 1)), &ustat)) return err();
        istat[0].st_uid = ustat.st_uid, istat[0].st_gid = ustat.st_gid;
    }

    if(istat[0].st_uid != istat[1].st_uid || istat[0].st_gid != istat[1].st_gid)
    {
        if(chown(nitem, istat[0].st_uid, istat[0].st_gid) || ((istat[0].st_mode != (istat[0].st_mode & ~(S_ISUID | S_ISGID)) || istat[0].st_mode != istat[1].st_mode) && chmod(nitem, istat[0].st_mode))) return err();
    }
    else if(istat[0].st_mode != istat[1].st_mode && chmod(nitem, istat[0].st_mode))
        return err();

    if(istat[0].st_atim.tv_sec != istat[1].st_atim.tv_sec || istat[0].st_mtim.tv_sec != istat[1].st_mtim.tv_sec)
    {
        utimbuf sitimes;
        sitimes.actime = istat[0].st_atim.tv_sec, sitimes.modtime = istat[0].st_mtim.tv_sec;
        if(utime(nitem, &sitimes)) return err();
    }

    return true;
}

bool sb::cplink(cQStr &srclink, cQStr &newlink)
{
    auto err([&] { return error("\n " % tr("An error occurred while cloning the following symbolic link:") % "\n\n  " % srclink % "\n\n " % tr("Target symlink:") % "\n\n  " % newlink % fdbg(srclink, newlink), true); });
    struct stat sistat;
    if(lstat(bstr(srclink), &sistat) || ! S_ISLNK(sistat.st_mode)) return err();
    QStr path(rlink(srclink, sistat.st_size));
    bstr nlink(newlink);
    if(path.isEmpty() || symlink(bstr(path), nlink)) return err();
    timeval sitimes[2];
    sitimes[0].tv_sec = sistat.st_atim.tv_sec, sitimes[1].tv_sec = sistat.st_mtim.tv_sec, sitimes[0].tv_usec = sitimes[1].tv_usec = 0;
    return lutimes(nlink, sitimes) ? err() : true;
}

bool sb::cpfile(cQStr &srcfile, cQStr &newfile, bool skel)
{
    auto err([&] { return error("\n " % tr("An error occurred while cloning the following file:") % "\n\n  " % srcfile % "\n\n " % tr("Target file:") % "\n\n  " % newfile % fdbg(srcfile, newfile), true); });
    int src, dst;
    struct stat fstat;
    { bstr sfile(srcfile);
    if(stat(sfile, &fstat) || (src = open(sfile, O_RDONLY | O_NOATIME)) == -1) return err(); }
    bstr nfile(newfile);
    bool herr;

    if(! (herr = (dst = creat(nfile, fstat.st_mode)) == -1))
    {
        if(fstat.st_size)
        {
            llong size(0);

            do {
                llong csize(size)
#ifdef __i386__
                    , rsize(fstat.st_size - size)
#endif
                    ;
                if((size += sendfile(dst, src, nullptr,
#ifdef __i386__
                    rsize < 2147483648 ? rsize : 2147483647
#else
                    fstat.st_size - size
#endif
                    )) <= csize) herr = true;
            } while(! herr && size < fstat.st_size);
        }

        close(dst);
    }

    close(src);
    if(herr) return err();

    if(skel)
    {
        struct stat ustat;
        if(stat(bstr(left(newfile, instr(newfile, "/", 21) - 1)), &ustat)) return err();
        fstat.st_uid = ustat.st_uid, fstat.st_gid = ustat.st_gid;
    }

    if(fstat.st_uid + fstat.st_gid && (chown(nfile, fstat.st_uid, fstat.st_gid) || (fstat.st_mode != (fstat.st_mode & ~(S_ISUID | S_ISGID)) && chmod(nfile, fstat.st_mode)))) return err();
    utimbuf sftimes;
    sftimes.actime = fstat.st_atim.tv_sec, sftimes.modtime = fstat.st_mtim.tv_sec;
    return utime(nfile, &sftimes) ? err() : true;
}

bool sb::cpdir(cQStr &srcdir, cQStr &newdir)
{
    auto err([&] { return error("\n " % tr("An error occurred while cloning the following directory:") % "\n\n  " % srcdir % "\n\n " % tr("Target directory:") % "\n\n  " % newdir % fdbg(srcdir, newdir), true); });
    struct stat dstat;
    if(stat(bstr(srcdir), &dstat) || ! S_ISDIR(dstat.st_mode)) return err();
    bstr ndir(newdir);
    if(mkdir(ndir, dstat.st_mode) || (dstat.st_uid + dstat.st_gid && (chown(ndir, dstat.st_uid, dstat.st_gid) || (dstat.st_mode != (dstat.st_mode & ~(S_ISUID | S_ISGID)) && chmod(ndir, dstat.st_mode))))) return err();
    utimbuf sdtimes;
    sdtimes.actime = dstat.st_atim.tv_sec, sdtimes.modtime = dstat.st_mtim.tv_sec;
    return utime(ndir, &sdtimes) ? err() : true;
}

void sb::edetect(QSL &elst, bool spath)
{
    QSL mpts;

    {
        QBA mnts(fload("./proc/self/mounts"));
        QTS in(&mnts, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr cline(in.readLine());
            if(cline.contains(" /var/lib/")) mpts.append(cline.split(' ').at(1) % '/');
        }
    }

    if(! mpts.isEmpty())
    {
        if(isfile("./etc/fstab"))
        {
            QFile file("./etc/fstab");

            if(fopen(file))
                while(! file.atEnd())
                {
                    QStr cline(file.readLine().trimmed());

                    if(! cline.startsWith('#'))
                    {
                        cline.replace('\t', ' ');

                        for(uchar a(0) ; a < mpts.count() ; ++a)
                            if(cline.contains(' ' % left(mpts.at(a), -1) % ' '))
                            {
                                if(mpts.count() == 1) return;
                                mpts.removeAt(a);
                                break;
                            }
                    }
                }
        }

        if(spath)
            for(cQStr &mpt : mpts) elst.append(right(mpt, -5));
        else
            elst.append(mpts);
    }
}

bool sb::exclcheck(cQSL &elist, cQStr &item)
{
    for(cQStr &excl : elist)
        if(excl.endsWith('/'))
        {
            if(item.startsWith(excl)) return true;
        }
        else if(excl.endsWith('*'))
        {
            if(item.startsWith(left(excl, -1))) return true;
        }
        else if(like(item, {'_' % excl % '_', '_' % excl % "/*"}))
            return true;

    return false;
}

bool sb::inclcheck(cQSL &ilist, cQStr &item)
{
    for(cQStr &ixcl : ilist)
        if(ixcl.length() >= item.length())
        {
            if(like(ixcl, {'_' % item % '_', '_' % item % "/*"})) return true;
        }
        else if(item.startsWith(ixcl % '/'))
            return true;

    return false;
}

bool sb::lcomp(cQStr &link1, cQStr &link2)
{
    struct stat istat[2];
    if(lstat(bstr(link1), &istat[0]) || lstat(bstr(link2), &istat[1]) || ! (S_ISLNK(istat[0].st_mode) && S_ISLNK(istat[1].st_mode)) || istat[0].st_mtim.tv_sec != istat[1].st_mtim.tv_sec) return false;
    QStr lnk(rlink(link1, istat[0].st_size));
    return ! lnk.isEmpty() && lnk == rlink(link2, istat[1].st_size);
}

bool sb::rodir(QBA &ba, QUCL &ucl, cQStr &path, uchar hidden, cQSL &ilist, uchar oplen)
{
    DIR *dir(opendir(bstr(path)));

    if(dir)
    {
        dirent *ent;
        QStr ppath(ba.isEmpty() ? nullptr : QStr(right(path, -(oplen == 1 ? 1 : oplen + 1)) % '/'));
        QSL dd{"_._", "_.._"};

        while(! ThrdKill && (ent = readdir(dir)))
        {
            QStr iname(ent->d_name), npath(ppath % iname);

            if(! like(iname, dd) && [&, hidden] {
                    switch(hidden) {
                    case False:
                        return true;
                    case True:
                        return like(iname, {"_.*", "_snap_"}) || (! ilist.isEmpty() && inclcheck(ilist, iname));
                    default:
                        return like(npath, {"_.*", "_snap/*"}) || inclcheck(ilist, npath);
                    }
                }())
            {
                uchar type([&]() -> uchar {
                        switch(ent->d_type) {
                        case DT_LNK:
                            return Islink;
                        case DT_DIR:
                            return Isdir;
                        case DT_REG:
                            return Isfile;
                        case DT_UNKNOWN:
                            return stype(path % '/' % iname);
                        default:
                            return Unknown;
                        }
                    }());

                switch(type) {
                case Islink:
                case Isfile:
                    ucl.append(type), ba.append(npath % '\n');
                    break;
                case Isdir:
                    rodir(ba.append(npath % '\n'), ucl << Isdir, path % '/' % iname, hidden == True ? ilist.isEmpty() ? uchar(False) : uchar(Include) : hidden, ilist, (oplen ? oplen : path.length()));
                }
            }
        }

        closedir(dir);
        if(! (ThrdKill || oplen)) ba.squeeze();
    }

    return ! ThrdKill;
}

bool sb::rodir(cQStr &path, QBA &ba, QUCL &ucl, ullong id, uchar oplen)
{
    DIR *dir(opendir(bstr(path)));

    if(dir)
    {
        if(! oplen)
        {
            struct stat dstat;
            if(! stat(bstr(path), &dstat)) id = dstat.st_dev;
        }

        dirent *ent;
        QStr ppath(ba.isEmpty() ? nullptr : QStr(right(path, -(oplen == 1 ? 1 : oplen + 1)) % '/'));
        QSL dd{"_._", "_.._"};

        while(! ThrdKill && (ent = readdir(dir)))
        {
            QStr iname(ent->d_name), npath(ppath % iname);

            if(! like(iname, dd))
            {
                uchar type([&]() -> uchar {
                        switch(ent->d_type) {
                        case DT_LNK:
                            return Islink;
                        case DT_DIR:
                            return Isdir;
                        case DT_REG:
                            return Isfile;
                        case DT_UNKNOWN:
                            return stype(path % '/' % iname);
                        default:
                            return Unknown;
                        }
                    }());

                switch(type) {
                case Islink:
                case Isfile:
                    ucl.append(type), ba.append(npath % '\n');
                    break;
                case Isdir:
                    QStr fpath(path % '/' % iname);
                    struct stat dstat;

                    if(! stat(bstr(fpath), &dstat) && dstat.st_dev == id)
                        rodir(fpath, ba.append(npath % '\n'), ucl << Isdir, id, (oplen ? oplen : path.length()));
                    else
                        ucl.append(Isdir), ba.append(npath % '\n');
                }
            }
        }

        closedir(dir);
        if(! (ThrdKill || oplen)) ba.squeeze();
    }

    return ! ThrdKill;
}

bool sb::rodir(QBA &ba, cQStr &path, uchar oplen)
{
    DIR *dir(opendir(bstr(path)));

    if(dir)
    {
        dirent *ent;
        QStr ppath(ba.isEmpty() ? nullptr : QStr(right(path, -(oplen == 1 ? 1 : oplen + 1)) % '/'));
        QSL dd{"_._", "_.._"};

        while(! ThrdKill && (ent = readdir(dir)))
        {
            QStr iname(ent->d_name);

            if(! like(iname, dd))
                switch([&]() -> uchar {
                        switch(ent->d_type) {
                        case DT_LNK:
                        case DT_REG:
                            return Isfile;
                        case DT_DIR:
                            return Isdir;
                        case DT_UNKNOWN:
                            return stype(path % '/' % iname);
                        default:
                            return Unknown;
                        }
                    }()) {
                case Islink:
                case Isfile:
                    ba.append(ppath % iname % '\n');
                    break;
                case Isdir:
                    rodir(ba.append(ppath % iname % '\n'), path % '/' % iname, (oplen ? oplen : path.length()));
                }
        }

        closedir(dir);
        if(! (ThrdKill || oplen)) ba.squeeze();
    }

    return ! ThrdKill;
}

bool sb::rodir(QUCL &ucl, cQStr &path, uchar oplen)
{
    DIR *dir(opendir(bstr(path)));

    if(dir)
    {
        dirent *ent;
        QSL dd{"_._", "_.._"};

        while(! ThrdKill && (ent = readdir(dir)))
        {
            QStr iname(ent->d_name);

            if(! like(iname, dd))
                switch([&]() -> uchar {
                        switch(ent->d_type) {
                        case DT_LNK:
                        case DT_REG:
                            return Isfile;
                        case DT_DIR:
                            return Isdir;
                        case DT_UNKNOWN:
                            return stype(path % '/' % iname);
                        default:
                            return Unknown;
                        }
                    }()) {
                case Islink:
                case Isfile:
                    ucl.append(0);
                    break;
                case Isdir:
                    rodir(ucl << 0, path % '/' % iname, (oplen ? oplen : path.length()));
                }
        }

        closedir(dir);
    }

    return ! ThrdKill;
}

bool sb::odir(QBAL &balst, cQStr &path, uchar hidden, cQSL &ilist, cQStr &ppath)
{
    DIR *dir(opendir(bstr(path)));

    if(dir)
    {
        balst.reserve(1000);
        dirent *ent;
        QSL dd{"_._", "_.._"};

        while(! ThrdKill && (ent = readdir(dir)))
        {
            QStr iname(ent->d_name);

            if(! like(iname, dd) && [&, hidden] {
                    switch(hidden) {
                    case False:
                        return true;
                    case True:
                        return like(iname, {"_.*", "_snap_"}) || (! ilist.isEmpty() && inclcheck(ilist, iname));
                    default:
                        return inclcheck(ilist, ppath % '/' % iname);
                    }
                }()) balst.append(QBA(ent->d_name));
        }

        closedir(dir);
    }

    return ! ThrdKill;
}

bool sb::recrmdir(cbstr &path, bool slimit)
{
    DIR *dir(opendir(path));

    if(dir)
    {
        auto size([](cbstr &spath) -> ullong {
                struct stat fstat;
                if(stat(spath, &fstat)) return 8000001;
                return fstat.st_size;
            });

        dirent *ent;
        QSL dd{"_._", "_.._"};

        while(! ThrdKill && (ent = readdir(dir)))
        {
            QBA iname(ent->d_name);

            if(! like(iname, dd))
            {
                QBA fpath(QBA(path) % '/' % iname);

                switch(ent->d_type) {
                case DT_UNKNOWN:
                    switch(stype(fpath)) {
                    case Isfile:
                        if(slimit && size(fpath) > 8000000) continue;
                    default:
                        rmfile(fpath);
                        continue;
                    case Isdir:;
                    }
                case DT_DIR:
                    if(! recrmdir(fpath, slimit))
                    {
                        closedir(dir);
                        return false;
                    }

                    break;
                case DT_REG:
                    if(slimit && size(fpath) > 8000000) break;
                default:
                    rmfile(fpath);
                }
            }
        }

        closedir(dir);
    }

    return ! ThrdKill && (! rmdir(path) || slimit || (errno == ENOTEMPTY ? false : error("\n " % tr("An error occurred while deleting the following directory:") % "\n\n  " % QStr(path) % fdbg(QStr(path)), true)));
}

void sb::run()
{
    if(ThrdKill) ThrdKill = false;

    auto psalign([](ullong pstart, ushort ssize) -> ullong {
            if(pstart <= 1048576 / ssize) return 1048576 / ssize;
            ushort rem(pstart % (1048576 / ssize));
            return rem ? pstart + 1048576 / ssize - rem : pstart;
        });

    auto pealign([](ullong end, ushort ssize) -> ullong {
            ushort rem(end % (1048576 / ssize));
            return rem ? rem < (1048576 / ssize) - 1 ? end - rem - 1 : end : end - 1;
        });

    switch(ThrdType) {
    case Remove:
        ThrdRslt = [this] {
                switch(stype(ThrdStr[0])) {
                case Isdir:
                    return recrmdir(ThrdStr[0]);
                default:
                    return rmfile(ThrdStr[0]);
                }
            }();

        break;
    case Copy:
        ThrdRslt = cpfile(ThrdStr[0], ThrdStr[1]);
        break;
    case Sync:
        return sync();
    case Mount:
    {
        libmnt_context *mcxt(mnt_new_context());
        mnt_context_set_source(mcxt, bstr(ThrdStr[0])),
        mnt_context_set_target(mcxt, bstr(ThrdStr[1])),
        mnt_context_set_options(mcxt, ! ThrdStr[2].isEmpty() ? bstr(ThrdStr[2]).data : isdir(ThrdStr[0]) ? "bind" : "noatime"),
        ThrdRslt = ! mnt_context_mount(mcxt);
        return mnt_free_context(mcxt);
    }
    case Umount:
        ThrdRslt = umnt(ThrdStr[0]);
        return;
    case Readprttns:
    {
        ThrdSlst->reserve(25);
        QSL dlst{"_/dev/sd*", "_/dev/hd*", "_/dev/vd*", "_/dev/mmcblk*"};

        for(cQStr &spath : QDir("/dev").entryList(QDir::System))
        {
            QStr path("/dev/" % spath);

            if(like(path.length(), {8, 12}) && like(path, dlst) && devsize(path) > 536870911)
            {
                PedDevice *dev(ped_device_get(bstr(path)));
                PedDisk *dsk(ped_disk_new(dev));

                uchar type(dsk ? [&dsk] {
                        QStr name(dsk->type->name);
                        return name == "gpt" ? GPT : name == "msdos" ? MSDOS : Clear;
                    }() : Clear);

                ThrdSlst->append(path % '\n' % QStr::number(dev->length * dev->sector_size) % '\n' % QStr::number(type));

                if(type != Clear)
                {
                    PedPartition *prt(nullptr);
                    QLIL egeom;

                    while((prt = ped_disk_next_partition(dsk, prt)))
                        if(prt->geom.length >= 1048576 / dev->sector_size)
                        {
                            if(prt->num > 0)
                            {
                                QStr ppath(path % (path.length() == 12 ? "p" : nullptr) % QStr::number(prt->num));

                                if(stype(ppath) == Isblock)
                                {
                                    if(prt->type == PED_PARTITION_EXTENDED)
                                        ThrdSlst->append(ppath % '\n' % QStr::number(prt->geom.length * dev->sector_size) % '\n' % QStr::number(Extended) % '\n' % QStr::number(prt->geom.start * dev->sector_size)),
                                        egeom.append({prt->geom.start, prt->geom.end});
                                    else
                                    {
                                        if(egeom.count() > 2)
                                        {
                                            ullong pstart(psalign(egeom.at(2), dev->sector_size));
                                            ThrdSlst->append(path % "?\n" % QStr::number((pealign(egeom.at(3), dev->sector_size) - pstart + 1) * dev->sector_size - (prt->type == PED_PARTITION_LOGICAL ? 2097152 : 1048576 - dev->sector_size)) % '\n' % QStr::number(Emptyspace) % '\n' % QStr::number(pstart * dev->sector_size + 1048576));
                                            for(uchar a(3) ; a > 1 ; --a) egeom.removeAt(a);
                                        }

                                        blkid_probe pr(blkid_new_probe_from_filename(bstr(ppath)));
                                        blkid_do_probe(pr);
                                        cchar *uuid(nullptr), *fstype("?"), *label(nullptr);

                                        if(! blkid_probe_lookup_value(pr, "UUID", &uuid, nullptr)) blkid_probe_lookup_value(pr, "TYPE", &fstype, nullptr),
                                                                                                   blkid_probe_lookup_value(pr, "LABEL", &label, nullptr);

                                        ThrdSlst->append(ppath % '\n' % QStr::number(prt->geom.length * dev->sector_size) % '\n' % QStr::number(prt->type == PED_PARTITION_LOGICAL ? Logical : Primary) % '\n' % QStr::number(prt->geom.start * dev->sector_size) % '\n' % fstype % '\n' % label % '\n' % uuid),
                                        blkid_free_probe(pr);
                                    }
                                }
                            }
                            else if(prt->type == PED_PARTITION_FREESPACE)
                            {
                                if(egeom.count() > 2)
                                {
                                    ullong pstart(psalign(egeom.at(2), dev->sector_size));
                                    ThrdSlst->append(path % "?\n" % QStr::number((pealign(egeom.at(3), dev->sector_size) - pstart + 1) * dev->sector_size - 1048576) % '\n' % QStr::number(Emptyspace) % '\n' % QStr::number(pstart * dev->sector_size + 1048576)),
                                    egeom.clear();
                                }

                                llong fgeom[]{llong(prt->geom.start < 1048576 / dev->sector_size ? 1048576 / dev->sector_size : psalign(prt->geom.start, dev->sector_size)), llong(prt->next && prt->next->type == PED_PARTITION_METADATA ? type == MSDOS ? prt->next->geom.end : prt->next->geom.end - (34816 / dev->sector_size * 10 + 5) / 10 : pealign(prt->geom.end, dev->sector_size))};
                                if(fgeom[1] - fgeom[0] > 1048576 / dev->sector_size - 2) ThrdSlst->append(path % "?\n" % QStr::number((fgeom[1] - fgeom[0] + 1) * dev->sector_size) % '\n' % QStr::number(Freespace) % '\n' % QStr::number(fgeom[0] * dev->sector_size));
                            }
                            else if(! egeom.isEmpty())
                            {
                                if(prt->geom.end <= egeom.at(1))
                                    switch(egeom.count()) {
                                    case 2:
                                        if(prt->geom.length >= 3145728 / dev->sector_size) egeom.append({(prt->geom.start - egeom.at(0) < 1048576 / dev->sector_size ? egeom.at(0) : prt->geom.start), prt->geom.end + (prt->geom.end == egeom.at(1) ? 1 : 0)});
                                        break;
                                    default:
                                        egeom.replace(3, prt->geom.end + (prt->geom.end == egeom.at(1) ? 1 : 0));
                                    }
                                else
                                    egeom.clear();
                            }
                        }

                    if(egeom.count() > 2)
                    {
                        ullong pstart(psalign(egeom.at(2), dev->sector_size));
                        ThrdSlst->append(path % "?\n" % QStr::number((pealign(egeom.at(3), dev->sector_size) - pstart + 1) * dev->sector_size - 1048576) % '\n' % QStr::number(Emptyspace) % '\n' % QStr::number(pstart * dev->sector_size + 1048576));
                    }
                }
                else if(! dsk)
                    goto next_1;

                ped_disk_destroy(dsk);
            next_1:
                ped_device_destroy(dev);
            }
        }

        break;
    }
    case Readlvdevs:
    {
        ThrdSlst->reserve(10);
        QBA fstab(fload("/etc/fstab"));
        QSL dlst[]{{"_usb-*", "_mmc-*"}, {"_/dev/sd*", "_/dev/mmcblk*"}};

        for(cQStr &item : QDir("/dev/disk/by-id").entryList(QDir::Files))
        {
            if(like(item, dlst[0]) && islink("/dev/disk/by-id/" % item))
            {
                QStr path(rlink("/dev/disk/by-id/" % item, 14));

                if(! path.isEmpty() && like((path = "/dev" % right(path, -5)).length(), {8, 12}) && like(path, dlst[1]))
                {
                    ullong size(devsize(path));

                    if(size > 536870911)
                    {
                        if(! fstab.isEmpty())
                        {
                            QSL fchk('_' % path % '*');

                            {
                                PedDevice *dev(ped_device_get(bstr(path)));
                                PedDisk *dsk(ped_disk_new(dev));

                                if(dsk)
                                {
                                    PedPartition *prt(nullptr);

                                    while((prt = ped_disk_next_partition(dsk, prt)))
                                        if(prt->num > 0 && prt->type != PED_PARTITION_EXTENDED)
                                        {
                                            QStr ppath(path % (path.length() == 12 ? "p" : nullptr) % QStr::number(prt->num));

                                            if(stype(ppath) == Isblock)
                                            {
                                                blkid_probe pr(blkid_new_probe_from_filename(bstr(ppath)));
                                                blkid_do_probe(pr);
                                                cchar *uuid(nullptr);
                                                if(! blkid_probe_lookup_value(pr, "UUID", &uuid, nullptr) && uuid) fchk.append("_UUID=" % QStr(uuid) % '*');
                                                blkid_free_probe(pr);
                                            }
                                        }

                                    ped_disk_destroy(dsk);
                                }

                                ped_device_destroy(dev);
                            }

                            QTS in(&fstab, QIODevice::ReadOnly);

                            while(! in.atEnd())
                                if(like(in.readLine().trimmed(), fchk)) goto next_2;
                        }

                        ThrdSlst->append(path % '\n' % mid(item, 5, rinstr(item, "_") - 5).replace('_', ' ') % '\n' % QStr::number(size));
                    }
                }
            }

        next_2:;
        }

        return ThrdSlst->sort();
    }
    case Ruuid:
    {
        blkid_probe pr(blkid_new_probe_from_filename(bstr(ThrdStr[0])));
        blkid_do_probe(pr);
        cchar *uuid(nullptr);
        blkid_probe_lookup_value(pr, "UUID", &uuid, nullptr),
        ThrdStr[1] = uuid;
        return blkid_free_probe(pr);
    }
    case Setpflag:
    {
        PedDevice *dev(ped_device_get(bstr(left(ThrdStr[0], (ThrdStr[0].contains("mmc") ? 12 : 8)))));
        PedDisk *dsk(ped_disk_new(dev));
        PedPartition *prt(nullptr);
        if(ThrdRslt) ThrdRslt = false;

        while(! ThrdKill && (prt = ped_disk_next_partition(dsk, prt)))
            if(QStr(ped_partition_get_path(prt)) == ThrdStr[0])
            {
                for(cQStr &flag : ThrdStr[1].split(' '))
                    if(! ped_partition_set_flag(prt, ped_partition_flag_get_by_name(bstr(flag)), 1)) goto err;

                if(ped_disk_commit_to_dev(dsk)) ThrdRslt = true;
            err:
                ped_disk_commit_to_os(dsk);
                break;
            }

        return ped_disk_destroy(dsk), ped_device_destroy(dev);
    }
    case Mkptable:
    {
        PedDevice *dev(ped_device_get(bstr(ThrdStr[0])));
        PedDisk *dsk(ped_disk_new_fresh(dev, ped_disk_type_get(bstr(ThrdStr[1]))));
        ThrdRslt = ped_disk_commit_to_dev(dsk);
        return ped_disk_commit_to_os(dsk), ped_disk_destroy(dsk), ped_device_destroy(dev);
    }
    case Mkpart:
    {
        PedDevice *dev(ped_device_get(bstr(ThrdStr[0])));
        PedDisk *dsk(ped_disk_new(dev));
        PedPartition *prt(nullptr);
        if(ThrdRslt) ThrdRslt = false;

        if(ThrdLng[0] && ThrdLng[1])
        {
            PedPartition *crtprt(ped_partition_new(dsk, ThrdChr == Primary ? PED_PARTITION_NORMAL : ThrdChr == Extended ? PED_PARTITION_EXTENDED : PED_PARTITION_LOGICAL, ped_file_system_type_get("ext2"), psalign(ThrdLng[0] / dev->sector_size, dev->sector_size), ullong(dev->length - 1048576 / dev->sector_size) >= (ThrdLng[0] + ThrdLng[1]) / dev->sector_size - 1 ? pealign((ThrdLng[0] + ThrdLng[1]) / dev->sector_size - 1, dev->sector_size) : (ThrdLng[0] + ThrdLng[1]) / dev->sector_size - 1));
            if(ped_disk_add_partition(dsk, crtprt, ped_constraint_exact(&crtprt->geom)) && ped_disk_commit_to_dev(dsk)) ThrdRslt = true;
            ped_disk_commit_to_os(dsk);
        }
        else
            while(! ThrdKill && (prt = ped_disk_next_partition(dsk, prt)))
                if(prt->type == PED_PARTITION_FREESPACE && prt->geom.length >= 1048576 / dev->sector_size)
                {
                    PedPartition *crtprt(ped_partition_new(dsk, PED_PARTITION_NORMAL, ped_file_system_type_get("ext2"), ThrdLng[0] ? psalign(ThrdLng[0] / dev->sector_size, dev->sector_size) : psalign(prt->geom.start, dev->sector_size), ThrdLng[1] ? pealign((ThrdLng[0] + ThrdLng[1]) / dev->sector_size - 1, dev->sector_size) : prt->next && prt->next->type == PED_PARTITION_METADATA ? prt->next->geom.end : pealign(prt->geom.end, dev->sector_size)));
                    if(ped_disk_add_partition(dsk, crtprt, ped_constraint_exact(&crtprt->geom)) && ped_disk_commit_to_dev(dsk)) ThrdRslt = true;
                    ped_disk_commit_to_os(dsk);
                    break;
                }

        return ped_disk_destroy(dsk), ped_device_destroy(dev);
    }
    case Delpart:
    {
        bool ismmc(ThrdStr[0].contains("mmc"));
        PedDevice *dev(ped_device_get(bstr(left(ThrdStr[0], ismmc ? 12 : 8))));
        PedDisk *dsk(ped_disk_new(dev));
        if(ped_disk_delete_partition(dsk, ped_disk_get_partition(dsk, right(ThrdStr[0], -(ismmc ? 13 : 8)).toUShort()))) ped_disk_commit_to_dev(dsk), ped_disk_commit_to_os(dsk);
        return ped_disk_destroy(dsk), ped_device_destroy(dev);
    }
    case Crtrpoint:
        ThrdRslt = thrdcrtrpoint(ThrdStr[0]);
        break;
    case Srestore:
        ThrdRslt = thrdsrestore(ThrdChr, ThrdStr[0], ThrdStr[1], ThrdStr[2], ThrdBool);
        break;
    case Scopy:
        ThrdRslt = thrdscopy(ThrdChr, ThrdStr[0], ThrdStr[1]);
        break;
    case Lvprpr:
        ThrdRslt = thrdlvprpr(ThrdBool);
    }
}

bool sb::umnt(cbstr &dev)
{
    libmnt_context *ucxt(mnt_new_context());
    mnt_context_set_target(ucxt, dev),
    mnt_context_enable_force(ucxt, true),
    mnt_context_enable_lazy(ucxt, true);
#ifndef C_MNT_LIB
    mnt_context_enable_loopdel(ucxt, true);
#endif
    bool rv(! mnt_context_umount(ucxt));
    mnt_free_context(ucxt);
    return rv;
}

bool sb::thrdcrtrpoint(cQStr &trgt)
{
    QStr rsdir('.' % QDir(sdir[1]).canonicalPath());
    if(chroot(bstr(sdir[1]))) return false;

    auto out([](bool val = false) {
            chroot(bstr("./"));
            return val;
        });

    QStr rtrgt(rsdir % trgt);
    uint lcnt;

    {
        QBA sysitms[13];
        QUCL sysitmst[13];

        {
            QStr dirs[]{"./bin", "./boot", "./etc", "./lib", "./lib32", "./lib64", "./opt", "./sbin", "./selinux", "./srv", "./usr", "./var"};
            uint bbs[]{10000, 20000, 100000, 500000, 10000, 10000, 100000, 10000, 10000, 10000, 10000000, 1000000}, ibs[]{500, 1000, 10000, 20000, 500, 500, 5000, 1000, 500, 500, 500000, 50000};

            for(uchar a(0) ; a < 12 ; ++a)
                if(isdir(dirs[a]))
                {
                    sysitms[a].reserve(bbs[a]), sysitmst[a].reserve(ibs[a]);
                    if(! rodir(sysitms[a], sysitmst[a], dirs[a])) return out();
                }
        }

        if(isdir("./snap"))
        {
            sysitms[12].reserve(10000), sysitmst[12].reserve(500);
            if(! rodir("./snap", sysitms[12], sysitmst[12])) return out();
        }

        uint anum(0), cnum(0);
        for(uchar a(0) ; a < 12 ; ++a) anum += sysitmst[a].count();
        QSL rplst;
        QBA *cditms;
        QUCL *cditmst;
        uchar cperc;

        {
            QSL usrs;
            QBAL homeitms;
            QUCLL homeitmst;

            {
                QFile file("./etc/passwd");
                if(! fopen(file)) return out();

                while(! file.atEnd())
                {
                    QStr usr(file.readLine().trimmed());

                    if(usr.contains(":/home/") && isdir("./home/" % (usr = left(usr, instr(usr, ":") -1)))) usrs.append(usr),
                                                                                                            homeitms.append(nullptr),
                                                                                                            homeitmst.append(QUCL());
                }
            }

            {
                QSL ilst;

                {
                    QFile file("." incfile);
                    if(! fopen(file)) return out();

                    while(! file.atEnd())
                    {
                        QStr cline(left(file.readLine(), -1));
                        if(! cline.isEmpty()) ilst.append(cline);
                        if(ThrdKill) return out();
                    }
                }

                if(! (usrs << (isdir("./root") ? "" : nullptr)).last().isNull())
                {
                    homeitms.append(nullptr), homeitms.last().reserve(50000),
                    homeitmst.append(QUCL()), homeitmst.last().reserve(1000);
                    if(! rodir(homeitms.last(), homeitmst.last(), "./root", True, ilst)) return out();
                }

                for(schar a(usrs.count() - 2) ; a > -1 ; --a)
                {
                    homeitms[a].reserve(5000000), homeitmst[a].reserve(100000);
                    if(! rodir(homeitms[a], homeitmst[a], "./home/" % usrs.at(a), True, ilst)) return out();
                }
            }

            for(cQUCL &cucl : homeitmst) anum += cucl.count();
            Progress = 0;
            if(! crtdir(rtrgt) || (isdir("./home") && ! crtdir(rtrgt % "/home"))) return out();

            if(incrmtl)
            {
                rplst.reserve(15);
                QSL incl{"_S01_*", "_S02_*", "_S03_*", "_S04_*", "_S05_*", "_S06_*", "_S07_*", "_S08_*", "_S09_*", "_S10_*", "_H01_*", "_H02_*", "_H03_*", "_H04_*", "_H05_*"};

                for(cQStr &item : QDir("/").entryList(QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot))
                {
                    if(like(item, incl)) rplst.append(item);
                    if(ThrdKill) return out();
                }
            }

            QSL elst{".sbuserdata", ".cache/gvfs", ".local/share/Trash/files/", ".local/share/Trash/info/", ".Xauthority", ".ICEauthority"};

            {
                QFile file("." excfile);
                if(! fopen(file)) return out();

                while(! file.atEnd())
                {
                    QStr cline(left(file.readLine(), -1));
                    if(like(cline, {"_.*", "_snap_", "_snap/*"})) elst.append(cline);
                    if(ThrdKill) return out();
                }
            }

            QSL excl{"_lost+found_", "_lost+found/*", "*/lost+found_", "*/lost+found/*", "_Systemback_", "_Systemback/*", "*/Systemback_", "*/Systemback/*", "*~_", "*~/*"};

            for(uchar a(0) ; a < usrs.count() ; ++a)
            {
                cQStr &usr(usrs.at(a));

                if(! usr.isNull())
                {
                    QStr srcd(usr.isEmpty() ? QStr("/root") : "/home/" % usr);
                    if(! crtdir(rtrgt % srcd)) return out();

                    if(! (cditmst = &homeitmst[a])->isEmpty())
                    {
                        lcnt = 0;
                        QTS in(cditms = &homeitms[a], QIODevice::ReadOnly);

                        while(! in.atEnd())
                        {
                            if(Progress < (cperc = (++cnum * 100 + 50) / anum)) Progress = cperc;
                            QStr item(in.readLine());

                            if(! (like(item, excl) || (like(item, {"_.*", "_snap_", "_snap/*"}) && exclcheck(elst, item))))
                            {
                                QStr srci(srcd % '/' % item), rsrci('.' % srci);

                                if(exist(rsrci))
                                {
                                    QStr nrpi(rtrgt % srci);

                                    switch(cditmst->at(lcnt)) {
                                    case Islink:
                                        for(cQStr &pname : rplst)
                                        {
                                            QStr orpi(rsdir % '/' % pname % srci);

                                            if(stype(orpi) == Islink && lcomp(rsrci, orpi))
                                            {
                                                if(! crthlnk(orpi, nrpi)) return out();
                                                goto nitem_1;
                                            }

                                            if(ThrdKill) return out();
                                        }

                                        if(! cplink(rsrci, nrpi)) return out();
                                        break;
                                    case Isdir:
                                        if(! crtdir(nrpi)) return out();
                                        break;
                                    case Isfile:
                                        if(fsize(rsrci) <= 8000000)
                                        {
                                            for(cQStr &pname : rplst)
                                            {
                                                QStr orpi(rsdir % '/' % pname % srci);

                                                if(stype(orpi) == Isfile && fcomp(rsrci, orpi) == 2)
                                                {
                                                    if(! crthlnk(orpi, nrpi)) return out();
                                                    goto nitem_1;
                                                }

                                                if(ThrdKill) return out();
                                            }

                                            if(! cpfile(rsrci, nrpi)) return out();
                                        }
                                    }
                                }
                            }

                        nitem_1:
                            if(ThrdKill) return out();
                            ++lcnt;
                        }

                        in.seek(0), lcnt = 0;

                        while(! in.atEnd())
                        {
                            QStr item(in.readLine());

                            if(cditmst->at(lcnt++) == Isdir)
                            {
                                QStr srci(srcd % '/' % item), nrpi(rtrgt % srci);
                                if(exist(nrpi) && ! cpertime('.' % srci, nrpi)) return out();
                            }

                            if(ThrdKill) return out();
                        }

                        cditms->clear(), cditmst->clear();
                    }

                    if(! cpertime('.' % srcd, rtrgt % srcd)) return out();
                }
            }
        }

        if(isdir(rtrgt % "/home") && ! cpertime("./home", rtrgt % "/home")) return out();

        {
            QSL incl{"_initrd.img_", "_initrd.img.old_", "_vmlinuz_", "_vmlinuz.old_"};

            for(cQStr &item : QDir("./").entryList(QDir::Files))
            {
                if(like(item, incl))
                {
                    QStr srci('/' % item), rsrci('.' % srci);

                    if(islink(rsrci))
                    {
                        QStr nrpi(rtrgt % srci);

                        for(cQStr &pname : rplst)
                        {
                            QStr orpi(rsdir % '/' % pname % srci);

                            if(stype(orpi) == Islink && lcomp(rsrci, orpi))
                            {
                                if(! crthlnk(orpi, nrpi)) return out();
                                goto nitem_2;
                            }

                            if(ThrdKill) return out();
                        }

                        if(! cplink(rsrci, nrpi)) return out();
                    }
                }

            nitem_2:
                if(ThrdKill) return out();
            }
        }

        for(cQStr &cdir : {"/cdrom", "/dev", "/mnt", "/proc", "/run", "/sys", "/tmp"})
        {
            QStr rcdir('.' % cdir);
            if(isdir(rcdir) && ! cpdir(rcdir, rtrgt % cdir)) return out();
            if(ThrdKill) return out();
        }

        QSL elst{"/etc/mtab", "/snap/.sblvtmp", "/var/.sblvtmp", "/var/cache/fontconfig/", "/var/lib/dpkg/lock", "/var/lib/udisks2/", "/var/lib/ureadahead/", "/var/log/", "/var/run/", "/var/tmp/"}, dlst{"/bin", "/boot", "/etc", "/lib", "/lib32", "/lib64", "/opt", "/sbin", "/selinux", "/srv", "/usr", "/var", "/snap"}, excl[]{{"_lost+found_", "_lost+found/*", "*/lost+found_", "*/lost+found/*", "_Systemback_", "_Systemback/*", "*/Systemback_", "*/Systemback/*", "*.dpkg-old_", "*~_", "*~/*"}, {"+_/var/cache/apt/*", "-*.bin_", "-*.bin.*"}, {"_/var/cache/apt/archives/*", "*.deb_"}};
        edetect(elst);

        for(uchar a(0) ; a < dlst.count() ; ++a)
        {
            cQStr &cdir(dlst.at(a)), rcdir('.' % cdir);

            if(isdir(rcdir))
            {
                if(! crtdir(rtrgt % cdir)) return out();

                if(! (cditmst = &sysitmst[a])->isEmpty())
                {
                    lcnt = 0;
                    QTS in(cditms = &sysitms[a], QIODevice::ReadOnly);

                    while(! in.atEnd())
                    {
                        if(Progress < (cperc = (++cnum * 100 + 50) / anum)) Progress = cperc;
                        QStr item(in.readLine());

                        if(! like(item, excl[0]))
                        {
                            QStr srci(cdir % '/' % item), rsrci;

                            if(! (like(srci, excl[1], Mixed) || like(srci, excl[2], All) || exclcheck(elst, srci)) && exist(rsrci = '.' % srci))
                            {
                                QStr nrpi(rtrgt % srci);

                                switch(cditmst->at(lcnt)) {
                                case Islink:
                                    for(cQStr &pname : rplst)
                                    {
                                        QStr orpi(rsdir % '/' % pname % srci);

                                        if(stype(orpi) == Islink && lcomp(rsrci, orpi))
                                        {
                                            if(! crthlnk(orpi, nrpi)) return out();
                                            goto nitem_3;
                                        }

                                        if(ThrdKill) return out();
                                    }

                                    if(! cplink(rsrci, nrpi)) return out();
                                    break;
                                case Isdir:
                                    if(! crtdir(nrpi)) return out();
                                    break;
                                case Isfile:
                                    for(cQStr &pname : rplst)
                                    {
                                        QStr orpi(rsdir % '/' % pname % srci);

                                        if(stype(orpi) == Isfile && fcomp(rsrci, orpi) == 2)
                                        {
                                            if(! crthlnk(orpi, nrpi)) return out();
                                            goto nitem_3;
                                        }

                                        if(ThrdKill) return out();
                                    }

                                    if(! cpfile(rsrci, nrpi)) return out();
                                }
                            }
                        }

                    nitem_3:
                        if(ThrdKill) return out();
                        ++lcnt;
                    }

                    in.seek(0), lcnt = 0;

                    while(! in.atEnd())
                    {
                        QStr item(in.readLine());

                        if(cditmst->at(lcnt++) == Isdir)
                        {
                            QStr srci(cdir % '/' % item), nrpi(rtrgt % srci);
                            if(exist(nrpi) && ! cpertime('.' % srci, nrpi)) return out();
                        }

                        if(ThrdKill) return out();
                    }

                    cditms->clear(), cditmst->clear();
                }

                if(! cpertime(rcdir, rtrgt % cdir)) return out();
            }
        }
    }

    if(isdir("./media"))
    {
        if(! crtdir(rtrgt % "/media")) return out();

        if(isfile("./etc/fstab"))
        {
            QSL dlst(QDir("./media").entryList(QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot));
            QFile file("./etc/fstab");
            if(! fopen(file)) return out();

            for(uchar a(0) ; a < dlst.count() ; ++a)
            {
                cQStr &item(dlst.at(a));
                if(a && ! fopen(file)) return out();
                QSL incl{"* /media/" % item % " *", "* /media/" % item % "/*"};

                while(! file.atEnd())
                {
                    QStr cline(file.readLine().trimmed()), fdir;

                    if(! cline.startsWith('#') && like(cline.replace('\t', ' ').contains("\\040") ? QStr(cline).replace("\\040", " ") : cline, incl))
                        for(cQStr cdname : mid(cline, instr(cline, "/media/") + 7, instr(cline, " ", instr(cline, "/media/")) - instr(cline, "/media/") - 7).split('/'))
                            if(! cdname.isEmpty())
                            {
                                QStr nrpi(rtrgt % "/media" % fdir.append('/' % (cdname.contains("\\040") ? QStr(cdname).replace("\\040", " ") : cdname)));
                                if(! (isdir(nrpi) || cpdir("./media" % fdir, nrpi))) return out();
                            }

                    if(ThrdKill) return out();
                }

                file.close();
            }
        }

        if(! cpertime("./media", rtrgt % "/media")) return out();
    }

    if(isdir("./var/log"))
    {
        QBA logitms;
        QUCL logitmst;
        logitms.reserve(20000), logitmst.reserve(1000);
        if(! rodir(logitms, logitmst, "./var/log")) return out();

        if(! logitmst.isEmpty())
        {
            QSL excl{"*.gz_", "*.old_"};
            lcnt = 0;
            QTS in(&logitms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr item(in.readLine());

                switch(logitmst.at(lcnt++)) {
                case Isdir:
                    if(! crtdir(rtrgt % "/var/log/" % item)) return out();
                    break;
                case Isfile:
                    if(! (like(item, excl) || (item.contains('.') && isnum(right(item, -rinstr(item, "."))))))
                    {
                        QStr srci("/var/log/" % item), nrpi(rtrgt % srci);
                        crtfile(nrpi);
                        if(! cpertime('.' % srci, nrpi)) return out();
                    }
                }

                if(ThrdKill) return out();
            }

            in.seek(0), lcnt = 0;

            while(! in.atEnd())
            {
                QStr item(in.readLine());

                if(logitmst.at(lcnt++) == Isdir)
                {
                    QStr srci("/var/log/" % item), nrpi(rtrgt % srci);
                    if(exist(nrpi) && ! cpertime('.' % srci, nrpi)) return out();
                }

                if(ThrdKill) return out();
            }
        }

        if(! (cpertime("./var/log", rtrgt % "/var/log") && cpertime("./var", rtrgt % "/var"))) return out();
    }

    return out(true);
}

bool sb::thrdsrestore(uchar mthd, cQStr &usr, cQStr &srcdir, cQStr &trgt, bool sfstab)
{
    QSL usrs, ilst;
    QBAL homeitms;
    QUCLL homeitmst;
    uint anum(0);

    if(mthd != 2)
    {
        if(! like(mthd, {4, 6}))
        {
            if(isdir(srcdir % "/home"))
                for(cQStr &cusr : QDir(srcdir % "/home").entryList(QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot)) usrs.append(cusr),
                                                                                                                          homeitms.append(nullptr),
                                                                                                                          homeitmst.append(QUCL());

            usrs.append(isdir(srcdir % "/root") ? "" : nullptr);
        }
        else if(usr == "root")
            usrs.append(isdir(srcdir % "/root") ? "" : nullptr);
        else if(isdir(srcdir % "/home/" % usr))
            usrs = QSL{usr, nullptr}, homeitms.append(nullptr), homeitmst.append(QUCL());
        else
            usrs.append(nullptr);

        if(isfile(srcdir % incfile))
        {
            QFile file(srcdir % incfile);
            if(! fopen(file)) return false;

            while(! file.atEnd())
            {
                QStr cline(left(file.readLine(), -1));
                if(! cline.isEmpty()) ilst.append(cline);
                if(ThrdKill) return false;
            }
        }

        if(! usrs.last().isNull())
        {
            homeitms.append(nullptr), homeitms.last().reserve(50000),
            homeitmst.append(QUCL()), homeitmst.last().reserve(1000);
            if(! rodir(homeitms.last(), homeitmst.last(), srcdir % "/root", True, ilst)) return false;
        }

        for(schar a(usrs.count() - 2) ; a > -1 ; --a)
        {
            homeitms[a].reserve(5000000), homeitmst[a].reserve(100000);
            if(! rodir(homeitms[a], homeitmst[a], srcdir % "/home/" % usrs.at(a), True, ilst)) return false;
        }

        for(cQUCL &cucl : homeitmst) anum += cucl.count();
    }

    QBA *cditms;
    QUCL *cditmst;
    uint cnum(0), lcnt;
    uchar cperc;
    auto fspchk([&trgt] { return dfree(trgt.isEmpty() ? "/" : trgt) > 10485760; });

    if(mthd < 3)
    {
        {
            QBA sysitms[13];
            QUCL sysitmst[13];

            {
                QStr dirs[]{srcdir % "/bin", srcdir % "/boot", srcdir % "/etc", srcdir % "/lib", srcdir % "/lib32", srcdir % "/lib64", srcdir % "/opt", srcdir % "/sbin", srcdir % "/selinux", srcdir % "/snap", srcdir % "/srv", srcdir % "/usr", srcdir % "/var"};
                uint bbs[]{10000, 20000, 100000, 500000, 10000, 10000, 100000, 10000, 10000, 10000, 10000, 10000000, 1000000}, ibs[]{500, 1000, 10000, 20000, 500, 500, 5000, 1000, 500, 500, 500, 500000, 50000};

                for(uchar a(0) ; a < 13 ; ++a)
                    if(isdir(dirs[a]))
                    {
                        sysitms[a].reserve(bbs[a]), sysitmst[a].reserve(ibs[a]);
                        if(! rodir(sysitms[a], sysitmst[a], dirs[a])) return false;
                    }
            }

            for(uchar a(0) ; a < 12 ; ++a) anum += sysitmst[a].count();
            Progress = 0;

            {
                QSL incl{"_initrd.img_", "_initrd.img.old_", "_vmlinuz_", "_vmlinuz.old_"};

                for(cQStr &item : QDir(trgt.isEmpty() ? "/" : trgt).entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Files | QDir::System))
                {
                    if(like(item, incl))
                    {
                        QStr trgi(trgt % '/' % item);

                        switch(stype(trgi)) {
                        case Islink:
                        {
                            QStr srci(srcdir % '/' % item);
                            if(exist(srci) && lcomp(srci, trgi)) break;
                        }
                        case Isfile:
                            rmfile(trgi);
                            break;
                        case Isdir:
                            recrmdir(trgi);
                        }
                    }

                    if(ThrdKill) return false;
                }

                for(cQStr &item : QDir(srcdir).entryList(QDir::Files | QDir::System))
                {
                    if(like(item, incl))
                    {
                        QStr trgi(trgt % '/' % item);
                        if(! exist(trgi)) cplink(srcdir % '/' % item, trgi);
                    }

                    if(ThrdKill) return false;
                }
            }

            for(cQStr &cdir : {"/cdrom", "/dev", "/home", "/mnt", "/root", "/proc", "/run", "/sys", "/tmp"})
            {
                QStr srci(srcdir % cdir), trgi(trgt % cdir);

                if(! isdir(srci))
                {
                    if(exist(trgi)) stype(trgi) == Isdir ? recrmdir(trgi) : rmfile(trgi);
                }
                else if(isdir(trgi))
                    cpertime(srci, trgi);
                else
                {
                    if(exist(trgi)) rmfile(trgi);
                    if(! (cpdir(srci, trgi) || fspchk())) return false;
                }

                if(ThrdKill) return false;
            }

            QSL elst;
            if(trgt.isEmpty()) elst = QSL{"/etc/mtab", "/var/cache/fontconfig/", "/var/lib/dpkg/lock", "/var/lib/udisks2/", "/var/run/", "/var/tmp/"};
            if(trgt.isEmpty() || (isfile("/mnt/etc/xdg/autostart/sbschedule.desktop") && isfile("/mnt/etc/xdg/autostart/sbschedule-kde.desktop") && isfile("/mnt/usr/bin/systemback") && isfile("/mnt/usr/lib/systemback/libsystemback.so.1.0.0") && isfile("/mnt/usr/lib/systemback/sbscheduler") && isfile("/mnt/usr/lib/systemback/sbsustart") && isfile("/mnt/usr/lib/systemback/sbsysupgrade")&& isdir("/mnt/usr/share/systemback/lang") && isfile("/mnt/usr/share/systemback/efi.tar.gz") && isfile("/mnt/usr/share/systemback/splash.png") && isfile("/mnt/var/lib/dpkg/info/systemback.list") && isfile("/mnt/var/lib/dpkg/info/systemback.md5sums"))) elst.append({"/etc/systemback", "/etc/xdg/autostart/sbschedule*", "/usr/bin/systemback*", "/usr/lib/systemback/", "/usr/share/systemback/", "/var/lib/dpkg/info/systemback*"});
            if(sfstab) elst.append("/etc/fstab");
            edetect(elst);
            QSL dlst{"/bin", "/boot", "/etc", "/lib", "/lib32", "/lib64", "/opt", "/sbin", "/selinux", "/snap", "/srv", "/usr", "/var"}, excl[]{{"_lost+found_", "_Systemback_"}, {"_lost+found_", "_lost+found/*", "*/lost+found_", "*/lost+found/*", "_Systemback_", "_Systemback/*", "*/Systemback_", "*/Systemback/*"}};
            QStr mnts;

            auto mchk([&](cQStr &dir) {
                    if(mcheck(dir % '/', mnts.isEmpty() ? mnts = fload("/proc/self/mounts", true) : mnts))
                    {
                        cQStr &mpt(dir.contains(' ') ? bstr(dir).rplc(" ", "\\040") : dir);
                        QTS in(&mnts, QIODevice::ReadOnly);

                        while(! in.atEnd())
                        {
                            QStr cline(in.readLine());
                            if(like(cline, {"* " % mpt % " *", "* " % mpt % "/*"})) umnt(cline.split(' ').value(1).replace("\\040", " "));
                        }
                    }
                });

            for(uchar a(0) ; a < dlst.count() ; ++a)
            {
                cQStr &cdir(dlst.at(a));
                QStr srcd(srcdir % cdir), trgd(trgt % cdir);

                if(isdir(srcd))
                {
                    if(isdir(trgd))
                    {
                        QBAL sdlst;
                        if(! odir(sdlst, trgd)) return false;

                        for(cQStr &item : sdlst)
                        {
                            if(! (like(item, excl[0]) || exclcheck(elst, cdir % '/' % item) || exist(srcd % '/' % item)))
                            {
                                QStr trgi(trgd % '/' % item);
                                if(a == 9) mchk(trgi);
                                stype(trgi) == Isdir ? recrmdir(trgi) : rmfile(trgi);
                            }

                            if(ThrdKill) return false;
                        }
                    }
                    else
                    {
                        if(exist(trgd)) rmfile(trgd);
                        if(! (crtdir(trgd) || fspchk())) return false;
                    }

                    if(! (cditmst = &sysitmst[a])->isEmpty())
                    {
                        lcnt = 0;
                        QTS in(cditms = &sysitms[a], QIODevice::ReadOnly);

                        while(! in.atEnd())
                        {
                            if(Progress < (cperc = (++cnum * 100 + 50) / anum)) Progress = cperc;
                            QStr item(in.readLine());

                            if(! (like(item, excl[1]) || exclcheck(elst, cdir % '/' % item)))
                            {
                                QStr srci(srcd % '/' % item), trgi(trgd % '/' % item);

                                switch(cditmst->at(lcnt)) {
                                case Islink:
                                    switch(stype(trgi)) {
                                    case Islink:
                                        if(lcomp(srci, trgi)) goto nitem_1;
                                    case Isfile:
                                        rmfile(trgi);
                                        break;
                                    case Isdir:
                                        if(a == 9) mchk(trgi);
                                        recrmdir(trgi);
                                    }

                                    if(! (cplink(srci, trgi) || fspchk())) return false;
                                    break;
                                case Isdir:
                                    switch(stype(trgi)) {
                                    case Isdir:
                                    {
                                        if(a == 9) mchk(trgi);
                                        QBAL sdlst;
                                        if(! odir(sdlst, trgi)) return false;

                                        for(cQStr &sitem : sdlst)
                                        {
                                            if(! (like(sitem, excl[0]) || exclcheck(elst, cdir % '/' % item % '/' % sitem) || exist(srci % '/' % sitem)))
                                            {
                                                QStr strgi(trgi % '/' % sitem);
                                                if(a == 9) mchk(strgi);
                                                stype(strgi) == Isdir ? recrmdir(strgi) : rmfile(strgi);
                                            }

                                            if(ThrdKill) return false;
                                        }

                                        goto nitem_1;
                                    }
                                    case Islink:
                                    case Isfile:
                                        rmfile(trgi);
                                    }

                                    if(! (crtdir(trgi) || fspchk())) return false;
                                    break;
                                case Isfile:
                                    switch(stype(trgi)) {
                                    case Isfile:
                                        switch(fcomp(srci, trgi)) {
                                        case 1:
                                            cpertime(srci, trgi);
                                        case 2:
                                            goto nitem_1;
                                        }
                                    case Islink:
                                        rmfile(trgi);
                                        break;
                                    case Isdir:
                                        if(a == 9) mchk(trgi);
                                        recrmdir(trgi);
                                    }

                                    if(! (cpfile(srci, trgi) || fspchk())) return false;
                                }
                            }

                        nitem_1:
                            if(ThrdKill) return false;
                            ++lcnt;
                        }

                        in.seek(0), lcnt = 0;

                        while(! in.atEnd())
                        {
                            QStr item(in.readLine());

                            if(cditmst->at(lcnt++) == Isdir)
                            {
                                QStr trgi(trgd % '/' % item);
                                if(exist(trgi)) cpertime(srcd % '/' % item, trgi);
                            }

                            if(ThrdKill) return false;
                        }

                        cditms->clear(), cditmst->clear();
                    }

                    cpertime(srcd, trgd);
                }
                else if(exist(trgd))
                {
                    if(a == 9) mchk(trgd);
                    stype(trgd) == Isdir ? recrmdir(trgd) : rmfile(trgd);
                }
            }
        }

        {
            QStr srcd(srcdir % "/media"), trgd(trgt % "/media");

            if(isdir(srcd))
            {
                if(isdir(trgd))
                    for(cQStr &item : QDir(trgd).entryList(QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot))
                    {
                        if(! exist(srcd % '/' % item))
                        {
                            QStr trgi(trgd % '/' % item);
                            if(! mcheck(trgi % '/')) recrmdir(trgi);
                        }

                        if(ThrdKill) return false;
                    }
                else
                {
                    if(exist(trgd)) rmfile(trgd);
                    if(! (crtdir(trgd) || fspchk())) return false;
                }

                QBA mediaitms;
                mediaitms.reserve(10000);
                if(! rodir(mediaitms, srcd)) return false;

                if(! mediaitms.isEmpty())
                {
                    QTS in(&mediaitms, QIODevice::ReadOnly);

                    while(! in.atEnd())
                    {
                        QStr item(in.readLine()), trgi(trgd % '/' % item);

                        if(exist(trgi))
                        {
                            if(stype(trgi) != Isdir)
                            {
                                rmfile(trgi);
                                if(! (crtdir(trgi) || fspchk())) return false;
                            }

                            cpertime(srcd % '/' % item, trgi);
                        }
                        else if(! (cpdir(srcd % '/' % item, trgi) || fspchk()))
                            return false;

                        if(ThrdKill) return false;
                    }
                }

                cpertime(srcd, trgd);
            }
            else if(exist(trgd))
                stype(trgd) == Isdir ? recrmdir(trgd) : rmfile(trgd);
        }

        if(srcdir == "/.systembacklivepoint" && isdir("/.systembacklivepoint/.systemback"))
        {
            QBA sbitms;
            QUCL sbitmst;
            sbitms.reserve(10000), sbitmst.reserve(500);
            if(! rodir(sbitms, sbitmst, "/.systembacklivepoint/.systemback")) return false;
            lcnt = 0;
            QTS in(&sbitms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr item(in.readLine());

                switch(sbitmst.at(lcnt++)) {
                case Islink:
                    if(! ((sfstab && item == "etc/fstab") || cplink("/.systembacklivepoint/.systemback/" % item, trgt % '/' % item) || fspchk())) return false;
                    break;
                case Isfile:
                    if(! ((sfstab && item == "etc/fstab") || cpfile("/.systembacklivepoint/.systemback/" % item, trgt % '/' % item) || fspchk())) return false;
                    break;
                }

                if(ThrdKill) return false;
            }

            in.seek(0), lcnt = 0;

            while(! in.atEnd())
            {
                QStr item(in.readLine());
                if(sbitmst.at(lcnt++) == Isdir) cpertime("/.systembacklivepoint/.systemback/" % item, trgt % '/' % item);
                if(ThrdKill) return false;
            }
        }
    }
    else
        Progress = 0;

    if(mthd != 2)
    {
        QSL elst{".cache/gvfs", ".gvfs", ".local/share/Trash/files/", ".local/share/Trash/info/", ".Xauthority", ".ICEauthority"};

        {
            QFile file(srcdir % excfile);
            if(! fopen(file)) return false;

            while(! file.atEnd())
            {
                QStr cline(left(file.readLine(), -1));
                if(like(cline, {"_.*", "_snap_", "_snap/*"})) elst.append(cline);
                if(ThrdKill) return false;
            }
        }

        bool skppd;
        QSL excl[]{{"_lost+found_", "_lost+found/*", "*/lost+found_", "*/lost+found/*", "_Systemback_", "_Systemback/*", "*/Systemback_", "*/Systemback/*", "*~_", "*~/*"}, {"_lost+found_", "_Systemback_", "*~_"}};

        for(schar a(usrs.count() - 1) ; a > -1 ; --a)
        {
            cQStr &cusr(usrs.at(a));

            if(! cusr.isNull())
            {
                QStr srcd(srcdir), trgd(trgt);

                if(cusr.isEmpty())
                    for(QStr *dir : {&srcd, &trgd}) dir->append("/root");
                else
                {
                    QStr hdir("/home/" % cusr);
                    for(QStr *dir : {&srcd, &trgd}) dir->append(hdir);
                }

                {
                    if(! isdir(trgd))
                    {
                        if(exist(trgd)) rmfile(trgd);
                        if(! (crtdir(trgd) || fspchk())) return false;
                    }
                    else if(! like(mthd, {3, 4}))
                    {
                        QBAL sdlst;
                        if(! odir(sdlst, trgd, True, ilst)) return false;

                        for(cQStr &item : sdlst)
                        {
                            bool inc(! (ilst.isEmpty() || like(item, {"_.*", "_snap_"})));

                            if((inc || ! (item.endsWith('~') || exclcheck(elst, item))) && ! exist(srcd % '/' % item))
                            {
                                QStr trgi(trgd % '/' % item);

                                switch(stype(trgi)) {
                                case Isdir:
                                    recrmdir(trgi, true);
                                    break;
                                case Isfile:
                                    if(! inc && fsize(trgi) > 8000000) break;
                                case Islink:
                                    rmfile(trgi);
                                }
                            }

                            if(ThrdKill) return false;
                        }
                    }
                }

                if(! (cditmst = &homeitmst[a])->isEmpty() && anum)
                {
                    lcnt = 0;
                    QTS in(cditms = &homeitms[a], QIODevice::ReadOnly);

                    while(! in.atEnd())
                    {
                        if(Progress < (cperc = (++cnum * 100 + 50) / anum)) Progress = cperc;
                        QStr item(in.readLine());
                        bool inc;

                        if(! like(item, excl[0]) && ((inc = ! (ilst.isEmpty() || like(item, {"_.*", "_snap_", "_snap/*"}))) || ! exclcheck(elst, item)))
                        {
                            QStr srci(srcd % '/' % item), trgi(trgd % '/' % item);

                            switch(cditmst->at(lcnt)) {
                            case Islink:
                                switch(stype(trgi)) {
                                case Islink:
                                    if(lcomp(srci, trgi)) goto nitem_2;
                                case Isfile:
                                    rmfile(trgi);
                                    break;
                                case Isdir:
                                    recrmdir(trgi);
                                }

                                if(! (cplink(srci, trgi) || fspchk())) return false;
                                break;
                            case Isdir:
                                switch(stype(trgi)) {
                                case Isdir:
                                    if(! like(mthd, {3, 4}))
                                    {
                                        QBAL sdlst;
                                        if(! (inc ? odir(sdlst, trgi, Include, ilst, item) : odir(sdlst, trgi))) return false;

                                        for(cQStr &sitem : sdlst)
                                        {
                                            if(! (like(sitem, excl[1]) || exclcheck(elst, item % '/' % sitem) || exist(srci % '/' % sitem)))
                                            {
                                                QStr strgi(trgi % '/' % sitem);

                                                switch(stype(strgi)) {
                                                case Isdir:
                                                    recrmdir(strgi, true);
                                                    break;
                                                case Isfile:
                                                    if(! inc && fsize(strgi) > 8000000) break;
                                                case Islink:
                                                    rmfile(strgi);
                                                }
                                            }

                                            if(ThrdKill) return false;
                                        }
                                    }

                                    goto nitem_2;
                                case Islink:
                                case Isfile:
                                    rmfile(trgi);
                                }

                                if(! (crtdir(trgi) || fspchk())) return false;
                                break;
                            case Isfile:
                                skppd = ! inc && fsize(srci) > 8000000;

                                switch(stype(trgi)) {
                                case Isfile:
                                    switch(fcomp(trgi, srci)) {
                                    case 1:
                                        cpertime(srci, trgi);
                                    case 2:
                                        goto nitem_2;
                                    }

                                    if(skppd) goto nitem_2;
                                case Islink:
                                    rmfile(trgi);
                                    break;
                                case Isdir:
                                    recrmdir(trgi);
                                }

                                if(! (skppd || cpfile(srci, trgi) || fspchk())) return false;
                            }
                        }

                    nitem_2:
                        if(ThrdKill) return false;
                        ++lcnt;
                    }

                    in.seek(0), lcnt = 0;

                    while(! in.atEnd())
                    {
                        QStr item(in.readLine());

                        if(cditmst->at(lcnt++) == Isdir)
                        {
                            QStr trgi(trgd % '/' % item);
                            if(exist(trgi)) cpertime(srcd % '/' % item, trgi);
                        }

                        if(ThrdKill) return false;
                    }

                    cditms->clear(), cditmst->clear();
                }

                cpertime(srcd, trgd);
            }
        }
    }

    return true;
}

bool sb::thrdscopy(uchar mthd, cQStr &usr, cQStr &srcdir)
{
    uint lcnt;

    {
        QBA sysitms[13];
        QUCL sysitmst[13];

        {
            QStr dirs[]{srcdir % "/bin", srcdir % "/boot", srcdir % "/etc", srcdir % "/lib", srcdir % "/lib32", srcdir % "/lib64", srcdir % "/opt", srcdir % "/sbin", srcdir % "/selinux", srcdir % "/srv", srcdir % "/usr", srcdir % "/var"};
            uint bbs[]{10000, 20000, 100000, 500000, 10000, 10000, 100000, 10000, 10000, 10000, 10000000, 1000000}, ibs[]{500, 1000, 10000, 20000, 500, 500, 5000, 1000, 500, 500, 500000, 50000};

            for(uchar a(0) ; a < 12 ; ++a)
                if(isdir(dirs[a]))
                {
                    sysitms[a].reserve(bbs[a]), sysitmst[a].reserve(ibs[a]);
                    if(! rodir(sysitms[a], sysitmst[a], dirs[a])) return false;
                }
        }

        if(isdir(srcdir % "/snap"))
        {
            sysitms[12].reserve(10000), sysitmst[12].reserve(500);
            if(! rodir("./snap", sysitms[12], sysitmst[12])) return false;
        }

        uint anum(0), cnum(0);
        for(uchar a(0) ; a < 12 ; ++a) anum += sysitmst[a].count();
        QStr macid;
        QBA *cditms;
        QUCL *cditmst;
        uchar cperc;

        {
            QSL usrs;
            QBAL homeitms;
            QUCLL homeitmst;

            if(mthd > 2)
            {
                if(isdir(srcdir % "/home/" % usr)) usrs.append(usr);
                homeitms.append(nullptr), homeitmst.append(QUCL());
            }
            else if(mthd && isdir(srcdir % "/home"))
            {
                if(srcdir.isEmpty())
                {
                    QFile file("/etc/passwd");
                    if(! fopen(file)) return false;

                    while(! file.atEnd())
                    {
                        QStr cusr(file.readLine().trimmed());

                        if(cusr.contains(":/home/") && isdir("/home/" % (cusr = left(cusr, instr(cusr, ":") -1)))) usrs.append(cusr),
                                                                                                                   homeitms.append(nullptr),
                                                                                                                   homeitmst.append(QUCL());
                    }
                }
                else
                    for(cQStr &cusr : QDir(srcdir % "/home").entryList(QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot)) usrs.append(cusr),
                                                                                                                              homeitms.append(nullptr),
                                                                                                                              homeitmst.append(QUCL());

                if(ThrdKill) return false;
            }

            usrs.append(isdir(srcdir % "/root") ? "" : nullptr);

            if(mthd == 5)
            {
                homeitms[0].reserve(10000), homeitmst[0].reserve(500);
                if(! rodir(homeitms[0], homeitmst[0], srcdir % "/etc/skel")) return false;
            }
            else
            {
                if(! usrs.last().isNull())
                {
                    homeitms.append(nullptr), homeitms.last().reserve(50000),
                    homeitmst.append(QUCL()), homeitmst.last().reserve(1000);
                    if(! rodir(homeitms.last(), homeitmst.last(), srcdir % "/root", like(mthd, {2, 3}) ? True : False)) return false;
                }

                for(schar a(usrs.count() - 2) ; a > -1 ; --a)
                {
                    homeitms[a].reserve(5000000), homeitmst[a].reserve(100000);
                    if(! rodir(homeitms[a], homeitmst[a], srcdir % "/home/" % usrs.at(a), like(mthd, {2, 3}) ? True : False)) return false;
                }
            }

            for(cQUCL &cucl : homeitmst) anum += cucl.count();
            Progress = 0;

            if(isdir(srcdir % "/home") && ! (isdir("/.sbsystemcopy/home") || crtdir("/.sbsystemcopy/home")))
            {
                QFile::rename("/.sbsystemcopy/home", "/.sbsystemcopy/home_" % rndstr());
                if(! crtdir("/.sbsystemcopy/home")) return false;
            }

            if(mthd > 2)
            {
                QStr mfile(isfile(srcdir % "/var/lib/dbus/machine-id") ? "/var/lib/dbus/machine-id" : isfile(srcdir % "/etc/machine-id") ? "/etc/machine-id" : nullptr);

                if(! mfile.isEmpty())
                {
                    QFile file(srcdir % mfile);
                    if(! fopen(file)) return false;
                    QStr cline(file.readLine().trimmed());
                    if(cline.length() == 32) macid = cline;
                 }
            }

            QSL elst{".cache/gvfs", ".gvfs", ".local/share/Trash/files/", ".local/share/Trash/info/", ".Xauthority", ".ICEauthority"};

            {
                QFile file(srcdir % excfile);
                if(! fopen(file)) return false;

                while(! file.atEnd())
                {
                    QStr cline(left(file.readLine(), -1));
                    if(like(cline, {"_.*", "_snap_", "_snap/*"})) elst.append(cline);
                    if(ThrdKill) return false;
                }
            }

            bool skppd;
            QSL excl{"_lost+found_", "_lost+found/*", "*/lost+found_", "*/lost+found/*", "_Systemback_", "_Systemback/*", "*/Systemback_", "*/Systemback/*", "*~_", "*~/*"};

            for(uchar a(0) ; a < usrs.count() ; ++a)
            {
                cQStr &cusr(usrs.at(a));

                if(! cusr.isNull())
                {
                    QStr srcd[2], trgd;
                    cusr.isEmpty() ? (srcd[0] = srcdir % "/root", trgd = "/.sbsystemcopy/root") : (srcd[0] = srcdir % "/home/" % cusr, trgd = "/.sbsystemcopy/home/" % cusr);
                    srcd[1] = mthd == 5 ? srcdir % "/etc/skel" : srcd[0];

                    if(! isdir(trgd))
                    {
                        if(exist(trgd)) rmfile(trgd);
                        if(! cpdir(srcd[0], trgd)) return false;
                    }

                    if(! (cditmst = &homeitmst[mthd == 5 ? 0 : a])->isEmpty())
                    {
                        lcnt = 0;
                        QTS in(cditms = &homeitms[mthd == 5 ? 0 : a], QIODevice::ReadOnly);

                        while(! in.atEnd())
                        {
                            if(Progress < (cperc = (++cnum * 100 + 50) / anum)) Progress = cperc;
                            QStr item(in.readLine());

                            if((mthd == 5 || (! (like(item, excl) || exclcheck(elst, item)) && (macid.isEmpty() || ! item.contains(macid)))) && (! srcdir.isEmpty() || exist(srcd[1] % '/' % item)))
                            {
                                QStr trgi(trgd % '/' % item);

                                switch(cditmst->at(lcnt)) {
                                case Islink:
                                {
                                    QStr srci(srcd[1] % '/' % item);

                                    switch(stype(trgi)) {
                                    case Islink:
                                        if(! lcomp(srci, trgi)) goto nitem_1;
                                    case Isfile:
                                        rmfile(trgi);
                                        break;
                                    case Isdir:
                                        recrmdir(trgi);
                                    }

                                    if(! cplink(srci, trgi)) return false;
                                    break;
                                }
                                case Isdir:
                                    switch(stype(trgi)) {
                                    case Isdir:
                                        goto nitem_1;
                                    case Islink:
                                    case Isfile:
                                        rmfile(trgi);
                                    }

                                    if(! crtdir(trgi)) return false;
                                    break;
                                case Isfile:
                                    QStr srci(srcd[1] % '/' % item);
                                    skppd = like(mthd, {2, 3}) && fsize(srci) > 8000000;

                                    switch(stype(trgi)) {
                                    case Isfile:
                                        if(mthd == 5)
                                            switch(fcomp(trgi, srci)) {
                                            case 1:
                                                if(! cpertime(srci, trgi, ! cusr.isEmpty())) return false;
                                            case 2:
                                                goto nitem_1;
                                            }
                                        else
                                        {
                                            switch(fcomp(trgi, srci)) {
                                            case 1:
                                                if(! cpertime(srci, trgi)) return false;
                                            case 2:
                                                goto nitem_1;
                                            }

                                            if(skppd) goto nitem_1;
                                        }
                                    case Islink:
                                        rmfile(trgi);
                                        break;
                                    case Isdir:
                                        recrmdir(trgi);
                                    }

                                    if(mthd == 5)
                                    {
                                        if(! cpfile(srci, trgi, ! cusr.isEmpty())) return false;
                                    }
                                    else if(! (skppd || cpfile(srci, trgi)))
                                        return false;
                                }
                            }

                        nitem_1:
                            if(ThrdKill) return false;
                            ++lcnt;
                        }

                        in.seek(0), lcnt = 0;

                        while(! in.atEnd())
                        {
                            QStr item(in.readLine());

                            if(cditmst->at(lcnt++) == Isdir)
                            {
                                QStr trgi(trgd % '/' % item);
                                if(exist(trgi) && ! cpertime(srcd[1] % '/' % item, trgi, mthd == 5 && ! cusr.isEmpty())) return false;
                            }

                            if(ThrdKill) return false;
                        }

                        if(mthd < 5) cditms->clear(), cditmst->clear();
                    }

                    if(! cusr.isEmpty())
                    {
                        if(isfile(srcd[0] % "/.config/user-dirs.dirs"))
                        {
                            QFile file(srcd[0] % "/.config/user-dirs.dirs");
                            if(! fopen(file)) return false;

                            while(! file.atEnd())
                            {
                                QStr cline(file.readLine().trimmed()), dir;

                                if(! cline.startsWith('#') && cline.contains("$HOME") && (dir = left(right(cline, -instr(cline, "/")), -1)).length())
                                {
                                    QStr trgi(trgd % '/' % dir);

                                    if(! isdir(trgi))
                                    {
                                        QStr srci(srcd[0] % '/' % dir);

                                        if(isdir(srci))
                                        {
                                            if(! cpdir(srci, trgi)) return false;
                                        }
                                        else if(srcdir.startsWith(sdir[1]) && ! (crtdir(trgi) && cpertime(trgd, trgi)))
                                            return false;
                                    }
                                }

                                if(ThrdKill) return false;
                            }
                        }

                        if(! cpertime(srcd[0], trgd)) return false;
                    }
                }
            }
        }

        {
            QSL incl{"_initrd.img_", "_initrd.img.old_", "_vmlinuz_", "_vmlinuz.old_"};

            for(cQStr &item : QDir("/.sbsystemcopy").entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Files | QDir::System))
            {
                if(like(item, incl))
                {
                    QStr trgi("/.sbsystemcopy/" % item);

                    switch(stype(trgi)) {
                    case Islink:
                    {
                        QStr srci(srcdir % '/' % item);
                        if(islink(srci) && lcomp(srci, trgi)) break;
                    }
                    case Isfile:
                        if(! rmfile(trgi)) return false;
                        break;
                    case Isdir:
                        if(! recrmdir(trgi)) return false;
                    }
                }

                if(ThrdKill) return false;
            }

            for(cQStr &item : QDir(srcdir.isEmpty() ? "/" : srcdir).entryList(QDir::Files | QDir::System))
            {
                if(like(item, incl))
                {
                    QStr srci(srcdir % '/' % item);

                    if(islink(srci))
                    {
                        QStr trgi("/.sbsystemcopy/" % item);
                        if(! (exist(trgi) || cplink(srci, trgi))) return false;
                    }
                }

                if(ThrdKill) return false;
            }
        }

        for(cQStr &cdir : {"/cdrom", "/dev", "/home", "/mnt", "/root", "/proc", "/run", "/sys", "/tmp"})
        {
            QStr trgd("/.sbsystemcopy" % cdir);

            if(! isdir(srcdir % cdir))
            {
                if(exist(trgd)) stype(trgd) == Isdir ? recrmdir(trgd) : rmfile(trgd);
            }
            else if(! isdir(trgd))
            {
                if(exist(trgd)) rmfile(trgd);
                if(! cpdir(srcdir % cdir, trgd)) return false;
            }
            else if(! cpertime(srcdir % cdir, trgd))
                return false;

            if(ThrdKill) return false;
        }

        QSL elst{"/boot/efi", "/etc/crypttab", "/etc/mtab", "/snap/.sblvtmp", "/var/.sblvtmp", "/var/cache/fontconfig/", "/var/lib/dpkg/lock", "/var/lib/udisks2/", "/var/log", "/var/run/", "/var/tmp/"};
        if(mthd > 2) elst.append({"/etc/machine-id", "/var/lib/dbus/machine-id"});

        if(srcdir.isEmpty())
            elst.append(cfgfile);
        else
        {
            elst.append("/etc/systemback/");
            if(srcdir == "/.systembacklivepoint" && fload("/proc/cmdline").contains("noxconf")) elst.append({"/etc/X11/xorg.conf", "/etc/X11/xorg.conf.d/"});
        }

        edetect(elst);

        for(cQStr &cdir : {"/etc/rc0.d", "/etc/rc1.d", "/etc/rc2.d", "/etc/rc3.d", "/etc/rc4.d", "/etc/rc5.d", "/etc/rc6.d", "/etc/rcS.d"})
        {
            QStr srcd(srcdir % cdir);

            if(isdir(srcd))
                for(cQStr &item : QDir(srcd).entryList(QDir::Files))
                    if(item.contains("cryptdisks")) elst.append(cdir % '/' % item);
        }

        QSL dlst{"/bin", "/boot", "/etc", "/lib", "/lib32", "/lib64", "/opt", "/sbin", "/selinux", "/srv", "/usr", "/var", "/snap"}, excl[]{{"_lost+found_", "_Systemback_"}, {"_lost+found_", "_lost+found/*", "*/lost+found_", "*/lost+found/*", "_Systemback_", "_Systemback/*", "*/Systemback_", "*/Systemback/*"}, {"_/etc/udev/rules.d*", "*-persistent-*"}};

        for(uchar a(0) ; a < dlst.count() ; ++a)
        {
            cQStr &cdir(dlst.at(a));
            QStr srcd(srcdir % cdir), trgd("/.sbsystemcopy" % cdir);

            if(isdir(srcd))
            {
                if(isdir(trgd))
                {
                    QBAL sdlst;
                    if(! odir(sdlst, trgd)) return false;

                    for(cQStr &item : sdlst)
                    {
                        bool exc;

                        if(! like(item, excl[0]) && ((exc = exclcheck(elst, cdir % '/' % item)) || ! exist(srcd % '/' % item)))
                        {
                            QStr trgi(trgd % '/' % item);

                            switch(stype(trgi)) {
                            case Isdir:
                                if(! exc) recrmdir(trgi);
                                break;
                            default:
                                rmfile(trgi);
                            }
                        }

                        if(ThrdKill) return false;
                    }
                }
                else
                {
                    if(exist(trgd)) rmfile(trgd);
                    if(! crtdir(trgd)) return false;
                }

                if(! (cditmst = &sysitmst[a])->isEmpty())
                {
                    lcnt = 0;
                    QTS in(cditms = &sysitms[a], QIODevice::ReadOnly);

                    while(! in.atEnd())
                    {
                        if(Progress < (cperc = (++cnum * 100 + 50) / anum)) Progress = cperc;
                        QStr item(in.readLine());

                        if(! like(item, excl[1]))
                        {
                            QStr pdi(cdir % '/' % item);

                            if(! exclcheck(elst, pdi) && (macid.isEmpty() || ! item.contains(macid)) && (mthd < 3 || ! like(pdi, excl[2], All)) && (! srcdir.isEmpty() || exist(pdi)))
                            {
                                QStr srci(srcd % '/' % item), trgi(trgd % '/' % item);

                                switch(cditmst->at(lcnt)) {
                                case Islink:
                                    switch(stype(trgi)) {
                                    case Islink:
                                        if(lcomp(srci, trgi)) goto nitem_2;
                                    case Isfile:
                                        rmfile(trgi);
                                        break;
                                    case Isdir:
                                        recrmdir(trgi);
                                    }

                                    if(! cplink(srci, trgi)) return false;
                                    break;
                                case Isdir:
                                    switch(stype(trgi)) {
                                    case Isdir:
                                    {
                                        QBAL sdlst;
                                        if(! odir(sdlst, trgi)) return false;

                                        for(cQStr &sitem : sdlst)
                                        {
                                            bool exc;

                                            if(! like(sitem, excl[0]) && ((exc = exclcheck(elst, pdi % '/' % sitem)) || ! exist(srci % '/' % sitem)))
                                            {
                                                QStr strgi(trgi % '/' % sitem);

                                                switch(stype(strgi)) {
                                                case Isdir:
                                                    if(! exc) recrmdir(strgi);
                                                    break;
                                                default:
                                                    rmfile(strgi);
                                                }
                                            }

                                            if(ThrdKill) return false;
                                        }

                                        goto nitem_2;
                                    }
                                    case Islink:
                                    case Isfile:
                                        rmfile(trgi);
                                    }

                                    if(! crtdir(trgi)) return false;
                                    break;
                                case Isfile:
                                    switch(stype(trgi)) {
                                    case Isfile:
                                        switch(fcomp(trgi, srci)) {
                                        case 1:
                                            if(! cpertime(srci, trgi)) return false;
                                        case 2:
                                            goto nitem_2;
                                        }
                                    case Islink:
                                        rmfile(trgi);
                                        break;
                                    case Isdir:
                                        recrmdir(trgi);
                                    }

                                    if(! cpfile(srci, trgi)) return false;
                                }
                            }
                        }

                    nitem_2:
                        if(ThrdKill) return false;
                        ++lcnt;
                    }

                    in.seek(0), lcnt = 0;

                    while(! in.atEnd())
                    {
                        QStr item(in.readLine());

                        if(cditmst->at(lcnt++) == Isdir)
                        {
                            QStr trgi(trgd % '/' % item);
                            if(exist(trgi) && ! cpertime(srcd % '/' % item, trgi)) return false;
                        }

                        if(ThrdKill) return false;
                    }

                    cditms->clear(), cditmst->clear();
                }

                if(! cpertime(srcd, trgd)) return false;
            }
            else if(exist(trgd))
                stype(trgd) == Isdir ? recrmdir(trgd) : rmfile(trgd);
        }
    }

    {
        QStr srcd(srcdir % "/media"), trgd("/.sbsystemcopy/media");

        if(isdir(srcd))
        {
            if(isdir(trgd))
                for(cQStr &item : QDir(trgd).entryList(QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot))
                {
                    QStr trgi(trgd % '/' % item);
                    if(! (exist(srcdir % '/' % item) || mcheck(trgi % '/'))) recrmdir(trgi);
                    if(ThrdKill) return false;
                }
            else
            {
                if(exist(trgd)) rmfile(trgd);
                if(! crtdir(trgd)) return false;
            }

            if(! srcdir.isEmpty())
            {
                QBA mediaitms;
                mediaitms.reserve(10000);
                if(! rodir(mediaitms, srcd)) return false;

                if(! mediaitms.isEmpty())
                {
                    QTS in(&mediaitms, QIODevice::ReadOnly);

                    while(! in.atEnd())
                    {
                        QStr item(in.readLine()), trgi(trgd % '/' % item);

                        if(exist(trgi))
                        {
                            if(stype(trgi) != Isdir)
                            {
                                rmfile(trgi);
                                if(! crtdir(trgi)) return false;
                            }

                            if(! cpertime(srcdir % "/media/" % item, trgi)) return false;
                        }
                        else if(! cpdir(srcdir % "/media/" % item, trgi))
                            return false;

                        if(ThrdKill) return false;
                    }
                }
            }
            else if(isfile("/etc/fstab"))
            {
                QSL dlst(QDir(srcd).entryList(QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot));
                QFile file("/etc/fstab");
                if(! fopen(file)) return false;

                for(uchar a(0) ; a < dlst.count() ; ++a)
                {
                    cQStr &item(dlst.at(a));
                    if(a && ! fopen(file)) return false;
                    QSL incl{"* /media/" % item % " *", "* /media/" % item % "/*"};

                    while(! file.atEnd())
                    {
                        QStr cline(file.readLine().trimmed()), fdir;

                        if(! cline.startsWith('#') && like(cline.replace('\t', ' ').contains("\\040") ? QStr(cline).replace("\\040", " ") : cline, incl))
                            for(cQStr cdname : mid(cline, instr(cline, "/media/") + 7, instr(cline, " ", instr(cline, "/media/")) - instr(cline, "/media/") - 7).split('/'))
                                if(! cdname.isEmpty())
                                {
                                    QStr trgi(trgd % fdir.append('/' % (cdname.contains("\\040") ? QStr(cdname).replace("\\040", " ") : cdname)));
                                    if(! (isdir(trgi) || cpdir("/media" % fdir, trgi))) return false;
                                }

                        if(ThrdKill) return false;
                    }

                    file.close();
                }
            }

            if(! cpertime(srcd, trgd)) return false;
        }
        else if(exist(trgd))
            stype(trgd) == Isdir ? recrmdir(trgd) : rmfile(trgd);
    }

    if(exist("/.sbsystemcopy/var/log")) stype("/.sbsystemcopy/var/log") == Isdir ? recrmdir("/.sbsystemcopy/var/log") : rmfile("/.sbsystemcopy/var/log");

    if(isdir(srcdir % "/var/log"))
    {
        if(! crtdir("/.sbsystemcopy/var/log")) return false;
        QBA logitms;
        QUCL logitmst;
        logitms.reserve(20000), logitmst.reserve(1000);
        if(! rodir(logitms, logitmst, srcdir % "/var/log")) return false;

        if(! logitmst.isEmpty())
        {
            QSL excl{"*.gz_", "*.old_"};
            lcnt = 0;
            QTS in(&logitms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr item(in.readLine());

                switch(logitmst.at(lcnt++)) {
                case Isdir:
                    if(! crtdir("/.sbsystemcopy/var/log/" % item)) return false;
                    break;
                case Isfile:
                    if(! like(item, excl) && ! (item.contains('.') && isnum(right(item, -rinstr(item, ".")))))
                    {
                        QStr trgi("/.sbsystemcopy/var/log/" % item);
                        crtfile(trgi);
                        if(! cpertime(srcdir % "/var/log/" % item, trgi)) return false;
                    }
                }

                if(ThrdKill) return false;
            }

            in.seek(0), lcnt = 0;

            while(! in.atEnd())
            {
                QStr item(in.readLine());
                if((logitmst.at(lcnt++) == Isdir && ! cpertime(srcdir % "/var/log/" % item, "/.sbsystemcopy/var/log/" % item)) || ThrdKill) return false;
            }
        }

        if(! (cpertime(srcdir % "/var/log", "/.sbsystemcopy/var/log") && cpertime(srcdir % "/var", "/.sbsystemcopy/var"))) return false;
    }

    if(srcdir == "/.systembacklivepoint" && isdir("/.systembacklivepoint/.systemback"))
    {
        QBA sbitms;
        QUCL sbitmst;
        sbitms.reserve(10000), sbitmst.reserve(500);
        if(! rodir(sbitms, sbitmst, "/.systembacklivepoint/.systemback")) return false;
        lcnt = 0;
        QTS in(&sbitms, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr item(in.readLine());

            switch(sbitmst.at(lcnt++)) {
            case Islink:
                if(! (item == "etc/fstab" || cplink("/.systembacklivepoint/.systemback/" % item, "/.sbsystemcopy/" % item))) return false;
                break;
            case Isfile:
                if(! (item == "etc/fstab" || cpfile("/.systembacklivepoint/.systemback/" % item, "/.sbsystemcopy/" % item))) return false;
            }

            if(ThrdKill) return false;
        }

        in.seek(0), lcnt = 0;

        while(! in.atEnd())
        {
            QStr item(in.readLine());
            if((sbitmst.at(lcnt++) == Isdir && ! cpertime("/.systembacklivepoint/.systemback/" % item, "/.sbsystemcopy/" % item)) || ThrdKill) return false;
        }
    }

    return true;
}

bool sb::thrdlvprpr(bool iudata)
{
    if(ThrdLng[0]) ThrdLng[0] = 0;

    {
        QUCL sitmst;
        sitmst.reserve(1000000);

        for(cQStr &item : {"/bin", "/boot", "/etc", "/lib", "/lib32", "/lib64", "/opt", "/sbin", "/selinux", "/usr", "/initrd.img", "/initrd.img.old", "/vmlinuz", "/vmlinuz.old"})
            if(isdir(item))
            {
                if(! rodir(sitmst, item)) return false;
                ++ThrdLng[0];
            }
            else if(exist(item))
                ++ThrdLng[0];

        ThrdLng[0] += sitmst.count();
    }

    if(! (crtdir(sdir[2] % "/.sblivesystemcreate/.systemback") && crtdir(sdir[2] % "/.sblivesystemcreate/.systemback/etc"))) return false;

    if(isdir("/etc/udev"))
    {
        if(! crtdir(sdir[2] % "/.sblivesystemcreate/.systemback/etc/udev")) return false;

        if(isdir("/etc/udev/rules.d") && (! crtdir(sdir[2] % "/.sblivesystemcreate/.systemback/etc/udev/rules.d") ||
            (isfile("/etc/udev/rules.d/70-persistent-cd.rules") && ! cpfile("/etc/udev/rules.d/70-persistent-cd.rules", sdir[2] % "/.sblivesystemcreate/.systemback/etc/udev/rules.d/70-persistent-cd.rules")) ||
            (isfile("/etc/udev/rules.d/70-persistent-net.rules") && ! cpfile("/etc/udev/rules.d/70-persistent-net.rules", sdir[2] % "/.sblivesystemcreate/.systemback/etc/udev/rules.d/70-persistent-net.rules")) ||
            ! cpertime("/etc/udev/rules.d", sdir[2] % "/.sblivesystemcreate/.systemback/etc/udev/rules.d"))) return false;

        if(! cpertime("/etc/udev", sdir[2] % "/.sblivesystemcreate/.systemback/etc/udev")) return false;
    }

    if((isfile("/etc/fstab") && ! cpfile("/etc/fstab", sdir[2] % "/.sblivesystemcreate/.systemback/etc/fstab")) || ! cpertime("/etc", sdir[2] % "/.sblivesystemcreate/.systemback/etc")) return false;
    if(exist("/.sblvtmp")) stype("/.sblvtmp") == Isdir ? recrmdir("/.sblvtmp") : rmfile("/.sblvtmp");
    if(! crtdir("/.sblvtmp")) return false;

    for(cQStr &cdir : {"/cdrom", "/dev", "/mnt", "/proc", "/run", "/srv", "/sys", "/tmp"})
        if(isdir(cdir))
        {
            if(! cpdir(cdir, "/.sblvtmp" % cdir)) return false;
            ++ThrdLng[0];
        }

    if(ThrdKill) return false;

    if(! isdir("/media"))
    {
        if(! crtdir("/media")) return false;
    }
    else if(exist("/media/.sblvtmp"))
       stype("/media/.sblvtmp") == Isdir ? recrmdir("/media/.sblvtmp") : rmfile("/media/.sblvtmp");

    if(! (crtdir("/media/.sblvtmp") && crtdir("/media/.sblvtmp/media"))) return false;
    ++ThrdLng[0];
    if(ThrdKill) return false;

    if(isfile("/etc/fstab"))
    {
        QFile file("/etc/fstab");
        if(! fopen(file)) return false;
        QSL dlst(QDir("/media").entryList(QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot));

        for(uchar a(0) ; a < dlst.count() ; ++a)
        {
            cQStr &item(dlst.at(a));
            if(a && ! fopen(file)) return false;
            QSL incl{"* /media/" % item % " *", "* /media/" % item % "/*"};

            while(! file.atEnd())
            {
                QStr cline(file.readLine().trimmed()), fdir;

                if(! cline.startsWith('#') && like(cline.replace('\t', ' ').contains("\\040") ? QStr(cline).replace("\\040", " ") : cline, incl))
                    for(cQStr cdname : mid(cline, instr(cline, "/media/") + 7, instr(cline, " ", instr(cline, "/media/")) - instr(cline, "/media/") - 7).split('/'))
                        if(! cdname.isEmpty())
                        {
                            QStr sbli("/media/.sblvtmp/media" % fdir.append('/' % (cdname.contains("\\040") ? QStr(cdname).replace("\\040", " ") : cdname)));

                            if(! isdir(sbli))
                            {
                                if(! cpdir("/media" % fdir, sbli)) return false;
                                ++ThrdLng[0];
                            }
                        }

                if(ThrdKill) return false;
            }

            file.close();
        }
    }

    if(exist("/var/.sblvtmp")) stype("/var/.sblvtmp") == Isdir ? recrmdir("/var/.sblvtmp") : rmfile("/var/.sblvtmp");
    if(ThrdKill) return false;
    uint lcnt;

    {
        QBA varitms;
        QUCL varitmst;
        varitms.reserve(1000000), varitmst.reserve(50000);
        if(! rodir(varitms, varitmst, "/var")) return false;
        if(! (crtdir("/var/.sblvtmp") && crtdir("/var/.sblvtmp/var"))) return false;
        ++ThrdLng[0];
        QSL elst{"lib/dpkg/lock", "lib/udisks/mtab", "lib/ureadahead/", "log/", "run/", "tmp/"};
        edetect(elst, true);

        if(! varitmst.isEmpty())
        {
            QSL excl[]{{"+_cache/apt/*", "-*.bin_", "-*.bin.*"}, {"_cache/apt/archives/*", "*.deb_"}, {"_lost+found_", "_lost+found/*", "*/lost+found_", "*/lost+found/*", "_Systemback_", "_Systemback/*", "*/Systemback_", "*/Systemback/*", "*.dpkg-old_", "*~_", "*~/*"}, {"*.gz_", "*.old_"}};
            lcnt = 0;
            QTS in(&varitms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr item(in.readLine()), srci("/var/" % item);

                if(exist(srci))
                {
                    if(! (like(item, excl[0], Mixed) || like(item, excl[1], All) || like(item, excl[2]) || exclcheck(elst, item)))
                        switch(varitmst.at(lcnt)) {
                        case Islink:
                            if(! cplink(srci, "/var/.sblvtmp/var/" % item)) return false;
                            ++ThrdLng[0];
                            break;
                        case Isdir:
                            if(! crtdir("/var/.sblvtmp/var/" % item)) return false;
                            ++ThrdLng[0];
                            break;
                        case Isfile:
                            if(issmfs("/var/.sblvtmp", srci) && ! crthlnk(srci, "/var/.sblvtmp/var/" % item)) return false;
                            ++ThrdLng[0];
                        }
                    else if(item.startsWith("log"))
                        switch(varitmst.at(lcnt)) {
                        case Isdir:
                            if(! crtdir("/var/.sblvtmp/var/" % item)) return false;
                            ++ThrdLng[0];
                            break;
                        case Isfile:
                            if(! (like(item, excl[3]) || (item.contains('.') && isnum(right(item, -rinstr(item, "."))))))
                            {
                                QStr sbli("/var/.sblvtmp/var/" % item);
                                crtfile(sbli);
                                if(! cpertime(srci, sbli)) return false;
                                ++ThrdLng[0];
                            }
                        }
                }

                if(ThrdKill) return false;
                ++lcnt;
            }

            in.seek(0), lcnt = 0;

            while(! in.atEnd())
            {
                QStr item(in.readLine());

                if(varitmst.at(lcnt++) == Isdir)
                {
                    QStr sbli("/var/.sblvtmp/var/" % item);
                    if(exist(sbli) && ! cpertime("/var/" % item, sbli)) return false;
                }

                if(ThrdKill) return false;
            }
        }
    }

    if(! cpertime("/var", "/var/.sblvtmp/var")) return false;

    if(isdir("/snap"))
    {
        if(exist("/snap/.sblvtmp")) stype("/snap/.sblvtmp") == Isdir ? recrmdir("/snap/.sblvtmp") : rmfile("/snap/.sblvtmp");
        if(ThrdKill) return false;
        QBA snapitms;
        QUCL snapitmst;
        snapitms.reserve(10000), snapitmst.reserve(500);
        if(! rodir("/snap", snapitms, snapitmst)) return false;
        if(! (crtdir("/snap/.sblvtmp") && crtdir("/snap/.sblvtmp/snap"))) return false;
        ++ThrdLng[0];

        if(! snapitmst.isEmpty())
        {
            QSL excl{"_lost+found_", "_lost+found/*", "*/lost+found_", "*/lost+found/*", "_Systemback_", "_Systemback/*", "*/Systemback_", "*/Systemback/*", "*.dpkg-old_", "*~_", "*~/*"};
            lcnt = 0;
            QTS in(&snapitms, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr item(in.readLine()), srci("/snap/" % item);

                if(exist(srci) && ! like(item, excl))
                    switch(snapitmst.at(lcnt)) {
                    case Islink:
                    case Isfile:
                        if(! crthlnk(srci, "/snap/.sblvtmp/snap/" % item)) return false;
                        ++ThrdLng[0];
                        break;
                    case Isdir:
                        if(! crtdir("/snap/.sblvtmp/snap/" % item)) return false;
                        ++ThrdLng[0];
                    }

                if(ThrdKill) return false;
                ++lcnt;
            }

            in.seek(0), lcnt = 0;

            while(! in.atEnd())
            {
                QStr item(in.readLine());

                if(snapitmst.at(lcnt++) == Isdir)
                {
                    QStr sbli("/snap/.sblvtmp/snap/" % item);
                    if(exist(sbli) && ! cpertime("/snap/" % item, sbli)) return false;
                }

                if(ThrdKill) return false;
            }
        }

        if(! cpertime("/snap", "/snap/.sblvtmp/snap")) return false;
    }

    QSL usrs;

    {
        QFile file("/etc/passwd");
        if(! fopen(file)) return false;

        while(! file.atEnd())
        {
            QStr usr(file.readLine().trimmed());
            if(usr.contains(":/home/") && isdir("/home/" % (usr = left(usr, instr(usr, ":") -1)))) usrs.prepend(usr);
        }
    }

    usrs.prepend(isdir("/root") ? "" : nullptr);
    if(ThrdKill) return false;

    bool uhl(dfree("/home") > 104857600 && dfree("/root") > 104857600 && [&usrs] {
            QStr mnts(fload("/proc/self/mounts"));

            for(uchar a(1) ; a < usrs.count() ; ++a)
                if(mnts.contains(" /home/" % usrs.at(a) % '/') || ! issmfs("/home", "/home/" % usrs.at(a))) return false;

            return true;
        }());

    if(ThrdKill) return false;

    if(uhl)
    {
        if(exist("/home/.sbuserdata")) stype("/home/.sbuserdata") == Isdir ? recrmdir("/home/.sbuserdata") : rmfile("/home/.sbuserdata");
        if(! (crtdir("/home/.sbuserdata") && cpdir("/home", "/home/.sbuserdata/home"))) return false;
    }
    else if(! (crtdir(sdir[2] % "/.sblivesystemcreate/userdata") && cpdir("/home", sdir[2] % "/.sblivesystemcreate/userdata/home")))
        return false;

    ++ThrdLng[0];
    if(ThrdKill) return false;
    QSL elst{".sbuserdata", ".cache/gvfs", ".local/share/Trash/files/", ".local/share/Trash/info/", ".Xauthority", ".ICEauthority"};

    {
        QFile file(excfile);
        if(! fopen(file)) return false;

        while(! file.atEnd())
        {
            QStr cline(left(file.readLine(), -1));
            if(! cline.isEmpty()) elst.append(cline);
            if(ThrdKill) return false;
        }
    }

    QSL excl{"_lost+found_", "_lost+found/*", "*/lost+found_", "*/lost+found/*", "_Systemback_", "_Systemback/*", "*/Systemback_", "*/Systemback/*", "*~_", "*~/*"};

    for(cQStr &usr : usrs)
        if(! usr.isNull())
        {
            QStr usdir;

            if(uhl)
            {
                if(usr.isEmpty())
                {
                    usdir = "/root/.sbuserdata";
                    if(exist(usdir)) stype(usdir) == Isdir ? recrmdir(usdir) : rmfile(usdir);
                    if(! (crtdir(usdir) && crtdir(usdir.append("/root")))) return false;
                }
                else
                {
                    usdir = "/home/.sbuserdata/home/" % usr;
                    if(! crtdir(usdir)) return false;
                }
            }
            else
            {
                usdir = sdir[2] % (usr.isEmpty() ? "/.sblivesystemcreate/userdata/root" : QStr("/.sblivesystemcreate/userdata/home/" % usr));
                if(! crtdir(usdir)) return false;
            }

            ++ThrdLng[0];
            QStr srcd(usr.isEmpty() ? "/root" : QStr("/home/" % usr));
            QBA useritms;
            QUCL useritmst;
            useritms.reserve(usr.isEmpty() ? 50000 : 5000000), useritmst.reserve(usr.isEmpty() ? 1000 : 100000);
            if(! rodir(useritms, useritmst, srcd, ! iudata ? True : False)) return false;

            if(! useritmst.isEmpty())
            {
                lcnt = 0;
                QTS in(&useritms, QIODevice::ReadOnly);

                while(! in.atEnd())
                {
                    QStr item(in.readLine());

                    if(! (like(item, excl) || exclcheck(elst, item)))
                    {
                        QStr srci(srcd % '/' % item);

                        if(exist(srci))
                        {
                            switch(useritmst.at(lcnt)) {
                            case Islink:
                                if(uhl)
                                {
                                    if(! crthlnk(srci, usdir % '/' % item)) return false;
                                }
                                else if(! cplink(srci, usdir % '/' % item))
                                    return false;

                                ++ThrdLng[0];
                                break;
                            case Isdir:
                                if(! crtdir(usdir % '/' % item)) return false;
                                ++ThrdLng[0];
                                break;
                            case Isfile:
                                if(iudata || fsize(srci) <= 8000000)
                                {
                                    if(uhl)
                                    {
                                        if(! crthlnk(srci, usdir % '/' % item)) return false;
                                    }
                                    else if(! cpfile(srci, usdir % '/' % item))
                                        return false;

                                    ++ThrdLng[0];
                                }
                            }
                        }
                    }

                    if(ThrdKill) return false;
                    ++lcnt;
                }

                in.seek(0), lcnt = 0;

                while(! in.atEnd())
                {
                    QStr item(in.readLine());

                    if(useritmst.at(lcnt++) == Isdir)
                    {
                        QStr sbli(usdir % '/' % item);
                        if(exist(sbli) && ! cpertime(srcd % '/' % item, sbli)) return false;
                    }

                    if(ThrdKill) return false;
                }
            }

            if(! (iudata || usr.isEmpty()) && isfile(srcd % "/.config/user-dirs.dirs"))
            {
                QFile file(srcd % "/.config/user-dirs.dirs");
                if(! fopen(file)) return false;

                while(! file.atEnd())
                {
                    QStr cline(file.readLine().trimmed()), dir;

                    if(! cline.startsWith('#') && cline.contains("$HOME") && (dir = left(right(cline, -instr(cline, "/")), -1)).length())
                    {
                        QStr srci(srcd % '/' % dir);

                        if(isdir(srci))
                        {
                            QStr sbld(usdir % '/' % dir);

                            if(! isdir(sbld))
                            {
                                if(! cpdir(srci, sbld)) return false;
                                ++ThrdLng[0];
                            }
                        }
                    }

                    if(ThrdKill) return false;
                    continue;
                }

                file.close();
            }

            if(! cpertime(srcd, usdir)) return false;
        }

    return true;
}
