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

#ifndef SINGLEPARTHEADER_H_
#define SINGLEPARTHEADER_H_

#include <QString>
#include <QMap>
#include "HeaderBase.h"
#include "common.h"

class SinglePartHeader : public HeaderBase
{
public:

	SinglePartHeader(quint32 keySize, char* k, char *p, quint16 version=SINGLEPARTDB_VERSION);
	SinglePartHeader();

    QString getMessageId()                       { return messageId; }
    quint16 getParts()                           {if (!serverPart.isEmpty()) return 1; else return 0;}
    quint16 getServerParts(quint16 serverId)     { if (isServerPresent(serverId)) return 1; else return 0;}
    QMap<quint16,quint64>& getServerArticlemap() { return serverNumMap; }

	inline void setMessageId(QString id) { messageId = id; }
	inline void setServerPartNum(quint16 id, quint64 articleNum) { serverNumMap.insert(id, articleNum); serverPart.insert(id, 1); }
	inline void setServerMap(QMap<quint16,quint64> m) { serverNumMap = m; }
	inline void clearServerMap() { serverNumMap.clear(); }

	char *data();
	uint getRecordSize();
	QString getIndex();
	void removeServer(Db* pDB, quint16);
	quint16 getMissingParts() {if (getParts()) return 0; else return 1; }

	void getAllArticleNums(Db* pDB, PartNumMap* serverArticleNos, QMap<quint16, quint64>* partSize,QMap<quint16, QString>* partMsgId);

    static bool getSinglePartHeader(unsigned int keySize, char *k, char *p, SinglePartHeader* sph);

private:
	QString messageId;
	QMap<quint16,quint64> serverNumMap;     // Holds server number, article number
};

#endif /* SINGLEPARTHEADER_H_ */
