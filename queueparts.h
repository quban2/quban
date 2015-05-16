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

#ifndef _QUBAN_QUEUEPARTS_H
#define _QUBAN_QUEUEPARTS_H

#include <QObject>
#include <QThread>
#include <QSslSocket>

#include "nntpthreadsocket.h"
#include <QTreeWidget>
#include <QLinkedList>
#include "MultiPartHeader.h"
#include "SinglePartHeader.h"
#include "common.h"
#include <QTimer>

#define SPEED_MEAN 5

#define RETRYCOUNT 5
//Stops the thread of xx seconds for timeoutErr...
static const int retryTimeouts[] =
{ 1, 5, 10, 30, 60 };

enum QItemType
{
	UpdItem = 0, ArtItem = 1, PostItem = 2, GroupLimitsItem = 3, CapabilitiesItem
};

class QPostItem;
class NntpThread;

class Thread: public QObject
{
Q_OBJECT
private:

	uint serverId;
	uint threadId;

	uint retryCount;
	uint threadTimeout;

	NntpThread* nntpT;

	uint timeout;
	bool validated;

	uint *threadBytes; //bytes info...
	uint prevBytes; //Previous bytes, for speed...this will change
	uint priority; //Probably :) obsolete
	QString name;
	QTimer *speedTimer;
	QTimer *idleTimer;
	QTimer *retryTimer;

	//Speed calculation
	QTime prevTime;
	QTime curTime;
	int speedBytes[SPEED_MEAN][2];
	int speedIndex;
	int interval;
	float bytes;
	void resetSpeed();

	//Autovalue, to be passed to the thread...
	// 			QMutex statusLock;

	//Passed from the queueManager...
	QLinkedList<int> *threadQueue;
	QMap<int, QItem*> *queue;
	QMutex *queueLock;

public:
    Thread(uint _serverId, uint _threadId, uint to, uint rc, bool _validated, bool _validate, QWidget *p, NntpHost *nh = 0);
	virtual ~Thread();
	void start();
	void started();
	void pause();
	void paused();
	void resume();
	bool stop();
	void updateNilPeriod(qint64 oldSpeed, qint64 newSpeed);
	void threadCancel();
	void resetRetryCounter();
	void setValidated(bool);

	// 			uint getStatus() {return *status;};
	// 			uint getControl() {return *control;};
	// 			void setStatus(uint s) {status=s;};
	// 			void setJob(Job *j);
	bool isActive()
	{
		return nntpT->isRunning();
	}
    void validate();
	void error();
	void comError();
	void canceled();
	void kill();
	void closeCleanly();
	// 			void lock() {statusLock.lock();};
	// 			void unlock() {statusLock.unlock();};
	void wait()
	{
		nntpT->wait();
	}
	void setQueue(QList<int> *tq, QMap<int, QItem*> *q, QMutex *qLock, int*po);
	void setDbSync(QMutex *qLock);

private slots:
	void slotSpeedTimeout();
	void slotIdleTimeout();
	void slotRetryTimeout();signals:
	void sigThreadSpeedChanged(int, int, int);
	void sigThreadDisconnected(quint16, int);
	void sigThreadResumed();
	void sigThreadPaused(quint16, int, int);
};

/**
 @author Alessandro Bonometti
 */

struct Part
{
	int status; //status of the part
	QTreeWidgetItem* listItem; //the listViewItem that is showing the part
	QString desc; //the description of the part
	int jobId; //JobId of the part...
	int qId; //Where this part is queued
	Job *job; //Hmmmmm...


};

class QItem: public QObject
{
Q_OBJECT
	// Need to access the variables from the qItem's children
	friend class QMgr;
	friend class DecodeManager;
	friend class NntpThread;
protected:
	enum PartStatus
	{
		Finished_Part = 0,
		Processing_Part = 1,
		Failed_Part = 2,
		Queued_Part = 3,
		Canceled_Part,
		Paused_Part,
		Requeue_Part
	};
	enum ItemStatus
	{
		Finished_Item = 0,
		Processing_Item = 1,
		Failed_Item = 2,
		Queued_Item = 3,
		Canceled_Item,
		Paused_Item
	};
	enum OnError
	{
		Continue, Finished, RequeMe
	};
	int qItemId; //id of this item
	volatile bool processing;
	int type; //type of item
	int status; //status of the whole item
	int partsToDo;
	int failedParts;
	int workingThreads;
	int totalParts;
	quint16 userGroupId;
	quint16 userFileType;
	QMap<int, bool> queuePresence;
	QMap<int, Part*> parts; //partId->part*


	QTreeWidgetItem *listItem;

signals:
	// OR: QMgr->addJob...hmmm...


public:

	virtual void addJobPart(int jobId, int partId, Part* part)=0;
	virtual void update(int partId, int partial, int total, int)=0;
    virtual void update(int , int , int , int, int) {}
	virtual bool finished(int partId);
	virtual int error(int partId, QString &error)=0;
	virtual void comError(int partId)=0;
	virtual void canceled(int partId);
	virtual void paused(int partId);
	virtual void resumed(int partId);
	virtual void requeue(int partId);
	virtual void start(int);
	virtual quint16 getUserGroupId() { return userGroupId; }
	virtual quint16 getUserFileType() { return userFileType; }

    QItem(QTreeWidget *parent, int id, int _type, NewsGroup *_ng);
    QItem(QTreeWidget *parent, int id, int _type, NewsGroup *_ng, QString &hostname);
	QItem(QTreeWidget *parent, int id, int _type, HeaderBase *hb, bool first);
    QItem(QString hostname, QTreeWidget *parent, int id, int _type);
	QItem(QTreeWidget *parent, int id, int _type, QString subj, bool first);
    QItem() {}
    virtual ~QItem();
};

class QUpdItem: public QItem
{
private:
	// 		Db* gdb;

public:
    QUpdItem(QTreeWidget *parent, int id, NewsGroup *ng, QString &hostname);
	virtual ~QUpdItem();
	// 		virtual int fillJob(ThreadMap tm, Job *job);
	virtual void update(int partId, int partial, int total, int);
	virtual void addJobPart(int jobId, int partId, Part* part);
	virtual bool finished(int partId);
	virtual int error(int partId, QString &error);

	virtual void comError(int partId);

	void startExpiring(int partId);
	void stopExpiring(int partId);
	void stopUpdateDb(int partId);
	void downloadFinished(int partId);

};

class QGroupLimitsItem: public QItem
{

public:
    QGroupLimitsItem(QTreeWidget *parent, int id, NewsGroup *ng);
	virtual ~QGroupLimitsItem();

	virtual void update(int partId, int partial, int total, int);


	virtual void addJobPart(int jobId, int partId, Part* part);
	virtual int error(int partId, QString &error);
	virtual void comError(int partId);
};

class QListItem: public QItem
{
private:
	// Db* groupsDb;

public:
    QListItem(QString& hostname, QTreeWidget *parent, int id);
	virtual ~QListItem();
	virtual void addJobPart(int jobId, int partId, Part* part);
	virtual bool finished(int partId);
	virtual int error(int partId, QString &error);
	virtual void update(int, int, int, int);
	virtual void comError(int)
	{
	}

};

class QExtensionsItem: public QItem
{
public:
    QExtensionsItem(QString& hostname, QTreeWidget *parent, int id);
	virtual ~QExtensionsItem();
	virtual void update(int , int , int , int) {}
	virtual void addJobPart(int jobId, int partId, Part* part);
	virtual int error(int partId, QString &error);
	virtual void comError(int partId);
};

class QPostItem: public QItem
{
Q_OBJECT
	friend class QMgr;
private:
	bool view;
	HeaderBase *post; //Do I need it?? Hmmmm...
	PartNumMap serverArticleNos;
	QString rootFName;
	QString savePath;
	uint totalPostLines;
	uint curPostLines;
	uint intervalPostLines;
	uint retryCount;
	uint partialLines;
	QTimer *etaTimer;
	int etaTimeout;
	bool overwriteExisting;
	bool deleteFailed;

	QTime prevTime, curTime;
	// 		int etaCount;

public:

	QPostItem(QTreeWidget *parent, int id, HeaderBase *hb, PartNumMap& _serverArticleNos, QString rfn,
			QString _savePath, bool first, bool _view, quint16 _userGroupId, quint16 _userFileType);
	QPostItem(QTreeWidget *parent, int id, QString subj, QString rfn,
			QString _savePath, bool first, bool _view, quint16 _userGroupId, quint16 _userFileType);
	virtual ~QPostItem();
	virtual void update(int partId, int partial, int total, int);
    virtual void update(int , int , int , int, int);
	virtual void addJobPart(int jobId, int partId, Part* part);
	void addJobPartWithStatus(int jobId, int partId, Part *part, int status);
	virtual bool finished(int partId);
	virtual int error(int partId, QString &error);
	virtual void start(int);
	virtual void canceled(int partId);
	virtual void comError(int partId);

	PartNumMap& getServerArticleNos() { return serverArticleNos; }
	void deletePost() { delete post; }
	bool over() { return overwriteExisting; }
	bool dFailed() { return deleteFailed; }
	void setTotalLines(uint tl) { totalPostLines = tl; }
	void updateDecodingProgress(int prog);
	void startDecoding();
	QString getFName() { return rootFName; }
    void    setFName(QString& fn) { rootFName = fn; }
	QString getSavePath() { return savePath; }
	HeaderBase * getHeaderBase() { return post; }
	void decodeFinished();

signals:
	void decodeMe(QPostItem*);
	void decodeNotNeeded(QPostItem*, bool, QString, QString);
    void addDataRead(quint16 serverId, int bytesRead);

private slots:
	void slotEtaTimeout();

};

#endif //_QUBAN_QUEUEPARTS_H
