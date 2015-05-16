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

#include <QMessageBox>
#include <QFileDialog>
#include <QTreeWidgetItem>
#include <QRegExp>
#include <qlabel.h>
#include <QList>
#include <QDebug>
#include <QtDebug>
#include "common.h"
#include "nzbform.h"
#include "MultiPartHeader.h"

NzbForm::NzbForm(QList<NzbHeader*> *hList, QString dDir, QString groupName, QString category,  QWidget* parent,
        const char* name, Qt::WindowFlags f)
	: QDialog(parent,f), headerList(hList), destDir(dDir)
{
	setupUi(this);

	groupItems=false;
	keepOpen = false;

    if (groupName.isEmpty())
        nzbTitle->setHidden(true);
    else
        nzbTitle->setText(tr("Title: ") + groupName);

    if (category.isEmpty())
        nzbCategory->setHidden(true);
    else
        nzbCategory->setText(tr("Category: ") + category);

	dirEdit->setText(destDir);
	nzbFile = QString(name);
	QString label=QString("<b>Nzb file: ") + nzbFile + QString("</b>");
	fileLabel->setText(label);
	markSelBtn->setEnabled(false);
	unmarkSelBtn->setEnabled(false);

	artList->setSortingEnabled(false);
	artList->setAllColumnsShowFocus(true);
	artList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    artList->setContentsMargins(3,3,3,3);

	QList<NzbHeader*>::iterator it;
    QTreeWidgetItem* item;
    NzbHeader* nh;

	int i=1;

	for ( it = headerList->begin(); it != headerList->end(); ++it)
	{
		nh=*it;
		item = new QTreeWidgetItem(artList);
		item->setFlags(item->flags()|Qt::ItemIsUserCheckable);
		item->setText(0, QString::number(i));
		++i;
		item->setCheckState(0,Qt::Checked);
		item->setText(1, nh->getSubj());
		item->setText(2, QString::number(nh->getSize()));
	}

	config = Configuration::getConfig();
	if (config->autoUnpack == false)
	{
		groupButton->setEnabled(false);
		partGroupButton->setEnabled(false);
	}

	connect(artList, SIGNAL(itemSelectionChanged()), this, SLOT(slotSelectionChanged()));

	connect(markAllBtn, SIGNAL(clicked()), this, SLOT(slotMarkAll()));
	connect(unmarkAllBtn, SIGNAL(clicked()), this, SLOT(slotUnmarkAll()));
	connect(markSelBtn, SIGNAL(clicked()), this, SLOT(slotMarkSelected()));
	connect(unmarkSelBtn, SIGNAL(clicked()), this, SLOT(slotUnmarkSelected()));
    connect(markRelatedButton, SIGNAL(clicked()), this, SLOT(slotMarkRelated()));

	connect(dirBtn, SIGNAL(clicked()), this, SLOT(slotSelectDir()));
	connect(groupButton, SIGNAL(clicked()), this, SLOT(slotGroupItems()));
	connect(partGroupButton, SIGNAL(clicked()), this, SLOT(slotPartGroupItems()));

	artList->resizeColumnToContents(0);
	artList->resizeColumnToContents(1);
	artList->resizeColumnToContents(2);
}

NzbForm::~NzbForm()
{
}

/*$SPECIALIZATION$*/
void NzbForm::reject()
{
	// The list is cleared in the calling function, but need to free memory for list members here
	if (keepOpen == false)
	{
		qDeleteAll(*headerList);
	}
	else
	{
		// delete the remaining items by their numbers (as in normal accept)
		QTreeWidgetItem *item;
    	item = (QTreeWidgetItem *) artList->topLevelItem(0);

		while (item)
		{
			NzbHeader *nh = (*headerList)[item->text(0).toInt() - 1];
			delete nh;
			item = (QTreeWidgetItem*) artList->itemBelow(item);
		}
	}

  QDialog::reject();
}

void NzbForm::slotGroupItems()
{
	groupItems=true;
	accept();
}

void NzbForm::slotPartGroupItems()
{
	keepOpen = true;
	slotGroupItems();
}

void NzbForm::accept()
{
	if (dirEdit->text().trimmed().isEmpty())
	{
		QMessageBox::warning ( this, tr("Missing Directory"), tr("The directory name can not be empty"));
	}
	else if (!checkAndCreateDir(dirEdit->text().trimmed()) )
	{
		QMessageBox::warning ( this, tr("Invalid Directory"), tr("Could not create directory"));
	}
	else
	{
		bool first = firstCheck->isChecked();
		QTreeWidgetItem *item;
		if (first)
			item=(QTreeWidgetItem*) artList->topLevelItem(artList->topLevelItemCount() - 1);
		else
			item = (QTreeWidgetItem *) artList->topLevelItem(0);

		NzbHeader *nh=0;
		//Check the dir...
		QString dir =  dirEdit->text().trimmed() + '/';

		while (item)
		{
			if (item->checkState(0) == Qt::Checked)
			{
				//Download the item...
				nh= (*headerList)[item->text(0).toInt() - 1];
				emit sigDownloadNzbPost(nh, groupItems, first, dir);
			}
			else
			{
				//delete the corresponding binHeader
				if (keepOpen == false)
				{
					nh= (*headerList)[item->text(0).toInt() - 1];
					delete nh;
				}
			}
			if (first)
				item = (QTreeWidgetItem*) artList->itemAbove(item);
			else
				item = (QTreeWidgetItem*) artList->itemBelow(item);
		}

		if (keepOpen == true) // clear the ones that we've just downloaded
		{
			QList<int> deletedItemsIndex;
			int	indexItem = 0;
			item=(QTreeWidgetItem*) artList->topLevelItem(0);

			while (item)
			{
				if (item->checkState(0) == Qt::Checked)
				{
					indexItem = artList->indexOfTopLevelItem(item);
					deletedItemsIndex.append(indexItem);
				}

				item = (QTreeWidgetItem*) artList->itemBelow(item);
			}

			for (int i = deletedItemsIndex.count() - 1; i >= 0; --i)
			{
				artList->takeTopLevelItem(deletedItemsIndex.at(i));
			}
		}

		emit sigFinishedNzbPosts(groupItems, nzbFile);

		if (keepOpen == false)
		{
		    QDialog::accept();
		}
	}
}

void NzbForm::slotMarkAll()
{
	QTreeWidgetItem * item = (QTreeWidgetItem *) artList->topLevelItem(0);

    while (item)
    {
		item->setCheckState(0,Qt::Checked);
		item = (QTreeWidgetItem*) artList->itemBelow(item);
	}
}

void NzbForm::slotUnmarkAll()
{
	QTreeWidgetItem * item = (QTreeWidgetItem *) artList->topLevelItem(0);

    while (item)
    {
		item->setCheckState(0,Qt::Unchecked);
		item = (QTreeWidgetItem*) artList->itemBelow(item);
	}
}

void NzbForm::slotMarkSelected()
{
	QList<QTreeWidgetItem*> selection = artList->selectedItems();
	QListIterator<QTreeWidgetItem*> it(selection);

	QTreeWidgetItem* selected;
	while ( it.hasNext())
	{
		selected = (QTreeWidgetItem*) it.next();
		selected->setCheckState(0,Qt::Checked);
	}
}

void NzbForm::slotUnmarkSelected()
{
	QList<QTreeWidgetItem*> selection = artList->selectedItems();
	QListIterator<QTreeWidgetItem*> it(selection);
	QTreeWidgetItem* selected;
	while ( it.hasNext())
	{
		selected = (QTreeWidgetItem*) it.next();
		selected->setCheckState(0,Qt::Unchecked);
	}
}

void NzbForm::slotMarkRelated()
{
    QList<QTreeWidgetItem*> selection = artList->selectedItems();
    if (selection.count() != 1)
    {
        QMessageBox::warning ( this, tr("Invalid Selection"), tr("A single article must be selected to find related articles"));
        return;
    }

    slotUnmarkAll();

    QTreeWidgetItem* selected;
    QString subject;
    QString matched;
    QRegExp match;

    match.setCaseSensitivity(Qt::CaseInsensitive);

    selected = (QTreeWidgetItem*)selection[0];

    // Need a regexp to match articles
    // use QString contains() and section()
    subject = selected->text(1);

    match.setPattern("cd\\s*\\d");
    if (subject.contains(match))
    {
        matched = match.cap(0);
        qDebug() << "Matched CD: " << match.cap(0);
    }
    else
    {
        match.setPattern("disk\\s*\\d");
        if (subject.contains(match))
            matched = match.cap(0);
        else
        {
            match.setPattern("disc\\s*\\d");
            if (subject.contains(match))
                matched = match.cap(0);
        }
    }

    selected->setCheckState(0,Qt::Checked);

    if (!matched.isEmpty())
    {
        QTreeWidgetItem * item = (QTreeWidgetItem *) artList->topLevelItem(0);

        while (item)
        {
            if (item->text(1).contains(matched))
                item->setCheckState(0,Qt::Checked);

            item = (QTreeWidgetItem*) artList->itemBelow(item);
        }
    }
}

void NzbForm::slotSelectDir( )
{
	 QString dir = QFileDialog::getExistingDirectory(this, tr("Please select a directory"),
			 dirEdit->text().trimmed(),
			 QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	if (!dir.isEmpty())
		dirEdit->setText(dir);
}

void NzbForm::slotSelectionChanged( )
{
	QList<QTreeWidgetItem*> selection = artList->selectedItems();
	if (selection.isEmpty()) {
		markSelBtn->setEnabled(false);
		unmarkSelBtn->setEnabled(false);
	} else {
		markSelBtn->setEnabled(true);
		unmarkSelBtn->setEnabled(true);
	}
}
