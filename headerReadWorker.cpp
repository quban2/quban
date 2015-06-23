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
#include <QStringBuilder>
#include <QDebug>
#include <QtDebug>
#include "headerReadWorker.h"
#include "zlib.h"

HeaderReadWorker::HeaderReadWorker(QMutex*_headerDbLock, QObject *parent) :
    QObject(parent)
{
    error = 0;
    lines = 0;
    groupArticles = 0;
    unreadArticles = 0;
    rejects=0;
    lineProcessed = 0;
    finishedReading = false;
    cacheFlushed = false;
    cache.clear();
    cache.reserve(CACHESIZE);

    headerDbLock = _headerDbLock;

    rx.setPattern("\\((\\d+)/(\\d+)\\)");

    tempng = 0;

    lineBufSize=1000;
    line=new char[lineBufSize];
    memset(line, 0, sizeof(lineBufSize));
}

HeaderReadWorker::~HeaderReadWorker()
{
    Q_DELETE_ARRAY(tempng);
    Q_DELETE_ARRAY(line);
}

void HeaderReadWorker::lock()
{
    busyMutex.lock();
}

char* HeaderReadWorker::m_findEndLine( char * start, char * end )
{
    char *p = 0;
    for (p=start; p < end; p++)
    {
        if (p[0] == '\r' &&
            p+1 < end && p[1] == '\n')
                return p;
    }
    return NULL;
}

HeaderBase* HeaderReadWorker::dbBinHeaderGet(QString index)
{
    Dbt key, data;
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    data.set_flags(DB_DBT_MALLOC);
    HeaderBase* hb = 0;
    MultiPartHeader *mph = 0;
    SinglePartHeader *sph = 0;
    char* dataBlock = 0;
    QByteArray ba = index.toLocal8Bit();
    const char *indexCharArr = ba.constData();
    key.set_data((void*) indexCharArr);
    key.set_size(index.length());

    if ((db->get(NULL, &key, &data, 0)) == 0)
    {
        dataBlock = (char*)data.get_data();
        if (*dataBlock == 'm')
        {
            mph = new MultiPartHeader(key.get_size(), (char*)key.get_data(), dataBlock);
            //qDebug() << "Just read mph with key " << mph->multiPartKey;
            hb = (HeaderBase*)mph;
        }
        else if (*dataBlock == 's')
        {
            sph = new SinglePartHeader(key.get_size(), (char*)key.get_data(), dataBlock);
            hb = (HeaderBase*)sph;
        }

        Q_FREE(dataBlock);
    }

    return hb;
}

bool HeaderReadWorker::dbBinHeaderPut(RawHeader* h)
{
    pos = 0;
    index = -1;

    NewsGroup* ng = job->ng;

    while ((pos = rx.indexIn(h->m_subj, pos)) != -1)
    {
        index = pos;
        pos += rx.matchedLength();
    }

    if (index == -1)
    {
        if (ng->areUnsequencedArticlesIgnored())
        {
            // qDebug() << "Can't find part and or total in header";
            return true;
        }
    }

    headerCache.enqueue(h);

    if (headerCache.count() > CACHEWATERMARK)
    {
        //Empty only half of the cache
        if (!cacheFlush(CACHEFLUSH))
            return false;
    }

    return true;
}

bool HeaderReadWorker::cacheFlush(uint size)
{
    Dbt key, data;
    int ret;
    RawHeader* h = 0;
    QString cIndex;
    NewsGroup* ng = job->ng;
    Db* pdb = ng->getPartsDb(); // get the parts Db
    MultiPartHeader* mph = 0;
    SinglePartHeader* sph = 0;
    HeaderBase* hb = 0;
    quint16 capPart = 0;
    quint16 capTotal = 0;

    if (size == 0)
        size = headerCache.count();

    qDebug() << "Flushing cache";
    qDebug() << "cacheIndex is " << headerCache.count() << " .Flushing " << size
            << " records";

    //Flush "size" elements, with a FIFO policy
    if ((int) size > headerCache.count())
    {
        qDebug() << "wrong flush size!";
        size = headerCache.count();
    }

    uint partsAdded = size;

    QMutexLocker locker(headerDbLock);

    quint64 mPHKey = ng->getMultiPartSequence();
    //qDebug() << "Multi Part Sequence in cache is " << mPHKey << " for host " << hostId;

    for (uint i = 0; i < size; i++)
    {
        h = headerCache.dequeue();

        if (h == 0) // didn't exist
        {
            //qDebug() << "Failed to find header in cache flush !! i = " << i << ", size = " << size;
            continue;
        }

        pos = 0;
        capPart = 0;
        capTotal = 0;
        index = -1;

        //qDebug() << "Subject in flush is " << h->m_subj;

        while ((pos = rx.indexIn(h->m_subj, pos)) != -1)
        {
            index = pos;
            pos += rx.matchedLength();
            capPart = rx.cap(1).toInt();
            capTotal = rx.cap(2).toInt();
        }

        if (index == -1) // It looks like a single part header
        {
            cIndex = h->m_subj.left(index).simplified() % '\n' % h->m_mid; // This is the common part of the subject + separator + msgId

            sph = (SinglePartHeader*)(cache.value(cIndex));
            if (sph) // This is a duplicate (We've already seen this subj and msgId for this server ...
            {
                partsAdded--;
                //qDebug() << "Duplicate article in cache : " << sph->getSubj() << "for server " << hostId;
            }
            else
            {
                //Try to get the header from the db...
                sph = (SinglePartHeader*)dbBinHeaderGet(cIndex);
                if (sph)
                {
                    if (!sph->isServerPresent(hostId)) // Article not currently registered to this server
                    {
                        sph->setServerPartNum(hostId, h->m_num);
                        cache.insert(cIndex, sph);
                        //qDebug() << "Server " << hostId << ", db find";
                    }
                    else // This is a duplicate (We've already seen this subj and msgId for this server ...
                    {
                        partsAdded--;
                        //qDebug() << "Duplicate article in db : " << sph->getSubj() << "for server " << hostId;
                    }
                }
                else
                {
                    //qDebug() << "Server " << hostId << ", sph create";

                    //Create new header and put it in the cache...
                    sph = new SinglePartHeader;
                    groupArticles++;
                    //The new article is, of course, unread...
                    unreadArticles++;

                    sph->setSubj(h->m_subj.left(index).simplified());
                    sph->setFrom(h->m_from);
                    sph->setStatus(MultiPartHeader::bh_new);
                    sph->setLines(h->m_lines.toLong());
                    sph->setSize(h->m_bytes.toLongLong());

                    if (h->m_date.isNull())
                        qDebug() << "Date is null!!!";
                    else
                        sph->setPostingDate(createDateTime(h->m_date.split(" ")));

                    sph->setDownloadDate(QDateTime::currentDateTime().toTime_t());

                    sph->setMessageId(h->m_mid);
                    sph->setServerPartNum(hostId, h->m_num);

                    cache.insert(cIndex, sph);

                    ng->setHeadersNeedGrouping(true);
                }
            }
        }
        else
        {
            cIndex = h->m_subj.left(index).simplified() % '\n' % h->m_from; // This is the common part of the subject + separator + sender

            mph = (MultiPartHeader*)(cache.value(cIndex));

            if (mph == 0)
            {
                //Try to get the header from the db...
                //mph = dynamic_cast<MultiPartHeader*>(dbBinHeaderGet(db, cIndex));
                mph = (MultiPartHeader*)(dbBinHeaderGet(cIndex));
                if (mph)
                {
                    //qDebug() << "Server " << hostId << ", db find";
                    //qDebug() << "Using mph with rec type " << mph->getHeaderType();
                    //qDebug() << "Using mph with key " << mph->multiPartKey;

                    //update the header in the cache...
                    switch (mph->addPart(pdb, capPart, h, hostId))
                    {
                        case MultiPartHeader::Duplicate_Part:
                        case MultiPartHeader::Error_Part:
                            partsAdded--;
                            break;
                        case MultiPartHeader::Unread_Status:
                            //Added a part that changed the status of the post...
                            unreadArticles++;
                            break;
                        case MultiPartHeader::New_Part:
                        case MultiPartHeader::Updated_Part:
                            break;
                    }
                    cache.insert(cIndex, mph);
                }
            }
            else
            {
                //qDebug() << "Server " << hostId << ", cache find";
                //qDebug() << "Using mph with key " << mph->multiPartKey;

                //update the header in the cache...
                switch (mph->addPart(pdb, capPart, h, hostId))
                {
                    case MultiPartHeader::Duplicate_Part:
                    case MultiPartHeader::Error_Part:
                        partsAdded--;
                        break;
                    case MultiPartHeader::Unread_Status:
                        //Added a part that changed the status of the post...
                        unreadArticles++;
                        break;
                    case MultiPartHeader::New_Part:
                    case MultiPartHeader::Updated_Part:
                        break;
                }
            }

            if (mph == 0)
            {
                //qDebug() << "Server " << hostId << ", mph create";

                //Create new header and put it in the cache...
                mph = new MultiPartHeader;
                groupArticles++;
                //The new article is, of course, unread...
                unreadArticles++;

                mph->setSubj(h->m_subj.left(index).simplified());
                mph->setFrom(h->m_from);
                mph->setStatus(MultiPartHeader::bh_new);

                mph->setKey(++mPHKey);
                //qDebug() << "Created mph with key " << mPHKey;

                if (h->m_date.isNull())
                    qDebug() << "Date is null!!!";
                else
                    mph->setPostingDate(createDateTime(h->m_date.split(" ")));

                mph->setDownloadDate(QDateTime::currentDateTime().toTime_t());
                mph->setNumParts(capTotal); // Also sets missing parts to capTotal
                mph->addPart(pdb, capPart, h, hostId);

                cache.insert(cIndex, mph);
                ng->setHeadersNeedGrouping(true);
            }
        }

        delete h;
    }

    // Multiple threads are still controlled by mutex at this point

    ng->servLocalParts[hostId] += size;
    qDebug() << "Server " << hostId << ", total articles = " << ng->totalArticles << ", adding " << groupArticles;
    ng->totalArticles += groupArticles;
    ng->unreadArticles += unreadArticles;
    qDebug() << "Server " << hostId << ", total articles = " << ng->totalArticles;
    ng->setMultiPartSequence(mPHKey);

    groupArticles = 0;
    unreadArticles = 0;

    // sync the parts db
    pdb->sync(0);

    //Flush the cache that we've just built
    QHash<QString, HeaderBase*>::iterator it = cache.begin();

    QByteArray ba;
    const char *cIndexCharArr;

    while (it != cache.end())
    {
        cIndex = it.key();
        hb = (HeaderBase*)(it.value());
        if (hb->getHeaderType() == 'm')
        {
            mph = (MultiPartHeader*)(it.value());
            data.set_data(mph->data());
            data.set_size(mph->getRecordSize());
            //qDebug() << "Just saved mph with key " << mph->multiPartKey;
        }
        else
        {
            sph = (SinglePartHeader*)(it.value());
            data.set_data(sph->data());
            data.set_size(sph->getRecordSize());
        }

        ba = cIndex.toLocal8Bit();
        cIndexCharArr = ba.constData();
        key.set_data((void*) cIndexCharArr);
        key.set_size(cIndex.length());
        ret = db->put(NULL, &key, &data, 0);
        if (ret != 0)
        {
            qDebug() << "Error flushing cache: " << ret;
            // errorString=g_dbenv->strerror(ret);
            errorString = "Failure whilst writing header record to database";
            return false;
        }

        void *ptr = data.get_data();
        Q_FREE(ptr);
        if (hb->getHeaderType() == 'm')
            delete mph;
        else
            delete sph;
        ++it;
    }

    locker.unlock(); // *************** this is a massive hold ***********************

    cache.clear();

    qDebug() << "Ok, cache flushed";

    cacheFlushed = true;

    return true;
}

uint HeaderReadWorker::createDateTime(QStringList dateTime)
{
    // Have seen totally invalid date formats here, so take care

    if (dateTime[0].length() == 0)
        return 0;

    int i = (dateTime[0].at(dateTime[0].length() - 1) == ',') ? 1 : 0;

    if (dateTime.count() < (i + 3))
        return 0;

    QDate d;

    if (dateTime[i].length() == 2)
        d = QDate::fromString(
                dateTime[i] + " " + dateTime[i + 1] + " " + dateTime[i + 2],
                "dd MMM yyyy");
    else if (dateTime[i].length() == 1)
        d = QDate::fromString(
                dateTime[i] + " " + dateTime[i + 1] + " " + dateTime[i + 2],
                "d MMM yyyy");
    else
        d = QDate::currentDate();

    QTime t(0, 0);

    if (dateTime.count() >= (i + 4))
        t = QTime::fromString(dateTime[i + 3]);

    QDateTime dt(d, t);

    return dt.toTime_t();
}

bool HeaderReadWorker::saveGroup()
{
    int tret;
    Dbt groupkey, groupdata;

    tempng = job->ng->data(tempng);
    groupdata.set_data(tempng);
    groupdata.set_size(job->ng->getRecordSize());

    QByteArray ba = job->ng->ngName.toLocal8Bit();
    const char *tempkey = ba.data();
    groupkey.set_data((void*) tempkey);
    groupkey.set_size(job->ng->ngName.length());

    if ((tret = job->gdb->put(NULL, &groupkey, &groupdata, 0)) != 0)
    {
        qDebug() << "Error updating newsgroup: %d" << tret;
        return false;
    }

    job->gdb->sync(0);

    return true;
}

/* report a zlib or i/o error */
void HeaderReadWorker::zerr(int ret)
{
    qDebug() << "inflate: ";
    switch (ret) {
    case Z_ERRNO:
        if (ferror(stdin))
            qDebug() << "error reading stdin";
        if (ferror(stdout))
            qDebug() << "error writing stdout";
        break;
    case Z_STREAM_ERROR:
        qDebug() << "invalid compression level";
        break;
    case Z_DATA_ERROR:
        qDebug() << "invalid or incomplete deflate data";
        break;
    case Z_MEM_ERROR:
        qDebug() << "out of memory";
        break;
    case Z_VERSION_ERROR:
        qDebug() << "zlib version mismatch!";
    }
}
