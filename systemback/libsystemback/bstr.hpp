/*
 * Copyright(C) 2015-2016, Krisztián Kende <nemh@freemail.hu>
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

#ifndef BSTR_HPP
#define BSTR_HPP

#include "sbtypedef.hpp"

class bstr
{
private:
    QBA ba;

public:
    inline bstr() {}
    inline bstr(cchar *txt) : data(txt) {}
    inline bstr(cQBA &txt) : data(txt.constData()) {}
    inline bstr(cQStr &txt) : ba(txt.toUtf8()), data(ba.constData()) {}
    operator cchar *() const;

    cchar *data;

    cchar *rplc(cchar *bef, cchar *aft);
};

inline bstr::operator cchar *() const
{
    return data;
}

inline cchar *bstr::rplc(cchar *bef, cchar *aft)
{
    return ba.replace(bef, aft);
}

#endif
