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
#include <QDebug>
#include <QtDebug>
#include "IdListWidgetItem.h"
#include "addserver.h"

AddServer::AddServer(QWidget* parent, Qt::WindowFlags fl)
: QDialog(parent, fl)
{
	setupUi(this);

	this->setWindowTitle(tr("Add server"));

	addServerWidget = new AddServerWidget(this);
    addServerWidget->updateButton->setHidden(true);
    addServerWidget->testServerButton->setHidden(true);
	vLayout->addWidget(addServerWidget);
	addServerWidget->groupBox->setEnabled(false);
	addServerWidget->groupBox_2->setEnabled(false);
}

AddServer::AddServer( NntpHost *host, QWidget * parent, Qt::WindowFlags fl ) : QDialog(parent, fl)
{
	setupUi(this);

	this->setWindowTitle(tr("Amend server"));

	addServerWidget = new AddServerWidget(host, this);
    addServerWidget->updateButton->setHidden(true);
	vLayout->addWidget(addServerWidget);
	addServerWidget->groupBox->setEnabled(true);

	IdListWidgetItem* item;

	quint64 serverFlags = host->getServerFlags();
    for (quint64 i=NntpHost::xzver; i<=NntpHost::xfeatgzip; i *= 2)
	{
		if (serverFlags & i)
		{
			item = new IdListWidgetItem(i, NntpHost::extensionDescriptions.value(i), addServerWidget->serverExtensionsList);
			if (host->getEnabledServerExtensions() & i)
			    item->setCheckState(Qt::Checked);
			else
				item->setCheckState(Qt::Unchecked);
		}
	}

	if (host->getSslSocket())
	{
	    addServerWidget->groupBox_2->setEnabled(true);

	    quint64 sslAllErrors = host->getSslAllErrors();
	    quint64 sslAllowableErrors = host->getSslAllowableErrors();
		for (quint64 i=NntpHost::UnableToGetIssuerCertificate; i<=NntpHost::CertificateBlacklisted; i *= 2)
		{
			if (sslAllErrors & i)
			{
				item = new IdListWidgetItem(i, NntpHost::sslErrorDescriptions.value(i), addServerWidget->SSLErrorsList);
				if (sslAllowableErrors & i)
				    item->setCheckState(Qt::Checked);
				else
					item->setCheckState(Qt::Unchecked);
			}
		}
	}
}

AddServer::~AddServer()
{
}

void AddServer::reject()
{
  QDialog::reject();
}

void AddServer::accept()
{
	QDialog::accept();
}

void AddServer::newServerReq(NntpHost *host)
{
	emit newServer(host);
}

void AddServer::testServerReq(QString addr, quint16 qport, int sslSocket, qint16 sslProtocol, int qtimeout)
{
    emit testServer(addr, qport, sslSocket, sslProtocol, qtimeout);
}
