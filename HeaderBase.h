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

#ifndef HEADERBASE_H_
#define HEADERBASE_H_

#include <db_cxx.h>
#include "common.h"

	typedef QMap<quint16,quint64> ServerNumMap;     // Holds server number, article num
	typedef QMap<quint16, ServerNumMap> PartNumMap; //Hold <part number 1, 2 etc, <serverid, articlenum>>
	typedef QMap <quint16, quint16> ServerPartMap; //Holds <serverid, number-of-parts-on-server>

class HeaderBase
{
// TODO make variables private !!!
	public:
	    char    headerType;
		QString subj;
		QString from;
        quint16 status;
		quint32 postingDate;
		quint32 downloadDate;
		quint32 lines;
		quint64 size;
		QMap<quint16, quint64> serverLowest; // server, part
		ServerPartMap serverPart; //Useful for speeding loading of headers...

		QMap<quint16, quint64>::iterator lowestit;
		ServerPartMap::iterator spmit;

	public:
        enum status {bh_downloaded, bh_new, bh_read, bh_queued, bh_downloading, MarkedForDeletion = 1024 };

		enum Expire_Code {Delete_Read, Delete_Unread, No_Delete, No_Change };
		enum Add_Code { Duplicate_Part, New_Part, Unread_Status, Updated_Part, Error_Part };

		void setHeaderType(char c) { headerType=c; }
		void setSubj(QString s) {subj=s;}
		void setFrom(QString s) {from=s;}
		void setPostingDate(quint32 u) { postingDate = u; }
		void setDownloadDate(quint32 u) { downloadDate = u; }
        inline void setStatus(quint16 s) {status=s;}
		inline void setLines(quint32 l) {lines = l; }
		inline void setSize(quint64 s) { size = s; }
		inline void increaseSize(quint64 s) { size += s; }

        char getHeaderType() {return headerType;}
        QString getSubj() {return subj;}
        QString getFrom() {return from;}
        quint32 getPostingDate() {return postingDate; }
        quint32 getDownloadDate() {return downloadDate;}
        inline quint32 getSize() {return size;}
        inline quint32 getLines() {return lines;}
        inline quint16 getStatus () {return status;}
        inline quint16 getStatusIgnoreMark() { return status & 1023; }
		QMap<quint16, quint64>& getServerLowest() { return serverLowest; }
		bool isServerPresent(quint16 serverId) { return serverPart.contains(serverId); }

		HeaderBase();
		virtual ~HeaderBase();

		virtual char *data() = 0;
		virtual uint getRecordSize() = 0;
		virtual QString getIndex() = 0;
		virtual quint16 getParts() = 0;
        virtual quint16 getServerParts(quint16 serverId) = 0;
		virtual void removeServer(Db* pDB, quint16) = 0;
		virtual quint16 getMissingParts() = 0;
		virtual QString getMessageId()  { return QString::null; }
		virtual void getAllArticleNums(Db* pDB, PartNumMap* serverArticleNos, QMap<quint16, quint64>* partSize,QMap<quint16, QString>* partMsgId) = 0;

		virtual void deleteAllParts(Db* pDB);


};

#endif /* HEADERBASE_H_ */
