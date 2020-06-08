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

#include "ui_systemback.h"
#include "systemback.hpp"
#include <QFontDatabase>
#include <QStyleFactory>
#include <QTemporaryDir>
#include <QTextStream>
#include <QScrollBar>
#include <QDateTime>
#include <sys/utsname.h>
#include <sys/swap.h>
#include <X11/Xlib.h>
#include <dirent.h>
#include <signal.h>

#ifdef FontChange
#undef FontChange
#endif

#ifdef KeyRelease
#undef KeyRelease
#endif

#ifdef KeyPress
#undef KeyPress
#endif

#ifdef False
#undef False
#endif

#ifdef True
#undef True
#endif

Display *dsply(XOpenDisplay(nullptr));
ushort lblevent::MouseX, lblevent::MouseY;

systemback::systemback() : QMainWindow(nullptr, Qt::FramelessWindowHint), ui(new Ui::systemback)
{
    ocfg = (sb::sdir[0] % sb::sdir[2] % sb::schdlr[0] % sb::schdlr[1] % sb::lang % sb::style % sb::wsclng).toUtf8() % char(sb::ismpnt) % char(sb::pnumber) % char(sb::incrmtl) % char(sb::xzcmpr) % char(sb::autoiso) % char(sb::schdle[0]) % char(sb::schdle[1]) % char(sb::schdle[2]) % char(sb::schdle[3]) % char(sb::schdle[4]) % char(sb::schdle[5]) % char(sb::waot) % char(sb::ecache), shdltimer = dlgtimer = intrptimer = nullptr, wmblck = wismax = fscrn = nrxth = false;

    if(sb::style != "auto")
    {
        if(QStyleFactory::keys().contains(sb::style))
            qApp->setStyle(QStyleFactory::create(sb::style));
        else
            sb::style = "auto";
    }

    ui->setupUi(this), ui->dialogpanel->move(0, 0);
    for(QWdt wdgt : QWL{ui->statuspanel, ui->scalingbuttonspanel, ui->buttonspanel, ui->resizepanel}) wdgt->hide();
    for(QWdt wdgt : QWL{ui->dialogpanel, ui->windowmaximize, ui->windowminimize, ui->windowclose}) wdgt->setBackgroundRole(QPalette::Foreground);
    for(QWdt wdgt : QWL{ui->function3, ui->windowbutton3, ui->windowmaximize, ui->windowminimize, ui->windowclose}) wdgt->setForegroundRole(QPalette::Base);
    ui->subdialogpanel->setBackgroundRole(QPalette::Background),
    ui->buttonspanel->setBackgroundRole(QPalette::Highlight),
    connect(ui->function3, &lblevent::Mouse_Pressed, this, &systemback::wpressed),
    connect(ui->function3, &lblevent::Mouse_Move, this, &systemback::wmove),
    connect(ui->function3, &lblevent::Mouse_Released, this, &systemback::wreleased),
    connect(ui->windowbutton3, SIGNAL(Mouse_Enter()), this, SLOT(benter())),
    connect(ui->windowbutton3, &lblevent::Mouse_Pressed, this, &systemback::bpressed),
    connect(ui->windowbutton3, &lblevent::Mouse_Released, this, &systemback::wbreleased);
    for(QWdt wdgt : QWL{ui->windowbutton3, ui->buttonspanel, ui->windowminimize, ui->windowclose}) connect(wdgt, SIGNAL(Mouse_Move()), this, SLOT(bmove()));
    connect(ui->buttonspanel, &pnlevent::Mouse_Leave, [this] { if(ui->buttonspanel->isVisible() && ! ui->buttonspanel->y()) bttnshide(); });

    for(QWdt wdgt : QWL{ui->windowminimize, ui->windowclose}) connect(wdgt, SIGNAL(Mouse_Enter()), this, SLOT(wbenter())),
                                                              connect(wdgt, SIGNAL(Mouse_Leave()), this, SLOT(wbleave())),
                                                              connect(wdgt, SIGNAL(Mouse_Pressed()), this, SLOT(mpressed())),
                                                              connect(wdgt, SIGNAL(Mouse_Released()), this, SLOT(wbreleased()));

    dialog = getuid() + getgid() ? 306 : [this] {
            QSL args(qApp->arguments());

            if(args.count() == 2 && args.at(1) == "schedule" && [] {
                    QStr ppath(sb::isdir("/run") ? "/run/sbscheduler.pid" : "/var/run/sbscheduler.pid");
                    return sb::isfile(ppath) && getppid() == sb::fload(ppath).toUShort();
                }())

                sstart = true;
            else if(! sb::lock(sb::Sblock))
                return 300;
            else
            {
                if(! (sislive = sb::isfile("/cdrom/casper/filesystem.squashfs") || sb::isfile("/lib/live/mount/medium/live/filesystem.squashfs")))
                {
                    if(! sb::lock(sb::Dpkglock))
                        return 301;
                    else if(! sb::lock(sb::Aptlock))
                        return 302;
                }

                sstart = false;
            }

            return 0;
        }();

    QRect sgm(sgeom());

    {
        QFont fnt;
        if(! sb::like(fnt.family(), {"_Ubuntu_", "_FreeSans_"})) fnt.setFamily(QFontDatabase().families().contains("Ubuntu") ? "Ubuntu" : "FreeSans");
        if(fnt.weight() != QFont::Normal) fnt.setWeight(QFont::Normal);
        if(fnt.bold()) fnt.setBold(false);
        if(fnt.italic()) fnt.setItalic(false);
        if(fnt.overline()) fnt.setOverline(false);
        if(fnt.strikeOut()) fnt.setStrikeOut(false);
        if(fnt.underline()) fnt.setUnderline(false);

        if(! (sb::like(sb::wsclng, {"_auto_", "_1_"}) && fontInfo().pixelSize() == 15))
        {
            sfctr = sb::wsclng == "auto" ? fontInfo().pixelSize() > 28 ? Max : fontInfo().pixelSize() > 21 ? High : Normal : sb::wsclng == "2" ? Max : sb::wsclng == "1.5" ? High : Normal;
            while(sfctr > Normal && (sgm.width() - ss(30) < ss(698) || sgm.height() - ss(30) < ss(465))) sfctr = sfctr == Max ? High : Normal;
            fnt.setPixelSize(ss(15));
            for(QWdt wdgt : QWL{ui->storagedir, ui->liveworkdir, ui->interrupt, ui->partitiondelete}) wdgt->setFont(fnt);
            qApp->setFont(fnt),
            fnt.setPixelSize(ss(27)),
            ui->buttonspanel->setFont(fnt),
            fnt.setPixelSize(ss(17)), fnt.setBold(true),
            ui->passwordtitletext->setFont(fnt);

            if(sfctr > Normal)
            {
                for(QWdt wdgt : findChildren<QWdt>()) wdgt->setGeometry(ss(wdgt->x()), ss(wdgt->y()), ss(wdgt->width()), ss(wdgt->height()));
                for(QPB pbtn : findChildren<QPB>()) pbtn->setIconSize(QSize(ss(pbtn->iconSize().width()), ss(pbtn->iconSize().height())));

                if(! (sstart || dialog))
                {
                    {
                        ushort sz[]{ss(10), ss(20)};

                        for(QTblW tblw : findChildren<QTblW>()) tblw->horizontalHeader()->setMinimumSectionSize(sz[0]),
                                                                tblw->verticalHeader()->setDefaultSectionSize(sz[1]);
                    }

                    { QSize nsize(ss(112), ss(32));
                    for(QCbB cmbx : findChildren<QCbB>()) cmbx->setMinimumSize(nsize); }
                    ui->partitionsettings->verticalScrollBar()->adjustSize();
                    QStr nsize(QStr::number(ss(ui->partitionsettings->verticalScrollBar()->width())));
                    for(QWdt wdgt : QWL{ui->partitionsettings, ui->livelist, ui->livedevices, ui->excludeitemslist, ui->excludedlist, ui->includeitemslist, ui->includedlist, ui->license, ui->dirchoose}) wdgt->setStyleSheet("QScrollBar::vertical{width: " % nsize % "px}\nQScrollBar::horizontal{height: " % nsize % "px}");
                    QStyleOption optn;
                    optn.init(ui->pointpipe1),
                    nsize = QStr::number(ss(ui->pointpipe1->style()->subElementRect(QStyle::SE_CheckBoxClickRect, &optn).width()));
                    for(QCB ckbx : findChildren<QCB>()) ckbx->setStyleSheet("QCheckBox::indicator{width:" % nsize % "px; height:" % nsize % "px}");
                    optn.init(ui->pnumber3),
                    nsize = QStr::number(ss(ui->pnumber3->style()->subElementRect(QStyle::SE_RadioButtonClickRect, &optn).width()));
                    for(QCbB rbtn : findChildren<QCbB>()) rbtn->setStyleSheet("QRadioButton::indicator{width:" % nsize % "px; height:" % nsize % "px}");
                }
            }
        }
        else
        {
            sfctr = Normal;

            if(fnt != font())
            {
                for(QWdt wdgt : QWL{ui->storagedir, ui->liveworkdir, ui->interrupt, ui->partitiondelete}) wdgt->setFont(fnt);
                qApp->setFont(fnt);
            }
        }
    }

    bfnt = font();

    if(dialog)
    {
        wndw = this;
        for(QWdt wdgt : QWL{ui->mainpanel, ui->passwordpanel, ui->schedulerpanel}) wdgt->hide();
        dialogopen(dialog);
    }
    else
    {
        intrrpt = irblck = utblck = false, prun.type = prun.pnts = ppipe = busycnt = 0,
        ui->dialogpanel->hide();

        for(QWdt wdgt : QWL{ui->statuspanel, ui->resizepanel}) wdgt->move(0, 0),
                                                               wdgt->setBackgroundRole(QPalette::Foreground);

        for(QWdt wdgt : QWL{ui->substatuspanel, ui->subpanel}) wdgt->setBackgroundRole(QPalette::Background);
        for(QWdt wdgt : QWL{ui->function2, ui->function4, ui->windowbutton2, ui->windowbutton4}) wdgt->setForegroundRole(QPalette::Base);

        for(QWdt wdgt : QWL{ui->function2, ui->function4}) connect(wdgt, SIGNAL(Mouse_Pressed()), this, SLOT(wpressed())),
                                                           connect(wdgt, SIGNAL(Mouse_Move()), this, SLOT(wmove())),
                                                           connect(wdgt, SIGNAL(Mouse_Released()), this, SLOT(wreleased()));

        for(QWdt wdgt : QWL{ui->windowbutton2, ui->windowbutton4}) connect(wdgt, SIGNAL(Mouse_Enter()), this, SLOT(benter())),
                                                                   connect(wdgt, SIGNAL(Mouse_Pressed()), this, SLOT(bpressed()));

        connect(ui->windowbutton4, &lblevent::Mouse_Released, this, &systemback::wbreleased),
        connect(ui->windowbutton4, &lblevent::Mouse_Move, this, &systemback::bmove);

        connect(ui->statuspanel, &pnlevent::Hide, [this] {
                if(! ui->statuspanel->isVisibleTo(ui->wpanel))
                {
                    prun.type = 0, prun.txt.clear(),
                    ui->processrun->clear();
                    if(prun.pnts) prun.pnts = 0;
                    if(sb::Progress > -1) sb::Progress = -1;
                    if(! ui->progressbar->maximum()) ui->progressbar->setMaximum(100);
                    if(ui->progressbar->value()) ui->progressbar->setValue(0);
                    if(ui->interrupt->isEnabled()) ui->interrupt->setDisabled(true);

                    if(sb::dbglev == sb::Errdbg)
                    {
                        if(! sb::eout.isEmpty()) sb::eout.clear();
                        sb::dbglev = sb::Nulldbg;
                    }
                }
            });

        if(! sstart)
        {
            icnt = 0, cpos = -1, nohmcpy[1] = uchkd = false;
            for(QWdt wdgt : QWL{ui->restorepanel, ui->copypanel, ui->installpanel, ui->livepanel, ui->repairpanel, ui->excludepanel, ui->includepanel, ui->schedulepanel, ui->aboutpanel, ui->licensepanel, ui->settingspanel, ui->choosepanel, ui->storagedirbutton, ui->fullnamepipe, ui->usernamepipe, ui->usernameerror, ui->passwordpipe, ui->passworderror, ui->rootpasswordpipe, ui->rootpassworderror, ui->hostnamepipe, ui->hostnameerror}) wdgt->hide();
            ui->storagedir->resize(ss(236), ss(28)),
            ui->installpanel->move(ui->sbpanel->pos()),
            ui->mainpanel->setBackgroundRole(QPalette::Foreground);
            for(QWdt wdgt : QWL{ui->sbpanel, ui->installpanel, ui->subscalingbuttonspanel}) wdgt->setBackgroundRole(QPalette::Background);
            for(QWdt wdgt : QWL{ui->function1, ui->scalingbutton, ui->windowbutton1}) wdgt->setForegroundRole(QPalette::Base);
            for(QWdt wdgt : QWL{ui->scalingfactor, ui->storagedirarea}) wdgt->setBackgroundRole(QPalette::Base);
            ui->scalingbuttonspanel->setBackgroundRole(QPalette::Highlight);

            if(sb::wsclng == "auto")
                ui->scalingdown->setDisabled(true);
            else if(sb::wsclng == "2")
            {
                ui->scalingfactor->setText("x2");
                ui->scalingup->setDisabled(true);
            }
            else
                ui->scalingfactor->setText('x' % sb::wsclng);

            connect(ui->windowmaximize, &lblevent::Mouse_Enter, this, &systemback::wbenter),
            connect(ui->windowmaximize, &lblevent::Mouse_Leave, this, &systemback::wbleave);
            for(QWdt wdgt : QWL{ui->windowmaximize, ui->scalingbutton, ui->homepage1, ui->homepage2, ui->email, ui->donate}) connect(wdgt, SIGNAL(Mouse_Pressed()), this, SLOT(mpressed()));

            for(QWdt wdgt : QWL{ui->windowmaximize, ui->windowbutton1}) connect(wdgt, SIGNAL(Mouse_Move()), this, SLOT(bmove())),
                                                                        connect(wdgt, SIGNAL(Mouse_Released()), this, SLOT(wbreleased()));

            for(QWdt wdgt : QWL{ui->chooseresize, ui->copyresize, ui->excluderesize, ui->includeresize}) connect(wdgt, SIGNAL(Mouse_Enter()), this, SLOT(renter())),
                                                                                                         connect(wdgt, SIGNAL(Mouse_Leave()), this, SLOT(rleave())),
                                                                                                         connect(wdgt, SIGNAL(Mouse_Pressed()), this, SLOT(rpressed())),
                                                                                                         connect(wdgt, SIGNAL(Mouse_Released()), this, SLOT(rreleased())),
                                                                                                         connect(wdgt, SIGNAL(Mouse_Move()), this, SLOT(rmove()));

            connect(ui->function1, &lblevent::Mouse_Pressed, this, &systemback::wpressed),
            connect(ui->function1, &lblevent::Mouse_Move, this, &systemback::wmove),
            connect(ui->function1, &lblevent::Mouse_Released, this, &systemback::wreleased),
            connect(ui->function1, &lblevent::Mouse_DblClick, [this] { if(ui->copypanel->isVisible() || ui->excludepanel->isVisible() || ui->includepanel->isVisible() || ui->choosepanel->isVisible()) stschange(); }),
            connect(ui->windowbutton1, SIGNAL(Mouse_Enter()), this, SLOT(benter())),
            connect(ui->windowbutton1, &lblevent::Mouse_Pressed, this, &systemback::bpressed);

            connect(ui->scalingbutton, &lblevent::Mouse_Released, [this] {
                    if(ui->scalingbutton->foregroundRole() == QPalette::Highlight)
                    {
                        uchar a(ss(24));
                        ui->scalingbuttonspanel->move(-ui->scalingbuttonspanel->width() + a, -ui->scalingbuttonspanel->height() + a),
                        ui->scalingbuttonspanel->show(),
                        a = ss(1);
                        short px(ui->scalingbuttonspanel->x());
                        do ui->scalingbuttonspanel->move(px += a, ui->scalingbuttonspanel->y() < -a ? ui->scalingbuttonspanel->y() + a : 0), qApp->processEvents();
                        while(px < 0 && px == ui->scalingbuttonspanel->x());
                    }
                });

            connect(ui->scalingbutton, &lblevent::Mouse_Move, [this] {
                    if(minside(ui->scalingbutton))
                    {
                        if(ui->scalingbutton->foregroundRole() == QPalette::Base) ui->scalingbutton->setForegroundRole(QPalette::Highlight);
                    }
                    else if(ui->scalingbutton->foregroundRole() == QPalette::Highlight)
                        ui->scalingbutton->setForegroundRole(QPalette::Base);
                });

            connect(ui->scalingbuttonspanel, &pnlevent::Mouse_Leave, [this] {
                    QStr nsclng(ui->scalingfactor->text() == "auto" ? "auto" : sb::right(ui->scalingfactor->text(), -1));

                    if(sb::wsclng == nsclng)
                    {
                        ui->scalingbutton->setForegroundRole(QPalette::Base);
                        uchar a(ss(1));
                        do ui->scalingbuttonspanel->move(ui->scalingbuttonspanel->x() - a, ui->scalingbuttonspanel->y() - a), qApp->processEvents();
                        while(ui->scalingbuttonspanel->y() > -ui->scalingbuttonspanel->height());
                        ui->scalingbuttonspanel->hide();
                    }
                    else
                    {
                        sb::wsclng = nsclng,
                        sb::cfgwrite(), ocfg.clear(),
                        sb::unlock(sb::Sblock), sb::unlock(sb::Dpkglock), sb::unlock(sb::Aptlock);

                        if(fscrn)
                            utimer.stop(), hide(),
                            sb::exec("systemback finstall", sb::Wait);
                        else
                            sb::exec("systemback", sb::Bckgrnd);

                        close();
                    }
                });

            connect(ui->homepage1, &lblevent::Mouse_Move, [this] {
                    if(minside(ui->homepage1))
                    {
                        if(ui->homepage1->foregroundRole() == QPalette::Text) ui->homepage1->setForegroundRole(QPalette::Highlight);
                    }
                    else if(ui->homepage1->foregroundRole() == QPalette::Highlight)
                        ui->homepage1->setForegroundRole(QPalette::Text);
                });

            connect(ui->homepage2, &lblevent::Mouse_Move, [this] {
                    if(minside(ui->homepage2))
                    {
                        if(ui->homepage2->foregroundRole() == QPalette::Text) ui->homepage2->setForegroundRole(QPalette::Highlight);
                    }
                    else if(ui->homepage2->foregroundRole() == QPalette::Highlight)
                        ui->homepage2->setForegroundRole(QPalette::Text);
                });

            connect(ui->email, &lblevent::Mouse_Move, [this] {
                    if(minside(ui->email))
                    {
                        if(ui->email->foregroundRole() == QPalette::Text) ui->email->setForegroundRole(QPalette::Highlight);
                    }
                    else if(ui->email->foregroundRole() == QPalette::Highlight)
                        ui->email->setForegroundRole(QPalette::Text);
                });

            connect(ui->donate, &lblevent::Mouse_Move, [this] {
                    if(minside(ui->donate))
                    {
                        if(ui->donate->foregroundRole() == QPalette::Text) ui->donate->setForegroundRole(QPalette::Highlight);
                    }
                    else if(ui->donate->foregroundRole() == QPalette::Highlight)
                        ui->donate->setForegroundRole(QPalette::Text);
                });

            connect(ui->partitionsettings, &tblwdgtevent::Focus_In, [this] {
                    if(ui->partitionsettingspanel2->isVisible())
                        for(ushort a(ui->partitionsettings->currentRow() + 1) ; a < ui->partitionsettings->rowCount() && ui->partitionsettings->item(a, 0)->background() == QPalette().color(QPalette::Inactive, QPalette::Highlight) ; ++a) ui->partitionsettings->item(a, 0)->setBackground(QPalette().highlight()),
                                                                                                                                                                                                                                             ui->partitionsettings->item(a, 0)->setForeground(QPalette().highlightedText());
                });

            connect(ui->partitionsettings, &tblwdgtevent::Focus_Out, [this] {
                    if(ui->partitionsettingspanel2->isVisibleTo(ui->copypanel))
                        for(ushort a(ui->partitionsettings->currentRow() + 1) ; a < ui->partitionsettings->rowCount() && ui->partitionsettings->item(a, 0)->background() == QPalette().highlight() ; ++a) ui->partitionsettings->item(a, 0)->setBackground(QPalette().color(QPalette::Inactive, QPalette::Highlight)),
                                                                                                                                                                                                          ui->partitionsettings->item(a, 0)->setForeground(QPalette().color(QPalette::Inactive, QPalette::HighlightedText));
                });

            for(QWdt wdgt : QWL{ui->homepage1, ui->homepage2, ui->email, ui->donate}) connect(wdgt, SIGNAL(Mouse_Released()), this, SLOT(abtreleased()));
            for(QLE ldt : ui->points->findChildren<QLE>()) connect(ldt, SIGNAL(Focus_Out()), this, SLOT(foutpnt()));
            connect(ui->usersettingscopy, &chckbxevent::Mouse_Enter, [this] { if(ui->usersettingscopy->checkState() == Qt::PartiallyChecked) ui->usersettingscopy->setText(tr("Transfer user configuration and data files")); }),
            connect(ui->usersettingscopy, &chckbxevent::Mouse_Leave, [this] { if(ui->usersettingscopy->checkState() == Qt::PartiallyChecked && ui->usersettingscopy->text() == tr("Transfer user configuration and data files")) ui->usersettingscopy->setText(tr("Transfer user configuration files")); }),
            connect(ui->unmountdelete, &bttnevent::Mouse_Leave, [this] { if(! ui->unmountdelete->isEnabled() && ui->unmountdelete->text() == tr("! Delete !") && (ui->partitionsettings->item(ui->partitionsettings->currentRow(), 4)->text().isEmpty() || ui->partitionsettings->item(ui->partitionsettings->currentRow(), 5)->text() != "btrfs")) ui->unmountdelete->setEnabled(true); });
        }

        wndw = [&]() -> QWdt  {
                QSL args(qApp->arguments());

                if([&args] {
                        switch(args.count()) {
                        case 2:
                            return args.at(1) == "finstall";
                        case 3:
                            return args.at(1) == "authorization";
                        default:
                            return false;
                        }
                    }() && ! (sislive && sb::like(sb::fload("/proc/self/mounts"), {"* / overlay *","* / overlayfs *", "* / aufs *", "* / unionfs *", "* / fuse.unionfs-fuse *"})))
                {
                    for(QWdt wdgt : QWL{ui->mainpanel, ui->schedulerpanel, ui->adminpasswordpipe, ui->adminpassworderror}) wdgt->hide();
                    ui->passwordpanel->move(0, 0);
                    for(QLbl lbl : QLbL{ui->adminstext, ui->adminpasswordtext}) lbl->resize(fontMetrics().width(lbl->text() + ss(7)), lbl->height());
                    ui->admins->move(ui->adminstext->x() + ui->adminstext->width(), ui->admins->y()),
                    ui->admins->setMaximumWidth(ui->passwordpanel->width() - ui->admins->x() - ss(8)),
                    ui->adminpassword->move(ui->adminpasswordtext->x() + ui->adminpasswordtext->width(), ui->adminpassword->y()),
                    ui->adminpassword->resize(ss(336) - ui->adminpassword->x(), ui->adminpassword->height());

                    {
                        QFile file("/etc/group");

                        if(sb::fopen(file))
                            while(! file.atEnd())
                            {
                                QStr cline(file.readLine().trimmed());

                                if(cline.startsWith("sudo:"))
                                    for(cQStr &usr : sb::right(cline, -sb::rinstr(cline, ":")).split(','))
                                        if(! usr.isEmpty() && ui->admins->findText(usr) == -1) ui->admins->addItem(usr);
                            }
                    }

                    if(! ui->admins->count())
                        ui->admins->addItem("root");
                    else if(ui->admins->count() > 1)
                    {
                        schar i(ui->admins->findText(args.at(2)));
                        if(i > 0) ui->admins->setCurrentIndex(i);
                    }

                    setFixedSize(wgeom[2] = ss(376), wgeom[3] = ss(224)),
                    move(wgeom[0] = sgm.x() + sgm.width() / 2 - ss(188), wgeom[1] = sgm.y() + sgm.height() / 2 - ss(112)),
                    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
                }
                else
                {
                    ui->passwordpanel->hide(), busy();

                    QTimer::singleShot(0, this,
#if QT_VERSION < QT_VERSION_CHECK(5, 4, 0)
                        SLOT(unitimer())
#else
                        &systemback::unitimer
#endif
                        );

                    if(sstart)
                    {
                        ui->mainpanel->hide(),
                        ui->schedulerpanel->move(0, 0),
                        ui->schedulerpanel->setBackgroundRole(QPalette::Foreground),
                        ui->subschedulerpanel->setBackgroundRole(QPalette::Background),
                        ui->function4->setText("Systemback " % tr("scheduler"));

                        if(sb::schdlr[0] == "topleft")
                        {
                            ushort sz(ss(30));
                            wgeom[0] = sgm.x() + sz, wgeom[1] = sgm.y() + sz;
                        }
                        else if(sb::schdlr[0] == "center")
                            wgeom[0] = sgm.x() + sgm.width() / 2 - ss(201), wgeom[1] = sgm.y() + sgm.height() / 2 - ss(80);
                        else if(sb::schdlr[0] == "bottomleft")
                            wgeom[0] = sgm.x() + ss(30), wgeom[1] = sgm.y() + sgm.height() - ss(191);
                        else if(sb::schdlr[0] == "bottomright")
                            wgeom[0] = sgm.x() + sgm.width() - ss(432), wgeom[1] = sgm.y() + sgm.height() - ss(191);
                        else
                            wgeom[0] = sgm.x() + sgm.width() - ss(432), wgeom[1] = sgm.y() + ss(30);

                        setFixedSize(wgeom[2] = ss(402), wgeom[3] = ss(161)),
                        move(wgeom[0], wgeom[1]);

                        QTimer::singleShot(0, this,
#if QT_VERSION < QT_VERSION_CHECK(5, 4, 0)
                            SLOT(schedulertimer())
#else
                            &systemback::schedulertimer
#endif
                            );

                        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
                    }
                    else
                    {
                        ui->schedulerpanel->hide();

                        if(sislive && args.count() == 2 && args.at(1) == "finstall")
                        {
                            resize(sgm.size());
                            for(QWdt wdgt : QWL{ui->wallpaper, ui->logo}) wdgt->move(0, 0);
                            fscrn = true, wgeom[2] = ss(698), wgeom[3] = ss(465),
                            ui->wpanel->setGeometry(wgeom[0] = sgm.width() < wgeom[2] ? 0 : sgm.x() + width() / 2 - ss(349), wgeom[1] = sgm.height() < wgeom[3] ? 0 : sgm.y() + height() / 2 - ss(232), wgeom[2], wgeom[3]);

                            connect(ui->wpanel, &pnlevent::Move, [this] {
                                    if(fscrn && ! (wismax || wmblck))
                                    {
                                        if(wgeom[0] != ui->wpanel->x()) wgeom[0] = ui->wpanel->x();
                                        if(wgeom[1] != ui->wpanel->y()) wgeom[1] = ui->wpanel->y();
                                    }
                                });

                            connect(ui->logo, &lblevent::Mouse_Click, [this] {
                                    if(ui->wpanel->isHidden()) ui->wpanel->show(),
                                                               fwdgt->setFocus(),
                                                               ui->logo->setFocusPolicy(Qt::NoFocus);
                                });

                            on_installmenu_clicked(),
                            ui->installback->hide(),
                            setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
                            return ui->wpanel;
                        }
                        else
                            setFixedSize(wgeom[2] = ss(698), wgeom[3] = ss(465)),
                            move(wgeom[0] = sgm.x() + sgm.width() / 2 - ss(349), wgeom[1] = sgm.y() + sgm.height() / 2 - ss(232));
                    }
                }

                return this;
            }();
    }

    connect(ui->wpanel, &pnlevent::Resize, [this] {
            for(QWdt wdgt : QWL{ui->mainpanel, ui->resizepanel}) wdgt->resize(wndw->size());
            for(QWdt wdgt : QWL{ui->function1, ui->border, ui->resizeborder}) wdgt->resize(wndw->width(), wdgt->height());
            ui->windowbutton1->move(wndw->width() - ui->windowbutton1->height(), 0),
            ui->subpanel->resize(wndw->width() - ui->subpanel->x(), wndw->height() - ss(24));

            if(ui->copypanel->isVisibleTo(ui->wpanel))
            {
                ui->copypanel->resize(wndw->width() - ui->copypanel->x() * 2, wndw->height() - ss(25)),
                ui->partitionsettingstext->resize(ui->copypanel->width(), ui->partitionsettingstext->height()),
                ui->partitionsettings->resize(ui->copypanel->width() - ss(152), ui->copypanel->height() - ss(200)),
                ui->partitionsettingspanel1->move(ui->partitionsettings->x() + ui->partitionsettings->width(), ui->partitionsettingspanel1->y());
                for(QWdt wdgt : QWL{ui->partitionsettingspanel2, ui->partitionsettingspanel3}) wdgt->move(ui->partitionsettingspanel1->pos());
                ui->partitionoptionstext->setGeometry(0, ui->copypanel->height() - ss(160), ui->copypanel->width(), ui->partitionoptionstext->height()),
                ui->userdatafilescopy->move(ui->userdatafilescopy->x(), ui->copypanel->height() - ss(122) - (sfctr == High ? 1 : 2)),
                ui->usersettingscopy->move(ui->usersettingscopy->x(), ui->userdatafilescopy->y()),
                ui->grubinstalltext->move(ui->grubinstalltext->x(), ui->copypanel->height() - ss(96)),
                ui->grubinstallcopy->move(ui->grubinstallcopy->x(), ui->grubinstalltext->y()),
                ui->grubinstallcopydisable->move(ui->grubinstallcopy->pos()),
                ui->efiwarning->setGeometry(ui->efiwarning->x(), ui->grubinstalltext->y() - ss(4), ui->copypanel->width() - ui->efiwarning->x() - ss(8), ui->efiwarning->height()),
                ui->copyback->move(ui->copyback->x(), ui->copypanel->height() - ss(48)),
                ui->copynext->move(ui->partitionsettings->width(), ui->copyback->y()),
                ui->copyresize->move(ui->copypanel->width() - ui->copyresize->width(), ui->copypanel->height() - ui->copyresize->height()),
                ui->copycover->resize(ui->copypanel->width() - ss(10), ui->copypanel->height() - ss(10));
            }

            if(fscrn && ! (wismax || wmblck))
            {
                if(wgeom[2] != wndw->width()) wgeom[2] = wndw->width();
                if(wgeom[3] != wndw->height()) wgeom[3] = wndw->height();
            }
        });

    if(! fscrn)
    {
        for(QWdt wdgt : QWL{ui->wallpaper, ui->logo}) wdgt->hide();
        if(sb::waot && ! windowFlags().testFlag(Qt::WindowStaysOnTopHint)) setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
        Atom atm(XInternAtom(dsply, "_MOTIF_WM_HINTS", 0));
        ulong hnts[]{3, 44, 0, 0, 0};
        XChangeProperty(dsply, winId(), atm, atm, 32, PropModeReplace, (uchar *)&hnts, 5);
    }

    XFlush(dsply), installEventFilter(this);
}

systemback::~systemback()
{
    if(! ocfg.isEmpty())
    {
        if(ocfg != (sb::sdir[0] % sb::sdir[2] % sb::schdlr[0] % sb::schdlr[1] % sb::lang % sb::style % sb::wsclng).toUtf8() % char(sb::ismpnt) % char(sb::pnumber) % char(sb::incrmtl) % char(sb::xzcmpr) % char(sb::autoiso) % char(sb::schdle[0]) % char(sb::schdle[1]) % char(sb::schdle[2]) % char(sb::schdle[3]) % char(sb::schdle[4]) % char(sb::schdle[5]) % char(sb::waot) % char(sb::ecache)) sb::cfgwrite();

        if(fscrn && sb::isfile("/usr/bin/ksplashqml"))
        {
            for(cQStr &item : QDir("/usr/bin").entryList(QDir::Files))
                if(sb::like(item, {"_ksplash*", "_plasma*"})) cfmod(bstr("/usr/bin/" % item), 0755);

            if(sb::isfile("/usr/share/autostart/plasma-desktop.desktop_")) sb::rename("/usr/share/autostart/plasma-desktop.desktop_", "/usr/share/autostart/plasma-desktop.desktop");
            if(sb::isfile("/usr/share/autostart/plasma-netbook.desktop_")) sb::rename("/usr/share/autostart/plasma-netbook.desktop_", "/usr/share/autostart/plasma-netbook.desktop");
        }

        if(! nrxth)
        {
            QBA xauth(qgetenv("XAUTHORITY"));
            if(xauth.startsWith("/tmp/sbXauthority-")) sb::rmfile(xauth);
        }
    }

    for(QTimer *tmr : {shdltimer, dlgtimer, intrptimer})
        if(tmr) delete tmr;

    delete ui, XCloseDisplay(dsply);
}

void systemback::closeEvent(QCloseEvent *ev)
{
    ui->statuspanel->isVisible() && ! (sstart && sb::ThrdKill) && prun.type != 11 ? ev->ignore() : removeEventFilter(this);
}

void systemback::unitimer()
{
    if(! utblck)
    {
        utblck = true;

        if(! utimer.isActive())
        {
            switch(sb::pnumber) {
            case 3:
                ui->pnumber3->setChecked(true),
                on_pnumber3_clicked();
                break;
            case 4:
                ui->pnumber4->setChecked(true),
                on_pnumber4_clicked();
                break;
            case 6:
                ui->pnumber6->setChecked(true),
                on_pnumber6_clicked();
                break;
            case 7:
                ui->pnumber7->setChecked(true),
                on_pnumber7_clicked();
                break;
            case 8:
                ui->pnumber8->setChecked(true),
                on_pnumber8_clicked();
                break;
            case 9:
                ui->pnumber9->setChecked(true),
                on_pnumber9_clicked();
                break;
            case 10:
                ui->pnumber10->setChecked(true),
                on_pnumber10_clicked();
            }

            if(! sstart)
            {
                ui->storagedir->setText(sb::sdir[0]),
                ui->storagedir->setToolTip(sb::sdir[0]),
                ui->liveworkdir->setText(sb::sdir[2]),
                ui->liveworkdir->setToolTip(sb::sdir[2]);
                for(QLE ldt : QList<QLE>{ui->storagedir, ui->liveworkdir}) ldt->setCursorPosition(0);

                for(QWdt wdgt : QWL{ui->restorepanel, ui->copypanel, ui->livepanel, ui->repairpanel, ui->excludepanel, ui->includepanel, ui->schedulepanel, ui->aboutpanel, ui->licensepanel, ui->settingspanel, ui->choosepanel}) wdgt->move(ui->sbpanel->pos()),
                                                                                                                                                                                                                                   wdgt->setBackgroundRole(QPalette::Background);

                for(QWdt wdgt : QWL{ui->liveworkdirarea, ui->schedulerday, ui->schedulerhour, ui->schedulerminute, ui->schedulersecond}) wdgt->setBackgroundRole(QPalette::Base);
                { QPalette pal(ui->license->palette());
                pal.setBrush(QPalette::Base, pal.background()),
                ui->license->setPalette(pal); }
                ui->partitionsettings->setHorizontalHeaderLabels({tr("Partition"), tr("Size"), tr("Label"), tr("Current mount point"), tr("New mount point"), tr("Filesystem"), tr("Format")});
                for(uchar a(7) ; a < 11 ; ++a) ui->partitionsettings->setColumnHidden(a, true);
                { QFont fnt;
                fnt.setPixelSize(ss(14));
                for(QTblW wdgt : QList<QTblW>{ui->partitionsettings, ui->livedevices}) wdgt->horizontalHeader()->setFont(fnt); }
                for(uchar a : {0, 1, 5, 6}) ui->partitionsettings->horizontalHeader()->setSectionResizeMode(a, QHeaderView::Fixed);
                ui->livedevices->setHorizontalHeaderLabels({tr("Partition"), tr("Size"), tr("Device"), tr("Format")});
                for(uchar a : {0, 1, 3}) ui->livedevices->horizontalHeader()->setSectionResizeMode(a, QHeaderView::Fixed);
                for(QWdt wdgt : QWL{ui->livenamepipe, ui->livenameerror, ui->partitionsettingspanel2, ui->partitionsettingspanel3, ui->grubreinstallrestoredisable, ui->grubreinstallrepairdisable, ui->usersettingscopy, ui->repaircover}) wdgt->hide();
                for(QCbB cmbx : QCbBL{ui->grubreinstallrestoredisable, ui->grubinstallcopydisable, ui->grubreinstallrepairdisable}) cmbx->addItem(tr("Disabled"));
                if(sb::schdle[0]) ui->schedulerstate->click();
                if(sb::schdle[5]) ui->silentmode->setChecked(true);
                ui->windowposition->addItems({tr("Top left"), tr("Top right"), tr("Center"), tr("Bottom left"), tr("Bottom right")});
                if(sb::schdlr[0] != "topleft") ui->windowposition->setCurrentIndex(ui->windowposition->findText(sb::schdlr[0] == "topright" ? tr("Top right") : sb::schdlr[0] == "center" ? tr("Center") : sb::schdlr[0] == "bottomleft" ? tr("Bottom left") : tr("Bottom right")));
                ui->schedulerday->setText(QStr::number(sb::schdle[1]) % ' ' % tr("day(s)")),
                ui->schedulerhour->setText(QStr::number(sb::schdle[2]) % ' ' % tr("hour(s)")),
                ui->schedulerminute->setText(QStr::number(sb::schdle[3]) % ' ' % tr("minute(s)")),
                ui->schedulersecond->setText(QStr::number(sb::schdle[4]) % ' ' % tr("seconds"));

                auto lodsbl([this](bool chkd = false) {
                        ui->languageoverride->setDisabled(true);
                        if(chkd || sb::lang != "auto") sb::lang = "auto";
                    });

                if(sb::isdir("/usr/share/systemback/lang"))
                {
                    {
                        QSL lst("English (common)");
                        lst.reserve(13);

                        for(cQStr &item : QDir("/usr/share/systemback/lang").entryList(QDir::Files))
                        {
                            QStr lcode(sb::left(sb::right(item, -11), -3));

                            cchar *lname(lcode == "ar_EG" ? "المصرية العربية"
                                       : lcode == "ca_ES" ? "Català"
                                       : lcode == "cs" ? "Čeština"
                                       : lcode == "da_DK" ? "Dansk"
                                       : lcode == "de" ? "Deutsch"
                                       : lcode == "en_GB" ? "English (United Kingdom)"
                                       : lcode == "es" ? "Español"
                                       : lcode == "fi" ? "Suomi"
                                       : lcode == "fr" ? "Français"
                                       : lcode == "gl_ES" ? "Galego"
                                       : lcode == "hu" ? "Magyar"
                                       : lcode == "id" ? "Bahasa Indonesia"
                                       : lcode == "pt_BR" ? "Português (Brasil)"
                                       : lcode == "ro" ? "Română"
                                       : lcode == "ru" ? "Русский"
                                       : lcode == "tr" ? "Türkçe"
                                       : lcode == "uk" ? "Українськa"
                                       : lcode == "zh_CN" ? "中文（简体）" : nullptr);

                            if(lname) lst.append(lname);
                        }

                        if(lst.count() == 1)
                            lodsbl();
                        else
                        {
                            lst.sort();

                            if(sb::lang == "auto")
                                ui->languages->addItems(lst);
                            else
                            {
                                schar indx(sb::lang == "id_ID" ? 0
                                         : sb::lang == "ar_EG" ? lst.indexOf("المصرية العربية")
                                         : sb::lang == "ca_ES" ? lst.indexOf("Català")
                                         : sb::lang == "cs_CS" ? lst.indexOf("Čeština")
                                         : sb::lang == "da_DK" ? lst.indexOf("Dansk")
                                         : sb::lang == "de_DE" ? lst.indexOf("Deutsch")
                                         : sb::lang == "en_EN" ? lst.indexOf("English (common)")
                                         : sb::lang == "en_GB" ? lst.indexOf("English (United Kingdom)")
                                         : sb::lang == "es_ES" ? lst.indexOf("Español")
                                         : sb::lang == "fi_FI" ? lst.indexOf("Suomi")
                                         : sb::lang == "fr_FR" ? lst.indexOf("Français")
                                         : sb::lang == "gl_ES" ? lst.indexOf("Galego")
                                         : sb::lang == "hu_HU" ? lst.indexOf("Magyar")
                                         : sb::lang == "pt_BR" ? lst.indexOf("Português (Brasil)")
                                         : sb::lang == "ro_RO" ? lst.indexOf("Română")
                                         : sb::lang == "ru_RU" ? lst.indexOf("Русский")
                                         : sb::lang == "tr_TR" ? lst.indexOf("Türkçe")
                                         : sb::lang == "uk_UK" ? lst.indexOf("Українськa")
                                         : sb::lang == "zh_CN" ? lst.indexOf("中文（简体）") : -1);

                                if(indx == -1)
                                    lodsbl(true);
                                else
                                {
                                    ui->languageoverride->setChecked(true),
                                    ui->languages->addItems(lst);
                                    if(indx) ui->languages->setCurrentIndex(indx);
                                    ui->languages->setEnabled(true);
                                }
                            }
                        }
                    }

                    for(QLbl lbl : findChildren<QLbl>())
                        if(lbl->alignment() == (Qt::AlignLeft | Qt::AlignVCenter) && lbl->text().isRightToLeft()) lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                }
                else
                    lodsbl();

                { QSL kys(QStyleFactory::keys());
                kys.sort(), ui->styles->addItems(kys); }

                if(sb::style != "auto") ui->styleoverride->setChecked(true),
                                        ui->styles->setCurrentIndex(ui->styles->findText(sb::style)),
                                        ui->styles->setEnabled(true);

                if(sb::waot) ui->alwaysontop->setChecked(true);
                if(! sb::incrmtl) ui->incrementaldisable->setChecked(true);
                if(! sb::ecache) ui->cachemptydisable->setChecked(true);
                if(sb::xzcmpr) ui->usexzcompressor->setChecked(true);
                if(sb::autoiso) ui->autoisocreate->setChecked(true);

                if(sb::schdlr[1] != "false")
                {
                    if(sb::schdlr[1] == "everyone")
                        ui->schedulerdisable->click(),
                        ui->schedulerusers->setText(tr("Everyone"));
                    else
                    {
                        ui->schedulerdisable->setChecked(true);
                        QSL susr(sb::right(sb::schdlr[1], -1).split(','));
                        if(! susr.contains("root")) ui->users->addItem("root");
                        QFile file("/etc/passwd");

                        if(sb::fopen(file))
                            while(! file.atEnd())
                            {
                                QStr usr(file.readLine().trimmed());
                                if(usr.contains(":/home/") && ! susr.contains(usr = sb::left(usr, sb::instr(usr, ":") -1)) && sb::isdir("/home/" % usr)) ui->users->addItem(usr);
                            }

                        if(ui->users->count())
                            for(QWdt wdgt : QWL{ui->users, ui->adduser}) wdgt->setEnabled(true);

                        ui->schedulerusers->setText(sb::right(sb::schdlr[1], -1)),
                        ui->schedulerrefresh->setEnabled(true);
                    }

                    ui->schedulerusers->setCursorPosition(0);
                }

                ui->version->setText(sb::appver());
                ushort sz[]{ss(7), ss(8), ss(28), ss(3)};
                for(QLbl lbl : QLbL{ui->includeuserstext, ui->grubreinstallrestoretext, ui->grubinstalltext, ui->grubreinstallrepairtext, ui->schedulerstatetext, ui->schedulersecondtext, ui->windowpositiontext, ui->homepage1, ui->homepage2, ui->email, ui->donate, ui->version, ui->filesystemwarning}) lbl->resize(lbl->fontMetrics().width(lbl->text()) + sz[0], lbl->height());
                ui->includeusers->move(ui->includeuserstext->x() + ui->includeuserstext->width(), ui->includeusers->y()),
                ui->includeusers->setMaximumWidth(width() - ui->includeusers->x() - sz[1]),
                ui->grubreinstallrestore->move(ui->grubreinstallrestoretext->x() + ui->grubreinstallrestoretext->width(), ui->grubreinstallrestore->y()),
                ui->grubreinstallrestoredisable->move(ui->grubreinstallrestore->pos()),
                ui->grubinstallcopy->move(ui->grubinstalltext->x() + ui->grubinstalltext->width(), ui->grubinstallcopy->y()),
                ui->grubinstallcopydisable->move(ui->grubinstallcopy->pos()),
                ui->grubreinstallrepair->move(ui->grubreinstallrepairtext->x() + ui->grubreinstallrepairtext->width(), ui->grubreinstallrepair->y()),
                ui->grubreinstallrepairdisable->move(ui->grubreinstallrepair->pos()),
                ui->schedulerstate->move(ui->schedulerstatetext->x() + ui->schedulerstatetext->width(), ui->schedulerstate->y()),
                ui->schedulersecondpanel->move(ui->schedulersecondtext->x() + ui->schedulersecondtext->width(), ui->schedulersecondpanel->y()),
                ui->windowposition->move(ui->windowpositiontext->x() + ui->windowpositiontext->width(), ui->windowposition->y());
                for(QCB ckbx : QList<QCB>{ui->format, ui->keepfiles, ui->autorestoreoptions, ui->skipfstabrestore, ui->autorepairoptions, ui->skipfstabrepair, ui->userdatafilescopy, ui->userdatainclude, ui->silentmode, ui->languageoverride, ui->styleoverride, ui->alwaysontop, ui->incrementaldisable, ui->cachemptydisable, ui->usexzcompressor, ui->autoisocreate, ui->schedulerdisable}) ckbx->resize(fontMetrics().width(ckbx->text()) + sz[2], ckbx->height());
                ui->usersettingscopy->resize(fontMetrics().width(tr("Transfer user configuration and data files")) + sz[2], ui->usersettingscopy->height()),
                ui->usersettingscopy->setCheckState(Qt::PartiallyChecked),
                ui->format->move((ui->partitionsettingspanel1->width() - ui->format->width()) / 2, ui->format->y());
                for(QRB rbtn : QList<QRB>{ui->fullrestore, ui->systemrestore, ui->configurationfilesrestore, ui->systemrepair, ui->fullrepair, ui->grubrepair, ui->pointexclude, ui->liveexclude}) rbtn->resize(fontMetrics().width(rbtn->text()) + sz[2], rbtn->height());
                ui->languages->move(ui->languageoverride->x() + ui->languageoverride->width() + sz[3], ui->languages->y()),
                ui->languages->setMaximumWidth(width() - ui->languages->x() - sz[1]),
                ui->styles->move(ui->styleoverride->x() + ui->styleoverride->width() + sz[3], ui->styles->y()),
                ui->styles->setMaximumWidth(width() - ui->styles->x() - sz[1]),
                ui->filesystem->addItems({"ext4", "ext3", "ext2"});

                for(cQStr &fs : {"btrfs", "reiserfs", "jfs", "xfs"})
                    if(sb::execsrch("mkfs." % fs)) ui->filesystem->addItem(fs);

                ui->repairmountpoint->addItems({nullptr, "/mnt", "/mnt/home", "/mnt/boot"});
#ifdef __amd64__
                if(sb::isdir("/sys/firmware/efi")) goto isefi;

                {
                    QStr ckernel(ckname());

                    if(sb::isfile("/lib/modules/" % ckernel % "/modules.builtin"))
                    {
                        QFile file("/lib/modules/" % ckernel % "/modules.builtin");

                        if(sb::fopen(file))
                            while(! file.atEnd())
                                if(file.readLine().trimmed().endsWith("efivars.ko")) goto noefi;
                    }

                    if(sb::isfile("/proc/modules") && ! sb::fload("/proc/modules").contains("efivars ") && sb::isfile("/lib/modules/" % ckernel % "/modules.order"))
                    {
                        QFile file("/lib/modules/" % ckernel % "/modules.order");

                        if(sb::fopen(file))
                            while(! file.atEnd())
                            {
                                QBA cline(file.readLine().trimmed());
                                if(cline.endsWith("efivars.ko") && sb::isfile("/lib/modules/" % ckernel % '/' % cline) && ! sb::exec("modprobe efivars", sb::Silent) && sb::isdir("/sys/firmware/efi")) goto isefi;
                            }
                    }
                }

                goto noefi;
            isefi:
                grub.name = "efi-amd64-bin", grub.isEFI = true,
                ui->repairmountpoint->addItem("/mnt/boot/efi"),
                ui->grubinstallcopy->hide();
                for(QCbB cmbx : QCbBL{ui->grubinstallcopy, ui->grubreinstallrestore, ui->grubreinstallrepair}) cmbx->addItems({"EFI", tr("Disabled")});
                ui->grubinstallcopydisable->adjustSize(),
                ui->efiwarning->move(ui->grubinstallcopydisable->x() + ui->grubinstallcopydisable->width() + ss(5), ui->grubinstallcopydisable->y() - ss(4)),
                ui->efiwarning->resize(ui->copypanel->width() - ui->efiwarning->x() - sz[1], ui->efiwarning->height()),
                ui->efiwarning->setForegroundRole(QPalette::Highlight);
                goto next_1;
            noefi:
#endif
                grub.name = "pc-bin", grub.isEFI = false;
                for(QWdt wdgt : QWL{ui->grubinstallcopydisable, ui->efiwarning}) wdgt->hide();
#ifdef __amd64__
            next_1:
#endif
                ui->repairmountpoint->addItems({"/mnt/usr", "/mnt/var", "/mnt/opt", "/mnt/usr/local"}),
                ui->repairmountpoint->setCurrentIndex(1);

                nohmcpy[0] = sb::isfile("/etc/fstab") && [] {
                        QFile file("/etc/fstab");

                        if(sb::fopen(file))
                        {
                            QSL incl{"* /home *", "* /home/ *"};

                            while(! file.atEnd())
                            {
                                QStr cline(file.readLine().trimmed());
                                if(! cline.startsWith('#') && sb::like(cline.replace('\t', ' '), incl)) return true;
                            }
                        }

                        return false;
                    }();

                on_partitionrefresh_clicked(),
                on_livedevicesrefresh_clicked(),
                on_pointexclude_clicked();

                {
                    QFile file(incfile);

                    if(sb::fopen(file))
                        while(! file.atEnd())
                        {
                            QStr cline(sb::left(file.readLine(), -1));
                            if(! cline.isEmpty()) ui->includedlist->addItem(cline);
                        }
                }

                ilstupdt(true),
                ui->storagedir->resize(ss(210), sz[2]),
                ui->storagedirbutton->show();
                for(QWdt wdgt : QWL{ui->repairmenu, ui->aboutmenu, ui->settingsmenu, ui->pnumber3, ui->pnumber4, ui->pnumber5, ui->pnumber6, ui->pnumber7, ui->pnumber8, ui->pnumber9, ui->pnumber10}) wdgt->setEnabled(true);

                if(! sislive)
                {
                    ickernel = [this] {
                            QStr ckernel(ckname()), fend[]{"order", "builtin"};

                            for(uchar a(0) ; a < 2 ; ++a)
                                if(sb::isfile("/lib/modules/" % ckernel % "/modules." % fend[a]))
                                {
                                    QFile file("/lib/modules/" % ckernel % "/modules." % fend[a]);

                                    if(sb::fopen(file))
                                    {
                                        QSL incl{"*aufs.ko_", "*overlay.ko_", "*overlayfs.ko_", "*unionfs.ko_"};

                                        while(! file.atEnd())
                                            if(sb::like(file.readLine().trimmed(), incl)) return true;
                                    }
                                }

                            return sb::execsrch("unionfs-fuse");
                        }();

                    for(QWdt wdgt : QWL{ui->copymenu, ui->installmenu, ui->systemupgrade, ui->excludemenu, ui->includemenu, ui->schedulemenu}) wdgt->setEnabled(true);
                    pname = tr("Currently running system");
                }
            }

            pntupgrade(),
            busy(false);

            connect(&utimer,
#if QT_VERSION < QT_VERSION_CHECK(5, 3, 0)
                SIGNAL(timeout()), this, SLOT(unitimer())
#else
                &QTimer::timeout, this, &systemback::unitimer
#endif
                );

            utimer.start(500);

            if(sstart)
                ui->schedulerstart->setEnabled(true);
            else if(sislive && sb::exist("/etc/xdg/autostart/sbfinstall.desktop"))
                for(cchar *file : {"/etc/xdg/autostart/sbfinstall.desktop", "/etc/xdg/autostart/sbfinstall-kde.desktop"}) sb::remove(file);
        }
        else if(ui->statuspanel->isVisibleTo(ui->wpanel))
        {
            if(prun.type)
            {
                ui->processrun->setText(prun.txt % [this] {
                        switch(++prun.pnts) {
                        case 1:
                            return " .  ";
                        case 2:
                            return " .. ";
                        case 3:
                            return " ...";
                        default:
                            prun.pnts = 0;
                            return "    ";
                        }
                    }());

                switch(prun.type) {
                case 2 ... 6:
                case 8 ... 10:
                case 15:
                case 19 ... 21:
                {
                    if(irblck)
                    {
                        if(ui->interrupt->isEnabled()) ui->interrupt->setDisabled(true);
                    }
                    else if(! ui->interrupt->isEnabled())
                        ui->interrupt->setEnabled(true);

                    schar cperc(sb::Progress);

                    if(cperc > -1)
                    {
                        if(! ui->progressbar->maximum()) ui->progressbar->setMaximum(100);

                        if(cperc > 99)
                        {
                            if(ui->progressbar->value() < 100) ui->progressbar->setValue(100);
                        }
                        else if(ui->progressbar->value() < cperc)
                            ui->progressbar->setValue(cperc);
                        else if(sb::like(99, {cperc, ui->progressbar->value()}, true))
                            ui->progressbar->setValue(100);
                    }
                    else if(ui->progressbar->maximum() == 100)
                        ui->progressbar->setMaximum(0);

                    break;
                }
                case 18:
                {
                    if(! ui->interrupt->isEnabled()) ui->interrupt->setEnabled(true);
                    schar cperc(sb::Progress);

                    if(cperc > -1)
                    {
                        if(! ui->progressbar->maximum()) ui->progressbar->setMaximum(100);
                        if(cperc < 101 && ui->progressbar->value() < cperc) ui->progressbar->setValue(cperc);
                    }
                    else if(ui->progressbar->maximum() == 100)
                        ui->progressbar->setMaximum(0);

                    break;
                }
                default:
                    if(! irblck && sb::like(prun.type, {12, 14, 16, 17}))
                    {
                        if(! ui->interrupt->isEnabled()) ui->interrupt->setEnabled(true);
                    }
                    else if(ui->interrupt->isEnabled())
                        ui->interrupt->setDisabled(true);

                    if(ui->progressbar->maximum() == 100) ui->progressbar->setMaximum(0);
                }
            }
        }
        else if(! sstart)
        {
            if(! fscrn)
            {
                auto acserr([this] {
                        pntupgrade();

                        if(ui->dialogquestion->isVisible())
                            on_dialogcancel_clicked();
                        else if(ui->restorepanel->isVisible())
                            on_restoreback_clicked();
                        else if(ui->copypanel->isVisible())
                        {
                            on_copyback_clicked();
                            if(ui->function1->text() != "Systemback") on_installback_clicked();
                        }
                        else if(ui->installpanel->isVisible())
                            on_installback_clicked();
                        else if(ui->repairpanel->isVisible())
                            on_repairback_clicked();
                    });

                if(sb::isdir(sb::sdir[1]) && sb::access(sb::sdir[1], sb::Write))
                {
                    if(! ui->storagedirarea->styleSheet().isEmpty()) ui->storagedirarea->setStyleSheet(nullptr),
                                                                     pntupgrade();

                    if(! (ppipe || ui->newrestorepoint->isEnabled()) && pname == tr("Currently running system")) ui->newrestorepoint->setEnabled(true);
                    schar num(-1);

                    for(QLE ldt : ui->points->findChildren<QLE>())
                    {
                        ++num;

                        if(ldt->isEnabled() && ! sb::isdir(sb::sdir[1] % [num]() -> QStr {
                                switch(num) {
                                case 9:
                                    return "/S10_";
                                case 10 ... 14:
                                    return "/H0" % QStr::number(num - 9) % '_';
                                default:
                                    return "/S0" % QStr::number(num + 1) % '_';
                                }
                            }() % sb::pnames[num]))
                        {
                            acserr();
                            break;
                        }
                    }
                }
                else
                {
                    if(ui->point1->isEnabled() || ui->pointpipe11->isEnabled()) acserr();
                    if(ui->newrestorepoint->isEnabled()) ui->newrestorepoint->setDisabled(true);
                    if(ui->storagedirarea->styleSheet().isEmpty()) ui->storagedirarea->setStyleSheet("background-color: rgb(255, 103, 103)");
                }
            }

            if(ui->installpanel->isVisible())
            {
                if(ui->installmenu->isEnabled() && ui->fullnamepipe->isVisible() && ui->usernamepipe->isVisible() && ui->hostnamepipe->isVisible() && ui->passwordpipe->isVisible() && (ui->rootpassword1->text().isEmpty() || ui->rootpasswordpipe->isVisible()) && ! ui->installnext->isEnabled()) ui->installnext->setEnabled(true);
            }
            else if(ui->livepanel->isVisible())
            {
                if(ui->livenameerror->isVisible() || ui->liveworkdir->text().isEmpty())
                {
                    if(ui->livenew->isEnabled()) ui->livenew->setDisabled(true);
                }
                else if(ui->livenamepipe->isVisible() || ui->livename->text() == "auto")
                {
                    if(sb::isdir(sb::sdir[2]) && sb::access(sb::sdir[2], sb::Write))
                    {
                        if(! ui->liveworkdirarea->styleSheet().isEmpty()) ui->liveworkdirarea->setStyleSheet(nullptr);
                        if(ickernel && ! ui->livenew->isEnabled()) ui->livenew->setEnabled(true);

                        if(! ui->livelist->isEnabled()) ui->livelist->setEnabled(true),
                                                        on_livemenu_clicked();
                    }
                    else
                    {
                        if(ui->liveworkdirarea->styleSheet().isEmpty()) ui->liveworkdirarea->setStyleSheet("background-color: rgb(255, 103, 103)");

                        for(QWdt wdgt : QWL{ui->livenew, ui->livedelete, ui->liveconvert, ui->livewritestart})
                            if(wdgt->isEnabled()) wdgt->setDisabled(true);

                        if(ui->livelist->isEnabled())
                        {
                            if(ui->livelist->count()) ui->livelist->clear();
                            ui->livelist->setDisabled(true);
                        }
                    }
                }
            }
            else if(ui->repairpanel->isVisible())
                rmntcheck();
        }

        if(ui->buttonspanel->isVisible() && ! (ui->buttonspanel->y() || minside(ui->buttonspanel))) bttnshide();
        utblck = false;
    }
}

QRect systemback::sgeom(bool rdc, QDW dtp)
{
    if(fscrn) return {QPoint(), size()};
    if(! dtp) dtp = qApp->desktop();
    return rdc ? dtp->availableGeometry(dtp->screenNumber(this)) : dtp->screenGeometry(dtp->screenNumber(this));
}

inline ushort systemback::ss(ushort dsize)
{
    switch(sfctr) {
    case Max:
        return dsize * 2;
    case High:
        switch(dsize) {
        case 0:
            return 0;
        case 1:
            return 2;
        default:
            ushort ssize((dsize * High + 50) / 100);

            switch(dsize) {
            case 154:
            case 201:
            case 402:
            case 506:
            case 698:
                return ssize + 1;
            default:
                return ssize;
            }
        }
    default:
        return dsize;
    }
}

QLE systemback::getpoint(uchar num)
{
    schar cnum(-1);

    for(QLE ldt : ui->points->findChildren<QLE>())
        if(++cnum == num) return ldt;

    return nullptr;
}

QCB systemback::getppipe(uchar num)
{
    schar cnum(-1);

    for(QCB ckbx : ui->sbpanel->findChildren<QCB>())
        if(++cnum == num) return ckbx;

    return nullptr;
}

void systemback::busy(bool state)
{
    state ? ++busycnt : --busycnt;

    switch(busycnt) {
    case 0:
        if(qApp->overrideCursor()->shape() == Qt::WaitCursor) qApp->restoreOverrideCursor();
        break;
    case 1:
        if(! qApp->overrideCursor()) qApp->setOverrideCursor(Qt::WaitCursor);
    }
}

inline bool systemback::minside(QWdt wgt)
{
    return wgt->rect().contains(wgt->mapFromGlobal(QCursor::pos()));
}

inline bool systemback::minside(cQRect &rct)
{
    return rct.contains(mapFromGlobal(QCursor::pos()));
}

QStr systemback::guname()
{
    if(! uchkd && (! ui->admins->count() || ui->admins->currentText() == "root"))
    {
        uchkd = true;
        QSL usrs;

        {
            QFile file("/etc/passwd");

            if(sb::fopen(file))
            {
                QSL incl{"*:x:1000:10*", "*:x:1001:10*", "*:x:1002:10*", "*:x:1003:10*", "*:x:1004:10*", "*:x:1005:10*", "*:x:1006:10*", "*:x:1007:10*", "*:x:1008:10*", "*:x:1009:10*", "*:x:1010:10*", "*:x:1011:10*", "*:x:1012:10*", "*:x:1013:10*", "*:x:1014:10*", "*:x:1015:10*"};

                while(! file.atEnd())
                {
                    QStr usr(file.readLine().trimmed());
                    if(sb::like(usr, incl)) usrs.append(sb::left(usr, sb::instr(usr, ":") -1));
                }
            }
        }

        if(usrs.count())
        {
            QStr uname;

            for(cQStr &usr : usrs)
                if(sb::isdir("/home/" % usr))
                {
                    uname = usr;
                    break;
                }

            if(uname.isEmpty()) uname = usrs.at(0);
            if(ui->admins->findText(uname) == -1) ui->admins->addItem(uname);
            if(ui->admins->count() > 1) ui->admins->setCurrentIndex(ui->admins->findText(uname));
        }
        else if(ui->admins->currentText().isNull())
            ui->admins->addItem("root");
    }

    return ui->admins->currentText();
}

QStr systemback::ckname()
{
    utsname snfo;
    uname(&snfo);
    return snfo.release;
}

void systemback::pset(uchar type, cbstr &tend)
{
    prun.txt = [type, &tend]() -> QStr {
            switch(type) {
            case 1:
                return sb::ecache ? tr("Emptying cache") : tr("Flushing filesystem buffers");
            case 2:
                return tr("Restoring the full system");
            case 3:
                return tr("Restoring the system files");
            case 4:
                return tr("Restoring the user(s) configuration files");
            case 5:
                return tr("Repairing the system files");
            case 6:
                return tr("Repairing the full system");
            case 7:
                return tr("Repairing the GRUB 2");
            case 8:
                return tr("Copying the system");
            case 9:
                return tr("Installing the system");
            case 10:
                return tr("Writing Live image to the target device");
            case 11:
                return tr("Upgrading the system");
            case 12:
                return tr("Deleting incomplete restore point");
            case 13:
                return tr("Interrupting the current process");
            case 14:
                return tr("Deleting old restore point") % ' ' % tend.data;
            case 15:
                return tr("Creating restore point");
            case 16:
                return tr("Deleting restore point") % ' ' % tend.data;
            case 21:
                return tr("Converting Live system image") % '\n' % tr("process") % tend.data;
            default:
                return tr("Creating Live system") % '\n' % tr("process") % tend.data % (type < 20 && sb::autoiso ? "+1" : nullptr);
            }
        }();

    prun.type = type;
}

void systemback::emptycache()
{
    pset(1), sb::fssync();
    if(sb::ecache) sb::crtfile("/proc/sys/vm/drop_caches", "3");
}

void systemback::stschange()
{
    QRect agm(sgeom(true)), rct([&]() -> QRect {
            if(wismax)
            {
                wismax = false;
                return {wgeom[0], wgeom[1], wgeom[2], wgeom[3]};
            }

            wismax = true,
            wndw->setMaximumSize(agm.size());
            return agm;
        }());

    wmblck = true;
    int vls[]{qAbs(rct.x() - wndw->x()) / 6, qAbs(rct.y() - wndw->y()) / 6, qAbs(rct.width() - wndw->width()) / 6, qAbs(rct.height() - wndw->height()) / 6};
    ui->resizepanel->show();

    for(uchar a(0) ; a < 6 ; ++a) wndw->setGeometry(qAbs(rct.x() - wndw->x()) > vls[0] ? wndw->x() - (rct.x() < wndw->x() ? vls[0] : -vls[0]) : rct.x(), qAbs(rct.y() - wndw->y()) > vls[1] ? wndw->y() - (rct.y() < wndw->y() ? vls[1] : -vls[1]) : rct.y(), qAbs(rct.width() - wndw->width()) > vls[2] ? wndw->width() - (rct.width() < wndw->width() ? vls[2] : -vls[2]) : rct.width(), qAbs(rct.height() - wndw->height()) > vls[3] ? wndw->height() - (rct.height() < wndw->height() ? vls[3] : -vls[3]) : rct.height()),
                                  QThread::msleep(1),
                                  repaint();

    wndw->setGeometry(rct.x(), rct.y(), rct.width(), rct.height()),
    ui->resizepanel->hide(),
    wmblck = false;

    if(! wismax)
    {
        ushort sz(ss(60));
        wndw->setMaximumSize(agm.width() - sz, agm.height() - sz);
    }
    else if(ui->copypanel->isVisible())
        for(uchar a(2) ; a < 5 ; ++a) ui->partitionsettings->resizeColumnToContents(a);
}

void systemback::schedulertimer()
{
    if(ui->schedulernumber->text().isEmpty())
    {
        ui->schedulernumber->setText(QStr::number(sb::schdle[4]) % 's');

        connect(shdltimer = new QTimer,
#if QT_VERSION < QT_VERSION_CHECK(5, 3, 0)
            SIGNAL(timeout()), this, SLOT(schedulertimer())
#else
            &QTimer::timeout, this, &systemback::schedulertimer
#endif
            );

        shdltimer->start(1000);
    }
    else if(ui->schedulernumber->text() == "1s")
        on_schedulerstart_clicked();
    else
        ui->schedulernumber->setText(QStr::number(sb::left(ui->schedulernumber->text(), -1).toUShort() - 1) % 's');
}

void systemback::dialogtimer()
{
    ui->dialognumber->setText(QStr::number(sb::left(ui->dialognumber->text(), -1).toUShort() - 1) % "s");
    if(ui->dialognumber->text() == "0s" && sb::like(ui->dialogok->text(), {'_' % tr("Reboot") % '_', '_' % tr("X restart") % '_'})) on_dialogok_clicked();
}

void systemback::wpressed()
{
    if(! wismax)
    {
        if(qApp->overrideCursor()) qApp->restoreOverrideCursor();
        qApp->setOverrideCursor(Qt::SizeAllCursor);
    }
}

void systemback::wreleased()
{
    if([] {
            QCr csr(qApp->overrideCursor());
            return csr && csr->shape() == Qt::SizeAllCursor;
        }())
    {
        qApp->restoreOverrideCursor();
        if(busycnt) qApp->setOverrideCursor(Qt::WaitCursor);
    }

    if(! (fscrn || wismax))
    {
        QDW dtp(qApp->desktop());
        QRect agm(sgeom(true, dtp)), sgm;
        ushort frm(ss(30));

        if(agm.width() >= wgeom[2] + frm)
        {
            sgm = sgeom(false, dtp);

            if(x() < sgm.x())
            {
                short val(sgm.x() + frm);
                wgeom[0] = val < agm.x() ? agm.x() : val;
            }
            else if(x() < agm.x())
                wgeom[0] = agm.x();
            else
            {
                short bnd[2];

                if(x() > (bnd[0] = sgm.x() + sgm.width() - wgeom[2]))
                {
                    short val(bnd[0] - frm);
                    wgeom[0] = val < (bnd[1] = agm.x() + agm.width() - wgeom[2]) ? val : bnd[1];
                }
                else if(x() > (bnd[0] = agm.x() + agm.width() - wgeom[2]))
                    wgeom[0] = bnd[0];
            }
        }
        else if(wgeom[0] != agm.x())
            wgeom[0] = agm.x();

        if(agm.height() >= wgeom[3] + frm)
        {
            if(sgm.isEmpty()) sgm = sgeom(false, dtp);

            if(y() < sgm.y())
            {
                short val(sgm.y() + frm);
                wgeom[1] = val < agm.y() ? agm.y() : val;
            }
            else if(y() < agm.y())
                wgeom[1] = agm.y();
            else
            {
                short bnd[2];

                if(y() > (bnd[0] = sgm.y() + sgm.height() - wgeom[3]))
                {
                    short val(bnd[0] - frm);
                    wgeom[1] = val < (bnd[1] = agm.y() + agm.height() - wgeom[3]) ? val : bnd[1];
                }
                else if(y() > (bnd[0] = agm.y() + agm.height() - wgeom[3]))
                    wgeom[1] = bnd[0];
            }
        }
        else if(wgeom[1] != agm.y())
            wgeom[1] = agm.y();

        if(pos() != QPoint(wgeom[0], wgeom[1])) move(wgeom[0], wgeom[1]);
    }
}

void systemback::benter(bool click)
{
    if(click || (qApp->mouseButtons() == Qt::NoButton && ui->function3->foregroundRole() == QPalette::Base))
    {
        if(ui->statuspanel->isVisible())
        {
            if(ui->windowmaximize->isVisibleTo(ui->buttonspanel))
            {
                ui->windowmaximize->hide();
                ushort sz(ss(2));
                ui->windowminimize->move(sz, sz);
            }

            if(ui->windowclose->isVisibleTo(ui->buttonspanel)) ui->windowclose->hide();
            if(ui->buttonspanel->width() != ui->buttonspanel->height()) ui->buttonspanel->resize(ui->buttonspanel->height(), ui->buttonspanel->height());
        }
        else if(ui->copypanel->isVisible() || ui->excludepanel->isVisible() || ui->includepanel->isVisible() || ui->choosepanel->isVisible())
        {
            if(! ui->windowmaximize->isVisibleTo(ui->buttonspanel)) ui->windowmaximize->show(),
                                                                    ui->windowminimize->move(ss(47), ss(2));

            if(wismax)
            {
                if(ui->windowmaximize->text() == "□") ui->windowmaximize->setText("▭");
            }
            else if(ui->windowmaximize->text() == "▭")
                ui->windowmaximize->setText("□");

            ushort sz(ss(92));
            if(ui->windowclose->x() != sz) ui->windowclose->move(sz, ss(2));
            if(! ui->windowclose->isVisibleTo(ui->buttonspanel)) ui->windowclose->show();
            if(ui->buttonspanel->width() != (sz = ss(138))) ui->buttonspanel->resize(sz, ui->buttonspanel->height());
        }
        else
        {
            if(ui->windowmaximize->isVisibleTo(ui->buttonspanel))
            {
                ui->windowmaximize->hide();
                ushort sz(ss(2));
                ui->windowminimize->move(sz, sz);
            }

            ushort sz(ss(47));
            if(ui->windowclose->x() != sz) ui->windowclose->move(sz, ss(2));
            if(! ui->windowclose->isVisibleTo(ui->buttonspanel)) ui->windowclose->show();
            if(ui->buttonspanel->width() != (sz = ss(93))) ui->buttonspanel->resize(sz, ui->buttonspanel->height());
        }

        wbleave(), bttnsshow();
    }
}

void systemback::bpressed()
{
    benter(true);
    if(ui->windowclose->isVisible() && qApp->mouseButtons() == Qt::LeftButton && minside(ui->windowclose)) ui->windowclose->setForegroundRole(QPalette::Highlight);
}

void systemback::bttnsshow()
{
    ui->buttonspanel->move(wndw->width() - ui->buttonspanel->width(), -ui->buttonspanel->height() + ss(27)),
    ui->buttonspanel->show();
    uchar a(ss(1));
    QRect rct({(fscrn ? ui->wpanel->x() : 0) + wndw->width() - ui->buttonspanel->width(), fscrn ? ui->wpanel->y() : 0, ui->buttonspanel->width(), ui->buttonspanel->height()});

    do {
        ui->buttonspanel->move(ui->buttonspanel->x(), ui->buttonspanel->y() + a),
        QThread::msleep(1),
        qApp->processEvents();
        if(! minside(rct)) return bttnshide();
    } while(ui->buttonspanel->y() < -a);

    ui->buttonspanel->move(ui->buttonspanel->x(), 0);
}

void systemback::bttnshide()
{
    schar a(ss(1)), b(-ui->buttonspanel->height() + ss(24) + a);
    QRect rct({(fscrn ? ui->wpanel->x() : 0) + wndw->width() - ui->buttonspanel->width(), fscrn ? ui->wpanel->y() : 0, ui->buttonspanel->width(), ui->buttonspanel->height()});

    do {
        ui->buttonspanel->move(ui->buttonspanel->x(), ui->buttonspanel->y() - a),
        QThread::msleep(1),
        qApp->processEvents();
        if(minside(rct)) return bttnsshow();
    } while(ui->buttonspanel->y() > b);

    ui->buttonspanel->hide();
}

void systemback::bmove()
{
    if(minside({(fscrn ? ui->wpanel->x() : 0) + wndw->width() - ui->buttonspanel->width(), fscrn ? ui->wpanel->y() : 0, ui->buttonspanel->width(), ui->buttonspanel->height()}))
    {
        if(ui->windowmaximize->isVisibleTo(ui->buttonspanel))
        {
            if(minside(ui->windowmaximize))
            {
                if(ui->windowmaximize->backgroundRole() == QPalette::Foreground) ui->windowmaximize->setBackgroundRole(QPalette::Background),
                                                                                 ui->windowmaximize->setForegroundRole(QPalette::Text);
            }
            else if(ui->windowmaximize->backgroundRole() == QPalette::Background)
                ui->windowmaximize->setBackgroundRole(QPalette::Foreground),
                ui->windowmaximize->setForegroundRole(QPalette::Base);
        }

        if(minside(ui->windowminimize))
        {
            if(ui->windowminimize->backgroundRole() == QPalette::Foreground) ui->windowminimize->setBackgroundRole(QPalette::Background),
                                                                             ui->windowminimize->setForegroundRole(QPalette::Text);
        }
        else if(ui->windowminimize->backgroundRole() == QPalette::Background)
            ui->windowminimize->setBackgroundRole(QPalette::Foreground),
            ui->windowminimize->setForegroundRole(QPalette::Base);

        if(ui->windowclose->isVisibleTo(ui->buttonspanel))
        {
            if(minside(ui->windowclose))
            {
                if(ui->windowclose->backgroundRole() == QPalette::Foreground) ui->windowclose->setBackgroundRole(QPalette::Background),
                                                                              ui->windowclose->setForegroundRole(QPalette::Text);
            }
            else if(ui->windowclose->backgroundRole() == QPalette::Background)
                ui->windowclose->setBackgroundRole(QPalette::Foreground),
                ui->windowclose->setForegroundRole(QPalette::Base);
        }

        if(ui->buttonspanel->isHidden() && minside(ui->windowbutton1)) bttnsshow();
    }
    else if(ui->buttonspanel->isVisible() && ! ui->buttonspanel->y())
        bttnshide();
}

void systemback::wbenter()
{
    QWdt wdgt(minside(ui->windowminimize) ? ui->windowminimize : ui->windowclose->isVisible() && minside(ui->windowclose) ? ui->windowclose : ui->windowmaximize);
    wdgt->setBackgroundRole(QPalette::Background),
    wdgt->setForegroundRole(QPalette::Text);
}

void systemback::wbleave()
{
    for(QWdt wdgt : QWL{ui->windowminimize, ui->windowclose, ui->windowmaximize})
        if(wdgt->backgroundRole() == QPalette::Background)
        {
            wdgt->setBackgroundRole(QPalette::Foreground),
            wdgt->setForegroundRole(QPalette::Base);
            break;
        }
}

void systemback::wbreleased()
{
    if(ui->buttonspanel->isVisible())
    {
        if(ui->buttonspanel->y() < 0)
        {
            if(ui->windowclose->foregroundRole() == QPalette::Highlight) ui->windowclose->setForegroundRole(ui->windowclose->backgroundRole() == QPalette::Background ? QPalette::Text : QPalette::Base);
        }
        else if(ui->windowclose->isVisible() && minside(ui->windowclose))
        {
            if(ui->windowclose->foregroundRole() == QPalette::Highlight) close();
        }
        else if(ui->windowmaximize->isVisible() && minside(ui->windowmaximize))
        {
            if(ui->windowmaximize->foregroundRole() == QPalette::Highlight) ui->buttonspanel->hide(),
                                                                            stschange();
        }
        else if(ui->windowminimize->foregroundRole() == QPalette::Highlight)
        {
            if(fscrn)
            {
                for(QWdt wdgt : QWL{ui->wpanel, ui->buttonspanel}) wdgt->hide();
                fwdgt = qApp->focusWidget(),
                ui->logo->setFocusPolicy(Qt::ClickFocus), ui->logo->setFocus();
            }
            else
            {
                XWindowAttributes attr;
                XGetWindowAttributes(dsply, winId(), &attr),
                XIconifyWindow(dsply, winId(), XScreenNumberOfScreen(attr.screen)),
                XFlush(dsply);
            }
        }
    }
}

void systemback::renter()
{
    QWdt wdgt(ui->copypanel->isVisible() ? ui->copyresize : ui->choosepanel->isVisible() ? ui->chooseresize : ui->excludepanel->isVisible() ? ui->excluderesize : ui->includeresize);

    if(! wismax)
    {
        if(wdgt->cursor().shape() == Qt::ArrowCursor) wdgt->setCursor(Qt::PointingHandCursor);

        if(wdgt->width() == ss(10))
        {
            ushort sz[]{ss(20), ss(30)};
            wdgt->setGeometry(wdgt->x() - sz[0], wdgt->y() - sz[0], sz[1], sz[1]);
        }
    }
    else if(wdgt->cursor().shape() == Qt::PointingHandCursor)
        wdgt->setCursor(Qt::ArrowCursor);
}

void systemback::rleave()
{
    QWdt wdgt(ui->copypanel->isVisible() ? ui->copyresize : ui->choosepanel->isVisible() ? ui->chooseresize : ui->excludepanel->isVisible() ? ui->excluderesize : ui->includeresize);

    if(wdgt->width() == ss(30) && [] {
            QCr csr(qApp->overrideCursor());
            return ! (csr && csr->shape() == Qt::SizeFDiagCursor);
        }())
    {
        ushort sz[]{ss(20), ss(10)};
        wdgt->setGeometry(wdgt->x() + sz[0], wdgt->y() + sz[0], sz[1], sz[1]);
    }
}

void systemback::rpressed()
{
    if(! wismax)
    {
        if(qApp->overrideCursor()) qApp->restoreOverrideCursor();
        qApp->setOverrideCursor(Qt::SizeFDiagCursor);
    }
}

void systemback::rreleased()
{
    QCr csr(qApp->overrideCursor());

    if(csr && csr->shape() == Qt::SizeFDiagCursor)
    {
        qApp->restoreOverrideCursor();
        if(busycnt) qApp->setOverrideCursor(Qt::WaitCursor);
        QRect sgm(sgeom());

        bool algn[]{[&] {
                ushort wdth(sgm.width() - wndw->x());

                if(wndw->width() > wdth)
                {
                    wgeom[2] = wdth;
                    return true;
                }

                return false;
            }(), [&] {
                    ushort hght(sgm.height() - wndw->y());

                    if(wndw->height() > hght)
                    {
                        wgeom[3] = hght;
                        return true;
                    }

                    return false;
                }()};

        if(algn[0] || algn[1]) wndw->resize(wgeom[2], wgeom[3]);
    }
}

void systemback::mpressed()
{
    (ui->buttonspanel->isVisible() ? minside(ui->windowminimize) ? ui->windowminimize : ui->windowclose->isVisible() && minside(ui->windowclose) ? ui->windowclose : ui->windowmaximize : ui->scalingbutton->isVisible() && minside(ui->scalingbutton) ? ui->scalingbutton : minside(ui->homepage1) ? ui->homepage1 : minside(ui->homepage2) ? ui->homepage2 : minside(ui->email) ? ui->email : ui->donate)->setForegroundRole(QPalette::Highlight);
}

void systemback::abtreleased()
{
    if(ui->homepage1->foregroundRole() == QPalette::Highlight)
        ui->homepage1->setForegroundRole(QPalette::Text),
        sb::exec("su -c \"xdg-open https://sourceforge.net/projects/systemback &\" " % guname(), sb::Bckgrnd);
    else if(ui->homepage2->foregroundRole() == QPalette::Highlight)
        ui->homepage2->setForegroundRole(QPalette::Text),
        sb::exec("su -c \"xdg-open https://launchpad.net/systemback &\" " % guname(), sb::Bckgrnd);
    else if(ui->email->foregroundRole() == QPalette::Highlight)
        ui->email->setForegroundRole(QPalette::Text),
        sb::exec("su -c \"xdg-email nemh@freemail.hu &\" " % guname(), sb::Bckgrnd);
    else if(ui->donate->foregroundRole() == QPalette::Highlight)
        ui->donate->setForegroundRole(QPalette::Text),
        sb::exec("su -c \"xdg-open 'https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=ZQ668BBR7UCEQ' &\" " % guname(), sb::Bckgrnd);
}

void systemback::foutpnt()
{
    schar num(-1);

    for(QLE ldt : ui->points->findChildren<QLE>())
    {
        ++num;

        if(ldt->isEnabled() && ldt->text().isEmpty())
        {
            ldt->setText(sb::pnames[num]);
            break;
        }
    }
}

void systemback::on_usersettingscopy_stateChanged(int arg1)
{
    if(! ppipe && ui->copypanel->isVisible()) ui->usersettingscopy->setText(arg1 == 1 ? tr("Transfer user configuration files") : tr("Transfer user configuration and data files"));
}

void systemback::pntupgrade()
{
    sb::pupgrade();
    schar num(-1);

    for(QLE ldt : ui->points->findChildren<QLE>())
        if(! sb::pnames[++num].isEmpty())
        {
            if(! ldt->isEnabled())
            {
                ldt->setEnabled(true);

                switch(num) {
                case 2:
                    if(sb::pnumber == 3 && ldt->styleSheet().isEmpty()) ldt->setStyleSheet("background-color: rgb(255, 103, 103)");
                    break;
                case 3 ... 8:
                    if(sb::pnumber < num + 2 && ldt->styleSheet().isEmpty()) ldt->setStyleSheet("background-color: rgb(255, 103, 103)");
                    break;
                case 9:
                    if(ldt->styleSheet().isEmpty()) ldt->setStyleSheet("background-color: rgb(255, 103, 103)");
                }

                ldt->setText(sb::pnames[num]);
            }
            else if(ldt->text() != sb::pnames[num])
                ldt->setText(sb::pnames[num]);
        }
        else if(ldt->isEnabled())
        {
            ldt->setDisabled(true);
            if(! ldt->styleSheet().isEmpty()) ldt->setStyleSheet(nullptr);

            switch(num) {
            case 3 ... 9:
                if(sb::pnumber < num + 1)
                {
                    if(ldt->text() != tr("not used")) ldt->setText(tr("not used"));
                }
                else if(ldt->text() != tr("empty"))
                    ldt->setText(tr("empty"));

                break;
            default:
                if(ldt->text() != tr("empty")) ldt->setText(tr("empty"));
            }
        }

    if(! sstart) on_pointpipe1_clicked();
}

void systemback::statustart()
{
    if(sstart && dialog != 305)
        ui->schedulerpanel->hide(),
        ui->statuspanel->show(),
        setwontop(false);
    else
    {
        if(ui->mainpanel->isVisible())
            ui->mainpanel->hide();
        else if(ui->dialogpanel->isVisible())
            ui->dialogpanel->hide(),
            setwontop(false);

        ui->statuspanel->show(),
        windowmove(ui->statuspanel->width(), ui->statuspanel->height());
        if(sb::dbglev == sb::Nulldbg) sb::dbglev = sb::Errdbg;
    }

    ui->progressbar->setMaximum(0),
    ui->focusfix->setFocus();
}

void systemback::restore()
{
    statustart();
    uchar mthd(ui->fullrestore->isChecked() ? 1 : ui->systemrestore->isChecked() ? 2 : ui->keepfiles->isChecked() ? ui->includeusers->currentIndex() == 0 ? 3 : 4 : ui->includeusers->currentIndex() == 0 ? 5 : 6);
    pset(mthd > 2 ? 4 : mthd + 1);
    uchar fcmp(sb::isfile("/etc/fstab") && sb::isfile(sb::sdir[1] % '/' % cpoint % '_' % pname % "/etc/fstab") ? sb::fload("/etc/fstab") == sb::fload(sb::sdir[1] % '/' % cpoint % '_' % pname % "/etc/fstab") ? 2 : 1 : 0);
    auto exit([this] { intrrpt = false; });
    if(intrrpt) return exit();

    if(sb::srestore(mthd, ui->includeusers->currentText(), sb::sdir[1] % '/' % cpoint % '_' % pname, nullptr, ui->autorestoreoptions->isChecked() ? fcmp : ui->skipfstabrestore->isChecked()))
    {
        if(intrrpt) return exit();
        sb::Progress = -1;

        if(mthd < 3)
        {
            if(ui->grubreinstallrestore->isVisibleTo(ui->restorepanel) && ! (ui->grubreinstallrestore->isEnabled() && ui->grubreinstallrestore->currentText() == tr("Disabled")))
            {
                sb::exec("update-grub");
                if(intrrpt) return exit();

                if(fcmp < 2 || ! (ui->autorestoreoptions->isChecked() || ui->grubreinstallrestore->currentText() == "Auto"))
                {
                    if(sb::exec("grub-install --force " % (grub.isEFI ? nullptr : ui->autorestoreoptions->isChecked() || ui->grubreinstallrestore->currentText() == "Auto" ? sb::gdetect() : ui->grubreinstallrestore->currentText()))) dialog = 309;
                    if(intrrpt) return exit();
                }
            }

            sb::crtfile(sb::sdir[1] % "/.sbschedule");
        }

        if(dialog != 309) dialog = [mthd, this] {
                switch(mthd) {
                case 1:
                    return 205;
                case 2:
                    return 204;
                default:
                    return ui->keepfiles->isChecked() ? 201 : 200;
                }
            }();
    }
    else
        dialog = 339;

    if(intrrpt) return exit();
    dialogopen();
}

void systemback::repair()
{
    statustart();
    uchar mthd(ui->systemrepair->isChecked() ? 2 : ui->fullrepair->isChecked() ? 1 : 0);
    pset(7 - mthd);
    auto exit([this] { intrrpt = false; });

    if(! mthd)
    {
        QSL mlst{"dev", "dev/pts", "proc", "sys"};
        for(cQStr &bpath : (sb::mcheck("/run") ? mlst << "/run" : mlst)) sb::mount('/' % bpath, "/mnt/" % bpath);
        dialog = sb::exec("chroot /mnt sh -c \"update-grub ; grub-install --force " % (grub.isEFI ? nullptr : ui->grubreinstallrepair->currentText() == "Auto" ? sb::gdetect("/mnt") : ui->grubreinstallrepair->currentText()) % '\"') ? 318 : 208,
        mlst.move(0, 1);
        for(cQStr &pend : mlst) sb::umount("/mnt/" % pend);
        if(intrrpt) return exit();
    }
    else
    {
        uchar fcmp(sb::isfile("/mnt/etc/fstab") ? 1 : 0);
        bool rv;

        if(! ppipe)
        {
            if(! (sb::isdir("/.systembacklivepoint") || sb::crtdir("/.systembacklivepoint"))) sb::rename("/.systembacklivepoint", "/.systembacklivepoint_" % sb::rndstr()),
                                                                                              sb::crtdir("/.systembacklivepoint");

            if(! sb::mount(sb::isfile("/cdrom/casper/filesystem.squashfs") ? "/cdrom/casper/filesystem.squashfs" : "/lib/live/mount/medium/live/filesystem.squashfs", "/.systembacklivepoint", "loop")) return dialogopen(334);

            if(fcmp == 1)
            {
                if(! sb::isfile("/.systembacklivepoint/etc/fstab"))
                    --fcmp;
                else if(sb::fload("/mnt/etc/fstab") == sb::fload("/.systembacklivepoint/etc/fstab"))
                    ++fcmp;
            }

            if(intrrpt) return exit();
            rv = sb::srestore(mthd, nullptr, "/.systembacklivepoint", "/mnt", ui->autorepairoptions->isChecked() ? fcmp : ui->skipfstabrepair->isChecked()),
            sb::umount("/.systembacklivepoint"),
            rmdir("/.systembacklivepoint");
        }
        else
        {
            if(fcmp == 1)
            {
                if(! sb::isfile(sb::sdir[1] % '/' % cpoint % '_' % pname % "/etc/fstab"))
                    --fcmp;
                else if(sb::fload("/mnt/etc/fstab") == sb::fload(sb::sdir[1] % '/' % cpoint % '_' % pname % "/etc/fstab"))
                    ++fcmp;
            }

            rv = sb::srestore(mthd, nullptr, sb::sdir[1] % '/' % cpoint % '_' % pname, "/mnt", ui->autorepairoptions->isChecked() ? fcmp : ui->skipfstabrepair->isChecked());
        }

        if(intrrpt) return exit();

        if(rv)
        {
            if(ui->grubreinstallrepair->isVisibleTo(ui->repairpanel) && ! (ui->grubreinstallrepair->isEnabled() && ui->grubreinstallrepair->currentText() == tr("Disabled")))
            {
                QSL mlst{"dev", "dev/pts", "proc", "sys"};
                for(cQStr &bpath : (sb::mcheck("/run") ? mlst << "/run" : mlst)) sb::mount('/' % bpath, "/mnt/" % bpath);
                sb::exec("chroot /mnt update-grub");
                if(intrrpt) return exit();
                if((fcmp < 2 || ! (ui->autorepairoptions->isChecked() || ui->grubreinstallrepair->currentText() == "Auto")) && sb::exec("chroot /mnt grub-install --force " % (grub.isEFI ? nullptr : ui->autorepairoptions->isChecked() || ui->grubreinstallrepair->currentText() == "Auto" ? sb::gdetect("/mnt") : ui->grubreinstallrepair->currentText()))) dialog = ui->fullrepair->isChecked() ? 310 : 304;
                mlst.move(0, 1);
                for(cQStr &pend : mlst) sb::umount("/mnt/" % pend);
                if(intrrpt) return exit();
            }

            emptycache();

            if(sb::like(dialog, {101, 102}))
            {
                if(ppipe && sb::isdir(sb::sdir[1]) && sb::access(sb::sdir[1], sb::Write)) sb::crtfile(sb::sdir[1] % "/.sbschedule");
                dialog = ui->fullrepair->isChecked() ? 202 : 203;
            }
        }
        else
            dialog = 339;
    }

    dialogopen();
}

void systemback::systemcopy()
{
    statustart(), pset(ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 8 : 9);

    auto err([this](ushort dlg = 0, cbstr &dev = nullptr) {
            if(! (intrrpt || (dlg && sb::like(dlg, {308, 314, 315, 317, 330, 331, 332, 333})))) dlg = [this] {
                    if(sb::dfree("/.sbsystemcopy") > 104857600 && (! sb::isdir("/.sbsystemcopy/home") || sb::dfree("/.sbsystemcopy/home") > 104857600) && (! sb::isdir("/.sbsystemcopy/boot") || sb::dfree("/.sbsystemcopy/boot") > 52428800) && (! sb::isdir("/.sbsystemcopy/boot/efi") || sb::dfree("/.sbsystemcopy/boot/efi") > 10485760))
                    {
                        irblck = true;
                        return ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 320 : 321;
                    }
                    else
                        return ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 307 : 316;
                }();

            {
                QStr mnts(sb::fload("/proc/self/mounts", true));
                QTS in(&mnts, QIODevice::ReadOnly);
                QSL incl{"* /.sbsystemcopy*", "* /.sbmountpoints*", "* /.systembacklivepoint *"};

                while(! in.atEnd())
                {
                    QStr cline(in.readLine());
                    if(sb::like(cline, incl)) sb::umount(cline.split(' ').at(1));
                }
            }

            if(sb::isdir("/.sbmountpoints"))
            {
                for(cQStr &item : QDir("/.sbmountpoints").entryList(QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot)) rmdir(bstr("/.sbmountpoints/" % item));
                rmdir("/.sbmountpoints");
            }

            rmdir("/.sbsystemcopy");
            if(sb::isdir("/.systembacklivepoint")) rmdir("/.systembacklivepoint");

            if(intrrpt)
                intrrpt = false;
            else
            {
                if(irblck) irblck = false;
                dialogopen(dlg, dev);
            }
        });

    if(! (sb::isdir("/.sbsystemcopy") || sb::crtdir("/.sbsystemcopy"))) return err();

    {
        QSL msort;
        msort.reserve(ui->partitionsettings->rowCount() - 1);

        for(ushort a(0) ; a < ui->partitionsettings->rowCount() ; ++a)
        {
            QStr nmpt(ui->partitionsettings->item(a, 4)->text());

            if(! (nmpt.isEmpty() || (nohmcpy[1] && nmpt == "/home")))
                msort.append(nmpt % '\n' % (ui->partitionsettings->item(a, 6)->text() == "x" ? ui->partitionsettings->item(a, 5)->text() : ui->partitionsettings->item(a, 5)->text() == "btrfs" ? "-b" : "-") % '\n' % ui->partitionsettings->item(a, 0)->text());
        }

        msort.sort();
        QSL ckd;
        ckd.reserve(msort.count());

        for(cQStr &vals : msort)
        {
            QSL cval(vals.split('\n'));
            cQStr &mpoint(cval.at(0)), &fstype(cval.at(1)), &part(cval.at(2));
            if(! ckd.contains(part) && sb::mcheck(part) && (! (grub.isEFI && mpoint == "/boot/efi") || fstype.length() > 2)) sb::umount(part);
            if(intrrpt) return err();
            sb::fssync();
            if(intrrpt) return err();

            if(fstype.length() > 2)
            {
                QStr lbl("SB@" % (mpoint.startsWith('/') ? sb::right(mpoint, -1) : mpoint));

                uchar rv(fstype == "swap" ? sb::exec("mkswap -L " % lbl % ' ' % part)
                       : fstype == "jfs" ? sb::exec("mkfs.jfs -qL " % lbl % ' ' % part)
                       : fstype == "reiserfs" ? sb::exec("mkfs.reiserfs -ql " % lbl % ' ' % part)
                       : fstype == "xfs" ? sb::exec("mkfs.xfs -fL " % lbl % ' ' % part)
                       : fstype == "vfat" ? sb::setpflag(part, "boot") ? sb::exec("mkfs.vfat -F 32 -n " % lbl.toUpper() % ' ' % part) : 255
                       : fstype == "btrfs" ? (ckd.contains(part) ? 0 : sb::exec("mkfs.btrfs -fL " % lbl % ' ' % part)) ? sb::exec("mkfs.btrfs -L " % lbl % ' ' % part) : 0
                       : sb::exec("mkfs." % fstype % " -FL " % lbl % ' ' % part));

                if(intrrpt) return err();
                if(rv) return err(ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 317 : 331, part);
            }

            if(mpoint != "SWAP")
            {
                if(! sb::isdir("/.sbsystemcopy" % mpoint))
                {
                    QStr path("/.sbsystemcopy");

                    for(cQStr &cpath : mpoint.split('/'))
                        if(! (sb::isdir(path.append('/' % cpath)) || sb::crtdir(path)))
                        {
                            sb::rename(path, path % '_' % sb::rndstr());
                            if(! sb::crtdir(path)) return err();
                        }
                }

                if(intrrpt) return err();

                if(fstype.length() == 2 || fstype == "btrfs")
                {
                    QStr mpt("/.sbmountpoints"), sv('@' % sb::right(mpoint, -1).replace('/', '_'));

                    for(uchar a(0) ; a < 4 ; ++a)
                        switch(a) {
                        case 0 ... 1:
                            if(! sb::isdir(mpt))
                            {
                                if(sb::exist(mpt)) sb::remove(mpt);
                                if(! sb::crtdir(mpt)) return err();
                            }

                            if(! a)
                                mpt.append('/' % sb::right(part, -sb::rinstr(part, "/")));
                            else if(! ckd.contains(part))
                            {
                                if(sb::mount(part, mpt))
                                    ckd.append(part);
                                else
                                    return err(ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 314 : 330, part);
                            }

                            break;
                        case 2 ... 3:
                            sb::exec("btrfs subvolume create " % mpt % '/' % sv);
                            if(intrrpt) return err();

                            if(sb::mount(part, "/.sbsystemcopy" % mpoint, "noatime,subvol=" % sv))
                                ++a;
                            else if(a == 3 || ! sb::rename(mpt % '/' % sv, mpt % '/' % sv % '_' % sb::rndstr()))
                                return err(ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 314 : 330, part);
                        }
                }
                else if(! sb::mount(part, "/.sbsystemcopy" % mpoint))
                    return err(ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 314 : 330, part);
            }

            if(intrrpt) return err();
        }
    }

    auto cfgwrt([](bool rpnt = true) {
            if(rpnt && ! (sb::copy(excfile, "/.sbsystemcopy" excfile) && sb::copy(incfile, "/.sbsystemcopy" incfile))) return false;
            return sb::cfgwrite("/.sbsystemcopy" cfgfile);
        });

    if(! ppipe)
    {
        if(pname == tr("Live image"))
        {
            if(! (sb::isdir("/.systembacklivepoint") || sb::crtdir("/.systembacklivepoint")))
            {
                sb::rename("/.systembacklivepoint", "/.systembacklivepoint_" % sb::rndstr());
                if(! sb::crtdir("/.systembacklivepoint") || intrrpt) return err();
            }

            if(! sb::mount(sb::isfile("/cdrom/casper/filesystem.squashfs") ? "/cdrom/casper/filesystem.squashfs" : "/lib/live/mount/medium/live/filesystem.squashfs", "/.systembacklivepoint", "loop")) return err(ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 332 : 333);
            if(intrrpt) return err();

            if(ui->usersettingscopy->isVisibleTo(ui->copypanel))
            {
                if(! sb::scopy([this] {
                        switch(ui->usersettingscopy->checkState()) {
                        case Qt::Unchecked:
                            return 5;
                        case Qt::PartiallyChecked:
                            return 3;
                        default:
                            return 4;
                        }
                    }(), guname(), "/.systembacklivepoint")) return err();
            }
            else if(! (sb::scopy(nohmcpy[1] ? 0 : ui->userdatafilescopy->isChecked() ? 1 : 2, nullptr, "/.systembacklivepoint") && cfgwrt()))
                return err();

            sb::umount("/.systembacklivepoint"),
            rmdir("/.systembacklivepoint");
        }
        else if(ui->usersettingscopy->isVisibleTo(ui->copypanel))
        {
            if(! sb::scopy([this] {
                    switch(ui->usersettingscopy->checkState()) {
                    case Qt::Unchecked:
                        return 5;
                    case Qt::PartiallyChecked:
                        return 3;
                    default:
                        return 4;
                    }
                }(), guname(), nullptr)) return err();
        }
        else if(! (sb::scopy(nohmcpy[1] ? 0 : ui->userdatafilescopy->isChecked() ? 1 : 2, nullptr, nullptr) && cfgwrt(false)))
            return err();
    }
    else if(ui->usersettingscopy->isVisibleTo(ui->copypanel))
    {
        if(! sb::scopy(ui->usersettingscopy->isChecked() ? 4 : 5, guname(), sb::sdir[1] % '/' % cpoint % '_' % pname)) return err();
    }
    else if(! (sb::scopy(nohmcpy[1] ? 0 : ui->userdatafilescopy->isChecked() ? 1 : 2, nullptr, sb::sdir[1] % '/' % cpoint % '_' % pname) && cfgwrt()))
        return err();

    if(intrrpt) return err();
    sb::Progress = -1;

    if(ui->usersettingscopy->isVisibleTo(ui->copypanel))
    {
        if(sb::exec("chroot /.sbsystemcopy sh -c \"for rmuser in $(grep :\\$6\\$* /etc/shadow | cut -d : -f 1) ; do [ $rmuser = " % guname() % " ] || [ $rmuser = root ] || userdel $rmuser ; done\"")) return err();
        QStr nfile;

        if(guname() != "root")
        {
            QStr nuname(ui->username->text());

            if(guname() != nuname)
            {
                if(sb::isdir("/.sbsystemcopy/home/" % guname()) && ((sb::exist("/.sbsystemcopy/home/" % nuname) && ! sb::rename("/.sbsystemcopy/home/" % nuname, "/.sbsystemcopy/home/" % nuname % '_' % sb::rndstr())) || ! sb::rename("/.sbsystemcopy/home/" % guname(), "/.sbsystemcopy/home/" % nuname))) return err();

                if(sb::isfile("/.sbsystemcopy/home/" % nuname % "/.config/gtk-3.0/bookmarks"))
                {
                    QStr cnt(sb::fload("/.sbsystemcopy/home/" % nuname % "/.config/gtk-3.0/bookmarks"));
                    if(cnt.contains("file:///home/" % guname() % '/') && ! sb::crtfile("/.sbsystemcopy/home/" % nuname % "/.config/gtk-3.0/bookmarks", cnt.replace("file:///home/" % guname() % '/', "file:///home/" % nuname % '/'))) return err();
                }

                if(sb::isfile("/.sbsystemcopy/etc/subuid") && sb::isfile("/.sbsystemcopy/etc/subgid"))
                {
                    {
                        QFile file("/.sbsystemcopy/etc/subuid");
                        if(! sb::fopen(file)) return err();

                        while(! file.atEnd())
                        {
                            QStr nline(file.readLine().trimmed());
                            if(nline.startsWith(guname() % ':')) nline.replace(0, guname().length(), nuname);
                            nfile.append(nline % '\n');
                            if(intrrpt) return err();
                        }
                    }

                    if(! sb::crtfile("/.sbsystemcopy/etc/subuid", nfile)) return err();
                    nfile.clear();

                    {
                        QFile file("/.sbsystemcopy/etc/subgid");
                        if(! sb::fopen(file)) return err();

                        while(! file.atEnd())
                        {
                            QStr nline(file.readLine().trimmed());
                            if(nline.startsWith(guname() % ':')) nline.replace(0, guname().length(), nuname);
                            nfile.append(nline % '\n');
                            if(intrrpt) return err();
                        }
                    }

                    if(! sb::crtfile("/.sbsystemcopy/etc/subgid", nfile)) return err();
                    nfile.clear();
                }
            }

            for(cQStr &cfname : {"/.sbsystemcopy/etc/group", "/.sbsystemcopy/etc/gshadow"})
            {
                QFile file(cfname);
                if(! sb::fopen(file)) return err();

                while(! file.atEnd())
                {
                    QStr nline(file.readLine().trimmed());

                    if(nline.startsWith("sudo:") && ui->rootpassword1->text().isEmpty() && ! sb::right(nline, -sb::rinstr(nline, ":")).split(',').contains(guname()))
                        nline.append((nline.endsWith(':') ? nullptr : ",") % nuname);
                    else if(guname() != nuname)
                    {
                        if(nline.startsWith(guname() % ':')) nline.replace(0, guname().length(), nuname);
                        nline.replace(':' % guname() % ',', ':' % nuname % ','), nline.replace(',' % guname() % ',', ',' % nuname % ',');
                        if(sb::like(nline, {"*:" % guname() % '_', "*," % guname() % '_'})) nline.replace(nline.length() - guname().length(), guname().length(), nuname);
                    }

                    nfile.append(nline % '\n');
                    if(intrrpt) return err();
                }

                if(! sb::crtfile(cfname , nfile)) return err();
                nfile.clear(), qApp->processEvents();
            }

            QFile file("/.sbsystemcopy/etc/passwd");
            if(! sb::fopen(file)) return err();

            while(! file.atEnd())
            {
                QStr cline(file.readLine().trimmed());

                nfile.append((! cline.startsWith(guname() % ':') ? cline : [&] {
                            QSL uslst(cline.split(':'));
                            QStr nline;

                            for(uchar a(0) ; a < uslst.count() ; ++a)
                                nline.append([&, a]() -> QStr {
                                        switch(a) {
                                        case 0:
                                            return nuname;
                                        case 4:
                                            return ui->fullname->text() % ",,,";
                                        case 5:
                                            return "/home/" % nuname;
                                        default:
                                            return uslst.at(a);
                                        }
                                    }() % ':');

                            return sb::left(nline, -1);
                    }()) % '\n');

                if(intrrpt) return err();
            }

            if(! sb::crtfile("/.sbsystemcopy/etc/passwd", nfile)) return err();
            nfile.clear();
        }

        {
            QFile file("/.sbsystemcopy/etc/shadow");
            if(! sb::fopen(file)) return err();

            while(! file.atEnd())
            {
                QStr cline(file.readLine().trimmed());

                nfile.append((cline.startsWith(guname() % ':') ? [&] {
                        QSL uslst(cline.split(':'));
                        QStr nline;

                        for(uchar a(0) ; a < uslst.count() ; ++a)
                            nline.append([&, a]() -> QStr {
                                    switch(a) {
                                    case 0:
                                        return guname() == "root" ? "root" : ui->username->text();
                                    case 1:
                                        return QStr(crypt(bstr(ui->password1->text()), bstr("$6$" % sb::rndstr(16))));
                                    default:
                                        return uslst.at(a);
                                    }
                                }() % ':');

                        return sb::left(nline, -1);
                    }() : cline.startsWith("root:") ? [&] {
                        QSL uslst(cline.split(':'));
                        QStr nline;

                        for(uchar a(0) ; a < uslst.count() ; ++a)
                            nline.append([&, a]() -> QStr {
                                    switch(a) {
                                    case 1:
                                        return ui->rootpassword1->text().isEmpty() ? "!" : QStr(crypt(bstr(ui->rootpassword1->text()), bstr("$6$" % sb::rndstr(16))));
                                    default:
                                        return uslst.at(a);
                                    }
                                }() % ':');

                        return sb::left(nline, -1);
                    }() : cline) % '\n');

                if(intrrpt) return err();
            }
        }

        if(! sb::crtfile("/.sbsystemcopy/etc/shadow", nfile)) return err();
        nfile.clear();

        {
            QFile file("/.sbsystemcopy/etc/hostname");
            if(! sb::fopen(file)) return err();
            QStr ohname(file.readLine().trimmed()), nhname(ui->hostname->text());
            file.close();

            if(ohname != nhname)
            {
                if(! sb::crtfile("/.sbsystemcopy/etc/hostname", nhname % '\n')) return err();
                file.setFileName("/.sbsystemcopy/etc/hosts");
                if(! sb::fopen(file)) return err();

                while(! file.atEnd())
                {
                    QStr nline(file.readLine().trimmed());
                    nline.replace('\t' % ohname % '\t', '\t' % nhname % '\t');
                    if(nline.endsWith('\t' % ohname)) nline.replace(nline.length() - ohname.length(), ohname.length(), nhname);
                    nfile.append(nline % '\n');
                    if(intrrpt) return err();
                }

                if(! sb::crtfile("/.sbsystemcopy/etc/hosts", nfile)) return err();
                nfile.clear();
            }
        }

        for(uchar a(0) ; a < 5 ; ++a)
        {
            QStr fpath("/.sbsystemcopy/etc/" % [a]() -> QStr {
                    switch(a) {
                    case 0:
                        return "lightdm/lightdm.conf";
                    case 1:
                        return "kde4/kdm/kdmrc";
                    case 2:
                        return "sddm.conf";
                    case 3:
                        return "gdm/custom.conf";
                    case 4:
                        return "gdm3/daemon.conf";
                    default:
                        return "mdm/mdm.conf";
                    }
                }());

            if(sb::isfile(fpath))
            {
                QFile file(fpath);
                if(! sb::fopen(file)) return err();
                uchar mdfd(0);

                QSL incl([a]() -> QSL {
                        switch(a) {
                        case 0:
                            return {"_autologin-user=*"};
                        case 1:
                            return {"_AutoLoginUser=*"};
                        case 2:
                            return {"_User=*", "_HideUsers=*"};
                        default:
                            return {"_AutomaticLogin=*", "_TimedLogin=*"};
                        }
                    }());

                while(! file.atEnd())
                {
                    QStr nline(file.readLine().trimmed());

                    if(sb::like(nline, incl))
                        if(! nline.endsWith('='))
                        {
                            bool algn(nline.endsWith('=' % guname()) && ! (a == 2 && nline.startsWith("HideUsers=")));
                            nline = sb::left(nline, sb::instr(nline, "="));

                            if(algn)
                            {
                                nline.append(ui->username->text());
                                if(! mdfd) ++mdfd;
                            }
                            else
                                switch(a) {
                                case 1:
                                    if(mdfd < 2) mdfd = 2;
                                    break;
                                case 3 ... 5:
                                    if(mdfd < 4) mdfd = nline.at(0) == 'A' ? mdfd == 3 ? 4 : 2 : mdfd == 2 ? 4 : 3;
                                    break;
                                default:
                                    if(! mdfd) ++mdfd;
                                }
                        }

                    nfile.append(nline % '\n');
                    if(intrrpt) return err();
                }

                if(mdfd)
                {
                    switch(a) {
                    case 1:
                        if(mdfd == 2) nfile.replace("AutoLoginEnabled=true", "AutoLoginEnabled=false");
                        break;
                    case 3 ... 5:
                        if(mdfd > 1)
                        {
                            if(sb::like(mdfd, {2, 4})) nfile.replace("AutomaticLoginEnable=true", "AutomaticLoginEnable=false");
                            if(sb::like(mdfd, {3, 4})) nfile.replace("TimedLoginEnable=true", "TimedLoginEnable=false");
                        }
                    }

                    if(! sb::crtfile(fpath, nfile)) return err();
                }

                nfile.clear(), qApp->processEvents();
            }
        }

        QBA mid(sb::rndstr(16).toUtf8().toHex() % '\n');
        if(intrrpt || ! (sb::crtfile("/.sbsystemcopy/etc/machine-id", mid) && cfmod("/.sbsystemcopy/etc/machine-id", 0444)) || (sb::isdir("/.sbsystemcopy/var/lib/dbus") && ! sb::crtfile("/.sbsystemcopy/var/lib/dbus/machine-id", mid))) return err();
    }

    {
        QStr fstabtxt("# /etc/fstab: static file system information.\n#\n# Use 'blkid' to print the universally unique identifier for a\n# device; this may be used with UUID= as a more robust way to name devices\n# that works even if disks are added and removed. See fstab(5).\n#\n# <file system> <mount point>   <type>  <options>       <dump>  <pass>\n");

        for(ushort a(0) ; a < ui->partitionsettings->rowCount() ; ++a)
        {
            QStr nmpt(ui->partitionsettings->item(a, 4)->text());

            if(! nmpt.isEmpty())
            {
                if(nohmcpy[1] && nmpt == "/home")
                {
                    QFile file("/etc/fstab");
                    if(! sb::fopen(file)) return err();
                    QSL incl{"* /home *", "* /home/ *"};

                    while(! file.atEnd())
                    {
                        QStr cline(file.readLine().trimmed());

                        if(! cline.startsWith('#') && sb::like(cline.replace('\t', ' '), incl))
                        {
                            fstabtxt.append("# /home\n" % cline % '\n');
                            break;
                        }
                    }
                }
                else
                {
                    QStr uuid(sb::ruuid(ui->partitionsettings->item(a, 0)->text())), nfs(ui->partitionsettings->item(a, 5)->text());

                    fstabtxt.append("# " % (nmpt == "SWAP" ? QStr("SWAP\nUUID=" % uuid % "   none   swap   sw   0   0")
                        : nmpt % "\nUUID=" % uuid % "   " % nmpt % "   " % nfs % "   noatime"
                            % (nmpt == "/" ? QStr(nfs == "reiserfs" ? ",notail" : nfs == "btrfs" ? ",subvol=@" : ",errors=remount-ro") % "   0   1"
                                : nmpt == "/boot/efi" ? QStr(",umask=0077   0   1")
                                    : (nfs == "reiserfs" ? ",notail" : nfs == "btrfs" ? QStr(",subvol=@" % sb::right(nmpt, -1).replace('/', '_')) : nullptr) % "   0   2")) % '\n');
                }
            }

            if(intrrpt) return err();
        }

        if(sb::isfile("/etc/fstab"))
        {
            QFile file("/etc/fstab");
            if(! sb::fopen(file)) return err();
            QSL incl{"*/dev/cdrom*", "*/dev/sr*"};

            while(! file.atEnd())
            {
                QStr cline(file.readLine().trimmed());
                if(! cline.startsWith('#') && sb::like(cline, incl)) fstabtxt.append("# cdrom\n" % cline % '\n');
                if(intrrpt) return err();
            }
        }

        if(! sb::crtfile("/.sbsystemcopy/etc/fstab", fstabtxt)) return err();
    }

    {
        QStr cfpath(ppipe ? QStr(sb::sdir[1] % '/' % cpoint % '_' % pname % "/etc/crypttab") : "/etc/crypttab");

        if(sb::isfile(cfpath))
        {
            QFile file(cfpath);
            if(! sb::fopen(file)) return err();

            while(! file.atEnd())
            {
                QBA cline(file.readLine().trimmed());

                if(! cline.startsWith('#') && cline.contains("UUID="))
                {
                    if(intrrpt || ! sb::crtfile("/.sbsystemcopy/etc/mtab") || sb::exec("chroot /.sbsystemcopy update-initramfs -tck all")) return err();
                    break;
                }
            }
        }
    }

    if(ui->grubinstallcopy->isVisibleTo(ui->copypanel))
    {
        if(intrrpt) return err();
        { QSL mlst{"dev", "dev/pts", "proc", "sys"};
        for(cQStr &bpath : (sb::mcheck("/run") ? mlst << "/run" : mlst)) sb::mount('/' % bpath, "/.sbsystemcopy/" % bpath); }

        if(ui->grubinstallcopy->currentText() == tr("Disabled"))
            sb::exec("chroot /.sbsystemcopy update-grub");
        else if(sb::exec("chroot /.sbsystemcopy sh -c \"update-grub ; grub-install --force " % (grub.isEFI ? nullptr : ui->grubinstallcopy->currentText() == "Auto" ? sb::gdetect("/.sbsystemcopy") : ui->grubinstallcopy->currentText()) % '\"'))
            return err(ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 308 : 315);
    }

    if(intrrpt) return err();
    pset(1);

    {
        QStr mnts(sb::fload("/proc/self/mounts", true));
        QTS in(&mnts, QIODevice::ReadOnly);
        QSL incl{"* /.sbsystemcopy*", "* /.sbmountpoints*"};

        while(! in.atEnd())
        {
            QStr cline(in.readLine());
            if(sb::like(cline, incl)) sb::umount(cline.split(' ').at(1));
        }
    }

    if(sb::isdir("/.sbmountpoints"))
    {
        for(cQStr &item : QDir("/.sbmountpoints").entryList(QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot)) rmdir(bstr("/.sbmountpoints/" % item));
        rmdir("/.sbmountpoints");
    }

    rmdir("/.sbsystemcopy"),
    sb::fssync();
    if(sb::ecache) sb::crtfile("/proc/sys/vm/drop_caches", "3");
    dialogopen(ui->userdatafilescopy->isVisibleTo(ui->copypanel) ? 206 : 209);
}

void systemback::livewrite()
{
    statustart(), pset(10);
    QStr ldev(ui->livedevices->item(ui->livedevices->currentRow(), 0)->text());
    bool ismmc(ldev.contains("mmc"));

    auto err([&, ismmc](ushort dlg = 336) {
            if(sb::isdir("/.sblivesystemwrite"))
            {
                if(sb::mcheck("/.sblivesystemwrite/sblive")) sb::umount("/.sblivesystemwrite/sblive");
                rmdir("/.sblivesystemwrite/sblive");

                if(sb::isdir("/.sblivesystemwrite/sbroot"))
                {
                    if(sb::mcheck("/.sblivesystemwrite/sbroot")) sb::umount("/.sblivesystemwrite/sbroot");
                    rmdir("/.sblivesystemwrite/sbroot");
                }

                rmdir("/.sblivesystemwrite");
            }

            if(intrrpt)
                intrrpt = false;
            else
                dialogopen(dlg, sb::like(dlg, {337, 338}) ? bstr(ldev % (ismmc ? "p" : nullptr) % '1') : nullptr);
        });

    if(! sb::exist(ldev))
        return err(338);
    else if(sb::mcheck(ldev))
    {
        for(cQStr &sitem : QDir("/dev").entryList(QDir::System))
        {
            QStr item("/dev/" % sitem);

            if(item.length() > (ismmc ? 12 : 8) && item.startsWith(ldev))
                while(sb::mcheck(item)) sb::umount(item);

            if(intrrpt) return err();
        }

        if(sb::mcheck(ldev)) sb::umount(ldev);
        sb::fssync();
        if(intrrpt) return err();
    }

    if(! sb::mkptable(ldev) || intrrpt) return err(338);
    sb::delay(100);
    QStr lrdir;

    {
        ullong isize(sb::fsize(sb::sdir[2] % '/' % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1) % ".sblive"));

        if(isize < 4294967295)
        {
            if(! sb::mkpart(ldev) || intrrpt) return err(338);
            sb::delay(100),
            lrdir = "sblive";
        }
        else
        {
            if(! (sb::mkpart(ldev, 1048576, 104857600) && sb::mkpart(ldev)) || intrrpt) return err(338);
            sb::delay(100);
            if(sb::exec("mkfs.ext2 -FL SBROOT " % ldev % (ismmc ? "p" : nullptr) % '2') || intrrpt) return err(338);
            lrdir = "sbroot";
        }

        if(sb::exec("mkfs.vfat -F 32 -n SBLIVE " % ldev % (ismmc ? "p" : nullptr) % '1') || intrrpt) return err(338);

        if(sb::exec("dd if=/usr/lib/syslinux/" % QStr(sb::isfile("/usr/lib/syslinux/mbr.bin") ? nullptr : "mbr/") % "mbr.bin of=" % ldev % " conv=notrunc bs=440 count=1") || ! sb::setpflag(ldev % (ismmc ? "p" : nullptr) % '1', "boot lba")
            || intrrpt || (sb::exist("/.sblivesystemwrite") && (((sb::mcheck("/.sblivesystemwrite/sblive") && ! sb::umount("/.sblivesystemwrite/sblive")) || (sb::mcheck("/.sblivesystemwrite/sbroot") && ! sb::umount("/.sblivesystemwrite/sbroot"))) || ! sb::remove("/.sblivesystemwrite")))
            || intrrpt || ! (sb::crtdir("/.sblivesystemwrite") && sb::crtdir("/.sblivesystemwrite/sblive"))
            || intrrpt) return err();

        sb::delay(100);
        if(! sb::mount(ldev % (ismmc ? "p" : nullptr) % '1', "/.sblivesystemwrite/sblive") || intrrpt) return err(337);

        if(lrdir == "sbroot")
        {
            if(! sb::crtdir("/.sblivesystemwrite/sbroot")) return err();
            if(! sb::mount(ldev % (ismmc ? "p" : nullptr) % '2', "/.sblivesystemwrite/sbroot") || intrrpt) return err(337);
        }

        if(sb::dfree("/.sblivesystemwrite/" % lrdir) < isize + 52428800) return err(322);
        sb::ThrdStr[0] = "/.sblivesystemwrite", sb::ThrdLng[0] = isize;
    }

    if(lrdir == "sblive")
    {
        if(sb::exec("tar -xf \"" % sb::sdir[2] % "\"/" % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1) % ".sblive -C /.sblivesystemwrite/sblive --no-same-owner --no-same-permissions", sb::Prgrss)) return err(323);
    }
    else if(sb::exec("tar -xf \"" % sb::sdir[2] % "\"/" % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1) % ".sblive -C /.sblivesystemwrite/sblive --exclude=casper/filesystem.squashfs --exclude=live/filesystem.squashfs --no-same-owner --no-same-permissions") || sb::exec("tar -xf \"" % sb::sdir[2] % "\"/" % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1) % ".sblive -C /.sblivesystemwrite/sbroot --exclude=.disk --exclude=boot --exclude=EFI --exclude=syslinux --exclude=casper/initrd.gz --exclude=casper/vmlinuz --exclude=live/initrd.gz --exclude=live/vmlinuz --no-same-owner --no-same-permissions", sb::Prgrss))
        return err(323);

    pset(1);
    if(sb::exec("syslinux -ifd syslinux " % ldev % (ismmc ? "p" : nullptr) % '1')) return err();
    sb::fssync();
    if(sb::ecache) sb::crtfile("/proc/sys/vm/drop_caches", "3");
    sb::umount("/.sblivesystemwrite/sblive"),
    rmdir("/.sblivesystemwrite/sblive");

    if(lrdir == "sbroot")
    {
        sb::umount("/.sblivesystemwrite/sbroot");
        rmdir("/.sblivesystemwrite/sbroot");
    }

    rmdir("/.sblivesystemwrite"),
    dialogopen(210);
}

void systemback::dialogopen(ushort dlg, cbstr &dev)
{
    if(ui->dialognumber->isVisibleTo(ui->dialogpanel)) ui->dialognumber->hide();

    if(! dlg)
        dlg = dialog;
    else if(dialog != dlg)
        dialog = dlg;

    if(dlg < 200)
    {
        for(QWdt wdgt : QWL{ui->dialoginfo, ui->dialogerror})
            if(wdgt->isVisibleTo(ui->dialogpanel)) wdgt->hide();

        for(QWdt wdgt : QWL{ui->dialogquestion, ui->dialogcancel})
            if(! wdgt->isVisibleTo(ui->dialogpanel)) wdgt->show();

        if(ui->dialogok->text() != tr("Start")) ui->dialogok->setText(tr("Start"));

        ui->dialogtext->setText([this, dlg]() -> QStr {
                switch(dlg) {
                case 100:
                    return tr("Restore the system files to the following restore point:") % "<p><b>" % pname;
                case 101:
                    return tr("Repair the system files with the following restore point:") % "<p><b>" % pname;
                case 102:
                    return tr("Repair the complete system with the following restore point:") % "<p><b>" % pname;
                case 103:
                    return tr("Restore the complete user(s) configuration files to the following restore point:") % "<p><b>" % pname;
                case 104:
                    return tr("Restore the user(s) configuration files to the following restore point:") % "<p><b>" % pname;
                case 105:
                    return tr("Copy the system, using the following restore point:") % "<p><b>" % pname;
                case 106:
                    return tr("Install the system, using the following restore point:") % "<p><b>" % pname;
                case 107:
                    return tr("Restore the complete system to the following restore point:") % "<p><b>" % pname;
                case 108:
                    return tr("Format the %1, and write the following Live system image:").arg(" <b>" % ui->livedevices->item(ui->livedevices->currentRow(), 0)->text() % "</b>") % "<p><b>" % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1) % "</b>";
                default:
                    return tr("Repair the GRUB 2 bootloader.");
                }
            }());
    }
    else
    {
        for(QWdt wdgt : QWL{ui->dialogquestion, ui->dialogcancel})
            if(wdgt->isVisibleTo(ui->dialogpanel)) wdgt->hide();

        if(dlg < 300)
        {
            if(ui->dialogerror->isVisibleTo(ui->dialogpanel)) ui->dialogerror->hide();
            if(! ui->dialoginfo->isVisibleTo(ui->dialogpanel)) ui->dialoginfo->show();
            bool cntd(false);

            ui->dialogtext->setText([&, dlg]() -> QStr {
                    switch(dlg) {
                    case 200:
                        if(ui->dialogok->text() != tr("X restart")) ui->dialogok->setText(tr("X restart"));
                        for(QWdt wdgt : QWL{ui->dialogcancel, ui->dialognumber}) wdgt->show();
                        cntd = true;
                        return tr("The user(s) configuration files full restoration are completed.") % "<p>" % tr("The X server will restart automatically within 30 seconds.");
                    case 201:
                        if(ui->dialogok->text() != tr("X restart")) ui->dialogok->setText(tr("X restart"));
                        for(QWdt wdgt : QWL{ui->dialogcancel, ui->dialognumber}) wdgt->show();
                        cntd = true;
                        return tr("The user(s) configuration files restoration are completed.") % "<p>" % tr("The X server will restart automatically within 30 seconds.");
                    case 202:
                        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
                        return tr("The full system repair is completed.");
                    case 203:
                        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
                        return tr("The system repair is completed.");
                    case 204:
                        if(ui->dialogok->text() != tr("Reboot")) ui->dialogok->setText(tr("Reboot"));
                        for(QWdt wdgt : QWL{ui->dialogcancel, ui->dialognumber}) wdgt->show();
                        cntd = true;
                        return tr("The system files restoration are completed.") % "<p>" % tr("The computer will restart automatically within 30 seconds.");
                    case 205:
                        if(ui->dialogok->text() != tr("Reboot")) ui->dialogok->setText(tr("Reboot"));
                        for(QWdt wdgt : QWL{ui->dialogcancel, ui->dialognumber}) wdgt->show();
                        cntd = true;
                        return tr("The full system restoration is completed.") % "<p>" % tr("The computer will restart automatically within 30 seconds.");
                    case 206:
                        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
                        return tr("The system copy is completed.");
                    case 207:
                        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
                        return tr("The Live system creation is completed.") % "<p>" % tr("The created .sblive file can be written to pendrive.");
                    case 208:
                        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
                        return tr("The GRUB 2 repair is completed.");
                    case 209:
                        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
                        return tr("The system install is completed.");
                    case 210:
                        if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");
                        return tr("The Live system image write is completed.");
                    default:
                        ui->dialogok->setText(tr("Reboot"));
                        for(QWdt wdgt : QWL{ui->dialogcancel, ui->dialognumber}) wdgt->show();
                        cntd = true;
                        return tr("The computer will restart automatically within 30 seconds.");
                    }
                }());

            if(cntd)
            {
                connect(dlgtimer = new QTimer,
#if QT_VERSION < QT_VERSION_CHECK(5, 3, 0)
                    SIGNAL(timeout()), this, SLOT(dialogtimer())
#else
                    &QTimer::timeout, this, &systemback::dialogtimer
#endif
                    );

                dlgtimer->start(1000);
            }
        }
        else
        {
            if(! sb::eout.isEmpty()) sb::error(sb::eout);
            if(ui->dialoginfo->isVisibleTo(ui->dialogpanel)) ui->dialoginfo->hide();
            if(! ui->dialogerror->isVisibleTo(ui->dialogpanel)) ui->dialogerror->show();
            if(ui->dialogok->text() != "OK") ui->dialogok->setText("OK");

            ui->dialogtext->setText([&, dlg]() -> QStr {
                    switch(dlg) {
                    case 300:
                        return tr("An another Systemback process is currently running, please wait until it finishes.");
                    case 301:
                        return tr("Unable to get exclusive lock!") % "<p>" % tr("First, close all package manager.");
                    case 302:
                        return tr("The re-synchronization of package index files currently in progress, please wait until it finishes.");
                    case 303:
                        return tr("The specified name contain(s) unsupported character(s)!") % "<p>" % tr("Please enter a new name.");
                    case 304:
                        return tr("The system files repair are completed, but an error occurred while reinstalling the GRUB!") % ' ' % tr("The system may not bootable! (In general, the different architecture is causing the problem.)");
                    case 305:
                        return tr("The restore point creation is aborted!") % "<p>" % tr("Not enough free disk space to complete the process.");
                    case 306:
                        return tr("Root privileges are required for running the Systemback!");
                    case 307:
                        return tr("The system copy is aborted!") % "<p>" % tr("The specified partition(s) does not have enough free space to copy the system. The copied system will not function properly.");
                    case 308:
                        return tr("The system copy is completed, but an error occurred while installing the GRUB!") % ' ' % tr("You need to manually install a bootloader.");
                    case 309:
                        return tr("The system restoration is aborted!") % "<p>" % tr("An error occurred while reinstalling the GRUB.");
                    case 310:
                        return tr("The full system repair is completed, but an error occurred while reinstalling the GRUB!") % ' ' % tr("The system may not bootable! (In general, the different architecture is causing the problem.)");
                    case 311:
                        return tr("The Live system creation is aborted!") % "<p>" % tr("An error occurred while creating the file system image.");
                    case 312:
                        return tr("The Live system creation is aborted!") % "<p>" % tr("An error occurred while creating the container file.");
                    case 313:
                        return tr("The Live system creation is aborted!") % "<p>" % tr("Not enough free disk space to complete the process.");
                    case 314:
                        return tr("The system copy is aborted!") % "<p>" % tr("The specified partition could not be mounted.") % "<p><b>" % dev.data;
                    case 315:
                        return tr("The system install is completed, but an error occurred while installing the GRUB!") % ' ' % tr("You need to manually install a bootloader.");
                    case 316:
                        return tr("The system installation is aborted!") % "<p>" % tr("The specified partition(s) does not have enough free space to install the system. The installed system will not function properly.");
                    case 317:
                        return tr("The system copy is aborted!") % "<p>" % tr("The specified partition could not be formatted (in use or unavailable).") % "<p><b>" % dev.data;
                    case 318:
                        return tr("An error occurred while reinstalling the GRUB!") % ' ' % tr("The system may not bootable! (In general, the different architecture is causing the problem.)");
                    case 319:
                        return tr("The restore point creation is aborted!") % "<p>" % tr("There has been critical changes in the file system during this operation.");
                    case 320:
                        return tr("The system copy is aborted!") % "<p>" % tr("There has been critical changes in the file system during this operation.");
                    case 321:
                        return tr("The system installation is aborted!") % "<p>" % tr("There has been critical changes in the file system during this operation.");
                    case 322:
                        return tr("The Live write is aborted!") % "<p>" % tr("The selected device does not have enough space to write the Live system.");
                    case 323:
                        return tr("The Live write is aborted!") % "<p>" % tr("An error occurred while unpacking the Live system files.");
                    case 324:
                        return tr("The Live conversion is aborted!") % "<p>" % tr("An error occurred while renaming the essential Live files.");
                    case 325:
                        return tr("The Live conversion is aborted!") % "<p>" % tr("An error occurred while creating the .iso image.");
                    case 326:
                        return tr("The Live conversion is aborted!") % "<p>" % tr("An error occurred while reading the .sblive image.");
                    case 327:
                        return tr("The Live system creation is aborted!") % "<p>" % tr("An error occurred while creating the new initramfs image.");
                    case 328:
                        return tr("The Live system creation is aborted!") % "<p>" % tr("There has been critical changes in the file system during this operation.");
                    case 329:
                        return tr("The restore point deletion is aborted!") % "<p>" % tr("An error occurred while during the process.");
                    case 330:
                        return tr("The system installation is aborted!") % "<p>" % tr("The specified partition could not be mounted.") % "<p><b>" % dev.data;
                    case 331:
                        return tr("The system installation is aborted!") % "<p>" % tr("The specified partition could not be formatted (in use or unavailable).") % "<p><b>" % dev.data;
                    case 332:
                        return tr("The system copy is aborted!") % "<p>" % tr("The Live image could not be mounted.");
                    case 333:
                        return tr("The system installation is aborted!") % "<p>" % tr("The Live image could not be mounted.");
                    case 334:
                        return tr("The system repair is aborted!") % "<p>" % tr("The Live image could not be mounted.");
                    case 335:
                        return tr("The Live conversion is aborted!") % "<p>" % tr("There has been critical changes in the file system during this operation.");
                    case 336:
                        return tr("The Live write is aborted!") % "<p>" % tr("There has been critical changes in the file system during this operation.");
                    case 337:
                        return tr("The Live write is aborted!") % "<p>" % tr("The specified partition could not be mounted.") % "<p><b>" % dev.data;
                    case 338:
                        return tr("The Live write is aborted!") % "<p>" % tr("The specified partition could not be formatted (in use or unavailable).") % "<p><b>" % dev.data;
                    case 339:
                        return tr("The system restoration is aborted!") % "<p>" % tr("There is not enough free space.");
                    default:
                        return tr("The system repair is aborted!") % "<p>" % tr("There is not enough free space.");
                    }
                }());
        }
    }

    for(QWdt wdgt : QWL{ui->mainpanel, ui->statuspanel, ui->schedulerpanel})
        if(wdgt->isVisibleTo(ui->wpanel)) wdgt->hide();

    if(ui->dialogpanel->isHidden()) ui->dialogpanel->show();
    ui->dialogok->setFocus();

    if(width() != ui->dialogpanel->width())
    {
        if(utimer.isActive() && ! sstart)
            windowmove(ui->dialogpanel->width(), ui->dialogpanel->height());
        else
        {
            if(sstart && ! (sb::like(dialog, {300, 301, 306}) || ui->function3->text().contains(' '))) ui->function3->setText("Systemback " % tr("scheduler"));
            setFixedSize(wgeom[2] = ui->dialogpanel->width(), wgeom[3] = ui->dialogpanel->height());
            QRect sgm(sgeom());
            move(wgeom[0] = sgm.x() + sgm.width() / 2 - ss(253), wgeom[1] = sgm.y() + sgm.height() / 2 - ss(100));
        }
    }

    if(ui->wpanel->isHidden()) ui->wpanel->show(),
                               ui->logo->setFocusPolicy(Qt::NoFocus);

    setwontop();
}

void systemback::setwontop(bool state)
{
    if(! (fscrn || sb::waot))
    {
        XEvent ev;
        ev.xclient.type = ClientMessage,
        ev.xclient.message_type = XInternAtom(dsply, "_NET_WM_STATE", 0),
        ev.xclient.display = dsply,
        ev.xclient.window = winId(),
        ev.xclient.format = 32,
        ev.xclient.data.l[0] = state ? 1 : 0,
        ev.xclient.data.l[1] = XInternAtom(dsply, "_NET_WM_STATE_ABOVE", 0);
        Window win(XDefaultRootWindow(dsply));
        XSendEvent(dsply, win, 0, SubstructureRedirectMask | SubstructureNotifyMask, &ev),
        ev.xclient.data.l[1] = XInternAtom(dsply, "_NET_WM_STATE_STAYS_ON_TOP", 0),
        XSendEvent(dsply, win, 0, SubstructureRedirectMask | SubstructureNotifyMask, &ev),
        XFlush(dsply);
    }
}

void systemback::windowmove(ushort nwidth, ushort nheight, bool fxdw)
{
    if(wismax) stschange();

    if(wndw->size() != QSize(wgeom[2] = nwidth, wgeom[3] = nheight))
    {
        {
            QDW dtp(fscrn ? nullptr : qApp->desktop());
            QRect agm(sgeom(true, dtp)), sgm;
            ushort frm(ss(30));

            if(agm.width() >= wgeom[2] + frm)
            {
                sgm = sgeom(false, dtp);
                short bnd(sgm.x() + frm);

                if((wgeom[0] = wndw->x() + (wndw->width() - wgeom[2]) / 2) < bnd)
                    wgeom[0] = bnd < agm.x() ? agm.x() : bnd;
                else if(wgeom[0] < agm.x())
                    wgeom[0] = agm.x();
                else
                {
                    short bnd1;

                    if(wgeom[0] > (bnd = sgm.x() + sgm.width() - wgeom[2] - frm))
                        wgeom[0] = bnd > (bnd1 = agm.x() + agm.width() - wgeom[2]) ? bnd1 : bnd;
                    else if(wgeom[0] > (bnd1 = agm.x() + agm.width() - wgeom[2]))
                        wgeom[0] = bnd1;
                }
            }
            else if(wgeom[0] != agm.x())
                wgeom[0] = agm.x();

            if(agm.height() >= wgeom[3] + frm)
            {
                if(sgm.isEmpty()) sgm = sgeom(false, dtp);
                short bnd(sgm.y() + frm);

                if((wgeom[1] = wndw->y() + (wndw->height() - wgeom[3]) / 2) < bnd)
                    wgeom[1] = bnd < agm.y() ? agm.y() : bnd;
                else if(wgeom[1] < agm.y())
                    wgeom[1] = agm.y();
                else
                {
                    short bnd1;

                    if(wgeom[1] > (bnd = sgm.y() + sgm.height() - wgeom[3] - frm))
                        wgeom[1] = bnd > (bnd1 = agm.y() + agm.height() - wgeom[3]) ? bnd1 : bnd;
                    else if(wgeom[1] > (bnd1 = agm.y() + agm.height() - wgeom[3]))
                        wgeom[1] = bnd1;
                }
            }
            else if(wgeom[1] != agm.y())
                wgeom[1] = agm.y();

            wndw->setMinimumSize(0, 0), wndw->setMaximumSize(agm.width() < wndw->width() ? wndw->width() : agm.width(), agm.height() < wndw->height() ? wndw->height() : agm.height());
        }

        wmblck = true;
        int vls[]{qAbs(wgeom[0] - wndw->x()) / 6, qAbs(wgeom[1] - wndw->y()) / 6, qAbs(wgeom[2] - wndw->width()) / 6, qAbs(wgeom[3] - wndw->height()) / 6};
        ui->resizepanel->show();

        for(uchar a(0) ; a < 6 ; ++a) wndw->setGeometry(qAbs(wgeom[0] - wndw->x()) > vls[0] ? wndw->x() - (wgeom[0] < wndw->x() ? vls[0] : -vls[0]) : wgeom[0], qAbs(wgeom[1] - wndw->y()) > vls[1] ? wndw->y() - (wgeom[1] < wndw->y() ? vls[1] : -vls[1]) : wgeom[1], qAbs(wgeom[2] - wndw->width()) > vls[2] ? wndw->width() - (wgeom[2] < wndw->width() ? vls[2] : -vls[2]) : wgeom[2], qAbs(wgeom[3] - wndw->height()) > vls[3] ? wndw->height() - (wgeom[3] < wndw->height() ? vls[3] : -vls[3]) : wgeom[3]),
                                      QThread::msleep(1),
                                      repaint();

        wndw->setGeometry(wgeom[0], wgeom[1], wgeom[2], wgeom[3]),
        wmblck = false;
        if(fxdw) wndw->setFixedSize(wgeom[2], wgeom[3]);
        ui->resizepanel->hide();
    }
    else if(fxdw && minimumSize() != maximumSize())
        wndw->setFixedSize(wgeom[2], wgeom[3]);
}

void systemback::wmove()
{
    if(! wismax)
    {
        QPoint npos{QCursor::pos().x() - lblevent::MouseX, QCursor::pos().y() - lblevent::MouseY};

        wndw->move(fscrn ? [&] {
                QRect agm(sgeom(true));

                return QPoint{agm.width() < wgeom[2] || npos.x() < 0 ? 0 : [&] {
                        short bpos;
                        return npos.x() > (bpos = width() - wgeom[2]) ? bpos : npos.x();
                    }(), agm.height() < wgeom[3] || npos.y() < 0 ? 0 : [&] {
                            short bpos;
                            return npos.y() > (bpos = height() - wgeom[3]) ? bpos : npos.y();
                        }()};
            }() : npos);
    }
}

void systemback::rmove()
{
    if(! wismax)
    {
        ushort sz(ss(31));
        wndw->resize(QCursor::pos().x() - wndw->x() + sz - lblevent::MouseX, QCursor::pos().y() - wndw->y() + sz - lblevent::MouseY);
    }
}

void systemback::on_functionmenunext_clicked()
{
    ui->functionmenunext->setDisabled(true);
    uchar a(ss(7));
    short b(-ss(217));
    do ui->functionmenu->move(ui->functionmenu->x() - a, 0), qApp->processEvents();
    while(ui->functionmenu->x() > b);
    ui->functionmenu->move(-ss(224), 0),
    ui->functionmenuback->setEnabled(true),
    ui->functionmenuback->setFocus();
}

void systemback::on_functionmenuback_clicked()
{
    ui->functionmenuback->setDisabled(true);
    uchar a(ss(7));
    do ui->functionmenu->move(ui->functionmenu->x() + a, 0), qApp->processEvents();
    while(ui->functionmenu->x() < -a);
    ui->functionmenu->move(0, 0),
    ui->functionmenunext->setEnabled(true),
    ui->functionmenunext->setFocus();
}

bool systemback::eventFilter(QObject *, QEvent *ev)
{
    if(fscrn)
        switch(ev->type()) {
        case QEvent::FontChange:
            if(font() != bfnt) qApp->setFont(bfnt);
            return true;
        case QEvent::Resize:
            for(QWdt wdgt : QWL{ui->wallpaper, ui->logo}) wdgt->resize(size());
            ui->logo->setPixmap(QPixmap("/usr/share/systemback/logo.png").scaledToWidth((ui->wallpaper->width() > ui->wallpaper->height() ? ui->wallpaper->height() : ui->wallpaper->width()) / 2));

            if(wismax)
            {
                ui->wpanel->setMaximumSize(size());
                if(ui->wpanel->size() != size()) ui->wpanel->resize(size());
            }
            else
            {
                if(ui->copypanel->isVisibleTo(ui->wpanel))
                {
                    ushort sz(ss(60));
                    ui->wpanel->setMaximumSize(width() - sz, height() - sz);
                }

                bool algn[]{wgeom[0] > 0 && [this] {
                        if(wgeom[0] + wgeom[2] > width())
                        {
                            short nx(width() - wgeom[2]);
                            wgeom[0] = nx < 0 ? 0 : nx;
                            return true;
                        }

                        return false;
                    }(), wgeom[1] > 0 && [this] {
                            if(wgeom[1] + wgeom[3] > height())
                            {
                                short ny(height() - wgeom[3]);
                                wgeom[1] = ny < 0 ? 0 : ny;
                                return true;
                            }

                            return false;
                        }()};

                if(algn[0] || algn[1]) ui->wpanel->move(wgeom[0], wgeom[1]);
            }
        default:;
        }
    else
    {
        switch(ev->type()) {
        case QEvent::WindowActivate:
            if(ui->function3->foregroundRole() == QPalette::Dark)
            {
                for(QWdt wdgt : QWL{ui->scalingbutton, ui->function1, ui->windowbutton1, ui->function2, ui->windowbutton2, ui->function3, ui->windowbutton3, ui->function4, ui->windowbutton4}) wdgt->setForegroundRole(QPalette::Base);
                goto gcheck;
            }

            return false;
        case QEvent::WindowDeactivate:
            if(ui->function3->foregroundRole() == QPalette::Base)
            {
                for(QWdt wdgt : QWL{ui->scalingbutton, ui->function1, ui->windowbutton1, ui->function2, ui->windowbutton2, ui->function3, ui->windowbutton3, ui->function4, ui->windowbutton4}) wdgt->setForegroundRole(QPalette::Dark);

                if(ui->copypanel->isVisible())
                {
                    if(ui->partitionsettings->hasFocus() && ui->partitionsettings->currentRow() == -1) ui->copyback->setFocus();
                }
                else if(ui->livepanel->isVisible())
                {
                    if((ui->livelist->hasFocus() && ui->livelist->currentRow() == -1) || (ui->livedevices->hasFocus() && ui->livedevices->currentRow() == -1)) ui->liveback->setFocus();
                }
                else if(ui->excludepanel->isVisible())
                {
                    if((ui->excludeitemslist->hasFocus() && ! ui->excludeitemslist->currentItem()) || (ui->excludedlist->hasFocus() && ui->excludedlist->currentRow() == -1)) ui->excludeback->setFocus();
                }
                else if(ui->includepanel->isVisible())
                {
                    if((ui->includeitemslist->hasFocus() && ! ui->includeitemslist->currentItem()) || (ui->includedlist->hasFocus() && ui->includedlist->currentRow() == -1)) ui->includeback->setFocus();
                }
                else if(ui->choosepanel->isVisible() && ui->dirchoose->hasFocus() && ! ui->dirchoose->currentItem())
                    ui->dirchoosecancel->setFocus();

                goto gcheck;
            }

            return false;
        case QEvent::Resize:
            if(wismax && ! wmblck)
            {
                QRect agm(sgeom(true));

                if(geometry() != agm)
                {
                    setGeometry(agm);
                    return true;
                }
            }

            ui->wpanel->resize(size());

            if(ui->choosepanel->isVisible())
            {
                ui->choosepanel->resize(width() - ui->choosepanel->x() * 2, height() - ss(25));
                ushort sz(ss(40));
                ui->dirpath->resize(ui->choosepanel->width() - sz, ui->dirpath->height()),
                ui->dirrefresh->move(ui->choosepanel->width() - ui->dirrefresh->width(), 0),
                ui->dirchoose->resize(ui->choosepanel->width(), ui->choosepanel->height() - ss(80)),
                ui->dirchooseok->move(ui->choosepanel->width() - ss(120), ui->choosepanel->height() - sz),
                ui->dirchoosecancel->move(ui->choosepanel->width() - ss(240), ui->choosepanel->height() - sz),
                ui->filesystemwarning->move(ui->filesystemwarning->x(), ui->choosepanel->height() - ss(41)),
                ui->chooseresize->move(ui->choosepanel->width() - ui->chooseresize->width(), ui->choosepanel->height() - ui->chooseresize->height());
            }
            else if(ui->excludepanel->isVisible())
            {
                ui->excludepanel->resize(width() - ui->excludepanel->x() * 2, height() - ss(25)),
                ui->excludeitemstext->resize(ui->excludepanel->width() / 2 - ss(44) + (sfctr == High ? 1 : 0), ui->excludeitemstext->height());
                ushort sz[]{ss(36), ss(24)};
                ui->excludedtext->setGeometry(ui->excludepanel->width() / 2 + sz[0], ui->excludedtext->y(), ui->excludeitemstext->width(), ui->excludedtext->height()),
                ui->excludeitemslist->resize(ui->excludeitemstext->width(), ui->excludepanel->height() - ss(160)),
                ui->excludedlist->setGeometry(ui->excludepanel->width() / 2 + sz[0], ui->excludedlist->y(), ui->excludeitemslist->width(), ui->excludeitemslist->height()),
                ui->excludeadditem->move(ui->excludepanel->width() / 2 - sz[1], ui->excludeitemslist->height() / 2 + sz[0]),
                ui->excluderemoveitem->move(ui->excludeadditem->x(), ui->excludeitemslist->height() / 2 + ss(108)),
                ui->excludeback->move(ui->excludeback->x(), ui->excludepanel->height() - ss(48)),
                ui->excludekendektext->move(ui->excludepanel->width() - ss(306), ui->excludepanel->height() - sz[1]),
                ui->excluderesize->move(ui->excludepanel->width() - ui->excluderesize->width(), ui->excludepanel->height() - ui->excluderesize->height());
            }
            else if(ui->includepanel->isVisible())
            {
                ui->includepanel->resize(width() - ui->includepanel->x() * 2, height() - ss(25)),
                ui->includetext->resize(ui->includepanel->width(), ui->includetext->height()),
                ui->includeitemstext->resize(ui->includepanel->width() / 2 - ss(44) + (sfctr == High ? 1 : 0), ui->includeitemstext->height());
                ushort sz[]{ss(36), ss(24)};
                ui->includedtext->setGeometry(ui->includepanel->width() / 2 + sz[0], ui->includedtext->y(), ui->includeitemstext->width(), ui->includedtext->height()),
                ui->includeitemslist->resize(ui->includeitemstext->width(), ui->includepanel->height() - ss(144)),
                ui->includedlist->setGeometry(ui->includepanel->width() / 2 + sz[0], ui->includedlist->y(), ui->includeitemslist->width(), ui->includeitemslist->height()),
                ui->includeadditem->move(ui->includepanel->width() / 2 - sz[1], ui->includeitemslist->height() / 2 + ss(19)),
                ui->includeremoveitem->move(ui->includeadditem->x(), ui->includeitemslist->height() / 2 + ss(91)),
                ui->includeback->move(ui->includeback->x(), ui->includepanel->height() - ss(48)),
                ui->includekendektext->move(ui->includepanel->width() - ss(306), ui->includepanel->height() - sz[1]),
                ui->includeresize->move(ui->includepanel->width() - ui->includeresize->width(), ui->includepanel->height() - ui->includeresize->height());
            }

            if(! wismax)
            {
                if(! wmblck)
                {
                    if(wgeom[2] != width()) wgeom[2] = width();
                    if(wgeom[3] != height()) wgeom[3] = height();
                }

                goto bcheck;
            }

            return false;
        case QEvent::Move:
            if(! wismax)
            {
                if(! wmblck)
                {
                    if(wgeom[0] != x()) wgeom[0] = x();
                    if(wgeom[1] != y()) wgeom[1] = y();
                }

                goto bcheck;
            }
            else if(! wmblck)
            {
                QRect agm(sgeom(true));

                if(geometry() != agm)
                {
                    setGeometry(agm);
                    return true;
                }
            }

            return false;
        case QEvent::FontChange:
            if(font() != bfnt) qApp->setFont(bfnt);
            return true;
        case QEvent::WindowStateChange:
        {
            QEvent nev(isMinimized() ? QEvent::WindowDeactivate : QEvent::WindowActivate);
            qApp->sendEvent(this, &nev);
        }
        default:
            return false;
        }

gcheck:
        if(! (wismax || wmblck))
        {
            QDW dtp(qApp->desktop());
            QRect agm(sgeom(true, dtp)), sgm;
            ushort frm(ss(30));

            if(agm.width() >= wgeom[2] + frm)
            {
                sgm = sgeom(false, dtp);

                if(x() < sgm.x())
                {
                    if(x() > sgm.x() - wgeom[2])
                    {
                        short val(sgm.x() + frm);
                        wgeom[0] = val < agm.x() ? agm.x() : val;
                    }
                }
                else if(x() < agm.x())
                    wgeom[0] = agm.x();
                else
                {
                    short bnd[2];

                    if(x() <= (bnd[0] = (bnd[1] = sgm.x() + sgm.width()) - wgeom[2]))
                    {
                        if(x() > (bnd[0] = agm.x() + agm.width() - wgeom[2])) wgeom[0] = bnd[0];
                    }
                    else if(x() < bnd[1] + wgeom[2])
                    {
                        short val(bnd[0] - frm);
                        wgeom[0] = val < (bnd[1] = agm.x() + agm.width() - wgeom[2]) ? val : bnd[1];
                    }
                }
            }
            else if(wgeom[0] != agm.x())
                wgeom[0] = agm.x();

            if(agm.height() >= wgeom[3] + frm)
            {
                if(sgm.isEmpty()) sgm = sgeom(false, dtp);

                if(y() < sgm.y())
                {
                    if(y() > sgm.y() - wgeom[3])
                    {
                        short val(sgm.y() + frm);
                        wgeom[1] = val < agm.y() ? agm.y() : val;
                    }
                }
                else if(y() < agm.y())
                    wgeom[1] = agm.y();
                else
                {
                    short bnd[2];

                    if(y() <= (bnd[0] = (bnd[1] = sgm.y() + sgm.height()) - wgeom[3]))
                    {
                        if(y() > (bnd[0] = agm.y() + agm.height() - wgeom[3])) wgeom[1] = bnd[0];
                    }
                    else if(y() < bnd[1] + wgeom[3])
                    {
                        short val(bnd[0] - frm);
                        wgeom[1] = val < (bnd[1] = agm.y() + agm.height() - wgeom[3]) ? val : bnd[1];
                    }
                }
            }
            else if(wgeom[1] != agm.y())
                wgeom[1] = agm.y();

            if(pos() != QPoint(wgeom[0], wgeom[1]))
            {
                move(wgeom[0], wgeom[1]);
                return false;
            }
        }

bcheck:
        if(ui->buttonspanel->isVisible() && ! ui->buttonspanel->y()) ui->buttonspanel->hide();
    }

    return false;
}

void systemback::keyPressEvent(QKeyEvent *ev)
{
    if(! qApp->overrideCursor())
        switch(ev->key()) {
        case Qt::Key_Escape:
            if(ui->passwordpanel->isVisible()) close();
            break;
        case Qt::Key_Enter:
        case Qt::Key_Return:
            { QKeyEvent press(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier);
            qApp->sendEvent(qApp->focusObject(), &press); }

            if(ui->sbpanel->isVisible())
            {
                if(ui->pointrename->isEnabled())
                {
                    schar num(-1);

                    for(QLE ldt : ui->points->findChildren<QLE>())
                    {
                        ++num;

                        if(ldt->hasFocus() && getppipe(num)->isChecked())
                        {
                            on_pointrename_clicked();
                            break;
                        }
                    }
                }
            }
            else if(ui->dirchoose->hasFocus())
                ui->dirchoose->currentItem()->setExpanded(! ui->dirchoose->currentItem()->isExpanded());
            else if(ui->repairmountpoint->hasFocus())
            {
                if(ui->repairmount->isEnabled())
                {
                    on_repairmount_clicked();
                    if(! ui->repairmountpoint->hasFocus()) ui->repairmountpoint->setFocus();
                }
            }
            else if(ui->excludeitemslist->hasFocus())
            {
                if(ui->excludeadditem->isEnabled())
                {
                    if(! ui->excludeitemslist->currentItem()->childCount())
                        on_excludeadditem_clicked();
                    else if(ui->excludeitemslist->currentItem()->isExpanded())
                        ui->excludeitemslist->currentItem()->setExpanded(false);
                    else
                        ui->excludeitemslist->currentItem()->setExpanded(true);
                }
            }
            else if(ui->excludedlist->hasFocus())
            {
                if(ui->excluderemoveitem->isEnabled()) on_excluderemoveitem_clicked();
            }
            else if(ui->includeitemslist->hasFocus())
            {
                if(ui->includeadditem->isEnabled())
                {
                    if(! ui->includeitemslist->currentItem()->childCount())
                        on_includeadditem_clicked();
                    else if(ui->includeitemslist->currentItem()->isExpanded())
                        ui->includeitemslist->currentItem()->setExpanded(false);
                    else
                        ui->includeitemslist->currentItem()->setExpanded(true);
                }
            }
            else if(ui->includedlist->hasFocus())
            {
                if(ui->includeremoveitem->isEnabled()) on_includeremoveitem_clicked();
            }
            else if(ui->copypanel->isVisible())
            {
                if(ui->mountpoint->hasFocus())
                {
                    if(ui->changepartition->isEnabled()) on_changepartition_clicked();
                }
                else if(ui->partitionsize->hasFocus() && ui->newpartition->isEnabled())
                    on_newpartition_clicked();
            }
            else if(ui->installpanel->isVisible())
            {
                if(ui->fullname->hasFocus())
                {
                    if(ui->fullnamepipe->isVisible()) ui->username->setFocus();
                }
                else if(ui->username->hasFocus())
                {
                    if(ui->usernamepipe->isVisible()) ui->password1->setFocus();
                }
                else if(ui->password1->hasFocus())
                {
                    if(ui->password2->isEnabled()) ui->password2->setFocus();
                }
                else if(ui->password2->hasFocus())
                {
                    if(ui->passwordpipe->isVisible()) ui->rootpassword1->setFocus();
                }
                else if(ui->rootpassword1->hasFocus())
                    ui->rootpassword2->isEnabled() ? ui->rootpassword2->setFocus() : ui->hostname->setFocus();
                else if(ui->rootpassword2->hasFocus())
                {
                    if(ui->rootpasswordpipe->isVisible()) ui->hostname->setFocus();
                }
                else if(ui->hostname->hasFocus() && ui->installnext->isEnabled())
                    ui->installnext->setFocus();
            }

            break;
        case Qt::Key_F5:
            if(ui->sbpanel->isVisible())
            {
                schar num(-1);

                for(QLE ldt : ui->points->findChildren<QLE>())
                {
                    ++num;

                    if(ldt->hasFocus())
                    {
                        if(ldt->text() != sb::pnames[num]) ldt->setText(sb::pnames[num]);
                        break;
                    }
                }
            }
            else if(ui->partitionsettings->hasFocus() || ui->mountpoint->hasFocus() || ui->partitionsize->hasFocus())
                on_partitionrefresh2_clicked();
            else if(ui->livepanel->isVisible())
            {
                if(ui->livename->hasFocus())
                {
                    if(ui->livename->text() != "auto") ui->livename->setText("auto");
                }
                else if(ui->livedevices->hasFocus())
                    on_livedevicesrefresh_clicked();
                else if(ui->livelist->hasFocus())
                    on_livemenu_clicked(), ui->liveback->setFocus();
            }
            else if(ui->dirchoose->hasFocus())
                on_dirrefresh_clicked();
            else if(ui->repairmountpoint->hasFocus())
                on_repairpartitionrefresh_clicked();
            else if(ui->excludeitemslist->hasFocus())
                ilstupdt();
            else if(ui->includeitemslist->hasFocus())
                ilstupdt(true);

            break;
        case Qt::Key_Delete:
            if(ui->partitionsettings->hasFocus())
            {
                if(ui->unmountdelete->isEnabled() && ui->unmountdelete->text() == tr("Unmount")) on_unmountdelete_clicked();
            }
            else if(ui->livelist->hasFocus())
            {
                if(ui->livedelete->isEnabled()) on_livedelete_clicked();
            }
            else if(ui->excludeitemslist->hasFocus())
            {
                if(ui->excludeadditem->isEnabled()) on_excludeadditem_clicked();
            }
            else if(ui->excludedlist->hasFocus())
            {
                if(ui->excluderemoveitem->isEnabled()) on_excluderemoveitem_clicked();
            }
            else if(ui->includeitemslist->hasFocus())
            {
                if(ui->includeadditem->isEnabled()) on_includeadditem_clicked();
            }
            else if(ui->includedlist->hasFocus() && ui->includeremoveitem->isEnabled())
                on_includeremoveitem_clicked();
        }
}

void systemback::keyReleaseEvent(QKeyEvent *ev)
{
    if(fscrn && ui->wpanel->isHidden())
    {
        ui->wpanel->show();
        fwdgt->setFocus();
        ui->logo->setFocusPolicy(Qt::NoFocus);
    }
    else if(! qApp->overrideCursor())
        switch(ev->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
            QKeyEvent release(QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier);
            qApp->sendEvent(qApp->focusObject(), &release);
        }
}

void systemback::on_admins_currentIndexChanged(cQStr &arg1)
{
    ui->admins->resize(fontMetrics().width(arg1) + ss(30), ui->admins->height());
    if(! hash.isEmpty()) hash.clear();

    {
        QFile file("/etc/shadow");

        if(sb::fopen(file))
            while(! file.atEnd())
            {
                QStr cline(file.readLine().trimmed());

                if(cline.startsWith(arg1 % ':'))
                {
                    hash = sb::mid(cline, arg1.length() + 2, sb::instr(cline, ":", arg1.length() + 2) - arg1.length() - 2);
                    break;
                }
            }
    }

    if(ui->adminpassword->text().length()) ui->adminpassword->clear();

    if(! hash.isEmpty() && QStr(crypt("", bstr(hash))) == hash)
    {
        ui->adminpasswordpipe->show();
        for(QWdt wdgt : QWL{ui->adminpassword, ui->admins}) wdgt->setDisabled(true);
        ui->passwordinputok->setEnabled(true);
    }
}

void systemback::on_adminpassword_textChanged(cQStr &arg1)
{
    uchar ccnt(icnt == 100 ? icnt = 0 : ++icnt);

    if(arg1.isEmpty())
    {
        if(ui->adminpassworderror->isVisible()) ui->adminpassworderror->hide();
    }
    else if(! hash.isEmpty() && QStr(crypt(bstr(arg1), bstr(hash))) == hash)
    {
        sb::delay(300);

        if(ccnt == icnt)
        {
            if(ui->adminpassworderror->isVisible()) ui->adminpassworderror->hide();
            ui->adminpasswordpipe->show();
            for(QWdt wdgt : QWL{ui->adminpassword, ui->admins}) wdgt->setDisabled(true);
            ui->passwordinputok->setEnabled(true), ui->passwordinputok->setFocus();
        }
    }
    else if(ui->adminpassworderror->isHidden())
        ui->adminpassworderror->show();
}

void systemback::on_startcancel_clicked()
{
    close();
}

void systemback::on_passwordinputok_clicked()
{
    busy();

    QTimer::singleShot(0, this,
#if QT_VERSION < QT_VERSION_CHECK(5, 4, 0)
        SLOT(unitimer())
#else
        &systemback::unitimer
#endif
        );

    ui->passwordpanel->hide(),
    ui->mainpanel->show(),
    ui->sbpanel->isVisible() ? ui->functionmenunext->setFocus() : ui->fullname->setFocus(),
    windowmove(ss(698), ss(465)),
    setwontop(false);
}

void systemback::on_schedulerstart_clicked()
{
    delete shdltimer, shdltimer = nullptr,
    ui->function2->setText("Systemback " % tr("scheduler")),
    on_newrestorepoint_clicked();
}

void systemback::on_dialogcancel_clicked()
{
    if(! fscrn)
    {
        if(dialog != 108)
        {
            if(dlgtimer)
            {
                if(dialog == 211) return void(close());
                delete dlgtimer, dlgtimer = nullptr;
                if(ui->dialognumber->text() != "30s") ui->dialognumber->setText("30s");
            }

            if(! ui->sbpanel->isVisibleTo(ui->mainpanel))
            {
                if(ui->restorepanel->isVisibleTo(ui->mainpanel))
                    ui->restorepanel->hide();
                else if(ui->copypanel->isVisibleTo(ui->mainpanel))
                    ui->copypanel->hide();
                else if(ui->livepanel->isVisibleTo(ui->mainpanel))
                    ui->livepanel->hide();
                else if(ui->repairpanel->isVisibleTo(ui->mainpanel))
                    ui->repairpanel->hide();

                ui->sbpanel->show(),
                ui->function1->setText("Systemback");
            }

            for(QCB ckbx : ui->sbpanel->findChildren<QCB>())
                if(ckbx->isChecked())
                {
                    ckbx->click();
                    break;
                }
        }

        ui->dialogpanel->hide(),
        ui->mainpanel->show(),
        ui->functionmenunext->setFocus(),
        windowmove(ss(698), ss(465)),
        setwontop(false);
    }
    else if(ui->dialogok->text() == tr("Reboot"))
        close();
    else
    {
        ui->dialogpanel->hide(),
        ui->mainpanel->show();
        short nwidth(ss(154) + ui->partitionsettings->width() - ui->partitionsettings->contentsRect().width() + ui->partitionsettings->columnWidth(0) + ui->partitionsettings->columnWidth(1) + ui->partitionsettings->columnWidth(2) + ui->partitionsettings->columnWidth(3) + ui->partitionsettings->columnWidth(4) + ui->partitionsettings->columnWidth(5) + ui->partitionsettings->columnWidth(6));
        ushort sz[]{ss(850), ss(465), ss(60)};
        windowmove(nwidth < sz[0] ? nwidth : sz[0], sz[1], false),
        ui->wpanel->setMinimumSize(ss(698), sz[1]), ui->wpanel->setMaximumSize(width() - sz[2], height() - sz[2]);
    }
}

void systemback::pnmchange(uchar num)
{
    if(sb::pnumber != num) sb::pnumber = num;
    uchar cnum(0);

    for(QLE ldt : ui->points->findChildren<QLE>())
        switch(++cnum) {
        case 1 ... 2:
            break;
        case 3:
            if(ldt->isEnabled())
                switch(num) {
                case 3:
                    if(ldt->styleSheet().isEmpty()) ldt->setStyleSheet("background-color: rgb(255, 103, 103)");
                    break;
                default:
                    if(! ldt->styleSheet().isEmpty()) ldt->setStyleSheet(nullptr);
                }

            break;
        case 11:
            return;
        default:
            if(ldt->isEnabled())
            {
                if(cnum < num)
                {
                    if(! ldt->styleSheet().isEmpty()) ldt->setStyleSheet(nullptr);
                }
                else if(ldt->styleSheet().isEmpty())
                    ldt->setStyleSheet("background-color: rgb(255, 103, 103)");
            }
            else if(cnum <= num)
            {
                if(ldt->text() == tr("not used")) ldt->setText(tr("empty"));
            }
            else if(ldt->text() == tr("empty"))
                ldt->setText(tr("not used"));
        }
}

void systemback::on_pnumber3_clicked()
{
    pnmchange(3);
}

void systemback::on_pnumber4_clicked()
{
    pnmchange(4);
}

void systemback::on_pnumber5_clicked()
{
    pnmchange(5);
}

void systemback::on_pnumber6_clicked()
{
    pnmchange(6);
}

void systemback::on_pnumber7_clicked()
{
    pnmchange(7);
}

void systemback::on_pnumber8_clicked()
{
    pnmchange(8);
}

void systemback::on_pnumber9_clicked()
{
    pnmchange(9);
}

void systemback::on_pnumber10_clicked()
{
    pnmchange(10);
}

void systemback::ptxtchange(uchar num, cQStr &txt)
{
    QLE ldt(getpoint(num));
    QCB ckbx(getppipe(num));

    if(ldt->isEnabled())
    {
        if(txt.isEmpty())
        {
            if(ckbx->isChecked()) ckbx->click();
            ckbx->setDisabled(true);
        }
        else if(sb::like(txt, {"* *", "*/*"}))
        {
            uchar cps(ldt->cursorPosition() - 1);
            ldt->setText(QStr(txt).replace(cps, 1, nullptr)),
            ldt->setCursorPosition(cps);
        }
        else
        {
            if(! ckbx->isEnabled()) ckbx->setEnabled(true);
            if(! ldt->hasFocus()) ldt->setCursorPosition(0);

            if(txt != sb::pnames[num])
            {
                if(! ckbx->isChecked()) ckbx->click();
                if(! ui->pointrename->isEnabled()) ui->pointrename->setEnabled(true);
            }
            else if(ckbx->isChecked())
                ckbx->click();
        }
    }
    else if(ckbx->isEnabled())
    {
        if(ckbx->isChecked()) ckbx->click();
        ckbx->setDisabled(true);
    }
}

void systemback::on_point1_textChanged(cQStr &arg1)
{
    ptxtchange(0, arg1);
}

void systemback::on_point2_textChanged(cQStr &arg1)
{
    ptxtchange(1, arg1);
}

void systemback::on_point3_textChanged(cQStr &arg1)
{
    ptxtchange(2, arg1);
}

void systemback::on_point4_textChanged(cQStr &arg1)
{
    ptxtchange(3, arg1);
}

void systemback::on_point5_textChanged(cQStr &arg1)
{
    ptxtchange(4, arg1);
}

void systemback::on_point6_textChanged(cQStr &arg1)
{
    ptxtchange(5, arg1);
}

void systemback::on_point7_textChanged(cQStr &arg1)
{
    ptxtchange(6, arg1);
}

void systemback::on_point8_textChanged(cQStr &arg1)
{
    ptxtchange(7, arg1);
}

void systemback::on_point9_textChanged(cQStr &arg1)
{
    ptxtchange(8, arg1);
}

void systemback::on_point10_textChanged(cQStr &arg1)
{
    ptxtchange(9, arg1);
}

void systemback::on_point11_textChanged(cQStr &arg1)
{
    ptxtchange(10, arg1);
}

void systemback::on_point12_textChanged(cQStr &arg1)
{
    ptxtchange(11, arg1);
}

void systemback::on_point13_textChanged(cQStr &arg1)
{
    ptxtchange(12, arg1);
}

void systemback::on_point14_textChanged(cQStr &arg1)
{
    ptxtchange(13, arg1);
}

void systemback::on_point15_textChanged(cQStr &arg1)
{
    ptxtchange(14, arg1);
}

void systemback::on_restoremenu_clicked()
{
    if(sb::execsrch("update-grub2", sb::sdir[1] % '/' % cpoint % '_' % pname) && sb::isfile(sb::sdir[1] % '/' % cpoint % '_' % pname % "/var/lib/dpkg/info/grub-" % grub.name % ".list"))
    {
        if(ui->grubreinstallrestoredisable->isVisibleTo(ui->restorepanel)) ui->grubreinstallrestoredisable->hide(),
                                                                           ui->grubreinstallrestore->show();
    }
    else if(ui->grubreinstallrestore->isVisibleTo(ui->restorepanel))
        ui->grubreinstallrestore->hide(),
        ui->grubreinstallrestoredisable->show();

    if(ui->includeusers->count()) ui->includeusers->clear();
    ui->includeusers->addItems({tr("Everyone"), "root"});
    if(! ui->restorenext->isEnabled()) ui->restorenext->setEnabled(true);
    ui->sbpanel->hide(),
    ui->restorepanel->show(),
    ui->function1->setText(tr("System restore")),
    ui->restoreback->setFocus();
    QFile file("/etc/passwd");

    if(sb::fopen(file))
        while(! file.atEnd())
        {
            QStr usr(file.readLine().trimmed());
            if(usr.contains(":/home/") && sb::isdir("/home/" % (usr = sb::left(usr, sb::instr(usr, ":") -1)))) ui->includeusers->addItem(usr);
        }
}

void systemback::on_copymenu_clicked()
{
    if(! grub.isEFI || ui->grubinstallcopy->isVisibleTo(ui->copypanel))
    {
        if(ppipe ? sb::execsrch("update-grub2", sb::sdir[1] % '/' % cpoint % '_' % pname) && sb::isfile(sb::sdir[1] % '/' % cpoint % '_' % pname % "/var/lib/dpkg/info/grub-" % grub.name % ".list") : sb::execsrch("update-grub2") && sb::isfile("/var/lib/dpkg/info/grub-" % grub.name % ".list"))
        {
            if(ui->grubinstallcopydisable->isVisibleTo(ui->copypanel)) ui->grubinstallcopydisable->hide(),
                                                                       ui->grubinstallcopy->show();
        }
        else if(ui->grubinstallcopy->isVisibleTo(ui->copypanel))
            ui->grubinstallcopy->hide(),
            ui->grubinstallcopydisable->show();
    }

    if(ui->usersettingscopy->isVisibleTo(ui->copypanel)) ui->usersettingscopy->hide(),
                                                         ui->userdatafilescopy->show();

    if(! ppipe)
    {
        if(! ui->userdatafilescopy->isEnabled()) ui->userdatafilescopy->setEnabled(true);
    }
    else if(ui->userdatafilescopy->isEnabled())
    {
        ui->userdatafilescopy->setDisabled(true);
        if(ui->userdatafilescopy->isChecked()) ui->userdatafilescopy->setChecked(false);
    }

    ui->sbpanel->hide(),
    ui->copypanel->show(),
    ui->function1->setText(tr("System copy")),
    ui->copyback->setFocus();

    {
        short nwidth(ss(154) + ui->partitionsettings->width() - ui->partitionsettings->contentsRect().width() + ui->partitionsettings->columnWidth(0) + ui->partitionsettings->columnWidth(1) + ui->partitionsettings->columnWidth(2) + ui->partitionsettings->columnWidth(3) + ui->partitionsettings->columnWidth(4) + ui->partitionsettings->columnWidth(5) + ui->partitionsettings->columnWidth(6));
        ushort sz[]{ss(698), ss(465), ss(60)};

        if(nwidth > sz[0])
        {
            ushort sz1(ss(850));
            windowmove(nwidth < sz1 ? nwidth : sz1, sz[1], false);
        }

        setMinimumSize(sz[0], sz[1]);
        QRect agm(sgeom(true));
        setMaximumSize(agm.width() - sz[2], agm.height() - sz[2]);
    }

    if(ui->partitionsettings->currentItem())
    {
        if(sb::mcheck("/.sbsystemcopy/") || sb::mcheck("/.sblivesystemwrite/"))
            on_partitionrefresh_clicked();
        else if(ui->mountpoint->isEnabled())
        {
            if(! ui->mountpoint->currentText().isEmpty()) on_mountpoint_currentTextChanged(ui->mountpoint->currentText());
        }
        else if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->text() == "/home")
            ui->mountpoint->setEnabled(true);
    }
}

void systemback::on_installmenu_clicked()
{
    ui->sbpanel->hide(),
    ui->installpanel->show(),
    ui->function1->setText(tr("System install")),
    ui->fullname->setFocus();
}

void systemback::on_livemenu_clicked()
{
    if(ui->livelist->count()) ui->livelist->clear();

    for(QWdt wdgt : QWL{ui->livedelete, ui->liveconvert, ui->livewritestart})
        if(wdgt->isEnabled()) wdgt->setDisabled(true);

    if(ui->sbpanel->isVisible())
    {
        ui->sbpanel->hide(),
        ui->livepanel->show(),
        ui->function1->setText(tr("Live system create")),
        ui->liveback->setFocus();
    }

    if(sb::isdir(sb::sdir[2]))
        for(cQStr &item : QDir(sb::sdir[2]).entryList(QDir::Files | QDir::Hidden))
            if(item.endsWith(".sblive") && ! (item.contains(' ') || sb::islink(sb::sdir[2] % '/' % item)) && sb::fsize(sb::sdir[2] % '/' % item))
            {
                QLWI *lwi(new QLWI(sb::left(item, -7) % " (" % QStr::number(qRound64(sb::fsize(sb::sdir[2] % '/' % item) * 100.0 / 1024.0 / 1024.0 / 1024.0) / 100.0) % " GiB, " % (sb::stype(sb::sdir[2] % '/' % sb::left(item, -6) % "iso") == sb::Isfile && sb::fsize(sb::sdir[2] % '/' % sb::left(item, -6) % "iso") ? "sblive+iso" : "sblive") % ')'));
                ui->livelist->addItem(lwi);
            }
}

void systemback::on_repairmenu_clicked()
{
    if(ppipe || pname == tr("Live image"))
    {
        if(! ui->systemrepair->isEnabled())
            for(QWdt wdgt : QWL{ui->systemrepair, ui->fullrepair}) wdgt->setEnabled(true);

        rmntcheck();
    }
    else if(ui->systemrepair->isEnabled())
    {
        for(QWdt wdgt : QWL{ui->systemrepair, ui->fullrepair}) wdgt->setDisabled(true);
        if(! ui->grubrepair->isChecked()) ui->grubrepair->click();
    }

    on_repairmountpoint_currentTextChanged(ui->repairmountpoint->currentText()),
    ui->sbpanel->hide(),
    ui->repairpanel->show(),
    ui->function1->setText(tr("System repair")),
    ui->repairback->setFocus();
}

void systemback::on_systemupgrade_clicked()
{
    statustart(), pset(11);
    QDateTime ofdate(QFileInfo("/usr/bin/systemback").lastModified());
    sb::unlock(sb::Dpkglock), sb::unlock(sb::Aptlock),
    sb::exec("xterm +sb -bg grey85 -fg grey25 -fa a -fs 9 -geometry 80x24+" % QStr::number(ss(80)) % '+' % QStr::number(ss(70)) % " -n \"System upgrade\" -T \"System upgrade\" -cr grey40 -selbg grey86 -bw 0 -bc -bcf 500 -bcn 500 -e sbsysupgrade", sb::Noflag, "DBGLEV=0");

    if(isVisible())
    {
        if(ofdate != QFileInfo("/usr/bin/systemback").lastModified())
            nrxth = true,
            sb::unlock(sb::Sblock),
            sb::exec("systemback", sb::Bckgrnd),
            close();
        else if(sb::lock(sb::Dpkglock) && sb::lock(sb::Aptlock))
            ui->statuspanel->hide(),
            ui->mainpanel->show(),
            ui->functionmenunext->setFocus(),
            windowmove(ss(698), ss(465));
        else
            utimer.stop(),
            dialogopen(301);
    }
}

void systemback::on_excludemenu_clicked()
{
    ui->sbpanel->hide(),
    ui->excludepanel->show(),
    ui->function1->setText(tr("Exclude")),
    ui->excludeback->setFocus();
    QRect agm(sgeom(true));
    ushort sz(ss(60));
    setMaximumSize(agm.width() - sz, agm.height() - sz);
}

void systemback::on_includemenu_clicked()
{
    ui->sbpanel->hide(),
    ui->includepanel->show(),
    ui->function1->setText(tr("Include")),
    ui->includeback->setFocus();
    QRect agm(sgeom(true));
    ushort sz(ss(60));
    setMaximumSize(agm.width() - sz, agm.height() - sz);
}

void systemback::on_schedulemenu_clicked()
{
    ui->sbpanel->hide(),
    ui->schedulepanel->show(),
    ui->function1->setText(tr("Schedule")),
    ui->schedulerback->setFocus();
}

void systemback::on_aboutmenu_clicked()
{
    ui->sbpanel->hide(),
    ui->aboutpanel->show(),
    ui->function1->setText(tr("About")),
    ui->aboutback->setFocus();
}

void systemback::on_settingsmenu_clicked()
{
    ui->sbpanel->hide(),
    ui->settingspanel->show(),
    ui->function1->setText(tr("Settings")),
    ui->settingsback->setFocus();
}

void systemback::on_partitionrefresh_clicked()
{
    busy();
    if(! ui->copycover->isVisibleTo(ui->copypanel)) ui->copycover->show();
    if(ui->copynext->isEnabled()) ui->copynext->setDisabled(true);
    if(ui->mountpoint->count()) ui->mountpoint->clear();
    ui->mountpoint->addItems({nullptr, "/", "/home", "/boot"});

    if(grub.isEFI)
    {
        ui->mountpoint->addItem("/boot/efi");
        if(! ui->efiwarning->isVisibleTo(ui->copypanel)) ui->efiwarning->show();

        if(ui->grubinstallcopy->isVisibleTo(ui->copypanel)) ui->grubinstallcopy->hide(),
                                                            ui->grubinstallcopydisable->show();
    }

    ui->mountpoint->addItems({"/tmp", "/usr", "/var", "/srv", "/opt", "/usr/local", "SWAP"});

    if(ui->mountpoint->isEnabled())
    {
        ui->mountpoint->setDisabled(true);
        if(ui->changepartition->isEnabled()) ui->changepartition->setDisabled(true);
    }

    if(ui->format->isEnabled())
    {
        ui->format->setDisabled(true);
        if(ui->filesystem->isEnabled()) ui->filesystem->setDisabled(true);
    }

    if(ui->unmountdelete->text() == tr("! Delete !")) ui->unmountdelete->setText(tr("Unmount")),
                                                      ui->unmountdelete->setStyleSheet(nullptr);

    if(ui->unmountdelete->isEnabled()) ui->unmountdelete->setDisabled(true);

    if(nohmcpy[1])
    {
        nohmcpy[1] = false;

        if(! ppipe)
            ui->userdatafilescopy->setEnabled(true);
        else if(ui->userdatafilescopy->isChecked())
            ui->userdatafilescopy->setChecked(false);
    }

    if(ui->partitionsettings->rowCount()) ui->partitionsettings->clearContents();
    if(ui->repairpartition->count()) ui->repairpartition->clear();

    if(! wismax)
        for(uchar a(2) ; a < 5 ; ++a) ui->partitionsettings->resizeColumnToContents(a);

    QSL plst;
    sb::readprttns(plst);

    if(! grub.isEFI)
    {
        if(ui->grubinstallcopy->count())
            for(QCbB cmbx : QCbBL{ui->grubinstallcopy, ui->grubreinstallrestore, ui->grubreinstallrepair}) cmbx->clear();

        for(QCbB cmbx : QCbBL{ui->grubinstallcopy, ui->grubreinstallrestore, ui->grubreinstallrepair}) cmbx->addItems({"Auto", tr("Disabled")});

        for(cQStr &dts : plst)
        {
            QStr path(dts.split('\n').at(0));

            if(sb::like(path.length(), {8, 12}))
                for(QCbB cmbx : QCbBL{ui->grubinstallcopy, ui->grubreinstallrestore, ui->grubreinstallrepair}) cmbx->addItem(path);
        }
    }

    schar sn(-1);
    QBA mnts[]{sb::fload("/proc/self/mounts"), sb::fload("/proc/swaps")};

    for(cQStr &cdts : plst)
    {
        QSL dts(cdts.split('\n'));
        cQStr &path(dts.at(0)), &type(dts.at(2));
        ullong bsize(dts.at(1).toULongLong());

        if(sb::like(path.length(), {8, 12}))
        {
            ui->partitionsettings->setRowCount(++sn + 1);
            if(sn) ui->partitionsettings->setRowHeight(sn, ss(25));
            { QTblWI *dev(new QTblWI(path));
            dev->setTextAlignment(Qt::AlignBottom),
            ui->partitionsettings->setItem(sn, 0, dev);
            QTblWI *rsize(new QTblWI(sb::hunit(bsize)));
            rsize->setTextAlignment(Qt::AlignRight | Qt::AlignBottom),
            ui->partitionsettings->setItem(sn, 1, rsize);
            QFont fnt;
            fnt.setWeight(QFont::DemiBold);
            for(QTblWI *twi : {dev, rsize}) twi->setFont(fnt); }
            for(uchar a(2) ; a < 7 ; ++a) ui->partitionsettings->setItem(sn, a, new QTblWI);
            QTblWI *tp(new QTblWI(type));
            ui->partitionsettings->setItem(sn, 8, tp);
            QTblWI *lngth(new QTblWI(QStr::number(bsize)));
            ui->partitionsettings->setItem(sn, 10, lngth);
        }
        else
        {
            switch(type.toUShort()) {
            case sb::Extended:
                ui->partitionsettings->setRowCount(++sn + 1);
                { QTblWI *dev(new QTblWI(path));
                ui->partitionsettings->setItem(sn, 0, dev);
                QTblWI *rsize(new QTblWI(sb::hunit(bsize)));
                rsize->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter),
                ui->partitionsettings->setItem(sn, 1, rsize);
                QFont fnt;
                fnt.setWeight(QFont::DemiBold), fnt.setItalic(true);
                for(QTblWI *twi : {dev, rsize}) twi->setFont(fnt); }
                for(uchar a(2) ; a < 7 ; ++a) ui->partitionsettings->setItem(sn, a, new QTblWI);
                break;
            case sb::Primary:
            case sb::Logical:
                if(! grub.isEFI)
                    for(QCbB cmbx : QCbBL{ui->grubinstallcopy, ui->grubreinstallrestore, ui->grubreinstallrepair}) cmbx->addItem(path);

                ui->partitionsettings->setRowCount(++sn + 1);
                { QTblWI *dev(new QTblWI(path));
                ui->partitionsettings->setItem(sn, 0, dev);
                QTblWI *rsize(new QTblWI(sb::hunit(bsize)));
                rsize->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter),
                ui->partitionsettings->setItem(sn, 1, rsize);
                QTblWI *lbl(new QTblWI(dts.at(5)));
                lbl->setTextAlignment(Qt::AlignCenter);
                if(! dts.at(5).isEmpty()) lbl->setToolTip(dts.at(5));
                ui->partitionsettings->setItem(sn, 2, lbl); }

                {
                    QTblWI *mpt(new QTblWI([&]() -> QStr {
                            cQStr &uuid(dts.at(6));

                            if(uuid.isEmpty())
                            {
                                if(QStr('\n' % mnts[0]).contains('\n' % path % ' '))
                                {
                                    QStr mnt(sb::right(mnts[0], -sb::instr(mnts[0], path % ' ')));
                                    short spc(sb::instr(mnt, " "));
                                    return sb::mid(mnt, spc + 1, sb::instr(mnt, " ", spc + 1) - spc - 1).replace("\\040", " ");
                                }
                            }
                            else if(QStr('\n' % mnts[0]).contains('\n' % path % ' '))
                                return QStr('\n' % mnts[0]).count('\n' % path % ' ') > 1 || QStr('\n' % mnts[0]).contains("\n/dev/disk/by-uuid/" % uuid % ' ') ? tr("Multiple mount points") : [&] {
                                        QStr mnt(sb::right(mnts[0], -sb::instr(mnts[0], path % ' ')));
                                        short spc(sb::instr(mnt, " "));
                                        return sb::mid(mnt, spc + 1, sb::instr(mnt, " ", spc + 1) - spc - 1).replace("\\040", " ");
                                    }();
                            else if(QStr('\n' % mnts[0]).contains("\n/dev/disk/by-uuid/" % uuid % ' '))
                                return QStr('\n' % mnts[0]).count("\n/dev/disk/by-uuid/" % uuid % ' ') > 1 ? tr("Multiple mount points") : [&] {
                                        QStr mnt(sb::right(mnts[0], -sb::instr(mnts[0], "/dev/disk/by-uuid/" % uuid % ' ')));
                                        short spc(sb::instr(mnt, " "));
                                        return sb::mid(mnt, spc + 1, sb::instr(mnt, " ", spc + 1) - spc - 1).replace("\\040", " ");
                                    }();
                            else if(QStr('\n' % mnts[1]).contains('\n' % path % ' '))
                                return "SWAP";

                            ui->repairpartition->addItem(path);
                            return nullptr;
                        }()));

                    if(! mpt->text().isEmpty()) mpt->setToolTip(mpt->text());
                    ui->partitionsettings->setItem(sn, 3, mpt);
                    QTblWI *empty(new QTblWI);
                    ui->partitionsettings->setItem(sn, 4, empty);
                    QTblWI *fs(new QTblWI(dts.at(4)));
                    ui->partitionsettings->setItem(sn, 5, fs);
                    QTblWI *frmt(new QTblWI("-"));
                    ui->partitionsettings->setItem(sn, 6, frmt),
                    ui->partitionsettings->setItem(sn, 7, fs->clone());
                    for(QTblWI *wdi : QList<QTblWI *>{mpt, empty, fs, frmt}) wdi->setTextAlignment(Qt::AlignCenter);
                }

                break;
            case sb::Freespace:
            case sb::Emptyspace:
                ui->partitionsettings->setRowCount(++sn + 1);
                { QTblWI *dev(new QTblWI(path));
                ui->partitionsettings->setItem(sn, 0, dev);
                QTblWI *rsize(new QTblWI(sb::hunit(bsize)));
                rsize->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter),
                ui->partitionsettings->setItem(sn, 1, rsize);
                QFont fnt;
                fnt.setItalic(true);
                for(QTblWI *twi : {dev, rsize}) twi->setFont(fnt); }
                for(uchar a(2) ; a < 7 ; ++a) ui->partitionsettings->setItem(sn, a, new QTblWI);
                break;
            }

            QTblWI *tp(new QTblWI(type));
            ui->partitionsettings->setItem(sn, 8, tp);
            QTblWI *start(new QTblWI(dts.at(3)));
            ui->partitionsettings->setItem(sn, 9, start);
            QTblWI *lngth(new QTblWI(QStr::number(bsize)));
            ui->partitionsettings->setItem(sn, 10, lngth);
        }
    }

    for(uchar a(0) ; a < 7 ; ++a)
        if(wismax || a < 2 || a > 4) ui->partitionsettings->resizeColumnToContents(a);

    if(ui->copypanel->isVisible() && ! ui->copyback->hasFocus()) ui->copyback->setFocus();
    ui->copycover->hide(), busy(false);
}

void systemback::on_partitionrefresh2_clicked()
{
    on_partitionrefresh_clicked();

    if(! ui->partitionsettingspanel1->isVisibleTo(ui->copypanel)) ui->partitionsettingspanel2->isVisibleTo(ui->copypanel) ? ui->partitionsettingspanel2->hide() : ui->partitionsettingspanel3->hide(),
                                                                  ui->partitionsettingspanel1->show();
}

void systemback::on_partitionrefresh3_clicked()
{
    on_partitionrefresh2_clicked();
}

void systemback::on_unmountdelete_clicked()
{
    busy(), ui->copycover->show();

    if(ui->unmountdelete->text() == tr("Unmount"))
    {
        if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->text() != "SWAP")
        {
            QStr mnts(sb::fload("/proc/self/mounts", true));
            QTS in(&mnts, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr cline(in.readLine()), mpt(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->text().replace(" ", "\\040"));
                if(sb::like(cline, {"* " % mpt % " *", "* " % mpt % "/*"})) sb::umount(cline.split(' ').value(1).replace("\\040", " "));
            }

            mnts = sb::fload("/proc/self/mounts");

            for(ushort a(0) ; a < ui->partitionsettings->rowCount() ; ++a)
            {
                QStr mpt(ui->partitionsettings->item(a, 3)->text());

                if(! (mpt.isEmpty() || sb::like(mpt, {"_SWAP_", '_' % tr("Multiple mount points") % '_'}) || mnts.contains(' ' % mpt.replace(" ", "\\040") % ' '))) ui->partitionsettings->item(a, 3)->setText(nullptr),
                                                                                                                                                                    ui->partitionsettings->item(a, 3)->setToolTip(nullptr);
            }

            sb::fssync();
        }
        else if(! (QStr('\n' % sb::fload("/proc/swaps")).contains('\n' % ui->partitionsettings->item(ui->partitionsettings->currentRow(), 0)->text() % ' ') && swapoff(bstr(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 0)->text()))))
            ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->setText(nullptr),
            ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->setToolTip(nullptr);

        if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->text().isEmpty())
        {
            ui->unmountdelete->setText(tr("! Delete !")),
            ui->unmountdelete->setStyleSheet("QPushButton:enabled{color: red}");
            if(minside(ui->unmountdelete)) ui->unmountdelete->setDisabled(true);
            if(! ui->mountpoint->isEnabled()) ui->mountpoint->setEnabled(true);
            for(QWdt wdgt : QWL{ui->filesystem, ui->format}) wdgt->setEnabled(true);
        }

        ui->copycover->hide();
    }
    else
        sb::delpart(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 0)->text()),
        on_partitionrefresh2_clicked();

    busy(false);
}

void systemback::on_unmount_clicked()
{
    busy(), ui->copycover->show();
    bool umntd(true);

    {
        QStr mnts[2]{sb::fload("/proc/self/mounts", true)};

        {
            QSL umlst;
            umlst.reserve(ui->partitionsettings->rowCount() - 1);

            for(ushort a(ui->partitionsettings->currentRow() + 1) ; a < ui->partitionsettings->rowCount() && ui->partitionsettings->item(a, 0)->background() != QBrush() ; ++a)
            {
                QStr mpt(ui->partitionsettings->item(a, 3)->text());

                if(! mpt.isEmpty())
                {
                    if(mpt == "SWAP")
                        swapoff(bstr(ui->partitionsettings->item(a, 0)->text()));
                    else
                    {
                        QTS in(&mnts[0], QIODevice::ReadOnly);
                        QSL incl{"* " % mpt.replace(" ", "\\040") % " *", "* " % mpt % "/*"};

                        while(! in.atEnd())
                        {
                            QStr cline(in.readLine());

                            if(sb::like(cline, incl))
                            {
                                QSL pslst(cline.split(' '));

                                if(! umlst.contains(pslst.at(0))) sb::umount(pslst.value(1).replace("\\040", " ")),
                                                                  umlst.append(pslst.at(0));
                            }
                        }
                    }
                }
            }
        }

        mnts[0] = sb::fload("/proc/self/mounts"), mnts[1] = sb::fload("/proc/swaps");

        for(ushort a(ui->partitionsettings->currentRow() + 1) ; a < ui->partitionsettings->rowCount() && ui->partitionsettings->item(a, 0)->background() != QBrush() ; ++a)
        {
            QStr mpt(ui->partitionsettings->item(a, 3)->text());

            if(! mpt.isEmpty())
            {
                if((mpt == "SWAP" && ! QStr('\n' % mnts[1]).contains('\n' % ui->partitionsettings->item(a, 0)->text() % ' ')) || ! (mpt == "SWAP" || mnts[0].contains(' ' % mpt.replace(" ", "\\040") % ' ')))
                    ui->partitionsettings->item(a, 3)->setText(nullptr),
                    ui->partitionsettings->item(a, 3)->setToolTip(nullptr);
                else if(umntd)
                    umntd = false;
            }
        }
    }

    sb::fssync();

    if(umntd) ui->unmount->setDisabled(true),
              ui->partitiondelete->setEnabled(true);

    ui->copycover->hide(), busy(false);
}

void systemback::on_restoreback_clicked()
{
    ui->restorepanel->hide(),
    ui->sbpanel->show(),
    ui->function1->setText("Systemback"),
    ui->functionmenunext->setFocus();
}

void systemback::on_copyback_clicked()
{
    if(ui->copycover->isHidden())
    {
        windowmove(ss(698), ss(465)),
        ui->copypanel->hide();

        if(ui->function1->text() == tr("System copy"))
            ui->sbpanel->show(),
            ui->function1->setText("Systemback"),
            ui->functionmenunext->setFocus();
        else
            ui->installpanel->show(),
            ui->fullname->setFocus();

        ui->partitionsettings->resizeColumnToContents(6);
    }
}

void systemback::on_installback_clicked()
{
    ui->installpanel->hide(),
    ui->sbpanel->show(),
    ui->function1->setText("Systemback"),
    ui->functionmenunext->setFocus();
}

void systemback::on_liveback_clicked()
{
    if(ui->livecover->isHidden()) ui->livepanel->hide(),
                                  ui->sbpanel->show(),
                                  ui->function1->setText("Systemback"),
                                  ui->functionmenunext->setFocus();
}

void systemback::on_repairback_clicked()
{
    if(ui->repaircover->isHidden()) ui->repairpanel->hide(),
                                    ui->sbpanel->show(),
                                    ui->function1->setText("Systemback"),
                                    ui->functionmenunext->setFocus();
}

void systemback::on_excludeback_clicked()
{
    if(ui->excludecover->isHidden()) windowmove(ss(698), ss(465)),
                                     ui->excludepanel->hide(),
                                     ui->sbpanel->show(),
                                     ui->function1->setText("Systemback"),
                                     ui->functionmenunext->setFocus();
}

void systemback::on_includeback_clicked()
{
    if(ui->includecover->isHidden()) windowmove(ss(698), ss(465)),
                                     ui->includepanel->hide(),
                                     ui->sbpanel->show(),
                                     ui->function1->setText("Systemback"),
                                     ui->functionmenunext->setFocus();
}

void systemback::on_schedulerback_clicked()
{
    ui->schedulepanel->hide(),
    ui->sbpanel->show(),
    ui->function1->setText("Systemback"),
    ui->functionmenuback->setFocus();
}

void systemback::on_aboutback_clicked()
{
    ui->aboutpanel->hide(),
    ui->sbpanel->show(),
    ui->function1->setText("Systemback"),
    ui->functionmenuback->setFocus();
}

void systemback::on_licenseback_clicked()
{
    ui->licensepanel->hide(),
    ui->aboutpanel->show(),
    ui->function1->setText(tr("About")),
    ui->aboutback->setFocus();
}

void systemback::on_licensemenu_clicked()
{
    ui->aboutpanel->hide(),
    ui->licensepanel->show(),
    ui->function1->setText(tr("License")),
    ui->licenseback->setFocus();

    if(! ui->license->isEnabled())
    {
        busy(),
        ui->license->setText(sb::fload("/usr/share/common-licenses/GPL-3")),
        ui->license->setEnabled(true),
        busy(false);
    }
}

void systemback::on_settingsback_clicked()
{
    ui->settingspanel->hide(),
    ui->sbpanel->show(),
    ui->function1->setText("Systemback"),
    ui->functionmenuback->setFocus();
}

void systemback::on_pointpipe1_clicked()
{
    if(ppipe) ppipe = 0;
    bool rnmenbl(false);
    schar num(-1);

    for(QCB ckbx : ui->sbpanel->findChildren<QCB>())
    {
        ++num;

        if(ckbx->isChecked())
        {
            if(++ppipe == 1) cpoint = [num]() -> QStr {
                    switch(num) {
                    case 9:
                        return "S10";
                    case 10 ... 14:
                        return "H0" % QStr::number(num - 9);
                    default:
                        return "S0" % QStr::number(num + 1);
                    }
                }(), pname = sb::pnames[num];

            if(! (rnmenbl || getpoint(num)->text() == sb::pnames[num])) rnmenbl = true;
        }
        else
        {
            QLE ldt(getpoint(num));
            if(ldt->isEnabled() && ! (ldt->text() == sb::pnames[num] || ldt->text().isEmpty())) ldt->setText(sb::pnames[num]);
        }
    }

    if(! ppipe)
    {
        if(ui->restoremenu->isEnabled()) ui->restoremenu->setDisabled(true);

        if(ui->storagedirbutton->isHidden())
        {
            ui->storagedir->resize(ss(210), ui->storagedir->height()),
            ui->storagedirbutton->show();
            for(QWdt wdgt : QWL{ui->pointrename, ui->pointdelete}) wdgt->setDisabled(true);
        }

        if(ui->pointhighlight->isEnabled()) ui->pointhighlight->setDisabled(true);
        if(! ui->repairmenu->isEnabled()) ui->repairmenu->setEnabled(true);

        pname = [this]() -> QStr {
                if(! sislive)
                {
                    for(QWdt wdgt : QWL{ui->newrestorepoint, ui->livemenu})
                        if(! wdgt->isEnabled()) wdgt->setEnabled(true);

                    return tr("Currently running system");
                }
                else if(sb::isdir("/.systemback"))
                {
                    if(! ui->copymenu->isEnabled())
                        for(QWdt wdgt : QWL{ui->copymenu, ui->installmenu}) wdgt->setEnabled(true);

                    return tr("Live image");
                }

                return nullptr;
            }();
    }
    else
    {
        if(! rnmenbl && ui->pointrename->isEnabled()) ui->pointrename->setDisabled(true);

        if(ppipe == 1)
        {
            if(ui->newrestorepoint->isEnabled()) ui->newrestorepoint->setDisabled(true);

            if(ui->storagedirbutton->isVisible()) ui->storagedirbutton->hide(),
                                                  ui->storagedir->resize(ss(236), ui->storagedir->height()),
                                                  ui->pointdelete->setEnabled(true);

            if(ui->pointpipe11->isChecked() || ui->pointpipe12->isChecked() || ui->pointpipe13->isChecked() || ui->pointpipe14->isChecked() || ui->pointpipe15->isChecked())
            {
                if(ui->pointhighlight->isEnabled()) ui->pointhighlight->setDisabled(true);
            }
            else if(! (ui->point15->isEnabled() || ui->pointhighlight->isEnabled()))
                ui->pointhighlight->setEnabled(true);

            if(! ui->copymenu->isEnabled())
                for(QWdt wdgt : QWL{ui->copymenu, ui->installmenu}) wdgt->setEnabled(true);

            if(! (sislive || ui->restoremenu->isEnabled())) ui->restoremenu->setEnabled(true);
            if(ui->livemenu->isEnabled()) ui->livemenu->setDisabled(true);
            if(! ui->repairmenu->isEnabled()) ui->repairmenu->setEnabled(true);
        }
        else
        {
            if(ui->restoremenu->isEnabled()) ui->restoremenu->setDisabled(true);
            for(QWdt wdgt : QWL{ui->copymenu, ui->installmenu, ui->repairmenu}) wdgt->setDisabled(true);
            if(ui->pointhighlight->isEnabled()) ui->pointhighlight->setDisabled(true);
        }
    }
}

void systemback::on_pointpipe2_clicked()
{
    on_pointpipe1_clicked();
}

void systemback::on_pointpipe3_clicked()
{
    on_pointpipe1_clicked();
}

void systemback::on_pointpipe4_clicked()
{
    on_pointpipe1_clicked();
}

void systemback::on_pointpipe5_clicked()
{
    on_pointpipe1_clicked();
}

void systemback::on_pointpipe6_clicked()
{
    on_pointpipe1_clicked();
}

void systemback::on_pointpipe7_clicked()
{
    on_pointpipe1_clicked();
}

void systemback::on_pointpipe8_clicked()
{
    on_pointpipe1_clicked();
}

void systemback::on_pointpipe9_clicked()
{
    on_pointpipe1_clicked();
}

void systemback::on_pointpipe10_clicked()
{
    on_pointpipe1_clicked();
}

void systemback::on_pointpipe11_clicked()
{
    on_pointpipe1_clicked();
}

void systemback::on_pointpipe12_clicked()
{
    on_pointpipe1_clicked();
}

void systemback::on_pointpipe13_clicked()
{
    on_pointpipe1_clicked();
}

void systemback::on_pointpipe14_clicked()
{
    on_pointpipe1_clicked();
}

void systemback::on_pointpipe15_clicked()
{
    on_pointpipe1_clicked();
}

void systemback::on_livedevicesrefresh_clicked()
{
    busy();
    if(! ui->livecover->isVisibleTo(ui->livepanel)) ui->livecover->show();
    if(ui->livedevices->rowCount()) ui->livedevices->clearContents();
    QSL dlst;
    sb::readlvdevs(dlst);
    schar sn(-1);

    for(cQStr &cdts : dlst)
    {
        ui->livedevices->setRowCount(++sn + 1);
        QSL dts(cdts.split('\n'));
        QTblWI *dev(new QTblWI(dts.at(0)));
        ui->livedevices->setItem(sn, 0, dev);
        ullong bsize(dts.at(2).toULongLong());
        QTblWI *rsize(new QTblWI(sb::hunit(bsize)));
        rsize->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter),
        ui->livedevices->setItem(sn, 1, rsize);
        QTblWI *name(new QTblWI(dts.at(1)));
        name->setToolTip(dts.at(1)),
        ui->livedevices->setItem(sn, 2, name);
        QTblWI *format(new QTblWI("-"));
        for(QTblWI *wdi : QList<QTblWI *>{name, format}) wdi->setTextAlignment(Qt::AlignCenter);
        ui->livedevices->setItem(sn, 3, format);
    }

    for(uchar a(0) ; a < 4 ; ++a) ui->livedevices->resizeColumnToContents(a);
    if(ui->livedevices->columnWidth(0) + ui->livedevices->columnWidth(1) + ui->livedevices->columnWidth(2) + ui->livedevices->columnWidth(3) > ui->livedevices->contentsRect().width()) ui->livedevices->setColumnWidth(2, ui->livedevices->contentsRect().width() - ui->livedevices->columnWidth(0) - ui->livedevices->columnWidth(1) - ui->livedevices->columnWidth(3));
    if(ui->livewritestart->isEnabled()) ui->livewritestart->setDisabled(true);
    if(ui->livepanel->isVisible() && ! ui->liveback->hasFocus()) ui->liveback->setFocus();
    ui->livecover->hide(), busy(false);
}

void systemback::ilstupdt(bool inc, cQStr &dir)
{
    if(dir.isEmpty())
    {
        busy();

        if(inc)
        {
            if(! ui->includecover->isVisibleTo(ui->includepanel))
            {
                ui->includecover->show();
                if(ui->includeadditem->isEnabled()) ui->includeadditem->setDisabled(true);
            }

            if(ui->includeitemslist->topLevelItemCount()) ui->includeitemslist->clear();
        }
        else
        {
            if(! ui->excludecover->isVisibleTo(ui->excludepanel))
            {
                ui->excludecover->show();
                if(ui->excludeadditem->isEnabled()) ui->excludeadditem->setDisabled(true);
            }

            if(ui->excludeitemslist->topLevelItemCount()) ui->excludeitemslist->clear();
        }

        ilstupdt(inc, "/root");
        QFile file("/etc/passwd");

        if(sb::fopen(file))
            while(! file.atEnd())
            {
                QStr usr(file.readLine().trimmed());
                if(usr.contains(":/home/") && sb::isdir("/home/" % (usr = sb::left(usr, sb::instr(usr, ":") -1)))) ilstupdt(inc, "/home/" % usr);
            }

        if(inc)
        {
            ui->includeitemslist->sortItems(0, Qt::AscendingOrder);
            if(ui->includepanel->isVisible() && ! ui->includeback->hasFocus()) ui->includeback->setFocus();
            ui->includecover->hide();
        }
        else
        {
            ui->excludeitemslist->sortItems(0, Qt::AscendingOrder);
            if(ui->excludepanel->isVisible() && ! ui->excludeback->hasFocus()) ui->excludeback->setFocus();
            ui->excludecover->hide();
        }

        busy(false);
    }
    else
    {
        QTreeWidget *ilst;
        QLW dlst;
        inc ? (ilst = ui->includeitemslist, dlst = ui->includedlist) : (ilst = ui->excludeitemslist, dlst = ui->excludedlist);

        for(cQStr &item : QDir(dir).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot))
            if((inc || ui->liveexclude->isChecked() ? ! sb::like(item, {"_.*", "_snap_"}) : item.startsWith('.') ? ! sb::like(item, {"_.gvfs_", "_.Xauthority_", "_.ICEauthority_"}) : item == "snap") && dlst->findItems(item, Qt::MatchExactly).isEmpty())
            {
                QList<QTrWI *> flst(ilst->findItems(item, Qt::MatchExactly));

                if(flst.isEmpty())
                {
                    QTrWI *twi(new QTrWI);
                    twi->setText(0, item);

                    if(sb::access(dir % '/' % item) && sb::stype(dir % '/' % item) == sb::Isdir)
                    {
                        twi->setIcon(0, QIcon(QPixmap(":pictures/dir.png").scaled(ss(12), ss(9), Qt::IgnoreAspectRatio, Qt::SmoothTransformation))),
                        ilst->addTopLevelItem(twi);

                        if(dlst->findItems(item % '/', Qt::MatchExactly).isEmpty())
                        {
                            QSL sdlst(QDir(dir % '/' % item).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot));

                            for(cQStr &sitem : sdlst)
                                if(dlst->findItems(item % '/' % sitem, Qt::MatchExactly).isEmpty() && item % '/' % sitem != ".cache/gvfs")
                                {
                                    QTrWI *ctwi(new QTrWI);
                                    ctwi->setText(0, sitem),
                                    twi->addChild(ctwi);
                                }
                        }
                    }
                    else
                        ilst->addTopLevelItem(twi);
                }
                else if(sb::access(dir % '/' % item) && sb::stype(dir % '/' % item) == sb::Isdir)
                {
                    QTrWI *ctwi(flst.at(0));
                    if(ctwi->icon(0).isNull()) ctwi->setIcon(0, QIcon(":pictures/dir.png"));

                    if(dlst->findItems(item % '/', Qt::MatchExactly).isEmpty())
                    {
                        QSL sdlst(QDir(dir % '/' % item).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot)), itmlst;
                        for(ushort a(0) ; a < ctwi->childCount() ; ++a) itmlst.append(ctwi->child(a)->text(0));

                        for(cQStr &sitem : sdlst)
                        {
                            if(dlst->findItems(item % '/' % sitem, Qt::MatchExactly).isEmpty() && item % '/' % sitem != ".cache/gvfs")
                            {
                                for(cQStr &citem : itmlst)
                                    if(citem == sitem) goto next;

                                QTrWI *sctwi(new QTrWI);
                                sctwi->setText(0, sitem),
                                ctwi->addChild(sctwi);
                            }

                        next:;
                        }
                    }
                }
            }
    }
}

void systemback::on_pointexclude_clicked()
{
    busy();
    if(! ui->excludecover->isVisibleTo(ui->excludepanel)) ui->excludecover->show();
    if(ui->excludedlist->count()) ui->excludedlist->clear();

    if(ui->excludeadditem->isEnabled())
        ui->excludeadditem->setDisabled(true);
    else if(ui->excluderemoveitem->isEnabled())
        ui->excluderemoveitem->setDisabled(true);

    {
        QFile file(excfile);

        if(sb::fopen(file))
            while(! file.atEnd())
            {
                QStr cline(sb::left(file.readLine(), -1));

                if(sb::like(cline, {"_.*", "_snap_", "_snap/*"}))
                {
                    if(ui->pointexclude->isChecked()) ui->excludedlist->addItem(cline);
                }
                else if(ui->liveexclude->isChecked() && ! cline.isEmpty())
                    ui->excludedlist->addItem(cline);
            }
    }

    ilstupdt(),
    busy(false);
}

void systemback::on_liveexclude_clicked()
{
    on_pointexclude_clicked();
}

void systemback::on_dialogok_clicked()
{
    if(ui->dialogok->text() == "OK")
    {
        if(dialog == 309)
            dialogopen(ui->fullrestore->isChecked() ? 205 : 204);
        else if(dialog == 305)
        {
            statustart();

            for(cQStr &item : QDir(sb::sdir[1]).entryList(QDir::Dirs | QDir::Hidden | QDir::NoSymLinks | QDir::NoDotAndDotDot))
                if(item.startsWith(".S00_"))
                {
                    pset(12);

                    if(sb::remove(sb::sdir[1] % '/' % item))
                    {
                        emptycache();

                        if(sstart)
                        {
                            sb::crtfile(sb::sdir[1] % "/.sbschedule");
                            sb::ThrdKill = true;
                            close();
                        }
                        else
                        {
                            ui->statuspanel->hide();
                            ui->mainpanel->show();
                            ui->functionmenunext->isEnabled() ? ui->functionmenunext->setFocus() : ui->functionmenuback->setFocus();
                            windowmove(ss(698), ss(465));
                        }
                    }
                    else
                    {
                        if(sstart) sb::crtfile(sb::sdir[1] % "/.sbschedule");

                        if(intrrpt)
                            intrrpt = false;
                        else
                            dialogopen(329);
                    }

                    return;
                }

            if(sstart)
            {
                sb::crtfile(sb::sdir[1] % "/.sbschedule");
                sb::ThrdKill = true;
                close();
            }
            else
                on_dialogcancel_clicked();
        }
        else if(! utimer.isActive() || sstart)
            close();
        else if(sb::like(dialog, {207, 210, 303, 307, 311, 312, 313, 314, 316, 317, 320, 321, 322, 323, 324, 325, 326, 327, 328, 330, 331, 332, 333, 334, 335, 336, 337, 338}))
        {
            ui->dialogpanel->hide();
            ui->mainpanel->show();

            if(ui->copypanel->isVisible())
            {
                ui->copyback->setFocus();
                short nwidth(ss(154) + ui->partitionsettings->width() - ui->partitionsettings->contentsRect().width() + ui->partitionsettings->columnWidth(0) + ui->partitionsettings->columnWidth(1) + ui->partitionsettings->columnWidth(2) + ui->partitionsettings->columnWidth(3) + ui->partitionsettings->columnWidth(4) + ui->partitionsettings->columnWidth(5) + ui->partitionsettings->columnWidth(6));

                if(nwidth > ss(698))
                {
                    ushort sz(ss(850));
                    windowmove(nwidth < sz ? nwidth : sz, ss(465), false);
                }
            }
            else
            {
                if(ui->sbpanel->isVisible())
                    ui->functionmenunext->isEnabled() ? ui->functionmenunext->setFocus() : ui->functionmenuback->setFocus();
                else if(ui->livepanel->isVisible())
                    ui->liveback->setFocus();

                windowmove(ss(698), ss(465));
            }

            setwontop(false);
        }
        else if(sislive && sb::like(dialog, {206, 209}))
            fscrn ? dialogopen(211) : void(close());
        else
            on_dialogcancel_clicked();
    }
    else if(ui->dialogok->text() == tr("Start"))
        switch(dialog) {
        case 100:
        case 103 ... 104:
        case 107:
            return restore();
        case 101 ... 102:
        case 109:
            return repair();
        case 105 ... 106:
            return systemcopy();
        case 108:
            livewrite();
        }
    else if(ui->dialogok->text() == tr("Reboot"))
    {
        sb::exec(sb::execsrch("reboot") ? "reboot" : "systemctl reboot", sb::Bckgrnd);

        if(fscrn)
        {
            fscrn = false;
            qApp->exit(1);
        }
        else
            close();
    }
    else if(ui->dialogok->text() == tr("X restart"))
    {
        DIR *dir(opendir("/proc"));
        dirent *ent;
        QSL dd{"_._", "_.._"};

        while((ent = readdir(dir)))
        {
            QStr iname(ent->d_name);

            if(! sb::like(iname, dd) && ent->d_type == DT_DIR && sb::isnum(iname) && sb::islink("/proc/" % iname % "/exe") && QFile::readLink("/proc/" % iname % "/exe").endsWith("/Xorg"))
            {
                closedir(dir);
                kill(iname.toInt(), SIGTERM);
            }
        }

        close();
    }
}

void systemback::on_pointhighlight_clicked()
{
    busy(),
    sb::rename(sb::sdir[1] % '/' % cpoint % '_' % pname, sb::sdir[1] % "/H05_" % pname),
    pntupgrade(),
    busy(false);
}

void systemback::on_pointrename_clicked()
{
    busy();
    if(dialog == 303) dialog = 0;
    schar num(-1);

    for(QCB ckbx : ui->sbpanel->findChildren<QCB>())
    {
        ++num;

        if(ckbx->isChecked())
        {
            QLE ldt(getpoint(num));

            if(ldt->text() != sb::pnames[num])
            {
                QStr ppath([num]() -> QStr {
                        switch(num) {
                        case 9:
                            return "/S10_";
                        case 10 ... 14:
                            return "/H0" % QStr::number(num - 9) % '_';
                        default:
                            return "/S0" % QStr::number(num + 1) % '_';
                        }
                    }());

                if(sb::rename(sb::sdir[1] % ppath % sb::pnames[num], sb::sdir[1] % ppath % ldt->text()))
                    ckbx->click();
                else if(dialog != 303)
                    dialog = 303;
            }
        }
    }

    pntupgrade(),
    busy(false);
    if(dialog == 303) dialogopen();
}

void systemback::on_autorestoreoptions_clicked(bool chckd)
{
    for(QWdt wdgt : QWL{ui->skipfstabrestore, ui->grubreinstallrestore, ui->grubreinstallrestoredisable}) wdgt->setDisabled(chckd);
}

void systemback::on_autorepairoptions_clicked(bool chckd)
{
    if(chckd)
    {
        if(ui->skipfstabrepair->isEnabled()) ui->skipfstabrepair->setDisabled(true);

        if(ui->grubreinstallrepair->isEnabled())
            for(QWdt wdgt : QWL{ui->grubreinstallrepair, ui->grubreinstallrepairdisable}) wdgt->setDisabled(true);
    }
    else
    {
        if(! ui->skipfstabrepair->isEnabled()) ui->skipfstabrepair->setEnabled(true);

        if(! ui->grubreinstallrepair->isEnabled())
            for(QWdt wdgt : QWL{ui->grubreinstallrepair, ui->grubreinstallrepairdisable}) wdgt->setEnabled(true);
    }
}

void systemback::on_storagedirbutton_clicked()
{
    for(QWdt wdgt : QWL{ui->sbpanel, ui->scalingbutton}) wdgt->hide();
    ui->choosepanel->show(),
    ui->function1->setText(tr("Storage directory")),
    ui->dirchooseok->setFocus();
    { ushort sz[]{ss(642), ss(481), ss(60)};
    windowmove(sz[0], sz[1], false),
    setMinimumSize(sz[0], sz[1]);
    QRect agm(sgeom(true));
    setMaximumSize(agm.width() - sz[2], agm.height() - sz[2]); }
    on_dirrefresh_clicked();
}

void systemback::on_liveworkdirbutton_clicked()
{
    for(QWdt wdgt : QWL{ui->livepanel, ui->scalingbutton}) wdgt->hide();
    ui->choosepanel->show(),
    ui->function1->setText(tr("Working directory")),
    ui->dirchooseok->setFocus();
    { ushort sz[]{ss(642), ss(481), ss(60)};
    windowmove(sz[0], sz[1], false),
    setMinimumSize(sz[0], sz[1]);
    QRect agm(sgeom(true));
    setMaximumSize(agm.width() - sz[2], agm.height() - sz[2]); }
    on_dirrefresh_clicked();
}

void systemback::on_dirrefresh_clicked()
{
    busy();
    if(ui->dirchoose->topLevelItemCount()) ui->dirchoose->clear();
    QStr pwdrs(sb::fload("/etc/passwd"));
    QSL excl{"bin", "boot", "cdrom", "dev", "etc", "lib", "lib32", "lib64", "opt", "proc", "root", "run", "sbin", "selinux", "snap", "srv", "sys", "tmp", "usr", "var"};
    ushort sz(ss(12));

    for(cQStr &item : QDir("/").entryList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot))
    {
        QTrWI *twi(new QTrWI);
        twi->setText(0, item);
        QStr cpath(QDir('/' % item).canonicalPath());

        if(excl.contains(item) || excl.contains(sb::right(cpath, -1)) || pwdrs.contains(':' % cpath % ':') || ! sb::islnxfs('/' % item))
            twi->setTextColor(0, Qt::red),
            twi->setIcon(0, QIcon(QPixmap(":pictures/dirx.png").scaled(sz, sz, Qt::IgnoreAspectRatio, Qt::SmoothTransformation))),
            ui->dirchoose->addTopLevelItem(twi);
        else
        {
            if(ui->function1->text() == tr("Storage directory") && sb::isfile('/' % item % "/Systemback/.sbschedule")) twi->setTextColor(0, Qt::green),
                                                                                                                       twi->setIcon(0, QIcon(QPixmap(":pictures/isdir.png").scaled(sz, sz, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));

            ui->dirchoose->addTopLevelItem(twi);

            if(item == "home") ui->dirchoose->setCurrentItem(twi),
                               twi->setSelected(true);

            for(cQStr &sitem : QDir('/' % item).entryList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot))
            {
                QTrWI *ctwi(new QTrWI);
                ctwi->setText(0, sitem);

                if(excl.contains(sb::right(cpath = QDir('/' % item % '/' % sitem).canonicalPath(), -1)) || pwdrs.contains(':' % cpath % ':') || (item == "home" && pwdrs.contains(":/home/" % sitem % ":"))) ctwi->setTextColor(0, Qt::red),
                                                                                                                                                                                                             ctwi->setIcon(0, QIcon(QPixmap(":pictures/dirx.png").scaled(sz, sz, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));

                twi->addChild(ctwi);
            }
        }
    }

    if(! (ui->dirchoose->currentItem() || ui->dirpath->text() == "/"))
    {
        ui->dirpath->setText("/");

        if(ui->dirpath->styleSheet().isEmpty()) ui->dirpath->setStyleSheet("color: red"),
                                                ui->dirchooseok->setDisabled(true);

        ui->dirchoosecancel->setFocus();
    }

    busy(false);
}

void systemback::on_dirchoose_currentItemChanged(QTrWI *crrnt)
{
    if(crrnt)
    {
        cQTrWI *twi(crrnt);
        QStr path('/' % crrnt->text(0));
        while(twi->parent()) path.prepend('/' % (twi = twi->parent())->text(0));

        if(sb::isdir(path))
        {
            ui->dirpath->setText(path);

            if(crrnt->textColor(0) == Qt::red)
            {
                if(ui->dirpath->styleSheet().isEmpty()) ui->dirpath->setStyleSheet("color: red"),
                                                        ui->dirchooseok->setDisabled(true);
            }
            else if(! ui->dirpath->styleSheet().isEmpty())
                ui->dirpath->setStyleSheet(nullptr),
                ui->dirchooseok->setEnabled(true);
        }
        else
        {
            crrnt->setDisabled(true);

            if(crrnt->isSelected())
            {
                crrnt->setSelected(false);
                ui->dirchoosecancel->setFocus();

                if(ui->dirpath->text() != "/")
                {
                    ui->dirpath->setText("/");

                    if(ui->dirpath->styleSheet().isEmpty()) ui->dirpath->setStyleSheet("color: red"),
                                                            ui->dirchooseok->setDisabled(true);
                }
            }
        }
    }
}

void systemback::on_dirchoose_itemExpanded(QTrWI *item)
{
    if(item->backgroundColor(0) != Qt::transparent)
    {
        item->setBackgroundColor(0, Qt::transparent),
        busy();
        cQTrWI *twi(item);
        QStr path('/' % twi->text(0));
        while(twi->parent()) path.prepend('/' % (twi = twi->parent())->text(0));

        if(sb::isdir(path))
        {
            QStr pwdrs(sb::fload("/etc/passwd"));
            ushort sz(ss(12));

            for(ushort a(0) ; a < item->childCount() ; ++a)
            {
                QTrWI *ctwi(item->child(a));

                if(ctwi->textColor(0) != Qt::red)
                {
                    QStr iname(ctwi->text(0));

                    if(! sb::isdir(path % '/' % iname))
                        ctwi->setDisabled(true);
                    else if(iname == "Systemback" || pwdrs.contains(':' % QDir(path % '/' % iname).canonicalPath() % ':') || ! sb::islnxfs(path % '/' % iname))
                        ctwi->setTextColor(0, Qt::red),
                        ctwi->setIcon(0, QIcon(QPixmap(":pictures/dirx.png").scaled(sz, sz, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
                    else
                    {
                        if(ui->function1->text() == tr("Storage directory") && sb::isfile(path % '/' % iname % '/' % "/Systemback/.sbschedule")) ctwi->setTextColor(0, Qt::green),
                                                                                                                                                 ctwi->setIcon(0, QIcon(QPixmap(":pictures/isdir.png").scaled(sz, sz, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));

                        for(cQStr &cdir : QDir(path % '/' % iname).entryList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot))
                        {
                            QTrWI *sctwi(new QTrWI);
                            sctwi->setText(0, cdir),
                            ctwi->addChild(sctwi);
                        }
                    }
                }
            }
        }
        else
        {
            item->setDisabled(true);

            if(item->isSelected())
            {
                item->setSelected(false),
                ui->dirchoosecancel->setFocus();

                if(ui->dirpath->text() != "/")
                {
                    ui->dirpath->setText("/");

                    if(ui->dirpath->styleSheet().isEmpty()) ui->dirpath->setStyleSheet("color: red"),
                                                            ui->dirchooseok->setDisabled(true);
                }
            }
        }

        busy(false);
    }
}

void systemback::on_dirchoosecancel_clicked()
{
    ui->choosepanel->hide(),
    ui->scalingbutton->show();

    if(ui->function1->text() == tr("Storage directory"))
        ui->sbpanel->show(),
        ui->function1->setText("Systemback"),
        ui->functionmenunext->isEnabled() ? ui->functionmenunext->setFocus() : ui->functionmenuback->setFocus();
    else
        ui->livepanel->show(),
        ui->function1->setText(tr("Live system create")),
        ui->liveback->setFocus();

    windowmove(ss(698), ss(465)),
    ui->dirchoose->clear();
}

void systemback::on_dirchooseok_clicked()
{
    if(sb::isdir(ui->dirpath->text()))
    {
        if(ui->function1->text() == tr("Storage directory"))
        {
            if(sb::sdir[0] != ui->dirpath->text())
            {
                QSL dlst(QDir(sb::sdir[1]).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot));

                if(! dlst.count())
                    rmdir(bstr(sb::sdir[1]));
                else if(dlst.count() == 1 && sb::isfile(sb::sdir[1] % "/.sbschedule"))
                    sb::remove(sb::sdir[1]);

                sb::sdir[0] = ui->dirpath->text(), sb::sdir[1] = sb::sdir[0] % "/Systemback",
                ui->storagedir->setText(sb::sdir[0]),
                ui->storagedir->setToolTip(sb::sdir[0]),
                ui->storagedir->setCursorPosition(0),
                pntupgrade();
            }

            if(! (sb::isdir(sb::sdir[1]) || sb::crtdir(sb::sdir[1]))) sb::rename(sb::sdir[1], sb::sdir[1] % '_' % sb::rndstr()),
                                                                      sb::crtdir(sb::sdir[1]);

            sb::ismpnt = ! sb::issmfs(sb::sdir[0], sb::sdir[0].count('/') == 1 ? "/" : sb::left(sb::sdir[0], sb::rinstr(sb::sdir[0], "/") - 1));
            if(! sb::isfile(sb::sdir[1] % "/.sbschedule")) sb::crtfile(sb::sdir[1] % "/.sbschedule");
            ui->choosepanel->hide(),
            ui->sbpanel->show(),
            ui->function1->setText("Systemback"),
            ui->functionmenunext->isEnabled() ? ui->functionmenunext->setFocus() : ui->functionmenuback->setFocus();
        }
        else
        {
            ui->choosepanel->hide(),
            ui->livepanel->show(),
            ui->function1->setText(tr("Live system create")),
            ui->liveback->setFocus();

            if(sb::sdir[2] != ui->dirpath->text()) sb::sdir[2] = ui->dirpath->text(),
                                                   ui->liveworkdir->setText(sb::sdir[2]),
                                                   ui->liveworkdir->setToolTip(sb::sdir[2]),
                                                   ui->liveworkdir->setCursorPosition(0),
                                                   on_livemenu_clicked();
        }

        ui->scalingbutton->show(),
        windowmove(ss(698), ss(465)),
        ui->dirchoose->clear();
    }
    else
        delete ui->dirchoose->currentItem(),
        ui->dirchoose->setCurrentItem(nullptr),
        ui->dirchoosecancel->setFocus(),
        ui->dirpath->setText("/"),
        ui->dirpath->setStyleSheet("color: red"),
        ui->dirchooseok->setDisabled(true);
}

void systemback::on_fullrestore_clicked()
{
    if(ui->keepfiles->isEnabled())
    {
        for(QWdt wdgt : QWL{ui->keepfiles, ui->includeusers}) wdgt->setDisabled(true);
        ui->autorestoreoptions->setEnabled(true);

        if(! ui->autorestoreoptions->isChecked())
            for(QWdt wdgt : QWL{ui->skipfstabrestore, ui->grubreinstallrestore, ui->grubreinstallrestoredisable}) wdgt->setEnabled(true);
    }

    if(! ui->restorenext->isEnabled()) ui->restorenext->setEnabled(true);
}

void systemback::on_systemrestore_clicked()
{
    on_fullrestore_clicked();
}

void systemback::on_configurationfilesrestore_clicked()
{
    if(! ui->keepfiles->isEnabled())
    {
        for(QWdt wdgt : QWL{ui->keepfiles, ui->includeusers}) wdgt->setEnabled(true);
        ui->autorestoreoptions->setDisabled(true);

        if(! ui->autorestoreoptions->isChecked())
            for(QWdt wdgt : QWL{ui->skipfstabrestore, ui->grubreinstallrestore, ui->grubreinstallrestoredisable}) wdgt->setDisabled(true);
    }

    on_includeusers_currentIndexChanged(ui->includeusers->currentText());
}

void systemback::on_includeusers_currentIndexChanged(cQStr &arg1)
{
    if(! arg1.isEmpty())
    {
        ui->includeusers->resize(fontMetrics().width(arg1) + ss(30), ui->includeusers->height());

        if(ui->includeusers->currentIndex() < 2 || (ui->includeusers->currentIndex() > 1 && sb::isdir(sb::sdir[1] % '/' % cpoint % '_' % pname % "/home/" % arg1)))
        {
            if(! ui->restorenext->isEnabled()) ui->restorenext->setEnabled(true);
        }
        else if(ui->restorenext->isEnabled())
            ui->restorenext->setDisabled(true);
    }
}

void systemback::on_restorenext_clicked()
{
    dialogopen(ui->fullrestore->isChecked() ? 107 : ui->systemrestore->isChecked() ? 100 : ui->keepfiles->isChecked() ? 104 : 103);
}

void systemback::on_livelist_currentItemChanged(QLWI *crrnt)
{
    if(crrnt)
    {
        if(sb::isfile(sb::sdir[2] % '/' % sb::left(crrnt->text(), sb::instr(crrnt->text(), " ") - 1) % ".sblive"))
        {
            if(! ui->livedelete->isEnabled()) ui->livedelete->setEnabled(true);
            ullong isize(sb::fsize(sb::sdir[2] % '/' % sb::left(crrnt->text(), sb::instr(crrnt->text(), " ") - 1) % ".sblive"));

            if(isize && isize < 4294967295 && isize * 2 + 104857600 < sb::dfree(sb::sdir[2]) && ! sb::exist(sb::sdir[2] % '/' % sb::left(crrnt->text(), sb::instr(crrnt->text(), " ") - 1) % ".iso"))
            {
                if(! ui->liveconvert->isEnabled()) ui->liveconvert->setEnabled(true);
            }
            else if(ui->liveconvert->isEnabled())
                ui->liveconvert->setDisabled(true);

            if(ui->livedevices->currentItem() && isize)
            {
                if(! ui->livewritestart->isEnabled()) ui->livewritestart->setEnabled(true);
            }
            else if(ui->livewritestart->isEnabled())
                ui->livewritestart->setDisabled(true);
        }
        else
        {
            delete crrnt, ui->livelist->setCurrentItem(nullptr);

            for(QWdt wdgt : QWL{ui->livedelete, ui->liveconvert, ui->livewritestart})
                if(wdgt->isEnabled()) wdgt->setDisabled(true);

            ui->liveback->setFocus();
        }
    }
}

void systemback::on_livedelete_clicked()
{
    busy(), ui->livecover->show();
    QStr path(sb::sdir[2] % '/' % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1));
    sb::remove(path % ".sblive");
    if(sb::exist(path % ".iso")) sb::remove(path % ".iso");
    on_livemenu_clicked(),
    ui->liveback->setFocus(),
    ui->livecover->hide(), busy(false);
}

void systemback::on_livedevices_currentItemChanged(QTblWI *crrnt, QTblWI *prvs)
{
    if(crrnt && ! (prvs && crrnt->row() == prvs->row()))
    {
        ui->livedevices->item(crrnt->row(), 3)->setText("x");
        if(prvs) ui->livedevices->item(prvs->row(), 3)->setText("-");
        if(ui->livelist->currentItem() && ! ui->livewritestart->isEnabled()) ui->livewritestart->setEnabled(true);
    }
}

void systemback::rmntcheck()
{
    auto grnst([this](bool enable = true) {
            if(enable)
            {
                if(ui->grubreinstallrepairdisable->isVisibleTo(ui->repairpanel)) ui->grubreinstallrepairdisable->hide(),
                                                                                 ui->grubreinstallrepair->show();
            }
            else if(ui->grubreinstallrepair->isVisibleTo(ui->repairpanel))
                ui->grubreinstallrepair->hide(),
                ui->grubreinstallrepairdisable->show();
        });

    if(sb::issmfs("/", "/mnt"))
    {
        grnst(false);
        if(ui->repairnext->isEnabled()) ui->repairnext->setDisabled(true);
    }
    else if(! ui->grubrepair->isChecked())
    {
        grnst(! (grub.isEFI && sb::issmfs("/mnt/boot", "/mnt/boot/efi")) && [this] {
                switch(ppipe) {
                case 0:
                    if(sb::execsrch("update-grub2") && sb::isfile("/var/lib/dpkg/info/grub-" % grub.name % ".list")) return true;
                    break;
                case 1:
                    if(sb::execsrch("update-grub2", sb::sdir[1] % '/' % cpoint % '_' % pname) && sb::isfile(sb::sdir[1] % '/' % cpoint % '_' % pname % "/var/lib/dpkg/info/grub-" % grub.name % ".list")) return true;
                }

                return false;
            }());

        if(! ui->repairnext->isEnabled()) ui->repairnext->setEnabled(true);
    }
    else if(! (grub.isEFI && sb::issmfs("/mnt/boot", "/mnt/boot/efi")) && sb::execsrch("update-grub2", "/mnt") && sb::isfile("/mnt/var/lib/dpkg/info/grub-" % grub.name % ".list"))
    {
        grnst();
        if(! ui->repairnext->isEnabled()) ui->repairnext->setEnabled(true);
    }
    else
    {
        grnst(false);
        if(ui->repairnext->isEnabled()) ui->repairnext->setDisabled(true);
    }
}

void systemback::on_systemrepair_clicked()
{
    if(ui->grubrepair->isChecked())
    {
        if(ui->skipfstabrepair->isEnabled()) ui->skipfstabrepair->setDisabled(true);

        if(ui->autorepairoptions->isEnabled())
        {
            ui->autorepairoptions->setDisabled(true);
            for(QWdt wdgt : QWL{ui->grubreinstallrepair, ui->grubreinstallrepairdisable}) wdgt->setEnabled(true);
        }
    }
    else
    {
        if(! ui->autorepairoptions->isEnabled()) ui->autorepairoptions->setEnabled(true),
                                                 on_autorepairoptions_clicked(ui->autorepairoptions->isChecked());

        if(ui->grubreinstallrepair->findText(tr("Disabled")) == -1) ui->grubreinstallrepair->addItem(tr("Disabled"));
    }

    rmntcheck();
}

void systemback::on_fullrepair_clicked()
{
    on_systemrepair_clicked();
}

void systemback::on_grubrepair_clicked()
{
    on_systemrepair_clicked();
    if(ui->grubreinstallrepair->currentText() == tr("Disabled")) ui->grubreinstallrepair->setCurrentIndex(0);
    ui->grubreinstallrepair->removeItem(ui->grubreinstallrepair->findText(tr("Disabled")));
}

void systemback::on_repairnext_clicked()
{
    dialogopen(ui->systemrepair->isChecked() ? 101 : ui->fullrepair->isChecked() ? 102 : 109);
}

void systemback::on_skipfstabrestore_clicked(bool chckd)
{
    if(chckd && ! sb::isfile("/etc/fstab")) ui->skipfstabrestore->setChecked(false);
}

void systemback::on_skipfstabrepair_clicked(bool chckd)
{
    if(chckd && ! sb::isfile("/mnt/etc/fstab")) ui->skipfstabrepair->setChecked(false);
}

void systemback::on_installnext_clicked()
{
    if(! grub.isEFI || ui->grubinstallcopy->isVisibleTo(ui->copypanel))
    {
        if(ppipe ? sb::execsrch("update-grub2", sb::sdir[1] % '/' % cpoint % '_' % pname) && sb::isfile(sb::sdir[1] % '/' % cpoint % '_' % pname % "/var/lib/dpkg/info/grub-" % grub.name % ".list") : sb::execsrch("update-grub2") && sb::isfile("/var/lib/dpkg/info/grub-" % grub.name % ".list"))
        {
            if(ui->grubinstallcopydisable->isVisibleTo(ui->copypanel)) ui->grubinstallcopydisable->hide(),
                                                                       ui->grubinstallcopy->show();
        }
        else if(ui->grubinstallcopy->isVisibleTo(ui->copypanel))
            ui->grubinstallcopy->hide(),
            ui->grubinstallcopydisable->show();
    }

    if(ppipe)
    {
        if(ui->usersettingscopy->isTristate())
        {
            ui->usersettingscopy->setTristate(false);
            if(! ui->usersettingscopy->isChecked()) ui->usersettingscopy->setChecked(true);
            if(ui->usersettingscopy->text() == tr("Transfer user configuration and data files")) ui->usersettingscopy->setText(tr("Transfer user configuration files"));
            ui->usersettingscopy->resize(fontMetrics().width(tr("Transfer user configuration files")) + ss(28), ui->usersettingscopy->height());
        }
    }
    else if(! ui->usersettingscopy->isTristate())
        ui->usersettingscopy->setTristate(true),
        ui->usersettingscopy->setCheckState(Qt::PartiallyChecked),
        ui->usersettingscopy->resize(fontMetrics().width(tr("Transfer user configuration and data files")) + ss(28), ui->usersettingscopy->height());

    if(ui->userdatafilescopy->isVisibleTo(ui->copypanel)) ui->userdatafilescopy->hide(),
                                                          ui->usersettingscopy->show();

    ui->installpanel->hide(),
    ui->copypanel->show(),
    ui->copyback->setFocus();

    {
        short nwidth(ss(154) + ui->partitionsettings->width() - ui->partitionsettings->contentsRect().width() + ui->partitionsettings->columnWidth(0) + ui->partitionsettings->columnWidth(1) + ui->partitionsettings->columnWidth(2) + ui->partitionsettings->columnWidth(3) + ui->partitionsettings->columnWidth(4) + ui->partitionsettings->columnWidth(5) + ui->partitionsettings->columnWidth(6));
        ushort sz[]{ss(698), ss(465), ss(60)};

        if(nwidth > sz[0])
        {
            ushort sz1(ss(850));
            windowmove(nwidth < sz1 ? nwidth : sz1, sz[1], false);
        }

        wndw->setMinimumSize(sz[0], sz[1]);
        QRect agm(sgeom(true));
        wndw->setMaximumSize(agm.width() - sz[2], agm.height() - sz[2]);
    }

    if(ui->mountpoint->currentText().startsWith("/home/")) ui->mountpoint->setCurrentText(nullptr);

    if(ui->partitionsettings->currentItem())
    {
        if(sb::mcheck("/.sbsystemcopy/") || sb::mcheck("/.sblivesystemwrite/"))
            ui->copyback->setDisabled(true),
            on_partitionrefresh_clicked(),
            ui->copyback->setEnabled(true);
        else
        {
            if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->text() == "/home" && ui->mountpoint->isEnabled())
            {
                if(ui->mountpoint->currentIndex()) ui->mountpoint->setCurrentIndex(0);
                ui->mountpoint->setDisabled(true);
            }

            if(nohmcpy[1])
            {
                nohmcpy[1] = false;

                for(ushort a(0) ; a < ui->partitionsettings->rowCount() ; ++a)
                    if(ui->partitionsettings->item(a, 3)->text() == "/home" && ! ui->partitionsettings->item(a, 4)->text().isEmpty())
                    {
                        ui->partitionsettings->item(a, 4)->setText(nullptr);
                        return ui->mountpoint->addItem("/home");
                    }
            }
        }
    }
}

void systemback::on_partitionsettings_currentItemChanged(QTblWI *crrnt, QTblWI *prvs)
{
    if(crrnt && ! (prvs && crrnt->row() == prvs->row()))
    {
        if(ui->partitionsettingspanel2->isVisible())
            for(ushort a(prvs->row() + 1) ; a < ui->partitionsettings->rowCount() && ui->partitionsettings->item(a, 0)->background() != QBrush() ; ++a) ui->partitionsettings->item(a, 0)->setBackground(QBrush()),
                                                                                                                                                        ui->partitionsettings->item(a, 0)->setForeground(QBrush());

        uchar type(ui->partitionsettings->item(crrnt->row(), 8)->text().toUShort()), pcnt(0);

        switch(type) {
        case sb::MSDOS:
        case sb::GPT:
        case sb::Clear:
        case sb::Extended:
        {
            if(ui->partitionsettingspanel2->isHidden()) ui->partitionsettingspanel1->isVisible() ? ui->partitionsettingspanel1->hide() : ui->partitionsettingspanel3->hide(),
                                                        ui->partitionsettingspanel2->show();

            bool mntd(false), mntcheck(false);
            QStr dev(ui->partitionsettings->item(crrnt->row(), 0)->text());

            for(ushort a(crrnt->row() + 1) ; a < ui->partitionsettings->rowCount() && ((type == sb::Extended && ui->partitionsettings->item(a, 0)->text().startsWith(sb::left(dev, dev.contains("mmc") ? 12 : 8)) && sb::like(ui->partitionsettings->item(a, 8)->text().toInt(), {sb::Logical, sb::Emptyspace})) || (type != sb::Extended && ui->partitionsettings->item(a, 0)->text().startsWith(dev))) ; ++a)
            {
                ui->partitionsettings->item(a, 0)->setBackground(QPalette().highlight()),
                ui->partitionsettings->item(a, 0)->setForeground(QPalette().highlightedText());

                if(! mntcheck)
                {
                    QStr mpt(ui->partitionsettings->item(a, 3)->text());

                    if(! mpt.isEmpty())
                    {
                        if(! mntd) mntd = true;

                        if(((ui->point1->isEnabled() || ui->pointpipe11->isEnabled()) && sb::sdir[0].startsWith(mpt)) || sb::like(mpt, {'_' % tr("Multiple mount points") % '_', "_/cdrom_", "_/live/image_", "_/lib/live/mount/medium_"}))
                            mntcheck = true;
                        else if(sb::isfile("/etc/fstab"))
                        {
                            QFile file("/etc/fstab");

                            if(sb::fopen(file))
                                while(! file.atEnd())
                                {
                                    QBA cline(file.readLine().trimmed());

                                    if(! cline.startsWith('#') && sb::like(cline.replace('\t', ' ').replace("\\040", " "), {"* " % mpt % " *", "* " % mpt % "/ *"}))
                                    {
                                        mntcheck = true;
                                        break;
                                    }
                                }
                        }
                    }
                }
            }

            if(mntd)
            {
                if(ui->partitiondelete->isEnabled()) ui->partitiondelete->setDisabled(true);

                if(mntcheck)
                {
                    if(ui->unmount->isEnabled()) ui->unmount->setDisabled(true);
                }
                else if(! ui->unmount->isEnabled())
                    ui->unmount->setEnabled(true);
            }
            else
            {
                if(! ui->partitiondelete->isEnabled()) ui->partitiondelete->setEnabled(true);
                if(ui->unmount->isEnabled()) ui->unmount->setDisabled(true);
            }

            break;
        }
        case sb::Freespace:
        {
            QStr dev(sb::left(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 0)->text(), ui->partitionsettings->item(ui->partitionsettings->currentRow(), 0)->text().contains("mmc") ? 12 : 8));

            for(ushort a(0) ; a < ui->partitionsettings->rowCount() && pcnt < 4 ; ++a)
                if(ui->partitionsettings->item(a, 0)->text().startsWith(dev))
                    switch(ui->partitionsettings->item(a, 8)->text().toUShort()) {
                    case sb::GPT:
                        pcnt = 5;
                        break;
                    case sb::Primary:
                    case sb::Extended:
                        ++pcnt;
                    }
        }
        case sb::Emptyspace:
            if(ui->partitionsettingspanel3->isHidden()) ui->partitionsettingspanel1->isVisible() ? ui->partitionsettingspanel1->hide() : ui->partitionsettingspanel2->hide(),
                                                        ui->partitionsettingspanel3->show();

            if(pcnt == 4)
            {
                if(ui->newpartition->isEnabled()) ui->newpartition->setDisabled(true);
            }
            else if(! ui->newpartition->isEnabled())
                ui->newpartition->setEnabled(true);

            ui->partitionsize->setMaximum((ui->partitionsettings->item(crrnt->row(), 10)->text().toULongLong() * 10 / 1048576 + 5) / 10),
            ui->partitionsize->setValue(ui->partitionsize->maximum());
            break;
        default:
            if(ui->partitionsettingspanel1->isHidden()) ui->partitionsettingspanel2->isVisible() ? ui->partitionsettingspanel2->hide() : ui->partitionsettingspanel3->hide(),
                                                        ui->partitionsettingspanel1->show();

            if(ui->partitionsettings->item(crrnt->row(), 3)->text().isEmpty())
            {
                if(! ui->mountpoint->isEnabled()) ui->mountpoint->setEnabled(true);

                if(ui->unmountdelete->text() == tr("Unmount")) ui->unmountdelete->setText(tr("! Delete !")),
                                                               ui->unmountdelete->setStyleSheet("QPushButton:enabled{color: red}");

                if(! ui->partitionsettings->item(crrnt->row(), 4)->text().isEmpty() && ui->partitionsettings->item(crrnt->row(), 5)->text() == "btrfs")
                {
                    if(ui->format->isEnabled())
                    {
                        ui->format->setDisabled(true);
                        if(ui->filesystem->isEnabled()) ui->filesystem->setDisabled(true);
                    }

                    if(ui->filesystem->currentText() != "btrfs") ui->filesystem->setCurrentIndex(ui->filesystem->findText("btrfs"));
                    if(ui->unmountdelete->isEnabled()) ui->unmountdelete->setDisabled(true);

                    if(ui->partitionsettings->item(crrnt->row(), 6)->text() == "-")
                    {
                        if(ui->format->isChecked()) ui->format->setChecked(false);
                    }
                    else if(! ui->format->isChecked())
                        ui->format->setChecked(true);
                }
                else
                {
                    if(! ui->filesystem->isEnabled())
                    {
                        ui->filesystem->setEnabled(true);
                        if(! ui->format->isEnabled()) ui->format->setEnabled(true);
                    }

                    if(! ui->format->isChecked()) ui->format->setChecked(true);
                    if(! ui->unmountdelete->isEnabled()) ui->unmountdelete->setEnabled(true);
                    schar indx(ui->filesystem->findText(ui->partitionsettings->item(crrnt->row(), 7)->text()));
                    if(indx != -1 && ui->filesystem->currentIndex() != indx) ui->filesystem->setCurrentIndex(indx);
                }

                if(ui->mountpoint->currentIndex())
                    ui->mountpoint->setCurrentIndex(0);
                else if(! ui->mountpoint->currentText().isEmpty())
                    ui->mountpoint->setCurrentText(nullptr);
            }
            else
            {
                if(ui->format->isEnabled())
                {
                    ui->format->setDisabled(true);
                    if(ui->filesystem->isEnabled()) ui->filesystem->setDisabled(true);
                }

                if(ui->unmountdelete->text() == tr("! Delete !")) ui->unmountdelete->setText(tr("Unmount")),
                                                                  ui->unmountdelete->setStyleSheet(nullptr);

                QStr mpt(ui->partitionsettings->item(crrnt->row(), 3)->text());

                if(grub.isEFI && mpt == "/boot/efi")
                {
                    if(! ui->mountpoint->isEnabled()) ui->mountpoint->setEnabled(true);
                    if(ui->unmountdelete->isEnabled()) ui->unmountdelete->setDisabled(true);

                    if(ui->mountpoint->currentText() != "/boot/efi")
                    {
                        if(ui->mountpoint->currentIndex())
                            ui->mountpoint->setCurrentIndex(0);
                        else if(! ui->mountpoint->currentText().isEmpty())
                            ui->mountpoint->setCurrentText(nullptr);
                    }
                }
                else if(mpt == "SWAP")
                {
                    for(QWdt wdgt : QWL{ui->mountpoint, ui->unmountdelete})
                        if(! wdgt->isEnabled()) wdgt->setEnabled(true);

                    if(ui->mountpoint->currentText() != "SWAP")
                    {
                        if(ui->mountpoint->currentIndex())
                            ui->mountpoint->setCurrentIndex(0);
                        else if(! ui->mountpoint->currentText().isEmpty())
                            ui->mountpoint->setCurrentText(nullptr);
                    }
                    else if(ui->partitionsettings->item(crrnt->row(), 4)->text() == "SWAP")
                    {
                        if(ui->changepartition->isEnabled()) ui->changepartition->setDisabled(true);
                    }
                    else if(! ui->changepartition->isEnabled())
                        ui->changepartition->setEnabled(true);
                }
                else if(nohmcpy[0] && ui->userdatafilescopy->isVisible() && mpt == "/home")
                {
                    if(! ui->mountpoint->isEnabled()) ui->mountpoint->setEnabled(true);
                    if(ui->unmountdelete->isEnabled()) ui->unmountdelete->setDisabled(true);

                    if(ui->mountpoint->currentText() != "/home")
                    {
                        if(ui->mountpoint->currentIndex())
                            ui->mountpoint->setCurrentIndex(0);
                        else if(! ui->mountpoint->currentText().isEmpty())
                            ui->mountpoint->setCurrentText(nullptr);
                    }
                }
                else
                {
                    if(ui->mountpoint->isEnabled()) ui->mountpoint->setDisabled(true);

                    if(ui->mountpoint->currentIndex())
                    {
                        ui->mountpoint->setCurrentIndex(0);
                        if(ui->changepartition->isEnabled()) ui->changepartition->setDisabled(true);
                    }
                    else if(! ui->mountpoint->currentText().isEmpty())
                        ui->mountpoint->setCurrentText(nullptr);

                    if(sb::sdir[0].startsWith(mpt) || sb::like(mpt, {'_' % tr("Multiple mount points") % '_', "_/cdrom_", "_/live/image_", "_/lib/live/mount/medium_"}) || [&mpt] {
                            if(sb::isfile("/etc/fstab"))
                            {
                                QFile file("/etc/fstab");

                                if(sb::fopen(file))
                                    while(! file.atEnd())
                                    {
                                        QBA cline(file.readLine().trimmed());
                                        if(! cline.startsWith('#') && sb::like(cline.replace('\t', ' ').replace("\\040", " "), {"* " % mpt % " *", "* " % mpt % "/ *"})) return true;
                                    }
                            }

                            return false;
                        }())
                    {
                        if(ui->unmountdelete->isEnabled()) ui->unmountdelete->setDisabled(true);
                    }
                    else if(! ui->unmountdelete->isEnabled())
                        ui->unmountdelete->setEnabled(true);
                }

                if(! ui->format->isChecked()) ui->format->setChecked(true);
            }
        }
    }
}

void systemback::on_changepartition_clicked()
{
    busy();
    QStr ompt(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 4)->text()), mpt(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->text());

    if(! ompt.isEmpty())
    {
        if(mpt.isEmpty() && ui->partitionsettings->item(ui->partitionsettings->currentRow(), 5)->text() == "btrfs")
        {
            ui->partitionsettings->setRowCount(ui->partitionsettings->rowCount() + 1);

            for(short a(ui->partitionsettings->rowCount() - 1) ; a > ui->partitionsettings->currentRow() - 1 ; --a)
                for(uchar b(0) ; b < 11 ; ++b)
                {
                    QTblWI *item(ui->partitionsettings->item(a, b));
                    ui->partitionsettings->setItem(a + 1, b, item ? item->clone() : nullptr);
                }
        }
        else if(ompt == "/")
            ui->copynext->setDisabled(true),
            ui->mountpoint->addItem("/");
        else if(grub.isEFI && ompt == "/boot/efi")
        {
            if(ui->grubinstallcopy->isVisible()) ui->grubinstallcopy->hide(),
                                                 ui->grubinstallcopydisable->show();

            ui->mountpoint->addItem("/boot/efi"),
            ui->efiwarning->show();
        }
        else if(sb::like(ompt, {"_/home_", "_/boot_", "_/tmp_", "_/usr_", "_/usr/local_", "_/var_", "_/srv_", "_/opt_"}))
            ui->mountpoint->addItem(ompt);
    }

    if(mpt.isEmpty())
    {
        ui->partitionsettings->item(ui->partitionsettings->currentRow(), 4)->setText(ui->mountpoint->currentText());

        if(ui->mountpoint->currentText() == "/boot/efi")
        {
            if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 5)->text() != "vfat") ui->partitionsettings->item(ui->partitionsettings->currentRow(), 5)->setText("vfat");

            if(grub.isEFI)
            {
                if(ppipe ? sb::execsrch("update-grub2", sb::sdir[1] % '/' % cpoint % '_' % pname) && sb::isfile(sb::sdir[1] % '/' % cpoint % '_' % pname % "/var/lib/dpkg/info/grub-" % grub.name % ".list") : sb::execsrch("update-grub2") && sb::isfile("/var/lib/dpkg/info/grub-" % grub.name % ".list"))
                {
                    if(ui->grubinstallcopydisable->isVisible()) ui->grubinstallcopydisable->hide(),
                                                                ui->grubinstallcopy->show();
                }
                else if(ui->grubinstallcopy->isVisible())
                    ui->grubinstallcopy->hide(),
                    ui->grubinstallcopydisable->show();

                ui->efiwarning->hide();
            }
        }
        else if(ui->mountpoint->currentText() != "SWAP")
        {
            if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 5)->text() != ui->filesystem->currentText()) ui->partitionsettings->item(ui->partitionsettings->currentRow(), 5)->setText(ui->filesystem->currentText());
            if(ui->mountpoint->currentText() == "/") ui->copynext->setEnabled(true);
        }
        else if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 5)->text() != "swap")
            ui->partitionsettings->item(ui->partitionsettings->currentRow(), 5)->setText("swap");

        if(ui->format->isChecked())
        {
            if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 6)->text() == "-") ui->partitionsettings->item(ui->partitionsettings->currentRow(), 6)->setText("x");
        }
        else if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 6)->text() == "x")
            ui->partitionsettings->item(ui->partitionsettings->currentRow(), 6)->setText("-");

        if(ui->filesystem->isEnabled() && ui->filesystem->currentText() == "btrfs") ui->filesystem->setDisabled(true),
                                                                                    ui->format->setDisabled(true);
    }
    else if(grub.isEFI && mpt == "/boot/efi")
    {
        ui->partitionsettings->item(ui->partitionsettings->currentRow(), 4)->setText("/boot/efi"),
        ui->efiwarning->hide();

        if(ppipe ? sb::execsrch("update-grub2", sb::sdir[1] % '/' % cpoint % '_' % pname) && sb::isfile(sb::sdir[1] % '/' % cpoint % '_' % pname % "/var/lib/dpkg/info/grub-" % grub.name % ".list") : sb::execsrch("update-grub2") && sb::isfile("/var/lib/dpkg/info/grub-" % grub.name % ".list"))
        {
            if(ui->grubinstallcopydisable->isVisible()) ui->grubinstallcopydisable->hide(),
                                                        ui->grubinstallcopy->show();
        }
        else if(ui->grubinstallcopy->isVisible())
            ui->grubinstallcopy->hide(),
            ui->grubinstallcopydisable->show();
    }
    else if(nohmcpy[0] && mpt == "/home")
        ui->partitionsettings->item(ui->partitionsettings->currentRow(), 4)->setText("/home"),
        ui->userdatafilescopy->setDisabled(true),
        nohmcpy[1] = true;
    else
        ui->partitionsettings->item(ui->partitionsettings->currentRow(), 4)->setText("SWAP");

    if(ui->mountpoint->currentIndex() && ui->mountpoint->currentText() != "SWAP") ui->mountpoint->removeItem(ui->mountpoint->currentIndex());

    if(ui->mountpoint->currentIndex())
        ui->mountpoint->setCurrentIndex(0);
    else if(! ui->mountpoint->currentText().isEmpty())
        ui->mountpoint->setCurrentText(nullptr);

    ui->partitionsettings->item(ui->partitionsettings->currentRow(), 4)->setToolTip(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 4)->text()),
    busy(false);
}

void systemback::on_filesystem_currentIndexChanged(cQStr &arg1)
{
    if(! (ui->format->isChecked() || ui->partitionsettings->item(ui->partitionsettings->currentRow(), 7)->text() == arg1)) ui->format->setChecked(true);
}

void systemback::on_format_clicked(bool chckd)
{
    if(! chckd)
    {
        if(ui->mountpoint->currentText() == "/boot/efi")
        {
            if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 7)->text() != "vfat") ui->format->setChecked(true);
        }
        else if(ui->mountpoint->currentText() == "SWAP")
        {
            if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 7)->text() != "swap") ui->format->setChecked(true);
        }
        else if(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 7)->text() != ui->filesystem->currentText())
            ui->format->setChecked(true);
    }
}

void systemback::on_mountpoint_currentTextChanged(cQStr &arg1)
{
    uchar ccnt(icnt == 100 ? icnt = 0 : ++icnt);

    if(ui->mountpoint->isEnabled())
    {
        if(! arg1.isEmpty() && (! sb::like(arg1, {"_/*", "_S_", "_SW_", "_SWA_", "_SWAP_"}) || sb::like(arg1, {"* *", "*//*"})))
            ui->mountpoint->setCurrentText(sb::left(arg1, -1));
        else
        {
            QStr mpt(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 3)->text()), ompt(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 4)->text());

            if(sb::like(mpt, {"_/boot/efi_", "_/home_", "_SWAP_"}))
            {
                if(ui->format->isEnabled())
                {
                    ui->format->setDisabled(true);
                    if(ui->filesystem->isEnabled()) ui->filesystem->setDisabled(true);
                }
            }
            else if(ompt.isEmpty() || ui->partitionsettings->item(ui->partitionsettings->currentRow(), 5)->text() != "btrfs")
            {
                if(sb::like(arg1, {"_/boot/efi_", "_SWAP_"}))
                {
                    if(ui->filesystem->isEnabled()) ui->filesystem->setDisabled(true);
                }
                else if(! ui->filesystem->isEnabled())
                    ui->filesystem->setEnabled(true);

                if(! ui->format->isEnabled()) ui->format->setEnabled(true);
                if(! ui->format->isChecked()) ui->format->setChecked(true);
            }
            else if(sb::like(arg1, {"_/boot/efi_", "_SWAP_"}))
                return ui->changepartition->isEnabled() ? ui->changepartition->setDisabled(true) : void();

            if(arg1.isEmpty() || (arg1.length() > 1 && arg1.endsWith('/')) || sb::like(arg1, {'_' % ompt % '_', "_/bin_", "_/sbin_", "_/etc_", "_/lib_", "_/lib32_", "_/lib64_", "_/media_"}) || (ui->usersettingscopy->isVisible() && arg1.startsWith("/home/")) || (arg1 != "/boot/efi" && ui->partitionsettings->item(ui->partitionsettings->currentRow(), 10)->text().toULongLong() < 268435456) || (grub.isEFI && mpt == "/boot/efi" && arg1 != "/boot/efi") || (nohmcpy[0] && mpt == "/home" && arg1 != "/home") || (mpt == "SWAP" && arg1 != "SWAP")
                || (arg1 != "SWAP" && [&] {
                        for(ushort a(0) ; a < ui->partitionsettings->rowCount() ; ++a)
                            if(ui->partitionsettings->item(a, 4)->text() == arg1) return true;

                        return false;
                    }()))
            {
                if(ui->changepartition->isEnabled()) ui->changepartition->setDisabled(true);
            }
            else if(! sb::like(arg1, {"_/_", "_/home_", "_/boot_", "_/boot/efi_", "_/tmp_", "_/usr_", "_/usr/local_", "_/var_", "_/srv_", "_/opt_", "_SWAP_"}))
            {
                if(ui->changepartition->isEnabled()) ui->changepartition->setDisabled(true);
                sb::delay(300);

                if(ccnt == icnt && [&] {
                        switch(sb::stype(ppipe ? sb::sdir[1] % '/' % cpoint % '_' % pname % arg1 : arg1)) {
                        case sb::Notexist:
                        case sb::Isdir:
                            return true;
                        }

                        return false;
                    }() && QTemporaryDir("/tmp/" % QStr(arg1).replace('/', '_') % '_' % sb::rndstr()).isValid()) ui->changepartition->setEnabled(true);
            }
            else if(! ui->changepartition->isEnabled())
                ui->changepartition->setEnabled(true);
        }
    }
}

void systemback::on_copynext_clicked()
{
    dialogopen(ui->function1->text() == tr("System copy") ? 105 : 106);
}

void systemback::on_repairpartitionrefresh_clicked()
{
    busy(), ui->repaircover->show();

    {
        QStr mnts(sb::fload("/proc/self/mounts", true));
        QTS in(&mnts, QIODevice::ReadOnly);

        while(! in.atEnd())
        {
            QStr cline(in.readLine());
            if(cline.contains(" /mnt")) sb::umount(cline.split(' ').at(1));
        }
    }

    sb::fssync(),
    ui->repairmountpoint->clear(),
    ui->repairmountpoint->addItems({nullptr, "/mnt", "/mnt/home", "/mnt/boot"});
    if(grub.isEFI) ui->repairmountpoint->addItem("/mnt/boot/efi");
    ui->repairmountpoint->addItems({"/mnt/usr", "/mnt/var", "/mnt/opt", "/mnt/usr/local"}),
    ui->repairmountpoint->setCurrentIndex(1),
    rmntcheck(),
    on_partitionrefresh2_clicked(),
    on_repairmountpoint_currentTextChanged("/mnt");
    if(ui->repairpanel->isVisible() && ! ui->repairback->hasFocus()) ui->repairback->setFocus();
    ui->repaircover->hide(), busy(false);
}

void systemback::on_repairpartition_currentIndexChanged(cQStr &arg1)
{
    ui->repairpartition->resize(fontMetrics().width(arg1) + ss(30), ui->repairpartition->height()),
    ui->repairpartition->move(ui->repairmountpoint->x() - ui->repairpartition->width() - ss(8), ui->repairpartition->y());
}

void systemback::on_repairmountpoint_currentTextChanged(cQStr &arg1)
{
    uchar ccnt(icnt == 100 ? icnt = 0 : ++icnt);

    if(! arg1.isEmpty() && (! sb::like(arg1, {"_/_", "_/m_", "_/mn_", "_/mnt_", "_/mnt/*"}) || sb::like(arg1, {"* *", "*//*"})))
        ui->repairmountpoint->setCurrentText(sb::left(arg1, -1));
    else if(! arg1.startsWith("/mnt") || sb::like(arg1, {"*/_", "_/mnt/bin_", "_/mnt/sbin_", "_/mnt/etc_", "_/mnt/lib_", "_/mnt/lib32_", "_/mnt/lib64_", "_/mnt/media_"}) || (arg1.length() > 5 && sb::issmfs("/", "/mnt")) || sb::mcheck(arg1 % '/'))
    {
        if(ui->repairmount->isEnabled()) ui->repairmount->setDisabled(true);
    }
    else if(! sb::like(arg1, {"_/mnt_", "_/mnt/home_", "_/mnt/boot_", "_/mnt/boot/efi_", "_/mnt/usr_", "_/mnt/usr/local_", "_/mnt/var_", "_/mnt/opt_"}))
    {
        if(ui->repairmount->isEnabled()) ui->repairmount->setDisabled(true);
        sb::delay(300);
        if(ccnt == icnt && QTemporaryDir("/tmp/" % QStr(arg1).replace('/', '_') % '_' % sb::rndstr()).isValid()) ui->repairmount->setEnabled(true);
    }
    else if(! ui->repairmount->isEnabled())
        ui->repairmount->setEnabled(true);
}

void systemback::on_repairmount_clicked()
{
    if(! ui->repairmount->text().isEmpty())
    {
        busy(), ui->repaircover->show();

        {
            QStr path;

            for(cQStr &cpath : sb::right(ui->repairmountpoint->currentText(), -5).split('/'))
                if(! (sb::isdir("/mnt/" % path.append('/' % cpath)) || sb::crtdir("/mnt" % path))) sb::rename("/mnt" % path, "/mnt" % path % '_' % sb::rndstr()),
                                                                                                   sb::crtdir("/mnt" % path);
        }

        if(sb::mount(ui->repairpartition->currentText(), ui->repairmountpoint->currentText()))
        {
            on_partitionrefresh2_clicked();

            if(ui->repairmountpoint->currentIndex())
            {
                ui->repairmountpoint->removeItem(ui->repairmountpoint->currentIndex());
                if(ui->repairmountpoint->currentIndex()) ui->repairmountpoint->setCurrentIndex(0);
            }
            else if(! ui->repairmountpoint->currentText().isEmpty())
                ui->repairmountpoint->setCurrentText(nullptr);
        }
        else
            ui->repairmount->setText(nullptr),
            ui->repairmount->setIcon(QIcon(":pictures/error.png"));

        ui->repaircover->hide(), busy(false);

        if(ui->repairmount->text().isEmpty()) ui->repairmount->setEnabled(true),
                                              sb::delay(500),
                                              ui->repairmount->setIcon(QIcon()),
                                              ui->repairmount->setText(tr("Mount"));
    }

    if(! (ui->repairback->hasFocus() || ui->repairmountpoint->hasFocus())) ui->repairback->setFocus();
}

void systemback::on_livename_textChanged(cQStr &arg1)
{
    uchar ccnt(icnt == 100 ? icnt = 0 : ++icnt);

    if(cpos > -1) ui->livename->setCursorPosition(cpos),
                  cpos = -1;

    if([&arg1] {
            for(uchar a(0) ; a < arg1.length() ; ++a)
            {
                cQChar &ctr(arg1.at(a));
                if(ctr == '/' || ((ctr < 'a' || ctr > 'z') && (ctr < 'A' || ctr > 'Z') && ! (ctr.isDigit() || ctr.isPunct()))) return true;
            }

            return false;
        }() || arg1.toUtf8().length() > 32 || arg1.toLower().endsWith(".iso"))

        ui->livename->setText(QStr(arg1).replace(cpos = ui->livename->cursorPosition() - 1, 1, nullptr));
    else
    {
        if(ui->livenamepipe->isVisible()) ui->livenamepipe->hide();

        if(arg1 == "auto")
        {
            QFont fnt;
            fnt.setItalic(true),
            ui->livename->setFont(fnt);
            if(ui->livenameerror->isVisible()) ui->livenameerror->hide();
        }
        else
        {
            if(ui->livenew->isEnabled()) ui->livenew->setDisabled(true);
            if(ui->livename->fontInfo().italic()) ui->livename->setFont(font());

            if(! arg1.isEmpty())
            {
                sb::delay(300);

                if(ccnt == icnt)
                {
                    if(QTemporaryDir("/tmp/" % arg1 % '_' % sb::rndstr()).isValid())
                    {
                        if(ui->livenameerror->isVisible()) ui->livenameerror->hide();
                        ui->livenamepipe->show();
                    }
                    else if(ui->livenameerror->isHidden())
                        ui->livenameerror->show();
                }
            }
            else if(ui->livenameerror->isHidden())
                ui->livenameerror->show();
        }
    }
}

void systemback::dirxpnd(QTrWI *item, bool inc)
{
    item->setBackgroundColor(0, Qt::transparent),
    busy();
    cQTrWI *twi(item);
    QStr path('/' % twi->text(0));
    while(twi->parent()) path.prepend('/' % (inc ? (twi = twi->parent())->text(0) : sb::left((twi = twi->parent())->text(0), -1)));
    QLW clw(inc ? ui->includedlist : ui->excludedlist);

    auto itmxpnd([&](cQStr &pdir) {
            QStr fpath(pdir % path);

            for(ushort a(0) ; a < item->childCount() ; ++a)
            {
                QTrWI *ctwi(item->child(a));
                QStr iname(ctwi->text(0)), ipath(fpath % '/' % iname);

                if(sb::stype(ipath) == sb::Isdir)
                {
                    if(ctwi->icon(0).isNull()) ctwi->setIcon(0, QIcon(QPixmap(":pictures/dir.png").scaled(ss(12), ss(9), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
                    QStr epath(sb::right(path, -1) % '/' % iname % '/');

                    if(clw->findItems(epath, Qt::MatchExactly).isEmpty())
                    {
                        QSL itmlst;
                        itmlst.reserve(ctwi->childCount());
                        for(ushort b(0) ; b < ctwi->childCount() ; ++b) itmlst.append(ctwi->child(b)->text(0));

                        for(cQStr &siname : QDir(ipath).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot))
                        {
                            if(clw->findItems(epath % siname, Qt::MatchExactly).isEmpty())
                            {
                                for(ushort b(0) ; b < itmlst.count() ; ++b)
                                    if(itmlst.at(b) == siname)
                                    {
                                        itmlst.removeAt(b);
                                        goto next;
                                    }

                                QTrWI *sctwi(new QTrWI);
                                sctwi->setText(0, siname),
                                ctwi->addChild(sctwi);
                            }

                        next:;
                        }
                    }
                }

                ctwi->sortChildren(0, Qt::AscendingOrder);
            }
        });

    if(sb::stype("/root" % path) == sb::Isdir) itmxpnd("/root");
    QFile file("/etc/passwd");

    if(sb::fopen(file))
        while(! file.atEnd())
        {
            QStr usr(file.readLine().trimmed());
            if(usr.contains(":/home/") && sb::stype("/home/" % (usr = sb::left(usr, sb::instr(usr, ":") -1)) % path) == sb::Isdir) itmxpnd("/home/" % usr);
        }

    busy(false);
}

void systemback::on_excludeitemslist_itemExpanded(QTrWI *item)
{
    if(item->backgroundColor(0) != Qt::transparent) dirxpnd(item);
    item->setText(0, item->text(0) % '/');
}

void systemback::on_includeitemslist_itemExpanded(QTrWI *item)
{
    if(item->backgroundColor(0) != Qt::transparent) dirxpnd(item, true);
}

void systemback::on_excludeitemslist_itemCollapsed(QTrWI *item)
{
    item->setText(0, sb::left(item->text(0), -1));
}

void systemback::on_excludeitemslist_currentItemChanged(QTrWI *crrnt)
{
    if(crrnt && ! ui->excludeadditem->isEnabled())
    {
        ui->excludeadditem->setEnabled(true);

        if(ui->excluderemoveitem->isEnabled()) ui->excludedlist->setCurrentItem(nullptr),
                                               ui->excluderemoveitem->setDisabled(true);
    }
}

void systemback::on_includeitemslist_currentItemChanged(QTrWI *crrnt)
{
    if(crrnt && ! ui->includeadditem->isEnabled())
    {
        ui->includeadditem->setEnabled(true);

        if(ui->includeremoveitem->isEnabled()) ui->includedlist->setCurrentItem(nullptr),
                                               ui->includeremoveitem->setDisabled(true);
    }
}

void systemback::on_excludedlist_currentItemChanged(QLWI *crrnt)
{
    if(crrnt && ! ui->excluderemoveitem->isEnabled())
    {
        ui->excluderemoveitem->setEnabled(true);

        if(ui->excludeadditem->isEnabled()) ui->excludeitemslist->setCurrentItem(nullptr),
                                            ui->excludeadditem->setDisabled(true);
    }
}

void systemback::on_includedlist_currentItemChanged(QLWI *crrnt)
{
    if(crrnt && ! ui->includeremoveitem->isEnabled())
    {
        ui->includeremoveitem->setEnabled(true);

        if(ui->includeadditem->isEnabled()) ui->includeitemslist->setCurrentItem(nullptr),
                                            ui->includeadditem->setDisabled(true);
    }
}

void systemback::on_excludeadditem_clicked()
{
    busy(), ui->excludecover->show();
    cQTrWI *twi(ui->excludeitemslist->currentItem());
    QStr path(twi->text(0));
    while(twi->parent()) path.prepend((twi = twi->parent())->text(0));
    QStr elst;
    QFile file(excfile);

    if(sb::fopen(file))
    {
        while(! file.atEnd())
        {
            QStr cline(sb::left(file.readLine(), -1));
            if(! (cline.isEmpty() || cline.startsWith(path))) elst.append(cline % '\n');
        }

        file.close();

        if(sb::crtfile(excfile, elst.append(path % '\n')))
        {
            if(ui->excludedlist->count()) ui->excludedlist->clear();
            QTS in(&elst, QIODevice::ReadOnly);

            while(! in.atEnd())
            {
                QStr cline(in.readLine());

                if(sb::like(cline, {"_.*", "_snap_", "_snap/*"}))
                {
                    if(ui->pointexclude->isChecked()) ui->excludedlist->addItem(cline);
                }
                else if(ui->liveexclude->isChecked())
                    ui->excludedlist->addItem(cline);
            }

            if(path.endsWith('/'))
            {
                ui->excludeitemslist->currentItem()->setExpanded(false);
                QList<QTrWI *> ilst;
                ilst.reserve((twi = ui->excludeitemslist->currentItem())->childCount());
                for(ushort a(0) ; a < twi->childCount() ; ++a) ilst.append(twi->child(a));
                for(cQTrWI *ctwi : ilst) delete ctwi;
            }
            else
            {
                QTrWI *ptwi(ui->excludeitemslist->currentItem()->parent());
                delete ui->excludeitemslist->currentItem();
                if(ptwi && ! ptwi->childCount()) ptwi->setText(0, sb::left(ptwi->text(0), -1));
            }

            ui->excludeitemslist->setCurrentItem(nullptr),
            ui->excludeadditem->setDisabled(true),
            ui->excludeback->setFocus();
        }
    }

    ui->excludecover->hide(), busy(false);
}

void systemback::on_includeadditem_clicked()
{
    busy(), ui->includecover->show();
    cQTrWI *twi(ui->includeitemslist->currentItem());
    QStr path(twi->text(0));
    while(twi->parent()) path.prepend((twi = twi->parent())->text(0) % '/');
    QStr ilst;
    QFile file(incfile);

    if(sb::fopen(file))
    {
        while(! file.atEnd())
        {
            QStr cline(sb::left(file.readLine(), -1));
            if(! (cline.isEmpty() || cline.startsWith(path))) ilst.append(cline % '\n');
        }

        file.close();

        if(sb::crtfile(incfile, ilst.append(path % '\n')))
        {
            if(ui->includedlist->count()) ui->includedlist->clear();
            QTS in(&ilst, QIODevice::ReadOnly);
            while(! in.atEnd()) ui->includedlist->addItem(in.readLine());
            QTrWI *ptwi(ui->includeitemslist->currentItem()->parent());
            delete ui->includeitemslist->currentItem();
            if(ptwi && ! ptwi->childCount()) ptwi->setText(0, sb::left(ptwi->text(0), -1));
            ui->includeitemslist->setCurrentItem(nullptr),
            ui->includeadditem->setDisabled(true),
            ui->includeback->setFocus();
        }
    }

    ui->includecover->hide(), busy(false);
}

void systemback::on_excluderemoveitem_clicked()
{
    busy(), ui->excludecover->show();
    QFile file(excfile);

    if(sb::fopen(file))
    {
        QStr ctxt(ui->excludedlist->currentItem()->text()), elst;

        while(! file.atEnd())
        {
            QStr cline(sb::left(file.readLine(), -1));
            if(! (cline.isEmpty() || cline == ctxt)) elst.append(cline % '\n');
        }

        file.close();

        if(sb::crtfile(excfile, elst)) delete ui->excludedlist->currentItem(),
                                       ui->excludedlist->setCurrentItem(nullptr),
                                       ui->excluderemoveitem->setDisabled(true),
                                       ilstupdt(),
                                       ui->excludeback->setFocus();
    }

    ui->excludecover->hide(), busy(false);
}

void systemback::on_includeremoveitem_clicked()
{
    busy(), ui->includecover->show();
    QFile file(incfile);

    if(sb::fopen(file))
    {
        QStr ctxt(ui->includedlist->currentItem()->text()), ilst;

        while(! file.atEnd())
        {
            QStr cline(sb::left(file.readLine(), -1));
            if(! (cline.isEmpty() || cline == ctxt)) ilst.append(cline % '\n');
        }

        file.close();

        if(sb::crtfile(incfile, ilst)) delete ui->includedlist->currentItem(),
                                       ui->includedlist->setCurrentItem(nullptr),
                                       ui->includeremoveitem->setDisabled(true),
                                       ilstupdt(true),
                                       ui->includeback->setFocus();
    }

    ui->includecover->hide(), busy(false);
}

void systemback::on_fullname_textChanged(cQStr &arg1)
{
    if(cpos > -1) ui->fullname->setCursorPosition(cpos),
                  cpos = -1;

    if(arg1.isEmpty())
    {
        if(ui->fullnamepipe->isVisible())
            ui->fullnamepipe->hide();
        else if(ui->installnext->isEnabled())
            ui->installnext->setDisabled(true);
    }
    else if([&arg1] {
            for(uchar a(0) ; a < arg1.length() ; ++a)
            {
                cQChar &ctr(arg1.at(a));
                if(ctr == ':' || ctr == ',' || ctr == '=' || ! (ctr.isLetterOrNumber() || ctr.isPrint())) return true;
            }

            return false;
        }() || sb::like(arg1, {"_ *", "*  *", "*ß*"}))

        ui->fullname->setText(QStr(arg1).replace(cpos = ui->fullname->cursorPosition() - 1, 1, nullptr));
    else if(arg1.at(0).isLower())
        cpos = ui->fullname->cursorPosition(),
        ui->fullname->setText(arg1.at(0).toUpper() % sb::right(arg1, -1));
    else
    {
        for(cQStr &word : arg1.split(' '))
            if(! word.isEmpty() && word.at(0).isLower())
            {
                cpos = ui->fullname->cursorPosition();
                return ui->fullname->setText(QStr(arg1).replace(' ' % word.at(0) % sb::right(word, -1), ' ' % word.at(0).toUpper() % sb::right(word, -1)));
            }

        if(ui->fullnamepipe->isHidden()) ui->fullnamepipe->show();
    }
}

void systemback::on_fullname_editingFinished()
{
    if(ui->fullname->text().endsWith(' ')) ui->fullname->setText(ui->fullname->text().trimmed());
}

void systemback::on_username_textChanged(cQStr &arg1)
{
    if(cpos > -1) ui->username->setCursorPosition(cpos),
                  cpos = -1;

    if(arg1.isEmpty())
    {
        if(ui->usernameerror->isVisible())
            ui->usernameerror->hide();
        else if(ui->usernamepipe->isVisible())
        {
            ui->usernamepipe->hide();
            if(ui->installnext->isEnabled()) ui->installnext->setDisabled(true);
        }
    }
    else if(arg1 == "root")
    {
        if(ui->usernameerror->isHidden())
        {
            if(ui->usernamepipe->isVisible())
            {
                ui->usernamepipe->hide();
                if(ui->installnext->isEnabled()) ui->installnext->setDisabled(true);
            }

            ui->usernameerror->show();
        }
    }
    else if(arg1 != arg1.toLower())
        ui->username->setText(arg1.toLower());
    else if([&arg1] {
            for(uchar a(0) ; a < arg1.length() ; ++a)
            {
                cQChar &ctr(arg1.at(a));
                if((ctr < 'a' || ctr > 'z') && ! (ctr == '.' || ctr == '_' || ctr == '@' || (a && (ctr.isDigit() || ctr == '-' || ctr == '$')))) return true;
            }

            return false;
        }() || (arg1.contains('$') && (arg1.count('$') > 1 || ! arg1.endsWith('$'))))

        ui->username->setText(QStr(arg1).replace(cpos = ui->username->cursorPosition() - 1, 1, nullptr));
    else if(ui->usernamepipe->isHidden())
    {
        if(ui->usernameerror->isVisible()) ui->usernameerror->hide();
        ui->usernamepipe->show();
    }
}

void systemback::on_hostname_textChanged(cQStr &arg1)
{
    if(cpos > -1) ui->hostname->setCursorPosition(cpos),
                  cpos = -1;

    if(arg1.isEmpty())
    {
        if(ui->hostnameerror ->isVisible())
            ui->hostnameerror->hide();
        else if(ui->hostnamepipe->isVisible())
        {
            ui->hostnamepipe->hide();
            if(ui->installnext->isEnabled()) ui->installnext->setDisabled(true);
        }
    }
    else if(arg1.length() > 1 && sb::like(arg1, {"*._", "*-_"}) && ! sb::like(arg1, {"*..*", "*--*", "*.-*", "*-.*"}))
    {
        if(ui->hostnameerror->isHidden())
        {
            if(ui->hostnamepipe->isVisible())
            {
                ui->hostnamepipe->hide();
                if(ui->installnext->isEnabled()) ui->installnext->setDisabled(true);
            }

            ui->hostnameerror->show();
        }
    }
    else if([&arg1] {
            for(uchar a(0) ; a < arg1.length() ; ++a)
            {
                cQChar &ctr(arg1.at(a));
                if((ctr < 'a' || ctr > 'z') && (ctr < 'A' || ctr > 'Z') && ! (ctr.isDigit() || (a && (ctr == '-' || ctr == '.')))) return true;
            }

            return false;
        }() || (arg1.length() > 1 && sb::like(arg1, {"*..*", "*--*", "*.-*", "*-.*"})))

        ui->hostname->setText(QStr(arg1).replace(cpos = ui->hostname->cursorPosition() - 1, 1, nullptr));
    else if(ui->hostnamepipe->isHidden())
    {
        if(ui->hostnameerror->isVisible()) ui->hostnameerror->hide();
        ui->hostnamepipe->show();
    }
}

void systemback::on_password1_textChanged(cQStr &arg1)
{
    if(ui->passwordpipe->isVisible())
    {
        ui->passwordpipe->hide();
        if(ui->installnext->isEnabled()) ui->installnext->setDisabled(true);
    }

    if(arg1.isEmpty())
    {
        if(! ui->password2->text().isEmpty()) ui->password2->clear();
        if(ui->password2->isEnabled()) ui->password2->setDisabled(true);
        if(ui->passworderror->isVisible()) ui->passworderror->hide();
    }
    else
    {
        if(! ui->password2->isEnabled()) ui->password2->setEnabled(true);

        if(ui->password2->text().isEmpty())
        {
            if(ui->passworderror->isVisible()) ui->passworderror->hide();
        }
        else if(arg1 == ui->password2->text())
        {
            if(ui->passworderror->isVisible()) ui->passworderror->hide();
            ui->passwordpipe->show();
        }
        else if(ui->passworderror->isHidden())
            ui->passworderror->show();
    }
}

void systemback::on_password2_textChanged()
{
    on_password1_textChanged(ui->password1->text());
}

void systemback::on_rootpassword1_textChanged(cQStr &arg1)
{
    if(ui->rootpasswordpipe->isVisible())
    {
        ui->rootpasswordpipe->hide();
        if(ui->installnext->isEnabled()) ui->installnext->setDisabled(true);
    }

    if(arg1.isEmpty())
    {
        if(! ui->rootpassword2->text().isEmpty()) ui->rootpassword2->clear();
        if(ui->rootpassword2->isEnabled()) ui->rootpassword2->setDisabled(true);
        if(ui->rootpassworderror->isVisible()) ui->rootpassworderror->hide();
    }
    else
    {
        if(! ui->rootpassword2->isEnabled()) ui->rootpassword2->setEnabled(true);

        if(ui->rootpassword2->text().isEmpty())
        {
            if(ui->rootpassworderror->isVisible()) ui->rootpassworderror->hide();
        }
        else if(arg1 == ui->rootpassword2->text())
        {
            if(ui->rootpassworderror->isVisible()) ui->rootpassworderror->hide();
            ui->rootpasswordpipe->show();
        }
        else if(ui->rootpassworderror->isHidden())
            ui->rootpassworderror->show();
    }
}

void systemback::on_rootpassword2_textChanged()
{
    on_rootpassword1_textChanged(ui->rootpassword1->text());
}

void systemback::on_schedulerstate_clicked()
{
    if(ui->schedulerstate->isChecked())
    {
        if(! sb::schdle[0])
        {
            sb::schdle[0] = sb::True;
            if(sb::isdir(sb::sdir[1]) && sb::access(sb::sdir[1], sb::Write)) sb::crtfile(sb::sdir[1] % "/.sbschedule");
        }

        ui->schedulerstate->setText(tr("Enabled"));
        if(sb::schdle[1]) ui->daydown->setEnabled(true);
        if(sb::schdle[1] < 7) ui->dayup->setEnabled(true);
        if(sb::schdle[2]) ui->hourdown->setEnabled(true);
        if(sb::schdle[2] < 23) ui->hourup->setEnabled(true);
        if(sb::schdle[3] && (sb::schdle[1] || sb::schdle[2] || sb::schdle[3] > 30)) ui->minutedown->setEnabled(true);
        if(sb::schdle[3] < 59) ui->minuteup->setEnabled(true);
        ui->silentmode->setEnabled(true);

        if(! sb::schdle[5])
        {
            if(sb::schdle[4] > 10) ui->seconddown->setEnabled(true);
            if(sb::schdle[4] < 99) ui->secondup->setEnabled(true);
            ui->windowposition->setEnabled(true);
        }
    }
    else
    {
        sb::schdle[0] = sb::False,
        ui->schedulerstate->setText(tr("Disabled")),
        ui->silentmode->setDisabled(true);
        if(ui->windowposition->isEnabled()) ui->windowposition->setDisabled(true);

        for(QPB pbtn : ui->schedulerdayhrminpanel->findChildren<QPB>())
            if(pbtn->isEnabled()) pbtn->setDisabled(true);

        for(QWdt wdgt : QWL{ui->secondup, ui->seconddown})
            if(wdgt->isEnabled()) wdgt->setDisabled(true);
    }
}

void systemback::on_silentmode_clicked(bool chckd)
{
    if(! chckd)
    {
        sb::schdle[5] = sb::False;
        if(sb::schdle[4] > 10) ui->seconddown->setEnabled(true);
        if(sb::schdle[4] < 99) ui->secondup->setEnabled(true);
        ui->windowposition->setEnabled(true);
    }
    else if(! sb::schdle[5])
    {
        sb::schdle[5] = sb::True,
        ui->windowposition->setDisabled(true);

        for(QWdt wdgt : QWL{ui->secondup, ui->seconddown})
            if(wdgt->isEnabled()) wdgt->setDisabled(true);
    }
}

void systemback::on_dayup_clicked()
{
    ++sb::schdle[1],
    ui->schedulerday->setText(QStr::number(sb::schdle[1]) % ' ' % tr("day(s)"));
    if(! ui->daydown->isEnabled()) ui->daydown->setEnabled(true);
    if(sb::schdle[1] == 7) ui->dayup->setDisabled(true);
    if(! ui->minutedown->isEnabled() && sb::schdle[3]) ui->minutedown->setEnabled(true);
}

void systemback::on_daydown_clicked()
{
    --sb::schdle[1],
    ui->schedulerday->setText(QStr::number(sb::schdle[1]) % ' ' % tr("day(s)"));
    if(! ui->dayup->isEnabled()) ui->dayup->setEnabled(true);

    if(! sb::schdle[1])
    {
        if(! sb::schdle[2])
        {
            if(sb::schdle[3] < 30) sb::schdle[3] = 30,
                                   ui->schedulerminute->setText(QStr::number(sb::schdle[3]) % ' ' % tr("minute(s)"));

            if(ui->minutedown->isEnabled() && sb::schdle[3] < 31) ui->minutedown->setDisabled(true);
        }

        ui->daydown->setDisabled(true);
    }
}

void systemback::on_hourup_clicked()
{
    ++sb::schdle[2],
    ui->schedulerhour->setText(QStr::number(sb::schdle[2]) % ' ' % tr("hour(s)"));
    if(! ui->hourdown->isEnabled()) ui->hourdown->setEnabled(true);
    if(sb::schdle[2] == 23) ui->hourup->setDisabled(true);
    if(! ui->minutedown->isEnabled() && sb::schdle[3]) ui->minutedown->setEnabled(true);
}

void systemback::on_hourdown_clicked()
{
    --sb::schdle[2],
    ui->schedulerhour->setText(QStr::number(sb::schdle[2]) % ' ' % tr("hour(s)"));
    if(! ui->hourup->isEnabled()) ui->hourup->setEnabled(true);

    if(! sb::schdle[2])
    {
        if(! sb::schdle[1])
        {
            if(sb::schdle[3] < 30) sb::schdle[3] = 30,
                                   ui->schedulerminute->setText(QStr::number(sb::schdle[3]) % ' ' % tr("minute(s)"));

            if(ui->minutedown->isEnabled() && sb::schdle[3] < 31) ui->minutedown->setDisabled(true);
        }

        ui->hourdown->setDisabled(true);
    }
}

void systemback::on_minuteup_clicked()
{
    sb::schdle[3] = sb::schdle[3] + (sb::schdle[3] == 55 ? 4 : 5),
    ui->schedulerminute->setText(QStr::number(sb::schdle[3]) % ' ' % tr("minute(s)"));
    if(! ui->minutedown->isEnabled()) ui->minutedown->setEnabled(true);
    if(sb::schdle[3] == 59) ui->minuteup->setDisabled(true);
}

void systemback::on_minutedown_clicked()
{
    sb::schdle[3] = sb::schdle[3] - (sb::schdle[3] == 59 ? 4 : 5),
    ui->schedulerminute->setText(QStr::number(sb::schdle[3]) % ' ' % tr("minute(s)"));
    if(! ui->minuteup->isEnabled()) ui->minuteup->setEnabled(true);
    if(! sb::schdle[3] || (sb::schdle[3] == 30 && ! (sb::schdle[1] || sb::schdle[2]))) ui->minutedown->setDisabled(true);
}

void systemback::on_secondup_clicked()
{
    sb::schdle[4] = sb::schdle[4] + (sb::schdle[4] == 95 ? 4 : 5),
    ui->schedulersecond->setText(QStr::number(sb::schdle[4]) % ' ' % tr("seconds"));
    if(! ui->seconddown->isEnabled()) ui->seconddown->setEnabled(true);
    if(sb::schdle[4] == 99) ui->secondup->setDisabled(true);
}

void systemback::on_seconddown_clicked()
{
    sb::schdle[4] = sb::schdle[4] - (sb::schdle[4] == 99 ? 4 : 5),
    ui->schedulersecond->setText(QStr::number(sb::schdle[4]) % ' ' % tr("seconds"));
    if(! ui->secondup->isEnabled()) ui->secondup->setEnabled(true);
    if(sb::schdle[4] == 10) ui->seconddown->setDisabled(true);
}

void systemback::on_windowposition_currentIndexChanged(cQStr &arg1)
{
    ui->windowposition->resize(fontMetrics().width(arg1) + ss(30), ui->windowposition->height());

    if(ui->schedulepanel->isVisible())
    {
        QStr cval(arg1 == tr("Top left") ? "topleft"
                  : arg1 == tr("Top right") ? "topright"
                  : arg1 == tr("Center") ? "center"
                  : arg1 == tr("Bottom left") ? "bottomleft" : "bottomright");

        if(sb::schdlr[0] != cval) sb::schdlr[0] = cval;
    }
}

void systemback::on_userdatainclude_clicked(bool chckd)
{
    if(chckd)
    {
        ullong hfree(sb::dfree("/home"));
        QFile file("/etc/passwd");

        if(hfree > 104857600 && sb::dfree("/root") > 104857600 && sb::fopen(file))
            while(! file.atEnd())
            {
                QStr usr(file.readLine().trimmed());
                if(usr.contains(":/home/") && sb::isdir("/home/" % (usr = sb::left(usr, sb::instr(usr, ":") -1))) && sb::dfree("/home/" % usr) != hfree) return ui->userdatainclude->setChecked(false);
            }
        else
            ui->userdatainclude->setChecked(false);
    }
}

void systemback::on_interrupt_clicked()
{
    if(! (irblck || intrrpt))
    {
        if(! intrptimer)
        {
            pset(13),
            intrrpt = true,
            ui->interrupt->setDisabled(true);
            if(! sb::ExecKill) sb::ExecKill = true;

            if(sb::SBThrd.isRunning()) sb::ThrdKill = true,
                                       sb::thrdelay(),
                                       sb::error("\n " % tr("Systemback worker thread is interrupted by the user.") % "\n\n");

            connect(intrptimer = new QTimer,
#if QT_VERSION < QT_VERSION_CHECK(5, 3, 0)
                SIGNAL(timeout()), this, SLOT(on_interrupt_clicked())
#else
                &QTimer::timeout, this, &systemback::on_interrupt_clicked
#endif
                );

            intrptimer->start(100);
        }
        else if(sstart)
            sb::crtfile(sb::sdir[1] % "/.sbschedule"),
            close();
        else
        {
            delete intrptimer, intrptimer = nullptr;

            if(fscrn)
            {
                ui->statuspanel->hide(),
                ui->mainpanel->show();
                short nwidth(ss(154) + ui->partitionsettings->width() - ui->partitionsettings->contentsRect().width() + ui->partitionsettings->columnWidth(0) + ui->partitionsettings->columnWidth(1) + ui->partitionsettings->columnWidth(2) + ui->partitionsettings->columnWidth(3) + ui->partitionsettings->columnWidth(4) + ui->partitionsettings->columnWidth(5) + ui->partitionsettings->columnWidth(6));
                ushort sz[]{ss(850), ss(465), ss(60)};
                windowmove(nwidth < sz[0] ? nwidth : sz[0], sz[1], false),
                ui->wpanel->setMinimumSize(ss(698), sz[1]),
                ui->wpanel->setMaximumSize(width() - sz[2], height() - sz[2]);
            }
            else
            {
                for(QCB ckbx : ui->sbpanel->findChildren<QCB>())
                    if(ckbx->isChecked()) ckbx->setChecked(false);

                on_pointpipe1_clicked(),
                ui->statuspanel->hide();

                if(ui->livepanel->isVisibleTo(ui->mainpanel))
                    ui->liveback->setFocus();
                else if(! ui->sbpanel->isVisibleTo(ui->mainpanel))
                {
                    ui->sbpanel->show(),
                    ui->function1->setText("Systemback");

                    if(ui->restorepanel->isVisibleTo(ui->mainpanel))
                        ui->restorepanel->hide();
                    else if(ui->copypanel->isVisibleTo(ui->mainpanel))
                        ui->copypanel->hide();
                    else if(ui->repairpanel->isVisibleTo(ui->mainpanel))
                        ui->repairpanel->hide();

                    ui->functionmenunext->isEnabled() ? ui->functionmenunext->setFocus() : ui->functionmenuback->setFocus();
                }

                ui->mainpanel->show(),
                windowmove(ss(698), ss(465));
            }
        }
    }
}

void systemback::on_livewritestart_clicked()
{
    dialogopen(108);
}

void systemback::on_schedulerlater_clicked()
{
    if(sb::isdir(sb::sdir[1]) && sb::access(sb::sdir[1], sb::Write)) sb::crtfile(sb::sdir[1] % "/.sbschedule");
    close();
}

void systemback::on_scalingup_clicked()
{
    ui->scalingfactor->setText([this] {
            if(ui->scalingfactor->text() == "auto")
            {
                ui->scalingdown->setEnabled(true);
                return "x1";
            }
            else if(ui->scalingfactor->text() == "x1")
                return "x1.5";

            ui->scalingup->setDisabled(true);
            return "x2";
        }());
}

void systemback::on_scalingdown_clicked()
{
    ui->scalingfactor->setText([this] {
            if(ui->scalingfactor->text() == "x2")
            {
                ui->scalingup->setEnabled(true);
                return "x1.5";
            }
            else if(ui->scalingfactor->text() == "x1.5")
                return "x1";

            ui->scalingdown->setDisabled(true);
            return "auto";
        }());
}

void systemback::on_newrestorepoint_clicked()
{
    statustart();

    auto err([this] {
            if(intrrpt)
                intrrpt = false;
            else
            {
                dialogopen(sb::dfree(sb::sdir[1]) < 104857600 ? 305 : 319);
                if(! sstart) pntupgrade();
            }
        });

    for(cQStr &item : QDir(sb::sdir[1]).entryList(QDir::Dirs | QDir::Hidden | QDir::NoSymLinks | QDir::NoDotAndDotDot))
        if(sb::like(item, {"_.DELETED_*", "_.S00_*"}))
        {
            if(prun.type != 12) pset(12);
            if(! sb::remove(sb::sdir[1] % '/' % item) || intrrpt) return err();
        }

    for(uchar a(9) ; a > 1 ; --a)
        if(! sb::pnames[a].isEmpty() && (a == 9 || a > 2 ? sb::pnumber < a + 2 : sb::pnumber == 3)) ++ppipe;

    if(ppipe)
    {
        uchar dnum(0);

        for(uchar a(9) ; a > 1 ; --a)
            if(! sb::pnames[a].isEmpty() && (a == 9 || a > 2 ? sb::pnumber < a + 2 : sb::pnumber == 3))
            {
                pset(14, bstr(QStr::number(++dnum) % '/' % QStr::number(ppipe)));
                if(! (sb::rename(sb::sdir[1] % (a < 9 ? QStr("/S0" % QStr::number(a + 1)) : "/S10") % '_' % sb::pnames[a], sb::sdir[1] % "/.DELETED_" % sb::pnames[a]) && sb::remove(sb::sdir[1] % "/.DELETED_" % sb::pnames[a])) || intrrpt) return err();
            }
    }

    pset(15);
    QStr dtime(QDateTime().currentDateTime().toString("yyyy-MM-dd,hh.mm.ss"));
    if(! sb::crtrpoint(dtime)) return err();

    for(uchar a(0) ; a < 9 && sb::isdir(sb::sdir[1] % "/S0" % QStr::number(a + 1) % '_' % sb::pnames[a]) ; ++a)
        if(! sb::rename(sb::sdir[1] % "/S0" % QStr::number(a + 1) % '_' % sb::pnames[a], sb::sdir[1] % (a < 8 ? "/S0" : "/S") % QStr::number(a + 2) % '_' % sb::pnames[a])) return err();

    if(! sb::rename(sb::sdir[1] % "/.S00_" % dtime, sb::sdir[1] % "/S01_" % dtime)) return err();
    sb::crtfile(sb::sdir[1] % "/.sbschedule");
    if(intrrpt) return err();
    emptycache();

    if(sstart)
        sb::ThrdKill = true,
        close();
    else
        pntupgrade(),
        ui->statuspanel->hide(),
        ui->mainpanel->show(),
        ui->functionmenunext->isEnabled() ? ui->functionmenunext->setFocus() : ui->functionmenuback->setFocus(),
        windowmove(ss(698), ss(465));
}

void systemback::on_pointdelete_clicked()
{
    statustart();
    uchar dnum(0);

    for(uchar a : {9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 10, 11, 12, 13, 14})
    {
        if(getppipe(a)->isChecked())
        {
            pset(16, bstr(QStr::number(++dnum) % '/' % QStr::number(ppipe)));

            if(! sb::rename(sb::sdir[1] % [a]() -> QStr {
                    switch(a) {
                    case 9:
                        return "/S10_";
                    case 10 ... 14:
                        return "/H0" % QStr::number(a - 9) % '_';
                    default:
                        return "/S0" % QStr::number(a + 1) % '_';
                    }
                }() % sb::pnames[a], sb::sdir[1] % "/.DELETED_" % sb::pnames[a]) || ! sb::remove(sb::sdir[1] % "/.DELETED_" % sb::pnames[a]) || intrrpt) return [this] {
                        if(intrrpt)
                            intrrpt = false;
                        else
                            dialogopen(329),
                            pntupgrade();
                    }();
        }
    }

    pntupgrade(),
    emptycache(),
    ui->statuspanel->hide(),
    ui->mainpanel->show(),
    ui->functionmenunext->isEnabled() ? ui->functionmenunext->setFocus() : ui->functionmenuback->setFocus(),
    windowmove(ss(698), ss(465));
}

void systemback::on_livenew_clicked()
{
    statustart(), pset(17, " 1/3");

    auto err([this](ushort dlg = 0) {
            if(! (intrrpt || dlg == 327))
            {
                if(sb::dfree(sb::sdir[2]) < 104857600 || (sb::isdir("/home/.sbuserdata") && sb::dfree("/home") < 104857600))
                    dlg = 313;
                else if(! dlg)
                    dlg = 328;
            }

            for(cQStr &dir : {"/.sblvtmp", "/media/.sblvtmp", "/snap/.sblvtmp", "/var/.sblvtmp", "/home/.sbuserdata", "/root/.sbuserdata"})
                if(sb::isdir(dir)) sb::remove(dir);

            if(sb::autoiso) on_livemenu_clicked();

            if(intrrpt)
                intrrpt = false;
            else
            {
                if(sb::isdir(sb::sdir[2] % "/.sblivesystemcreate")) sb::remove(sb::sdir[2] % "/.sblivesystemcreate");
                dialogopen(dlg);
            }
        });

    QStr lvtype;

    if((sb::exist(sb::sdir[2] % "/.sblivesystemcreate") && ! sb::remove(sb::sdir[2] % "/.sblivesystemcreate"))
        || intrrpt || ! (sb::crtdir(sb::sdir[2] % "/.sblivesystemcreate") && sb::crtdir(sb::sdir[2] % "/.sblivesystemcreate/.disk") && sb::crtdir(sb::sdir[2] % "/.sblivesystemcreate/" % (lvtype = sb::isfile("/usr/share/initramfs-tools/scripts/casper") ? "casper" : "live")) && sb::crtdir(sb::sdir[2] % "/.sblivesystemcreate/syslinux"))) return err();

    QStr ifname(ui->livename->text() == "auto" ? "systemback_live_" % QDateTime().currentDateTime().toString("yyyy-MM-dd") : ui->livename->text()), ckernel;
    { uchar ncount(0);
    while(sb::exist(sb::sdir[2] % '/' % ifname % ".sblive")) ncount++ ? ifname = sb::left(ifname, sb::rinstr(ifname, "_")) % QStr::number(ncount) : ifname.append("_1"); }
    if(intrrpt || ! (sb::crtfile(sb::sdir[2] % "/.sblivesystemcreate/.disk/info", "Systemback Live (" % ifname % ") - Release " % sb::right(ui->version->text(), -sb::rinstr(ui->version->text(), "_")) % '\n') && sb::copy("/boot/vmlinuz-" % (ckernel = ckname()), sb::sdir[2] % "/.sblivesystemcreate/" % lvtype % "/vmlinuz")) || intrrpt) return err();
    irblck = true;

    if(lvtype == "casper")
    {
        QStr fname, did;

        {
            QFile file("/etc/passwd");
            if(! sb::fopen(file)) return err();

            while(! file.atEnd())
            {
                QStr cline(file.readLine().trimmed());

                if(cline.startsWith(guname() % ':'))
                {
                    QSL uslst(cline.split(':'));
                    if(uslst.count() > 4) fname = sb::left(uslst.at(4), sb::instr(uslst.at(4), ",") - 1);
                    break;
                }
            }
        }

        if(sb::isfile("/etc/lsb-release"))
        {
            QFile file("/etc/lsb-release");
            if(! sb::fopen(file)) return err();

            while(! file.atEnd())
            {
                QStr cline(file.readLine().trimmed());

                if(cline.startsWith("DISTRIB_ID="))
                {
                    did = sb::right(cline, -sb::instr(cline, "="));
                    break;
                }
            }
        }

        if(did.isEmpty()) did = "Ubuntu";
        QFile file("/etc/hostname");
        if(! sb::fopen(file)) return err();
        QStr hname(file.readLine().trimmed());
        file.close();
        if(! sb::crtfile("/etc/casper.conf", "USERNAME=\"" % guname() % "\"\nUSERFULLNAME=\"" % fname % "\"\nHOST=\"" % hname % "\"\nBUILD_SYSTEM=\"" % did % "\"\n\nexport USERNAME USERFULLNAME HOST BUILD_SYSTEM\n") || intrrpt) return err();
        QSL incl{"*integrity_check_", "*mountpoints_", "*fstab_", "*swap_", "*xconfig_", "*networking_", "*disable_update_notifier_", "*disable_hibernation_", "*disable_kde_services_", "*fix_language_selector_", "*disable_trackerd_", "*disable_updateinitramfs_", "*kubuntu_disable_restart_notifications_", "*kubuntu_mobile_session_"};

        for(cQStr &item : QDir("/usr/share/initramfs-tools/scripts/casper-bottom").entryList(QDir::Files))
            if(! (sb::like(item, incl) || cfmod("/usr/share/initramfs-tools/scripts/casper-bottom/" % item, 0644))) return err();
    }
    else
    {
        sb::crtfile("/usr/share/initramfs-tools/scripts/init-bottom/sbfstab", "#!/bin/sh\nif [ \"$1\" != prereqs ] && [ \"$BOOT\" = live ] && [ ! -e /root/etc/fstab ]\nthen touch /root/etc/fstab\nfi\n");
        if(! cfmod("/usr/share/initramfs-tools/scripts/init-bottom/sbfstab", 0755)) return err();
    }

    bool xmntry(sb::isfile("/etc/X11/xorg.conf") || (sb::isdir("/etc/X11/xorg.conf.d") && [] {
            for(cQStr &item : QDir("/etc/X11/xorg.conf.d").entryList(QDir::Files))
                if(item.endsWith(".conf")) return true;

            return false;
        }()));

    if(xmntry)
    {
        sb::crtfile("/usr/share/initramfs-tools/scripts/init-bottom/sbnoxconf", "#!/bin/sh\nif [ \"$1\" != prereqs ] && grep noxconf /proc/cmdline >/dev/null 2>&1\nthen\nif [ -s /root/etc/X11/xorg.conf ]\nthen rm /root/etc/X11/xorg.conf\nfi\nif [ -d /root/etc/X11/xorg.conf.d ] && ls /root/etc/X11/xorg.conf.d | grep .conf$ >/dev/null 2>&1\nthen rm /root/etc/X11/xorg.conf.d/*.conf 2>/dev/null\nfi\nfi\n");
        if(! cfmod("/usr/share/initramfs-tools/scripts/init-bottom/sbnoxconf", 0755)) return err();
    }

    sb::crtfile("/usr/share/initramfs-tools/scripts/init-bottom/sbfinstall", [this]() -> QStr {
            QStr ftxt("#!/bin/sh\nif [ \"$1\" != prereqs ] && grep finstall /proc/cmdline >/dev/null 2>&1\nthen\nif [ -f /root/home/" % guname() % "/.config/autostart/dropbox.desktop ]\nthen rm /root/home/" % guname() % "/.config/autostart/dropbox.desktop\nfi\nif [ -f /root/usr/bin/ksplashqml ]\nthen\nchmod -x /root/usr/bin/ksplash* /root/usr/bin/plasma*\nif [ -f /root/usr/share/autostart/plasma-desktop.desktop ]\nthen mv /root/usr/share/autostart/plasma-desktop.desktop /root/usr/share/autostart/plasma-desktop.desktop_\nfi\nif [ -f /root/usr/share/autostart/plasma-netbook.desktop ]\nthen mv /root/usr/share/autostart/plasma-netbook.desktop /root/usr/share/autostart/plasma-netbook.desktop_\nfi\nfi\n");

            for(uchar a(0) ; a < 5 ; ++a)
            {
                QStr fpath("/etc/" % [a]() -> QStr {
                        switch(a) {
                        case 0:
                            return "lightdm/lightdm.conf";
                        case 1:
                            return "kde4/kdm/kdmrc";
                        case 2:
                            return "sddm.conf";
                        case 3:
                            return "gdm/custom.conf";
                        case 4:
                            return "gdm3/daemon.conf";
                        default:
                            return "mdm/mdm.conf";
                        }
                    }());

                if(sb::isfile(fpath) || sb::isdir(sb::left(fpath, sb::rinstr(fpath, "/") - 1))) ftxt.append([&, a]() -> QStr {
                        switch(a) {
                        case 0:
                            return "cat << EOF >/root/etc/lightdm/lightdm.conf\n[SeatDefaults]\nautologin-guest=false\nautologin-user=" % guname() % "\nautologin-user-timeout=0\nautologin-session=lightdm-autologin\nEOF";
                        case 1:
                            return "sed -ir -e \"s/^#?AutoLoginEnable=.*\\$/AutoLoginEnable=true/\" -e \"s/^#?AutoLoginUser=.*\\$/AutoLoginUser=" % guname() % "/\" -e \"s/^#?AutoReLogin=.*\\$/AutoReLogin=true/\" /root/etc/kde4/kdm/kdmrc";
                        case 2:
                            return "cat << EOF >/root/etc/sddm.conf\n[Autologin]\nUser=" % guname() % "\nSession=plasma.desktop\nEOF";
                        default:
                            return "cat << EOF >/root" % fpath % "\n[daemon]\nAutomaticLoginEnable=True\nAutomaticLogin=" % guname() % "\nEOF";
                        }
                    }() % '\n');
            }

            QStr txt[]{"cat << EOF >/root/etc/xdg/autostart/sbfinstall", "[Desktop Entry]\nEncoding=UTF-8\nVersion=1.0\nName=Systemback installer\n", "Type=Application\nIcon=systemback\nTerminal=false\n", "NoDisplay=true\nEOF\n"};
            return ftxt % txt[0] % ".desktop\n" % txt[1] % "Exec=/usr/lib/systemback/sbsustart finstall gtk+\n" % txt[2] % "NotShowIn=KDE;\n" % txt[3] % txt[0] % "-kde.desktop\n" % txt[1] % "Exec=sh -c \"/usr/lib/systemback/sbsustart finstall && if [ -f /usr/bin/plasmashell ] ; then plasmashell --shut-up & elif [ -f /usr/bin/plasma-desktop ] ; then plasma-desktop & fi\"\n" % txt[2] % "OnlyShowIn=KDE;\n" % txt[3] % "fi\n";
        }());

    if(! cfmod("/usr/share/initramfs-tools/scripts/init-bottom/sbfinstall", 0755)) return err();

    {
        uchar rv(sb::exec("update-initramfs -tck" % ckernel));

        if(lvtype == "casper")
        {
            QSL incl{"*integrity_check_", "*mountpoints_", "*fstab_", "*swap_", "*xconfig_", "*networking_", "*disable_update_notifier_", "*disable_hibernation_", "*disable_kde_services_", "*fix_language_selector_", "*disable_trackerd_", "*disable_updateinitramfs_", "*kubuntu_disable_restart_notifications_", "*kubuntu_mobile_session_"};

            for(cQStr &item : QDir("/usr/share/initramfs-tools/scripts/casper-bottom").entryList(QDir::Files))
                if(! (sb::like(item, incl) || cfmod("/usr/share/initramfs-tools/scripts/casper-bottom/" % item, 0755))) return err();
        }
        else if(! sb::remove("/usr/share/initramfs-tools/scripts/init-bottom/sbfstab"))
            return err();

        if((xmntry && ! sb::remove("/usr/share/initramfs-tools/scripts/init-bottom/sbnoxconf")) || ! sb::remove("/usr/share/initramfs-tools/scripts/init-bottom/sbfinstall")) return err();
        if(rv) return err(327);
    }

    irblck = false;
    if(! sb::copy("/boot/initrd.img-" % ckernel, sb::sdir[2] % "/.sblivesystemcreate/" % lvtype % "/initrd.gz")) return err();

    if(sb::isfile("/usr/lib/syslinux/isolinux.bin"))
    {
        if(! (sb::copy("/usr/lib/syslinux/isolinux.bin", sb::sdir[2] % "/.sblivesystemcreate/syslinux/isolinux.bin") && sb::copy("/usr/lib/syslinux/vesamenu.c32", sb::sdir[2] % "/.sblivesystemcreate/syslinux/vesamenu.c32"))) return err();
    }
    else if(! (sb::copy("/usr/lib/ISOLINUX/isolinux.bin", sb::sdir[2] % "/.sblivesystemcreate/syslinux/isolinux.bin") && sb::copy("/usr/lib/syslinux/modules/bios/vesamenu.c32", sb::sdir[2] % "/.sblivesystemcreate/syslinux/vesamenu.c32") && sb::copy("/usr/lib/syslinux/modules/bios/libcom32.c32", sb::sdir[2] % "/.sblivesystemcreate/syslinux/libcom32.c32") && sb::copy("/usr/lib/syslinux/modules/bios/libutil.c32", sb::sdir[2] % "/.sblivesystemcreate/syslinux/libutil.c32") && sb::copy("/usr/lib/syslinux/modules/bios/ldlinux.c32", sb::sdir[2] % "/.sblivesystemcreate/syslinux/ldlinux.c32")))
        return err();

    if(! (sb::copy("/usr/share/systemback/splash.png", sb::sdir[2] % "/.sblivesystemcreate/syslinux/splash.png") && sb::lvprpr(ui->userdatainclude->isChecked()))) return err();

    {
        QStr ide;

        for(cQStr &item : {"/.sblvtmp/cdrom", "/.sblvtmp/dev", "/.sblvtmp/mnt", "/.sblvtmp/proc", "/.sblvtmp/run", "/.sblvtmp/srv", "/.sblvtmp/sys", "/.sblvtmp/tmp", "/bin", "/boot", "/etc", "/lib", "/lib32", "/lib64", "/opt", "/sbin", "/selinux", "/snap/.sblvtmp/snap", "/usr", "/initrd.img", "/initrd.img.old", "/vmlinuz", "/vmlinuz.old"})
            if(sb::exist(item)) ide.append(' ' % item);

        if(sb::isdir(sb::sdir[2] % "/.sblivesystemcreate/userdata"))
        {
            ide.append(" \"" % sb::sdir[2] % "\"/.sblivesystemcreate/userdata/home");
            if(sb::isdir(sb::sdir[2] % "/.sblivesystemcreate/userdata/root")) ide.append(" \"" % sb::sdir[2] % "\"/.sblivesystemcreate/userdata/root");
        }
        else
        {
            if(sb::isdir("/home/.sbuserdata")) ide.append(" /home/.sbuserdata/home");
            if(sb::isdir("/root/.sbuserdata")) ide.append(" /root/.sbuserdata/root");
        }

        if(intrrpt) return err();
        pset(18, " 2/3");
        QStr elist;

        for(cQStr &excl : {"/boot/efi/EFI", "/etc/fstab", "/etc/mtab", "/etc/udev/rules.d/70-persistent-cd.rules", "/etc/udev/rules.d/70-persistent-net.rules"})
            if(sb::exist(excl)) elist.append(" -e " % excl);

        for(cQStr &cdir : {"/etc/rc0.d", "/etc/rc1.d", "/etc/rc2.d", "/etc/rc3.d", "/etc/rc4.d", "/etc/rc5.d", "/etc/rc6.d", "/etc/rcS.d"})
            if(sb::isdir(cdir))
                for(cQStr &item : QDir(cdir).entryList(QDir::Files))
                    if(item.contains("cryptdisks")) elist.append(" -e " % cdir % '/' % item);

        if(sb::exec("mksquashfs" % ide % " \"" % sb::sdir[2] % "\"/.sblivesystemcreate/.systemback /media/.sblvtmp/media /var/.sblvtmp/var \"" % sb::sdir[2] % "\"/.sblivesystemcreate/" % lvtype % "/filesystem.squashfs " % (sb::xzcmpr ? "-comp xz " : nullptr) % "-info -b 1M -no-duplicates -no-recovery -always-use-fragments" % elist, sb::Prgrss)) return err(311);
    }

    pset(19, " 3/3"),
    sb::Progress = -1;
    for(cQStr &dir : {"/.sblvtmp", "/media/.sblvtmp", "/var/.sblvtmp"}) sb::remove(dir);

    for(cQStr &dir : {"/home/.sbuserdata", "/root/.sbuserdata", "/snap/.sblvtmp"})
        if(sb::isdir(dir)) sb::remove(dir);

    if(intrrpt) return err();

    {
        QStr rpart, grxorg, srxorg, prmtrs;
        if(sb::fsize(sb::sdir[2] % "/.sblivesystemcreate/" % lvtype % "/filesystem.squashfs") > 4294967295) rpart = "root=LABEL=SBROOT ";

        if(sb::isfile("/etc/default/grub"))
        {
            QFile file("/etc/default/grub");

            if(sb::fopen(file))
                while(! file.atEnd())
                {
                    QStr cline(file.readLine().trimmed());

                    if(cline.startsWith("GRUB_CMDLINE_LINUX_DEFAULT=") && cline.count('\"') > 1)
                    {
                        if(! prmtrs.isEmpty()) prmtrs.clear();
                        QStr pprt;

                        for(cQStr &cprmtr : sb::left(sb::right(cline, -sb::instr(cline, "\"")), -1).split(' '))
                            if(! (cprmtr.isEmpty() || (pprt.isEmpty() && sb::like(cprmtr, {"_quiet_", "_splash_", "_xforcevesa_"}))))
                            {
                                if(cprmtr.contains("\\\""))
                                {
                                    if(pprt.isEmpty())
                                        pprt = cprmtr % ' ';
                                    else
                                        prmtrs.append(' ' % pprt.append(cprmtr).replace("\\\"", "\"")),
                                        pprt.clear();
                                }
                                else if(pprt.isEmpty())
                                    prmtrs.append(' ' % cprmtr);
                                else
                                    pprt.append(cprmtr % ' ');
                            }
                    }
                }
        }

        if(xmntry) grxorg = "menuentry \"" % tr("Boot Live without xorg.conf file") % "\" {\n  set gfxpayload=keep\n  linux /" % lvtype % "/vmlinuz " % rpart % "boot=" % lvtype % " noxconf quiet splash" % prmtrs % "\n  initrd /" % lvtype % "/initrd.gz\n}\n\n", srxorg = "label noxconf\n  menu label " % tr("Boot Live without xorg.conf file") % "\n  kernel /" % lvtype % "/vmlinuz\n  append " % rpart % "boot=" % lvtype % " initrd=/" % lvtype % "/initrd.gz noxconf quiet splash" % prmtrs % "\n\n";
#ifdef __amd64__
        if(sb::isfile("/usr/share/systemback/efi-amd64.bootfiles") && (sb::exec("tar -xJf /usr/share/systemback/efi-amd64.bootfiles -C \"" % sb::sdir[2] % "\"/.sblivesystemcreate --no-same-owner --no-same-permissions") || ! (sb::copy("/usr/share/systemback/splash.png", sb::sdir[2] % "/.sblivesystemcreate/boot/grub/splash.png") &&
            sb::crtfile(sb::sdir[2] % "/.sblivesystemcreate/boot/grub/grub.cfg", "if loadfont /boot/grub/font.pf2\nthen\n  set gfxmode=auto\n  insmod efi_gop\n  insmod efi_uga\n  insmod gfxterm\n  terminal_output gfxterm\nfi\n\nset theme=/boot/grub/theme.cfg\n\nmenuentry \"" % tr("Boot Live system") % "\" {\n  set gfxpayload=keep\n  linux /" % lvtype % "/vmlinuz " % rpart % "boot=" % lvtype % " quiet splash" % prmtrs % "\n  initrd /" % lvtype % "/initrd.gz\n}\n\nmenuentry \"" % tr("Boot system installer") % "\" {\n  set gfxpayload=keep\n  linux /" % lvtype % "/vmlinuz " % rpart % "boot=" % lvtype % " finstall quiet splash" % prmtrs % "\n  initrd /" % lvtype % "/initrd.gz\n}\n\nmenuentry \"" % tr("Boot Live in safe graphics mode") % "\" {\n  set gfxpayload=keep\n  linux /" % lvtype % "/vmlinuz " % rpart % "boot=" % lvtype % " xforcevesa nomodeset quiet splash" % prmtrs % "\n  initrd /" % lvtype % "/initrd.gz\n}\n\n" % grxorg % "menuentry \"" % tr("Boot Live in debug mode") % "\" {\n  set gfxpayload=keep\n  linux /" % lvtype % "/vmlinuz " % rpart % "boot=" % lvtype % prmtrs % "\n  initrd /" % lvtype % "/initrd.gz\n}\n") &&
            sb::crtfile(sb::sdir[2] % "/.sblivesystemcreate/boot/grub/theme.cfg", "title-color: \"white\"\ntitle-text: \"Systemback Live (" % ifname % ")\"\ntitle-font: \"Sans Regular 16\"\ndesktop-color: \"black\"\ndesktop-image: \"/boot/grub/splash.png\"\nmessage-color: \"white\"\nmessage-bg-color: \"black\"\nterminal-font: \"Sans Regular 12\"\n\n+ boot_menu {\n  top = 150\n  left = 15%\n  width = 75%\n  height = " % (xmntry ? "150" : "130") % "\n  item_font = \"Sans Regular 12\"\n  item_color = \"grey\"\n  selected_item_color = \"white\"\n  item_height = 20\n  item_padding = 15\n  item_spacing = 5\n}\n\n+ vbox {\n  top = 100%\n  left = 2%\n  + label {text = \"" % tr("Press 'E' key to edit") % "\" font = \"Sans 10\" color = \"white\" align = \"left\"}\n}\n") &&
            sb::crtfile(sb::sdir[2] % "/.sblivesystemcreate/boot/grub/loopback.cfg", "menuentry \"" % tr("Boot Live system") % "\" {\n  set gfxpayload=keep\n  linux /" % lvtype % "/vmlinuz " % rpart % "boot=" % lvtype % " iso-scan/filename=$iso_path quiet splash" % prmtrs % "\n  initrd /" % lvtype % "/initrd.gz\n}\n\nmenuentry \"" % tr("Boot system installer") % "\" {\n  set gfxpayload=keep\n  linux /" % lvtype % "/vmlinuz " % rpart % "boot=" % lvtype % " iso-scan/filename=$iso_path finstall quiet splash" % prmtrs % "\n  initrd /" % lvtype % "/initrd.gz\n}\n\nmenuentry \"" % tr("Boot Live in safe graphics mode") % "\" {\n  set gfxpayload=keep\n  linux /" % lvtype % "/vmlinuz " % rpart % "boot=" % lvtype % " iso-scan/filename=$iso_path xforcevesa nomodeset quiet splash" % prmtrs % "\n  initrd /" % lvtype % "/initrd.gz\n}\n\n" % grxorg % "menuentry \"" % tr("Boot Live in debug mode") % "\" {\n  set gfxpayload=keep\n  linux /" % lvtype % "/vmlinuz " % rpart % "boot=" % lvtype % " iso-scan/filename=$iso_path" % prmtrs % "\n  initrd /" % lvtype % "/initrd.gz\n}\n")))) return err();
#endif
        if(! sb::crtfile(sb::sdir[2] % "/.sblivesystemcreate/syslinux/syslinux.cfg", "default vesamenu.c32\nprompt 0\ntimeout 100\n\nmenu title Systemback Live (" % ifname % ")\nmenu tabmsg " % tr("Press TAB key to edit") % "\nmenu background splash.png\n\nlabel live\n  menu label " % tr("Boot Live system") % "\n  kernel /" % lvtype % "/vmlinuz\n  append " % rpart % "boot=" % lvtype % " initrd=/" % lvtype % "/initrd.gz quiet splash" % prmtrs % "\n\nlabel install\n  menu label " % tr("Boot system installer") % "\n  kernel /" % lvtype % "/vmlinuz\n  append " % rpart % "boot=" % lvtype % " initrd=/" % lvtype % "/initrd.gz finstall quiet splash" % prmtrs % "\n\nlabel safe\n  menu label " % tr("Boot Live in safe graphics mode") % "\n  kernel /" % lvtype % "/vmlinuz\n  append " % rpart % "boot=" % lvtype % " initrd=/" % lvtype % "/initrd.gz xforcevesa nomodeset quiet splash" % prmtrs % "\n\n" % srxorg % "label debug\n  menu label " % tr("Boot Live in debug mode") % "\n  kernel /" % lvtype % "/vmlinuz\n  append " % rpart % "boot=" % lvtype % " initrd=/" % lvtype % "/initrd.gz" % prmtrs % '\n')
            || intrrpt || ! sb::remove(sb::sdir[2] % "/.sblivesystemcreate/.systemback")
            || intrrpt || (sb::isdir(sb::sdir[2] % "/.sblivesystemcreate/userdata") && ! sb::remove(sb::sdir[2] % "/.sblivesystemcreate/userdata"))
            || intrrpt) return err();
    }

    if(sb::ThrdLng[0]) sb::ThrdLng[0] = 0;
    sb::ThrdStr[0] = sb::sdir[2] % '/' % ifname % ".sblive";
    ui->progressbar->setValue(0);

    if(sb::exec("tar -cf \"" % sb::sdir[2] % "\"/" % ifname % ".sblive -C \"" % sb::sdir[2] % "\"/.sblivesystemcreate .", sb::Prgrss))
    {
        if(sb::exist(sb::sdir[2] % '/' % ifname % ".sblive")) sb::remove(sb::sdir[2] % '/' % ifname % ".sblive");
        return err(312);
    }

    if(! cfmod(sb::sdir[2] % '/' % ifname % ".sblive", 0666)) return err();

    if(sb::autoiso)
    {
        ullong isize(sb::fsize(sb::sdir[2] % '/' % ifname % ".sblive"));

        if(isize < 4294967295 && isize + 52428800 < sb::dfree(sb::sdir[2]))
        {
            pset(20, " 4/3+1"),
            sb::Progress = -1;
            if(! (sb::rename(sb::sdir[2] % "/.sblivesystemcreate/syslinux/syslinux.cfg", sb::sdir[2] % "/.sblivesystemcreate/syslinux/isolinux.cfg") && sb::rename(sb::sdir[2] % "/.sblivesystemcreate/syslinux", sb::sdir[2] % "/.sblivesystemcreate/isolinux")) || intrrpt) return err();
            ui->progressbar->setValue(0);

            if(sb::exec("genisoimage -r -V sblive -cache-inodes -J -l -b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table -c isolinux/boot.cat -o \"" % sb::sdir[2] % "\"/" % ifname % ".iso \"" % sb::sdir[2] % "\"/.sblivesystemcreate", sb::Prgrss))
            {
                if(sb::isfile(sb::sdir[2] % '/' % ifname % ".iso")) sb::remove(sb::sdir[2] % '/' % ifname % ".iso");
                return err(312);
            }

            if(sb::exec("isohybrid \"" % sb::sdir[2] % "\"/" % ifname % ".iso") || ! cfmod(sb::sdir[2] % '/' % ifname % ".iso", 0666) || intrrpt) return err();
        }
    }

    emptycache(),
    sb::remove(sb::sdir[2] % "/.sblivesystemcreate"),
    on_livemenu_clicked(),
    dialogopen(207);
}

void systemback::on_liveconvert_clicked()
{
    statustart(), pset(21, " 1/2");
    QStr path(sb::sdir[2] % '/' % sb::left(ui->livelist->currentItem()->text(), sb::instr(ui->livelist->currentItem()->text(), " ") - 1));

    auto err([&](ushort dlg = 0) {
            if(sb::isdir(sb::sdir[2] % "/.sblivesystemconvert")) sb::remove(sb::sdir[2] % "/.sblivesystemconvert");
            if(sb::isfile(path % ".iso")) sb::remove(path % ".iso");

            if(intrrpt)
                intrrpt = false;
            else
                dialogopen(dlg ? dlg : 335);
        });

    if((sb::exist(sb::sdir[2] % "/.sblivesystemconvert") && ! sb::remove(sb::sdir[2] % "/.sblivesystemconvert")) || ! sb::crtdir(sb::sdir[2] % "/.sblivesystemconvert")) return err();
    sb::ThrdLng[0] = sb::fsize(path % ".sblive"), sb::ThrdStr[0] = sb::sdir[2] % "/.sblivesystemconvert";
    if(sb::exec("tar -xf \"" % path % "\".sblive -C \"" % sb::sdir[2] % "\"/.sblivesystemconvert --no-same-owner --no-same-permissions", sb::Prgrss)) return err(326);
    if(! (sb::rename(sb::sdir[2] % "/.sblivesystemconvert/syslinux/syslinux.cfg", sb::sdir[2] % "/.sblivesystemconvert/syslinux/isolinux.cfg") && sb::rename(sb::sdir[2] % "/.sblivesystemconvert/syslinux", sb::sdir[2] % "/.sblivesystemconvert/isolinux")) || intrrpt) return err(324);
    pset(21, " 2/2"),
    sb::Progress = -1,
    ui->progressbar->setValue(0);
    if(sb::exec("genisoimage -r -V sblive -cache-inodes -J -l -b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table -c isolinux/boot.cat -o \"" % path % "\".iso \"" % sb::sdir[2] % "\"/.sblivesystemconvert", sb::Prgrss)) return err(325);
    if(sb::exec("isohybrid \"" % path % "\".iso") || ! cfmod(path % ".iso", 0666)) return err();
    sb::remove(sb::sdir[2] % "/.sblivesystemconvert");
    if(intrrpt) return err();
    emptycache(),
    ui->livelist->currentItem()->setText(sb::left(ui->livelist->currentItem()->text(), sb::rinstr(ui->livelist->currentItem()->text(), " ")) % "sblive+iso)"),
    ui->liveconvert->setDisabled(true),
    ui->statuspanel->hide(),
    ui->mainpanel->show(),
    ui->liveback->setFocus(),
    windowmove(ss(698), ss(465));
}

void systemback::on_partitiondelete_clicked()
{
    busy(), ui->copycover->show();

    switch(ui->partitionsettings->item(ui->partitionsettings->currentItem()->row(), 8)->text().toUShort()) {
    case sb::Extended:
        sb::delpart(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 0)->text());
        break;
    default:
        sb::mkptable(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 0)->text(), grub.isEFI ? "gpt" : "msdos");
    }

    on_partitionrefresh2_clicked(),
    busy(false);
}

void systemback::on_newpartition_clicked()
{
    busy(), ui->copycover->show();
    QStr dev(sb::left(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 0)->text(), (ui->partitionsettings->item(ui->partitionsettings->currentRow(), 0)->text().contains("mmc") ? 12 : 8)));
    bool msize(ui->partitionsize->value() == ui->partitionsize->maximum());
    ullong start(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 9)->text().toULongLong()), len(msize ? ui->partitionsettings->item(ui->partitionsettings->currentRow(), 10)->text().toULongLong() : ullong(ui->partitionsize->value()) * 1048576);
    uchar type;

    switch(ui->partitionsettings->item(ui->partitionsettings->currentRow(), 8)->text().toUShort()) {
    case sb::Freespace:
    {
        uchar pcnt(0), fcnt(0);

        for(ushort a(0) ; a < ui->partitionsettings->rowCount() ; ++a)
            if(ui->partitionsettings->item(a, 0)->text().startsWith(dev))
                switch(ui->partitionsettings->item(a, 8)->text().toUShort()) {
                case sb::GPT:
                case sb::Extended:
                    type = sb::Primary;
                    goto exec;
                case sb::Primary:
                    ++pcnt;
                    break;
                case sb::Freespace:
                    ++fcnt;
                }

        if(pcnt < 3 || (fcnt == 1 && ui->partitionsize->value() == ui->partitionsize->maximum()))
        {
            type = sb::Primary;
            break;
        }
        else if(! sb::mkpart(dev, start, ui->partitionsettings->item(ui->partitionsettings->currentRow(), 10)->text().toULongLong(), sb::Extended))
            goto end;

        start += 1048576;
        if(msize && (len -= 1048576) < 1048576) goto end;
    }
    default:
        type = sb::Logical;
    }

exec:
    sb::mkpart(dev, start, len, type);
end:
    on_partitionrefresh2_clicked(),
    busy(false);
}

void systemback::on_languageoverride_clicked(bool chckd)
{
    if(chckd)
    {
        QStr lname(ui->languages->currentText());

        sb::lang = lname == "المصرية العربية" ? "ar_EG"
                 : lname == "Català" ? "ca_ES"
                 : lname == "Čeština" ? "cs_CS"
                 : lname == "Dansk" ? "da_DK"
                 : lname == "Deutsch" ? "de_DE"
                 : lname == "English (common)" ? "en_EN"
                 : lname == "English (United Kingdom)" ? "en_GB"
                 : lname == "Español" ? "es_ES"
                 : lname == "Suomi" ? "fi_FI"
                 : lname == "Français" ? "fr_FR"
                 : lname == "Galego" ? "gl_ES"
                 : lname == "Magyar" ? "hu_HU"
                 : lname == "Bahasa Indonesia" ? "id_ID"
                 : lname == "Português (Brasil)" ? "pt_BR"
                 : lname == "Română" ? "ro_RO"
                 : lname == "Русский" ? "ru_RU"
                 : lname == "Türkçe" ? "tr_TR"
                 : lname == "Українськa" ? "uk_UK" : "zh_CN";

        ui->languages->setEnabled(true);
    }
    else
        sb::lang = "auto",
        ui->languages->setDisabled(true);
}

void systemback::on_languages_currentIndexChanged(cQStr &arg1)
{
    ui->languages->resize(fontMetrics().width(arg1) + ss(30), ui->languages->height());

    if(ui->languages->isEnabled()) sb::lang = arg1 == "المصرية العربية" ? "ar_EG"
                                            : arg1 == "Català" ? "ca_ES"
                                            : arg1 == "Čeština" ? "cs_CS"
                                            : arg1 == "Dansk" ? "da_DK"
                                            : arg1 == "Deutsch" ? "de_DE"
                                            : arg1 == "English (common)" ? "en_EN"
                                            : arg1 == "English (United Kingdom)" ? "en_GB"
                                            : arg1 == "Español" ? "es_ES"
                                            : arg1 == "Suomi" ? "fi_FI"
                                            : arg1 == "Français" ? "fr_FR"
                                            : arg1 == "Galego" ? "gl_ES"
                                            : arg1 == "Magyar" ? "hu_HU"
                                            : arg1 == "Bahasa Indonesia" ? "id_ID"
                                            : arg1 == "Português (Brasil)" ? "pt_BR"
                                            : arg1 == "Română" ? "ro_RO"
                                            : arg1 == "Русский" ? "ru_RU"
                                            : arg1 == "Türkçe" ? "tr_TR"
                                            : arg1 == "Українськa" ? "uk_UK" : "zh_CN";
}

void systemback::on_styleoverride_clicked(bool chckd)
{
    sb::style = chckd ? ui->styles->currentText() : "auto",
    ui->styles->setEnabled(chckd);
}

void systemback::on_styles_currentIndexChanged(cQStr &arg1)
{
    ui->styles->resize(fontMetrics().width(arg1) + ss(30), ui->styles->height());
    if(ui->styles->isEnabled()) sb::style = arg1;
}

void systemback::on_alwaysontop_clicked(bool chckd)
{
    if(chckd)
        setwontop(),
        sb::waot = sb::True;
    else
        sb::waot = sb::False,
        setwontop(false);
}

void systemback::on_incrementaldisable_clicked(bool chckd)
{
    sb::incrmtl = chckd ? sb::False : sb::True;
}

void systemback::on_cachemptydisable_clicked(bool chckd)
{
    sb::ecache = chckd ? sb::False : sb::True;
}

void systemback::on_usexzcompressor_clicked(bool chckd)
{
    sb::xzcmpr = chckd ? sb::True : sb::False;
}

void systemback::on_autoisocreate_clicked(bool chckd)
{
    sb::autoiso = chckd ? sb::True : sb::False;
}

void systemback::on_schedulerdisable_clicked(bool chckd)
{
    if(chckd)
        on_schedulerrefresh_clicked(),
        ui->adduser->setEnabled(true);
    else
    {
        sb::schdlr[1] = "false",
        ui->schedulerusers->setText(nullptr);
        if(ui->adduser->isEnabled()) ui->adduser->setDisabled(true);
    }

    ui->users->setEnabled(chckd),
    ui->schedulerrefresh->setEnabled(chckd);
}

void systemback::on_users_currentIndexChanged(cQStr &arg1)
{
    ui->users->setToolTip(arg1);
}

void systemback::on_adduser_clicked()
{
    if(ui->users->count() == 1)
    {
        if(sb::schdlr[1] != "everyone") sb::schdlr[1] = "everyone",
                                        ui->schedulerusers->setText(tr("Everyone"));

        ui->users->clear(),
        ui->adduser->setDisabled(true);
    }
    else
        sb::schdlr[1] == "everyone" ? sb::schdlr[1] = ':' % ui->users->currentText() : sb::schdlr[1].append(',' % ui->users->currentText()),
        ui->schedulerusers->setText(sb::right(sb::schdlr[1], -1)),
        ui->users->removeItem(ui->users->findText(ui->users->currentText()));
}

void systemback::on_schedulerrefresh_clicked()
{
    busy();

    if(sb::schdlr[1] != "everyone") sb::schdlr[1] = "everyone",
                                    ui->schedulerusers->setText(tr("Everyone"));

    ui->users->count() ? ui->users->clear() : ui->adduser->setEnabled(true),
    ui->users->addItem("root");
    QFile file("/etc/passwd");

    if(sb::fopen(file))
        while(! file.atEnd())
        {
            QStr usr(file.readLine().trimmed());
            if(usr.contains(":/home/") && sb::isdir("/home/" % (usr = sb::left(usr, sb::instr(usr, ":") -1)))) ui->users->addItem(usr);
        }

    busy(false);
}

void systemback::on_grubreinstallrestore_currentIndexChanged(cQStr &arg1)
{
    if(! arg1.isEmpty()) ui->grubreinstallrestore->resize(fontMetrics().width(arg1) + ss(30), ui->grubreinstallrestore->height());
}

void systemback::on_grubinstallcopy_currentIndexChanged(cQStr &arg1)
{
    if(! arg1.isEmpty()) ui->grubinstallcopy->resize(fontMetrics().width(arg1) + ss(30), ui->grubinstallcopy->height());
}

void systemback::on_grubreinstallrepair_currentIndexChanged(cQStr &arg1)
{
    if(! arg1.isEmpty()) ui->grubreinstallrepair->resize(fontMetrics().width(arg1) + ss(30), ui->grubreinstallrepair->height());
}
