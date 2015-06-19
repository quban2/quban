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
#include <QMessageBox>
#include <QDateTime>
#include <QProgressDialog>
#include "MultiPartHeader.h"
#include "appConfig.h"
#include "quban.h"
#include "JobList.h"
#include "bulkLoad.h"

extern Quban* quban;

BulkLoad::BulkLoad()
{
	m_cancel = false;
	running  = false;
	job = 0;
}

BulkLoad::~BulkLoad()
{
}

void BulkLoad::addBulkLoad(quint64 seq, NewsGroup* ng, HeaderList* headerList)
{
	BulkLoadDetails* j = new BulkLoadDetails;
	j->seq = seq;
	j->ng  = ng;
	j->headerList = headerList;

    jobList.append(j);

	if (!running)
	{
		running = true;
	    bulkLoad();
	}
}

void BulkLoad::bulkLoad()
{
	keyBuffer = new char[HEADER_BULK_BUFFER_LENGTH];
	dataBuffer = new char[HEADER_BULK_BUFFER_LENGTH];

	while (!jobList.isEmpty())
	{
		job = jobList.first();

		if (bulkLoadBody())
		{
			qDebug() << "Group bulk load successful for " << job->ng->getAlias();
            if (!jobList.isEmpty())
                jobList.removeAll(jobList.first());
			emit bulkLoadFinished(job->seq);
		}
		else if (m_cancel)
		{
			qDebug() << "Cancelled group bulk load for " << job->ng->getAlias();
            if (!jobList.isEmpty())
                jobList.removeAll(jobList.first());
			m_cancel = false;
			emit bulkLoadCancelled(job->seq);
		}
		else
		{
			// Db error...
			qDebug() << "Group bulk load failed for " << job->ng->getAlias();
            if (!jobList.isEmpty())
                jobList.removeAll(jobList.first());
			//set the error and exit...
			emit bulkLoadError(job->seq);
			break;
		}

		// 		qDebug("Updated, now the queue has %d items", jobList.count());
	}
	// 	qDebug("Update done");

	running = false;

	delete [] (char*)dataBuffer;
	delete [] (char*)keyBuffer;
}

bool BulkLoad::bulkLoadBody()
{
	if (m_cancel)
	{
		emit updateJob(JobList::BulkLoad, JobList::Cancelled, job->seq);
		return false;
	}

	emit updateJob(JobList::BulkLoad, JobList::Running, job->seq);

	NewsGroup*  ng = job->ng;
	HeaderList* hl = job->headerList;

	mphDeletionsList = 0;
	sphDeletionsList = 0;

	DBC *dbcp;

    MultiPartHeader mph;
    SinglePartHeader sph;
    HeaderBase* hb = 0;

	memset(&ckey, 0, sizeof(ckey));
	memset(&cdata, 0, sizeof(cdata));

    cdata.data = (void *) dataBuffer;
    cdata.ulen = HEADER_BULK_BUFFER_LENGTH;
    cdata.flags = DB_DBT_USERMEM;

    ckey.data = (void *) keyBuffer;
    ckey.ulen = HEADER_BULK_BUFFER_LENGTH;
    ckey.flags = DB_DBT_USERMEM;

    size_t retklen, retdlen;
    void *retkey, *retdata;
    int ret, t_ret;
    void *p;

    quint64 count=0;

	QTime start = QTime::currentTime();
	qDebug() << "Loading started: " << start.toString();    

    emit loadStarted(job->seq);

    /* Acquire a cursor for the database. */
    if ((ret = ng->getDb()->get_DB()->cursor(ng->getDb()->get_DB(), NULL, &dbcp, DB_CURSOR_BULK)) != 0)
    {
    	ng->getDb()->err(ret, "DB->cursor");
        return false;
    }

    quint32 numIgnored = 0;
    bool    mphFound = false;

	for (;;)
	{
		/*
		 * Acquire the next set of key/data pairs.  This code
		 * does not handle single key/data pairs that won't fit
		 * in a BUFFER_LENGTH size buffer, instead returning
		 * DB_BUFFER_SMALL to our caller.
		 */
		if ((ret = dbcp->get(dbcp, &ckey, &cdata, DB_MULTIPLE_KEY | DB_NEXT)) != 0)
		{
			if (ret != DB_NOTFOUND)
				ng->getDb()->err(ret, "DBcursor->get");
			break;
		}

		for (DB_MULTIPLE_INIT(p, &cdata);;)
		{
			DB_MULTIPLE_KEY_NEXT(p, &cdata, retkey, retklen, retdata, retdlen);
			if (p == NULL)
				break;

            if (retdlen){;} // MD TODO compiler .... unused variable

			if (*((char *)retdata) == 'm')
			{                
                MultiPartHeader::getMultiPartHeader((unsigned int)retklen, (char *)retkey, (char *)retdata, &mph);
                hb = (HeaderBase*)&mph;

                mphFound = true;
			}
            else if (*((char *)retdata) == 's')
            {
                SinglePartHeader::getSinglePartHeader((unsigned int)retklen, (char *)retkey, (char *)retdata, &sph);
                hb = (HeaderBase*)&sph;

                mphFound = false;
                // qDebug() << "Single index = " << sph.getIndex();
            }
			else
			{
				// What have we found ?????
				qDebug() << "Found unexpected identifier for header : " << (char)*((char *)retdata);
				continue;
			}

            if (hb->getStatus() & HeaderBase::MarkedForDeletion)
			{
				// ignore this header, garbage collection will remove it ...
				++numIgnored;
				++count; // ... but still count it as bulk delete will reduce the count

				if (numIgnored == 1)
				{
					// These two will be freed by bulk delete
					mphDeletionsList = new QList<QString>;
					sphDeletionsList = new QList<QString>;
				}

				if (mphFound)
					mphDeletionsList->append(hb->getIndex());
				else
					sphDeletionsList->append(hb->getIndex());

				continue;
			}

            if (m_cancel)
                break;

            hl->headerTreeModel->setupTopLevelItem(hb);

			++count;

            if (count % 50 == 0)
            {
                QString labelText = tr("Loaded ") + QString("%L1").arg(count) +
                        " of " + QString("%L1").arg(ng->getTotal()) +
                        " articles.";
                emit progress(job->seq, count, labelText);
                QCoreApplication::processEvents();
            }
		}

		if (m_cancel)
			break;
	}

	if ((t_ret = dbcp->close(dbcp)) != 0)
	{
		ng->getDb()->err(ret, "DBcursor->close");
		if (ret == 0)
			ret = t_ret;
	}

    QString labelText = tr("Loaded ") + QString("%L1").arg(count) +
            " of " + QString("%L1").arg(ng->getTotal()) +
            " articles.";
    emit progress(job->seq, count, labelText);
    QCoreApplication::processEvents();

	if (!m_cancel)
	{
		quint64 totalArticles = ng->getTotal();
        if (totalArticles != count || ng->unread() > count)
		{
			qint64 difference = count - totalArticles; // may be negative
			quint64 unread = (quint64)qMax((qint64)(ng->unread() + difference), (qint64)0);

            if (unread > count)
                unread = count;

			ng->setTotal(count);
			ng->setUnread(unread);

			emit updateArticleCounts(ng);
		}
	}

	qDebug() << "Loading finished. Seconds: " << start.secsTo(QTime::currentTime());
	qDebug() << "Ignored " << numIgnored << " articles";
    qDebug() << "Loaded " << count << " articles";

	if (mphDeletionsList) // need bulk deletion
        emit addBulkJob(JobList::BulkDelete, ng, hl, mphDeletionsList, sphDeletionsList);

	if (m_cancel)
	{
		emit updateJob(JobList::BulkLoad, JobList::Cancelled, job->seq);
		return false;
	}

	emit updateJob(JobList::BulkLoad, JobList::Finished_Ok, job->seq);
	return true;
}

void BulkLoad::cancel(quint64 seq)
{
	if (job->seq == seq)
	{
		m_cancel = true;
	}
	else
	{
		QLinkedList<BulkLoadDetails*>::iterator it;
		for (it = jobList.begin(); it != jobList.end(); ++it)
		{
			if ((*it)->seq == seq)
			{
				emit updateJob(JobList::BulkLoad, JobList::Cancelled, seq);
				//emit bulkLoadCancelled(*it);
				jobList.erase(it);
				break;
			}
		}
	}

	return;
}

void BulkLoad::shutdown()
{
	QLinkedList<BulkLoadDetails*>::iterator it;
	for (it = jobList.begin(); it != jobList.end(); ++it)
		jobList.erase(it);

	m_cancel = true;
}
