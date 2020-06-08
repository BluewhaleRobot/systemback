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

#ifndef SYSTEMBACKCLI_HPP
#define SYSTEMBACKCLI_HPP

#include "../libsystemback/sblib.hpp"
#include <QTimer>

class systemback : public QObject
{
    Q_OBJECT

public:
    systemback();
    ~systemback();

#if QT_VERSION < QT_VERSION_CHECK(5, 4, 0)
public slots:
#endif
    void main();

private:
    enum { Inprog = 0, Start = 1, Stop = 2 };

    struct {
        QStr txt, pbar;
        uchar type, cperc;
    } prun;

    QTimer *ptimer;
    QStr pname, cpoint;
    QChar yn[2];
    bstr sbtxt;
    uchar blgn;

    QStr twrp(cQStr &txt);
    uchar storagedir(cQSL &args);

    uchar clistart(),
          restore();

    bool newrpnt();

    void pset(uchar type),
         emptycache();

private slots:
    void progress(uchar status = Inprog);
};

inline systemback::systemback() : ptimer(nullptr)
{
    QStr yns(tr("(Y/N)"));
    yn[0] = yns.at(1), yn[1] = yns.at(3), prun.cperc = 0;
}

inline systemback::~systemback()
{
    if(ptimer) delete ptimer;
}

inline QStr systemback::twrp(cQStr &txt)
{
    return txt.length() > 78 ? QStr(txt).replace(txt.left(78).lastIndexOf(' '), 1, "\n ") : txt;
}

#endif
