#ifndef NZBFORM_H
#define NZBFORM_H

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

#include <QDialog>
#include <QList>
#include <db_cxx.h>
#include "appConfig.h"
#include "common.h"
#include "ui_nzbform.h"

class NzbHeader;

class NzbForm : public QDialog, private Ui::NzbFormDlg
{
    Q_OBJECT

public:
  NzbForm(QList<NzbHeader*> *hList, QString dDir, QString groupName, QString category, QWidget* parent = 0, const char* name = 0,
          Qt::WindowFlags f = 0 );
  virtual ~NzbForm();

protected slots:
  virtual void          reject();
  virtual void          accept();

  void slotSelectionChanged();

  void slotMarkAll();
  void slotUnmarkAll();
  void slotMarkSelected();
  void slotUnmarkSelected();
  void slotMarkRelated();
  void slotSelectDir();
  void slotGroupItems();
  void slotPartGroupItems();

signals:
	void sigDownloadNzbPost(NzbHeader*, bool, bool first, QString dir);
	void sigFinishedNzbPosts(bool, QString);

private:
  QList<NzbHeader*> *headerList;
  QString destDir;
  QString nzbFile;
  bool    groupItems;
  bool    keepOpen;
  Db*     pDb;

  Configuration* config;
};

#endif // NZBFORM_H
