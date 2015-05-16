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
#include "RawHeader.h"

RawHeader::RawHeader(QString s)
{
	QStringList field=s.split('\t', QString::KeepEmptyParts);
	if (field.count() < 8)
	{
		qDebug() << "Error parsing xover, only " << field.count() << " fields: " << s;
		if (field.count() > 0)
		    qDebug() << "Article number: " << field[0];
		ok=false;
	}
	else
	{
		ok=true;
		m_num=field[0].toULongLong();
		m_subj=field[1];
		m_from=field[2];
		m_date=field[3];
		m_mid=field[4];
		m_bytes=field[6];
		m_lines=field[7];
	}
}

RawHeader::~RawHeader()
{
	// TODO Auto-generated destructor stub
}
