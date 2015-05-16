#ifndef HEADERLOAD_H
#define HEADERLOAD_H

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
#include <QAtomicInt>
#include <QRegExp>
#include "nntpthreadsocket.h"
#include "headerQueue.h"

//CACHE PARAMETERS
#define CACHESIZE 28950
#define CACHEWATERMARK 28900
#define CACHEFLUSH 20501

class HeaderLoad : public QObject
{
    Q_OBJECT

    HeaderBase* dbBinHeaderGet(QString index);

    bool dbBinHeaderPut(RawHeader* h);
    bool cacheFlush(uint size);
    bool saveGroup();
    uint createDateTime(QStringList dateTime);

    NewsGroup* ng;
    Job* job;
    Db* db;
    QRegExp rx;
    HeaderQueue<RawHeader*>* groupHeaderQueue;

    QQueue<RawHeader*> headerCache;
    QMultiHash<QString, HeaderBase*> cache;
    bool cacheFlushed;

    int pos;
    int index;

    uint articles;
    uint step;
    uint lineProcessed;

    char* tempng;

public:
    explicit HeaderLoad(NewsGroup* _ng, QObject *parent = 0);
    ~HeaderLoad();

    QAtomicInt  idle;
    QAtomicInt  finishedWriting;
    QAtomicInt  numWriters;
    QMutex      busyMutex;

    int         error;
    QString     errorString;
    int         groupArticles;
    int         unreadArticles;
    int         rejects;

signals:
    void SigUpdate(Job *, uint, uint, uint);
    void sigHeaderDownloadProgress(Job*, quint64, quint64, quint64);

public slots:
    void startHeaderLoad();

};

#endif // HEADERLOAD_H
