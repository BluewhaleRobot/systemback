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

#ifndef LBLEVENT_HPP
#define LBLEVENT_HPP

#include <QMouseEvent>
#include <QLabel>

class lblevent : public QLabel
{
    Q_OBJECT

public:
    inline lblevent(QWidget *prnt) : QLabel(prnt) {}
    static ushort MouseX, MouseY;

protected:
    void mouseDoubleClickEvent(QMouseEvent *ev),
         mouseReleaseEvent(QMouseEvent *ev),
         mousePressEvent(QMouseEvent *ev),
         mouseMoveEvent(QMouseEvent *),
         enterEvent(QEvent *),
         leaveEvent(QEvent *);

private:
    bool MousePressed;

signals:
    void Mouse_Released();
    void Mouse_DblClick();
    void Mouse_Pressed();
    void Mouse_Click();
    void Mouse_Enter();
    void Mouse_Leave();
    void Mouse_Move();
};

inline void lblevent::mousePressEvent(QMouseEvent *ev)
{
    if(ev->button() == Qt::LeftButton)
    {
        MouseX = ev->x(), MouseY = ev->y(), MousePressed = true;
        emit Mouse_Pressed();
    }
}

inline void lblevent::mouseMoveEvent(QMouseEvent *)
{
    if(MousePressed) emit Mouse_Move();
}

inline void lblevent::mouseReleaseEvent(QMouseEvent *ev)
{
    if(ev->button() == Qt::LeftButton)
    {
        MousePressed = false;
        emit Mouse_Released();
    }

    emit Mouse_Click();
}

inline void lblevent::mouseDoubleClickEvent(QMouseEvent *ev)
{
    if(ev->button() == Qt::LeftButton) emit Mouse_DblClick();
}

inline void lblevent::enterEvent(QEvent *)
{
    emit Mouse_Enter();
}

inline void lblevent::leaveEvent(QEvent *)
{
    emit Mouse_Leave();
}

#endif
