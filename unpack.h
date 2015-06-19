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

#ifndef UNPACK_H_
#define UNPACK_H_

#include <QObject>
#include <QThread>
#include <QEvent>
#include <QProcess>
#include <QList>
#include <QString>
#include <QStringList>
#include <QtAlgorithms>
#include "MultiPartHeader.h"

class GroupList;

enum PackType { QUBAN_PACK_RAR=1, QUBAN_PACK_7Z, QUBAN_PACK_ZIP, QUBAN_PACK_OTHER };
enum RepairType { QUBAN_REPAIR_NONE=0, QUBAN_REPAIR_PAR2, QUBAN_REPAIR_OTHER };

typedef struct
{
	quint32 numFiles;
	quint32 numCancelled;
	quint32 succDecoded;
	quint32 failedDecode;
	quint32 failedDecodeParts;
	quint32 failedUnpack;
	quint32 unpackPriority;
	quint32 unused2;
} t_CompressedFiles;

typedef struct
{
	quint32 numFiles;
	quint32 numCancelled;
	quint32 numReleased;
	quint32 numBlocksReleased;
	quint32 succDecoded;
	quint32 failedDecode;
	quint32 failedDecodeParts;
	quint32 repairPriority;
	quint32 numBlocks;
} t_RepairFiles;

typedef struct
{
	quint32 numFiles;
	quint32 numCancelled;
	quint32 numReleased; // all at the moment
	quint32 succDecoded;
	quint32 failedDecode;
	quint32 failedDecodeParts;
	quint32 unused1;
	quint32 unused2;
} t_OtherFiles;

enum GroupStatus { QUBAN_GROUP_INIT=1, QUBAN_GROUP_DOWNLOADING_PACKED, QUBAN_GROUP_DOWNLOADING_REPAIR,
	               QUBAN_GROUP_REPAIRING, QUBAN_GROUP_UNPACKING, QUBAN_GROUP_SUCEEDED, QUBAN_GROUP_FAILED,
	               QUBAN_GROUP_CANCELLED };

class NewsGroup;

typedef struct
{
    quint16 groupId;
    quint32 groupStatus;
    t_CompressedFiles compressedFiles;
    t_RepairFiles repairFiles;
    t_OtherFiles otherFiles;
    quint32 unpackFailed;
    quint32 numBlocksNeeded;
    quint32 repairId;
    quint32 packId;
    quint32 unused1;
    quint32 unused2;
	QString dirPath;
	QString masterRepairFile;
	QString masterUnpackFile;
	QString processLog;
	NewsGroup* ng;

} t_DownloadGroup;

class AutoUnpackThread;
class QMgr;

class GroupManager : private QObject
{
public:
	GroupManager(Db *, NewsGroup* _ng, quint16);
	GroupManager(Db *, GroupList *gList, uchar* dbData, QMgr*);
	~GroupManager();

	inline quint16 getGroupId() { return downloadGroup->groupId; }
	inline enum GroupStatus getGroupStatus() { return (enum GroupStatus)downloadGroup->groupStatus; }
	inline QString& getGroupStatusString() { return groupStatusStrings[downloadGroup->groupStatus]; }
	inline NewsGroup* getNewsGroup() { return downloadGroup->ng; }

	inline QString& getDirPath() { return downloadGroup->dirPath; }
	inline QString& getMasterRepair() { return downloadGroup->masterRepairFile; }
	inline QString& getMasterUnpack() { return downloadGroup->masterUnpackFile; }
	inline QString& getProcessLog() { return downloadGroup->processLog; }

	inline quint32 getRepairId() { return downloadGroup->repairId; }
	inline quint32 getPackId() { return downloadGroup->packId; }

	inline quint32 getRepairPriority() { return downloadGroup->repairFiles.repairPriority; }
	inline quint32 getPackPriority() { return downloadGroup->compressedFiles.unpackPriority; }

	inline quint32 getNumUnpackFiles() { return downloadGroup->compressedFiles.numFiles; }
	inline quint32 getNumDecodedUnpackFiles() { return downloadGroup->compressedFiles.succDecoded; }
	inline quint32 getNumFailedDecodeUnpackFiles() { return downloadGroup->compressedFiles.failedDecode; }
	inline quint32 getNumFailedDecodeUnpackParts() { return downloadGroup->compressedFiles.failedDecodeParts; }
	inline quint32 getNumCancelledUnpackFiles() { return downloadGroup->compressedFiles.numCancelled; }

	inline quint32 getNumRepairFiles() { return downloadGroup->repairFiles.numFiles; }
	inline quint32 getNumRepairBlocks() { return downloadGroup->repairFiles.numBlocks; }
	inline quint32 getNumReleasedRepairFiles() { return downloadGroup->repairFiles.numReleased; }
	inline quint32 getNumDecodedRepairFiles() { return downloadGroup->repairFiles.succDecoded; }
	inline quint32 getNumFailedDecodeRepairFiles() { return downloadGroup->repairFiles.failedDecode; }
	inline quint32 getNumFailedDecodeRepairParts() { return downloadGroup->repairFiles.failedDecodeParts; }
	inline quint32 getNumCancelledRepairFiles() { return downloadGroup->repairFiles.numCancelled; }

	inline quint32 getNumOtherFiles() { return downloadGroup->otherFiles.numFiles; }
	inline quint32 getNumReleasedOtherFiles() { return downloadGroup->otherFiles.numReleased; }
	inline quint32 getNumDecodedOtherFiles() { return downloadGroup->otherFiles.succDecoded; }
	inline quint32 getNumFailedDecodeOtherFiles() { return downloadGroup->otherFiles.failedDecode; }
	inline quint32 getNumFailedDecodeOtherParts() { return downloadGroup->otherFiles.failedDecodeParts; }
	inline quint32 getNumCancelledOtherFiles() { return downloadGroup->otherFiles.numCancelled; }

	inline quint32 getNumFiles() { return downloadGroup->compressedFiles.numFiles +
			                                     downloadGroup->repairFiles.numFiles +
			                                     downloadGroup->otherFiles.numFiles; }
	inline quint32 getNumDecodedFiles() { return downloadGroup->compressedFiles.succDecoded +
			                                     downloadGroup->repairFiles.succDecoded +
			                                     downloadGroup->otherFiles.succDecoded; }
	inline quint32 getNumFailedFiles() { return downloadGroup->compressedFiles.failedDecode +
			                                     downloadGroup->repairFiles.failedDecode +
			                                     downloadGroup->otherFiles.failedDecode; }
	inline quint32 getNumCancelledFiles() { return downloadGroup->compressedFiles.numCancelled +
			                                     downloadGroup->repairFiles.numCancelled +
			                                     downloadGroup->otherFiles.numCancelled; }
	inline quint32 getNumOnHoldFiles() { return downloadGroup->repairFiles.numFiles -
			                                     downloadGroup->repairFiles.numReleased; }

	inline void setGroupStatus(enum GroupStatus groupStatus) { downloadGroup->groupStatus = (quint32)groupStatus; }
	inline void setNewsgroup(NewsGroup* ng) { downloadGroup->ng = ng; }
	inline void appendToProcessLog(QByteArray& ba) { downloadGroup->processLog.append(ba); }
	inline void appendToProcessLog(QString& str) { downloadGroup->processLog.append(str); }
	inline void addRepairFile() { downloadGroup->repairFiles.numFiles++; }
	inline void addRepairBlocks(int repairBlocks) { downloadGroup->repairFiles.numBlocks += repairBlocks; }
	inline void removeRepairBlocks(int repairBlocks) { downloadGroup->repairFiles.numBlocks -= repairBlocks; }
	inline void addPackedFile() { downloadGroup->compressedFiles.numFiles++; }
	inline void addOtherFile()  { downloadGroup->otherFiles.numFiles++; }

	inline void addSuccDecodeRepair() { downloadGroup->repairFiles.succDecoded++; }
	inline void addSuccDecodeUnpack() { downloadGroup->compressedFiles.succDecoded++; }
	inline void addSuccDecodeOther() { downloadGroup->otherFiles.succDecoded++; }

	inline void addCancelledRepair() { downloadGroup->repairFiles.numCancelled++; }
	inline void addCancelledUnpack() { downloadGroup->compressedFiles.numCancelled++; }
	inline void addCancelledOther() { downloadGroup->otherFiles.numCancelled++; }
	inline void setNumCancelledRepair(quint32 numCanc) { downloadGroup->repairFiles.numCancelled = numCanc; }
	inline void setNumCancelledUnpack(quint32 numCanc) { downloadGroup->compressedFiles.numCancelled = numCanc; }
	inline void setNumCancelledOther(quint32 numCanc) { downloadGroup->otherFiles.numCancelled = numCanc; }

	inline void addFailDecodeRepair() { downloadGroup->repairFiles.failedDecode++; }
	inline void addFailDecodeUnpack() { downloadGroup->compressedFiles.failedDecode++; }
	inline void addFailDecodeOther() { downloadGroup->otherFiles.failedDecode++; }

	inline void addFailDecodeRepairParts(int failedParts) { downloadGroup->repairFiles.failedDecodeParts += failedParts; }
	inline void addFailDecodeUnpackParts(int failedParts) { downloadGroup->compressedFiles.failedDecodeParts += failedParts; }
	inline void addFailDecodeOtherParts(int failedParts) { downloadGroup->otherFiles.failedDecodeParts += failedParts; }

	inline void addReleasedRepair() { downloadGroup->repairFiles.numReleased++; }
	inline void addReleasedOther() { downloadGroup->otherFiles.numReleased++; }

	inline void addReleasedRepairParts(int parts) { downloadGroup->repairFiles.numBlocksReleased += parts; }

	inline void setRepairId(quint32 _repairId) { downloadGroup->repairId = _repairId; }
	inline void setPackId(quint32 _packId) { downloadGroup->packId = _packId; }

	inline void setRepairPriority(quint32 _repairPriority) { downloadGroup->repairFiles.repairPriority = _repairPriority; }
	inline void setPackPriority(quint32 _packPriority) { downloadGroup->compressedFiles.unpackPriority = _packPriority; }

	inline void setDirPath(QString& dirPath) { downloadGroup->dirPath = dirPath; }
	inline void setMasterRepair(QString& masterRepairFile) { downloadGroup->masterRepairFile = masterRepairFile; }
	inline void setMasterUnpack(QString& masterUnpackFile) { downloadGroup->masterUnpackFile = masterUnpackFile; }

	inline AutoUnpackThread* getWorkingThread() { return workingThread; }
	inline void setWorkingThread(AutoUnpackThread* _workingThread) { workingThread = _workingThread; }

	bool dbSave();
	bool dbDelete();

	t_DownloadGroup*  downloadGroup;
	QStringList groupStatusStrings;
	bool        usedAllDownloadedRepairFiles;
	bool        repairNext;

private:

	int saveItemSize();

	AutoUnpackThread* workingThread;
	Db*               db;
};

//////////////////////////////////////////////

enum UnpackFileType { QUBAN_MASTER_REPAIR=1, QUBAN_REPAIR, QUBAN_MASTER_UNPACK, QUBAN_UNPACK, QUBAN_SUPPLEMENTARY };

typedef struct
{
    quint16 groupId;
    quint16 fileType;
    quint32 headerId;
    quint32 repairOrder;
    quint32 repairBlocks;

    char headerType;
    HeaderBase*  hb;
    QString subj;
    QString from;
    QString msgId;

    quint32 unused1;
    quint32 unused2;

} t_PendingHeader;

class PendingHeader
{
public:
	PendingHeader(Db *, quint16, quint16, quint32);
    PendingHeader(Db *, uchar* dbData);
	~PendingHeader();

	inline quint16 getGroupId() { return pendingHeader->groupId; }
	inline quint16 getFileType() { return pendingHeader->fileType; }
	inline quint32 getHeaderId() { return pendingHeader->headerId; }
	inline char getHeaderType() { return pendingHeader->headerType; }
	inline HeaderBase* getHeader() { return pendingHeader->hb; }
	inline quint32 getRepairOrder() { return pendingHeader->repairOrder; }
	inline quint32 getRepairBlocks() { return pendingHeader->repairBlocks; }

	void setHeader(HeaderBase* _hb);
    void deleteHeader() {if (pendingHeader->hb) {delete pendingHeader->hb; pendingHeader->hb = 0;}}
	inline void setHeaderType(char t) { pendingHeader->headerType = t; }
	inline void setRepairOrder(quint32 _repairOrder) { pendingHeader->repairOrder = _repairOrder; }
	inline void setRepairBlocks(quint32 _repairBlocks) { pendingHeader->repairBlocks = _repairBlocks; }

	bool operator< (const PendingHeader &other) const;

	char *getData();
	int saveItemSize();

	bool dbSave();
	bool dbDelete();

	t_PendingHeader* pendingHeader;

private:

	Db*              db;
};

//////////////////////////////////////////////

typedef struct
{
    quint16 groupId;
    quint16 fileType;
    QString fileName;
} t_AutoFile;

class AutoFile
{
public:
	AutoFile(Db *, quint16, quint16, QString&);
	AutoFile(Db *, uchar* dbData);
	~AutoFile();

	inline quint16 getGroupId() { return autoFile->groupId; }
	inline quint16 getFileType() { return autoFile->fileType; }
	inline QString& getFileName() { return autoFile->fileName; }

	bool dbDelete();

	t_AutoFile* autoFile;

private:
	int saveItemSize();
	bool dbSave();

	Db*              db;
};

//////////////////////////////////////////////

#define UNPACKTHREADEVENT 64550

class AutoUnpackEvent: public QEvent
{
	friend class QMgr;
private:
	GroupManager* groupManager;
	int err;
public:
	enum AU_Event
	{
		AU_UNKNOWN_OPERATION = 1, AU_NO_AVAILABLE_APP, AU_UNFOUND_APP,
		AU_UNPACK_SUCCESSFUL, AU_UNPACK_FAILED, AU_FILE_OPEN_FAILED,
		AU_FILE_READ_FAILED, AU_FILE_WRITE_FAILED,
		AU_REPAIR_SUCCESSFUL, AU_REPAIR_FAILED
	};
	AutoUnpackEvent(GroupManager* _groupManager, int _err) : QEvent((QEvent::Type) UNPACKTHREADEVENT), groupManager(_groupManager), err(_err) {}
};

class Configuration;

bool stringLessThan(const QString* s1, const QString* s2);

class AutoUnpackThread: public QThread
{
	Q_OBJECT
protected:
	GroupManager* groupManager;

public:
	void processGroup();
	virtual void run();
	AutoUnpackThread(GroupManager* groupManager, QWidget *parent);
	// 		~AutoUnpackThread();
	QProcess* getProcess() { return externalProcess; }
	void clearProcess() { externalProcess = 0; }

private:
	int runUnpack();
	int runRepair();

	Configuration* config;
	QWidget *parent;  //where to send the progress/start/finish messages
	QProcess* externalProcess;

private slots:
    void processFinished(int, QProcess::ExitStatus);
    void processError(QProcess::ProcessError);
};

//////////////////////////////////////////////

class ConfirmationHeader
{
public:
	ConfirmationHeader(HeaderBase*);

	enum UnpackFileType UnpackFileType;
	HeaderBase*         hb;
	QString             fileName;

	bool operator< (const ConfirmationHeader& val) const { return fileName < val.fileName; }
};

class UnpackConfirmation
{
public:
	quint16                    groupId;
	NewsGroup*                 ng;
	bool                       first;
	QString                    dDir;
	quint32                    packId;
	quint32                    repairId;
	quint32                    packPriority;
	quint32                    repairPriority;
	QList<ConfirmationHeader*> headers;
};

#endif /* UNPACK_H_ */
