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

#include <errno.h>
#include <QSslConfiguration>
#include <QSslCipher>
#include <QDebug>
#include <QtDebug>
#include <QMutexLocker>
#include <QStringBuilder>
#include "queueparts.h"
#include "appConfig.h"
#include "headerReadNoCompression.h"
#include "headerReadXfeatGzip.h"
#include "headerReadXzVer.h"
#include "nntpthreadsocket.h"

extern crc32_t crc_table[];

#define COMPRESSED_HEADER_THRESHOLD 18000

void NntpThread::run()
{
    //qDebug("In run(): starting thread %d,%d", qId,threadId);
    bool OS2_system = false;
    bool result = false;

    if (connClose)
    {
        connClose=false;
        emit SigClosingConnection(qId, threadId);

        if (kes && kes->state() == QAbstractSocket::ConnectedState)
        {
            reset();
        }
    }

    config = Configuration::getConfig();

    if (isNilPeriod)
    {
        status=Ready;
        emit SigReady(qId, threadId);
        return;
    }

    memset(line, 0, sizeof(lineBufSize));

    if (!kes)
    {
        kes = new RcSslSocket(isRatePeriod, isNilPeriod);
        Q_CHECK_PTR(kes);
        if (isRatePeriod)
            emit registerSocket(kes);
    }

    status=Working;

    kes->isActive = true;

    if (validate)
    {
        qDebug() << "Validating server " << qId;

#if defined(Q_OS_OS2) // In qglobal.h
        OS2_system = true;
#endif

#if defined(Q_OS_OS2EMX) // In qglobal.h
        OS2_system = true;
#endif
        if (OS2_system && nHost->getSslSocket() == 0) // OS2 SSL will need to proceed, non SSL can be marked as valid and connect when required
        {
            validate = false;
            validated = true;
            emit serverValidated(qId, true, errorString, sslErrs);
        }
        else if (m_connect() == true)
        {
            validate = false;
            validated = true;
            emit serverValidated(qId, true, errorString, sslErrs);
            qDebug() << "Ok validating server " << qId;
        }
        else if (nHost->getSslSocket()) // failed to validate, but we may have sorted out the SSL errs, so if SSL try again
        {
            if (!kes)
            {
                kes = new RcSslSocket(isRatePeriod, isNilPeriod);
                Q_CHECK_PTR(kes);
                if (isRatePeriod)
                    emit registerSocket(kes);
            }

            if (m_connect() == true)
            {
                validate = false;
                validated = true;
                emit serverValidated(qId, true, errorString, sslErrs);
                qDebug() << "Ok validating server " << qId;
            }
            else
            {
                emit serverValidated(qId, false, errorString, sslErrs);
                qDebug() << "Failed validating SSL server " << qId;
            }
        }
        else
        {
            emit serverValidated(qId, false, errorString, sslErrs);
            qDebug() << "Failed validating TCP server " << qId;
        }

        if (kes)
            kes->isActive = false;

        status=Ready;
        emit SigReady(qId, threadId);

        return;
    }

    //While there are jobs in the queue, process jobs :)
    emit StartedWorking(qId, threadId);

    while (true)
    {
        //qDebug("qID: %d, tID: %d", qId, threadId);

        if (pause)
        {
            status=Paused;
            pause=false;
        }

        if (connClose)
        {
            status=ClosingConnection;
            connClose=false;
        }

        queueLock->lock();
        if ( (*pendingOperations) != 0)
        {
            //Override paused state!
            (*pendingOperations)--;
            status=Delayed_Delete;
            queueLock->unlock();
            break;
        }
        queueLock->unlock();

        //Out of the "if" cause I want job assigned, for checking on exit.

        if ( (status != Working) || !validated || !(job=findFreeJob()) )
        {
            break;
        }

        if (!kes)
        {
            kes = new RcSslSocket(isRatePeriod, isNilPeriod);
            Q_CHECK_PTR(kes);
            if (isRatePeriod)
                emit registerSocket(kes);
        }

        if (!addJob())
        {
            // Write errors are considered non-fatal for the article...they're mostly out-of space errors.
            // the queue is paused, and the user's responsibility to restart it, possibly after making
            // room on the filesystem :)
            qDebug() << "Add job Failed!";

            queueLock->lock();
            job->status=Job::Queued_Job;
            queueLock->unlock();
            //thread is in paused state
            status=PausedOnError;
            break;
        }

        emit Start(job);

        if (isRatePeriod)
        {
            int counter = 0;

            while (counter < 4 && !kes->isRegistered)
            {
                msleep(500);
                ++counter;
            }

            if (!kes->isRegistered)
            {
                qDebug() << "Can't register with Rate Controller - exiting";
                exit(1);
            }
        }

        // qDebug() << "Got job " << job->jobType;

        switch (job->jobType)
        {
        case Job::UpdHead:
            if (job->ng->downloadingServers.contains(qId) == false)
                job->ng->downloadingServers.append(qId);

            result = getXover(newsGroup);
            job->ng->downloadingServers.removeOne(qId);

            break;

        case Job::GetPost:

            result = getArticle();
            saveFile->close();
            Q_DELETE(saveFile);
            saveFile=0;
            break;

        case Job::GetList:

            result = getListOfGroups();
            queueLock->lock();
            job->ag->stopUpdating();
            queueLock->unlock();
            break;

        case Job::GetGroupLimits:

            result = getGroupLimits(newsGroup);
            break;

        case Job::GetExtensions:

            result = getCapabilities();
            break;

        default:
            qDebug() << "NntpThread::run(): unknown job type!";
            result = false;
            break;
        }

        if (result == true)
        {
            queueLock->lock();
            job->status=Job::Finished_Job;
            queueLock->unlock();
            if (job->jobType != Job::UpdHead)
                emit Finished(job);
            else
                emit DownloadFinished(job);

        }
        else
        {
            if (cancel)
            {
                queueLock->lock();
                job->status = Job::Cancelled_Job;
                queueLock->unlock();
                partReset();

                if (job->jobType != Job::UpdHead)
                    emit Cancelled(job);
                else
                    emit DownloadCancelled(job);
            }
            else
            {
                qDebug("Error: %d", error);

                queueLock->lock();
                if (job->jobType != Job::GetPost)
                    job->status=Job::Failed_Job;
                job->error=errorString;
                queueLock->unlock();

                if (job->jobType != Job::GetPost)
                {
                    emit Failed(job, error);
                }
                else
                {
                    //Ugly, should only pause the Thread class, but oh, well...

                    if (error == NoSuchArticle_Err)
                    {
                        qDebug() << "run(): no such article";
                        queueLock->lock();
                        //Try (default: 2 tries)
                        //some servers often returns "no such article",
                        if (job->tries > 0)
                        {
                            job->tries--;
                            qDebug() << "NntpThread::run(): retry: "
                                     <<    job->tries;
                            job->status=Job::Queued_Job;
                            emit Err(job, No_Err);

                        }
                        else
                        {
                            //Job/article failed...do not retry!
                            job->status=Job::Failed_Job;
                            job->error=errorString;
                            emit Err(job, error);
                        }
                        queueLock->unlock();
                    }
                    else
                    {
                        //Pause the thread and reset the status of the job...
                        //The thread has to be restarted by the QMgr, after
                        // the proper time has passed
                        emit Err(job, error);
                        status=PausedOnError;
                        queueLock->lock();
                        job->status=Job::Queued_Job;
                        queueLock->unlock();
                    }
                }
            }
        }
    }

    if (kes)
        kes->isActive = false;

    //Exited. Why?

    switch (status)
    {
    case Paused:
        // qDebug("You paused me: %d", threadId);
        emit SigPaused(qId, threadId, false);
        break;
    case PausedOnError:
        qDebug("Paused on error: %d", threadId);
        emit SigPaused(qId, threadId, true);
        break;
    case Delayed_Delete:
        emit SigDelayed_Delete(qId, threadId);
        break;
    case Working:
    case Ready:
        status=Ready;
        emit SigReady(qId, threadId);
        break;
    case ClosingConnection:
        emit SigClosingConnection(qId, threadId);
        break;
    default:
        qDebug() << "Error: wrong status: " << status;
        break;
    }

    if (kes && status == ClosingConnection && kes->state() == QAbstractSocket::ConnectedState)
    {
        reset();
    }

    //qDebug("Exiting thread : %d", threadId);
}

Job *NntpThread::findFreeJob()
{
    QList<int>::iterator it;
    QMap<int, Part*>::iterator pit;
    QItem *item = 0;
    Job *j = 0;
    //For every item...
    queueLock->lock();

    for (it = threadQueue->begin(); it != threadQueue->end(); ++it)
    {
        //for every part of the item...
        //         qDebug("Thread %d: trying item %d", threadId, *it);
        item=(*queue)[*it];
        //         qDebug("item's part count: %d", item->parts.count());
        for (pit = item->parts.begin(); pit != item->parts.end(); ++pit)
        {
            //check every job of the item...
            //             qDebug("Trying part %d of item %d", pit.key(), *it);
            j=pit.value()->job;
            if ( (j->status==Job::Queued_Job) && (j->qId == qId) )
            {
                //                     qDebug("Job: %d", j->id);
                j->status=Job::Processing_Job;
                j->threadId=threadId;
                queueLock->unlock();
                return j; //Returns the job pointer...
            }
        }
    }

    queueLock->unlock();
    //     qDebug("thread %d: job not found", threadId);
    return 0;
}

bool NntpThread::addJob()
{
    //     qDebug("Entering NntpThread::addJob()");

    if (nHost == 0)
    {
        nHost=job->nh; //Redundant, but oh, well :)
        error=NntpThread::No_Err;
    }

    timeout=nHost->getTimeout();
    //     retries=nHost->tries;

    switch (job->jobType)
    {
    case Job::GetPost:
        db=job->ng->getDb();

        //         qDebug("NntpThread::addJob(): adding GetPost job");
        //         *status=NntpThread::NewJob;
        //         qDebug("Savefile: %s", (const char *) job->fName);
        saveFile=new QFile(job->fName);

        if (!saveFile->open(QIODevice::WriteOnly|QIODevice::Truncate))
        {
            //             qDebug("Failed to open file: %s", (const char *) job->fName);
            error=Write_Err;
            errorString="Cannot open file " + job->fName ;
            return false;
        }
        //         newsGroup=job->ng->ngName;
        artNum=job->artNum;
        lines=0;
        //         articles=0;
        articles=job->artSize;

        return true;

        break;

    case Job::UpdHead:

        db=job->ng->getDb();
        newsGroup=job->ng->ngName;
        lines=0;
        articles=0;

        return true;
        break;

    case Job::GetGroupLimits:

        db=job->ng->getDb();
        newsGroup=job->ng->ngName;
        lines=0;
        articles=0;

        return true;
        break;

    case Job::GetList:

        db=job->ag->getDb();
        job->ag->startUpdating();
        lines=1;
        articles=1;
        return true;
        break;

    case Job::GetExtensions:

        lines=0;
        articles=0;

        return true;
        break;

    default:
        return false;
        break;

    }
    return false; //job NOT added;
}

char * NntpThread::m_findEndLine( char * start, char * end )
{
    char *p = 0;
    for (p=start; p < end; p++)
    {
        if (p[0] == '\r' &&
                p+1 < end && p[1] == '\n')
            return p;
    }
    return NULL;
}

char * NntpThread::m_findDotEndLine( char * start, char * end )
{
    if (start <= end - 3)
    {
        char *p = end - 3;
        //qDebug("%d %d %d", *p, *(p+1), *(p+2));
        if (p[0] == '.' &&
                p[1] == '\r' &&
                p[2] == '\n')
        {
            return p;
        }
    }

    return NULL;
}

uint NntpThread::createDateTime(QStringList dateTime)
{
    // Have seen totally invalid date formats here, so take care
    QDate d;
    int i = 0;

    if (dateTime[0].length() == 0)
    {
        d = QDate::currentDate();
        qDebug() << "Invalid date time (1)";
        for (int i = 0; i < dateTime.size(); ++i)
             qDebug() << "Element " << i << " : " << dateTime.at(i).toLocal8Bit().constData();
    }
    else
    {
        i = (dateTime[0].at(dateTime[0].length() - 1) == ',') ? 1 : 0;

        if (dateTime.count() < (i + 3))
        {
            d = QDate::currentDate();
            qDebug() << "Invalid date time (2)";
            for (int i = 0; i < dateTime.size(); ++i)
                 qDebug() << "Element " << i << " : " << dateTime.at(i).toLocal8Bit().constData();
        }
        else
        {
            if (dateTime[i].length() == 2)
                d = QDate::fromString(
                            dateTime[i] + " " + dateTime[i + 1] + " " + dateTime[i + 2],
                        "dd MMM yyyy");
            else if (dateTime[i].length() == 1)
                d = QDate::fromString(
                            dateTime[i] + " " + dateTime[i + 1] + " " + dateTime[i + 2],
                        "d MMM yyyy");
            else
                d = QDate::currentDate();
        }
    }

    QTime t(0, 0);

    if (dateTime.count() >= (i + 4))
        t = QTime::fromString(dateTime[i + 3]);

    QDateTime dt(d, t);

    return dt.toTime_t();
}


bool NntpThread::waitLine( )
{
    if (isNilPeriod)
    {
        //reset the socket...
        qDebug("waitLine(): Now in Nil Period!");
        reset();
        return false;
    }

    quint64 bytesRead = 0;

    while (!m_findEndLine(buffer, watermark))
    {
        //qint64 bytesAvailable = kes->bytesAvailable();
        //qDebug() << "Socket says there are " << bytesAvailable << "bytes available";

        if (!kes->bytesAvailable())
        {
            //qDebug() << "In ReadyRead for thread " << threadId;
            if (kes->waitForReadyRead(1000 * timeout) == false)
            {
                qDebug("Timeout or error waiting for line");
                //TODO: set the error!
                reset();
                error=Comm_Err;
                errorString="Timeout or error waiting for data";

                return false;
            }
        }

        if (!cancel)
        {
            // Guard aganist very long article lines (> 100KB!) that exceeds
            // the allocated buffer space and expand if needed
            // Patch provided by Randy Pearson - Thank dude! :)

            if (bufferSize - (watermark-buffer) == 0)
            {
                qDebug("Warning: Buffer is full. Expanding to %d bytes", bufferSize*2);
                char* newbuff= new char[bufferSize*2];
                memcpy(newbuff, buffer, bufferSize);
                watermark=newbuff+bufferSize;
                Q_DELETE_ARRAY(buffer);
                buffer = newbuff;
                bufferSize *= 2;
            }

            isRatePeriod = kes->isRatePeriod;

            if (!isRatePeriod)
            {
                //qint64 bytesAvailable = kes->bytesAvailable();
                //qDebug() << "2: Socket says there are " << bytesAvailable << "bytes available";

                bytes = kes->read(watermark, (qint64)(bufferSize-(watermark-buffer)));
                // qDebug() << "Read " << bytes << "bytes";
                watermark+=bytes;
                bytesRead += bytes;
                if (!bytesRead)
                {
                    reset();
                    error=Comm_Err;
                    errorString="Unable to read any data after being advised data was present";

                    return false;
                }
            }
            else
            {
                kes->lockMutex();
                qint64 sleepDuration = kes->getSleepDuration();
                qint64 maxBytes = kes->getMaxBytes();
                bytes = kes->read(watermark, qMin<qint64>((qint64)(bufferSize-(watermark-buffer)), maxBytes));
                watermark+=bytes;
                bytesRead += bytes;
                if (!bytesRead)
                {
                    reset();
                    error=Comm_Err;
                    errorString="Unable to read any data after being advised data was present";
                    kes->unlockMutex();

                    return false;
                }
                //qDebug() << QTime::currentTime () << " socket: " << threadId << " has read " << bytes << " bytes";
                kes->incrementBytesRead(bytes);
                kes->unlockMutex();
                //qDebug() << QTime::currentTime () << " socket: " << threadId << " has sleep of " << sleepDuration << " msecs and speed of " <<
                //        qMin<qint64>((qint64)(bufferSize-(watermark-buffer)), maxBytes); // << ", read buffer size " << kes->readBufferSize();
                msleep(sleepDuration);
            }

            *curbytes += bytes;
        }
        else
        {
            //reset the socket...
            qDebug("waitLine(): Cancel!");
            return false;
        }
    }

    return true;
}

bool NntpThread::waitBigLine( )
{
    if (isNilPeriod)
    {
        qDebug("waitLine(): Now in Nil Period!");
        reset();
        return false;
    }

    quint64 bytesRead = 0;

    while (!m_findDotEndLine(bigBuffer, bigWatermark))
    {
        if (!kes->bytesAvailable())
        {
            // qDebug() << "In ReadyRead for thread " << threadId;
            if (kes->waitForReadyRead(1000 * timeout) == false)
            {
                qDebug("Timeout or error waiting for line");
                //TODO: set the error!
                reset();
                error=Comm_Err;
                errorString="Timeout or error waiting for data";

                return false;
            }
        }

        if (!cancel)
        {
            // Guard aganist very long article lines (> 100KB!) that exceeds
            // the allocated buffer space and expand if needed
            // Patch provided by Randy Pearson - Thank dude! :)

            if (bigBufferSize - (bigWatermark-bigBuffer) == 0)
            {
                qDebug("Warning: Buffer is full. Expanding to %d bytes", bigBufferSize*2);
                char* newbuff= new char[bigBufferSize*2];
                memcpy(newbuff, bigBuffer, bigBufferSize);
                bigWatermark=newbuff+bigBufferSize;
                Q_DELETE_ARRAY(bigBuffer);
                bigBuffer = newbuff;
                bigBufferSize *= 2;
            }

            isRatePeriod = kes->isRatePeriod;

            if (!isRatePeriod)
            {
                bytes = kes->read(bigWatermark, (qint64)(bigBufferSize-(bigWatermark-bigBuffer)));
                bigWatermark+=bytes;
                bytesRead += bytes;
                if (!bytesRead)
                {
                    reset();
                    error=Comm_Err;
                    errorString="Unable to read any data after being advised data was present";

                    return false;
                }
            }
            else
            {
                kes->lockMutex();
                qint64 sleepDuration = kes->getSleepDuration();
                qint64 maxBytes = kes->getMaxBytes();
                bytes = kes->read(bigWatermark, qMin<qint64>((qint64)(bigBufferSize-(bigWatermark-bigBuffer)), maxBytes));
                bigWatermark+=bytes;
                bytesRead += bytes;
                if (!bytesRead)
                {
                    reset();
                    error=Comm_Err;
                    errorString="Unable to read any data after being advised data was present";
                    kes->unlockMutex();

                    return false;
                }
                //qDebug() << QTime::currentTime () << " socket: " << threadId << " has read " << bytes << " bytes";
                kes->incrementBytesRead(bytes);
                kes->unlockMutex();
                //qDebug() << QTime::currentTime () << " socket: " << threadId << " has sleep of " << sleepDuration << " msecs and speed of " <<
                //        qMin<qint64>((qint64)(bufferSize-(watermark-buffer)), maxBytes); // << ", read buffer size " << kes->readBufferSize();
                msleep(sleepDuration);
            }

            *curbytes += bytes;
        }
        else
        {
            qDebug("waitLine(): Cancel!");
            return false;
        }
    }

    return true;
}

int NntpThread::m_handleError( int expected, QString received )
{
    switch (expected)
    {
    case NntpThread::pass:
        errorString=received;
        return NntpThread::Auth_Err;
    case NntpThread::group:
        errorString=received;
        if (received.left(3).toInt() == 411)
            return NntpThread::NoSuchGroup_Err;
        else return NntpThread::Other_Err;
    case NntpThread::article:
    case NntpThread::body:
        errorString=received;
        if ((received.left(3).toInt() == NoSuchArticle_Err || received.left(3).toInt() == NoSuchMid_Err) && !received.contains("DMCA removed"))
            return NntpThread::NoSuchArticle_Err;
        else return NntpThread::Other_Err;
    case NntpThread::xover:
        if (received.left(3).toInt() == 501)
            qDebug("501 error to xover command!");
        return NntpThread::Comm_Err;
    default:
        errorString=received;
        return NntpThread::Other_Err;
    }
}

bool NntpThread::m_sendCmd( QString& cmd, int response )
{
    QString s;
    error=NntpThread::No_Err;

    if (kes->bytesAvailable() > 0)
        kes->read(line, lineBufSize);

    //Invalidate contents of the buffer...
    watermark=buffer;

    QByteArray ba = cmd.toLocal8Bit();
    const char *c_str = ba.data();
    kes->write(c_str, cmd.length());

    //Now I can check if the socket is valid...

    if (!kes->isValid()) // may be logging in now
    {
        // TODO - if we lose the connection then we can't just send a body cmd
        // TODO we need a group cmd first - always save group name for a working thread
        // TODO and restore it if the connection is lost
        qDebug() << "thread " << threadId << ", m_sendCmd: Invalid socket for command " << cmd;

        if (!m_connect())
        {
            qDebug() << "m_sendCmd(): Failed to connect ...";
            error = Connection_Err;
            return false;
        }
        else
        {
            kes->write(c_str, cmd.length());
        }
    }

    if (!waitLine())
        return false;

    m_readLine();
    s=line;

    // qDebug() << "Thread " << threadId << ", request : " << cmd << ", response : " << s;

    if (response == s.left(3).toInt())
    {
        return true;
    }
    else
    { //set the error
        error=m_handleError(response, s);
        qDebug() << "m_sendCmd(): Invalid response from server " << nHost->getHostName() \
                 << ": " << s << ", cut down to " << s.left(3) << ", expected " << response << "cmd was " << cmd;

        //empty buffer
        watermark=buffer;
        return false;
    }
}

bool NntpThread::m_connect( )
{
    error=NntpThread::No_Err;
    if (isLoggedIn == true)
        return true;

    newsGroup="";
    watermark=buffer;

    if (! serverConnect(nHost->getHostName(), nHost->getPort()) )
    {
        qDebug("Cannot connect");
        error = Connection_Err;
        return false;
    }

    if (!kes->state() == QAbstractSocket::ConnectedState)
    {
        //socket is not valid, probably a timeout
        qDebug("Invalid socket");
        reset();
        return false;
    }

    //  qDebug("Connected, going on");

    if (!waitLine())
    {
    //  qDebug("Error connecting!!");
        return false;
    }
    //         qDebug("Line found");
    m_readLine();
    QString s = line;
    //        qDebug() << "Got this line: " << s;
    if ( (s.left(3).toInt() != NntpThread::ready) &&
         (s.left(3).toInt() != NntpThread::readyNoPost) )
    {
        qDebug("Bad response code. Response: %d", s.left(3).toInt());
        reset();
        error=NntpThread::Comm_Err;

        errorString=s;
        return false;
    }
    if (nHost->getUserName() == "") {
        //             qDebug("Username NULL, connection finished");
        isLoggedIn=true;
        return true;
    }
    cmd="authinfo user " + nHost->getUserName() + "\r\n";
    //         qDebug("Sending user");
    if (!m_sendCmd(cmd, NntpThread::user )) {
        qDebug("Bad response to the \"user\" cmd");
        reset();
        return false;
    }
    cmd="authinfo pass " + nHost->getPass() + "\r\n";
    //        qDebug("Sending password");
    if (!(m_sendCmd(cmd, NntpThread::pass ))) {
        qDebug("Authentication failed");
        qDebug("Error is: %d", error);
        qDebug() << "Response from server: " << errorString;
        //disconnected
        reset();

        return false;
    }

    isLoggedIn=true;
    //        qDebug("Logged in");

    return true;
}

bool NntpThread::getGroupLimits(QString group)
{
    QString s;
    if (!m_connect())
    {
        qDebug() << "getGroupLimits(): can't connect to  " << nHost->getName();
        return false;
    }

    error=NntpThread::No_Err;
    cmd = "group " + group + "\r\n";

    if (!m_sendCmd(cmd, NntpThread::group))
        return false;

    s=line;
    parse(s.split(" ", QString::KeepEmptyParts));

    qDebug() << "Highwatermark: " << highWatermark << " Lowwatermark: " << lowWatermark << " Articles: " << articles;

    emit SigUpdateLimits(job, lowWatermark, highWatermark, articles);

    if (cancel)
        return false;
    else
        return true;
}

bool NntpThread::getCapabilities()
{
    QString s;
    QString capabilityLine;
    quint64 capabilities = 0;

    if (!m_connect())
    {
        qDebug() << "getCapabilities(): can't connect to  " << nHost->getName();
        return true; // no point trying to queue for another server - this is server specific
    }

    error=NntpThread::No_Err;

    cmd = "help\r\n";

    if (!m_sendCmd(cmd, NntpThread::help))
        return false;

    while (line[0] != '.')
    {
        if (!waitLine())
            return false;

        while (m_readLine())
        {
            if (line[0] == '.')
            {
                break;
            }
            else
            {
                capabilityLine = QString::fromLocal8Bit(line).trimmed();
                qDebug() << "Got help : " << capabilityLine;

                if (capabilityLine.startsWith("xzver", Qt::CaseInsensitive))
                    capabilities |= NntpHost::xzver;
                else if (capabilityLine.startsWith("xfeature compress gzip", Qt::CaseInsensitive))
                    capabilities |= NntpHost::xfeatgzip;
                else if (capabilityLine.startsWith("xfeature-compress gzip", Qt::CaseInsensitive))
                    capabilities |= NntpHost::xfeatgzip;
                else if (capabilityLine.startsWith("capabilities", Qt::CaseInsensitive))
                    capabilities |= NntpHost::capab;
            }
        }
    }

    if (capabilities & NntpHost::capab)
    {
        cmd = "capabilities\r\n";

        if (!m_sendCmd(cmd, NntpThread::capabilities))
            return false;

        while (line[0] != '.')
        {
            if (!waitLine())
                return false;

            while (m_readLine())
            {
                if (line[0] == '.')
                {
                    break;
                }
                else
                {
                    capabilityLine = QString::fromLocal8Bit(line).trimmed();
                    qDebug() << "Got capability : " << capabilityLine;

                    if (capabilityLine.startsWith("xfeature compress gzip", Qt::CaseInsensitive))
                        capabilities |= NntpHost::xfeatgzip;
                    else if (capabilityLine.startsWith("xfeature-compress gzip", Qt::CaseInsensitive))
                        capabilities |= NntpHost::xfeatgzip;
                }
            }
        }
    }

    emit SigExtensions(job, nHost->getId(), capabilities);

    if (cancel)
        return false;
    else
        return true;
}

void NntpThread::charCRC(const unsigned char *c)
{
    crc ^= 0xffffffffL;
    crc=crc_table[((int)crc ^ (*c)) & 0xff] ^ (crc >> 8);
    crc ^= 0xffffffffL;
}

bool NntpThread::getXover(QString group)
{
    QString s;

    HeaderQueue<QByteArray*> headerQueue;

    uint from, to;
    quint64 maxHeaderNum;

    quint64 targetHWM  = 0;

    qint16 hostId = nHost->getId();
    Db* db = job->ng->getDb();

    maxHeaderNum = job->ng->servLocalHigh[hostId];

    if (!m_connect())
    {
        qDebug() << "getXover(): can't connect to  " << nHost->getName();
        return false;
    }

    qDebug() << hostId << ": getXover()";
    error=NntpThread::No_Err;


    if (config->downloadCompressed
            && (nHost->getServerFlags() & NntpHost::xfeatgzip)
            && (nHost->getEnabledServerExtensions() & NntpHost::xfeatgzip))
    {
        cmd="xfeature compress gzip\r\n";
        //Check first line
        if (!m_sendCmd(cmd, NntpThread::gzip)) // xfeatgzip uses the same response code as xover
        {
            qDebug() << "xfeature-compress gzip failed for command: " << cmd;
            return false;
        }

        qDebug() << "xfeature-compress gzip accepted for command: " << cmd;
    }

    cmd = "group " + group + "\r\n";

    if (!m_sendCmd(cmd, NntpThread::group))
        return false;

    s=line;
    //     qDebug("Line: %s", line);
    //parse fills articles, lowW and highW variables...
    parse(s.split(" ", QString::KeepEmptyParts)); // gives HWM, LWM and articles

    qDebug() << hostId << ": Highwatermark: " << highWatermark << " Lowwatermark: " << lowWatermark << " Articles: " << articles;

    int oldHighW = job->ng->servLocalHigh[nHost->getId()];

    // Now grab the articles from the old highw to the new highw...

    if (job->from == 0 && job->to == 0) // update without options (topup)
    {
        if (oldHighW != 0)
        {
            from = oldHighW + 1;
        }
        else
        {
            from = lowWatermark;
        }

        to   = highWatermark;
        articles = to - from + 1;
        targetHWM = highWatermark;
    }
    else
    {
        from = job->from;
        to   = job->to;

        targetHWM = job->to;

        articles = to - from + 1;
    }

    if (from > to) // nothing to do
        return true;

    qDebug() << hostId << ": Getting articles " << from << '-' << to;

    step=articles/100;
    qDebug() << hostId << ": Step = " << step;

    lines=0;
    if (step == 0)
        step=1;

    groupArticles = 0;
    unreadArticles = 0;

    bool workersStarted = false;

    HeaderReadXFeatGzip* headerReadXFeatGzip[NUM_HEADER_WORKERS];
    HeaderReadXzVer* headerReadXzVer[NUM_HEADER_WORKERS];
    HeaderReadNoCompression* headerReadNoCompression[NUM_HEADER_WORKERS];
    HeaderReadWorker* headerReadWorker[NUM_HEADER_WORKERS];

    if (config->downloadCompressed
            && (nHost->getServerFlags() & NntpHost::xfeatgzip)
            && (nHost->getEnabledServerExtensions() & NntpHost::xfeatgzip))
    {
        // Create worker threads here .....
        for (int k=0; k<NUM_HEADER_WORKERS; ++k)
        {
            headerReadXFeatGzip[k] = new HeaderReadXFeatGzip(&headerQueue, job, articles, hostId, headerDbLock);
            headerReadWorker[k] = headerReadXFeatGzip[k];
            connect(this, SIGNAL(startHeaderRead()), headerReadXFeatGzip[k], SLOT(startHeaderRead()));
            connect(this, SIGNAL(setReaderBusy()), headerReadXFeatGzip[k], SLOT(lock()));
            connect(headerReadXFeatGzip[k], SIGNAL(SigUpdate(Job *, uint, uint, uint)), (QObject*)qm, SLOT(update(Job *, uint, uint, uint)));
            connect(headerReadXFeatGzip[k], SIGNAL(sigHeaderDownloadProgress(Job*, quint64, quint64, quint64)), (QObject*)qm, SLOT(slotHeaderDownloadProgress(Job*, quint64, quint64, quint64)));
            headerReadXFeatGzip[k]->moveToThread(&HeaderReadThread[k]);
            connect(this, SIGNAL(quitHeaderRead()), &HeaderReadThread[k], SLOT(quit()));

            HeaderReadThread[k].start();
        }

        emit setReaderBusy();

        bigBufferSize=6 * 1024 * 1024;
        if (!bigBuffer)
            bigBuffer = new char[bigBufferSize];

        if (!inflatedBuffer)
            inflatedBuffer = new char[40 * 1024 * 1024];
        bigWatermark=bigBuffer;

        uint batchSize = 0;

        qDebug() << hostId << ": It's time for header compression type 2";

        int chd=0;
        while ((chd*COMPRESSED_HEADER_THRESHOLD) < (int)articles)
        {
            batchSize = qMin(uint(COMPRESSED_HEADER_THRESHOLD), articles - (chd*COMPRESSED_HEADER_THRESHOLD));

            cmd="xover " + QString::number(from) + "-" + QString::number(from + batchSize - 1) + "\r\n";

            if (!m_sendCmd(cmd, NntpThread::xover))
            {
                qDebug() << "xover failed for command: " << cmd;
                return false;
            }

            qDebug() << "xover accepted for command: " << cmd;

            if (!waitBigLine())
            {
                qDebug() << "xover failed in waitBigLine()";
                return false;
            }

            if (!m_readBigLine())
            {
                qDebug() << "xover failed in m_readBigLine()";
                return false;
            }

            headerQueue.enqueue(bigLine);

            bigWatermark=bigBuffer;

            chd++;
            from += batchSize;

            if (!workersStarted)
            {
                emit startHeaderRead();
                workersStarted = true;
            }
        }
    }
    else if (config->downloadCompressed
             && (nHost->getServerFlags() & NntpHost::xzver)
             && (nHost->getEnabledServerExtensions() & NntpHost::xzver))
    {
        // Create worker threads here .....
        for (int k=0; k<NUM_HEADER_WORKERS; ++k)
        {
            headerReadXzVer[k] = new HeaderReadXzVer(&headerQueue, job, articles, hostId, headerDbLock);
            headerReadWorker[k] = headerReadXzVer[k];
            connect(this, SIGNAL(startHeaderRead()), headerReadXzVer[k], SLOT(startHeaderRead()));
            connect(this, SIGNAL(setReaderBusy()), headerReadXzVer[k], SLOT(lock()));
            connect(headerReadXzVer[k], SIGNAL(SigUpdate(Job *, uint, uint, uint)), (QObject*)qm, SLOT(update(Job *, uint, uint, uint)));
            connect(headerReadXzVer[k], SIGNAL(sigHeaderDownloadProgress(Job*, quint64, quint64, quint64)), (QObject*)qm, SLOT(slotHeaderDownloadProgress(Job*, quint64, quint64, quint64)));
            headerReadXzVer[k]->moveToThread(&HeaderReadThread[k]);
            connect(this, SIGNAL(quitHeaderRead()), &HeaderReadThread[k], SLOT(quit()));

            HeaderReadThread[k].start();
        }

        emit setReaderBusy();

        QString sLine;

        if (!bufferLine)
            bufferLine = new uchar[6 * 1024 * 1024];
        if (!inflatedBuffer)
            inflatedBuffer = new char[30 * 1024 * 1024];

        unsigned int i2;
        uint batchSize = 0;
        uint expectedCRC;
        uint bufferIndex=0;
        int beginCRC;

        bool badCRC=false;

        unsigned char ch;
        const char* ascii_rep = 0;

        qDebug() << hostId << ": It's time for header compression";
        for (int i=0; (i*COMPRESSED_HEADER_THRESHOLD) < (int)articles; ++i)
        {
            batchSize = qMin(uint(COMPRESSED_HEADER_THRESHOLD), articles - (i*COMPRESSED_HEADER_THRESHOLD));

            bufferIndex=0;
            badCRC=false;
            crc= 0L;

            cmd="xzver " + QString::number(from) + "-" + QString::number(from + batchSize - 1) + "\r\n";
            from += batchSize;

            //Check first line
            if (!m_sendCmd(cmd, NntpThread::xover)) // xzver uses the same response code as xover
            {
                qDebug() << "xzver failed for command: " << cmd;
                return false;
            }

            qDebug() << "xzver accepted for command: " << cmd;

            if (!waitLine())
            {
                qDebug() << "xzver failed in waitLine()";
                return false;
            }

            // Get the first line and make sure that it's yy encoded (size is not valid)
            if (m_readLine() == false || qstrncmp( line, "=ybegin", 7 ))
            {
                qDebug() << "xzver failed to find yy encoded input: *" << line << "*";
                return false;
            }

            // We know that it's yy encoded, so decode now
            while (line[0] != '.')
            {
                if (!waitLine())
                {
                    qDebug() << "xzver failed in waitLine()";
                    return false;
                }

                while (m_readLine())
                {
                    if (line[0] == '.')
                    {
                        break;
                    }

                    if (!qstrncmp( line, "=yend", 5 ))
                    {
                        sLine = QString::fromLocal8Bit(line);

                        if ((beginCRC=sLine.indexOf("pcrc32=")) != -1)
                        {
                            //May be a post crc
                            expectedCRC=sLine.mid(beginCRC + 7, 8).toUInt(NULL, 16);
                        }
                        else if ((beginCRC=sLine.indexOf("crc32=")) != -1)
                        {
                            expectedCRC=sLine.mid(beginCRC +6, 8).toUInt(NULL, 16);
                        }

                        if (beginCRC == -1)
                        {
                            qDebug() << "Cannot find crc!\n";
                            expectedCRC=0;
                            badCRC=false;
                        }
                        else
                        {
                            //CRC32 is always 8 chars...
                            //compare part crc...
                            if (crc != expectedCRC)
                            {
                                qDebug() << "Warning: part crc differs!";
                                qDebug() << "Expected CRC: " << expectedCRC << " WrittenCRC: " << crc;
                                badCRC=true;
                            }
                            else
                                badCRC=false;
                        }

                        if (badCRC)
                            return false;
                    }
                    else //It's a data line
                    {
                        ascii_rep = line;

                        for (i2=0; i2<qstrlen(line); ++i2)
                        {
                            if (ascii_rep[i2]=='=')
                            {
                                ch = ascii_rep[++i2] -106;
                            }
                            else
                            {
                                ch=ascii_rep[i2]-42;
                            }
                            bufferLine[bufferIndex++]=ch;
                            charCRC((const unsigned char *) &ch);
                        }
                    }
                }
            }

            lineBA = new QByteArray((const char *)bufferLine, (int)bufferIndex);

            headerQueue.enqueue(lineBA);

            if (!workersStarted)
            {
                emit startHeaderRead();
                workersStarted = true;
            }

            bufferIndex=0;
        }
    }
    else // no compression
    {
        qDebug() << hostId << ": in uncompressed xover";

        // Create worker threads here .....
        for (int k=0; k<NUM_HEADER_WORKERS; ++k)
        {
            headerReadNoCompression[k] = new HeaderReadNoCompression(&headerQueue, job, articles, hostId, headerDbLock);
            headerReadWorker[k] = headerReadNoCompression[k];
            connect(this, SIGNAL(startHeaderRead()), headerReadNoCompression[k], SLOT(startHeaderRead()));
            connect(this, SIGNAL(setReaderBusy()), headerReadNoCompression[k], SLOT(lock()));
            connect(headerReadNoCompression[k], SIGNAL(SigUpdate(Job *, uint, uint, uint)), (QObject*)qm, SLOT(update(Job *, uint, uint, uint)));
            connect(headerReadNoCompression[k], SIGNAL(sigHeaderDownloadProgress(Job*, quint64, quint64, quint64)), (QObject*)qm, SLOT(slotHeaderDownloadProgress(Job*, quint64, quint64, quint64)));
            headerReadNoCompression[k]->moveToThread(&HeaderReadThread[k]);
            connect(this, SIGNAL(quitHeaderRead()), &HeaderReadThread[k], SLOT(quit()));

            HeaderReadThread[k].start();
        }

        emit setReaderBusy();

        cmd="xover " + QString::number(from) + "-" + QString::number(to) + "\r\n";

        //Check first line
        if (!m_sendCmd(cmd, NntpThread::xover))
            return false;

        while (line[0] != '.')
        {
            if (!waitLine())
                return false;

            while (m_readLineBA())
            {
                if (lineBA->at(0) == '.')
                {
                    //                 qDebug("End found");
                    break;
                }
                else
                {
                    headerQueue.enqueue(lineBA);

                    if (!workersStarted)
                    {
                        emit startHeaderRead();
                        workersStarted = true;
                    }
                }
            }
        }
    }

    for (int k=0; k<NUM_HEADER_WORKERS; ++k)
    {
        headerReadWorker[k]->finishedReading = true;
    }

    qDebug() << "Server " << hostId << " has finished reading and will wait for header readers ...";

    for (int k=0; k<NUM_HEADER_WORKERS; ++k)
    {
        headerReadWorker[k]->busyMutex.lock();
        headerReadWorker[k]->busyMutex.unlock();
    }

    qDebug() << "Server " << hostId << " is about to kill header readers ...";

    for (int k=0; k<NUM_HEADER_WORKERS; ++k)
    {
        if (headerReadWorker[k]->error)
        {
            error = headerReadWorker[k]->error;
            errorString = headerReadWorker[k]->errorString;

            emit logMessage(LogAlertList::Error, tr("Server ") + nHost->getName() + " : " + errorString);
        }
        else
            maxHeaderNum = targetHWM;

        maxHeaderNum = qMax(headerReadWorker[k]->maxHeaderNum, maxHeaderNum);
    }

    emit quitHeaderRead();

    job->ng->servLocalHigh[hostId] = qMax(job->ng->servLocalHigh[hostId], maxHeaderNum);

    saveGroup();
    db->sync(0);

    emit sigHeaderDownloadProgress(job, job->ng->servLocalLow[hostId],
                                   job->ng->servLocalHigh[hostId], job->ng->servLocalParts[hostId]);

    for (int k=0; k<NUM_HEADER_WORKERS; ++k)
    {
        Q_DELETE(headerReadWorker[k]);
    }

    if (cancel)
        return false;
    else
        return true;
}

bool NntpThread::saveGroup()
{
    int tret;
    Dbt groupkey, groupdata;

    char* tempng = job->ng->data(); // MD TODO this allocates space each time called !!!
    groupdata.set_data(tempng);
    groupdata.set_size(job->ng->getRecordSize());

    QByteArray ba = job->ng->ngName.toLocal8Bit();
    const char* tempkey = ba.data();
    groupkey.set_data((void*) tempkey);
    groupkey.set_size(job->ng->ngName.length());

    if ((tret = job->gdb->put(NULL, &groupkey, &groupdata, 0)) != 0)
    {
        qDebug() << "Error updating newsgroup: %d" << tret;
        Q_DELETE_ARRAY(tempng);
        return false;
    }

    job->gdb->sync(0);

    Q_DELETE_ARRAY(tempng);

    return true;
}

void NntpThread::parse(QStringList qs)
{
    articles=qs[1].toUInt();
    lowWatermark=qs[2].toUInt();
    highWatermark=qs[3].toUInt();
}

void NntpThread::setQueue(QList<int> *tq, QMap<int, QItem*> *q, QMutex *qLock, int* po)
{
    threadQueue=tq;
    queue=q;
    queueLock=qLock;
    pendingOperations=po;
}

void NntpThread::setDbSync(QMutex *qLock)
{
    headerDbLock = qLock;
}

bool NntpThread::m_readLine()
{
    if ((lineEnd=m_findEndLine(buffer, watermark)))
    {
        lineEnd[0]=0;
        lineEnd[1]=0;
        lineEnd+=2;
        lineSize=lineEnd-buffer;

        if (lineSize > lineBufSize)
        {
            //             qDebug("Line Buffer overflow");
            //             qDebug("LineSize: %d", lineSize);
            lineBufSize=lineSize+1000;
            Q_DELETE_ARRAY(line);
            line=new char[lineBufSize];
            memset(line, 0, sizeof(lineBufSize));
        }
        memcpy(line, buffer, lineSize);

        memmove(buffer,lineEnd,watermark-lineEnd);

        watermark-=lineSize;
        return true;
    }
    else
        return false;
}

bool NntpThread::m_readLineBA()
{
    if ((lineEnd=m_findEndLine(buffer, watermark)))
    {
        lineEnd[0]=0;
        lineEnd[1]=0;
        lineEnd+=2;
        lineSize=lineEnd-buffer;

        lineBA = new QByteArray(buffer, lineSize);

        memmove(buffer,lineEnd,watermark-lineEnd);

        watermark-=lineSize;
        return true;
    }
    else
        return false;
}

bool NntpThread::m_readBigLine()
{
    if ((bigLineEnd=m_findDotEndLine(bigBuffer, bigWatermark)))
    {
        bigLineEnd[0]=0; // '.'
        bigLineEnd[1]=0; // CR
        bigLineEnd[2]=0; // LF
        bigLineEnd+=3;
        bigLineSize=bigLineEnd-bigBuffer;

        bigLine = new QByteArray(bigBuffer, bigLineSize);

        memmove(bigBuffer,bigLineEnd,bigWatermark-bigLineEnd);

        bigWatermark-=bigLineSize;
        return true;
    }
    else
        return false;
}

bool NntpThread::getArticle()
{
    if (!m_connect())
    {
        qDebug() << "getArticle(): can't connect";
        return false;
    }

    error=NntpThread::No_Err;

    if (job->mid.isNull() || job->mid.isEmpty())
    {
        if (newsGroup != job->ng->ngName)
        {
            newsGroup=job->ng->ngName;
            cmd = "group " + newsGroup + "\r\n";

            if (!m_sendCmd(cmd, NntpThread::group))
                return false;
        }


        cmd="body " + QString::number(artNum) + "\r\n";

        if (!m_sendCmd(cmd, NntpThread::body))
            return false;
    }
    else
    {
        QString cmd = "body " + job->mid + "\r\n";

        if (!m_sendCmd(cmd, NntpThread::body))
            return false;
    }

    int partialLines=0;

    prevTime=QTime::currentTime();

    int rlines = 0;

    while(!((line[0] == '.') && (line[1] == '\0')))
    {
        if (!waitLine())
            return false;
        while (m_readLine())
        {
            if (line[0] == '.')
            {
                if (line[1] == '.') {
                    if ( (saveFile->write(line+1, strlen(line+1) ) == -1) || \
                         (saveFile->write("\r\n", 2) == -1) )
                    {
                        //Write error...urgh! :)
                        error=Write_Err;
                        errorString="I/O error";
                        reset();
                        return false;
                    }
                }
                else
                {
                    break;

                    //finished
                }
            }
            else
            {
                if ( (saveFile->write(line, strlen(line) ) == -1) || \
                     (saveFile->write("\r\n",2) == -1)  )
                {
                    //Write error...urgh! :)
                    error=Write_Err;
                    errorString="I/O error";
                    reset();
                    return false;
                }
            }

            //*curbytes+=strlen(line);
            // kes->bytesRead += strlen(line);
            rlines+=strlen(line);  // it's really bytes not lines!!
            //            qDebug() << "rlines = " << rlines << ", increased by strlen(line)";
            currentTime=QTime::currentTime();
        }

        //         if ((articles != 0) && (prevTime.secsTo(currentTime) > 1)) {
        if ( prevTime.secsTo(currentTime) > 1)
        {
            partialLines=rlines-partialLines;
            //            qDebug() << "Sending rlines " << rlines << ", articles " << articles;
            emit SigUpdatePost(job, rlines, articles, partialLines, qId);
            prevTime=QTime::currentTime();
            partialLines=rlines;
        }
    }

    //send a "final" progress message, to correctly update the post lines
    partialLines=rlines-partialLines;
    emit SigUpdatePost(job, rlines, articles, partialLines, qId);
    prevTime=QTime::currentTime();

    queueLock->lock();
    job->status=Job::Finished_Job;
    queueLock->unlock();

    if ( cancel  )
        return false;
    else
        return true;
}

bool NntpThread::getListOfGroups( )
{
    qDebug("getListOfGroups(): Entered");

    if (!m_connect()) {
        qDebug("getListOfGroups(): can't connect");
        return false;
    }

    error=NntpThread::No_Err;
    cmd = "list\r\n";

    if (!m_sendCmd(cmd, NntpThread::list))
        return false;

    while (!(line[0] == '.') && !(line[1] == '\0')  )
    {
        if (!waitLine())
            return false;

        while (m_readLine())
        {
            if ( (line[0] == '.') && (line[1] == '\0') )
                break;
            else
            {
                //*curbytes += strlen(line);
                // kes->bytesRead += strlen(line);

                // put the group in the group db...

                if (dbGroupPut(db, line, nHost->getId()) != 0) {
                    qDebug("dbGroupPut() failed");

                }
            }
        }
    }
    //Update NewsGroup

    db->sync(0);
    if (cancel)
        return false;
    else
        return true;
}

int NntpThread::dbGroupPut( Db * db, const char *line, int hostId )
{
    int ret;
    Dbt listkey, listdata;

    listdata.set_flags(DB_DBT_MALLOC);
    //build the key...
    QString s=line;
    QStringList fields=s.split(' ', QString::KeepEmptyParts);
    QString ngName=fields[0];
    int articles = 0;
    articles = fields[1].toInt() - fields[2].toInt() + 1;
    if (articles < 0)
        articles = 0;
    //     qDebug() <<"Articles: " << articles;
    //     QString desc=fields[1];
    QByteArray ba = ngName.toLocal8Bit();
    const char *i = ba.data();
    listkey.set_data((void*)i);
    listkey.set_size(ngName.length());
    //     qDebug() << "Articles on " << ngName << ": " << articles;

    //     memcpy(keymem, i, ngName.length());

    //     key->set_size(ngName.length());
    ret=db->get(NULL, &listkey, &listdata, 0);
    if (ret == ENOMEM)
        qDebug("Memory error?? WTF??");
    if (ret == DB_NOTFOUND) {

        //Not found, create new group and insert into db
        AvailableGroup *g=new AvailableGroup(ngName, hostId, articles);
        //         g->setArticles(hostId, articles);
        //save into db...
        char *p=g->data();

        //key is already built, build data
        // memcpy(datamem, p, g->size());
        // data->set_size(g->size());
        // delete[] p;

        listdata.set_data(p);
        listdata.set_size(g->size());

        ret=db->put(NULL, &listkey, &listdata, 0);
        if (ret != 0)
            qDebug("Error inserting into groups db: %d", ret);
        Q_DELETE_ARRAY(p);
        return ret;

    } 
    else if (ret == 0) 
    {
        //found, update and resave
        AvailableGroup *g = new AvailableGroup((char*)listdata.get_data());
        void* ptr = listdata.get_data();
        Q_FREE(ptr);
        g->addHost(hostId);
        g->setArticles(hostId, articles);
        char *p=g->data();
        // memcpy(datamem, p, g->size());
        // data->set_size(g->size());
        listdata.set_data(p);
        listdata.set_size(g->size());

        ret=db->put(NULL, &listkey, &listdata, 0);
        if (ret != 0)
            qDebug("Error updating group db: %d", ret);
        Q_DELETE_ARRAY(p);
        Q_DELETE(g);
        return ret;
    }
    else
        qDebug("WTF??, %d", ret);

    return -1;
}

void NntpThread::reset()
{
    qDebug() << "Resetting thread " << threadId;

    freeSocket();
    partReset();
}

void NntpThread::partReset()
{
    cancel=false;
    pause=false;
    status=Ready;
    error=NntpThread::No_Err;

    watermark=buffer; //buffer emptied :)
    newsGroup="";
}

void NntpThread::freeSocket()
{
    if (kes)
    {
        if (kes->isRegistered)
        {
            kes->isRegistered = false;
            emit unregisterSocket(kes);
        }
        kes->close();
        Q_DELETE(kes);
        kes = 0;

        qDebug() << "Deleted socket";
    }

    isLoggedIn=false;
}

bool NntpThread::serverConnect( QString & addr, quint16 port )
{
    // for OS\2 - occasionally gives QAbstractSocket::UnfinishedSocketOperationError
    if (kes->state())
        kes->abort();

    bool connected = false;
    int sslSocket = nHost->getSslSocket();

    if (sslSocket == 0)
    {
        kes->connectToHost(addr, port);
        if (kes->waitForConnected(1000 * timeout))
        {
            qDebug("TCP connected! : %d", threadId);
            connected = true;
        }
        else
        {
            qDebug() << "TCP connect problem: " << threadId << ", " << kes->error();

            // This is a situation that I can only reproduce on OS\2
            if (kes->error() == QAbstractSocket::UnfinishedSocketOperationError)
            {

                //#if defined(__OS2__)
                // the error often appears to be spurious .... sleep for timeout ????
                //                connected = true;
                //#  endif
                kes->abort();
                kes->connectToHost(addr, port);

                if (kes->waitForConnected(1000 * timeout))
                {
                    qDebug("TCP connected at second attempt! : %d", threadId);
                    connected = true;
                }
            }
        }
    }
    else
    {
        kes->setProtocol((QSsl::SslProtocol)nHost->getSslProtocol());
        kes->connectToHostEncrypted(addr, port);
        kes->ignoreSslErrors(nHost->getExpectedSslErrors());
        qDebug() << "Setting ignore SSL errs count to: " << nHost->getExpectedSslErrors().size() << " for server " << qId;

        if (kes->waitForEncrypted(1000 * timeout))
        {
            qDebug("SSL connected");
            connected = true;
        }
        else
            qDebug() << "SSL connect problem: " << kes->error() << ", " << kes->errorString();
    }

    // state changed or cancelled?
    //Cancelled?
    if (cancel)
        return false;

    // qDebug() << "Mode = " << kes->mode();
    if (sslSocket == 1)
    {
        ; //qDebug() << "Cipher = " << kes->sslConfiguration().sessionCipher();
    }

    if (connected)
        return true;

    if (sslSocket == 0)
    {
        //Ok, not cancelled, work out the error
        QAbstractSocket::SocketError err = kes->error();
        qDebug() << "TCP failure, error: " << err;

        switch (err)
        {
        case QAbstractSocket::SocketTimeoutError:
            //No error, it's a timeout
            errorString="Timeout";
            error=NntpThread::Timeout_Err;
            break;
        case QAbstractSocket::SocketAccessError:
            errorString="OS or firewall prohibited the connection";
            error=NntpThread::Comm_Err;
            break;
        case QAbstractSocket::ConnectionRefusedError:
            errorString="Connection refused";
            error=NntpThread::Comm_Err;
            break;
        case QAbstractSocket::NetworkError:
            errorString="Network failure while connecting";
            error=NntpThread::Comm_Err;
            break;
        default:
            errorString="TCP unknown error while connecting: " + QString::number(err);
            error=NntpThread::Comm_Err;
            break;
        }
    }
    else
    {
        sslErrs = kes->sslErrors();

        if (validate == false)
        {
            qDebug() << "SSL failure";

            foreach(QSslError sslErr, sslErrs)
            {
                qDebug() << "SSL error : " << (int)((enum QSslError::SslError)sslErr.error()) << " : " << sslErr.errorString();
                qDebug() << tr("The following error cannot be ignored at this version : ")
                         << (int)((enum QSslError::SslError)sslErr.error()) << " : " << sslErr.errorString();
            }

            errorString="SSL error while connecting";
            error=NntpThread::Comm_Err;
        }
        else // MD TODO validate will not have expectedSslErrors set the first time through
        {
            QMap<qint16, quint64> sslErrorMap = nHost->getSslErrorMap();
            quint64  sslAllowableErrors = nHost->getSslAllowableErrors();
            quint64 sslAllErrors = 0;
            QSslCertificate sslCertificate = *(nHost->getSslCertificate());


            foreach(QSslError sslErr, sslErrs)
            {
                if (sslErrorMap.contains((int)((enum QSslError::SslError)sslErr.error())))
                    sslAllErrors |= sslErrorMap.value((int)((enum QSslError::SslError)sslErr.error()));
                else
                {
                    qDebug() << "Unexpected SSL error : " << ((int)((enum QSslError::SslError)sslErr.error()));
                    sslAllErrors |= sslErrorMap.value(-1);
                }

                qDebug() << "SSL error : " << (int)((enum QSslError::SslError)sslErr.error());

                if (!sslErr.certificate().isNull())
                    newCertificate = sslErr.certificate();
            }

            // qDebug() << "Errors = " << sslAllErrors << ", allowable = " << sslAllowableErrors << " for server " << qId;

            if (sslAllErrors == sslAllowableErrors && // Nothing has changed ...
                    newCertificate == sslCertificate)
            {
                qDebug() << nHost->getHostName() << " has matching SSL certificate and ignore errors of " << sslAllowableErrors;
                nHost->setExpectedSslErrors(sslErrs);
            }
            else
            {
                nHost->setNewCertificate(&newCertificate);
            }

            nHost->setSslAllErrors(sslAllErrors);
        }
    }

    reset();
    return false;
}

// Below are called from the controlling thread - not the worker


NntpThread::NntpThread(uint serverId, uint _threadId, uint* cb, bool _isRatePeriod, bool _isNilPeriod, bool _validated, bool _validate, NntpHost *nh,
                       QMgr* _qm)
{
    qId = serverId;
    threadId = _threadId;
    isRatePeriod = _isRatePeriod;
    isNilPeriod = _isNilPeriod;

    cancel=false;
    pause=false;
    connClose=false;
    status=Ready;
    validated = _validated;
    validate = _validate;

    lineBufSize=1000;
    line=new char[lineBufSize];
    memset(line, 0, sizeof(lineBufSize));
    bufferSize=100000;
    buffer=new char[bufferSize];
    watermark=buffer;

    bigBuffer = 0;
    inflatedBuffer = 0;
    bufferLine = 0;

    error=NntpThread::No_Err;

    isLoggedIn=false;
    newsGroup="";
    //Thread...
    curbytes=cb;
    //     retries=0;
    nHost=nh;
    qm = _qm;
    saveFile=0;
    datamemSize=DATAMEM_SIZE;

    kes = 0;

    if (nh)
        timeout=nh->getTimeout();
}

NntpThread::~ NntpThread( )
{
    Q_DELETE_ARRAY(line);
    Q_DELETE_ARRAY(buffer);
    Q_DELETE_ARRAY(bigBuffer);

    Q_DELETE_ARRAY(inflatedBuffer);

    Q_DELETE_ARRAY(bufferLine);

    Q_DELETE(saveFile);

    if (kes)
    {
        if (kes->isRegistered)
        {
            emit unregisterSocket(kes);
            while (kes->isRegistered) // make sure the RateController has let go of our pointer
                msleep(250);
        }
    }
}

void NntpThread::closeConnection(bool finally)
{
    if (!isLoggedIn)
        return;

    error=NntpThread::No_Err;

    if (finally) // It's a shutdown ...
    {
        if (kes)
        {
            kes->isRatePeriod = false;
            kes->isNilPeriod = false;
            emit unregisterSocket(kes);
        }
        connClose=true;
    }
    else // It's an idle timeout
    {
        if (kes)
        {
            kes->close();
            Q_DELETE(kes);
            kes = 0;
        }

        isLoggedIn=false;

        cancel=false;
        pause=false;
        status=Ready;
        error=NntpThread::No_Err;

        watermark=buffer; //buffer emptied :)
        newsGroup="";
    }
}

void NntpThread::setValidated(bool _validated )
{
    // qDebug() << "Setting validated to " << _validated;
    validated=_validated;
    if (isRunning() || isNilPeriod)
        return;

    if (validated)
    {
        //qDebug() << "Status is " << status;
        if (status == Ready)
        {
            status=Working;
            // qDebug() << "About to start thread after validation. Parent object: " << QThread::currentThreadId();
            start();
        }
    }
}

void NntpThread::setRatePeriod(bool _isRatePeriod )
{
    isRatePeriod=_isRatePeriod;
}

void NntpThread::setNilPeriod(bool _isNilPeriod )
{
    isNilPeriod=_isNilPeriod;
}

void NntpThread::tCancel( )
{
    cancel=true;
}

void NntpThread::tPause( )
{
    pause=true;
}

void NntpThread::tResume( )
{
    if (isRunning())
    {
        qDebug("Thread running");
        return;
    }

    if (isNilPeriod)
    {
        qDebug("Thread is in no_activity period");
        return;
    }

    cancel=false;

    watermark=buffer;

    error=NntpThread::No_Err;

    if (kes)
    {
        kes->close();
        Q_DELETE(kes);
        kes = 0;
    }

    isLoggedIn=false;
    newsGroup="";
    nHost=0;
    saveFile=0;
    datamemSize=DATAMEM_SIZE;

    if (status != Delayed_Delete)
    {
        qDebug("Resuming thread");
        pause=false;
        connClose = false;
        status=Working;
        start();
    }
}

void NntpThread::tStart()
{
    //Start the thread...
    if (isRunning() || isNilPeriod)
        return;

    if (validated || validate)
    {
        if (status == Ready)
        {
            status=Working;
            //qDebug() << "About to start thread. Parent object: " << QThread::currentThreadId();
            start();
        }
    }
}

void NntpThread::tValidate()
{
    validate = true;
}
