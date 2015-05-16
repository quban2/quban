#ifndef HEADERREADWORKER_H
#define HEADERREADWORKER_H

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
#include <QRegExp>
//#include <QAtomicInt>
#include "nntpthreadsocket.h"
#include "headerQueue.h"

class Sleeper : public QThread
{
public:
   void msleep(int ms) { QThread::msleep(ms); }
};


//CACHE PARAMETERS
#define CACHESIZE 28950
#define CACHEWATERMARK 28900
#define CACHEFLUSH 20501

class HeaderReadWorker : public QObject
{
    Q_OBJECT

public:
    explicit HeaderReadWorker(QMutex *_headerDbLock, QObject *parent = 0);
    ~HeaderReadWorker();

    bool        idle;
    bool        finishedReading;
    QMutex      busyMutex;

    int         error;
    QString     errorString;
    int         groupArticles;
    int         unreadArticles;
    int         rejects;
    quint64     maxHeaderNum;

protected:

    char* m_findEndLine( char * start, char * end );
    HeaderBase* dbBinHeaderGet(QString index);
    bool dbBinHeaderPut(RawHeader* h);
    bool cacheFlush(uint size);
    uint createDateTime(QStringList dateTime);
    bool saveGroup();

    Db* db;
    HeaderQueue <QByteArray*>* headerQueue;
    QRegExp rx;

    QQueue<RawHeader*> headerCache;
    QMultiHash<QString, HeaderBase*> cache;
    bool cacheFlushed;

    QMutex* headerDbLock;
    Job*  job;
    char* lineEnd;
    char* line;
    int lineBufSize;
    uint lines;
    uint articles;
    uint step;
    uint lineProcessed;
    int pos;
    int index;
    qint16 hostId;

    char* tempng;

public slots:
    void lock();

signals:
    void SigUpdate(Job *, uint, uint, uint);
    void sigHeaderDownloadProgress(Job*, quint64, quint64, quint64);

};

#endif // HEADERREADWORKER_H
