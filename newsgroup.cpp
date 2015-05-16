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
#include "newsgroup.h"
#include <QDir>
#include <QDebug>
#include <QtDebug>
#include <QMessageBox>
#include "MultiPartHeader.h"
#include "qubanDbEnv.h"
#include "quban.h"
#include "newsgroup.h"

extern Quban* quban;

int compare_q16(Db *, const Dbt *, const Dbt *);

NewsGroup::NewsGroup(DbEnv * _dbEnv, QString _ngName, QString _saveDir, QString _alias, QString _useAlias)
{
	ngName = _ngName;
	dbName = ngName;
	totalArticles = 0;
	unreadArticles = 0;
	showOnlyComplete = false;
	showOnlyUnread = false;
	sortColumn = -1;
	ascending = true;
	view = 0;
	deleteOlderPosting = 0;
	listItem = 0;
	autoSetSaveDir = true;
	partsDb = 0;
    groupingDb = 0;
    groupingBulkSeq = 0;

	saveDir = _saveDir + '/';
	dbEnv = _dbEnv; //Probably not needed...

	int ret = 0;

	QByteArray ba;
	if (_useAlias == "N")
		ba = dbName.toLocal8Bit();
	else
		ba = _alias.toLocal8Bit();

	const char *c_str = ba.constData();

	if (!(db = quban->getDbEnv()->openDb(c_str, true, &ret)))
		QMessageBox::warning(0, tr("Error"), tr("Unable to open newsgroup database ..."));

	partsDb = new Db(dbEnv, 0);
	if (!partsDb)
		qDebug("Error creating parts database: %s", dbEnv->strerror(ret));

	partsDb->set_flags(DB_DUP|DB_DUPSORT);
	partsDb->set_dup_compare(compare_q16);

	QByteArray ba2;
	if (_useAlias == "N")
		ba2 = dbName.toLocal8Bit() + "__parts";
	else
		// MD TODO need to make sure that the alias is unique
		ba2 = _alias.toLocal8Bit() + "__parts";

	const char *c_str2 = ba2.constData();
	if ((ret = partsDb->open(NULL, c_str2, NULL, DB_BTREE, DB_CREATE | DB_THREAD, 0644)) != 0)
		qDebug("Error opening parts database: %s", dbEnv->strerror(ret));

//    RE1   = "^(.*)[0-9 ]*-.+$";
//    RE2 = "[-0-9 ]*$";
//    RE3 = "[\\[\\(][0-9 ]+[/of\\\\]{1,2}[0-9 ]+[\\]\\)]";

}

int compare_q16(Db *, const Dbt *a, const Dbt *b)
{
	quint16 ai,
            bi;

	// Returns:
	// < 0 if a < b
	// = 0 if a = b
	// > 0 if a > b
	memcpy(&ai, a->get_data(), sizeof(quint16));
	memcpy(&bi, b->get_data(), sizeof(quint16));
	return (int)(ai - bi);
}

NewsGroup::~NewsGroup()
{
    if (listItem)
        delete listItem;

	if (partsDb)
	{
		partsDb->close(0);
        delete partsDb;
		partsDb = 0;
	}

    if (groupingDb)
    {
        groupingDb->close(0);
        delete groupingDb;
        groupingDb = 0;
    }

    if (db)
    {
        db->close(0);
        delete db;
        db = 0;
    }
    // 	qDebug("Database %s closed", (const char *) dbName);
}

char* NewsGroup::data(char *buffer)
{
    char *p;

    if (!buffer)
        p = new char[getRecordSize()];
    else
        p = buffer;

	//qDebug("Start pointer %d", p);
	char *i = p;

	i = insert(ngName, i);
	i = insert(alias, i);
	i = insert(category, i);
	i = insert(dbName, i);
	i = insert(saveDir, i);

	quint16 sz16 = sizeof(quint16);
	quint16 sz32 = sizeof(quint32);
	quint16 sz64 = sizeof(quint64);

	memcpy(i, &totalArticles, sz64);
	i += sz64;
	// 	qDebug("data() - Unread articles: %d", unreadArticles);
	memcpy(i, &unreadArticles, sz64);
	i += sz64;

	memcpy(i, &multiPartSequence, sz64);
	i += sz64;

	qDebug() << "Group " << this->getAlias() << " has multiPartSequence " << multiPartSequence;

	memcpy(i, &newsGroupFlags, sz64);
	i += sz64;

	memcpy(i, &showOnlyUnread, sizeof(bool));
	i += sizeof(bool);
	memcpy(i, &showOnlyComplete, sizeof(bool));
	i += sizeof(bool);
	memcpy(i, &deleteOlderPosting, sz32);
	i += sz32;
	memcpy(i, &deleteOlderDownload, sz32);
	i += sz32;

	quint16 count = high.count();
	memcpy(i, &count, sz16);
	i += sz16;
	quint16 id;
	int w;
	qint64 w2;

	for (uit = high.begin(); uit != high.end(); ++uit)
	{
		id = uit.key();
		w2 = uit.value();
		memcpy(i, &id, sz16);
		i += sz16;
		memcpy(i, &w2, sz64);
		i += sz64;
	}
	count = low.count();
	memcpy(i, &count, sz16);
	i += sz16;
	for (uit = low.begin(); uit != low.end(); ++uit)
	{
		id = uit.key();
		w2 = uit.value();
		memcpy(i, &id, sz16);
		i += sz16;
		memcpy(i, &w2, sz64);
		i += sz64;
	}

	//New in version 5: remember sort column, sort order, column widths, column reordering
	count = colOrder.count();
	memcpy(i, &count, sz16);
	i += sz16;
	for (it = colOrder.begin(); it != colOrder.end(); ++it)
	{
		id = it.key();
		w = it.value();
		memcpy(i, &id, sz16);
		i += sz16;
		memcpy(i, &w, sz16);
		i += sz16;
		// 		qDebug() << "Column: " << id << " index: " << w << endl;
	}

	count = colSize.count();
	memcpy(i, &count, sz16);
	i += sz16;

	for (it = colSize.begin(); it != colSize.end(); ++it)
	{
		id = it.key();
		w = it.value();
		memcpy(i, &id, sz16);
		i += sz16;
		memcpy(i, &w, sz16);
		i += sz16;
	}

	memcpy(i, &sortColumn, sz16);
	i += sz16;

	memcpy(i, &ascending, sizeof(bool));
	i += sizeof(bool);

	// *****************
	// New in version 6
	// *****************

	count = serverParts.count();
	memcpy(i, &count, sz16);
	i += sz16;
	for (uit = serverParts.begin(); uit != serverParts.end(); ++uit)
	{
		id = uit.key();
		w2 = uit.value();
		memcpy(i, &id, sz16);
		i += sz16;
		memcpy(i, &w2, sz64);
		i += sz64;
	}

	QString dateToSave;

	count = serverRefresh.count();
	memcpy(i, &count, sz16);
	i += sz16;
	for (dit = serverRefresh.begin(); dit != serverRefresh.end(); ++dit)
	{
		id = dit.key();
		dateToSave = dit.value();
		memcpy(i, &id, sz16);
		i += sz16;
		i = insert(dateToSave, i);
	}

	bool sp;
	count = serverPresence.count();
	memcpy(i, &count, sz16);
	i += sz16;
	for (bit = serverPresence.begin(); bit != serverPresence.end(); ++bit)
	{
		id = bit.key();
		sp = bit.value();
		memcpy(i, &id, sz16);
		i += sz16;
		memcpy(i, &sp, sizeof(bool));
		i += sizeof(bool);
	}

	memcpy(i, &autoSetSaveDir, sizeof(bool));
	i += sizeof(bool);

	count = servLocalLow.count();
	memcpy(i, &count, sz16);
	i += sz16;
	for (uit = servLocalLow.begin(); uit != servLocalLow.end(); ++uit)
	{
		id = uit.key();
		w2 = uit.value();
		memcpy(i, &id, sz16);
		i += sz16;
		memcpy(i, &w2, sz64);
		i += sz64;
	}

	count = servLocalHigh.count();
	memcpy(i, &count, sz16);
	i += sz16;
	for (uit = servLocalHigh.begin(); uit != servLocalHigh.end(); ++uit)
	{
		id = uit.key();
		w2 = uit.value();
		memcpy(i, &id, sz16);
		i += sz16;
		memcpy(i, &w2, sz64);
		i += sz64;
	}

	count = servLocalParts.count();
	memcpy(i, &count, sz16);
	i += sz16;
	for (uit = servLocalParts.begin(); uit != servLocalParts.end(); ++uit)
	{
		id = uit.key();
		w2 = uit.value();
		memcpy(i, &id, sz16);
		i += sz16;
		memcpy(i, &w2, sz64);
		i += sz64;
	}

	i = insert(lastExpiry, i);   

    memcpy(i, &totalGroups, sz64);
    i += sz64;

    i = insert(RE1, i);
    i = insert(RE2, i);
    i = insert(RE3, i);

    memcpy(i, &RE1AtBack, sizeof(bool));
    i += sizeof(bool);

    memcpy(i, &RE2AtBack, sizeof(bool));
    i += sizeof(bool);

    memcpy(i, &RE3AtBack, sizeof(bool));
    i += sizeof(bool);

    memcpy(i, &matchDistance, sz16);
    i += sz16;

	return p;
}

uint NewsGroup::getRecordSize()
{
	uint recSize =
           ((10 * sizeof(quint16)) +
            (2 * sizeof(quint32)) +
            (2 * sizeof(quint64)) +
			ngName.length() +
			alias.length() +
			category.length() +
			dbName.length() +
			saveDir.length() +
			(high.count() * sizeof(quint16)) +
			(high.count() * sizeof(quint64)) +
			(low.count() * sizeof(quint16)) +
			(low.count() * sizeof(quint64)) +
			(2 * sizeof(bool)) +
			(2 * colOrder.count() * sizeof(quint16)) +
			(2 * colSize.count() * sizeof(quint16)) +
			sizeof(bool) +
			// *****************
			// New in version 6
			// *****************
			(3 * sizeof(quint16)) + // the 3 new map counts
			(serverParts.count() * sizeof(quint16)) +
			(serverParts.count() * sizeof(quint64)) +
			(serverRefresh.count() * sizeof(quint16)) +
			(serverRefresh.count() * sizeof(quint16)) + // strlen() in insert
			(serverRefresh.count() * 11) +  // dd.MMM.yyyy
			(serverPresence.count() * sizeof(quint16)) +
			(serverPresence.count() * sizeof(bool)) +
			sizeof(bool) +
			sizeof(quint16) + // strlen() in insert
			lastExpiry.length()  +// dd.MMM.yyyy
			// *****************
			// New in version 7
			// *****************
			(3 * sizeof(quint16)) + // the map counts
			(servLocalLow.count() * sizeof(quint16))  +
			(servLocalLow.count() * sizeof(quint64))  +
			(servLocalHigh.count() * sizeof(quint16))  +
			(servLocalHigh.count() * sizeof(quint64))  +
			(servLocalParts.count() * sizeof(quint16))  +
			(servLocalParts.count() * sizeof(quint64)) +
			// *****************
			// New in version 8
			// *****************
            (2 * sizeof(quint64)) +
            // *****************
            // New in version 9
            // *****************
            sizeof(quint64) +
            (3 * sizeof(quint16)) + // the 3 string lengths
            RE1.length()  +
            RE2.length()  +
            RE3.length()  +
            (3 * sizeof(bool)) +
            sizeof(qint16)
			);

	return recSize;
}

NewsGroup::NewsGroup(DbEnv *_dbEnv, char *data, uint version)
{
    db = 0;
	partsDb = 0;
    groupingDb = 0;
    groupingBulkSeq = 0;

    //RE1   = "^(.*)[0-9 ]*-.+$";
    //RE2 = "[-0-9 ]*$";
    //RE3 = "[\\[\\(][0-9 ]+[/of\\\\]{1,2}[0-9 ]+[\\]\\)]";

    quint16 sz16 = sizeof(quint16);
    quint16 sz32 = sizeof(quint32);
    quint16 sz64 = sizeof(quint64);

    char *p = data;
    p = retrieve(p, ngName);
    p = retrieve(p, alias);
    p = retrieve(p, category);

    p = retrieve(p, dbName);
    p = retrieve(p, saveDir);
    saveDir = QDir::cleanPath(saveDir);
    saveDir += '/';

    memcpy(&totalArticles, p, sz64);
    p += sz64;

    qDebug() << "Group: " << ngName << ", total articles: " << totalArticles;

    memcpy(&unreadArticles, p, sz64);
    p += sz64;

    memcpy(&multiPartSequence, p, sz64);
    p += sz64;

    memcpy(&newsGroupFlags, p, sz64);
    p += sz64;

    memcpy(&showOnlyUnread, p, sizeof(bool));
    p += sizeof(bool);
    memcpy(&showOnlyComplete, p, sizeof(bool));
    p += sizeof(bool);

    memcpy(&deleteOlderPosting, p, sz32);
    p += sz32;
    memcpy(&deleteOlderDownload, p, sz32);
    p += sz32;

    quint16 count, id, w;
    quint64 w2;

    memcpy(&count, p, sz16);
    p += sz16;

    for (quint16 i = 0; i < count; i++)
    {
        memcpy(&id, p, sz16);
        p += sz16;
        memcpy(&w2, p, sz64);
        p += sz64;
        high.insert(id, w2);
    }

    memcpy(&count, p, sz16);
    p += sz16;
    for (quint16 i = 0; i < count; i++)
    {
        memcpy(&id, p, sz16);
        p += sz16;
        memcpy(&w2, p, sz64);
        p += sz64;
        low.insert(id, w2);
    }

    memcpy(&count, p, sz16);
    p += sz16;
    qDebug() << "Read " << count << " col orders";
    for (quint16 i = 0; i < count; i++)
    {
        memcpy(&id, p, sz16);
        p += sz16;
        memcpy(&w, p, sz16);
        p += sz16;
        colOrder.insert(id, w);
    }

    memcpy(&count, p, sz16);
    p += sz16;
    qDebug() << "Read " << count << " col sizes";
    for (qint16 i = 0; i < count; i++)
    {
        memcpy(&id, p, sz16);
        p += sz16;
        memcpy(&w, p, sz16);
        p += sz16;
        colSize.insert(id, w);
    }

    memcpy(&sortColumn, p, sz16);
    p += sz16;

    if (sortColumn > 10)
        sortColumn = 0;

    memcpy(&ascending, p, sizeof(bool));
    p += sizeof(bool);

    memcpy(&count, p, sz16);
    p += sz16;
    for (quint16 i = 0; i < count; i++)
    {
        memcpy(&id, p, sz16);
        p += sz16;
        memcpy(&w2, p, sz64);
        p += sz64;
        serverParts.insert(id, w2);
    }

    QString dateAsString;

    memcpy(&count, p, sz16);
    p += sz16;
    for (quint16 i = 0; i < count; i++)
    {
        memcpy(&id, p, sz16);
        p += sz16;
        p = retrieve(p, dateAsString);

        serverRefresh.insert(id, dateAsString);
    }

    bool sp;
    memcpy(&count, p, sz16);
    p += sz16;
    for (quint16 i = 0; i < count; i++)
    {
        memcpy(&id, p, sz16);
        p += sz16;
        memcpy(&sp, p, sizeof(bool));
        p += sizeof(bool);

        serverPresence.insert(id, sp);
    }

    memcpy(&autoSetSaveDir, p, sizeof(bool));
    p += sizeof(bool);

    memcpy(&count, p, sz16);
    p += sz16;

    for (quint16 i = 0; i < count; i++)
    {
        memcpy(&id, p, sz16);
        p += sz16;
        memcpy(&w2, p, sz64);
        p += sz64;
        servLocalLow.insert(id, w2);
    }

    memcpy(&count, p, sz16);
    p += sz16;
    for (quint16 i = 0; i < count; i++)
    {
        memcpy(&id, p, sz16);
        p += sz16;
        memcpy(&w2, p, sz64);
        p += sz64;
        servLocalHigh.insert(id, w2);
    }

    memcpy(&count, p, sz16);
    p += sz16;
    for (quint16 i = 0; i < count; i++)
    {
        memcpy(&id, p, sz16);
        p += sz16;
        memcpy(&w2, p, sz64);
        p += sz64;
        servLocalParts.insert(id, w2);
    }

    p = retrieve(p, lastExpiry);

    if (version > 8)
    {
        memcpy(&totalGroups, p, sz64);
        p += sz64;

        p = retrieve(p, RE1);
        p = retrieve(p, RE2);
        p = retrieve(p, RE3);

        memcpy(&RE1AtBack, p, sizeof(bool));
        p += sizeof(bool);

        memcpy(&RE2AtBack, p, sizeof(bool));
        p += sizeof(bool);

        memcpy(&RE3AtBack, p, sizeof(bool));
        p += sizeof(bool);

        memcpy(&matchDistance, p, sz16);
        p += sz16;

        // 	qDebug("p-data: %d", p-data);
        dbEnv = _dbEnv;
        int ret = 0;

        if (shouldDbBeRenamedAtStartup())
        {
            QByteArray bad = dbName.toLocal8Bit();
            QByteArray baa = alias.toLocal8Bit();

            const char *bad_str = bad.data();
            const char *baa_str = baa.data();

            if (shouldDbBeNamedAsAlias())
                // MD TODO need to make sure that the alias is unique
                dbEnv->dbrename(NULL, bad_str, NULL, baa_str, 0);
            else
                dbEnv->dbrename(NULL, baa_str, NULL, bad_str, 0);

            bad += "__parts";
            baa += "__parts";

            bad_str = bad.data();
            baa_str = baa.data();

            if (shouldDbBeNamedAsAlias())
                // MD TODO need to make sure that the alias is unique
                dbEnv->dbrename(NULL, bad_str, NULL, baa_str, 0);
            else
                dbEnv->dbrename(NULL, baa_str, NULL, bad_str, 0);

            if (this->areHeadersGrouped())
            {
                QByteArray bad = dbName.toLocal8Bit();
                QByteArray baa = alias.toLocal8Bit();

                bad += "__grouping";
                baa += "__grouping";

                bad_str = bad.data();
                baa_str = baa.data();

                if (shouldDbBeNamedAsAlias())
                    // MD TODO need to make sure that the alias is unique
                    dbEnv->dbrename(NULL, bad_str, NULL, baa_str, 0);
                else
                    dbEnv->dbrename(NULL, baa_str, NULL, bad_str, 0);
            }

            dbCurrentlyNamedAsAlias(shouldDbBeNamedAsAlias());
            renameDbAtStartup(false);
        }

        QByteArray ba;

        if (shouldDbBeNamedAsAlias())
            ba = alias.toLocal8Bit();
        else
            ba = dbName.toLocal8Bit();

        const char *c_str = ba.data();

        if (!(db = quban->getDbEnv()->openDb(c_str, true, &ret)))
            QMessageBox::warning(0, tr("Error"), tr("Unable to open newsgroup database ..."));

        partsDb = new Db(dbEnv, 0);
        if (!partsDb)
            qDebug("Error opening parts database: %s", dbEnv->strerror(ret));

        partsDb->set_flags(DB_DUP|DB_DUPSORT);
        partsDb->set_dup_compare(compare_q16);

        ba += "__parts";
        c_str = ba.data();
        if ((ret = partsDb->open(NULL, c_str, NULL, DB_BTREE, DB_CREATE | DB_THREAD, 0644)) != 0)
            qDebug("Error opening parts database: %s", dbEnv->strerror(ret));

        if (areHeadersGrouped())
        {
            if (shouldDbBeNamedAsAlias())
                ba = alias.toLocal8Bit();
            else
                ba = dbName.toLocal8Bit();

            groupingDb = new Db(dbEnv, 0);
            if (!groupingDb)
                qDebug("Error opening grouping database: %s", dbEnv->strerror(ret));

            ba += "__grouping";
            c_str = ba.data();

            qDebug() << "Grouped Db = " << c_str;

            if ((ret = groupingDb->open(NULL, c_str, NULL, DB_BTREE, DB_CREATE | DB_THREAD, 0644)) != 0)
                qDebug("Error opening grouping database: %s", dbEnv->strerror(ret));
        }
    }

	view = 0;
	listItem = 0;
}

void NewsGroup::decUnread()
{
	if (unreadArticles > 0)
		unreadArticles--;
	else
		unreadArticles = 0;
}

void NewsGroup::setAlias(QString s)
{
	alias = s;
}

void NewsGroup::setCategory(QString c)
{
	category = c;
}

uchar * NewsGroup::getBinHeader(QString index)
{
	Dbt key, data;
	data.set_flags(DB_DBT_MALLOC);
	key.set_flags(DB_DBT_USERMEM);

	QByteArray ba = index.toLocal8Bit();
	const char *c_str = ba.data();
	key.set_data((void*) c_str);
	key.set_size(index.length());
	int ret;
	if ((ret = db->get(NULL, &key, &data, 0)) != 0)
		qDebug("Error getting binheader: %d", ret);

	return (uchar*) data.get_data();

}

int NewsGroup::close()
{
	int ret = db->close(0);
	delete db;
	return ret;

}

int NewsGroup::open()
{
int ret;

	QByteArray ba = dbName.toLocal8Bit();
	const char *c_str = ba.data();

	if (!(db = quban->getDbEnv()->openDb(c_str, false, &ret)))
		QMessageBox::warning(0, tr("Error"), tr("Unable to open newsgroup database ..."));

	return ret;
}

void NewsGroup::resetSettings()
{
	showOnlyComplete = false;
	showOnlyUnread = false;
}

void NewsGroup::decTotal()
{
	if (totalArticles > 0)
		totalArticles--;
	else
		totalArticles = 0;
}

void NewsGroup::setColIndex(quint16 col, quint16 index)
{
	colOrder[col] = index;
}

qint16 NewsGroup::getColIndex(quint16 col)
{
	if (colOrder.contains(col))
		return colOrder[col];
	else
		return -1;
}

void NewsGroup::setColSize(quint16 col, quint16 size)
{
	colSize[col] = size;
}

qint16 NewsGroup::getColSize(quint16 col)
{
	if (colSize.contains(col))
		return colSize[col];
	else
		return -1;
}

void NewsGroup::groupHeaders()
{    
    setHeadersNeedGrouping(true);

    if (!groupingDb)
    {
        int ret = 0;
        QByteArray ba;

        if (shouldDbBeNamedAsAlias())
            ba = alias.toLocal8Bit();
        else
            ba = dbName.toLocal8Bit();

        groupingDb = new Db(dbEnv, 0);
        if (!groupingDb)
            qDebug("Error creating grouping database: %s", dbEnv->strerror(ret));

        ba += "__grouping";
        const char *c_str = ba.data();
        if ((ret = groupingDb->open(NULL, c_str, NULL, DB_BTREE, DB_CREATE | DB_THREAD, 0644)) != 0)
            qDebug("Error opening grouping database: %s", dbEnv->strerror(ret));
    }
}
