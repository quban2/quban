#ifndef HLISTVIEWITEM_H_
#define HLISTVIEWITEM_H_

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

#include "MultiPartHeader.h"
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QIcon>
#include <QPalette>
#include <QPainter>

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QSortFilterProxyModel>
#include <QItemDelegate>
#include <QList>
#include <QVector>
#include <QVariant>
#include <QDateTime>
#include <QRegExp>
#include "MultiPartHeader.h"
#include "SinglePartHeader.h"
#include "headerGroup.h"

#define GITEM 1002
#define GLITEM 1003

//3 mandatory columns
#define MAN_COL 3

class MultiPartHeader;
class QUpdItem;
class QItem;
class NewsGroup;
class QDateTime;

class HeaderTreeView : public QTreeView
{
    public:
    HeaderTreeView( QWidget * parent = 0 );
};

class RowDelegate : public QItemDelegate
{
    public:
        RowDelegate ( QObject * parent = 0 );
        QWidget* createEditor ( QWidget * parent, const QStyleOptionViewItem & option, const QModelIndex & index ) const;
};

class HeaderTreeModel;

class HeaderTreeItem
{
public:
    HeaderTreeItem(QVector<QVariant>* data, QVector<QIcon*>* icons, QString _msgId, quint16 _status, quint16 _iconKey, bool _complete,
                   quint8 _type, HeaderTreeItem *parent = 0, quint64 totalRows = 0);
    ~HeaderTreeItem();

    QList<HeaderTreeItem*>& getChildren() { return childItems; }
    void appendChild(HeaderTreeItem *child);
    inline quint16 getStatus() { return status; }
    void setStatus(quint16 s, HeaderTreeModel *headerTreeModel);
    inline QString& getMsgId() { return id; }
    inline void setMsgId(QString _msgId) { id = _msgId; }
    inline bool isComplete() { return complete; }
    inline quint16 getType() { return type; }
    inline void setType(quint8 _type) { type = _type; }

    HeaderTreeItem *child(int row);
    quint32 childCount() const;
    int columnCount() const;
    bool removeChildren(int position, int count);
    QVariant data(int column) const;
    QIcon* icon(int column) const;
    int row() const;
    HeaderTreeItem *parent();

    enum { HEADER_GROUP = 1, HEADER, PART };

private:
    QList<HeaderTreeItem*> childItems;
    QVector<QVariant> *itemData;
    QVector<QIcon*>   *iconData;
    HeaderTreeItem *parentItem;
	quint16 status;
    quint16 iconKey;
    QString id;
    quint8 type;
	bool complete;
};

 class HeaderTreeModel : public QAbstractItemModel
 {
     Q_OBJECT

 public:
     HeaderTreeModel(Servers *_servers, Db* _db, Db* _partsDb, Db* _groupsDb, QObject *parent = 0, quint64 totalRows = 0);
     ~HeaderTreeModel();

     void appendRows(MultiPartHeader*, quint32, SinglePartHeader*, quint32);
     void setupTopLevelItem(HeaderBase*);
     void setupTopLevelGroupItem(HeaderGroup* hg);
     void setupChildItems(HeaderTreeItem *);
     void setupChildParts(HeaderTreeItem *);
     void setupChildHeaders(HeaderTreeItem *);

     HeaderTreeItem *getItem(const QModelIndex &index) const;
     QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
     Qt::ItemFlags flags(const QModelIndex &index) const;
     QVariant headerData(int section, Qt::Orientation orientation,
                         int role = Qt::DisplayRole) const;
     QModelIndex index(int row, int column,
                       const QModelIndex &parent = QModelIndex()) const;
     QModelIndex parent(const QModelIndex &index) const;
     int  rowCount(const QModelIndex &parent = QModelIndex()) const;
     quint32  fullRowCount(const QModelIndex &parent = QModelIndex()) const;
     int  columnCount(const QModelIndex &parent = QModelIndex()) const;
     bool hasChildren ( const QModelIndex & parent = QModelIndex() ) const;
     void removeAllRows(const QModelIndex &parent = QModelIndex());
     bool removeRows(int position, int rows,
                     const QModelIndex &parent = QModelIndex());

     quint16 updateFirstIcon(QVector<QIcon*>**, quint32, quint16);

 protected:
    // bool canFetchMore(const QModelIndex &parent) const;
    // void fetchMore(const QModelIndex &parent);

 private:
     HeaderTreeItem* rootItem;
     Servers*        servers;
	 Db*             db;
	 Db*             partsDb;
     Db*             groupsDb;
	 quint32         displayedArticles;
     QMap<quint16, QVector<QIcon*>*> iconMap;
     QList<QString>  fromList;
 };

typedef struct
 {
     QRegExp pattern;
     quint8 filterAction;
 } FilterAction;

 class HeaderSortFilterProxyModel : public QSortFilterProxyModel
 {
     Q_OBJECT

 public:

     HeaderSortFilterProxyModel(bool _showOnlyNew, bool _showOnlyComplete, QObject *parent = 0);

     void setShowOnlyNew(bool b) { showOnlyNew = b; }
     void setShowOnlyComplete(bool b) { showOnlyComplete = b; }
     void setPerformAnd(bool b) { performAnd = b; }

     void clearFilter();
     void setFilterKeyColumns(const QList<qint32> &filterColumns);
     void addFilter(qint32 column, quint8 filterAction);
     void addFilterRegExp(qint32 column, const QRegExp &rx, quint8 filterAction);
     void addFilterFixedString(qint32 column, const QString &pattern, quint8 filterAction);

protected:
     bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const;

private:
     bool showOnlyNew,
          showOnlyComplete,
          performAnd;
     QMap<qint32, FilterAction> m_columnPatterns;
 };


class NGListViewItem: public QTreeWidgetItem
{
	private:
		NewsGroup *ng;
	public:
		NGListViewItem(QTreeWidget *, NewsGroup *);
		NGListViewItem(QTreeWidgetItem *, NewsGroup *);
        NewsGroup* getNg() {return ng;}
		void paintRow(NewsGroup *);
};

class TListViewItem : public QTreeWidgetItem
{
	private:
		friend class ServersList;
		int qId;
		int threadId;

	public:
		TListViewItem(int q, int t, QTreeWidgetItem *p, QTreeWidgetItem *last=0);
};


#endif /* HLISTVIEWITEM_H_ */
