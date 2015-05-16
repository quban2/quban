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

#ifndef JOBLIST_H_
#define JOBLIST_H_

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QIcon>
#include <QMultiMap>

struct ListedJob
{
	quint16 jobType;
    quint64 jobIdentifier;
	QTreeWidgetItem *job;
};

struct JobStatusDetail
{
	QIcon*  icon;
	QString desc;
};

class JobList : public QObject
{
	Q_OBJECT

public:
    JobList(QObject* p);
	virtual ~JobList();

    enum { Expire = 1, Repair, Uncompress, BulkDelete, BulkLoad, BulkHeaderGroup };
	enum { Queued = 1, Running, Paused, Cancelled, Finished_Ok, Finished_Fail };

public slots:

    void addJob(quint16 jobType, quint16 status, quint64 identifier, QString description);
    void updateJob(quint16 jobType, quint16 status, quint64 identifier);
    void updateJob(quint16 jobType, QString newDesc, quint64 identifier);
    //void slotPauseJob();
    //void slotResumeJob();
    void slotCancelJob();
    void slotDeleteJob();

signals:
    void cancelExpire(int);
    void cancelExternal(quint64);
    void cancelBulkDelete(quint64);
    void cancelBulkLoad(quint64);

private:
    void cancelJob(bool);

	QTreeWidget* jobTreeWidget;
	QMultiMap<quint64, struct ListedJob*>  jobMap;
	QMap<quint16, struct JobStatusDetail*> jobStatusMap;
	QMap<quint16, QString>                 jobTypeMap;
    QList<QIcon*>                          iconList;
};

#endif /* JOBLIST_H_ */
