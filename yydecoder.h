#ifndef YYDECODER_H_
#define YYDECODER_H_

/***************************************************************************
                   yydecoder.h  -  Quick and dirty yydecoder class
                             -------------------
    begin                : Sat Jul 3 2004
    copyright            : (C) 2004 by David Pye
    email                : dmp@davidmpye.dyndns.org
	Modified and adapted for KLibido by Alessandro Bonometti - bauno[at]inwind.it
	Original Crc32 code taken (with slight modifications) by crc32.c.
	Here's the original copyright notice:
	***************************************************************************
	crc32.c -- compute the CRC-32 of a data stream
  Copyright (C) 1995-1998 Mark Adler

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Jean-loup Gailly        Mark Adler
  jloup@gzip.org          madler@alumni.caltech.edu
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


typedef unsigned long crc32_t;

/**
  *@author David Pye
  *@author Alessandro Bonometti
  */


class yyDecoder : virtual public Decoder {

public:
	//QStringList of all the file parts that should be decoded.
	//Output file name is derived from the partfiles, if output filename
	//is null, otherwise it is as specified.
	yyDecoder(QStringList fileParts, QString outDirectory, QString outFilename=QString::null, int size=0);
	virtual ~yyDecoder();

	//Returns the filename the posts specify.
	QString encodedFilename();

	//Returns true if this decoder class can decode the parts or not
	//ie whether they are yyencoded
	bool isDecodable();

	//Decodes the next part in the list.
	//Returns Err_no if everything's ok, else return a Decoding_Error (defined in decoder.h)
	int decodeNextPart();

	//Returns true when all the posts are decoded - does not indicate
	//whether all posts were decoded successfully
	bool decodingComplete();
	bool isSizeCorrect();
	void crcAdd(int c);
	void crcInit();

private:

	inline void charCRC(const unsigned char *c);

	QStringList m_fileParts;
	QStringList::Iterator fileIterator;

	unsigned long expectedSize, bytesWritten, expectedPartSize, partBytes;
	unsigned int expectedCRC;
// 	int crc_val;
	uchar bufferLine[5120];
	int bufferIndex;

	crc32_t crc;
	bool badCRC, badPartSize;

	QFile m_outputFile;

	unsigned char ch;
	bool started;


	bool m_decodingComplete;

};


#endif /* YYDECODER_H_ */
