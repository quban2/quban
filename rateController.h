/***************************************************************************
 Copyright (C) 2011-2015 by Martin Demet
 quban100@gmail.com

 This file is part of Quban.

 This file was adapted from rctcpsocket which is part of the
 documentation of Qt.

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

#ifndef RATECONTROLLER_H_
#define RATECONTROLLER_H_

#include <QSet>
#include <QTime>
#include <QTimer>
#include <QMutex>
#include <QSslSocket>

class RcSslSocket : public QSslSocket
{
    Q_OBJECT

public:
    RcSslSocket(QObject *parent = 0);
    RcSslSocket(bool _isRatePeriod = false, bool _isNilPeriod = false, QObject *parent = 0);
    ~RcSslSocket();

public:
    volatile bool isActive;
    volatile bool isRatePeriod;
    volatile bool isNilPeriod;
    volatile bool isRegistered;

    void setSleepDuration(qint64 s) { msleepDuration = s; }
    void setMaxBytes(qint64 m) { maxBytes = m; }
    void setBytesRead(qint64 r) { bytesRead = r; }
    void incrementBytesRead(qint64 r) { bytesRead += r; }

    qint64 getSleepDuration() { return msleepDuration; }
    qint64 getMaxBytes() { return maxBytes; }
    qint64 getBytesRead() { return bytesRead; }

    void lockMutex() { socketMutex.lock(); }
    void unlockMutex() { socketMutex.unlock(); }

private:
    qint64 msleepDuration;
    qint64 maxBytes;
    qint64 bytesRead;

    QMutex socketMutex;
};

class RateController : public QObject
{
    Q_OBJECT
public:
    RateController(bool _isRatePeriod, qint64 _downLimit, QObject *parent = 0);

    inline qint64 downloadLimit() const { return downLimit; }

public slots:

    void addSocket(RcSslSocket *socket);
    void removeSocket(RcSslSocket *socket);
    void slotSpeedChanged(qint64);
    void slotUpdateSpeed();
    void setDownloadLimit(qint64 bytesPerSecond);
    void setRatePeriod(bool _isRatePeriod);

signals:
    void sigNilPeriodChange(qint64, qint64);

private:
    QTimer *timer;
    QSet<RcSslSocket *> pendingSockets;
    QSet<RcSslSocket *> sockets;
    qint64 downLimit;

    qint64 currentSleep;
    qint64 maxBytes;

    bool isRatePeriod;
    bool isNilPeriod;
};

#endif /* RATECONTROLLER_H_ */
