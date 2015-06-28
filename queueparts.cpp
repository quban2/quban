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
#include <QFileInfo>
#include <fstream>
#include <libgen.h>
#include <QRegExp>
#include "appConfig.h"
#include "qmgr.h"
#include "queueparts.h"

extern Quban* quban;

static int col2width = 0;
static int col3width = 0;

//Q_DECLARE_METATYPE(QList<QSslError>)

Thread::Thread(uint _serverId, uint _threadId, uint to, uint rc, bool _validated, bool _validate, QWidget * p, NntpHost *nh) :
    serverId(_serverId), threadId(_threadId), threadTimeout(rc * 60 * 1000) /* minutes */, validated(_validated)
{
    retryCount = 0;
    timeout = to * 1000;

    //qDebug() << "Idle to = " << threadTimeout;
    threadBytes = new uint;
    *threadBytes = 0;
    prevBytes = 0;
    resetSpeed();

    QueueScheduler* queueScheduler = ((QMgr*)p)->getQueueScheduler();
    nntpT = new NntpThread(serverId, threadId, threadBytes, queueScheduler->getIsRatePeriod(), queueScheduler->getIsNilPeriod(), validated, _validate, nh, (QMgr*)p);

    connect(nntpT, SIGNAL(StartedWorking(int, int)), p, SLOT(started(int, int)));
    connect(nntpT, SIGNAL(Start(Job *)), p, SLOT(start(Job *)));
    connect(nntpT, SIGNAL(DownloadFinished(Job *)), p, SLOT(finished(Job *)));
    connect(nntpT, SIGNAL(DownloadCancelled(Job *)), p, SLOT(downloadCancelled(Job *)));
    connect(nntpT, SIGNAL(DownloadError(Job *, int)), p, SLOT(downloadError(Job *, int)));
    connect(nntpT, SIGNAL(Finished(Job *)), p, SLOT(finished(Job *)));
    connect(nntpT, SIGNAL(Cancelled(Job *)), p, SLOT(cancel(Job *)));
    connect(nntpT, SIGNAL(Err(Job *, int)), p, SLOT(Err(Job *, int)));
    connect(nntpT, SIGNAL(Failed(Job *, int)), p, SLOT(Failed(Job *, int)));
    connect(nntpT, SIGNAL(SigPaused(int, int, bool)), p, SLOT(paused(int, int, bool)));
    connect(nntpT, SIGNAL(SigDelayed_Delete(int, int)), p, SLOT(delayedDelete(int, int)));
    connect(nntpT, SIGNAL(SigReady(int, int)), p, SLOT(stopped(int, int)));
    connect(nntpT, SIGNAL(SigClosingConnection(int, int)), p, SLOT(connClosed(int, int)));
    connect(nntpT, SIGNAL(SigUpdate(Job *, uint, uint, uint)), p, SLOT(update(Job *, uint, uint, uint)));
    connect(nntpT, SIGNAL(SigUpdatePost(Job *, uint, uint, uint, uint)), p, SLOT(updatePost(Job *, uint, uint, uint, uint)));
    connect(nntpT, SIGNAL(SigUpdateLimits(Job *, uint, uint, uint)), p, SLOT(updateLimits(Job *, uint, uint, uint)));
    connect(nntpT, SIGNAL(sigHeaderDownloadProgress(Job*, quint64, quint64, quint64)), p, SLOT(slotHeaderDownloadProgress(Job*, quint64, quint64, quint64)));
    connect(nntpT, SIGNAL(SigExtensions(Job *, quint16, quint64)), p, SLOT(updateExtensions(Job *, quint16, quint64)));
    connect(nntpT, SIGNAL(logMessage(int, QString)), quban->getLogAlertList(), SLOT(logMessage(int, QString)));
    connect(nntpT, SIGNAL(logEvent(QString)), quban->getLogEventList(), SLOT(logEvent(QString)));

    connect(nntpT, SIGNAL(serverValidated(uint, bool, QString, QList<QSslError>)), p, SLOT(serverValidated(uint, bool, QString, QList<QSslError>)));

    connect(nntpT, SIGNAL(registerSocket(RcSslSocket*)), p, SIGNAL(registerSocket(RcSslSocket*))); // Pass it on to RateController
    connect(nntpT, SIGNAL(unregisterSocket(RcSslSocket*)), p, SIGNAL(unregisterSocket(RcSslSocket*))); // Pass it on to RateController

    speedTimer = new QTimer();
    speedTimer->setSingleShot(false);
    idleTimer = new QTimer();
    retryTimer = new QTimer();
    connect(speedTimer, SIGNAL(timeout()), SLOT(slotSpeedTimeout()));
    connect(idleTimer, SIGNAL(timeout()), SLOT(slotIdleTimeout()));
    connect(retryTimer, SIGNAL(timeout()), SLOT(slotRetryTimeout()));
}

void Thread::resetSpeed()
{
    speedIndex = 0;
    for (int i = 0; i < SPEED_MEAN; i++)
    {
        speedBytes[i][0] = 0;
        speedBytes[i][1] = 0;
    }
    prevTime = QTime::currentTime();
}

void Thread::setValidated(bool _validated)
{
    validated = _validated;
    nntpT->setValidated(validated);
}

void Thread::validate()
{
    nntpT->tValidate();
    start();
}

bool Thread::stop()
{
    speedTimer->stop();

    if (nntpT->isConnected())
    {
        idleTimer->start(threadTimeout);
        return true;
    }
    else
    {
        return false;
    }
}

void Thread::slotSpeedTimeout()
{
    curTime = QTime::currentTime();
    interval = prevTime.msecsTo(curTime);
    prevTime = curTime;
    speedBytes[speedIndex][0] = *threadBytes - prevBytes;
    prevBytes = *threadBytes;
    speedBytes[speedIndex][1] = interval;
    bytes = 0;
    interval = 0;
    for (int i = 0; i < SPEED_MEAN; i++)
    {
        bytes += speedBytes[i][0];
        interval += speedBytes[i][1];

    }

    emit sigThreadSpeedChanged(serverId, threadId,
            (int) (((bytes / 1024) / (float) (interval / 1000))));
    if (++speedIndex >= SPEED_MEAN)
        speedIndex = 0;
}

void Thread::error()
{
    // 	retryCount=0;

}

void Thread::slotRetryTimeout()
{
    //Unpause the job
    qDebug("%d, %d: Thread::slotRetryTimeout", serverId, threadId);

    nntpT->tResume();
    emit sigThreadResumed();
    speedTimer->start(1000);
    idleTimer->stop();
}

void Thread::comError()
{
    //Now I'm paused...
    speedTimer->stop();
    resetSpeed();
    idleTimer->stop();

    // 	qDebug("%d, %d: retrycount: %d", serverId, threadId, retryCount);

    if (retryCount < RETRYCOUNT)
    {
        // 		qDebug("Starting %d,%d retry Timer", serverId, threadId);
        retryTimer->setSingleShot(true);
        retryTimer->start(retryTimeouts[retryCount] * 1000);
        emit sigThreadPaused(serverId, threadId, retryTimeouts[retryCount]);
        retryCount++;
    }
    else if (retryCount > RETRYCOUNT)
    //else if (retryCount > (RETRYCOUNT + threadTimeout + 1))
    {
        //Pause the thread!!!
        emit sigThreadPaused(serverId, threadId, 0);
    }
    else
    {
        retryTimer->setSingleShot(true);
        retryTimer->start(60000);
        retryCount++;

        emit sigThreadPaused(serverId, threadId, 60);
    }
}

void Thread::paused()
{
    speedTimer->stop();
    resetSpeed();

    // TODO	idleTimer->start(threadTimeout, true);
    // 	qDebug("Thread Paused");

}


void Thread::threadCancel()
{
    nntpT->tCancel();

    retryCount = 0;
}

void Thread::canceled()
{
    if (nntpT->isRunning())
        nntpT->wait();

    speedTimer->stop();
    resetSpeed();
}

void Thread::start()
{
    if (!nntpT->isRunning())
        nntpT->tStart();
}

void Thread::pause()
{
    nntpT->tPause();
}

void Thread::resume()
{
    idleTimer->stop();
    nntpT->tResume();
}

void Thread::updateNilPeriod(qint64 oldSpeed, qint64 newSpeed) // one of these must be zero ...
{
    if (oldSpeed == 0)
    {
        nntpT->setNilPeriod(false);
        if (newSpeed != -1)
            nntpT->setRatePeriod(true);
        else
            nntpT->setRatePeriod(false);

        // wake up the thread
        idleTimer->stop();
        nntpT->tResume();
    }
    else
    {
        nntpT->setNilPeriod(true);
        nntpT->setRatePeriod(true);
    }
}

Thread::~Thread()
{
    if (speedTimer)
    {
        speedTimer->stop();
        Q_DELETE(speedTimer);
    }
    if (idleTimer)
    {
        idleTimer->stop();
        Q_DELETE(idleTimer);
    }
    if (retryTimer)
    {
        retryTimer->stop();
        Q_DELETE(retryTimer);
    }

    Q_DELETE(nntpT);
    Q_DELETE(threadBytes);
}

void Thread::closeCleanly()
{
    nntpT->closeConnection(true);

    if (!nntpT->isRunning())
        nntpT->tStart();
}

void Thread::kill()
{
    if (nntpT->isRunning())
    {
        nntpT->terminate();
        nntpT->wait();
    }
}


void Thread::slotIdleTimeout()
{
    nntpT->closeConnection(false);
    //qDebug("Quit idle thread");
    emit sigThreadDisconnected(serverId, threadId);
}


void Thread::setQueue(QList<int> *tq, QMap<int, QItem*> *q, QMutex *qLock,
        int *po)
{
    nntpT->setQueue(tq, q, qLock, po);
}


void Thread::setDbSync(QMutex *qLock)
{
    nntpT->setDbSync(qLock);
}

void Thread::resetRetryCounter()
{
    retryCount = 0;
}

void Thread::started()
{
    speedTimer->start(1000);
    resetSpeed();
    idleTimer->stop();
}

QItem::QItem(QTreeWidget * parent, int id, int _type, NewsGroup *_ng)
{
    qItemId = id;
    type = _type;
    status = QItem::Queued_Item;
    partsToDo = 0;
    failedParts = 0;
    totalParts = 0;
    workingThreads = 0;
    userGroupId = 0;
    userFileType = 0;
    processing = false;

    listItem = new QTreeWidgetItem();
    listItem->setText(0, QString::number(id));
    listItem->setText(1, "Update " + _ng->getAlias());
    listItem->setText(2, "Queued");
    parent->insertTopLevelItem(0, listItem);
}

QItem::QItem(QTreeWidget * parent, int id, int _type, NewsGroup *_ng, QString& hostname)
{
    qItemId = id;
    type = _type;
    status = QItem::Queued_Item;
    partsToDo = 0;
    failedParts = 0;
    totalParts = 0;
    workingThreads = 0;
    userGroupId = 0;
    userFileType = 0;
    processing = false;

    listItem = new QTreeWidgetItem();
    listItem->setText(0, QString::number(id));
    listItem->setText(1, "Update " + _ng->getAlias() + " for " + hostname);
    listItem->setText(2, "Queued");
    parent->insertTopLevelItem(0, listItem);
}

QItem::QItem(QTreeWidget *parent, int id, int _type, QString subj, bool first)
{
    qItemId = id;
    type = _type;
    status = QItem::Queued_Item;
    partsToDo = 0;
    totalParts = 0;
    failedParts = 0;
    workingThreads = 0;
    userGroupId = 0;
    userFileType = 0;

    if (!first)
    {
        listItem = new QTreeWidgetItem(parent);
        listItem->setText(0, QString::number(id));
        listItem->setText(1, subj);
    }
    else
    {
        listItem = new QTreeWidgetItem();
        listItem->setText(0, QString::number(id));
        listItem->setText(1, subj);
        parent->insertTopLevelItem(0, listItem);
    }
    listItem->setText(2, "Queued");
}

QItem::QItem(QTreeWidget * parent, int id, int _type, HeaderBase *hb, bool first)
{
    qItemId = id;
    type = _type;
    status = QItem::Queued_Item;
    partsToDo = 0;
    totalParts = 0;
    failedParts = 0;
    workingThreads = 0;
    userGroupId = 0;
    userFileType = 0;

    processing = false;

    if (!first)
    {
        listItem = new QTreeWidgetItem(parent);
        listItem->setText(0, QString::number(id));
        listItem->setText(1, hb->getSubj());
    }
    else
    {
        listItem = new QTreeWidgetItem();
        listItem->setText(0, QString::number(id));
        listItem->setText(1, hb->getSubj());
        parent->insertTopLevelItem(0, listItem);
    }
    listItem->setText(2, "Queued");
}

QItem::QItem(QString hostname, QTreeWidget* parent, int id, int _type)
{
    qItemId = id;
    type = _type;
    status = QItem::Queued_Item;
    partsToDo = 0;
    failedParts = 0;
    workingThreads = 0;
    userGroupId = 0;
    userFileType = 0;

    listItem = new QTreeWidgetItem(parent);
    listItem->setText(0, QString::number(id));
    listItem->setText(1, "Get list of all groups for " + hostname);
    processing = false;
}

bool QItem::finished(int partId)
{
    // 	status=QItem::Finished_Item;
    parts[partId]->status = QItem::Finished_Part;
    parts[partId]->listItem->setText(2, "Finished");
    parts[partId]->listItem->setText(3, "100%");

    workingThreads--;

    partsToDo--;
    if (partsToDo == 0)
    {
        listItem->setText(2, "Finished");
        status = QItem::Finished_Item;

        return true;
    }
    else
    {
        if (workingThreads == 0)
            listItem->setText(2, "Queued");
        else
            listItem->setText(2,
                    "Processing (" + QString::number(workingThreads) + ")");

        return false;
    }
}

void QItem::canceled(int partId)
{
    status = QItem::Canceled_Item;
    parts[partId]->status = QItem::Canceled_Part;
    partsToDo--;
    // 	workingThreads--;

    //Not useful anymore
    if (partsToDo == 0)
    {
        listItem->setText(2, "Cancelled");
        status = QItem::Canceled_Item;
    }
    parts[partId]->listItem->setText(2, "Cancelled");
    parts[partId]->listItem->setText(3, "");
}


QItem::~QItem()
{
    if (parts.count())
    {
        qDeleteAll(parts);
        parts.clear();
    }
}

void QItem::requeue(int partId)
{
    parts[partId]->status = QItem::Queued_Part;
    workingThreads--;
    parts[partId]->listItem->setText(2, "Queued");
    parts[partId]->listItem->setText(3, "");
}

void QItem::paused(int partId)
{
    status = QItem::Paused_Item;
    parts[partId]->status = QItem::Paused_Part;
    parts[partId]->listItem->setText(2, "Paused");
}

void QItem::resumed(int partId)
{
    status = QItem::Queued_Item;
    parts[partId]->status = QItem::Queued_Part;
    parts[partId]->listItem->setText(2, "Queued");
}

void QItem::start(int partId)
{
    parts[partId]->listItem->setText(2, "Downloading");
    parts[partId]->listItem->setText(3, "0%");
    workingThreads++;
    listItem->setText(2, "Processing (" + QString::number(workingThreads) + ")");
}

QUpdItem::QUpdItem(QTreeWidget * parent, int id, NewsGroup * ng, QString& hostname) :
    QItem(parent, id, Job::UpdHead, ng, hostname)
{
}

void QUpdItem::update(int partId, int partial, int total, int)
{
	if (total == 0)
		return;

	parts[partId]->listItem->setText(3, QString::number((partial * 100) / total) + '%');

	listItem->setText(
			2,
			"Processing (" + QString::number(workingThreads) + ") "
                + QString::number(((partial * 100) / total)) + '%');

	if (workingThreads == 1)
	{
		if (((QAbstractItemView*)listItem->treeWidget())->sizeHintForColumn(2) > col2width)
		{
			col2width = ((QAbstractItemView*)listItem->treeWidget())->sizeHintForColumn(2);
			listItem->treeWidget()->resizeColumnToContents(2);
		}
	}
}

QUpdItem::~QUpdItem()
{
    listItem->setSelected(false);
    Q_DELETE(listItem);
}

bool QUpdItem::finished(int partId)
{
    // 	qDebug("QUpdItem::finished");

    return QItem::finished(partId);
}

void QUpdItem::addJobPart(int jobId, int partId, Part * part)
{
    // 	jobPart.insert(jobId, partId);
    parts.insert(partId, part);
    if (listItem->childCount() == 0)
    {
        part->listItem = new QTreeWidgetItem(listItem);
        part->listItem->setText(0, QString::number(jobId));
        part->listItem->setText(1, part->desc);
        part->listItem->setText(2, "Queued");
    }
    else
    {
        QMap<int, Part*>::iterator it = parts.end();
        --it;
        --it;
        part->listItem = new QTreeWidgetItem(listItem, it.value()->listItem);
        part->listItem->setText(0, QString::number(jobId));
        part->listItem->setText(1, part->desc);
        part->listItem->setText(2, "Queued");
    }
    ++partsToDo;
    // TODO	part->listItem->setSelectable(false);
}

int QUpdItem::error(int partId, QString & error)
{
    parts[partId]->status = QItem::Failed_Part;
    parts[partId]->job->status = Job::Failed_Job;
    partsToDo--;
    failedParts++;
    workingThreads--;
    parts[partId]->listItem->setText(2, tr("Failed"));
    parts[partId]->listItem->setText(3, error);
    if (partsToDo == 0)
    {
        listItem->setText(2, tr("Finished!"));
        status = QItem::Finished_Item;
        return QItem::Finished;
    }
    else
    {
        listItem->setText(2, tr("Queued"));
        return QItem::Continue;
    }
}

void QUpdItem::startExpiring(int partId)
{

    parts[partId]->listItem->setText(2, "Expiring");
    parts[partId]->listItem->setText(3, "0%");
}

void QUpdItem::stopExpiring(int partId)
{
    parts[partId]->listItem->setText(2, "Idle");
}

void QUpdItem::stopUpdateDb(int partId)
{
    parts[partId]->listItem->setText(2, "Done");
}

void QUpdItem::comError(int partId)
{
    parts[partId]->status = QItem::Requeue_Part;
    // 	workingThreads--;
    parts[partId]->listItem->setText(2, "Waiting for db update");
    parts[partId]->listItem->setText(3, "");
}

void QGroupLimitsItem::update(int partId, int partial, int total, int)
{
	if (total == 0)
		return;

	parts[partId]->listItem->setText(3,
			QString::number((partial * 100) / total) + '%');

	listItem->setText(
			2,
			"Processing (" + QString::number(workingThreads) + ") "
                + QString::number(((partial * 100) / total)) + '%');

	if (workingThreads == 1)
	{
		if (((QAbstractItemView*)listItem->treeWidget())->sizeHintForColumn(2) > col2width)
		{
			col2width = ((QAbstractItemView*)listItem->treeWidget())->sizeHintForColumn(2);
			listItem->treeWidget()->resizeColumnToContents(2);
		}
	}
}

void QGroupLimitsItem::addJobPart(int jobId, int partId, Part * part)
{
    // 	jobPart.insert(jobId, partId);
    parts.insert(partId, part);
    if (listItem->childCount() == 0)
    {
        part->listItem = new QTreeWidgetItem(listItem);
        part->listItem->setText(0, QString::number(jobId));
        part->listItem->setText(1, part->desc);
        part->listItem->setText(2, "Queued");
    }
    else
    {
        QMap<int, Part*>::iterator it = parts.end();
        --it;
        --it;
        part->listItem = new QTreeWidgetItem(listItem, it.value()->listItem);
        part->listItem->setText(0, QString::number(jobId));
        part->listItem->setText(1, part->desc);
        part->listItem->setText(2, "Queued");
    }
    ++partsToDo;
    // TODO	part->listItem->setSelectable(false);
}

QGroupLimitsItem::QGroupLimitsItem(QTreeWidget * parent, int id, NewsGroup * ng) :
    QItem(parent, id, Job::GetGroupLimits, ng)
{
}

int QGroupLimitsItem::error(int partId, QString & error)
{

    parts[partId]->status = QItem::Failed_Part;
    parts[partId]->job->status = Job::Failed_Job;
    partsToDo--;
    failedParts++;
    workingThreads--;
    parts[partId]->listItem->setText(2, tr("Failed"));
    parts[partId]->listItem->setText(3, error);
    if (partsToDo == 0)
    {
        listItem->setText(2, tr("Finished!"));
        status = QItem::Finished_Item;
        return QItem::Finished;
    }
    else
    {
        listItem->setText(2, tr("Queued"));
        return QItem::Continue;
    }
}

QGroupLimitsItem::~QGroupLimitsItem()
{
    listItem->setSelected(false);
    Q_DELETE(listItem);
}

void QGroupLimitsItem::comError(int partId)
{
    parts[partId]->status = QItem::Requeue_Part;
    // 	workingThreads--;
    parts[partId]->listItem->setText(2, "Queued");
    parts[partId]->listItem->setText(3, "");
}

QListItem::QListItem(QString& hostname, QTreeWidget * parent, int id) :
    QItem(hostname, parent, id, Job::GetList)
{
}

bool QListItem::finished(int partId)
{
	// 	parts[partId]->job->ag->stopUpdating();
	return QItem::finished(partId);
}

void QListItem::addJobPart(int jobId, int partId, Part * part)
{
    parts.insert(partId, part);
    if (listItem->childCount() == 0)
    {
        part->listItem = new QTreeWidgetItem(listItem);
        part->listItem->setText(0, QString::number(jobId));
        part->listItem->setText(1, part->desc);
        part->listItem->setText(2, "Queued");
    }
    else
    {
        QMap<int, Part*>::iterator it = parts.end();
        --it;
        --it;
        part->listItem = new QTreeWidgetItem(listItem, it.value()->listItem);
        part->listItem->setText(0, QString::number(jobId));
        part->listItem->setText(1, part->desc);
        part->listItem->setText(2, "Queued");
    }
    // TODO	part->listItem->setSelectable(false);
    ++partsToDo;
}

int QListItem::error(int partId, QString & error)
{
    qDebug() << "QListItem::error";
    parts[partId]->status = QItem::Failed_Part;
    // 	parts[partId]->job->status=Job::Failed_Job;
    partsToDo--;
    failedParts++;
    workingThreads--;
    parts[partId]->listItem->setText(2, "Failed");
    parts[partId]->listItem->setText(3, error);
    // 	parts[partId]->job->ag->stopUpdating();
    if (partsToDo == 0)
    {
        listItem->setText(2, tr("Finished"));
        status = QItem::Finished_Item;
        return QItem::Finished;
    }
    else
    {
        listItem->setText(2, tr("Queued"));
        return QItem::Continue;
    }
}

void QListItem::update(int, int, int, int)
{
}

QListItem::~QListItem()
{
    Q_DELETE(listItem);
}

QPostItem::QPostItem(QTreeWidget * parent, int id, QString subj, QString rfn,
		QString _savePath, bool first, bool _view, quint16 _userGroupId, quint16 _userFileType) :
	QItem(parent, id, Job::GetPost, subj, first)
{
	userGroupId = _userGroupId;
	userFileType = _userFileType;
	rootFName = rfn;
	savePath = _savePath;
	post = NULL;
	view = _view;
	etaTimeout = 5;
	etaTimer = 0;

	// 	qDebug("TotalLines: %d", totalPostLines);
	curPostLines = 0;
	intervalPostLines = 0;
	partialLines = 0;
	Configuration* config = Configuration::getConfig();
	overwriteExisting = config->overwriteExisting;
	deleteFailed = config->deleteFailed;
}

QPostItem::QPostItem(QTreeWidget * parent, int id, HeaderBase * hb, PartNumMap& _serverArticleNos, QString rfn,
		QString _savePath, bool first, bool _view, quint16 _userGroupId, quint16 _userFileType) :
	QItem(parent, id, Job::GetPost, hb, first)
{
	userGroupId = _userGroupId;
	userFileType = _userFileType;
	rootFName = rfn;
	savePath = _savePath;
	// 	totalPostLines=bh->getLines();
	totalPostLines = hb->getSize();
	post = hb;
	serverArticleNos = _serverArticleNos;
	view = _view;
	etaTimeout = 5;
	etaTimer = 0;

	// 	qDebug("TotalLines: %d", totalPostLines);
	curPostLines = 0;
	intervalPostLines = 0;
	partialLines = 0;
	Configuration* config = Configuration::getConfig();
	overwriteExisting = config->overwriteExisting;
	deleteFailed = config->deleteFailed;
	// 	qDebug("Created QPostItem with id %d, listItem: %d", id, listItem);
}

void QPostItem::addJobPart(int jobId, int partId, Part * part)
{

    // 	jobPart.insert(jobId, partId);


    parts.insert(partId, part);

    if (listItem->childCount() == 0)
    {
        part->listItem = new QTreeWidgetItem(listItem);
        part->listItem->setText(0, QString::number(jobId));
        part->listItem->setText(1, part->desc);
        part->listItem->setText(2, "Queued article");
    }
    else
    {
        QMap<int, Part*>::iterator it = parts.end();
        --it;
        --it;
        // 		part->listItem=new QTreeWidgetItem(listItem, parts[partId-1]->listItem);
        part->listItem = new QTreeWidgetItem(listItem, it.value()->listItem);
        part->listItem->setText(0, QString::number(jobId));
        part->listItem->setText(1, part->desc);
        part->listItem->setText(2, "Queued article");
        // 	qDebug("JobId: %d, PartId: %d", jobId, partId);
    }
    // TODO	part->listItem->setSelectable(false);
    ++partsToDo;
    ++totalParts;
    listItem->setText(3, "0/0/" + QString::number(totalParts));
    // 	qDebug("Added jobId %d to listItem %d, qItemId: %d", jobId, listItem, qItemId);
}

void QPostItem::addJobPartWithStatus(int jobId, int partId, Part * part,
        int status)
{
    //First, normally add the part...
    // 	qDebug("Status: %d", status);
    addJobPart(jobId, partId, part);
    QString err = "Failed from previous session";
    //Then simulate what happened...
    switch (status)
    {
    case QItem::Queued_Part:
        //Nothing to do...
        break;
    case QItem::Failed_Part:
        partsToDo--;
        failedParts++;
        parts[partId]->job->status = Job::Failed_Job;
        parts[partId]->listItem->setText(2, "Failed");
        parts[partId]->listItem->setText(3, err);
        break;
    case QItem::Finished_Part:
        parts[partId]->status = QItem::Finished_Part;
        parts[partId]->listItem->setText(2, "Finished");
        parts[partId]->listItem->setText(3, "100%");
        parts[partId]->job->status = Job::Finished_Job;
        partsToDo--;
        break;
    }
}

void QPostItem::update(int, int, int, int)
{
    qDebug() << "In incorrect version of QPostItem::update";
    return;
}

void QPostItem::update(int partId, int partial, int total, int lastIntervalLines, int hostId)
{
    if (total == 0)
        return;

    uint percentage = uint((float(partial) / float(total)) * 100);
    if ((percentage <= 100))
        parts[partId]->listItem->setText(3, QString::number(percentage) + '%');
    else
        parts[partId]->listItem->setText(3, QString::number(partial) + " lines");

    curPostLines += lastIntervalLines;
    if (curPostLines > totalPostLines)
    {
        curPostLines = totalPostLines;
    }

    listItem->setText(
            2,
            "Processing (" + QString::number(workingThreads) + ") "
                    + QString::number(
                            uint(float(curPostLines) / float(totalPostLines)
                                            * 100)) + '%');

    if (((QAbstractItemView*)listItem->treeWidget())->sizeHintForColumn(2) > col2width)
    {
        col2width = ((QAbstractItemView*)listItem->treeWidget())->sizeHintForColumn(2);
        listItem->treeWidget()->resizeColumnToContents(2);
    }

    // lastIntervalLines is the number of bytes read since last called
    // hostId identifies the host to add the bytes to .... signal it

//    qDebug() << "Server : " << hostId << " has just read " << lastIntervalLines << " bytes";

    emit addDataRead(hostId, lastIntervalLines);
}

void QPostItem::updateDecodingProgress(int prog)
{
    listItem->setText(2, tr("Decoding (") + QString::number(prog) + "%)");
}

void QPostItem::startDecoding()
{
    listItem->setText(2, tr("Decoding..."));
}

bool QPostItem::finished(int partId)
{
    QItem::finished(partId);
    partialLines = 0;

    if (partsToDo == 0)
    {
        if (etaTimer)
            etaTimer->stop();

        if (totalParts > failedParts)
        {
            listItem->setText(2, tr("Waiting for decoding..."));
            listItem->setText(3, "");
            listItem->setText(4, "");

            processing = true;
            emit decodeMe(this);
        }
        else
            emit decodeNotNeeded(this, false, QString::null, tr("Nothing downloaded to decode"));

        return true;
    }
    else
    {
        if (workingThreads == 0)
            etaTimer->stop();

        listItem->setText(
                3,
                QString::number(totalParts - partsToDo) + '/'
                        + QString::number(failedParts) + '/' + QString::number(
                        totalParts));

        if (((QAbstractItemView*)listItem->treeWidget())->sizeHintForColumn(3) > col3width)
        {
            col3width = ((QAbstractItemView*)listItem->treeWidget())->sizeHintForColumn(3);
            listItem->treeWidget()->resizeColumnToContents(3);
        }

        return false;
    }
}

void QPostItem::decodeFinished()
{
    listItem->setText(2, "Decoded");
}

void QPostItem::comError(int partId)
{
    parts[partId]->status = QItem::Queued_Part;
    workingThreads--;
    parts[partId]->listItem->setText(2, tr("Queued"));
    parts[partId]->listItem->setText(3, "");
    if (workingThreads == 0)
    {
        listItem->setText(2, tr("Queued"));
        if (etaTimer)
            etaTimer->stop();
    }
    else
        listItem->setText(2,
                tr("Processing (") + QString::number(workingThreads) + ")");
}

int QPostItem::error(int partId, QString & error)
{
    parts[partId]->status = QItem::Failed_Part;
    // 	parts[partId]->job->status=Job::Failed_Job;

    //Check if there is another server for the part.
    //Yes->tell the qManager to re-queue
    //No-> if this was the last part, send

    //Delete the server from the part
    serverArticleNos[partId].remove(parts[partId]->qId);
    //check if there are other parts
    workingThreads--;
    if (workingThreads == 0)
    {
        listItem->setText(2, tr("Queued"));
        etaTimer->stop();
    }
    else
        listItem->setText(2,
                tr("Processing (") + QString::number(workingThreads) + ")");

    if (serverArticleNos[partId].count() != 0)        
    {
        Servers* servers = quban->getServers();

        QMapIterator<quint16,quint64> it(serverArticleNos[partId]);
        while (it.hasNext())
        {
            it.next();
            if (servers->contains(it.key())) // it should ...
            {
                if (servers->value(it.key())->getServerType() != NntpHost::dormantServer )
                {
                    //Yes, re-queue me
                    return QItem::RequeMe;
                }
            }
        }
    }

    partsToDo--;
    failedParts++;
    parts[partId]->listItem->setText(2, tr("Failed"));
    parts[partId]->listItem->setText(3, error);
    //No, check if this was the last part...
    if (partsToDo == 0)
    {
        //last part, remove from queue
        status = QItem::Finished_Item;
        listItem->setText(2, tr("Waiting for decoding..."));
        listItem->setText(3, "");
        if (etaTimer)
        {
            etaTimer->stop();
        }
        emit decodeMe(this);
        return QItem::Finished;
    }
    else
    {
        //there are other parts, continue
        // 			listItem->setText(3, QString::number(partsToDo) + " parts left");
        listItem->setText(
                    3,
                    QString::number(totalParts - partsToDo) + '/'
                    + QString::number(failedParts) + '/'
                    + QString::number(totalParts));

        if (((QAbstractItemView*)listItem->treeWidget())->sizeHintForColumn(3) > col3width)
        {
            col3width = ((QAbstractItemView*)listItem->treeWidget())->sizeHintForColumn(3);
            listItem->treeWidget()->resizeColumnToContents(3);
        }

        return QItem::Continue;
    }

    return -1;
}


QPostItem::~QPostItem()
{
    Q_DELETE(listItem);
    if (post)
    {
        ;//Q_DELETE(post); // Why has this started core dumping ????
    }
}


void QPostItem::start(int partId)
{
    parts[partId]->listItem->setText(2, "Downloading");
    parts[partId]->listItem->setText(3, "0%");
    if (etaTimer == 0)
    {
        etaTimer = new QTimer(this);
        connect(etaTimer, SIGNAL(timeout()), this, SLOT(slotEtaTimeout()));
        prevTime = QTime::currentTime();
        intervalPostLines = curPostLines;
        etaTimer->start(etaTimeout * 1000);
    }
    else if (workingThreads == 0)
    {
        prevTime = QTime::currentTime();
        etaTimer->start(etaTimeout * 1000);
        intervalPostLines = curPostLines;
    }
    workingThreads++;
    listItem->setText(
            2,
            "Processing (" + QString::number(workingThreads) + ") "
                    + QString::number(
                            uint(
                                    float(curPostLines) / float(totalPostLines)
                                            * 100)) + '%');

    if (((QAbstractItemView*)listItem->treeWidget())->sizeHintForColumn(2) > col2width)
    {
        col2width = ((QAbstractItemView*)listItem->treeWidget())->sizeHintForColumn(2);
        listItem->treeWidget()->resizeColumnToContents(2);
    }
}

void QPostItem::slotEtaTimeout()
{
    curTime = QTime::currentTime();
    uint interval = prevTime.msecsTo(curTime);
    uint intervalLines = curPostLines - intervalPostLines;

    float speed = ((float(intervalLines) / float(interval)) * float(1000));

//	qDebug() << "In slotEtaTimeout, speed = " << speed;

    if (speed == 0)
        return;
    uint eta = uint(float(totalPostLines - curPostLines) / float(speed));
    //Maybe a mean can "stabilize" the result
    // 	uint eta=prevEta+currEta/2;
    // 	prevEta=currEta;

    //Now convert to HH::MM::SECS
    // 	qDebug("eta: %d", eta);
    uint hours = (uint) (eta / 3600);

    eta %= 3600;

    uint minutes = eta / 60;
    eta %= 60;
    //now eta = seconds.
    QString etaString = QString::number(hours).rightJustified(2, '0') + ':'
            +\
 QString::number(minutes).rightJustified(2, '0') + ':'
            + QString::number(eta).rightJustified(2, '0');

    listItem->setText(4, etaString);
}

void QPostItem::canceled(int partId)
{
    status = QItem::Canceled_Item;
    parts[partId]->status = QItem::Canceled_Part;
    partsToDo--;
    // 	workingThreads--;
    //Not useful anymore ?
    if (partsToDo == 0)
    {
        listItem->setText(2, "Cancelled");
        status = QItem::Canceled_Item;
    }
    parts[partId]->listItem->setText(2, "Cancelled");
    parts[partId]->listItem->setText(3, "");

}

void QExtensionsItem::addJobPart(int jobId, int partId, Part * part)
{
	// 	jobPart.insert(jobId, partId);
	parts.insert(partId, part);
	if (listItem->childCount() == 0)
	{
		part->listItem = new QTreeWidgetItem(listItem);
		part->listItem->setText(0, QString::number(jobId));
		part->listItem->setText(1, part->desc);
		part->listItem->setText(2, "Queued");
	}
	else
	{
		QMap<int, Part*>::iterator it = parts.end();
		--it;
		--it;
		part->listItem = new QTreeWidgetItem(listItem, it.value()->listItem);
		part->listItem->setText(0, QString::number(jobId));
		part->listItem->setText(1, part->desc);
		part->listItem->setText(2, "Queued");
	}
	++partsToDo;
	// TODO	part->listItem->setSelectable(false);
}

QExtensionsItem::QExtensionsItem(QString& hostname, QTreeWidget* parent, int id)
{
	qItemId = id;
	type = Job::GetExtensions;
	status = QItem::Queued_Item;
	partsToDo = 0;
	failedParts = 0;
	workingThreads = 0;
	userGroupId = 0;
	userFileType = 0;

	listItem = new QTreeWidgetItem(parent);
	listItem->setText(0, QString::number(id));
    listItem->setText(1, tr("Get server extensions for ") + hostname);
	processing = false;
}

int QExtensionsItem::error(int partId, QString & error)
{

    parts[partId]->status = QItem::Failed_Part;
    parts[partId]->job->status = Job::Failed_Job;
    partsToDo--;
    failedParts++;
    workingThreads--;
    parts[partId]->listItem->setText(2, tr("Failed"));
    parts[partId]->listItem->setText(3, error);
    if (partsToDo == 0)
    {
        listItem->setText(2, tr("Finished!"));
        status = QItem::Finished_Item;
        return QItem::Finished;
    }
    else
    {
        listItem->setText(2, tr("Queued"));
        return QItem::Continue;
    }
}

QExtensionsItem::~QExtensionsItem()
{
    listItem->setSelected(false);
    Q_DELETE(listItem);
}

void QExtensionsItem::comError(int partId)
{
    parts[partId]->status = QItem::Requeue_Part;
    // 	workingThreads--;
    parts[partId]->listItem->setText(2, "Queued");
    parts[partId]->listItem->setText(3, "");
}
