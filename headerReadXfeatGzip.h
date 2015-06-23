#ifndef HEADERREADXFEATGZIP_H
#define HEADERREADXFEATGZIP_H

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

#include "headerReadWorker.h"

class HeaderReadXFeatGzip : public HeaderReadWorker
{
    Q_OBJECT

public:
    HeaderReadXFeatGzip(HeaderQueue<QByteArray*>* _headerQueue, Job* _job, uint _articles, qint16 _hostId, QMutex *_headerDbLock);
    ~HeaderReadXFeatGzip();

private:
    int inf(uchar *source, uint bufferIndex, char *dest, char **destEnd);

    char* inflatedBuffer;
    char* tempInflatedBuffer;
    char* destEnd;

public slots:
    void startHeaderRead();
};

#endif // HEADERREADXFEATGZIP_H
