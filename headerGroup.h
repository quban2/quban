#ifndef HEADERGROUP_H
#define HEADERGROUP_H

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
#include <QList>
#include <QObject>
#include "common.h"

class HeaderGroup : public QObject
{
    Q_OBJECT
public:
    explicit HeaderGroup(QObject *parent = 0);

    HeaderGroup(quint32, char*, char*);

    char *data();
    uint getRecordSize();

    QString&       getMatch() { return match; }
    QString&       getFrom() { return from; }
    QList<QString>& getMphKeys() { return mphKeys; }
    QList<QString>& getSphKeys() { return sphKeys; }
    quint32        getMphKeyCount() { return mphKeys.count(); }
    quint32        getSphKeyCount() { return sphKeys.count(); }
    QString&       getDisplayName() { return displayName; }
    quint32        getPostingDate() { return postingDate; }
    quint32        getDownloadDate() { return downloadDate; }
    quint16        getStatus() { return status; }
    qint16         getNextDistance() { return nextDistance; }

    void           addMphKey(QString key) { mphKeys.append(key); }
    void           addSphKey(QString key) { sphKeys.append(key); }
    void           setDisplayName(QString d) { displayName = d; }
    void           setPostingDate(quint32 d) { postingDate = d; }
    void           setDownloadDate(quint32 d) { downloadDate = d; }
    void           setStatus(quint16 s) { status = s; }
    void           setNextDistance(qint16 m) { nextDistance = m; }

    static bool getHeaderGroup(unsigned int, char *, char *, HeaderGroup*);

    enum { RE1 = 1 };

private:
    QList<QString> mphKeys;
    QList<QString> sphKeys;
    QString displayName;
    quint32 postingDate;
    quint32 downloadDate;
    quint16 status;
    qint16 nextDistance;

    QString match; // these two are built from the key
    QString from;
    
signals:
    
public slots:
    
};

#endif // HEADERGROUP_H
