#ifndef AVAILABLEGROUPSWIDGET_H
#define AVAILABLEGROUPSWIDGET_H
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

#include <QWidget>
#include <QMenu>
#include "availablegroups.h"
#include "ui_availablegroupswidget.h"

class AvailableGroupsWidget : public QWidget, public Ui::AvailableGroupsWidget
{
    Q_OBJECT

public:
    AvailableGroupsWidget(QWidget *parent = 0);
    ~AvailableGroupsWidget();

};

#endif // AVAILABLEGROUPSWIDGET_H
