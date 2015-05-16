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
#include <QTreeWidgetItem>
#include <QString>
#include <QComboBox>
#include <QMessageBox>
#include <QIcon>
#include <QRegExp>
#include "autounpackconfirmation.h"
#include "qmgr.h"
#include "appConfig.h"

AutoUnpackConfirmation::AutoUnpackConfirmation(UnpackConfirmation* _unpackConfirmation, QString _fullNzbFilename, QWidget *parent)
    : QDialog(parent), unpackConfirmation(_unpackConfirmation), fullNzbFilename(_fullNzbFilename)
{
	setupUi(this);

    mainRepair = -1;
    mainUnpack = -1;

    qMgr = (QMgr*)parent;

    QTreeWidgetItem* item;
    QComboBox* cb;
	QString    fileName;

	QList<ConfirmationHeader*>::iterator it;

	int j=0;

	config = Configuration::getConfig();

	for ( it = unpackConfirmation->headers.begin(), j=0; it != unpackConfirmation->headers.end(); ++it, j++)
	{
		item = new QTreeWidgetItem(confirmTreeWidget);
		cb = new QComboBox;
		cb->addItem("Repair");
		cb->addItem("Unpack");
		cb->addItem("Other");
		fileName = (*it)->fileName;

		if ((*it)->UnpackFileType == QUBAN_MASTER_UNPACK || (*it)->UnpackFileType == QUBAN_UNPACK)
			cb->setCurrentIndex(1);
		else if ((*it)->UnpackFileType == QUBAN_MASTER_REPAIR || (*it)->UnpackFileType == QUBAN_REPAIR)
			cb->setCurrentIndex(0);
		else
			cb->setCurrentIndex(2);

		if ((*it)->UnpackFileType == QUBAN_MASTER_UNPACK)
		{
			mainUnpack = j;
			item->setIcon(0, QIcon(":quban/images/rar.png"));
		}
		else if ((*it)->UnpackFileType == QUBAN_MASTER_REPAIR)
		{
			mainRepair = j;
			item->setIcon(0, QIcon(":quban/images/par.png"));
		}

		confirmTreeWidget->setItemWidget(item, 1, cb);
		item->setText(2, fileName);
	}

	confirmTreeWidget->resizeColumnToContents(0);
	confirmTreeWidget->resizeColumnToContents(1);
	confirmTreeWidget->resizeColumnToContents(2);

	connect(okButton, SIGNAL(clicked()), this, SLOT(slotOkClicked()));
	connect(cancelButton, SIGNAL(clicked()), this, SLOT(slotCancelClicked()));
	connect(toggleButton, SIGNAL(clicked()), this, SLOT(slotToggleClicked()));
	connect(setNameButton, SIGNAL(clicked()), this, SLOT(slotSetNameClicked()));
	connect(repairButton, SIGNAL(clicked()), this, SLOT(slotRepairClicked()));
	connect(unpackButton, SIGNAL(clicked()), this, SLOT(slotUnpackClicked()));
}

AutoUnpackConfirmation::~AutoUnpackConfirmation()
{

}

void AutoUnpackConfirmation::slotOkClicked()
{
	QTreeWidgetItem* item;
	QComboBox* cb;
	int numRepair = 0,
		numMasterRepair = 0,
		numUnpack = 0,
		numMasterUnpack = 0;

	unpackConfirmation->repairId=0;
	unpackConfirmation->packId=0;

	for (int i=0; i<unpackConfirmation->headers.count(); i++)
	{
		item = confirmTreeWidget->topLevelItem(i);

		if (mainRepair == i)
		{
			unpackConfirmation->headers.at(i)->UnpackFileType = QUBAN_MASTER_REPAIR;
			unpackConfirmation->repairId = qMgr->determineRepairType(unpackConfirmation->headers.at(i)->fileName, &unpackConfirmation->repairPriority);
			numMasterRepair++;
			numRepair++;
		}
		else if (mainUnpack == i)
		{
			unpackConfirmation->headers.at(i)->UnpackFileType = QUBAN_MASTER_UNPACK;
			unpackConfirmation->packId = qMgr->determinePackType(unpackConfirmation->headers.at(i)->fileName, &unpackConfirmation->packPriority);
			numMasterUnpack++;
			numUnpack++;
		}
		else
		{
			cb = (QComboBox*)confirmTreeWidget->itemWidget(item,1);
			if (cb->currentIndex() == 0)
			{
				unpackConfirmation->headers.at(i)->UnpackFileType = QUBAN_REPAIR;
				numRepair++;
			}
			else if (cb->currentIndex() == 1)
			{
				unpackConfirmation->headers.at(i)->UnpackFileType = QUBAN_UNPACK;
				numUnpack++;
			}
			else
				unpackConfirmation->headers.at(i)->UnpackFileType = QUBAN_SUPPLEMENTARY;
		}
	}

	if (numRepair > 0 && numMasterRepair != 1)
		QMessageBox::warning(this, tr("Error"), tr("Must set a single file as Main Repairer"));
	else if (numUnpack > 0 && numMasterUnpack != 1)
	    QMessageBox::warning(this, tr("Error"), tr("Must set a single file as Main Unpacker"));
	else
	{
        emit sigUnpackConfirmed(fullNzbFilename);
        accept();
	}
}

void AutoUnpackConfirmation::slotCancelClicked()
{
	emit sigUnpackCancelled();
	reject();
}

void AutoUnpackConfirmation::slotToggleClicked()
{
	QTreeWidgetItem* item;
	QString    fileName;
	HeaderBase* hb;
	int i=0;
	QList<ConfirmationHeader*>::iterator it;

	for ( it = unpackConfirmation->headers.begin(); it != unpackConfirmation->headers.end(); ++it)
	{
		item = confirmTreeWidget->topLevelItem(i++);

	    if (toggleButton->isChecked()) // display subjects
	    {
	    	hb=(*it)->hb;
	    	item->setText(2, hb->getSubj().simplified());
	    }
	    else
	    {
	    	fileName = (*it)->fileName;
			item->setText(2, fileName);
	    }
	}

	confirmTreeWidget->resizeColumnToContents(2);
}

void AutoUnpackConfirmation::slotSetNameClicked()
{
	QTreeWidgetItem* item;
	QString    fileName;
	HeaderBase* hb;
	int i=0;
	QList<ConfirmationHeader*>::iterator it;

	for ( it = unpackConfirmation->headers.begin(); it != unpackConfirmation->headers.end(); ++it)
	{
		item = confirmTreeWidget->topLevelItem(i++);

		hb=(*it)->hb;
		fileName = hb->getSubj().simplified();
		item->setText(2, fileName);
		(*it)->fileName = fileName;
	}

	confirmTreeWidget->resizeColumnToContents(2);

	toggleButton->setEnabled(false);
}

void AutoUnpackConfirmation::slotRepairClicked()
{
	QList<QTreeWidgetItem*> selection = confirmTreeWidget->selectedItems();
    if (selection.count() != 1)
    {
    	QMessageBox::warning(this, tr("Error"), tr("Must select a single item to set Main Repairer"));
    	return;
    }

    if (mainRepair != -1)
    	confirmTreeWidget->topLevelItem(mainRepair)->setIcon(0, QIcon());

	selection.at(0)->setIcon(0, QIcon(":quban/images/par.png"));
	mainRepair = confirmTreeWidget->indexOfTopLevelItem(selection.at(0));
}

void AutoUnpackConfirmation::slotUnpackClicked()
{
	QList<QTreeWidgetItem*> selection = confirmTreeWidget->selectedItems();
    if (selection.count() != 1)
    {
    	QMessageBox::warning(this, tr("Error"), tr("Must select a single item to set Main Unpacker"));
    	return;
    }

    if (mainUnpack != -1)
    	confirmTreeWidget->topLevelItem(mainUnpack)->setIcon(0, QIcon());

	selection.at(0)->setIcon(0, QIcon(":quban/images/rar.png"));
	mainUnpack = confirmTreeWidget->indexOfTopLevelItem(selection.at(0));

}
