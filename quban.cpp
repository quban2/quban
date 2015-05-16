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

#include <QMenu>
#include <QDebug>
#include <QtDebug>
#include <QMessageBox>
#include <QDateTime>
#include <QProgressDialog>
#include <QCursor>
#include <QAction>
#include <QSize>
#include <QtAlgorithms>
#include <QCryptographicHash>
#include <QMap>
#include <QStringBuilder>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "appConfig.h"
#include "about.h"
#include "newsgroup.h"
#include "MultiPartHeader.h"
#include "fileviewer.h"
#include "availablegroups.h"
#include "AvailableGroupsView.h"
#include "groupprogress.h"
#include "qnzb.h"
#include "qubanwizard.h"
#include "queueschedulerdialog.h"
#include "ratecontroldialog.h"
#include "unpack.h"
#include "qubanDbEnv.h"
#include "JobList.h"
#include "zlib.h"
#include "quban.h"

extern DbEnv *g_dbenv;
extern QIcon* completeIcon;
extern QIcon* incompleteIcon;
extern QIcon* articleNewIcon;
extern QIcon* articleReadIcon;
extern QIcon* articleunreadIcon;
extern QIcon* binaryIcon;
extern QIcon* tickIcon;
extern QIcon* crossIcon;

extern int compare_q16(Db *, const Dbt *, const Dbt *);

Quban::Quban(QWidget *parent) : QMainWindow(parent)
{
	aGroups = 0;
	agroupDb = 0;
    aGroupsView = 0;
	groupDb=0;
	qTWidget = 0;
	rateControlInfo = 0;
	qMgr = 0;
	servers = 0;
	serversList = 0;
	tabWidget_2 = 0;
	schedDb = 0;
	queueScheduler = 0;

	startupScheduleRequired = false;
	isNilPeriod = false;
    queuePaused = false;

	settings = new QSettings("Quban", "Quban");
}

Quban::~Quban()
{
	qDebug() << "Quban::~Quban()";

	if (agroupDb)
	{
		agroupDb->close(0);
		delete agroupDb;
	}

	qDeleteAll(sWidgets);
	sWidgets.clear();

	if (qTWidget)
	    delete qTWidget;

	if (rateControlInfo)
		delete rateControlInfo;

	if (qMgr)
	    delete qMgr;

	if (servers)
	{
		qDeleteAll(*servers);
		servers->clear();

		delete servers;
	}

	if (groupDb)
	{
		groupDb->close(0);
		delete groupDb;
	}

	if (serversList)
	{
		Db* serverDb = serversList->getDb();
		if (serverDb)
		{
			serverDb->close(0);
			delete serverDb;
		}
	}

	if (tabWidget_2)
	{
		// Count and delete the tabs ...
		for (int i= tabWidget_2->count() - 1; i>0; --i)
		{
			mainTabIndex = i;

			if ( tabWidget_2->tabText(mainTabIndex) == tr("Available Newsgroups"))
			{
				AvailableGroups* view = (AvailableGroups *)tabWidget_2->widget(mainTabIndex);
				view->prepareToClose();
				tabWidget_2->removeTab(mainTabIndex);
			}
			else if (QString("HeaderList") == tabWidget_2->widget(mainTabIndex)->metaObject()->className())
			{
				HeaderList* view = (HeaderList *)tabWidget_2->widget(mainTabIndex);
				view->prepareToClose(false);
				tabWidget_2->removeTab(mainTabIndex);
				delete view;
			}
			else
			{
				FileViewer* view = (FileViewer *)tabWidget_2->widget(mainTabIndex);
				tabWidget_2->removeTab(mainTabIndex);
				delete view;
			}
		}
	}

	if (completeIcon)
	{
		delete completeIcon;
		delete incompleteIcon;
		delete articleNewIcon;
		delete articleReadIcon;
		delete articleunreadIcon;
		delete binaryIcon;
		delete tickIcon;
		delete crossIcon;
	}

	if (aGroups)
	    delete aGroups;

	if (schedDb)
	{
		schedDb->close(0);
		delete schedDb;
	}

	if (queueScheduler)
	    delete queueScheduler;

	delete settings;
}

void Quban::startup()
{
	config = Configuration::getConfig();

	qubanDbEnv = new QubanDbEnv(config);

	setupUi(this);

	actionDelete->setShortcut(QKeySequence::Delete);

    logAlertList = new LogAlertList(this);
    logEventList = new LogEventList(this);
    jobList = new JobList(this);

	if (!qubanDbEnv->dbsExists())
	{
		if (!config->configured) // First time ..
		{
			qubanDbEnv->createDbs();
		}
		else
		{
			QString dbQuestion = tr("Can't find all the files of your database in ") + config->dbDir +
					tr(". Do you want to recreate missing files? If not, the application will exit.");
			int result = QMessageBox::question(0, tr("Database invalid"), dbQuestion, QMessageBox::Yes, QMessageBox::No);

			switch (result)
			{
			  case QMessageBox::Yes:
				  qubanDbEnv->migrateDbs();
				break;
			  case QMessageBox::No:
				exit(-1);
				break;
			}
		}
	 }
	 else
		 qubanDbEnv->migrateDbs();

	//Sub tool bars
	// queue
    toolBarTab_4 = new QToolBar(tab_3);
    toolBarTab_4->setObjectName(QString::fromUtf8("toolBarTab_4"));
    toolBarTab_4->setMovable(false);
    toolBarTab_4->addAction(actionPause_queue);
    toolBarTab_4->addAction(actionMove_to_top);
    toolBarTab_4->addAction(actionMove_to_bottom);
    toolBarTab_4->addSeparator();
    toolBarTab_4->addAction(actionCancel);
    toolBarTab_4->addSeparator();
    toolBarTab_4->resize(200, 36);
    slotQueue_Toolbar(config->showQueueBar);
    actionQueue_Toolbar->setChecked(config->showQueueBar);

    // groups
    toolBarTab_1 = new QToolBar(tab_8);
    toolBarTab_1->setObjectName(QString::fromUtf8("toolBarTab_1"));
    toolBarTab_1->setMovable(false);
    toolBarTab_1->addAction(actionUpdate_selected_newsgroups);
    toolBarTab_1->addAction(actionUpdate_subscribed_newsgroups);
    toolBarTab_1->addSeparator();
    toolBarTab_1->addAction(actionGroup_properties);
    toolBarTab_1->addAction(actionNewsgroup_Management);
    toolBarTab_1->addSeparator();
    toolBarTab_1->addAction(actionUnsubscribeToNewsgroup);
    toolBarTab_1->addSeparator();
    toolBarTab_1->resize(250, 36);
    slotNewsgroup_Toolbar(config->showGroupBar);
    actionNewsgroup_Toolbar->setChecked(config->showGroupBar);

    // servers
    toolBarTab_2 = new QToolBar(tab_7);
    toolBarTab_2->setObjectName(QString::fromUtf8("toolBarTab_2"));
    toolBarTab_2->setMovable(false);
    toolBarTab_2->addAction(actionNew_Server);
    toolBarTab_2->addSeparator();
    toolBarTab_2->addAction(actionServer_properties);
    toolBarTab_2->addAction(actionTest_server_connection);
    toolBarTab_2->addSeparator();
    toolBarTab_2->addAction(actionDelete_Server);
    toolBarTab_2->addSeparator();
    toolBarTab_2->resize(200, 36);
    slotServer_Toolbar(config->showServerBar);
    actionServer_Toolbar->setChecked(config->showServerBar);

    // jobs
    toolBarTab_3 = new QToolBar(tab_9);
    toolBarTab_3->setObjectName(QString::fromUtf8("toolBarTab_3"));
    toolBarTab_3->setMovable(false);
    // Unable to implement at the moment, if ever ...
    //toolBarTab_3->addAction(actionPause_Job);
    //toolBarTab_3->addAction(actionResume_Job);
    //toolBarTab_3->addSeparator();
    toolBarTab_3->addAction(actionCancel_Job);
    toolBarTab_3->addAction(actionDelete_Job);
    toolBarTab_3->addSeparator();
    toolBarTab_3->resize(200, 36);
    slotJob_Toolbar(config->showJobBar);
    actionJob_Toolbar->setChecked(config->showJobBar);

    actionHeaders_Toolbar->setChecked(config->showHeaderBar);

	mustShutDown = false;
	shutDownCount = 0;

	queueScheduler = new QueueScheduler();

	connect(queueScheduler, SIGNAL(sigSpeedChanged(qint64)), this, SLOT(slotRateSpeedChanged(qint64)));

	int ret;
	if (!(schedDb = getDbEnv()->openDb("scheduler", true, &ret)))
		QMessageBox::warning(0, tr("Error"), tr("Unable to open scheduler database ..."));

	queueScheduler->loadData(schedDb, startupScheduleRequired);




	if (1 == 2)
		echoInterface->echo("Plugin test");

	//logAlertList->logMessage(LogAlertList::Alert, tr("Alert !!!"));
	//logAlertList->logMessage(LogAlertList::Warning, tr("Warning !!!"));
	//logAlertList->logMessage(LogAlertList::Error, tr("Error !!!"));
	//quban->getLogEventList()->logEvent(tr("Queue check started"));

	servers = new Servers;

	serversList->setServers(servers);
	serversList->setDbEnv(g_dbenv);
	serversList->serversListInit();

	subGroupList->setDbPath(config->dbDir);
    subGroupList->loadGroups(servers, g_dbenv);
    groupDb=subGroupList->getGroupDb();

	qMgr=new QMgr(servers, groupDb, queueList, doneList, failedList);

	 QMapIterator<quint16, NntpHost*> i(*servers);
	 while (i.hasNext())
	 {
	     i.next();
	     connect(i.value(), SIGNAL(sigServerValidated(quint16, bool)), qMgr, SLOT(slotServerValidated(quint16, bool)));
	 }

	qMgr->setDbEnv(config->dbDir, g_dbenv, subGroupList);
	qMgr->setDecodeOverwrite(config->overwriteExisting);

	subGroupList->setQ(qMgr);

    connect(serversList, SIGNAL(sigUpdateServerExtensions(quint16)), qMgr, SLOT(slotUpdateServerExtensions(quint16)));
    connect(serversList, SIGNAL(startAllThreads(int)), qMgr, SLOT(startAllThreads(int)));
	connect(qMgr, SIGNAL(sigExtensionsUpdate(NntpHost *, quint64)), serversList, SLOT(slotExtensionsUpdate(NntpHost *, quint64)));

	queueList->setContextMenuPolicy(Qt::ActionsContextMenu);
	queueMenu = new QMenu(queueList);
	serversList->setContextMenuPolicy(Qt::CustomContextMenu);
	serverMenu = new QMenu(serversList);
	jobsTreeWidget->setContextMenuPolicy(Qt::ActionsContextMenu);
	jobsMenu = new QMenu(jobsTreeWidget);
	threadMenu = new QMenu(serversList);
	availTabMenu = new QMenu(this);
	headerTabMenu = new QMenu(this);
	viewerTabMenu = new QMenu(this);
	connect(serversList, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(serverContextMenu(QPoint)));
	subGroupList->setContextMenuPolicy(Qt::ActionsContextMenu);
	subGroupMenu = new QMenu(subGroupList);

	agroupDb = new Db(g_dbenv, 0);
	aGroups = new AvailableGroups(g_dbenv, agroupDb, subGroupList->getSubscribedGroups(), servers, this);

	setupActions();

	connect(serversList, SIGNAL(serverSelected(bool)), SLOT(slotServerSelection(bool)));
	connect(serversList, SIGNAL(sigHaveServers(bool)), SLOT(slotHaveServers(bool)));
    connect(serversList, SIGNAL(validateServer(quint16)), qMgr, SLOT(validateServer(quint16)));
    connect(qMgr, SIGNAL(sigServerValidated(quint16)), serversList, SLOT(slotValidServer(quint16)));
    connect(subGroupList, SIGNAL(openNewsGroup(NewsGroup* )), SLOT(slotOpenNewsGroup(NewsGroup* )));

 	connect(subGroupList, SIGNAL(isSelected(bool )), SLOT(slotGroupSelection(bool )));
 	connect(subGroupList, SIGNAL(deleteDeletedServer(quint16)), serversList, SLOT(slotDeleteDeletedServer(quint16)));
// TODO	connect(subGroupList, SIGNAL(activateNewsGroup(HeaderList*)), this, SLOT(slotActivateNewsGroup(HeaderList* )));

    connect(aGroups, SIGNAL(subscribe(AvailableGroup*)), subGroupList, SLOT(slotAddGroup(AvailableGroup*)));
	connect(subGroupList, SIGNAL(unsubscribe(QString)), aGroups, SLOT(slotUnsubscribe(QString)));
    connect(subGroupList, SIGNAL(sigGroupHeaders(NewsGroup*)), qMgr, SLOT(startBulkGrouping(NewsGroup*)));
    connect(subGroupList, SIGNAL(sigCancelBulkDelete(quint64)), qMgr, SLOT(slotCancelBulkDelete(quint64)));

    connect(this, SIGNAL(getGroups(AvailableGroups*)), qMgr, SLOT(slotAddListItems(AvailableGroups*)));

	connect(jobList, SIGNAL(cancelExpire(int)), qMgr, SIGNAL(cancelExpire(int)));
	connect(jobList, SIGNAL(cancelExternal(quint64)), qMgr, SLOT(slotCancelExternal(quint64)));
	connect(jobList, SIGNAL(cancelBulkDelete(quint64)), qMgr, SLOT(slotCancelBulkDelete(quint64)));
	connect(jobList, SIGNAL(cancelBulkLoad(quint64)), qMgr, SLOT(slotCancelBulkLoad(quint64)));

	connect(qubanDbEnv, SIGNAL(sigCompact()), subGroupList, SLOT(slotCompactDbs()));

	connect(qMgr, SIGNAL(sigSpeedChanged(int, int, int )), serversList, SLOT(slotSpeedChanged(int, int, int )));
	connect(qMgr, SIGNAL(sigThreadDisconnected(quint16, int )), serversList, SLOT(slotThreadDisconnect(quint16, int )));
	connect(qMgr, SIGNAL(sigThreadCanceling(quint16, int )), serversList, SLOT(slotCanceling(quint16, int )));
	connect(qMgr, SIGNAL(sigThreadError(quint16, int )), serversList, SLOT(slotThreadStop(quint16, int )));
	connect(qMgr, SIGNAL(sigThreadFinished(quint16, int )), serversList, SLOT(slotThreadStop(quint16, int )));
	connect(qMgr, SIGNAL(sigThreadStart(quint16, int )), serversList, SLOT(slotThreadStart(quint16, int )));

    connect(subGroupList, SIGNAL(subscribed(AvailableGroup* )), SLOT(slotSubscribed()));
	connect(subGroupList, SIGNAL(updateNewsGroup(quint16, NewsGroup*, quint64, quint64)), qMgr, SLOT(slotAddUpdItem(quint16, NewsGroup*, quint64, quint64)));
	connect(subGroupList, SIGNAL(updateNewsGroup(NewsGroup*,uint, uint)), qMgr, SLOT(slotAddUpdItem(NewsGroup*, uint, uint)));
	connect(subGroupList, SIGNAL(expireNewsGroup(NewsGroup*,uint, uint)), qMgr, SLOT(slotexpire(NewsGroup*, uint, uint)));

	connect(subGroupList, SIGNAL(sigUnsubscribe(QString)), this, SLOT(slotUnsubscribe(QString)));

	connect(qMgr, SIGNAL(sigUpdateFinished(NewsGroup* )), subGroupList, SLOT(slotUpdateFinished(NewsGroup*)));
	connect(qMgr, SIGNAL(sigLimitsUpdate(NewsGroup *, NntpHost *, uint, uint, uint)), subGroupList,
			      SLOT(slotLimitsFinished(NewsGroup *, NntpHost *, uint, uint, uint)));

	connect(qMgr, SIGNAL(groupListDownloaded()), this, SLOT(slotShowGroups()));
	connect(qMgr, SIGNAL(groupListDownloaded()), this, SLOT(slotUpdateSubscribed()));
	connect(qMgr, SIGNAL(sigExtProcStarted(quint16, enum UnpackFileType, QString&)), SLOT(slotExtProcStarted(quint16, enum UnpackFileType, QString&)));
	connect(qMgr, SIGNAL(sigStatusMessage(QString)), this, SLOT(slotStatusMessge(QString)));
	connect(this, SIGNAL(sigShutdown()), qMgr, SIGNAL(quitExpireDb()));
    connect(this, SIGNAL(sigShutdown()), qMgr, SLOT(slotShutdown()));

	connect(serversList, SIGNAL(sigServerAdded(NntpHost* )), qMgr, SLOT(slotAddServer(NntpHost* )));
	connect(serversList, SIGNAL(sigAddThread(int, int )), qMgr, SLOT(slotAddThread(int, int )));
	connect(serversList, SIGNAL(sigDeleteThread(int )), qMgr, SLOT(slotDeleteThread(int )));

	connect(qMgr, SIGNAL(sigThreadDeleted(int, int )), serversList, SLOT(slotThreadDeleted(int, int )));

	connect(serversList, SIGNAL(deleteServer(quint16)), subGroupList, SLOT(slotDeleteAllArticles(quint16)));
	connect(serversList, SIGNAL(deleteServer(quint16)), this, SLOT(slotDeleteServerWidget(quint16)));

	connect(serversList, SIGNAL(deleteServer(quint16)), qMgr, SLOT(slotDeleteServerQueue(quint16 )));
	connect(serversList, SIGNAL(deleteServer(quint16)), this, SIGNAL(deleteServer(quint16)));

	connect(qMgr, SIGNAL(sigThreadPaused(quint16, int, int )), serversList, SLOT(slotThreadPaused(quint16, int, int )));
	connect(qMgr, SIGNAL(sigThreadConnClosed(quint16, int)), serversList, SLOT(slotThreadDisconnect(quint16, int)));
	connect(serversList, SIGNAL(sigPauseThread(int, int )), qMgr, SLOT(slotPauseThread(int, int )));
	connect(serversList, SIGNAL(sigResumeThread(int, int )), qMgr, SLOT(slotResumeThread(int, int )));
    connect(serversList, SIGNAL(sigServerTypeChanged(int, int, int, bool, QString&)), SLOT(slotServTypeChanged(int, int, int, bool, QString&)));
	connect(serversList, SIGNAL(sigServerInvalid(quint16)), SLOT(slotServerInvalid(quint16)));

	connect(subGroupList, SIGNAL(updateNewsGroup(NewsGroup*,uint, uint)), this, SLOT(slotUpdateNewsGroup(NewsGroup*)));
	connect(subGroupList, SIGNAL(sigNewGroup(NewsGroup*)), qMgr, SLOT(slotGetGroupLimits(NewsGroup*)));

	connect(qMgr, SIGNAL(viewItem(QString&)), this, SLOT(slotOpenFileViewer(QString&)));

	// dropped files
	connect(queueList, SIGNAL(fileDropped(QString)), qMgr, SLOT(slotNzbFileDropped(QString)));

	//servers widget
	Servers::iterator it;
	for (it = servers->begin() ; it != servers->end(); ++it)
	{
		if (it.value()->getServerType() != NntpHost::dormantServer)
		{
            sWidgets[it.key()] = new QStatusServerWidget(it.value()->getName(), it.value()->getServerType(), it.value()->isPaused(), this);
			statusBar()->addPermanentWidget(sWidgets[it.key()], 0);
		}
	}

	//Statusbar widget
	qTWidget= new QToolbarWidget(this);
	statusBar()->addPermanentWidget(qTWidget, 0);
	connect(qMgr, SIGNAL(queueInfo(unsigned long long, unsigned long long)), qTWidget, SLOT(slotQueueInfo(unsigned long long,  unsigned long long)));
	connect(serversList, SIGNAL(sigServerSpeed(int, int )), SLOT(slotUpdateSpeed(int, int )));

	connect(serversList, SIGNAL(sigServerAdded(NntpHost*)), SLOT(slotAddServerWidget(NntpHost*)));

	/*
	//Initial kwallet implementation
	//Easy to use, but is it supported on all versions of kde? Don't think so...


	QStringList wallets=KWallet::Wallet::walletList();
	for (QStringList::iterator it = wallets.begin(); it != wallets.end(); ++it)
		qDebug("Wallet name: %s", (const char *) *it);
	KWallet::Wallet* wallet=KWallet::Wallet::openWallet("kdewallet");
	QStringList folders=wallet->folderList();
	for (QStringList::iterator it = folders.begin(); it != folders.end(); ++it)
		qDebug("Folders: %s", (const char *) *it);
	wallet->createFolder("klibido");
	wallet->setFolder("klibido");
	wallet->writePassword("bauno", "bauno");
	*/

    tabWidget->setCurrentIndex(config->mainTab);
    jobsAndLogsTabs->setCurrentIndex(config->jobTab);

	subGroupList->checkForDeletedServers();



	qMgr->checkQueue();

	setAttribute(Qt::WA_DeleteOnClose);

	// No servers defined!!
	if (!serversList->topLevelItemCount())
	{
		actionDownload_nzb->setEnabled(false);
		actionGet_list_of_groups->setEnabled(false);
		actionDelete_Server->setEnabled(false);
		actionServer_properties->setEnabled(false);
		actionTest_server_connection->setEnabled(false);
		actionAddThread->setEnabled(false);
		actionRemove_thread->setEnabled(false);
		actionShow_available_newsgroups->setEnabled(false);
		menuQueue->setEnabled(false);
		actionPause_queue->setEnabled(false);
		actionMove_to_top->setEnabled(false);
		actionMove_to_bottom->setEnabled(false);
		actionCancel->setEnabled(false);
		actionSchedulerEdit->setEnabled(false);
		actionTraffic_Management->setEnabled(false);
	}

	// No subscribed groups defined!!
	if (!subGroupList->topLevelItemCount())
	{
		actionView_group_articles->setEnabled(false);
		actionUnsubscribeToNewsgroup->setEnabled(false);
		actionUpdate_current->setEnabled(false);
		actionUpdate_subscribed_newsgroups->setEnabled(false);
		actionUpdate_selected_newsgroups->setEnabled(false);
		actionReset_selected->setEnabled(false);
		actionGroup_properties->setEnabled(false);
		actionNewsgroup_Management->setEnabled(false);
	}

	// not available at this point ...
	actionSubscribe_to_newsgroup->setEnabled(false);
	menuArticles->setEnabled(false);
	actionDownload_selected->setEnabled(false);
	actionView_article->setEnabled(false);
	actionShow_only_complete_articles->setEnabled(false);
	actionShow_only_unread_articles->setEnabled(false);

	if (config->autoUnpack == false)
	{
		actionGroup_and_download_selected->setEnabled(false);
		actionDownload_nzb_files_and_auto_unpack->setEnabled(false);
		actionGroup_Info->setEnabled(false);
	}

	subGroupList->checkExpiry();
    subGroupList->checkGrouping();
}

void Quban::showWizard(bool atStartup)
{
    QubanWizard *wiz=new QubanWizard(this);

	connect(wiz, SIGNAL(newServer()), serversList, SLOT(slotNewServer()));
	connect(wiz, SIGNAL(sigGetGroups()), this, SLOT(slotGetGroups()));

	if (atStartup)
		wiz->wizardTabs->setCurrentIndex(0);

    wiz->show();
}

void Quban::setupActions( )
{
    // **********
    // Main menus
    // **********

	// File menu
	connect(actionDownload_nzb, SIGNAL(triggered()), qMgr->qnzb, SLOT(slotOpenNzb()));
	connect(actionDownload_nzb_files_and_auto_unpack, SIGNAL(triggered()), qMgr->qnzb, SLOT(slotGroupFullNzb()));
    connect(actionCompact_databases, SIGNAL(triggered()), qubanDbEnv, SLOT(slotCompactDbs()));
    connect(actionQuit, SIGNAL(triggered()), SLOT(quit_slot()));

    // View menu
    connect(actionQueue_Toolbar, SIGNAL(triggered(bool)), SLOT(slotQueue_Toolbar(bool)));
    connect(actionServer_Toolbar, SIGNAL(triggered(bool)), SLOT(slotServer_Toolbar(bool)));
    connect(actionNewsgroup_Toolbar, SIGNAL(triggered(bool)), SLOT(slotNewsgroup_Toolbar(bool)));
    connect(actionHeaders_Toolbar, SIGNAL(triggered(bool)), SLOT(slotHeaders_Toolbar(bool)));
    connect(actionJob_Toolbar, SIGNAL(triggered(bool)), SLOT(slotJob_Toolbar(bool)));

    // Jobs menu
    //connect(actionPause_Job, SIGNAL(triggered()),  jobList, SLOT(slotPauseJob()));
    //connect(actionResume_Job, SIGNAL(triggered()), jobList, SLOT(slotResumeJob()));
    connect(actionCancel_Job, SIGNAL(triggered()), jobList, SLOT(slotCancelJob()));
    connect(actionDelete_Job, SIGNAL(triggered()), jobList, SLOT(slotDeleteJob()));

    // Scheduler menu
	connect(actionSchedulerEdit, SIGNAL(triggered()), SLOT(slotDisplayScheduler()));
	connect(actionTraffic_Management, SIGNAL(triggered()), SLOT(slotTraffic()));

    // Server menu
    connect(actionGet_list_of_groups, SIGNAL(triggered()), SLOT(slotGetGroups()));
    connect(actionNew_Server, SIGNAL(triggered()), serversList, SLOT(slotNewServer()));
    connect(actionDelete_Server, SIGNAL(triggered()), SLOT(slotDeleteServerMessage()));
    connect(actionDelete_Server, SIGNAL(triggered()), serversList, SLOT(slotDeleteServer()));
	connect(actionServer_properties, SIGNAL(triggered()), serversList, SLOT(slotServerProperties()));
	connect(actionTest_server_connection, SIGNAL(triggered()), serversList, SLOT(slotServerTest()));
    connect(actionValidate_Server, SIGNAL(triggered()), serversList, SLOT(slotValidateServer()));
	connect(actionAddThread, SIGNAL(triggered()), serversList, SLOT(slotAddThread()));
	connect(actionRemove_thread, SIGNAL(triggered()), serversList, SLOT(slotDeleteThread()));
    connect(actionPause_Server, SIGNAL(triggered()), serversList, SLOT(slotPauseServer()));
    connect(actionResume_Server, SIGNAL(triggered()), serversList, SLOT(slotResumeServer()));

    // Newsgroup menu
	connect(actionView_group_articles, SIGNAL(triggered()), subGroupList, SLOT(slotExecuted()));
	connect(actionShow_available_newsgroups, SIGNAL(triggered()), SLOT(slotShowGroups()));
	connect(actionUnsubscribeToNewsgroup, SIGNAL(triggered()), subGroupList, SLOT(slotDeleteSelected()));
	connect(actionUpdate_current, SIGNAL(triggered()), SLOT(slotUpdateCurrent()));
	connect(actionUpdate_subscribed_newsgroups, SIGNAL(triggered()), subGroupList, SLOT(slotUpdateSubscribed()));
	connect(actionUpdate_selected_newsgroups, SIGNAL(triggered()), subGroupList, SLOT(slotUpdateSelected()));
	connect(actionReset_selected, SIGNAL(triggered()), subGroupList, SLOT(slotZeroSelected()));
	connect(actionGroup_properties, SIGNAL(triggered()), subGroupList, SLOT(slotGroupProperties()));
	connect(actionNewsgroup_Management, SIGNAL(triggered()), subGroupList, SLOT(slotGroupManagement()));

    // Articles menu
	// Done dynamically in Quban::slotOpenNewsGroup, but this is for all tabs !!
    connect(tabWidget_2->getTab(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(headerContextMenu(QPoint)));

    // Queue menu
	connect(actionPause_queue, SIGNAL(triggered()), SLOT(slotPauseQ()));
	connect(actionEmpty_finished_list, SIGNAL(triggered()), qMgr, SLOT(slotEmptyFinishedQ()));
	connect(actionEmpty_failed_list, SIGNAL(triggered()), qMgr, SLOT(slotEmptyFailedQ()));

	// Group menu
	connect(actionGroup_Info, SIGNAL(triggered()), SLOT(slotGroupInfo()));

    // Settings menu
    connect(actionWizard, SIGNAL(triggered()), SLOT(showWizard()));

    // Help menu    
    connect(actionDiagnostics, SIGNAL(triggered()), SLOT(diagnostics_slot()));
    connect(actionAbout, SIGNAL(triggered()), SLOT(about_slot()));
    connect(actionAbout_Qt, SIGNAL(triggered()), SLOT(aboutQt_slot()));

    // *************
    // Context menus
    // *************

	//Server context menu

	// This is the part that sets the menu order...
    serverMenu->addAction(actionServer_properties);
	serverMenu->addAction(actionTest_server_connection);
    serverMenu->addAction(actionValidate_Server);
	serverMenu->addAction(serverMenu->addSeparator());
	serverMenu->addAction(actionAddThread);     // Works but it's not saved unless you go to properties and ok it!!!
	serverMenu->addAction(actionRemove_thread);  // Doesn't work at all ...
	serverMenu->addAction(serverMenu->addSeparator());
	serverMenu->addAction(actionDelete_Server);

	threadMenu->addAction(sPauseThreadAction);
	threadMenu->addAction(sResumeThreadAction);
	threadMenu->addAction(threadMenu->addSeparator());
	threadMenu->addAction(actionServer_properties);

	connect(sPauseThreadAction, SIGNAL(triggered()), serversList, SLOT(slotPauseThread()));
	connect(sResumeThreadAction, SIGNAL(triggered()), serversList, SLOT(slotResumeThread()));

	//Newsgroup context menu

	// This is the part that sets the menu order...
	subGroupList->addAction(actionView_group_articles);
	subGroupList->addAction(subGroupMenu->addSeparator());
	subGroupList->addAction(actionUpdate_selected_newsgroups);
	subGroupList->addAction(actionNewsgroup_Management);
	subGroupList->addAction(actionGroup_properties);
	subGroupList->addAction(subGroupMenu->addSeparator());
	subGroupList->addAction(actionUnsubscribeToNewsgroup);
	subGroupList->addAction(actionReset_selected);
	subGroupList->addAction(subGroupMenu->addSeparator());

	//Job context menu

	// This is the part that sets the menu order...
    jobsMenu->addAction(actionCancel_Job);
	jobsMenu->addAction(actionDelete_Job);


/* TODO
	(void) new KAction(tr("Remove category"), 0, subGroupList, SLOT(slotRemoveCategory()), actionCollection(), "remove_category");
MD */

	//Q context menu

	// This is the part that sets the menu order...
	queueList->addAction(actionMove_to_top);
	queueList->addAction(actionMove_to_bottom);
	queueList->addAction(queueMenu->addSeparator());
	queueList->addAction(actionPause);
	queueList->addAction(actionResume);
	queueList->addAction(actionCancel);

	connect(actionPause, SIGNAL(triggered()), qMgr, SLOT(slotPauseSelected()));
	connect(actionResume, SIGNAL(triggered()), qMgr, SLOT(slotResumeSelected()));
	connect(actionCancel, SIGNAL(triggered()), qMgr, SLOT(slotCancelSelected()));
	connect(actionMove_to_top, SIGNAL(triggered()), qMgr, SLOT(slotMoveSelectedToTop()));
	connect(actionMove_to_bottom, SIGNAL(triggered()), qMgr, SLOT(slotMoveSelectedToBottom()));

    // Tab menus

	headerTabMenu->addAction(headerCloseAction);
	headerTabMenu->addAction(headerCloseMarkAction);
	headerTabMenu->addAction(headerCloseNoMarkAction);

	connect(headerCloseAction, SIGNAL(triggered()), SLOT(slotCloseHead()));
	connect(headerCloseMarkAction, SIGNAL(triggered()), SLOT(slotCloseHeadAndMark()));
	connect(headerCloseNoMarkAction, SIGNAL(triggered()), SLOT(slotCloseHeadAndNoMark()));

	availTabMenu->addAction(availCloseAction);

	connect(availCloseAction, SIGNAL(triggered()), SLOT(slotCloseAvail()));

	viewerTabMenu->addAction(viewerCloseAction);
	viewerTabMenu->addAction(viewerCloseDeleteAction);
	viewerTabMenu->addAction(viewerCloseNoDeleteAction);

	connect(viewerCloseAction, SIGNAL(triggered()), SLOT(slotCloseView()));
	connect(viewerCloseDeleteAction, SIGNAL(triggered()), SLOT(closeAndDelete()));
	connect(viewerCloseNoDeleteAction, SIGNAL(triggered()), SLOT(closeNoDelete()));

    readSettings();
}

void Quban::readSettings()
{
    settings->beginGroup("MainWindow");
    restoreGeometry(settings->value("geometry").toByteArray());

    splitter->restoreState(
            settings->value("horizSplitter").toByteArray());
    splitter_2->restoreState(
            settings->value("vertSplitter").toByteArray());

    //restoreState(settings->value("windowState").toByteArray());
    settings->endGroup();
}

void Quban::closeEvent(QCloseEvent *event)
{
	bool closeDown = false;
	int maxShutdownWait = config->maxShutDownWait;

    if (qMgr->empty() || queuePaused || mustShutDown == true || maxShutdownWait == 0 || config->noQueueSavePrompt)
		closeDown = true;
	else
	{
		int result = QMessageBox::question(this, tr("Question"), tr("The download queue is not empty. Are you sure you want to quit?"),
				QMessageBox::Yes, QMessageBox::No);
		switch (result)
		{
			case QMessageBox::Yes:
				closeDown = true;
				statusBar()->showMessage(tr("Closing connections..."), 0);
				break;
			case QMessageBox::No:
				break;
		}
	}

	if (closeDown == true)
	{
        if (qMgr->empty() || queuePaused || mustShutDown == true || maxShutdownWait == 0)
		{
			emit sigShutdown();
			saveConfig();
			settings->beginGroup("MainWindow");
			settings->setValue("geometry", saveGeometry());
			//settings->setValue("windowState", saveState());
		    settings->setValue("horizSplitter", splitter->saveState());
		    settings->setValue("vertSplitter", splitter_2->saveState());
			settings->endGroup();

			QMainWindow::closeEvent(event);
		}
		else
		{
			emit sigShutdown(); // may emit this several times
			event->ignore();
			shutdownTimer = new QTimer(this);
			shutdownTimer->setInterval(3 * 1000);
			connect(shutdownTimer, SIGNAL(timeout()), this, SLOT(slotShutdownTimeout()));
			shutdownTimer->start();
			qMgr->closeAllConnections();
//			qDebug() << "Closed connections";
		}
	}
	else
		event->ignore();
}

void Quban::saveConfig()
{
    config->mainTab = tabWidget->currentIndex();
    config->jobTab  = jobsAndLogsTabs->currentIndex();
	config->write();
}

void Quban::slotShutdownTimeout()
{
	++shutDownCount;
	int maxShutdownWait = config->maxShutDownWait/3;

	if (shutDownCount >= maxShutdownWait ||
		    (serversList->countWorkingThreads() == 0 && !qMgr->isDecoding())
		)
	{
		shutdownTimer->stop();
	    mustShutDown = true;
	    close();
	}
}

void Quban::slotStatusMessge(QString message)
{
	statusBar()->showMessage(message, 5000);
}

void Quban::quit_slot()
{
	QApplication::closeAllWindows();
}

void Quban::diagnostics_slot()
{
    QString dFile = config->nzbDir + "/qubanDiagnostics.txt";
    qDebug() << "Diagnostics will be written to " << dFile;
    getLogEventList()->logEvent(tr("Diagnostics will be written to ") + dFile);

    QFile file(dFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qDebug() << "Failed to write Diagnostics to " << dFile;
        logAlertList->logMessage(LogAlertList::Error, tr("Failed to write Diagnostics"));
        return;
    }

    QTextStream out(&file);
    out << tr("Quban 0.7.0 Diagnostic Report\t\t") << QDateTime::currentDateTime().toString("ddd MMMM dd yyyy hh:mm:ss") << "\n\n";

    out << tr("Third Party App Versions") << "\n";
    out << tr("------------------------") << "\n\n";
    out << tr("Berkeley db version = ") << QString(g_dbenv->version((int*)0, (int*)0, (int*)0)) << "\n";
    out << tr("Qt compile version = ") << QT_VERSION_STR << "\n";
    out << tr("Qt runtime version = ") << qVersion() << "\n";
    out << tr("zlib version = ") << ZLIB_VERSION << "\n\n";

    out << tr("Quban config settings") << "\n";
    out << tr("---------------------") << "\n\n";

    out << tr("General settings") << "\n\n";

    out << tr("Database directory = ") << config->dbDir << "\n";
    out << tr("Decode directory = ") << config->decodeDir << "\n";
    out << tr("NZB directory = ") << config->nzbDir << "\n";
    out << tr("Temp directory = ") << config->tmpDir << "\n\n";

    out << tr("Max wait before shutdown = ") << config->maxShutDownWait << "\n";
    out << tr("Rename NZB files when contents have been queued = ") << config->renameNzbFiles << "\n";
    out << tr("Don't prompt to save queue at shutdown = ") << config->noQueueSavePrompt << "\n";
    out << tr("Only display Newsgroups that exceed a minimum number = ") << config->minAvailEnabled << "\n";
    out << tr("If above true,only display Newsgroups that have more article parts than = ") << config->minAvailValue << "\n";
    out << tr("Application font = ") << config->appFont.toString() << "\n\n";

    out << tr("Decode settings") << "\n\n";

    out << tr("During decoding, overwite exiting files = ") << config->overwriteExisting << "\n";
    out << tr("Delete failed files during decoding = ") << config->deleteFailed << "\n\n";

    out << tr("View settings") << "\n\n";

    out << tr("File deletion policy = ") << config->dViewed << "\n";
    out << tr("View files in a single tab = ") << config->singleViewTab << "\n";
    out << tr("Text file suffixes = ") << config->textSuffixes << "\n";
    out << tr("Activate View tab when a new file is loaded = ") << config->activateTab << "\n";
    out << tr("Enable Status Bar on initial load = ") << config->showStatusBar << "\n";
    out << tr("Show Queue toolbar = ") << config->showQueueBar << "\n";
    out << tr("Show Server toolbar = ") << config->showServerBar << "\n";
    out << tr("Show Group toolbar = ") << config->showGroupBar << "\n";
    out << tr("Show Header toolbar = ") << config->showHeaderBar << "\n";
    out << tr("Show Job toolbar = ") << config->showJobBar << "\n\n";

    out << tr("Header List settings") << "\n\n";

    out << tr("Show From field = ") << config->showFrom << "\n";
    out << tr("Show Server Details field(s) = ") << config->showDetails << "\n";
    out << tr("Show Date field = ") << config->showDate << "\n";
    out << tr("Ignore articles that don't list the article number = ") << config->ignoreNoPartArticles << "\n";
    out << tr("Double click to view articles = ") << config->alwaysDoubleClick << "\n";
    out << tr("Remember sort order = ") << config->rememberSort << "\n";
    out << tr("Remember column widths = ") << config->rememberWidth << "\n";
    out << tr("Download compressed headers if possible = ") << config->downloadCompressed << "\n";
    out << tr("Close group options = ") << config->markOpt << "\n";
    out << tr("Expiry check interval = ") << config->checkDaysValue << "\n";
    out << tr("Bulk Load threshold = ") << config->bulkLoadThreshold << "\n";
    out << tr("Bulk Delete threshold = ") << config->bulkDeleteThreshold << "\n\n";

    out << tr("General Unpack settings") << "\n\n";

    out << tr("Auto Unpack = ") << config->autoUnpack << "\n";
    out << tr("Other file types = ") << config->otherFileTypes << "\n";
    out << tr("Num repairers = ") << config->numRepairers << "\n";
    out << tr("Num unpackers = ") << config->numUnpackers << "\n";
    out << tr("Del Group files = ") << config->deleteGroupFiles << "\n";
    out << tr("Del Repair files = ") << config->deleteRepairFiles << "\n";
    out << tr("Del Compressed files = ") << config->deleteCompressedFiles << "\n";
    out << tr("Del Other files = ") << config->deleteOtherFiles << "\n";
    out << tr("Merge 001 files = ") << config->merge001Files << "\n";
    out << tr("Don't show type confirm = ") << config->dontShowTypeConfirmUnlessNeeded << "\n\n";

    out << tr("Version settings") << "\n\n";

    out << tr("Group db version = ") << config->groupDbVersion << "\n";
    out << tr("Header db version = ") << config->headerDbVersion << "\n";
    out << tr("Avail Groups db version = ") << config->availableGroupsDbVersion << "\n";
    out << tr("Server db version = ") << config->serverDbVersion << "\n";
    out << tr("Queue db version = ") << config->queueDbVersion << "\n";
    out << tr("Unpack db version = ") << config->unpackDbVersion << "\n";
    out << tr("Scheduler db version = ") << config->schedulerDbVersion << "\n";
    out << tr("Version db version = ") << config->versionDbVersion << "\n\n";

    out << tr("Server settings") << "\n";
    out << tr("---------------") << "\n\n";

    NntpHost* nh = 0;
    QMapIterator<quint16, NntpHost*> i(*servers);
    while (i.hasNext())
    {
        i.next();
        nh = i.value();

        out << tr("Server id = ") << nh->getId() << "\n";
        out << tr("Host name = ") << nh->getHostName() << "\n";
        out << tr("Port = ") << nh->getPort() << "\n";
        out << tr("Timeout = ") << nh->getTimeout() << "\n";
        out << tr("Priority = ") << nh->getPriority() << "\n";
        out << tr("Thread timeout = ") << nh->getThreadTimeout() << "\n";
        out << tr("Retries = ") << nh->getRetries() << "\n";
        out << tr("Max threads = ") << nh->getMaxThreads() << "\n";
        out << tr("Working threads = ") << nh->getWorkingThreads() << "\n";
        out << tr("SSL socket = ") << nh->getSslSocket() << "\n";
        out << tr("Type = ") << nh->getServerType() << "\n";
        out << tr("Status = ") << nh->getServerStatus() << "\n";
        out << tr("Flags = ") << nh->getServerFlags() << "\n";
        out << tr("Flags date = ") << nh->getServerFlagsDate() << "\n";
        out << tr("Enabled = ") << nh->getEnabled() << "\n";
        out << tr("Deleted = ") << nh->getDeleted() << "\n";
        out << tr("Validated = ") << nh->getValidated() << "\n";
        out << tr("Validating = ") << nh->getValidating() << "\n";
        out << tr("Speed = ") << nh->getSpeed() << "\n";
        out << tr("Queue size = ") << nh->getQueueSize() << "\n";
        out << tr("Extensions = ") << nh->getEnabledServerExtensions() << "\n";
        out << tr("SSL allowable errors = ") << nh->getSslAllowableErrors() << "\n\n";
    }

    out << tr("Newsgroup settings") << "\n";
    out << tr("------------------") << "\n\n";

    NewsGroup* ng = 0;
    QHash<QString, NewsGroup*> groups = subGroupList->getGroups();
    QHash<QString, NewsGroup*>::iterator it;
    for (it = groups.begin(); it != groups.end(); ++it)
    {
        ng = it.value();

        out << tr("Total articles = ") << ng->totalArticles << "\n";
        out << tr("Unread articles = ") << ng->unreadArticles << "\n";
        out << tr("Only show unread = ") << ng->showOnlyUnread << "\n";
        out << tr("Only show complete = ") << ng->showOnlyComplete << "\n";
        out << tr("Delete older posting = ") << ng->deleteOlderPosting << "\n";
        out << tr("Delete older download = ") << ng->deleteOlderDownload << "\n";
        out << tr("Last expiry = ") << ng->lastExpiry << "\n";
        out << tr("Bulk seq = ") << ng->groupingBulkSeq << "\n";
        out << tr("Sort column = ") << ng->sortColumn << "\n";
        out << tr("Ascending = ") << ng->ascending << "\n";
        out << tr("Multi part seq = ") << ng->multiPartSequence << "\n";
        out << tr("Flags = ") << ng->newsGroupFlags << "\n\n";
    }

    file.close();

    getLogEventList()->logEvent(tr("Diagnostics successfully written"));
}

void Quban::about_slot()
{
    (new About(g_dbenv))->show();
}

void Quban::aboutQt_slot()
{
    QApplication::aboutQt();
}

void Quban::slotGetGroups( )
{

    emit getGroups(aGroups);
}

void Quban::enableUnpack(bool enabled)
{
	actionGroup_and_download_selected->setEnabled(enabled);
	actionDownload_nzb_files_and_auto_unpack->setEnabled(enabled);
	actionGroup_Info->setEnabled(enabled);
}

void Quban::slotDeleteServer( )
{
	if (!qMgr->empty())
		QMessageBox::warning(this, tr("Error"), tr("Can delete a server only if the download queue is empty"));
	else
	{
		//close ALL views, but first mark all groups as updating, so
		//we don't ask to mark articles...
		statusBar()->showMessage(tr("Deleting server, please be patient ..."), 5000);
/* TODO
		closeAllViews();
		enable(false);
		emit sigOkToDeleteServer();
		enable(true);
MD */
	}
}

void Quban::serverContextMenu(QPoint p)
{
	QTreeWidgetItem* item = serversList->itemAt(p);

	if (!item)
		return;

	if (item->type() == ServersList::ThreadItem)
	{
		threadMenu->exec(serversList->mapToGlobal(p));
	}
	else
		serverMenu->exec(serversList->mapToGlobal(p));
}

void Quban::slotServerSelection(bool)
{
}

void Quban::slotHaveServers(bool haveServers)
{
	if (haveServers)
	{
		actionDownload_nzb->setEnabled(true);
		actionGet_list_of_groups->setEnabled(true);
		actionDelete_Server->setEnabled(true);
		actionServer_properties->setEnabled(true);
        actionValidate_Server->setEnabled(true);
		actionTest_server_connection->setEnabled(true);
		actionAddThread->setEnabled(true);
		actionRemove_thread->setEnabled(true);
		actionShow_available_newsgroups->setEnabled(true);
		menuQueue->setEnabled(true);
		actionPause_queue->setEnabled(true);
		actionMove_to_top->setEnabled(true);
		actionMove_to_bottom->setEnabled(true);
		actionCancel->setEnabled(true);
		actionSchedulerEdit->setEnabled(true);
		actionTraffic_Management->setEnabled(true);
	}
	else // No servers defined!!
	{
		actionDownload_nzb->setEnabled(false);
		actionGet_list_of_groups->setEnabled(false);
		actionDelete_Server->setEnabled(false);
		actionServer_properties->setEnabled(false);
        actionValidate_Server->setEnabled(false);
		actionTest_server_connection->setEnabled(false);
		actionAddThread->setEnabled(false);
		actionRemove_thread->setEnabled(false);
		actionShow_available_newsgroups->setEnabled(false);
		menuQueue->setEnabled(false);
		actionPause_queue->setEnabled(false);
		actionMove_to_top->setEnabled(false);
		actionMove_to_bottom->setEnabled(false);
		actionCancel->setEnabled(false);
		actionSchedulerEdit->setEnabled(false);
		actionTraffic_Management->setEnabled(false);
	}
}

void Quban::slotUnsubscribe(QString groupName)
{
	for (int i=0; i<tabWidget_2->count(); ++i)
	{
		if (tabWidget_2->tabText(i) == groupName)
		{
			HeaderList* view = (HeaderList *)tabWidget_2->widget(i);
			tabWidget_2->removeTab(i);
			delete view;
		}
		else
		{
			if ( tabWidget_2->tabText(i) == tr("Available Newsgroups"))
			{
				; //aGroups->unsubscribeIcon(groupName);
			}
		}
	}
}

void Quban::headerContextMenu(QPoint point)
{
	if (point.isNull())
	    return;

	mainTabIndex = tabWidget_2->getTab()->tabAt(point);
//	qDebug() << "tab is " << mainTabIndex << ", tab text = " << tabWidget_2->widget(mainTabIndex)->metaObject()->className();

	if ( tabWidget_2->tabText(mainTabIndex) == tr("Queue")||
		 tabWidget_2->tabText(mainTabIndex) == tr("Finished")||
		 tabWidget_2->tabText(mainTabIndex) == tr("Failed"))
	{
		qDebug() << "It's the queue manager, ignore !!";
		return;
	}
	else if ( tabWidget_2->tabText(mainTabIndex) == tr("Available Newsgroups"))
	{
		qDebug() << "It's available groups !!";
		availTabMenu->exec(tabWidget_2->getTab()->mapToGlobal(point));
		return;
	}
	else if (QString("HeaderList") == tabWidget_2->widget(mainTabIndex)->metaObject()->className())
	{
		qDebug() << "It's headers !!";
		headerTabMenu->exec(tabWidget_2->getTab()->mapToGlobal(point));
		return;
	}
	else
	{
		qDebug() << "It's file viewer";
		viewerTabMenu->exec(tabWidget_2->getTab()->mapToGlobal(point));
		return;
	}
}

int Quban::countHeaderTabs()
{
	int tabCount = 0;

	for (int i=0; i<tabWidget_2->count(); ++i)
	{
		if (QString("HeaderList") == tabWidget_2->widget(i)->metaObject()->className())
  		    ++tabCount;
	}

	return tabCount;
}

void Quban::slotCloseHead()
{
	HeaderList* view = (HeaderList *)tabWidget_2->widget(mainTabIndex);
	if (view->prepareToClose(true) == true)
	{
		tabWidget_2->removeTab(mainTabIndex);
		delete view;

        tabWidget_2->setCurrentIndex(0);

		if (countHeaderTabs() == 0)
		{
		    menuArticles->setEnabled(false);
		    actionDownload_selected->setEnabled(false);
		    actionView_article->setEnabled(false);
		    actionShow_only_complete_articles->setEnabled(false);
		    actionShow_only_unread_articles->setEnabled(false);
		}
	}
}

void Quban::slotCloseHeadAndMark()
{
	HeaderList* view = (HeaderList *)tabWidget_2->widget(mainTabIndex);

	view->closeAndMark();
	tabWidget_2->removeTab(mainTabIndex);
	delete view;

    tabWidget_2->setCurrentIndex(0);

	if (countHeaderTabs() == 0)
	{
	    menuArticles->setEnabled(false);
	    actionDownload_selected->setEnabled(false);
	    actionView_article->setEnabled(false);
	    actionShow_only_complete_articles->setEnabled(false);
	    actionShow_only_unread_articles->setEnabled(false);
	}
}

void Quban::slotCloseHeadAndNoMark()
{
	HeaderList* view = (HeaderList *)tabWidget_2->widget(mainTabIndex);

	view->closeAndNoMark();
	tabWidget_2->removeTab(mainTabIndex);
	delete view;

    tabWidget_2->setCurrentIndex(0);

	if (countHeaderTabs() == 0)
	{
	    menuArticles->setEnabled(false);
	    actionDownload_selected->setEnabled(false);
	    actionView_article->setEnabled(false);
	    actionShow_only_complete_articles->setEnabled(false);
	    actionShow_only_unread_articles->setEnabled(false);
	}
}

void Quban::slotCloseView()
{
	FileViewer* view = (FileViewer *)tabWidget_2->widget(mainTabIndex);

	QString dbQuestion = tr("Do you want to delete the file ") + tabWidget_2->tabText(mainTabIndex) + " ?";
	int result = QMessageBox::question(this, tr("Question"), dbQuestion, QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel);

    if (result == QMessageBox::Cancel)
    	return;
    else if (result == QMessageBox::Yes)
    	view->deleteFile();

	tabWidget_2->removeTab(mainTabIndex);
	delete view;
}

void Quban::closeAndDelete()
{
	FileViewer* view = (FileViewer *)tabWidget_2->widget(mainTabIndex);
	view->deleteFile();
	tabWidget_2->removeTab(mainTabIndex);
	delete view;
}

void Quban::closeNoDelete()
{
	FileViewer* view = (FileViewer *)tabWidget_2->widget(mainTabIndex);
	tabWidget_2->removeTab(mainTabIndex);
	delete view;
}

void Quban::slotCloseAvail()
{
	AvailableGroups* view = (AvailableGroups *)tabWidget_2->widget(mainTabIndex);
	view->prepareToClose();
	tabWidget_2->removeTab(mainTabIndex);
	actionSubscribe_to_newsgroup->setEnabled(false);
    tabWidget_2->setCurrentIndex(0);
    delete aGroupsView;
    aGroupsView = 0;
//	aGroups->setVisible(false);
}

void Quban::slotSubscribed()
{
	if (subGroupList->topLevelItemCount())
	{
		actionView_group_articles->setEnabled(true);
		actionUnsubscribeToNewsgroup->setEnabled(true);
		actionUpdate_current->setEnabled(true);
		actionUpdate_subscribed_newsgroups->setEnabled(true);
		actionUpdate_selected_newsgroups->setEnabled(true);
		actionReset_selected->setEnabled(true);
		actionGroup_properties->setEnabled(true);
		actionNewsgroup_Management->setEnabled(true);
	}
}

void Quban::slotOpenNewsGroup( NewsGroup * ng )
{
	statusBar()->showMessage("Loading " + ng->getAlias() + "...", 3000);
	setEnabled(false);

	HeaderList * view=new HeaderList(ng, servers, ng->getName(), this);

	ng->setView( view );
	//subGroupList->saveGroup(ng);

	QMenu* headerMenu = view->getMenu();
	QTreeView *headerList = view->getTree();

	// This is the part that sets the menu order...
	headerList->addAction(actionView_article);
	headerList->addAction(actionDownload_selected);
	headerList->addAction(actionDownload_selected_first);
	headerList->addAction(actionDownload_to_dir);
	headerList->addAction(actionGroup_and_download_selected);
	headerList->addAction(headerMenu->addSeparator());
	headerList->addAction(actionMark_as_read);
	headerList->addAction(actionMark_as_unread);
	headerList->addAction(actionDelete);
	headerList->addAction(headerMenu->addSeparator());
//	headerList->addAction(actionGroup_articles);
	headerList->addAction(actionShow_only_complete_articles);
	headerList->addAction(actionShow_only_unread_articles);
	headerList->addAction(headerMenu->addSeparator());
	headerList->addAction(actionList_article_parts);
	headerList->addAction(headerMenu->addSeparator());
    headerList->addAction(copyToFilter);
	headerList->addAction(clearFilter);
	headerList->addAction(focusFilter);
    headerList->addAction(actionFilter_articles);

	connect(actionView_article, SIGNAL(triggered()), view, SLOT(slotViewArticle()));
	connect(actionDownload_selected, SIGNAL(triggered()), view, SLOT(slotDownloadSelected()));
	connect(actionDownload_selected_first, SIGNAL(triggered()), view, SLOT(slotDownloadSelectedFirst()));
	connect(actionDownload_to_dir, SIGNAL(triggered()), view, SLOT(slotDownloadToDir()));
	connect(actionGroup_and_download_selected, SIGNAL(triggered()), view, SLOT(slotGroupAndDownload()));
	connect(actionMark_as_read, SIGNAL(triggered()), view, SLOT(slotMarkSelectedAsRead()));
	connect(actionMark_as_unread, SIGNAL(triggered()), view, SLOT(slotMarkSelectedAsUnread()));
	connect(actionDelete, SIGNAL(triggered()), view, SLOT(slotDelSelected()));
	connect(actionShow_only_unread_articles, SIGNAL(triggered()), view, SLOT(slotShowOnlyNew()));
	connect(actionShow_only_complete_articles, SIGNAL(triggered()), view, SLOT(slotShowOnlyComplete()));
	connect(actionList_article_parts, SIGNAL(triggered()), view, SLOT(slotListParts()));
	connect(actionHeaders_Toolbar, SIGNAL(triggered(bool)), view, SLOT(slotHeaders_Toolbar(bool)));
//#ifndef NDEBUG
//	connect(actionGroup_articles, SIGNAL(triggered()), view, SLOT(slotGroupArticles()));
//#endif
	connect(focusFilter, SIGNAL(triggered()), view, SLOT(slotFocusFilter()));
	connect(clearFilter, SIGNAL(triggered()), view, SLOT(slotClearFilter()));
    connect(copyToFilter, SIGNAL(triggered()), view, SLOT(slotCopyToFilter()));

    connect(actionFilter_articles, SIGNAL(triggered(bool)), view, SLOT(slotFilterArticles(bool)));

// 	connect(view, SIGNAL(lostFocus(KMdiChildView* )), this, SLOT(slotGroupDeactivated(KMdiChildView* )));
// 	connect(view, SIGNAL(deactivated(KMdiChildView* )), this, SLOT(slotGroupDeactivated(KMdiChildView* )));
	connect(view, SIGNAL(downloadPost(HeaderBase*, NewsGroup*, bool, bool,  QString )), qMgr, SLOT(slotAddPostItem(HeaderBase*, NewsGroup*, bool , bool, QString )));
	connect(view, SIGNAL(viewPost(HeaderBase*, NewsGroup* )), qMgr, SLOT(slotViewArticle(HeaderBase*, NewsGroup* )));
	connect(view, SIGNAL(updateFinished(NewsGroup* )), subGroupList, SLOT(slotUpdateFinished(NewsGroup* )));
	connect(view, SIGNAL(sigSaveSettings(NewsGroup*, bool, bool )), subGroupList, SLOT(slotSaveSettings(NewsGroup*, bool, bool )));
	connect(view, SIGNAL(updateArticleCounts(NewsGroup*)), subGroupList, SLOT(saveGroup(NewsGroup*)));

	subGroupList->slotSaveSettings(ng, ng->onlyUnread(), ng->onlyComplete());

	setEnabled(true);
	statusBar()->showMessage(ng->getName() + " loaded", 3000);

    int tabIndex =
    		tabWidget_2->addTab(view, QIcon(":quban/images/hi16-action-icon_newsgroup.png"), ng ->getAlias());

    QPushButton *closeButton = new QPushButton();
    closeButton->setIcon(QIcon(":quban/images/fatCow/Cancel.png"));
    closeButton->setIconSize(QSize(16,16));

    connect(closeButton, SIGNAL(clicked()), this, SLOT(closeCurrentTab()));

    tabWidget_2->getTab()->setTabButton(tabIndex, QTabBar::RightSide, closeButton);

    tabWidget_2->setCurrentIndex(tabIndex);

	slotGroupActivated( view );

// 	wMenu->insertItem(QString::number(index) + ' ' + view->caption(), index, -1);
// 	wMenu->setItemParameter(index, 99);

	menuArticles->setEnabled(true);
	actionDownload_selected->setEnabled(true);
	actionView_article->setEnabled(true);
	actionShow_only_complete_articles->setEnabled(true);
	actionShow_only_unread_articles->setEnabled(true);
}

void Quban::slotOpenFileViewer(QString& fileName )
{
	FileViewer* fileView=new FileViewer(fileName, this);

	QStringList strList = fileName.split('/');
	if (strList.count() < 1)
		strList << "File Viewer";

    int tabIndex =
    		tabWidget_2->addTab(fileView, QIcon(":quban/images/fatCow/Picture.png"), strList.at(strList.count() - 1));

    QPushButton *closeButton = new QPushButton();
    closeButton->setIcon(QIcon(":quban/images/fatCow/Cancel.png"));
    closeButton->setIconSize(QSize(16,16));

    connect(closeButton, SIGNAL(clicked()), this, SLOT(closeCurrentTab()));

    tabWidget_2->getTab()->setTabButton(tabIndex, QTabBar::RightSide, closeButton);

    tabWidget_2->setCurrentIndex(tabIndex);
}

void Quban::closeCurrentTab()
{
    QPoint point = tabWidget_2->mapFromGlobal(QCursor::pos());
	if (point.isNull())
		 return;

	int activeIndex = tabWidget_2->getTab()->tabAt(point);
	qDebug() << "About to close tab " << activeIndex;

	if (QString("HeaderList") == tabWidget_2->widget(activeIndex)->metaObject()->className())
	{
	    HeaderList* view = (HeaderList *)tabWidget_2->widget(activeIndex);
	    tabWidget_2->removeTab(activeIndex);
	    view->closeAndNoMark();
	    delete view;

        tabWidget_2->setCurrentIndex(0);
	}
    else if ( tabWidget_2->tabText(activeIndex) == tr("Available Newsgroups"))
    {
        delete aGroupsView;
        aGroupsView = 0;
        tabWidget_2->setCurrentIndex(0);
        tabWidget_2->removeTab(activeIndex);
    }
	else
	    tabWidget_2->removeTab(activeIndex);
}

void Quban::slotGroupSelection( bool b )
{
	// TODO to suppress warning
    if (b) {;}
}

void Quban::slotUpdateNewsGroup( NewsGroup * ng)
{
	HeaderList *view;
	Q_ASSERT(ng);

	if ((view=ng->getView()))
	{
		for (int i=0; i<tabWidget_2->count(); ++i)
		{
			if (QString("HeaderList") == tabWidget_2->widget(i)->metaObject()->className())
			{
				if (tabWidget_2->tabText(i) == ng->getName())
					tabWidget_2->setTabIcon(i, QIcon(":quban/images/fatCow/Page-Refresh.png"));
			}
		}
	}
}

void Quban::slotUpdateCurrent( )
{
// TODO	HeaderList *activeGroup=(HeaderList *)activeWindow();
// TODO	subGroupList->updateCurrent(activeGroup->getNg());
}

void Quban::slotGroupActivated( QTabWidget * view)
{
// 	qDebug("Activated %s", (const char*) view->caption() );

	//set the status of the filter action...
// TODO	stateChanged("no_article_window", StateReverse);
	actionShow_only_unread_articles->setChecked( ((HeaderList*)view)->onlyNew());
	actionShow_only_complete_articles->setChecked(((HeaderList*)view)->onlyComplete());
}

void Quban::slotAddServerWidget(NntpHost *nh)
{
    sWidgets[nh->getId()] = new QStatusServerWidget(nh->getName(), nh->getServerType(), nh->isPaused(), this);
	statusBar()->insertPermanentWidget(0, sWidgets[nh->getId()], 0);
}

void Quban::slotPauseQ( )
{
	if (actionPause_queue->isChecked())
	{ //Pause the queue
		actionPause_queue->setText(("Resume Queue"));
		//now pause the queue
		qMgr->pauseQ(true);
        queuePaused = true;
		statusBar()->showMessage(tr("Queue Paused"), 3000);

	}
	else
	{ //resume the queue
		actionPause_queue->setText(tr("Pause Queue"));
		//now resume the queue
		qMgr->pauseQ(false);
        queuePaused = false;
		statusBar()->showMessage(tr("Queue resumed"), 3000);
	}
}

void Quban::slotServTypeChanged(int significantChange, int serverId, int newServerType, bool paused, QString& serverName)
{
	// The server has either just become dormant or is no longer dormant
	if (newServerType == NntpHost::dormantServer)
	{
		qDebug() << "Dormant";
		statusBar()->removeWidget(sWidgets[serverId]);
		sWidgets.remove(serverId);
		delete sWidgets[serverId];
	}
    else if (significantChange)
	{
		qDebug() << "No longer Dormant";
        sWidgets[serverId] = new QStatusServerWidget(serverName, newServerType, paused, this);
		statusBar()->insertPermanentWidget(0, sWidgets[serverId], 0);
	}
	else
	{
        sWidgets[serverId]->updateIcon(newServerType, paused);
	}
}

void Quban::slotServerInvalid(quint16 serverId)
{
    sWidgets[serverId]->updateIcon(-1, false);
}

void Quban::slotDeleteServerMessage()
{
    statusBar()->showMessage(tr("Deleting server, please be patient ...."), 0);
    QCoreApplication::processEvents();
}

void Quban::slotDeleteServerWidget(quint16 serverId)
{
	statusBar()->removeWidget(sWidgets[serverId]);
	delete sWidgets[serverId];
	sWidgets.remove(serverId);
	statusBar()->clearMessage();
}

void Quban::slotGroupInfo()
{
	GroupProgress* gp = new GroupProgress(qMgr);
	gp->setModal(false);
	gp->show();
	gp->raise();
}

void Quban::slotExtProcStarted(quint16 groupId, enum UnpackFileType groupType, QString& description)
{
	if (groupType == QUBAN_MASTER_REPAIR || groupType == QUBAN_REPAIR)
	    getJobList()->addJob(JobList::Repair, JobList::Running, groupId, description);
	else
		getJobList()->addJob(JobList::Uncompress, JobList::Running, groupId, description);
}

void Quban::slotDisplayScheduler()
{
	(new QueueSchedulerDialog(queueScheduler))->show();
}

void Quban::slotTraffic()
{
	RateControlDialog* rateControlDialog = new RateControlDialog();
	connect(rateControlDialog, SIGNAL(setSpeed(qint64)), qMgr->getRateController(), SLOT(slotSpeedChanged(qint64)));
	connect(rateControlDialog, SIGNAL(setSpeed(qint64)), this, SLOT(slotRateSpeedChanged(qint64)));

	rateControlDialog->exec();
}


void Quban::slotShowGroups()
{
	for (int i=0; i<tabWidget_2->count(); ++i)
	{
		if ( tabWidget_2->tabText(i) == tr("Available Newsgroups"))
		{
			tabWidget_2->setCurrentIndex(i);
			return;
		}
	}

	aGroupsView = new AvailableGroupsView(aGroups, tabWidget_2, this);

	connect(subGroupList, SIGNAL(unsubscribe(QString )), aGroups, SLOT(slotUnsubscribe(QString )));
	connect(serversList, SIGNAL(deleteServer(quint16 )), aGroups, SLOT(slotDeleteServer(quint16 )));
	connect(actionSubscribe_to_newsgroup, SIGNAL(triggered()), aGroupsView, SLOT(slotSubscribe()));

	connect(aGroupsView, SIGNAL(sigUnsubscribe(QString)), subGroupList, SLOT(slotUnsubscribe(QString)));
	connect(subGroupList, SIGNAL(unsubscribe(QString)), aGroupsView, SLOT(slotUnsubscribed(QString)));


	// connect(qMgr, SIGNAL(groupListDownloaded()), aGroups, SLOT(slotRebuildlist())); MD TODO send this to the view and wizard

// TODO	QMenu* headerMenu = aGroups->getMenu();
	QTreeView *availList = aGroupsView->getTree();

	// This is the part that sets the menu order...
	availList->addAction(actionSubscribe_to_newsgroup);

	actionSubscribe_to_newsgroup->setEnabled(true);

	availList->setFocus();
}

void Quban::slotUpdateSubscribed()
{
	subGroupList->updateGroups(aGroups);
}

void Quban::slotUpdateSpeed(int serverId, int speed)
{
	sWidgets[serverId]->updateSpeed(speed);
}

void Quban::slotRateSpeedChanged(qint64 newSpeed)
{
	if (rateControlInfo)
	{
		statusBar()->removeWidget(rateControlInfo);
		delete rateControlInfo;
		rateControlInfo = 0;
	}

	if (newSpeed != -1) // not normal speed
	{
		rateControlInfo = new QRateControlWidget(newSpeed);
		statusBar()->insertPermanentWidget(0, rateControlInfo, 0);
	}

	if (newSpeed == 0)
		isNilPeriod = true;
	else
		isNilPeriod = false;
}

void Quban::slotQueue_Toolbar(bool checked)
{
	if (checked)
	{
		toolBarTab_4->setHidden(false);
		toolBarTab_4->setEnabled(true);
		verticalLayout_6->setContentsMargins(0, 36, 0, 0);
	}
	else
	{
		toolBarTab_4->setHidden(true);
		toolBarTab_4->setEnabled(false);
		verticalLayout_6->setContentsMargins(0, 0, 0, 0);
	}

	config->showQueueBar = checked;
}

void Quban::slotServer_Toolbar(bool checked)
{
	if (checked)
	{
		toolBarTab_2->setHidden(false);
		toolBarTab_2->setEnabled(true);
		horizontalLayout_3->setContentsMargins(0, 36, 0, 0);
	}
	else
	{
		toolBarTab_2->setHidden(true);
		toolBarTab_2->setEnabled(false);
		horizontalLayout_3->setContentsMargins(0, 0, 0, 0);
	}

	config->showServerBar = checked;
}

void Quban::slotNewsgroup_Toolbar(bool checked)
{
	if (checked)
	{
		toolBarTab_1->setHidden(false);
		toolBarTab_1->setEnabled(true);
		horizontalLayout_4->setContentsMargins(0, 36, 0, 0);
	}
	else
	{
		toolBarTab_1->setHidden(true);
		toolBarTab_1->setEnabled(false);
		horizontalLayout_4->setContentsMargins(0, 0, 0, 0);
	}

	config->showGroupBar = checked;
}

void Quban::slotHeaders_Toolbar(bool checked)
{
	config->showHeaderBar = checked;
}

void Quban::slotJob_Toolbar(bool checked)
{
	if (checked)
	{
		toolBarTab_3->setHidden(false);
		toolBarTab_3->setEnabled(true);
		verticalLayout_5->setContentsMargins(0, 36, 0, 0);
	}
	else
	{
		toolBarTab_3->setHidden(true);
		toolBarTab_3->setEnabled(false);
		verticalLayout_5->setContentsMargins(0, 0, 0, 0);
	}

	config->showJobBar = checked;
}

QStatusServerWidget::QStatusServerWidget( QString serverName, int serverType, bool paused, QWidget * parent )
    : QWidget(parent)
{
	m_sName = serverName;
	QHBoxLayout *layout = new QHBoxLayout(this);
	layout->setSpacing(1);
	layout->setContentsMargins(2,0,2,0);

	iLabel = new QLabel(this);
    if (serverType == NntpHost::activeServer)
    {
        if (!paused)
            ssicon = new QPixmap(QString::fromUtf8(":/quban/images/ginux/Entire_Network-16.png"));
        else
            ssicon = new QPixmap(QString::fromUtf8(":/quban/images/ginux/Entire_Network_Paused-16.png"));
    }
    else if (serverType == NntpHost::passiveServer)
    {
        if (!paused)
            ssicon = new QPixmap(QString::fromUtf8(":/quban/images/ginux/Passive_Network-16.png"));
        else
            ssicon = new QPixmap(QString::fromUtf8(":/quban/images/ginux/Passive_Network_Paused-16.png"));
    }
	else
		ssicon = new QPixmap(QString::fromUtf8(":/quban/images/ginux/Dormant_Network-16.png"));

	iLabel->setPixmap(*ssicon);
	iLabel->setContentsMargins(1,1,1,1);

	sLabel = new QLabel(this);
	sLabel->setText(m_sName + ": Disconnected");
	sLabel->setContentsMargins(1,1,1,1);
	layout->addWidget(iLabel);
	layout->addWidget(sLabel);
}

QStatusServerWidget::~QStatusServerWidget()
{
	delete ssicon;
}

void QStatusServerWidget::updateIcon(int serverType, bool paused)
{
	if (serverType == NntpHost::activeServer)
    {
        if (!paused)
            ssicon = new QPixmap(QString::fromUtf8(":/quban/images/ginux/Entire_Network-16.png"));
        else
            ssicon = new QPixmap(QString::fromUtf8(":/quban/images/ginux/Entire_Network_Paused-16.png"));
    }
	else if (serverType == NntpHost::passiveServer)
    {
        if (!paused)
            ssicon = new QPixmap(QString::fromUtf8(":/quban/images/ginux/Passive_Network-16.png"));
        else
            ssicon = new QPixmap(QString::fromUtf8(":/quban/images/ginux/Passive_Network_Paused-16.png"));
    }
	else if (serverType == NntpHost::dormantServer)
		ssicon = new QPixmap(QString::fromUtf8(":/quban/images/ginux/Dormant_Network-16.png"));
	else
		ssicon = new QPixmap(QString::fromUtf8(":/quban/images/ginux/Poorly_Network-16.png"));

	iLabel->setPixmap(*ssicon);
}

void QStatusServerWidget::updateSpeed( int speed )
{
	if (speed == -1)
	{
		//Idle
		sLabel->setText(m_sName + ": Idle");
	}
	else if (speed == -2)
	{
		//Disconnected
		sLabel->setText(m_sName + ": Disconnected");
	}
	else
	{
		sLabel->setText(m_sName + ':' + QString::number(speed) + " KB/s");
	}
}

QToolbarWidget::QToolbarWidget( QWidget * parent ) :
		QWidget (parent)
{
	QHBoxLayout *layout = new QHBoxLayout(this);
	layout->setSpacing(1);
	layout->setContentsMargins(2,0,2,0);
	tLabel=new QLabel(this);
	pLabel=new QLabel(this);
	tIcon=new QPixmap(":/quban/images/hi16-action-icon_article_unread.png");

	pLabel->setPixmap(*tIcon);
	pLabel->setContentsMargins(1,1,1,1);
	tLabel->setText(" 0 items, 0 Mb");
	tLabel->setContentsMargins(1,1,1,1);
	layout->addWidget(pLabel);
	layout->addWidget(tLabel);
}

QToolbarWidget::~QToolbarWidget()
{
	delete tIcon;
}

void QToolbarWidget::slotQueueInfo( unsigned long long  count, unsigned long long size )
{
	QString sSize;
	QString units(" Mb");

	if (size < 1073741824) // 1024 * 1024 * 1024
	{
		sSize=QString::number(((double(size)/1024)/1024), 'f', 2 );
	}
	else
	{
		sSize=QString::number((((double(size)/1024)/1024)/1024), 'f', 2 );
		units = " Gb";
	}

	tLabel->setText(QString::number(count) + " items, " + sSize + units);
}

QRateControlWidget::QRateControlWidget(qint64 _speed, QWidget *)
{
	QHBoxLayout *layout = new QHBoxLayout(this);
	layout->setSpacing(1);
	layout->setContentsMargins(2,0,2,0);

	iLabel = new QLabel(this);
	speedLabel = new QLabel(this);

	speedicon = new QPixmap(QString::fromUtf8(":/quban/images/fatCow/Speedometer16.png"));
	speedLabel->setText(QString::number(((double(_speed)/1024)/1024), 'f', 2 ) + " Mb/sec");

	iLabel->setPixmap(*speedicon);
	iLabel->setContentsMargins(1,1,1,1);

	speedLabel->setContentsMargins(1,1,1,1);
	layout->addWidget(iLabel);
	layout->addWidget(speedLabel);

}

void QRateControlWidget::update(qint64 _speed)
{
	speedLabel->setText(QString::number(((double(_speed)/1024)/1024), 'f', 2 ) + " Mb/sec");
}

QRateControlWidget::~QRateControlWidget()
{
	delete speedicon;
}
