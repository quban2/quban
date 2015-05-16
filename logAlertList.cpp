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
#include <QIcon>
#include "common.h"
#include "quban.h"
#include "logAlertList.h"

extern Quban* quban;

LogAlertList::LogAlertList(QObject * p) : QObject(p)
{
	alertTreeWidget = quban->getAlertTreeWidget();
}

LogAlertList::~LogAlertList()
{
}

void LogAlertList::logMessage(int type, QString description)
{
    QIcon* icon = 0;

	// insert at head of list
    QTreeWidgetItem *alert = new QTreeWidgetItem();
    alert->setText(0, QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm:ss"));
    if (type == LogAlertList::Error)
    	icon = new QIcon(QString::fromUtf8(":/quban/images/fatCow/Tag-Red.png"));
    else if (type == LogAlertList::Warning)
    	icon = new QIcon(QString::fromUtf8(":/quban/images/fatCow/Tag-Orange.png"));
    else // if (type == LogAlertList::Alert)
    	icon = new QIcon(QString::fromUtf8(":/quban/images/fatCow/Tag-Blue.png"));
    alert->setIcon(0, *icon);

    alert->setText(1, description);
    alertTreeWidget->insertTopLevelItem(0, alert);

 	delete icon;

    alertTreeWidget->resizeColumnToContents(0);
    alertTreeWidget->setColumnWidth(0, alertTreeWidget->columnWidth(0) + 6);
    alertTreeWidget->resizeColumnToContents(1);
    alertTreeWidget->setColumnWidth(1, alertTreeWidget->columnWidth(1) + 6);

    // Events are Tab 1
    quban->jobsAndLogsTabs->setTabTextColor(1, QColor(Qt::red));
}
