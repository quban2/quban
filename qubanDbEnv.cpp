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
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QProgressDialog>
#include <QStringBuilder>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "quban.h"
#include "qubanDbEnv.h"

extern DbEnv *g_dbenv;
extern Quban* quban;

extern int compare_q16(Db *, const Dbt *, const Dbt *);

const char* VERSIONDB_NAME = "versiondb";
const char* AVAILGROUPSDB_NAME = "availableGroups";
const char* SERVERSDB_NAME = "servers";
const char* SCHEDULERDB_NAME = "scheduler";
const char* QUEUEDB_NAME = "queue";
const char* AUTOUNPACKDB_NAME = "autoUnpack";
const char* PENDINGDB_NAME = "pendingFiles";
const char* SUBSGROUPSDB_NAME = "newsgroups";

QubanDbEnv::QubanDbEnv(Configuration* _config)
{
	config = _config;
	dbDir = config->dbDir;

	g_dbenv=new DbEnv(DB_CXX_NO_EXCEPTIONS);
	g_dbenv->set_cachesize(0, 33554432, 0); // 32 Mb

    QByteArray ba = dbDir.toLocal8Bit();
    const char *d = ba.data();

    int ret = 0;

	if ((ret=g_dbenv->open(d, DB_CREATE | DB_THREAD | DB_INIT_CDB |DB_INIT_MPOOL | DB_PRIVATE, 0644))!= 0)
 	    qDebug() << "Error opening environment: " <<  g_dbenv->strerror(ret);
}

QubanDbEnv::~QubanDbEnv()
{
}

Db* QubanDbEnv::openDb(const char* dbName, bool create, int* ret)
{
	Db* thisDb=new Db(g_dbenv,0);

	if (create)
	{
		if ((*ret = thisDb->open(NULL, dbName, NULL, DB_BTREE, DB_CREATE | DB_THREAD, 0644)!=0))
		{
			quban->getLogAlertList()->logMessage(LogAlertList::Error, tr("Error creating ") + dbName + " db! : " + g_dbenv->strerror(*ret));
			delete thisDb;
			thisDb = 0;
		}
	}
	else
	{
		if ((*ret = thisDb->open(NULL, dbName, NULL, DB_BTREE, DB_THREAD, 0644)!=0))
		{
			quban->getLogAlertList()->logMessage(LogAlertList::Error, tr("Error opening ") + dbName + " db! : " + g_dbenv->strerror(*ret));
			delete thisDb;
			thisDb = 0;
		}
	}

	return thisDb;
}

Db* QubanDbEnv::openDb(const char* dbMasterName, const char* dbName, bool create, int* ret)
{
	Db* thisDb=new Db(g_dbenv,0);

	if (create)
	{
		if ((*ret = thisDb->open(NULL, dbMasterName, dbName, DB_BTREE, DB_CREATE | DB_THREAD, 0644)!=0))
		{
			quban->getLogAlertList()->logMessage(LogAlertList::Error, tr("Error creating ") + dbName + " db! : " + g_dbenv->strerror(*ret));
			delete thisDb;
			thisDb = 0;
		}
	}
	else
	{
		if ((*ret = thisDb->open(NULL, dbMasterName, dbName, DB_BTREE, DB_THREAD, 0644)!=0))
		{
			quban->getLogAlertList()->logMessage(LogAlertList::Error, tr("Error opening ") + dbName + " db! : " + g_dbenv->strerror(*ret));
			delete thisDb;
			thisDb = 0;
		}
	}

	return thisDb;
}

bool QubanDbEnv::dbsExists()
{
  bool retCode = true;
  QString dir = QDir::cleanPath(dbDir);

  if (!QFile::exists(QString("%1/availableGroups").arg(dir)) ||
      !QFile::exists(QString("%1/newsgroups").arg(dir)) ||
      !QFile::exists(QString("%1/queue").arg(dir)) ||
      !QFile::exists(QString("%1/servers").arg(dir)) ||
      !QFile::exists(QString("%1/autoUnpack").arg(dir)) ||
      !QFile::exists(QString("%1/scheduler").arg(dir)) ||
      !QFile::exists(QString("%1/versiondb").arg(dir)))
  {
	  qDebug() << "Failed to find one or more database";
	  retCode = false;
  }

  if (retCode == false &&
		  !QFile::exists(QString("%1/scheduler").arg(dir)))
	  quban->setStartupScheduleRequired(true);
  else
	  quban->setStartupScheduleRequired(false);

  return retCode;
}

void QubanDbEnv::createDbs()
{
	int ret;

	groupDbVersion=GROUPDB_VERSION;
	headerDbVersion=HEADERDB_VERSION;
	availableGroupsDbVersion = AVAILABLEGROUPSDB_VERSION;
	serverDbVersion = SERVERDB_VERSION;
	queueDbVersion = QUEUEDB_VERSION;
	unpackDbVersion = UNPACKDB_VERSION;
	schedulerDbVersion = SCHEDULERDB_VERSION;
	versionDbVersion = VERSIONDB_VERSION;

	Db *versionDb;

	if ((versionDb = openDb(VERSIONDB_NAME, true, &ret)))
	{
		saveVersionData(versionDb);
		versionDb->close(0);
		delete versionDb;
	}
	else
	{
		qDebug() << "Error creating version db : Continuing with backup values only ... : " <<  g_dbenv->strerror(ret);
		config->groupDbVersion=groupDbVersion;
		config->headerDbVersion=headerDbVersion;
		config->availableGroupsDbVersion = availableGroupsDbVersion;
		config->serverDbVersion = serverDbVersion;
		config->queueDbVersion = queueDbVersion;
		config->unpackDbVersion = unpackDbVersion;
		config->schedulerDbVersion = schedulerDbVersion;
		config->versionDbVersion = versionDbVersion;
	}

	versionDb=0;
}

void QubanDbEnv::saveVersionData(Db *versionDb)
{
	//KEY is one of GROUPDB, AVAILABLEGROUPSDB, HEADERDB etc
	//Keys, data have fixed size:
	//DATA is a uint

	const char *groupDbKey="GROUPDB";
	const char *headerDbKey="HEADERDB";
	const char *availableGroupsDbKey="AVAILABLEGROUPSDB";
	const char *serverDbKey="SERVERDB";
	const char *queueDbKey="QUEUEDB";
	const char *unpackDbKey="UNPACKDB";
	const char *schedulerDbKey="SCHEDULERDB";
	const char *versionDbKey="VERSIONDB";

	int ret;

	Dbt  key((void*)groupDbKey, strlen(groupDbKey));
	Dbt data((void*)&groupDbVersion, sizeof(quint32));
	//Put groupDb version in versionDb
	ret=versionDb->put(0, &key, &data, 0);
	if (ret != 0)
	{
		qDebug("Error putting group db version: %s", g_dbenv->strerror(ret));
	}

	data.set_data((void*)&headerDbVersion);
	data.set_size(sizeof(quint32));
	key.set_data((void*)headerDbKey);
	key.set_size(quint32(strlen(headerDbKey)));
	ret=versionDb->put(0, &key, &data, 0);
	if (ret != 0) {
		qDebug("Error putting header db version: %s", g_dbenv->strerror(ret));
	}

	data.set_data((void*)&availableGroupsDbVersion);
	data.set_size(sizeof(quint32));
	key.set_data((void*)availableGroupsDbKey);
	key.set_size(quint32(strlen(availableGroupsDbKey)));
	ret=versionDb->put(0, &key, &data, 0);
	if (ret != 0)
	{
		qDebug() << "Error putting agroups db version: " <<  g_dbenv->strerror(ret);
	}

	data.set_data((void*)&serverDbVersion);
	data.set_size(sizeof(quint32));
	key.set_data((void*)serverDbKey);
	key.set_size(quint32(strlen(serverDbKey)));
	ret=versionDb->put(0, &key, &data, 0);
	if (ret != 0)
	{
		qDebug() << "Error putting server db version: " <<  g_dbenv->strerror(ret);
	}

	data.set_data((void*)&queueDbVersion);
	data.set_size(sizeof(quint32));
	key.set_data((void*)queueDbKey);
	key.set_size(quint32(strlen(queueDbKey)));
	ret=versionDb->put(0, &key, &data, 0);
	if (ret != 0)
	{
		qDebug() << "Error putting queue db version: " <<  g_dbenv->strerror(ret);
	}

	data.set_data((void*)&unpackDbVersion);
	data.set_size(sizeof(quint32));
	key.set_data((void*)unpackDbKey);
	key.set_size(quint32(strlen(unpackDbKey)));
	ret=versionDb->put(0, &key, &data, 0);
	if (ret != 0)
	{
		qDebug() << "Error putting unpack db version: " <<  g_dbenv->strerror(ret);
	}

	data.set_data((void*)&schedulerDbVersion);
	data.set_size(sizeof(quint32));
	key.set_data((void*)schedulerDbKey);
	key.set_size(quint32(strlen(schedulerDbKey)));
	ret=versionDb->put(0, &key, &data, 0);
	if (ret != 0)
	{
		qDebug() << "Error putting scheduler db version: " <<  g_dbenv->strerror(ret);
	}

	data.set_data((void*)&versionDbVersion);
	data.set_size(sizeof(quint32));
	key.set_data((void*)versionDbKey);
	key.set_size(quint32(strlen(versionDbKey)));
	ret=versionDb->put(0, &key, &data, 0);
	if (ret != 0)
	{
		qDebug() << "Error putting version db version: " <<  g_dbenv->strerror(ret);
	}

	config->groupDbVersion=groupDbVersion;
	config->headerDbVersion=headerDbVersion;
	config->availableGroupsDbVersion = availableGroupsDbVersion;
	config->serverDbVersion = serverDbVersion;
	config->queueDbVersion = queueDbVersion;
	config->unpackDbVersion = unpackDbVersion;
	config->schedulerDbVersion = schedulerDbVersion;
	config->versionDbVersion = versionDbVersion;

	versionDb->sync(0);

	return;
}

void QubanDbEnv::slotCompactDbs()
{
	switch (QMessageBox::question(quban, tr("Do you want to compact the databases?"), tr("Compacting the newsgroups databases can be very lengthy.\nAll the views will be closed. Are you sure you want to continue?"),
			QMessageBox::Yes, QMessageBox::No))
	{
	    case QMessageBox::No:
			return;
			break;
	}
	if (!(quban->getQMgr()->empty()))
	{
		QMessageBox::warning(quban, tr("Error!"), tr("Can compact databases only when the download queue is empty.\n\nAbandoning Compact."));
		return;
	}

	//Close all the Windows (Except the qmgr, of course)
// TODO	closeAllViews();
// TODO	enable(false);

	emit sigCompact(); // TODO MD why do it in grouplist ???
// TODO	enable(true);
}

void QubanDbEnv::migrateDbs()
{
    int ret,
        retCode = 0;
	Dbt verKey, verData;
	const char *groupDbKey="GROUPDB";
	const char *headerDbKey="HEADERDB";
	const char *availableGroupsDbKey="AVAILABLEGROUPSDB";
	const char *serverDbKey="SERVERDB";
	const char *schedulerDbKey="SCHEDULERDB";
	const char *queueDbKey="QUEUEDB";
	const char *unpackDbKey="UNPACKDB";
    const char *versionDbKey="VERSIONDB";

	// Initialise all to backup values
	groupDbVersion = config->groupDbVersion;
	headerDbVersion = config->headerDbVersion;
	availableGroupsDbVersion = config->availableGroupsDbVersion;
	serverDbVersion = config->serverDbVersion;
	queueDbVersion = config->queueDbVersion;
	unpackDbVersion = config->unpackDbVersion;
	schedulerDbVersion = config->schedulerDbVersion;
	versionDbVersion = config->versionDbVersion;

	//First, open the version db...
    Db *versionDb;

	if (!(versionDb = openDb(VERSIONDB_NAME, false, &ret))) // Try and open without creating first
	{
 		if (ret == ENOENT) // database does not exist ... try and create
		{
 			// Try and create ...
 			if (!(versionDb = openDb(VERSIONDB_NAME, true, &ret)))
			{
				if (ret == ENOENT) // dir does not exist
				{
					if (checkAndCreateDir(config->dbDir))
					{
				 		// Try and create again ...
						if (!(versionDb = openDb(VERSIONDB_NAME, true, &ret)))
						{
							QMessageBox::critical(0, tr("Fatal error"), tr("Sorry, your database directory does not exist and quban is unable to create it, exiting ..."));
							QApplication::exit(-5);
						}
					}
					else
					{
						QMessageBox::critical(0, tr("Fatal error"), tr("Sorry, your database directory does not exist and quban is unable to create it, exiting ..."));
						QApplication::exit(-5);
					}
				}
				else
				{
					qDebug() << "Error creating version db : Continuing with backup values only ... : " <<  ret << ", " << g_dbenv->strerror(ret);
					return;
				}
			}
		}
 		else
 		{
			qDebug() << "Error opening version db : Continuing with backup values only ... : " <<  g_dbenv->strerror(ret);
			return;
 		}
	}
	else
	{
        verData.set_data((void*)&versionDbVersion);
        verData.set_ulen(sizeof(quint32));
        verData.set_flags(DB_DBT_USERMEM);

        //Check & migrate the groupdb...
        verKey.set_data((void *)versionDbKey);
        verKey.set_size(quint32(strlen(versionDbKey)));
        ret=versionDb->get(0, &verKey, &verData, 0);

        if (ret != 0)
        {
            qDebug() << "Error retrieving version db version: " <<  g_dbenv->strerror(ret);
        }

        if (versionDbVersion != VERSIONDB_VERSION)
        {
            QMessageBox::critical(0, tr("Fatal error"), tr("Sorry, your Version db is unsupported by this version of quban (you need to migrate the db using version 0.6.1.5 of quban)"));
            QApplication::exit(-2);
        }

		//Keys, data have fixed size:
		//DATA is a quint32
        //KEY is one of GROUPDB, AVAILABLEGROUPSDB, HEADERDB, SERVERDB, NZBDB, QUEUEDB, UNPACKDB, SCHEDULERDB, VERSIONDB

		// **************** Group Db **********************

		verData.set_data((void*)&groupDbVersion);
		verData.set_ulen(sizeof(quint32));
		verData.set_flags(DB_DBT_USERMEM);

		//Check & migrate the groupdb...
		verKey.set_data((void *)groupDbKey);
		verKey.set_size(quint32(strlen(groupDbKey)));
		ret=versionDb->get(0, &verKey, &verData, 0);

		if (ret != 0)
		{
			qDebug() << "Error retrieving group version: " <<  g_dbenv->strerror(ret);
		}

		if (groupDbVersion > GROUPDB_VERSION)
		{
			//Trying to open a newer db...exit NOW
			QMessageBox::critical(0, tr("Fatal error"), tr("Sorry, your Group db is unsupported by this version of quban (you probably already migrated the db with a newer version of quban)"));
			QApplication::exit(-2);
		}

		// **************** Header Db **********************

		verData.set_data((void*)&headerDbVersion);

		verKey.set_data((void*)headerDbKey);
		verKey.set_size(quint32(strlen(headerDbKey)));
		ret=versionDb->get(0, &verKey, &verData, 0);

		if (ret != 0)
		{
			qDebug() << "Error retrieving header version: " <<  g_dbenv->strerror(ret);
		}

		if (HEADERDB_VERSION < headerDbVersion)
		{
			QMessageBox::critical(0, tr("Fatal error"), tr("Sorry, your Header db is unsupported by this version of quban (you probably already migrated the db with a newer version of quban)"));
			QApplication::exit(-2);
		}

		// **************** Available Groups Db **********************

		verData.set_data((void*)&availableGroupsDbVersion);

		verKey.set_data((void*)availableGroupsDbKey);
		verKey.set_size(quint32(strlen(availableGroupsDbKey)));
		ret=versionDb->get(0, &verKey, &verData, 0);

		if (ret != 0)
		{
			qDebug() << "Error retrieving Available Groups version: " <<  g_dbenv->strerror(ret);
		}

		if (AVAILABLEGROUPSDB_VERSION < availableGroupsDbVersion)
		{
			QMessageBox::critical(0, tr("Fatal error"), tr("Sorry, your Available Groups db is unsupported by this version of quban (you probably already migrated the db with a newer version of quban)"));
			QApplication::exit(-2);
		}

		// **************** Server Db **********************

		verData.set_data((void*)&serverDbVersion);

		verKey.set_data((void*)serverDbKey);
		verKey.set_size(quint32(strlen(serverDbKey)));
		ret=versionDb->get(0, &verKey, &verData, 0);

		if (ret != 0)
		{
			qDebug() << "Error retrieving Servers version: " <<  g_dbenv->strerror(ret);
		}

		if (SERVERDB_VERSION < serverDbVersion)
		{
			QMessageBox::critical(0, tr("Fatal error"), tr("Sorry, your Server db is unsupported by this version of quban (you probably already migrated the db with a newer version of quban)"));
			QApplication::exit(-2);
		}

		// **************** Scheduler Db **********************

		verData.set_data((void*)&schedulerDbVersion);

		verKey.set_data((void*)schedulerDbKey);
		verKey.set_size(quint32(strlen(schedulerDbKey)));
		ret=versionDb->get(0, &verKey, &verData, 0);

		if (ret != 0)
		{
			qDebug() << "Error retrieving Scheduler version: " <<  g_dbenv->strerror(ret);
		}

		if (SCHEDULERDB_VERSION < schedulerDbVersion)
		{
			QMessageBox::critical(0, tr("Fatal error"), tr("Sorry, your Scheduler db is unsupported by this version of quban (you probably already migrated the db with a newer version of quban)"));
			QApplication::exit(-2);
		}

		// **************** Queue Db **********************

		verData.set_data((void*)&queueDbVersion);

		verKey.set_data((void*)queueDbKey);
		verKey.set_size(quint32(strlen(queueDbKey)));
		ret=versionDb->get(0, &verKey, &verData, 0);

		if (ret != 0)
		{
			qDebug() << "Error retrieving Queue version: " <<  g_dbenv->strerror(ret);
		}

		if (QUEUEDB_VERSION < queueDbVersion)
		{
			QMessageBox::critical(0, tr("Fatal error"), tr("Sorry, your Queue db is unsupported by this version of quban (you probably already migrated the db with a newer version of quban)"));
			QApplication::exit(-2);
		}

		// **************** Unpack Db **********************

		verData.set_data((void*)&unpackDbVersion);

		verKey.set_data((void*)unpackDbKey);
		verKey.set_size(quint32(strlen(unpackDbKey)));
		ret=versionDb->get(0, &verKey, &verData, 0);

		if (ret != 0)
		{
			qDebug() << "Error retrieving Unpack version: " <<  g_dbenv->strerror(ret);
		}

		if (UNPACKDB_VERSION < unpackDbVersion)
		{
			QMessageBox::critical(0, tr("Fatal error"), tr("Sorry, your Unpack db is unsupported by this version of quban (you probably already migrated the db with a newer version of quban)"));
			QApplication::exit(-2);
		}
	}

    // **********************************  Looks ok so far ... no db versions are too new *********************************

	if (availableGroupsDbVersion < AVAILABLEGROUPSDB_VERSION)
	{
        QMessageBox::critical(0, tr("Fatal error"), tr("Sorry, your Available Groups db is unsupported by this version of quban (you need to migrate the db using version 0.6.1.5 of quban)"));
        QApplication::exit(-3);
    }

	/*********************
	 *  Server Database
	 *********************/

	if (serverDbVersion < SERVERDB_VERSION)
	{
        QMessageBox::critical(0, tr("Fatal error"), tr("Sorry, your Server db is unsupported by this version of quban (you need to migrate the db using version 0.6.2 of quban)"));
        QApplication::exit(-3);
	}

	/*********************
	 *  Scheduler Database
	 *********************/

	if (schedulerDbVersion < SCHEDULERDB_VERSION)
    {
        QMessageBox::critical(0, tr("Fatal error"), tr("Sorry, your Scheduler db is unsupported by this version of quban (you need to migrate the db using version 0.6.1.5 of quban)"));
        QApplication::exit(-3);
    }

	/*********************
	 *  Queue Database
	 *********************/

	if (queueDbVersion < QUEUEDB_VERSION)
    {
        QMessageBox::critical(0, tr("Fatal error"), tr("Sorry, your Queue db is unsupported by this version of quban (you need to migrate the db using version 0.6.1.5 of quban)"));
        QApplication::exit(-3);
    }

	/*********************
	 *  Unpack Database - (pending only)
	 *********************/

	if (unpackDbVersion < UNPACKDB_VERSION)
    {
        QMessageBox::critical(0, tr("Fatal error"), tr("Sorry, your Unpack db is unsupported by this version of quban (you need to migrate the db using version 0.6.1.5 of quban)"));
        QApplication::exit(-3);
    }

	/*********************
	 *  Group Database
	 *********************/
/*
	if (groupDbVersion < GROUPDB_VERSION)
    {
        QMessageBox::critical(0, tr("Fatal error"), tr("Sorry, your Newsgroup db is unsupported by this version of quban (you need to migrate the db using version 0.6.1.5 of quban)"));
        QApplication::exit(-3);
    }
*/

    if (groupDbVersion < GROUPDB_VERSION)
    {
        //Migrate the db!

        qDebug() << "Need group db migration from " << groupDbVersion << " to " << GROUPDB_VERSION;
        Db* groupDb;

        if (!(groupDb = openDb(SUBSGROUPSDB_NAME, false, &ret)))
        {
            qDebug() << "Error opening groupdb! : " <<  g_dbenv->strerror(ret);
            // TODO: So we know that we need a migration but failed to open the newsgroup db
            return;
        }

        //Now, for all the groups...

        Dbc *cursor;

        char datamem[DATAMEM_SIZE];
        char keymem[KEYMEM_SIZE];

        Dbt key, data;
        key.set_flags(DB_DBT_USERMEM);
        key.set_ulen(KEYMEM_SIZE);
        key.set_data(keymem);

        data.set_flags(DB_DBT_USERMEM);
        data.set_ulen(DATAMEM_SIZE);
        data.set_data(datamem);

        groupDb->cursor(0, &cursor, DB_WRITECURSOR);

// 		qDebug("Starting...");
        while ((retCode = cursor->get(&key, &data, DB_NEXT)) == 0)
        {
            //Load the old group...
            qDebug("read %d bytes", data.get_size());
            NewsGroup* tempg = new NewsGroup(g_dbenv, datamem, groupDbVersion);

            //For every newsgroup...

            switch (groupDbVersion)
            {
                case 8:

                    tempg->setTotalGroups(0);
                    tempg->setGroupRE1("-.*$");
                    tempg->setGroupRE2("");
                    tempg->setGroupRE3("");
                    tempg->setGroupRE1Back(true);
                    tempg->setGroupRE2Back(true);
                    tempg->setGroupRE3Back(true);
                    tempg->setMatchDistance(12);
                    break;

                default:
                    qDebug() << "Something very very wrong happened with Db versions!";
                    break;
            }

            //Now save the group...
            (void)tempg->data(datamem); // read the data directly into datamem
            data.set_size(tempg->getRecordSize());
            //qDebug("About to save group");
            //qDebug("New group size: %d", tempg->getRecordSize());
            ret=cursor->put(&key,&data,DB_CURRENT);
            if (ret != 0)
                qDebug("Return from cursor put: %s", g_dbenv->strerror(ret));

            delete tempg;
            qDebug("End of cycle");
        }

        cursor->close();
        groupDb->close(0);
        delete groupDb;

        groupDbVersion = GROUPDB_VERSION;
        saveVersionData(versionDb);
        quban->saveConfig();
    }

    // **************** Header Db **********************

	if (headerDbVersion < HEADERDB_VERSION)
    {
        QMessageBox::critical(0, tr("Fatal error"), tr("Sorry, your Header db is unsupported by this version of quban (you need to migrate the db using version 0.6.1.5 of quban)"));
        QApplication::exit(-3);
    }


	//   ********* Save the version Db... ****************

	saveVersionData(versionDb);

	versionDb->close(0);
	delete versionDb;
}

