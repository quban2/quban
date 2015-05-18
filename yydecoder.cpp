/***************************************************************************
                          yydecoder.cpp  -  description
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

#include <qfileinfo.h>
#include <QDebug>
#include <QtDebug>
#include "yydecoder.h"

#define ABSOLUTEMAXLINESIZE 4095

crc32_t crc_table[256] = {
  0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
  0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
  0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
  0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
  0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
  0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
  0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
  0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
  0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
  0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
  0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
  0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
  0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
  0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
  0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
  0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
  0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
  0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
  0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
  0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
  0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
  0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
  0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
  0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
  0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
  0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
  0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
  0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
  0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
  0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
  0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
  0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
  0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
  0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
  0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
  0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
  0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
  0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
  0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
  0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
  0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
  0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
  0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
  0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
  0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
  0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
  0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
  0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
  0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
  0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
  0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
  0x2d02ef8dL
};

yyDecoder::yyDecoder(QStringList fileParts, QString outDirectory, QString outFilename, int size):
	Decoder(fileParts,outDirectory, outFilename)
{
	m_fileParts = fileParts;
	fileIterator = m_fileParts.begin();

	if (!outFilename.isNull())
	{
		m_outputFile.setFileName(outDirectory+"/"+outFilename);
	}
	else
	{
		QString encodedFilename = this->encodedFilename();
		if (!encodedFilename.isNull())
		{
			m_outputFile.setFileName(outDirectory+"/"+encodedFilename);
		}
		else
		{
			qDebug()<<"yyDecoder: Unable to obtain filename for decoded file\n";
			return;
		}
        qDebug()<<"Output file is "<<outDirectory<<"/"<<encodedFilename;
	}

	m_decodingComplete=false;
	bytesWritten=0;
	expectedSize=size;

}

yyDecoder::~yyDecoder()
{
	//An explicit close is almost certainly unnecessary, but to be safe.
	m_outputFile.close();
    //qDebug("~yyDecoder()");
}

bool yyDecoder::isDecodable()
{
	QFile partFile(*(m_fileParts.begin()));
	bool retval=false;
	if (partFile.open(QIODevice::ReadOnly))
	{
		//Find the begin tag
		QString line;
		char cline[1025];
		while (partFile.readLine(cline,1024)!=-1)
		{
            line = QString::fromLatin1(cline);
			//Remove leading and trailing whitespace - we're looking for the begin line
			line = line.trimmed();
			if (line.left(7)=="=ybegin") {
				//Split the line...
				int sizePos = line.indexOf("size=");
				expectedSize=line.mid(sizePos+5, line.indexOf(" ", sizePos+5) - sizePos - 5).toLong();

				retval=true;
				break;
			}
		}
	}
	return retval;
}

QString yyDecoder::encodedFilename()
{
	QFileInfo fi(m_outputFile);
	return fi.fileName();
}

int yyDecoder::decodeNextPart()
{
	if (!m_outputFile.isOpen()) {
		if (!m_outputFile.open(QIODevice::ReadWrite))
		{
			qDebug()<<"yyDecoder: Unable to open output file "<<
					m_outputFile.fileName()<<" - aborting\n";
			return Decoder::Err_Write;
		}
	}

	QFile partFile(*fileIterator);

	//holds the PART crc
	crc= 0L;
	started = false;
	badCRC=false;
	badPartSize=true;
	char* cline = 0;

	bufferIndex=0;
	if (!partFile.open(QIODevice::ReadOnly))
	{
		qDebug()<<"yyDecoder: Unable to open file part "+ partFile.fileName()+"\n";
		//Do not consider it a disk error...go on to the next file
		fileIterator++;
		if (fileIterator==m_fileParts.end())
			m_decodingComplete=true;
		return Decoder::Err_MissingPart;
	}
	else
	{
// 		qDebug("File opened");
		QString line;

		long partLineSize = 0L;
		int maxLineSize = 1024;

		cline = (char*)malloc(maxLineSize + 1);
		Q_CHECK_PTR(cline);
		unsigned int len = 0;

		while (partFile.readLine(cline,maxLineSize)!=-1)
		{
			if (!started && !qstrncmp( cline, "=ybegin", 7 ))
			{
                line = QString::fromLatin1(cline);
				len = line.length();

				//Remove the \r\n
//				len-=2;
//				line.remove(len,2);

				int linePos=line.indexOf("line=");
				QString partSize=line.mid(linePos+5, line.indexOf( " ", linePos +5) - linePos-5);
				partLineSize=partSize.toLong();
//				qDebug() << "Part line size: " << partLineSize;

				if (partLineSize > maxLineSize && partLineSize <= ABSOLUTEMAXLINESIZE)
				{
					cline = (char*)realloc(cline, partLineSize + 1);
					Q_CHECK_PTR(cline);
				}

				int sizePos=line.indexOf("size=");
				QString size=line.mid(sizePos+5, line.indexOf( " ", sizePos +5) - sizePos-5);
// 				qDebug() << "Size: " << size << endl;
				expectedPartSize=size.toLong();
				m_outputFile.seek(0);
				started=true;
				partBytes=0;

				partFile.readLine(cline, maxLineSize);
                line = QString::fromLatin1(cline);

// 				qDebug() << "Read: " << line << endl;
				if (line.left(6) == "=ypart") {
					int beginPos = line.indexOf("begin=");
				//6 is the length of 'begin='
					QString off = line.mid(beginPos+6,line.indexOf(" ",beginPos+6)-beginPos-6);
					if (!m_outputFile.seek(off.toLong()-1)) qDebug()<<"Set offset failed!\n";
					int endPos = line.indexOf("end=");
				//4 is the length of 'end='
					QString end = line.mid(endPos+4,line.indexOf(" ",endPos+4)-endPos-4);
// 				qDebug() << "Start: " << off.toInt() << " End: " << end.toInt() << endl;
					expectedPartSize=end.toInt() - off.toInt() + 1;
					started=true;
					partBytes=0;
					partFile.readLine(cline, maxLineSize);
				}
			}
			else if (started && !qstrncmp( cline, "=yend", 5 ))
			{
                line = QString::fromLatin1(cline);
				len = line.length();

				//Remove the \r\n
				len-=2;
				line.remove(len,2);

				int beginCRC;
				if ((beginCRC=line.indexOf("pcrc32=")) != -1)
				{
					//May be a post crc
					expectedCRC=line.mid(beginCRC + 7, 8).toUInt(NULL, 16);
				}
				else if ((beginCRC=line.indexOf("crc32=")) != -1)
				{
					expectedCRC=line.mid(beginCRC +6, 8).toUInt(NULL, 16);
				}
				if (beginCRC == -1)
				{
					qDebug() << "Cannot find crc!\n";
					expectedCRC=0;
					badCRC=false;
				}
				else
				{
					//CRC32 is always 8 chars...
					//compare part crc...
					if (crc != expectedCRC)
					{
						qDebug() << "Warning: part crc differs!\n";
						qDebug() << "Expected CRC: " << expectedCRC << " WrittenCRC: " << crc;
						badCRC=true;
					} else badCRC=false;
				}
				if (partBytes != expectedPartSize)
				{
					qDebug() << "Wrong part size. Expected: " << expectedPartSize << " Written: " << partBytes;
// 				else qDebug() << "Part size ok!\n";
					badPartSize=true;
				}
				else
					badPartSize=false;

				break;
			}
			if (started)
			{
				//Remove the \r\n
				len = qstrlen(cline);
				if (len > 1)
				{
				    len -= 2;
				    cline[len] = 0;
				}

			    //It's a data line
				const char *ascii_rep = cline;

				for (unsigned int i=0; i<qstrlen(cline); ++i)
				{
					if (ascii_rep[i]=='=')
					{
						ch = ascii_rep[++i] -106;
					}
					else
					{
						ch=ascii_rep[i]-42;
					}
					bufferLine[bufferIndex++]=ch;
					bytesWritten++;
					partBytes++;
					charCRC((const unsigned char *) &ch);
				}

				//We don't want to risk a buffer overflow...
				if (bufferIndex > 4095)
				{
					if (m_outputFile.write((const char *)bufferLine, bufferIndex) != bufferIndex)
					{
						//Write error
						qDebug() << "Disk error!!!\n";
						partFile.close();
						return Decoder::Err_Write;
					}

					bufferIndex=0;
				}
			}
		}
		// Flush the remaining buffer...
		// We do this here and not when we found a =yend (where it'd be more logical)
		// because we want to be safe, in case the part file is corrupt and no end is found...

		if (bufferIndex != 0)
		{
			if (m_outputFile.write((const char *)bufferLine, bufferIndex) != bufferIndex )
			{
				qDebug() << "Disk error!!!\n";
				partFile.close();
				return Decoder::Err_Write;
			}
			else
				bufferIndex=0;
		}
		partFile.close();
	}

	if (cline)
	    free(cline);

	fileIterator++;
	if (fileIterator==m_fileParts.end())
	{
		m_decodingComplete=true;
		//Compare total size...
	}

	if (!started)
	{
		qDebug() << "Part is not yencoded\n";
		return Decoder::Err_No;
	}
	else if (badCRC)
	{
		qDebug() << "Bad part crc!" << endl;
		return Decoder::Err_BadCRC;
	}
	else if (badPartSize)
	{
		qDebug() << "Bad part size!" << endl;
		return Decoder::Err_SizeMismatch;
	}
	else
		return Decoder::Err_No;
}

bool yyDecoder::decodingComplete() {
	return m_decodingComplete;
}




void yyDecoder::charCRC(const unsigned char *c) {
	crc ^= 0xffffffffL;
	crc=crc_table[((int)crc ^ (*c)) & 0xff] ^ (crc >> 8);
	crc ^= 0xffffffffL;


}

bool yyDecoder::isSizeCorrect( )
{
	return (expectedSize == bytesWritten) ? true : false;
}

