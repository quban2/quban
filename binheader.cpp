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
#include <QDebug>
#include <QtDebug>
#include "binheader.h"

BinHeader::BinHeader()
{
    size=0;
    lines=0;
	status=BinHeader::bh_new;
}

//Puts the size and the data of string s into array p;
//moves p to the next free element;
//returns p

char* BinHeader::insert(QString s, char* p)
{
	quint16 strlen = s.length();
	quint16 suint = sizeof(strlen);
	memcpy(p, &strlen, suint);

	p += suint;
	QByteArray ba = s.toLocal8Bit();
	const char *c_str = ba.data();
	memcpy(p, c_str, strlen);
	p += strlen;
	return p;
}

char* BinHeader::retrieve(char* i, QString &s, quint16 version)
{
	if (version < 8)
	{
		int strlen;
		int ssize = sizeof(strlen);

		char *temp;
		memcpy(&strlen, i, ssize);

		i += ssize;
		temp = new char[strlen + 1];
		memcpy(temp, i, strlen);
		temp[strlen] = '\0';
		s = temp;
		delete[] temp;
		i += strlen;
		return i;
	}
	else
	{
		quint16 strlen;
		quint16 ssize = sizeof(strlen);

		char *temp;
		memcpy(&strlen, i, ssize);

		i += ssize;
		temp = new char[strlen + 1];
		memcpy(temp, i, strlen);
		temp[strlen] = '\0';
		s = temp;
		delete[] temp;
		i += strlen;
		return i;
	}
}

// Marshall the binHeader

char *BinHeader::data()
{
    char *p=new char[getRecordSize()];
    char *i = p;

	quint16 part, serverNumItems, serverId;
	quint32 pSize;
	quint64 artNum;
	quint16 sz16 = sizeof(quint16);
	quint16 sz32 = sizeof(quint32);
	quint16 sz64 = sizeof(quint64);

	memcpy(i, &postingDate, sz32);
	i +=sz32;

	memcpy(i, &downloadDate, sz32);
	i +=sz32;

	memcpy(i, &numParts, sz16);
	i+=sz16;

	serverNumItems=serverLowest.count();
	memcpy(i, &serverNumItems, sz16);
	i+=sz16;

	for (spmit=serverLowest.begin(); spmit != serverLowest.end(); ++spmit)
	{
		serverId=spmit.key();
		artNum=spmit.value();
		memcpy(i, &serverId, sz16);
		i+=sz16;
		memcpy(i, &artNum, sz64);
		i+=sz64;
	}

    i=insert(subj, i);
    i=insert(from,i);

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
		artNum=spmit.value();
		memcpy(i, &serverId, sz16);
		i+=sz16;
		memcpy(i, &artNum, sz64);
		i+=sz64;
	}

	quint16 partNumItems=partNum.count();
    memcpy(i, &partNumItems , sz16);
    i+=sz16;

    for (pnmit=partNum.begin(); pnmit != partNum.end(); ++pnmit)
    {
        //save part number - number of items in server nummap, then the couples
        // serverid-artNum
        part=pnmit.key();
        memcpy(i, &part, sz16);
        i+=sz16;
        serverNumItems=pnmit.value().count();
        memcpy(i, &serverNumItems, sz16);
        i+=sz16;
        for (snmit=pnmit.value().begin(); snmit != pnmit.value().end(); ++snmit)
        {
            serverId=snmit.key();
            artNum=snmit.value();
            memcpy(i, &serverId, sz16);
            i+=sz16;
            memcpy(i, &artNum, sz64);
            i+=sz64;
        }
    }

    quint16 partMidItems = partMid.count();
	memcpy(i, &partMidItems, sz16);
	i+=sz16;
	PartMid::Iterator partMidIt;

	for (partMidIt=partMid.begin(); partMidIt != partMid.end(); ++partMidIt) {
		part=partMidIt.key();
		memcpy(i, &part, sz16);
		i+=sz16;

		i=insert(partMidIt.value(), i);
	}

	//Using serverPartitems for partSize
	serverPartItems=partSize.count();

	memcpy(i, &serverPartItems, sz16);
	i+=sz16;
	QMap<quint16, quint32>::iterator psit;
	//Using serverId, artNum to limit autovariables allocation
	for (psit=partSize.begin(); psit !=partSize.end(); ++psit) {
		serverId=psit.key();
		pSize=psit.value();
		memcpy(i, &serverId, sz16);
		i+=sz16;
		memcpy(i, &pSize, sz32);
		i+=sz32;
	}

	i=insert(xref,i);

    return p;
}

BinHeader::BinHeader(char *p, quint16 version)
{
	char *pi =p;

	if (version < 8)
	{
		int szInt=sizeof(int);
		int dummy;
		uint  part, serverNumItems, serverId, artNum;

		if (version >= 7)
		{
			memcpy(&dummy, pi, szInt);
			pi +=szInt;

			memcpy(&dummy, pi, szInt);
			pi +=szInt;
			postingDate = dummy;

			memcpy(&dummy, pi, szInt);
			pi+=szInt;
			numParts = dummy;
		}

		int serverLowestItems;
		memcpy(&serverLowestItems, pi, szInt);
		pi += szInt;

		for (int i = 0; i < serverLowestItems; i++)
		{
			memcpy(&dummy, pi, szInt);
			pi += szInt;
			serverId = dummy;
			memcpy(&dummy, pi, szInt);
			pi += szInt;
			part = dummy;
			serverLowest.insert(serverId, part);
		}

		pi = retrieve(pi, subj, version);
		pi = retrieve(pi, from, version);

		if (version < 7)
		{
			memcpy(&dummy, pi, szInt);
			pi += szInt;
			postingDate = dummy;

			memcpy(&dummy, pi, szInt);
			pi +=szInt;
			numParts = dummy;
		}

		memcpy(&dummy, pi, szInt);
		pi +=szInt;
		missingParts=dummy;
		memcpy(&dummy, pi, szInt);
		pi +=szInt;
		status = dummy;

		memcpy(&dummy, pi, szInt);
		pi +=szInt;
		lines = dummy;
		memcpy(&dummy, pi, szInt);
		pi +=szInt;
		size = dummy;

		uint serverPartItems, partNumItems;

		memcpy(&serverPartItems, pi, szInt);
		pi +=szInt;

		for (uint i = 0; i < serverPartItems; i++)
		{
			memcpy(&serverId, pi, szInt);
			pi += szInt;
			memcpy(&artNum, pi, szInt);
			pi += szInt;
			serverPart[serverId] = artNum;
		}

		memcpy(&partNumItems, pi, szInt);
		pi +=szInt;

		for (uint i = 0; i < partNumItems; i++)
		{
			memcpy(&part, pi, szInt);
			pi +=szInt;

			memcpy(&serverNumItems, pi, szInt);
			pi +=szInt;

			for (uint j = 0; j < serverNumItems; j++)
			{
				memcpy(&serverId, pi, szInt);
				pi += szInt;
				memcpy(&artNum, pi, szInt);
				pi += szInt;
				partNum[part].insert(serverId, artNum);
			}
		}

		uint partMidItems;
		memcpy(&partMidItems, pi, szInt);
		pi +=szInt;

		QString mid;
		for (uint i = 0; i < partMidItems; i++)
		{
			memcpy(&part, pi, szInt);
			pi +=szInt;
			pi = retrieve(pi, mid, version);
			partMid[part] = mid;

		}

		memcpy(&serverPartItems, pi, szInt);
		pi +=szInt;

		for (uint i = 0; i < serverPartItems; i++)
		{
			memcpy(&serverId, pi, szInt);
			pi += szInt;
			memcpy(&artNum, pi, szInt);
			pi += szInt;
			partSize[serverId] = artNum;
		}
	}
	else // version is >= 8
	{
		quint16 sz16 = sizeof(quint16);
		quint16 sz32 = sizeof(quint32);
		quint16 sz64 = sizeof(quint64);

		quint16 serverId, serverNumItems, localPart;
		quint64 part, artNum;

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

		pi = retrieve(pi, subj, version);
		pi = retrieve(pi, from, version);

		memcpy(&missingParts, pi, sz16);
		pi +=sz16;
		memcpy(&status, pi, sz16);
		pi +=sz16;

		memcpy(&lines, pi, sz32);
		pi +=sz32;

		memcpy(&size, pi, sz64);
		pi +=sz64;

		quint16 serverPartItems;
		quint16 partNumItems;

		memcpy(&serverPartItems, pi, sz16);
		pi +=sz16;

		for (quint16 i = 0; i < serverPartItems; i++)
		{
			memcpy(&serverId, pi, sz16);
			pi += sz16;
			memcpy(&artNum, pi, sz64);
			pi += sz64;
			serverPart[serverId] = artNum;
		}

		memcpy(&partNumItems, pi, sz16);
		pi +=sz16;

		for (quint16 i = 0; i < partNumItems; i++)
		{
			memcpy(&localPart, pi, sz16);
			pi +=sz16;

			memcpy(&serverNumItems, pi, sz16);
			pi +=sz16;

			for (quint16 j = 0; j < serverNumItems; j++)
			{
				memcpy(&serverId, pi, sz16);
				pi += sz16;
				memcpy(&artNum, pi, sz64);
				pi += sz64;
				partNum[localPart].insert(serverId, artNum);
			}
		}

		quint16 partMidItems;
		quint32 partSize2;
		memcpy(&partMidItems, pi, sz16);
		pi +=sz16;

		QString mid;
		for (quint16 i = 0; i < partMidItems; i++)
		{
			memcpy(&part, pi, sz16);
			pi +=sz16;
			pi = retrieve(pi, mid, version);
			partMid[part] = mid;

		}

		memcpy(&serverPartItems, pi, sz16);
		pi +=sz16;

		for (quint16 i = 0; i < serverPartItems; i++)
		{
			memcpy(&serverId, pi, sz16);
			pi += sz16;
			memcpy(&partSize2, pi, sz32);
			pi += sz32;
			partSize[serverId] = partSize2;
		}

		pi = retrieve(pi, xref, version);
	}
}

uint BinHeader::getRecordSize( )
{
	quint32 partNumSize=0;
    quint32 bhSize=0;
    static quint32 maxBhSize=0;
	quint16 sz16 = sizeof(quint16);
	quint16 sz32 = sizeof(quint32);
	quint16 sz64 = sizeof(quint64);

    for (pnmit=partNum.begin(); pnmit != partNum.end(); ++pnmit)
    {
		partNumSize+=(pnmit.value().count()*(sz16+sz64));
		partNumSize+=sz16;
        partNumSize+=sz16;
    }

	PartMid::Iterator partMidIt;
	uint partMidSize = 2*sz16*partMid.count()+sz16;

	for (partMidIt = partMid.begin(); partMidIt != partMid.end(); ++partMidIt)
		partMidSize+=partMidIt.value().length();

	bhSize = 10*sz16 + 3*sz32 + sz64 + subj.length() + from.length() + xref.length() + serverPart.count()*(sz16+sz64) +
			partNumSize + ((sz16+sz32)*partSize.count()) + partMidSize + (sz16+sz64)*serverLowest.count();

//	qDebug() << "bhSize " << bhSize;

	if (bhSize > maxBhSize)
	{
		maxBhSize = bhSize;
		qDebug() << "Found bh size of " << bhSize;
		qDebug() << "Subj = " << subj << ", subj len = " << subj.length() << ", from = " << from.length() << ", xref = " << xref.length() <<
				", serverParts = " << serverPart.count() << ", partNumSize = " << partNumSize <<
				", partSizes = " << partSize.count() << ", partMidSize = "  << partMidSize << ", serverLowests = " << serverLowest.count() <<
				", part nums = " << partNum.count() << ", partMids = " << partMid.count();
	}

	return bhSize;
}

bool BinHeader::isCompleted()
{
    return (missingParts == 0);
}

#ifndef NDEBUG
void BinHeader::printServerPart( )
{
    for (pnmit=partNum.begin() ; pnmit != partNum.end() ; ++pnmit)
    {
        qDebug() << "Part: " << pnmit.key();
        for (snmit=pnmit.value().begin(); snmit != pnmit.value().end(); snmit++)
        {
            qDebug() << "\tserverid:" << snmit.key() << " articlenum: " << snmit.value();
        }
    }
}
#endif

void BinHeader::addNzbPart(int part, int bytes, QString mid )
{
	partSize[part]=bytes;
	partMid[part]="<" + mid + ">";
	size+=bytes;
	missingParts--;
}

BinHeader::Add_Code BinHeader::addPart( quint16 part, Header *h, quint16 hostId )
{
	BinHeader::Add_Code retCode;

    if (!partNum.contains(part))
    { //new part
        partNum[part].insert(hostId, h->m_num);
		partSize[part]=h->m_bytes.toULong();
		//qDebug() << "BH Part bytes = " << partSize[part];

		partMid[part]=h->m_mid;

        if (serverPart.contains(hostId))
            (serverPart[hostId])++;
        else
            serverPart[hostId]=1;

		if (serverLowest.contains(hostId))
		{
			if (serverLowest[hostId] > h->m_num)
				serverLowest[hostId] = h->m_num;
		}
		else
			serverLowest[hostId] = h->m_num;

		//Ok, every time I add a part that was missing, the article becomes new


// 		qDebug("part Size: %d", h->m_bytes.toInt());
        size+=h->m_bytes.toULong();
        lines+=h->m_lines.toInt();

        xref = h->m_xref;
        //qDebug() << "Binheader bytes = " << size << ", lines = " << lines;
        //         hi->setText(1,QString::number(size));
        //         hi->setText(2,QString::number(lines));

        //         new KListViewItem(hi, h.m_subj,h.m_bytes, h.m_lines, h.m_from, h.m_mid);
		if (part==0 || missingParts == 0)
			numParts++;
		else
			missingParts--;

//        if (isCompleted())
//             hi->setPixmap(0,BarIcon("button_ok", KIcon::SizeSmall));
	//         else
        //             hi->setPixmap(0,BarIcon("button_cancel", KIcon::SizeSmall));
		if (status != bh_new)
		{
			status=bh_new;
			retCode =  Unread_Status;
		}
		else
			retCode =  New_Part;

    }
    else if (!partNum[part].contains(hostId))
    {// already have this part, but on another server
        partNum[part].insert(hostId, h->m_num);
        if (serverPart.contains(hostId))
            (serverPart[hostId])++;
        else
            serverPart[hostId]=1;

		if (serverLowest.contains(hostId))
		{
			if (serverLowest[hostId] > h->m_num)
				serverLowest[hostId] = h->m_num;
		}
		else
			serverLowest[hostId] = h->m_num;

		if (xref.isEmpty())
		    xref = h->m_xref;

		retCode =  New_Part;
    }
    else if (partNum[part][hostId]< h->m_num)
    { // replace part with new...
		//Am I replacing the lowest? If yes, rescan to find a new lowest..
		if (serverLowest[hostId] == partNum[part][hostId] )
		{

			//Replace article number
			partNum[part][hostId]=h->m_num;
			//Rescan to find a new lowest
			quint64 lowest=Q_UINT64_C(18446744073709551615);
			for (pnmit = partNum.begin(); pnmit != partNum.end(); ++ pnmit)
			{
				if (pnmit.value().contains(hostId) )
				{
					if (pnmit.value()[hostId] < lowest)
						lowest = pnmit.value()[hostId];
				}
			}
			serverLowest[hostId] = lowest;
		}
		else
			partNum[part][hostId]=h->m_num;

		if (xref.isEmpty())
		    xref = h->m_xref;

		retCode = New_Part;
    }
    else
        return Duplicate_Part; //duplicate?

    return retCode;
}

BinHeader::Expire_Code BinHeader::expire(quint16 hostId)
{
	quint64 part;
	pnmit = partNum.begin();
	while (pnmit != partNum.end() )
	{
		part=pnmit.key();
		pnmit++;

		if (partNum[part].contains(hostId))
		{
			(serverPart[hostId])--;
			partNum[part].remove(hostId);

			if (partNum[part].isEmpty())
			{
				size-=partSize[part];
				partSize.remove(part);
				partNum.remove(part);
				missingParts++;
			}
		}
	}

    //now if the partList is empty, remove the post
	//To keep track of new articles I have to return 3 codes:
	// a) Don't delete (nothing changes)
	// b) Delete, but the article is not new
	// c) Delete a "new" article (unlikely, but...)

    if (partNum.isEmpty())
    {
		if (status == bh_new)
			return Delete_Unread;
		else
			return Delete_Read;
    }
    else
    {
		serverLowest.remove(hostId);
		return No_Delete; //re-save the post...
	}
}

void BinHeader::addHost( quint16 part, quint16 hostId )
{
	//Fake hosts, for chooseserver et al.
	partNum[part].insert(hostId, 0);
}

