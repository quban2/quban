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
#include "MultiPartHeader.h"
#include "appConfig.h"
#include "JobList.h"
#include "bulkDelete.h"

BulkDelete::BulkDelete()
{
	m_cancel = false;
	running  = false;
	job = 0;
}

BulkDelete::~BulkDelete()
{
}

void BulkDelete::addBulkDelete(quint16 type, quint64 seq, NewsGroup* ng, HeaderList* headerList, QList<QString>* mphList, QList<QString>* sphList)
{
	BulkDeleteDetails* j = new BulkDeleteDetails;
	j->type = type;
	j->seq = seq;
	j->ng  = ng;
	j->headerList = headerList;
	j->mphList = mphList;
	j->sphList = sphList;

    jobList.append(j);

	if (!running)
	{
		running = true;
	    bulkDelete();
	}
}

void BulkDelete::bulkDelete()
{
	keymem=new uchar[KEYMEM_SIZE];
	datamem=new uchar[DATAMEM_SIZE * 20];

	key.set_flags(DB_DBT_USERMEM);
	key.set_data(keymem);
	key.set_ulen(KEYMEM_SIZE);

	data.set_flags(DB_DBT_USERMEM);
	data.set_ulen(DATAMEM_SIZE * 20);
	bufSize = DATAMEM_SIZE * 20;
	data.set_data(datamem);

	while (!jobList.isEmpty())
	{
		job = jobList.first();

		if (bulkDeleteBody())
		{
			qDebug() << "Group bulk delete successful for " << job->ng->getAlias();
            if (!jobList.isEmpty())
                jobList.removeAll(jobList.first());
			emit bulkDeleteFinished(job->seq);
		}
		else if (m_cancel)
		{
			qDebug() << "Cancelled group bulk delete for " << job->ng->getAlias();
            if (!jobList.isEmpty())
                jobList.removeAll(jobList.first());
			m_cancel = false;
			emit bulkDeleteCancelled(job->seq);
		}
		else
		{
			// Db error...
			qDebug() << "Group bulk delete failed for " << job->ng->getAlias();
            if (!jobList.isEmpty())
                jobList.removeAll(jobList.first());
			//set the error and exit...
			emit bulkDeleteError(job->seq);
			break;
		}

		// 		qDebug("Updated, now the queue has %d items", jobList.count());
	}
	// 	qDebug("Update done");

	running = false;
    Q_DELETE_ARRAY(keymem);
    Q_DELETE_ARRAY(datamem);;
}

bool BulkDelete::bulkDeleteBody()
{
	// belt and braces
	key.set_data(keymem);
	data.set_data(datamem);

	if (m_cancel)
	{
		emit updateJob(JobList::BulkDelete, JobList::Cancelled, job->seq);
		return false;
	}

	emit updateJob(JobList::BulkDelete, JobList::Running, job->seq);

	QString index;

    NewsGroup* ng = job->ng;
    QList<QString>* mphList = job->mphList;
    QList<QString>* sphList = job->sphList;
    quint64 fullCount = sphList->size() + mphList->size();

    int j=0;
    int total=sphList->size() + mphList->size();

    for (int i=0; i<sphList->size(); ++i)
    {
        if (++j % 250 == 0)
            emit updateJob(JobList::BulkDelete, tr("Header bulk delete for newsgroup ") + job->ng->getAlias() + ": " +
                           QString::number(j) + " out of " + QString::number(total) + tr(" deleted"), job->seq);

		if (i % 5 == 0)
			QCoreApplication::processEvents();

    	if (m_cancel)
    		break;

    	index = sphList->at(i);

    	commonDelete(index, false);
    }

    Q_DELETE(sphList);

    // Now do the multi parts ...
    for (int i=0; i<mphList->size(); ++i)
    {
        if (++j % 250 == 0)
            emit updateJob(JobList::BulkDelete, tr("Header bulk delete for newsgroup ") + job->ng->getAlias() + ": " +
                          QString::number(j) + " out of " + QString::number(total) + tr(" deleted"), job->seq);

		if (i % 5 == 0)
			QCoreApplication::processEvents();

    	if (m_cancel)
    		break;

    	index = mphList->at(i);

    	commonDelete(index, true);
    }

    Q_DELETE(mphList);

    ng->articlesNeedDeleting(false);

    // Finally update the newsgroup
    emit saveGroup(ng);

    emit updateJob(JobList::BulkDelete, tr("Header bulk delete for newsgroup ") + job->ng->getAlias() + ": " +
                  QString::number(j) + " out of " + QString::number(total) + tr(" deleted"), job->seq);

	if (m_cancel)
	{
		emit updateJob(JobList::BulkDelete, JobList::Cancelled, job->seq);
		return false;
	}

	emit logEvent(tr("Bulk deletion of ") + fullCount + tr(" articles completed successfully."));

	emit updateJob(JobList::BulkDelete, JobList::Finished_Ok, job->seq);
	return true;
}

void BulkDelete::commonDelete(QString index, bool multiPart)
{
    QByteArray ba;
    const char *k = 0;
	Dbt dkey;

	int ret;
	quint16 serverId;

    MultiPartHeader *mph = 0;
    SinglePartHeader* sph = 0;
    HeaderBase* hb = 0;

	PartNumMap serverArticleNos;
	QMap<quint16, quint64> partSize;
	QMap<quint16, QString> partMsgId;

	ServerNumMap::Iterator servit;
	PartNumMap::iterator pnmit;

    NewsGroup* ng = job->ng;
    //HeaderList* hl = job->headerList;

    ba = index.toLocal8Bit();
	k= ba.data();

	memcpy(keymem, k, index.length());
	key.set_size(index.length());

	ret=ng->getDb()->get(NULL, &key, &data, 0);

	//qDebug() << "Get record returned " << ret << " for index " << index;

	if (ret == DB_BUFFER_SMALL)
	{
		//Grow key and repeat the query
		qDebug("Insufficient memory");
		qDebug("Size is: %d", data.get_size());
		uchar *p=datamem;
		datamem=new uchar[data.get_size()+1000];
		data.set_ulen(data.get_size()+1000);
		data.set_data(datamem);
        Q_DELETE_ARRAY(p);
		qDebug("Exiting growing array cycle");
		ret=ng->getDb()->get(0,&key, &data, 0);
	}

	if (ret==0)
	{
		if (multiPart)
		{
			mph = new MultiPartHeader(index.length(), (char*)k, (char*)datamem);
			hb = (HeaderBase*)mph;
			hb->getAllArticleNums(ng->getPartsDb(), &serverArticleNos, &partSize, &partMsgId);
		    mph->deleteAllParts(ng->getPartsDb());
		}
		else
		{
			sph = new SinglePartHeader(index.length(), (char*)k, (char*)datamem);
			hb = (HeaderBase*)sph;
			hb->getAllArticleNums(ng->getPartsDb(), &serverArticleNos, &partSize, &partMsgId);
		}

		ng->decTotal();
        if (hb->getStatusIgnoreMark() == HeaderBase::bh_new)
			ng->decUnread();

		// Don't need these two ...
		partSize.clear();
		partMsgId.clear();

		// walk through the parts ...
		for (pnmit = serverArticleNos.begin(); pnmit != serverArticleNos.end(); ++pnmit)
		{
			for (servit = pnmit.value().begin(); servit != pnmit.value().end(); ++servit)
			{
				serverId = servit.key();
				ng->reduceParts(serverId, 1);
				//qDebug() << "Reducing parts for server " << serverId << " by 1";
			}
		}

		serverArticleNos.clear();

        Q_DELETE(hb);

		memset(&dkey, 0, sizeof(dkey));
		dkey.set_data((void *)k);
		dkey.set_size(index.length());
		if ((ret = ng->getDb()->del(NULL, &dkey, 0)) != 0)
		{
			qDebug() << "Error deleting article, ret =  " << ret; // <<<<< mdm
		}
	}
}

void BulkDelete::cancel(quint64 seq)
{
	if (job->seq == seq)
	{
		m_cancel = true;
	}
	else
	{
		QLinkedList<BulkDeleteDetails*>::iterator it;
		for (it = jobList.begin(); it != jobList.end(); ++it)
		{
			if ((*it)->seq == seq)
			{
				emit updateJob(JobList::BulkDelete, JobList::Cancelled, seq);
				//emit bulkDeleteCancelled(*it);
				jobList.erase(it);
				break;
			}
		}
	}

	return;
}

void BulkDelete::shutdown()
{
	QLinkedList<BulkDeleteDetails*>::iterator it;
	for (it = jobList.begin(); it != jobList.end(); ++it)
		jobList.erase(it);

	m_cancel = true;
}
