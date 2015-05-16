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

#ifndef MULTIPARTHEADER_H_
#define MULTIPARTHEADER_H_

#include <qmap.h>
#include "RawHeader.h"
#include "HeaderBase.h"
#include "common.h"
#include "header.h"
#include "nntphost.h"

class MultiPartHeader : public HeaderBase
{
	friend class QMgr;
	friend class QPostItem;
	friend class QUpdItem;
	friend class NewsGroup;
	friend class ExpireDb;
	friend class Quban;

// TODO rework friends and make variables private !!!
	public:
	    quint64 multiPartKey;
		quint16 numParts;
		quint16 missingParts;

	public:
		void setKey(quint64 _multiPartKey) { multiPartKey = _multiPartKey; }
		void setNumParts(quint16 i) {numParts=missingParts=i;}
		Add_Code addPart(Db* pdb, quint16, RawHeader*, quint16);

		bool isCompleted();
		inline quint16 getParts() {return numParts;}
		inline quint16 getMissingParts() {return missingParts;}
        inline quint16 getServerParts(quint16 serverId)     { return serverPart[serverId]; }
        inline quint64 getMultiPartKey() { return multiPartKey; }
		MultiPartHeader();
        MultiPartHeader(quint32, char*, char*);
		virtual ~MultiPartHeader();

		char *data();
		uint getRecordSize();
		QString getIndex();
		void removeServer(Db* pDB, quint16);

		Header* partDbFind(Db* pdb, quint16 partNo);
		Add_Code modifyPart(Db* pdb, quint16 partNo, RawHeader *rh, quint16 hostId);

		void getAllArticleNums(Db* pDB, PartNumMap* serverArticleNos, QMap<quint16, quint64>* partSize,QMap<quint16, QString>* partMsgId);
		void deleteAllParts(Db* pDB);

        static bool getMultiPartHeader(unsigned int, char *, char *, MultiPartHeader*);
};

class NzbHeader : public MultiPartHeader
{
	private:

	    Db* pDB;
		QMap<quint16,Header*> tempHeaders; //part#, Header* Read from Nzb file and waiting to see if required. Never saved ...
		QMap<quint16,Header*>::iterator thit; //

	public:

		NzbHeader(Db* _pDB);
		~NzbHeader() { deleteNzbParts(); }

       void addNzbPart(quint16 num, Header* h) { tempHeaders.insert(num, h); }
       void deleteNzbParts() { qDeleteAll(tempHeaders); tempHeaders.clear(); }

       int saveNzbParts();
};

#endif /* MULTIPARTHEADER_H_ */
