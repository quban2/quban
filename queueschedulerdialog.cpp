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
#include <QColor>
#include <QMessageBox>
#include "addschedule.h"
#include "queueschedulerdialog.h"

#define NUM_SUMMARY_COLS 5

QueueSchedulerDialog::QueueSchedulerDialog(QueueScheduler* _queueScheduler, QWidget *parent)
    : QDialog(parent), queueScheduler(_queueScheduler)
{
	setupUi(this);

	leftArrow = new QPixmap(":/quban/images/gnome/32/Gnome-Go-First-32.png");
	leftC = new QCursor(*leftArrow, 0);

	rightArrow = new QPixmap(":/quban/images/gnome/32/Gnome-Go-Last-32.png");
	rightC = new QCursor(*rightArrow, 31);

	greenButton = "background-color: lightgreen; border-style: outset; border-width: 2px; border-radius: 6px; border-color: beige;";
	orangeButton = "background-color: orange; border-style: outset; border-width: 2px; border-radius: 6px; border-color: beige;";
	redButton = "background-color: red; border-style: outset; border-width: 2px; border-radius: 6px; border-color: beige;";

	changeDetected = false;
	screenAlreadyUpdated = false;
	queueSchedule = 0;
	startIndex = -1;

	queueSchedule = 0;

	setPeriodModeComboBox->addItem(tr("View"));
	setPeriodModeComboBox->addItem(tr("Set"));

    PeriodModeChanged(tr("Set"));
    setPeriodModeComboBox->setCurrentIndex(1);

	pbList << mon00 << mon01 << mon02 << mon03 << mon04 << mon05 << mon06 << mon07 << mon08 << mon09 <<
	          mon10 << mon11 << mon12 << mon13 << mon14 << mon15 << mon16 << mon17 << mon18 << mon19 <<
	          mon20 << mon21 << mon22 << mon23;

	pbList << tue00 << tue01 << tue02 << tue03 << tue04 << tue05 << tue06 << tue07 << tue08 << tue09 <<
			  tue10 << tue11 << tue12 << tue13 << tue14 << tue15 << tue16 << tue17 << tue18 << tue19 <<
			  tue20 << tue21 << tue22 << tue23;

	pbList << wed00 << wed01 << wed02 << wed03 << wed04 << wed05 << wed06 << wed07 << wed08 << wed09 <<
			  wed10 << wed11 << wed12 << wed13 << wed14 << wed15 << wed16 << wed17 << wed18 << wed19 <<
			  wed20 << wed21 << wed22 << wed23;


	pbList << thu00 << thu01 << thu02 << thu03 << thu04 << thu05 << thu06 << thu07 << thu08 << thu09 <<
			  thu10 << thu11 << thu12 << thu13 << thu14 << thu15 << thu16 << thu17 << thu18 << thu19 <<
			  thu20 << thu21 << thu22 << thu23;


	pbList << fri00 << fri01 << fri02 << fri03 << fri04 << fri05 << fri06 << fri07 << fri08 << fri09 <<
			  fri10 << fri11 << fri12 << fri13 << fri14 << fri15 << fri16 << fri17 << fri18 << fri19 <<
			  fri20 << fri21 << fri22 << fri23;


	pbList << sat00 << sat01 << sat02 << sat03 << sat04 << sat05 << sat06 << sat07 << sat08 << sat09 <<
			  sat10 << sat11 << sat12 << sat13 << sat14 << sat15 << sat16 << sat17 << sat18 << sat19 <<
			  sat20 << sat21 << sat22 << sat23;


	pbList << sun00 << sun01 << sun02 << sun03 << sun04 << sun05 << sun06 << sun07 << sun08 << sun09 <<
			  sun10 << sun11 << sun12 << sun13 << sun14 << sun15 << sun16 << sun17 << sun18 << sun19 <<
			  sun20 << sun21 << sun22 << sun23;

	QPushButton* pb;
	for (int i = 0; i < pbList.size(); ++i)
	{
		pb = pbList.at(i);
		pb->setStyleSheet(greenButton);
		connect(pb, SIGNAL(clicked()), this, SLOT(timeSet()));
	}

	model = new QStandardItemModel(0,NUM_SUMMARY_COLS,this);

	model->setHorizontalHeaderItem(0, new QStandardItem(tr("Start Time")));
	model->setHorizontalHeaderItem(1, new QStandardItem(tr("End Time")));
	model->setHorizontalHeaderItem(2, new QStandardItem(tr("Speed")));
	model->setHorizontalHeaderItem(3, new QStandardItem(tr("Units")));
	model->setHorizontalHeaderItem(4, new QStandardItem(tr("Id")));

	overviewTable->setModel(model);
	overviewTable->verticalHeader()->hide();

	QueueSchedule* origSchedule = queueScheduler->getCurrentSchedule(); // may be 0

	if (origSchedule)
	{
		queueSchedule = new QueueSchedule(*origSchedule);

		QList<QueueSchedule*> queueSchedules = queueScheduler->getSchedules();
		QListIterator<QueueSchedule*> i(queueSchedules);

		int comboIndex = 0,
			currentIndex = -1;
		QString thisName;
		QString currName = queueSchedule->getName();

		while (i.hasNext())
		{
			thisName = i.next()->getName();
			scheduleComboBox->addItem(thisName);
			if (thisName == currName)
			{
				currentIndex = comboIndex;
				break;
			}
			++comboIndex;
		}

		if (currentIndex >= 0)
		    scheduleComboBox->setCurrentIndex(currentIndex);

        // The common bit starts here !!!
		buildScreen(queueSchedule);
	}
	else
	    enableWidgets(false);

    connect(setPeriodModeComboBox, SIGNAL(currentIndexChanged ( const QString &)), this, SLOT(PeriodModeChanged(const QString &)));
    connect(scheduleComboBox, SIGNAL(currentIndexChanged ( const QString &)), this, SLOT(ScheduleChanged(const QString &)));

    connect(modifyPeriodButton, SIGNAL(clicked()), this, SLOT(modifyCurrentPeriod()));
    connect(deletePeriodButton, SIGNAL(clicked()), this, SLOT(deleteCurrentPeriod()));
    connect(deleteAllPeriodsButton, SIGNAL(clicked()), this, SLOT(deleteAllPeriods()));

    connect(createPushButton, SIGNAL(clicked()), this, SLOT(addSchedule()));
    connect(savePushButton, SIGNAL(clicked()), this, SLOT(saveSchedule()));
    connect(saveAsPushButton, SIGNAL(clicked()), this, SLOT(saveAsSchedule()));
    connect(deleteButton, SIGNAL(clicked()), this, SLOT(deleteSchedule()));
	connect(cancelPushButton, SIGNAL(clicked()), this, SLOT(reject()));
	connect(enabledRadioButton, SIGNAL(toggled(bool)), this, SLOT(enabledChecked(bool)));

    connect(overviewTable->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)),
    		this, SLOT(SelectionChanged(const QItemSelection &, const QItemSelection &)));
}

QueueSchedulerDialog::~QueueSchedulerDialog()
{
	pbList.clear();

	delete leftArrow;
	delete leftC;

	delete rightArrow;
	delete rightC;

	if (queueSchedule)  // It's a copy ...
		delete queueSchedule;
}

void QueueSchedulerDialog::reject()
{
	if (changeDetected)
	{
		QString dbQuestion = tr("There are unsaved changes, are you sure that you wish to close the dialog?");
		int result = QMessageBox::question(0, tr("Unsaved changes"), dbQuestion, QMessageBox::Yes, QMessageBox::No);

		switch (result)
		{
		  case QMessageBox::Yes:
			  QDialog::reject();
			break;
		  case QMessageBox::No:
			return;
			break;
		}
	}
	else
		QDialog::reject();
}

void QueueSchedulerDialog::timeSet()
{
    QObject* sender = this->sender();
    QPushButton* button = qobject_cast<QPushButton*>(sender);
    if(button)
    {
    	qint64 speed;
    	int    units;
		double displaySpeed;
		int    displayUnits;
		QString unitsText;

    	int buttonIndex = -1;
    	int startSecs = 0,
    		endSecs = 0;

    	buttonIndex = pbList.indexOf(button);

    	if (setPeriodModeComboBox->currentText() == tr("Set"))
    	{
			if (startIndex == -1)
			{
				if (isPeriodInvalid(buttonIndex * 60 * 60, (buttonIndex * 60 * 60) + (59 * 60) + 59, 0))
					return;
				if ((startSecs = setStartTime(buttonIndex * 60 * 60, 0)) == -1)
					return;
				if ((endSecs = setEndTime((buttonIndex * 60 * 60) + (59 * 60) + 59, 0)) == -1)
					return;

				// change the cursor
				scrollAreaWidgetContents->setCursor(*rightC);

				if (mbButton2->isChecked())
				{
				    speed = speedSpinBox2->value() * 1024 * 1024;
				    units = 1;
				}
				else
				{
				    speed = speedSpinBox2->value() * 1024;
				    units = 0;
				}

                // Add to the Schedule
				elementId = queueSchedule->addElement(startSecs, endSecs, speed, units);
				changeDetected = true;

                // add to the table
				QueueScheduleElement* element = queueSchedule->getElement(elementId);
				// qDebug() << "Got element for Id " << elementId << ", address = " << element;

				QStandardItem** modelItem = new QStandardItem*[NUM_SUMMARY_COLS];
				QList<QStandardItem *> rowData;

				modelItem[0] = new QStandardItem();
				modelItem[0]->setText(element->getStart());
				rowData.append(modelItem[0]);

				modelItem[1] = new QStandardItem();
				modelItem[1]->setText(element->getEnd());
				rowData.append(modelItem[1]);

				displaySpeed = element->getSpeed() / 1024; // Kb/Sec
				displayUnits = element->getDisplayGranularity();
				if (displayUnits > 0)
				{
					unitsText = tr("Mb/Sec");
					displaySpeed = displaySpeed / 1024; // Mb/Sec
				}
				else
					unitsText = tr("Kb/Sec");

				modelItem[2] = new QStandardItem();
				modelItem[2]->setText(QString::number(displaySpeed));
				modelItem[2]->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
				rowData.append(modelItem[2]);

				modelItem[3] = new QStandardItem();
				modelItem[3]->setText(unitsText);
				rowData.append(modelItem[3]);

				modelItem[4] = new QStandardItem();
				modelItem[4]->setText(QString::number(element->getId()));
				modelItem[4]->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
				rowData.append(modelItem[4]);

				model->appendRow( rowData );
				overviewTable->selectRow(model->rowCount() - 1);

				delete[] modelItem;

				if (speedSpinBox2->value() > 0)
					button->setStyleSheet(orangeButton);
				else
					button->setStyleSheet(redButton);

				startIndex = buttonIndex;
			}
			else
			{
				if (isPeriodInvalid(queueSchedule->getElement(elementId)->getStartSecs(), (buttonIndex * 60 * 60) + (59 * 60) + 59,
						queueSchedule->getElement(elementId)))
					return;

				if ((endSecs = setEndTime((buttonIndex * 60 * 60) + (59 * 60) + 59, queueSchedule->getElement(elementId))) == -1)
					return;

				// change the cursor
				scrollAreaWidgetContents->setCursor(*leftC);

				if (startIndex < buttonIndex)
				{
					if (mbButton2->isChecked())
					{
					    speed = speedSpinBox2->value() * 1024 * 1024;
					    units = 1;
					}
					else
					{
					    speed = speedSpinBox2->value() * 1024;
					    units = 0;
					}

					QueueScheduleElement* element = queueSchedule->getElement(elementId); // elementId will be set from first pass
					changeDetected = true;

					element->setEnd(endSecs);
					element->setSpeed(speed);
					element->setDisplayGranularity(units);

					// Update the row
					int row = overviewTable->selectionModel()->selectedRows().first().row();
					QStandardItem** modelItem = new QStandardItem*[NUM_SUMMARY_COLS];

					modelItem[0] = new QStandardItem();
					modelItem[0]->setText(element->getStart());
					model->setItem(row, 0, modelItem[0]);

					modelItem[1] = new QStandardItem();
					modelItem[1]->setText(element->getEnd());
					model->setItem(row, 1, modelItem[1]);

				    displaySpeed = element->getSpeed() / 1024; // Kb/Sec
				    displayUnits = element->getDisplayGranularity();
				    if (displayUnits > 0)
				    {
				    	unitsText = tr("Mb/Sec");
				    	displaySpeed = displaySpeed / 1024; // Mb/Sec
				    }
				    else
				    	unitsText = tr("Kb/Sec");

				    modelItem[2] = new QStandardItem();
				    modelItem[2]->setText(QString::number(displaySpeed));
				    modelItem[2]->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
				    model->setItem(row, 2, modelItem[2]);

				    modelItem[3] = new QStandardItem();
				    modelItem[3]->setText(unitsText);
				    model->setItem(row, 3, modelItem[3]);

				    modelItem[4] = new QStandardItem();
				    modelItem[4]->setText(QString::number(element->getId()));
				    modelItem[4]->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
				    model->setItem(row, 4, modelItem[4]);

				    overviewTable->selectRow(model->rowCount() - 1);
					// QItemSelection dummy;
					// slot_SelectionChanged(dummy, dummy);

				    delete[] modelItem;

					for (int i=startIndex; i <=buttonIndex; ++i )
					{
						if (speedSpinBox2->value() > 0)
							pbList.at(i)->setStyleSheet(orangeButton);
						else
							pbList.at(i)->setStyleSheet(redButton);
					}
				}
				startIndex = -1;
			}
		}
    }
}

void QueueSchedulerDialog::addSchedule()
{
  (new AddSchedule(1, queueScheduler, this))->exec();
}

void QueueSchedulerDialog::addSchedule(QString & newScheduleN)
{
	if (queueSchedule)
		delete queueSchedule;

	queueSchedule = new QueueSchedule(queueScheduler->getDb(), newScheduleN, queueScheduler);
	queueScheduler->addNewSchedule(*queueSchedule);
	changeDetected = true;

	clearScreen();

	screenAlreadyUpdated = true;

	enableWidgets(true);
	scheduleComboBox->addItem(newScheduleN);
	scheduleComboBox->setCurrentIndex(scheduleComboBox->count() - 1);
}

void QueueSchedulerDialog::saveSchedule()
{
	if (queueSchedule)
	{
		queueScheduler->updateCurrentSchedule(*queueSchedule);
		queueScheduler->getCurrentSchedule()->dbSave();
		changeDetected = false;
	}
}

void QueueSchedulerDialog::saveAsSchedule()
{
	(new AddSchedule(2, queueScheduler, this))->exec();
}

void QueueSchedulerDialog::saveAsSchedule(QString & saveAsSchedule)
{
	QueueSchedule* thisSchedule = new QueueSchedule(*queueSchedule);
	if (queueSchedule)
		delete queueSchedule;

	queueSchedule = thisSchedule;
	queueSchedule->setName(saveAsSchedule);
	queueScheduler->addNewSchedule(*queueSchedule);

	queueScheduler->getCurrentSchedule()->dbSave();
	changeDetected = false;

	screenAlreadyUpdated = true;

	scheduleComboBox->addItem(saveAsSchedule);
	scheduleComboBox->setCurrentIndex(scheduleComboBox->count() - 1);
}

void QueueSchedulerDialog::deleteSchedule()
{
	queueScheduler->removeCurrentSchedule();

	scheduleComboBox->removeItem(scheduleComboBox->currentIndex());
	if (scheduleComboBox->count() == 0)
		enableWidgets(false);
}

void QueueSchedulerDialog::enableWidgets(bool enabled)
{
	scheduleComboBox->setEnabled(enabled);
	prioritySpinBox->setEnabled(enabled);
	enabledRadioButton->setEnabled(enabled);
	disabledRadioButton->setEnabled(enabled);
	setPeriodModeComboBox->setEnabled(enabled);
	savePushButton->setEnabled(enabled);
	saveAsPushButton->setEnabled(enabled);
	deleteButton->setEnabled(enabled);
	groupBox_2->setEnabled(enabled);
	groupBox_3->setEnabled(enabled);
}

void QueueSchedulerDialog::clearScreen()
{
	for (int i = 0; i < pbList.size(); ++i)
		pbList.at(i)->setStyleSheet(greenButton);

	model->clear();

	fromLabel->setText(tr("From :"));
	startTimeEdit->setTime(startTimeEdit->minimumTime());
	toLabel->setText(tr("to :"));
	endTimeEdit->setTime(endTimeEdit->minimumTime());

	model->setHorizontalHeaderItem(0, new QStandardItem(tr("Start Time")));
	model->setHorizontalHeaderItem(1, new QStandardItem(tr("End Time")));
	model->setHorizontalHeaderItem(2, new QStandardItem(tr("Speed")));
	model->setHorizontalHeaderItem(3, new QStandardItem(tr("Units")));
	model->setHorizontalHeaderItem(4, new QStandardItem(tr("Id")));

	overviewTable->hideColumn(4);
}

void QueueSchedulerDialog::buildScreen(QueueSchedule* newSchedule)
{
	QPushButton* pb;

	QString boxTitle = tr("Periods for ") + newSchedule->getName();
	groupBox->setTitle(boxTitle);
	QMap<quint32, QueueScheduleElement*>* queuePeriods = newSchedule->getElements();

	model->setRowCount(queuePeriods->count());

	prioritySpinBox->setValue(newSchedule->getPriority());
	if (newSchedule->getIsActive())
	{
		enabledRadioButton->setChecked(true);
		disabledRadioButton->setChecked(false);
	}
	else
	{
		enabledRadioButton->setChecked(false);
		disabledRadioButton->setChecked(true);
	}

	QueueScheduleElement* element;
	QStandardItem** modelItem = new QStandardItem*[queuePeriods->count() * NUM_SUMMARY_COLS];

	QMapIterator<quint32, QueueScheduleElement*> it(*(newSchedule->getElements()));

	qint64 displaySpeed;
	int    displayUnits;
	QString unitsText;
	int row=0;
	int buttonIndexStart, buttonIndexEnd;

	while (it.hasNext())
	{
	    it.next();
	    element = it.value();
	    //element = new QueueScheduleElement(*it.value());

	    //newSchedule->addElement(element->getStartSecs(), element->getEndSecs(), element->getSpeed(),
	    		//element->getDisplayGranularity(), element->getId());

		modelItem[(row*NUM_SUMMARY_COLS)] = new QStandardItem();
	    modelItem[(row*NUM_SUMMARY_COLS)]->setText(element->getStart());
	    model->setItem(row, 0, modelItem[(row*NUM_SUMMARY_COLS)]);

	    modelItem[(row*NUM_SUMMARY_COLS) + 1] = new QStandardItem();
	    modelItem[(row*NUM_SUMMARY_COLS) + 1]->setText(element->getEnd());
	    model->setItem(row, 1, modelItem[(row*NUM_SUMMARY_COLS) + 1]);

	    displaySpeed = element->getSpeed() / 1024; // Kb/Sec
	    displayUnits = element->getDisplayGranularity();
	    if (displayUnits > 0)
	    {
	    	unitsText = tr("Mb/Sec");
	    	displaySpeed = displaySpeed / 1024; // Mb/Sec
	    }
	    else
	    	unitsText = tr("Kb/Sec");

	    modelItem[(row*NUM_SUMMARY_COLS) + 2] = new QStandardItem();
	    modelItem[(row*NUM_SUMMARY_COLS) + 2]->setText(QString::number(displaySpeed));
	    modelItem[(row*NUM_SUMMARY_COLS) + 2]->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
	    model->setItem(row, 2, modelItem[(row*NUM_SUMMARY_COLS) + 2]);

	    modelItem[(row*NUM_SUMMARY_COLS) + 3] = new QStandardItem();
	    modelItem[(row*NUM_SUMMARY_COLS) + 3]->setText(unitsText);
	    model->setItem(row, 3, modelItem[(row*NUM_SUMMARY_COLS) + 3]);

	    modelItem[(row*NUM_SUMMARY_COLS) + 4] = new QStandardItem();
	    modelItem[(row*NUM_SUMMARY_COLS) + 4]->setText(QString::number(element->getId()));
	    modelItem[(row*NUM_SUMMARY_COLS) + 4]->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
	    model->setItem(row, 4, modelItem[(row*NUM_SUMMARY_COLS) + 4]);

	    ++row;

	    buttonIndexStart = element->getStartSecs() / 60 / 60;
	    buttonIndexEnd   = element->getEndSecs() / 60 / 60;

	    for (int j=buttonIndexStart; j<=buttonIndexEnd; ++j)
	    {
			 pb = pbList.at(j);
			 if (element->getSpeed())
			     pb->setStyleSheet(orangeButton);
			 else
				 pb->setStyleSheet(redButton);
	    }
	}

	delete[] modelItem;

	if (row)
	{
		overviewTable->hideColumn(4);
		overviewTable->resizeColumnsToContents();
		overviewTable->selectRow(0);

		QItemSelection dummy;
		SelectionChanged(dummy, dummy);
	}
}

void QueueSchedulerDialog::SelectionChanged(const QItemSelection &, const QItemSelection &)
{
	if (overviewTable->selectionModel()->selectedRows(0).count() == 0) // This slot will get called when we delete the last element
		return;

	QStringList timeParts = overviewTable->selectionModel()->selectedRows(0).first().data().toString().split(':');
    if (timeParts.size() == 3)
    {
	    fromLabel->setText(tr("From ") + timeParts.at(0) + ":");
	    startTimeEdit->setTime(QTime(0, timeParts.at(1).toInt(), timeParts.at(2).toInt()));
    }

	timeParts = overviewTable->selectionModel()->selectedRows(1).first().data().toString().split(':');
    if (timeParts.size() == 3)
    {
	    toLabel->setText(tr(" to ") + timeParts.at(0) + ":");
	    endTimeEdit->setTime(QTime(0, timeParts.at(1).toInt(), timeParts.at(2).toInt()));
    }

	speedSpinBox->setValue(overviewTable->selectionModel()->selectedRows(2).first().data().toDouble());

	QString speedUnits = overviewTable->selectionModel()->selectedRows(3).first().data().toString();

	if (speedUnits == tr("Mb/Sec"))
	{
		mbButton->setChecked(true);
		kbButton->setChecked(false);
	}
	else
	{
		mbButton->setChecked(false);
		kbButton->setChecked(true);
	}
}

void QueueSchedulerDialog::modifyCurrentPeriod()
{
	int selectedElementId = overviewTable->selectionModel()->selectedRows(4).first().data().toInt();
	QueueScheduleElement* element = queueSchedule->getElement(selectedElementId);
	qDebug() << "Request to modify element: " << selectedElementId;

	qint64 speed;
	int    units;

	if (mbButton->isChecked())
	{
	    speed = speedSpinBox->value() * 1024 * 1024;
	    units = 1;
	}
	else
	{
	    speed = speedSpinBox->value() * 1024;
	    units = 0;
	}

	int hours = 0;

	int startSecs = element->getStartSecs();
	int endSecs   = element->getEndSecs();

	// First get the number of hours from the existing start and end times, then remove mins and secs from start and end
	hours = startSecs / (60 * 60);
	startSecs = hours * (60 * 60);

	hours = endSecs / (60 * 60);
	endSecs = hours * (60 * 60);

	// Now add on the minutes and seconds
	QTime startTime = startTimeEdit->time();
	QTime endTime   = endTimeEdit->time();

	startSecs += ((startTime.minute() * 60) + startTime.second());
	endSecs += ((endTime.minute() * 60) + endTime.second());

	if (isPeriodInvalid(startSecs, endSecs, element))
		return;

	if ((startSecs = setStartTime(startSecs, element)) == -1)
		return;

	if ((endSecs = setEndTime(endSecs, element)) == -1)
		return;

	// Update the element
	element->setStart(startSecs);
	element->setEnd(endSecs);
	element->setSpeed(speed);
	element->setDisplayGranularity(units);

	int buttonIndexStart = element->getStartSecs() / 60 / 60;
	int buttonIndexEnd   = element->getEndSecs() / 60 / 60;
	QPushButton* pb;

	for (int j=buttonIndexStart; j<=buttonIndexEnd; ++j)
	{
		 pb = pbList.at(j);
		 if (speed)
		     pb->setStyleSheet(orangeButton);
		 else
			 pb->setStyleSheet(redButton);
	}

	/*
	qDebug() << "Start: " << element->getStart();
	qDebug() << "End: " << element->getEnd();
	qDebug() << "Speed: " << element->getSpeed();
	qDebug() << "Gran: " << element->getDisplayGranularity();
	*/

	// Change the model
	QString unitsText;
    double displaySpeed = element->getSpeed() / 1024; // Kb/Sec
    int displayUnits = element->getDisplayGranularity();
    if (displayUnits > 0)
    {
    	unitsText = tr("Mb/Sec");
    	displaySpeed = displaySpeed / 1024; // Mb/Sec
    }
    else
    	unitsText = tr("Kb/Sec");

	model->setData(overviewTable->selectionModel()->selectedRows(0).first(), element->getStart());
	model->setData(overviewTable->selectionModel()->selectedRows(1).first(), element->getEnd());
	model->setData(overviewTable->selectionModel()->selectedRows(2).first(), displaySpeed);
	model->setData(overviewTable->selectionModel()->selectedRows(3).first(), unitsText);

	changeDetected = true;
}

quint32 QueueSchedulerDialog::setStartTime(quint32 startTime, QueueScheduleElement* thisElement)
{
	QueueScheduleElement* element;
	QMapIterator<quint32, QueueScheduleElement*> it(*(queueSchedule->getElements()));

	quint32 hours;
	quint32 periodEndSecs;
	while (it.hasNext())
	{
	    it.next();
	    element = it.value();

	    if (thisElement && (element->getId() == thisElement->getId())) // don't compare to ourself
	    	continue;

	    // Skip any period that the startTime doesn't fall between
	    if (startTime >= element->getStartSecs() && startTime <= element->getEndSecs())
	    {
	    	hours = element->getEndSecs() / (60 * 60);
	    	periodEndSecs = hours * (60 * 60);

	    	if (element->getEndSecs() < (periodEndSecs + (59 * 60) + 59))
	    		startTime = element->getEndSecs() + 1;
	    	else
	    	{
	    		QMessageBox::warning(this,tr("Error"), tr("Invalid start time - clashes with existing schedule part") );
                return -1; // Can't use this start time
	    	}
	    }
	}

	return startTime;
}

quint32 QueueSchedulerDialog::setEndTime(quint32 endTime, QueueScheduleElement* thisElement)
{
	// Do backwards
	QueueScheduleElement* element;

	quint32 hours;
	quint32 periodStartSecs;

	QMapIterator<quint32, QueueScheduleElement*> it(*(queueSchedule->getElements()));
    it.toBack();

	 // Walk backwards ...
	 while (it.hasPrevious())
	 {
		 it.previous();
		 element = it.value();

	    if (thisElement && (element->getId() == thisElement->getId())) // don't compare to ourself
	    	continue;

	    // Skip any period that the startTime doesn't fall between
	    if (endTime >= element->getStartSecs() && endTime <= element->getEndSecs())
	    {
	    	hours = element->getStartSecs() / (60 * 60);
	    	periodStartSecs = hours * (60 * 60);

	    	if (element->getStartSecs() > periodStartSecs)
	    		endTime = element->getStartSecs() - 1;
	    	else
	    	{
	    		QMessageBox::warning(this,tr("Error"), tr("Invalid end time - clashes with existing schedule part") );
                return -1; // Can't use this end time
	    	}
	    }
	}

	return endTime;
}

bool QueueSchedulerDialog::isPeriodInvalid(quint32 startTime, quint32 endTime, QueueScheduleElement* thisElement)
{
	QueueScheduleElement* element;
	QMapIterator<quint32, QueueScheduleElement*> it(*(queueSchedule->getElements()));

	while (it.hasNext())
	{
	    it.next();
	    element = it.value();
	    if (thisElement && (element->getId() == thisElement->getId())) // don't compare to ourself
	    	continue;

	    if (startTime <= element->getStartSecs() && endTime >= element->getEndSecs())
	    {
	    	QMessageBox::warning(this,tr("Error"), tr("Invalid start and end time - encloses existing schedule part") );
            return true; // Can't use this start and end time
	    }

	    if (startTime >= element->getStartSecs() && endTime <= element->getEndSecs())
	    {
	    	QMessageBox::warning(this,tr("Error"), tr("Invalid start and end time - is enclosed by an existing schedule part") );
            return true; // Can't use this start and end time
	    }
	}

	return false;
}

void QueueSchedulerDialog::enabledChecked(bool isChecked)
{
	queueSchedule->setIsActive(isChecked);
	changeDetected = true;
}

void QueueSchedulerDialog::deleteCurrentPeriod()
{
	int selectedElementId = overviewTable->selectionModel()->selectedRows(4).first().data().toInt();
	qDebug() << "Request to delete element: " << selectedElementId;

	model->takeRow(overviewTable->selectionModel()->selectedRows().first().row());

	QueueScheduleElement* element = queueSchedule->getElement(selectedElementId);

	qDebug() << "Got element with id: " << element->getId();

	int buttonIndexStart = element->getStartSecs() / 60 / 60;
	int buttonIndexEnd   = element->getEndSecs() / 60 / 60;
	QPushButton* pb;

	for (int j=buttonIndexStart; j<=buttonIndexEnd; ++j)
	{
		 pb = pbList.at(j);
		 pb->setStyleSheet(greenButton);
	}

	queueSchedule->removeElement(selectedElementId);
	changeDetected = true;
}

void QueueSchedulerDialog::deleteAllPeriods()
{
	clearScreen();
	queueSchedule->removeAllElements();
	changeDetected = true;
}

void QueueSchedulerDialog::PeriodModeChanged(const QString &newText)
{
	if (newText == tr("View"))
	{
		scrollAreaWidgetContents->unsetCursor();
        groupBox_3->setEnabled(false);
        groupBox_2->setEnabled(false);
	}
	else
	{
		scrollAreaWidgetContents->setCursor(*leftC);
        groupBox_3->setEnabled(true);
        groupBox_2->setEnabled(true);
	}
}

void QueueSchedulerDialog::ScheduleChanged(const QString &newText)
{
	qDebug() << "Picked up schedule combo box name change to " << newText;

	if (screenAlreadyUpdated)
	{
		screenAlreadyUpdated = false;
		return;
	}

	if (queueSchedule)
	{
		if (changeDetected)
		{
			QString dbQuestion = tr("There are unsaved schedule changes, do you wish to save?");
			int result = QMessageBox::question(0, tr("Unsaved changes"), dbQuestion, QMessageBox::Yes, QMessageBox::No);

			switch (result)
			{
			  case QMessageBox::Yes:
				  saveSchedule();
				break;
			  case QMessageBox::No:
				break;
			}

			changeDetected = false;
		}

		delete queueSchedule;
	}

	QueueSchedule* thisSchedule = queueScheduler->getSchedule(newText);
	if (!thisSchedule)
	{
		qDebug() << "Failed to find schedule following schedule combo box change";
		queueSchedule = 0;
		queueScheduler->setCurrentSchedule(0);
		enableWidgets(false);
	}
	else
	{
    	queueSchedule = new QueueSchedule(*thisSchedule);
    	qDebug() << "Setting current schedule to " << queueSchedule->getName();
	    queueScheduler->setCurrentSchedule(queueScheduler->getSchedule(queueSchedule->getName()));
	}

	clearScreen();

	// Now rebuild the screen
	if (queueSchedule)
		buildScreen(queueSchedule);
}
