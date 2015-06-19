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

#ifndef BULKLOAD_H_
#define BULKLOAD_H_

#include <QObject>
#include <QList>
#include <QString>
#include "common.h"
#include "newsgroup.h"
#include <QLinkedList>
#include <QMultiHash>

typedef struct
{
	quint64     seq;
	NewsGroup*  ng;
	HeaderList* headerList;
} BulkLoadDetails;

class BulkLoad : public QObject
{
    Q_OBJECT
public:
	BulkLoad();
	virtual ~BulkLoad();

    volatile bool m_cancel;

private:
	void bulkLoad();
	bool bulkLoadBody();

	bool running;

	char* keyBuffer;
	char* dataBuffer;
	DBT ckey, cdata;

	QLinkedList<BulkLoadDetails*> jobList;
	QWidget *parent;
	BulkLoadDetails *job;

	QList<QString>* mphDeletionsList;
	QList<QString>* sphDeletionsList;

public slots:
    void cancel(quint64);
    void addBulkLoad(quint64 seq, NewsGroup* ng, HeaderList*);
    void shutdown();

signals:
	void bulkLoadFinished(quint64);
	void bulkLoadCancelled(quint64);
	void bulkLoadError(quint64);
	void updateArticleCounts(NewsGroup*);
    void progress(quint64, quint64, QString);
	void loadStarted(quint64);
	void updateJob(quint16 jobType, quint16 status, quint64 identifier);
	void logMessage(int type, QString description);
	void logEvent(QString description);
	void addBulkJob(quint16, NewsGroup*, HeaderList*, QList<QString>*, QList<QString>*);
};

#endif /* BULKLOAD_H_ */
