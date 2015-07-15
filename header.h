#ifndef HEADER_H_
#define HEADER_H_

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
#include "common.h"

class Header
{
public:
	Header(quint64 key, quint16 partNo);
	Header(quint64 key, quint16 partNo, quint64 size);
	Header(char*, char*, quint16 version=PARTDB_VERSION);

	inline quint64  getMultiPartKey()                { return multiPartKey; }
	inline quint16  getPartNo()                      { return partNo; }
	inline quint64  getSize()                        { return size; }
	inline QString& getMessageId()                   { return messageId; }
	inline quint16  isHeldByServer(quint16 serverId) { return (serverNumMap.value(serverId) > 0); }
	inline bool isServerNumMapEmpty()                { return serverNumMap.isEmpty(); }
	QMap<quint16,quint64>& getServerArticlemap()     { return serverNumMap; }

	inline void setSize(quint64 _size)    { size = _size; }
	inline void setMessageId(QString id) { messageId = id; }
	inline void setServerPartNum(quint16 id, quint64 articleNum) { serverNumMap.insert(id, articleNum); }
	inline void setServerMap(QMap<quint16,quint64> m) { serverNumMap = m; }

	inline void removeServerPartNum(quint16 id) { serverNumMap.remove(id); }

    //bool dbSave();
    //bool dbDelete();

	quint32 getRecordSize();
	char *data();

private:
	quint64 multiPartKey; // Key : multi part that this part is linked to
	// Body starts here :
	quint16 partNo;
	quint64 size;
	QString messageId;
	QMap<quint16,quint64> serverNumMap;     // Holds server number, article number
};

#endif /* HEADER_H_ */
