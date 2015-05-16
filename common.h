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

#ifndef COMMON_H_
#define COMMON_H_

#include <QString>
#include "logAlertList.h"
#include "logEventList.h"

#define BDB_PAGE_SIZE 32768
#define BDB_PAGE_SIZE_NEW 8192
#define CACHE_SIZE 33554432
#define KEYMEM_SIZE 10000
#define DATAMEM_SIZE 16000

#define HEADER_BULK_BUFFER_LENGTH (3 * 1024 * 1024)

#define GROUPDB_VERSION 9
#define HEADERDB_VERSION 9
#define AVAILABLEGROUPSDB_VERSION 2
#define SERVERDB_VERSION 3
#define QUEUEDB_VERSION 3
#define UNPACKDB_VERSION 3
#define SCHEDULERDB_VERSION 2
#define VERSIONDB_VERSION 3
#define PARTDB_VERSION 1
#define SINGLEPARTDB_VERSION 1
#define HEADERGROUP_VERSION 1

bool mkDeepDir( QString dirName );
bool checkAndCreateDir( QString dirName );

char* insert(QString s, char* p);
char* retrieve(char* i, QString &s);

typedef struct
{
    enum { Subj_Col=0, Parts_Col=1, KBytes_Col=2, From_Col=3, Date_Col=4, Download_Col=5, No_Col=10 };
    enum { CONTAINS = 1, EQUALS, MORE_THAN, LESS_THAN, READ, NOT_READ };
}
CommonDefs;

#endif /* COMMON_H_ */
