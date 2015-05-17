#ifndef NNTPHOST_H_
#define NNTPHOST_H_

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

#include <QString>
#include <QMap>
#include <QSslCertificate>
#include <QSslSocket>
#include <db_cxx.h>
#include "common.h"

#define QUBAN_GHOST_SERVER "*?_Q_DEL_SERV_?*"

class QTreeWidgetItem;
class QSslError;
class QTimer;

class NntpHost : public QObject
{
    Q_OBJECT

public:
    NntpHost(QWidget* p, Db* _serverDb) { parent = p; gettingStatus = false; kes = 0; sslAllErrors = 0; serverDb = _serverDb; }
    virtual ~NntpHost();
    void loadHost(char *p);
    char* saveHost();
	quint32 getSize();

    void pauseServer();
    void resumeServer();

    void serverValidated(bool validated, QString errorString, QList<QSslError> sslErrs);

	inline QString& getHostName() { return hostName; }
	inline QString& getName() { return name; }
	inline quint16  getPort() { return port; }
	inline QString& getUserName() { return userName; }
	inline QString& getPass() { return pass; }
	inline quint16  getTimeout() { return timeout; }
	inline quint16  getId() { return id; }
	inline quint16  getPriority() { return priority; }
	inline quint16  getThreadTimeout() { return threadTimeout; }
	inline quint16  getRetries() { return retries; }
	inline quint16  getMaxThreads() { return maxThreads; }
	inline quint16  getWorkingThreads() { return workingThreads; }
	inline quint16  getSslSocket() { return sslSocket; }
	inline quint16  getServerType() { return serverType; }
	inline quint8   getServerStatus() { return serverStatus; }
	inline quint64  getServerFlags() { return serverFlags; }
	inline quint32  getServerFlagsDate() { return serverFlagsDate; }
	inline bool     getEnabled() { return enabled; }
	inline bool     getDeleted() { return deleted; }
	inline bool     getValidated() { return validated; }
	inline bool     getValidating() { return validating; }
    inline bool     isPaused() { return paused; }
	inline quint64  getSpeed() { return speed; }
	inline QTreeWidgetItem * getItem() { return item; }
	inline quint64  getQueueSize() { return size; }
	inline quint64  getEnabledServerExtensions() { return enabledServerExtensions; }
	inline quint64  getSslAllowableErrors() { return sslAllowableErrors; }
	inline quint64  getSslAllErrors() { return sslAllErrors; }
    inline qint16  getSslProtocol() { return sslProtocol; }
	inline QSslCertificate* getSslCertificate() { return &sslCertificate; }
	inline QList<QSslError>& getExpectedSslErrors() { return expectedSslErrors; }
    inline QMap<qint16, quint64>& getSslErrorMap() { return sslErrorMap; }    
    inline bool     getServerLimitEnabled() { return enableServerLimit; }
    inline float    getMaxDownloadLimit() { return maxDownloadLimit; }
    inline quint64  getDownloaded() { return downloaded; }
    inline quint16  getLimitUnits() { return limitUnits; }
    inline quint8   getFrequency() { return frequency; }
    inline quint32  getResetDate() { return resetDate; }

	inline void     setHostName(QString a) { hostName=a; }
	inline void     setName(QString a) { name=a; }
	inline void     setPort(quint16 a) { port=a; }
	inline void     setUserName(QString a) { userName=a; }
	inline void     setPass(QString a) { pass=a; }
	inline void     setTimeout(quint16 a) { timeout=a; }
	inline void     setId(quint16 a) { id=a; }
	inline void     setPriority(quint16 a) { priority=a; }
	inline void     setThreadTimeout(quint16 a) { threadTimeout=a; }
	inline void     setRetries(quint16 a) { retries=a; }
	inline void     setMaxThreads(quint16 a) { maxThreads=a; }
	inline void     setWorkingThreads(quint16 a) { workingThreads=a; }
	inline void     setSslSocket(quint16 a) { sslSocket=a; }
	inline void     setServerType(quint16 a) { serverType=a; }
	inline void     setServerStatus(quint8 a) { serverStatus=a; }
	inline void     setEnabled(bool e) { enabled=e; }
	inline void     setDeleted(bool d) { deleted=d; }
	inline void     setValidated(bool v) { validated=v; }
	inline void     setValidating(bool v) { validating=v; }
    inline void     setPaused(bool p) { paused=p; }
	inline void     setServerFlags(quint64 f) { serverFlags=f; }
	inline void     setServerFlagsDate(quint32 f) { serverFlagsDate=f; }
	inline void     setSpeed(quint64 f) { speed=f; }
	inline void     setItem(QTreeWidgetItem* f) { item=f; }
	inline void     setQueueSize(quint64 f) { size=f; }
	inline void     increaseQueueSize(quint64 f) { size+=f; }
	inline void     decreaseQueueSize(quint64 f) { size-=f; }
	inline void     setEnabledServerExtensions(quint64 f) { enabledServerExtensions = f; }
	inline void     addEnabledServerExtension(quint64 f) { enabledServerExtensions &= f; }
	inline void     removeEnabledServerExtension(quint64 f) { enabledServerExtensions |= ~f; }
	inline void     setSslAllowableErrors(quint64 f) { sslAllowableErrors = f; }
	inline void     addSslAllowableErrors(quint64 f) { sslAllowableErrors &= f; }
	inline void     removeSslAllowableErrors(quint64 f) { sslAllowableErrors |= ~f; }
	inline void     setSslAllErrors(quint64 f) { sslAllErrors = f; }
    inline void     setSslProtocol(qint16 a) { sslProtocol=a; }
	inline void     setSslCertificate(QSslCertificate* a) { sslCertificate=*a; }
    inline void     setNewCertificate(QSslCertificate* a) { newCertificate=*a; }
	inline void     setExpectedSslErrors(QList<QSslError>& a) { expectedSslErrors=a; }    
    inline void     setServerLimitEnabled(bool e) { enableServerLimit=e; }
    inline void     setMaxDownloadLimit(float m) { maxDownloadLimit=m; }
    inline void     setDownloaded(quint64 m) { downloaded=m; }
    inline void     incrementDownloaded(quint64 m) { downloaded+=m; }
    inline void     setLimitUnits(quint16 a) { limitUnits=a; }
    inline void     setFrequency(quint8 a) { frequency=a; }
    inline void     setResetDate(quint32 r) { resetDate=r; }
    inline void     setDb(Db* _serverDb) { serverDb = _serverDb; }

    inline bool     isDownloadLimitExceeded() { return qint64(downloaded - maxDownloadLimitBytes) > 0; }

    void computeMaxDownloadLimitBytes();

private:
	QWidget* parent;

    QString hostName;
	QString name;
	quint16 port;
    QString userName;
    QString pass;
    quint16 timeout;
    quint16 id;
    quint16 priority;
    quint16 threadTimeout;
    quint16 retries;
    quint16 maxThreads;
    quint16 sslSocket;
    quint16 serverType;
    quint8  serverStatus;

    bool    enableServerLimit;
    float   maxDownloadLimit;
    quint64 maxDownloadLimitBytes;
    quint64 downloaded;
    quint16 limitUnits; // Mb, Gb etc
    quint8  frequency; // Daily, Weekly etc
    quint32 resetDate;

    bool paused;
	bool enabled;
	bool deleted;

	volatile bool validating;   // We're checking now

    QTimer* serverTestTimer;
  	QSslSocket* kes;
  	bool gettingStatus;

	QTreeWidgetItem* item;
	quint64 size;

	quint16 workingThreads;
	quint64 speed;

	quint64 serverFlags;
	quint32 serverFlagsDate;
	quint64 enabledServerExtensions;
	quint64 sslAllErrors;
	quint64 sslAllowableErrors;
    qint16  sslProtocol;

	QSslCertificate sslCertificate;
    QSslCertificate newCertificate;

	QList<QSslError> expectedSslErrors;

    Db* serverDb;

public:
    enum serverLimitsUnits {Kb=0, Mb, Gb};
    enum serverLimitsFrequency {Daily=0, Weekly, Monthly, Quarterly, Annually};
	enum serverTypeVal {activeServer=0, passiveServer, dormantServer};
	enum serverStatusVal {serverOk=0, serverConnectionProblem, serverLoginProblem};
    enum extensions { capab = 1, xzver=4, xfeatgzip = 8 };
	enum sslErrors { UnableToGetIssuerCertificate = 1, UnableToDecryptCertificateSignature = 2, UnableToDecodeIssuerPublicKey = 4,
		CertificateSignatureFailed = 8, CertificateNotYetValid = 16, CertificateExpired = 32, InvalidNotBeforeField = 64,
		InvalidNotAfterField = 128, SelfSignedCertificate = 256, SelfSignedCertificateInChain = 512,
		UnableToGetLocalIssuerCertificate = 1024, UnableToVerifyFirstCertificate = 2048, CertificateRevoked = 4096,
		InvalidCaCertificate = 8192, PathLengthExceeded = 16384, InvalidPurpose = 32768, CertificateUntrusted = 65536,
		CertificateRejected = 131072, SubjectIssuerMismatch = 262144, AuthorityIssuerSerialNumberMismatch = 524288,
		NoPeerCertificate = 1048576, HostNameMismatch = 2097152, UnspecifiedError = 4194304,
		NoSslSupport = 8388608, CertificateBlacklisted = 16777216
	};

	static QMap<quint64, QString> extensionDescriptions;
	static QMap<qint16, quint64> sslErrorMap;
	static QMap<qint64, quint16> sslReverseErrorMap;
	static QMap<quint64, QString> sslErrorDescriptions;

    volatile bool validated;    // Have we checked connection and SSL details this session?

	static void initHostMaps();

public slots:
	void slotTestServer();

protected slots:
	void slotTestConnected();
	void slotTestTimeout();
	void slotSslError(const QList<QSslError> &);
	void slotTcpError( QAbstractSocket::SocketError);

signals:
    void sigServerValidated(quint16, bool);
    void sigSaveServer(NntpHost*);
    void sigInvalidServer(quint16);
    void sigPauseThreads(int);
    void sigResumeThreads(int);
};

typedef QMap<quint16, NntpHost*> Servers;

#endif /* NNTPHOST_H_ */
