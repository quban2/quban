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
#include <QStringBuilder>
#include <stdio.h>
#include <string.h>
#include "header.h"
#include "MultiPartHeader.h"

NzbHeader::NzbHeader(Db* _pDB) : MultiPartHeader()
{
	pDB = _pDB;
}

int NzbHeader::saveNzbParts()
{
	int ret;
	char *phead;
	Dbt pkey;
	Dbt pdata;
	Header* h;

	pkey.set_data(&multiPartKey);
	pkey.set_size(sizeof(quint64));

	for (thit = tempHeaders.begin(); thit != tempHeaders.end(); ++thit)
	{
		h = thit.value();

		phead=h->data();

		pdata.set_data(phead);
		pdata.set_size(h->getRecordSize());

		ret = pDB->put(0, &pkey, &pdata, 0);
		if (ret)
		{
			qDebug() << "Unable to create part " << thit.key() << " for key " << multiPartKey << ", result = " << ret;
			delete [] phead;
			break;
		}
		else
			missingParts = missingParts - 1;

		delete [] phead;
	}

	return ret;
}

MultiPartHeader::MultiPartHeader() : HeaderBase()
{
	headerType = 'm';
}

MultiPartHeader::~MultiPartHeader()
{
	serverPart.clear();
	serverLowest.clear();
}

// Marshall the binHeader

char *MultiPartHeader::data()
{
    char *p=new char[getRecordSize()];
    char *i = p;

	quint16 serverNumItems, serverId;
	quint64 artNum;
	quint16 sz16 = sizeof(quint16);
	quint16 sz32 = sizeof(quint32);
	quint16 sz64 = sizeof(quint64);

	*i = headerType;
	i++;

	memcpy(i, &postingDate, sz32);
	i +=sz32;

	memcpy(i, &downloadDate, sz32);
	i +=sz32;

	memcpy(i, &numParts, sz16);
	i+=sz16;

	serverNumItems=serverLowest.count();
	memcpy(i, &serverNumItems, sz16);
	i+=sz16;

	for (lowestit=serverLowest.begin(); lowestit != serverLowest.end(); ++lowestit)
	{
		serverId=lowestit.key();
		artNum=lowestit.value();
		memcpy(i, &serverId, sz16);
		i+=sz16;
		memcpy(i, &artNum, sz64);
		i+=sz64;
	}

	memcpy(i, &multiPartKey, sz64);
	i +=sz64;

    memcpy(i, &missingParts, sz16);
    i+=sz16;

    memcpy(i, &status, sz16);
    i+=sz16;

    memcpy(i, &lines, sz32);
    i+=sz32;

    memcpy(i, &size, sz64);
    i+=sz64;

    // This is the area that maps the number of parts on each server
	quint16 serverPartItems=serverPart.count();

	memcpy(i, &serverPartItems, sz16);
	i+=sz16;
	for (spmit=serverPart.begin() ; spmit != serverPart.end(); ++spmit)
	{
		serverId=spmit.key();
		serverNumItems=spmit.value();
		memcpy(i, &serverId, sz16);
		i+=sz16;
		memcpy(i, &serverNumItems, sz16);
		i+=sz16;
	}

    return p;
}

MultiPartHeader::MultiPartHeader(quint32 keySize, char* k, char *p)
{
	char *pi =p;

    quint16 sz16 = sizeof(quint16);
    quint16 sz32 = sizeof(quint32);
    quint16 sz64 = sizeof(quint64);

    quint16 serverId, serverNumItems;
    quint64 part;

    headerType = *pi;
    pi++;

    memcpy(&postingDate, pi, sz32);
    pi +=sz32;

    memcpy(&downloadDate, pi, sz32);
    pi +=sz32;

    memcpy(&numParts, pi, sz16);
    pi+=sz16;

    quint16 serverLowestItems;
    memcpy(&serverLowestItems, pi, sz16);
    pi += sz16;

    for (quint16 i = 0; i < serverLowestItems; i++)
    {
        memcpy(&serverId, pi, sz16);
        pi += sz16;
        memcpy(&part, pi, sz64);
        pi += sz64;
        serverLowest.insert(serverId, part);
    }

    char *splitpoint;

    if ((splitpoint = (char *)memchr(k, '\n', keySize)))
    {
        subj = QString::fromLatin1(k, (splitpoint - k));
        from = QString::fromLatin1(splitpoint + 1, k + (keySize - 1) - splitpoint);
    }
    else
    {
        subj = QString::fromLatin1(k, keySize);
        from = subj;
        qDebug("Single element header encountered ...");
    }

    memcpy(&multiPartKey, pi, sz64);
    pi +=sz64;

    memcpy(&missingParts, pi, sz16);
    pi +=sz16;

    memcpy(&status, pi, sz16);
    pi +=sz16;

    memcpy(&lines, pi, sz32);
    pi +=sz32;

    memcpy(&size, pi, sz64);
    pi +=sz64;

    quint16 serverPartItems;

    memcpy(&serverPartItems, pi, sz16);
    pi +=sz16;

    for (quint16 i = 0; i < serverPartItems; i++)
    {
        memcpy(&serverId, pi, sz16);
        pi += sz16;
        memcpy(&serverNumItems, pi, sz16);
        pi += sz16;
        serverPart[serverId] = serverNumItems;
    }
}

uint MultiPartHeader::getRecordSize( )
{
    quint32 bhSize=0;
    static quint32 maxBhSize=0;
	quint16 sz16 = sizeof(quint16);
	quint16 sz32 = sizeof(quint32);
	quint16 sz64 = sizeof(quint64);

	bhSize = 7*sz16 + 3*sz32 + 2*sz64 //+ subj.length() + from.length()
			+ serverPart.count()*(sz16+sz16) +
			// partNumSize + ((sz16+sz32)*partSize.count()) // + partMidSize
			+ (sz16+sz64)*serverLowest.count() + sizeof(char);

	if (bhSize > maxBhSize)
	{
		maxBhSize = bhSize;
		qDebug() << "Found bh size of " << bhSize;
		qDebug() << "Subj = " << subj << ", subj len = " << subj.length() << ", from = " << from.length() <<
                ", serverParts = " << serverPart.count() //<<
//				", partSizes = " << partSize.count() // << ", partMidSize = "  << partMidSize
				<< ", serverLowests = " << serverLowest.count();
	}

	return bhSize;
}

bool MultiPartHeader::getMultiPartHeader(unsigned int keySize, char *k, char *p, MultiPartHeader* mph)
{
    char *i=p;
    quint16 sz16 = sizeof(quint16);
    quint16 sz32 = sizeof(quint32);
    quint16 sz64 = sizeof(quint64);
    quint16 count;

    mph->setHeaderType(*i);
    i++;

    memcpy(&(mph->postingDate), i, sz32);
    i+=sz32;

    memcpy(&(mph->downloadDate), i, sz32);
    i+=sz32;

    memcpy(&(mph->numParts), i, sz16);
    i+=sz16;

    memcpy(&count, i, sz16);
    //Skip serverLowest
    i+=(count*(sz16+sz64)+sz16);

    char *splitpoint;

    if ((splitpoint = (char *)memchr(k, '\n', keySize)))
    {
        mph->subj = QString::fromLatin1(k, (splitpoint - k));
        mph->from = QString::fromLatin1(splitpoint + 1, k + (keySize - 1) - splitpoint);
    }
    else
    {
        mph->subj = QString::fromLatin1(k, keySize);
        mph->from = mph->subj;
        qDebug() << "Single element header encountered ... " <<  mph->subj;
    }

    memcpy(&(mph->multiPartKey), i, sz64);
    i +=sz64;

    memcpy(&(mph->missingParts), i, sz16);
    i+=sz16;

    memcpy(&(mph->status), i, sz16);
    i+=sz16;

    //Skip lines
    i+=sz32;

    memcpy(&(mph->size), i, sz64);
    i+=sz64;

    quint16 id, parts;
    memcpy(&count, i, sz16);
    i+=sz16;

    mph->serverPart.clear();

    for (quint16 j=0; j < count; j++)
    {
        memcpy(&id, i, sz16);
        i+=sz16;
        memcpy(&parts, i, sz16);
        i+=sz16;
        mph->serverPart.insert(id, parts);
    }

    return true;
}

bool MultiPartHeader::isCompleted()
{
    return (missingParts == 0);
}

MultiPartHeader::Add_Code MultiPartHeader::addPart(Db* pdb, quint16 part, RawHeader *rh, quint16 hostId )
{
	// Create or update the part
	MultiPartHeader::Add_Code retCode = modifyPart(pdb, part, rh, hostId);

	if (retCode != Error_Part)
	{
	    if (retCode == New_Part)
	    {
	        size+=rh->m_bytes.toULong();
	        lines+=rh->m_lines.toInt();

			if (part==0 || missingParts == 0)
				numParts++;
			else
				missingParts--;

			if (status == bh_read)
			{
				status=bh_new;
				retCode =  Unread_Status;
			}
            //qDebug() << "Added a new part " << part << " for " << subj;
	    }

	    if (retCode == Duplicate_Part)
            ;
            //qDebug() << "Found a duplicate part " << part << " for " << subj;
	    else if (serverPart.contains(hostId))
			(serverPart[hostId])++;
		else
			serverPart[hostId]=1;

		if (serverLowest.contains(hostId))
		{
			if (serverLowest[hostId] > rh->m_num)
				serverLowest[hostId] = rh->m_num;
		}
		else
			serverLowest[hostId] = rh->m_num;
	}

    //qDebug() << "Exiting addPart";

    return retCode;
}

Header* MultiPartHeader::partDbFind(Db* pdb, quint16 partNo)
{
	int ret = 0;
	Header* h = 0;
	Dbc *cursorp;

	Dbt key;
	Dbt data;

	key.set_data(&multiPartKey);
	key.set_size(sizeof(quint64));

	data.set_data(&partNo);
	data.set_size(sizeof(quint16));

	// Get a cursor
	pdb->cursor(NULL, &cursorp, 0);

	// Position the cursor to the first record in the database whose
	// key matches the search key and whose data begins with the search
	// data.
	if  (!(ret = cursorp->get(&key, &data, DB_GET_BOTH_RANGE)))
	{
		h = new Header((char*)data.get_data(), (char*)key.get_data());
	}

	// Close the cursor
	if (cursorp != NULL)
	    cursorp->close();

	return h;
}

// Modify or create a part based on the following information
MultiPartHeader::Add_Code MultiPartHeader::modifyPart(Db* pdb, quint16 partNo, RawHeader *rh, quint16 hostId)
{
	//qDebug() << "Entering modifyPart";
    Add_Code retCode = New_Part;
	int ret = 0;
	bool createPart = false;
	Header* h = 0;
	char *phead;
	Dbc *cursorp;

	Dbt key;
	Dbt data;

	//qDebug() << "Creating part for key " << multiPartKey;

	key.set_data(&multiPartKey);
	key.set_size(sizeof(quint64));

	data.set_data(&partNo);
	data.set_size(sizeof(quint16));

	// Get a cursor
	pdb->cursor(NULL, &cursorp, DB_WRITECURSOR);
	//qDebug("1");

	// Position the cursor to the first record in the database whose
	// key matches the search key and whose data begins with the search
	// data.
	ret = cursorp->get(&key, &data, DB_GET_BOTH);
	if  (ret == 0)
	{
		//qDebug("2");
	    // update the data
		h = new Header((char*)data.get_data(), (char*)key.get_data());
		//qDebug("3");

		if (h->isHeldByServer(hostId))
		{
			retCode = Duplicate_Part;
		}
		else
			retCode = Updated_Part;

        // qDebug() << "Found an update for part " << partNo << ", existing part " << h->getPartNo() << ", existing msgid " << h->getMessageId() << ", new msgid " << rh->m_mid;

		// Always update the article number
		h->setServerPartNum(hostId, rh->m_num);

		//qDebug("4");
		phead=h->data();

		data.set_data(phead);
		data.set_size(h->getRecordSize());

		//qDebug("5");
		cursorp->put(&key, &data, DB_CURRENT);
		//qDebug("6");
		delete [] phead;
	}
	else if (ret == DB_NOTFOUND) // create the part
	{
		//qDebug("7");
		createPart = true;
	}
	else // encountered an error
	{
		qDebug() << "Unable to find part " << partNo << " for key " << multiPartKey << ", result = " << ret;
		retCode = Error_Part;
	}

	// Close the cursor
	if (cursorp != NULL)
	    cursorp->close();

	if (createPart == true)
	{
		//qDebug("8");
		quint32 partSize = rh->m_bytes.toULong();
		h = new Header(multiPartKey, partNo, partSize);

		//qDebug("9");

		h->setMessageId(rh->m_mid);
		h->setServerPartNum(hostId, rh->m_num);

		//save in the parts db
		phead=h->data();

		data.set_data(phead);
		data.set_size(h->getRecordSize());

		//qDebug("10");

		ret = pdb->put(0, &key, &data, 0);
		if (ret)
		{
		    qDebug() << "Unable to create part " << partNo << " for key " << multiPartKey << ", result = " << ret;
		    retCode = Error_Part;
		}
		else
			retCode = New_Part;
		//qDebug("11");
		delete [] phead;
	}

	if (h)
		delete h;

	//qDebug("12\n");

	//qDebug() << "Exiting modifyPart";
	return retCode;
}


QString MultiPartHeader::getIndex()
{
    return subj % '\n' % from;
}

void MultiPartHeader::getAllArticleNums(Db* pDB, PartNumMap* serverArticleNos, QMap<quint16, quint64>* partSize,QMap<quint16, QString>* partMsgId)
{
	quint64 multiPartKey = getMultiPartKey();
	Header* h;

	// Walk the headers to get the PartNumMap
	Dbc *cursorp;

	// Get a cursor
	pDB->cursor(NULL, &cursorp, 0);

	// Set up our DBTs
	Dbt key(&multiPartKey, sizeof(quint64));
	Dbt data;

	int ret = cursorp->get(&key, &data, DB_SET);

	while (ret != DB_NOTFOUND)
	{
		h = new Header((char*)data.get_data(), (char*)key.get_data());
		serverArticleNos->insert(h->getPartNo(), h->getServerArticlemap());
		partSize->insert(h->getPartNo(), h->getSize());
		partMsgId->insert(h->getPartNo(), h->getMessageId());

		delete h;
		ret = cursorp->get(&key, &data, DB_NEXT_DUP);
	}

	if (cursorp != NULL)
	    cursorp->close();
}

void MultiPartHeader::deleteAllParts(Db* pDB)
{
	int ret = 0;
	Dbc *cursorp;

	Dbt key;
	Dbt data;

    //qDebug() << "Deleting all parts for key " << multiPartKey;

	key.set_data(&multiPartKey);
	key.set_size(sizeof(quint64));

	// Get a cursor
	pDB->cursor(NULL, &cursorp, DB_WRITECURSOR);

	while ((ret = cursorp->get(&key, &data, DB_SET)) == 0)
	    cursorp->del(0);

	pDB->sync(0);

	if (cursorp != NULL)
	    cursorp->close();
}

void MultiPartHeader::removeServer(Db* pDB, quint16 serverId)
{
	serverPart.remove(serverId);
	serverLowest.remove(serverId);

	quint64 multiPartKey = getMultiPartKey();
	Header* h;

	// Walk the headers to get the PartNumMap
	Dbc *cursorp;

	// Get a cursor
	pDB->cursor(NULL, &cursorp, DB_WRITECURSOR);

	// Set up our DBTs
	Dbt key(&multiPartKey, sizeof(quint64));
	Dbt data;

	int ret = cursorp->get(&key, &data, DB_SET);

	while (ret != DB_NOTFOUND)
	{
		h = new Header((char*)data.get_data(), (char*)key.get_data());

		if (h->isHeldByServer(serverId))
		{
			h->removeServerPartNum(serverId);

			if (h->isServerNumMapEmpty())
			{
				cursorp->del(0);

				size-=h->getSize();
				missingParts++;
			}
		}

		delete h;
		ret = cursorp->get(&key, &data, DB_NEXT_DUP);
	}

	if (cursorp != NULL)
	    cursorp->close();
}
