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


#ifndef AVAILABLEGROUPSVIEW_H_
#define AVAILABLEGROUPSVIEW_H_

#include <QTreeView>
#include <QLineEdit>
#include <QMenu>
#include <QModelIndex>
#include "appConfig.h"
#include "availablegroupswidget.h"
#include "availablegroups.h"

class AvailableGroupsView : public QObject
 {
 	Q_OBJECT

public:
 	AvailableGroupsView(AvailableGroups*, QWidget*, QWidget*);
	QMenu* getMenu() {return contextMenu;}
	QTreeView* getTree() { return availableNewsgroups;}

public slots:
        void slotRefreshView();
		void slotUnsubscribe();
    	void slotUnsubscribed(QString);
    	void slotGroupSelectionChanged(const QModelIndex & selected);

protected slots:
		void slotActivateFilter();
		void slotSubscribe();
    	void slotShow();
        void slotHide(int);
        void slotHideSpin();

signals:
		void sigRefreshView();
		void sigUnsubscribe(QString);

private:
	AvailableGroupsWidget* availableWidget;
    QWidget*         availableNewsgroupsTab;
	QLineEdit*       m_filterEdit;
    QTreeView*       availableNewsgroups;
    AvailGroupModel* model;
    AvailableGroups* aGroups;
	QMenu*           contextMenu;

    Configuration* config;

	QWidget*     parent;

	bool             showAll;

 };

#endif /* AVAILABLEGROUPSVIEW_H_ */
