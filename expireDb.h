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

#ifndef EXPIREDB_H_
#define EXPIREDB_H_

#include <QObject>
#include "common.h"
#include "newsgroup.h"
#include "nntpthreadsocket.h"
#include <QLinkedList>
#include <QMultiHash>

#define EXPIREBYPOSTINGDATE  2
#define EXPIREBYDOWNLOADDATE 3
#define EXPIREALL            4
#define EXPIREALLFORSERVER   5
#define EXPIRETEST           6

class ExpireDb: public QObject
{
    Q_OBJECT
public:
	ExpireDb();
	virtual ~ExpireDb();

    volatile bool m_cancel;

private:
	void expireList();
	bool expire(NewsGroup *ng, uint expType, uint expValue);
	uint createDateTime( QStringList dateTime);

	bool running;

	Dbt key, data;
	uchar *keymem;
	uchar *datamem;

	QMap<quint16, quint64> low; //holds serverId - LowWatermark
	QMap<quint16, quint64> high; //holds serverId - LowWatermark
	QMap<quint16, quint64> servParts; //holds serverId - numParts
	QLinkedList<Job*> jobList;
	QWidget *parent;
	Job *job;
	QFile *headerFile;
	QString line;
	Db* db;

	quint32 bufSize;

	quint64 jId;

public slots:
    void cancel(int jID);
    void expireNg(Job*);
    void shutdown();

signals:
	void expireFinished(Job*);
	void expireCancelled(Job*);
	void expireError(Job*);
	void updateJob(quint16 jobType, quint16 status, quint64 identifier);
	void logMessage(int type, QString description);
	void logEvent(QString description);
};

#endif /* EXPIREDB_H_ */
