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
#include "header.h"

Header::Header(quint64 _key, quint16 _partNo)
{
	multiPartKey = _key;
	partNo       = _partNo;
}

Header::Header(quint64 _key, quint16 _partNo, quint64 _size)
{
	multiPartKey = _key;
	partNo       = _partNo;
	size         = _size;
}

Header::Header(char* p, char* key, quint16)
{
	quint16 sz16 = sizeof(quint16);
	quint16 sz64 = sizeof(quint64);

	memcpy(&multiPartKey, key, sz64);

	char *pi =p;

	quint16 serverCount, serverId;
	quint64 articleNum;

	memcpy(&partNo, pi, sz16);
	pi +=sz16;

	memcpy(&size, pi, sz64);
	pi +=sz64;

	pi = retrieve(pi, messageId);

	memcpy(&serverCount, pi, sz16);
	pi += sz16;

	for (quint16 i = 0; i < serverCount; i++)
	{
		memcpy(&serverId, pi, sz16);
		pi += sz16;
		memcpy(&articleNum, pi, sz64);
		pi += sz64;
		serverNumMap.insert(serverId, articleNum);
	}
}

quint32 Header::getRecordSize()
{
	quint16 sz16 = sizeof(quint16);
	quint16 sz64 = sizeof(quint64);

    quint32 partSize= (sz16 * 2) + // 1 String + 1 array
    		messageId.length() +
    		sz16 + sz64 +          // partNo and size
    		((sz16 + sz64) * serverNumMap.count());

	return partSize;
}

char* Header::data()
{
    char *p=new char[getRecordSize()];
    char *i = p;

	quint16 serverCount, serverId;
	quint64 articleNum;
	quint16 sz16 = sizeof(quint16);
	quint16 sz64 = sizeof(quint64);

	memcpy(i, &partNo, sz16);
	i +=sz16;

	memcpy(i, &size, sz64);
	i +=sz64;

	i=insert(messageId,i);

	serverCount=serverNumMap.count();
	memcpy(i, &serverCount, sz16);
	i+=sz16;

	QMap<quint16, quint64>::iterator articleNoit;
	for (articleNoit=serverNumMap.begin(); articleNoit != serverNumMap.end(); ++articleNoit)
	{
		serverId=articleNoit.key();
		articleNum=articleNoit.value();
		memcpy(i, &serverId, sz16);
		i+=sz16;
		memcpy(i, &articleNum, sz64);
		i+=sz64;
	}

    return p;
}

bool Header::dbSave()
{
	/*
	Dbt key, data;
	int szQuint32 = sizeof(quint32);

	char *p = getData();

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	key.set_data(&(pendingHeader->headerId));
	key.set_size(szQuint32);
	data.set_data(p);
	data.set_size(saveItemSize());

	if ((db->put(0, &key, &data, 0)) != 0)
	{
        Q_DELETE_ARRAY(p);
		return false;
	}
	else
	{
        Q_DELETE_ARRAY(p);
		db->sync(0);
		return true;
	}
	*/
    return false;
}

bool Header::dbDelete()
{
	/*
	Dbt key(&(pendingHeader->headerId), sizeof(quint32));

	if ((db->del(NULL, &key, 0)) != 0)
		return false;
	else
	{
		db->sync(0);
		return true;
	}
	*/
    return false;
}
