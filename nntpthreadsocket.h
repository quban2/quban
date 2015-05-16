#ifndef NNTPTHREADSOCKET_H_
#define NNTPTHREADSOCKET_H_

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

#include <QThread>
#include <QFile>
#include <QDateTime>
#include <QStringList>
#include <QQueue>
#include <QMultiHash>
#include <QApplication>
#include <QEvent>
#include <QRegExp>
#include "common.h"
#include "newsgroup.h"
#include "availablegroups.h"
#include "rateController.h"
#include "yydecoder.h"

struct Job { //Is it persistent???

	int id;			//assigned by qmgr
	int qItemId;	//assigned by qitem
	int jobType; 	//returned by qitem
	int partId;
	int status;
	int qId;
	int threadId;
	QString mid;
    enum JobTypes {UpdHead, GetPost, GetList, GetGroupLimits, ExpireHead, GetExtensions};
    enum Status {Queued_Job, Processing_Job, Finished_Job, Failed_Job, Cancelled_Job, Paused_Job, Updating_Job};
	QString error;
	int artNum;		//article number, returned by qitem
	int artSize;
	QString fName;
	Db* gdb;		//inserted by qmgr
 	AvailableGroups *ag;
	NntpHost *nh; //Not persistent?? Hmmm...
	NewsGroup *ng;	//returned by qitem
	int tries;
	uint from;
	uint to;
};

class QItem;

/*
Implementation of the thread - trying to make it as autonomous as possible.
*Status:
 - Ready: Idle thread
 - Working: thread is running
 - Paused: the queue manager stopped the thread
 - PausedOnErr: thread paused itself because of an error (typically a Communication error), a timer will restart it after a proper period of time (see Thread::slotRetryTimeout()
 - DelayedDelete: about to delete the thread (after a "remove thread")

*controls:
 - cancel: stop current download, but don't stop the thread. After cancel of current item is acknowledged, thread continues -> doesn't change the state.
 - pause: pause the thread -> changes the state
 controls are reset (by the thread itself) after their work is done

tStart() starts the thread, but if and only if status == Ready -> if the thread has been paused, it has to be *explicitly* resumed with tResume();

*/

#define NUM_HEADER_WORKERS 1

class NntpThread : public QThread
{
    Q_OBJECT

	private:

	    crc32_t crc;

        volatile bool isRatePeriod;
        volatile bool isNilPeriod;
		//For locking...
		QMutex *queueLock; //Queue locking, for grabbing jobs
		QMutex *headerDbLock; // for syncing multiple server updates

        QThread   HeaderReadThread[NUM_HEADER_WORKERS];

		//For control...
		volatile bool pause;
		uint status;	//Status of the thread
		volatile bool cancel;
		volatile bool connClose;
		volatile bool validated;

        volatile bool validate;

		int *pendingOperations;
		//The queue(s)!

		QMap<int, QItem*> *queue;
		QList<int> *threadQueue;

		uint *curbytes;   //Bytes currently downloaded
		uint lines;		  //Progress lines
		uint step;		  //Step (for message progress)
		uint totalLines;  //Total lines to read from the net

		// QList<QSslError> expectedSslErrors;
		RcSslSocket  *kes;
        qint64 bytes;
		QStringList addrList;
		int groupArticles;
        QMgr* qm;

		Job *job;		  //our job. Should be persistent...check!

		// int retries;	//Hmmm
		int error;		//Error status
			//timer...started when I start async ops and restarted every byte received...
		int timeout;
// 		bool *isTimeout;
		QString fileName;

		uint lowWatermark, highWatermark, articles;
		Db *db;	//This points to ng->db

		uint datamemSize;

		uchar *keymem, *datamem;
		Dbt *key, *data;
		void reset();
        void partReset();

        QFile *saveFile;

		int unreadArticles;
		int pos;
		quint16 capPart;
		quint16 capTotal;
        int index;

		Configuration* config;

		bool saveGroup();

		int dbGroupPut(Db *db, const char *line, int hostId);
		uint createDateTime( QStringList dateTime);  //Creates a date/time string suitable for QDateTime
		void parse(QStringList);   //Stupid function that parses the xover information
		bool serverConnect(QString & addr, quint16 port);
		Job *findFreeJob();
		bool m_readLine();
        bool m_readLineBA();
		bool m_readBigLine();

        void freeSocket();

		//and now, from KNntpSocket:
		int lineBufSize;
        char* line;
        QByteArray* lineBA;
		char *buffer, *watermark, *lineEnd;
		int bufferSize, lineSize;

		char* inflatedBuffer;
		uchar* bufferLine;

        QByteArray* bigLine;

		char *bigBuffer, *bigWatermark, *bigLineEnd;
		qint32 bigBufferSize, bigLineSize;

    	int timeoutVal; //timeout in cycles...

		enum CommandType {Cmd_None=0, Cmd_Login=1, Cmd_GetPost=2, Cmd_GetXoverInfo, Cmd_GetXover, Cmd_GetHead, Cmd_Quit};
		enum CommandProgress {Cmd_Username=0, Cmd_Pass=1, Cmd_Group=2, Cmd_Article,  Cmd_Header, Cmd_Xover, Cmd_Connected, Cmd_NoCmd, Cmd_Finished, Cmd_Connect}; //incomplete, just trying...
		enum response { xover=224, user=381, pass=281, article=220, body=222, head=221, group=211, quit=205, ready=200, readyNoPost=201, list=215, gzip=290, capabilities=101, help=100};
		QString errorString;
        QList<QSslError> sslErrs;
        QSslCertificate newCertificate;


		bool isLoggedIn;

		ulong xoverStart, xoverEnd;

		QString cmd;
		NntpHost *nHost;
		QString newsGroup;
		QString pattern;
		int artNum;
		int threadId;
		int qId;

		//Members...
		bool waitLine(); //True if new line exists, false if timeout;
		bool waitBigLine();
		int m_handleError(int expected, QString received); //Handles line error...

		bool m_connect(); //Connect & login
    	bool m_disconnect();  //Disconnect
    	bool m_isConnected();	//Is the socket connected & logged in?
		bool getXover(QString group);  //Works
		bool getGroupLimits(QString group);
		bool getCapabilities();
		bool getXPat(QString group, QString pat);
		bool getArticle();  //Works
		bool getListOfGroups();
		bool getBody(QString group, int artnum); //Unimplemented
		bool getHead(QString group, int artnum); //Unimplemented
		int m_getError() {return error;}
// TODO    	const char *m_getErrorDesc() {return (const char *) errorString;}
		char * m_findEndLine( char * start, char * end );
		char * m_findDotEndLine( char * start, char * end );
		void setHost(NntpHost *nh);
		bool m_sendCmd( QString& cmd, int response );

		void charCRC(const unsigned char *c);

		QTime prevTime, currentTime;

public:
		enum Status {Ready=0, Working=1, Paused, PausedOnError, Delayed_Delete, NewJob, Cancelling, ClosingConnection }; //ready means "Accepting job"
        enum Error {No_Err, Timeout_Err, Comm_Err, DbWrite_Err, FatalTimeout_Err, TooManyThreads_Err, Auth_Err, Other_Err, Write_Err=90, Decode_Err, Unzip_Err=100, NoSuchArticle_Err=423, NoSuchMid_Err=430, NoSuchGroup_Err=411,
                   Connection_Err = 900};
		enum Control {Go, Pause};
		bool addJob();
// 		void setStatus(int s) {status=s;};
        bool isConnected() {return isLoggedIn;}


        NntpThread(uint, uint, uint*, bool, bool, bool, bool, NntpHost *, QMgr*);
		virtual ~NntpThread();

		void setRatePeriod(bool);
		void setNilPeriod(bool);
        virtual void run();
        void tStart();
        void setValidated(bool);
		void tCancel();
		void tPause();
		void tResume();
        void tValidate();
		void closeConnection(bool);
		void setQueue(QList<int> *tq, QMap<int, QItem*> *q, QMutex *qLock, int *po);
		void setDbSync(QMutex *qLock);

signals:
       void startWork();
       void StartedWorking(int, int);
       void Start(Job *);
       void DownloadFinished(Job *);
       void DownloadCancelled(Job *);
       void DownloadError(Job *, int);
       void Finished(Job *);
       void Cancelled(Job *);
       void Failed(Job *, int);
       void Err(Job *, int);
       void SigPaused(int, int, bool);
       void SigDelayed_Delete(int, int);
       void SigReady(int, int);
       void SigClosingConnection(int, int);
       void SigUpdate(Job *, uint, uint, uint);
       void SigUpdatePost(Job *, uint, uint, uint, uint);
       void SigUpdateLimits(Job *, uint, uint, uint);
       void SigExtensions(Job *, quint16, quint64);

	   void sigHeaderDownloadProgress(Job*, quint64, quint64, quint64);

       void registerSocket(RcSslSocket*);
       void unregisterSocket(RcSslSocket*);

       void logMessage(int type, QString description);
       void logEvent(QString description);

       void serverValidated(uint, bool, QString, QList<QSslError>);

       void startHeaderRead();
       void quitHeaderRead();
       void setReaderBusy();
};

#endif /* NNTPTHREADSOCKET_H_ */
