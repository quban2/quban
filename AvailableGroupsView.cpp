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

#include <QVBoxLayout>
#include <QGroupBox>
#include <QDebug>
#include <QtDebug>
#include <QPushButton>
#include <QMessageBox>
#include "quban.h"
#include "maintabwidget.h"
#include "AvailableGroupsView.h"

extern Quban* quban;

AvailableGroupsView::AvailableGroupsView(AvailableGroups* _aGroups, QWidget* tabWidget_2, QWidget* p) : aGroups(_aGroups)
{
	parent = p;

	showAll = true;

    config = Configuration::getConfig();

    availableNewsgroupsTab = new QWidget(tabWidget_2);
    availableNewsgroupsTab->setObjectName(QString::fromUtf8("availableNewsgroupsTab"));
    availableNewsgroupsTab->setEnabled(true);
    QVBoxLayout *verticalLayout = new QVBoxLayout(availableNewsgroupsTab);
    verticalLayout->setContentsMargins(0, 0, 0, 0);
    verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));

    availableWidget = new AvailableGroupsWidget(availableNewsgroupsTab);

	availableNewsgroups = availableWidget->availableNewsgroups;
	availableNewsgroups->setObjectName(QString::fromUtf8("availableNewsgroups"));
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
	availableNewsgroups->header()->setMovable(false);
#else
    availableNewsgroups->header()->setSectionsMovable(false);
#endif

	verticalLayout->addWidget(availableWidget);

	availableNewsgroups->setAllColumnsShowFocus(true);
	availableNewsgroups->setSelectionMode(QAbstractItemView::SingleSelection);
	availableNewsgroups->setRootIsDecorated(false);
	availableNewsgroups->setSortingEnabled(false);
	availableNewsgroups->setAutoScroll(false);

	m_filterEdit = availableWidget->m_filterEdit;
	connect(m_filterEdit, SIGNAL(returnPressed()), this, SLOT(slotActivateFilter()));

	model = aGroups->loadData();
	availableNewsgroups->setModel(model);

    connect(model, SIGNAL(sigFilter()), this, SLOT(slotActivateFilter())); // after a sort
	connect(this, SIGNAL(sigRefreshView()), aGroups, SLOT(slotRefreshView()));
	connect(aGroups, SIGNAL(sigRefreshView()), this, SLOT(slotRefreshView()));

    int j=0;
    for (j=0; j < model->columnCount(); ++j)
		availableNewsgroups->resizeColumnToContents(j);

	availableNewsgroups->setSortingEnabled(true);
	availableNewsgroups->header()->setSortIndicatorShown(true);

#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
    availableNewsgroups->header()->setClickable(true);
#else
    availableNewsgroups->header()->setSectionsClickable(true);
#endif

	//availableNewsgroups->sortByColumn(0, Qt::AscendingOrder);

	connect(availableNewsgroups, SIGNAL(doubleClicked(const QModelIndex)), this, SLOT(slotSubscribe()));

    availableWidget->hideCheckBox->setChecked(config->minAvailEnabled);
    availableWidget->hideSpinBox->setValue(config->minAvailValue);

    connect(availableWidget->hideCheckBox, SIGNAL(stateChanged(int)), this, SLOT(slotHide(int)));
    connect(availableWidget->hideSpinBox, SIGNAL(editingFinished()), this, SLOT(slotHideSpin()));
	connect(availableWidget->subscribeButton, SIGNAL(clicked()), this, SLOT(slotSubscribe()));
	connect(availableWidget->unsubscribeButton, SIGNAL(clicked()), this, SLOT(slotUnsubscribe()));
	connect(availableWidget->showButton, SIGNAL(clicked()), this, SLOT(slotShow()));

	connect(availableNewsgroups->selectionModel(), SIGNAL(currentRowChanged(const QModelIndex &, const QModelIndex &)),
			this, SLOT(slotGroupSelectionChanged(const QModelIndex &)));

    contextMenu = new QMenu(availableNewsgroups);
	availableNewsgroups->setContextMenuPolicy(Qt::ActionsContextMenu);

	MainTabWidget* tabWidget = (MainTabWidget*)tabWidget_2;
    int tabIndex =
    		tabWidget->addTab(availableNewsgroupsTab, QIcon(":quban/images/hi16-action-icon_newsgroup.png"), QString(tr("Available Newsgroups")));

    QPushButton *closeButton = new QPushButton();
    closeButton->setIcon(QIcon(":quban/images/fatCow/Cancel.png"));
    closeButton->setIconSize(QSize(16,16));

    connect(closeButton, SIGNAL(clicked()), parent, SLOT(closeCurrentTab()));

    tabWidget->getTab()->setTabButton(tabIndex, QTabBar::RightSide, closeButton);

    tabWidget->setCurrentIndex(tabIndex);
}

void AvailableGroupsView::slotHide(int)
{
    slotActivateFilter();
}

void AvailableGroupsView::slotHideSpin()
{
    if (availableWidget->hideCheckBox->isChecked())
        slotActivateFilter();
}

void AvailableGroupsView::slotShow()
{
	if (showAll == true) // switch to showing subscribed only
	{
		showAll = false;
		availableWidget->showButton->setText(tr("Show All"));
		availableWidget->subscribeButton->setEnabled(false);
	}
	else // switch back to showing all
	{
		showAll = true;
		availableWidget->showButton->setText(tr("Only Show Subscribed"));
		availableWidget->subscribeButton->setEnabled(true);
	}

	availableWidget->unsubscribeButton->setEnabled(true);

	slotActivateFilter();
}

void AvailableGroupsView::slotActivateFilter()
{
	QString s = m_filterEdit->text();
	qDebug() << "Filtering by string: " << s;

	availableNewsgroups->setEnabled(false);
	m_filterEdit->setEnabled(false);

	int rowCount = model->fullRowCount();

	if (rowCount == 0)
		return;

    QRegExp::PatternSyntax syntax = QRegExp::FixedString;
    Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive;

    QRegExp rx(s, caseSensitivity, syntax);

	if (!rx.isValid())
		QMessageBox::warning(parent, tr("Error"), tr("Invalid regular expression!") );

	QModelIndex subjItem;
    bool checkSize = false;

    uint mustHaveArticles = 9999999; // This is the spin box limit
    if (availableWidget->hideCheckBox->isChecked())
    {
        checkSize = true;
        mustHaveArticles = availableWidget->hideSpinBox->value();
    }

    uint maxArticlesFound;

	for  (int i=rowCount - 1; i>=0; --i)
	{        
        subjItem = model->index(i, 0);

        maxArticlesFound = 0;
        if (checkSize)
        {
            for (int j=1; j<=model->columnCount(); ++j)
            {
                if (model->index(i, j).data().type() == QVariant::String)
                    maxArticlesFound = qMax(maxArticlesFound, model->rawData(model->index(i, j)).toUInt());
            }
        }

		if (!availableNewsgroups->isRowHidden(subjItem.row(), QModelIndex()))
		{
			if ((subjItem.data().toString().indexOf(rx, 0) == -1) ||
                    (checkSize && maxArticlesFound < mustHaveArticles) ||
					(!showAll && !model->getItem(subjItem)->isSubscribed()))
			{
				availableNewsgroups->setRowHidden(subjItem.row(), QModelIndex(), true);
			}
		}
		else // currently hidden
		{
			if ((subjItem.data().toString().indexOf(rx,0) != -1) &&
                    (!checkSize || maxArticlesFound >= mustHaveArticles) &&
					(showAll || model->getItem(subjItem)->isSubscribed()))
			{
				availableNewsgroups->setRowHidden(subjItem.row(), QModelIndex(), false);
			}
		}
	}

    for (int j=0; j < model->columnCount(); ++j)
        availableNewsgroups->resizeColumnToContents(j);

	availableNewsgroups->setEnabled(true);
	m_filterEdit->setEnabled(true);
}

void AvailableGroupsView::slotSubscribe()
{
    QList<QModelIndex> selectedGroups = availableNewsgroups->selectionModel()->selectedRows(ag_AvailableGroup_Col);

	if (selectedGroups.size() == 0)
	{
		quban->getLogAlertList()->logMessage(LogAlertList::Alert, tr("No newsgroups selected for subscription"));
		return;
	}

	QString groupName = selectedGroups.at(0).data().toString();
	aGroups->subscribe(groupName);
	model->getItem(selectedGroups.at(0))->setSubscribed(true);
	emit sigRefreshView();
}

void AvailableGroupsView::slotUnsubscribe()
{
	availableWidget->subscribeButton->setEnabled(true);
	availableWidget->unsubscribeButton->setEnabled(false);

    QList<QModelIndex> selectedGroups = availableNewsgroups->selectionModel()->selectedRows(ag_AvailableGroup_Col);

	if (selectedGroups.size() == 0)
	{
		quban->getLogAlertList()->logMessage(LogAlertList::Alert, tr("No newsgroups selected to un-subscribe from"));
		return;
	}

	QString groupName = selectedGroups.at(0).data().toString();
	QString dbQuestion = tr("Do you want to unsubscribe from \n ") + groupName + " ?";
	int result = QMessageBox::question(parent, tr("Question"), dbQuestion, QMessageBox::Yes, QMessageBox::No);
	switch (result)
	{
		case QMessageBox::Yes:
			aGroups->slotUnsubscribe(groupName);
			emit sigUnsubscribe(groupName); // this goes to subscribed groups

			if (!showAll)
			{
			    QModelIndex subjItem = selectedGroups.at(0);
			    availableNewsgroups->setRowHidden(subjItem.row(), QModelIndex(), true);
			}

			break;
		case QMessageBox::No:
			break;
	}
}

void AvailableGroupsView::slotUnsubscribed(QString)
{
	availableWidget->subscribeButton->setEnabled(true);
	availableWidget->unsubscribeButton->setEnabled(false);
}

void AvailableGroupsView::slotGroupSelectionChanged(const QModelIndex & selected)
{
	if (model->getItem(selected)->isSubscribed())
	{
		availableWidget->subscribeButton->setEnabled(false);
		availableWidget->unsubscribeButton->setEnabled(true);
	}
	else
	{
		availableWidget->subscribeButton->setEnabled(true);
		availableWidget->unsubscribeButton->setEnabled(false);
	}
}

void AvailableGroupsView::slotRefreshView()
{
    for (int i=0; i<availableNewsgroups->model()->columnCount(); ++i)
        availableNewsgroups->resizeColumnToContents(i);
}
