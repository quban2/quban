/***************************************************************************
                          decoder.cpp  -  description
                             -------------------
    begin                : Sat Jul 3 2004
    copyright            : (C) 2004 by David Pye
    email                : dmp@davidmpye.dyndns.org
 ***************************************************************************/
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

#include "decoder.h"

Decoder::Decoder(QStringList , QString , QString, int )
{

}

Decoder::~Decoder()
{

}

QString Decoder::encodedFilename()
{
	return QString::null;
}

bool Decoder::isDecodable()
{
	return false;
}

int Decoder::decodeNextPart()
{
	return false;
}

bool Decoder::decodingComplete()
{
	return false;
}
