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
#include "addserverwidget.h"
#include "IdListWidgetItem.h"
#include "addserver.h"

AddServerWidget::AddServerWidget(QWidget * _parent) // This is a create
  : QWidget(_parent), parent(_parent)
{
	setupUi(this);

	m_priorityInput->setRange(1,10);
	m_threadInput->setRange(1,50);
    m_timeoutInput->setRange(1,300);
	m_timeoutInput->setSingleStep(5);
	m_threadTimeoutInput->setRange(1,30);
	validator=new QIntValidator(1,65535,this);
	m_portEdit->setValidator(validator);

    olddt = QDateTime::currentDateTime();

    protocol_label->setEnabled(false);
    protocol_comboBox->setEnabled(false);

    protocol_comboBox->addItem("SSL v3", QSsl::SslV3); // default at 4.7.4
    protocol_comboBox->addItem("SSL v2", QSsl::SslV2);

#if (QT_VERSION < QT_VERSION_CHECK(4, 8, 5))

    protocol_comboBox->addItem("TLSv1.0", QSsl::TlsV1);
    protocol_comboBox->setCurrentIndex(0);

#elif (QT_VERSION < QT_VERSION_CHECK(5, 0, 2))

    protocol_comboBox->addItem("TLSv1.0", QSsl::TlsV1);
    protocol_comboBox->addItem("TLSv1.0 and SSLv3", QSsl::TlsV1SslV3);
    protocol_comboBox->addItem("Protocols known to be secure", QSsl::SecureProtocols);
    protocol_comboBox->setCurrentIndex(4);

#else

    protocol_comboBox->addItem("TLSv1.0", QSsl::TlsV1_0);
    protocol_comboBox->addItem("TLSv1.1", QSsl::TlsV1_1);
    protocol_comboBox->addItem("TLSv1.2", QSsl::TlsV1_2);
    protocol_comboBox->addItem("TLSv1.0 and SSLv3", QSsl::TlsV1SslV3);
    protocol_comboBox->addItem("Protocols known to be secure", QSsl::SecureProtocols);
    protocol_comboBox->setCurrentIndex(6);

#endif

	connect(testServerButton, SIGNAL(clicked()), this, SLOT(slotTestServer()));
	connect(updateButton, SIGNAL(clicked()), this, SLOT(slotUpdateServer()));

    connect(m_nameEdit, SIGNAL(textChanged(const QString &)), this, SIGNAL(sigSaveNeeded()));
    connect(m_portEdit, SIGNAL(textChanged(const QString &)), this, SLOT(slotPortChanged(const QString &)));
    connect(m_addrEdit, SIGNAL(textChanged(const QString &)), this, SLOT(slotEnableTest()));
    connect(m_portEdit, SIGNAL(textChanged(const QString &)), this, SLOT(slotEnableTest()));
	connect(m_logCheck, SIGNAL(toggled(bool)), this, SLOT(slotLoginRequired(bool)));
    connect(limitsCheckBox, SIGNAL(toggled(bool)), this, SLOT(slotLimitsRequired(bool)));
    connect(resetDateEdit, SIGNAL(dateChanged(const QDate &)), this, SLOT(slotResetDateChanged(const QDate &)));

	testServerButton->setEnabled(false);

	connect(buttonOk, SIGNAL(clicked()), this, SLOT(accept()));
	connect(buttonCancel, SIGNAL(clicked()), this, SLOT(reject()));

    connect(sslCheck, SIGNAL(stateChanged(int)), this, SLOT(sslStateChange(int)));

}

AddServerWidget::AddServerWidget( NntpHost *host, QWidget * _parent)  // This is an update
    : QWidget(_parent), parent(_parent)
{
	setupUi(this);

	m_priorityInput->setRange(1,10);
	m_threadInput->setRange(1,50);
    m_timeoutInput->setRange(1,300);
	m_timeoutInput->setSingleStep(5);
	m_threadTimeoutInput->setRange(1,30);
	validator=new QIntValidator(1,65535,this);
	m_portEdit->setValidator(validator);

	currentCertificate = *(host->getSslCertificate());
	populate(host);

	connect(buttonOk, SIGNAL(clicked()), this, SLOT(accept()));
	connect(buttonCancel, SIGNAL(clicked()), this, SLOT(reject()));
	connect(m_portEdit, SIGNAL(textChanged (const QString &)), this, SLOT(slotPortChanged(const QString &)));
    connect(limitsCheckBox, SIGNAL(toggled(bool)), this, SLOT(slotLimitsRequired(bool)));
    connect(resetDateEdit, SIGNAL(dateChanged(const QDate &)), this, SLOT(slotResetDateChanged(const QDate &)));
}

AddServerWidget::~AddServerWidget()
{
    Q_DELETE(validator);
}

void AddServerWidget::populate(NntpHost* host)
{
	m_addrEdit->setText(host->getHostName());
	m_threadInput->setValue(host->getMaxThreads());
	m_nameEdit->setText(host->getName());
	m_passEdit->setText(host->getPass());
	m_portEdit->setText(QString::number(host->getPort()));
	m_priorityInput->setValue(host->getPriority());
	m_timeoutInput->setValue(host->getTimeout());
	m_threadTimeoutInput->setValue(host->getThreadTimeout());
	m_retriesInput->setValue(host->getRetries());
	if (host->getSslSocket() == 1)
	    sslCheck->setChecked(true);

    pausedCheckBox->setChecked(host->isPaused());

//	qDebug() << "Adding server type " << host->serverType;
	serverType->setCurrentIndex(host->getServerType());

	connect(m_logCheck, SIGNAL(toggled(bool)), this, SLOT(slotLoginRequired(bool)));

	if (!(host->getUserName().isEmpty()))
	{
		m_logCheck->setChecked(true);
		m_userEdit->setText(host->getUserName());
		m_passEdit->setText(host->getPass());
	}

    if (host->getServerLimitEnabled())
    {
        limitsCheckBox->setChecked(true);
        enableLimits();
    }

    limitsSpinBox->setValue(host->getMaxDownloadLimit());

    // Downloaded (read only)
    quint64 downloaded = host->getDownloaded();
    double  dDownloaded = 0.0;
    quint16 units = host->getLimitUnits();
    if (units == 0)
        dDownloaded = (double)downloaded / 1024.0; // Kb
    else if (units == 1)
        dDownloaded = (double)downloaded / (1024.0 * 1024.0);  // Mb
    else
        dDownloaded = (double)downloaded / (1024.0 * 1024.0 * 1024.0); // Gb

    currentDownloadsSpinBox->setValue(dDownloaded);

    QDateTime dt;
    dt.setTime_t(host->getResetDate());
    resetDateEdit->setDisplayFormat("dd.MMM.yyyy");
    resetDateEdit->setDate(dt.date());
    olddt = dt;

    limitsUnitsComboBox->addItem("Kb");
    limitsUnitsComboBox->addItem("Mb");
    limitsUnitsComboBox->addItem("Gb");
    limitsUnitsComboBox->setCurrentIndex(units);

    limitsFreqComboBox->addItem(tr("Daily"));
    limitsFreqComboBox->addItem(tr("Weekly"));
    limitsFreqComboBox->addItem(tr("Monthly"));
    limitsFreqComboBox->setCurrentIndex(host->getFrequency());

    resetEndDateEdit->setDisplayFormat("dd.MMM.yyyy");

    QDateTime newdt;
    if (host->getFrequency() == 2) // monthly
        newdt = dt.addMonths(1);
    else if (host->getFrequency() == 1) // weekly
        newdt = dt.addDays(7);
    else // daily
         newdt = dt.addDays(1);

    resetEndDateEdit->setDate(newdt.date());

    if (!host->getSslSocket())
    {
        protocol_label->setEnabled(false);
        protocol_comboBox->setEnabled(false);
    }

    qint16 currentSslProtocol = host->getSslProtocol();

    protocol_comboBox->addItem("SSL v3", QSsl::SslV3); // default at 4.7.4
    if (currentSslProtocol == QSsl::SslV3) protocol_comboBox->setCurrentIndex(0);

    protocol_comboBox->addItem("SSL v2", QSsl::SslV2);
    if (currentSslProtocol == QSsl::SslV2) protocol_comboBox->setCurrentIndex(1);

#if (QT_VERSION < QT_VERSION_CHECK(4, 8, 5))

    protocol_comboBox->addItem("TLSv1.0", QSsl::TlsV1);
    if (currentSslProtocol == QSsl::TlsV1) protocol_comboBox->setCurrentIndex(2);

#elif (QT_VERSION < QT_VERSION_CHECK(5, 0, 2))

    protocol_comboBox->addItem("TLSv1.0", QSsl::TlsV1);
    if (currentSslProtocol == QSsl::TlsV1) protocol_comboBox->setCurrentIndex(2);

    protocol_comboBox->addItem("TLSv1.0 and SSLv3", QSsl::TlsV1SslV3);
    if (currentSslProtocol == QSsl::TlsV1SslV3) protocol_comboBox->setCurrentIndex(3);

    protocol_comboBox->addItem("Protocols known to be secure", QSsl::SecureProtocols);
    if (currentSslProtocol == QSsl::SecureProtocols) protocol_comboBox->setCurrentIndex(4);

#else

    protocol_comboBox->addItem("TLSv1.0", QSsl::TlsV1_0);
    if (currentSslProtocol == QSsl::TlsV1_0) protocol_comboBox->setCurrentIndex(2);

    protocol_comboBox->addItem("TLSv1.1", QSsl::TlsV1_1);
    if (currentSslProtocol == QSsl::TlsV1_1) protocol_comboBox->setCurrentIndex(3);

    protocol_comboBox->addItem("TLSv1.2", QSsl::TlsV1_2);
    if (currentSslProtocol == QSsl::TlsV1_2) protocol_comboBox->setCurrentIndex(4);

    protocol_comboBox->addItem("TLSv1.0 and SSLv3", QSsl::TlsV1SslV3);
    if (currentSslProtocol == QSsl::TlsV1SslV3) protocol_comboBox->setCurrentIndex(5);

    protocol_comboBox->addItem("Protocols known to be secure", QSsl::SecureProtocols);
    if (currentSslProtocol == QSsl::SecureProtocols) protocol_comboBox->setCurrentIndex(6);

#endif

    connect(testServerButton, SIGNAL(clicked()), this, SLOT(slotTestServer()));
}

void AddServerWidget::slotLoginRequired(bool b)
{
	m_userEdit->setEnabled(b);
	m_passEdit->setEnabled(b);
}

void AddServerWidget::slotLimitsRequired(bool b)
{
    if (b)
    {
        resetDateEdit->setDate(QDate::currentDate());
    }
    limitsSpinBox->setEnabled(b);
    resetDateEdit->setEnabled(b);
    limitsUnitsComboBox->setEnabled(b);
    limitsFreqComboBox->setEnabled(b);
    resetLabel->setEnabled(b);
    resetEndLabel->setEnabled(b);
    resetEndDateEdit->setEnabled(b);
    currentDownloadsLabel->setEnabled(b);
    currentDownloadsSpinBox->setEnabled(b);
}

void AddServerWidget::enableLimits()
{
    limitsSpinBox->setEnabled(true);
    resetDateEdit->setEnabled(true);
    limitsUnitsComboBox->setEnabled(true);
    limitsFreqComboBox->setEnabled(true);
    resetLabel->setEnabled(true);
    resetEndLabel->setEnabled(true);
    resetEndDateEdit->setEnabled(true);
    currentDownloadsLabel->setEnabled(true);
    currentDownloadsSpinBox->setEnabled(true);
}

void AddServerWidget::reject()
{
  ((AddServer*)parent)->reject();
}

void AddServerWidget::accept()
{
	//Check input fields

	if (m_nameEdit->text().isEmpty())
	    QMessageBox::warning ( this, tr("Missing Name"), tr("Please fill in name field"));
	else if (m_addrEdit->text().isEmpty())
	    QMessageBox::warning ( this, tr("Missing Host Name"), tr("Please fill in host name field"));
	else if (m_portEdit->text().isEmpty())
	    QMessageBox::warning ( this, tr("Missing Port"), tr("Please fill in port field"));
	else
	{
        host=new NntpHost(parent, 0); // Db will be added by serverslist
		host->setHostName(m_addrEdit->text().trimmed());
		host->setMaxThreads(m_threadInput->value());
		host->setName(m_nameEdit->text().trimmed());

		host->setPort(m_portEdit->text().toInt());
		host->setPriority(m_priorityInput->value());
		host->setTimeout(m_timeoutInput->value());
		host->setThreadTimeout(m_threadTimeoutInput->value());
		host->setRetries(m_retriesInput->value());
		host->setSpeed(0);
		host->setWorkingThreads(0);
		if (sslCheck->isChecked())
        {
			host->setSslSocket(1);
            host->setSslProtocol((QSsl::SslProtocol)(protocol_comboBox->itemData(protocol_comboBox->currentIndex()).toInt()));
        }
		else
        {
			host->setSslSocket(0);
#if (QT_VERSION < QT_VERSION_CHECK(4, 8, 5))
            host->setSslProtocol(QSsl::SslV3);
#else
            host->setSslProtocol(QSsl::SecureProtocols);
#endif
        }

		host->setServerType(serverType->currentIndex());

        host->setPaused(pausedCheckBox->isChecked());

		host->setServerStatus(NntpHost::serverOk);

		if (m_logCheck->isChecked())
		{
			host->setUserName(m_userEdit->text().trimmed());
			host->setPass(m_passEdit->text().trimmed());
		}
		else
		{
			host->setUserName("");
			host->setPass("");
		}

        if (limitsCheckBox->isChecked()) // checked save all details
        {
           host->setServerLimitEnabled(true);
        }
        else
        {
            host->setServerLimitEnabled(false);
        }

        host->setMaxDownloadLimit(limitsSpinBox->value());

        QDate d = resetDateEdit->date();
        QTime t = QTime::currentTime();
        QDateTime dt(d, t);
        host->setResetDate(dt.toTime_t());

        host->setLimitUnits(limitsUnitsComboBox->currentIndex());
        host->setFrequency(limitsFreqComboBox->currentIndex());

        host->setEnabled(true);

		IdListWidgetItem* item;
		quint64 extensions = 0,
				enabledExtensions = 0;

		for (int i=0; i<serverExtensionsList->count(); ++i)
		{
			item = (IdListWidgetItem*)serverExtensionsList->item(i);
			extensions += item->id;
			if (item->checkState() == Qt::Checked)
				enabledExtensions += item->id;
		}

		qDebug() << "Setting extensions to " << extensions << ", use to " << enabledExtensions;
		host->setServerFlags(extensions);
		host->setServerFlagsDate(0);

		host->setEnabledServerExtensions(enabledExtensions);

		quint64 sslAllowableErrors = 0;

		for (int i=0; i<SSLErrorsList->count(); ++i)
		{
			item = (IdListWidgetItem*)SSLErrorsList->item(i);
			if (item->checkState() == Qt::Checked)
				sslAllowableErrors += item->id;
		}

		qDebug() << "Setting allowable ssl to " << sslAllowableErrors;
		host->setSslAllowableErrors(sslAllowableErrors);

		host->setSslCertificate(new QSslCertificate(currentCertificate));

		((AddServer*)parent)->newServerReq(host);
		((AddServer*)parent)->accept();
	}
}

void AddServerWidget::slotPortChanged(const QString& newText)
{
	if (newText == "443" || newText == "563")
    {
		sslCheck->setChecked(true);
    }
	else
    {
		sslCheck->setChecked(false);
    }
}

void AddServerWidget::slotResetDateChanged(const QDate& newDate)
{
    QDateTime dt;
    dt.setDate(newDate);

    if (dt > QDateTime::currentDateTime()) // cannot set into the future - disable the feature until required ...
    {
        resetDateEdit->setDate(olddt.date());
        return;
    }

    resetEndDateEdit->setDisplayFormat("dd.MMM.yyyy");

    QDateTime newdt;
    if (limitsFreqComboBox->currentIndex() == 2) // monthly
        newdt = dt.addMonths(1);
    else if (limitsFreqComboBox->currentIndex() == 1) // weekly
        newdt = dt.addDays(7);
    else // daily
         newdt = dt.addDays(1);

    resetEndDateEdit->setDate(newdt.date());

    olddt = dt;
}

void AddServerWidget::slotEnableTest()
{
	if (m_addrEdit->text().size() && m_portEdit->text().size())
		testServerButton->setEnabled(true);
	else
		testServerButton->setEnabled(false);
}

void AddServerWidget::slotTestServer()
{
	int sslSocket;

	if (sslCheck->isChecked())
		sslSocket = 1;
	else
		sslSocket = 0;

	((AddServer*)parent)->testServerReq(m_addrEdit->text().trimmed(), m_portEdit->text().toInt(),
            sslSocket, (qint16)protocol_comboBox->itemData(protocol_comboBox->currentIndex()).toInt(), m_timeoutInput->value());
}

void AddServerWidget::slotUpdateServer()
{
	//Check input fields

	if (m_nameEdit->text().isEmpty())
	    QMessageBox::warning ( this, tr("Missing Name"), tr("Please fill in name field"));
	else if (m_addrEdit->text().isEmpty())
	    QMessageBox::warning ( this, tr("Missing Host Name"), tr("Please fill in host name field"));
	else if (m_portEdit->text().isEmpty())
	    QMessageBox::warning ( this, tr("Missing Port"), tr("Please fill in port field"));
	else
	{
        host=new NntpHost(parent, 0);
		host->setHostName(m_addrEdit->text().trimmed());
		host->setMaxThreads(m_threadInput->value());
		host->setName(m_nameEdit->text().trimmed());

		host->setPort(m_portEdit->text().toInt());
		host->setPriority(m_priorityInput->value());
		host->setTimeout(m_timeoutInput->value());
		host->setThreadTimeout(m_threadTimeoutInput->value());
		host->setRetries(m_retriesInput->value());
		host->setSpeed(0);
		host->setWorkingThreads(0);

        if (sslCheck->isChecked())
        {
            host->setSslSocket(1);
            host->setSslProtocol((QSsl::SslProtocol)(protocol_comboBox->itemData(protocol_comboBox->currentIndex()).toInt()));
        }
        else
        {
            host->setSslSocket(0);
#if (QT_VERSION < QT_VERSION_CHECK(4, 8, 5))
            host->setSslProtocol(QSsl::SslV3);
#else
            host->setSslProtocol(QSsl::SecureProtocols);
#endif
        }

		host->setServerType(serverType->currentIndex());

		host->setServerStatus(NntpHost::serverOk);

        if (limitsCheckBox->isChecked()) // checked save all details
        {
           host->setServerLimitEnabled(true);
        }
        else
        {
            host->setServerLimitEnabled(false);
        }

        host->setMaxDownloadLimit(limitsSpinBox->value());

        QDate d = resetDateEdit->date();
        QTime t(0, 0);
        QDateTime dt(d, t);
        host->setResetDate(dt.toTime_t());

        host->setLimitUnits(limitsUnitsComboBox->currentIndex());
        host->setFrequency(limitsFreqComboBox->currentIndex());

		IdListWidgetItem* item;
		quint64 extensions = 0,
				enabledExtensions = 0;

		for (int i=0; i<serverExtensionsList->count(); ++i)
		{
			item = (IdListWidgetItem*)serverExtensionsList->item(i);
			extensions += item->id;
			if (item->checkState() == Qt::Checked)
				enabledExtensions += item->id;
		}

		qDebug() << "Setting extensions to " << extensions << ", use to " << enabledExtensions;
		host->setServerFlags(extensions);
		host->setServerFlagsDate(0);

		host->setEnabledServerExtensions(enabledExtensions);

		quint64 sslAllowableErrors = 0;

		for (int i=0; i<SSLErrorsList->count(); ++i)
		{
			item = (IdListWidgetItem*)SSLErrorsList->item(i);
			if (item->checkState() == Qt::Checked)
				sslAllowableErrors += item->id;
		}

		qDebug() << "Setting allowable ssl to " << sslAllowableErrors;
		host->setSslAllowableErrors(sslAllowableErrors);

		host->setSslCertificate(new QSslCertificate(currentCertificate));

		host->setUserName(m_userEdit->text().trimmed());
		host->setPass(m_passEdit->text().trimmed());
	}
}

void AddServerWidget::sslStateChange(int state)
{
    if (state)
    {
        protocol_label->setEnabled(true);
        protocol_comboBox->setEnabled(true);
    }
    else
    {
        protocol_label->setEnabled(false);
        protocol_comboBox->setEnabled(false);
    }
}
