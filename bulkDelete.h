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

#ifndef BULKDELETE_H_
#define BULKDELETE_H_

#include <QObject>
#include "common.h"
#include "newsgroup.h"
#include <QLinkedList>
#include <QMultiHash>

typedef struct
{
	quint16         type; // BULK_DELETE, AUTO_DELETE etc
	quint64         seq;
	NewsGroup*      ng;
	HeaderList*     headerList;
	QList<QString>* mphList;
	QList<QString>* sphList;
} BulkDeleteDetails;

class BulkDelete : public QObject
{
    Q_OBJECT
public:
	BulkDelete();
	virtual ~BulkDelete();

    volatile bool m_cancel;

	enum { BULK_DELETE, AUTO_DELETE };

private:
	void bulkDelete();
	bool bulkDeleteBody();
	void commonDelete(QString, bool);

	bool running;

	Dbt key, data;
	uchar *keymem;
	uchar *datamem;

	QLinkedList<BulkDeleteDetails*> jobList;
	QWidget *parent;
	BulkDeleteDetails *job;

	quint32 bufSize;

public slots:
    void cancel(quint64);
    void addBulkDelete(quint16, quint64 seq, NewsGroup* ng, HeaderList*, QList<QString>*, QList<QString>*);
    void shutdown();

signals:
	void bulkDeleteFinished(quint64);
	void bulkDeleteCancelled(quint64);
	void bulkDeleteError(quint64);
	void updateJob(quint16 jobType, quint16 status, quint64 identifier);
    void updateJob(quint16 jobType, QString newDesc, quint64 identifier);
	void logMessage(int type, QString description);
	void logEvent(QString description);
    void saveGroup(NewsGroup*);
};

#endif /* BULKDELETE_H_ */
