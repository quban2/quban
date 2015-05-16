#ifndef AVAILABLEGROUPS_H_
#define AVAILABLEGROUPS_H_

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

#include <QMap>
#include <QHash>
#include <QAbstractItemModel>
#include <QModelIndex>
#include <QList>
#include <QVariant>
#include "common.h"
#include "nntphost.h"
#include "newsgroup.h"

class AvailableGroups;

class AvailableGroup
{
private:
	QString name;
	bool subscribed;
	QMap<quint16, quint64> articles; // These are max parts - computed by quban as: server HWM - LWM + 1
	QMap<quint16, bool> serverPresence;

public:
        AvailableGroup(char*);
        AvailableGroup(QString, quint16, quint64);
        AvailableGroup() {}
        ~AvailableGroup();
		QString& getName() { return name; }
		void setSubscribed(bool s) { subscribed = s; }
		bool isSubscribed() { return subscribed; }
		quint16 countServerPresence() { return serverPresence.count(); }
		QMap<quint16, bool> getServerPresence() { return serverPresence; }
		bool noServerPresence() { return serverPresence.isEmpty(); }
		bool getServerPresence(quint16 i) { return serverPresence[i]; }
		bool hasServerPresence(quint16 i) { return serverPresence.contains(i); }
		void removeServerPresence(quint16 i) { serverPresence.remove(i); }
		quint64 getArticles(quint16 i) { return articles[i]; }
		char *data();
		quint32 size();
		void addHost(quint16 hostId);
		void setArticles(quint16 hostId, quint64 art) {articles[hostId] = art;}
};

class AvailGroupModel;

class AvailableGroups : public QObject
 {
 	Q_OBJECT

	QWidget *parent;
	Db* groupDb;
	DbEnv *dbEnv;
	Servers *servers;
	bool updating;
    QHash<QString, AvailableGroup*> groupsList;
	QHash<QString, NewsGroup*>* subscribedGroups;
    AvailableGroup *g;

	AvailGroupModel* model;

	quint16 Num_Col;

	public:
		AvailableGroups(DbEnv * dbenv, Db* agroupDb, QHash<QString, NewsGroup*>*, Servers *_servers, QWidget* p = 0);
		virtual ~AvailableGroups();
		AvailGroupModel* loadData();
		AvailGroupModel* reloadData();

		void prepareToClose();
        bool isUpdating () {return updating;}
        void startUpdating() {updating=true;}
        void stopUpdating() {updating=false;}
        Db *getDb() {return groupDb;}
		void subscribe(QString&);
        void getGroups(QHash<QString, AvailableGroup*>*);

	public slots:
		void slotUnsubscribe(QString); // this should really be called Unsubscribed
		void slotDeleteServer(quint16);
		void slotRefreshView();
	signals:
        void subscribe(AvailableGroup *);
	    void sigRefreshView();
};

enum {ag_AvailableGroup_Col=0, ag_Articles_Col=1};


class AvailGroupItem
{
public:
	AvailGroupItem(const QList<QVariant> &data, const QList<QVariant> &icons, bool _subscribed, AvailGroupItem *parent = 0);
    ~AvailGroupItem();

    void appendChild(AvailGroupItem *child);
    void setSubscribed(bool subsVal);
    bool isSubscribed() { return subscribed; }

    AvailGroupItem *child(int row);
    int childCount() const;
    int columnCount() const;
    bool removeChildren(int position, int count);
    QVariant data(int column) const;
    QVariant icon(int column) const;
    int row() const;
    AvailGroupItem *parent();

    QList<AvailGroupItem*>& getChildItems() { return childItems; }
    void setChildItems(QList<AvailGroupItem*>& ag) { childItems = ag; }

    QList<AvailGroupItem*> childItems;
private:

    QList<QVariant> itemData;
    QList<QVariant> iconData;
    AvailGroupItem *parentItem;
	bool subscribed;
};

 class AvailGroupModel : public QAbstractItemModel
 {
     Q_OBJECT

 public:
     AvailGroupModel(Servers *_servers, Db* _db,QObject *parent = 0);
     ~AvailGroupModel();

     void setupTopLevelItem(AvailableGroup*);

     void sort ( int column, Qt::SortOrder order = Qt::AscendingOrder );

     AvailGroupItem *getRootItem() const { return rootItem; }
     AvailGroupItem *getItem(const QModelIndex &index) const;
     QVariant data(const QModelIndex &index, int role) const;
     QVariant rawData(const QModelIndex &index, int role=Qt::DisplayRole) const;
     Qt::ItemFlags flags(const QModelIndex &index) const;
     QVariant headerData(int section, Qt::Orientation orientation,
                         int role = Qt::DisplayRole) const;
     QModelIndex index(int row, int column,
                       const QModelIndex &parent = QModelIndex()) const;
     QModelIndex parent(const QModelIndex &index) const;
     int  rowCount(const QModelIndex &parent = QModelIndex()) const;
     int  fullRowCount(const QModelIndex &parent = QModelIndex()) const;
     int  columnCount(const QModelIndex &parent = QModelIndex()) const;
     bool hasChildren ( const QModelIndex & parent = QModelIndex() ) const;
     bool removeRows(int position, int rows,
                     const QModelIndex &parent = QModelIndex());

 private:
     AvailGroupItem* rootItem;
     Servers*        servers;
	 Db*             db;
	 quint32         displayedArticles;

 signals:
     void sigFilter();
 };


#endif /* AVAILABLEGROUPS_H_ */
