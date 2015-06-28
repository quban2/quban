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
#include <QTimer>
#include <QMessageBox>
#include <QList>
#include <QtAlgorithms>
#include <QMapIterator>
#include "quban.h"
#include "serverslist.h"
#include "qubanDbEnv.h"
#include "hlistviewitem.h"

extern Quban* quban;

ServersList::ServersList(QString dbName, DbEnv *_dbEnv, Servers *_servers, QWidget* parent)
: QTreeWidget(parent), servers(_servers), dbEnv(_dbEnv),serversDbName(dbName)
{
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
    this->header()->setMovable(false);
#else
    this->header()->setSectionsMovable(false);
#endif
	m_loadServers(); //this populates the serverlist
	addServers(); //this populates the listview
	connect(this, SIGNAL(itemSelectionChanged()), this, SLOT(slotSelectionChanged()));
    serverSpeedTimer = new QTimer();
    connect(serverSpeedTimer, SIGNAL(timeout()), this, SLOT(slotServerSpeedTimeout()));
    qDebug()  << "In ServersList::ServersList 1";
}

ServersList::ServersList(Servers *_servers, QWidget * parent )
:  QTreeWidget(parent), servers(_servers)

{
	serversDbName="servers";
	dbEnv=0;
	serverDb=0;
	connect(this, SIGNAL(itemSelectionChanged()), this, SLOT(slotSelectionChanged()));
    serverSpeedTimer = new QTimer();
	connect(serverSpeedTimer, SIGNAL(timeout()), this, SLOT(slotServerSpeedTimeout()));
    qDebug()  << "In ServersList::ServersList 2";
}

ServersList::ServersList(QWidget * parent )
:  QTreeWidget(parent)
{
	serversDbName="servers";
}

ServersList::~ServersList()
{
    qDebug() << "In ServersList::~ServersList";

    Q_DELETE(activeIcon);
    Q_DELETE(activePausedIcon);
    Q_DELETE(passiveIcon);
    Q_DELETE(passivePausedIcon);
    Q_DELETE(dormantIcon);

    QMapIterator<int, ThreadView> i(serverThreads);
    while (i.hasNext())
    {
        i.next();

        QMapIterator<int, SpeedThread*> j(i.value());
        while (j.hasNext())
        {
            j.next();

            if (j.value())
            {
                if (j.value()->item)
                    Q_DELETE(j.value()->item);

                Q_DELETE_NO_LVALUE(j.value());
            }
        }
    }

    Q_DELETE(serverSpeedTimer);

    serverThreads.clear();
}

void ServersList::serversListInit()
{
	NntpHost::initHostMaps();

    activeIcon = new QIcon(QString::fromUtf8(":/quban/images/ginux/Entire_Network-16.png"));
    activePausedIcon = new QIcon(QString::fromUtf8(":/quban/images/ginux/Entire_Network_Paused-16.png"));
    passiveIcon = new QIcon(QString::fromUtf8(":/quban/images/ginux/Passive_Network-16.png"));
    passivePausedIcon = new QIcon(QString::fromUtf8(":/quban/images/ginux/Passive_Network_Paused-16.png"));
    dormantIcon = new QIcon(QString::fromUtf8(":/quban/images/ginux/Dormant_Network-16.png"));

    m_loadServers(); //this populates the serverlist
 	addServers(); //this populates the listview

	connect(this, SIGNAL(itemSelectionChanged()), this, SLOT(slotSelectionChanged()));

    qDebug()  << "In ServersList::serversListInit";
    serverSpeedTimer = new QTimer(this);
    connect(serverSpeedTimer, SIGNAL(timeout()), this, SLOT(slotServerSpeedTimeout()));
}

void ServersList::addServers()
{
	Servers::iterator it;
	for (it =servers->begin(); it != servers->end(); ++it)
	{
		if (!it.value()->getDeleted()) // hide deleted servers
		    m_addServer(it.value());
	}
	this->setRootIsDecorated(true);
}

void ServersList::slotSpeedChanged( int serverId, int threadId, int speed )
{
	if ((*servers)[serverId]->getServerType() == NntpHost::dormantServer)
		return;

	//alternate way...
	SpeedThread *sp=serverThreads[serverId][threadId];

// 	sp->speed=(sp->speed + speed) /2;

	if (sp->status == Running)
	{
	    sp->speed = speed;
	    sp->item->setText(SpeedCol, QString::number( sp->speed ) + " KB/s");
	}
    else
        qDebug() << "Getting updates for non running server : " << serverId << " from thread : " << threadId;

}

void ServersList::slotThreadStart( quint16 serverId, int threadId)
{
	if ((*servers)[serverId]->getServerType() == NntpHost::dormantServer)
		return;

	SpeedThread *sp=serverThreads[serverId][threadId];

    if (sp->status != Running)
    {
		sp->item->setText(StatusCol, tr("Working..."));
		sp->item->setText(SpeedCol, "0 KB/s");
		sp->status=Running;
	// 	qDebug("WorkingThread: %d", (*servers)[serverId]->workingThreads);
		(*servers)[serverId]->setWorkingThreads((*servers)[serverId]->getWorkingThreads() + 1);
        //qDebug() << serverId << "," <<  threadId << ": slotThreadStart(); workingThreads: " << (*servers)[serverId]->getWorkingThreads();
	// 	qDebug("%d, %d: slotThreadStart(); workingThreads: %d", serverId, threadId, (*servers)[serverId]->workingThreads);
    }

    if (!serverSpeedTimer->isActive())
        serverSpeedTimer->start(SPEED_TIMEOUT);
}

void ServersList::slotThreadStop( quint16 serverId, int threadId)
{
	if ((*servers)[serverId]->getServerType() == NntpHost::dormantServer)
		return;

	//qDebug() << serverId << "," << threadId << ": slotThreadStop()";
	SpeedThread *sp=serverThreads[serverId][threadId];
	sp->item->setText(StatusCol, tr("Idle"));
	sp->item->setText(SpeedCol, "");
	sp->count=1;
	sp->speed=0;
	sp->status=Idle;
    if ((*servers)[serverId]->getWorkingThreads())
        (*servers)[serverId]->setWorkingThreads((*servers)[serverId]->getWorkingThreads() - 1);
	if ((*servers)[serverId]->getWorkingThreads() == 0)
	{
		(*servers)[serverId]->getItem()->setText(SpeedCol, "");
		emit sigServerSpeed(serverId, -1);
	}
}

void ServersList::slotThreadPaused( quint16 serverId, int threadId , int seconds)
{
	if ((*servers)[serverId]->getServerType() == NntpHost::dormantServer)
		return;

	SpeedThread *sp=serverThreads[serverId][threadId];
	if (seconds == 0)
		sp->item->setText(StatusCol, tr("Paused"));
	else sp->item->setText(StatusCol, tr("Paused ") + QString::number(seconds) + " s");
 	sp->item->setText(SpeedCol, "");
	sp->count=1;
	sp->speed=0;
	sp->status=Paused;
    if ((*servers)[serverId]->getWorkingThreads())
        (*servers)[serverId]->setWorkingThreads((*servers)[serverId]->getWorkingThreads() - 1);
    if ((*servers)[serverId]->getWorkingThreads() == 0)
    {
		(*servers)[serverId]->getItem()->setText(SpeedCol, "");
		emit sigServerSpeed(serverId, -1);
	}
}

void ServersList::slotCountDown( quint16 serverId, int threadId, int seconds)
{
	SpeedThread *sp=serverThreads[serverId][threadId];
	sp->item->setText(SpeedCol, QString::number(seconds) + " s");
}

void ServersList::slotExtensionsUpdate(NntpHost* nh, quint64 extensions)
{
	qDebug() << "At final dest, Received extensions of " << extensions << "for server " << nh->getName();

	Servers::iterator sit;
	for (sit=servers->begin(); sit != servers->end(); ++sit)
	{
		if (sit.value()->getName() == nh->getName())
		{
			if (sit.value()->getServerFlags() == 0 && extensions) // First time through, enable all extensions
				sit.value()->setEnabledServerExtensions(extensions);
			sit.value()->setServerFlags(extensions);
			sit.value()->setServerFlagsDate(QDateTime::currentDateTime().toTime_t());

			m_saveServer(sit.value());
			break;
		}
	}
}

void ServersList::slotCanceling( quint16 serverId, int threadId)
{
    //qDebug() << "slotCancelling()";
	SpeedThread *sp=serverThreads[serverId][threadId];
	sp->item->setText(StatusCol, tr("Cancelling..."));
	sp->item->setText(SpeedCol, "");
	sp->count=1;
	sp->speed=0;
	sp->status=Canceling;
}

void ServersList::slotThreadDisconnect( quint16 serverId , int threadId)
{
	if ((*servers)[serverId]->getServerType() == NntpHost::dormantServer)
		return;

	SpeedThread *sp=serverThreads[serverId][threadId];
	sp->item->setText(StatusCol, tr("Disconnected"));
	sp->item->setText(SpeedCol, "");
	sp->count=1;
	sp->speed=0;

	if (sp->status != Idle && sp->status != Stopped && sp->status != Paused)
	{
        if ((*servers)[serverId]->getWorkingThreads())
            (*servers)[serverId]->setWorkingThreads((*servers)[serverId]->getWorkingThreads() - 1);
		sp->status = Stopped;
	}

	if ((*servers)[serverId]->getWorkingThreads() == 0)
	{
// 			serverSpeedTimer->stop();
			(*servers)[serverId]->getItem()->setText(SpeedCol, "");
			emit sigServerSpeed(serverId, -2);
	}
}

int ServersList::countWorkingThreads()
{
	int numThreads=0;

	Servers::iterator it;
	for (it =servers->begin(); it != servers->end(); ++it)
	{
        if (it.value())
            numThreads += it.value()->getWorkingThreads();
	}

	return numThreads;
}

void ServersList::slotNewServer()
{
	AddServer *as=new AddServer(this->parentWidget());
	connect(as, SIGNAL(newServer(NntpHost* )), this, SLOT(slotAddServer(NntpHost* )));
	as->exec();
}

void ServersList::slotAddServer( NntpHost *nh )
{
	NntpHost *eh;
	Servers::iterator it;

	for (it =servers->begin(); it != servers->end(); ++it)
	{
		eh = it.value();
		if (nh->getName() == eh->getName())
		{
			// Server with this name already exists!!!
			QMessageBox::warning(this, tr("Duplicate server identifier"),
					tr("A server with this local name already exists, please choose another"));
			return;
		}
	}

	//Next, find a free id...
	int id=0;
	while (servers->contains(++id));
	nh->setId(id);
	(*servers)[id]=nh;
	m_saveServer(nh);
	m_addServer(nh);
	emit sigServerAdded(nh);
    if (nh->getServerType() != NntpHost::dormantServer)
    {
        emit sigHaveServers(true);

        nh->setValidating(true);
        connect(nh, SIGNAL(sigSaveServer(NntpHost*)), this, SLOT(m_saveServer(NntpHost*)));
        connect(nh, SIGNAL(sigInvalidServer(quint16)), this, SLOT(slotInvalidServer(quint16)));
        connect(nh, SIGNAL(sigServerValidated(quint16, bool)), quban->getQMgr(), SLOT(slotServerValidated(quint16, bool)));
        nh->slotTestServer();

        emit sigUpdateServerExtensions(nh->getId());
    }
}

void ServersList::m_addServer( NntpHost *nh )
{
	QTreeWidgetItem *parent=new QTreeWidgetItem(ServersList::ServerItem);
	parent->setText(0,nh->getName());
	nh->setItem(parent);

    QIcon* icon = new QIcon(QString::fromUtf8(":/quban/images/ginux/Dormant_Network-16.png"));

 	parent->setIcon(0, *icon);
 	this->addTopLevelItem(parent);

    Q_DELETE(icon);

    nh->setDb(serverDb);

	QTreeWidgetItem * prev = 0;

	QStringList sList;
	QString threadName;

	for (int i =1 ; i <= nh->getMaxThreads(); i++)
	{
		serverThreads[nh->getId()].insert(i, new SpeedThread);
		serverThreads[nh->getId()][i]->item=new TListViewItem(nh->getId(), i, parent, prev);
		threadName = "Thread #" + QString::number(i);
		sList.clear();
		sList << threadName;

		prev = serverThreads[nh->getId()][i]->item;
		serverThreads[nh->getId()][i]->speed=0;
		serverThreads[nh->getId()][i]->count=1;
		serverThreads[nh->getId()][i]->status=Stopped;
	}

	QHeaderView *header = parent->treeWidget()->header();
	header->resizeSection(0, 150);
	header->resizeSection(1, 125);
	header->resizeSection(2, 75);

    quban->getLogEventList()->logEvent(tr("Successfully opened server ") + nh->getName());
}

void ServersList::m_loadServers()
{
	int ret=0;

    QByteArray ba = serversDbName.toLocal8Bit();
    const char *c_str = ba.data();

	if (!(serverDb = quban->getDbEnv()->openDb(c_str, true, &ret)))
		QMessageBox::warning(0, tr("Error"), tr("Unable to open server database ..."));

	char datamem[10000];
	char keymem[10000];

	Dbt key, data;
	key.set_flags(DB_DBT_USERMEM);
	key.set_ulen(10000);
	key.set_data(keymem);

	data.set_flags(DB_DBT_USERMEM);
	data.set_ulen(10000);
	data.set_data(datamem);
// 	groups.clear();

	Dbc *cursor;
	serverDb->cursor(0, &cursor, 0);

	while((cursor->get(&key, &data, DB_NEXT))==0)
	{
        NntpHost *nh= new NntpHost(this, serverDb);
		nh->loadHost(datamem);

        qDebug() << "Just loded server with id " << nh->getId();
        qDebug() << "Just loded server with name " << nh->getName();

		(*servers)[nh->getId()]=nh;
		nh->setEnabled(true);
		nh->setValidated(false); 

        if (nh->getServerType() != NntpHost::dormantServer && !nh->getDeleted())
		{
			nh->setValidating(true);
			connect(nh, SIGNAL(sigSaveServer(NntpHost*)), this, SLOT(m_saveServer(NntpHost*)));
			connect(nh, SIGNAL(sigInvalidServer(quint16)), this, SLOT(slotInvalidServer(quint16)));

            connect(nh, SIGNAL(sigPauseThreads(int)), this, SLOT(slotPauseThreads(int)));
            connect(nh, SIGNAL(sigResumeThreads(int)), this, SLOT(slotResumeThreads(int)));
		}
		else
			nh->setValidating(false);
	}

	cursor->close();
}

void ServersList::m_saveServer(NntpHost* nh)
{
    char datamem[10000];
    char keymem[10000];

	Dbt key, data;
	key.set_flags(DB_DBT_USERMEM);
    key.set_ulen(10000);
	key.set_data(keymem);

	data.set_flags(DB_DBT_USERMEM);
    data.set_ulen(10000);
	data.set_data(datamem);
	char *p=nh->saveHost();
	memcpy(datamem, p, nh->getSize());
    Q_DELETE_ARRAY(p);

	qDebug() << "Saving flags of " << nh->getServerFlags() << " for " << nh->getName();

	data.set_size(nh->getSize());
	quint16 serverId = nh->getId();
	memcpy(keymem, &serverId, sizeof(quint16));
	key.set_size(sizeof(quint16));
	int ret=serverDb->put(0, &key,&data, 0);
	if (ret != 0)
        quban->getLogAlertList()->logMessage(LogAlertList::Error, tr("Failed to save server ") + nh->getName());
	else
        quban->getLogEventList()->logEvent(tr("Successfully saved server ") + nh->getName());
	serverDb->sync(0);
}

void ServersList::slotDeleteServer()
{
	bool foundServer = false;

	qDebug() << "Delete server";
	QTreeWidgetItem* selected=(QTreeWidgetItem*)this->currentItem();

	if (!selected)
    {
        adviseNothingSelected();
        return;
    }

	if (selected->type() == ServersList::ThreadItem)
		selected=(QTreeWidgetItem*)selected->parent();

	Servers::iterator sit;
	for (sit=servers->begin(); sit != servers->end(); ++sit)
	{
		if (sit.value()->getName() == selected->text(0))
		{
			foundServer = true;
			break;
		}
	}

	if (foundServer)
	{
		quint16 serverId=sit.key();
		servers->value(serverId)->setUserName(QUBAN_GHOST_SERVER);
		servers->value(serverId)->setPass(QString::null);
		servers->value(serverId)->setServerType(NntpHost::dormantServer);
		servers->value(serverId)->setDeleted(true);
		m_saveServer(servers->value(serverId));

		//Various steps:
		//1) Delete the server's articles from ALL the newsgroups
		//2) Delete the server's queue in the queue manager
		//3) Delete the server from the availablegroups
		//4) Delete the server from the list.
		//THIS CAN BE VERY LONG!!!

		//1, 2, 3
		emit deleteServer(serverId);

		//4)
		qDebug("phase 4");
        Q_DELETE(selected);
	}
}

void ServersList::slotValidateServer()
{
    bool foundServer = false;

    qDebug() << "Validate server";
    QTreeWidgetItem* selected=(QTreeWidgetItem*)this->currentItem();

    if (!selected)
    {
        adviseNothingSelected();
        return;
    }

    if (selected->type() == ServersList::ThreadItem)
        selected=(QTreeWidgetItem*)selected->parent();

    Servers::iterator sit;
    for (sit=servers->begin(); sit != servers->end(); ++sit)
    {
        if (sit.value()->getName() == selected->text(0))
        {
            foundServer = true;
            break;
        }
    }

    if (foundServer)
    {
        quint16 serverId=sit.key();

        if (sit.value()->validated) // server doesn't need validating
            return;

        emit validateServer(serverId);
    }
}

void ServersList::slotDeleteDeletedServer(quint16 serverId)
{
	//Delete the server from the
	// - servers' list, container and db.

    Q_DELETE((*servers)[serverId]);
	servers->remove(serverId);
	Dbt key;
	memset(&key, 0, sizeof(key));
	key.set_data(&serverId);
	key.set_size(sizeof(serverId));
	if ((serverDb->del(NULL, &key, 0)) != 0)        
		qDebug() << "Error deleting server";
	else
    {
		qDebug() << "Server " << serverId << " deleted";
        serverDb->sync(0);
    }
	if (servers->isEmpty())
		emit sigHaveServers(false);
}

void ServersList::slotServerProperties()
{
	selected=(QTreeWidgetItem*)this->currentItem();
	if (!selected)
    {
        adviseNothingSelected();
        return;
    }

	if (selected->type() == ServersList::ThreadItem)
		selected=(QTreeWidgetItem*)selected->parent();

	Servers::iterator sit;
	for (sit=servers->begin(); sit != servers->end(); ++sit)
	{
		if (sit.value()->getName() == selected->text(0))
			break;
	}
	this->setEnabled(false);

	NntpHost* nh = sit.value();

	AddServer *as=new AddServer(nh, this->parentWidget());
	connect(as, SIGNAL(newServer(NntpHost*)), this, SLOT(slotModifyServer(NntpHost*)));
    connect(as, SIGNAL(testServer(QString,quint16,int,qint16,int)), nh, SLOT(slotTestServer()));
	as->exec();
	this->setEnabled(true);
}

void ServersList::slotServerTest()
{
	QTreeWidgetItem* selected=(QTreeWidgetItem*)this->currentItem();
	if (!selected)
    {
        adviseNothingSelected();
        return;
    }

	if (selected->type() == ServersList::ThreadItem)
		selected=(QTreeWidgetItem*)selected->parent();

	Servers::iterator sit;
	for (sit=servers->begin(); sit != servers->end(); ++sit) {
		if (sit.value()->getName() == selected->text(0))
			break;
	}

    sit.value()->slotTestServer();
}

void ServersList::slotValidServer(quint16 id)
{
    NntpHost * nh = 0;
    QIcon*icon = 0;

    QTreeWidgetItem *serverItem = 0;
    for (int i=0; i<this->topLevelItemCount(); ++i)
    {
        serverItem = this->topLevelItem(i);
        nh = servers->value(id);
        if (serverItem->text(0)== nh->getName())
            break;
    }

    if (serverItem)
    {
        if (nh->getServerType() == NntpHost::activeServer)
        {
            if (!nh->isPaused())
                icon = activeIcon;
            else
                icon = activePausedIcon;
        }
        else if (nh->getServerType() == NntpHost::passiveServer)
        {
            if (!nh->isPaused())
                icon = passiveIcon;
            else
                icon = passivePausedIcon;
        }
        else
            icon = dormantIcon;

        serverItem->setIcon(0, *icon);
        quban->setStatusIcon(id, nh->getServerType(), nh->isPaused());

        if (nh->getServerLimitEnabled())
        {
            /* reset if required */
            QDateTime dt;
            dt.setTime_t(nh->getResetDate());

            QDate newDate = QDate::currentDate();
            QTime t = QTime::currentTime();

            if (nh->getFrequency() == 0) // Daily
            {
                if (dt.date().addDays(1) < newDate)
                {
                    nh->setDownloaded(0);
                    QDateTime newdt(newDate, t);
                    nh->setResetDate(newdt.toTime_t());
                }
            }
            else if (nh->getFrequency() == 1) // Weekly
            {
                if (dt.date().addDays(7) < newDate)
                {
                    nh->setDownloaded(0);
                    QDateTime newdt(newDate, t);
                    nh->setResetDate(newdt.toTime_t());
                }
            }
            else // Monthly
            {
                if (dt.date().addMonths(1) < newDate)
                {
                    nh->setDownloaded(0);
                    QDateTime newdt(newDate, t);
                    nh->setResetDate(newdt.toTime_t());
                }
            }

            if (nh->isDownloadLimitExceeded())
            {
                quban->getQMgr()->serverDownloadsExceeded(id);
            }
        }
    }
}

void ServersList::slotInvalidServer(quint16 id)
{
	NntpHost * nh;
	QIcon*icon = new QIcon(QString::fromUtf8(":/quban/images/ginux/Poorly_Network-16.png"));

	QTreeWidgetItem *serverItem = 0;
	for (int i=0; i<this->topLevelItemCount(); ++i)
	{
		serverItem = this->topLevelItem(i);
		nh = servers->value(id);
		if (serverItem->text(0)== nh->getName())
			break;
	}

	if (serverItem)
	{
	    serverItem->setIcon(0, *icon);
	    emit sigServerInvalid(id);
	}
}

void ServersList::slotModifyServer(NntpHost * nh)
{
	int significantChange = 0;

	Servers::iterator sit;
	for (sit=servers->begin(); sit != servers->end(); ++sit)
	{
		if (sit.value()->getName() == selected->text(0))
			break;
	}

	NntpHost* oh = sit.value();
	QTreeWidgetItem *serverItem = oh->getItem();

    if ((oh->getServerType() == NntpHost::dormantServer && nh->getServerType() != NntpHost::dormantServer) ||
         (oh->getServerType() != NntpHost::dormantServer && nh->getServerType() == NntpHost::dormantServer))
	{
		significantChange = 1;
	}

    if (nh->getServerType() != oh->getServerType() || oh->isPaused() != nh->isPaused())
	{
        emit sigServerTypeChanged(significantChange, oh->getId(), nh->getServerType(), nh->isPaused(), nh->getName());

		QIcon* icon;

        if (nh->getServerType() == NntpHost::activeServer)
        {
            if (!nh->isPaused())
                icon = new QIcon(QString::fromUtf8(":/quban/images/ginux/Entire_Network-16.png"));
            else
                icon = new QIcon(QString::fromUtf8(":/quban/images/ginux/Entire_Network_Paused-16.png"));
        }
        else if (nh->getServerType() == NntpHost::passiveServer)
        {
            if (!nh->isPaused())
                icon = new QIcon(QString::fromUtf8(":/quban/images/ginux/Passive_Network-16.png"));
            else
                icon = new QIcon(QString::fromUtf8(":/quban/images/ginux/Passive_Network_Paused-16.png"));
        }
		else
			icon = new QIcon(QString::fromUtf8(":/quban/images/ginux/Dormant_Network-16.png"));

		selected->setIcon(0, *icon);
		serverItem->setIcon(0, *icon);
	}

	oh->setHostName(nh->getHostName());
	oh->setName(nh->getName());
	oh->setPort(nh->getPort());
	oh->setUserName(nh->getUserName());
	oh->setPass(nh->getPass());
	oh->setTimeout(nh->getTimeout());
	oh->setPriority(nh->getPriority());
	oh->setThreadTimeout(nh->getThreadTimeout());
	oh->setRetries(nh->getRetries());
	oh->setSslSocket(nh->getSslSocket());
    oh->setSslProtocol(nh->getSslProtocol());
	oh->setServerType(nh->getServerType());
	oh->setServerFlags(nh->getServerFlags());
	oh->setEnabledServerExtensions(nh->getEnabledServerExtensions());
	oh->setSslCertificate(new QSslCertificate(*(nh->getSslCertificate())));
	oh->setExpectedSslErrors(nh->getExpectedSslErrors());
	oh->setSslAllowableErrors(nh->getSslAllowableErrors());
	// MD TODO we may need to drop server connections if modified in above line ...

    oh->setServerLimitEnabled(nh->getServerLimitEnabled());
    oh->setMaxDownloadLimit(nh->getMaxDownloadLimit());
    oh->setPaused(nh->isPaused());

    oh->setResetDate(nh->getResetDate());
    oh->setLimitUnits(nh->getLimitUnits());
    oh->setFrequency(nh->getFrequency());

    if (!nh->getServerLimitEnabled())
        oh->setDownloaded(0);  

    oh->computeMaxDownloadLimitBytes();

	//Add or remove threads...not done for now...
	qint16 i = oh->getMaxThreads() - nh->getMaxThreads();
	if ( i > 0 )
	{
		//delete i threads
		for (qint16 j = 0; j < i ; j++)
		{
			deleteThread(sit.key()); // MD TODO need to check both lists
		}
	}
	else if ( i < 0 )
	{
		for (qint16 j = 0; j > i; j--)
		{
			addThread(selected, sit.key());
			addThread(serverItem, sit.key()); // MD TODO check this as we don't need speed etc in Explorer
		}
	}

	oh->setMaxThreads(nh->getMaxThreads());

	selected->setText(0, nh->getName());
	serverItem->setText(0, nh->getName());

    oh->setDb(serverDb);

	m_saveServer(oh);
    quban->getLogEventList()->logEvent(tr("Successfully updated server ") + nh->getName());

    nh->setServerLimitEnabled(false); // to prevent save by destructor ...
    Q_DELETE(nh);
}

void ServersList::slotDisableServer()
{
	QTreeWidgetItem *selected=this->currentItem();
	if (!selected)
    {
        adviseNothingSelected();
        return;
    }

	if (selected->parent() != (QTreeWidgetItem*)this && selected->parent() != (QTreeWidgetItem*)0)
		selected=(QTreeWidgetItem*)selected->parent();
	Servers::iterator it;
	for (it=servers->begin(); it != servers->end(); ++it)
		if (selected->text(0) == it.value()->getName())
			break;
	//Toggle
	if (it.value()->getEnabled()==false)
	{
		selected->setDisabled(false);
		it.value()->setEnabled(true);

	}
	else
	{
// TODO		selected->setOpen(false);
		selected->setDisabled(true);
		it.value()->setEnabled(false);
	}
}

void ServersList::slotServersPopup( QTreeWidgetItem *item, const QPoint &p )
{
	if (!item)
		return;
	if (item->type() == ThreadItem)
	{
		//consider the status of the thread...
		TListViewItem *tItem = (TListViewItem*)item;
		emit sigThreadPopup(serverThreads[tItem->qId][tItem->threadId]->status, p );
	}
	else
		emit sigServerPopup(p);
}

void ServersList::slotSelectionChanged()
{
	QList<QTreeWidgetItem *> selection=this->selectedItems();
	if (selection.isEmpty())
		emit serverSelected(false);
	else
		emit serverSelected(true);
}

void ServersList::slotAddThread()
{
	QTreeWidgetItem *selected=this->currentItem();
	if (!selected)
    {
        adviseNothingSelected();
        return;
    }

	if (selected->type() == ServersList::ThreadItem)
		selected=(QTreeWidgetItem*)selected->parent();

	Servers::iterator it;
	for (it=servers->begin(); it != servers->end(); ++it)
		if (selected->text(0) == it.value()->getName())
			break;

	//Add the thread to the list...
	//Find a free thread...
	addThread(selected, it.key());

	m_saveServer((*servers)[it.key()]);
}

void ServersList::slotDeleteThread()
{
	QTreeWidgetItem *selected=this->currentItem();
	if (!selected)
    {
        adviseNothingSelected();
        return;
    }

	if (selected->parent() != (QTreeWidgetItem*)this && selected->parent() != (QTreeWidgetItem*)0)
		selected=(QTreeWidgetItem*)selected->parent();
	Servers::iterator it;
	for (it=servers->begin(); it != servers->end(); ++it)
		if (selected->text(0) == it.value()->getName())
			break;

	if (it.value()->getMaxThreads() == 1) {
		//Only one thread left, don't delete
		QMessageBox::warning(this, tr("Invalid deletion"), tr("Only one thread left, cannot delete it"));
		return;
	}
	//else...

	deleteThread(it.key());

	m_saveServer((*servers)[it.key()]);
}

void ServersList::slotThreadDeleted( int serverId , int threadId)
{
    if (serverThreads[serverId][threadId]->status == Running && (*servers)[serverId]->getWorkingThreads())
    {
		(*servers)[serverId]->setWorkingThreads((*servers)[serverId]->getWorkingThreads() - 1);
	}
    Q_DELETE(serverThreads[serverId][threadId]->item);
    Q_DELETE(serverThreads[serverId][threadId]);
	serverThreads[serverId].remove(threadId);

	//Now reorder the queue...not anymore
	//This happens while other threads are running, but since
	//SIGNAL and slots are synchronous, it should be safe...

	int i = 1;
	ThreadView::iterator it;
    for (it = serverThreads[serverId].begin(); it != serverThreads[serverId].end() ; ++it)
    {
		it.value()->item->setText(0, "Thread #" + QString::number(i));
		++i;
	}
}

void ServersList::enable( bool b)
{
	if (b) {
		this->setCursor(Qt::ArrowCursor);
		setCursor(Qt::ArrowCursor);
	} else {
		this->setCursor(Qt::WaitCursor);
		setCursor(Qt::WaitCursor);
	}
// 	this->setEnabled(b);
// 	setEnabled(b);


	//if this was the last server,

}

void ServersList::addThread( QTreeWidgetItem* selected,  int serverId)
{
	ThreadView::iterator tit;
	int threadId=1; //threads starts at one

    for (tit=serverThreads[serverId].begin() ; tit!= serverThreads[serverId].end(); ++tit)
    {
		if (tit.key() != threadId)
			break;
		else ++threadId;
	}
	//Now threadId holds the first "free place" in the threadList...it's our threadId!
	qDebug("New threadId: %d", threadId);

	QTreeWidgetItem *lastChild=0;
	if (selected->childCount())
		lastChild=selected->child(selected->childCount()-1);

	SpeedThread *newSt=new SpeedThread;
	newSt->count=1;
	newSt->speed=0;

	newSt->item=new TListViewItem(serverId, serverThreads[serverId].count()+1, selected, lastChild);
	serverThreads[serverId].insert(threadId, newSt);
	newSt->status = Stopped;

	//Add to the queueThreads
	emit sigAddThread(serverId, threadId);
	(*servers)[serverId]->setMaxThreads((*servers)[serverId]->getMaxThreads() + 1);

}

void ServersList::deleteThread( int serverId)
{
	(*servers)[serverId]->setMaxThreads((*servers)[serverId]->getMaxThreads() - 1);
    emit sigDeleteThread(serverId);
}

void ServersList::slotPauseThread()
{
	//Tell the Q manager to pause the thread
	TListViewItem *item=(TListViewItem*)this->currentItem();
 	emit sigPauseThread(item->qId, item->threadId);
}

void ServersList::slotResumeThread()
{
	//Tell the Q Manager to resume the thread
	TListViewItem *item=(TListViewItem*)this->currentItem();
 	emit sigResumeThread(item->qId, item->threadId);

}

void ServersList::checkServers()
{
 	if (servers->isEmpty())
 		emit sigHaveServers(false);
 	else
 	{
 		bool validServer = false;
 		Servers::iterator sit;
 		for (sit=servers->begin(); sit != servers->end(); ++sit)
 		{
 			if (sit.value()->getServerType() != NntpHost::dormantServer)
 			{
 				validServer = true;
 				break;
 			}
 		}
 		if (validServer)
 		    emit sigHaveServers(true);
 		else
 			emit sigHaveServers(false);
 	}
}

void ServersList::slotServerSpeedTimeout()
{
	Servers::iterator sit;
	ThreadView::iterator it;
	int serverSpeed;
    bool oneIsWorking = false;

    for (sit = servers->begin(); sit != servers->end(); ++sit)
    {
        if (sit.value() && sit.value()->getWorkingThreads() > 0)
        {
            oneIsWorking = true;
			ThreadView tv = serverThreads[sit.key()];
			serverSpeed = 0;
            for (it = tv.begin(); it != tv.end() ; ++it)
				serverSpeed += it.value()->speed;

// MD TODO try without the balance for a while			serverSpeed = (serverSpeed+sit.value()->speed)/2;
			sit.value()->getItem()->setText(SpeedCol, QString::number( serverSpeed ) + " KB/s");
			sit.value()->setSpeed(serverSpeed);
 			emit sigServerSpeed(sit.key(), serverSpeed);
        }
	}

    if (!oneIsWorking)
            serverSpeedTimer->stop();
}

void ServersList::slotPauseServer()
{
    QIcon*icon = 0;
    QTreeWidgetItem *selected=this->currentItem();
    if (!selected)
    {
        adviseNothingSelected();
        return;
    }

    if (selected->parent() != (QTreeWidgetItem*)this && selected->parent() != (QTreeWidgetItem*)0)
        selected=(QTreeWidgetItem*)selected->parent();
    Servers::iterator it;
    for (it=servers->begin(); it != servers->end(); ++it)
        if (selected->text(0) == it.value()->getName())
            break;

    it.value()->pauseServer();

    if (it.value()->getServerType() == NntpHost::activeServer)
        icon = new QIcon(QString::fromUtf8(":/quban/images/ginux/Entire_Network_Paused-16.png"));
    else if (it.value()->getServerType() == NntpHost::passiveServer)
        icon = new QIcon(QString::fromUtf8(":/quban/images/ginux/Passive_Network_Paused-16.png"));
    else
        icon = new QIcon(QString::fromUtf8(":/quban/images/ginux/Dormant_Network-16.png"));

    selected->setIcon(0, *icon);
    quban->setStatusIcon(it.value()->getId(), it.value()->getServerType(),true);
}

void ServersList::slotResumeServer()
{
    QIcon*icon = 0;
    QTreeWidgetItem *selected=this->currentItem();
    if (!selected)
    {
        adviseNothingSelected();
        return;
    }

    if (selected->parent() != (QTreeWidgetItem*)this && selected->parent() != (QTreeWidgetItem*)0)
        selected=(QTreeWidgetItem*)selected->parent();
    Servers::iterator it;
    for (it=servers->begin(); it != servers->end(); ++it)
        if (selected->text(0) == it.value()->getName())
            break;

    it.value()->resumeServer();

    if (it.value()->getServerType() == NntpHost::activeServer)
        icon = new QIcon(QString::fromUtf8(":/quban/images/ginux/Entire_Network-16.png"));
    else if (it.value()->getServerType() == NntpHost::passiveServer)
        icon = new QIcon(QString::fromUtf8(":/quban/images/ginux/Passive_Network-16.png"));
    else
        icon = new QIcon(QString::fromUtf8(":/quban/images/ginux/Dormant_Network-16.png"));

    selected->setIcon(0, *icon);
    quban->setStatusIcon(it.value()->getId(), it.value()->getServerType(),false);
}

void ServersList::slotPauseThreads(int serverId)
{
    ThreadView::iterator tit;

    for (tit=serverThreads[serverId].begin() ; tit!= serverThreads[serverId].end(); ++tit)
    {
        emit sigPauseThread(serverId, tit.key());
    }
}

void ServersList::slotResumeThreads(int serverId)
{
    ThreadView::iterator tit;

    for (tit=serverThreads[serverId].begin() ; tit!= serverThreads[serverId].end(); ++tit)
    {
        emit sigResumeThread(serverId, tit.key());
    }
}

void ServersList::adviseNothingSelected()
{
    QMessageBox::warning(0, tr("Information"), tr("You need to select a server by clicking on it first ..."));
}
