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
#include "fileviewer.h"
#include "appConfig.h"
#include <QMessageBox>
#include <QVBoxLayout>
#include <QFile>
#include <QTextStream>
#include <QStringList>
#include "common.h"

FileViewer::FileViewer(QString& fileName, QWidget *parent) : QTabWidget(parent)
{
	bool imageFound = false;

    // qDebug() << "Request to view file : " << fileName;
    fullFileName = fileName;

    // try and load as an image first
    imageLabel = new QLabel;
    imageLabel->setBackgroundRole(QPalette::Base);
    imageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    QImage image(fileName);
	if (!image.isNull())
	{
		imageLabel->setPixmap(QPixmap::fromImage(image));
		imageFound = true;
	}
	else // not an image that Qt handles
	{
		textArea = new QTextEdit;
		textArea->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

		// see if we think it's a text file
		Configuration* config = Configuration::getConfig();
		QStringList suffixList = config->textSuffixes.split(" ", QString::SkipEmptyParts);

		bool textFileFound = false;

		for (int i=0; i<suffixList.count(); i++)
		{
		    if (fileName.endsWith(suffixList.at(i), Qt::CaseInsensitive))
		    {
		    	textFileFound=true;
		    	break;
		    }
		}

		if (textFileFound == true)
		{
		     QFile file(fileName);
		     if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		     {
		         QMessageBox::information(this, tr("File Viewer"),
								  tr("Cannot load %1.").arg(fileName));
		         return;
		     }

		     while (!file.atEnd())
		     {
		         QByteArray line = file.read(4096);
		         textArea->insertPlainText(line);
		     }

		     file.close();
		}
		else
		{
			textArea->insertPlainText("Unknown file type: " + fileName);
		}

	    textArea->moveCursor(QTextCursor::Start);
	    textArea->ensureCursorVisible();
	}

    QVBoxLayout *layout=new QVBoxLayout(this);

	if (imageFound == true)
	{
		scrollArea = new QScrollArea;
		layout->addWidget(scrollArea);
		scrollArea->setBackgroundRole(QPalette::Dark);
	    scrollArea->setWidget(imageLabel);
	}
	else
		layout->addWidget(textArea);
}

void FileViewer::deleteFile()
{
	QFile::remove(fullFileName);
}

FileViewer::~ FileViewer( )
{
}
