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

#ifndef RAWHEADER_H_
#define RAWHEADER_H_

#include <QStringList>

class RawHeader
{
public:
	RawHeader(QString);
	virtual ~RawHeader();

    QString m_from;
    QString m_subj;
    QString m_lines;
    QString m_mid;
    QString m_bytes;
    QString m_date;
    QString m_xref;
    quint16 status;
	quint64 m_num;

	bool isOk() {return ok;}

private:
	bool ok;
};

#endif /* RAWHEADER_H_ */
