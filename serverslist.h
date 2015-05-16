#ifndef SERVERSLIST_H
#define SERVERSLIST_H

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

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QMap>
#include "addserver.h"
#include "common.h"
#include "hlistviewitem.h"

#define SPEED_TIMEOUT 2000

class TListViewItem;

enum ServersColums {ServerCol=0, StatusCol=1, SpeedCol=2};
enum ThreadStatus {Stopped, Running, Paused, Canceling, Idle, Disconnected};

// #define SPEED_MEAN 5

struct SpeedThread {
// 	uint speeds[SPEED_MEAN];
    TListViewItem* item;
	int count;
	uint speed;
	ThreadStatus status;
};

typedef QMap<int, SpeedThread*> ThreadView;
typedef QMap<int, ThreadView> ServerThreads;

class ServersList : public QTreeWidget
{
    Q_OBJECT

  void addServers();
  Servers *servers;
  ServerThreads serverThreads;
  Db* serverDb;
  DbEnv *dbEnv;

  QString serversDbName;

private:
  QTreeWidgetItem* selected;
  QTreeWidget* explorer;
  
  void adviseNothingSelected();

public:
  void m_addServer(NntpHost*);
  void m_loadServers();

  void addThread(QTreeWidgetItem*, int);
  void deleteThread(int);
  QTimer *serverSpeedTimer;
//   void checkTimer(bool);

public:
  ServersList(QString, DbEnv *_dbEnv, Servers *_servers, QWidget* parent = 0);
  ServersList(Servers *_servers, QWidget *parent=0);
  ServersList(QWidget *parent=0);
  virtual ~ServersList();
  Db* getDb() {return serverDb;}
  void enable(bool);
  void checkServers();
  void setServers(Servers* p_servers) { servers = p_servers; }
  void setDbEnv(DbEnv* p_dbEnv) { dbEnv = p_dbEnv; }
  void serversListInit();
  int countWorkingThreads();

  /*$PUBLIC_FUNCTIONS$*/

public slots:
  /*$PUBLIC_SLOTS$*/
    void m_saveServer(NntpHost*);
	void slotSpeedChanged(int , int , int );
	void slotThreadStart(quint16, int);
	void slotThreadStop(quint16,int);
	void slotThreadDisconnect(quint16,int);
	void slotCanceling(quint16,int);
	void slotNewServer();
	void slotAddServer(NntpHost*);
	void slotDeleteServer();
    void slotValidateServer();
    void slotPauseServer();
    void slotResumeServer();
    void slotPauseThreads(int);
    void slotResumeThreads(int);

	void slotServerProperties();
	void slotModifyServer(NntpHost*);
	void slotDisableServer();
 	void slotServersPopup(QTreeWidgetItem *, const QPoint&);
	void slotThreadDeleted(int, int);
	void slotThreadPaused(quint16, int, int);
	void slotCountDown(quint16, int, int);
	void slotExtensionsUpdate(NntpHost *, quint64);
	void slotDeleteDeletedServer(quint16);
	void slotServerTest();
	void slotInvalidServer(quint16);
    void slotValidServer(quint16);

protected:
  /*$PROTECTED_FUNCTIONS$*/

protected slots:
  /*$PROTECTED_SLOTS$*/
	void slotSelectionChanged();
	void slotAddThread();
	void slotDeleteThread();
	void slotPauseThread();
	void slotResumeThread();
	void slotServerSpeedTimeout();

signals:
	void sigServerAdded(NntpHost*);
	void sigServerDeleted(NntpHost*);
	void sigHaveServers(bool);
	void sigAddThread(int, int);
	void sigDeleteThread(int);
	void sigServerPopup(const QPoint &);
	void sigThreadPopup(ThreadStatus, const QPoint &);
	void serverSelected(bool);
	void deleteServer(quint16);
	void sigPauseThread(int, int);
	void sigResumeThread(int, int);
	void sigServerSpeed(int, int);
    void sigServerTypeChanged(int, int, int, bool, QString&);
	void sigServerInvalid(quint16);
    void sigUpdateServerExtensions(quint16);
    void startAllThreads(int);
    void validateServer(quint16);

public:
	enum { ServerItem = 1005, ThreadItem = 1010 };
};

#endif // SERVERSLIST_H
