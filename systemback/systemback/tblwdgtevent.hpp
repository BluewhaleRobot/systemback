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

#ifndef TBLWDGTEVENT_HPP
#define TBLWDGTEVENT_HPP

#include <QTableWidget>

class tblwdgtevent : public QTableWidget
{
    Q_OBJECT

public:
    inline tblwdgtevent(QWidget *prnt) : QTableWidget(prnt) {}

protected:
    void focusInEvent(QFocusEvent *ev),
         focusOutEvent(QFocusEvent *ev);

signals:
    void Focus_In();
    void Focus_Out();
};

inline void tblwdgtevent::focusInEvent(QFocusEvent *ev)
{
    QTableWidget::focusInEvent(ev);
    emit Focus_In();
}

inline void tblwdgtevent::focusOutEvent(QFocusEvent *ev)
{
    QTableWidget::focusOutEvent(ev);
    emit Focus_Out();
}

#endif
