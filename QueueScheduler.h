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

#ifndef QUEUESCHEDULER_H_
#define QUEUESCHEDULER_H_

#include <QString>
#include <QDate>
#include <QDateTime>
#include <QTimer>
#include <QList>
#include <QMultiMap>
#include <db_cxx.h>
#include "common.h"

struct QueuePeriod
{
	quint32 startSecs; // since Mon 00:00
	quint32 endSecs;   // since Mon 00:00
};

class QueueScheduler;

class QueueScheduleElement
{
public:
    QueueScheduleElement(QueueScheduler*);
	virtual ~QueueScheduleElement();

	void setId(quint16 _id) { id = _id; }
	void setStart(quint32 _start);
	void setEnd(quint32 _end);
	void setSpeed(quint64 _speed) { restrictedBytesPerSec = _speed; }
	void setDisplayGranularity(quint8 _displayGranularity) { displayGranularity = _displayGranularity; }

	quint16 getId() { return id; }
	quint32 getStartSecs() { return ElementStartEnd.startSecs; }
	QString getStart() { return startDateTime.toString("ddd hh:mm:ss"); }
	quint32 getEndSecs() { return ElementStartEnd.endSecs; }
	QString getEnd() { return endDateTime.toString("ddd hh:mm:ss"); }
	quint64 getSpeed() { return restrictedBytesPerSec; }
	quint8 getDisplayGranularity() { return displayGranularity; }

private:
	QueueScheduler* queueScheduler;

	quint16    id;
	struct QueuePeriod ElementStartEnd;
	quint64 restrictedBytesPerSec;
	quint8    displayGranularity;                        // 0 = Kb/Sec, 1 = Mb/Sec

	// Don't save any of the items below
	QDateTime startDateTime;
	QDateTime endDateTime;
};

class QueueSchedule
{
public:
    QueueSchedule(QueueSchedule&);
    QueueSchedule(Db*, uchar*, QueueScheduler*);
	QueueSchedule(Db*, QString, QueueScheduler*);
	virtual ~QueueSchedule();

	QString getName() { return scheduleName; }
	bool getIsActive() { return isActive; }
	quint32 getPriority() { return priority; }
	QueueScheduleElement* getElement(quint16 id);
	QMap<quint32, QueueScheduleElement*>* getElements() { return &queuePeriods; }
	bool dbSave();
	quint16 saveItemSize();
	char* data();

	void setMaxId(quint16 _maxId) { maxId =_maxId; }
	void setName(QString _scheduleName) { scheduleName = _scheduleName; }
	void setIsActive(bool _isActive) { isActive = _isActive; }
	void setPriority(quint32 _priority) { priority = _priority; }
	int  addElement(quint32, quint32, quint64, quint8);
	void addElement(quint32, quint32, quint64, quint8, quint16);
	void removeElement(quint16);
	void removeAllElements();

private:

	quint16 maxId;

	QString scheduleName;
	bool    isActive;
	quint32 priority;
	QMap<quint32, QueueScheduleElement*> queuePeriods; // by startSecs
	QMap<quint16, QueueScheduleElement*> queuePeriodsById;

	QueueScheduler* queueScheduler;
	Db*     schedDb;
	char *savedName;
};

class QueueScheduler : public QObject
{
    Q_OBJECT

public:
	QueueScheduler();
	virtual ~QueueScheduler();
	void    loadData(Db* _schedDb, bool);
	void    setCurrentSchedule(QueueSchedule* _currentSchedule) { currentSchedule = _currentSchedule; }
	void    updateCurrentSchedule(QueueSchedule& newSchedule);
	void    addNewSchedule(QueueSchedule& newSchedule);

	void    removeCurrentSchedule();
	Db*     getDb() { return schedDb; }
	bool    getIsRatePeriod() {return isRatePeriod;}
	bool    getIsNilPeriod() {return (restrictedSpeed == 0)?true:false;}
	qint64  getSpeed() { return restrictedSpeed; }
	bool    scheduleNameExists(QString&);
	QueueSchedule* getSchedule(const QString&);
	QueueSchedule* getCurrentSchedule() { return currentSchedule; }
	QList<QueueSchedule*>& getSchedules() { return queueSchedules; }
	QDateTime* getCurrentMonday() { return &currentMonday; }

	bool dbDelete(QString);

signals:
	void sigSpeedChanged(qint64);

public slots:

    void timerExpired();

private:

    void managePeriods();

	enum CurrentState { NOT_ENABLED = 0, WAITING_FOR_START, WAITING_FOR_END };

	QueueSchedule* currentSchedule;
    QList<QueueSchedule*> queueSchedules;
    QMultiMap<quint32,QueueSchedule*> enabledSchedules; // by priority
	QTimer  nextBoundary;
	quint16 currentState;
	bool    isRatePeriod;
	Db*     schedDb;

	qint64    restrictedSpeed;

	QDateTime currentTime;
	QDateTime currentMonday;
};

#endif /* QUEUESCHEDULER_H_ */
