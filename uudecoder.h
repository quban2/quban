#ifndef UUDECODER_H_
#define UUDECODER_H_

/***************************************************************************
                   uudecoder.h  -  Quick and dirty uudecoder class
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
#include "decoder.h"

/**
  *@author David Pye
  */

class uuDecoder : virtual public Decoder {

public:
	//QStringList of all the file parts that should be decoded.
	//Output file name is derived from the partfiles, if output filename
	//is null, otherwise it is as specified.
	uuDecoder(QStringList fileParts, QString outDirectory, QString outFilename=QString::null, int size=0);
	~uuDecoder();

	//Returns the filename the posts specify.
    QString encodedFilename();

	//Returns true is the post appears to be uuencoded.
	bool isDecodable();

	//Decodes the next part in the list.
	//Returns false if an error occurred
	int decodeNextPart();
	//Returns true when all the posts are decoded - does not indicate
	//whether all posts were decoded successfully
	bool decodingComplete();
	bool isSizeCorrect();

private:
	QStringList m_fileParts;
	QStringList::Iterator fileIterator;

	QFile m_outputFile;

	bool m_decodingComplete;
	uchar bufferLine[5120];
	int bufferIndex;

// 	unsigned char ch;
	bool started, ended;
	int totalParts;
	int decodedParts;

};

#endif /* UUDECODER_H_ */
