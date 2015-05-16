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

#ifndef DECODEMANAGER_H_
#define DECODEMANAGER_H_

#include <QObject>
#include <QLinkedList>
#include "uudecoder.h"
#include "yydecoder.h"
#include "queueparts.h"

class Configuration;

class DecodeManager: public QObject
{
    Q_OBJECT
public:
	DecodeManager();
	virtual ~DecodeManager();
    volatile bool m_cancel;

protected:
	QLinkedList<QPostItem*> items;
	bool m_overWrite; //if false, rename


	QPostItem* item;
	Configuration* config;
	Decoder *decoder;
	QString destDir;
	int parts;

	Decoder* getDecoderForPost(QStringList);
	bool decode(QPostItem*);

public:
	bool getCancel() { return m_cancel;}
	QPostItem *getItem() { return item; }

private:
	void decodeList();

public slots:
	void setOverWrite(bool o) { m_overWrite = o;}
	void decodeItem(QPostItem*);
	void cancel(int qId);
    void cancel();

signals:
	void decodeStarted(QPostItem*);
	void decodeCancelled(QPostItem*);
	void decodeProgress(QPostItem*, int);
	void decodeFinished(QPostItem*, bool, QString, QString);
	void decodeDiskError();
    void logMessage(int type, QString description);
    void logEvent(QString description);
};

#endif /* DECODEMANAGER_H_ */
