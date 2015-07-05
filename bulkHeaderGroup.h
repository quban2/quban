#ifndef BULKHEADERGROUP_H
#define BULKHEADERGROUP_H

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
#include <QObject>
#include <QString>
#include <QMap>
#include "common.h"
#include "newsgroup.h"
#include <QLinkedList>
#include <QMultiHash>

typedef QMap<QString, QString> HeaderGroupIndexes; // subj, headerGroup index
typedef QMap<QString, HeaderGroup*> HeaderGroups; //  headerGroup index, headerGroup *

typedef struct
{
    quint64         seq;
    NewsGroup*      ng;
} BulkHeaderGroupDetails;

class BulkHeaderGroup : public QObject
{
    Q_OBJECT
public:
    BulkHeaderGroup();
    virtual ~BulkHeaderGroup();

    volatile bool m_cancel;

private:
    void BulkGroup();
    bool BulkHeaderGroupBody();
    qint16 levenshteinDistance(const QString& a_compare1, const QString& a_compare2);
    HeaderGroup* getGroup(NewsGroup* ng, QString& articleIndex);

    bool running;

    Dbt key, data;
    uchar *keymem;
    uchar *datamem;

    QLinkedList<BulkHeaderGroupDetails*> jobList;
    QWidget *parent;
    BulkHeaderGroupDetails *job;

    quint32 bufSize;

public slots:
    void cancel(quint64);
    void addBulkGroup(quint64 seq, NewsGroup* ng);
    void shutdown();

signals:
    void bulkGroupFinished(quint64);
    void bulkGroupCancelled(quint64);
    void bulkGroupError(quint64);
    void updateJob(quint16 jobType, quint16 status, quint64 identifier);
    void updateJob(quint16 jobType, QString newDesc, quint64 identifier);
    void logMessage(int type, QString description);
    void logEvent(QString description);
    void saveGroup(NewsGroup*);
};

#endif // BULKHEADERGROUP_H
