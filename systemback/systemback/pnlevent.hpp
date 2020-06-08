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

#ifndef PNLEVENT_HPP
#define PNLEVENT_HPP

#include <QWidget>

class pnlevent : public QWidget
{
    Q_OBJECT

public:
    inline pnlevent(QWidget *prnt) : QWidget(prnt) {}

protected:
    void mouseMoveEvent(QMouseEvent *),
         resizeEvent(QResizeEvent *),
         moveEvent(QMoveEvent *),
         hideEvent(QHideEvent *),
         leaveEvent(QEvent *);

signals:
    void Mouse_Leave();
    void Mouse_Move();
    void Resize();
    void Move();
    void Hide();
};

inline void pnlevent::mouseMoveEvent(QMouseEvent *)
{
    emit Mouse_Move();
}

inline void pnlevent::resizeEvent(QResizeEvent *)
{
    emit Resize();
}

inline void pnlevent::moveEvent(QMoveEvent *)
{
    emit Move();
}

inline void pnlevent::hideEvent(QHideEvent *)
{
    emit Hide();
}

inline void pnlevent::leaveEvent(QEvent *)
{
    emit Mouse_Leave();
}

#endif
