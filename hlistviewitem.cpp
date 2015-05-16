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

#include <QDebug>
#include <QtDebug>
#include <QDateTime>
#include <QStringBuilder>
#include <qpainter.h>
#include "appConfig.h"
#include "newsgroup.h"
#include "serverslist.h"
#include "hlistviewitem.h"

static const char *cPatterns[] = {"Subject", "Parts", "KBytes", "From", "Posting Date", "Download Date"};

Configuration* g_config;

QIcon* completeIcon = 0;
QIcon* incompleteIcon = 0;
QIcon* articleNewIcon = 0;
QIcon* articleReadIcon = 0;
QIcon* articleunreadIcon = 0;
QIcon* binaryIcon = 0;
QIcon* tickIcon = 0;
QIcon* crossIcon = 0;
QIcon* skullIcon = 0;

HeaderTreeView::HeaderTreeView(QWidget* p) : QTreeView(p)
{
    setItemDelegate(new RowDelegate(this));
}

RowDelegate::RowDelegate(QObject* p) : QItemDelegate(p)
{
}

QWidget* RowDelegate::createEditor( QWidget* parent, const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
    QWidget* rval = QItemDelegate::createEditor(parent,option,index);
// MD TODO    rval->setReadOnly(true);
    return rval;
}

HeaderTreeItem::HeaderTreeItem(QVector<QVariant>* data, QVector<QIcon*>* icons, QString _msgId, quint16 _status, quint16 _iconKey, bool _complete,
                               quint8 _type, HeaderTreeItem *parent)
{
    parentItem = parent;
    itemData   = data;
    iconData   = icons;
    status     = _status;
    iconKey    = _iconKey;
    complete   = _complete;
    id         = _msgId;
    type       = _type;
}

HeaderTreeItem::~HeaderTreeItem()
{
	qDeleteAll(childItems);
	childItems.clear();
	itemData->clear();
	iconData->clear();
	delete itemData;
    //delete iconData;
}

void HeaderTreeItem::appendChild(HeaderTreeItem *item)
{
	childItems.append(item);
}

void HeaderTreeItem::setStatus(quint16 s, HeaderTreeModel *headerTreeModel)
{
    if (status == s)
        return;

	status = s;

    //qDebug() << "Icondata = " << iconData;
    iconKey = headerTreeModel->updateFirstIcon(&iconData, s, iconKey);
    //qDebug() << "Icondata = " << iconData << "\n";
}

HeaderTreeItem *HeaderTreeItem::child(int row)
{
	return childItems.value(row);
}

quint32 HeaderTreeItem::childCount() const
{
	return (quint32)(childItems.count());
}

bool HeaderTreeItem::removeChildren(int position, int count)
{
    if (position < 0 || position + count > childItems.size())
        return false;

    for (int row = 0; row < count; ++row)
        delete childItems.takeAt(position);

    return true;
}

int HeaderTreeItem::columnCount() const
{
	return itemData->count();
}

QVariant HeaderTreeItem::data(int column) const
{
	return itemData->value(column);
}

QIcon* HeaderTreeItem::icon(int column) const
{
	return iconData->value(column);
}

HeaderTreeItem *HeaderTreeItem::parent()
{
	return parentItem;
}

int HeaderTreeItem::row() const
{
	if (parentItem)
		return parentItem->childItems.indexOf(const_cast<HeaderTreeItem*>(this));

	return 0;
}

///////////////////////////////////////////////////////////////////

HeaderTreeModel::HeaderTreeModel(Servers *_servers, Db* _db,  Db* _partsDb, Db* _groupsDb, QObject *parent)
: QAbstractItemModel(parent), servers(_servers), db(_db), partsDb(_partsDb), groupsDb(_groupsDb)
{
	QVector<QVariant>* rootData = new QVector<QVariant>;
	QVector<QIcon*>* iconData = new QVector<QIcon*>;

	displayedArticles = 0;

	g_config = Configuration::getConfig();

	if (!completeIcon)
	{
		completeIcon      = new QIcon(":/quban/images/hi16-action-icon_binary_complete.png");
		incompleteIcon    = new QIcon(":/quban/images/hi16-action-icon_binary_incomplete.png");
		articleNewIcon    = new QIcon(":/quban/images/hi16-action-icon_article_new.png");
		articleReadIcon   = new QIcon(":/quban/images/hi16-action-icon_article_read.png");
		articleunreadIcon = new QIcon(":/quban/images/hi16-action-icon_article_unread.png");
		binaryIcon        = new QIcon(":/quban/images/hi16-action-icon_binary.png");
		tickIcon          = new QIcon(":/quban/images/fatCow/Tick.png");
		crossIcon         = new QIcon(":/quban/images/fatCow/Cross.png");
		skullIcon         = new QIcon(":/quban/images/fatCow/Scull.png");
	}

    *rootData << tr(cPatterns[CommonDefs::Subj_Col]) << tr(cPatterns[CommonDefs::Parts_Col]) << tr(cPatterns[CommonDefs::KBytes_Col])
         << tr(cPatterns[CommonDefs::From_Col]) << tr(cPatterns[CommonDefs::Date_Col]) << tr(cPatterns[CommonDefs::Download_Col]);

    QByteArray ba;
    const char *c_str;
	Servers::iterator it;
	for (it=servers->begin(); it != servers->end(); ++it)
	{
        if (it.value() && it.value()->getServerType() != NntpHost::dormantServer)
		{
	        ba = it.value()->getName().toLocal8Bit();
	        c_str = ba.data();
		    rootData->append(tr(c_str));
		}
	}

    rootItem = new HeaderTreeItem(rootData, iconData, QString::null, 0, 0, false, 0);
}

HeaderTreeModel::~HeaderTreeModel()
{
	delete rootItem;
    qDeleteAll(iconMap);
}

quint16 HeaderTreeModel::updateFirstIcon(QVector<QIcon*>** iconData, quint32 newStatus, quint16 iconKey)
{
    quint16 newIconKey = iconKey;
    QVector<QIcon*>* newIconData = new QVector<QIcon*>(**iconData);

    // 1 = new
    // 2 = read
    // 3 = downloaded
    // 6 = marked

    //qDebug() << "Old icon key = " << iconKey;

    if (newStatus == HeaderBase::MarkedForDeletion)
    {
        newIconKey += 6;
        (*newIconData)[0] = skullIcon;
    }
    else if (newStatus == HeaderBase::bh_downloaded)
    {
        newIconKey += 3;
        (*newIconData)[0] = binaryIcon;
    }
    else if (newStatus == HeaderBase::bh_read)
    {
        newIconKey += 2;
        (*newIconData)[0] = articleReadIcon;
    }
    else if (newStatus == HeaderBase::bh_new)
    {
        newIconKey += 1;
        (*newIconData)[0] = articleNewIcon;
    }

    // remove the old status from the key

    if ((**iconData)[0] == articleNewIcon)
        newIconKey -= 1;
    else if ((**iconData)[0] == articleReadIcon)
        newIconKey -= 2;
    else if ((**iconData)[0] == binaryIcon)
        newIconKey -= 3;
    else if ((**iconData)[0] == skullIcon)
        newIconKey -= 6;


    //qDebug() << "New icon key = " << newIconKey;

    // iconKey will now hold a value that maps to this vector of icons
    if (iconMap.contains(newIconKey))
    {
        *iconData = iconMap.value(newIconKey);
        delete newIconData;
        //qDebug() << "Retrieved from List\n";
    }
    else
    {
        *iconData = newIconData;
        iconMap.insert(newIconKey,newIconData);
        //qDebug() << "Adding to List\n";
    }

    return newIconKey;
}

int HeaderTreeModel::columnCount(const QModelIndex &parent) const
{
	if (parent.isValid())
		return static_cast<HeaderTreeItem*>(parent.internalPointer())->columnCount();
	else
		return rootItem->columnCount();
}

bool HeaderTreeModel::hasChildren ( const QModelIndex & parent ) const
{
	if (parent.isValid())
		return static_cast<HeaderTreeItem*>(parent.internalPointer())->childCount();
	else
		return rootItem->childCount();
}

void HeaderTreeModel::removeAllRows(const QModelIndex &parent)
{
	if (!parent.isValid())
	    removeRows(0, rootItem->childCount() - 1, parent);
}

bool HeaderTreeModel::removeRows(int position, int rows, const QModelIndex &parent)
{
	HeaderTreeItem *parentItem = getItem(parent);
    bool success = true;

    beginRemoveRows(parent, position, position + rows - 1);
    success = parentItem->removeChildren(position, rows);
    endRemoveRows();

    if (success)
        displayedArticles -= rows;

    return success;
}

HeaderTreeItem *HeaderTreeModel::getItem(const QModelIndex &index) const
{
    if (index.isValid())
    {
    	HeaderTreeItem *item = static_cast<HeaderTreeItem*>(index.internalPointer());
        if (item)
        	return item;
    }

    return rootItem;
}

QVariant HeaderTreeModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid())
		return QVariant();

    if ((quint32)(index.row()) >= rootItem->childCount() || index.row() < 0)
        return QVariant();

	if (role != Qt::DisplayRole && role != Qt::DecorationRole)
		return QVariant();

	HeaderTreeItem *item = static_cast<HeaderTreeItem*>(index.internalPointer());

	if (role == Qt::DisplayRole)
	    return item->data(index.column());
	else
	{
		QIcon *thisIcon = item->icon(index.column());

		if (thisIcon)
		    return QVariant(*thisIcon);
		else
			return QVariant();
	}
}

Qt::ItemFlags HeaderTreeModel::flags(const QModelIndex &index) const
{
	if (!index.isValid())
		return 0;

	return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant HeaderTreeModel::headerData(int section, Qt::Orientation orientation,
		int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
		return rootItem->data(section);

	return QVariant();
}

QModelIndex HeaderTreeModel::index(int row, int column, const QModelIndex &parent) const
{
	if (!hasIndex(row, column, parent))
		return QModelIndex();

	HeaderTreeItem *parentItem;

	if (!parent.isValid())
		parentItem = rootItem;
	else
		parentItem = static_cast<HeaderTreeItem*>(parent.internalPointer());

	HeaderTreeItem *childItem = parentItem->child(row);
	if (childItem)
		return createIndex(row, column, childItem);
	else
		return QModelIndex();
}

QModelIndex HeaderTreeModel::parent(const QModelIndex &index) const
{
	if (!index.isValid())
		return QModelIndex();

	HeaderTreeItem *childItem = static_cast<HeaderTreeItem*>(index.internalPointer());
	HeaderTreeItem *parentItem = childItem->parent();

	if (parentItem == rootItem)
		return QModelIndex();

	return createIndex(parentItem->row(), 0, parentItem);
}

quint32 HeaderTreeModel::fullRowCount(const QModelIndex &parent) const
{
	HeaderTreeItem *parentItem;
	if (parent.column() > 0)
		return 0;

	if (!parent.isValid())
		return rootItem->childCount();
	else
		parentItem = static_cast<HeaderTreeItem*>(parent.internalPointer());

	return parentItem->childCount();
}

int HeaderTreeModel::rowCount(const QModelIndex &parent) const
{
	HeaderTreeItem *parentItem;
	if (parent.column() > 0)
		return 0;

	if (!parent.isValid())
        // return displayedArticles;
        return (int)(rootItem->childCount());  // Switch this with above line for lazy load
	else
		parentItem = static_cast<HeaderTreeItem*>(parent.internalPointer());

	return (int)(parentItem->childCount());
}

void HeaderTreeModel::appendRows(MultiPartHeader* mphList, quint32 mphListCount, SinglePartHeader* sphList, quint32 sphListCount)
{
	beginInsertRows(QModelIndex(), rootItem->childCount(), rootItem->childCount() + mphListCount + sphListCount - 1);
	for (quint32 i=0; i<mphListCount; ++i)
	{
		setupTopLevelItem((HeaderBase*)&(mphList[i]));
		//qDebug() << "mph name: " << ((HeaderBase*)&(mphList[i]))->subj;
	}
	for (quint32 i=0; i<sphListCount; ++i)
	{
		setupTopLevelItem((HeaderBase*)&(sphList[i]));
		//qDebug() << "sph name: " << ((HeaderBase*)&(sphList[i]))->subj;
	}
	endInsertRows();
}

void HeaderTreeModel::setupTopLevelItem(HeaderBase* hb)
{
	static QString stringBuilder;
	QVector<QVariant>* columnData = new QVector<QVariant>;
	QVector<QIcon*>* iconData = new QVector<QIcon*>;
	QColor lineColor; // THIS needs fixing!!!
	bool complete = false;
	quint16 missingParts = 0;
	quint16 totalParts = 0;
	QString msgId;
    quint16 iconKey = 0;

    totalParts = hb->getParts();
    missingParts = hb->getMissingParts();
    msgId = hb->getMessageId();

    if (hb->getStatusIgnoreMark()==HeaderBase::bh_new)
    {
		*iconData << articleNewIcon;
        iconKey += 1;
    }
    else if (hb->getStatusIgnoreMark()==HeaderBase::bh_read)
    {
		*iconData <<  articleReadIcon;
        iconKey += 2;
    }
    else if (hb->getStatusIgnoreMark()==HeaderBase::bh_downloaded)
    {
		*iconData <<  binaryIcon;
        iconKey += 3;
    }

	*columnData << hb->getSubj();

	if (missingParts == 0)
	{
		lineColor=Qt::black;
		complete = true;

		stringBuilder = "*/" + QString::number(totalParts);
		*columnData << stringBuilder;
		*iconData <<  completeIcon;
        iconKey += 4;
	}
	else
	{
		lineColor=Qt::red;  // TODO where does this get applied ???
		complete = false;

		stringBuilder = QString::number(totalParts - missingParts) % "/" % QString::number(totalParts);
		*columnData << stringBuilder;
		*iconData <<   incompleteIcon;
        iconKey += 5;
	}

    // iconKey 6 is reserved for skull

    *columnData <<  hb->getSize()/1024;
    *iconData << 0;

	*columnData <<  hb->getFrom();
	*iconData << 0;

    QDateTime qdt;
    qdt.setTime_t(hb->getPostingDate());
    *columnData << qdt;
	*iconData << 0;

    qdt.setTime_t(hb->getDownloadDate());
    *columnData << qdt;
	*iconData << 0;

    int i = 0;

	if (servers != NULL)
	{
		QMap<quint16, NntpHost*>::iterator it;
		for (it=servers->begin(); it != servers->end(); ++it)
		{
            ++i;

            if (it.value() && it.value()->getServerType() != NntpHost::dormantServer)
            {
                if (hb->isServerPresent(it.key()))
                {
                    if (hb->getServerParts(it.key()) == totalParts)
                    {
                        //qDebug() << "Server " << it.key() << " has all parts for this article";
                        *columnData << QVariant();
                        *iconData << tickIcon;
                        iconKey += (30 + i);
                    }
                    else
                    {
                        //qDebug() << "Server " << it.key() << " has " << hb->getServerParts(it.key()) << " parts of " << totalParts;
                        *columnData << hb->getServerParts(it.key());
                        *iconData << 0;
                    }
                }
                else
                {
                    //qDebug() << "Server " << it.key() << " has no link to this article";
                    *columnData << QVariant();
                    *iconData << crossIcon;
                    iconKey += (15 + i);
                }
            }
		}
	}

    // iconKey will now hold a value that maps to this vector of icons
    if (iconMap.contains(iconKey))
    {
        delete iconData;
        iconData = iconMap.value(iconKey);
        //qDebug() << "Re-using icons !!";
    }
    else
        iconMap.insert(iconKey,iconData);

	// Append a new item to the root item's list of children.
	rootItem->appendChild(new HeaderTreeItem(columnData, iconData, msgId,
            hb->getStatusIgnoreMark(), iconKey, complete, HeaderTreeItem::HEADER, rootItem));
}

void HeaderTreeModel::setupTopLevelGroupItem(HeaderGroup* hg)
{
    QVector<QVariant>* columnData = new QVector<QVariant>;
    QVector<QIcon*>* iconData = new QVector<QIcon*>;
    QString matchId;
    quint16 iconKey = 0;

    matchId = hg->getMatch();

    if (hg->getStatus()==HeaderBase::bh_new)
    {
        *iconData << articleNewIcon;
        iconKey += 1;
    }
    else if (hg->getStatus()==HeaderBase::bh_read)
    {
        *iconData <<  articleReadIcon;
        iconKey += 2;
    }
    else if (hg->getStatus()==HeaderBase::bh_downloaded)
    {
        *iconData <<  binaryIcon;
        iconKey += 3;
    }

    *columnData << hg->getDisplayName();

    *iconData << 0;
    *columnData << hg->getMphKeyCount() + hg->getSphKeyCount();

    // iconKey 6 is reserved for skull

    *columnData <<  QVariant();
    *iconData << 0;

    *columnData <<  hg->getFrom();
    *iconData << 0;

    QDateTime qdt;
    qdt.setTime_t(hg->getPostingDate());
    *columnData << qdt;
    *iconData << 0;

    qdt.setTime_t(hg->getDownloadDate());
    *columnData << qdt;
    *iconData << 0;

    if (servers != NULL)
    {
        QMap<quint16, NntpHost*>::iterator it;
        for (it=servers->begin(); it != servers->end(); ++it)
        {
            if (it.value() && it.value()->getServerType() != NntpHost::dormantServer)
            {
                *columnData << QVariant();
                *iconData << 0;
            }
        }
    }

    // iconKey will now hold a value that maps to this vector of icons
    if (iconMap.contains(iconKey))
    {
        delete iconData;
        iconData = iconMap.value(iconKey);
        //qDebug() << "Re-using icons !!";
    }
    else
        iconMap.insert(iconKey,iconData);

    // Append a new item to the root item's list of children.
    rootItem->appendChild(new HeaderTreeItem(columnData, iconData, matchId,
            hg->getStatus(), iconKey, true, HeaderTreeItem::HEADER_GROUP, rootItem));
}

void HeaderTreeModel::setupChildItems(HeaderTreeItem *item)
{
    if (item->getType() == HeaderTreeItem::HEADER)
        setupChildParts(item);
    else
        setupChildHeaders(item);
}

void HeaderTreeModel::setupChildHeaders(HeaderTreeItem * item)
{
    if (!item->childCount()) // not already expanded
    {
        int ret;
        QString articleIndex = item->data(CommonDefs::Subj_Col).toString() % '\n' % item->data(CommonDefs::From_Col).toString();

        static QString stringBuilder;
        QVector<QVariant>* columnData;
        QVector<QIcon*>* iconData;
        QColor lineColor; // THIS needs fixing!!!
        quint16 missingParts = 0;
        quint16 totalParts = 0;
        QString msgId;
        quint16 iconKey = 0;

        Dbt groupkey;
        Dbt groupdata;
        memset(&groupkey, 0, sizeof(groupkey));
        memset(&groupdata, 0, sizeof(groupdata));
        groupdata.set_flags(DB_DBT_MALLOC);

        QByteArray ba = articleIndex.toLocal8Bit();
        const char *k= ba.constData();
        groupkey.set_data((void*)k);
        groupkey.set_size(articleIndex.length());

        ret=groupsDb->get(NULL, &groupkey, &groupdata, 0);
        if (ret != 0) //key not found
        {
            qDebug() << "Failed to find group with key " << articleIndex;
            return;
        }

        qDebug() << "Found group with key " << articleIndex;

        HeaderGroup *hg=new HeaderGroup(articleIndex.length(), (char*)k, (char*)groupdata.get_data());
        free(groupdata.get_data());

        Dbt key;
        Dbt data;

        memset(&key, 0, sizeof(key));
        memset(&data, 0, sizeof(data));
        data.set_flags(DB_DBT_MALLOC);

        HeaderBase* hb = 0;
        SinglePartHeader* sph= 0;
        MultiPartHeader* mph= 0;

        QList<QString>hbKeys = hg->getSphKeys();
        hbKeys.append(hg->getMphKeys());

        beginInsertRows(index(item->row(), 0), 0, hbKeys.count() -1);

        for (int h=0; h<hbKeys.count(); ++h)
        {
            qDebug() << "Found header with key " << hbKeys.at(h);

            columnData = new QVector<QVariant>;
            iconData = new QVector<QIcon*>;

            ba = hbKeys.at(h).toLocal8Bit();
            k= ba.constData();
            key.set_data((void*)k);
            key.set_size(hbKeys.at(h).size());

            ret=db->get(NULL, &key, &data, 0);
            if (ret != 0) //key not found
            {
                qDebug() << "Failed to find header with key " << hbKeys.at(h);
                return;
            }

            if (*((char *)data.get_data()) == 'm')
            {
                mph = new MultiPartHeader(key.get_size(), (char*)key.get_data(), (char*)data.get_data());
                hb = (HeaderBase*)mph;
                free(data.get_data());
                missingParts = mph->getMissingParts();
                totalParts = mph->getParts();
                msgId = QString::null;
            }
            else if (*((char *)data.get_data()) == 's')
            {
                sph = new SinglePartHeader(key.get_size(), (char*)key.get_data(), (char*)data.get_data());
                hb = (HeaderBase*)sph;
                free(data.get_data());
                missingParts = 0;
                totalParts = 1;
                msgId = sph->getMessageId();
            }
            else
            {
                // What have we found ?????
                qDebug() << "Found unexpected identifier for header : " << (char)*((char *)data.get_data());
                continue;
            }

            if (hb->getStatusIgnoreMark()==HeaderBase::bh_new)
            {
                *iconData << articleNewIcon;
                iconKey += 1;
            }
            else if (hb->getStatusIgnoreMark()==HeaderBase::bh_read)
            {
                *iconData <<  articleReadIcon;
                iconKey += 2;
            }
            else if (hb->getStatusIgnoreMark()==HeaderBase::bh_downloaded)
            {
                *iconData <<  binaryIcon;
                iconKey += 3;
            }

            *columnData << hb->getSubj();

            //qDebug() << "Looking at " << hb->getSubj();


            if (missingParts == 0)
            {
                lineColor=Qt::black;

                stringBuilder = "*/" + QString::number(totalParts);
                *columnData << stringBuilder;
                *iconData <<  completeIcon;
                iconKey += 4;
            }
            else
            {
                lineColor=Qt::red;  // TODO where does this get applied ???

                stringBuilder = QString::number(totalParts - missingParts) % "/" % QString::number(totalParts);
                *columnData << stringBuilder;
                *iconData <<   incompleteIcon;
                iconKey += 5;
            }

            // iconKey 6 is reserved for skull

            *columnData <<  hb->getSize()/1024;
            *iconData << 0;

            *columnData <<  hb->getFrom();
            *iconData << 0;

            QDateTime qdt;
            qdt.setTime_t(hb->getPostingDate());
            *columnData << qdt;
            *iconData << 0;

            qdt.setTime_t(hb->getDownloadDate());
            *columnData << qdt;
            *iconData << 0;

            int j = 0;

            if (servers != NULL)
            {
                QMap<quint16, NntpHost*>::iterator it;
                for (it=servers->begin(); it != servers->end(); ++it)
                {
                    ++j;

                    if (it.value() && it.value()->getServerType() != NntpHost::dormantServer)
                    {
                        if (hb->isServerPresent(it.key()))
                        {
                            if (hb->getServerParts(it.key()) == totalParts)
                            {
                                //qDebug() << "Server " << it.key() << " has all parts for this article";
                                *columnData << QVariant();
                                *iconData << tickIcon;
                                iconKey += (30 + j);
                            }
                            else
                            {
                                //qDebug() << "Server " << it.key() << " has " << hb->getServerParts(it.key()) << " parts of " << totalParts;
                                *columnData << hb->getServerParts(it.key());
                                *iconData << 0;
                            }
                        }
                        else
                        {
                            //qDebug() << "Server " << it.key() << " has no link to this article";
                            *columnData << QVariant();
                            *iconData << crossIcon;
                            iconKey += (15 + j);
                        }
                    }
                }
            }                                 

            // iconKey will now hold a value that maps to this vector of icons
            if (iconMap.contains(iconKey))
            {
                delete iconData;
                iconData = iconMap.value(iconKey);
                //qDebug() << "Re-using icons !!";
            }
            else
                iconMap.insert(iconKey,iconData);

            item->appendChild(new HeaderTreeItem(columnData, iconData, QString::null, hb->getStatusIgnoreMark(), iconKey, 0, HeaderTreeItem::HEADER, item));          

            delete hb;
        }

        endInsertRows();

        delete hg;
    }
}

void HeaderTreeModel::setupChildParts(HeaderTreeItem * item)
{
    if (item->getMsgId() != QString::null) // Silently ignore attempt to expand a single part article
    {
        qDebug() << "Attempt made to expand a single part article";
        return;
    }

    if (!item->childCount()) // not already expanded
    {
        int ret;
        QString articleIndex = item->data(CommonDefs::Subj_Col).toString() % '\n' % item->data(CommonDefs::From_Col).toString();

        Dbt mphkey;
        Dbt mphdata;
        memset(&mphkey, 0, sizeof(mphkey));
        memset(&mphdata, 0, sizeof(mphdata));
        mphdata.set_flags(DB_DBT_MALLOC);

        QByteArray ba = articleIndex.toLocal8Bit();
        const char *k= ba.constData();
        mphkey.set_data((void*)k);
        mphkey.set_size(articleIndex.length());

        ret=db->get(NULL, &mphkey, &mphdata, 0);
        if (ret != 0) //key not found
        {
            qDebug() << "Failed to find header with key " << articleIndex;
            return;
        }

        MultiPartHeader *mph=new MultiPartHeader(articleIndex.length(), (char*)k, (char*)mphdata.get_data());
        free(mphdata.get_data());

        Dbt key;
        Dbt data;
        Dbc *cursorp;
        db_recno_t count;

        memset(&key, 0, sizeof(key));
        memset(&data, 0, sizeof(data));
        data.set_flags(DB_DBT_MALLOC);
        key.set_data(&(mph->multiPartKey));
        key.set_size(sizeof(quint64));

        partsDb->cursor(NULL, &cursorp, 0);

        ret=cursorp->get(&key, &data, DB_SET);

        if (ret != DB_NOTFOUND)
        {
            cursorp->count(&count, 0);
            beginInsertRows(index(item->row(), 0), 0, count -1);

            while (ret != DB_NOTFOUND)
            {
                Header* h = new Header((char*)data.get_data(), (char*)key.get_data());
                free(data.get_data());

                uint pad=1;

                uint temp=mph->getParts();
                // calculate the pad;
                while( temp >= 10)
                {
                    pad++;
                    temp/=10;
                }

                QString s;

                quint16 iconKey = 1000;

                QVector<QVariant>* columnData = new QVector<QVariant>;
                QVector<QIcon*>* iconData = new QVector<QIcon*>;

                s=QString::number(h->getPartNo()).rightJustified(pad,'0');

                *columnData << mph->getSubj() << s + "/" + QString::number(mph->getParts())
                    << QString::number(h->getSize()/1024)
                    << mph->getFrom();

                *columnData << QVariant(); // don't bother with posting date ...
                *columnData << QVariant(); // don't bother with download date ...
                *iconData << 0 << 0 << 0 << 0 << 0 << 0;

                QMap<quint16,NntpHost*>::iterator sit;

                int i = 0;

                for (sit=servers->begin(); sit != servers->end(); ++sit)
                {
                    ++i;

                    if (sit.value()->getServerType() != NntpHost::dormantServer)
                    {
                        if (h->isHeldByServer(sit.key()))
                        {
                            *iconData << tickIcon;
                            iconKey += (50 + i);
                        }
                        else
                        {
                            *iconData << crossIcon;
                            iconKey += (25 + i);
                        }
                    }
                }

                // iconKey will now hold a value that maps to this vector of icons
                if (iconMap.contains(iconKey))
                {
                    delete iconData;
                    iconData = iconMap.value(iconKey);
                    //qDebug() << "Re-using icons !!";
                }
                else
                    iconMap.insert(iconKey,iconData);

                item->appendChild(new HeaderTreeItem(columnData, iconData, QString::null, mph->getStatusIgnoreMark(), iconKey, 0, HeaderTreeItem::PART, item));

                delete h;

                ret = cursorp->get(&key, &data, DB_NEXT_DUP);
            }

            endInsertRows();
        }
        else
        { //Error: what's happened?
            qDebug("Error retrieving key: %d",ret);
            qDebug() << "Key: " << mph->multiPartKey;
        }

        delete mph;
    }
}

/*
bool HeaderTreeModel::canFetchMore(const QModelIndex & ) const
{
    if (displayedArticles < rootItem->childCount())
        return true;
    else
        return false;
}

void HeaderTreeModel::fetchMore(const QModelIndex & )
{
    int remainder = rootItem->childCount() - displayedArticles;
    int itemsToFetch = qMin(100000, remainder);

    beginInsertRows(QModelIndex(), displayedArticles, displayedArticles+itemsToFetch-1);
    displayedArticles += itemsToFetch;
    endInsertRows();

    qDebug() << "Displayed " << displayedArticles << " of " << rootItem->childCount();
}

*/
HeaderSortFilterProxyModel::HeaderSortFilterProxyModel(bool _showOnlyNew, bool _showOnlyComplete, QObject *parent)
    : QSortFilterProxyModel(parent), showOnlyNew(_showOnlyNew), showOnlyComplete(_showOnlyComplete)
{
     m_columnPatterns.clear();
     performAnd = true;
}


void HeaderSortFilterProxyModel::clearFilter()
{
    m_columnPatterns.clear();
}

void HeaderSortFilterProxyModel::setFilterKeyColumns(const QList<qint32> &filterColumns)
{
    m_columnPatterns.clear();

    foreach(qint32 column, filterColumns)
        m_columnPatterns.insert(column, FilterAction());
}

void HeaderSortFilterProxyModel::addFilterRegExp(qint32 column, const QRegExp &rx, quint8 filterAction)
{
    if(!m_columnPatterns.contains(column))
        return;

    FilterAction fullFilterAction;
    fullFilterAction.pattern = rx;
    fullFilterAction.filterAction = filterAction;

    m_columnPatterns[column] = fullFilterAction;
}

void HeaderSortFilterProxyModel::addFilterFixedString(qint32 column, const QString &pattern, quint8 filterAction)
{
    if(!m_columnPatterns.contains(column))
        return;

    FilterAction fullFilterAction;
    QRegExp rx(pattern, Qt::CaseInsensitive, QRegExp::FixedString);
    fullFilterAction.pattern = rx;
    fullFilterAction.filterAction = filterAction;

    m_columnPatterns[column] = fullFilterAction;
}

void HeaderSortFilterProxyModel::addFilter(qint32 column, quint8 filterAction)
{
    if(!m_columnPatterns.contains(column))
        return;

    // This is for READ or NOT_READ
    FilterAction fullFilterAction;
    fullFilterAction.filterAction = filterAction;

    m_columnPatterns[column] = fullFilterAction;
}

bool HeaderSortFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if(m_columnPatterns.isEmpty())
        return true;

    uint status;
    bool complete;
    QModelIndex index;

    bool ret = false;

    index = sourceModel()->index(sourceRow, 0, sourceParent);

    status   =  ((HeaderTreeModel*)sourceModel())->getItem(index)->getStatus();
    complete = ((HeaderTreeModel*)sourceModel())->getItem(index)->isComplete();

    for(QMap<qint32, FilterAction>::const_iterator iter = m_columnPatterns.constBegin();
        iter != m_columnPatterns.constEnd();
        ++iter)
    {        
        index = sourceModel()->index(sourceRow, iter.key(), sourceParent);

        switch (iter.value().filterAction)
        {
            case CommonDefs::CONTAINS:
                ret = (index.data().toString().contains(iter.value().pattern) &&
                       (!showOnlyComplete || complete) &&
                       (!showOnlyNew || (status == MultiPartHeader::bh_new)));
            break;

            case CommonDefs::EQUALS:
                ret = ((iter.value().pattern.exactMatch(index.data().toString())) &&
                   (!showOnlyComplete || complete) &&
                   (!showOnlyNew || (status == MultiPartHeader::bh_new)));
            break;

            case CommonDefs::MORE_THAN:
                ret = (index.data().toString().toDouble() > iter.value().pattern.pattern().toDouble() &&
                   (!showOnlyComplete || complete) &&
                   (!showOnlyNew || (status == MultiPartHeader::bh_new)));
            break;

            case CommonDefs::LESS_THAN:
                ret = (index.data().toString().toDouble() < iter.value().pattern.pattern().toDouble() &&
                   (!showOnlyComplete || complete) &&
                   (!showOnlyNew || (status == MultiPartHeader::bh_new)));
            break;

            case CommonDefs::READ:
                ret = (status != MultiPartHeader::bh_new);
            break;

            case CommonDefs::NOT_READ:
                ret = (status == MultiPartHeader::bh_new);
            break;

            default:
                // Error message ....
                break;
        }

        // These are ANDs
        if(!ret && performAnd)
            return ret;

        // These are ORs
        if(ret && !performAnd)
            return ret;
    }

    return ret;
}

NGListViewItem::NGListViewItem(QTreeWidget *parent, NewsGroup * _ng) : QTreeWidgetItem(parent, GITEM), ng(_ng)
{
	paintRow(_ng);
}

NGListViewItem::NGListViewItem( QTreeWidgetItem *parent, NewsGroup *_ng )
    : QTreeWidgetItem(parent, GITEM) , ng(_ng)
{
	paintRow(_ng);
}

void NGListViewItem::paintRow(NewsGroup *ng)
{
	QFont f=this->font(0);

	if (ng->unreadArticles != 0) //Draw bold text
		f.setBold(true);
	else
		f.setBold(false);

	this->setFont(0, f);
	this->setFont(1, f);
	this->setFont(2, f);

	// TODO	setPixmap(0, BarIcon("icon_newsgroup", KIcon::SizeSmall));
	setText(0, ng->getAlias());
	setText(1, QString("%L1").arg(ng->unreadArticles));
	setText(2, QString("%L1").arg(ng->totalArticles));
}

TListViewItem::TListViewItem( int q, int t, QTreeWidgetItem * p, QTreeWidgetItem *last )
    : QTreeWidgetItem(p, last, ServersList::ThreadItem)
{
	qId=q;
	threadId=t;
	setText(0, "Thread #" + QString::number(t));
	setText(1, "Disconnected");
}

