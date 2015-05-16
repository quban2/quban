#ifndef BINHEADER_H_
#define BINHEADER_H_

/***************************************************************************
     Copyright (C) 2011 by Martin Demet
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

#include <qmap.h>
#include <header.h>
#include <common.h>
#include "nntphost.h"

/**
@original author Alessandro Bonometti
*/

#define SBH_MAX_STRING_SIZE 2047

struct SmallBinHeader
{
	char    subj[SBH_MAX_STRING_SIZE + 1];
	char    from[SBH_MAX_STRING_SIZE + 1];
	quint32 postingDate;
	quint32 downloadDate;
	quint16 missingParts;
	quint16 parts;
	quint64 size;
	quint16 status;
	QMap<quint16,quint64> serverParts; // server id, num parts
};

typedef QMap<quint16,quint64> ServerNumMap;     // Holds server number, article num
typedef QMap<quint16, ServerNumMap> PartNumMap; //Hold <part number 1, 2 etc, <serverid, articlenum>>
typedef QMap <quint16, quint64> ServerPartMap; //Holds <serverid, number-of-parts-on-server>
typedef QMap <quint16, QString> PartMid;       // part number, message id

class BinHeader
{
	friend class QMgr;
	friend class QPostItem;
	friend class QUpdItem;
	friend class NewsGroup;
	friend class ExpireDb;
	friend class quban;

// TODO rework friends and make variables private !!!
	public:
		QString subj;
		QString from;
		QString date;
		quint32 postingDate;
		quint32 downloadDate;
		quint16 numParts;
		quint16 missingParts;
		quint16 status;
		quint32 lines;
		quint64 size;
		QString xref;
		PartNumMap partNum;
		PartMid partMid; //part#, mid
		ServerPartMap serverPart; //Useful for speeding loading of headers...
		QMap<quint16,quint32> partSize; //part#, part Size in bytes
		QMap<quint16, quint64> serverLowest; // server, part

		ServerNumMap::iterator snmit;
		ServerPartMap::iterator spmit;
		QMap<quint16, quint64>::iterator lowestit;
		PartNumMap::iterator pnmit;

	public:
		enum status {bh_downloaded, bh_new, bh_read, bh_queued, bh_downloading};

		enum Expire_Code {Delete_Read, Delete_Unread, No_Delete, No_Change };
		enum Add_Code { Duplicate_Part, New_Part, Unread_Status };
		void setSubj(QString s) {subj=s;}
		void setFrom(QString s) {from=s;}
		void setPostingDate(quint32 u) { postingDate = u; }
		void setDownloadDate(quint32 u) { downloadDate = u; }
		void setNumParts(quint16 i) {numParts=missingParts=i;}
		void addNzbPart(int, int, QString);
		void addHost(quint16 part, quint16 hostId);
		Add_Code addPart(quint16, Header*, quint16);
		char* retrieve(char* i, QString &s, quint16 version);
		char* insert(QString, char*);

		bool isCompleted();
		QString getSubj() {return subj;};
		QString getFrom() {return from;};
		quint32 getPostingDate() {return postingDate;};
		quint32 getDownloadDate() {return downloadDate;};
		inline quint32 getSize() {return size;};
		inline quint32 getLines() {return lines;};
		inline quint16 getParts() {return numParts;};
		inline quint16 getMissingParts() {return missingParts;};
		inline quint16 getStatus () {return status;};
		inline void setStatus(quint16 s) {status=s;};
		BinHeader();
		BinHeader(char*, quint16 version=HEADERDB_VERSION);
		char *data();

		uint getRecordSize();
#ifndef NDEBUG
		void printServerPart(); //Debug function
#endif
		Expire_Code expire(quint16 hostId);
};

#endif /* BINHEADER_H_ */
