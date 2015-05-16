/***************************************************************************
     Copyright (C) 2011 by Martin Demet
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
#include "newsgroupdialog.h"
#include "newsgroupdetail.h"

NewsGroupDetail::NewsGroupDetail(QString , QString , QStringList _cats, Group * _g, QWidget* _parent)
	: QWidget(_parent), parent(_parent), cats(_cats), g(_g)
{
	setupUi(this);
}

NewsGroupDetail::NewsGroupDetail( NewsGroup * _ng, Servers *_servers, QStringList _cats, QWidget * _parent)
      : QWidget(_parent), parent(_parent), cats(_cats), servers(_servers),  ng(_ng)
{
	setupUi(this);
	buildDetails();
}

void NewsGroupDetail::updateDetails(NewsGroup* _ng, Servers* _servers, QWidget* _parent)
{
	servers = _servers;
	ng = _ng;
	parent = _parent;
	buildDetails();
}

void NewsGroupDetail::buildDetails()
{
	m_serverList->setSortingEnabled(false);
	m_serverList->setAllColumnsShowFocus(true);
// TODO	m_serverList->setColumnAlignment(1, Qt::AlignRight);
// TODO	m_serverList->setColumnAlignment(2, Qt::AlignRight);
// TODO	m_serverList->setColumnAlignment(3, Qt::AlignRight);
// TODO	m_serverList->setColumnAlignment(4, Qt::AlignRight);
// TODO	m_serverList->setColumnAlignment(5, Qt::AlignRight);

	bool HWMheld = false;
	bool serverAvailable = false;

	QMap<int, NntpHost*>::iterator it;
	QTreeWidgetItem *serverItem;
	for (it=servers->begin() ; it != servers->end() ; ++it)
	{
		NntpHost *nh=*it;

		serverIdList.append(nh->id);

		serverItem=new QTreeWidgetItem(m_serverList);
		serverItem->setText(0, nh->name);
		if (it.value()->serverType != NntpHost::dormantServer &&
		   (ng->serverPresence.contains(it.key()) && (ng->serverPresence[it.key()] == true)))
		{
//			serverItem->setText(1, QString("%L1").arg(ng->low[nh->id]));
//			serverItem->setText(2, QString("%L1").arg(ng->high[nh->id]));
//			serverItem->setText(3, QString("%L1").arg(ng->serverParts[nh->id]));
			serverAvailable = true;
			serverItem->setText(1, QString("%1").arg(ng->low[nh->id]));
			serverItem->setText(2, QString("%1").arg(ng->high[nh->id]));
			if (ng->high[nh->id] > 0)
				HWMheld = true;
			serverItem->setText(3, QString("%1").arg(ng->serverParts[nh->id]));

			serverItem->setText(4, ng->serverRefresh[nh->id]);
		}
		else
		{
			serverItem->setText(1, "N/A");
			serverItem->setText(2, "N/A");
			serverItem->setText(3, "N/A");
			serverItem->setText(4, "N/A");
		}
	}

	serverItem=new QTreeWidgetItem(m_localServer);
	serverItem->setText(0, tr("localhost"));

//	serverItem->setText(1, QString("%L1").arg(ng->localLow));
//	serverItem->setText(2, QString("%L1").arg(ng->localHigh));
//	serverItem->setText(3, QString("%L1").arg(ng->localParts));
	serverItem->setText(1, QString("%1").arg(ng->localLow));
	serverItem->setText(2, QString("%1").arg(ng->localHigh));
	serverItem->setText(3, QString("%1").arg(ng->localParts));


	// Start of get settings
	bool updateRequired = false;

	uint getFrom = 0;

	if (ng->localHigh)
	    getFrom = ng->localHigh + 1;
	uint getTo   = ng->localHigh + 1;

	for (it=servers->begin() ; it != servers->end() ; ++it)
	{
		NntpHost *nh=*it;

		if (it.value()->serverType != NntpHost::dormantServer &&
		   (ng->serverPresence.contains(it.key()) && (ng->serverPresence[it.key()] == true)))
		{
			if (ng->localHigh < ng->high[nh->id])
			{
				getTo = ng->high[nh->id];
				updateRequired = true;
			}

			if (getFrom == 0)
			{
				getFrom = ng->low[nh->id];
				updateRequired = true;
			}
		}
	}

	if (updateRequired)
	{
//		getFromVal->setText(QString("%L1").arg(getFrom));
//		getToVal->setText(QString("%L1").arg(getTo));
		getFromVal->setText(QString("%1").arg(getFrom));
		getToVal->setText(QString("%1").arg(getTo));
	}

	// Start of expire settings
	if (ng->localParts == 0)
		expireGroupBox->setEnabled(false);
	else
	{
        if (((NewsGroupDialog*)parent)->severAvailabilityRetention->isChecked())
        {
        	expireByServButton->setChecked(true);

        	uint lowest = 0;
        	for (it=servers->begin() ; it != servers->end() ; ++it)
        	{
        		NntpHost *nh=*it;

        		if (it.value()->serverType != NntpHost::dormantServer &&
        		   (ng->serverPresence.contains(it.key()) && (ng->serverPresence[it.key()] == true)))
        		{
        			if (lowest == 0 || ng->low[nh->id] < lowest)
        			    lowest = ng->low[nh->id];
        		}
        	}

//        	expireIdVal->setText(QString("%L1").arg(lowest));
        	expireIdVal->setText(QString("%1").arg(lowest));
        }
        else
        {
        	expireByDateButton->setChecked(true);
//        	expireDaysVal->setText(QString("%L1").arg(((NewsGroupDialog*)parent)->daySpin->value()));
        	expireDaysVal->setText(QString("%1").arg(((NewsGroupDialog*)parent)->daySpin->value()));
        }

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

	connect(refreshServersButton, SIGNAL(clicked()), parent, SLOT(slotRefreshServers()));
	connect(this, SIGNAL(sigExpireHeaders(uint, uint)), parent, SLOT(slotExpireHeaders(uint, uint)));
	connect(this, SIGNAL(sigGetHeaders(uint, uint)), parent, SLOT(slotGetHeaders(uint, uint)));

//	if (serverAvailable == true && HWMheld == false)
//		((NewsGroupDialog*)parent)->slotRefreshServers();
}

NewsGroupDetail::~NewsGroupDetail()
{
}

void NewsGroupDetail::updateGroupStats(NewsGroup *ng, NntpHost *nh, uint lowWater, uint highWater, uint articleParts)
{
	ng->low.insert(nh->id, lowWater);
	ng->high.insert(nh->id, highWater);
	ng->serverParts.insert(nh->id, articleParts);
	ng->serverRefresh.insert(nh->id, QDate::currentDate().toString("dd.MMM.yyyy"));

    QTreeWidgetItemIterator it(m_serverList);
    while (*it)
    {
        if ((*it)->text(0) == nh->name)
        {
//            (*it)->setText(1, QString("%L1").arg(lowWater));
//            (*it)->setText(2, QString("%L1").arg(highWater));
//            (*it)->setText(3, QString("%L1").arg(articleParts));
            (*it)->setText(1, QString("%1").arg(lowWater));
            (*it)->setText(2, QString("%1").arg(highWater));
            (*it)->setText(3, QString("%1").arg(articleParts));
            (*it)->setText(4, QDate::currentDate().toString("dd.MMM.yyyy"));
            break;
        }
        ++it;
    }

	// Start of get settings
    bool updateRequired = false;
	uint getTo   = ng->localHigh + 1;

	QMap<int, NntpHost*>::iterator mit;
	for (mit=servers->begin() ; mit != servers->end() ; ++mit)
	{
		NntpHost *nh=*mit;

		if (mit.value()->serverType != NntpHost::dormantServer &&
		   (ng->serverPresence.contains(mit.key()) && (ng->serverPresence[mit.key()] == true)))
		{
			if (ng->localHigh >= ng->high[nh->id])
			    continue;
			else
			{
				getTo = ng->high[nh->id];
				updateRequired = true;
			}
		}
	}

	if (updateRequired)
//		getToVal->setText(QString("%L1").arg(getTo));
	    getToVal->setText(QString("%1").arg(getTo));

	// expired
	if (expireGroupBox->isEnabled() == true &&
		expireByIdButton->isEnabled() == true)
	{
		updateRequired = false;

		uint expireBelow = INT_MAX;

		QMap<int, NntpHost*>::iterator mit;
		for (mit=servers->begin() ; mit != servers->end() ; ++mit)
		{
			NntpHost *nh=*mit;

			if (mit.value()->serverType != NntpHost::dormantServer &&
			   (ng->serverPresence.contains(mit.key()) && (ng->serverPresence[mit.key()] == true)))
			{
				if (ng->low[nh->id] >= expireBelow)
				    continue;
				else
				{
					expireBelow = ng->low[nh->id];
					updateRequired = true;
				}
			}
		}

		if (updateRequired)
//			expireIdVal->setText(QString("%L1").arg(expireBelow));
		    expireIdVal->setText(QString("%1").arg(expireBelow));
	}
}

void NewsGroupDetail::slotGetClicked()
{
	if (ng->isUpdating() == false)
	{
		if (getFromButton->isChecked() == true)
		{
			if (getFromVal->text().size() > 0 && getToVal->text().size() > 0)
			{
				bool ok;
				bool ok2;

	//        	uint from = getFromVal->text().remove(',').remove(' ').toUInt(&ok); // Should get rid of thousand separators in most locales
	//        	uint to   = getToVal->text().remove(',').remove(' ').toUInt(&ok2); // Should get rid of thousand separators in most locales
				uint from = getFromVal->text().toUInt(&ok);
				uint to   = getToVal->text().toUInt(&ok2);

				if (ok == true && ok2 == true && to >= from)
				{
					ng->startUpdating();
					emit sigGetHeaders(from, to);
				}
				else
				{
					qDebug() << "Bad vals " << ok << ", " << ok2 << ", " << from << ", " << to << ", " << getFromVal->text();
				    QMessageBox::warning ( this, tr("Invalid Value"), tr("Values entered can only be positive integers"));
				}
			}
			else
			{
				qDebug() << "Bad strings " << getFromVal->text() << ", " << getToVal->text();
				QMessageBox::warning ( this, tr("Invalid Value"), tr("Values entered can only be positive integers"));
			}
		}
		else if (getAllButton->isChecked() == true)
		{
			emit sigGetHeaders(0, 0);
		}
		else if (getLastButton->isChecked() == true)
		{
			if (getLastVal->text().size() > 0)
			{
				bool ok;
				bool updateRequired = false;
				uint from = INT_MAX;
				uint to = 0;

	//        	uint last = getLastVal->text().remove(',').remove(' ').toUInt(&ok); // Should get rid of thousand separators in most locales
				uint last = getLastVal->text().toUInt(&ok);

				if (ok == true && last)
				{
					--last;

					QMap<int, NntpHost*>::iterator it;
					for (it=servers->begin() ; it != servers->end() ; ++it)
					{
						NntpHost *nh=*it;

						if (it.value()->serverType != NntpHost::dormantServer &&
						   (ng->serverPresence.contains(it.key()) && (ng->serverPresence[it.key()] == true)))
						{
							if (to < ng->high[nh->id])
							{
								to = ng->high[nh->id];
								updateRequired = true;
							}

							if (from > ng->low[nh->id])
							{
								from = ng->low[nh->id];
								updateRequired = true;
							}
						}
					}

					if (updateRequired == true)
					{
						if (from < (to - last))
							from = to - last;
						ng->startUpdating();
						emit sigGetHeaders(from, to);
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

	//        	uint next = getNextVal->text().remove(',').remove(' ').toUInt(&ok); // Should get rid of thousand separators in most locales
				uint next = getNextVal->text().toUInt(&ok);

				if (ok == true && next)
				{
					--next;

					QMap<int, NntpHost*>::iterator it;
					for (it=servers->begin() ; it != servers->end() ; ++it)
					{
						NntpHost *nh=*it;

						if (it.value()->serverType != NntpHost::dormantServer &&
						   (ng->serverPresence.contains(it.key()) && (ng->serverPresence[it.key()] == true)))
						{
							if (to < ng->high[nh->id])
							{
								to = ng->high[nh->id];
								updateRequired = true;
							}

							if (from > ng->low[nh->id])
							{
								from = ng->low[nh->id];
								updateRequired = true;
							}
						}
					}

					if (updateRequired == true)
					{
						if (ng->localHigh >= from)
							from = ng->localHigh + 1;

						if (to > (from + next))
							to = from + next;
						ng->startUpdating();
						emit sigGetHeaders(from, to);
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
	else
	{
		QMessageBox::warning(this, tr("Error"), tr("Cannot get headers while the newsgroup is updating"));
	}
}

void NewsGroupDetail::slotExpireClicked()
{
	if (ng->isUpdating() == false)
	{
		if (expireByServButton->isChecked() == true)
		{
			ng->lastExpiry = QDate::currentDate().toString("dd.MMM.yyyy");
			lastExpiryDate->setText(ng->lastExpiry);
			ng->startUpdating();
			emit sigExpireHeaders(EXPIREBYSERV, 0);
		}
		else if (expireByDateButton->isChecked() == true)
		{
			if (expireDaysVal->text().size() > 0)
			{
				bool ok;

				uint expireVal = expireDaysVal->text().toUInt(&ok);

				if (ok == true)
				{
					ng->lastExpiry = QDate::currentDate().toString("dd.MMM.yyyy");
					lastExpiryDate->setText(ng->lastExpiry);
					ng->startUpdating();
					emit sigExpireHeaders(EXPIREBYDATE, expireVal);
				}
				else
				{
					qDebug() << "Bad value for expire";
					QMessageBox::warning ( this, tr("Invalid Value"), tr("Values entered can only be positive integers"));
				}
			}
			else
			{
				qDebug() << "Missing value for expire";
				QMessageBox::warning ( this, tr("Invalid Value"), tr("Values entered can only be positive integers"));
			}
		}
		else if (expireByIdButton->isChecked() == true)
		{
			if (expireIdVal->text().size() > 0)
			{
				bool ok;

				uint expireVal = expireIdVal->text().toUInt(&ok);

				if (ok == true)
				{
					ng->lastExpiry = QDate::currentDate().toString("dd.MMM.yyyy");
					lastExpiryDate->setText(ng->lastExpiry);
					ng->startUpdating();
					emit sigExpireHeaders(EXPIREBYID, expireVal);
				}
				else
				{
					qDebug() << "Bad value for expire";
					QMessageBox::warning ( this, tr("Invalid Value"), tr("Values entered can only be positive integers"));
				}
			}
			else
			{
				qDebug() << "Missing value for expire";
				QMessageBox::warning ( this, tr("Invalid Value"), tr("Values entered can only be positive integers"));
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
			ng->startUpdating();
			emit sigExpireHeaders(EXPIREALL, 0);
		}
	}
	else
	{
		QMessageBox::warning(this, tr("Error"), tr("Cannot expire while the newsgroup is updating"));
	}
}

void NewsGroupDetail::refreshDetails()
{
	QTreeWidgetItem *serverItem = m_localServer->topLevelItem(0) ;

	serverItem->setText(1, QString("%1").arg(ng->localLow));
	serverItem->setText(2, QString("%1").arg(ng->localHigh));
	serverItem->setText(3, QString("%1").arg(ng->localParts));

	ng->stopUpdating();
}
