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

#include <common.h>
#include <QDebug>
#include <QtDebug>
#include <QIcon>
#include <QMessageBox>
#include "quban.h"
#include "availablegroups.h"

extern Quban* quban;

static const char *cPatterns[] = {"Newsgroup", "Articles"};

static QIcon* subscribedIcon    = 0;
static QIcon* notSubscribedIcon = 0;
static QIcon* crossIcon = 0;

bool availItemLessThan(const AvailGroupItem* s1, const AvailGroupItem* s2);

static int g_sortColumn;
static Qt::SortOrder g_sortOrder;

AvailableGroup::AvailableGroup(QString _name, quint16 hostId, quint64 _articles)
{
	name=_name;
	subscribed = false;
	serverPresence.insert(hostId, true);
	articles.insert(hostId, _articles);
}

AvailableGroup::AvailableGroup(char *p)
{
    subscribed = false;

    char*i = p;
    i=retrieve(i, name);

    quint16 count;
    quint16 id;
    bool b;
    quint64 artNum;

    memcpy(&count, i, sizeof(quint16));
    i+=sizeof(quint16);
    for (quint16 j = 0; j < count; j++)
    {
        memcpy(&id, i, sizeof(quint16));
        i+=sizeof(quint16);
        memcpy(&b, i, sizeof(bool));
        i+=sizeof(bool);
        serverPresence.insert(id, b);
    }

    memcpy(&count, i, sizeof(quint16));
    i+=sizeof(quint16);

    for (quint16 j = 0; j < count; j++)
    {
        memcpy(&id, i, sizeof(quint16));
        i+=sizeof(quint16);
        memcpy(&artNum, i, sizeof(quint64));
        i+=sizeof(quint64);
        articles.insert(id, artNum);
    }
}

AvailableGroup::~AvailableGroup()
{
    articles.clear();
    serverPresence.clear();
}

quint32 AvailableGroup::size()
{
	return (name.length() + sizeof(quint16)*3 + serverPresence.count()*(sizeof(quint16) + sizeof(bool))
			    + articles.count()*(sizeof(quint16) + sizeof(quint64)));

}

char * AvailableGroup::data()
{
	char *p=new char[size()];
	char *i=p;

	i=insert(name, i);

	//Map count
	quint16 count=serverPresence.count();
	memcpy(i, &count, sizeof(quint16));
	i+=sizeof(quint16);
	quint16 id;
	bool b;
	QMap<quint16, bool>::iterator it;
	for (it = serverPresence.begin(); it != serverPresence.end(); ++it)
	{
		id=it.key();
		b=it.value();
		memcpy(i, &id, sizeof(quint16));
		i+=sizeof(quint16);
		memcpy(i, &b, sizeof(bool));
		i+=sizeof(bool);
	}

	count = articles.count();
	memcpy(i, &count, sizeof(quint16));
	i+=sizeof(quint16);
	quint64 artNumber;
	QMap<quint16, quint64>::iterator aIt;
	for (aIt = articles.begin(); aIt != articles.end(); ++aIt)
	{
		id = aIt.key();
		artNumber = aIt.value();
// 		qDebug() << "id: " << id << " articles: " << artNumber;
		memcpy(i, &id, sizeof(quint16));
		i+=sizeof(quint16);
		memcpy(i, &artNumber, sizeof(quint64));
		i+=sizeof(quint64);
	}

	return p;
}

void AvailableGroup::addHost(quint16 hostId)
{
	serverPresence[hostId]=true;
}

AvailableGroups::AvailableGroups(DbEnv *dbenv, Db* agroupDb, QHash<QString, NewsGroup*>* _subscribedGroups, Servers *_servers, QWidget * p)
    : parent(p), groupDb(agroupDb), dbEnv(dbenv), servers(_servers), subscribedGroups(_subscribedGroups)
{
	int ret;

	if ((ret=agroupDb->open(NULL, "availableGroups", NULL, DB_BTREE, DB_CREATE | DB_THREAD, 0644)) != 0)
		QMessageBox::warning(0, tr("Error"), tr("Unable to open available groups database ..."));

	updating=false;
	model = 0;
	Num_Col = 1;
}

AvailableGroups::~ AvailableGroups( )
{
	qDeleteAll(groupsList);
	groupsList.clear();

	if (subscribedIcon)
	{
        Q_DELETE(subscribedIcon);
        Q_DELETE(notSubscribedIcon);
        Q_DELETE(crossIcon);
	}
}

void AvailableGroups::getGroups(QHash<QString, AvailableGroup*>* aGroupsUpdate)
{
    QHashIterator<QString, AvailableGroup*> it(*aGroupsUpdate);

	while(it.hasNext())
	{
		it.next();

		if (groupsList.contains(it.key()))
		    aGroupsUpdate->insert(it.key(), groupsList.value(it.key()));
	}
}

AvailGroupModel* AvailableGroups::loadData()
{
	if (!model)
	{
	    model = new AvailGroupModel(servers, groupDb,this);

		Dbt *key, *data;
		key=new Dbt;
		data=new Dbt;
		Dbc *cursor;
        AvailableGroup *g;
		if (groupDb->cursor(NULL, &cursor, 0))
			qDebug() << "Error opening cursor";
		int ret;
		quint32 count=0;

		while ((ret=cursor->get(key, data, DB_NEXT)) != DB_NOTFOUND)
		{
            g=new AvailableGroup((char*)data->get_data());

			if (subscribedGroups->contains(g->getName()))
				g->setSubscribed(true);

			model->setupTopLevelItem(g); // add to model

			groupsList.insert(g->getName(), g);

			++count;
		}

		qDebug() << "Available Groups: " << count;
		cursor->close();
        Q_DELETE(key);
        Q_DELETE(data);
	}

	return model;
}

AvailGroupModel* AvailableGroups::reloadData()
{
	qDeleteAll(groupsList);
	groupsList.clear();
    Q_DELETE(model);

	return loadData();
}

void AvailableGroups::prepareToClose( )
{
//	visible = false;
}

void AvailableGroups::slotUnsubscribe(QString name) // this is called from subscribed groups and views
{
	if (groupsList.contains(name))
	{
        AvailableGroup* g = groupsList.value(name);
	    if (g->isSubscribed())
	    {
	    	g->setSubscribed(false);

	    	AvailGroupItem* rootItem = model->getRootItem();
	    	AvailGroupItem *child;

	    	for (int i=0; i<rootItem->childCount(); ++i)
	    	{
	    		child = rootItem->child(i);
	    		if (child->data(0).toString() == name)
	    		{
	    			qDebug() << "Matched!!";
	    			child->setSubscribed(false);
	    			break;
	    		}
	    	}

		    emit sigRefreshView();
	    }
	    else
	    	quban->statusBar()->showMessage(tr("News group is not subscribed to."), 3000);
	}
}

void AvailableGroups::slotDeleteServer(quint16 serverId)
{
	Dbt key, data;
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	Dbc *cursor;
	int ret;
	groupDb->cursor(0, &cursor, DB_WRITECURSOR);

	while ((ret=cursor->get(&key, &data, DB_NEXT)) != DB_NOTFOUND)
	{
        g=new AvailableGroup((char*)data.get_data());

		g->removeServerPresence(serverId);
		//resave the group...
		const char *p=g->data();
		memset(&data, 0, sizeof(data));
		data.set_data((void*)p);
		data.set_size(g->size());
		if (g->noServerPresence())
		{
			ret=cursor->del(0);
			if (ret != 0)
				quban->getLogAlertList()->logMessage(LogAlertList::Error, tr("Cannot delete server ") + servers->value(serverId)->getHostName() + tr(" group record"));

		}
		else if ((ret=cursor->put(&key, &data, DB_CURRENT)) != 0)
			quban->getLogAlertList()->logMessage(LogAlertList::Error, tr("Cannot update group record. Error: ") + dbEnv->strerror(ret));

        Q_DELETE(p);
	}
	cursor->close();

	reloadData();
}

void AvailableGroups::subscribe(QString& name)
{
	if (groupsList.contains(name))
	{
        AvailableGroup* g = groupsList.value(name);
	    if (!g->isSubscribed())
	    {
	    	g->setSubscribed(true);
	        emit subscribe(g);
	    }
	    else
	    	quban->statusBar()->showMessage(tr("News group is already subscribed to."), 3000);
	}
}

void AvailableGroups::slotRefreshView()
{
	emit sigRefreshView(); // received from one view to propagate to all
}

///////////////////////////////////////////////////////////////////////////////////////



AvailGroupItem::AvailGroupItem(const QList<QVariant> &data, const QList<QVariant> &icons, bool _subscribed, AvailGroupItem *parent)
{
	parentItem     = parent;
	itemData       = data;
	iconData       = icons;
	subscribed     = _subscribed;
}

AvailGroupItem::~AvailGroupItem()
{
	qDeleteAll(childItems);
}

void AvailGroupItem::appendChild(AvailGroupItem *item)
{
	childItems.append(item);
}

void AvailGroupItem::setSubscribed(bool subsVal)
{
	subscribed = subsVal;

	if (subscribed)
		iconData[0] = *subscribedIcon;
	else
		iconData[0] = *notSubscribedIcon;
}

AvailGroupItem *AvailGroupItem::child(int row)
{
	return childItems.value(row);
}

int AvailGroupItem::childCount() const
{
	return childItems.count();
}

bool AvailGroupItem::removeChildren(int position, int count)
{
    if (position < 0 || position + count > childItems.size())
        return false;

    AvailGroupItem* item = 0;

    for (int row = 0; row < count; ++row)
    {
        item = childItems.takeAt(position);
        Q_DELETE(item);
    }

    return true;
}

int AvailGroupItem::columnCount() const
{
	return itemData.count();
}

QVariant AvailGroupItem::data(int column) const
{
	return itemData.value(column);
}

QVariant AvailGroupItem::icon(int column) const
{
	return iconData.value(column);
}

AvailGroupItem *AvailGroupItem::parent()
{
	return parentItem;
}

int AvailGroupItem::row() const
{
	if (parentItem)
		return parentItem->childItems.indexOf(const_cast<AvailGroupItem*>(this));

	return 0;
}

AvailGroupModel::AvailGroupModel(Servers *_servers, Db* _db, QObject *parent)
: QAbstractItemModel(parent), servers(_servers), db(_db)
{
	QList<QVariant> rootData;
	QList<QVariant> iconData;

	displayedArticles = 0;

	if (!subscribedIcon)
	{
		subscribedIcon = new QIcon(":/quban/images/fatCow/Newspaper-Add.png");
		notSubscribedIcon = new QIcon(":/quban/images/fatCow/Newspaper.png");
		crossIcon         = new QIcon(":/quban/images/fatCow/Cross.png");
	}

    rootData << tr(cPatterns[ag_AvailableGroup_Col]);

    QByteArray ba;
    const char *c_str;
	Servers::iterator it;
	for (it=servers->begin(); it != servers->end(); ++it)
	{
        if (it.value() && it.value()->getServerType() != NntpHost::dormantServer)
		{
	        ba = it.value()->getName().toLocal8Bit();
	        c_str = ba.data();
		    rootData.append(tr(c_str));
		}
	}

	rootItem = new AvailGroupItem(rootData, iconData, false);
}

AvailGroupModel::~AvailGroupModel()
{
    Q_DELETE(rootItem);
}

int AvailGroupModel::columnCount(const QModelIndex &) const
{
	return rootItem->columnCount();
}

bool AvailGroupModel::hasChildren ( const QModelIndex &) const
{
	return rootItem->childCount();
}

bool AvailGroupModel::removeRows(int position, int rows, const QModelIndex &parent)
{
	AvailGroupItem *parentItem = getItem(parent);
    bool success = true;

    beginRemoveRows(parent, position, position + rows - 1);
    success = parentItem->removeChildren(position, rows);
    endRemoveRows();

    if (success)
        displayedArticles -= rows;

    return success;
}

AvailGroupItem *AvailGroupModel::getItem(const QModelIndex &index) const
{
    if (index.isValid())
    {
    	AvailGroupItem *item = static_cast<AvailGroupItem*>(index.internalPointer());
        if (item)
        	return item;
    }

    return rootItem;
}

QVariant AvailGroupModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid())
		return QVariant();

    if (index.row() >= rootItem->childCount() || index.row() < 0)
        return QVariant();

	if (role != Qt::DisplayRole && role != Qt::DecorationRole)
		return QVariant();

	AvailGroupItem *item = static_cast<AvailGroupItem*>(index.internalPointer());

	if (role == Qt::DisplayRole)
	{
		if (index.column() == 0)
	        return item->data(index.column());
		else
        {
            if (!item->icon(index.column()).isValid()) // doesn't have an icon
                return QString("%L1").arg(item->data(index.column()).toULongLong());
            else
                return QVariant();
        }
	}
	else
		return item->icon(index.column());
}

QVariant AvailGroupModel::rawData(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.row() >= rootItem->childCount() || index.row() < 0)
        return QVariant();

    if (role != Qt::DisplayRole && role != Qt::DecorationRole)
        return QVariant();

    AvailGroupItem *item = static_cast<AvailGroupItem*>(index.internalPointer());

    if (role == Qt::DisplayRole)
    {
        if (index.column() == 0)
            return item->data(index.column());
        else
            return item->data(index.column()).toUInt();
    }
    else
        return item->icon(index.column());
}

Qt::ItemFlags AvailGroupModel::flags(const QModelIndex &index) const
{
	if (!index.isValid())
		return 0;

	return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant AvailGroupModel::headerData(int section, Qt::Orientation orientation,
		int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
		return rootItem->data(section);

	return QVariant();
}

QModelIndex AvailGroupModel::index(int row, int column, const QModelIndex &parent) const
{
	if (!hasIndex(row, column, parent))
		return QModelIndex();

	AvailGroupItem *parentItem;

	parentItem = rootItem;

	AvailGroupItem *childItem = parentItem->child(row);
	if (childItem)
		return createIndex(row, column, childItem);
	else
		return QModelIndex();
}

QModelIndex AvailGroupModel::parent(const QModelIndex &index) const
{
	if (!index.isValid())
		return QModelIndex();

	AvailGroupItem *childItem = static_cast<AvailGroupItem*>(index.internalPointer());
	AvailGroupItem *parentItem = childItem->parent();

	if (parentItem == rootItem)
		return QModelIndex();

	return createIndex(parentItem->row(), 0, parentItem);
}

int AvailGroupModel::fullRowCount(const QModelIndex &parent) const
{
	if (parent.column() > 0)
		return 0;

	return rootItem->childCount();
}

int AvailGroupModel::rowCount(const QModelIndex &parent) const
{
	if (parent.column() > 0)
		return 0;

	return rootItem->childCount();
}

void AvailGroupModel::setupTopLevelItem(AvailableGroup* g)
{
	QList<QVariant> columnData;
	QList<QVariant> iconData;

	columnData << g->getName();

	if (g->isSubscribed())
        iconData << *subscribedIcon;
	else
		iconData << *notSubscribedIcon;

	if (servers != NULL)
	{
		QMap<quint16, NntpHost*>::iterator it;
		for (it=servers->begin(); it != servers->end(); ++it)
		{
            if (!it.value())
                qDebug() << "Got invalid server !!!!!!!!!!!";

            if (it.value() && it.value()->getServerType() != NntpHost::dormantServer)
			{
				if (g->getServerPresence(it.key()) == true)
				{
					//columnData << QString("%L1").arg(g->getArticles(it.key()));
                    columnData << g->getArticles(it.key());
					iconData << QVariant();
				}
				else
				{
					columnData << QVariant();
					iconData << *crossIcon;
				}
			}
		}
	}

	// Append a new item to the root item's list of children.
	rootItem->appendChild(new AvailGroupItem(columnData, iconData, g->isSubscribed(), rootItem));
}

void AvailGroupModel::sort ( int column, Qt::SortOrder order )
{
    emit layoutAboutToBeChanged();

	g_sortColumn = column;
	g_sortOrder = order;
	qSort(rootItem->getChildItems().begin(), rootItem->getChildItems().end(), availItemLessThan);

    emit layoutChanged();
    emit sigFilter();
}

bool availItemLessThan(const AvailGroupItem* s1, const AvailGroupItem* s2)
{
	if (g_sortColumn == 0)
	{
		if (g_sortOrder == Qt::AscendingOrder)
			return s2->data(g_sortColumn).toString() < s1->data(g_sortColumn).toString();
		else
			return s1->data(g_sortColumn).toString() < s2->data(g_sortColumn).toString();
	}
	else
	{
		if (g_sortOrder == Qt::AscendingOrder)
			return s2->data(g_sortColumn).toLongLong() < s1->data(g_sortColumn).toLongLong();
		else
			return s1->data(g_sortColumn).toLongLong() < s2->data(g_sortColumn).toLongLong();
	}
}
