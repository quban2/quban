/***************************************************************************
                          uudecoder.cpp  -  description
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
#include "uudecoder.h"

uuDecoder::uuDecoder(QStringList fileParts, QString outDirectory, QString outFilename, int size):
		Decoder(fileParts, outDirectory, outFilename) {

	m_fileParts = fileParts;
	fileIterator = m_fileParts.begin();

	if (outFilename!=QString::null) {
		m_outputFile.setFileName(outDirectory+"/"+outFilename);
	}
	else {
		QString encodedFilename = this->encodedFilename();
		if (encodedFilename != QString::null) {
			m_outputFile.setFileName(outDirectory+"/"+encodedFilename);
		}
		else {
			qDebug()<<"uuEncoder:: Unable to obtain filename for decoded file";
			return;
		}
	}
	m_decodingComplete=false;
	decodedParts=0;
	totalParts=size;
	started=false;
	ended=false;
}

uuDecoder::~uuDecoder(){
	//An explicit close is almost certainly unnecessary, but to be safe.
	m_outputFile.close();
//	qDebug("~uuDecoder()");
}

bool uuDecoder::isDecodable() {
// 	bool retval=false;
	//The first part alone contains the output filename
	QStringList::Iterator partIt = m_fileParts.begin();
	while (partIt != m_fileParts.end()) {

		QFile partFile(*(partIt));
		++partIt;
		if (partFile.open(QIODevice::ReadOnly)) {
			//Find the begin tag
			QString line;
			char cline[1025];
			while (partFile.readLine(cline,1024)!=-1)
			{
                line = QString::fromLatin1(cline);
				//Remove leading and trailing whitespace - we're looking for the begin line
				line = line.trimmed();
				if (line.left(5)=="begin") {
// 					qDebug() << "UUencoded file found!\n";
					return true;
				}
			}
		}
	}
	return false;
}

QString uuDecoder::encodedFilename()
{
	QFileInfo fi(m_outputFile);
	return fi.fileName();
}

int uuDecoder::decodeNextPart()
{
	if (m_decodingComplete)
		return true;

	if (!m_outputFile.isOpen())
	{
		//If the output file is not open, open it now.
		//It remains open until the object is destroyed.
		if (!m_outputFile.open(QIODevice::ReadWrite)) {
			qDebug()<<"Unable to open output file "<<m_outputFile.fileName()<<" - aborting";
			return Err_Write;
		}
	}

	QFile partFile(*fileIterator);
	if (started && partFile.size() == 0)
	{
		//Wrong size...a 423/430 error
		fileIterator++;
		if (fileIterator == m_fileParts.end())
			m_decodingComplete=true;
		return Decoder::Err_MissingPart;

	}
	else if (!partFile.open(QIODevice::ReadOnly))
	{
		qDebug()<<"uuDecoder: Unable to open file part "+ partFile.fileName();
		fileIterator++;
		decodedParts++;
		if (fileIterator==m_fileParts.end())
			m_decodingComplete=true;
		return Decoder::Err_MissingPart;
	}
	else {

		bufferIndex=0;

		char cline[1025];
		unsigned int len = 0;

		while (partFile.readLine(cline,1024)!=-1)
		{
			if (!started && !qstrncmp( cline, "begin", 5 ))
			{
				started = true;
// 				qDebug() << "Begin line: " << line << endl;
			}
			else if ( started && !qstrncmp( cline, "end", 3 ))
			{
				ended=true;
				break;
			}
			else if (started)
			{
				//Strip the CRLF from the end of the line
				len = qstrlen(cline);
				if (len > 1)
				{
				    len -= 2;
				    cline[len] = 0;
				}

				const char *ascii_rep = cline;
				unsigned int output_bytes = ascii_rep[0]-' ';
				unsigned int line_length = len;
				//qDebug()<<"The output bytes count is "<<output_bytes<<"\n";
				//qDebug()<<"The input bytes count is "<<line_length-1<<"\n";
				for (unsigned int i=1; i<line_length; i+=4)
				{
					//Blocks of four characters.
					char data[4];
					memcpy(data, &ascii_rep[i],4);
					for (int j=0; j<4; j++)
					{
						data[j] = (data[j]-' ')&0x3F;
					}

					bufferLine[bufferIndex++]= ( (data[0] << 2) | ((data[1] >> 4) & 0x03) );
					if (output_bytes > 1)
					{
						bufferLine[bufferIndex++] = (((data[1]&0x0f) << 4) | ((data[2] >> 2) &0x0f));
						if (output_bytes > 2)
						{
							bufferLine[bufferIndex++] = (((data[2]& 0x03) << 6) | ((data[3]) &0x3f));
						}
					}

					output_bytes-=3;
				}
				if (bufferIndex > 4095)
				{
					if (m_outputFile.write((const char *) bufferLine, bufferIndex) != bufferIndex)
					{
						qDebug() << "Disk error!!!\n";
						partFile.close();
						return Decoder::Err_Write;
					}

					bufferIndex=0;
				}
			}
		}
		if (bufferIndex > 0)
		{
			if (m_outputFile.write((const char *) bufferLine, bufferIndex) != bufferIndex)
			{
				qDebug() << "Disk error!!!\n";
				partFile.close();
				return Decoder::Err_Write;
			}
		}
		partFile.close();
		decodedParts++;
	}

	fileIterator++;
	if (fileIterator==m_fileParts.end()) {
		m_decodingComplete=true;
	}
	return Decoder::Err_No;
}

bool uuDecoder::decodingComplete() {
	return m_decodingComplete;
}

bool uuDecoder::isSizeCorrect( )
{
	//How?
	qDebug() << "TotalParts: " << totalParts << " DecodedParts: " << decodedParts;
	return ( ended && started && (decodedParts >= totalParts));
}
