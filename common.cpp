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

#include <QDir>
#include <QMessageBox>
#include <QObject>
#include "common.h"

//recursive

bool mkDeepDir( QString dirName )
{
	QDir d;

	QString newDir = dirName.mid(0, dirName.lastIndexOf('/'));

// 	qDebug("Dir: %s", newDir.latin1());

	if (d.exists(newDir))
		return true;

	if (mkDeepDir(newDir))
	{
// 		qDebug("create dir: %s", (const char *) newDir);
		if (d.mkdir(newDir))
			return true;
		else
			return false;
	}
	else
		return false;
}

bool checkAndCreateDir( QString dirName )
{
	QString dir = QDir::cleanPath(dirName);
	dir+='/';
	if (QDir::isRelativePath(dir) )
	{
		QMessageBox::warning ( 0, "Invalid configuration", "Please enter only absolute paths");
		return false;
	}

	if (!mkDeepDir(dir))
		return false;
	else
	{
	    //check if the dir is writable
		QFileInfo fi(dir);

	    if (fi.isWritable())
		    return true;
	    else
		    return false;
	}
}

char* insert(QString s, char* p)
{
	quint16 strlen = s.length();
	quint16 suint = sizeof(strlen);
	memcpy(p, &strlen, suint);

	p += suint;
	QByteArray ba = s.toLocal8Bit();
	const char *c_str = ba.constData();
	memcpy(p, c_str, strlen);
	p += strlen;
	return p;
}

char* retrieve(char* i, QString &s)
{
	quint16 strlen;
	quint16 ssize = sizeof(strlen);

	memcpy(&strlen, i, ssize);
	i += ssize;

    s = QString::fromLatin1(i, strlen);
	i += strlen;
	return i;
}
