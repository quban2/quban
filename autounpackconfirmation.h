#ifndef AUTOUNPACKCONFIRMATION_H
#define AUTOUNPACKCONFIRMATION_H

/***************************************************************************
     Copyright (C) 2011-2015 by Martin Demet
     quban100@gmail.com

    This file is part of Quban.

    Quban is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Quban is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Quban.  If not, see <http://www.gnu.org/licenses/>.
 ***************************************************************************/

#include <QtGui/QDialog>
#include "unpack.h"
#include "ui_autounpackconfirmation.h"

class Configuration;
class QMgr;

class AutoUnpackConfirmation : public QDialog, private Ui::AutoUnpackConfirmationClass
{
    Q_OBJECT

public:
    AutoUnpackConfirmation(UnpackConfirmation* unpackConfirmation, QString fullNzbFilename, QWidget *parent);
    virtual ~AutoUnpackConfirmation();

protected slots:
    void slotOkClicked();
    void slotCancelClicked();
    void slotToggleClicked();
    void slotSetNameClicked();
    void slotRepairClicked();
    void slotUnpackClicked();

signals:
    void sigUnpackConfirmed(QString fullNzbFilename);
    void sigUnpackCancelled();

private:
    UnpackConfirmation* unpackConfirmation;
    int                 mainRepair;
    int                 mainUnpack;
    QString             fullNzbFilename;
    Configuration*      config;

    QMgr*               qMgr;
};

#endif // AUTOUNPACKCONFIRMATION_H
