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
#include "common.h"
#include "QueueScheduler.h"

QueueScheduleElement::QueueScheduleElement(QueueScheduler* _queueScheduler)
{
	queueScheduler = _queueScheduler;
}

QueueScheduleElement::~QueueScheduleElement()
{
}

void QueueScheduleElement::setStart(quint32 _start)
{
	ElementStartEnd.startSecs = _start;
	startDateTime = queueScheduler->getCurrentMonday()->addSecs((int)_start);
}

void QueueScheduleElement::setEnd(quint32 _end)
{
	ElementStartEnd.endSecs = _end;
	endDateTime = queueScheduler->getCurrentMonday()->addSecs((int)_end);
}

QueueSchedule::QueueSchedule(Db* _schedDb, uchar* dbData, QueueScheduler* _queueScheduler)
{
	queueScheduler = _queueScheduler;
	schedDb = _schedDb;

	maxId = 0;

    quint32 elementsSize = 0;
    quint32 strSize = 0;
    int sz8  = sizeof(qint8);
    int sz16 = sizeof(qint16);
    int sz32 = sizeof(qint32);
    int sz64 = sizeof(qint64);
    char *temp;
    uchar *i = dbData;

    memcpy(&strSize, i, sz32);
    i += sz32;

    temp = new char[strSize + 1];
    memcpy(temp, i, strSize);
    temp[strSize] = '\0';
    scheduleName = temp;
    Q_DELETE_ARRAY(temp);
    i += strSize;

    memcpy(&isActive, i, sizeof(bool));
    i += sizeof(bool);

    memcpy(&priority, i, sz32);
    i += sz32;

    // Now the elements
    memcpy(&elementsSize, i, sz32);
    i += sz32;

    // qDebug() << "Loading schedule " << scheduleName << ", with num elements = " << elementsSize;
    QueueScheduleElement* element;
    quint8  element8;
    quint16 element16;
    quint32 elementInt;
    qint64 elementQint64;

    for (quint32 j=0; j<elementsSize; ++j)
    {
        element = new QueueScheduleElement(queueScheduler);

        memcpy(&elementInt, i, sz32);
        i += sz32;

        element->setStart(elementInt);

        memcpy(&elementInt, i, sz32);
        i += sz32;

        element->setEnd(elementInt);

        memcpy(&elementQint64, i, sz64);
        i += sz64;

        element->setSpeed(elementQint64);

        memcpy(&element8, i, sz8);
        i += sz8;

        element->setDisplayGranularity(element8);

        memcpy(&element16, i, sz16);
        i += sz16;

        element->setId(element16);
        if (elementInt > maxId)
            maxId = elementInt;

        queuePeriods.insert(element->getStartSecs(), element);
        queuePeriodsById.insert(elementInt, element);
    }

}

QueueSchedule::QueueSchedule(QueueSchedule& orig) // Only used in the dialog
{
	maxId          = orig.maxId;
	scheduleName   = orig.scheduleName;
	isActive       = orig.isActive;
	priority       = orig.priority;
	queueScheduler = orig.queueScheduler;
	schedDb        = orig.schedDb;

	QueueScheduleElement* element;
	QMapIterator<quint32, QueueScheduleElement*> it(*(orig.getElements()));

	while (it.hasNext())
	{
	    it.next();
	    element = new QueueScheduleElement(*it.value());
	    qDebug() << "Added element with id: " << element->getId();
	    queuePeriods.insert(element->getStartSecs(), element);
	    queuePeriodsById.insert(element->getId(), element);
	}
}

QueueSchedule::QueueSchedule(Db* _schedDb, QString _scheduleName, QueueScheduler* _queueScheduler)
{
	queueScheduler = _queueScheduler;
	schedDb      = _schedDb;
	scheduleName = _scheduleName;
	isActive     = false;
	priority     = 0;
	maxId        = 0;
}

QueueSchedule::~QueueSchedule()
{
	queuePeriodsById.clear();
	qDeleteAll(queuePeriods);
	queuePeriods.clear();
}

int QueueSchedule::addElement(quint32 startT, quint32 endT, quint64 speed, quint8 granularity)
{
	if (startT > endT)
	{
		qDebug() << "Invalid element start: " << startT << " is after end: " << endT;
		return 0;
	}

	if (endT > (10080 * 60))
	{
		qDebug() << "Invalid element end (greater than max of 10080): " << endT;
		return 0;
	}

	QueueScheduleElement* element = new QueueScheduleElement(queueScheduler);
	element->setStart(startT);
	element->setEnd(endT);
	element->setSpeed(speed);
	element->setDisplayGranularity(granularity);
	element->setId(++maxId);

	queuePeriods.insert(startT, element);
	queuePeriodsById.insert(maxId, element);

	qDebug() << "Added element id " << maxId;

	return maxId;
}

void QueueSchedule::addElement(quint32 startT, quint32 endT, quint64 speed, quint8 granularity, quint16 _id)
{
	if (startT > endT)
	{
		qDebug() << "Invalid element start: " << startT << " is after end: " << endT;
		return;
	}

	if (endT > (10080 * 60))
	{
		qDebug() << "Invalid element end (greater than max of 10080): " << endT;
		return;
	}

	QueueScheduleElement* element = new QueueScheduleElement(queueScheduler);
	element->setStart(startT);
	element->setEnd(endT);
	element->setSpeed(speed);
	element->setDisplayGranularity(granularity);
	element->setId(_id);

	queuePeriods.insert(startT, element);
	queuePeriodsById.insert(_id, element);

	return;
}

void QueueSchedule::removeElement(quint16 id)
{
	if (queuePeriodsById.contains(id))
	{
		QueueScheduleElement* element = queuePeriodsById.take(id);
		if (queuePeriods.contains(element->getStartSecs()))
			queuePeriods.remove(element->getStartSecs());

        Q_DELETE(element);
	}
}

void QueueSchedule::removeAllElements()
{
	queuePeriodsById.clear();
	qDeleteAll(queuePeriods);
	queuePeriods.clear();
}

QueueScheduleElement* QueueSchedule::getElement(quint16 id)
{
	if (queuePeriodsById.contains(id))
		return queuePeriodsById.value(id);
	else
		return 0;
}

quint16 QueueSchedule::saveItemSize()
{
	return (3 * sizeof(quint32)) +
		   (1 * sizeof(bool))  +
           scheduleName.size() +
           (queuePeriods.size() * ((2 * sizeof(quint32)) + sizeof(quint16) + sizeof(quint8) + sizeof(quint64)));
}

char* QueueSchedule::data()
{
	quint32 strSize;
	QByteArray ba;

	char *p = new char[saveItemSize()];
	char *i = p;

	int sz8  = sizeof(qint8);
	int sz16 = sizeof(qint16);
	int sz32 = sizeof(qint32);
	int sz64 = sizeof(qint64);

	strSize = scheduleName.size();
	memcpy(i, &strSize, sz32);
	i += sz32;

	ba = scheduleName.toLocal8Bit();
	savedName = ba.data();
	memcpy(i, savedName, strSize);
	i += strSize;

	memcpy(i, &isActive, sizeof(bool));
	i += sizeof(bool);

	memcpy(i, &priority,sz32);
	i += sz32;

    // Now the elements
	quint32 elements = queuePeriods.size();
	memcpy(i, &elements, sz32);
	i += sz32;

	QueueScheduleElement* element;
	quint8  element8;
	quint16 element16;
	quint32 elementInt;
	qint64 elementQint64;

	QMapIterator<quint32, QueueScheduleElement*> it(queuePeriods);
	while (it.hasNext())
	{
	    it.next();
	    element = it.value();

	    elementInt = element->getStartSecs();
		memcpy(i, &elementInt, sz32);
		i += sz32;

	    elementInt = element->getEndSecs();
		memcpy(i, &elementInt, sz32);
		i += sz32;

		elementQint64 = element->getSpeed();
		memcpy(i, &elementQint64, sz64);
		i += sz64;

		element8 = element->getDisplayGranularity();
		memcpy(i, &element8, sz8);
		i += sz8;

		element16 = element->getId();
	    qDebug() << "Saving element " << element16;
		memcpy(i, &element16, sz16);
		i += sz16;
	}

    return p;
}

bool QueueSchedule::dbSave()
{
	Dbt key, data;

	char *p = this->data();

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	key.set_data((void*)savedName);
	key.set_size(scheduleName.size());
	data.set_data(p);
	data.set_size(saveItemSize());
	//TODO:free the data?

	if ((schedDb->put(0, &key, &data, 0)) != 0)
	{
		qDebug() << "Db update failed";
        Q_DELETE_ARRAY(p);
		return false;
	}
	else
	{
        Q_DELETE_ARRAY(p);
		schedDb->sync(0);
		qDebug() << "Db update worked";
		return true;
	}
}

QueueScheduler::QueueScheduler()
{
	currentState    = NOT_ENABLED;
	isRatePeriod    = false;
	schedDb         = 0;
	currentSchedule = 0;
	restrictedSpeed = -1; // not restricted
	nextBoundary.setSingleShot(true);
	connect(&nextBoundary, SIGNAL(timeout()), this, SLOT(timerExpired()));

	currentTime = QDateTime::currentDateTime();
	currentMonday = currentTime.addDays((currentTime.date().dayOfWeek() - 1) * -1);
	QTime mondayTime(0,0);
	currentMonday.setTime(mondayTime);
	// qDebug() << "Monday is " << currentMonday.toString("ddd dd MM yyyy hh:mm:ss");
}

QueueScheduler::~QueueScheduler()
{
	qDeleteAll(queueSchedules);
	queueSchedules.clear();
}

void QueueScheduler::updateCurrentSchedule(QueueSchedule& newSchedule)
{
	bool wasActive = false;

    if (currentSchedule)
    {
    	queueSchedules.removeOne(currentSchedule);
    	if (currentSchedule->getIsActive())
    	{
    	    enabledSchedules.remove(currentSchedule->getPriority(), currentSchedule);
    	    wasActive = true;
    	}
        Q_DELETE(currentSchedule);
    }

    currentSchedule = new QueueSchedule(newSchedule);
	// TODO For the moment only allow one enabled schedule
	if (newSchedule.getIsActive())
	{
		 QListIterator<QueueSchedule*> i(queueSchedules);
		 while (i.hasNext())
			 i.next()->setIsActive(false);

		 enabledSchedules.clear();
	}

	queueSchedules.append(currentSchedule);
	if (currentSchedule->getIsActive())
	{
		enabledSchedules.insert(currentSchedule->getPriority(), currentSchedule);
	    managePeriods();
	}
	else
	{
		if (wasActive) // was active, but not any more
			managePeriods();
	}

	currentSchedule->dbSave();
}

void QueueScheduler::addNewSchedule(QueueSchedule& newSchedule)
{
	// TODO For the moment only allow one enabled schedule
	if (newSchedule.getIsActive())
	{
		 QListIterator<QueueSchedule*> i(queueSchedules);
		 while (i.hasNext())
			 i.next()->setIsActive(false);

		 enabledSchedules.clear();
	}

    currentSchedule = new QueueSchedule(newSchedule);
	queueSchedules.append(currentSchedule);
	if (currentSchedule->getIsActive())
	{
		enabledSchedules.insert(currentSchedule->getPriority(), currentSchedule);
		managePeriods();
	}

	currentSchedule->dbSave();
}

void QueueScheduler::removeCurrentSchedule()
{
	if (currentSchedule)
	{
		dbDelete(currentSchedule->getName());
		queueSchedules.removeOne(currentSchedule);
    	if (currentSchedule->getIsActive())
    	{
    	    enabledSchedules.remove(currentSchedule->getPriority(), currentSchedule);
    	    managePeriods();
    	}
        Q_DELETE(currentSchedule);
	}
}

void QueueScheduler::loadData(Db* _schedDb, bool createStartup)
{
	Dbc *cursor;
	Dbt key, data;
	uchar *keymem=new uchar[KEYMEM_SIZE];
	uchar *datamem=new uchar[DATAMEM_SIZE];

	key.set_flags(DB_DBT_USERMEM);
	key.set_data(keymem);
	key.set_ulen(KEYMEM_SIZE);

	data.set_flags(DB_DBT_USERMEM);
	data.set_ulen(DATAMEM_SIZE);
	data.set_data(datamem);

	int maxId = 0;

	int ret = 0;

	schedDb = _schedDb;

	schedDb->cursor(0, &cursor, DB_WRITECURSOR);

	QueueSchedule* queueSchedule;

	while (ret == 0)
	{
		if ((ret = cursor->get(&key, &data, DB_NEXT)) == 0)
		{
			// qDebug() << "Just read schedule from Db, key = : " << key.get_data();
			queueSchedule = new QueueSchedule(schedDb, (uchar*)data.get_data(), this);
			queueSchedules.append(queueSchedule);
			currentSchedule = queueSchedule;
			if (queueSchedule->getIsActive())
				enabledSchedules.insert(queueSchedule->getPriority(), queueSchedule);
		}
		else if (ret == DB_BUFFER_SMALL)
		{
			ret = 0;
			qDebug("Insufficient memory");
			qDebug("Size required is: %d", data.get_size());
			uchar *p=datamem;
			datamem=new uchar[data.get_size()+1000];
			data.set_ulen(data.get_size()+1000);
			data.set_data(datamem);
            Q_DELETE_ARRAY(p);
		}
	}

	cursor->close();

	if (createStartup)
	{
		queueSchedule = new QueueSchedule(schedDb, tr("Un-metered"), this);
		queueSchedule->setIsActive(false);
		queueSchedule->setPriority(2);
		queueSchedule->addElement( 480 * 60, (1439 * 60) + 59, 128*1024, 0);
		queueSchedule->addElement(1920 * 60, (2879 * 60) + 59, 128*1024, 0);
		queueSchedule->addElement(3360 * 60, (4319 * 60) + 59, 128*1024, 0);
		queueSchedule->addElement(4800 * 60, (5759 * 60) + 59, 128*1024, 0);
		maxId = queueSchedule->addElement(6240 * 60, (7199 * 60) + 59, 128*1024, 0);
		queueSchedule->setMaxId(maxId);

		queueSchedules.append(queueSchedule);
		currentSchedule = queueSchedule;

		queueSchedule->dbSave();
	}

	managePeriods();

    Q_DELETE_ARRAY(keymem);
    Q_DELETE_ARRAY(datamem);
}

void QueueScheduler::managePeriods()
{
	// may not be running, but make sure it's not
	nextBoundary.stop();

	currentTime = QDateTime::currentDateTime();

	// Decide if we have an active period and if it has any periods ..
	if (enabledSchedules.count()) // TODO only one at the moment ...
	{
		quint32 currentSecs;
		QList<QueueSchedule*> schedules = enabledSchedules.values();
		QueueSchedule* activeSchedule = schedules.at(0);

		QMap<quint32, QueueScheduleElement*>* elements =  activeSchedule->getElements();
		if (elements->count())
		{
			QueueScheduleElement* element;

			currentState = WAITING_FOR_START;
			while (currentMonday.secsTo(currentTime) > (7*24*60*60)) // in case we've moved to a later Monday ...
				currentMonday.addSecs((7*24*60*60));

			currentSecs = currentMonday.secsTo(currentTime);

			// walk through the elements to find our position ...
			QMap<quint32, QueueScheduleElement*>::iterator i = elements->begin();

			 while (i != elements->end())
			 {
				 element = i.value();

			    // walk queuePeriods until we hit a startTime beyond us ..
			     if (currentSecs < element->getStartSecs())
			     {
			    	 restrictedSpeed = -1;
			    	 emit sigSpeedChanged(restrictedSpeed);

			    	 nextBoundary.start(1000*(element->getStartSecs() - currentSecs));
			    	 qDebug() << "Waiting for " << (element->getStartSecs() - currentSecs) << " secs for start";
			    	 break;
			     }
			     else if (currentSecs < element->getEndSecs())
			     {
			    	 currentState = WAITING_FOR_END;
			    	 restrictedSpeed = element->getSpeed();
			    	 emit sigSpeedChanged(restrictedSpeed);

			    	 nextBoundary.start(1000*(element->getEndSecs() - currentSecs));
			    	 qDebug() << "Waiting for " << (element->getEndSecs() - currentSecs) << " secs for end";
			    	 break;
			     }

			     ++i;
			 }

			 // Beyond the last element
			 if (i == elements->end())
			 {
				 i = elements->begin();
				 element = i.value();

				 restrictedSpeed = -1;
				 emit sigSpeedChanged(restrictedSpeed);

				 nextBoundary.start(1000*(element->getStartSecs() + (7*24*60*60) - currentSecs));
				 qDebug() << "Waiting for " << (element->getStartSecs() + (7*24*60*60) - currentSecs) << " secs for start";
			 }
		}
		else
		{
			 currentState = NOT_ENABLED;
		     restrictedSpeed = -1;
		     emit sigSpeedChanged(restrictedSpeed);
		}
	}
	else
	{
		 currentState = NOT_ENABLED;
	     restrictedSpeed = -1;
	     emit sigSpeedChanged(restrictedSpeed);
	}
}

void QueueScheduler::timerExpired()
{
    managePeriods();
}

bool QueueScheduler::dbDelete(QString _scheduleName)
{
	Dbt key;

	QByteArray ba;
	const char *c_str;

	ba = _scheduleName.toLocal8Bit();
	c_str = ba.data();

	key.set_data((void*)c_str);
	key.set_size(_scheduleName.size());

	if ((schedDb->del(NULL, &key, 0)) != 0)
		return false;
	else
	{
		schedDb->sync(0);
		return true;
	}
}

QueueSchedule* QueueScheduler::getSchedule(const QString& scheduleName)
{
	QueueSchedule* thisSchedule = 0;

	// qDebug() << "There are " << queueSchedules.count() << " schedules in the scheduler list";
	 QListIterator<QueueSchedule*> i(queueSchedules);
	 while (i.hasNext())
	 {
		 thisSchedule = i.next();
		 // qDebug() << "Just picked up schedule pointer " << (QueueSchedule*)thisSchedule;
	     if (thisSchedule->getName() == scheduleName)
	    	 return thisSchedule;
	 }

	 return 0;
}

bool QueueScheduler::scheduleNameExists(QString& scheduleName)
{
	 QListIterator<QueueSchedule*> i(queueSchedules);
	 while (i.hasNext())
	 {
	     if (i.next()->getName() == scheduleName)
	    	 return true;
	 }

	 return false;
}
