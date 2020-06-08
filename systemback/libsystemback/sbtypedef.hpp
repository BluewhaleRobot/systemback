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

#ifndef SBTYPEDEF_HPP
#define SBTYPEDEF_HPP

#include <QtWidgets/QDesktopWidget>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QLabel>
#include <QTranslator>
#include <QTextStream>
#include <QStringList>

class bstr;

typedef QTranslator QTrn;
typedef QCursor *QCr;
typedef QDesktopWidget *QDW;
typedef QWidget *QWdt;
typedef QTableWidget *QTblW;
typedef QListWidget *QLW;
typedef QPushButton *QPB;
typedef QLineEdit *QLE;
typedef QComboBox *QCbB;
typedef QRadioButton *QRB;
typedef QCheckBox *QCB;
typedef QLabel *QLbl;
typedef QTextStream QTS;
typedef QList<QWidget *> QWL;
typedef QList<QComboBox *> QCbBL;
typedef QList<QLabel *> QLbL;
typedef const QStringList cQSL;
typedef QStringList QSL;
typedef QList<QByteArray> QBAL;
typedef QList<QList<uchar>> QUCLL;
typedef QList<long long> QLIL;
typedef const QList<uchar> cQUCL;
typedef QList<uchar> QUCL;
typedef const std::initializer_list<int> cSIL;
typedef const QString cQStr;
typedef QString QStr;
typedef const QByteArray cQBA;
typedef QByteArray QBA;
typedef const QChar cQChar;
typedef const QRect cQRect;
typedef const QSize cQSize;
typedef const QPoint cQPoint;
typedef const bstr cbstr;
typedef unsigned long long ullong;
typedef long long llong;
typedef const char cchar;
typedef signed char schar;

#endif
