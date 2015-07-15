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

#include "qmgr.h"
#include "grouplist.h"
#include <QMessageBox>
#include <QHeaderView>
#include <QMutexLocker>
#include <QRegExp>
#include <QFile>
#include <QDateTime>
#include <QDebug>
#include <QtDebug>
#include <QFileDialog>
#include <QCloseEvent>
#include <QtAlgorithms>
#include <QTemporaryFile>
#include "appConfig.h"
#include "autounpackconfirmation.h"
#include "logEventList.h"
#include "JobList.h"
#include "qubanDbEnv.h"
#include "qnzb.h"

extern Quban* quban;

QMgr::QMgr(Servers *_servers, Db *_gdb, QTreeWidget*_queueList, QTreeWidget*_doneList, QTreeWidget*_failedList) :
	queueList(_queueList), doneList(_doneList), failedList(_failedList), servers(_servers), gdb(_gdb)
{
	timer = 0;
	unpackConfirmation = 0;
	nzbGroupId = 0;
	decoding = false;
	nextItemId = 0;
	nextJobId = 0;
	operationsPending = 0;
	queueSize = 0;
	queueCount = 0;
	diskError = 0;

	maxGroupId = 0;
	maxPendingHeader = 0;

    getListJobs = 0;

	queueScheduler = quban->getQueueScheduler();

	// TODO	queueList->setColumnAlignment(4, Qt::AlignRight);

	//Initialise the List
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
	queueList->header()->setMovable(false);
#else
    queueList->header()->setSectionsMovable(false);
#endif
	queueList->setRootIsDecorated(true);
	queueList->setSortingEnabled(false);
	queueList->setAcceptDrops(true);
	queueList->setDragEnabled(false);
	queueList->setContentsMargins(3, 3, 3, 3);
	queueList->setAllColumnsShowFocus(true);
	queueList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	queueList->header()->resizeSection(3, 70);

	doneList->setRootIsDecorated(true);
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
    doneList->header()->setMovable(false);
#else
    doneList->header()->setSectionsMovable(false);
#endif
	doneList->setSortingEnabled(false);
	doneList->setAcceptDrops(false);
	doneList->setDragEnabled(false);
	doneList->setContentsMargins(3, 3, 3, 3);
	doneList->setAllColumnsShowFocus(true);
	doneList->setSelectionMode(QAbstractItemView::ExtendedSelection);

#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
    failedList->header()->setMovable(false);
#else
    failedList->header()->setSectionsMovable(false);
#endif
	failedList->setRootIsDecorated(true);
	failedList->setSortingEnabled(false);
	failedList->setAcceptDrops(false);
	failedList->setDragEnabled(false);
	failedList->setContentsMargins(3, 3, 3, 3);
	failedList->setAllColumnsShowFocus(true);
	failedList->setSelectionMode(QAbstractItemView::ExtendedSelection);

	//for every server, create the q, the qinfo and the qthreads...
    qRegisterMetaType<QList<QSslError> >();

	QMap<quint16, NntpHost*>::iterator sit;
	for (sit = servers->begin(); sit != servers->end(); ++sit)
	{
		pendingOperations[sit.key()] = 0;
		addServerQueue(sit.value());
	}

	decodeManager = new DecodeManager();
	connect(decodeManager, SIGNAL(decodeStarted(QPostItem*)), this, SLOT(decodeStarted(QPostItem*)));
	connect(decodeManager, SIGNAL(decodeCancelled(QPostItem*)), this, SLOT(decodeItemCanceled(QPostItem*)));
	connect(decodeManager, SIGNAL(decodeProgress(QPostItem*, int)), this, SLOT(decodeProgress(QPostItem*, int)));
	connect(decodeManager, SIGNAL(decodeFinished(QPostItem*, bool, QString, QString)), this, SLOT(decodeFinished(QPostItem*, bool, QString, QString)));
	connect(decodeManager, SIGNAL(decodeDiskError()), this, SLOT(writeError()));
    connect(decodeManager, SIGNAL(logMessage(int, QString)), quban->getLogAlertList(), SLOT(logMessage(int, QString)));
    connect(decodeManager, SIGNAL(logEvent(QString)), quban->getLogEventList(), SLOT(logEvent(QString)));
	connect(this, SIGNAL(setOverwrite(bool)), decodeManager, SLOT(setOverWrite(bool)));
	connect(this, SIGNAL(decodeItem(QPostItem*)), decodeManager, SLOT(decodeItem(QPostItem*)));
	connect(this, SIGNAL(cancelDecode(int)), decodeManager, SLOT(cancel(int)));
    connect(this, SIGNAL(sigShutdown()), decodeManager, SLOT(cancel()));
	decodeManager->moveToThread(&decodeManagerThread);
	connect(this, SIGNAL(quitDecode()), &decodeManagerThread, SLOT(quit()));

	config = Configuration::getConfig();
	emit setOverwrite(config->overwriteExisting);

	expireDb = 0;
	bulkDelete = 0;
	bulkLoad = 0;
    bulkGroup = 0;
	bulkSeq = 0;

    // this is not a seperate thread
	rateController = new RateController(queueScheduler->getIsRatePeriod(), queueScheduler->getSpeed());
	connect(this, SIGNAL(registerSocket(RcSslSocket*)), rateController, SLOT(addSocket(RcSslSocket*)));
	connect(this, SIGNAL(unregisterSocket(RcSslSocket*)), rateController, SLOT(removeSocket(RcSslSocket*)));
	connect(queueScheduler, SIGNAL(sigSpeedChanged(qint64)), rateController, SLOT(slotSpeedChanged(qint64)));
	connect(rateController, SIGNAL(sigNilPeriodChange(qint64, qint64)), this, SLOT(adviseAllThreads(qint64, qint64)));

	// 	connect(this, SIGNAL(newItem(int)), this, SLOT(slotProcessQ(int)));
	connect(this, SIGNAL(newItem(int )), this, SLOT(startAllThreads(int )));
	connect(this, SIGNAL(threadFinished()), this, SLOT(slotThreadFinished()));
    connect(this, SIGNAL(sigProcessJobs()), this, SLOT(slotProcessJobs()));
	connect(queueList, SIGNAL(itemSelectionChanged()), this,
			SLOT(slotSelectionChanged()));
	// TODO	LOOK AT THE HEADER ??? connect(queueList, SIGNAL(moved(QTreeWidgetItem*, QTreeWidgetItem*, QTreeWidgetItem*)),
	// TODO			this, SLOT(slotItemMoved(QTreeWidgetItem*, QTreeWidgetItem*, QTreeWidgetItem*)));
	connect(queueList, SIGNAL(collapsed(const QModelIndex &)), this,
			SLOT(slotQueueWidthChanged()));
	connect(queueList, SIGNAL(expanded(const QModelIndex &)), this,
			SLOT(slotQueueWidthChanged()));

	qnzb = new QNzb(this, config);

	connect(this, SIGNAL(sigUpdateServerExtensions(quint16)), this, SLOT(slotUpdateServerExtensions(quint16)));
}

QMgr::~QMgr()
{
	ThreadMap::iterator tit;
	QueueThreads::iterator qit;
	for (qit = threads.begin(); qit != threads.end(); ++qit)
	{
		for (tit = qit.value().begin(); tit != qit.value().end(); ++tit)
		{
			tit.value()->kill();
			// MD TODO tit.value()->closeCleanly();
            Q_DELETE(tit.value());
		}
		qit.value().clear();
	}
	threads.clear();

	autoFileDb->close(0);
    Q_DELETE(autoFileDb);

	pendDb->close(0);
    Q_DELETE(pendDb);

	unpackDb->close(0);
    Q_DELETE(unpackDb);

	if (expireDb)
	{
		if (expireDbThread.isRunning())
		{
            expireDb->m_cancel = true;
			//  need to tell the thread to finish as it may be waiting on the event loop
			emit quitExpireDb();
			expireDbThread.wait(1000); // give the thread one second
		}
        Q_DELETE(expireDb);
	}

	if (bulkDelete)
	{
		if (bulkDeleteThread.isRunning())
		{
            bulkDelete->m_cancel = true;
			//  need to tell the thread to finish as it may be waiting on the event loop
			emit quitBulkDelete();
			bulkDeleteThread.wait(1000); // give the thread one second
		}
        Q_DELETE(bulkDelete);
	}

	if (bulkLoad)
	{
		if (bulkLoadThread.isRunning())
		{
            bulkLoad->m_cancel = true;
			//  need to tell the thread to finish as it may be waiting on the event loop
			emit quitBulkLoad();
			bulkLoadThread.wait(1000); // give the thread one second
		}
        Q_DELETE(bulkLoad);
	}

    if (bulkGroup)
    {
        if (bulkGroupThread.isRunning())
        {
            bulkGroup->m_cancel = true;
            //  need to tell the thread to finish as it may be waiting on the event loop
            emit quitBulkGroup();
            bulkGroupThread.wait(1000); // give the thread one second
        }
        Q_DELETE(bulkGroup);
    }

    Q_DELETE(rateController);

	if (saveQThread.isRunning())
	{
		//  need to tell the thread to finish as it may be waiting on the event loop
		emit quitSaveQ();
		saveQThread.wait(2000); // give the thread two seconds
	}
    Q_DELETE(saveQItems);

	qDb->close(0);
    Q_DELETE(qDb);

    Q_DELETE(qnzb);
    Q_DELETE(nzbGroup);

	if (decodeManagerThread.isRunning())
	{
		//  need to tell the thread to finish as it may be waiting on the event loop
		emit quitDecode();
		decodeManagerThread.wait(2000); // give the thread two seconds
	}
    Q_DELETE(decodeManager);

	// free any group containers
	qDeleteAll(autoFiles);
	autoFiles.clear();

	qDeleteAll(pendingHeaders);
	pendingHeaders.clear();

	qDeleteAll(groupedDownloads);
	groupedDownloads.clear();

	qDeleteAll(queue);
	queue.clear();
}

void QMgr::slotShutdown()
{
    QueueThreads::iterator tit;
    ThreadMap::iterator it;
    uint serverId;

    for (tit = threads.begin(); tit != threads.end(); ++tit)
    {
        serverId = tit.key();
        for (it = threads[serverId].begin(); it != threads[serverId].end(); ++it)
        {
            it.value()->pause();
        }
    }

    emit sigShutdown();
    decodeManager->m_cancel = true;
}

void QMgr::slotUpdateServerExtensions(quint16 serverId)
{
	qDebug() << "Server extensions being updating for server " << serverId;

	nextItemId++;

    NntpHost* nh = servers->value(serverId);

	//create the item...
	QMutexLocker locker(&queueLock);

    queue.insert(nextItemId, new QExtensionsItem(nh->getName(), queueList, nextItemId));

	//create the part; create the job; add part/jobs to item...
    Part *tPart = 0;
    Job *tJob = 0;

	queueView.append(nextItemId);
	queueCount++;

	nextJobId++;
	//Create the job
	tJob = new Job;
	tJob->qId = serverId;
	tJob->gdb = gdb;
	tJob->id = nextJobId;
	tJob->jobType = Job::GetExtensions;
	tJob->status = Job::Queued_Job;
	tJob->fName = config->tmpDir + '/' + "extensions" + '.'
			+ QString::number(serverId);
	tJob->ng = 0;
	tJob->nh = nh;
	tJob->qItemId = nextItemId;
	tJob->partId = serverId;

	queue[nextItemId]->queuePresence[serverId] = true;
	queueItems[serverId].append(nextItemId);

	//create the part...
	tPart = new Part;
	tPart->desc = nh->getName();
	tPart->status = QItem::Queued_Part;
	tPart->jobId = nextJobId;
	tPart->job = tJob;
	tPart->qId = serverId;
	queue[nextItemId]->addJobPart(nextJobId, serverId, tPart);

	//move to head of Q, risky =:-|
	locker.unlock();
	slotItemMoved(queue[nextItemId]->listItem, 0, 0);
	locker.relock();

	emit queueInfo(queueCount, queueSize);
	locker.unlock();

    emit sigProcessJobs();
	// 	qDebug("Ok, q processing finished");

	slotQueueWidthChanged();
}

void QMgr::slotAddListItems(AvailableGroups* ag)
{
	QMap<quint16, NntpHost*>::iterator sit;

	for (sit = servers->begin(); sit != servers->end(); ++sit)
	{
		if (sit.value()->getServerType() == NntpHost::dormantServer)
			continue;

        slotAddListItem(sit.key(), ag);
	}
}

void QMgr::slotAddListItem(quint16 serverId, AvailableGroups* ag)
{
    ++getListJobs;
    NntpHost* nh = servers->value(serverId);

	nextItemId++;
	QMutexLocker locker(&queueLock);
    queue.insert(nextItemId, new QListItem(nh->getName(), queueList, nextItemId));
    Part *tPart = 0;
    Job *tJob = 0;

	queueView.append(nextItemId);
	queueCount++;

	nextJobId++;
	tJob = new Job;
	tJob->qId = serverId;
	tJob->id = nextJobId;
	tJob->jobType = Job::GetList;
	tJob->status = Job::Queued_Job;
    tJob->ag = ag;
    tJob->fName = QString::number(getListJobs);

	tJob->nh = nh;
	tJob->qItemId = nextItemId;
	tJob->partId = serverId;

	queue[nextItemId]->queuePresence[serverId] = true;
	queueItems[serverId].append(nextItemId);

	tPart = new Part;
	tPart->desc = nh->getName();
	tPart->status = QItem::Queued_Part;
	tPart->jobId = nextJobId;
	tPart->job = tJob;
	tPart->qId = serverId;
	queue[nextItemId]->addJobPart(nextJobId, serverId, tPart);

	//move to head of Q, risky =:-|
	locker.unlock();
	slotItemMoved(queue[nextItemId]->listItem, 0, 0);
	locker.relock();

	emit queueInfo(queueCount, queueSize);

	locker.unlock();

    emit sigProcessJobs();
	slotQueueWidthChanged();
}

void QMgr::slotexpire(NewsGroup * _ng, uint expType, uint expValue)
{
	// All expire requests go through here ..

	nextJobId++;
	//Create the job
	Job* tJob = new Job;
	tJob->qId = 0;
	tJob->gdb = gdb;
	tJob->id = nextJobId;
	tJob->jobType = Job::ExpireHead;
	tJob->status = Job::Queued_Job;
	tJob->ng = _ng;
	tJob->nh = 0;
	tJob->qItemId = 0;
	tJob->partId = 0;

	tJob->from = expType;
	tJob->to = expValue;

	if (expireDb == 0)
	{
		expireDb = new ExpireDb();
		connect(expireDb, SIGNAL(expireFinished(Job*)), this, SLOT(expireFinished(Job*)));
		connect(expireDb, SIGNAL(expireCancelled(Job*)), this, SLOT(expireFailed(Job*)));
		connect(expireDb, SIGNAL(expireError(Job*)), this, SLOT(expireFailed(Job*)));
		connect(expireDb, SIGNAL(logMessage(int, QString)), quban->getLogAlertList(), SLOT(logMessage(int, QString)));
		connect(expireDb, SIGNAL(logEvent(QString)), quban->getLogEventList(), SLOT(logEvent(QString)));
        connect(expireDb, SIGNAL(updateJob(quint16, quint16, quint64)), quban->getJobList(), SLOT(updateJob(quint16, quint16, quint64)));
		connect(this, SIGNAL(expireNg(Job*)), expireDb, SLOT(expireNg(Job*)));
		connect(this, SIGNAL(cancelExpire(int)), expireDb, SLOT(cancel(int)));
		connect(quban, SIGNAL(sigShutdown()), expireDb, SLOT(shutdown()));
		expireDb->moveToThread(&expireDbThread);
		connect(this, SIGNAL(quitExpireDb()), &expireDbThread, SLOT(quit()));
	}

	if (!expireDbThread.isRunning())
		expireDbThread.start();

    quban->getJobList()->addJob(JobList::Expire, JobList::Queued, tJob->id, tr("Header expire for newsgroup ") + _ng->getAlias());
	emit expireNg(tJob);
}

void QMgr::startBulkGrouping(NewsGroup* _ng)
{
    // All bulk grouping requests go through here ..
    bulkSeq++;
    _ng->setGroupingBulkSeq(bulkSeq);

    qDebug() << "In startBulkGrouping using seq: " << bulkSeq;

     if (bulkGroup == 0)
    {
        bulkGroup = new BulkHeaderGroup();
        connect(bulkGroup, SIGNAL(bulkGroupFinished(quint64)), this, SLOT(bulkGroupFinished(quint64)));
        connect(bulkGroup, SIGNAL(bulkGroupError(quint64)), this, SLOT(bulkGroupFailed(quint64)));
        connect(bulkGroup, SIGNAL(updateJob(quint16, quint16, quint64)), quban->getJobList(), SLOT(updateJob(quint16, quint16, quint64)));
        connect(bulkGroup, SIGNAL(updateJob(quint16, QString, quint64)), quban->getJobList(), SLOT(updateJob(quint16, QString, quint64)));
        connect(bulkGroup, SIGNAL(logMessage(int, QString)), quban->getLogAlertList(), SLOT(logMessage(int, QString)));
        connect(bulkGroup, SIGNAL(logEvent(QString)), quban->getLogEventList(), SLOT(logEvent(QString)));
        connect(this, SIGNAL(addBulkGroup(quint64, NewsGroup*)), bulkGroup, SLOT(addBulkGroup(quint64, NewsGroup*)));
        connect(this, SIGNAL(cancelBulkGroup(quint64)), bulkGroup, SLOT(cancel(quint64)));
        connect(quban, SIGNAL(sigShutdown()), bulkGroup, SLOT(shutdown()));
        connect(bulkGroup, SIGNAL(saveGroup(NewsGroup*)), gList, SLOT(saveGroup(NewsGroup*)));
        bulkGroup->moveToThread(&bulkGroupThread);
        connect(this, SIGNAL(quitBulkGroup()), &bulkGroupThread, SLOT(quit()));
    }

    if (!bulkGroupThread.isRunning())
        bulkGroupThread.start();

    quban->getJobList()->addJob(JobList::BulkHeaderGroup, JobList::Queued, bulkSeq, tr("Header bulk grouping for newsgroup ") + _ng->getAlias());
    emit addBulkGroup(bulkSeq, _ng);
}

quint64 QMgr::startBulkDelete(NewsGroup* _ng, HeaderList* headerList, QList<QString>* mphList, QList<QString>* sphList)
{
    // All bulk delete requests go through here ..

	bulkSeq++;

	if (bulkDelete == 0)
	{
		bulkDelete = new BulkDelete();
		connect(bulkDelete, SIGNAL(bulkDeleteFinished(quint64)), this, SLOT(bulkDeleteFinished(quint64)));
		connect(bulkDelete, SIGNAL(bulkDeleteError(quint64)), this, SLOT(bulkDeleteFailed(quint64)));
		connect(bulkDelete, SIGNAL(updateJob(quint16, quint16, quint64)), quban->getJobList(), SLOT(updateJob(quint16, quint16, quint64)));
        connect(bulkDelete, SIGNAL(updateJob(quint16, QString, quint64)), quban->getJobList(), SLOT(updateJob(quint16, QString, quint64)));
		connect(bulkDelete, SIGNAL(logMessage(int, QString)), quban->getLogAlertList(), SLOT(logMessage(int, QString)));
		connect(bulkDelete, SIGNAL(logEvent(QString)), quban->getLogEventList(), SLOT(logEvent(QString)));
        connect(this, SIGNAL(addBulkDelete(quint16, quint64, NewsGroup*, HeaderList*, QList<QString>*, QList<QString>*)),
				bulkDelete, SLOT(addBulkDelete(quint16, quint64, NewsGroup*, HeaderList*, QList<QString>*, QList<QString>*)));
		connect(this, SIGNAL(cancelBulkDelete(quint64)), bulkDelete, SLOT(cancel(quint64)));
		connect(quban, SIGNAL(sigShutdown()), bulkDelete, SLOT(shutdown()));
        connect(bulkDelete, SIGNAL(saveGroup(NewsGroup*)), gList, SLOT(saveGroup(NewsGroup*)));
		bulkDelete->moveToThread(&bulkDeleteThread);
		connect(this, SIGNAL(quitBulkDelete()), &bulkDeleteThread, SLOT(quit()));
	}

	if (!bulkDeleteThread.isRunning())
		bulkDeleteThread.start();

    quban->getJobList()->addJob(JobList::BulkDelete, JobList::Queued, bulkSeq, tr("Header bulk delete for newsgroup ") + _ng->getAlias());
	emit addBulkDelete(BulkDelete::BULK_DELETE, bulkSeq, _ng, headerList, mphList, sphList);

	return bulkSeq;
}

quint64 QMgr::startBulkLoad(NewsGroup* _ng, HeaderList* headerList)
{
	// All bulk delete requests go through here ..

	bulkSeq++;

	if (bulkLoad == 0)
	{
		bulkLoad = new BulkLoad();

		connect(bulkLoad, SIGNAL(bulkLoadFinished(quint64)), this, SLOT(bulkLoadFinished(quint64)));
		connect(bulkLoad, SIGNAL(bulkLoadFinished(quint64)), headerList, SLOT(bulkLoadFinished(quint64)));
        connect(bulkLoad, SIGNAL(bulkLoadCancelled(quint64)), headerList, SLOT(bulkLoadCancelled(quint64)));
		connect(bulkLoad, SIGNAL(bulkLoadError(quint64)), this, SLOT(bulkLoadFailed(quint64)));
		connect(bulkLoad, SIGNAL(bulkLoadError(quint64)), headerList, SLOT(bulkLoadFailed(quint64)));
		connect(bulkLoad, SIGNAL(updateArticleCounts(NewsGroup*)), quban->subGroupList, SLOT(saveGroup(NewsGroup*)));
        connect(bulkLoad, SIGNAL(progress(quint64, quint64, QString)), headerList, SLOT(slotProgress(quint64, quint64, QString)));
        connect(bulkLoad, SIGNAL(loadReady(quint64)), headerList, SLOT(slotBulkLoadReady(quint64)));
        connect(bulkLoad, SIGNAL(updateJob(quint16, quint16, quint64)), quban->getJobList(), SLOT(updateJob(quint16, quint16, quint64)));
		connect(bulkLoad, SIGNAL(logMessage(int, QString)), quban->getLogAlertList(), SLOT(logMessage(int, QString)));
        connect(bulkLoad, SIGNAL(logEvent(QString)), quban->getLogEventList(), SLOT(logEvent(QString)));
        connect(bulkLoad, SIGNAL(addBulkJob(quint16, NewsGroup*, HeaderList*, QList<QString>*, QList<QString>*)),
                      this, SLOT(addBulkJob(quint16, NewsGroup*, HeaderList*, QList<QString>*, QList<QString>*)));
		connect(this, SIGNAL(addBulkLoad(quint64, NewsGroup*, HeaderList*)), bulkLoad, SLOT(addBulkLoad(quint64, NewsGroup*, HeaderList*)));
		connect(this, SIGNAL(cancelBulkLoad(quint64)), bulkLoad, SLOT(cancel(quint64)));
		connect(quban, SIGNAL(sigShutdown()), bulkLoad, SLOT(shutdown()));
		bulkLoad->moveToThread(&bulkLoadThread);
		connect(this, SIGNAL(quitBulkLoad()), &bulkLoadThread, SLOT(quit()));
	}
    else
    {
        connect(bulkLoad, SIGNAL(bulkLoadFinished(quint64)), headerList, SLOT(bulkLoadFinished(quint64)));
        connect(bulkLoad, SIGNAL(bulkLoadCancelled(quint64)), headerList, SLOT(bulkLoadCancelled(quint64)));
        connect(bulkLoad, SIGNAL(bulkLoadError(quint64)), headerList, SLOT(bulkLoadFailed(quint64)));
        connect(bulkLoad, SIGNAL(progress(quint64, quint64, QString)), headerList, SLOT(slotProgress(quint64, quint64, QString)));
        connect(bulkLoad, SIGNAL(loadReady(quint64)), headerList, SLOT(slotBulkLoadReady(quint64)));
    }

	if (!bulkLoadThread.isRunning())
		bulkLoadThread.start();

    quban->getJobList()->addJob(JobList::BulkLoad, JobList::Queued, bulkSeq, tr("Header bulk load for newsgroup ") + _ng->getAlias());
	emit addBulkLoad(bulkSeq, _ng, headerList);

	return bulkSeq;
}

void QMgr::addBulkJob(quint16 jobType, NewsGroup* _ng, HeaderList* headerList, QList<QString>* mphList, QList<QString>* sphList)
{
	if (jobType == JobList::BulkDelete)
		headerList->bulkDeleteSeq = startBulkDelete(_ng, headerList, mphList, sphList);
	else
		headerList->bulkLoadSeq = startBulkLoad(_ng, headerList);
}

void QMgr::slotAddUpdItem(NewsGroup* ng, uint from, uint to)
{
	QMap<quint16, NntpHost*>::iterator sit;

	for (sit = servers->begin(); sit != servers->end(); ++sit)
	{
        if (sit.value()->getServerType() == NntpHost::dormantServer ||
                ng->downloadingServers.contains(sit.key()))
			continue;

		slotAddUpdItem(sit.key(), ng, from, to);
	}
}

void QMgr::slotAddUpdItem(quint16 hostId, NewsGroup* ng, quint64 from, quint64 to)
{
    Part *tPart = 0;
    Job *tJob = 0;

    NntpHost* nh = servers->value(hostId);

	nextItemId++;
	QMutexLocker locker(&queueLock);
    queue.insert(nextItemId, new QUpdItem(queueList, nextItemId, ng, nh->getName()));

	queueView.append(nextItemId);
	queueCount++;

	nextJobId++;
	//Create the job
	tJob = new Job;
	tJob->qId = hostId;
	tJob->gdb = gdb;
	tJob->id = nextJobId;
	tJob->jobType = Job::UpdHead;
	tJob->status = Job::Queued_Job;
	tJob->ng = ng;
	tJob->nh = nh;
	tJob->qItemId = nextItemId;
	tJob->partId = hostId;

	tJob->from = from;
	tJob->to = to;

	queue[nextItemId]->queuePresence[hostId] = true;
	queueItems[hostId].append(nextItemId);

	//create the part...
	tPart = new Part;
	tPart->desc = nh->getName();
	tPart->status = QItem::Queued_Part;
	tPart->jobId = nextJobId;
	tPart->job = tJob;
	tPart->qId = hostId;
	queue[nextItemId]->addJobPart(nextJobId, hostId, tPart);

	//move to head of Q, risky =:-|
	locker.unlock();
	slotItemMoved(queue[nextItemId]->listItem, 0, 0);
	locker.relock();

	emit queueInfo(queueCount, queueSize);
	locker.unlock();

    emit sigProcessJobs();

	slotQueueWidthChanged();
}

void QMgr::slotNzbFileDropped(QString nzbFileName)
{
	// Need a mechanism to handle multiple drops
	droppedNzbFiles.append(nzbFileName);

	if (unpackConfirmation == 0 && droppedNzbFiles.size() == 1) // no group operations in progress
	{
		qnzb->OpenNzbFile(nzbFileName);
	}
}

void QMgr::processNextDroppedFile()
{
	if (droppedNzbFiles.size() > 0)
	{
		droppedNzbFiles.removeFirst();
		if (droppedNzbFiles.size() > 0)
		{
			qnzb->OpenNzbFile(droppedNzbFiles.at(0));
		}
	}
}

void QMgr::slotGetGroupLimits(NewsGroup * _ng)
{
	qDebug() << "In QMgr::slotGetGroupLimits";
	nextItemId++;
	//create the item...
	QMutexLocker locker(&queueLock);

	queue.insert(nextItemId, new QGroupLimitsItem(queueList, nextItemId, _ng));

	//create the part; create the job; add part/jobs to item...
    Part *tPart = 0;
    Job *tJob = 0;

	queueView.append(nextItemId);
	queueCount++;
	QMap<quint16, NntpHost*>::iterator sit;
	for (sit = servers->begin(); sit != servers->end(); ++sit)
	{
		// 		qDebug("Serverid: %d", sit.key());
		queue[nextItemId]->queuePresence[sit.key()] = false;
	}

	for (sit = servers->begin(); sit != servers->end(); ++sit)
	{
        if (sit.value() && sit.value()->getServerType() == NntpHost::dormantServer)
			continue;

		if (_ng->serverPresence.contains(sit.key()) && _ng->serverPresence[sit.key()] == false)
			continue;

		nextJobId++;
		//Create the job
		tJob = new Job;
		tJob->qId = sit.key();
		tJob->gdb = gdb;
		tJob->id = nextJobId;
		tJob->jobType = Job::GetGroupLimits;
		tJob->status = Job::Queued_Job;
		tJob->fName = config->tmpDir + '/' + _ng->ngName + '.'
				+ QString::number(sit.key());
		tJob->ng = _ng;
		tJob->nh = sit.value();
		tJob->qItemId = nextItemId;
		tJob->partId = sit.key();

		if (queue[nextItemId]->queuePresence[sit.key()] == false)
		{
			queue[nextItemId]->queuePresence[sit.key()] = true;
			queueItems[sit.key()].append(nextItemId);
		}

		//create the part...
		tPart = new Part;
		tPart->desc = sit.value()->getName();
		tPart->status = QItem::Queued_Part;
		tPart->jobId = nextJobId;
		tPart->job = tJob;
		tPart->qId = sit.key();
		queue[nextItemId]->addJobPart(nextJobId, sit.key(), tPart);

		//move to head of Q, risky =:-|
		locker.unlock();
		slotItemMoved(queue[nextItemId]->listItem, 0, 0);
		locker.relock();
	}

	emit queueInfo(queueCount, queueSize);
	locker.unlock();

    emit sigProcessJobs();
	// 	qDebug("Ok, q processing finished");

	slotQueueWidthChanged();
}

void QMgr::slotGetGroupLimits(NewsGroup * _ng, quint16 hostId, NntpHost *nh)
{
	nextItemId++;
	//create the item...
	QMutexLocker locker(&queueLock);

	queue.insert(nextItemId, new QGroupLimitsItem(queueList, nextItemId, _ng));

	//create the part; create the job; add part/jobs to item...
    Part *tPart = 0;
    Job *tJob = 0;

	queueView.append(nextItemId);
	queueCount++;

	nextJobId++;
	//Create the job
	tJob = new Job;
	tJob->qId = hostId;
	tJob->gdb = gdb;
	tJob->id = nextJobId;
	tJob->jobType = Job::GetGroupLimits;
	tJob->status = Job::Queued_Job;
	tJob->fName = config->tmpDir + '/' + _ng->ngName + '.'
			+ QString::number(hostId);
	tJob->ng = _ng;
	tJob->nh = nh;
	tJob->qItemId = nextItemId;
	tJob->partId = hostId;

	queue[nextItemId]->queuePresence[hostId] = true;
	queueItems[hostId].append(nextItemId);

	//create the part...
	tPart = new Part;
	tPart->desc = nh->getName();
	tPart->status = QItem::Queued_Part;
	tPart->jobId = nextJobId;
	tPart->job = tJob;
	tPart->qId = hostId;
	queue[nextItemId]->addJobPart(nextJobId, hostId, tPart);

	//move to head of Q, risky =:-|
	locker.unlock();
	slotItemMoved(queue[nextItemId]->listItem, 0, 0);
	locker.relock();

	emit queueInfo(queueCount, queueSize);
	locker.unlock();

    emit sigProcessJobs();
	// 	qDebug("Ok, q processing finished");

	slotQueueWidthChanged();
}

void QMgr::slotAddPostItem(HeaderBase * hb, NewsGroup *ng, bool first, bool view, QString dDir, quint16 userGroupId, quint16 userFileType)
{
	// 	qDebug("Should add this job: %s", (const char *) bh->getSubj());

	//To keep track of the presence of an item in the queue...
	if (diskError)
		return;

	bool availableServer = false;
	NntpHost* nh = 0;

	QMapIterator<quint16, NntpHost *> i(*servers);
	while (i.hasNext())
	{
		i.next();
		nh = i.value();
		if (nh->getServerType() == NntpHost::activeServer || nh->getServerType()
				== NntpHost::passiveServer)
		{
			availableServer = true;
			break;
		}
	}

	if (availableServer == false) // no servers to pick up the job
	{
		qDebug("No available servers ...");
		return;
	}

	nextItemId++;
	//create the item...

	QSaveItem *qsi = new QSaveItem;

    Part *tPart = 0;
    Job *tJob = 0;
	int serverId = 0;
    QString tempDir = config->tmpDir + '/';

    QTemporaryFile* saveFile=new QTemporaryFile(config->tmpDir + "/Quban");
    saveFile->open();
    QString rootFName = saveFile->fileName();
    saveFile->setAutoRemove(false);  // NEED TO TIDY THIS UP AFTER DECODE ....
    saveFile->close();
    delete saveFile;

	queueSize += hb->getSize();
	queueCount++;

	QMutexLocker locker(&queueLock);
	QString dir = dDir.isNull() ? ng->getSaveDir() : dDir;
	qDebug() << "Save dir is " << dir << ", ng save dir = " << ng->getSaveDir()
			<< ", ng = " << ng->ngName;

    qsi->tmpDir = tempDir;
    qsi->tmpDir = rootFName;
	qsi->group = ng->getName();
	qsi->id = nextItemId;
	qsi->type = QSaveItem::QSI_Add;

	qsi->index = hb->getIndex();
	qsi->rootFName = rootFName;
	qsi->savePath = dir;
	qsi->curPostLines = 0;
	// These next two are for auto unpacking
	qsi->userGroupId = userGroupId;
	qsi->userFileType = userFileType;

	//typedef QMap<quint16,quint64> ServerNumMap;     // Holds server number, article num
	//typedef QMap<quint16, ServerNumMap> PartNumMap; //Hold <part number 1, 2 etc, <serverid, articlenum>>
	PartNumMap serverArticleNos;
	QMap<quint16, quint64> partSize;
	QMap<quint16, QString> partMsgId;

	hb->getAllArticleNums(ng->getPartsDb(), &serverArticleNos, &partSize, &partMsgId);

    PartNumMap::iterator pnmit;

    ServerNumMap::iterator snmit;

    for (pnmit = serverArticleNos.begin(); pnmit != serverArticleNos.end(); ++pnmit)
    {
        snmit =  pnmit.value().begin();
        while (snmit !=  pnmit.value().end())
        {
            if (!servers->contains(snmit.key()))
                snmit = pnmit.value().erase(snmit);
            else
                ++snmit;
        }
    }

	queue.insert(
			nextItemId, new QPostItem(queueList, nextItemId, hb, serverArticleNos, rootFName, dir, first, view, userGroupId, userFileType));

	connect(queue[nextItemId], SIGNAL(decodeMe(QPostItem*)), this, SLOT(addDecodeItem(QPostItem* )));
	connect(queue[nextItemId], SIGNAL(decodeNotNeeded(QPostItem*, bool, QString, QString)), this, SLOT(decodeFinished(QPostItem*, bool, QString, QString)));
    connect(queue[nextItemId], SIGNAL(addDataRead(quint16, int)), this, SLOT(addDataRead(quint16, int)));

	QMap<quint16, NntpHost*>::iterator sit; //Server iterator
	for (sit = servers->begin(); sit != servers->end(); ++sit)
		queue[nextItemId]->queuePresence[sit.key()] = false;

	// 	if (first)
	// 		queueView.prepend(nextItemId);
	// 	else queueView.append(nextItemId);
	queueView.append(nextItemId);


	// walk through the parts ...
	for (pnmit = serverArticleNos.begin(); pnmit != serverArticleNos.end(); ++(pnmit))
	{
		nextJobId++;
		tJob = new Job;

		// Choose the right server, based on priority....
		serverId = chooseServer(pnmit.value());
		if (serverId == 0) // no servers to pick up the job
		{
			qDebug("No server found ...");
			return;
		}
		(*servers)[serverId]->increaseQueueSize(partSize[pnmit.key()]);
		tJob->artSize = partSize[pnmit.key()];
		// 		qDebug("Added to serverId %d", serverId);
		// 		tJob->artSize=bh->partSize[bh->pnmit.key()];
		tJob->qId = serverId;
		tJob->id = nextJobId;
		tJob->jobType = Job::GetPost;
		tJob->nh = (*servers)[serverId]; //Got to choose the server....
		tJob->ng = ng;
		tJob->qItemId = nextItemId;
		tJob->partId = pnmit.key();
		tJob->status = Job::Queued_Job;
		tJob->artNum = pnmit.value()[serverId];
		tJob->tries = tJob->nh->getRetries();
		tJob->fName = rootFName + '.' + QString::number(tJob->partId);
		if (partMsgId.contains(pnmit.key()))
		{
			// 			qDebug() << "Have mid";
			tJob->mid = partMsgId[pnmit.key()];
			qDebug() << "Mid: " << tJob->mid;
		}
		else
		{
			tJob->mid = QString::null;
			// 			qDebug() << "Have number";
		}

		if (queue[nextItemId]->queuePresence[serverId] == false)
		{
			queue[nextItemId]->queuePresence[serverId] = true;
			queueItems[serverId].append(nextItemId);
		}

		tPart = new Part;
		tPart->desc = hb->getSubj() + " " + QString::number(pnmit.key())
				+ "/" + QString::number(hb->getParts());
		tPart->status = QItem::Queued_Part;
		tPart->jobId = nextJobId;
		tPart->job = tJob;
		tPart->qId = serverId;
		queue[nextItemId]->addJobPart(nextJobId, pnmit.key(), tPart);
		// 		queues[serverId].insert(nextJobId, tJob);

		//New for queue saving
		qsi->partStatus[pnmit.key()] = QItem::Queued_Part;

		// 		qDebug("Part: %d, ArtNum: %d, Queue: %d", tJob->partId, tJob->artNum, serverId);

	}

	if (first)
	{
		//risky =:-|
		locker.unlock();
		slotItemMoved(queue[nextItemId]->listItem, 0, 0);
		locker.relock();
	}

	emit (queueInfo(queueCount, queueSize));
	locker.unlock();
    emit sigProcessJobs();
	//Save the item in the queue db...
	writeSaveItem(qsi);

	// 	qDebug("addpost loop finished");
	slotQueueWidthChanged();
}

void QMgr::adviseAllThreads(qint64 oldSpeed, qint64 newSpeed)
{
	QueueThreads::iterator tit;
	ThreadMap::iterator it;
	uint serverId;

	for (tit = threads.begin(); tit != threads.end(); ++tit)
	{
		serverId = tit.key();
		for (it = threads[serverId].begin(); it != threads[serverId].end(); ++it)
		{
			// 		qDebug("Starting thread %d", it.key());
			it.value()->updateNilPeriod(oldSpeed, newSpeed);
		}
	}
}

int QMgr::chooseServer(ServerNumMap& snm)
{
	int maxPriority = 0;
	int workingPriority = 0, activePriority = 0, passivePriority = 0;

	quint64 maxSize = Q_UINT64_C(18446744073709551615);
	quint64 activeSize = Q_UINT64_C(0);
	quint64 passiveSize = Q_UINT64_C(0);

	int serverId = 0;
	NntpHost* nh = 0;
    quint16 serverPriority = 0;

	QMapIterator<quint16, NntpHost *> i(*servers);
	while (i.hasNext())
	{
		i.next();
		nh = i.value();
        serverPriority = nh->getPriority();
        if (nh->isPaused())
            serverPriority = 1;

        if (nh && nh->getServerType() == NntpHost::activeServer)
		{
            activePriority += serverPriority;
			activeSize += nh->getQueueSize();
		}
        else if (nh && nh->getServerType() == NntpHost::passiveServer)
		{
            passivePriority += serverPriority;
			passiveSize += nh->getQueueSize();
		}
	}

	nh = 0;
	if (activeSize == 0)
	{
		activeSize = 1;
		activePriority = 1;
	}

	ServerNumMap::iterator snmit;

	for (snmit = snm.begin(); snmit != snm.end(); ++snmit)
	{
        // we may have a stale server id saved away in error :(
        if (!servers->contains(snmit.key()))
            continue;

        // qDebug() << "Looking for server key " << snmit.key();
		//For every server, get its priority
		nh = (*servers)[snmit.key()];

        serverPriority = nh->getPriority();
        if (nh->isPaused())
            serverPriority = 1;

        if (nh && nh->getServerType() == NntpHost::activeServer)
		{
            if (((float) nh->getQueueSize() / activeSize) < ((float) serverPriority
					/ activePriority))
                workingPriority = serverPriority * 15;
			else
                workingPriority = serverPriority;

			//			qDebug() << "Looking at server " << nh->id << ", " << nh->name <<
			//					" with priority " << nh->priority << " and working priority "
			//					<< workingPriority;

			if (nh->getQueueSize() < maxSize && workingPriority > maxPriority)
			{
				maxPriority = workingPriority;
				serverId = nh->getId();
			}
		}
	}

	//	if (serverId != 0)
	//	    qDebug("Active serverId: %d got the job", serverId);

	if (serverId == 0) // couldn't find an active server ...
	{
		if (passiveSize == 0)
		{
			passiveSize = 1;
			passivePriority = 1;
		}

		for (snmit = snm.begin(); snmit != snm.end(); ++snmit)
		{
            // we may have a stale server id saved away in error :(
            if (!servers->contains(snmit.key()))
                continue;

			//For every server, get its priority
			nh = (*servers)[snmit.key()];
            serverPriority = nh->getPriority();
            if (nh->isPaused())
                serverPriority = 1;

            if (nh && nh->getServerType() == NntpHost::passiveServer)
			{
                if (((float) nh->getQueueSize() / passiveSize) < ((float) serverPriority
						/ passivePriority))
                    workingPriority = serverPriority * 15;
				else
                    workingPriority = serverPriority;

				if (nh->getQueueSize() < maxSize && workingPriority > maxPriority)
				{
					maxPriority = workingPriority;
					serverId = nh->getId();
				}
			}
		}

		//	if (serverId != 0)
		//	    qDebug("Passive serverId: %d got the job", serverId);
	}

	if (serverId == 0)
		qDebug("ServerId returned 0!!! Something's seriously wrong!");

	return serverId;
}

Job* QMgr::findNextJob(int qId)
{
	QList<int>::iterator it;
	QMap<int, Part*>::iterator pit;
    QItem *item = 0;
    Job *j = 0;
	//For every item...
	for (it = queueItems[qId].begin(); it != queueItems[qId].end(); ++it)
	{
		//for every part of the item...
		item = queue[*it];
		for (pit = item->parts.begin(); pit != item->parts.end(); ++pit)
		{
			//check every job of the item...
			j = pit.value()->job;
			if ((j->status == Job::Queued_Job) && (j->qId == qId))
			{
				if (!(j->jobType == Job::GetList) || !(j->ag->isUpdating()))
				{
					return j; //Returns the job pointer...
				}
			}
		}
	}

	return 0;
}

void QMgr::update(Job *j, uint partial, uint total, uint interval)
{
	QMutexLocker locker(&queueLock);

	queue[j->qItemId]->update(j->partId, partial, total, interval);
	locker.unlock();
}

void QMgr::updatePost(Job *j, uint partial, uint total, uint interval, uint serverId)
{
    QMutexLocker locker(&queueLock);

    queue[j->qItemId]->update(j->partId, partial, total, interval, serverId);
    locker.unlock();
}

void QMgr::start(Job * j)
{
	QMutexLocker locker(&queueLock);
	queue[j->qItemId]->start(j->partId);
	locker.unlock();
	threads[j->qId][j->threadId]->started();
	emit sigThreadStart(j->qId, j->threadId);
}

void QMgr::expiring(Job * j, int start)
{
	QMutexLocker locker(&queueLock);
	QUpdItem *item = (QUpdItem*) queue[j->qItemId];
	if (start)
		item->startExpiring(j->partId);
	else
		item->stopExpiring(j->partId);
	locker.unlock();
}

void QMgr::updateLimits(Job * j, uint lowWater, uint highWater, uint articleParts)
{
	qDebug() << "Thread returned the following group limits: name " << j->ng->name() << ", host " << j->nh->getName()
			<< "\n,low " << lowWater << ", high " << highWater << ", article parts " << articleParts;

	emit sigLimitsUpdate(j->ng, j->nh, lowWater, highWater, articleParts);
}

void QMgr::slotHeaderDownloadProgress(Job* j, quint64 lowWater, quint64 highWater, quint64 articleParts)
{
	emit sigHeaderDownloadProgress(j->ng, j->nh, lowWater, highWater, articleParts);
}

void QMgr::updateExtensions(Job * j, quint16, quint64 extensions)
{
	emit sigExtensionsUpdate(j->nh, extensions);
}

void QMgr::finished(Job * j)
{
	if (j->jobType != Job::ExpireHead)
	{
	    threads[j->qId][j->threadId]->resetRetryCounter();
	}

    QMutexLocker locker(&queueLock);

	if (j->jobType == Job::GetPost)
	{
		QPostItem *qp = (QPostItem*) queue[j->qItemId];
   //     qp->setFName(j->fName);
		qp->finished(j->partId);
		updateSaveItem(j->qItemId, j->partId, QItem::Finished_Part,
				qp->curPostLines);
		//Remove from the queue...
		(*servers)[j->qId]->decreaseQueueSize(j->artSize);
		// 		threads[j->qId][j->threadId]->stop();
		// 		emit sigThreadFinished(j->qId, j->threadId);

		// 		emit threadFinished();
	}
	else if (j->jobType == Job::UpdHead)
	{
		locker.unlock();
		emit sigUpdateFinished(j->ng);
		locker.relock();
		moveItem(queue[j->qItemId]);
	}
	else if (j->jobType == Job::ExpireHead)
	{
			emit sigUpdateFinished(j->ng);
	}
	else if (j->jobType == Job::GetGroupLimits)
	{
		if (queue[j->qItemId]->parts[j->partId]->status == QItem::Requeue_Part)
		{
			j->status = Job::Queued_Job;
			qDebug() << "Re-queueing update job";
			queue[j->qItemId]->requeue(j->partId);
		}
		else if (queue[j->qItemId]->finished(j->partId))
		{
			locker.unlock();
			emit sigLimitsFinished(j->ng);
			locker.relock();
			moveItem(queue[j->qItemId]);
		}
	}
	else if (j->jobType == Job::GetExtensions)
	{
		if (queue[j->qItemId]->parts[j->partId]->status == QItem::Requeue_Part)
		{
			j->status = Job::Queued_Job;
			qDebug() << "Re-queueing extensions job";
			queue[j->qItemId]->requeue(j->partId);
		}
		else if (queue[j->qItemId]->finished(j->partId))
		{
			moveItem(queue[j->qItemId]);
		}
	}
	else if (j->jobType == Job::GetList)
	{
        if (queue[j->qItemId]->finished(j->partId))
        {
            bool completelyFinished = true;

            if (--getListJobs > 0)
                completelyFinished = false;

            moveItem(queue[j->qItemId]);
            if (completelyFinished)
                emit groupListDownloaded();
        }
	}

    locker.unlock();

	if (j->jobType != Job::ExpireHead)
	{
        emit threadFinished();
	}

	// 	qDebug("Exiting QMgr::finished()");

}

void QMgr::cancel(Job *j)
{
    // 	j->status=Job::Cancelled_Job;
	// 	qDebug("Canceled message received");
	QMutexLocker locker(&queueLock);
	// 	if (j->jobType != Job::UpdHead)
	// 		threads[j->qId][j->threadId]->canceled();

	// 	emit sigThreadFinished(j->qId,j->threadId);
	emit sigThreadDisconnected(j->qId, j->threadId);
	queue[j->qItemId]->canceled(j->partId);
	if (j->jobType == Job::GetPost)
		(*servers)[j->qId]->decreaseQueueSize(j->artSize);

	if (queue[j->qItemId]->partsToDo == 0 || j->jobType != Job::GetPost )
	{
		if (j->jobType == Job::UpdHead)
		{
			locker.unlock();
			emit sigUpdateFinished(j->ng);
			locker.relock();
		}
		else if (j->jobType == Job::GetList)
		{
			j->ag->stopUpdating();
		}
		else if (j->jobType == Job::GetGroupLimits)
		{
			locker.unlock();
			emit sigLimitsFinished(j->ng);
			locker.relock();
		}

		moveCanceledItem(queue[j->qItemId], &locker);
	}
	locker.unlock();

    emit newItem(j->qId);
    //emit threadFinished();
	// 	qDebug("exiting QMgr::Cancelled");
}

void QMgr::comError(Job * j)
{
	//Release the job, so it's queueble by other threads
	QMutexLocker locker(&queueLock);
	queue[j->qItemId]->comError(j->partId);

	//Pause the thread...
	// 	threads[j->qId][j->threadId]->comError();
	locker.unlock();

    emit newItem(j->qId);
    //emit threadFinished();
}

void QMgr::downloadError(Job *j)
{
	//Only if response == nosuchgroup -> don't update the db!
	//Remove the file and move the item...
	qDebug("Download Error");
	QMutexLocker locker(&queueLock);
	if (j->fName != QString::null)
	    QFile::remove(j->fName);

	switch (queue[j->qItemId]->error(j->partId, j->error))
	{
	case QItem::Finished:
		locker.unlock();
		emit sigUpdateFinished(j->ng);
		locker.relock();
		moveItem(queue[j->qItemId]);

		break;
	case QItem::Continue:
		//Do nothing?
		break;
	}

	locker.unlock();
}

void QMgr::upDbErr(Job * j)
{
	if (j->fName != QString::null)
	    QFile::remove(j->fName);
	QMutexLocker locker(&queueLock);
	j->error = "Error updating Db!";

	switch (queue[j->qItemId]->error(j->partId, j->error))
	{
		case QItem::Finished:
			locker.unlock();
			emit sigUpdateFinished(j->ng);
			locker.relock();
			moveItem(queue[j->qItemId]);

			break;
		case QItem::Continue:
			//Do nothing?
			break;
	}

	locker.unlock();
}

void QMgr::error(Job * j)
{
	threads[j->qId][j->threadId]->resetRetryCounter();
	if (j->jobType == Job::GetPost)
		(*servers)[j->qId]->decreaseQueueSize(j->artSize);
	else if (j->jobType == Job::UpdHead || j->jobType == Job::GetGroupLimits) // should only be GetPost from now on
	{
		if (j->fName != QString::null)
		    QFile::remove(j->fName);
	}

	QMutexLocker locker(&queueLock);
	switch (queue[j->qItemId]->error(j->partId, j->error))
	{
	case QItem::Finished:
		//Move item out of the q
		if (j->jobType == Job::UpdHead || j->jobType == Job::GetList || j->jobType == Job::GetGroupLimits)
		{

			if (j->jobType == Job::UpdHead)
			{
				locker.unlock();
				emit sigUpdateFinished(j->ng);
				locker.relock();
			}
			else if (j->jobType == Job::GetGroupLimits)
			{
				locker.unlock();
				emit sigLimitsFinished(j->ng);
				locker.relock();
			}
			moveItem(queue[j->qItemId]);

		}
		break;
	case QItem::Continue:
		//Continue as usual, but save the status of the part...
		if (j->jobType == Job::GetPost)
			updateSaveItem(j->qItemId, j->partId, QItem::Failed_Part,
					((QPostItem*) queue[j->qItemId])->curPostLines);
		break;
	case QItem::RequeMe:
		//Delete the item, the job and requeue it...sic!
		qDebug() << "QMgr::error() : requeue!";
		requeue(j);
		break;
	default:
		qDebug() << "Error: should not be here";
		break;
	}
	locker.unlock();

    emit newItem(j->qId);
    //emit threadFinished();
}

void QMgr::slotProcessJobs()
{
    QMap<int, QueueList>::iterator it;

    for (it = queueItems.begin(); it != queueItems.end(); ++it)
    {
        if (!queueItems[it.key()].isEmpty())
            emit newItem(it.key()); //This calls slotProcessQueue. This starts all threads for the server ... unless they're already running
                                    // The number of queue items doesn't tell us the number of jobs unfortunately
    }
}

void QMgr::slotThreadFinished()
{
    // MD TODO this is redundant now ... I think
}

void QMgr::slotQueueWidthChanged()
{
	queueList->resizeColumnToContents(0);
	queueList->resizeColumnToContents(1);
	queueList->resizeColumnToContents(4);
}

void QMgr::addDecodeItem(QPostItem * item)
{
	if (!decodeManagerThread.isRunning())
		decodeManagerThread.start();

	emit decodeItem(item);
}

void QMgr::slotSelectionChanged()
{
	QList<QTreeWidgetItem*> selection = queueList->selectedItems();

	emit sigItemsSelected(!selection.isEmpty());
}

void QMgr::slotItemMoved(QTreeWidgetItem *item, QTreeWidgetItem* afterFirst,
		QTreeWidgetItem *afterNow)
{
	//Now I have to move only the pointers in the queueList, and not the jobs...pheww! :)

	QMutexLocker locker(&queueLock);
	int itemIndex = item->text(0).toInt();
	QItem *qItem = queue[itemIndex];
	int it;
	QMap<int, bool>::iterator sit;
	int newItemIndex;
	bool up;
	int qId;
	int afterIndex;

	int depth = 0;
	QTreeWidgetItem *depthCheck = item;
	while (depthCheck != 0)
	{
		depth++;
		depthCheck = depthCheck->parent();
	}

	if (depth != 1) // 1 signifies top level
	{
		locker.unlock();
		qDebug() << "Itemindex = " << itemIndex;
		qDebug() << "Failure: parent = " << item->parent() << ", Tree = "
				<< queueList;
		return;
	}

	if (afterNow)
	{ //Item is not in the first place of the list...
		depthCheck = afterNow;
		while (depthCheck != 0)
		{
			depth++;
			depthCheck = depthCheck->parent();
		}
		if (depth > 1) // 1 signifies top level
		{
			afterNow = afterNow->parent();
			afterNow->removeChild(item);
			queueList->addTopLevelItem(item);
			// TODO			item->moveItem(afterNow);
		}

		afterIndex = afterNow->text(0).toInt();
		// 		qDebug("Afterindex: %d", afterIndex);
		if (afterFirst)
		{
			// Find the index to decide if the item has been moved up or down
			int afterFirstIndex = afterFirst->text(0).toInt();
			if (queueView.indexOf(afterFirstIndex) > queueView.indexOf(
					afterIndex))
			{
				//moved up
				up = true;
				qDebug("Moved up");

			}
			else
			{
				up = false; //moved down
				qDebug("Moved down");
			}
		}
		else
			up = false; //AfterFirst= NULL -> Moved from the top of the list

		for (sit = qItem->queuePresence.begin(); sit
				!= qItem->queuePresence.end(); ++sit)
		{
			if (sit.value() == false)
				continue;
			//ok, now find where the hell I have to put the item....
			qId = sit.key();
			newItemIndex = findIndex(qId, afterIndex, up);
			// 			qDebug("NewItemIndex: %d", newItemIndex);

			if (newItemIndex != -1)
			{
				// 				qDebug("Queueindex != 0");
				queueItems[qId].removeAll(itemIndex);
				it = queueItems[qId].indexOf(afterIndex);
				++it;
				queueItems[qId].insert(it, itemIndex);
				// 				qDebug("Queue: %d", qId);
				// 				for (it=queueItems[qId].begin(); it != queueItems[qId].end(); ++it)
				// 					qDebug("%d", *it);

			}
			else if (up)
			{
				//Move to the top of the queue....
				queueItems[qId].removeAll(itemIndex);
				queueItems[qId].prepend(itemIndex);
				// 				for (it=queueItems[qId].begin(); it != queueItems[qId].end(); ++it)
				// 					qDebug("%d", *it);
			}
			else
			{
				//Move to the bottom...
				queueItems[qId].removeAll(itemIndex);
				queueItems[qId].append(itemIndex);
				// 				for (it=queueItems[qId].begin(); it != queueItems[qId].end(); ++it)
				// 					qDebug("%d", *it);
			}
		}

		queueView.removeAll(itemIndex);
		it = queueView.indexOf(afterIndex);
		++it;
		queueView.insert(it, itemIndex);
	}
	else
	{ //Item is first-in-the-list :)
	// 		qDebug("Moved to top");

		for (sit = qItem->queuePresence.begin(); sit
				!= qItem->queuePresence.end(); ++sit)
		{
			qId = sit.key();
			// 			qDebug("Presence in queue: %d", sit.key());
			// 			qDebug("Before moving:");
			if (qItem->queuePresence[qId] == true)
			{
				// 			for (it=queueItems[qId].begin(); it != queueItems[qId].end(); ++it)
				// 				qDebug("%d", *it);
				queueItems[qId].removeAll(itemIndex);
				queueItems[qId].prepend(itemIndex);
				// 			qDebug("After moving:");
				// 			qDebug("Queue %d:", sit.key());
				// 			for (it=queueItems[qId].begin(); it != queueItems[qId].end(); ++it)
				// 				qDebug("%d", *it);
			}
		}
		queueView.removeAll(itemIndex);
		queueView.prepend(itemIndex);
	}
	locker.unlock();
    //emit threadFinished();
}

int QMgr::findIndex(int qId, int itemAfter, bool up)
{
	if (queueItems[qId].contains(itemAfter))
	{
		//
		// 		qDebug("Ok, contained");
		return queueItems[qId].indexOf(itemAfter);

	}
	else
	{
		//Worst case:
		//we have to find the jobs travelling in the queue...
		//1: Find the item
		QListIterator<int> it(queueView);
		int currVal = 0;
		it.findNext(itemAfter);
		if (up)
		{
			while (it.hasPrevious())
			{
				currVal = it.previous();
				if (queueItems[qId].contains(currVal))
				{
					//FOUND! find last job & return it
					return queueItems[qId].indexOf(currVal);
				}

			}
		}
		else
		{
			while (it.hasNext())
			{
				currVal = it.next();
				if (queueItems[qId].contains(currVal))
				{
					return queueItems[qId].indexOf(currVal);
				}
			}
		}
		//If I reach this point, no jobs are found->return 0, meaning we got to move
		//The jobs to the top of the q...
		return -1;
	}
}

//Returns the index of the last item's job inside the queue...


void QMgr::downloadError(Job* job, int err)
{
	qDebug() << "download error";

	switch (err)
	{
	case NntpThread::Decode_Err:
		qDebug() << "Decode_Err";
		downloadError(job);
		break;
	case NntpThread::DbWrite_Err:
		qDebug() << "DbWrite_Err";
		downloadError(job);
		break;
	case NntpThread::NoSuchGroup_Err:
		qDebug() << "Nosuchgroup_err";
		downloadError(job);
		break;
	case NntpThread::Write_Err:
		//Argh...
		qDebug() << "Write error during header update";
		downloadError(job);
		//Pause the queue...
		writeError();
		break;
	default:
		qDebug() << "comErr while updating";
		comError(job);
		break;
	}
}

void QMgr::Err(Job* job, int err)
{
	// 					qDebug("Error received");
	switch (err)
	{
	case NntpThread::NoSuchArticle_Err:
	case NntpThread::NoSuchGroup_Err:
		// 							qDebug("Error: %d", nte->err);
		qDebug() << "QMgr::Err(): no such article error";
		error(job);
		break;
	case NntpThread::Write_Err:
		writeError(job);
		break;
	default:
		comError(job);
		break;
	}
}

void QMgr::Failed(Job* job, int err)
{
	QItem *item = queue[job->qItemId];

	queue.remove(item->qItemId);
	queueView.removeAll(item->qItemId);

	QTreeWidgetItem *temp = NULL, *child = NULL, *after = NULL;
	temp = new QTreeWidgetItem(failedList);
	temp->setText(0, QString::number(item->qItemId));
	temp->setText(1, item->listItem->text(1));
	temp->setText(2, "Job failed");

	QMap<int, Part*>::iterator pit;

	for (pit = item->parts.begin(); pit != item->parts.end(); ++pit)
	{
		if (temp->childCount() == 0)
			child = new QTreeWidgetItem(temp);
		else
			child = new QTreeWidgetItem(temp, after);
		if (item->queuePresence[pit.value()->qId] == true)
		{
			queueItems[pit.value()->qId].removeAll(item->qItemId);
			item->queuePresence[pit.value()->qId] = false;
		}

		child->setText(0, QString::number(pit.value()->jobId));
		child->setText(1, pit.value()->desc);

        switch (err)
        {
        case NntpThread::NoSuchArticle_Err:
        case NntpThread::NoSuchGroup_Err:
        case NntpThread::NoSuchMid_Err:
            child->setText(2, tr("Invalid group or article specified"));
            break;
        case NntpThread::Write_Err:
        case NntpThread::DbWrite_Err:
            child->setText(2, tr("Write error"));
            break;
        case NntpThread::Timeout_Err:
        case NntpThread::FatalTimeout_Err:
        case NntpThread::Comm_Err:
            child->setText(2, tr("Timeout error"));
            break;
        case NntpThread::Decode_Err:
            child->setText(2, tr("Decode error"));
            break;
        case NntpThread::Unzip_Err:
            child->setText(2, tr("Failed to inflate zipped data"));
            break;
        case NntpThread::Auth_Err:
            child->setText(2, tr("Password error"));
            break;
        case NntpThread::Connection_Err:
            child->setText(2, tr("Connection error"));
            break;
        default:
            child->setText(2, tr("Unspecified error: ") + err);
            break;
        }

		after = child;

		delete pit.value()->job;
		//delete the part...
		delete pit.value();
	}
	item->parts.clear();
	item->listItem->setSelected(false);
	queueCount--;
	delete item;
	emit queueInfo(queueCount, queueSize);
}

void QMgr::customEvent(QEvent * e)
{
    AutoUnpackEvent* aue = 0;
    AutoUnpackThread* unpackThread = 0;
	QString message;
	QString area;

	switch ((int) e->type())
	{

	case UNPACKTHREADEVENT:
		aue = (AutoUnpackEvent*) e;

		switch (aue->err)
		{
		case AutoUnpackEvent::AU_UNKNOWN_OPERATION:
		case AutoUnpackEvent::AU_UNFOUND_APP:
		case AutoUnpackEvent::AU_NO_AVAILABLE_APP:
		case AutoUnpackEvent::AU_UNPACK_FAILED:
		case AutoUnpackEvent::AU_REPAIR_FAILED:
		case AutoUnpackEvent::AU_FILE_OPEN_FAILED:
		case AutoUnpackEvent::AU_FILE_READ_FAILED:
		case AutoUnpackEvent::AU_FILE_WRITE_FAILED:
			qDebug()
					<< "Auto repair/unpack failed for group with master unpack file: "
					<< aue->groupManager->downloadGroup->masterUnpackFile;
			qDebug() << "Error was: " << aue->err;


			if (aue->groupManager->downloadGroup->groupStatus == QUBAN_GROUP_UNPACKING)
			{
				area = tr("unpack");
				quban->getJobList()->updateJob(JobList::Uncompress, JobList::Finished_Fail, aue->groupManager->getGroupId());
			}
			else
			{
				area = tr("repair");
			    quban->getJobList()->updateJob(JobList::Repair, JobList::Finished_Fail, aue->groupManager->getGroupId());
			}

            aue->groupManager->getWorkingThread()->deleteLater();
			aue->groupManager->setWorkingThread(0);

			aue->groupManager->setGroupStatus(QUBAN_GROUP_FAILED);
			aue->groupManager->dbSave();

			// Advise user of failure ...

			// TODO put more detail in the event and improve this
			switch (aue->err)
			{
			case AutoUnpackEvent::AU_UNKNOWN_OPERATION:
				message = tr(
						"Couldn't determine operation required on group files");
				break;
			case AutoUnpackEvent::AU_UNFOUND_APP:
				message = tr("Couldn't find an application to ") + area + tr(
						" group files");
				break;
			case AutoUnpackEvent::AU_NO_AVAILABLE_APP:
				message = tr("No applications available to ") + area + tr(
						" group files");
				break;
			case AutoUnpackEvent::AU_UNPACK_FAILED:
				message = tr("Failed to unpack group files, looking for repair files");
				emit sigStatusMessage(message);
				checkIfGroupDownloadOk((quint16)QUBAN_REPAIR, aue->groupManager);
				break;
			case AutoUnpackEvent::AU_REPAIR_FAILED:
				message = tr("Failed to repair group files, looking for more repair files");
				emit sigStatusMessage(message);
				checkIfGroupDownloadOk((quint16)QUBAN_REPAIR, aue->groupManager);
				break;
			case AutoUnpackEvent::AU_FILE_OPEN_FAILED:
				message = tr("Failed to open file during combine of split files");
				break;
			case AutoUnpackEvent::AU_FILE_READ_FAILED:
				message = tr("Failed to read file during combine of split files");
				break;
			case AutoUnpackEvent::AU_FILE_WRITE_FAILED:
				message = tr("Failed to write file during combine of split files");
				break;
			default:
				break;
			}

			// These errors have already displayed the message
			if (aue->err != AutoUnpackEvent::AU_UNPACK_FAILED && aue->err != AutoUnpackEvent::AU_REPAIR_FAILED)
				emit sigStatusMessage(message);
			break;

		case AutoUnpackEvent::AU_UNPACK_SUCCESSFUL:
			qDebug()
					<< "Auto unpack successful for group with master unpack file: "
					<< aue->groupManager->downloadGroup->masterUnpackFile;

			quban->getJobList()->updateJob(JobList::Uncompress, JobList::Finished_Ok, aue->groupManager->getGroupId());

			aue->groupManager->setGroupStatus(QUBAN_GROUP_SUCEEDED);
			aue->groupManager->dbSave();

			delete aue->groupManager->getWorkingThread();
			aue->groupManager->setWorkingThread(0);

			if (config->deleteGroupFiles)
			   removeAutoGroup(aue->groupManager);
			else // keep the group, but delete unwanted files
				removeAutoGroupFiles(aue->groupManager, true);

			emit sigStatusMessage(tr("Unpack completed successfully"));
			break;

		case AutoUnpackEvent::AU_REPAIR_SUCCESSFUL:
			qDebug()
					<< "Auto repair successful for group with master repair file: "
					<< aue->groupManager->downloadGroup->masterRepairFile;

			quban->getJobList()->updateJob(JobList::Repair, JobList::Finished_Ok, aue->groupManager->getGroupId());

			// Now try unpack if any packed files !!!
			if (aue->groupManager->getNumUnpackFiles())
			{
				aue->groupManager->setGroupStatus(QUBAN_GROUP_UNPACKING);
				aue->groupManager->dbSave();

				delete aue->groupManager->getWorkingThread();
				aue->groupManager->setWorkingThread(0);

				unpackThread = new AutoUnpackThread(aue->groupManager, this);
				emit sigExtProcStarted(aue->groupManager->getGroupId(), QUBAN_UNPACK, aue->groupManager->getMasterUnpack());
				aue->groupManager->setWorkingThread(unpackThread);
				unpackThread->processGroup();
			}
			else  // Just mark as complete
			{
				aue->groupManager->setGroupStatus(QUBAN_GROUP_SUCEEDED);
				aue->groupManager->dbSave();

				delete aue->groupManager->getWorkingThread();
				aue->groupManager->setWorkingThread(0);

				if (config->deleteGroupFiles)
				   removeAutoGroup(aue->groupManager);
				else // keep the group, but delete unwanted files
					removeAutoGroupFiles(aue->groupManager, true);

				emit sigStatusMessage(tr("Repair completed successfully"));
			}
			break;
		}
		break;

	default:
		break;
	}
}

/*
 void QMgr::slotThreadSpeedChanged( int serverId, int threadId, int speed)
 {
 emit sigSpeedChanged(serverId, threadId, speed);
 }*/

void QMgr::slotPauseSelected()
{
	QList<QTreeWidgetItem*> selection = queueList->selectedItems();
	QListIterator<QTreeWidgetItem*> it(selection);
	while (it.hasNext())
	{
		QMutexLocker locker(&queueLock);
		QItem *item = queue[it.next()->text(0).toInt()];
		QMap<int, Part*>::iterator pit;
		item->listItem->setText(2, "Paused");
		for (pit = item->parts.begin(); pit != item->parts.end(); ++pit)
		{

			Part *part = pit.value();
			if (part->job->status == Job::Queued_Job)
			{
				part->job->status = Job::Paused_Job;
				item->paused(part->job->partId); //ugly
			}
		}
		locker.unlock();
	}
}

void QMgr::slotResumeSelected()
{
	QList<QTreeWidgetItem*> selection = queueList->selectedItems();
	QListIterator<QTreeWidgetItem*> it(selection);
	while (it.hasNext())
	{
		QMutexLocker locker(&queueLock);
		QItem *item = queue[it.next()->text(0).toInt()];
		if (item->workingThreads == 0)
			item->listItem->setText(2, "Queued");

		QMap<int, Part*>::iterator pit;
		for (pit = item->parts.begin(); pit != item->parts.end(); ++pit)
		{
			Part *part = pit.value();
			if (part->job->status == Job::Paused_Job)
			{
				part->job->status = Job::Queued_Job;
				item->resumed(part->job->partId); //ugly
			}
		}
		locker.unlock();

	}
    emit sigProcessJobs();

}

void QMgr::slotCancelSelected()
{
	QList<QTreeWidgetItem*> selection = queueList->selectedItems();
	QListIterator<QTreeWidgetItem*> it(selection);

	QMutexLocker locker(&queueLock);
	while (it.hasNext())
	{
		QItem *item = queue[it.next()->text(0).toInt()];
        if (!item)    // attempt to cancel a subpart
            continue;
		QMap<int, Part*>::iterator pit;
		NewsGroup *ng = 0;
        Part *part = 0;
		if (item->type == Job::GetPost && item->processing)
		{
			//Item is in the decoding queue...
			emit cancelDecode(item->qItemId);
		}
		else
		{

			for (pit = item->parts.begin(); pit != item->parts.end(); ++pit)
			{
				// 			queueLock.lock();

				part = pit.value();

				if (part->job->status == Job::Queued_Job || part->job->status
						== Job::Paused_Job)
				{
                    part->job->status = Job::Cancelled_Job;
					if (part->job->jobType == Job::GetPost)
						(*servers)[part->job->qId]->decreaseQueueSize(part->job->artSize);
					item->canceled(part->job->partId); //ugly
					ng = part->job->ng;
                    // 				part->job->status=Job::Cancelled_Job;
                    // 				part->listItem->setText(2, "Cancelled job");
				}
				else if (part->job->status == Job::Processing_Job)
				{
					// 				qDebug("Job is isRunning: %d", part->job->id);
					// 				qDebug("cancelling thread...");
					part->listItem->setText(2, tr("Cancelling job..."));
					threads[part->job->qId][part->job->threadId]->threadCancel();
					emit sigThreadCanceling(part->job->qId, part->job->threadId);

				}
				else if (part->job->status == Job::Updating_Job)
				{
					qDebug() << "Cancelling item in update db status";
					emit cancelExpire(part->jobId);
				}
			}
			if (item->partsToDo == 0)
			{
				if (item->type == Job::UpdHead)
				{
					locker.unlock();
					emit sigUpdateFinished(ng);
					locker.relock();
				}
				else if (item->type == Job::GetGroupLimits)
				{
					locker.unlock();
					emit sigLimitsFinished(ng);
					locker.relock();
				}
				moveCanceledItem(item, &locker);
			}
		}
	}
	locker.unlock();
}

void QMgr::itemCanceling(QItem *item)
{
	item->listItem->setText(2, tr("Cancelling..."));
}

void QMgr::decodeItemCanceled(QPostItem * item)
{
	qDebug() << "Item id in customEvent: " << item->qItemId;

	QMutexLocker locker(&queueLock);
	moveCanceledItem(item, &locker);
	locker.unlock();

	decoding = false;
}

void QMgr::addDataRead(quint16 serverId, int bytesRead)
{
    NntpHost* nh = 0;

    if (servers->contains(serverId))
    {
        nh = (*servers)[serverId];
        if (nh->getServerLimitEnabled())
        {
            nh->incrementDownloaded(bytesRead);
            if (nh->isDownloadLimitExceeded())
            {
                serverDownloadsExceeded(serverId);
            }
        }
    }
}

void QMgr::serverDownloadsExceeded(quint16 serverId)
{
    NntpHost* nh = 0;

    if (servers->contains(serverId))
    {
        nh = (*servers)[serverId];

        // pause, green or amber cross icons etc
        nh->pauseServer();
    }
}

void QMgr::decodeFinished(QPostItem *item, bool ok, QString fileName, QString err)
{
	// 	item->decodeFinished();
	//Finished decoding, remove the item, delete the jobs, remove from the list...
	//ARGH!
	qDebug() << "QMgr::decodeFinished()";
	//Only if it's a nzb part...hmmmm
	// 	delNzbItem(item->post->getSubj().simplifyWhiteSpace() + item->post->getFrom());
	delSaveItem(item->qItemId);
	QMutexLocker locker(&queueLock);
	QString fileWithPath;
	if (!fileName.isNull())
	{
		fileWithPath = item->getSavePath();
		fileWithPath += fileName;
	}
	else
	{
		fileWithPath = tr("No file decoded");
	}
	//so I can unlock the queue before viewing the file...
	bool view = item->view;

	queue.remove(item->qItemId);

	// 	QValueList<int>::iterator it;
	// 	it=queueView.find(item->qItemId);
	queueView.removeAll(item->qItemId);

    QTreeWidgetItem *temp = 0, *child = 0, *after = 0;
	if (item->failedParts == 0 && ok)
	{
		temp = new QTreeWidgetItem(doneList);
	}
	else
	{
		temp = new QTreeWidgetItem(failedList);
	}
	temp->setText(0, QString::number(item->qItemId));
	temp->setText(1, item->listItem->text(1));
	if (ok)
		temp->setText(2, tr("Decoded to ") + fileWithPath);
	else
	{
		if (!fileName.isEmpty())
			temp->setText(2, tr("Error decoding ") + fileWithPath + ": " + err);
		else
			temp->setText(2, err);
	}

	if (item->failedParts == 0 && ok)
	{
		doneList->resizeColumnToContents(0);
		doneList->resizeColumnToContents(1);
		doneList->resizeColumnToContents(2);
	}
	else
	{
		failedList->resizeColumnToContents(0);
		failedList->resizeColumnToContents(1);
		failedList->resizeColumnToContents(2);
	}

    if (item->getFName() != QString::null)
        QFile::remove(item->getFName());

	QMap<int, Part*>::iterator pit;
	for (pit = item->parts.begin(); pit != item->parts.end(); ++pit)
	{
		//Delete from the queues...
		queueItems[pit.value()->qId].removeAll(item->qItemId);
		//delete the file

		QString file = item->getFName() + '.' + QString::number(pit.key());
		QFile::remove(file);

		//delete the jobs...
		// 		queues[pit.value()->qId].remove(pit.value()->job);
		if (temp->childCount() == 0)
			child = new QTreeWidgetItem(temp);
		else
			child = new QTreeWidgetItem(temp, after);

		child->setText(0, QString::number(pit.value()->jobId));
		child->setText(1, pit.value()->desc);
		if (pit.value()->status == QItem::Failed_Part)
			child->setText(2, pit.value()->job->error);
		else
			child->setText(2, "Done");
		after = child;
		delete pit.value()->job;
		//delete the part...
		delete pit.value();
	}
	item->parts.clear();

	//Debug!
	// 	QValueList<int>::iterator it;
	// 	qDebug("Items in queue:");
	// 	for (it=queueView.begin(); it != queueView.end(); ++it) {
	// 		qDebug("item: %d", *it);
	// 	}
	//End debug

	// 	delete item->listItem;
	// 	delete item->deletePost();


	item->listItem->setSelected(false);
	queueSize -= item->post->size;
	// 	delete item->post;
	queueCount--;

	locker.unlock();

	// Update the Group info if appropriate
	if (item->getUserGroupId() != 0)
	{
		// Find the group manager
		GroupManager* thisGroup = 0;

		thisGroup = groupedDownloads.value(item->getUserGroupId());

		 if (thisGroup)
		 {
			quint16 fileType = item->getUserFileType();
			QString savePath = item->getSavePath();
			qDebug() << "Setting save path to: " << savePath;
			thisGroup->setDirPath(savePath);

			AutoFile* autoFile = new AutoFile(autoFileDb, item->getUserGroupId(), fileType, fileName );
			autoFiles.insert(autoFile->getGroupId(), autoFile);

			if (fileType == QUBAN_MASTER_UNPACK || fileType == QUBAN_UNPACK)
			{
				if (fileType == QUBAN_MASTER_UNPACK)
				{
					qDebug() << "Setting master unpack file name to: " << fileName;
					thisGroup->setMasterUnpack(fileName);
					// The header subject may be in a strange format and we failed to determine the pack type and priority earlier
					if (thisGroup->getPackId() == 0)
					{
						thisGroup->setPackId(determinePackType(fileName, &thisGroup->downloadGroup->compressedFiles.unpackPriority));
					}
				}

				if (item->failedParts == 0 && ok)
					thisGroup->addSuccDecodeUnpack();
				else
				{
					thisGroup->addFailDecodeUnpack();
					thisGroup->addFailDecodeUnpackParts(item->failedParts);
				}

				if (thisGroup->getNumUnpackFiles() ==
					(thisGroup->getNumDecodedUnpackFiles() +
					 thisGroup->getNumFailedDecodeUnpackFiles() +
					 thisGroup->getNumCancelledUnpackFiles()))
				{
					qDebug() << "Ready to check group";
					checkIfGroupDownloadOk(fileType, thisGroup);
				}
				else
				{
					qDebug() << "Num unpack : " << thisGroup->getNumUnpackFiles();
					qDebug() << "Num decoded : " << thisGroup->getNumDecodedUnpackFiles();
					qDebug() << "Num failed decode : " << thisGroup->getNumFailedDecodeUnpackFiles();
					qDebug() << "Num cancelled : " << thisGroup->getNumCancelledUnpackFiles();
					thisGroup->dbSave();
				}
			}
			else if (fileType == QUBAN_MASTER_REPAIR || fileType == QUBAN_REPAIR)
			{
				if (fileType == QUBAN_MASTER_REPAIR)
				{
					qDebug() << "Setting master repair file name to: " << fileName;
					thisGroup->setMasterRepair(fileName);
					// The header subject may be in a strange format and we failed to determine the pack type and priority earlier
					if (thisGroup->getRepairId() == 0)
					{
						thisGroup->setRepairId(determineRepairType(fileName, &thisGroup->downloadGroup->repairFiles.repairPriority));
					}
				}

				if (item->failedParts == 0 && ok)
					thisGroup->addSuccDecodeRepair();
				else
				{
					thisGroup->addFailDecodeRepair();
					thisGroup->addFailDecodeRepairParts(item->failedParts);
				}
			}
			else if (fileType == QUBAN_SUPPLEMENTARY)
			{
				if (item->failedParts == 0 && ok)
					thisGroup->addSuccDecodeOther();
				else
				{
					thisGroup->addFailDecodeOther();
					thisGroup->addFailDecodeOtherParts(item->failedParts);
				}
			}

			if (fileType == QUBAN_MASTER_REPAIR || fileType == QUBAN_REPAIR || fileType == QUBAN_SUPPLEMENTARY)
			{
				if ((thisGroup->getNumReleasedRepairFiles() ==
					(thisGroup->getNumDecodedRepairFiles() + thisGroup->getNumFailedDecodeRepairFiles() +
					 thisGroup->getNumCancelledRepairFiles())) &&
					 (thisGroup->getNumOtherFiles() ==
					 (thisGroup->getNumDecodedOtherFiles() + thisGroup->getNumFailedDecodeOtherFiles() +
					 thisGroup->getNumCancelledOtherFiles()))
				    )
				{
					qDebug() << "Ready to check group";
					checkIfGroupDownloadOk(fileType, thisGroup);
				}
				else
				    thisGroup->dbSave();
			}
		 }
	}

	delete item;

	if (!ok && config->deleteFailed)
		QFile::remove(fileWithPath);
	else if (view && !fileName.isEmpty())
		emit (viewItem(fileWithPath));

	emit queueInfo(queueCount, queueSize);

	decoding = false;
}

void QMgr::moveItem(QItem *item)
{
	// 	qDebug("Moving");

	queue.remove(item->qItemId);
	// 	QValueList<int>::iterator it;
	// 	it=queueView.find(item->qItemId);
	queueView.removeAll(item->qItemId);

	QTreeWidgetItem *temp = NULL, *child = NULL, *after = NULL;
	if (item->failedParts == 0)
		temp = new QTreeWidgetItem(doneList);
	else
		temp = new QTreeWidgetItem(failedList);
	temp->setText(0, QString::number(item->qItemId));
	temp->setText(1, item->listItem->text(1));
	if (item->failedParts == 0)
		temp->setText(2, "Done");
	else
		temp->setText(2, QString::number(item->failedParts) + " Failed part(s)");

	QMap<int, Part*>::iterator pit;
	//Delete from the queues...

	for (pit = item->parts.begin(); pit != item->parts.end(); ++pit)
	{

		//delete the jobs...
		// 		queues[pit.value()->qId].remove(pit.value()->job);
		if (temp->childCount() == 0)
			child = new QTreeWidgetItem(temp);
		else
			child = new QTreeWidgetItem(temp, after);
		if (item->queuePresence[pit.value()->qId] == true)
		{
			queueItems[pit.value()->qId].removeAll(item->qItemId);
			item->queuePresence[pit.value()->qId] = false;
		}

		child->setText(0, QString::number(pit.value()->jobId));
		child->setText(1, pit.value()->desc);
		if (pit.value()->status == QItem::Failed_Part)
		{
			child->setText(2, pit.value()->job->error);
		}
		else
			child->setText(2, "Done");
		after = child;

		delete pit.value()->job;
		//delete the part...
		delete pit.value();
	}
	item->parts.clear();

	// 	delete item->listItem;
	// 	delete item->deletePost();
	item->listItem->setSelected(false);
	queueCount--;
	delete item;
	emit queueInfo(queueCount, queueSize);

}

void QMgr::requeue(Job * j)
{
	// 	qDebug("Requeueing!");
    QPostItem* item = 0;
	bool found = false;

	//Delete the job from the server queue

	int newServerId;
	// Check if there are other item's part on the server q. If not,
	// delete the item from the server's queue.
	item = (QPostItem*) queue[j->qItemId];
	QMap<int, Part*>::iterator pit;
	for (pit = item->parts.begin(); pit != item->parts.end(); ++pit)
	{
		if (pit.value()->qId == j->qId && pit.key() != j->partId)
		{
			found = true;
			break;
		}
	}

	if (!found)
	{
		qDebug() << "No other items found; removing " << item->qItemId;
		queueItems[j->qId].removeAll(item->qItemId);
		//serverPrecence on the item???
		item->queuePresence[j->qId] = false;
	} //else qDebug("Other items in this q, keeping...");


	bool availableServer = false;
	NntpHost* nh = 0;

	QMapIterator<quint16, NntpHost *> i(*servers);
	while (i.hasNext())
	{
		i.next();
		nh = i.value();
        if (j->qId != nh->getId() && (nh->getServerType() == NntpHost::activeServer ||
                nh->getServerType() == NntpHost::passiveServer))
		{
			availableServer = true;
			break;
		}
	}

	if (availableServer == false) // no servers to pick up the job
	{
		qDebug("No available servers ...");
		return;
	}

	ServerNumMap snm = item->getServerArticleNos().value(j->partId);
	newServerId = chooseServer(snm);
	qDebug("New serverId: %d", newServerId);
	if (newServerId == 0) // no servers to pick up the job
	{
		qDebug("No server found ...");
		return;
	}

	//Find the index for the new item....to do so, we:
	//First, check if there are other jobs for this item in the server's q.
	//If no job is found, we put the job immediately after the last job of
	//the previous item

	found = false;
	//Very boring, silly, and convoluted. But should work...
	if (item->queuePresence[newServerId] == false)
	{
		//Gotta add the item to the server's queue, possibly in the right place...
		// 		qDebug() << "Presence on new server: false\n";
		qDebug("Presence on new server: false");

		QListIterator<int> it(queueView);
		int currVal = 0;
		it.findNext(item->qItemId);

		while (it.hasPrevious())
		{
			// 			qDebug("Not first");
			qDebug() << "Finding in queueView?";
			currVal = it.previous();
			qDebug() << "Looking for " << currVal;
			if (queueItems[newServerId].contains(currVal))
			{
				// 				//Ok, got the item. Now find index and insert the item
				found = true;
				qDebug() << "Found in queueview";

				// 				newIndex=findLastJob(newServerId, queue[*it]);
				//Insert the item in the queueItems
				//Dangerous?

				int foundIt = queueItems[newServerId].indexOf(currVal);
				queueItems[newServerId].insert(foundIt, item->qItemId);
				// 				item->queuePresence[newServerId]=true;
				break;
			}
		}

		if (!found)
		{
			//insert at the top...
			// 			qDebug() << "Inserted at the top of the list\n";
			qDebug("Inserted at the top of the list");
			queueItems[newServerId].prepend(item->qItemId);
			// 			item->queuePresence[newServerId]=true;
			// 			QueueList::iterator qit;
		}

		item->queuePresence[newServerId] = true;
		QueueList::iterator qit;
		for (qit = queueItems[newServerId].begin(); qit
				!= queueItems[newServerId].end(); ++qit)
			qDebug() << "Queue: " << newServerId << " item: " << *qit;

	}
	else
		qDebug() << "item " << item->qItemId << "is already in queue "
				<< newServerId;

	//Now change the job (finally! :)

	j->qId = newServerId;
	j->nh = (*servers)[newServerId];
	j->tries = (*servers)[newServerId]->getRetries();
	j->nh->increaseQueueSize(j->artSize);
	j->artNum = snm.value(newServerId);
	queue[j->qItemId]->parts[j->partId]->qId = newServerId;
	queue[j->qItemId]->parts[j->partId]->status = QItem::Queued_Part;
	j->status = Job::Queued_Job;
	queue[j->qItemId]->parts[j->partId]->listItem->setText(2,
			"Requeued on " + (*servers)[j->qId]->getName());
	queue[j->qItemId]->parts[j->partId]->listItem->setText(3, "");

}

void QMgr::pauseQ(bool p)
{
	ThreadMap::iterator it;
	QueueThreads::iterator qit;

	if (p)
	{
		qDebug("Pausing queue");
		queuePaused = true;
		for (qit = threads.begin(); qit != threads.end(); ++qit)
		{
			for (it = qit.value().begin(); it != qit.value().end(); ++it)
				it.value()->pause();
		}
	}
	else
	{
		qDebug("Resume queue");
		queuePaused = false;

		for (qit = threads.begin(); qit != threads.end(); ++qit)
		{
			for (it = qit.value().begin(); it != qit.value().end(); ++it)
				it.value()->resume();
		}
		if (diskError)
		{
			//Start decode thread...
			if (!decodeManagerThread.isRunning())
				decodeManagerThread.start();
			diskError = false;
		}
		// 		emit threadFinished();
	}
}

void QMgr::moveCanceledItem(QItem *item, QMutexLocker* locker)
{
	// 	qDebug("MoveCanceledItem");
	queue.remove(item->qItemId);
	// 	QValueList<int>::iterator it;
	// 	it=queueView.find(item->qItemId);
	queueView.removeAll(item->qItemId);

	if (item->type == Job::GetPost)
	{
		//Delete files
		QFile::remove(((QPostItem*) item)->getFName());
		queueSize -= ((QPostItem*) item)->post->size;

		// 		delNzbItem(((QPostItem*)item)->post->getSubj().simplifyWhiteSpace() +
		// 				((QPostItem*) item)->post->getFrom());
		delSaveItem(item->qItemId);
	}
	queueCount--;

    QTreeWidgetItem *temp = 0, *child = 0, *after = 0;
	temp = new QTreeWidgetItem(failedList);
	temp->setText(0, QString::number(item->qItemId));
	temp->setText(1, item->listItem->text(1));
	temp->setText(2, tr("User Cancel"));

	QMap<int, Part*>::iterator pit;
	for (pit = item->parts.begin(); pit != item->parts.end(); ++pit)
	{
		//Delete from the queues...
		if (item->type == Job::GetPost)
		{
			QString fileName = ((QPostItem*) item)->getFName() + '.'
					+ QString::number(pit.key());
			QFile::remove(fileName);
		}
		queueItems[pit.value()->qId].removeAll(item->qItemId);
		//delete the jobs...
		// 		queues[pit.value()->qId].remove(pit.value()->job);
		if (temp->childCount() == 0)
			child = new QTreeWidgetItem(temp);
		else
			child = new QTreeWidgetItem(temp, after);

		child->setText(0, QString::number(pit.value()->jobId));
		child->setText(1, pit.value()->desc);
		child->setText(2, tr("Cancelled"));
		after = child;

		delete pit.value()->job;
		//delete the part...
		delete pit.value();
	}
	item->parts.clear();

	//Debug!
	// 	QValueList<int>::iterator it;
	// 	qDebug("Items in queue:");
	// 	for (it=queueView.begin(); it != queueView.end(); ++it) {
	// 		qDebug("item: %d", *it);
	// 	}
	//End debug


	// 	delete item->listItem;
	// 	delete item->deletePost();

	item->listItem->setSelected(false);

	// Update the Group info if appropriate
	if (item->getUserGroupId() != 0)
	{
		// Find the group manager
		GroupManager* thisGroup = 0;

		thisGroup = groupedDownloads.value(item->getUserGroupId());

		 if (thisGroup)
		 {
			quint16 fileType = item->getUserFileType();

			if (fileType == QUBAN_MASTER_UNPACK || fileType == QUBAN_UNPACK)
			{
				thisGroup->addCancelledUnpack();

				if (thisGroup->getNumUnpackFiles() ==
					(thisGroup->getNumDecodedUnpackFiles() + thisGroup->getNumFailedDecodeUnpackFiles() +
					 thisGroup->getNumCancelledUnpackFiles()))
				{
					qDebug() << "Ready to check group";
					locker->unlock();
					checkIfGroupDownloadOk(fileType, thisGroup);
					locker->relock();
				}
				else
					thisGroup->dbSave();
			}
			else if (fileType == QUBAN_MASTER_REPAIR || fileType == QUBAN_REPAIR)
			{
				thisGroup->addCancelledRepair();
			}
			else if (fileType == QUBAN_SUPPLEMENTARY)
			{
				thisGroup->addCancelledOther();
			}

			if (fileType == QUBAN_MASTER_REPAIR || fileType == QUBAN_REPAIR || fileType == QUBAN_SUPPLEMENTARY)
			{
				if ((thisGroup->getNumReleasedRepairFiles() ==
					(thisGroup->getNumDecodedRepairFiles() + thisGroup->getNumFailedDecodeRepairFiles() +
					 thisGroup->getNumCancelledRepairFiles())) &&
					 (thisGroup->getNumReleasedOtherFiles() ==
					 (thisGroup->getNumDecodedOtherFiles() + thisGroup->getNumFailedDecodeOtherFiles() +
					 thisGroup->getNumCancelledOtherFiles()))
				    )
				{
					qDebug() << "Ready to check group";
					locker->unlock();
					checkIfGroupDownloadOk(fileType, thisGroup);
					locker->relock();
				}
				else
					thisGroup->dbSave();
			}
		 }
	}

	delete item;
	emit queueInfo(queueCount, queueSize);
}

void QMgr::slotEmptyFinishedQ()
{
	doneList->clear();
}

void QMgr::slotEmptyFailedQ()
{
	failedList->clear();
}

void QMgr::slotMoveSelectedToBottom()
{
	QList<QTreeWidgetItem*> selection = queueList->selectedItems();
	QListIterator<QTreeWidgetItem*> it(selection);
    QTreeWidgetItem* nextItem = 0;

	while (it.hasNext())
	{
		nextItem = it.next();
		nextItem = queueList->takeTopLevelItem(queueList->indexOfTopLevelItem(nextItem));
		slotItemToBottom(nextItem);
	}

	queueList->addTopLevelItems(selection);
}

void QMgr::slotItemToBottom(QTreeWidgetItem *item)
{
	QMutexLocker locker(&queueLock);
	int itemIndex = item->text(0).toInt();
	QItem *qItem = queue[itemIndex];
//	int it;
	QMap<int, bool>::iterator sit;
//	int newItemIndex;
//	bool up;
	int qId;
//	int afterIndex;

	int depth = 0;
	QTreeWidgetItem *depthCheck = item;
	while (depthCheck != 0)
	{
		depth++;
		depthCheck = depthCheck->parent();
	}

	if (depth != 1) // 1 signifies top level
	{
		locker.unlock();
		qDebug() << "Itemindex = " << itemIndex;
		qDebug() << "Failure: parent = " << item->parent() << ", Tree = "
				<< queueList;
		return;
	}

	for (sit = qItem->queuePresence.begin(); sit
			!= qItem->queuePresence.end(); ++sit)
	{
		qId = sit.key();
		// 			qDebug("Presence in queue: %d", sit.key());
		// 			qDebug("Before moving:");
		if (qItem->queuePresence[qId] == true)
		{
			// 			for (it=queueItems[qId].begin(); it != queueItems[qId].end(); ++it)
			// 				qDebug("%d", *it);
			queueItems[qId].removeAll(itemIndex);
			queueItems[qId].append(itemIndex);
			// 			qDebug("After moving:");
			// 			qDebug("Queue %d:", sit.key());
			// 			for (it=queueItems[qId].begin(); it != queueItems[qId].end(); ++it)
			// 				qDebug("%d", *it);
		}
	}

	queueView.removeAll(itemIndex);
	queueView.append(itemIndex);

	locker.unlock();
    //emit threadFinished();
}

void QMgr::slotMoveSelectedToTop()
{
	QList<QTreeWidgetItem*> selection = queueList->selectedItems();
	QListIterator<QTreeWidgetItem*> it(selection);
    QTreeWidgetItem* nextItem = 0;

	it.toBack();

	while (it.hasPrevious())
	{
		nextItem = it.previous();
		nextItem = queueList->takeTopLevelItem(queueList->indexOfTopLevelItem(nextItem));
		slotItemMoved(nextItem, 0, 0);
	}

	queueList->insertTopLevelItems(0, selection);
}

/*
 void QMgr::slotThreadDisconnected( int serverId, int threadId)
 {
 emit sigThreadDisconnected(serverId, threadId);
 }*/

void QMgr::slotServerValidated(quint16 hostId, bool validated)
{
	 QMapIterator<uint, Thread*> i(threads[hostId]);

	 while (i.hasNext())
	 {
	     i.next();
	     i.value()->setValidated(validated);
	 }
}

void QMgr::validateServer(quint16 serverId)
{
    threads[serverId][1]->validate();
}

void QMgr::addServerQueue(NntpHost *nh)
{
	//Create the empty q
	queueItems[nh->getId()];
	//create the threads...
    //qDebug() << "Creating threads for " << nh->getName();

    bool validate = false;

    if (nh->getServerType() != NntpHost::dormantServer && !nh->getDeleted())  // TODO do we need a Q for dormant and deleted servers??
        validate = true;

	for (int i = 1; i <= nh->getMaxThreads(); i++)
	{
		threads[nh->getId()].insert(i,
                new Thread(nh->getId(), i, nh->getTimeout(), nh->getThreadTimeout(), nh->getValidated(), validate, this, nh));
		connect(threads[nh->getId()][i], SIGNAL(sigThreadSpeedChanged(int,int,int)),
				this, SIGNAL(sigSpeedChanged(int, int, int )));
		connect(threads[nh->getId()][i], SIGNAL(sigThreadDisconnected(quint16, int)),
				this, SIGNAL(sigThreadDisconnected(quint16, int )));
		connect(threads[nh->getId()][i], SIGNAL(sigThreadResumed()), this,
				SLOT(slotResumeThreads()));
		connect(threads[nh->getId()][i], SIGNAL(sigThreadPaused(quint16, int, int)),
				this, SIGNAL(sigThreadPaused(quint16, int, int)));
		threads[nh->getId()][i]->setQueue(&(queueItems[nh->getId()]), &queue, &queueLock,
				&pendingOperations[nh->getId()]);
		threads[nh->getId()][i]->setDbSync(&headerDbLock);

        if (validate)
        {
            threads[nh->getId()][i]->start();
            qDebug() << "Starting validate thread for server " << nh->getId();
            validate = false;
        }
	}
}

void QMgr::slotAddServer(NntpHost *nh)
{
	addServerQueue(nh);
}

void QMgr::slotAddThread(int serverId, int threadId)
{
	threads[serverId].insert(
			threadId,
			new Thread(serverId, threadId, ((*servers)[serverId])->getTimeout(),
                    ((*servers)[serverId])->getThreadTimeout(), ((*servers)[serverId])->getValidated(), false, this));
	connect(threads[serverId][threadId],
			SIGNAL(sigThreadSpeedChanged(int, int, int)), this,
			SIGNAL(sigSpeedChanged(int, int, int )));
	connect(threads[serverId][threadId],
			SIGNAL(sigThreadDisconnected(int, int)), this,
			SIGNAL(sigThreadDisconnected(int, int )));
	connect(threads[serverId][threadId], SIGNAL(sigThreadResumed()), this,
			SLOT(slotResumeThreads()));
	connect(threads[serverId][threadId],
			SIGNAL(sigThreadPaused(int, int, int)), this,
			SIGNAL(sigThreadPaused(int, int, int)));
	threads[serverId][threadId]->setQueue(&(queueItems[serverId]), &queue,
			&queueLock, &pendingOperations[serverId]);
	threads[serverId][threadId]->setDbSync(&headerDbLock);

    emit sigProcessJobs();
}

void QMgr::slotDeleteThread(int serverId)
{
	ThreadMap::iterator it;
	int deleted = 0; //threads starts at one...
	for (it = threads[serverId].begin(); it != threads[serverId].end(); ++it)
	{

		if (!it.value()->isActive())
		{
			//can safely delete the thread...
			deleted = it.key();

            Q_DELETE((it.value())); //delete thread
			threads[serverId].erase(it);

			//Now move other threads...
			//Hhmmmm...disabled for now
			/*
			 ThreadMap::iterator moveIterator = it;
			 ++moveIterator;
			 while (moveIterator != threads[serverId].end()) {

			 it.value()=moveIterator.value();

			 ++it;
			 ++moveIterator;
			 }
			 threads[serverId].remove(it);
			 */
			break;

		}
	}
	// 	qDebug("thread list:");
	for (it = threads[serverId].begin(); it != threads[serverId].end(); ++it)
	{
		qDebug("id: %d", it.key());
	}
	// 	qDebug("Total #: %d", threads[serverId].count());
	if (deleted)
	{
		emit sigThreadDeleted(serverId, deleted);
		return;
	}

	//Else, do a delayed delete (sic)

	pendingOperations[serverId]++;
}

void QMgr::closeAllConnections()
{
	NntpHost* nh = 0;

	ThreadMap::iterator it;
	QMapIterator<quint16, NntpHost *> i(*servers);
	while (i.hasNext())
	{
		i.next();
		nh = i.value();

        if (nh && nh->getServerType() != NntpHost::dormantServer)
		{
			for (it = threads[nh->getId()].begin(); it != threads[nh->getId()].end(); ++it)
			{
				it.value()->closeCleanly();
			}
		}
	}

	rateController->setRatePeriod(false);
}

void QMgr::enable(bool b)
{
	if (b)
	{
		queueList->setCursor(Qt::ArrowCursor);
		doneList->setCursor(Qt::ArrowCursor);
		failedList->setCursor(Qt::ArrowCursor);
		setCursor(Qt::ArrowCursor);
	}
	else
	{
		queueList->setCursor(Qt::WaitCursor);
		doneList->setCursor(Qt::WaitCursor);
		failedList->setCursor(Qt::WaitCursor);
		setCursor(Qt::WaitCursor);
	}

	queueList->setEnabled(b);
	doneList->setEnabled(b);
	failedList->setEnabled(b);
	setEnabled(b);
}

bool QMgr::empty()
{
	QMap<int, QueueList>::iterator it;
	for (it = queueItems.begin(); it != queueItems.end(); ++it)
	{
		if (!it.value().isEmpty())
			return false;
	}
	return true;
}

void QMgr::downloadCancelled(Job *j)
{
	if (j->fName != QString::null)
	    QFile::remove(j->fName);
	cancel(j);
	// 	emit sigThreadDisconnected(j->qId, j->threadId);


	// 	emit threadFinished();

}

void QMgr::slotDeleteServerQueue(quint16 qId)
{
	//delete server queue
	queueItems.remove(qId);
	ThreadMap::iterator it;
	for (it = threads[qId].begin(); it != threads[qId].end(); ++it)
		delete it.value();
	threads.remove(qId);
	queueItems.remove(qId);

}

void QMgr::startExpiring(Job * j)
{
	((QUpdItem*) queue[j->qItemId])->startExpiring(j->partId);
}

void QMgr::slotViewArticle(HeaderBase *hb, NewsGroup *ng)
{
	slotAddPostItem(hb, ng, true, true);
}

void QMgr::slotResumeThreads()
{
    emit sigProcessJobs();
}

void QMgr::startAllThreads(int qId)
{
	ThreadMap::iterator it;
	for (it = threads[qId].begin(); it != threads[qId].end(); ++it)
	{
        //qDebug("Starting thread %d", it.key());
        //if (!queueItems[qId].isEmpty())
        {
            it.value()->start();
        }
	}
}

void QMgr::stopped(int qId, int threadId)
{
// 	qDebug("%d,%d: QMgr::stopped()", qId, threadId);
	if (threads[qId][threadId]->stop())
		emit sigThreadFinished(qId, threadId);
	else
	{
		emit sigThreadDisconnected(qId, threadId);
		// 		qDebug("%d,%d disconnected", qId, threadId);
	}
}

void QMgr::paused(int qId, int threadId, bool error)
{
	Thread *t = threads[qId][threadId];

	if (!error)
	{
		t->paused();
		emit sigThreadPaused(qId, threadId, 0);
	}
	else if (!diskError)
		t->comError();
}

void QMgr::connClosed(int qId, int threadId)
{
	emit sigThreadConnClosed(qId, threadId);
}

void QMgr::delayedDelete(int qId, int threadId)
{
	//Wait for termination if the thread has been preempted after sending the message...
	qDebug("Delayed delete of thread %d, %d", qId, threadId);
	threads[qId][threadId]->wait();
	delete threads[qId][threadId];
	threads[qId].remove(threadId);
	emit sigThreadDeleted(qId, threadId);
}

void QMgr::slotPauseThread(int qId, int threadId)
{
	threads[qId][threadId]->pause();
}

void QMgr::slotResumeThread(int qId, int threadId)
{
	//Resume the thread I paused...
	threads[qId][threadId]->resume();
	slotResumeThreads();
}

QSaveItem * QMgr::loadSaveItem(char *data)
{

	char *i = &data[0];
	QSaveItem * si = new QSaveItem;
	i = retrieve(i, si->group);
	i = retrieve(i, si->index);

	i = retrieve(i, si->rootFName);
	i = retrieve(i, si->savePath);
	i = retrieve(i, si->tmpDir);

	quint16 sz16  = sizeof(quint16);
	quint16 sz64 = sizeof(quint64);

	memcpy(&(si->curPostLines), i, sz64);
	i += sz64;

	quint64 count;
	quint64 id;
	quint16 status;

	memcpy(&count, i, sz64);
	i += sz64;
	for (quint64 j = 0; j < count; j++)
	{
		memcpy(&id, i, sz64);
		i += sz64;
		memcpy(&status, i, sz16);
		i += sz16;
		si->partStatus.insert(id, status);
	}

	memcpy(&(si->userGroupId), i, sz16);
	i += sz16;

	memcpy(&(si->userFileType), i, sz16);
	i += sz16;

	return si;
}

void QMgr::writeSaveItem(QSaveItem * si)
{
	if (!saveQThread.isRunning())
	  saveQThread.start();

	emit addSaveItem(si);
}

void QMgr::updateSaveItem(int id, int part, int status, uint lines)
{
	QSaveItem *si = new QSaveItem;
	si->id = id;
	si->modPart = part;
	si->modStatus = status;
	si->curPostLines = lines;
	si->type = QSaveItem::QSI_Update;
	writeSaveItem(si);
}

void QMgr::setDbEnv(QString d, DbEnv * dbe, GroupList *gl)
{
	int ret = 0;

	dbEnv = dbe;
	dbDir = d;
	gList = gl;

	if (!(pendDb = quban->getDbEnv()->openDb("autoUnpack", "pendingFiles", true, &ret)))
		QMessageBox::warning(0, tr("Error"), tr("Unable to open pending files database ..."));

	autoFileDb = new Db(dbEnv, 0);
	autoFileDb->set_flags(DB_DUPSORT);
	if (autoFileDb->open(NULL, "autoUnpack", "autoFiles", DB_BTREE, DB_CREATE | DB_THREAD, 0644) != 0)
	    QMessageBox::warning(0, tr("Error"), tr("Unable to open auto files database ..."));

	if (!(unpackDb = quban->getDbEnv()->openDb("autoUnpack", "unpackGroup", true, &ret)))
		QMessageBox::warning(0, tr("Error"), tr("Unable to open unpack groups database ..."));

	if (!(qDb = quban->getDbEnv()->openDb("queue", true, &ret)))
		QMessageBox::warning(0, tr("Error"), tr("Unable to open queue database ..."));

	nzbGroup = new NewsGroup(dbEnv, "nzb", config->decodeDir, QString::null, "N");
	nzbGroup->setMultiPartSequence(0);

	saveQItems = new SaveQItems(qDb);
	connect(saveQItems, SIGNAL(deleteNzb(QString)), this, SLOT(delNzbItem(QString)));
	connect(saveQItems, SIGNAL(saveQError()), this, SLOT(writeError()));
	connect(this, SIGNAL(addSaveItem(QSaveItem*)), saveQItems, SLOT(addSaveItem(QSaveItem*)));
	saveQItems->moveToThread(&saveQThread);
	connect(this, SIGNAL(quitSaveQ()), &saveQThread, SLOT(quit()));

}

void QMgr::checkServers()
{
	checkQueue();
}

//This is called after program init...

void QMgr::checkQueue()
{
    Dbc *cursor = 0;
	Dbt key, data;

	// Make sure that the status is known for all servers
	QMapIterator<quint16, NntpHost*> i(*servers);
	while (i.hasNext())
	{
	    i.next();
	    if (i.value()->getValidating())
	    {
	    	//quban->getLogAlertList()->logMessage(LogAlertList::Alert, tr("Still validating server: ") + i.value()->getHostName());
	    	quban->slotStatusMessge(tr("Server validation in progress"));
	    	if (!timer)
	    	{
				timer = new QTimer(this);
				connect(timer, SIGNAL(timeout()), this, SLOT(checkServers()));
				timer->start(3000);
	    	}
	        return;
	    }
	}

	quban->getLogEventList()->logEvent(tr("Server validation complete, queue check started"));

	if (timer) // may be no servers yet
	{
		timer->stop();
		timer->deleteLater();
		timer = 0;
	}

	uchar *keymem=new uchar[KEYMEM_SIZE];
	uchar *datamem=new uchar[DATAMEM_SIZE * 10];

	key.set_flags(DB_DBT_USERMEM);
	key.set_data(keymem);
	key.set_ulen(KEYMEM_SIZE);

	data.set_flags(DB_DBT_USERMEM);
	data.set_ulen(DATAMEM_SIZE  * 10);
	data.set_data(datamem);

	int ret = 0;

	uint id;
    QSaveItem *qsi = 0;
    GroupManager* groupManager = 0;
    PendingHeader* pendingHeader = 0;
    AutoFile* autoFile = 0;
	HeaderBase* hb = 0;
	SinglePartHeader* sph = 0;
	MultiPartHeader* mph = 0;
	QMap<uint, QSaveItem*> queueMap;
	bool groupDataHeld = false;

    //qDebug() << "Getting groups details";
	unpackDb->cursor(0, &cursor, DB_WRITECURSOR);

	while (ret == 0)
	{
		if ((ret = cursor->get(&key, &data, DB_NEXT)) == 0)
		{
			groupManager = new GroupManager(unpackDb, gList, (uchar*)data.get_data(), this);
			groupedDownloads.insert(groupManager->getGroupId(), groupManager);
            //qDebug() << "Just got group: " << groupManager->getGroupId() << ", master unpack: " << groupManager->getMasterUnpack();
			groupDataHeld = true;

			if (groupManager->getGroupId() > maxGroupId)
				maxGroupId = groupManager->getGroupId();

			// If we were terminated, due to shutdown?
			if (groupManager->getGroupStatus() == QUBAN_GROUP_UNPACKING
				||	groupManager->getGroupStatus() == QUBAN_GROUP_REPAIRING
			)
			{
				AutoUnpackThread* unpackThread = new AutoUnpackThread(groupManager, this);
				if (groupManager->getGroupStatus() == QUBAN_GROUP_UNPACKING)
					emit sigExtProcStarted(groupManager->getGroupId(), QUBAN_UNPACK, groupManager->getMasterUnpack());
				else
					emit sigExtProcStarted(groupManager->getGroupId(), QUBAN_REPAIR, groupManager->getMasterRepair());
				groupManager->setWorkingThread(unpackThread);
				unpackThread->processGroup();
			}
		}
		else if (ret == DB_BUFFER_SMALL)
		{
			ret = 0;
			qDebug("Insufficient memory");
			qDebug("Size required is: %d", data.get_size());
			uchar *p=datamem;
			datamem=new uchar[data.get_size()+1000];
			data.set_ulen(data.get_size()+1000);
			data.set_data(datamem);
            Q_DELETE_ARRAY(p);
		}
	}

	cursor->close();

    // qDebug() << "Getting pending files details";

	pendDb->cursor(0, &cursor, DB_WRITECURSOR);

	ret = 0;

	while (ret == 0)
	{
		if ((ret = cursor->get(&key, &data, DB_NEXT)) == 0)
		{
            pendingHeader = new PendingHeader(pendDb, (uchar*)data.get_data());
			pendingHeaders.insert(pendingHeader->getGroupId(), pendingHeader);

			if (pendingHeader->getHeaderId() > maxPendingHeader)
				maxPendingHeader = pendingHeader->getHeaderId();
		}
		else if (ret == DB_BUFFER_SMALL)
		{
			ret = 0;
			qDebug("Insufficient memory");
			qDebug("Size required is: %d", data.get_size());
			uchar *p=datamem;
			datamem=new uchar[data.get_size()+1000];
			data.set_ulen(data.get_size()+1000);
			data.set_data(datamem);
            Q_DELETE_ARRAY(p);
		}
	}

	cursor->close();

	qDebug() << "Getting auto files details";
	autoFileDb->cursor(0, &cursor, DB_WRITECURSOR);

	ret = 0;
	while (ret == 0)
	{
		if ((ret = cursor->get(&key, &data, DB_NEXT)) == 0)
		{
			autoFile = new AutoFile(autoFileDb, (uchar*)data.get_data());
			autoFiles.insert(autoFile->getGroupId(), autoFile);
		}
		else if (ret == DB_BUFFER_SMALL)
		{
			ret = 0;
			qDebug("Insufficient memory");
			qDebug("Size required is: %d", data.get_size());
			uchar *p=datamem;
			datamem=new uchar[data.get_size()+1000];
			data.set_ulen(data.get_size()+1000);
			data.set_data(datamem);
            Q_DELETE_ARRAY(p);
		}
	}

	cursor->close();

	qDebug() << "Checking queue";

	qDb->cursor(0, &cursor, DB_WRITECURSOR);

	// 	qDebug("Saved the following items:");
    ret = 0;
	bool go = false;

	while (ret == 0)
	{
		if (((ret = cursor->get(&key, &data, DB_NEXT))) == 0)
		{
			//I know, I know, this sucks :)
			if (go == false)
			{
				if (!config->noQueueSavePrompt)
				{
					//ask
					int result = QMessageBox::question(
							this,
							tr("Reload download Queue?"),
							"Found items in the download Queue from the previous session. Do you want to reload them?",
							QMessageBox::Yes, QMessageBox::No);

					switch (result)
					{
					case QMessageBox::Yes:
						go = true;
						//Do nothing...
						break;
					case QMessageBox::No:
						//cleanup & return
						cursor->close();
						uint count;
						qDb->truncate(0, &count, 0);
						qDebug() << "Truncated " << count
								<< "items from queue db";
						nzbGroup->getDb()->truncate(0, &count, 0);
						qDebug() << "Truncated " << count
								<< " items from nzb db";

						nzbGroup->getPartsDb()->truncate(0, &count, 0);
						qDebug() << "Truncated " << count
								<< " items from nzb parts db";

						cancelGroupItems();

                        Q_DELETE_ARRAY(keymem);
                        Q_DELETE_ARRAY(datamem);

						return;
						break;
					}
				}
				else if (config->noQueueSavePrompt)
				{
					go = true;
				}
			}

			memcpy((void*)&id, (const void*)key.get_data(), sizeof(uint));

			if (id > nextItemId)
				nextItemId = id;

			qsi = loadSaveItem((char*) data.get_data());

			if (queueMap.contains(id))
				qDebug("Warning: duplicate id!!!");
			queueMap.insert(id, qsi);
		}
		else if (ret == DB_BUFFER_SMALL)
		{
			ret = 0;
			qDebug("Insufficient memory");
			qDebug("Size required is: %d", data.get_size());
			uchar *p=datamem;
			datamem=new uchar[data.get_size()+1000];
			data.set_ulen(data.get_size()+1000);
			data.set_data(datamem);
            Q_DELETE_ARRAY(p);
		}
	}

	if (ret != DB_NOTFOUND)
	{
		//Error!
		QMessageBox::warning(this, tr("Error!"),
				tr("The saved queue seems to be corrupted. Please exit Quban and remove \"queue\" from the db/ dir"));
	}
	cursor->close();

    Q_DELETE_ARRAY(keymem);
    Q_DELETE_ARRAY(datamem);

	if (go == false) // nothing to load
	{
		uint count;
		int retCode;

		retCode = qDb->truncate(0, &count, 0);
		qDebug() << "Truncated " << count << "items from queue db, retCode " << retCode;

		if (groupDataHeld == false)
		{
		    retCode = nzbGroup->getDb()->truncate(0, &count, 0);
		    qDebug() << "Truncated " << count << " items from nzb db, retCode " << retCode;

		    retCode = nzbGroup->getPartsDb()->truncate(0, &count, 0);
		    qDebug() << "Truncated " << count << " items from nzb parts db, retCode " << retCode;
		}

		// Just in case we're accumulating temp files flush the tmp dir at this point
		QDir tempDir(config->tmpDir + '/');

	    QStringList filters;
	    filters << "Quban*";
	    tempDir.setNameFilters(filters);
	    QString fullAbandonedFile;

	    QStringList abandonedFiles = tempDir.entryList();

	     for (int i = 0; i < abandonedFiles.size(); ++i)
	     {
	    	 fullAbandonedFile = config->tmpDir + '/' + abandonedFiles.at(i);
	    	 qDebug() << "Deleting " << fullAbandonedFile;
	         QFile::remove(fullAbandonedFile);
	     }

	 	quban->getLogEventList()->logEvent(tr("Startup complete, no queued items found to restore"));

		return;
	}

	//Now reload the map...
    QMap<uint, QSaveItem*>::iterator it = queueMap.begin();
    uchar *p2 = 0;
    while (it != queueMap.end())
    {
		if (it.value()->group == "nzb")
			p2 = nzbGroup->getBinHeader(it.value()->index);
		else
			p2 = gList->getBinHeader(it.value()->index, it.value()->group);

		if (!p2)
		{
			qDebug() << "Error getting binheader";
			delSaveItem(it.key());
            ++it;
		}
		else
		{
			QByteArray ba = it.value()->index.toLocal8Bit();
			const char *c_str = ba.constData();

			if (*((char *)p2) == 'm')
			{
			    mph = new MultiPartHeader(it.value()->index.length(), (char *)c_str, (char *)p2);
			    hb = (HeaderBase*)mph;
			    if (it.value()->group == "nzb")
			    {
			    	nzbGroup->setMultiPartSequence(qMax(mph->getMultiPartKey(), nzbGroup->getMultiPartSequence()));
			    }
			}
			else
			{
				sph = new SinglePartHeader(it.value()->index.length(), (char *)c_str, (char *)p2);
				hb = (HeaderBase*)sph;
			}

			// 		qDebug() << "Reloading id: " << it.key() << " subj: " << bh->getSubj();
            Q_DELETE_ARRAY(p2);
			if (it.value()->group == "nzb")
			{
				createPostItemFromQ(it.key(), it.value(), hb, nzbGroup);
			}
			else
				createPostItemFromQ(it.key(), it.value(), hb, gList->getNg(it.value()->group));
		}

        ++it;
	}

    qDeleteAll(queueMap);
    queueMap.clear();

	emit (queueInfo(queueCount, queueSize));

    emit sigProcessJobs();

	queueList->resizeColumnToContents(0);
	queueList->resizeColumnToContents(1);
	queueList->resizeColumnToContents(4);

    QMap<quint16, NntpHost*>::iterator sit;
    QDateTime now = QDateTime::currentDateTime();
    QDateTime qdt;
    quint32 serverExtensionsUpdate;
    NntpHost *nh = 0;

    for (sit = servers->begin(); sit != servers->end(); ++sit)
    {
        nh = sit.value();

        if (!nh || nh->getServerType() == NntpHost::dormantServer)
            continue;

        serverExtensionsUpdate = nh->getServerFlagsDate();
        qdt.setTime_t(serverExtensionsUpdate);
        if (serverExtensionsUpdate == 0 ||qdt.daysTo(now) > 30)
        {
            qDebug() << "Server extensions need updating";
            emit sigUpdateServerExtensions(nh->getId());
        }
    }

	quban->getLogEventList()->logEvent(tr("Startup complete, queued items restored"));
}

void QMgr::delSaveItem(int id)
{

	QSaveItem *qsi = new QSaveItem;
	qsi->type = QSaveItem::QSI_Delete;
	qsi->id = id;
	writeSaveItem(qsi);
}

void QMgr::delNzbItem(QString index)
{
	Dbt key, data;
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	data.set_flags(DB_DBT_MALLOC);
	MultiPartHeader *mph = 0;
    char* dataBlock = 0;

	QByteArray ba = index.toLocal8Bit();
	const char *c_str = ba.constData();
	key.set_data((void*) c_str);
	key.set_size(index.length());

	if ((nzbGroup->getDb()->get(NULL, &key, &data, 0)) == 0)
	{
		dataBlock = (char*)data.get_data();
		mph = new MultiPartHeader(key.get_size(), (char*)key.get_data(), dataBlock);
		mph->deleteAllParts(nzbGroup->getPartsDb());
		nzbGroup->getPartsDb()->sync(0);

        Q_FREE(dataBlock);
	}
	else
		qDebug() << "Failed to get mph for deletion of parts";

	if (nzbGroup->getDb()->del(0, &key, 0) != 0)
	{
		qDebug() << "QMgr::delNzbItem(): Error deleting item from nzb db with index " << index;
	}
	else
	{
		// 		qDebug() << "QMgr::delNzbItem(): nzb item deleted\n";
		nzbGroup->getDb()->sync(0);
	}

	return;
}

void QMgr::createPostItemFromQ(int id, QSaveItem * qsi, HeaderBase* hb, NewsGroup *ng)
{
    Part *tPart = 0;
    Job *tJob = 0;
	int serverId = 0;
	QString rootFName, tempDir;

	bool availableServer = false;
	NntpHost* nh = 0;

	QMapIterator<quint16, NntpHost *> i(*servers);
	while (i.hasNext())
	{
		i.next();
		nh = i.value();
        if (nh && (nh->getServerType() == NntpHost::activeServer || nh->getServerType() == NntpHost::passiveServer))
		{
			availableServer = true;
			break;
		}
	}

	if (availableServer == false) // no servers to pick up the job
	{
		qDebug("No available servers ...");
		return;
	}

	rootFName = qsi->rootFName;

	queueSize += hb->size;
	queueCount++;

	QString dir = qsi->savePath.isNull() ? ng->getSaveDir() : qsi->savePath;
	tempDir = qsi->tmpDir;

	PartNumMap serverArticleNos;
	QMap<quint16, quint64> partSize;
	QMap<quint16, QString> partMsgId;
	MultiPartHeader* mph = 0;
	SinglePartHeader* sph = 0;

	if (hb->getHeaderType() == 'm')
	{
		mph = (MultiPartHeader*)hb;
		quint64 multiPartKey = mph->getMultiPartKey();
        Header* h = 0;

		// Walk the headers to get the PartNumMap
        Dbc *cursorp = 0;

		// Get a cursor
		ng->getPartsDb()->cursor(NULL, &cursorp, 0);

		// Set up our DBTs
		Dbt key(&multiPartKey, sizeof(quint64));
		Dbt data;

		int ret = cursorp->get(&key, &data, DB_SET);

		while (ret != DB_NOTFOUND)
		{
			h = new Header((char*)data.get_data(), (char*)key.get_data());
			serverArticleNos.insert(h->getPartNo(), h->getServerArticlemap());   
			partSize.insert(h->getPartNo(), h->getSize());
			partMsgId.insert(h->getPartNo(), h->getMessageId());

			delete h;
			ret = cursorp->get(&key, &data, DB_NEXT_DUP);
		}

		// Close the cursor
		if (cursorp != NULL)
		    cursorp->close();
	}
	else
	{
		sph = (SinglePartHeader*)hb;
		serverArticleNos.insert(1, sph->getServerArticlemap());
		partSize.insert(1, sph->getSize());
		partMsgId.insert(1, sph->getMessageId());
	}

    PartNumMap::iterator pnmit;

    ServerNumMap::iterator snmit;

    for (pnmit = serverArticleNos.begin(); pnmit != serverArticleNos.end(); ++pnmit)
    {
        snmit =  pnmit.value().begin();
        while (snmit !=  pnmit.value().end())
        {
            if (!servers->contains(snmit.key()))
                snmit = pnmit.value().erase(snmit);
            else
                ++snmit;
        }
    }

	QPostItem *qp = new QPostItem(queueList, id, hb, serverArticleNos, rootFName, dir, 0, 0, qsi->userGroupId, qsi->userFileType);
	queue.insert(id, qp);

	qp->curPostLines = qsi->curPostLines;

	connect(qp, SIGNAL(decodeMe(QPostItem*)), this, SLOT(addDecodeItem(QPostItem* )));
	connect(qp, SIGNAL(decodeNotNeeded(QPostItem*, bool, QString, QString)), this, SLOT(decodeFinished(QPostItem*, bool, QString, QString)));
    connect(qp, SIGNAL(addDataRead(quint16, int)), this, SLOT(addDataRead(quint16, int)));

	QMap<quint16, NntpHost*>::iterator sit; //Server iterator
	for (sit = servers->begin(); sit != servers->end(); ++sit)
		queue[id]->queuePresence[sit.key()] = false;

	// 	if (first)
	// 		queueView.prepend(nextItemId);
	// 	else queueView.append(nextItemId);
	queueView.append(id);


	// walk through the parts ...
	for (pnmit = serverArticleNos.begin(); pnmit != serverArticleNos.end(); ++pnmit)
	{
		nextJobId++;
		// 			qDebug("Adding part %d to item %d with jobId %d (subject: %s)", bh->pnmit.key(), id, nextJobId, bh->getSubj().latin1());
		tJob = new Job;
		// Choose the right server, based on priority....
		// In the future, will try to balance the queues with the same priority (using qinfo)

		serverId = chooseServer(pnmit.value());
		if (serverId == 0) // no servers to pick up the job
		{
			qDebug("No server found ...");
			return;
		}
		(*servers)[serverId]->increaseQueueSize(partSize[pnmit.key()]);
		tJob->artSize = partSize[pnmit.key()];

		tJob->qId = serverId;
		tJob->id = nextJobId;
		tJob->jobType = Job::GetPost;
		tJob->nh = (*servers)[serverId]; //Got to choose the server....
		tJob->ng = ng;
		tJob->qItemId = id;
		tJob->partId = pnmit.key();
		tJob->tries = (*servers)[serverId]->getRetries();
		if (partMsgId.contains(pnmit.key()))
		{
			// 			qDebug() << "Have mid";
			tJob->mid = partMsgId[pnmit.key()];
			// 			qDebug() << "Mid: " << tJob->mid;

		}
		else
		{
			tJob->mid = QString::null;
			// 			qDebug() << "Have number";
		}

		tJob->artNum = pnmit.value()[serverId];
		tJob->fName = rootFName + '.' + QString::number(tJob->partId);

		if (qp->queuePresence[serverId] == false)
		{
			qp->queuePresence[serverId] = true;
			queueItems[serverId].append(id);
			// 				qDebug("Appended item %d on queue %d", id, serverId);
		}

		tPart = new Part;
		tPart->desc = hb->getSubj() + " " + QString::number(pnmit.key())
				+ "/" + QString::number(hb->getParts());

		tPart->status = QItem::Queued_Part;
		tJob->status = Job::Queued_Job;

		tPart->jobId = nextJobId;
		tPart->job = tJob;
		tPart->qId = serverId;

		if (qsi->partStatus.contains(pnmit.key()))
			qp->addJobPartWithStatus(nextJobId, pnmit.key(), tPart,
					qsi->partStatus[pnmit.key()]);
		else
			qp->addJobPart(nextJobId, pnmit.key(), tPart);
	}
	qp->listItem->setText(
			3,
			QString::number(qp->totalParts - qp->partsToDo) + '/'
					+ QString::number(qp->failedParts) + '/' + QString::number(
					qp->totalParts));
	//Check if the item is complete, and I have to decode it...
	if (qp->partsToDo == 0)
	{
		qp->listItem->setText(2, "Decoding");
		qp->listItem->setText(3, "");
		qp->listItem->setText(4, "");
		addDecodeItem(qp);
	}
}

void QMgr::writeError(Job * j)
{
	//Currently, it pause the queue and show a dialog box.
	//May change in the future (i.e.: check if the disk is full, if not retry...
	if (j != 0)
	{
		queue[j->qItemId]->comError(j->partId);
		threads[j->qId][j->partId]->paused();
		emit sigThreadPaused(j->qId, j->threadId, 0);
	}
	if (!queuePaused)
	{
		//Pause the queue
		pauseQ(true);

		//This variable for queue resume....
		diskError = true;
		//Should emit a signal to press the checkButton....
		emit
		sigQPaused(true);
		//Display an error dialog...
		QMessageBox::warning(
				this,
				tr("Write error"),
				tr("There was an error writing to disk - the download queue has been paused. Chances are that the filesystem is out of space. Correct the error and restart the queue"));
	}
}



bool confHeaderLessThan(const ConfirmationHeader* s1, const ConfirmationHeader* s2)
{
    return s1->fileName < s2->fileName;
}

void QMgr::completeUnpackDetails(UnpackConfirmation* unpackConfirmation, QString fullNzbFilename)
{
    int mainRepair = -1;
    int mainUnpack = -1;

	QString    fileName;
	bool       fileTypeFound = false;
	QString    repairSuffixes;
	QString    unpackSuffixes;
	QString    reRepairSuffixes;
	QString    reUnpackSuffixes;
	QRegExp    re;
	QString    reSuffix;

	re.setCaseSensitivity(Qt::CaseInsensitive);

	for (int i=0; i<config->repairAppsList.count(); i++)
	{
		repairSuffixes = repairSuffixes + " " + config->repairAppsList.at(i)->suffixes;
		reRepairSuffixes = reRepairSuffixes + " " + config->repairAppsList.at(i)->reSuffixes;
	}
	QStringList repairSuffixList = repairSuffixes.split(" ", QString::SkipEmptyParts);
	QStringList reRepairSuffixList = reRepairSuffixes.split(" ", QString::SkipEmptyParts);

	for (int i=0; i<config->unpackAppsList.count(); i++)
	{
		unpackSuffixes = unpackSuffixes + " " + config->unpackAppsList.at(i)->suffixes;
		reUnpackSuffixes = reUnpackSuffixes + " " + config->unpackAppsList.at(i)->reSuffixes;
	}
	QStringList unpackSuffixList = unpackSuffixes.split(" ", QString::SkipEmptyParts);
	QStringList reUnpackSuffixList = reUnpackSuffixes.split(" ", QString::SkipEmptyParts);

	qSort(unpackConfirmation->headers.begin(), unpackConfirmation->headers.end(), confHeaderLessThan);
	QList<ConfirmationHeader*>::iterator it;

	int i=0;
	int j=0;

	for ( it = unpackConfirmation->headers.begin(), j=0; it != unpackConfirmation->headers.end(); ++it, j++)
	{
		fileName = (*it)->fileName;

		for (i=0; i<unpackSuffixList.count(); i++)
		{
			if (fileName.endsWith(unpackSuffixList.at(i), Qt::CaseInsensitive))
			{
				fileTypeFound=true;
				break;
			}
		}

		if (fileTypeFound)
		{
			fileTypeFound=false;

		    if (mainUnpack != -1)
		    	(*it)->UnpackFileType = QUBAN_UNPACK;
		    else
		    {
		    	mainUnpack = j;
		    	(*it)->UnpackFileType = QUBAN_MASTER_UNPACK;
		    	unpackConfirmation->packId=determinePackType((*it)->fileName, &unpackConfirmation->packPriority);
		    }
		}
		else
		{
			for (i=0; i<reUnpackSuffixList.count(); i++)
			{
				reSuffix = reUnpackSuffixList.at(i) + "$";
				re.setPattern(reSuffix);
				if (fileName.contains(re))
				{
					fileTypeFound=true;
					break;
				}
			}

			if (fileTypeFound)
			{
				fileTypeFound=false;

			    if (mainUnpack != -1)
			    	(*it)->UnpackFileType = QUBAN_UNPACK;
			    else
			    {
			    	if (reUnpackSuffixList.at(i) != "r[0-9]{2}") // TODO, make this a proper determination for rar
			    	{
			    	    mainUnpack = j;
			    	    (*it)->UnpackFileType = QUBAN_MASTER_UNPACK;
			    	    unpackConfirmation->packId=determinePackType((*it)->fileName, &unpackConfirmation->packPriority);
			    	}
			    	else
			    		(*it)->UnpackFileType = QUBAN_UNPACK;
			    }
			}
			else
			{
				for (i=0; i<repairSuffixList.count(); i++)
				{
					if (fileName.endsWith(repairSuffixList.at(i), Qt::CaseInsensitive))
					{
							fileTypeFound=true;
							break;
					}
				}

				if (fileTypeFound)
				{
					fileTypeFound=false;

				    if (mainRepair != -1)
				    	(*it)->UnpackFileType = QUBAN_REPAIR;
				    else
				    {
				    	mainRepair = j;
				    	(*it)->UnpackFileType = QUBAN_MASTER_REPAIR;
				    	unpackConfirmation->repairId=determineRepairType((*it)->fileName, &unpackConfirmation->repairPriority);
				    }
				}
				else
				{
					for (i=0; i<reRepairSuffixList.count(); i++)
					{
						reSuffix = reRepairSuffixList.at(i) + "$";
						re.setPattern(reSuffix);
						if (fileName.contains(re))
						{
							fileTypeFound=true;
							break;
						}
					}

					if (fileTypeFound)
					{
						fileTypeFound=false;

					    if (mainRepair != -1)
					    	(*it)->UnpackFileType = QUBAN_REPAIR;
					    else
					    {
					    	mainRepair = j;
					    	(*it)->UnpackFileType = QUBAN_MASTER_REPAIR;
					    	unpackConfirmation->repairId=determineRepairType((*it)->fileName, &unpackConfirmation->repairPriority);
					    }
					}
					else // Doesn't end with a suffix - check if it's embedded (in subject)
					{
						for (i=0; i<unpackSuffixList.count(); i++)
						{
							if (fileName.contains(unpackSuffixList.at(i), Qt::CaseInsensitive))
							{
								fileTypeFound=true;
								break;
							}
						}

						if (fileTypeFound)
						{
							fileTypeFound=false;

						    if (mainUnpack != -1)
						    	(*it)->UnpackFileType = QUBAN_UNPACK;
						    else
						    {
						    	mainUnpack = j;
						    	(*it)->UnpackFileType = QUBAN_MASTER_UNPACK;
						    	unpackConfirmation->packId=determinePackType((*it)->fileName, &unpackConfirmation->packPriority);
						    }
						}
						else
						{
							for (i=0; i<reUnpackSuffixList.count(); i++)
							{
								re.setPattern(reUnpackSuffixList.at(i));
								if (fileName.contains(re))
								{
									fileTypeFound=true;
									break;
								}
							}

							if (fileTypeFound)
							{
								fileTypeFound=false;

							    if (mainUnpack != -1)
							    	(*it)->UnpackFileType = QUBAN_UNPACK;
							    else
							    {
							    	if (reUnpackSuffixList.at(i) != "r[0-9]{2}") // TODO, make this a proper determination for rar
							    	{
							    	    mainUnpack = j;
							    	    (*it)->UnpackFileType = QUBAN_MASTER_UNPACK;
							    	    unpackConfirmation->packId=determinePackType((*it)->fileName, &unpackConfirmation->packPriority);
							    	}
							    	else
							    		(*it)->UnpackFileType = QUBAN_UNPACK;
							    }
							}
							else
							{
								for (i=0; i<repairSuffixList.count(); i++)
								{
									if (fileName.contains(repairSuffixList.at(i), Qt::CaseInsensitive))
									{
										fileTypeFound=true;
										break;
									}
								}

								if (fileTypeFound)
								{
									fileTypeFound=false;

								    if (mainRepair != -1)
								    	(*it)->UnpackFileType = QUBAN_REPAIR;
								    else
								    {
								    	mainRepair = j;
								    	(*it)->UnpackFileType = QUBAN_MASTER_REPAIR;
								    	unpackConfirmation->repairId=determineRepairType((*it)->fileName, &unpackConfirmation->repairPriority);
								    }
								}
								else
								{
									for (i=0; i<reRepairSuffixList.count(); i++)
									{
										re.setPattern(reRepairSuffixList.at(i));
										if (fileName.contains(re))
										{
											fileTypeFound=true;
											break;
										}
									}

									if (fileTypeFound)
									{
										fileTypeFound=false;

									    if (mainRepair != -1)
									    	(*it)->UnpackFileType = QUBAN_REPAIR;
									    else
									    {
									    	mainRepair = j;
									    	(*it)->UnpackFileType = QUBAN_MASTER_REPAIR;
									    	unpackConfirmation->repairId=determineRepairType((*it)->fileName, &unpackConfirmation->repairPriority);
									    }
									}
									else // Assume 'other'
									{
										(*it)->UnpackFileType = QUBAN_SUPPLEMENTARY;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (config->dontShowTypeConfirmUnlessNeeded == false || mainUnpack == -1)
	{
		AutoUnpackConfirmation *autoUnpackConfirmation = new AutoUnpackConfirmation(unpackConfirmation, fullNzbFilename, this);

		connect(autoUnpackConfirmation, SIGNAL(sigUnpackConfirmed(QString)), this, SLOT(slotUnpackConfirmed(QString)));
		connect(autoUnpackConfirmation, SIGNAL(sigUnpackCancelled()), this, SLOT(slotUnpackCancelled()));
		autoUnpackConfirmation->exec();
	}
	else // No need to confirm as we have a master unpacker
		slotUnpackConfirmed(fullNzbFilename);
}

quint32 QMgr::determineRepairType(QString& fileName, quint32* repairPriority)
{
	QString    repairSuffixes;
	QString    reRepairSuffixes;
	QStringList repairSuffixList;
	QStringList reRepairSuffixList;
	QRegExp    re;
	QString    reSuffix;

	re.setCaseSensitivity(Qt::CaseInsensitive);

	for (int i=0; i<config->repairAppsList.count(); i++)
	{
		repairSuffixes = config->repairAppsList.at(i)->suffixes;
		repairSuffixList = repairSuffixes.split(" ", QString::SkipEmptyParts);

		reRepairSuffixes = config->repairAppsList.at(i)->reSuffixes;
		reRepairSuffixList = reRepairSuffixes.split(" ", QString::SkipEmptyParts);

		for (int j=0; j<repairSuffixList.count(); j++)
		{
			if (fileName.endsWith(repairSuffixList.at(j), Qt::CaseInsensitive))
			{
				*repairPriority = config->repairAppsList.at(i)->priority;
				return config->repairAppsList.at(i)->appId;
			}
		}

		for (int j=0; j<reRepairSuffixList.count(); j++)
		{
			reSuffix = reRepairSuffixList.at(j) + "$";
			re.setPattern(reSuffix);
			if (fileName.contains(re))
			{
				*repairPriority = config->repairAppsList.at(i)->priority;
				return config->repairAppsList.at(i)->appId;
			}
		}

		for (int j=0; j<repairSuffixList.count(); j++)
		{
			if (fileName.contains(repairSuffixList.at(j), Qt::CaseInsensitive))
			{
				*repairPriority = config->repairAppsList.at(i)->priority;
				return config->repairAppsList.at(i)->appId;
			}
		}

		for (int j=0; j<reRepairSuffixList.count(); j++)
		{
			re.setPattern(reRepairSuffixList.at(j));
			if (fileName.contains(re))
			{
				*repairPriority = config->repairAppsList.at(i)->priority;
				return config->repairAppsList.at(i)->appId;
			}
		}
	}

	return 0;
}

quint32 QMgr::determinePackType(QString& fileName, quint32* packPriority)
{
	QString    unpackSuffixes;
	QString    reUnpackSuffixes;
	QStringList unpackSuffixList;
	QStringList reUnpackSuffixList;
	QRegExp    re;
	QString    reSuffix;

	re.setCaseSensitivity(Qt::CaseInsensitive);

	for (int i=0; i<config->unpackAppsList.count(); i++)
	{
		unpackSuffixes = config->unpackAppsList.at(i)->suffixes;
	    unpackSuffixList = unpackSuffixes.split(" ", QString::SkipEmptyParts);

		reUnpackSuffixes = config->unpackAppsList.at(i)->reSuffixes;
		reUnpackSuffixList = reUnpackSuffixes.split(" ", QString::SkipEmptyParts);

		for (int j=0; j<unpackSuffixList.count(); j++)
		{
			if (fileName.endsWith(unpackSuffixList.at(j), Qt::CaseInsensitive))
			{
				*packPriority = config->unpackAppsList.at(i)->priority;
				return config->unpackAppsList.at(i)->appId;
			}
		}

		for (int j=0; j<reUnpackSuffixList.count(); j++)
		{
			reSuffix = reUnpackSuffixList.at(j) + "$";
			re.setPattern(reSuffix);
			if (fileName.contains(re))
			{
				*packPriority = config->unpackAppsList.at(i)->priority;
				return config->unpackAppsList.at(i)->appId;
			}
		}

		for (int j=0; j<unpackSuffixList.count(); j++)
		{
			if (fileName.contains(unpackSuffixList.at(j), Qt::CaseInsensitive))
			{
				*packPriority = config->unpackAppsList.at(i)->priority;
				return config->unpackAppsList.at(i)->appId;
			}
		}

		for (int j=0; j<reUnpackSuffixList.count(); j++)
		{
			re.setPattern(reUnpackSuffixList.at(j));
			if (fileName.contains(re))
			{
				*packPriority = config->unpackAppsList.at(i)->priority;
				return config->unpackAppsList.at(i)->appId;
			}
		}
	}

	return 0;
}

void QMgr::slotUnpackCancelled()
{
	qDebug() << "Cancelled groupId " << unpackConfirmation->groupId;

	GroupManager* thisGroup = 0;

	// Find the group manager
	thisGroup = groupedDownloads.value(unpackConfirmation->groupId);

	 if (!thisGroup)
	 {
         qDebug() << "Failed to find cancelled Group with id " << unpackConfirmation->groupId;
	 }
	 else
	     removeAutoGroup(thisGroup);

	qDeleteAll(unpackConfirmation->headers);
	unpackConfirmation->headers.clear();

	delete unpackConfirmation;
	unpackConfirmation = 0;

	if (droppedNzbFiles.size() > 0)
	{
		processNextDroppedFile();
	}
}

void QMgr::slotUnpackConfirmed(QString fullNzbFilename)
{
	qDebug() << "groupId " << unpackConfirmation->groupId << ", packId "
			<< unpackConfirmation->packId << ", repairId "
			<< unpackConfirmation->repairId;

	GroupManager* thisGroup = 0;
	quint32 repairOrder = 2;

	// Find the group manager
	thisGroup = groupedDownloads.value(unpackConfirmation->groupId);

	 if (!thisGroup)
	 {
			QMessageBox::warning(this, tr("Error!"),
					tr("Unable to determine the internal group identifier: please download and unpack manually"));
			return;
	 }

	 thisGroup->setRepairId(unpackConfirmation->repairId);
	 thisGroup->setPackId(unpackConfirmation->packId);

	 thisGroup->setRepairPriority(unpackConfirmation->repairPriority);
	 thisGroup->setPackPriority(unpackConfirmation->packPriority);

	 thisGroup->setNewsgroup(unpackConfirmation->ng);

     PendingHeader* pendingHeader = 0;
	 quint16 fileType;
	 quint32 repairBlocks;

	// Add to the appropriate queue/area
	for (int i = 0; i < unpackConfirmation->headers.count(); i++)
	{
		qDebug() << "File type "
				<< unpackConfirmation->headers.at(i)->UnpackFileType
				<< ", name " << unpackConfirmation->headers.at(i)->fileName;

		fileType = unpackConfirmation->headers.at(i)->UnpackFileType;

		if (fileType == QUBAN_MASTER_UNPACK || fileType == QUBAN_UNPACK)
		{
			thisGroup->addPackedFile();
			if (fileType == QUBAN_MASTER_UNPACK)
			{
				thisGroup->setMasterUnpack(unpackConfirmation->headers.at(i)->fileName);
				thisGroup->setDirPath(unpackConfirmation->dDir); // may be null
			}
		    slotAddPostItem(unpackConfirmation->headers.at(i)->hb,unpackConfirmation->ng, unpackConfirmation->first, false,
				            unpackConfirmation->dDir, unpackConfirmation->groupId, unpackConfirmation->headers.at(i)->UnpackFileType);
		}
		else if (fileType == QUBAN_MASTER_REPAIR || fileType == QUBAN_REPAIR)
		{
			thisGroup->addRepairFile();
			// Add to pending ...
			pendingHeader = new PendingHeader(pendDb, unpackConfirmation->groupId, unpackConfirmation->headers.at(i)->UnpackFileType, ++maxPendingHeader);
			pendingHeader->setHeader(unpackConfirmation->headers.at(i)->hb);
			pendingHeader->setHeaderType(unpackConfirmation->headers.at(i)->hb->getHeaderType());

			qDebug() << "Just created pending header with subj : " << pendingHeader->getHeader()->getSubj()
								<< ", type = " << pendingHeader->getHeader()->getHeaderType();

			if (fileType == QUBAN_MASTER_REPAIR)
			{
				pendingHeader->setRepairOrder(1);
				thisGroup->setMasterRepair(unpackConfirmation->headers.at(i)->fileName);
			}
			else
			{
				// set the repair order and blocks
				pendingHeader->setRepairOrder(repairOrder++);

				if (unpackConfirmation->repairId == 1) // par2
				{
					int pos = unpackConfirmation->headers.at(i)->fileName.lastIndexOf('+');
					if (pos != -1)
					{
						int endPos = unpackConfirmation->headers.at(i)->fileName.indexOf ( '.', pos);
						if (endPos != -1)
						{
							repairBlocks = unpackConfirmation->headers.at(i)->fileName.mid(pos+1, endPos - (pos + 1)).toInt();
							pendingHeader->setRepairBlocks(repairBlocks);
							thisGroup->addRepairBlocks(repairBlocks);

							qDebug() << "Setting num blocks for: " << unpackConfirmation->headers.at(i)->fileName
									<< " to: " << repairBlocks;

						}
					}
				}
			}

			pendingHeaders.insert(unpackConfirmation->groupId, pendingHeader);
			pendingHeader->dbSave();
		}
		else if (fileType == QUBAN_SUPPLEMENTARY)
		{
			thisGroup->addOtherFile();

			qDebug() << "Just added post item for ConfirmationHeader with subj : " << unpackConfirmation->headers.at(i)->hb->getSubj()
					<< ", type = " << unpackConfirmation->headers.at(i)->hb->getHeaderType();


// MD ConfirmationHeader
		    slotAddPostItem(unpackConfirmation->headers.at(i)->hb, unpackConfirmation->ng, unpackConfirmation->first, false,
				            unpackConfirmation->dDir, unpackConfirmation->groupId, unpackConfirmation->headers.at(i)->UnpackFileType);
		}
	}

	thisGroup->setGroupStatus(QUBAN_GROUP_DOWNLOADING_PACKED);
	thisGroup->dbSave();

	qDeleteAll(unpackConfirmation->headers);
	unpackConfirmation->headers.clear();

	delete unpackConfirmation;
	unpackConfirmation = 0;

    if (config->renameNzbFiles == true && fullNzbFilename != QString::null)
    {
	    QFile::rename(fullNzbFilename, fullNzbFilename + tr(NZB_QUEUED_SUFFIX));
    }

	if (droppedNzbFiles.size() > 0)
	{
		processNextDroppedFile();
	}
}

bool pendHeaderLessThan(const PendingHeader* s1, const PendingHeader* s2)
{
    return s1->pendingHeader->repairOrder < s2->pendingHeader->repairOrder;
}

void QMgr::checkIfGroupDownloadOk(quint16 fileType, GroupManager* thisGroup) // It's finished the stage it was doing
{
    if (fileType == QUBAN_MASTER_UNPACK || fileType == QUBAN_UNPACK)
    {
		if (thisGroup->getNumUnpackFiles() == thisGroup->getNumDecodedUnpackFiles()) // attempt unpack
		{
			qDebug() << "Attempt to unpack";
			thisGroup->setGroupStatus(QUBAN_GROUP_UNPACKING);
			thisGroup->dbSave();

			AutoUnpackThread* unpackThread = new AutoUnpackThread(thisGroup, this);
			emit sigExtProcStarted(thisGroup->getGroupId(), QUBAN_UNPACK, thisGroup->getMasterUnpack());
			thisGroup->setWorkingThread(unpackThread);
			unpackThread->processGroup();
		}
		else // needs a repair
		{
			startRepair(thisGroup);
		}
    }
    else if (fileType == QUBAN_MASTER_REPAIR || fileType == QUBAN_REPAIR ||
    		(fileType == QUBAN_SUPPLEMENTARY && thisGroup->getNumReleasedRepairFiles() > 0))
    {
    	if (thisGroup->downloadGroup->repairId > 0)
    	    startRepair(thisGroup);
    	else
    		emit sigStatusMessage(tr("Unable to unpack group files and no repair files available"));;
    }
    else if (fileType == QUBAN_SUPPLEMENTARY && thisGroup->getNumUnpackFiles() == 0) // may be a group of text files? release all repair?
    {
    	if (thisGroup->getNumRepairFiles() > 0)
    	{
    		startRepair(thisGroup);
    	}
    }
}

void QMgr::startRepair(GroupManager* thisGroup)
{
	if (thisGroup->repairNext == true ||
			(thisGroup->getNumReleasedRepairFiles() == thisGroup->getNumDecodedRepairFiles() &&
			 thisGroup->getNumReleasedRepairFiles() > 0 &&
			 thisGroup->usedAllDownloadedRepairFiles == false)) // attempt repair
	{
		thisGroup->repairNext = false;
		thisGroup->usedAllDownloadedRepairFiles = true;
		thisGroup->setGroupStatus(QUBAN_GROUP_REPAIRING);
		thisGroup->dbSave();

		AutoUnpackThread* unpackThread = new AutoUnpackThread(thisGroup, this);
		emit sigExtProcStarted(thisGroup->getGroupId(), QUBAN_REPAIR, thisGroup->getMasterRepair());
		thisGroup->setWorkingThread(unpackThread);
		unpackThread->processGroup();
	}
	else // Get more repair files ??
	{
		quint32 numPartsRequired = 5;
		quint32 numPartsFound = 0;

		if (thisGroup->downloadGroup->numBlocksNeeded > 0)
		{
			numPartsRequired = thisGroup->downloadGroup->numBlocksNeeded;
			thisGroup->downloadGroup->numBlocksNeeded = 0;
		}
		else if (thisGroup->getNumFailedDecodeUnpackParts() > numPartsRequired)
			numPartsRequired = thisGroup->getNumFailedDecodeUnpackParts();

		qDebug() << "Attempting to get " << numPartsRequired << " blocks";

		if (thisGroup->getNumReleasedRepairFiles() < thisGroup->getNumRepairFiles()) // Do we have any more repair files?
		{
			thisGroup->repairNext = true;
			thisGroup->usedAllDownloadedRepairFiles = false;
			qDebug() << "Attempt to download repair files";
			thisGroup->setGroupStatus(QUBAN_GROUP_DOWNLOADING_REPAIR);

			QList <PendingHeader*> headerList = pendingHeaders.values(thisGroup->getGroupId());
			qSort(headerList.begin(), headerList.end(), pendHeaderLessThan);

			bool finished = false;

			// Make sure that we have the master repair file!!
			if (headerList.size() > 0 &&
				headerList.at(0)->getFileType() == QUBAN_MASTER_REPAIR)
			{
				slotAddPostItem(headerList.at(0)->getHeader(), thisGroup->getNewsGroup(), true, false,
						thisGroup->getDirPath(), thisGroup->getGroupId(), headerList.at(0)->getFileType());

				numPartsFound += headerList.at(0)->getRepairBlocks();
				thisGroup->addReleasedRepair();
				thisGroup->addReleasedRepairParts(headerList.at(0)->getRepairBlocks());
				thisGroup->removeRepairBlocks(headerList.at(0)->getRepairBlocks());
				thisGroup->dbSave();
				// remove the pendingHeader from the multi map
				pendingHeaders.remove(thisGroup->getGroupId(), headerList.at(0));
				headerList.at(0)->dbDelete();
				delete headerList.at(0);
				headerList.removeAt(0);
			}

			while (finished == false && headerList.size() > 0)
			{
				for (int i = (headerList.size() - 1); i >= 0; --i)
				{
					if (numPartsFound < numPartsRequired)
					{
						if (headerList.at(i)->getRepairBlocks() <= (numPartsRequired - numPartsFound) || i == 0)
						{
							slotAddPostItem(headerList.at(i)->getHeader(), thisGroup->getNewsGroup(), true, false,
									thisGroup->getDirPath(), thisGroup->getGroupId(), headerList.at(i)->getFileType());

							numPartsFound += headerList.at(i)->getRepairBlocks();
							thisGroup->addReleasedRepair();
							thisGroup->addReleasedRepairParts(headerList.at(i)->getRepairBlocks());
							thisGroup->removeRepairBlocks(headerList.at(i)->getRepairBlocks());
							thisGroup->dbSave();
							// remove the pendingHeader from the multi map
							pendingHeaders.remove(thisGroup->getGroupId(), headerList.at(i));
							headerList.at(i)->dbDelete();
							delete headerList.at(i);
							headerList.removeAt(i);
						}
					}
					else
						finished = true;
				}
			}
		}
		else if (thisGroup->usedAllDownloadedRepairFiles == false) // No more files, attempt repair then unpack
		{
			thisGroup->repairNext = false;
			thisGroup->setGroupStatus(QUBAN_GROUP_REPAIRING);
			thisGroup->dbSave();
			qDebug() << "No more files, attempt to repair ...";

			thisGroup->usedAllDownloadedRepairFiles = true;
			AutoUnpackThread* unpackThread = new AutoUnpackThread(thisGroup, this);
			emit sigExtProcStarted(thisGroup->getGroupId(), QUBAN_REPAIR, thisGroup->getMasterRepair());
			thisGroup->setWorkingThread(unpackThread);
			unpackThread->processGroup();
		}
		else // Repair is not possible!!
		{
			thisGroup->setGroupStatus(QUBAN_GROUP_FAILED);
			thisGroup->dbSave();

			// Advise user of failure ...
			emit sigStatusMessage(tr("Unable to repair group files"));
		}
	}
}

void QMgr::removeAutoGroup(GroupManager* thisGroup)
{
	QString index;

	// First see if we need to get rid of any downloaded files
	removeAutoGroupFiles(thisGroup, false);

	// Now get rid of any Pending Files that we didn't download
	QList <PendingHeader*> headerList = pendingHeaders.values(thisGroup->getGroupId());
	QList<PendingHeader*>::iterator it;

	for ( it = headerList.begin(); it != headerList.end(); ++it)
	{
		// If we have pending nzb items that we are about to delete them remove them from the nzb db
        if (thisGroup->getNewsGroup()->getName() == "nzb")
        {
			index = (*it)->getHeader()->getSubj().simplified() + "\n" + (*it)->getHeader()->getFrom();
			qDebug() << "deleting nzb item " << index << " from nzb db";

			delNzbItem(index);
        }

		// remove the pendingHeader from the multi map
		pendingHeaders.remove(thisGroup->getGroupId(), (*it));
		(*it)->dbDelete();
        (*it)->deleteHeader();
        Q_DELETE((*it));
	}

	// Finally get rid of the group
	groupedDownloads.remove(thisGroup->getGroupId());
	thisGroup->dbDelete();
	delete thisGroup;
}

void QMgr::removeAutoGroupFiles(GroupManager* thisGroup, bool keepGroupRecord)
{
	// Get rid of any files that the user has advised need deleting
	QList <AutoFile*> autoFileList = autoFiles.values(thisGroup->getGroupId());
	QList<AutoFile*>::iterator it;
	QString fileToDelete;

	int i=0;

	for (it = autoFileList.begin(); it != autoFileList.end(); ++it)
	{
		if (thisGroup->getGroupStatus() == QUBAN_GROUP_SUCEEDED)
		{
			if (((*it)->getFileType() == QUBAN_MASTER_UNPACK || (*it)->getFileType() == QUBAN_UNPACK) &&
					config->deleteCompressedFiles)
			{
				fileToDelete = thisGroup->downloadGroup->dirPath + "/" + (*it)->getFileName();
				QFile::remove(fileToDelete);
			}

			if (((*it)->getFileType() == QUBAN_MASTER_REPAIR || (*it)->getFileType() == QUBAN_REPAIR) &&
					config->deleteRepairFiles)
			{
				fileToDelete = thisGroup->downloadGroup->dirPath + "/" + (*it)->getFileName();
				QFile::remove(fileToDelete);
			}

			if ((*it)->getFileType() == QUBAN_SUPPLEMENTARY  && config->deleteOtherFiles)
			{
				fileToDelete = thisGroup->downloadGroup->dirPath + "/" + (*it)->getFileName();
				QFile::remove(fileToDelete);
			}
		}

		if (keepGroupRecord == false)
		{
		    autoFiles.remove(thisGroup->getGroupId(), (*it));

			// the database delete takes all duplicate keys so needed once only
			if (!i)
			{
				++i;
				(*it)->dbDelete();
			}

            Q_DELETE((*it));
		}
	}
}

void QMgr::cancelGroupItems() // we get here if user has not reloaded saved queued items
{
bool updated = false;

    QList <GroupManager*>autoGroupList = groupedDownloads.values();
	QList<GroupManager*>::iterator it;

	for ( it = autoGroupList.begin(); it != autoGroupList.end(); ++it)
	{
		if ((*it)->getNumDecodedUnpackFiles() + (*it)->getNumFailedDecodeUnpackFiles() +
			(*it)->getNumCancelledUnpackFiles() != (*it)->getNumUnpackFiles())
		{
			(*it)->setNumCancelledUnpack((*it)->getNumUnpackFiles() - ((*it)->getNumDecodedUnpackFiles() +
					(*it)->getNumFailedDecodeUnpackFiles()));
		}

		if ((*it)->getNumDecodedRepairFiles() + (*it)->getNumFailedDecodeRepairFiles() +
			(*it)->getNumCancelledRepairFiles() != (*it)->getNumRepairFiles())
		{
			(*it)->setNumCancelledRepair((*it)->getNumRepairFiles() - ((*it)->getNumDecodedRepairFiles() +
					(*it)->getNumFailedDecodeRepairFiles()));
		}

		if ((*it)->getNumDecodedOtherFiles() + (*it)->getNumFailedDecodeOtherFiles() +
			(*it)->getNumCancelledOtherFiles() != (*it)->getNumOtherFiles())
		{
			(*it)->setNumCancelledOther((*it)->getNumOtherFiles() - ((*it)->getNumDecodedOtherFiles() +
					(*it)->getNumFailedDecodeOtherFiles()));
		}

		if (updated == true)
		{
			(*it)->setGroupStatus(QUBAN_GROUP_CANCELLED);
			(*it)->dbSave();

		    updated = false;
		}
	}
}

void QMgr::slotAdjustQueueWidth()
{
	queueList->resizeColumnToContents(0);
	queueList->resizeColumnToContents(1);
	queueList->resizeColumnToContents(4);
}

void QMgr::closeEvent(QCloseEvent * e)
{
	e->ignore();
}

void QMgr::started(int serverID, int threadID)
{
	emit sigThreadStart(serverID, threadID);

}

void QMgr::expireFinished(Job* j)
{
	qDebug() << "Finished expiring";
	finished(j);

	delete j;
}

void QMgr::expireFailed(Job* j)
{
	qDebug("Error updating Db or Cancelled");
	emit sigUpdateFinished(j->ng);

	delete j;
}

void QMgr::bulkDeleteFinished(quint64 seq)
{
	qDebug() << "Finished bulk deletion for seq " << seq;
}

void QMgr::bulkDeleteFailed(quint64 seq)
{
	qDebug() << "Error updating bulk delete Db or Cancelled for seq " << seq;
}

void QMgr::bulkLoadFinished(quint64 seq)
{
	qDebug() << "Finished bulk load for seq " << seq;
}

void QMgr::bulkLoadFailed(quint64 seq)
{
	qDebug() << "Error updating bulk load Db or Cancelled for seq " << seq;
}

void QMgr::bulkGroupFinished(quint64 seq)
{
    qDebug() << "Finished bulk grouping for seq " << seq;
}

void QMgr::bulkGroupFailed(quint64 seq)
{
    qDebug() << "Error updating bulk group Db or Cancelled for seq " << seq;
}

void QMgr::decodeStarted(QPostItem* item)
{
    decoding = true;
    item->startDecoding();
}

void QMgr::decodeProgress(QPostItem* item, int progress)
{
    item->updateDecodingProgress(progress);
}

void QMgr::slotCancelExternal(quint64 groupId)
{
	// Find the group manager
	GroupManager* thisGroup = 0;

	thisGroup = groupedDownloads.value((quint16)groupId);

	 if (thisGroup)
	 {
		 AutoUnpackThread* extProc = thisGroup->getWorkingThread();
		 if (extProc && extProc->getProcess())
		 {
			 extProc->getProcess()->close();
			 extProc->clearProcess();
		 }
	 }
}

void QMgr::slotCancelBulkDelete(quint64 seq)
{
	emit cancelBulkDelete(seq);
}

void QMgr::slotCancelBulkLoad(quint64 seq)
{
	emit cancelBulkLoad(seq);
}

void QMgr::slotCancelBulkGroup(quint64 seq)
{
    emit cancelBulkGroup(seq);
}

void QMgr::serverValidated(uint serverId, bool validated, QString errorString, QList<QSslError> sslErrs)
{
    NntpHost* nh = servers->value(serverId);
    nh->serverValidated(validated, errorString, sslErrs);
    if (validated)
        emit sigServerValidated(serverId);
}
