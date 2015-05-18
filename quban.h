#ifndef QUBAN_H
#define QUBAN_H

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

#include <QMainWindow>
#include <QPixmap>
#include <QSettings>
#include "ui_quban.h"
#include "common.h"
#include "qmgr.h"
#include "serverslist.h"
#include "maintabwidget.h"
#include "QueueScheduler.h"


 class EchoInterface
 {
 public:
     virtual ~EchoInterface() {}
     virtual QString echo(const QString &message) = 0;
 };

 Q_DECLARE_INTERFACE(EchoInterface,
                     "com.trolltech.Plugin.EchoInterface/1.0");

class QStatusServerWidget: public QWidget {
	Q_OBJECT
	private:
		QLabel *iLabel;
		QLabel *sLabel;
		//QLabel *speedLabel;
		int speed;
		QString m_sName;
		QPixmap *ssicon;
	public:
        QStatusServerWidget(QString serverName, int serverType, bool paused, QWidget *parent = 0);
		~QStatusServerWidget();
		void updateSpeed(int speed);
        void updateIcon(int, bool);
};

class QToolbarWidget : public QWidget {

	Q_OBJECT
	private:
		QLabel *tLabel;
		QLabel *pLabel;
		QPixmap *tIcon;

	public:
		QToolbarWidget(QWidget* parent = 0);
		~QToolbarWidget();

	public slots:
		void slotQueueInfo(unsigned long long count, unsigned long long size);
};

class QRateControlWidget: public QWidget {
	Q_OBJECT
	private:
	    bool   displayed;
		QLabel *speedLabel;
		QLabel *iLabel;
		QPixmap *speedicon;
	public:
		QRateControlWidget(qint64 _speed, QWidget *parent = 0);
		~QRateControlWidget();
		void update(qint64);
		void setDisplayed(bool _displayed) { displayed = _displayed; }
		bool getDisplayed() { return displayed; }
};

class QubanDbEnv;
class QMenu;
class FileViewer;
class AvailableGroups;
class AvailableGroupsView;

class JobList;

class Quban : public QMainWindow, public Ui::quban
{
    Q_OBJECT

public:
    Quban(QWidget *parent = 0);
    ~Quban();
    void startup();
    QTabWidget* getMainTab() { return tabWidget_2; }
    void enableUnpack(bool);

    QueueScheduler* getQueueScheduler() { return queueScheduler; }

    EchoInterface *echoInterface;

private:
    void readSettings();
    void closeEvent(QCloseEvent *);
	void setupActions();
    int countHeaderTabs();

    QSettings *settings;

public:
    QMgr* getQMgr() {return qMgr;}
    GroupList* getSubGroupList() { return subGroupList; }
    QubanDbEnv* getDbEnv() { return qubanDbEnv; }
    Servers* getServers() { return servers; }

	LogAlertList* getLogAlertList() { return logAlertList; }
	QTreeWidget* getAlertTreeWidget() { return alertTreeWidget; }
	LogEventList* getLogEventList() { return logEventList; }
	QTreeWidget* getEventTreeWidget() { return eventTreeWidget; }
	JobList* getJobList() { return jobList; }
	QTreeWidget* getJobsTreeWidget() { return jobsTreeWidget; }

    bool getIsNilPeriod() { return isNilPeriod; }

    void setStartupScheduleRequired(bool b) { startupScheduleRequired = b; }
    void saveConfig();

    void setStatusIcon(quint16 serverId, quint16 serverType, bool paused) { sWidgets[serverId]->updateIcon(serverType, paused); }

public slots:
    virtual void quit_slot();
    void showWizard(bool atStartup = false);    
    virtual void diagnostics_slot();
    virtual void about_slot();
    virtual void aboutQt_slot();

    void slotQueue_Toolbar(bool);
    void slotServer_Toolbar(bool);
    void slotNewsgroup_Toolbar(bool);
    void slotHeaders_Toolbar(bool);
    void slotJob_Toolbar(bool);

	void slotServerSelection(bool);
	void slotHaveServers(bool);

	void slotRateSpeedChanged(qint64);

	void slotDeleteServerMessage();
	void slotDeleteServerWidget(quint16);

	void slotOpenNewsGroup(NewsGroup *ng);
	void slotGroupSelection(bool);
// TODO	void slotActivateNewsGroup(HeaderList*pWnd) {activateView(pWnd);};

	void slotOpenFileViewer(QString&);

	void slotSubscribed();
    void slotServTypeChanged(int, int, int, bool, QString&);
	void slotServerInvalid(quint16);
	void slotUnsubscribe(QString);

	void slotAddServerWidget(NntpHost*);
	void serverContextMenu(QPoint);
	void headerContextMenu(QPoint);

	void slotGroupInfo();

	void slotExtProcStarted(quint16, enum UnpackFileType, QString&);

	void slotStatusMessge(QString);

    void slotDisplayScheduler();

private slots:
    void slotTraffic();
	void slotShowGroups();
	void slotGetGroups();
	void slotUpdateSubscribed();
	void slotDeleteServer();
	void slotUpdateNewsGroup(NewsGroup*);
	void slotGroupActivated(QTabWidget*);
	void slotUpdateCurrent();
	void slotPauseQ();
	void slotUpdateSpeed(int, int);
	void slotCloseHead();
	void slotCloseHeadAndMark();
	void slotCloseHeadAndNoMark();
	void slotCloseAvail();
	void slotCloseView();
	void closeAndDelete();
	void closeNoDelete();
	void slotShutdownTimeout();
	void closeCurrentTab();

signals:
    void getGroups(AvailableGroups*);
    void deleteServer(quint16);
    void sigShutdown();

private:
	Configuration* config;
	QubanDbEnv* qubanDbEnv;

	Servers* servers;
	Db *schedDb;
	Db* groupDb;
	Db* agroupDb; // available groups
	AvailableGroups* aGroups;
	AvailableGroupsView* aGroupsView;
	QMgr *qMgr;

	LogAlertList* logAlertList;
	LogEventList* logEventList;
	JobList* jobList;

	QToolBar* toolBarTab_1;
	QToolBar* toolBarTab_2;
	QToolBar* toolBarTab_3;
	QToolBar* toolBarTab_4;

	bool isNilPeriod;
	bool startupScheduleRequired;
	QueueScheduler* queueScheduler;

	int mainTabIndex;
	QMenu* queueMenu;
	QMenu* serverMenu;
	QMenu* jobsMenu;
	QMenu* threadMenu;
	QMenu* subGroupMenu;
	QMenu* availTabMenu;
	QMenu* headerTabMenu;
	QMenu* viewerTabMenu;

	QToolbarWidget *qTWidget;
	QRateControlWidget* rateControlInfo;

	FileViewer *fViewer;
	QMap<int, QStatusServerWidget*> sWidgets;

	QTimer* shutdownTimer;
	bool    mustShutDown;
    bool    queuePaused;
	int     shutDownCount;
};

#endif // QUBAN_H
