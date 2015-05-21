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

#include <QDebug>
#include <QtDebug>
#include <QMessageBox>
#include <QTimer>
#include <QSslConfiguration>
#include <QSslCipher>
#include <QDateTime>
#include "serverconnection.h"
#include "sslerrorchecker.h"
#include "common.h"
#include "quban.h"
#include "nntphost.h"

extern Quban* quban;

NntpHost::~NntpHost()
{
    if (enableServerLimit)
        quban->serversList->m_saveServer(this);
}

void NntpHost::loadHost( char *p)
{
	char *i = p;
    i=retrieve(i,hostName);

	if (!i) return;

    i=retrieve(i,name);

	if (!i) return;

    i=retrieve(i,userName);

	if (!i) return;

	if (userName == QUBAN_GHOST_SERVER)
		deleted = true;
	else
		deleted = false;
    i=retrieve(i,pass);

	if (!i) return;

    quint16 numCerts;
    int sz16=sizeof(quint16);

    memcpy(&timeout,i,sz16);
    i+=sz16;
    memcpy(&id,i,sz16);
    i+=sz16;
    memcpy(&priority, i, sz16);
    i+=sz16;
    memcpy(&threadTimeout,i,sz16);
    i+=sz16;
    memcpy(&retries, i, sz16);
    i+=sz16;
    memcpy(&maxThreads,i,sz16);
    i+=sz16;
    memcpy(&port, i, sz16);
    i+=sz16;
    memcpy(&sslSocket, i, sz16);
    i+=sz16;
    memcpy(&serverType, i, sz16);
    i+=sz16;
    memcpy(&serverFlags, i, sizeof(quint64));
    i+=sizeof(quint64);
    memcpy(&serverFlagsDate, i, sizeof(quint32));
    i+=sizeof(quint32);
    memcpy(&enabledServerExtensions, i, sizeof(quint64));
    i+=sizeof(quint64);
    memcpy(&sslAllowableErrors, i, sizeof(quint64));
    i+=sizeof(quint64);
    memcpy(&sslProtocol, i, sz16);
    i+=sz16;

    if (sslProtocol > 15 || sslProtocol < 0)
    {
#if (QT_VERSION < QT_VERSION_CHECK(4, 8, 5))

    sslProtocol = QSsl::SslV3;

#else

    sslProtocol = QSsl::SecureProtocols;

#endif
    }

    memcpy(&enableServerLimit, i, sizeof(bool));
    i+=sizeof(bool);

    memcpy(&maxDownloadLimit, i, sizeof(float));
    i+=sizeof(float);

    memcpy(&downloaded, i, sizeof(quint64));
    i+=sizeof(quint64);

    qDebug() << "host " << id <<  " has maxDownloadLimit = " << maxDownloadLimit;
    qDebug() << "host " << id <<  " has downloaded = " << downloaded;

    memcpy(&limitUnits, i, sizeof(quint16));
    i+=sizeof(quint16);

    memcpy(&frequency, i, sizeof(quint8));
    i+=sizeof(quint8);

    memcpy(&resetDate, i, sizeof(quint32));
    i+=sizeof(quint32);

    memcpy(&numCerts, i, sz16);
    i+=sz16;

    if (numCerts)
    {
        quint32 certSize;

        memcpy(&certSize, i, sizeof(quint32));
        i+=sizeof(quint32);

        QByteArray ba(i, certSize);
        QSslCertificate* cert = new QSslCertificate(ba);
        sslCertificate = *cert;
        delete cert;
    }

	workingThreads=0;
	speed=0;
	item=0;
	size=0;
	enabled=true;
    paused = false;

    computeMaxDownloadLimitBytes();

	return;
}

quint32 NntpHost::getSize()
{
	quint32 hostSize =  hostName.length()+name.length()+pass.length()+userName.length()+(14*sizeof(quint16))+(3*sizeof(quint64))+sizeof(quint32);

	hostSize += sizeof(quint16); // number of certificates

	if (!sslCertificate.isNull())
	{
		hostSize += sizeof(quint32); // certificate size
		hostSize += sslCertificate.toPem().length();
	}

    // for server limits
    hostSize += (sizeof(bool) + sizeof(quint64) + sizeof(quint16) + sizeof(quint8) + sizeof(quint32) + sizeof(float));

	return hostSize;
}

char * NntpHost::saveHost()
{
	quint16 numCerts = 0;
	char *p=new char[getSize()];
	char *i=p;
	i=insert(hostName, i);
	i=insert(name,i);
	i=insert(userName,i);
	i=insert(pass,i);

	int sz16=sizeof(quint16);

	memcpy(i,&timeout,sz16);
	i+=sz16;
	memcpy(i,&id,sz16);
	i+=sz16;
	memcpy(i,&priority,sz16);
	i+=sz16;
	memcpy(i,&threadTimeout,sz16);
	i+=sz16;
	memcpy(i, &retries, sz16);
	i+=sz16;
	memcpy(i,&maxThreads,sz16);
	i+=sz16;
	memcpy(i,&port, sz16);
	i+=sz16;
	memcpy(i,&sslSocket, sz16);
	i+=sz16;
	memcpy(i,&serverType, sz16);
	i+=sz16;
	memcpy(i,&serverFlags, sizeof(quint64));
	i+=sizeof(quint64);
	memcpy(i,&serverFlagsDate, sizeof(quint32));
	i+=sizeof(quint32);
	memcpy(i, &enabledServerExtensions, sizeof(quint64));
	i+=sizeof(quint64);
	memcpy(i, &sslAllowableErrors, sizeof(quint64));
	i+=sizeof(quint64);
    memcpy(i, &sslProtocol, sz16);
	i+=sz16;

    memcpy(i, &enableServerLimit, sizeof(bool));
    i+=sizeof(bool);
    memcpy(i, &maxDownloadLimit, sizeof(float));
    i+=sizeof(float);
    memcpy(i, &downloaded, sizeof(quint64));
    i+=sizeof(quint64);

    memcpy(i, &limitUnits, sz16);
    i+=sz16;
    memcpy(i, &frequency, sizeof(quint8));
    i+=sizeof(quint8);
    memcpy(i, &resetDate, sizeof(quint32));
    i+=sizeof(quint32);

	if (!sslCertificate.isNull())
	{
		numCerts = 1;
		memcpy(i, &numCerts, sz16);
		i+=sz16;
		quint32 certSize = sslCertificate.toPem().length();
		memcpy(i,&certSize, sizeof(quint32));
		i+=sizeof(quint32);
		memcpy(i,sslCertificate.toPem().data(), sslCertificate.toPem().length());
	}
	else
		memcpy(i, &numCerts, sz16);


	return p;
}

QMap<quint64, QString> NntpHost::extensionDescriptions;
QMap<qint16, quint64> NntpHost::sslErrorMap;
QMap<qint64, quint16> NntpHost::sslReverseErrorMap;
QMap<quint64, QString> NntpHost::sslErrorDescriptions;

void NntpHost::initHostMaps()
{
	extensionDescriptions[xzver]     = tr("xzver : Header compression");
	extensionDescriptions[xfeatgzip] = tr("xfeatgzip : Header compression");

	sslErrorDescriptions[UnableToGetIssuerCertificate]        = tr("Unable To Get Issuer Certificate");
	sslErrorDescriptions[UnableToDecryptCertificateSignature] = tr("Unable To Decrypt Certificate Signature");
	sslErrorDescriptions[UnableToDecodeIssuerPublicKey]       = tr("Unable To Decode Issuer Public Key");
	sslErrorDescriptions[CertificateSignatureFailed]          = tr("Certificate Signature Failed");
	sslErrorDescriptions[CertificateNotYetValid]              = tr("Certificate Not Yet Valid");
	sslErrorDescriptions[CertificateExpired]                  = tr("Certificate Expired");
	sslErrorDescriptions[InvalidNotBeforeField]               = tr("Invalid Not Before Field");
	sslErrorDescriptions[InvalidNotAfterField]                = tr("Invalid Not After Field");
	sslErrorDescriptions[SelfSignedCertificate]               = tr("Self Signed Certificate");
	sslErrorDescriptions[SelfSignedCertificateInChain]        = tr("Self Signed Certificate In Chain");
	sslErrorDescriptions[UnableToGetLocalIssuerCertificate]   = tr("Unable To Get Local Issuer Certificate");
	sslErrorDescriptions[UnableToVerifyFirstCertificate]      = tr("Unable To Verify First Certificate");
	sslErrorDescriptions[CertificateRevoked]                  = tr("Certificate Revoked");
	sslErrorDescriptions[InvalidCaCertificate]                = tr("Invalid Ca Certificate");
	sslErrorDescriptions[PathLengthExceeded]                  = tr("Path Length Exceeded");
	sslErrorDescriptions[InvalidPurpose]                      = tr("Invalid Purpose");
	sslErrorDescriptions[CertificateUntrusted]                = tr("Certificate Untrusted");
	sslErrorDescriptions[CertificateRejected]                 = tr("Certificate Rejected");
	sslErrorDescriptions[SubjectIssuerMismatch]               = tr("Subject Issuer Mismatch");
	sslErrorDescriptions[AuthorityIssuerSerialNumberMismatch] = tr("Authority Issuer Serial Number Mismatch");
	sslErrorDescriptions[NoPeerCertificate]                   = tr("No Peer Certificate");
	sslErrorDescriptions[HostNameMismatch]                    = tr("Host Name Mismatch");
	sslErrorDescriptions[UnspecifiedError]                    = tr("Unspecified Error");
	sslErrorDescriptions[NoSslSupport]                        = tr("No Ssl Support");
	sslErrorDescriptions[CertificateBlacklisted]              = tr("Certificate Blacklisted");

	sslErrorMap[1] = UnableToGetIssuerCertificate;
	sslErrorMap[2] = UnableToDecryptCertificateSignature;
	sslErrorMap[3] = UnableToDecodeIssuerPublicKey;
	sslErrorMap[4] = CertificateSignatureFailed;
	sslErrorMap[5] = CertificateNotYetValid;
	sslErrorMap[6] = CertificateExpired;
	sslErrorMap[7] = InvalidNotBeforeField;
	sslErrorMap[8] = InvalidNotAfterField;
	sslErrorMap[9] = SelfSignedCertificate;
	sslErrorMap[10] = SelfSignedCertificateInChain;
	sslErrorMap[11] = UnableToGetLocalIssuerCertificate;
	sslErrorMap[12] = UnableToVerifyFirstCertificate;
	sslErrorMap[13] = CertificateRevoked;
	sslErrorMap[14] = InvalidCaCertificate;
	sslErrorMap[15] = PathLengthExceeded;
	sslErrorMap[16] = InvalidPurpose;
	sslErrorMap[17] = CertificateUntrusted;
	sslErrorMap[18] = CertificateRejected;
	sslErrorMap[19] = SubjectIssuerMismatch;
	sslErrorMap[20] = AuthorityIssuerSerialNumberMismatch;
	sslErrorMap[21] = NoPeerCertificate;
	sslErrorMap[22] = HostNameMismatch;
	sslErrorMap[-1] = UnspecifiedError;
	sslErrorMap[23] = NoSslSupport;
	sslErrorMap[24] = CertificateBlacklisted;

	sslReverseErrorMap[UnableToGetIssuerCertificate] = 1;
	sslReverseErrorMap[UnableToDecryptCertificateSignature] = 2;
	sslReverseErrorMap[UnableToDecodeIssuerPublicKey] = 3;
	sslReverseErrorMap[CertificateSignatureFailed] = 4;
	sslReverseErrorMap[CertificateNotYetValid] = 5;
	sslReverseErrorMap[CertificateExpired] = 6;
	sslReverseErrorMap[InvalidNotBeforeField] = 7;
	sslReverseErrorMap[InvalidNotAfterField] = 8;
	sslReverseErrorMap[SelfSignedCertificate] = 9;
	sslReverseErrorMap[SelfSignedCertificateInChain] = 10;
	sslReverseErrorMap[UnableToGetLocalIssuerCertificate] = 11;
	sslReverseErrorMap[UnableToVerifyFirstCertificate] = 12;
	sslReverseErrorMap[CertificateRevoked] = 13;
	sslReverseErrorMap[InvalidCaCertificate] = 14;
	sslReverseErrorMap[PathLengthExceeded] = 15;
	sslReverseErrorMap[InvalidPurpose] = 16;
	sslReverseErrorMap[CertificateUntrusted] = 17;
	sslReverseErrorMap[CertificateRejected] = 18;
	sslReverseErrorMap[SubjectIssuerMismatch] = 19;
	sslReverseErrorMap[AuthorityIssuerSerialNumberMismatch] = 20;
	sslReverseErrorMap[NoPeerCertificate] = 21;
	sslReverseErrorMap[HostNameMismatch] = 22;
	sslReverseErrorMap[UnspecifiedError] = -1;
	sslReverseErrorMap[NoSslSupport] = 23;
	sslReverseErrorMap[CertificateBlacklisted] = 24;
}

void NntpHost::slotTestServer()
{
	if (gettingStatus == true)
		return;

	gettingStatus = true;

	kes = new QSslSocket(this);

	serverTestTimer = new QTimer(this);
	Q_CHECK_PTR(serverTestTimer);

    connect(serverTestTimer, SIGNAL(timeout()), this, SLOT(slotTestTimeout()));
    serverTestTimer->setInterval(timeout * 1000);
    serverTestTimer->setSingleShot(true);
    serverTestTimer->start();

	if (sslSocket == 0)
	{
		connect(kes, SIGNAL(connected()), this, SLOT(slotTestConnected()));
		connect(kes, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(slotTcpError(QAbstractSocket::SocketError)));
		kes->connectToHost(hostName, port);
	}
	else
	{
        kes->setProtocol((QSsl::SslProtocol)sslProtocol);
		connect(kes, SIGNAL(encrypted()), this, SLOT(slotTestConnected()));
		connect(kes, SIGNAL(sslErrors(const QList<QSslError> &)), this, SLOT(slotSslError(const QList<QSslError> &)));
		kes->connectToHostEncrypted(hostName, port);
	}
}

void NntpHost::slotSslError(const QList<QSslError> & sslErrs)
{
	QString errorString = hostName + " :\n\n";


/*
    QSslCipher cipher = kes->sessionCipher();
    dumpCipher( cipher );


void Connector::dumpCipher( const QSslCipher &cipher )
{
    qDebug() << "\n== Cipher ==";

    qDebug() << "Authentication:\t\t" << cipher.authenticationMethod();
    qDebug() << "Encryption:\t\t" << cipher.encryptionMethod();
    qDebug() << "Key Exchange:\t\t" << cipher.keyExchangeMethod();
    qDebug() << "Cipher Name:\t\t" << cipher.name();
    qDebug() << "Protocol:\t\t" <<  cipher.protocolString();
    qDebug() << "Supported Bits:\t\t" << cipher.supportedBits();
    qDebug() << "Used Bits:\t\t" << cipher.usedBits();
}

*/
	if (serverTestTimer)
	{
	    serverTestTimer->stop();
		serverTestTimer = 0;
	}

	QSslCertificate newCertificate;

	int i = 1;

	foreach(QSslError sslErr, sslErrs)
	{
		if (sslErrorMap.contains((int)((enum QSslError::SslError)sslErr.error())))
		    sslAllErrors |= sslErrorMap.value((int)((enum QSslError::SslError)sslErr.error()));
		else
		{
			qDebug() << "Unexpected SSL error : " << ((int)((enum QSslError::SslError)sslErr.error()));
			sslAllErrors |= sslErrorMap.value(-1);
		}

		qDebug() << "SSL error : " << (int)((enum QSslError::SslError)sslErr.error());

		if (!sslErr.certificate().isNull())
			newCertificate = sslCertificate = sslErr.certificate();

		errorString += (QString::number(i) + ") " + sslErr.errorString() + ".\n");
		i++;
	}

	if (sslAllErrors == sslAllowableErrors && // Nothing has changed ...
	    newCertificate == sslCertificate)
	{
        qDebug() << hostName << " has matching SSL certificate and ignore errors of " << sslAllowableErrors;
        expectedSslErrors = sslErrs;

        ServerConnection* serverTest = new ServerConnection(0);
        serverTest->setAddress(hostName);
        serverTest->setPort(QString::number(port));

        if (kes->mode() == 0)
        {
            serverTest->setMode("Unencrypted");
            serverTest->setEncryption("");
        }
        else
        {
            serverTest->setMode("Encrypted");
            serverTest->setEncryption(kes->sslConfiguration().sessionCipher().name() + " (" +
                                      QString::number(kes->sslConfiguration().sessionCipher().usedBits()) + ")");
        }

        serverTest->exec();
    }
	else
        QMessageBox::warning(parent, tr("SSL connection error"), errorString);

	kes->deleteLater();
	kes = 0;
	gettingStatus = false;
}

void NntpHost::slotTcpError( QAbstractSocket::SocketError err)
{
	QString errorString;

	kes->disconnect(SIGNAL(connected()));

	if (serverTestTimer)
	{
	    serverTestTimer->stop();
	    serverTestTimer = 0;
	}

	switch (err)
	{
		case QAbstractSocket::SocketTimeoutError:
				//No error, it's a timeout
			errorString= name + tr(" : Timeout");
			break;
		case QAbstractSocket::SocketAccessError:
			errorString= name + tr(" : OS or firewall prohibited the connection");
			break;
		case QAbstractSocket::ConnectionRefusedError:
			errorString= name + tr(" : Connection refused");
			break;
		case QAbstractSocket::NetworkError:
			errorString= name + tr(" : Network failure while connecting");
			break;
		default:
			errorString= name + tr(" : Unknown error while connecting: ") + QString::number(err);
			break;
	}

	kes->deleteLater();
	kes = 0;
	gettingStatus = false;

	QMessageBox::warning(parent,  name + tr(" : TCP connection error"), errorString);
}

void NntpHost::slotTestTimeout()
{
	if (serverTestTimer)
	{
	    serverTestTimer = 0;
	}

	if (kes)
	{
		kes->disconnect(SIGNAL(encrypted()));
		kes->abort();
		kes->deleteLater();
		kes = 0;
	}
	else
		return;

	QMessageBox::warning(parent, tr("Server connection error"), name + tr(" : Timeout whilst attempting to connect"));

	gettingStatus = false;
}

void NntpHost::slotTestConnected()
{
	if (serverTestTimer)
	{
	    serverTestTimer->stop();
	    serverTestTimer = 0;
	}

	if (sslSocket == 1)
		qDebug() << "Cipher = " << kes->sslConfiguration().sessionCipher();

    ServerConnection* serverTest = new ServerConnection(0);
    serverTest->setAddress(hostName);
    serverTest->setPort(QString::number(port));

    if (kes->mode() == 0)
    {
        serverTest->setMode("Unencrypted");
        serverTest->setEncryption("");
    }
    else
    {
        serverTest->setMode("Encrypted");
        serverTest->setEncryption(kes->sslConfiguration().sessionCipher().name() + " (" +
                                  QString::number(kes->sslConfiguration().sessionCipher().usedBits()) + ")");
    }

    serverTest->exec();

	kes->disconnectFromHost();

	kes->deleteLater();
	kes = 0;
	gettingStatus = false;
}

void NntpHost::serverValidated(bool validated, QString errorString, QList<QSslError> sslErrs)
{

    if (validated)
    {
        validated = true;
        emit sigServerValidated(id, true);
    }
    else
    {
        if (sslSocket)
        {
            if (errorString != "Unable to read any data after being advised data was present")
            {
                SslErrorChecker* sslErrorChecker = new SslErrorChecker(hostName, sslAllowableErrors, sslAllErrors, &sslCertificate, &newCertificate);

                int retCode = sslErrorChecker->exec();
                if (retCode == QDialog::Accepted)
                {
                    sslAllowableErrors = sslAllErrors;
                    sslCertificate = newCertificate;

                    expectedSslErrors = sslErrs;

                    //qDebug() << "Successfully validated " << hostName;
                    validated = true;
                    //qDebug() << "Emitting server is validated from SSL errs ok";
                    emit sigServerValidated(id, true);
                }
                else
                {
                    // Red Icon
                    emit sigInvalidServer(id);
                }
            }
            else
            {
                QMessageBox::warning(parent, tr("SSL connection error"), tr("Failed to connect to ") + hostName + ": " + errorString);
                // Red Icon
                emit sigInvalidServer(id);
            }
        }
        else
        {
            QMessageBox::warning(parent, tr("TCP connection error"), tr("Failed to connect to ") + hostName + ": " + errorString);
            // Red Icon
            emit sigInvalidServer(id);
        }
    }

    validating = false;
    emit sigSaveServer(this);

}

void NntpHost::pauseServer()
{
    // Reload the queue ????
    paused = true;

    emit sigPauseThreads(id);

}

void NntpHost::resumeServer()
{
    paused = false;

    emit sigResumeThreads(id);

}

void NntpHost::computeMaxDownloadLimitBytes()
{
    if (limitUnits == 0)  // Kb
        maxDownloadLimitBytes = maxDownloadLimit * 1024;
    else if (limitUnits == 1)  // Mb
        maxDownloadLimitBytes = maxDownloadLimit * 1024 * 1024;
    else // Gb
        maxDownloadLimitBytes = maxDownloadLimit * 1024 * 1024 * 1024;

    qDebug() << "host " << id <<  " has maxDownloadLimitBytes = " << maxDownloadLimitBytes;
}
