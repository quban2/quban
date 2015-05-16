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
#include <QMessageBox>
#include <QDate>
#include <QFileDialog>
#include <QDebug>
#include <QtDebug>
#include "appConfig.h"
#include "headerlist.h"
#include "catman.h"
#include "expireDb.h"
#include "newsgroupcontents.h"

NewsGroupContents::NewsGroupContents( NewsGroup * _ng, Servers *_servers, QWidget * _parent)
      : QDialog(_parent), parent(_parent), servers(_servers),  ng(_ng)
{
	setupUi(this);

	maxLocalLow = 0;
	maxLocalHigh = 0;
	maxLocalParts = 0;

	m_alias->setText(ng->getAlias());

	connect(buttonCancel, SIGNAL(clicked()), this, SLOT(reject()));
	connect(getButton, SIGNAL(clicked()), this, SLOT(slotGetClicked()));
	connect(expireButton, SIGNAL(clicked()), this, SLOT(slotExpireClicked()));
	connect(m_localServer, SIGNAL(clicked(const QModelIndex &)), this, SLOT(slotLocalServerClicked()));
	connect(getLastVal, SIGNAL(cursorPositionChanged(int,int)), this, SLOT(slotMoveToLast()));
	connect(getNextVal, SIGNAL(cursorPositionChanged(int,int)), this, SLOT(slotMoveToNext()));
	connect(expireDaysVal, SIGNAL(cursorPositionChanged(int,int)), this, SLOT(slotMoveToDays()));
	connect(expireDownloadDaysVal, SIGNAL(cursorPositionChanged(int,int)), this, SLOT(slotMoveToDownloadDays()));

	buildDetails();
}

void NewsGroupContents::reject()
{
  QDialog::reject();
}

void NewsGroupContents::updateDetails(NewsGroup* _ng, Servers* _servers, QWidget* _parent)
{
	servers = _servers;
	ng = _ng;
	parent = _parent;
	buildDetails();
}

void NewsGroupContents::buildDetails()
{
	m_serverList->setSortingEnabled(false);
	m_serverList->setAllColumnsShowFocus(true);
// TODO	m_serverList->setColumnAlignment(1, Qt::AlignRight);
// TODO	m_serverList->setColumnAlignment(2, Qt::AlignRight);
// TODO	m_serverList->setColumnAlignment(3, Qt::AlignRight);
// TODO	m_serverList->setColumnAlignment(4, Qt::AlignRight);
// TODO	m_serverList->setColumnAlignment(5, Qt::AlignRight);

	maxLocalLow = 0;
	maxLocalHigh = 0;
	maxLocalParts = 0;

	QMap<quint16, NntpHost*>::iterator it;
	QTreeWidgetItem *serverItem;

	if (servers->count() < 2)
	{
		allServersButton->setEnabled(false);
		allServersButton->setHidden(true);
		selectedServersButton->setEnabled(false);
		selectedServersButton->setHidden(true);
	}

	for (it=servers->begin() ; it != servers->end() ; ++it)
	{
		NntpHost *nh=*it;

        if (it.value() && it.value()->getServerType() != NntpHost::dormantServer &&
		   (ng->serverPresence.contains(it.key()) && (ng->serverPresence[it.key()] == true)))
		{
			serverIdList.append(nh->getId());

			serverItem=new QTreeWidgetItem(m_serverList);
			serverItem->setText(0, nh->getName());

			serverItem->setText(1, QString("%L1").arg(ng->low[nh->getId()]));
			serverItem->setText(2, QString("%L1").arg(ng->high[nh->getId()]));
			serverItem->setText(3, QString("%L1").arg(ng->serverParts[nh->getId()]));

			serverItem->setText(4, ng->serverRefresh[nh->getId()]);

			// Now populate local server details
			serverItem=new QTreeWidgetItem(m_localServer);
			serverItem->setText(0, tr("local ") + nh->getName());

			if (ng->low[nh->getId()] > ng->servLocalLow[nh->getId()])
			{
				ng->servLocalLow[nh->getId()] = ng->low[nh->getId()];
				ng->servLocalParts[nh->getId()] = ng->servLocalHigh[nh->getId()] -
						ng->servLocalLow[nh->getId()] + 1;
			}

			if (ng->servLocalLow[nh->getId()] > ng->servLocalHigh[nh->getId()])
			{
				ng->servLocalLow[nh->getId()] = 0;
				ng->servLocalHigh[nh->getId()] = 0;
				ng->servLocalParts[nh->getId()] = 0;
			}

			serverItem->setText(1, QString("%L1").arg(ng->servLocalLow[nh->getId()]));
			serverItem->setText(2, QString("%L1").arg(ng->servLocalHigh[nh->getId()]));
			serverItem->setText(3, QString("%L1").arg(ng->servLocalParts[nh->getId()]));

			maxLocalLow = qMax(maxLocalLow, ng->servLocalLow[nh->getId()]);
			maxLocalHigh = qMax(maxLocalHigh, ng->servLocalHigh[nh->getId()]);
			maxLocalParts = qMax(maxLocalParts, ng->servLocalParts[nh->getId()]);
		}
	}

	// Start of expire settings
	if (maxLocalParts == 0)
		expireGroupBox->setEnabled(false);
	else
	{
		if (ng->getDeleteOlderPosting() != 0)
        {
        	expireByDateButton->setChecked(true);
        	expireDaysVal->setText(QString("%1").arg(ng->getDeleteOlderPosting()));
        }
        else if (ng->getDeleteOlderDownload() != 0)
        {
        	expireByDownloadDate->setChecked(true);
        	expireDownloadDaysVal->setText(QString("%1").arg(ng->getDeleteOlderDownload()));
        }
        else
            neverExpireButton->setChecked(true);

        lastExpiryDate->setText(ng->lastExpiry);
	}

	m_serverList->resizeColumnToContents(0);
	m_serverList->resizeColumnToContents(1);
	m_serverList->resizeColumnToContents(2);
	m_serverList->resizeColumnToContents(3);
	m_serverList->resizeColumnToContents(4);

	m_localServer->resizeColumnToContents(0);
	m_localServer->resizeColumnToContents(1);
	m_localServer->resizeColumnToContents(2);
	m_localServer->resizeColumnToContents(3);

	connect(refreshServersButton, SIGNAL(clicked()), this, SLOT(slotRefreshServers()));
	connect(this, SIGNAL(sigExpireHeaders(uint, uint)), this, SLOT(slotExpireHeaders(uint, uint)));
	connect(this, SIGNAL(sigGetHeaders(quint16, quint64, quint64)), this, SLOT(slotGetHeaders(quint16, quint64, quint64)));
}

NewsGroupContents::~NewsGroupContents()
{
}

void NewsGroupContents::updateGroupStats(NewsGroup *ng, NntpHost *nh, uint lowWater, uint highWater, uint articleParts)
{
    QRadioButton* currentGet = getNextButton;

	if (getAllButton->isChecked())
		currentGet = getAllButton;
	else if (getLastButton->isChecked())
		currentGet = getLastButton;
	else if (getNextButton->isChecked())
		currentGet = getNextButton;

	ng->low.insert(nh->getId(), lowWater);
	ng->high.insert(nh->getId(), highWater);
	ng->serverParts.insert(nh->getId(), articleParts);
	ng->serverRefresh.insert(nh->getId(), QDate::currentDate().toString("dd.MMM.yyyy"));

    QTreeWidgetItemIterator it(m_serverList);
    while (*it)
    {
        if ((*it)->text(0) == nh->getName())
        {
            (*it)->setText(1, QString("%L1").arg(lowWater));
            (*it)->setText(2, QString("%L1").arg(highWater));
            (*it)->setText(3, QString("%L1").arg(articleParts));
            (*it)->setText(4, QDate::currentDate().toString("dd.MMM.yyyy"));
            break;
        }
        ++it;
    }

	currentGet->setChecked(true);
}

void NewsGroupContents::slotGetClicked()
{
    QMap<quint16, NntpHost*>::iterator mit;
    for (mit=servers->begin() ; mit != servers->end() ; ++mit)
    {
        NntpHost *nh=*mit;

        if (servers->count() > 1 && selectedServersButton->isChecked())
        {
            bool ignore = true;
            QList<QTreeWidgetItem*> selection=m_localServer->selectedItems();
            QListIterator<QTreeWidgetItem*> it(selection);
            QString thisServerLocalName = tr("local ") + nh->getName();

            while (it.hasNext())
            {
                QTreeWidgetItem*temp = it.next();
                if (temp->text(0) == thisServerLocalName)
                {
                    ignore = false; // if this server is selected then continue
                    break;
                }
            }

            if (ignore)
                continue;
        }

        if (nh && nh->getServerType() != NntpHost::dormantServer &&
                (ng->serverPresence.contains(nh->getId()) && (ng->serverPresence[nh->getId()] == true)))
        {
            if (ng->high[nh->getId()] == 0)// not yet populated
            {
                delayedServerIdList.append(nh->getId());
                emit sigGetGroupLimits(ng, nh->getId(), nh);
            }
            // we can download for this server whilst headers for this group are already being downloaded from other servers
            else if (ng->downloadingServers.contains(nh->getId()) == false)
                getHeaders(nh->getId());
        }
    }
}

void NewsGroupContents::getHeaders(quint16 hostId)
{
	if (getAllButton->isChecked() == true)
	{
		emit sigGetHeaders(hostId, ng->low[hostId], ng->high[hostId]);
	}
	else if (getLastButton->isChecked() == true)
	{
		if (getLastVal->text().size() > 0)
		{
			bool ok;
			bool updateRequired = false;
			uint from = INT_MAX;
			uint to = 0;

			uint last = getLastVal->text().toUInt(&ok);

			if (ok == true && last)
			{
				--last;

				if (to < ng->high[hostId])
				{
					to = ng->high[hostId];
					updateRequired = true;
				}

				if (from > ng->low[hostId])
				{
					from = ng->low[hostId];
					updateRequired = true;
				}

				if (updateRequired == true)
				{
					if (from < (to - last))
						from = to - last;
					emit sigGetHeaders(hostId, from, to);
				}
			}
			else
			{
				qDebug() << "Bad vals " << ok << ", " << last << ", " << getLastVal->text();
				QMessageBox::warning ( this, tr("Invalid Value"), tr("Values entered can only be positive integers"));
			}
		}
		else
		{
			QMessageBox::warning ( this, tr("Invalid Value"), tr("Values entered can only be positive integers"));
		}
	}
	else if (getNextButton->isChecked() == true)
	{
		if (getNextVal->text().size() > 0)
		{
			bool ok;
			bool updateRequired = false;
			uint from = INT_MAX;
			uint to = 0;

			uint next = getNextVal->text().toUInt(&ok);

			if (ok == true && next)
			{
				--next;

				if (to < ng->high[hostId])
				{
					to = ng->high[hostId];
					updateRequired = true;
				}

				if (from > ng->low[hostId])
				{
					from = ng->low[hostId];
					updateRequired = true;
				}

				if (updateRequired == true)
				{
					if (ng->servLocalHigh[hostId] >= from)
						from = ng->servLocalHigh[hostId] + 1;

					if (to > (from + next))
						to = from + next;

					if (from <= to)
					{
						emit sigGetHeaders(hostId, from, to);
					}
				}
			}
			else
			{
				qDebug() << "Bad vals " << ok << ", " << next << ", " << getNextVal->text();
				QMessageBox::warning ( this, tr("Invalid Value"), tr("Values entered can only be positive integers"));
			}
		}
		else
		{
			QMessageBox::warning ( this, tr("Invalid Value"), tr("Values entered can only be positive integers"));
		}
	}
}

void NewsGroupContents::slotExpireClicked()
{
    if (expireByDateButton->isChecked() == true)
    {
        if (expireDaysVal->text().size() > 0)
        {
            bool ok;

            uint expireVal = expireDaysVal->text().toUInt(&ok);

            if (ok == true)
            {
                ng->lastExpiry = QDate::currentDate().toString("dd.MMM.yyyy");
                lastExpiryDate->setText(ng->lastExpiry);
                emit sigExpireHeaders(EXPIREBYPOSTINGDATE, expireVal);
            }
            else
            {
                qDebug() << "Bad value for expire";
                QMessageBox::warning ( this, tr("Invalid Value for Posting Date"), tr("Values entered can only be positive integers"));
            }
        }
        else
        {
            qDebug() << "Missing value for expire";
            QMessageBox::warning ( this, tr("Invalid Value for Posting Date"), tr("Values entered can only be positive integers"));
        }
    }
    else if (expireByDownloadDate->isChecked() == true)
    {
        if (expireDownloadDaysVal->text().size() > 0)
        {
            bool ok;

            uint expireVal = expireDownloadDaysVal->text().toUInt(&ok);

            if (ok == true)
            {
                ng->lastExpiry = QDate::currentDate().toString("dd.MMM.yyyy");
                lastExpiryDate->setText(ng->lastExpiry);
                emit sigExpireHeaders(EXPIREBYDOWNLOADDATE, expireVal);
            }
            else
            {
                qDebug() << "Bad value for expire";
                QMessageBox::warning ( this, tr("Invalid Value for Download Date"), tr("Values entered can only be positive integers"));
            }
        }
        else
        {
            qDebug() << "Missing value for expire";
            QMessageBox::warning ( this, tr("Invalid Value for Download Date"), tr("Values entered can only be positive integers"));
        }
    }
    else if (expireAllButton->isChecked() == true)
    {
        int result = QMessageBox::question(
                    this,
                    tr("Expire all headers?"),
                    tr("This option will delete all headers for this newsgroup. Are you sure that you wish to continue?"),
                    QMessageBox::Yes, QMessageBox::No);

        switch (result)
        {
        case QMessageBox::Yes:
            //Do nothing...
            break;
        case QMessageBox::No:
            return;
            break;
        }

        ng->lastExpiry = QDate::currentDate().toString("dd.MMM.yyyy");
        lastExpiryDate->setText(ng->lastExpiry);
        emit sigExpireHeaders(EXPIREALL, 0);
    }
}

void NewsGroupContents::refreshDetails()
{
    ; // is this still needed ???
}

void NewsGroupContents::slotRefreshServers()
{
    emit sigGetGroupLimits(ng);
    refreshServersButton->setText("Refreshing...");
    refreshServersButton->setEnabled(false);
}

void NewsGroupContents::slotLimitsUpdate(NewsGroup *_ng, NntpHost *nh, uint lowWater, uint highWater, uint articleParts)
{
	if (ng->getName() != _ng->getName()) // may have several windows open
		return;

	updateGroupStats(_ng, nh, lowWater, highWater, articleParts);
    refreshServersButton->setText("Refresh");
    refreshServersButton->setEnabled(true);

    if (delayedServerIdList.contains(nh->getId()))
    {
    	delayedServerIdList.removeAll(nh->getId());
    	getHeaders(nh->getId());
    }
}

void NewsGroupContents::slotHeaderDownloadProgress(NewsGroup *_ng, NntpHost *nh, quint64 lowWater, quint64 highWater, quint64 articleParts)
{
	if (ng->getName() != _ng->getName()) // may have several windows open
		return;

    QTreeWidgetItemIterator it(m_localServer);
    while (*it)
    {
        if ((*it)->text(0) == tr("local ") + nh->getName())
        {
            (*it)->setText(1, QString("%L1").arg(lowWater));
            (*it)->setText(2, QString("%L1").arg(highWater));
            (*it)->setText(3, QString("%L1").arg(articleParts));
            break;
        }
        ++it;
    }
}

void NewsGroupContents::slotGetHeaders(quint16 hostId, quint64 from, quint64 to)
{
	emit sigGetHeaders(hostId, ng, from, to);

	getButton->setEnabled(false);
	expireButton->setEnabled(false);
}

void NewsGroupContents::slotUpdateFinished(NewsGroup *_ng)
{
	if (_ng->ngName == ng->ngName)
	    refreshDetails();

	getButton->setEnabled(true);
	expireButton->setEnabled(true);
}

void NewsGroupContents::slotExpireHeaders(uint exptype, uint expvalue)
{
	emit sigExpireNewsgroup(ng, exptype, expvalue);

	getButton->setEnabled(false);
	expireButton->setEnabled(false);
}

void NewsGroupContents::slotMoveToLast()
{
	getLastButton->setChecked(true);
}

void NewsGroupContents::slotMoveToNext()
{
	getNextButton->setChecked(true);
}

void NewsGroupContents::slotMoveToDays()
{
	expireByDateButton->setChecked(true);
}

void NewsGroupContents::slotMoveToDownloadDays()
{
	expireByDownloadDate->setChecked(true);
}

void NewsGroupContents::slotLocalServerClicked()
{
	selectedServersButton->setChecked(true);
}
