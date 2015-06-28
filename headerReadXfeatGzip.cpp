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
#include <assert.h>
#include <QDebug>
#include <QtDebug>
#include "header.h"
#include "headerReadXfeatGzip.h"
#include "zlib.h"

#define CHUNK 32768

HeaderReadXFeatGzip::HeaderReadXFeatGzip(HeaderQueue<QByteArray *> *_headerQueue, Job* _job, uint _articles, qint16 _hostId, QMutex* _headerDbLock) :
    HeaderReadWorker(_headerDbLock)
{
    hostId = _hostId;;
    headerQueue = _headerQueue;
    job = _job;
    articles = _articles;
    step=qMax(articles/100, (uint)1);
    db = job->ng->getDb();
    inflatedBuffer = new char[40 * 1024 * 1024];

    maxHeaderNum = job->ng->servLocalHigh[hostId];
}

HeaderReadXFeatGzip::~HeaderReadXFeatGzip()
{
    Q_DELETE_ARRAY(inflatedBuffer);
}

void HeaderReadXFeatGzip::startHeaderRead()
{
    RawHeader* h;
    QString headerLine;
    QByteArray* ba = 0;
    int ret = 0;
    int lineSize = 0;
    quint16 failures=0;

    static Sleeper slp;

    tempng = new char[job->ng->getRecordSize()];

    while ((ba = headerQueue->dequeue()) || finishedReading == false)
    {
        if (!ba) // nothing found but the provider hasn't finished yet
        {
            // sleep for 1 sec .....
            slp.msleep(1000);
            continue;
        }

        ret = inf((uchar*)ba->data(), ba->size(), inflatedBuffer, &destEnd);

        if (ret != Z_OK)
        {
            ++failures;
            zerr(ret);
            qDebug() << "Failed to inflate zipped data";

            if (failures > 20) // tolerate a low number of failures using this method ...
            {
                qDebug() << "Abandoning header download";
                error=100; // Unzip_Err
                errorString=tr("failed to inflate zipped data");
                Q_DELETE(ba);

                busyMutex.unlock();

                return;
            }
            else
            {
                Q_DELETE(ba);
                continue;
            }
        }

        *(destEnd + 1) = '\r';
        *(destEnd + 2) = '\n';
        destEnd += 2;

        tempInflatedBuffer = inflatedBuffer;

        do
        {
            if ((lineEnd=m_findEndLine(tempInflatedBuffer, destEnd)))
            {
                lineEnd[0]=0;
                lineEnd[1]=0;
                lineEnd+=2;

                lineSize=lineEnd-tempInflatedBuffer;

                if (lineSize > lineBufSize)
                {
                    lineBufSize=lineSize+1000;
                    Q_DELETE_ARRAY(line);
                    line=new char[lineBufSize];
                    memset(line, 0, sizeof(lineBufSize));
                }

                lines++;

                job->artSize++;

                if ((lines % step) == 0)
                    emit SigUpdate(job, lines, articles, 0); // MT ... QUpdItem::update won't work like this :(

                headerLine = QString::fromLocal8Bit(tempInflatedBuffer);
                tempInflatedBuffer += lineSize;

                h = new RawHeader(headerLine);

                lineProcessed++;

                if (h->isOk())
                {
                    if (dbBinHeaderPut(h) == false)
                    {
                        qDebug() << "Error inserting header!";
                        error = 90; //DbWrite_Err
                        errorString=tr("error inserting header");
                        Q_DELETE(ba);
                        busyMutex.unlock();

                        return;
                    }
                    else
                    {
                        maxHeaderNum = qMax((quint64)h->m_num, maxHeaderNum);

                        if (job->ng->servLocalLow[hostId] == 0)
                            job->ng->servLocalLow[hostId] = maxHeaderNum;

                        if (cacheFlushed == true)
                        {
                            cacheFlushed = false;
                            job->ng->servLocalHigh[hostId] = maxHeaderNum;

                            saveGroup(); // includes sync of group
                            db->sync(0); // sync the group headers

                            emit sigHeaderDownloadProgress(job, job->ng->servLocalLow[hostId],
                                    job->ng->servLocalHigh[hostId], job->ng->servLocalParts[hostId]);

                            failures=0; // reset for next block
                        }
                    }
                }
                else
                {
                    ++rejects;
                    qDebug() << "Bad header - maybe an incomplete line?";
                }
            }
            else
                break;

        } while (lineSize > 0);

        Q_DELETE(ba);
    }

    cacheFlush(0);

    if (cacheFlushed == true)
    {
        cacheFlushed = false;
        job->ng->servLocalHigh[hostId] = maxHeaderNum;

        saveGroup(); // includes sync of group
        db->sync(0); // sync the group headers

        emit sigHeaderDownloadProgress(job, job->ng->servLocalLow[hostId],
                job->ng->servLocalHigh[hostId], job->ng->servLocalParts[hostId]);
    }

    busyMutex.unlock();

    return;
}

int HeaderReadXFeatGzip::inf(uchar *source, uint bufferIndex, char *dest, char **destEnd)
{
    int ret;
    int avail_in;
    uint have;
    z_stream strm;

    char* localDest = dest;
    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit(&strm);
    if (ret != Z_OK)
        return ret;

    /* decompress until deflate stream ends or end of file */
    do {
        if (bufferIndex == 0)
            break;

        avail_in = qMin(bufferIndex, (uint)CHUNK);
        strm.avail_in = avail_in;
        if (strm.avail_in == 0)
            break;

        strm.next_in = source;
        source += avail_in;
        bufferIndex -= avail_in;

        /* run inflate() on input until output buffer not full */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = (uchar*)localDest;
            ret = inflate(&strm, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            switch (ret) {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;     /* and fall through */
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                (void)inflateEnd(&strm);
                return ret;
            }
            have = CHUNK - strm.avail_out;
            localDest += have;

        } while (strm.avail_out == 0);

        /* done when inflate() says it's done */
    } while (ret != Z_STREAM_END);

    *destEnd = localDest -1;

    /* clean up and return */
    (void)inflateEnd(&strm);
    return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

