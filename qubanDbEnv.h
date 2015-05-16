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

#ifndef QUBANDBENV_H_
#define QUBANDBENV_H_

#include <QObject>
#include <db_cxx.h>
#include "appConfig.h"
#include "common.h"

class QubanDbEnv : public QObject
{
    Q_OBJECT

public:
	QubanDbEnv(Configuration*);
	virtual ~QubanDbEnv();

	bool dbsExists();
	void createDbs();
	void saveVersionData(Db *versionDb);
    void migrateDbs();

    Db* openDb(const char*, bool, int*);
    Db* openDb(const char*, const char*, bool, int*);

private:

	QString dbDir;
	Configuration* config;

	quint32 groupDbVersion;
	quint32 headerDbVersion;
	quint32 availableGroupsDbVersion;
	quint32 serverDbVersion;
	quint32 nzbDbVersion;
	quint32 queueDbVersion;
	quint32 unpackDbVersion;
	quint32 schedulerDbVersion;
	quint32 versionDbVersion;

public slots:
	void slotCompactDbs();

signals:
	void sigCompact();
};

#endif /* QUBANDBENV_H_ */
