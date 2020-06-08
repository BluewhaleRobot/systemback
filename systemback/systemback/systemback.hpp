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

#ifndef SYSTEMBACK_HPP
#define SYSTEMBACK_HPP

#include "../libsystemback/sblib.hpp"
#include <QTreeWidgetItem>
#include <QMainWindow>
#include <QTimer>

#define QTblWI QTableWidgetItem
#define QLWI QListWidgetItem
#define QTrWI QTreeWidgetItem
#define cQTrWI const QTreeWidgetItem

namespace Ui {
class systemback;
}

class systemback : public QMainWindow
{
    Q_OBJECT

public:
    systemback();
    ~systemback();

    bool fscrn;

protected:
    bool eventFilter(QObject *, QEvent *ev);

    void keyReleaseEvent(QKeyEvent *ev),
         keyPressEvent(QKeyEvent *ev),
         closeEvent(QCloseEvent *ev);

private:
    enum { Normal = 100, High = 147, Max = 200 };

    Ui::systemback *ui;

    struct {
        QStr txt;
        uchar type, pnts;
    } prun;

    struct {
        QStr name;
        bool isEFI;
    } grub;

    QWdt wndw, fwdgt;
    QFont bfnt;
    QTimer utimer, *shdltimer, *dlgtimer, *intrptimer;
    QStr cpoint, pname, hash;
    QBA ocfg;
    ushort dialog;
    short wgeom[4], cpos;
    uchar busycnt, ppipe, sfctr, icnt;
    bool sislive, wismax, wmblck, uchkd, nrxth, ickernel, irblck, utblck, nohmcpy[2], sstart, intrrpt;

    QLE getpoint(uchar num);
    QCB getppipe(uchar num);

    QStr guname(),
         ckname();

    QRect sgeom(bool rdc = false, QDW dtp = nullptr);
    ushort ss(ushort dsize);
    template<typename T> bool cfmod(const T &path, ushort mode);

    bool minside(cQRect &rct),
         minside(QWdt wgt);

    void windowmove(ushort nwidth, ushort nheight, bool fxdw = true),
         dialogopen(ushort dlg = 0, cbstr &dev = nullptr),
         ilstupdt(bool inc = false, cQStr &dir = nullptr),
         pset(uchar type, cbstr &tend = nullptr),
         dirxpnd(QTrWI *item, bool inc = false),
         ptxtchange(uchar num, cQStr &txt),
         setwontop(bool state = true),
         busy(bool state = true),
         pnmchange(uchar num),
         pntupgrade(),
         emptycache(),
         statustart(),
         systemcopy(),
         livewrite(),
         rmntcheck(),
         stschange(),
         bttnsshow(),
         bttnshide(),
         restore(),
         repair();

#if QT_VERSION < QT_VERSION_CHECK(5, 3, 0)
private slots:
#endif
    void dialogtimer();

#if QT_VERSION < QT_VERSION_CHECK(5, 4, 0)
private slots:
#endif
    void schedulertimer();
    void unitimer();

private slots:
    void benter(bool click = false);
    void abtreleased();
    void wbreleased();
    void wreleased();
    void rreleased();
    void mpressed();
    void rpressed();
    void wpressed();
    void bpressed();
    void wbenter();
    void wbleave();
    void foutpnt();
    void renter();
    void rleave();
    void wmove();
    void rmove();
    void bmove();

    void on_partitionsettings_currentItemChanged(QTblWI *crrnt, QTblWI *prvs);
    void on_livedevices_currentItemChanged(QTblWI *crrnt, QTblWI *prvs);
    void on_grubreinstallrestore_currentIndexChanged(const QStr &arg1);
    void on_grubreinstallrepair_currentIndexChanged(const QStr &arg1);
    void on_grubinstallcopy_currentIndexChanged(const QStr &arg1);
    void on_repairpartition_currentIndexChanged(const QStr &arg1);
    void on_repairmountpoint_currentTextChanged(const QStr &arg1);
    void on_windowposition_currentIndexChanged(const QStr &arg1);
    void on_includeusers_currentIndexChanged(const QStr &arg1);
    void on_excludeitemslist_currentItemChanged(QTrWI *crrnt);
    void on_includeitemslist_currentItemChanged(QTrWI *crrnt);
    void on_filesystem_currentIndexChanged(const QStr &arg1);
    void on_languages_currentIndexChanged(const QStr &arg1);
    void on_mountpoint_currentTextChanged(const QStr &arg1);
    void on_excludedlist_currentItemChanged(QLWI *crrnt);
    void on_includedlist_currentItemChanged(QLWI *crrnt);
    void on_styles_currentIndexChanged(const QStr &arg1);
    void on_admins_currentIndexChanged(const QStr &arg1);
    void on_adminpassword_textChanged(const QStr &arg1);
    void on_rootpassword1_textChanged(const QStr &arg1);
    void on_users_currentIndexChanged(const QStr &arg1);
    void on_excludeitemslist_itemCollapsed(QTrWI *item);
    void on_dirchoose_currentItemChanged(QTrWI *crrnt);
    void on_excludeitemslist_itemExpanded(QTrWI *item);
    void on_includeitemslist_itemExpanded(QTrWI *item);
    void on_livelist_currentItemChanged(QLWI *crrnt);
    void on_password1_textChanged(const QStr &arg1);
    void on_usersettingscopy_stateChanged(int arg1);
    void on_autorestoreoptions_clicked(bool chckd);
    void on_incrementaldisable_clicked(bool chckd);
    void on_livename_textChanged(const QStr &arg1);
    void on_fullname_textChanged(const QStr &arg1);
    void on_username_textChanged(const QStr &arg1);
    void on_hostname_textChanged(const QStr &arg1);
    void on_autorepairoptions_clicked(bool chckd);
    void on_point10_textChanged(const QStr &arg1);
    void on_point11_textChanged(const QStr &arg1);
    void on_point12_textChanged(const QStr &arg1);
    void on_point13_textChanged(const QStr &arg1);
    void on_point14_textChanged(const QStr &arg1);
    void on_point15_textChanged(const QStr &arg1);
    void on_skipfstabrestore_clicked(bool chckd);
    void on_languageoverride_clicked(bool chckd);
    void on_schedulerdisable_clicked(bool chckd);
    void on_cachemptydisable_clicked(bool chckd);
    void on_point1_textChanged(const QStr &arg1);
    void on_point2_textChanged(const QStr &arg1);
    void on_point3_textChanged(const QStr &arg1);
    void on_point4_textChanged(const QStr &arg1);
    void on_point5_textChanged(const QStr &arg1);
    void on_point6_textChanged(const QStr &arg1);
    void on_point7_textChanged(const QStr &arg1);
    void on_point8_textChanged(const QStr &arg1);
    void on_point9_textChanged(const QStr &arg1);
    void on_skipfstabrepair_clicked(bool chckd);
    void on_userdatainclude_clicked(bool chckd);
    void on_usexzcompressor_clicked(bool chckd);
    void on_dirchoose_itemExpanded(QTrWI *item);
    void on_configurationfilesrestore_clicked();
    void on_styleoverride_clicked(bool chckd);
    void on_autoisocreate_clicked(bool chckd);
    void on_repairpartitionrefresh_clicked();
    void on_alwaysontop_clicked(bool chckd);
    void on_silentmode_clicked(bool chckd);
    void on_livedevicesrefresh_clicked();
    void on_liveworkdirbutton_clicked();
    void on_rootpassword2_textChanged();
    void on_partitionrefresh2_clicked();
    void on_partitionrefresh3_clicked();
    void on_excluderemoveitem_clicked();
    void on_includeremoveitem_clicked();
    void on_format_clicked(bool chckd);
    void on_functionmenunext_clicked();
    void on_functionmenuback_clicked();
    void on_storagedirbutton_clicked();
    void on_partitionrefresh_clicked();
    void on_fullname_editingFinished();
    void on_schedulerrefresh_clicked();
    void on_passwordinputok_clicked();
    void on_dirchoosecancel_clicked();
    void on_changepartition_clicked();
    void on_newrestorepoint_clicked();
    void on_partitiondelete_clicked();
    void on_schedulerstart_clicked();
    void on_schedulerstate_clicked();
    void on_pointhighlight_clicked();
    void on_livewritestart_clicked();
    void on_schedulerlater_clicked();
    void on_excludeadditem_clicked();
    void on_includeadditem_clicked();
    void on_systemupgrade_clicked();
    void on_systemrestore_clicked();
    void on_password2_textChanged();
    void on_schedulerback_clicked();
    void on_unmountdelete_clicked();
    void on_dialogcancel_clicked();
    void on_schedulemenu_clicked();
    void on_pointexclude_clicked();
    void on_systemrepair_clicked();
    void on_newpartition_clicked();
    void on_settingsmenu_clicked();
    void on_settingsback_clicked();
    void on_startcancel_clicked();
    void on_restoremenu_clicked();
    void on_installmenu_clicked();
    void on_excludemenu_clicked();
    void on_includemenu_clicked();
    void on_restoreback_clicked();
    void on_installback_clicked();
    void on_excludeback_clicked();
    void on_licensemenu_clicked();
    void on_licenseback_clicked();
    void on_pointpipe10_clicked();
    void on_pointpipe11_clicked();
    void on_pointpipe12_clicked();
    void on_pointpipe13_clicked();
    void on_pointpipe14_clicked();
    void on_pointpipe15_clicked();
    void on_pointrename_clicked();
    void on_dirchooseok_clicked();
    void on_fullrestore_clicked();
    void on_restorenext_clicked();
    void on_installnext_clicked();
    void on_repairmount_clicked();
    void on_liveexclude_clicked();
    void on_pointdelete_clicked();
    void on_liveconvert_clicked();
    void on_scalingdown_clicked();
    void on_includeback_clicked();
    void on_repairmenu_clicked();
    void on_repairback_clicked();
    void on_pointpipe1_clicked();
    void on_pointpipe2_clicked();
    void on_pointpipe3_clicked();
    void on_pointpipe4_clicked();
    void on_pointpipe5_clicked();
    void on_pointpipe6_clicked();
    void on_pointpipe7_clicked();
    void on_pointpipe8_clicked();
    void on_pointpipe9_clicked();
    void on_livedelete_clicked();
    void on_fullrepair_clicked();
    void on_grubrepair_clicked();
    void on_repairnext_clicked();
    void on_minutedown_clicked();
    void on_seconddown_clicked();
    void on_dirrefresh_clicked();
    void on_pnumber10_clicked();
    void on_aboutmenu_clicked();
    void on_aboutback_clicked();
    void on_interrupt_clicked();
    void on_scalingup_clicked();
    void on_pnumber3_clicked();
    void on_pnumber4_clicked();
    void on_pnumber5_clicked();
    void on_pnumber6_clicked();
    void on_pnumber7_clicked();
    void on_pnumber8_clicked();
    void on_pnumber9_clicked();
    void on_copymenu_clicked();
    void on_copyback_clicked();
    void on_livemenu_clicked();
    void on_liveback_clicked();
    void on_dialogok_clicked();
    void on_copynext_clicked();
    void on_hourdown_clicked();
    void on_minuteup_clicked();
    void on_secondup_clicked();
    void on_livenew_clicked();
    void on_daydown_clicked();
    void on_unmount_clicked();
    void on_adduser_clicked();
    void on_hourup_clicked();
    void on_dayup_clicked();
};

template<typename T> bool systemback::cfmod(const T &path, ushort mode)
{
    return chmod(bstr(path), mode) ? sb::error("\n " % tr("An error occurred while changing the access permissions of the following file:") % "\n\n  " % QStr(path) % "\n\n") : true;
}

#endif
