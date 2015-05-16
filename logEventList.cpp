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
#include "common.h"
#include "quban.h"
#include "logEventList.h"

extern Quban* quban;

LogEventList::LogEventList(QObject* p) : QObject(p)
{
	eventTreeWidget = quban->getEventTreeWidget();
}

LogEventList::~LogEventList()
{
}

void LogEventList::logEvent(QString description)
{
	// insert at head of list
    QTreeWidgetItem *event = new QTreeWidgetItem();
    event->setText(0, QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm:ss"));
    event->setText(1, description);
    eventTreeWidget->insertTopLevelItem(0, event);

    eventTreeWidget->resizeColumnToContents(0);
    eventTreeWidget->setColumnWidth(0, eventTreeWidget->columnWidth(0) + 6);
    eventTreeWidget->resizeColumnToContents(1);
    eventTreeWidget->setColumnWidth(1, eventTreeWidget->columnWidth(1) + 6);

    // Events are Tab 2
    quban->jobsAndLogsTabs->setTabTextColor(2, QColor(Qt::red));
}
