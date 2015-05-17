#ifndef ADDSERVER_H
#define ADDSERVER_H

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
#include "addserverwidget.h"
#include "ui_addserver.h"

class AddServer : public QDialog, private Ui::serverDlg
{
    Q_OBJECT

public:
  AddServer(QWidget* parent = 0, Qt::WindowFlags fl = Qt::Dialog );
  AddServer(NntpHost *, QWidget* parent = 0, Qt::WindowFlags fl = Qt::Dialog );
  virtual ~AddServer();

  void newServerReq(NntpHost *);
  void testServerReq(QString,quint16,int,qint16,int);

public slots:
  /*$PROTECTED_SLOTS$*/
  virtual void          reject();
  virtual void          accept();

  signals:
	void newServer(NntpHost*);
    void testServer(QString,quint16,int,qint16,int);

private:
	AddServerWidget* addServerWidget;
};

#endif // ADDSERVER_H
