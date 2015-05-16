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

#ifndef SAVEQITEMS_H_
#define SAVEQITEMS_H_

#include <QObject>
#include <QLinkedList>
#include <QMap>
#include <QList>
#include <QTimer>
#include <db_cxx.h>
#include "common.h"

struct QSaveItem
{
	//This should be enough to retrieve the binHeader from the Db...
	QString group;
	QString index;

	//Info about already saved parts
	QString rootFName;
	QString savePath;
	QString tmpDir;

	quint64 curPostLines;

	quint64 id;
	quint64 modPart;
	quint16 modStatus;
	quint16 type;

	quint16 userGroupId;
	quint16 userFileType;
	enum QSI_Type
	{
		QSI_Add, QSI_Update, QSI_Delete
	};

	//partID/status of the parts of the post.
	QMap<quint64, quint16> partStatus;
};

class SaveQItems: public QObject
{
    Q_OBJECT
public:
    SaveQItems(Db *qDb);
	virtual ~SaveQItems();
	quint16 saveItemSize(QSaveItem *);
	char* saveItemData(QSaveItem *);
    QSaveItem *loadSaveItem(char*);

protected:
	Dbt key, data;
	QList<QSaveItem*> saveQItems;
	QSaveItem *item;
	Db *saveQDb;

	bool processItem(QSaveItem *);
	QSaveItem* readSaveItem(int);
    bool writeSaveItem(QSaveItem *);
	bool updateItem(QSaveItem*);
	bool delSaveItem(int id);

signals:
	void deleteNzb(QString);
	void saveQError();

public slots:
    void addSaveItem(QSaveItem*);
private slots:
	void processAllItems();

private:
    QTimer startProcessingTimer;
};

#endif /* SAVEQITEMS_H_ */
