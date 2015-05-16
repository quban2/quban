#ifndef HEADERQUEUE_H
#define HEADERQUEUE_H

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

#include <QMutex>
#include <QQueue>

template <class T> class HeaderQueue
{
    QMutex headerMutex;
    QQueue<T> headerData;

  public:

    explicit HeaderQueue() {}

    void enqueue(T hData)
    {
        headerMutex.lock();
        headerData.enqueue(hData);
        headerMutex.unlock();
    }

    T dequeue()
    {
        T ba = 0;

        headerMutex.lock();
        if (!headerData.isEmpty())
            ba = headerData.dequeue();
        headerMutex.unlock();

        return ba;
    }
};

#endif // HEADERQUEUE_H
