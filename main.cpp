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

#include <stdlib.h>
#include <QtGui>
#include <QApplication>
#include <QDebug>
#include <QtDebug>
#include "appConfig.h"
#include "prefstartup.h"
#include "quban.h"

// need to make this global to make sure that we can delete it
DbEnv *g_dbenv;

Quban* quban;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    g_dbenv=0;

    Configuration* config = Configuration::getConfig();

    // Need the db location before we display the main window !!!
    if (!config->configured)
    {
    	(new PrefStartup(0))->show();
    }
    else
    {
        quban = new Quban;
        quban->startup();
        quban->show();
    }

    int ret = a.exec();

	if (g_dbenv)
	{
		g_dbenv->close(0);
	}

    return ret;
}
