#ifndef NEWSGROUPDIALOG_H
#define NEWSGROUPDIALOG_H

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
#include <QList>
#include <QStringList>
#include "common.h"
#include "newsgroup.h"
#include "availablegroups.h"
#include "headergrouptestdialog.h"
#include "ui_newsgroupdialog.h"

class Configuration;
class HeaderGroupingWidget;

class NewsGroupDialog : public QDialog, public Ui::NewsGroupDialog
{
    Q_OBJECT

public:
    NewsGroupDialog(QString ngName, QStringList _cats, AvailableGroup *_g, QWidget* parent = 0);
    NewsGroupDialog(NewsGroup *_ng, Servers *_servers, QStringList _cats, QWidget* parent = 0);
    virtual ~NewsGroupDialog();
  	void ExpireClicked();

    QStringList entries;
    QStringList cats;
    Servers *servers;
    NewsGroup *ng;
    AvailableGroup *g;
    bool  shouldBeNamedAsAlias;

    HeaderGroupingWidget* headerGroupingWidget;

    Configuration* config;

public slots:
    void amendGrouping(HeaderGroupingSettings& settings);

  protected slots:
    /*$PROTECTED_SLOTS$*/
    virtual void          reject();
    virtual void          accept();
    void slotNewCat(QString);
  private slots:
  	void selectDir();
  	void aliasChanged(const QString&);
  	void slotAddClicked();
  	void slotToggleDay(bool);
  	void slotToggleDownloadDay(bool);
    void slotGroupHeadersStateChanged(int newState);
    void slotRegroup();
    void slotTestGrouping();
    void help();

  signals:
    void newGroup(QStringList, AvailableGroup*);
  	void updateGroup(QStringList);
  	void addCat(QString);
    void sigGroupHeaders(NewsGroup*);

  private:
  	bool    creating;
  	bool    refreshed;
  	QString savedName;
  	QString savedAlias;
  	QString savedCategory;
  	QString savedSaveDir;
  	int     savedDeleteOlder;
  	int     savedDeleteOlder2;
    bool    savedGroupArticles;
    bool    groupingUpdated;
};

#endif // NEWSGROUPDIALOG_H
