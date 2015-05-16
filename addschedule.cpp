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
#include <QMessageBox>
#include "addschedule.h"

AddSchedule::AddSchedule(int winType, QueueScheduler* _queueScheduler, QWidget *parent)
    : QDialog(parent), queueScheduler(_queueScheduler)
{
	setupUi(this);

	if (winType == 1) // create
	{
	    connect(buttonOk, SIGNAL(clicked()), this, SLOT(accept()));
	    textLabel1->setText(tr("New schedule:"));
	    connect(this, SIGNAL(newSchedule(QString&)), parent, SLOT(addSchedule(QString&)));
	}
	else // save as
	{
	    connect(buttonOk, SIGNAL(clicked()), this, SLOT(saveAs()));
	    textLabel1->setText(tr("Save schedule as:"));
	    connect(this, SIGNAL(saveAsSchedule(QString&)), parent, SLOT(saveAsSchedule(QString&)));
	}

	connect(buttonCancel, SIGNAL(clicked()), this, SLOT(reject()));
}

AddSchedule::~AddSchedule()
{

}

void AddSchedule::reject()
{
  QDialog::reject();
}

void AddSchedule::accept()
{
	QString thisSchedule = m_schedEdit->text().trimmed();

	if (thisSchedule.isEmpty())
	{
		QMessageBox::warning(this, tr("Error"), tr("You must enter a schedule name"));
		return;
	}

	if (queueScheduler->scheduleNameExists(thisSchedule))
	{
		QMessageBox::warning(this, tr("Error"), tr("Schedule name already exists - please choose another"));
		return;
	}

	emit newSchedule(thisSchedule);
	QDialog::accept();
}

void AddSchedule::saveAs()
{
	QString thisSchedule = m_schedEdit->text().trimmed();

	if (thisSchedule.isEmpty())
	{
		QMessageBox::warning(this, tr("Error"), tr("You must enter a schedule name"));
		return;
	}

	if (queueScheduler->scheduleNameExists(thisSchedule))
	{
		QMessageBox::warning(this, tr("Error"), tr("Schedule name already exists - please choose another"));
		return;
	}

	emit saveAsSchedule(thisSchedule);
	QDialog::accept();
}
