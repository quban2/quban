#ifndef ADDSERVERWIDGET_H
#define ADDSERVERWIDGET_H

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
#include <QtGui/QWidget>
#include <QSslCertificate>
#include "nntphost.h"
#include "ui_addserverwidget.h"

class AddServerWidget : public QWidget, public Ui::AddServerWidget
{
    Q_OBJECT

	NntpHost *host;
    QSslCertificate currentCertificate;

  	QIntValidator *validator;

public:
    AddServerWidget(QWidget* parent = 0);
    AddServerWidget(NntpHost *, QWidget* parent = 0);
    ~AddServerWidget();

    /*$PUBLIC_FUNCTIONS$*/
    void populate(NntpHost* host);

  protected:
    void enableLimits();

  protected slots:
    /*$PROTECTED_SLOTS$*/
    virtual void          reject();
    virtual void          accept();
    void                  slotTestServer();
    void                  slotUpdateServer();
    void                  slotPortChanged(const QString &);
    void                  slotResetDateChanged(const QDate &);
    void                  slotEnableTest();
    void                  slotLoginRequired(bool);
    void                  slotLimitsRequired(bool);

  signals:
    void sigSaveNeeded();

private:
    QWidget*  parent;
    QDateTime olddt;
};

#endif // ADDSERVERWIDGET_H
