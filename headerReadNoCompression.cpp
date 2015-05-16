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
#include <QDebug>
#include <QtDebug>
#include "headerReadNoCompression.h"

HeaderReadNoCompression::HeaderReadNoCompression(HeaderQueue<QByteArray *> *_headerQueue, Job* _job, uint _articles, qint16 _hostId, QMutex* _headerDbLock) :
    HeaderReadWorker(_headerDbLock)
{
    hostId = _hostId;
    headerQueue = _headerQueue;
    job = _job;
    articles = _articles;
    step=qMax(articles/100, (uint)1);
    db = job->ng->getDb();

    maxHeaderNum = job->ng->servLocalHigh[hostId];
}

void HeaderReadNoCompression::startHeaderRead()
{
    RawHeader* h;
    QString headerLine;
    QByteArray* ba = 0;

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

        lines++;
        job->artSize++;
        if ((lines % step) == 0)
            emit SigUpdate(job, lines, articles, 0);  // MT ... QUpdItem::update won't work like this :(

        headerLine = QString::fromLocal8Bit(line);
        h = new RawHeader(headerLine);
        //qDebug() << "Subj: " << h->m_subj << "Read bytes " << h->m_bytes << ", lines " << h->m_lines;

        lineProcessed++;
        if (h->isOk())
        {
            if (dbBinHeaderPut(h) == false)
            {
                qDebug() << "Error inserting header!";
                error = 90; // DbWrite_Err
                errorString=tr("error inserting header");
                delete ba;
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
                }
            }
        }
        else
        {
            ++rejects;
            qDebug() << "Bad header - maybe an incomplete line?";
        }

        delete ba;
    }

    cacheFlush(0);

    idle = true;
    busyMutex.unlock();
}
