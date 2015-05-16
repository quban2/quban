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

#ifndef QMGR_H
#define QMGR_H

#include "queueparts.h"
#include <QEvent>
#include <QMap>
#include <QList>
#include <QMultiMap>
#include <QLinkedList>
#include <QUrl>
#include <QTimer>
#include "availablegroups.h"
#include "unpack.h"
#include "SaveQItems.h"
#include "expireDb.h"
#include "decodeManager.h"
#include "QueueScheduler.h"
#include "rateController.h"
#include "bulkDelete.h"
#include "bulkLoad.h"
#include "bulkHeaderGroup.h"
#include "quban.h"

#define NZB_QUEUED_SUFFIX ".queued"

typedef QMap<uint, Thread*> ThreadMap; //threadid-thread*

typedef QMap<quint16, ThreadMap> QueueThreads; //serverid-threadid-thread*

typedef QList<int> QueueList;
typedef QList<Job*> Queue;

typedef QMap<quint16, GroupManager*> GroupedDownloads;
typedef QMultiMap<quint16, PendingHeader*> PendingHeaders;
typedef QMultiMap<quint16, AutoFile*> AutoFiles;

class GroupList;

class Configuration;
class QNzb;
class QMutexLocker;

class QMgr : public QWidget
{
  Q_OBJECT

	friend class QNzb;

	Configuration* config;
	QTreeWidget *queueList;
    QTreeWidget *doneList;
    QTreeWidget *failedList;
// 	QMap<int, Queue> queues; //server-queue for the server
	QMap<quint16, NntpHost *> *servers;

	QList<int> queueView;
	QMap<int, QueueList> queueItems;

	QMap<int, QItem*> queue;  //Maps qitemId->QItem*
	QMutex queueLock;
	QMutex headerDbLock;

	QTimer *timer;

	uint nextItemId;
	uint nextThreadID;
	//for Queue infos
	unsigned long long queueSize;
	unsigned long long queueCount;

	// Next ID for jobs
	uint nextJobId;
	Db * gdb; //The group db, will change....

	QueueThreads threads;
	bool queuePaused;
	QMap<int, int> pendingOperations;
	int operationsPending;
	bool diskError;

	DecodeManager* decodeManager;
	QThread   decodeManagerThread;

	ExpireDb* expireDb;
	QThread   expireDbThread;

	BulkDelete* bulkDelete;
	QThread   bulkDeleteThread;

	BulkLoad* bulkLoad;
	QThread   bulkLoadThread;

    BulkHeaderGroup* bulkGroup;
    QThread   bulkGroupThread;

	RateController* rateController;

	QList<QString> droppedNzbFiles;

	int findIndex(int, int, bool);

	Job* findNextJob(int qId);

	void processNextDroppedFile();

	void startExpiring(Job *j);
	void expiring(Job *j, int);
	void error(Job *j);
	void upDbErr(Job *j);
	void comError(Job *j);
	int chooseServer(ServerNumMap& snm);
	virtual void customEvent(QEvent *);
	void moveItem(QItem *);
	void moveCanceledItem(QItem *, QMutexLocker*);
	void requeue(Job*);
	void addServerQueue(NntpHost *);
	void downloadError(Job*);
	void itemCanceling(QItem *item);

	QSaveItem* loadSaveItem(char*);

	void writeSaveItem(QSaveItem* si);
	void updateSaveItem(int id, int part, int status, uint lines);
	void delSaveItem(int id);
	void createPostItemFromQ(int id, QSaveItem *qsi, HeaderBase *hb, NewsGroup *ng);

	void cancelGroupItems();

	QueueScheduler* queueScheduler;

	//new queue saving
	SaveQItems* saveQItems;
	QThread     saveQThread;

	Db *qDb;
	Db *pendDb;
	Db *unpackDb;
	Db *autoFileDb;
	DbEnv *dbEnv;
	QString dbDir;
	GroupList *gList;

	bool decoding;

    quint32 getListJobs;

	quint64 bulkSeq;

public:
  QMgr(Servers *_servers, Db *_gdb, QTreeWidget*_queueList, QTreeWidget*_doneList, QTreeWidget*_failedList);
  virtual ~QMgr();

  QueueScheduler* getQueueScheduler() { return queueScheduler; }
  RateController* getRateController() { return rateController; }

  void pauseQ(bool);
  void enable(bool);
  bool empty();
  void setDbEnv(QString d, DbEnv * dbe, GroupList *gl);
  void checkQueue();
  void setDecodeOverwrite(bool o) {emit setOverwrite(o);}
  virtual void closeEvent(QCloseEvent *e);
  void closeAllConnections();
  bool isDecoding() { return decoding; }
  void completeUnpackDetails(UnpackConfirmation*, QString fullNzbFilename);
  quint32 determineRepairType(QString& fileName, quint32* repairPriority);
  quint32 determinePackType(QString& fileName, quint32* packPriority);
  void checkIfGroupDownloadOk(quint16 fileType, GroupManager* thisGroup);
  void startRepair(GroupManager* thisGroup);

  quint64 startBulkDelete(NewsGroup* _ng, HeaderList*, QList<QString>*, QList<QString>*);
  quint64 startBulkLoad(NewsGroup* _ng, HeaderList*);

    void serverDownloadsExceeded(quint16 serverId);

	// Auto unpack data
	quint16             nzbGroupId;
	GroupedDownloads    groupedDownloads;
	PendingHeaders      pendingHeaders;
	AutoFiles           autoFiles;
	UnpackConfirmation* unpackConfirmation;

	quint16             maxGroupId;
	quint32             maxPendingHeader;

	NewsGroup *nzbGroup;

	QNzb* qnzb;

	inline Db*          getUnpackDb() { return unpackDb; }
	void removeAutoGroup(GroupManager*);
	void removeAutoGroupFiles(GroupManager*, bool keepGroupRecord);

  /*$PUBLIC_FUNCTIONS$*/

public slots:
  /*$PUBLIC_SLOTS$*/

    void slotShutdown();

    void adviseAllThreads(qint64, qint64);
	void slotAddUpdItem(NewsGroup *_ng, uint _from, uint _to);
	void slotAddUpdItem(quint16, NewsGroup*, quint64, quint64);
	void slotexpire(NewsGroup *, uint, uint);
	void slotGetGroupLimits(NewsGroup * _ng);
	void slotGetGroupLimits(NewsGroup * _ng, quint16 hostId, NntpHost *nh);
    void slotAddListItems(AvailableGroups *);
    void slotAddListItem(quint16 serverId, AvailableGroups *ag);
	void slotAddPostItem(HeaderBase *hb, NewsGroup *ng, bool first=0, bool view =0, QString dDir=QString::null, quint16 userGroupId=0, quint16 userFileType=0);
// 	void slotThreadSpeedChanged(int, int, int);
// 	void slotThreadDisconnected(int, int);
	void slotEmptyFinishedQ();
	void slotEmptyFailedQ();
	void slotPauseSelected();
	void slotResumeSelected();
	void slotCancelSelected();
	void slotMoveSelectedToTop();
	void slotMoveSelectedToBottom();
	void slotAddServer(NntpHost*);
	void slotAddThread(int, int);
	void slotDeleteThread(int);
	void slotDeleteServerQueue(quint16);
	void slotViewArticle(HeaderBase *, NewsGroup *);
	void slotResumeThread(int, int);
	void slotResumeThreads();
	void slotPauseThread(int, int);
	void slotAdjustQueueWidth();
	void slotUnpackConfirmed(QString);
	void slotUnpackCancelled();
	void slotNzbFileDropped(QString);

    void validateServer(quint16 serverId);
	// For QSaveItem handling
	void delNzbItem(QString index);
	void writeError(Job *j=0);

	void expireFinished(Job*);
	void expireFailed(Job*);

	void bulkDeleteFinished(quint64);
	void bulkDeleteFailed(quint64);

	void bulkLoadFinished(quint64);
	void bulkLoadFailed(quint64);

    void bulkGroupFinished(quint64);
    void bulkGroupFailed(quint64);

	void addBulkJob(quint16, NewsGroup*, HeaderList*, QList<QString>*, QList<QString>*);

    void startBulkGrouping(NewsGroup* _ng);

    void addDataRead(quint16 serverId, int bytesRead);

	void decodeStarted(QPostItem*);
	void decodeProgress(QPostItem*, int);
	void decodeItemCanceled(QPostItem* item);
	void decodeFinished(QPostItem*, bool, QString, QString);

	void started(int serverID, int threadID);
	void start(Job *j);
	void finished (Job *j);
    void downloadCancelled(Job*);
	void downloadError(Job*, int);
	void Err(Job*, int);
	void Failed(Job*, int);
	void cancel(Job *j);
	void paused(int qId, int threadId, bool error);
	void connClosed(int qId, int threadId);
	void delayedDelete(int qId, int threadId);
	void stopped(int qId, int threadId);
	void update(Job *j, uint partial, uint total, uint interval);
    void updatePost(Job *j, uint partial, uint total, uint interval, uint serverId);
	void updateLimits (Job *j, uint, uint, uint);
	void updateExtensions(Job *, quint16, quint64);
	void slotHeaderDownloadProgress(Job*, quint64, quint64, quint64);

	void slotServerValidated(quint16, bool);

	void slotCancelExternal(quint64);
	void slotCancelBulkDelete(quint64);
	void slotCancelBulkLoad(quint64);
    void slotCancelBulkGroup(quint64);

    void serverValidated(uint, bool, QString, QList<QSslError>);

protected slots:
  /*$PROTECTED_SLOTS$*/

	void slotThreadFinished();
    void slotProcessJobs();
	void addDecodeItem(QPostItem*);
	void slotSelectionChanged();
	void slotItemMoved(QTreeWidgetItem*, QTreeWidgetItem*, QTreeWidgetItem*);
	void slotItemToBottom(QTreeWidgetItem *item);
	void startAllThreads(int qId);
	void slotQueueWidthChanged();
	void slotUpdateServerExtensions(quint16);
	void checkServers();

signals:

    void sigUpdateServerExtensions(quint16);
    void sigProcessJobs();
    void newItem(int);  //new item inserted in queue "int"
	void threadFinished();
	void sigSpeedChanged(int,int,int);
	void sigThreadStart(quint16,int);
	void sigThreadFinished(quint16,int);
	void sigThreadCanceling(quint16,int);
	void sigThreadError(quint16,int);
	void sigThreadPaused(quint16, int, int);
	void sigThreadConnClosed(quint16, int);
	void sigThreadDisconnected(quint16, int);
	void sigUpdateFinished(NewsGroup *);
	void sigLimitsFinished(NewsGroup *);
	void sigLimitsUpdate(NewsGroup * ng, NntpHost * nh, uint low, uint high, uint parts);
	void sigExtensionsUpdate(NntpHost * nh, quint64 extensions);
	void sigItemsSelected(bool);
	void sigThreadDeleted(int, int);
	void viewItem(QString&);
	void queueInfo(unsigned long long, unsigned long long);
	void sigQPaused(bool);
	void groupListDownloaded();
	void sigExtProcStarted(quint16, enum UnpackFileType, QString&);
	void sigStatusMessage(QString);

    void sigServerValidated(quint16);

	void addSaveItem(QSaveItem*);
	void quitSaveQ();

	void expireNg(Job*);
	void addBulkDelete(quint16, quint64, NewsGroup*, HeaderList*, QList<QString>*, QList<QString>*);
	void addBulkLoad(quint64, NewsGroup*, HeaderList*);
    void addBulkGroup(quint64, NewsGroup*);
	void cancelExpire(int);
	void cancelBulkDelete(quint64);
	void cancelBulkLoad(quint64);
    void cancelBulkGroup(quint64);
	void quitExpireDb();
	void quitBulkDelete();
	void quitBulkLoad();
    void quitBulkGroup();

	void decodeItem(QPostItem*);
	void setOverwrite(bool);
	void cancelDecode(int);
	void quitDecode();

	void sigHeaderDownloadProgress(NewsGroup *, NntpHost *, quint64, quint64, quint64);

	void registerSocket(RcSslSocket*);
	void unregisterSocket(RcSslSocket*);

    void sigShutdown();
};

#endif
