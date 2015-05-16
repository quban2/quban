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

#ifndef GROUPLIST_H
#define GROUPLIST_H

#include <QTreeWidget>
#include <QMultiHash>
#include <QTabWidget>
#include <QStringList>
#include "newsgroup.h"
#include "common.h"
#include "availablegroups.h"
#include "headerlist.h"

struct Category
{
	QTreeWidgetItem *listItem;
	uint childs; //Number of childs
	QString name; //Category name
};

class QMgr;

class GroupList : public QTreeWidget
{
  Q_OBJECT

private:
   QHash<QString, NewsGroup*> groups;
   Db* groupDb;
   DbEnv* dbenv;
   QString dbPath;
   Servers *servers;

   QString groupsDbName;
   QMultiHash<QString, Category*> categories;

   QMgr* qMgr;

public:
  enum Columns  {Name_Col=0, Unread_Col=1, Total_Col=2, Num_Col=3};
  GroupList(QWidget* parent = 0 );
  virtual ~GroupList();
  QHash<QString, NewsGroup*>* getSubscribedGroups() { return &groups; }
  Db* getGroupDb() {return groupDb;}
  void setDbEnv(DbEnv* dbe) {dbenv=dbe;}
  void setQ(QMgr* _qMgr) { qMgr=_qMgr; }
  void loadGroups(Servers *_servers, DbEnv* dbe=0);
  void setDbPath(QString dbp) {dbPath=dbp;}
  void checkGroups();
  void checkExpiry();
  void checkGrouping();
  void enable(bool);
  void deleteGroup(NewsGroup*);
  void updateCurrent(NewsGroup *ng);
  uchar *getBinHeader(QString index, QString group);
  NewsGroup *getNg(QString group);

  QHash<QString, NewsGroup*> getGroups() { return groups;}

  void updateGroups(AvailableGroups*);

  void checkForDeletedServers();

  bool isAliasUsedForDbName(QString alias);

public slots:
	void slotUpdateSelected();
	void slotUpdateSubscribed();
	void slotUpdateFinished(NewsGroup *);
    void slotAddGroup(AvailableGroup*);
	void slotDeleteAllArticles(quint16);
	void slotGroupProperties();
	void slotGroupManagement();
	void slotModifyServerProperties(QStringList);
	void saveGroup(NewsGroup *ng);
	void slotAddCategory(QString);
	void slotRemoveCategory();
	void slotCompactDbs();
	void slotSaveSettings(NewsGroup*, bool, bool);
	void slotUpdateWOptions(NewsGroup *ng, uint from, uint to);
	void slotUpdateByRange(quint16, NewsGroup *, quint64, quint64);
	void slotExpireNewsgroup(NewsGroup *, uint , uint );
	void slotExecuted();
	void slotUnsubscribe(QString);
	void slotUpdateGroup(QString, QWidget *);
	void slotLimitsFinished(NewsGroup *, NntpHost *, uint , uint , uint );

private slots:

	void slotExecuted(QTreeWidgetItem*);
	void slotSelectionChanged();
	void slotGroupPopup(QTreeWidgetItem *, const QPoint&);
	void slotDeleteSelected();
	void slotZeroSelected();
    void slotNewGroup(QStringList, AvailableGroup*);
	void slotExpanded();
	void slotCollapsed();

signals:
	void openNewsGroup(NewsGroup*);
	void isSelected(bool);
	void updateNewsGroup(NewsGroup*, uint, uint);
	void updateNewsGroup(NewsGroup*, QString);
	void updateNewsGroup(quint16, NewsGroup*, quint64, quint64);
	void expireNewsGroup(NewsGroup*, uint, uint);
	void popupMenu(const QPoint &);
	void popupCatMenu(const QPoint &);
	void activateNewsGroup(HeaderList*);
    void subscribed(AvailableGroup *);
	void unsubscribe(QString);
	void sigHaveGroups(bool);
	void sigCloseView(HeaderList*);
	void sigUnsubscribe(QString);
	void sigNewGroup(NewsGroup*);
	void sigUpdateFinished(NewsGroup *);
	void deleteDeletedServer(quint16);
    void sigGroupHeaders(NewsGroup*);
    void sigCancelBulkDelete(quint64);
};

#endif
