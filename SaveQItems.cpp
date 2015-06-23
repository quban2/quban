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
#include "SaveQItems.h"

static const int szInt = sizeof(int);
static const quint16 sz16 = sizeof(quint16);
static const quint16 sz64 = sizeof(quint64);

SaveQItems::SaveQItems(Db *qDb)
{
	saveQDb = qDb;
	item = 0;
	startProcessingTimer.setSingleShot(true);
	connect(&startProcessingTimer, SIGNAL(timeout()), this, SLOT(processAllItems()));
}

SaveQItems::~SaveQItems()
{
}

void SaveQItems::addSaveItem(QSaveItem* si)
{
	saveQItems.append(si);
	if (!startProcessingTimer.isActive())
	{
		startProcessingTimer.start(1000); // wait for up to 1 second in case there are a lot of changes
	}
}

void SaveQItems::processAllItems()
{
	int ok = 0;
	while (!saveQItems.isEmpty())
	{
		item = saveQItems.first();
		if (item && (ok = processItem(item)))
		{
			saveQItems.removeAll(saveQItems.first());
            Q_DELETE(item);
			item = 0;
		}
		else if (!ok)
		{
            emit saveQError();
			break;
		}
	}
}

bool SaveQItems::processItem(QSaveItem *qsi)
{
	switch (qsi->type)
	{
	case QSaveItem::QSI_Add:
		return writeSaveItem(qsi);
		break;
	case QSaveItem::QSI_Delete:
		return delSaveItem(qsi->id);
		break;
	case QSaveItem::QSI_Update:
		return updateItem(qsi);
		break;
	default:
		qDebug() << "Error: wrong QSaveItem type";
		return false;
		break;
	}
}

bool SaveQItems::writeSaveItem(QSaveItem *si)
{
	// qDebug() << "Writing save item " << si->id;
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	key.set_data(&(si->id));
	key.set_size(szInt);
	char *p = saveItemData(si);
	data.set_data(p);
	data.set_size(saveItemSize(si));
	//TODO:free the data?

	if ((saveQDb->put(0, &key, &data, 0)) != 0)
	{
        Q_DELETE_ARRAY(p);
		return false;
	}
	else
	{
        Q_DELETE_ARRAY(p);
		saveQDb->sync(0);
		return true;
	}
}

QSaveItem *SaveQItems::readSaveItem(int id)
{
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	key.set_data(&id);
	key.set_size(szInt);

	data.set_flags(DB_DBT_MALLOC);

	if ((saveQDb->get(0, &key, &data, 0)) != 0)
	{
		qDebug("Error getting item from queue db");
		return 0;
	}
	else
	{
		QSaveItem *si = loadSaveItem((char*) data.get_data());
		// 		qDebug("SaveItem Loaded");
        void* ptr = data.get_data();
        Q_FREE(ptr);
		// 		free(key.get_data());
		return si;
	}
}

bool SaveQItems::updateItem(QSaveItem *qsi)
{
	QSaveItem *si = readSaveItem(qsi->id);
	// qDebug() << "Updating saveitem " << qsi->id;
	if (!si)
	{
		qDebug() << "Error reading the save item during update" << qsi->id;
		return false;
	}
	else
	{
		si->partStatus[qsi->modPart] = qsi->modStatus;
		si->curPostLines = qsi->curPostLines;
		si->id = qsi->id;
		writeSaveItem(si);
        Q_DELETE(si);
		return true;
	}
}

bool SaveQItems::delSaveItem(int id)
{
	// qDebug() << "Deleting saveItem " << id;
	QSaveItem *si = readSaveItem(id);
    if (si)
    {
        if (si->group == "nzb")
        {
            emit deleteNzb(si->index);
        }
        Q_DELETE(si);
    }

	memset(&key, 0, sizeof(key));
	key.set_data(&id);
	key.set_size(szInt);

	if (saveQDb->del(0, &key, 0) != 0)
	{
		qDebug() << "Error deleting key from db";
		return false;
	}
	else
	{
		saveQDb->sync(0);
		return true;
	}
}

quint16 SaveQItems::saveItemSize(QSaveItem *si)
{
	return si->group.length() + si->index.length() + +si->rootFName.length()
			+ si->savePath.length() + si->tmpDir.length() + (5 * sizeof(quint16))
			+ (3 * sizeof(quint64))
			+ (si->partStatus.count() * sizeof(quint64)) + (si->partStatus.count() * sizeof(quint16))
			+ sizeof(quint16) + sizeof(quint16) //userGroupId and FileType
	;
}

char* SaveQItems::saveItemData(QSaveItem *si)
{
	char *p = new char[saveItemSize(si)];
	char *i = p;
	i = insert(si->group, i);
	i = insert(si->index, i);

	i = insert(si->rootFName, i);
	i = insert(si->savePath, i);
	i = insert(si->tmpDir, i);

	quint16 sz16 = sizeof(quint16);
	quint16 sz64 = sizeof(quint64);

	memcpy(i, &(si->curPostLines), sz64);
	i += sz64;

	//Now save the partMap...
	quint64 count = si->partStatus.count();
	memcpy(i, &count, sz64);
	i += sz64;

	quint64 id;
	quint16 status;
	QMap<quint64, quint16>::iterator it;
	for (it = si->partStatus.begin(); it != si->partStatus.end(); ++it)
	{
		id = it.key();
		status = it.value();
		memcpy(i, &id, sz64);
		i += sz64;
		memcpy(i, &status, sz16);
		i += sz16;
	}

	memcpy(i, &(si->userGroupId), sz16);
	i += sz16;

	memcpy(i, &(si->userFileType), sz16);
	i += sz16;

	return p;
}

QSaveItem* SaveQItems::loadSaveItem(char *data)
{
	char *i = &data[0];

	QSaveItem * si = new QSaveItem;

	quint16 sz16  = sizeof(quint16);
	quint16 sz64 = sizeof(quint64);

    i = retrieve(i, si->group);
    i = retrieve(i, si->index);

    i = retrieve(i, si->rootFName);
    i = retrieve(i, si->savePath);
    i = retrieve(i, si->tmpDir);

    memcpy(&(si->curPostLines), i, sz64);
    i += sz64;

    quint64 count;
    quint64 id;
    quint16 status;

    memcpy(&count, i, sz64);
    i += sz64;
    for (quint64 j = 0; j < count; j++)
    {
        memcpy(&id, i, sz64);
        i += sz64;
        memcpy(&status, i, sz16);
        i += sz16;
        si->partStatus.insert(id, status);
    }

	memcpy(&(si->userGroupId), i, sz16);
	i += sz16;

	memcpy(&(si->userFileType), i, sz16);
	i += sz16;

	return si;
}
