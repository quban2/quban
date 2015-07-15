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

#include "grouplist.h"
#include "MultiPartHeader.h"
#include "quban.h"
#include <qmessagebox.h>
#include <QDebug>
#include <QtDebug>
#include <QList>
#include <QFile>
#include <QStyle>
#include <QApplication>
#include <assert.h>
#include <QProgressDialog>
#include "qmgr.h"
#include "appConfig.h"
#include "qubanDbEnv.h"
#include "newsgroupdialog.h"
#include "newsgroupcontents.h"

extern Quban* quban;

// These are the subscribed groups
// *******************************

GroupList::GroupList( QWidget * parent) : QTreeWidget(parent)
{
	groupsDbName="newsgroups";
	dbenv=0;
	groupDb=0;

	this->setAllColumnsShowFocus(true);
	this->setRootIsDecorated(true);

#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
    this->header()->setMovable(false);
#else
    this->header()->setSectionsMovable(false);
#endif

	connect(this, SIGNAL(itemActivated(QTreeWidgetItem*, int)), this, SLOT(slotExecuted(QTreeWidgetItem*)));
	connect(this, SIGNAL(itemSelectionChanged()), this, SLOT(slotSelectionChanged()));
	connect(this, SIGNAL(itemExpanded(QTreeWidgetItem*)), this, SLOT(slotExpanded()));
	connect(this, SIGNAL(itemCollapsed(QTreeWidgetItem*)), this, SLOT(slotCollapsed()));

// TODO	this->setTextAlignment(1, Qt::AlignRight);
}

GroupList::~GroupList()
{
	qDebug() << "In GroupList::~GroupList";

	qDeleteAll(categories);
	categories.clear();

	qDeleteAll(groups);
	groups.clear();
}

void GroupList::loadGroups(Servers *_servers, DbEnv* dbe)
{
	if (dbe)
		dbenv=dbe;

	servers = _servers;

	int ret;

    QByteArray ba = groupsDbName.toLocal8Bit();
    const char *c_str = ba.data();

	if (!(groupDb = quban->getDbEnv()->openDb(c_str, true, &ret)))
		QMessageBox::warning(0, tr("Error"), tr("Unable to open group database ..."));

	char datamem[DATAMEM_SIZE];
	char keymem[KEYMEM_SIZE];

	Dbt key, data;
	key.set_flags(DB_DBT_USERMEM);
	key.set_ulen(KEYMEM_SIZE);
	key.set_data(keymem);

	data.set_flags(DB_DBT_USERMEM);
	data.set_ulen(DATAMEM_SIZE);
	data.set_data(datamem);
// 	groups.clear();

    Dbc *cursor = 0;
	groupDb->cursor(0, &cursor, 0);

    while((cursor->get(&key, &data, DB_NEXT))==0)
    {
		NewsGroup *tempg=new NewsGroup(dbenv, datamem);

        //qDebug() << "Group " << tempg->getName() << " ignores non sequenced headers " << tempg->areUnsequencedArticlesIgnored();

		groups.insert(tempg->getName(), tempg);
// 		tempg->setListItem(new KListViewItem(this, tempg->getName(), QString::number(tempg->getTotal())));
		//Create the category list...
        Category *cat = 0;
		if ( tempg->getCategory() != "None" ) {
			if ((cat = categories.value(tempg->getCategory() )) != 0) {
				//Category exist, add the item as a child of the folder...
				cat->childs++;
				cat->listItem->setText(2, QString::number(cat->childs));
				tempg->setListItem(new NGListViewItem(cat->listItem, tempg));

			} else {
				//Create category
				cat = new Category;

				cat->childs = 1;
				cat->name=tempg->getCategory();
				cat->listItem= new QTreeWidgetItem((QTreeWidget*)this);
				cat->listItem->setText(0, cat->name);
				cat->listItem->setIcon(0, QIcon(":/quban/images/gnome/32/Gnome-Folder-32.png"));
				cat->listItem->setText(2, QString::number(cat->childs));

// 				cat->listItem->setExpandable(true);
				categories.insert(cat->name, cat);
				//Now create the item
				tempg->setListItem(new NGListViewItem(cat->listItem, tempg));
			}
		}
		else
			tempg->setListItem(new NGListViewItem(this, tempg)); //No category

		checkAndCreateDir(tempg->getSaveDir());
	}
	cursor->close();

	sortByColumn(0, Qt::AscendingOrder);
	resizeColumnToContents(0);
	resizeColumnToContents(1);
	resizeColumnToContents(2);

	if (!groups.isEmpty())
		emit sigHaveGroups(true);
}

void GroupList::checkExpiry()
{
	QString currDate = QDate::currentDate().toString("dd.MMM.yyyy");
	int deleteOlder, deleteOlderDownload;
	Configuration* config = Configuration::getConfig();

	QHash<QString, NewsGroup*>::iterator it;
	for (it = groups.begin(); it != groups.end(); ++it)
	{
		if (config->checkDaysValue > 0 &&
		    QDate::fromString((it.value())->getLastExpiry(), "dd.MMM.yyyy").daysTo(QDate::currentDate()) > config->checkDaysValue)
		{
            quban->getLogEventList()->logEvent(tr("Auto expiring ") + (it.value())->getAlias());
			(it.value())->setLastExpiry(currDate);

			deleteOlder = (it.value())->getDeleteOlderPosting();
			deleteOlderDownload = (it.value())->getDeleteOlderDownload();

			if (deleteOlder != 0)
				slotExpireNewsgroup((it.value()), EXPIREBYPOSTINGDATE, deleteOlder);
			else if (deleteOlderDownload != 0)
				slotExpireNewsgroup((it.value()), EXPIREBYDOWNLOADDATE, deleteOlderDownload);
		}
	}
}

void GroupList::checkGrouping()
{
    QHash<QString, NewsGroup*>::iterator it;
    for (it = groups.begin(); it != groups.end(); ++it)
    {
        if (it.value()->doHeadersNeedGrouping())
        {
            if (it.value()->areHeadersGrouped())
            {
                quban->getLogEventList()->logEvent(tr("Grouping headers for ") + (it.value())->getAlias());
                it.value()->groupHeaders();

                // Launch a bulk job to group headers
                emit sigGroupHeaders(it.value());
            }
            else  // belt and braces
                it.value()->setHeadersNeedGrouping(false);
        }
    }
}

void GroupList::slotExpanded()
{
	resizeColumnToContents(0);
	resizeColumnToContents(1);
	resizeColumnToContents(2);
}

void GroupList::slotCollapsed()
{
	resizeColumnToContents(0);
	resizeColumnToContents(1);
	resizeColumnToContents(2);
}

void GroupList::slotAddGroup(AvailableGroup* g)
{
    qDebug() << "Update group";

	QHashIterator<QString, Category*> git(categories);
	QStringList cats;
	while (git.hasNext())
	{
		git.next();
// 		qDebug("Found category: %s", (const char *) git.value()->getCategory());
		cats.append(git.value()->name);
	}

	NewsGroupDialog *af=new NewsGroupDialog(g->getName(), cats, g, this);
    connect(af, SIGNAL(newGroup(QStringList, AvailableGroup*)), this, SLOT(slotNewGroup(QStringList, AvailableGroup*)));
	connect(af, SIGNAL(updateGroup(QStringList)), this, SLOT(slotModifyServerProperties(QStringList)));
	connect(af, SIGNAL(addCat(QString)), this, SLOT(slotAddCategory(QString)));

	af->show();
}

void GroupList::saveGroup( NewsGroup * ng )
{
	//qDebug("Saving group");
//	qDebug( ) << "Trying to save group\n";
	char datamem[DATAMEM_SIZE];
	char keymem[KEYMEM_SIZE];

	int ret;
	Dbt key, data;
	key.set_data(keymem);
	key.set_flags(DB_DBT_USERMEM);
	key.set_ulen(KEYMEM_SIZE);

	data.set_data(datamem);
	data.set_flags(DB_DBT_USERMEM);
	data.set_ulen(DATAMEM_SIZE);

    (void)ng->data(datamem);

	data.set_size(ng->getRecordSize());

    QByteArray ba = ng->getName().toLocal8Bit();
	const char *i= ba.data();
	memcpy(keymem, i, ng->getName().length());
	key.set_size(ng->getName().length());

	ret=groupDb->put(NULL, &key, &data, 0);
    if (ret!=0)
        quban->getLogAlertList()->logMessage(LogAlertList::Error, tr("Error updating newsgroup ") + ng->getAlias() + " : " + dbenv->strerror(ret));
    else
        quban->getLogEventList()->logEvent(tr("Successfully updated newsgroup ") + ng->getAlias());

	groupDb->sync(0);
}

void GroupList::slotExecuted()
{
	QList<QTreeWidgetItem *>items =	selectedItems();
	if (items.count() == 0)
		return;

	QTreeWidgetItem* item = items[0];

	if ( (item->type() != GITEM
		|| item->isDisabled()))
		return;

	NewsGroup *ng = ((NGListViewItem*) item)->getNg();
	HeaderList *view = ng->getView();
	if ( view == NULL)
		emit openNewsGroup(ng);
	else
		emit activateNewsGroup(view);
}

void GroupList::slotExecuted( QTreeWidgetItem * item)
{
	if ( (item->type() != GITEM
			|| item->isDisabled()))
		return;
	NewsGroup *ng = ((NGListViewItem*) item)->getNg();
	HeaderList *view = ng->getView();
	if ( view == NULL)
		emit openNewsGroup(ng);
	else
		emit activateNewsGroup(view);
}

void GroupList::slotUpdateSelected( )
{
	QList<QTreeWidgetItem*> selection=this->selectedItems();
	QListIterator<QTreeWidgetItem*> it(selection);
    NGListViewItem *selected = 0;

	while (it.hasNext()) {

		QTreeWidgetItem*temp = it.next();

		if (temp->type() == GITEM)
        {
            selected = (NGListViewItem*) temp;

            emit updateNewsGroup(selected->getNg(), 0, 0);
            slotSelectionChanged();
        }
	}
}

void GroupList::slotUpdateSubscribed( )
{
	QHashIterator<QString, NewsGroup*> it(groups);
	while(it.hasNext())
	{
		it.next();
        emit updateNewsGroup(it.value(), 0, 0);
	}
	slotSelectionChanged();
}

void GroupList::slotSelectionChanged( )
{
	QList<QTreeWidgetItem*> selection=this->selectedItems();
	if (selection.isEmpty())
		emit isSelected(false);
	else
		emit isSelected(true);
}

void GroupList::slotGroupPopup( QTreeWidgetItem *item, const QPoint & p)
{
	if (!item)
		return;
	if (item->type() == GITEM)
		emit popupMenu(p);
	else
	{
		QList<QTreeWidgetItem *>selItems = this->selectedItems();
		QListIterator<QTreeWidgetItem *> it(selItems);
		while (it.hasNext())
		    it.next()->setSelected(false);

		item->setSelected(true);
		emit popupCatMenu(p);
	}
}

void GroupList::slotDeleteSelected( )
{
	QList<QTreeWidgetItem*> selection=this->selectedItems();
	QListIterator<QTreeWidgetItem*> it(selection);
    NGListViewItem *selected = 0;

	while (it.hasNext())
	{
		QTreeWidgetItem*temp = it.next();

		if (temp->type() != GITEM )
		{
			continue;
		}
		selected = (NGListViewItem*) temp;
		NewsGroup *ng=selected->getNg() ;

		QString dbQuestion = tr("Do you want to unsubscribe from \n ") + ng->name() + tr(" ?");
		int result = QMessageBox::question(this, tr("Question"), dbQuestion, QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel);

	    switch (result)
	    {
			case QMessageBox::Yes:

				emit sigUnsubscribe(ng->name());

				if (selected->parent() == 0)
				{
                    Q_DELETE(selected);
				}
				else
				{
					int newCatCount = selected->parent()->text(2).toInt() - 1;
					if (newCatCount == 0)
					{
                        QTreeWidgetItem* ptr = selected->parent();
                        Q_DELETE(ptr);
					}
					else
					{
//						qDebug() << "Child index = " << selected->parent()->indexOfChild(selected);
						selected->parent()->setText(2, QString::number(newCatCount));
						selected->parent()->takeChild(selected->parent()->indexOfChild(selected));
					}
				}
				deleteGroup(ng);
				break;
			case QMessageBox::No:
				break;
			case QMessageBox::Cancel:
				break;
		}

		if (result==QMessageBox::Cancel)
			break;

	}

	if (groups.isEmpty())
		emit sigHaveGroups(false);
}

void GroupList::slotUpdateFinished( NewsGroup * ng)
{
// 	assert(ng);
	ng->listItem->setDisabled(false);
	ng->listItem->paintRow(ng);

	saveGroup(ng);

    emit sigUpdateFinished(ng);
}

void GroupList::slotZeroSelected( )
{
    if (QMessageBox::question(this, tr("question"), tr("This action will delete all the articles in the selected groups and reset the articles count. Do you want to continue?")) == QMessageBox::No )
		return;
	QList<QTreeWidgetItem*> selection=this->selectedItems();
	QListIterator<QTreeWidgetItem*> it(selection);
    NGListViewItem *selected = 0;

	while (it.hasNext())
	{
		QTreeWidgetItem*temp = it.next();

		if (temp->type() != GITEM) {
			//qDebug("No group selected");
			qDebug() << "No group selected";
			continue;
		}
		selected= (NGListViewItem*) temp;
		NewsGroup *ng=selected->getNg();
		uint *count=new uint;

		ng->getDb()->truncate(NULL, count, 0);
		qDebug() << "Truncated " << *count << " record(s)";

		Servers::iterator it;

		for (it =servers->begin(); it != servers->end(); ++it)
		{
			ng->servLocalParts.insert(it.key(), 0);
		}

		ng->totalArticles=0;
		ng->unreadArticles=0;

// 		ng->listItem->setText(Unread_Col, QString::number(ng->totalArticles));
// 		ng->listItem->setText(Total_Col, QString::number(ng->unreadArticles));
		slotSaveSettings(ng,  ng->onlyUnread(), ng->onlyComplete());
// 		saveGroup(ng);
        Q_DELETE(count);
	}
}

void GroupList::enable( bool b)
{
	if (b)
		setCursor(Qt::ArrowCursor);
	else
		setCursor(Qt::WaitCursor);

// 	this->setEnabled(b);
// 	setEnabled(b);
}

void GroupList::checkForDeletedServers()
{
	bool stillLinkedIn = false;

	Servers::iterator sit;
	for (sit=servers->begin(); sit != servers->end(); ++sit)
	{
		if (sit.value()->getDeleted())
		{
			stillLinkedIn = false;

			QHashIterator<QString, NewsGroup*> it(groups);
			while (it.hasNext())
			{
				it.next();
				NewsGroup *ng=it.value();

				if ((ng->serverPresence.contains(sit.key())) && (ng->serverPresence[sit.key()] == true))
				{
					stillLinkedIn = true;
				}
			}

			if (stillLinkedIn)
				slotDeleteAllArticles(sit.key());
			else // finished with this server now
			{
				emit deleteDeletedServer((quint16)sit.key());
			}
		}
	}






	//EXPIRETEST
	//slotDeleteAllArticles(4);


}

void GroupList::slotDeleteAllArticles( quint16 serverId)
{
	QHashIterator<QString, NewsGroup*> it(groups);
	while (it.hasNext())
	{
		it.next();
		NewsGroup *ng=it.value();



		//slotExpireNewsgroup(ng, EXPIRETEST, (uint)serverId);
		//continue;



		//Don't need to expire if the server doesn't have the group
		if ((ng->serverPresence.contains(serverId)) && (ng->serverPresence[serverId] == true))
		{
			//if it's the only server that carries the group, unsubscribe from group...
			QMap<quint16, bool>::iterator hit;
			for (hit=ng->serverPresence.begin(); hit!=ng->serverPresence.end(); ++hit)
			{
				if (ng->serverPresence[hit.key()] == true &&
					servers->value(hit.key())->getDeleted() == false) // ignore deleted servers
					break;
			}

			if (hit == ng->serverPresence.end()) // reached the end of the list->all other server don't carry the group
			{
				qDebug() << "Unsubscribing from " << ng->ngName;
				deleteGroup(ng);
			}
			else
			{
				slotExpireNewsgroup(ng, EXPIREALLFORSERVER, (uint)serverId);
				qDebug("all articles being deleted");
			}
		}
	}
}

void GroupList::slotUpdateGroup(QString ngName, QWidget *parent)
{
	if (groups.contains(ngName) == false)
		return;

	NewsGroup *ng = getNg(ngName);

	QStringList cats;
	//Iterate on the categories and create a QStringList to load in the combobox...
	QHashIterator<QString, Category*> git(categories);
	while (git.hasNext())
	{
		git.next();
// 		qDebug("Found category: %s", (const char *) git.value()->getCategory());
		cats.append(git.value()->name);
	}

	NewsGroupDialog *af=new NewsGroupDialog(ng, servers, cats, parent);
	connect(af, SIGNAL(updateGroup(QStringList)), this, SLOT(slotModifyServerProperties(QStringList)));
	connect(af, SIGNAL(addCat(QString )), this, SLOT(slotAddCategory(QString )));

	connect(qMgr, SIGNAL(sigLimitsUpdate(NewsGroup *, NntpHost *, uint, uint, uint)), af,
			      SLOT(slotLimitsUpdate(NewsGroup *, NntpHost *, uint, uint, uint)));
	connect(qMgr, SIGNAL(sigHeaderDownloadProgress(NewsGroup *, NntpHost *, quint64, quint64, quint64)), af,
			      SLOT(slotHeaderDownloadProgress(NewsGroup *, NntpHost *, quint64, quint64, quint64)));
	connect(this, SIGNAL(sigUpdateFinished(NewsGroup *)), af, SLOT(slotUpdateFinished(NewsGroup *)));

	af->show();
}

void GroupList::updateGroups(AvailableGroups* aGroups) // this is called following full refresh of available groups to make sure that server presence is correct
{
	QHashIterator<QString, NewsGroup*> it(groups);
    QHash<QString, AvailableGroup*> aGroupsUpdate;
    NewsGroup* ng = 0;

	while(it.hasNext())
	{
		it.next();
        aGroupsUpdate.insert(it.key(), (AvailableGroup*)0);
	}

	// Send the map to aGroups for them to add the group details
	aGroups->getGroups(&aGroupsUpdate);

	QMap<quint16, bool> groupServerPresence;
	QMapIterator<quint16, bool> sit(groupServerPresence);
    QHashIterator<QString, AvailableGroup*> git(aGroupsUpdate);

	while(git.hasNext())
	{
		git.next();

		if (groups.contains(git.key()) && git.value())
		{
			ng = groups.value(git.key());

			groupServerPresence = git.value()->getServerPresence();
			sit = groupServerPresence;

			while (sit.hasNext())
			{
				sit.next();
				ng->serverPresence.insert(sit.key(), sit.value());
			}

			saveGroup(ng);
		}
	}

	aGroupsUpdate.clear();
	groupServerPresence.clear();
}

void GroupList::slotLimitsFinished(NewsGroup *ng, NntpHost *nh, uint lowWater, uint highWater, uint articleParts)
{
	qDebug() << "In GroupList::slotLimitsFinished : " << nh->getId() << ", " << lowWater
			<< ", " << highWater << ", " << articleParts;
	ng->low.insert(nh->getId(), lowWater);
	ng->high.insert(nh->getId(), highWater);
	ng->serverParts.insert(nh->getId(), articleParts);
	ng->serverRefresh.insert(nh->getId(), QDate::currentDate().toString("dd.MMM.yyyy"));

	//groups.insert(ng->getName(), ng);

	saveGroup(ng);
}

void GroupList::slotUnsubscribe(QString ngName) // called externally, so only know the name (not the position)
{
	if (groups.contains(ngName) == false)
		return;

	NewsGroup *ng=getNg(ngName);
    NGListViewItem *selected = 0;

	for (int i=0; i<this->topLevelItemCount(); ++i)
	{
		if (this->topLevelItem(i)->type() == GITEM)
		{
			selected = (NGListViewItem*)this->topLevelItem(i);
		    if (selected->getNg()->getName() == ngName)
		    {
                Q_DELETE(selected);
		    	break;
		    }
		}
		else
		{
			for (int c=0; c<this->topLevelItem(i)->childCount(); ++c)
			{
				selected = (NGListViewItem*)this->topLevelItem(i)->child(c);
			    if (selected->getNg()->ngName == ngName)
			    {
					int newCatCount = selected->parent()->text(2).toInt() - 1;
					if (newCatCount == 0)
					{
                        QTreeWidgetItem* ptr = selected->parent();
                        Q_DELETE(ptr);
					}
					else
					{
		//						qDebug() << "Child index = " << selected->parent()->indexOfChild(selected);
						selected->parent()->setText(2, QString::number(newCatCount));
						selected->parent()->takeChild(selected->parent()->indexOfChild(selected));
					}
			    	break;
			    }
			}
		}
	}

	deleteGroup(ng);

	if (groups.isEmpty())
		emit sigHaveGroups(false);
}

void GroupList::deleteGroup( NewsGroup *ng )
{
	QString dbName=ng->ngName;
	QString groupName=ng->ngName;
	if (ng->getView())
		emit sigCloseView(ng->getView());

	groups.remove(groupName);

	//Delete from db
	Dbt key;
	char *keymem=new char[dbName.length()];
	key.set_data(keymem);
	key.set_flags(DB_DBT_USERMEM);
	QByteArray ba = dbName.toLocal8Bit();
	const char *p=ba.data();
	memcpy(keymem, p, dbName.length());
	key.set_size(dbName.length());
	key.set_ulen(dbName.length());
	int ret=groupDb->del(NULL, &key, 0);
	if (ret != 0)
		qDebug("Error deleting key from groupsDb: %d", ret);
	else
		groupDb->sync(0);

    if (ng->isDbNamedAsAlias())
    {
        dbName = ng->getAlias();
    }

	QFile* dbFile=new QFile(dbPath + "/" + dbName);
	dbFile->remove();
	QFile* dbFile2=new QFile(dbPath + "/" + dbName+"__parts");
	dbFile2->remove();

    if (ng->areHeadersGrouped())
    {
        QFile* dbFile3=new QFile(dbPath + "/" + dbName+"__grouping");
        dbFile3->remove();
    }

	emit unsubscribe(groupName);

	//Delete db file...
    Q_DELETE(keymem);
    Q_DELETE(dbFile);
    Q_DELETE(dbFile2);
}

void GroupList::slotGroupProperties( )
{
    qDebug() << "Update Group Properties";
	QList<QTreeWidgetItem*> selection=this->selectedItems();
	QListIterator<QTreeWidgetItem*> it(selection);
	if (!it.hasNext())
        return;

	QTreeWidgetItem*temp = it.next();
	if (temp->type() != GITEM)
		return;
	NGListViewItem *selected = (NGListViewItem*) temp;

	NewsGroup *ng=selected->getNg();

	QStringList cats;
	//Iterate on the categories and create a QStringList to load in the combobox...
	QHashIterator<QString, Category*> git(categories);
	while (git.hasNext())
	{
		git.next();
// 		qDebug("Found category: %s", (const char *) git.value()->getCategory());
		cats.append(git.value()->name);
	}

	NewsGroupDialog *af=new NewsGroupDialog(ng, servers, cats, this);
	connect(af, SIGNAL(updateGroup(QStringList)), this, SLOT(slotModifyServerProperties(QStringList)));
	connect(af, SIGNAL(addCat(QString )), this, SLOT(slotAddCategory(QString )));
    connect(af, SIGNAL(sigGroupHeaders(NewsGroup*)), qMgr, SLOT(startBulkGrouping(NewsGroup*)));

	af->show();
}

void GroupList::slotGroupManagement( )
{
	QList<QTreeWidgetItem*> selection=this->selectedItems();
	QListIterator<QTreeWidgetItem*> it(selection);
	if (!it.hasNext())
        return;

	QTreeWidgetItem*temp = it.next();
	if (temp->type() != GITEM)
		return;
	NGListViewItem *selected = (NGListViewItem*) temp;


	NewsGroup *ng=selected->getNg();

	NewsGroupContents *af=new NewsGroupContents(ng, servers, this);

	connect(qMgr, SIGNAL(sigLimitsUpdate(NewsGroup *, NntpHost *, uint, uint, uint)), af,
			      SLOT(slotLimitsUpdate(NewsGroup *, NntpHost *, uint, uint, uint)));
	connect(qMgr, SIGNAL(sigHeaderDownloadProgress(NewsGroup *, NntpHost *, quint64, quint64, quint64)), af,
			      SLOT(slotHeaderDownloadProgress(NewsGroup *, NntpHost *, quint64, quint64, quint64)));
	connect(af, SIGNAL(sigGetGroupLimits(NewsGroup *)), qMgr, SLOT(slotGetGroupLimits(NewsGroup *)));
	connect(af, SIGNAL(sigGetGroupLimits(NewsGroup *, quint16, NntpHost*)), qMgr, SLOT(slotGetGroupLimits(NewsGroup *, quint16, NntpHost*)));
	connect(af, SIGNAL(sigGetHeaders(quint16, NewsGroup *, quint64, quint64)), this, SLOT(slotUpdateByRange(quint16, NewsGroup *, quint64, quint64)));
	connect(af, SIGNAL(sigExpireNewsgroup(NewsGroup *, uint, uint)), this, SLOT(slotExpireNewsgroup(NewsGroup *, uint, uint)));
	connect(this, SIGNAL(sigUpdateFinished(NewsGroup *)), af, SLOT(slotUpdateFinished(NewsGroup *)));

	// af->exec(); //don't want it modal
	af->show();
}

void GroupList::slotNewGroup( QStringList entries, AvailableGroup *g )
{
	//field 0 is name, field1 is the savedir, 2 is the alias, 3 is the category

	if (groups.value(entries[0]) != 0) {
		QMessageBox::warning(this, tr("Error"), tr("Group is already subscribed"));
		return;
	}
	NewsGroup *tempNg = new NewsGroup(dbenv, entries[0], entries[1], entries[2], entries[7]);
	tempNg->setAlias(entries[2]);
	tempNg->setCategory(entries[3]);
	tempNg->setDeleteOlderPosting(entries[4].toInt());
	tempNg->setDeleteOlderDownload(entries[5].toInt());
	tempNg->ignoreUnsequencedArticles(entries[6] == "Y" ? true : false);
	tempNg->nameDbAsAlias(entries[7] == "Y" ? true : false);
	tempNg->dbCurrentlyNamedAsAlias(tempNg->shouldDbBeNamedAsAlias());
    tempNg->setHeadersGrouped(entries[8] == "Y" ? true : false);
	tempNg->setMultiPartSequence(0);

    tempNg->setGroupRE1(entries[9]);
    tempNg->setGroupRE2(entries[10]);
    tempNg->setGroupRE3(entries[11]);

    tempNg->setGroupRE1Back(entries[12].toInt());
    tempNg->setGroupRE2Back(entries[13].toInt());
    tempNg->setGroupRE3Back(entries[14].toInt());

    tempNg->setMatchDistance(entries[15].toInt());

    tempNg->setNoRegexGrouping(entries[16] == "Y" ? true : false);
    tempNg->setAdvancedGrouping(entries[18] == "Y" ? true : false);

	groups.insert(tempNg->getName(), tempNg);
	Servers::iterator it;

	qDebug() << "At group create, Group name = " << g->getName();

	for (it =servers->begin(); it != servers->end(); ++it)
	{
		if ((g->hasServerPresence(it.key())) && (g->getServerPresence(it.key()) == true))
		{
			qDebug() << "Just set server presence to true for newsgroup " << it.key() << ", " << g->getServerPresence(it.key());
			tempNg->serverPresence.insert(it.key(), true);
			tempNg->low.insert(it.key(), 0);
			tempNg->high.insert(it.key(), 0);
			tempNg->serverParts.insert(it.key(), 0);
			tempNg->serverRefresh.insert(it.key(), "Never");
		}
		else
		{
			qDebug() << "Just set server presence to false for newsgroup " << it.key() << ", " << g->getServerPresence(it.key());
			tempNg->serverPresence.insert(it.key(), false);
		}

		tempNg->servLocalLow.insert(it.key(), 0);
		tempNg->servLocalHigh.insert(it.key(), 0);
		tempNg->servLocalParts.insert(it.key(), 0);
	}
	//Display the new group...
	if (tempNg->getCategory() != "None")
	{
		//Add group to the category...
		tempNg->setListItem(new NGListViewItem(categories.value(tempNg->getCategory())->listItem, tempNg));
		QTreeWidgetItem *listItem = categories.value(tempNg->getCategory())->listItem;
		listItem->setText(2, QString::number(listItem->text(2).toInt() + 1));
	}
	else
		tempNg->setListItem(new NGListViewItem(this, tempNg));

	tempNg->lastExpiry = QDate::currentDate().toString("dd.MMM.yyyy");

	saveGroup(tempNg);
	emit sigHaveGroups(true);
	emit subscribed(g);
	emit sigNewGroup(tempNg);
}

void GroupList::slotModifyServerProperties( QStringList entries)
{
    bool groupingUpdated = false;

	//field 0 is name, field1 is the savedir, 2 is the alias, 3 is the category
	NewsGroup *ng=groups.value(entries[0]);
	ng->saveDir=entries[1] + '/';
	ng->setAlias(entries[2]);
	//Move the item if needed, and save the category
	if ( ng->getCategory() != entries[3] )
	{
		if (ng->getCategory() == "None")
		{
            NGListViewItem* ptr = ng->listItem;
            Q_DELETE(ptr);
		}
		else
		{
			//Take the item from the "old" parent...
			categories.value(ng->getCategory())->listItem->takeChild(categories.value(ng->getCategory())->listItem->indexOfChild(ng->listItem));
			categories.value(ng->getCategory())->childs--;
			categories.value(ng->getCategory())->listItem->setText(2, QString::number(categories.value(ng->getCategory())->childs));
		}
		//Item is taken, insert in the new place...
		if (entries[3] == "None")
		{
			//insert toplevel
			this->addTopLevelItem(ng->listItem);
		}
		else
		{
			//Insert as child of category
			categories.value(entries[3])->listItem->addChild(ng->listItem);
			categories.value(entries[3])->childs++;
			categories.value(entries[3])->listItem->setText(2, QString::number(categories.value(entries[3])->childs));
		}

		ng->setCategory(entries[3]);
	}
	ng->setDeleteOlderPosting(entries[4].toInt());
	ng->setDeleteOlderDownload(entries[5].toInt());
	ng->ignoreUnsequencedArticles(entries[6] == "Y" ? true : false);
	ng->nameDbAsAlias(entries[7] == "Y" ? true : false);

	if (ng->shouldDbBeNamedAsAlias() != ng->isDbNamedAsAlias())
		ng->renameDbAtStartup(true);

    bool previouslyGrouped = ng->areHeadersGrouped();
    ng->setHeadersGrouped(entries[8] == "Y" ? true : false);

    if (ng->areHeadersGrouped() == false)
        ng->setHeadersNeedGrouping(false);

    ng->setGroupRE1(entries[9]);
    ng->setGroupRE2(entries[10]);
    ng->setGroupRE3(entries[11]);

    ng->setGroupRE1Back(entries[12].toInt());
    ng->setGroupRE2Back(entries[13].toInt());
    ng->setGroupRE3Back(entries[14].toInt());

    ng->setMatchDistance(entries[15].toInt());

    ng->setNoRegexGrouping(entries[16] == "Y" ? true : false);
    groupingUpdated = entries[17] == "Y" ? true : false;
    ng->setAdvancedGrouping(entries[18] == "Y" ? true : false);

	saveGroup(ng);
	ng->listItem->setText(0, ng->getAlias());

    if (previouslyGrouped != ng->areHeadersGrouped() || groupingUpdated) // grouping has been enabled or disabled ... or changed
    {
        if (previouslyGrouped == false || groupingUpdated) // now grouped
        {
            ng->groupHeaders();

            // Launch a bulk job to group headers
            emit sigGroupHeaders(ng);
        }
        else // no longer grouped
        {
            quint64 bulkSeq = ng->getGroupingBulkSeq();
            if (bulkSeq) // grouping is in progress
            {
                emit sigCancelBulkDelete(bulkSeq);
                // MD TODO need to wait until stopped before can delete the db
            }
            else
            {
                QString dbName = ng->ngName;

                if (ng->isDbNamedAsAlias())
                {
                    dbName = ng->getAlias();
                }

                QFile* dbFile=new QFile(dbPath + "/" + dbName+"__grouping");
                dbFile->remove();
            }
        }
    }
}

void GroupList::slotAddCategory( QString catName)
{
	//Add the category to "categories" and to the listview...
	Category *cat = new Category;
	cat->childs=0;
	cat->name=catName;
	cat->listItem=new QTreeWidgetItem(this);
	cat->listItem->setText(0,cat->name);
	cat->listItem->setText(1,"0");
// 	cat->listItem->setExpandable(true);
	categories.insert(catName, cat);
	cat->listItem->setIcon(0, QIcon(":/quban/images/gnome/32/Gnome-Folder-32.png"));
}

uchar * GroupList::getBinHeader( QString index, QString group )
{
// 	qDebug() << "Getting header from " << group;
	return groups.value(group)->getBinHeader(index);
}

NewsGroup * GroupList::getNg( QString group )
{
	return groups.value(group);
}

void GroupList::slotRemoveCategory( )
{
	QList<QTreeWidgetItem*> selection=this->selectedItems();
	QListIterator<QTreeWidgetItem*> it(selection);

	if (!it.hasNext())
        return;

	QTreeWidgetItem *item=it.next();

	if (item->childCount() == 0)
	{
		//Remove the item
// 		qDebug("About to remove %s", (const char *) item->text(0));
        Q_DELETE(item);
	}
	else
		QMessageBox::warning(this, tr("Error"), tr("Can only remove empty categories!"));

}

void GroupList::checkGroups( )
{
	if (groups.isEmpty())
		emit sigHaveGroups(false);
	else emit sigHaveGroups(true);
}

void GroupList::slotCompactDbs( )
{
	QProgressDialog *qpd = new QProgressDialog(tr("Compacting ..."), QString(), 0, 100, this);

	Dbt key, data;
	uchar *keymem=new uchar[KEYMEM_SIZE];
	uchar *datamem=new uchar[DATAMEM_SIZE];

	key.set_flags(DB_DBT_USERMEM);
	key.set_data(keymem);
	key.set_ulen(KEYMEM_SIZE);

	data.set_flags(DB_DBT_USERMEM);
	data.set_ulen(DATAMEM_SIZE);
	data.set_data(datamem);

	//For every newsgroup....
    NewsGroup *ng = 0; //Current newsgroup
	QString tempName; //Name for temp newsgroup
    Db* newDb = 0; //Handle for the new Db.
    Db* oldDb = 0; //Copy of the "old" Db handle

    Dbc *cursor = 0; //To scan the "old" newsgroup
	QTime current, previous;

	QHashIterator<QString, NewsGroup*> it(groups);
	qpd->show();
	int gCount=0;
	int totalNewsgroups=groups.count();
	QFile logFile(dbPath + '/' + "compact.log");
	logFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
	QTextStream logStream(&logFile);

	while(it.hasNext())
	{
		it.next();
		ng=it.value();
		qDebug() << "Compacting " << ng->getName();
		gCount++;
		//-Create the new Db.
		int ret;
		tempName=ng->getName() + ".new";
	    QByteArray ba = tempName.toLocal8Bit();
	    const char *c_str = ba.data();

		if (!(newDb = quban->getDbEnv()->openDb(c_str, true, &ret)))
		{
			QMessageBox::warning(0, tr("Error"), tr("Unable to open temp newsgroup database ..."));
			return;
		}

		oldDb=ng->getDb();
		oldDb->cursor(0, &cursor,0);

		float total=(float)ng->getTotal();
		//Move data
		qpd->setLabelText(tr("Compacting %3 (%1/%2)").arg(gCount).arg(totalNewsgroups).arg(ng->getName()));
		uint unreadArticles=0;
		uint totalArticles=0;
		previous=QTime::currentTime();
        MultiPartHeader *bh = 0;
		while (cursor->get(&key, &data, DB_NEXT) != DB_NOTFOUND  )
		{
			bh = new MultiPartHeader(key.get_size(), (char*)key.get_data(), (char*)data.get_data());

            if (bh->getStatusIgnoreMark() == MultiPartHeader::bh_new)
				unreadArticles++;
			totalArticles++;
            Q_DELETE(bh);
			int transRet = newDb->put(0, &key, &data,0);
			switch  (transRet) {
				case DB_RUNRECOVERY:
					QMessageBox::critical(this, tr("Fatal error"), tr("Unrecoverable error on db. Please restart Quban"));
					exit (-1);
					break;
				case 0:
					break;
				default:
					logStream << "Error inserting record into new db: " << transRet;
					qDebug() << "Error inserting record into new db: " << dbenv->strerror(transRet);
					break;
			}

			current=QTime::currentTime();
			if (previous.msecsTo(current) > 1000)
			{
				qpd->setValue( uint((float(totalArticles)/float(total)) * 100));
				QApplication::processEvents();

// 				previous=current;
				previous=QTime::currentTime();
			}
		}
		qDebug() << ng->getName() << " Compacted";
		cursor->close();

		ng->setTotal(totalArticles);
		ng->listItem->setText(Total_Col, QString("%L1").arg(totalArticles));

		ng->setUnread(unreadArticles);
		ng->listItem->setText(Unread_Col, QString("%L1").arg(unreadArticles));

// 		qDebug("Moved %d records", count);
		//Rename db.
		newDb->sync(0);
		ng->close();

        Db *tempDb = new Db(dbenv,0);
	    QByteArray ba4 = ng->getName().toLocal8Bit();
	    const char *c_str4 = ba4.data();
		tempDb->remove(c_str4,0, 0);
        Q_DELETE(tempDb);

		newDb->close(0);
        Q_DELETE(newDb);


		tempDb=new Db(dbenv,0);
	    QByteArray ba2 = tempName.toLocal8Bit();
	    const char *c_str2 = ba2.data();
	    QByteArray ba3 = ng->getName().toLocal8Bit();
	    const char *c_str3 = ba3.data();
		tempDb->rename(c_str2, 0, c_str3, 0);

        Q_DELETE(tempDb);
        ng->open();

        quban->getLogEventList()->logEvent(tr("Successfully compacted newsgroup ") + ng->getAlias());
	}

    Q_DELETE_ARRAY(keymem);
    Q_DELETE_ARRAY(datamem);

	qpd->setValue(100);

	logFile.close();

    Q_DELETE(qpd);
}

void GroupList::slotSaveSettings( NewsGroup *ng, bool unread, bool complete)
{
	qDebug( ) << "slotSaveSettings()";

	ng->showOnlyUnread=unread;
	ng->showOnlyComplete=complete;
	ng->listItem->setText(Total_Col, QString("%L1").arg(ng->totalArticles));
	ng->listItem->setText(Unread_Col, QString("%L1").arg(ng->unreadArticles));
// 	qDebug("Saving settings");
// 	qDebug("Unread: %d", ng->onlyUnread());
// 	qDebug("Complete: %d", ng->onlyComplete());
	saveGroup(ng);

}

void GroupList::updateCurrent( NewsGroup * ng )
{
    emit updateNewsGroup(ng, 0, 0 );
}

void GroupList::slotUpdateWOptions(NewsGroup *ng, uint from, uint to)
{
	// The caller should have set the newsgroup to updating before entering here
	qDebug() << "Update with options " << ng->name() << ", " << from << ", " << to;

    NGListViewItem *selected = 0;

	for (int i=0; i<this->topLevelItemCount(); ++i)
	{
		if (this->topLevelItem(i)->type() == GITEM)
		{
			selected = (NGListViewItem*)this->topLevelItem(i);
		    qDebug() << "Item " << selected->getNg()->getAlias();
		    if (selected->getNg()->getAlias() == ng->getAlias())
		    {
		    	selected->setDisabled(true);
		    	break;
		    }
		}
		else
		{
			qDebug() << "Group";
			for (int c=0; c<this->topLevelItem(i)->childCount(); ++c)
			{
				selected = (NGListViewItem*)this->topLevelItem(i)->child(c);
			    qDebug() << "Item " << selected->getNg()->getAlias();
			    if (selected->getNg()->ngName == ng->ngName)
			    {
			    	selected->setDisabled(true);
			    	ng->listItem = selected;
			    	break;
			    }
			}
		}
	}

	emit updateNewsGroup(ng, from, to);
	slotSelectionChanged();
}

void GroupList::slotUpdateByRange(quint16 hostId, NewsGroup * ng, quint64 from, quint64 to)
{
	// The caller should have set the newsgroup to updating before entering here
	qDebug() << "Update with options " << ng->name() << ", " << from << ", " << to;

    NGListViewItem *selected = 0;

	for (int i=0; i<this->topLevelItemCount(); ++i)
	{
		if (this->topLevelItem(i)->type() == GITEM)
		{
			selected = (NGListViewItem*)this->topLevelItem(i);
		    qDebug() << "Item " << selected->getNg()->getAlias();
		    if (selected->getNg()->getAlias() == ng->getAlias())
		    {
		    	selected->setDisabled(true);
		    	break;
		    }
		}
		else
		{
			qDebug() << "Group";
			for (int c=0; c<this->topLevelItem(i)->childCount(); ++c)
			{
				selected = (NGListViewItem*)this->topLevelItem(i)->child(c);
			    qDebug() << "Item " << selected->getNg()->getAlias();
			    if (selected->getNg()->ngName == ng->ngName)
			    {
			    	selected->setDisabled(true);
			    	ng->listItem = selected;
			    	break;
			    }
			}
		}
	}

	emit updateNewsGroup(hostId, ng, from, to);
	slotSelectionChanged();
}


void GroupList::slotExpireNewsgroup(NewsGroup *ng, uint exptype, uint expvalue)
{
	// The caller should have set the newsgroup to updating before entering here
	qDebug() << "Expire " << ng->name() << ", " << exptype << ", " << expvalue;

    NGListViewItem *selected = 0;

	for (int i=0; i<this->topLevelItemCount(); ++i)
	{
		if (this->topLevelItem(i)->type() == GITEM)
		{
			selected = (NGListViewItem*)this->topLevelItem(i);
		    // qDebug() << "Item " << selected->getNg()->getAlias();
		    if (selected->getNg()->getAlias() == ng->getAlias())
		    {
		    	selected->setDisabled(true);
		    	break;
		    }
		}
		else
		{
			qDebug() << "Group";
			for (int c=0; c<this->topLevelItem(i)->childCount(); ++c)
			{
				selected = (NGListViewItem*)this->topLevelItem(i)->child(c);
			    // qDebug() << "Item " << selected->getNg()->getAlias();
			    if (selected->getNg()->ngName == ng->ngName)
			    {
			    	selected->setDisabled(true);
			    	ng->listItem = selected;
			    	break;
			    }
			}
		}
	}

	emit expireNewsGroup(ng, exptype, expvalue);
	slotSelectionChanged();
}

bool GroupList::isAliasUsedForDbName(QString alias)
{
	QHashIterator<QString, NewsGroup*> it(groups);
	while (it.hasNext())
	{
		it.next();
		NewsGroup *ng=it.value();

		if (ng->getAlias() == alias && ng->isDbNamedAsAlias())
			return true;
	}

	return false;
}
