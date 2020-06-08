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

#ifndef SBLIB_HPP
#define SBLIB_HPP

#include "sblib_global.hpp"
#include "bstr.hpp"
#include <QCoreApplication>
#include <QStringBuilder>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QThread>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <unistd.h>

#define fnln __attribute__((always_inline))
#define cfgfile "/etc/systemback/systemback.conf"
#define excfile "/etc/systemback/systemback.excludes"
#define incfile "/etc/systemback/systemback.includes"

class SHARED_EXPORT_IMPORT sb : public QThread
{
    Q_DECLARE_TR_FUNCTIONS(systemback)

public:
    enum { Remove = 0, Copy = 1, Sync = 2, Mount = 3, Umount = 4, Readprttns = 5, Readlvdevs = 6, Ruuid = 7, Setpflag = 8, Mkptable = 9, Mkpart = 10, Delpart = 11, Crtrpoint = 12, Srestore = 13, Scopy = 14, Lvprpr = 15,
           MSDOS = 0, GPT = 1, Clear = 2, Primary = 3, Extended = 4, Logical = 5, Freespace = 6, Emptyspace = 7,
           Nodbg = 0, Errdbg = 1, Alldbg = 2, Extdbg = 3, Cextdbg = 4, Nulldbg = 5, Falsedbg = 6,
           Notexist = 0, Isfile = 1, Isdir = 2, Islink = 3, Isblock = 4, Unknown = 5,
           Noflag = 0, Silent = 1, Bckgrnd = 2, Prgrss = 4, Wait = 8,
           Sblock = 0, Dpkglock = 1, Aptlock = 2, Schdlrlock = 3,
           False = 0, True = 1, Empty = 2, Include = 3,
           Crtdir = 0, Rmfile = 1, Crthlnk = 2,
           Read = 0, Write = 1, Exec = 2,
           Norm = 0, All = 1, Mixed = 2 };

    static sb SBThrd;
    static QStr ThrdStr[3], eout, sdir[3], schdlr[2], pnames[15], lang, style, wsclng;
    static ullong ThrdLng[2];
    static uchar dbglev, pnumber, ismpnt, schdle[6], waot, incrmtl, xzcmpr, autoiso, ecache;
    static schar Progress;
    static bool ExecKill, ThrdKill;

    static fnln QStr hunit(ullong size);

    static QStr mid(cQStr &txt, ushort start, ushort len),
                fload(cQStr &path, bool ascnt),
                right(cQStr &txt, short len),
                left(cQStr &txt, short len),
                gdetect(cQStr rdir = "/"),
                rndstr(uchar vlen = 10),
                ruuid(cQStr &part),
                appver(),
                dbginf();

    static QBA fload(cQStr &path);
    template<typename T> static ullong dfree(const T &path);
    static fnln ullong fsize(cQStr &path);

    static fnln ushort instr(cQStr &txt, cQStr &stxt, ushort start = 1),
                       rinstr(cQStr &txt, cQStr &stxt);

    template<typename T> static uchar stype(const T &path, bool flink = false);

    static uchar exec(cQStr &cmd, uchar flag = Noflag, cQStr &envv = nullptr),
                 exec(cQSL &cmds);

    template<typename T1, typename T2> static bool issmfs(const T1 &item1, const T2 &item2);
    template<typename T> static fnln bool crtdir(const T &path);
    template<typename T> static fnln bool rmfile(const T &file);
    template<typename T> static bool exist(const T &path);

    static fnln bool islink(cQStr &path),
                     isfile(cQStr &path),
                     isdir(cQStr &path);

    static bool srestore(uchar mthd, cQStr &usr, cQStr &srcdir, cQStr &trgt, bool sfstab = false),
                mkpart(cQStr &dev, ullong start = 0, ullong len = 0, uchar type = Primary),
                mcheck(cQStr &item, cQStr &mnts = fload("/proc/self/mounts")),
                mount(cQStr &dev, cQStr &mpoint, cQStr &moptns = nullptr),
                like(cQStr &txt, cQSL &lst, uchar mode = Norm),
                execsrch(cQStr &fname, cQStr &ppath = nullptr),
                scopy(uchar mthd, cQStr &usr, cQStr &srcdir),
                mkptable(cQStr &dev, cQStr &type = "msdos"),
                crtfile(cQStr &path, cQStr &txt = nullptr),
                like(int num, cSIL &lst, bool all = false),
                access(cQStr &path, uchar mode = Read),
                copy(cQStr &srcfile, cQStr &newfile),
                setpflag(cQStr &part, cQStr &flags),
                rename(cQStr &opath, cQStr &npath),
                error(QStr txt, bool dbg = false),
                cfgwrite(cQStr &file = cfgfile),
                crtrpoint(cQStr &pname),
                islnxfs(cQStr &path),
                remove(cQStr &path),
                lvprpr(bool iudata),
                fopen(QFile &file),
                umount(cQStr &dev),
                isnum(cQStr &txt),
                lock(uchar type);

    static void readprttns(QSL &strlst),
                readlvdevs(QSL &strlst),
                delpart(cQStr &part),
                unlock(uchar type),
                delay(ushort msec),
                print(cQStr &txt),
                supgrade(),
                pupgrade(),
                thrdelay(),
                cfgread(),
                fssync(),
                ldtltr();

protected:
    void run();

private:
    sb();
    ~sb();

    static QTrn *SBtr;
    static QSL *ThrdSlst;
    static int sblock[4];
    static uchar ThrdType, ThrdChr;
    static bool ThrdBool, ThrdRslt;

    static QStr fdbg(cQStr &path1, cQStr &path2 = nullptr),
                rlink(cQStr &path, ushort blen);

    static ullong devsize(cQStr &dev);

    static bool rodir(QBA &ba, QUCL &ucl, cQStr &path, uchar hidden = False, cQSL &ilist = QSL(), uchar oplen = 0),
                rodir(cQStr &path, QBA &ba, QUCL &ucl, ullong id = 0, uchar oplen = 0),
                cerr(uchar type, cQStr &str1, cQStr &str2 = nullptr),
                rodir(QUCL &ucl, cQStr &path, uchar oplen = 0),
                rodir(QBA &ba, cQStr &path, uchar oplen = 0),
                inclcheck(cQSL &ilist, cQStr &item);

    uchar fcomp(cQStr &file1, cQStr &file2);
    template<typename T1, typename T2> fnln bool crthlnk(const T1 &srclnk, const T2 &newlnk);

    bool odir(QBAL &balst, cQStr &path, uchar hidden = False, cQSL &ilist = QSL(), cQStr &ppath = nullptr),
         thrdsrestore(uchar mthd, cQStr &usr, cQStr &srcdir, cQStr &trgt, bool sfstab),
         cpertime(cQStr &srcitem, cQStr &newitem, bool skel = false),
         cpfile(cQStr &srcfile, cQStr &newfile, bool skel = false),
         thrdscopy(uchar mthd, cQStr &usr, cQStr &srcdir),
         recrmdir(cbstr &path, bool slimit = false),
         cplink(cQStr &srclink, cQStr &newlink),
         cpdir(cQStr &srcdir, cQStr &newdir),
         exclcheck(cQSL &elist, cQStr &item),
         lcomp(cQStr &link1, cQStr &link2),
         thrdcrtrpoint(cQStr &trgt),
         thrdlvprpr(bool iudata),
         umnt(cbstr &dev);

    void edetect(QSL &elst, bool spath = false);
};

inline QStr sb::left(cQStr &txt, short len)
{
    return txt.length() > qAbs(len) ? txt.left(len > 0 ? len : txt.length() + len) : len > 0 ? txt : nullptr;
}

inline QStr sb::right(cQStr &txt, short len)
{
    return txt.length() > qAbs(len) ? txt.right(len > 0 ? len : txt.length() + len) : len > 0 ? txt : nullptr;
}

inline QStr sb::mid(cQStr &txt, ushort start, ushort len)
{
    return txt.length() >= start ? txt.length() - start + 1 > len ? txt.mid(start - 1, len) : txt.right(txt.length() - start + 1) : nullptr;
}

inline ushort sb::instr(cQStr &txt, cQStr &stxt, ushort start)
{
    return txt.indexOf(stxt, start - 1) + 1;
}

inline ushort sb::rinstr(cQStr &txt, cQStr &stxt)
{
    return txt.lastIndexOf(stxt) + 1;
}

inline bool sb::like(int num, cSIL &lst, bool all)
{
    for(int val : lst)
        if(all ? num != val : num == val) return ! all;

    return all;
}

template<typename T> inline bool sb::exist(const T &path)
{
    struct stat istat;
    return ! lstat(bstr(path), &istat);
}

inline bool sb::islink(cQStr &path)
{
    return QFileInfo(path).isSymLink();
}

inline bool sb::isfile(cQStr &path)
{
    return QFileInfo(path).isFile();
}

inline bool sb::isdir(cQStr &path)
{
    return QFileInfo(path).isDir();
}

template<typename T> inline uchar sb::stype(const T &path, bool flink)
{
    struct stat istat;
    if(flink ? stat(bstr(path), &istat) : lstat(bstr(path), &istat)) return Notexist;

    switch(istat.st_mode & S_IFMT) {
    case S_IFREG:
        return Isfile;
    case S_IFDIR:
        return Isdir;
    case S_IFLNK:
        return Islink;
    case S_IFBLK:
        return Isblock;
    default:
        return Unknown;
    }
}

inline ullong sb::fsize(cQStr &path)
{
    return QFileInfo(path).size();
}

template<typename T> ullong sb::dfree(const T &path)
{
    struct statvfs dstat;
    return statvfs(bstr(path), &dstat) ? 0 : dstat.f_bavail * dstat.f_bsize;
}

inline QStr sb::hunit(ullong size)
{
    return size < 1024 ? QStr(QStr::number(size) % " B") : size < 1048576 ? QStr::number(qRound64(size * 100.0 / 1024.0) / 100.0) % " KiB" : size < 1073741824 ? QStr::number(qRound64(size * 100.0 / 1024.0 / 1024.0) / 100.0) % " MiB" : size < 1073741824000 ? QStr::number(qRound64(size * 100.0 / 1024.0 / 1024.0 / 1024.0) / 100.0) % " GiB" : QStr::number(qRound64(size * 100.0 / 1024.0 / 1024.0 / 1024.0 / 1024.0) / 100.0) % " TiB";
}

template<typename T1, typename T2> inline bool sb::issmfs(const T1 &item1, const T2 &item2)
{
    struct stat istat[2];
    return ! (stat(bstr(item1), &istat[0]) || stat(bstr(item2), &istat[1])) && istat[0].st_dev == istat[1].st_dev;
}

template<typename T> inline bool sb::crtdir(const T &path)
{
    return mkdir(bstr(path), 0755) ? cerr(Crtdir, path) : true;
}

template<typename T> inline bool sb::rmfile(const T &file)
{
    return unlink(bstr(file)) ? cerr(Rmfile, file) : true;
}

inline bool sb::isnum(cQStr &txt)
{
    for(uchar a(0) ; a < txt.length() ; ++a)
        if(! txt.at(a).isDigit()) return false;

    return ! txt.isEmpty();
}

#endif
