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
#include <QFileInfo>
#include <fstream>
#include "appConfig.h"
#include "common.h"
#include "decodeManager.h"

DecodeManager::DecodeManager()
{
	m_cancel = false;
}

DecodeManager::~DecodeManager()
{
}

void DecodeManager::decodeItem(QPostItem* item)
{
	items.append(item);
	decodeList();
}

void DecodeManager::decodeList()
{
	qDebug() << "Starting decode thread...";
	config = Configuration::getConfig();
	m_overWrite = config->overwriteExisting;

	while (!items.isEmpty())
	{
		item = items.first();
		if (decode(item))
		{
			items.removeAll(items.first());
			item = 0;
		}
		else if (m_cancel)
		{
            emit logMessage(LogAlertList::Alert, tr("Cancelled decode job"));
            emit decodeCancelled(item);
			items.removeAll(items.first());
			item = 0;
			m_cancel = false;
		}
		else
			break;

	}
}

Decoder * DecodeManager::getDecoderForPost(QStringList posts)
{
	int linesToCheck = 100;
	QStringList::Iterator partIt = posts.begin();
	while (partIt != posts.end())
	{
		QFile partFile(*partIt);
		++partIt;
		if (partFile.open(QIODevice::ReadOnly))
		{
			//Find the begin tag
			QString line;
			char lineBuf[1025];
			while ((linesToCheck-- > 0) && (partFile.readLine(lineBuf,
					(qint64) 1024) != -1))
			{
				line = QString::fromLocal8Bit(lineBuf);
				if (line.left(5) == "begin")
				{
					qDebug() << "UUencoded post found";
					// 					qDebug() << "UUencoded file found!\n";
					partFile.close();
					//We want everything after "begin 644 "
					QString fName = line.trimmed().mid(10);
                    //qDebug() << "Checking existance of " << destDir + fName;
					if (!m_overWrite && QFile::exists(destDir + fName))
					{
						qDebug() << "File exists!";
						//rename the file
						int i = 1;
						while (QFile::exists(
								destDir + fName + '.' + QString::number(i)))
						{
							i++;
						}
						fName += '.';
						fName += QString::number(i);
					}
					qDebug() << "Fname: " << fName;
					return new uuDecoder(posts, destDir, fName, parts);

				}
				else if (line.left(7) == "=ybegin")
				{
					//ok, found a yyencoded post
					line = line.trimmed();
					qDebug() << "yyenc post found";
					int namePos = line.indexOf("name=");
					QString fName = line.right(line.length() - namePos - 5);
					if (!m_overWrite && QFile::exists(destDir + fName))
					{
						int i = 1;
						while (QFile::exists(
								destDir + fName + '.' + QString::number(i)))
							i++;
						fName += '.';
						fName += QString::number(i);
					}
					qDebug() << "Filename: " << fName;
					int sizePos = line.indexOf("size=");
					int expectedSize =
							line.mid(
									sizePos + 5,
									line.indexOf(" ", sizePos + 5) - sizePos
											- 5).toLong();
					partFile.close();
					return new yyDecoder(posts, destDir, fName, expectedSize);

				}
			}
		}
		partFile.close();
	}

	return NULL;
}

bool DecodeManager::decode(QPostItem *postItem)
{
	emit decodeStarted(postItem);

    QFile thisPart;
	QTime start, end;
	start = QTime::currentTime();
	QStringList partList;
	bool errorFlag = false;
	bool diskError = false;
	bool asciiFile = false;
	QString destFile2;
	QString errorString = QString::null;
	QMap<int, Part *>::iterator pit;
	parts = 0;
	int progress = 0;

	//Build a QStringList of filenames (with path)
	for (pit = postItem->parts.begin(); pit != postItem->parts.end(); ++pit)
	{
		partList.append(postItem->getFName() + '.' + QString::number(pit.key()));
		if (pit.key() > parts)
			parts = pit.key();
	}

	destDir = postItem->getSavePath();

	decoder = getDecoderForPost(partList);
	int doneParts = 0;
	if (decoder != NULL)
	{
		while (!m_cancel && !diskError && !decoder->decodingComplete())
		{
			doneParts++;

			switch (decoder->decodeNextPart())
			{
			case Decoder::Err_BadCRC:
				//bad crc in part
				errorFlag = true;
				errorString += "Bad part crc; ";
				break;
			case Decoder::Err_MissingPart:
				//Missing part...right now only activated if cannot read from input
				errorFlag = true;
				errorString += "Missing part; ";
				break;
			case Decoder::Err_No:
				//No error
				break;
			case Decoder::Err_SizeMismatch:
				//Size mismatch between expected size and written bytes
				errorFlag = true;
				errorString += "Part size mismatch; ";
				break;
			case Decoder::Err_Write:
				//Disk error...disk full?
				errorFlag = true;
				diskError = true;
				errorString = "I/O error!";
				break;
			}
			progress = (int) (((float) doneParts / (float) parts) * 100);
			emit decodeProgress(postItem, progress);
		}
		if (!m_cancel && !decoder->isSizeCorrect())
		{
			//Error: size is wrong.
			//This can happen because there are some missing parts in the post.
			errorFlag = true;
			errorString += "Wrong size";
			qDebug() << "Wrong total size";
		}
	}
    else
    {
        qDebug() << "Post is not uu/yy encoded";

        if (config->deleteFailed)
        {
            errorFlag = true;
            errorString = "Post is not uu/yy encoded";
            emit logMessage(LogAlertList::Warning, tr("Decode error: Post is not uu or yy encoded"));
        }
        else if (!m_cancel)
        {
            QString tempFile;
            QString destFile1;
            QByteArray ba;
            char *c_str;
            char *c_str2;

            asciiFile = true;

            for (pit = postItem->parts.begin(); pit != postItem->parts.end(); ++pit)
            {
                tempFile = postItem->getFName() + '.' + QString::number(pit.key());

                thisPart.setFileName(tempFile);

                if (thisPart.open(QIODevice::ReadOnly | QIODevice::Text))
                {
                    //qDebug() << "Part size : " << thisPart.size();
                    if (thisPart.size() == 0)
                    {
                        thisPart.remove(); // closes as well
                    }
                    else
                    {
                        thisPart.close();
                        ba = tempFile.toLocal8Bit();
                        c_str = ba.data();

                        std::ifstream f1(c_str, std::fstream::binary);

                        destFile1 = postItem->getHeaderBase()->getSubj().replace(' ', '_');
                        destFile2 = (destDir + "/" + destFile1.remove(QRegExp("\\W"))
                                     + '.' + QString::number(pit.key()) + ".txt");

                        qDebug() << "Checking existance of " << destFile2;
                        if (!m_overWrite && QFile::exists(destFile2))
                        {
                            qDebug() << "File exists!";
                            //rename the file
                            int i = 1;
                            destFile1 = destFile2.remove(".txt");
                            while (QFile::exists(
                                       destFile1 + '.' + QString::number(i) + ".txt"))
                            {
                                i++;
                            }

                            destFile2 = destFile1 + '.' + QString::number(i) + ".txt";
                        }
                        qDebug() << "Fname: " << destFile2;

                        ba = destFile2.toLocal8Bit();
                        c_str2 = ba.data();
                        std::ofstream f2(c_str2,
                                         std::fstream::trunc | std::fstream::binary);
                        f2 << f1.rdbuf();
                    }
                }
            }
        }
	}

	end = QTime::currentTime();
	qDebug() << "Seconds used for decoding with knzb decoder: "
			<< start.secsTo(end);
	if (m_cancel)
	{
		qDebug() << "Cancelled decoding";
		qDebug() << "Deleting partial file: " << destDir + '/'
				+ decoder->encodedFilename();

		//Remove partial file
		if (decoder)
			QFile::remove(destDir + '/' + decoder->encodedFilename());
		return false;
	}
	else if (errorFlag && !diskError)
	{
        emit logMessage(LogAlertList::Warning, errorString);

		if (decoder)
            emit decodeFinished(postItem, false, decoder->encodedFilename(), errorString);
		else
            emit decodeFinished(postItem, false, QString::null, errorString);
	}
	else if (diskError)
	{
        emit logMessage(LogAlertList::Warning, errorString);
		emit decodeDiskError();
	}
	else
	{
		if (asciiFile == false)
		    emit decodeFinished(postItem, true, decoder->encodedFilename(), QString::null);
		else
		{
			QFileInfo fi(destFile2);
			emit decodeFinished(postItem, true, fi.fileName(), QString::null);
		}
	}

    Q_DELETE(decoder);

	if (diskError)
		return false;
	else
		return true;
}

void DecodeManager::cancel(int qID)
{
	//Check if we're cancelling the currently decoding item
	qDebug() << "Currently decoding: " << item->qItemId;
	if (item->qItemId == qID)
	{
		m_cancel = true;
	}
	else
	{
		//Sync cancel...
		QLinkedList<QPostItem*>::iterator it;
		for (it = items.begin(); it != items.end(); ++it)
		{
			if ((*it)->qItemId == qID)
			{
				//Cancel item
				items.erase(it);
				//send a message to the QMgr saying we cancelled the item
				emit decodeCancelled(*it);
				break;

			}
		}
	}
}

void DecodeManager::cancel()
{
    m_cancel = true;

    QLinkedList<QPostItem*>::iterator it;
    for (it = items.begin(); it != items.end(); ++it)
    {
        //Cancel item
        items.erase(it);
        //send a message to the QMgr saying we cancelled the item
        emit decodeCancelled(*it);
    }
}
