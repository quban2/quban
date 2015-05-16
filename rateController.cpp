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

#include <QTimer>
#include <QDebug>
#include <QtDebug>
#include "common.h"
#include "quban.h"
#include "rateController.h"

extern Quban* quban;

#define RATE_CONTROLLER_SLEEP  2800
#define RC_SOCKET_START_SLEEP  611
#define RC_SOCKET_SLEEP_ADJUST 50

RcSslSocket::RcSslSocket(QObject *parent) :
    QSslSocket(parent), isActive(false)
{
    isRatePeriod = false;
    isNilPeriod = false;
    isRegistered = false;
    msleepDuration = 0;
    maxBytes = 0;
    bytesRead = 0;
}

RcSslSocket::RcSslSocket(bool _isRatePeriod, bool _isNilPeriod, QObject *parent) :
		QSslSocket(parent), isActive(false), isRatePeriod(_isRatePeriod), isNilPeriod(_isNilPeriod)
{
    isRegistered = false;
    msleepDuration = 0;
    maxBytes = 0;
    bytesRead = 0;
}

RateController::RateController(bool _isRatePeriod, qint64 _downLimit, QObject *parent) : QObject(parent), downLimit(_downLimit),
		isRatePeriod(_isRatePeriod)
{
	currentSleep = RC_SOCKET_START_SLEEP;
	maxBytes = 0;

	if (downLimit == 0)
		isNilPeriod = true;
	else
		isNilPeriod = false;

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(slotUpdateSpeed()));

    if (isRatePeriod && !isNilPeriod)
    	timer->start(RATE_CONTROLLER_SLEEP);
}

void RateController::addSocket(RcSslSocket *socket)
{
	sockets << socket;

	if (isRatePeriod && !isNilPeriod)
	{
    	socket->setReadBufferSize(downLimit * 2);

    	// The max bytes figure is just a guess at this point
        socket->setSleepDuration(RC_SOCKET_START_SLEEP);
        socket->setMaxBytes(17066);
	}

	socket->isRegistered = true;
	// qDebug() << "Registered socket: " << socket;
}

void RateController::removeSocket(RcSslSocket *socket)
{
	// qDebug() << "Unregistered socket: " << socket;
	sockets.remove(socket);	
}

void RateController::setRatePeriod(bool _isRatePeriod)
{
	isRatePeriod = _isRatePeriod;

	foreach (RcSslSocket *client, sockets)
	{
        client->lockMutex();
        client->isRatePeriod = isRatePeriod;
        client->isNilPeriod =  isNilPeriod;
        client->unlockMutex();
	}

	if (isRatePeriod && !isNilPeriod)
		timer->start(RATE_CONTROLLER_SLEEP);
	else
		timer->stop();
}

void RateController::slotSpeedChanged(qint64 newSpeed) // This comes from the scheduler, -1 means not restricted
{
	if ((downLimit == 0 && newSpeed !=0) ||
	    (downLimit != 0 && newSpeed ==0))
	{
		emit sigNilPeriodChange(downLimit, newSpeed);
	}

	if (newSpeed == -1)
	{
		setRatePeriod(false);
		setDownloadLimit(0);
	}
	else
	{
		if (newSpeed == 0)
			isNilPeriod = true;
		else
			isNilPeriod = false;

		setRatePeriod(true);
		setDownloadLimit(newSpeed);
	}
}

void RateController::slotUpdateSpeed()
{
	// qDebug() << "In RateController::slotUpdateSpeed";

	if (!isRatePeriod || isNilPeriod  || sockets.isEmpty())
        return;

	qint64 bytesRead = 0;

	pendingSockets.clear();
	foreach (RcSslSocket *client, sockets)
	{
        client->lockMutex();
        bytesRead += client->getBytesRead();
        client->setBytesRead(0);
        client->unlockMutex();

		if (client->isActive)
			pendingSockets << client;
		else
		{
            client->setSleepDuration(0);
		}
	}

	qDebug() << "Found " << pendingSockets.size() << " active sockets and bytes read of " << bytesRead;

	if (pendingSockets.isEmpty())
		return;

	int actualSpeed = (int)((bytesRead / (float)(RATE_CONTROLLER_SLEEP / 1000))); // Speed in bytes per second

	qint64 sleepAdjust = 0;
	qDebug() << "Our limit is " << downLimit << ", actual speed is " << actualSpeed;

	if (actualSpeed > downLimit) // going too fast
		sleepAdjust += RC_SOCKET_SLEEP_ADJUST;
	else if (actualSpeed < downLimit) // going too slow
		sleepAdjust -= RC_SOCKET_SLEEP_ADJUST;

	currentSleep += sleepAdjust;
	currentSleep = qMax<qint64>(currentSleep, (quint64)0);
	maxBytes = downLimit / pendingSockets.size();

	qDebug() << "Setting sleep to: " << currentSleep << ", speed to " << maxBytes << " for " << pendingSockets.size() << " threads";

	QSetIterator<RcSslSocket *> it(pendingSockets);
	while (it.hasNext())
	{
		RcSslSocket *socket = it.next();

        socket->lockMutex();
        socket->setSleepDuration(currentSleep);
        socket->setMaxBytes(maxBytes);
        socket->unlockMutex();

		pendingSockets.remove(socket);
	}
}

void RateController::setDownloadLimit(qint64 bytesPerSecond)
{
	downLimit = bytesPerSecond;
	if (isRatePeriod && downLimit > 0 && !isNilPeriod)
	{
		foreach (RcSslSocket *socket, sockets)
		{
			socket->setReadBufferSize(downLimit * 2);
            socket->isRatePeriod = true;
		}

        quban->getLogEventList()->logEvent(tr("Download limit set to ") + bytesPerSecond/1024 + tr(" kilobytes per second"));
	}
}
