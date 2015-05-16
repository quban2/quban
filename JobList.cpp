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

#include <QDateTime>
#include <QDebug>
#include <QtDebug>
#include "common.h"
#include "quban.h"
#include "JobList.h"

extern Quban* quban;

JobList::JobList(QObject *p) : QObject(p)
{
	struct JobStatusDetail* jobStatusDetail = 0;

	jobTreeWidget = quban->getJobsTreeWidget();

	jobStatusDetail = new struct JobStatusDetail;
	jobStatusDetail->icon = new QIcon(QString::fromUtf8(":/quban/images/fatCow/Viewstack.png"));
        iconList.append(jobStatusDetail->icon);
	jobStatusDetail->desc = tr("Queued");
	jobStatusMap.insert(JobList::Queued, jobStatusDetail);

	jobStatusDetail = new struct JobStatusDetail;
	jobStatusDetail->icon = new QIcon(QString::fromUtf8(":/quban/images/fatCow/Control-Play-Blue.png"));
        iconList.append(jobStatusDetail->icon);
	jobStatusDetail->desc = tr("Running");
	jobStatusMap.insert(JobList::Running, jobStatusDetail);

	jobStatusDetail = new struct JobStatusDetail;
	jobStatusDetail->icon = new QIcon(QString::fromUtf8(":/quban/images/fatCow/Control-Pause-Blue.png"));
        iconList.append(jobStatusDetail->icon);
	jobStatusDetail->desc = tr("Paused");
	jobStatusMap.insert(JobList::Paused, jobStatusDetail);

	jobStatusDetail = new struct JobStatusDetail;
	jobStatusDetail->icon = new QIcon(QString::fromUtf8(":/quban/images/fatCow/Cancel.png"));
        iconList.append(jobStatusDetail->icon);
	jobStatusDetail->desc = tr("Cancelled");
	jobStatusMap.insert(JobList::Cancelled, jobStatusDetail);

	jobStatusDetail = new struct JobStatusDetail;
	jobStatusDetail->icon = new QIcon(QString::fromUtf8(":/quban/images/fatCow/Tick.png"));
        iconList.append(jobStatusDetail->icon);
	jobStatusDetail->desc = tr("Successful");
	jobStatusMap.insert(JobList::Finished_Ok, jobStatusDetail);

	jobStatusDetail = new struct JobStatusDetail;
	jobStatusDetail->icon = new QIcon(QString::fromUtf8(":/quban/images/fatCow/Error.png"));
        iconList.append(jobStatusDetail->icon);
	jobStatusDetail->desc = tr("Failure");
	jobStatusMap.insert(JobList::Finished_Fail, jobStatusDetail);

	jobTypeMap.insert(JobList::Expire, tr("Expire"));
	jobTypeMap.insert(JobList::Repair, tr("Repair"));
	jobTypeMap.insert(JobList::Uncompress, tr("Uncompress"));
	jobTypeMap.insert(JobList::BulkDelete, tr("Bulk Delete"));
	jobTypeMap.insert(JobList::BulkLoad, tr("Bulk Load"));
}

JobList::~JobList()
{
	qDeleteAll(jobMap);
	jobMap.clear();

	qDeleteAll(jobStatusMap);
	jobStatusMap.clear();

	jobTypeMap.clear();

        while (!iconList.isEmpty())
            delete iconList.takeFirst();
}

void JobList::addJob(quint16 jobType, quint16 status, quint64 identifier, QString description)
{
	// 0 = status
	// 1 = type
	// 2 = Details
	// 3 = Start
	// 4 = End      not yet

	// insert at head of list
    QTreeWidgetItem* job = new QTreeWidgetItem();

    job->setText(0, jobStatusMap.value(status)->desc);
    job->setIcon(0, *(jobStatusMap.value(status)->icon));

    job->setText(1, jobTypeMap.value(jobType));

    job->setText(2, description);

    if (status == JobList::Running) // may be Queued
    	job->setText(3, QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm:ss"));

    jobTreeWidget->insertTopLevelItem(0, job);

 	jobTreeWidget->resizeColumnToContents(0);
 	jobTreeWidget->setColumnWidth(0, jobTreeWidget->columnWidth(0) + 6);
 	jobTreeWidget->resizeColumnToContents(1);
 	jobTreeWidget->setColumnWidth(1, jobTreeWidget->columnWidth(1) + 6);
 	jobTreeWidget->resizeColumnToContents(2);
 	jobTreeWidget->setColumnWidth(2, jobTreeWidget->columnWidth(2) + 6);
 	jobTreeWidget->resizeColumnToContents(3);
 	jobTreeWidget->setColumnWidth(3, jobTreeWidget->columnWidth(3) + 6);
 	jobTreeWidget->resizeColumnToContents(4);
 	jobTreeWidget->setColumnWidth(4, jobTreeWidget->columnWidth(4) + 6);

 	struct ListedJob* listedJob = new struct ListedJob;
 	listedJob->jobType = jobType;
 	listedJob->jobIdentifier = identifier;
 	listedJob->job = job;

 	jobMap.insert(identifier, listedJob);

    // Jobs are Tab 0
    quban->jobsAndLogsTabs->setTabTextColor(0, QColor(Qt::red));
}

void JobList::updateJob(quint16 jobType, quint16 status, quint64 identifier)
{
	// 0 = status
	// 1 = type
	// 2 = Details
	// 3 = Start
	// 4 = End

	// insert at head of list
    QTreeWidgetItem *job = 0;

    QList<struct ListedJob*> values = jobMap.values(identifier);
    for (int i = 0; i < values.size(); ++i)
    {
        if (values.at(i)->jobType == jobType)
        {
        	job = values.at(i)->job;
        	break;
        }
    }

    if (!job) // Failed to find list item : may have been deleted by user
    {

    	qDebug() << "Failed to find job for type " << jobType << " with status " << status;
    	return;
    }

    job->setText(0, jobStatusMap.value(status)->desc);
    job->setIcon(0, *(jobStatusMap.value(status)->icon));

    if (status == JobList::Running)
    	job->setText(3, QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm:ss"));
    else if (status == JobList::Finished_Ok || status == JobList::Finished_Fail)
    	job->setText(4, QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm:ss"));

    if (jobTreeWidget->indexOfTopLevelItem(job) != 0)
    {
        job = jobTreeWidget->takeTopLevelItem(jobTreeWidget->indexOfTopLevelItem(job));
        jobTreeWidget->insertTopLevelItem(0, job);
    }

 	jobTreeWidget->resizeColumnToContents(0);
 	jobTreeWidget->setColumnWidth(0, jobTreeWidget->columnWidth(0) + 6);
 	jobTreeWidget->resizeColumnToContents(3);
 	jobTreeWidget->setColumnWidth(3, jobTreeWidget->columnWidth(3) + 6);
 	jobTreeWidget->resizeColumnToContents(4);
 	jobTreeWidget->setColumnWidth(4, jobTreeWidget->columnWidth(4) + 6);

    // Jobs are Tab 0
    quban->jobsAndLogsTabs->setTabTextColor(0, QColor(Qt::red));
}

void JobList::updateJob(quint16 jobType, QString newDesc, quint64 identifier)
{
    // 0 = status
    // 1 = type
    // 2 = Details
    // 3 = Start
    // 4 = End

    // insert at head of list
    QTreeWidgetItem *job = 0;

    QList<struct ListedJob*> values = jobMap.values(identifier);
    for (int i = 0; i < values.size(); ++i)
    {
        if (values.at(i)->jobType == jobType)
        {
            job = values.at(i)->job;
            break;
        }
    }

    if (!job) // Failed to find list item : may have been deleted by user
    {

        qDebug() << "Failed to find job for type " << jobType;
        return;
    }

    job->setText(2, newDesc);

    jobTreeWidget->resizeColumnToContents(2);
    jobTreeWidget->setColumnWidth(2, jobTreeWidget->columnWidth(2) + 6);

    // Jobs are Tab 0
    quban->jobsAndLogsTabs->setTabTextColor(0, QColor(Qt::red));
}

/*
void JobList::slotPauseJob()
{
	QList<QTreeWidgetItem*> selection=jobTreeWidget->selectedItems();
	QListIterator<QTreeWidgetItem*> it(selection);
	QTreeWidgetItem *selected;

	if (selection.isEmpty())
	{
		quban->getLogAlertList()->logMessage(LogAlertList::Warning, tr("Pause job attempted with no selected job"));
		return;
	}

    bool found     = false;
    ListedJob* job = 0;
    int i          = 0;

	while (it.hasNext())
	{
		selected = it.next();

	    QList<struct ListedJob*> values = jobMap.values(); // walk through the entire map
	    for (i=0; i < values.size(); ++i)
	    {
	    	if (values.at(i)->job == selected)
	        {
	        	found = true;
	        	break;
	        }
	    }

	    if (found)
	    {
	    	found = false;
	    	job = values.at(i); // for readability
	    	// MD TODO Pause the job

	    }
	    else
	    	quban->getLogAlertList()->logMessage(LogAlertList::Error, tr("Internal Error: Failed to find details of job to be paused"));
	}
}

void JobList::JobList::slotResumeJob()
{
	QList<QTreeWidgetItem*> selection=jobTreeWidget->selectedItems();
	QListIterator<QTreeWidgetItem*> it(selection);
	QTreeWidgetItem *selected;

	if (selection.isEmpty())
	{
		quban->getLogAlertList()->logMessage(LogAlertList::Warning, tr("Resume job attempted with no selected job"));
		return;
	}

    bool found     = false;
    ListedJob* job = 0;
    int i          = 0;

	while (it.hasNext())
	{
		selected = it.next();

	    QList<struct ListedJob*> values = jobMap.values(); // walk through the entire map
	    for (i=0; i < values.size(); ++i)
	    {
	    	if (values.at(i)->job == selected)
	        {
	        	found = true;
	        	break;
	        }
	    }

	    if (found)
	    {
	    	found = false;
	    	job = values.at(i); // for readability
	    	// MD TODO Resume the job

	    }
	    else
	    	quban->getLogAlertList()->logMessage(LogAlertList::Error, tr("Internal Error: Failed to find details of job to be resumed"));
	}
}
*/

void JobList::slotCancelJob()
{
	cancelJob(false);
}

void JobList::cancelJob(bool deleteAsWell)
{
	QList<QTreeWidgetItem*> selection=jobTreeWidget->selectedItems();
	QListIterator<QTreeWidgetItem*> it(selection);
	QTreeWidgetItem *selected;

	if (selection.isEmpty())
	{
		quban->getLogAlertList()->logMessage(LogAlertList::Warning, tr("Cancel job attempted with no selected job"));
		return;
	}

    bool found         = false;
    ListedJob* job     = 0;
    int i              = 0;

	while (it.hasNext())
	{
		selected = it.next();

	    QList<struct ListedJob*> values = jobMap.values(); // walk through the entire map
	    for (i=0; i < values.size(); ++i)
	    {
	    	if (values.at(i)->job == selected)
	        {
	        	found = true;
	        	break;
	        }
	    }

	    if (found)
	    {
	    	found = false;
	    	job = values.at(i); // for readability

	    	if (selected->text(0) == tr("Queued") || selected->text(0) == tr("Running"))
	    	{
				if (job->jobType == JobList::Expire)
					emit cancelExpire(job->jobIdentifier);
				else if (job->jobType == JobList::Repair || job->jobType == JobList::Uncompress)
					emit cancelExternal(job->jobIdentifier);
                else if (job->jobType == JobList::BulkLoad)
                    emit cancelBulkLoad(job->jobIdentifier);
                else if (job->jobType == JobList::BulkDelete)
                    emit cancelBulkDelete(job->jobIdentifier);
	    	}

	    	if (deleteAsWell)
	    		if (!jobTreeWidget->takeTopLevelItem(jobTreeWidget->indexOfTopLevelItem(selected)))
	    			quban->getLogAlertList()->logMessage(LogAlertList::Error, tr("Internal Error: Failed to delete selected job from list"));
	    }
	    else
	    	quban->getLogAlertList()->logMessage(LogAlertList::Error, tr("Internal Error: Failed to find details of job to be cancelled"));
	}
}

void JobList::slotDeleteJob()
{
	QList<QTreeWidgetItem*> selection=jobTreeWidget->selectedItems();

	if (selection.isEmpty())
	{
		quban->getLogAlertList()->logMessage(LogAlertList::Warning, tr("Delete job attempted with no selected job"));
		return;
	}

	cancelJob(true);
}
