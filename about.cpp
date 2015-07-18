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

#include <QString>
#include <QSysInfo>
#include "zlib.h"
#include "about.h"

About::About(DbEnv* dbenv, QWidget *parent)
    : QDialog(parent)
{
	setupUi(this);

	QString libVers = "Berkeley db version = " +
			QString(dbenv->version((int*)0, (int*)0, (int*)0)) +
			"\nQt compile version = " + QT_VERSION_STR +
			"\nQt runtime version = " + qVersion() +
            "\nzlib version = " + ZLIB_VERSION
#if (QT_VERSION >= QT_VERSION_CHECK(5, 4, 0))
            "\nOperating System = " + QSysInfo::prettyProductName()
#endif
            ;
	this->detailText->append(libVers);
}

About::~About()
{
}
