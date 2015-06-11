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

#include <stdlib.h>
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QtDebug>
#include <QtAlgorithms>
#include <QApplication>
#include "common.h"
#include "appConfig.h"

Configuration::Configuration( )
{
	conf = new QSettings("quban", "quban");
	read();
}

Configuration::~Configuration( )
{
	qDeleteAll(repairAppsList);
	repairAppsList.clear();

	qDeleteAll(unpackAppsList);
	unpackAppsList.clear();
}

Configuration* Configuration::getConfig()
{
static Configuration onlyManager;

    return &onlyManager;
}

void Configuration::read( )
{
	conf->beginGroup("AppGeneral");
	configured=conf->value("configured", false).toBool();
	dbDir=conf->value("dbdir", QDir::homePath() + "/quban/db").toString();
	decodeDir=conf->value("decodedir", QDir::homePath() + "/quban/decoded").toString();
	nzbDir=conf->value("nzbDir", QDir::homePath() + "/quban/nzb").toString();
	tmpDir=conf->value("tmpDir", QDir::homePath() + "/quban/tmp").toString();
	maxShutDownWait=conf->value("maxShutDownWait", 15).toInt();
	renameNzbFiles=conf->value("renameNzbFiles", true).toBool();
	noQueueSavePrompt=conf->value("noQueueSavePrompt", true).toBool();
	appFont.fromString(conf->value("appFont", QApplication::font()).toString());
	QApplication::setFont(appFont);
    minAvailEnabled=conf->value("minAvailEnabled", false).toBool();
    minAvailValue=conf->value("minAvailValue", 100).toInt();
    mainTab=conf->value("mainTab", 0).toInt();
    jobTab=conf->value("jobTab", 0).toInt();
	conf->endGroup();

	conf->beginGroup("Decode");
	overwriteExisting=conf->value("overwriteExisting", false).toBool();
	deleteFailed=conf->value("deleteFailed", false).toBool();
	conf->endGroup();

	conf->beginGroup("View");
	activateTab=conf->value("activateTab", true).toBool();
	singleViewTab=conf->value("singleView", false).toBool();
	textSuffixes=conf->value("textFileSuffixes", "nzb sfv nfo m3u txt").toString();
	showStatusBar=conf->value("showStatusBar", true).toBool();
	showQueueBar=conf->value("showQueueBar", true).toBool();
	showServerBar=conf->value("showServerBar", true).toBool();
	showGroupBar=conf->value("showGroupBar", true).toBool();
	showHeaderBar=conf->value("showHeaderBar", true).toBool();
	showJobBar=conf->value("showJobBar", true).toBool();
	dViewed=conf->value("deleteViewed", Configuration::Ask).toInt();
	conf->endGroup();

	conf->beginGroup("HeaderList");
	showFrom = conf->value("showfrom", false).toBool();
	showDetails = conf->value("showdetails", true).toBool();
	showDate = conf->value("showdate", true).toBool();
	markOpt = conf->value("headerclose", Configuration::Ask).toInt();
	ignoreNoPartArticles = conf->value("ignorenopartarticles", false).toBool();
	alwaysDoubleClick = conf->value("alwaysdoubleclick", false).toBool();
	rememberSort = conf->value("remembersort", false).toBool();
	rememberWidth = conf->value("rememberwidth", false).toBool();
	downloadCompressed = conf->value("downloadcompressed", true).toBool();
	checkDaysValue = conf->value("checkDaysValue", 7).toInt();
    bulkLoadThreshold = conf->value("bulkLoadThreshold", 250000).toInt();
    bulkDeleteThreshold = conf->value("bulkDeleteThreshold", 26).toInt();
    groupCheckCount = conf->value("groupCheckCount", 1000).toInt();
	conf->endGroup();

	conf->beginGroup("DbVersions");
	groupDbVersion=conf->value("groupDbVersion", GROUPDB_VERSION).toInt();
	headerDbVersion=conf->value("headerDbVersion", HEADERDB_VERSION).toInt();
	availableGroupsDbVersion=conf->value("availableGroupsDbVersion", AVAILABLEGROUPSDB_VERSION).toInt();
	serverDbVersion=conf->value("serverDbVersion", SERVERDB_VERSION).toInt();
	queueDbVersion=conf->value("queueDbVersion", QUEUEDB_VERSION).toInt();
	unpackDbVersion=conf->value("unpackDbVersion", UNPACKDB_VERSION).toInt();
	schedulerDbVersion=conf->value("schedulerDbVersion", SCHEDULERDB_VERSION).toInt();
	versionDbVersion=conf->value("versionDbVersion", VERSIONDB_VERSION).toInt();
	conf->endGroup();

	conf->beginGroup("Unpack");
	autoUnpack = conf->value("autounpack", true).toBool();
	maxAppId = conf->value("maxappid", 4).toInt();
	otherFileTypes = conf->value("otherfiletypes", "nzb sfv nfo").toString();
	numRepairers=conf->value("numrepairers", 1).toInt();
	numUnpackers=conf->value("numunpackers", 3).toInt();
	deleteGroupFiles = conf->value("deletegroupfiles", false).toBool();
	deleteRepairFiles = conf->value("deleterepairfiles", true).toBool();
	deleteCompressedFiles = conf->value("deletecompressedfiles", true).toBool();
	deleteOtherFiles = conf->value("deleteotherfiles", false).toBool();
	merge001Files = conf->value("merge001files", true).toBool();
	dontShowTypeConfirmUnlessNeeded = conf->value("dontshowconfirmdialogifallok", false).toBool();

	if (numRepairers > 0)
	{
		t_ExternalApp* repairExternalApp;

		for (int i=0; i<numRepairers; i++)
		{
			repairExternalApp = new t_ExternalApp;
			repairAppsList.append(repairExternalApp);

		    conf->beginGroup("RepairApp" + QString::number(i+1));

		    repairExternalApp->enabled = conf->value("enabled").toBool();
		    repairExternalApp->appId = conf->value("appid", 0).toInt();
		    repairExternalApp->appType = conf->value("apptype", 1).toInt();
		    repairExternalApp->suffixes = conf->value("suffixes").toString();
		    repairExternalApp->reSuffixes = conf->value("resuffixes").toString();
		    repairExternalApp->fullFilePath = conf->value("fullfilepath").toString();
		    repairExternalApp->filePathValid = conf->value("filepathvalid").toBool();
			repairExternalApp->parameters = conf->value("parameters").toString();
			repairExternalApp->priority = conf->value("priority").toInt();

		    conf->endGroup();
		}
	}

	if (numUnpackers > 0)
	{
		t_ExternalApp* unpackExternalApp;

		for (int i=0; i<numUnpackers; i++)
		{
			unpackExternalApp = new t_ExternalApp;
			unpackAppsList.append(unpackExternalApp);

		    conf->beginGroup("UnpackApp" + QString::number(i+1));

		    unpackExternalApp->enabled = conf->value("enabled").toBool();
		    unpackExternalApp->appId = conf->value("appid", 0).toInt();
		    unpackExternalApp->appType = conf->value("apptype", 3).toInt();
		    unpackExternalApp->suffixes = conf->value("suffixes").toString();
		    unpackExternalApp->reSuffixes = conf->value("resuffixes").toString();
		    unpackExternalApp->fullFilePath = conf->value("fullfilepath").toString();
		    unpackExternalApp->filePathValid = conf->value("filepathvalid").toBool();
		    unpackExternalApp->parameters = conf->value("parameters").toString();
		    unpackExternalApp->priority = conf->value("priority").toInt();

		    conf->endGroup();
		}
	}

	conf->endGroup();
}

void Configuration::write( ) const
{
	conf->beginGroup("AppGeneral");
	conf->setValue("configured", configured);
	conf->setValue("dbdir", dbDir);
	conf->setValue("decodedir", decodeDir);
	conf->setValue("nzbDir", nzbDir);
	conf->setValue("tmpDir", tmpDir);
	conf->setValue("maxShutDownWait", maxShutDownWait);
	conf->setValue("renameNzbFiles", renameNzbFiles);
	conf->setValue("noQueueSavePrompt", noQueueSavePrompt);
        conf->setValue("appFont", appFont.toString());
    conf->setValue("minAvailEnabled", minAvailEnabled);
    conf->setValue("minAvailValue", minAvailValue);
    conf->setValue("mainTab", mainTab);
    conf->setValue("jobTab", jobTab);
	conf->endGroup();

	conf->beginGroup("Decode");
	conf->setValue("overwriteExisting", overwriteExisting);
	conf->setValue("deleteFailed", deleteFailed);
	conf->endGroup();

	conf->beginGroup("View");
	conf->setValue("activateTab", activateTab);
	conf->setValue("singleView", singleViewTab);
	conf->setValue("textFileSuffixes", textSuffixes);
	conf->setValue("showStatusBar", showStatusBar);
	conf->setValue("showQueueBar", showQueueBar);
	conf->setValue("showServerBar", showServerBar);
	conf->setValue("showGroupBar", showGroupBar);
	conf->setValue("showHeaderBar", showHeaderBar);
	conf->setValue("showJobBar", showJobBar);
	conf->setValue("deleteViewed", dViewed);
	conf->endGroup();

	conf->beginGroup("HeaderList");
	conf->setValue("showfrom", showFrom);
	conf->setValue("showdetails", showDetails);
	conf->setValue("showdate", showDate);
	conf->setValue("ignorenopartarticles", ignoreNoPartArticles);
	conf->setValue("remembersort", rememberSort);
	conf->setValue("rememberwidth", rememberWidth);
	conf->setValue("downloadcompressed", downloadCompressed);
	conf->setValue("alwaysdoubleclick", alwaysDoubleClick);
	conf->setValue("headerclose", markOpt);
	conf->setValue("checkDaysValue", checkDaysValue);
    conf->setValue("bulkLoadThreshold", bulkLoadThreshold);
    conf->setValue("bulkDeleteThreshold", bulkDeleteThreshold);
    conf->setValue("groupCheckCount", groupCheckCount);
	conf->endGroup();

	conf->beginGroup("DbVersions");
	conf->setValue("groupDbVersion", groupDbVersion);
	conf->setValue("headerDbVersion", headerDbVersion);
	conf->setValue("availableGroupsDbVersion", availableGroupsDbVersion);
	conf->setValue("serverDbVersion", serverDbVersion);
	conf->setValue("queueDbVersion", queueDbVersion);
	conf->setValue("unpackDbVersion", unpackDbVersion);
	conf->setValue("schedulerDbVersion", schedulerDbVersion);
	conf->setValue("versionDbVersion", versionDbVersion);
	conf->endGroup();

	conf->beginGroup("Unpack");
	conf->setValue("autounpack", autoUnpack);
	conf->setValue("maxappid", maxAppId);
	conf->setValue("otherfiletypes", otherFileTypes);
	conf->setValue("numrepairers", numRepairers);
	conf->setValue("numunpackers", numUnpackers);
	conf->setValue("deletegroupfiles", deleteGroupFiles);
	conf->setValue("deleterepairfiles", deleteRepairFiles);
	conf->setValue("deletecompressedfiles", deleteCompressedFiles);
	conf->setValue("deleteotherfiles", deleteOtherFiles);
	conf->setValue("merge001files", merge001Files);
	conf->setValue("dontshowconfirmdialogifallok", dontShowTypeConfirmUnlessNeeded);

	if (numRepairers > 0)
	{
		for (int i=0; i<numRepairers; i++)
		{
		    conf->beginGroup("RepairApp" + QString::number(i+1));

		    conf->setValue("enabled", repairAppsList.at(i)->enabled);
		    conf->setValue("appid", repairAppsList.at(i)->appId);
		    conf->setValue("apptype", repairAppsList.at(i)->appType);
		    conf->setValue("suffixes", repairAppsList.at(i)->suffixes);
		    conf->setValue("resuffixes", repairAppsList.at(i)->reSuffixes);
		    conf->setValue("fullfilepath", repairAppsList.at(i)->fullFilePath);
		    conf->setValue("filepathvalid", repairAppsList.at(i)->filePathValid);
		    conf->setValue("parameters", repairAppsList.at(i)->parameters);
		    conf->setValue("priority", repairAppsList.at(i)->priority);

		    conf->endGroup();
		}
	}

	if (numUnpackers > 0)
	{
		for (int i=0; i<numUnpackers; i++)
		{
		    conf->beginGroup("UnpackApp" + QString::number(i+1));

		    conf->setValue("enabled", unpackAppsList.at(i)->enabled);
		    conf->setValue("appid", unpackAppsList.at(i)->appId);
		    conf->setValue("apptype", unpackAppsList.at(i)->appType);
		    conf->setValue("suffixes", unpackAppsList.at(i)->suffixes);
		    conf->setValue("resuffixes", unpackAppsList.at(i)->reSuffixes);
		    conf->setValue("fullfilepath", unpackAppsList.at(i)->fullFilePath);
		    conf->setValue("filepathvalid", unpackAppsList.at(i)->filePathValid);
		    conf->setValue("parameters", unpackAppsList.at(i)->parameters);
		    conf->setValue("priority", unpackAppsList.at(i)->priority);

		    conf->endGroup();
		}
	}

	conf->endGroup();
}
