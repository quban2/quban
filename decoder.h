#ifndef DECODER_H_
#define DECODER_H_

/***************************************************************************
                   decoder.h  -  Quick and dirty yydecoder class
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

#include <qobject.h>
#include <qstringlist.h>
#include <qstring.h>
#include <qfile.h>

/**
  *@author David Pye
  */

class Decoder {

public:
	//QStringList of all the file parts that should be decoded.
	//Output file name is derived from the partfiles, if output filename
	//is null, otherwise it is as specified.
	Decoder(QStringList fileParts, QString outDirectory, QString outFilename=QString::null, int size=0);
	enum Decoding_Error {Err_No, Err_Write, Err_MissingPart, Err_BadCRC, Err_SizeMismatch};
	virtual ~Decoder();

	//Returns the filename the posts specify.
    virtual QString encodedFilename();

	//Returns true if this decoder class can decode the parts or not
	//ie whether they are yyencoded
	virtual bool isDecodable();

	//Decodes the next part in the list.
	//Returns false if an error occurred
	virtual int decodeNextPart();

	//Returns true when all the posts are decoded - does not indicate
	//whether all posts were decoded successfully
	virtual bool decodingComplete();
	virtual bool isSizeCorrect() = 0;
};

#endif /* DECODER_H_ */

