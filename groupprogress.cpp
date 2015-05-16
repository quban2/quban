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

#include "qmgr.h"
#include "groupprogress.h"

GroupProgress::GroupProgress(QWidget *parent)
    : QDialog(parent)
{
    qMgr = (QMgr*)parent;
	currentGroupId = 0;

	setupUi(this);

	groupList->setSortingEnabled(false);
	groupList->setAllColumnsShowFocus(true);
	groupList->setSelectionMode(QAbstractItemView::SingleSelection);
	groupList->setContentsMargins(3,3,3,3);

	populateAll();

	connect(groupList, SIGNAL(itemClicked(QTreeWidgetItem *, int)), this, SLOT(slotItemSelected(QTreeWidgetItem *)));
	connect(deletePushButton, SIGNAL(clicked()), this, SLOT(slotDeleteSelected()));
	connect(markButton, SIGNAL(clicked()), this, SLOT(slotMarkAll()));
	connect(refreshPushButton, SIGNAL(clicked()), this, SLOT(populateAll()));
}

GroupProgress::~GroupProgress()
{

}

void GroupProgress::populateAll()
{
	QTreeWidgetItem* item;

	groupList->clear();

	autoGroupList = qMgr->groupedDownloads.values();
	QList<GroupManager*>::iterator it;

	for ( it = autoGroupList.begin(); it != autoGroupList.end(); ++it)
	{
		item = new QTreeWidgetItem(groupList);
		item->setFlags(item->flags()|Qt::ItemIsUserCheckable);
		item->setCheckState(0,Qt::Unchecked);
		if ((*it)->getMasterUnpack() != QString::null &&
			(*it)->getMasterUnpack() != "")
		    item->setText(1, (*it)->getMasterUnpack());
		else
			item->setText(1, tr("Uncompressed group"));
		item->setText(2, (*it)->getGroupStatusString());
		item->setText(3, QString::number((*it)->getNumFiles()));
		item->setText(4, QString::number((*it)->getNumDecodedFiles()));
		item->setText(5, QString::number((*it)->getNumFailedFiles()));
		item->setText(6, QString::number((*it)->getNumFailedDecodeUnpackParts()));
		item->setText(7, QString::number((*it)->getNumCancelledFiles()));
		item->setText(8, QString::number((*it)->getNumOnHoldFiles()));
		item->setText(9, QString::number((*it)->getNumRepairBlocks()));

		if ((currentGroupId && currentGroupId == (*it)->getGroupId()) ||
		    (currentGroupId == 0 && it == autoGroupList.begin()))
		{
			slotItemSelected(item);
			groupList->setCurrentItem(item);
		}
	}

	groupList->resizeColumnToContents(0);
	groupList->resizeColumnToContents(1);
	groupList->resizeColumnToContents(2);
	groupList->resizeColumnToContents(3);
	groupList->resizeColumnToContents(4);
	groupList->resizeColumnToContents(5);
	groupList->resizeColumnToContents(6);
	groupList->resizeColumnToContents(7);
	groupList->resizeColumnToContents(8);
	groupList->resizeColumnToContents(9);
}

void GroupProgress::slotMarkAll()
{
	QTreeWidgetItem *item = (QTreeWidgetItem *)groupList->topLevelItem(0);

	while (item)
	{
		item->setCheckState(0, Qt::Checked);
		item = (QTreeWidgetItem*) groupList->itemBelow(item);
	}
}

void GroupProgress::slotItemSelected(QTreeWidgetItem *item)
{
	int	indexItem = groupList->indexOfTopLevelItem(item);
	GroupManager* thisGroup = autoGroupList[indexItem];
	currentGroupId = thisGroup->getGroupId();

	dirLabel->setText(tr("Unpack dir: ") + thisGroup->getDirPath());
	repairLabel->setText(tr("Master Repair file: ") + thisGroup->getMasterRepair());
	unpackLabel->setText(tr("Master Unpack file: ") + thisGroup->getMasterUnpack());

	QList <PendingHeader*> headerList = qMgr->pendingHeaders.values(thisGroup->getGroupId());
	QList<PendingHeader*>::iterator it;

	onHoldList->clear();
	for ( it = headerList.begin(); it != headerList.end(); ++it)
	{
		item = new QTreeWidgetItem(onHoldList);
		item->setText(0, (*it)->getHeader()->getSubj());
	}

	onHoldList->resizeColumnToContents(0);

	QList <AutoFile*> autoFileList = qMgr->autoFiles.values(thisGroup->getGroupId());
	QList<AutoFile*>::iterator it2;

	decodedList->clear();
	for (it2 = autoFileList.begin(); it2 != autoFileList.end(); ++it2)
	{
		item = new QTreeWidgetItem(decodedList);
		item->setText(0, (*it2)->getFileName());
	}

	decodedList->resizeColumnToContents(0);

	logWindow->clear();
	logWindow->setReadOnly(true);
	logWindow->insertPlainText(thisGroup->getProcessLog());
}

void GroupProgress::clearBody()
{
	dirLabel->setText(tr("Unpack dir: "));
	repairLabel->setText(tr("Master Repair file: "));
	unpackLabel->setText(tr("Master Unpack file: "));

	onHoldList->clear();
	onHoldList->resizeColumnToContents(0);

	decodedList->clear();
	decodedList->resizeColumnToContents(0);

	logWindow->clear();
}

void GroupProgress::slotDeleteSelected()
{
	QTreeWidgetItem *item = (QTreeWidgetItem *)groupList->topLevelItem(0);
	GroupManager* thisGroup = 0;
	QList<int> deletedItemsIndex;
	int	indexItem = 0;

	while (item)
	{
		if (item->checkState(0) == Qt::Checked)
		{
			//Delete the item...
			indexItem = groupList->indexOfTopLevelItem(item);
			deletedItemsIndex.append(indexItem);
		}

		item = (QTreeWidgetItem*) groupList->itemBelow(item);
	}

	 for (int i = deletedItemsIndex.count() - 1; i >= 0; --i)
	 {
		 thisGroup = autoGroupList[deletedItemsIndex.at(i)];
		 qMgr->removeAutoGroup(thisGroup);
		 autoGroupList.removeAll(thisGroup);
		 groupList->takeTopLevelItem(deletedItemsIndex.at(i));
	 }

	 // clear the main screen if relates to a deleted item
	 if (qMgr->groupedDownloads.contains(currentGroupId) == false)
	 {
		 currentGroupId = 0;
		 clearBody();
	 }
	 else
	 {
		 int j=0;
		 QMapIterator<quint16, GroupManager *> i(qMgr->groupedDownloads);
		 while (i.hasNext())
		 {
		     i.next();
		     if (i.key() == currentGroupId)
		     {
		    	 groupList->setCurrentItem(groupList->topLevelItem(j));
		    	 break;
		     }
		     ++j;
		 }
	 }
}
