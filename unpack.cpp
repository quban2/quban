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

#include <QFile>
#include <QDebug>
#include <QtDebug>
#include <QMetaType>
#include <QStringBuilder>
#include "qmgr.h"
#include "newsgroup.h"
#include "grouplist.h"
#include "appConfig.h"
#include "unpack.h"

GroupManager::GroupManager(Db *_db, NewsGroup* _ng, quint16 groupId) : db(_db)
{
	workingThread = 0;

	usedAllDownloadedRepairFiles = false;
	repairNext = false;

	downloadGroup = new t_DownloadGroup;

	downloadGroup->groupId = groupId;
	downloadGroup->groupStatus = QUBAN_GROUP_INIT;

	downloadGroup->unpackFailed = 0;
	downloadGroup->numBlocksNeeded = 0;
	downloadGroup->repairId = 0;
	downloadGroup->packId = 0;
	downloadGroup->unused1 = 0;
	downloadGroup->unused2 = 0;
	downloadGroup->ng = _ng;

	downloadGroup->compressedFiles.numFiles = 0;
	downloadGroup->compressedFiles.numCancelled = 0;
	downloadGroup->compressedFiles.succDecoded = 0;
	downloadGroup->compressedFiles.failedDecode = 0;
	downloadGroup->compressedFiles.failedDecodeParts = 0;
	downloadGroup->compressedFiles.failedUnpack = 0;
	downloadGroup->compressedFiles.unpackPriority = 3;
	downloadGroup->compressedFiles.unused2 = 0;

	downloadGroup->repairFiles.numFiles = 0;
	downloadGroup->repairFiles.numCancelled = 0;
	downloadGroup->repairFiles.numReleased = 0;
	downloadGroup->repairFiles.numBlocksReleased = 0;
	downloadGroup->repairFiles.succDecoded = 0;
	downloadGroup->repairFiles.failedDecode = 0;
	downloadGroup->repairFiles.failedDecodeParts = 0;
	downloadGroup->repairFiles.repairPriority = 3;
	downloadGroup->repairFiles.numBlocks = 0;

	downloadGroup->otherFiles.numFiles = 0;
	downloadGroup->otherFiles.numCancelled = 0;
	downloadGroup->otherFiles.numReleased = 0;
	downloadGroup->otherFiles.succDecoded = 0;
	downloadGroup->otherFiles.failedDecode = 0;
	downloadGroup->otherFiles.failedDecodeParts = 0;
	downloadGroup->otherFiles.unused1 = 0;
	downloadGroup->otherFiles.unused2 = 0;

	groupStatusStrings << "" << tr("Initialising") << tr("Downloading Compressed Files") << tr("Downloading Repair Files") <<
			tr("Repairing") << tr("Unpacking") << tr("Succeeded") << tr("Failed") << tr("Cancelled");
}

GroupManager::GroupManager(Db *_db, GroupList *gList, uchar* dbData, QMgr* qmgr) : db(_db)
{
	workingThread = 0;

	usedAllDownloadedRepairFiles = false;
	repairNext = false;

	downloadGroup = new t_DownloadGroup;

	downloadGroup->numBlocksNeeded = 0;

	uint strSize = 0;
	char *temp;
	int szQuint16 = sizeof(quint16);
	int szQuint32 = sizeof(quint32);
	uchar *i = dbData;

	memcpy(&(downloadGroup->groupId), i, szQuint16);
	i += szQuint16;

	memcpy(&(downloadGroup->groupStatus), i, szQuint32);
	i += szQuint32;

	memcpy(&(downloadGroup->unpackFailed) ,i, szQuint32);
	i += szQuint32;

	memcpy(&(downloadGroup->repairId), i, szQuint32);
	i += szQuint32;

	memcpy(&(downloadGroup->packId), i, szQuint32);
	i += szQuint32;

	memcpy(&(downloadGroup->unused1), i, szQuint32);
	i += szQuint32;

	memcpy(&(downloadGroup->unused2), i, szQuint32);
	i += szQuint32;

	memcpy(&strSize, i, sizeof(uint));
	i += sizeof(uint);

	temp = new char[strSize + 1];
	memcpy(temp, i, strSize);
	temp[strSize] = '\0';
	downloadGroup->dirPath = temp;
	delete[] temp;
	i += strSize;

	memcpy(&strSize, i, sizeof(uint));
	i += sizeof(uint);

	temp = new char[strSize + 1];
	memcpy(temp, i, strSize);
	temp[strSize] = '\0';
	downloadGroup->masterRepairFile = temp;
	delete[] temp;
	i += strSize;

	memcpy(&strSize, i, sizeof(uint));
	i += sizeof(uint);

	temp = new char[strSize + 1];
	memcpy(temp, i, strSize);
	temp[strSize] = '\0';
	downloadGroup->masterUnpackFile = temp;
	delete[] temp;
	i += strSize;

	qDebug() << "Restored Group record with master unpack file: " << downloadGroup->masterUnpackFile;

	memcpy(&strSize, i, sizeof(uint));
	i += sizeof(uint);

	temp = new char[strSize + 1];
	memcpy(temp, i, strSize);
	temp[strSize] = '\0';
	downloadGroup->processLog = temp;
	delete[] temp;
	i += strSize;

	memcpy(&strSize, i, sizeof(uint));
	i += sizeof(uint);

	temp = new char[strSize + 1];
	memcpy(temp, i, strSize);
	temp[strSize] = '\0';

	if (QString(temp) == "nzb")
		downloadGroup->ng = qmgr->nzbGroup;
	else
	    downloadGroup->ng = gList->getNg(QString(temp));

	delete[] temp;
	i += strSize;

// Add all of the sub structs

	memcpy(&(downloadGroup->compressedFiles.numFiles), i, szQuint32);
	i += szQuint32;

	memcpy(&(downloadGroup->compressedFiles.numCancelled), i, szQuint32);
	i += szQuint32;

	memcpy(&(downloadGroup->compressedFiles.succDecoded), i, szQuint32);
	i += szQuint32;

	memcpy(&(downloadGroup->compressedFiles.failedDecode), i, szQuint32);
	i += szQuint32;

	memcpy(&(downloadGroup->compressedFiles.failedDecodeParts), i, szQuint32);
	i += szQuint32;

	memcpy(&(downloadGroup->compressedFiles.failedUnpack), i, szQuint32);
	i += szQuint32;

	memcpy(&(downloadGroup->compressedFiles.unpackPriority), i, szQuint32);
	i += szQuint32;

	memcpy(&(downloadGroup->compressedFiles.unused2), i, szQuint32);
	i += szQuint32;

	memcpy(&(downloadGroup->repairFiles.numFiles), i, szQuint32);
	i += szQuint32;

	memcpy(&(downloadGroup->repairFiles.numCancelled), i, szQuint32);
	i += szQuint32;

	memcpy(&(downloadGroup->repairFiles.numReleased), i, szQuint32);
	i += szQuint32;

	memcpy(&(downloadGroup->repairFiles.numBlocksReleased), i, szQuint32);
	i += szQuint32;

	memcpy(&(downloadGroup->repairFiles.succDecoded), i, szQuint32);
	i += szQuint32;

	memcpy(&(downloadGroup->repairFiles.failedDecode), i, szQuint32);
	i += szQuint32;

	memcpy(&(downloadGroup->repairFiles.failedDecodeParts), i, szQuint32);
	i += szQuint32;

	memcpy(&(downloadGroup->repairFiles.repairPriority), i, szQuint32);
	i += szQuint32;

	memcpy(&(downloadGroup->repairFiles.numBlocks), i, szQuint32);
	i += szQuint32;

	memcpy(&(downloadGroup->otherFiles.numFiles), i, szQuint32);
	i += szQuint32;

	memcpy(&(downloadGroup->otherFiles.numCancelled), i, szQuint32);
	i += szQuint32;

	memcpy(&(downloadGroup->otherFiles.numReleased), i, szQuint32);
	i += szQuint32;

	memcpy(&(downloadGroup->otherFiles.succDecoded), i, szQuint32);
	i += szQuint32;

	memcpy(&(downloadGroup->otherFiles.failedDecode), i, szQuint32);
	i += szQuint32;

	memcpy(&(downloadGroup->otherFiles.failedDecodeParts), i, szQuint32);
	i += szQuint32;

	memcpy(&(downloadGroup->otherFiles.unused1), i, szQuint32);
	i += szQuint32;

	memcpy(&(downloadGroup->otherFiles.unused2), i, szQuint32);
	i += szQuint32;

	groupStatusStrings << "" << tr("Initialising") << tr("Downloading Compressed Files") << tr("Downloading Repair Files") <<
			tr("Repairing") << tr("Unpacking") << tr("Succeeded") << tr("Failed") << tr("Cancelled");
}

GroupManager::~GroupManager()
{
    delete downloadGroup;
}

bool GroupManager::dbSave()
{
	Dbt key, data;

	uint strSize;
	QByteArray ba;
	const char *c_str;

	char *p = new char[saveItemSize()];
	char *i = p;

	int szQuint16 = sizeof(quint16);
	int szQuint32 = sizeof(quint32);

	memcpy(i, &(downloadGroup->groupId), szQuint16);
	i += szQuint16;

	memcpy(i, &(downloadGroup->groupStatus), szQuint32);
	i += szQuint32;

	memcpy(i, &(downloadGroup->unpackFailed), szQuint32);
	i += szQuint32;

	memcpy(i, &(downloadGroup->repairId), szQuint32);
	i += szQuint32;

	memcpy(i, &(downloadGroup->packId), szQuint32);
	i += szQuint32;

	memcpy(i, &(downloadGroup->unused1), szQuint32);
	i += szQuint32;

	memcpy(i, &(downloadGroup->unused2), szQuint32);
	i += szQuint32;

	strSize = downloadGroup->dirPath.size();
	memcpy(i, &strSize, sizeof(uint));
	i += sizeof(uint);

	ba = downloadGroup->dirPath.toLocal8Bit();
	c_str = ba.data();
	memcpy(i, c_str, strSize);
	i += strSize;

	strSize = downloadGroup->masterRepairFile.size();
	memcpy(i, &strSize, sizeof(uint));
	i += sizeof(uint);

	ba = downloadGroup->masterRepairFile.toLocal8Bit();
	c_str = ba.data();
	memcpy(i, c_str, strSize);
	i += strSize;

	strSize = downloadGroup->masterUnpackFile.size();
	memcpy(i, &strSize, sizeof(uint));
	i += sizeof(uint);

	ba = downloadGroup->masterUnpackFile.toLocal8Bit();
	c_str = ba.data();
	memcpy(i, c_str, strSize);
	i += strSize;

	strSize = downloadGroup->processLog.size();
	memcpy(i, &strSize, sizeof(uint));
	i += sizeof(uint);

	ba = downloadGroup->processLog.toLocal8Bit();
	c_str = ba.data();
	memcpy(i, c_str, strSize);
	i += strSize;

	strSize = downloadGroup->ng->getName().size();
	memcpy(i, &strSize, sizeof(uint));
	i += sizeof(uint);

	ba = downloadGroup->ng->getName().toLocal8Bit();
	c_str = ba.data();
	memcpy(i, c_str, strSize);
	i += strSize;

// Add all of the sub structs

	memcpy(i, &(downloadGroup->compressedFiles.numFiles), szQuint32);
	i += szQuint32;

	memcpy(i, &(downloadGroup->compressedFiles.numCancelled), szQuint32);
	i += szQuint32;

	memcpy(i, &(downloadGroup->compressedFiles.succDecoded), szQuint32);
	i += szQuint32;

	memcpy(i, &(downloadGroup->compressedFiles.failedDecode), szQuint32);
	i += szQuint32;

	memcpy(i, &(downloadGroup->compressedFiles.failedDecodeParts), szQuint32);
	i += szQuint32;

	memcpy(i, &(downloadGroup->compressedFiles.failedUnpack), szQuint32);
	i += szQuint32;

	memcpy(i, &(downloadGroup->compressedFiles.unpackPriority), szQuint32);
	i += szQuint32;

	memcpy(i, &(downloadGroup->compressedFiles.unused2), szQuint32);
	i += szQuint32;

	memcpy(i, &(downloadGroup->repairFiles.numFiles), szQuint32);
	i += szQuint32;

	memcpy(i, &(downloadGroup->repairFiles.numCancelled), szQuint32);
	i += szQuint32;

	memcpy(i, &(downloadGroup->repairFiles.numReleased), szQuint32);
	i += szQuint32;

	memcpy(i, &(downloadGroup->repairFiles.numBlocksReleased), szQuint32);
	i += szQuint32;

	memcpy(i, &(downloadGroup->repairFiles.succDecoded), szQuint32);
	i += szQuint32;

	memcpy(i, &(downloadGroup->repairFiles.failedDecode), szQuint32);
	i += szQuint32;

	memcpy(i, &(downloadGroup->repairFiles.failedDecodeParts), szQuint32);
	i += szQuint32;

	memcpy(i, &(downloadGroup->repairFiles.repairPriority), szQuint32);
	i += szQuint32;

	memcpy(i, &(downloadGroup->repairFiles.numBlocks), szQuint32);
	i += szQuint32;

	memcpy(i, &(downloadGroup->otherFiles.numFiles), szQuint32);
	i += szQuint32;

	memcpy(i, &(downloadGroup->otherFiles.numCancelled), szQuint32);
	i += szQuint32;

	memcpy(i, &(downloadGroup->otherFiles.numReleased), szQuint32);
	i += szQuint32;

	memcpy(i, &(downloadGroup->otherFiles.succDecoded), szQuint32);
	i += szQuint32;

	memcpy(i, &(downloadGroup->otherFiles.failedDecode), szQuint32);
	i += szQuint32;

	memcpy(i, &(downloadGroup->otherFiles.failedDecodeParts), szQuint32);
	i += szQuint32;

	memcpy(i, &(downloadGroup->otherFiles.unused1), szQuint32);
	i += szQuint32;

	memcpy(i, &(downloadGroup->otherFiles.unused2), szQuint32);
	i += szQuint32;

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	key.set_data(&(downloadGroup->groupId));
	key.set_size(szQuint16);
	data.set_data(p);
	data.set_size(saveItemSize());
	//TODO:free the data?

	if ((db->put(0, &key, &data, 0)) != 0)
	{
		delete[] p;
		return false;
	}
	else
	{
		delete[] p;
		db->sync(0);
		return true;
	}
}

int GroupManager::saveItemSize()
{
	return (31 * sizeof(quint32)) +
		   (1 * sizeof(quint16))  +
		   (5 * sizeof(uint))     +
		   downloadGroup->dirPath.size() +
		   downloadGroup->masterRepairFile.size() +
		   downloadGroup->masterUnpackFile.size() +
		   downloadGroup->processLog.size() +
		   downloadGroup->ng->getName().size();
}

bool GroupManager::dbDelete()
{
	Dbt key(&(downloadGroup->groupId), sizeof(quint16));

	if ((db->del(NULL, &key, 0)) != 0)
		return false;
	else
	{
		db->sync(0);
		return true;
	}
}

PendingHeader::PendingHeader(Db *_db, quint16 groupId, quint16 fileType, quint32 headerId) : db(_db)
{
	pendingHeader = new t_PendingHeader;

	pendingHeader->groupId = groupId;
	pendingHeader->fileType = fileType;
	pendingHeader->headerId = headerId;
	pendingHeader->repairOrder = 0;
	pendingHeader->repairBlocks = 0;
	pendingHeader->hb = 0;
	pendingHeader->headerType = 0;
	pendingHeader->unused1 = 0;
	pendingHeader->unused2 = 0;
}

PendingHeader::PendingHeader(Db *_db, uchar* dbData) : db(_db)
{
	pendingHeader = new t_PendingHeader;

	uint bhSize = 0;
	int szQuint16 = sizeof(quint16);
	int szQuint32 = sizeof(quint32);
	uchar *i = dbData;

	memcpy(&(pendingHeader->groupId), i, szQuint16);
	i += szQuint16;

	memcpy(&(pendingHeader->fileType), i, szQuint16);
	i += szQuint16;

	memcpy(&(pendingHeader->headerId), i, szQuint32);
	i += szQuint32;

	memcpy(&(pendingHeader->repairOrder), i, szQuint32);
	i += szQuint32;

	memcpy(&(pendingHeader->repairBlocks), i, szQuint32);
	i += szQuint32;

    setHeaderType(*i);
    i++;

    i = (uchar *)retrieve((char*)i, pendingHeader->subj);
    i = (uchar *)retrieve((char*)i, pendingHeader->from);
    i = (uchar *)retrieve((char*)i, pendingHeader->msgId);

    QString header;
    if(getHeaderType() == 'm')
        header = pendingHeader->subj % '\n' % pendingHeader->from;
    else
        header = pendingHeader->subj % '\n' % pendingHeader->msgId;

    QByteArray ba = header.toLocal8Bit();
    char *c_str = ba.data();

    memcpy(&bhSize, i, sizeof(uint));
    i += sizeof(uint);

    if(getHeaderType() == 'm')
        pendingHeader->hb = new MultiPartHeader(header.length(), c_str, (char*)i);
    else
        pendingHeader->hb = new SinglePartHeader(header.length(), c_str, (char*)i);

    i += bhSize;

	pendingHeader->unused1 = 0;
	pendingHeader->unused2 = 0;
}

PendingHeader::~PendingHeader()
{
    if (pendingHeader->hb)
        delete pendingHeader->hb;
    delete pendingHeader;
}

bool PendingHeader::operator< (const PendingHeader &other) const
{
	quint32 myNumber = pendingHeader->repairOrder;
	quint32 otherNumber = other.pendingHeader->repairOrder;
    return myNumber < otherNumber;
}

void PendingHeader::setHeader(HeaderBase* _hb)
{
	pendingHeader->hb = _hb;
	pendingHeader->headerType = _hb->getHeaderType();
	pendingHeader->subj = _hb->getSubj();
	pendingHeader->from = _hb->getFrom();
	pendingHeader->msgId = _hb->getMessageId();
}

char *PendingHeader::getData()
{
	uint bhSize;
	char *p = new char[saveItemSize()];
	char *i = p;

	int szQuint16 = sizeof(quint16);
	int szQuint32 = sizeof(quint32);

	memcpy(i, &(pendingHeader->groupId), szQuint16);
	i += szQuint16;

	memcpy(i, &(pendingHeader->fileType), szQuint16);
	i += szQuint16;

	memcpy(i, &(pendingHeader->headerId), szQuint32);
	i += szQuint32;

	memcpy(i, &(pendingHeader->repairOrder), szQuint32);
	i += szQuint32;

	memcpy(i, &(pendingHeader->repairBlocks), szQuint32);
	i += szQuint32;

	*i = getHeaderType();
	i++;

	i=insert(pendingHeader->subj, i);
	i=insert(pendingHeader->from, i);
	i=insert(pendingHeader->msgId, i);

	bhSize = pendingHeader->hb->getRecordSize();
	memcpy(i, &bhSize, sizeof(uint));
	i += sizeof(uint);

	memcpy(i, pendingHeader->hb->data(), pendingHeader->hb->getRecordSize());
	i += pendingHeader->hb->getRecordSize();

	memcpy(i, &(pendingHeader->unused1), szQuint32);
	i += szQuint32;

	memcpy(i, &(pendingHeader->unused2), szQuint32);
	i += szQuint32;

    return p;
}

bool PendingHeader::dbSave()
{
	Dbt key, data;
	int szQuint32 = sizeof(quint32);

	char *p = getData();

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	key.set_data(&(pendingHeader->headerId));
	key.set_size(szQuint32);
	data.set_data(p);
	data.set_size(saveItemSize());

	if ((db->put(0, &key, &data, 0)) != 0)
	{
		delete[] p;
		return false;
	}
	else
	{
		delete[] p;
		db->sync(0);
		return true;
	}
}

int PendingHeader::saveItemSize()
{
	return (5 * sizeof(quint32)) +
		   (5 * sizeof(quint16)) +
		    sizeof(char) +
		    pendingHeader->subj.length() +
		    pendingHeader->from.length() +
		    pendingHeader->msgId.length() +
		    sizeof(uint)         +
		    pendingHeader->hb->getRecordSize();
}

bool PendingHeader::dbDelete()
{
	Dbt key(&(pendingHeader->headerId), sizeof(quint32));

	if ((db->del(NULL, &key, 0)) != 0)
		return false;
	else
	{
		db->sync(0);
		return true;
	}
}

AutoFile::AutoFile(Db *_db, quint16 groupId, quint16 fileType, QString& fileName) : db(_db)
{
	autoFile = new t_AutoFile;

	autoFile->groupId = groupId;
	autoFile->fileType = fileType;
	autoFile->fileName = fileName;

	dbSave();
}

AutoFile::AutoFile(Db *_db, uchar* dbData) : db(_db)
{
	autoFile = new t_AutoFile;

	uint strSize = 0;
	char *temp;

	int szQuint16 = sizeof(quint16);
	uchar *i = dbData;

	memcpy(&(autoFile->groupId), i, szQuint16);
	i += szQuint16;

	memcpy(&(autoFile->fileType), i, szQuint16);
	i += szQuint16;

	memcpy(&strSize, i, sizeof(uint));
	i += sizeof(uint);

	temp = new char[strSize + 1];
	memcpy(temp, i, strSize);
	temp[strSize] = '\0';
	autoFile->fileName = temp;
	delete[] temp;
	i += strSize;

	qDebug() << "Restored Auto file with name: " << autoFile->fileName;
}

AutoFile::~AutoFile()
{
    delete autoFile;
}

bool AutoFile::dbSave()
{
	Dbt key, data;

	uint strSize;
	QByteArray ba;
	const char *c_str;

	char *p = new char[saveItemSize()];
	char *i = p;

	int szQuint16 = sizeof(quint16);

	memcpy(i, &(autoFile->groupId), szQuint16);
	i += szQuint16;

	memcpy(i, &(autoFile->fileType), szQuint16);
	i += szQuint16;

	strSize = autoFile->fileName.size();
	memcpy(i, &strSize, sizeof(uint));
	i += sizeof(uint);

	ba = autoFile->fileName.toLocal8Bit();
	c_str = ba.data();
	memcpy(i, c_str, strSize);
	i += strSize;

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	key.set_data(&(autoFile->groupId));
	key.set_size(szQuint16);
	data.set_data(p);
	data.set_size(saveItemSize());
	//TODO:free the data?

	if ((db->put(0, &key, &data, 0)) != 0)
	{
		delete[] p;
		return false;
	}
	else
	{
		delete[] p;
		db->sync(0);
		return true;
	}
}

int AutoFile::saveItemSize()
{
	return ((2 * sizeof(quint16)) +
		    sizeof(uint)         +
		    autoFile->fileName.size());
}

bool AutoFile::dbDelete()
{
	Dbt key(&(autoFile->groupId), sizeof(quint16));

	// We want to delete all files, so no need for a cursor
	if ((db->del(NULL, &key, 0)) != 0)
		return false;
	else
	{
		db->sync(0);
		return true;
	}

}

AutoUnpackThread::AutoUnpackThread(GroupManager* _groupManager, QWidget* _parent) :
		groupManager(_groupManager), parent(_parent)
{
	externalProcess = 0;
}

void AutoUnpackThread::processGroup()
{
	enum QThread::Priority appPriority[4] =
	    { QThread::LowestPriority, QThread::LowPriority, QThread::NormalPriority, QThread::InheritPriority };

	enum QThread::Priority thisPriority = appPriority[3];
	quint32 savedPriority;

	if (groupManager->downloadGroup->groupStatus == QUBAN_GROUP_UNPACKING)
	{
		savedPriority = groupManager->downloadGroup->compressedFiles.unpackPriority;
		if (savedPriority < 4)
		    thisPriority = appPriority[savedPriority];
	}
	else if (groupManager->downloadGroup->groupStatus == QUBAN_GROUP_REPAIRING)
	{
		savedPriority = groupManager->downloadGroup->repairFiles.repairPriority;
		if (savedPriority < 4)
		    thisPriority = appPriority[savedPriority];
	}

	start(thisPriority);
}

bool stringLessThan(const QString* s1, const QString* s2)
{
    return *s1 < *s2;
}

void AutoUnpackThread::run()
{
	int retCode = 0;
	config = Configuration::getConfig();
	AutoUnpackEvent *ce=0;

	externalProcess = new QProcess();

	// for the moment don't return progress updates
	qRegisterMetaType<QProcess::ProcessError>("QProcess::ProcessError");
	qRegisterMetaType<QProcess::ExitStatus>("QProcess::ExitStatus");
    connect(externalProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(processFinished(int, QProcess::ExitStatus)));
    connect(externalProcess, SIGNAL(error(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));

    // See if we have a .001 file as master
    if (config->merge001Files && groupManager->downloadGroup->masterUnpackFile.endsWith(".001"))
    {
    	QString newMasterUnpack = groupManager->downloadGroup->masterUnpackFile;
    	newMasterUnpack.chop(4);

    	qDebug() << "Merging files to build new master: " << newMasterUnpack;

    	// Build a list of the files to join
    	QList <QString*> filesToJoin;

    	QList <AutoFile*> autoFileList = ((QMgr*)parent)->autoFiles.values(groupManager->getGroupId());
    	QList<AutoFile*>::iterator it;
    	QString fileToMerge,
    	        strippedFile1,
    	        strippedFile2;
    	QString* fileToJoin;

    	for (it = autoFileList.begin(); it != autoFileList.end(); ++it)
    	{
    		if (((*it)->getFileType() == QUBAN_MASTER_UNPACK || (*it)->getFileType() == QUBAN_UNPACK))
    		{
    			fileToMerge = (*it)->getFileName();
    			if (fileToMerge.startsWith(newMasterUnpack))
    			{
    				// Need to make sure that it has correct suffix before adding to new list
    				strippedFile1 = fileToMerge;
    				strippedFile2 = strippedFile1.remove(newMasterUnpack);
    				if (strippedFile2.length() == 4 &&
    					strippedFile2[0] == '.' &&
    					strippedFile2[1].isDigit() &&
    					strippedFile2[2].isDigit() &&
    					strippedFile2[3].isDigit())
    				{
    					fileToJoin = new QString(fileToMerge);
    					filesToJoin.append(fileToJoin);
    					qDebug() << "Found candidate file : " << *fileToJoin;
    				}
    			}
    		}
    	}

    	if (filesToJoin.size())
    	{
    		char fileBuffer[4096];
    		qint64 readSize = 0,
    				writeSize = 0;

        	// sort the files in list
			qSort(filesToJoin.begin(), filesToJoin.end(), stringLessThan);
			QList<QString*>::iterator sit;

			QFile outFile(groupManager->downloadGroup->dirPath + "/" + newMasterUnpack);
		    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
		    {
				ce = new AutoUnpackEvent(groupManager, (int)AutoUnpackEvent::AU_FILE_OPEN_FAILED);
				QApplication::postEvent(parent, ce);
				delete externalProcess;
				return;
		    }

			qDebug() << "Opened output file : " << newMasterUnpack;

			for ( sit = filesToJoin.begin(); sit != filesToJoin.end(); ++sit)
			{
				QFile inFile(groupManager->downloadGroup->dirPath + "/" + *(*sit));
			    if (!inFile.open(QIODevice::ReadOnly))
			    {
					ce = new AutoUnpackEvent(groupManager, (int)AutoUnpackEvent::AU_FILE_OPEN_FAILED);
					QApplication::postEvent(parent, ce);
					delete externalProcess;
					outFile.close();
					return;
			    }

			    qDebug() << "Opened input file : " << groupManager->downloadGroup->dirPath + "/" + *(*sit);

	        	// Use QFile read/write to cat the files together
			    while ((readSize =  inFile.read(fileBuffer, 4096)) > 0)
			    {
			    	writeSize = outFile.write(fileBuffer, readSize);
				    if (writeSize != readSize)
				    {
						ce = new AutoUnpackEvent(groupManager, (int)AutoUnpackEvent::AU_FILE_WRITE_FAILED);
						QApplication::postEvent(parent, ce);
						delete externalProcess;
						inFile.close();
						outFile.close();
						return;
				    }
			    }

			    inFile.close();

			    if (readSize < 0)
			    {
					ce = new AutoUnpackEvent(groupManager, (int)AutoUnpackEvent::AU_FILE_READ_FAILED);
					QApplication::postEvent(parent, ce);
					delete externalProcess;
					outFile.close();
					return;
			    }
			}

			outFile.close();

			 while (!filesToJoin.isEmpty())
			     delete filesToJoin.takeFirst();
    	}

    	// Update the group record
    	groupManager->setMasterUnpack(newMasterUnpack);

    	qDebug() << "Set new master to " << newMasterUnpack;
    }

	if (groupManager->downloadGroup->groupStatus == QUBAN_GROUP_UNPACKING)
	{
		qDebug() << "Running unpack";
		retCode = runUnpack();
	}
	else if (groupManager->downloadGroup->groupStatus == QUBAN_GROUP_REPAIRING)
	{
		qDebug() << "Running repair";
		retCode = runRepair();
	}
	else
	{
		ce = new AutoUnpackEvent(groupManager, (int)AutoUnpackEvent::AU_UNKNOWN_OPERATION);
		QApplication::postEvent(parent, ce);
		delete externalProcess;
		return;
	}

	if (retCode)
	{
		delete externalProcess;
		externalProcess = 0;
		return; // event already sent
	}

	exec();

	delete externalProcess;
	externalProcess = 0;
}

int AutoUnpackThread::runUnpack()
{
	t_ExternalApp *externalApp = 0;
	AutoUnpackEvent *ce=0;

	qDebug() << "Looking for packid: " << groupManager->downloadGroup->packId;
	qDebug() << "Priority: " << groupManager->downloadGroup->compressedFiles.unpackPriority;
	if (config->numUnpackers)
	{
		for (int i=0; i<config->numUnpackers; i++)
		{
			if (config->unpackAppsList.at(i)->appId == (int)groupManager->downloadGroup->packId)
			{
				externalApp = config->unpackAppsList.at(i);
				break;
			}
		}
	}
	else
	{
		ce = new AutoUnpackEvent(groupManager, (int)AutoUnpackEvent::AU_NO_AVAILABLE_APP);
		QApplication::postEvent(parent, ce);
		return 1;
	}

	if (externalApp == 0)
	{
		qDebug() << "Can't find an unpack app";
		ce = new AutoUnpackEvent(groupManager, (int)AutoUnpackEvent::AU_UNFOUND_APP);
		QApplication::postEvent(parent, ce);
		return 2;
	}

	QString subCommand = "\"" + groupManager->downloadGroup->masterUnpackFile + "\"";
	QString command = externalApp->fullFilePath + " " + externalApp->parameters;

	command.replace("${DRIVING_FILE}", subCommand);

	qDebug() << "Run unpack command: " << command;

    externalProcess->setWorkingDirectory(groupManager->downloadGroup->dirPath);
    externalProcess->start(command);

	return 0;
}

int AutoUnpackThread::runRepair()
{
	t_ExternalApp *externalApp = 0;
	AutoUnpackEvent *ce=0;

	qDebug() << "Looking for repairId: " << groupManager->downloadGroup->repairId;
	qDebug() << "Priority: " << groupManager->downloadGroup->repairFiles.repairPriority;
	if (config->numRepairers)
	{
		for (int i=0; i<config->numRepairers; i++)
		{
			if (config->repairAppsList.at(i)->appId == (int)groupManager->downloadGroup->repairId)
			{
				externalApp = config->repairAppsList.at(i);
				break;
			}
		}
	}
	else
	{
		ce = new AutoUnpackEvent(groupManager, (int)AutoUnpackEvent::AU_NO_AVAILABLE_APP);
		QApplication::postEvent(parent, ce);
		return 1;
	}

	if (externalApp == 0)
	{
		qDebug() << "Can't find a repair app";
		ce = new AutoUnpackEvent(groupManager, (int)AutoUnpackEvent::AU_UNFOUND_APP);
		QApplication::postEvent(parent, ce);
		return 2;
	}

	QString subCommand = "\"" + groupManager->downloadGroup->masterRepairFile + "\"";
	QString command = externalApp->fullFilePath + " " + externalApp->parameters;

	command.replace("${DRIVING_FILE}", subCommand);

	qDebug() << "Run repair command: " << command;

    externalProcess->setWorkingDirectory(groupManager->downloadGroup->dirPath);
    externalProcess->start(command);

	return 0;
}

void AutoUnpackThread::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
	AutoUnpackEvent* ce = 0;

	QByteArray processOutput;
	QByteArray processError;
	QString    processExitCode;

	processOutput = externalProcess->readAllStandardOutput();
	groupManager->appendToProcessLog(processOutput);
	processError = externalProcess->readAllStandardError();
	groupManager->appendToProcessLog(processError);
	if (exitCode)
	{
		processExitCode = tr("*** Process exited with code ") + exitCode + " ***";
		groupManager->appendToProcessLog(processExitCode);
	}

	if (groupManager->downloadGroup->groupStatus == QUBAN_GROUP_UNPACKING)
	{
		if (!exitCode && exitStatus == QProcess::NormalExit)
			ce = new AutoUnpackEvent(groupManager, (int)AutoUnpackEvent::AU_UNPACK_SUCCESSFUL);
		else
			ce = new AutoUnpackEvent(groupManager, (int)AutoUnpackEvent::AU_UNPACK_FAILED);

		QApplication::postEvent(parent, ce);
	}
	else if (groupManager->downloadGroup->groupStatus == QUBAN_GROUP_REPAIRING)
	{
		if (!exitCode && exitStatus == QProcess::NormalExit)
			ce = new AutoUnpackEvent(groupManager, (int)AutoUnpackEvent::AU_REPAIR_SUCCESSFUL);
		else
		{
			int messagePos = processOutput.lastIndexOf(tr("You need "));
			if (messagePos != -1)
			{
				QString mess(processOutput.mid(messagePos));
				groupManager->downloadGroup->numBlocksNeeded = mess.section(' ', 2, 2).toInt();
			}
			ce = new AutoUnpackEvent(groupManager, (int)AutoUnpackEvent::AU_REPAIR_FAILED);
		}

		QApplication::postEvent(parent, ce);
	}

	exit(exitCode);
}

void AutoUnpackThread::processError(QProcess::ProcessError )
{
	// TODO look at the error and decide what to do
	// QByteArray QProcess::readAllStandardError(); etc
	// send event
	// close();
	// kill event loop
}

ConfirmationHeader::ConfirmationHeader(HeaderBase*_hb)
{
	hb= _hb;

	QString simplifiedSubject = hb->getSubj().simplified();

	// If only the filename is quoted then we can simply strip it out
	if (simplifiedSubject.count('"') == 2)
	    fileName = simplifiedSubject.section('"',1,1);
	else
	{
        // TODO need to improve this ..
		fileName = simplifiedSubject;
	}
}
