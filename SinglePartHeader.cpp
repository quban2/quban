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
#include "common.h"
#include "SinglePartHeader.h"

SinglePartHeader::SinglePartHeader() : HeaderBase()
{
	headerType = 's';
}

SinglePartHeader::SinglePartHeader(quint32 keySize, char* k, char *p, quint16)
{
	quint16 sz16 = sizeof(quint16);
	quint16 sz32 = sizeof(quint32);
	quint16 sz64 = sizeof(quint64);

	char *pi =p;

	quint16 serverCount, serverId;
	quint64 articleNum;

	headerType = *pi;
	pi++;

	char *splitpoint;

	if ((splitpoint = (char *)memchr(k, '\n', keySize)))
	{
        subj = QString::fromLocal8Bit(k, (splitpoint - k));
        messageId = QString::fromLocal8Bit(splitpoint + 1, k + (keySize - 1) - splitpoint);
	}
	else
	{
        subj = QString::fromLocal8Bit(k, keySize);
		messageId = subj;
		qDebug("Single element header encountered ...");
	}

	memcpy(&postingDate, pi, sz32);
	pi +=sz32;

	memcpy(&downloadDate, pi, sz32);
	pi +=sz32;

	pi = retrieve(pi, from);

	memcpy(&size, pi, sz64);
	pi +=sz64;

	memcpy(&lines, pi, sz32);
	pi +=sz32;

	memcpy(&status, pi, sz16);
	pi +=sz16;

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

uint SinglePartHeader::getRecordSize()
{
	quint16 sz16 = sizeof(quint16);
	quint16 sz32 = sizeof(quint32);
	quint16 sz64 = sizeof(quint64);

    uint headerSize= (sz16 * 2) + // 1 String + 1 array
    		from.length() +
    		sz16 + sz64 + (sz32 * 3) +
    		sizeof(char) +
    		((sz16 + sz64) * serverNumMap.count());

	return headerSize;
}

char* SinglePartHeader::data()
{
    char *p=new char[getRecordSize()];
    char *i = p;

	quint16 serverCount, serverId;
	quint64 articleNum;
	quint16 sz16 = sizeof(quint16);
	quint16 sz32 = sizeof(quint32);
	quint16 sz64 = sizeof(quint64);

	*i = headerType;
	i++;

	memcpy(i, &postingDate, sz32);
	i +=sz32;

	memcpy(i, &downloadDate, sz32);
	i +=sz32;

    i=insert(from,i);

	memcpy(i, &size, sz64);
	i +=sz64;

	memcpy(i, &lines, sz32);
	i +=sz32;

	memcpy(i, &status, sz16);
	i +=sz16;

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

bool SinglePartHeader::getSinglePartHeader(unsigned int keySize, char *k, char *p, SinglePartHeader* sph)
{
    char *i=p;
    quint16 sz16 = sizeof(quint16);
    quint16 sz32 = sizeof(quint32);
    quint16 sz64 = sizeof(quint64);
    quint16 count, serverId;
    quint64 articleNum;

    sph->setHeaderType(*i);
    i++;

    char *splitpoint = 0;

    if ((splitpoint = (char *)memchr(k, '\n', keySize)))
    {
        sph->subj = QString::fromLocal8Bit(k, (splitpoint - k));
        sph->setMessageId(QString::fromLocal8Bit(splitpoint + 1, k + (keySize - 1) - splitpoint));
    }
    else
    {
        sph->subj = QString::fromLocal8Bit(k, keySize);
        sph->setMessageId(sph->getSubj());
        qDebug("Single element header encountered ...");
    }

    memcpy(&(sph->postingDate), i, sz32);
    i+=sz32;

    memcpy(&(sph->downloadDate), i, sz32);
    i+=sz32;

    QString from;
    i = retrieve(i, from);
    sph->setFrom(from);

    memcpy(&(sph->size), i, sz64);
    i +=sz64;

    memcpy(&(sph->lines), i, sz32);
    i +=sz32;

    memcpy(&(sph->status), i, sz16);
    i +=sz16;

    memcpy(&count, i, sz16);
    i += sz16;

    sph->clearServerMap();

    for (quint16 j = 0; j < count; j++)
    {
        memcpy(&serverId, i, sz16);
        i += sz16;
        memcpy(&articleNum, i, sz64);
        i += sz64;
        sph->setServerPartNum(serverId, articleNum);
    }

    return true;
}

QString SinglePartHeader::getIndex()
{
    return subj % '\n' % messageId;
}

void SinglePartHeader::getAllArticleNums(Db*, PartNumMap* serverArticleNos, QMap<quint16, quint64>* partSize,QMap<quint16, QString>* partMsgId)
{
	serverArticleNos->insert(1, getServerArticlemap());
	partSize->insert(1, getSize());
	partMsgId->insert(1, getMessageId());
}

void SinglePartHeader::removeServer(Db* , quint16 serverId)
{
	serverNumMap.remove(serverId);
	serverPart.remove(serverId);
}
