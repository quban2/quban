#ifndef NEWSGROUP_H_
#define NEWSGROUP_H_

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

#include <QMap>
#include <QDate>
#include <QMutex>
#include "headerQueue.h"
#include "headerlist.h"
#include "hlistviewitem.h"
#include "common.h"

/**
@original author Alessandro Bonometti
*/

class HeaderList;
class NGListViewItem;

class NewsGroup : public QObject
{
	//if low[server]=-1, the group doesn't exist on the server!
	friend class QMgr;
	friend class NntpThread;
    friend class HeaderReadWorker;
    friend class HeaderReadXFeatGzip;
    friend class HeaderReadNoCompression;
    friend class HeaderReadXzVer;
	friend class UpdateDbThread;
    friend class ExpireDb;
	friend class GroupList;
	friend class NewsGroupDialog;
	friend class NewsGroupContents;
	friend class NGListViewItem;

	friend class Quban;
    friend class AddForm;

	bool  autoSetSaveDir;

	QString lastExpiry;

	QMap<quint16, quint64> low;               //holds serverId - LowWatermark
	QMap<quint16, quint64> high;              //holds serverId - HighWatermark
	QMap<quint16, quint64> serverParts;       //holds serverId - Parts
	QMap<quint16, quint64> servLocalLow;      //holds serverId - LowWatermark
	QMap<quint16, quint64> servLocalHigh;    //holds serverId - HighWatermark
	QMap<quint16, quint64> servLocalParts;   //holds serverId - Parts

	QMap<quint16, QString> serverRefresh; //holds serverId - Refresh Date
	QMap<quint16, bool> serverPresence;   // does the server have the group? False unless we know otherwise


	QMap<quint16, quint16>::iterator it;
	QMap<quint16, quint64>::iterator uit;
	QMap<quint16, bool>::iterator bit;
	QMap<quint16, QString>::iterator dit;

    QList<quint16> downloadingServers;

	NGListViewItem *listItem;
	quint64 totalArticles;
	quint64 unreadArticles;

    quint64 totalGroups;
    QString RE1;
    QString RE2;
    QString RE3;

    bool RE1AtBack;
    bool RE2AtBack;
    bool RE3AtBack;

    qint16 matchDistance;

	bool showOnlyUnread;
	bool showOnlyComplete;
	quint32 deleteOlderPosting;
	quint32 deleteOlderDownload;

    quint64 groupingBulkSeq;

	//remember column order
	QMap<quint16, quint16> colOrder;

	//remember sizes of columns
	QMap<quint16, quint16> colSize;


	//Remember sort column and sort order
	quint16 sortColumn;
	bool ascending;

	HeaderList *view; //tab where this group is displayed, if any

	QString ngName;
	QString alias;
	QString category;
    QString saveDir;  //Savedir...
    QString dbName;   //sometimes=ngName, sometimes not...(at least while debugging :)
    quint64 multiPartSequence;
    quint64 newsGroupFlags;
	Db* db;
	Db* partsDb;
    Db* groupingDb;
    DbEnv *dbEnv;

public:

    int getDownloadingServersCount() { return downloadingServers.count(); }

    enum { IGNORE_UNSEQUENCED_ARTICLES = 1, NAME_DB_AS_ALIAS = 2, RENAME_DB_AT_STARTUP = 4,  DB_CURRENTLY_NAMED_AS_ALIAS = 8, ARTICLES_NEED_DELETING = 16,
           HEADERS_GROUPED = 32, HEADERS_NEEDS_GROUPING = 64, NO_REGEX_ON_GROUPS = 128 };

	void ignoreUnsequencedArticles(bool b) { (b == true) ? newsGroupFlags |= IGNORE_UNSEQUENCED_ARTICLES : newsGroupFlags &= ~IGNORE_UNSEQUENCED_ARTICLES; }
	void nameDbAsAlias(bool b) { (b == true) ? newsGroupFlags |= NAME_DB_AS_ALIAS : newsGroupFlags &= ~NAME_DB_AS_ALIAS; }
	void renameDbAtStartup(bool b) { (b == true) ? newsGroupFlags |= RENAME_DB_AT_STARTUP : newsGroupFlags &= ~RENAME_DB_AT_STARTUP; }
	void dbCurrentlyNamedAsAlias(bool b) { (b == true) ? newsGroupFlags |= DB_CURRENTLY_NAMED_AS_ALIAS : newsGroupFlags &= ~DB_CURRENTLY_NAMED_AS_ALIAS; }
	void articlesNeedDeleting(bool b) { (b == true) ? newsGroupFlags |= ARTICLES_NEED_DELETING : newsGroupFlags &= ~ARTICLES_NEED_DELETING; }
    void setHeadersGrouped(bool b) { (b == true) ? newsGroupFlags |= HEADERS_GROUPED : newsGroupFlags &= ~HEADERS_GROUPED; }
    void setHeadersNeedGrouping(bool b) { (b == true) ? newsGroupFlags |= HEADERS_NEEDS_GROUPING : newsGroupFlags &= ~HEADERS_NEEDS_GROUPING; }
    void setNoRegexGrouping(bool b) { (b == true) ? newsGroupFlags |= NO_REGEX_ON_GROUPS : newsGroupFlags &= ~NO_REGEX_ON_GROUPS; }

	bool areUnsequencedArticlesIgnored() { return newsGroupFlags & IGNORE_UNSEQUENCED_ARTICLES; }
	bool shouldDbBeNamedAsAlias() { return newsGroupFlags & NAME_DB_AS_ALIAS; }
	bool shouldDbBeRenamedAtStartup() { return newsGroupFlags & RENAME_DB_AT_STARTUP; }
	bool isDbNamedAsAlias() { return newsGroupFlags & DB_CURRENTLY_NAMED_AS_ALIAS; }
	bool doArticlesNeedDeleting() { return newsGroupFlags & ARTICLES_NEED_DELETING; }
    bool areHeadersGrouped() { return newsGroupFlags & HEADERS_GROUPED; }
    bool doHeadersNeedGrouping() { return newsGroupFlags & HEADERS_NEEDS_GROUPING; }
    bool isThereNoRegexOnGrouping() { return newsGroupFlags & NO_REGEX_ON_GROUPS; }

	NewsGroup(DbEnv *_dbEnv, QString _ngName, QString _saveDir, QString _alias, QString _useAlias); // Opens? yes!
    NewsGroup(DbEnv *, char *, uint version=GROUPDB_VERSION); //un-marshal and open

	~NewsGroup(); //closes the db
	int open(); //Maybe not needed...maybe needed :)
	int close();
	Db* getDb() {return db;}
	void setDb(Db* d) {db=d;}
	Db* getPartsDb() {return partsDb;}
	void setPartsDb(Db* d) {partsDb=d;}
    Db* getGroupingDb() {return groupingDb;}
    void setGroupingDb(Db* d) {groupingDb=d;}

	void setMultiPartSequence(quint64 mps) { multiPartSequence = mps; }
	quint64 getMultiPartSequence() { return multiPartSequence; }
    char *data(char* buffer=0);
	uint getRecordSize();
    bool isOpened() {return (view!=0);}
    QString getName() {return ngName;}
    quint64 getTotal() {return totalArticles;}
    void setListItem(NGListViewItem *i) { listItem=i;}
    void setView(HeaderList *v) {view=v;}
    QString getSaveDir() {return saveDir;}
    HeaderList *getView() {return view;}
    QString name() {return ngName;}
    quint64 unread() {return unreadArticles;}
    void setUnread(quint64 u) {unreadArticles=u; }
    void incUnread() {unreadArticles++;}
	void decUnread();
	void setAlias(QString s);
	void setCategory(QString c);
    QString& getAlias() {return alias;}
    QString& getCategory() {return category;}
	uchar *getBinHeader(QString index);
	void setTotal(quint64 t) {totalArticles = t;}
	void decTotal();
    void setGroupingBulkSeq(quint64 s) {groupingBulkSeq = s;}
    quint64 getGroupingBulkSeq() {return groupingBulkSeq;}

	void reduceParts(quint16 serv, quint64 p) {servLocalParts[serv] = qMax(servLocalParts[serv] - p, (quint64)0);}

	void resetSettings();
	bool onlyComplete() {return showOnlyComplete;}
	bool onlyUnread() {return showOnlyUnread;}
	void setDeleteOlderPosting(quint32 i) { deleteOlderPosting = i; }
	quint32 getDeleteOlderPosting() { return deleteOlderPosting; }
	void setDeleteOlderDownload(quint32 i) { deleteOlderDownload = i; }
	quint32 getDeleteOlderDownload() { return deleteOlderDownload; }
	QString getLastExpiry() { return lastExpiry; }
	void setLastExpiry(QString e) { lastExpiry = e; }

	void setColIndex(quint16 col, quint16 index);
	qint16 getColIndex(quint16 col);
	void clearColOrder() { colOrder.clear();}

	quint16 getSortColumn() { return sortColumn;}
	bool getSortOrder() { return ascending;}
	void setSortColumn(quint16 col) { sortColumn = col;}
	void setSortOrder(bool a)  { ascending = a;}


	void setColSize(quint16 col, quint16 size);
	qint16 getColSize(quint16 col);
	void clearColSize() { colSize.clear();}

    void    groupHeaders();
    void    setTotalGroups(quint64 t) { totalGroups = t; }
    void    incTotalGroups() { ++totalGroups; }
    void    decTotalGroups() { --totalGroups; }
    quint64 getTotalGroups() { return totalGroups; }

    void    setMatchDistance(qint16 d) { matchDistance = d; }
    void    setGroupRE1(QString s) { RE1 = s; }
    void    setGroupRE2(QString s) { RE2 = s; }
    void    setGroupRE3(QString s) { RE3 = s; }
    void    setGroupRE1Back(bool b) { RE1AtBack = b; }
    void    setGroupRE2Back(bool b) { RE2AtBack = b; }
    void    setGroupRE3Back(bool b) { RE3AtBack = b; }

    qint16  getMatchDistance() { return matchDistance; }
    QString getGroupRE1()   { return RE1; }
    QString getGroupRE2()   { return RE2; }
    QString getGroupRE3()   { return RE3; }
    bool    getGroupRE1Back()   { return RE1AtBack; }
    bool    getGroupRE2Back()   { return RE2AtBack; }
    bool    getGroupRE3Back()   { return RE3AtBack; }

};

#endif /* NEWSGROUP_H_ */
