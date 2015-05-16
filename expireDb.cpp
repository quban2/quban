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
#include "expireDb.h"


ExpireDb::ExpireDb()
{
	m_cancel = false;
	running  = false;
}

ExpireDb::~ExpireDb()
{
}

void ExpireDb::expireNg(Job* j)
{
    jobList.append(j);

	if (!running)
	{
		running = true;
	    expireList();
	}
}

void ExpireDb::expireList()
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
		jId = job->id;

        qDebug() << "About to expire " << job->ng->getAlias();
		if (expire(job->ng, job->from, job->to))
		{
			qDebug() << "Group expire successful for " << job->ng->getAlias();
            if (!jobList.isEmpty())
                jobList.removeAll(jobList.first());
			emit expireFinished(job);
		}
		else if (m_cancel)
		{
			qDebug() << "Cancelled group expire for " << job->ng->getAlias();
            if (!jobList.isEmpty())
                jobList.removeAll(jobList.first());
			m_cancel = false;
			emit expireCancelled(job);
		}
		else
		{
			// Db error...
			qDebug() << "Group expire failed for " << job->ng->getAlias();
            if (!jobList.isEmpty())
                jobList.removeAll(jobList.first());
			//set the error and exit...
			emit expireError(job);
			break;
		}

		// 		qDebug("Updated, now the queue has %d items", jobList.count());
	}
	// 	qDebug("Update done");

	running = false;

	delete [] keymem;
	delete [] datamem;
}

bool ExpireDb::expire(NewsGroup* ng, uint expType, uint expValue)
{
	int retCode = 0;

	// belt and braces
	key.set_data(keymem);
	data.set_data(datamem);

	qDebug() << "Expire called with args: " << expType << ", " << expValue << " for " << ng->getName();

	if (m_cancel)
	{
		emit updateJob(JobList::Expire, JobList::Cancelled, jId);
		return false;
	}

	emit updateJob(JobList::Expire, JobList::Running, jId);

	QMap<quint16, quint16>::iterator spmit;
	QMap<quint16, quint64>::iterator lit;

	if (expType == EXPIREALL)
	{
		uint count;
		retCode = ng->getDb()->truncate(0, &count, 0);
        ng->setHeadersNeedGrouping(true);

		if (retCode == 0)
		{
            ng->setUnread(0);
            ng->setTotal(0);
            for (lit=ng->servLocalParts.begin(); lit!=ng->servLocalParts.end(); ++lit)
                ng->servLocalParts[lit.key()] = 0;

			for (lit=ng->servLocalLow.begin(); lit!=ng->servLocalLow.end(); ++lit)
			{
				if (ng->servLocalHigh.contains(lit.key()))
				    ng->servLocalLow[lit.key()] = ng->servLocalHigh[lit.key()];
			}

			qDebug() << "Truncated " << count << "items from " << ng->alias;
			emit updateJob(JobList::Expire, JobList::Finished_Ok, jId);
			return true;
		}
		else
		{
			qDebug() << "Failed to truncate " << ng->alias << ", error code: " << retCode;
			emit updateJob(JobList::Expire, JobList::Finished_Fail, jId);
			return false;
		}
	}

	QMap<quint16, bool>::iterator hit;

    // EXPIREBYDATE requires us to delete everything older than expValue days
	db  = ng->getDb();

	QDateTime hDate;
	QDateTime cDate = QDateTime::currentDateTime();
	Dbc *cursorp;

	low.clear();
	high.clear();
	servParts.clear();

	for (hit=ng->serverPresence.begin(); hit!=ng->serverPresence.end(); ++hit)
	{
		if (ng->serverPresence[hit.key()] == true)
		{
			low[hit.key()] = ng->servLocalHigh[hit.key()];
			high[hit.key()] = 0;
		    servParts [hit.key()] = 0;
		}
	}

	quint16 hostId;

	bool updateRequired = false;
	char *p;

	if (m_cancel)
	{
		emit updateJob(JobList::Expire, JobList::Cancelled, jId);
		return false;
	}

	quint16 commitPoint = 0;

	db->cursor(NULL, &cursorp, DB_WRITECURSOR);

	HeaderBase* hb = 0;
	MultiPartHeader *mph = 0;
	SinglePartHeader *sph = 0;
	char* dataBlock;
	QMap<quint16, quint64> serverLowest; // server, part

	int i =0;

	// Get the first record from the database
	while (retCode == 0)
	{
		if (i++ % 5 == 0)
			QCoreApplication::processEvents();

		if ((retCode = cursorp->get(&key, &data, DB_NEXT)) == 0)
		{
			if (m_cancel)
			{
				cursorp->close();
				emit updateJob(JobList::Expire, JobList::Cancelled, jId);
				return false;
			}

			dataBlock = (char*)data.get_data();

			if (*dataBlock == 'm')
			{
				mph = new MultiPartHeader(key.get_size(), (char*)key.get_data(), dataBlock);
				hb = (HeaderBase*)mph;
			}
			else if (*dataBlock == 's')
			{
				sph = new SinglePartHeader(key.get_size(), (char*)key.get_data(), dataBlock);
				hb = (HeaderBase*)sph;
			}

			// Do the check and delete here!!!

			if (expType == EXPIREBYPOSTINGDATE)
			{
				hDate.setTime_t(hb->getPostingDate());
				if (hDate.daysTo(cDate) > (int)expValue)
				{
                    ng->setHeadersNeedGrouping(true);

					if ((retCode = cursorp->del(0)) != 0)
					{
						qDebug() << "Failed to delete db record for key. Return code = " << retCode;
						cursorp->close();
						emit updateJob(JobList::Expire, JobList::Finished_Fail, jId);
						return false;
					}

					++commitPoint;
                    ng->decTotal();

                    if (hb->getStatusIgnoreMark() == HeaderBase::bh_new)
                        ng->decUnread();

				}
				else // being kept, it's a candidate for local LWM
				{
					serverLowest = hb->getServerLowest();
					for (lit=serverLowest.begin(); lit!=serverLowest.end(); ++lit)
					{
						low[lit.key()] = qMin(low[lit.key()], serverLowest[lit.key()]);
					}

					for (spmit=hb->serverPart.begin(); spmit!=hb->serverPart.end(); ++spmit)
					{
						servParts[spmit.key()] += hb->serverPart[spmit.key()];
					}
				}
			}
			if (expType == EXPIREBYDOWNLOADDATE)
			{
				hDate.setTime_t(hb->getDownloadDate());
				if (hDate.daysTo(cDate) > (int)expValue || hDate.daysTo(cDate) < 0)
				{
                    ng->setHeadersNeedGrouping(true);

					if ((retCode = cursorp->del(0)) != 0)
					{
						qDebug() << "Failed to delete db record. Return code = " << retCode;
						cursorp->close();
						emit updateJob(JobList::Expire, JobList::Finished_Fail, jId);
						return false;
					}

					++commitPoint;

                    ng->decTotal();

                    if (hb->getStatusIgnoreMark() == HeaderBase::bh_new)
                        ng->decUnread();
				}
				else // being kept, it's a candidate for local LWM
				{
					serverLowest = hb->getServerLowest();
					for (lit=serverLowest.begin(); lit!=serverLowest.end(); ++lit)
					{
						low[lit.key()] = qMin(low[lit.key()], serverLowest[lit.key()]);
					}

					for (spmit=hb->serverPart.begin(); spmit!=hb->serverPart.end(); ++spmit)
					{
						servParts[spmit.key()] += hb->serverPart[spmit.key()];
					}
				}
			}
			else if (expType == EXPIREALLFORSERVER)
			{
				hostId = expValue;

				if (hb->isServerPresent(hostId))
				{
					updateRequired = true;
					hb->removeServer(ng->getPartsDb(), hostId);
				}

				if (updateRequired)
				{
                    ng->setHeadersNeedGrouping(true);

					++commitPoint;

					updateRequired = false;
					if (hb->getMissingParts() >= hb->getParts()) // delete the current record
					{
                        ng->decTotal();

                        if (hb->getStatusIgnoreMark() == HeaderBase::bh_new)
                            ng->decUnread();

						if ((retCode = cursorp->del(0)) != 0)
						{
							qDebug() << "Failed to delete db record. Return code = " << retCode;
							cursorp->close();
							emit updateJob(JobList::Expire, JobList::Finished_Fail, jId);
							return false;
						}
					}
					else // modify it
					{
						p=hb->data();
						data.set_data(p);
						data.set_size(hb->getRecordSize());

						if ((retCode = cursorp->put(&key, &data, DB_CURRENT)) != 0)
						{
							qDebug() << "Failed to modify db record. Return code = " << retCode;
							delete [] p;
							cursorp->close();
							emit updateJob(JobList::Expire, JobList::Finished_Fail, jId);
							return false;
						}

						data.set_ulen(bufSize);
						data.set_data(datamem);
						delete [] p;

						serverLowest = hb->getServerLowest();
						for (lit=serverLowest.begin(); lit!=serverLowest.end(); ++lit)
						{
							low[lit.key()] = qMin(low[lit.key()], serverLowest[lit.key()]);
						}

						for (spmit=hb->serverPart.begin(); spmit!=hb->serverPart.end(); ++spmit)
						{
							servParts[spmit.key()] += hb->serverPart[spmit.key()];
						}
					}
				}
				else // being kept, it's a candidate for local LWM
				{
					serverLowest = hb->getServerLowest();
					for (lit=serverLowest.begin(); lit!=serverLowest.end(); ++lit)
					{
						low[lit.key()] = qMin(low[lit.key()], serverLowest[lit.key()]);
					}

					for (spmit=hb->serverPart.begin(); spmit!=hb->serverPart.end(); ++spmit)
					{
						servParts[spmit.key()] += hb->serverPart[spmit.key()];
					}
				}
			}
			else if (expType == EXPIRETEST)
			{
				serverLowest = hb->getServerLowest();
				for (lit=serverLowest.begin(); lit!=serverLowest.end(); ++lit)
				{
					low[lit.key()] = qMin(low[lit.key()], serverLowest[lit.key()]);
					high[lit.key()] = qMax(high[lit.key()], serverLowest[lit.key()] + hb->serverPart[lit.key()] - 1);
				}

				for (spmit=hb->serverPart.begin(); spmit!=hb->serverPart.end(); ++spmit)
				{
					servParts[spmit.key()] += hb->serverPart[spmit.key()];
				}
			}

			delete hb;

			if (commitPoint > 1000)
			{
				db->sync(0);
				commitPoint = 0;
			}
		}
		else if (retCode == DB_BUFFER_SMALL)
		{
			retCode = 0;
			qDebug("Insufficient memory");
			qDebug("Size required is: %d", data.get_size());
			uchar *p=datamem;
			datamem=new uchar[data.get_size()+1000];
			data.set_ulen(data.get_size()+1000);
			bufSize = data.get_size()+1000;
			data.set_data(datamem);
			delete [] p;
		}
	}

	if (commitPoint)
		db->sync(0);

    if (retCode != DB_NOTFOUND)
	{
		qDebug() << "Failed to find db record for delete. Return code = " << retCode;
		cursorp->close();
		emit updateJob(JobList::Expire, JobList::Finished_Fail, jId);
		return false;
	}

	cursorp->close();

	qDebug() << "Expire finished for " << ng->getName();

	if (m_cancel)
	{
		emit updateJob(JobList::Expire, JobList::Cancelled, jId);
		return false;
	}

	if (expType == EXPIREALLFORSERVER)
	{
		hostId = expValue;

        ng->serverPresence.remove(hostId);
        ng->serverRefresh.remove(hostId);
		low.remove(hostId);
		servParts.remove(hostId);

		ng->high.remove(hostId);
		ng->low.remove(hostId);
		ng->serverParts.remove(hostId);
		ng->servLocalHigh.remove(hostId);
	}

	ng->servLocalLow = low;
	if (expType == EXPIRETEST)
	    ng->servLocalHigh = high;
    ng->servLocalParts = servParts;

	emit updateJob(JobList::Expire, JobList::Finished_Ok, jId);
	return true;
}

void ExpireDb::cancel(int jID)
{
	if (job->id == jID)
	{
		m_cancel = true;
	}
	else
	{
		QLinkedList<Job*>::iterator it;
		for (it = jobList.begin(); it != jobList.end(); ++it)
		{
			if ((*it)->id == jID)
			{
				emit updateJob(JobList::Expire, JobList::Cancelled, jID);
				emit expireCancelled(*it);
				jobList.erase(it);
				break;
			}
		}
	}

	return;
}

void ExpireDb::shutdown()
{   
    QLinkedList<Job*>::iterator i = jobList.begin();
    while (i != jobList.end())
    {
        i = jobList.erase(i);
    }

	m_cancel = true;
}
