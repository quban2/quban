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
#include <QSslCertificate>
#include <QTextCursor>
#include <QDebug>
#include <QtDebug>
#include "IdListWidgetItem.h"
#include "nntphost.h"
#include "sslerrorchecker.h"

SslErrorChecker::SslErrorChecker(QString hostName, quint64 oldFlags, quint64 newFlags, QSslCertificate* oldCert,
		QSslCertificate* newCert, QWidget *parent) : QDialog(parent)
{
	qDebug() << "SslErrorChecker : " << oldFlags << ", " << newFlags;
	setupUi(this);

    this->setWindowTitle(this->windowTitle() + tr(" for server: ") + hostName);

	QTextCursor tmpCursor;
//	IdListWidgetItem* item;
    bool oldCertShown = false;

	if (oldFlags)
	{
		previousFrame->setHidden(false);
		previousFrame->setEnabled(true);

		for (quint64 i=NntpHost::UnableToGetIssuerCertificate; i<=NntpHost::CertificateBlacklisted; i *= 2)
		{
			if (oldFlags & i)
                new IdListWidgetItem(i, NntpHost::sslErrorDescriptions.value(i), SSLErrorsList);

            if (!oldCert->isNull() && oldCertShown == false)
			{
                oldCertShown = true;
				showCerificate(oldCert, existingCertificate);
				existingCertificate->setReadOnly(true);
			    tmpCursor = existingCertificate->textCursor();
			    tmpCursor.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
			    existingCertificate->setTextCursor(tmpCursor);
			}
		}
	}
	else
	{
		previousFrame->setHidden(true);
		previousFrame->setEnabled(false);
	}

	for (quint64 i=NntpHost::UnableToGetIssuerCertificate; i<=NntpHost::CertificateBlacklisted; i *= 2)
	{
		if (newFlags & i)
            new IdListWidgetItem(i, NntpHost::sslErrorDescriptions.value(i), SSLErrorsListNew);
	}

	if (!newCert->isNull())
	{
		showCerificate(newCert, newCertificate);
	    newCertificate->setReadOnly(true);
	    tmpCursor = newCertificate->textCursor();
	    tmpCursor.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
	    newCertificate->setTextCursor(tmpCursor);
	}

	connect(buttonOk, SIGNAL(clicked()), this, SLOT(accept()));
	connect(buttonCancel, SIGNAL(clicked()), this, SLOT(reject()));
}

SslErrorChecker::~SslErrorChecker()
{

}

void SslErrorChecker::showCerificate(QSslCertificate* certificate, QPlainTextEdit* displayArea)
{
	//qDebug() << certificate->toPem();

	displayArea->appendPlainText("== Subject Info ==\n");
	displayArea->appendPlainText("CommonName:\t\t" + certificate->subjectInfo( QSslCertificate::CommonName ));
	displayArea->appendPlainText("Organization:\t\t" + certificate->subjectInfo( QSslCertificate::Organization ));
	displayArea->appendPlainText("LocalityName:\t\t" + certificate->subjectInfo( QSslCertificate::LocalityName ));
	displayArea->appendPlainText("OrganizationalUnitName:\t" +certificate->subjectInfo( QSslCertificate::OrganizationalUnitName ));
	displayArea->appendPlainText("StateOrProvinceName:\t\t" + certificate->subjectInfo( QSslCertificate::StateOrProvinceName ));

	QMultiMap<QSsl::AlternateNameEntryType, QString> altNames = certificate->alternateSubjectNames();
	if ( !altNames.isEmpty() ) {
		displayArea->appendPlainText("\nAlternate Subject Names (DNS):");
		foreach (const QString &altName, altNames.values(QSsl::DnsEntry))
		{
			displayArea->appendPlainText(altName);
		}
	}

	displayArea->appendPlainText("\n== Issuer Info ==\n");
	displayArea->appendPlainText("CommonName:\t\t" + certificate->issuerInfo( QSslCertificate::CommonName ));
	displayArea->appendPlainText("Organization:\t\t" + certificate->issuerInfo( QSslCertificate::Organization ));
	displayArea->appendPlainText("LocalityName:\t\t" + certificate->issuerInfo( QSslCertificate::LocalityName ));
	displayArea->appendPlainText("OrganizationalUnitName:\t" + certificate->issuerInfo( QSslCertificate::OrganizationalUnitName ));
	displayArea->appendPlainText("StateOrProvinceName:\t\t" + certificate->issuerInfo( QSslCertificate::StateOrProvinceName ));

	displayArea->appendPlainText("\n== Certificate ==\n");
	//displayArea->appendPlainText("Serial Number:\t\t");
	//displayArea->appendPlainText(certificate->serialNumber()); // This seems buggy
	displayArea->appendPlainText("Effective Date:\t\t" + certificate->effectiveDate().toString());
	displayArea->appendPlainText("Expiry Date:\t\t" + certificate->expiryDate().toString());
	displayArea->appendPlainText("Valid:\t\t\t" + QString(certificate->isValid() ? "Yes" : "No"));
}
