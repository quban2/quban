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
#include "headerGroup.h"

HeaderGroup::HeaderGroup(QObject *parent) :
    QObject(parent)
{
}

HeaderGroup::HeaderGroup(quint32 keySize, char* k, char *p)
{
    char *pi =p;

    char* splitpoint = (char *)memchr(k, '\n', keySize);

    if (splitpoint)
    {
        match = QString::fromLocal8Bit(k, (splitpoint - k));
        from = QString::fromLocal8Bit(splitpoint + 1, k + (keySize - 1) - splitpoint);
    }
    else
    {
        match = QString::fromLocal8Bit(k, (keySize - 1));
        from = match;
    }

    quint16 sz16 = sizeof(quint16);
    quint16 sz32 = sizeof(quint32);

    memcpy(&postingDate, pi, sz32);
    pi +=sz32;

    memcpy(&downloadDate, pi, sz32);
    pi +=sz32;

    memcpy(&status, pi, sz16);
    pi+=sz16;

    memcpy(&nextDistance, pi, sz16);
    pi+=sz16;

    pi = retrieve(pi, displayName);

    quint16 numKeys;
    memcpy(&numKeys, pi, sz16);
    pi +=sz16;

    QString key;
    for (quint16 i = 0; i < numKeys; i++)
    {
        pi = retrieve(pi, key);
        mphKeys.append(key);
    }

    memcpy(&numKeys, pi, sz16);
    pi +=sz16;

    for (quint16 i = 0; i < numKeys; i++)
    {
        pi = retrieve(pi, key);
        sphKeys.append(key);
    }
}

uint HeaderGroup::getRecordSize()
{
    quint32 hgSize=0;
    quint16 sz16 = sizeof(quint16);
    quint16 sz32 = sizeof(quint32);

    hgSize = sz16 * 5 + // the two list counts + displayName size + status + matchType
             sz32 * 2 + // the two dates
             displayName.length();

    for (quint16 i=0; i < mphKeys.size(); i++)
    {
        hgSize += sz16; // the string size
        hgSize += mphKeys.at(i).length();
    }

    for (quint16 i=0; i < sphKeys.size(); i++)
    {
        hgSize += sz16; // the string size
        hgSize += sphKeys.at(i).length();
    }

    return hgSize;
}

char *HeaderGroup::data()
{
    char *p=new char[getRecordSize()];
    char *i = p;

    quint16 sz16 = sizeof(quint16);
    quint16 sz32 = sizeof(quint32);

    memcpy(i, &postingDate, sz32);
    i +=sz32;

    memcpy(i, &downloadDate, sz32);
    i +=sz32;

    memcpy(i, &status, sz16);
    i +=sz16;

    memcpy(i, &nextDistance, sz16);
    i +=sz16;

    i=insert(displayName, i);

    quint16 numKeys = mphKeys.size();
    memcpy(i, &numKeys, sz16);
    i +=sz16;

    for (quint16 j=0; j < mphKeys.size(); j++)
    {
        i=insert(mphKeys.at(j), i);
    }

    numKeys = sphKeys.size();
    memcpy(i, &numKeys, sz16);
    i +=sz16;

    for (quint16 j=0; j < sphKeys.size(); j++)
    {
        i=insert(sphKeys.at(j), i);
    }

    return p;
}

bool HeaderGroup::getHeaderGroup(unsigned int keySize, char *k, char *p, HeaderGroup* hg)
{
    char *pi=p;
    quint16 sz16 = sizeof(quint16);
    quint16 sz32 = sizeof(quint32);

    char* splitpoint = (char *)memchr(k, '\n', keySize);
    if (!splitpoint)
        return false;

    hg->match = QString::fromLocal8Bit(k, (splitpoint - k));
    hg->from = QString::fromLocal8Bit(splitpoint + 1, k + (keySize - 1) - splitpoint);

    memcpy(&hg->postingDate, pi, sz32);
    pi +=sz32;

    memcpy(&hg->downloadDate, pi, sz32);
    pi +=sz32;

    memcpy(&hg->status, pi, sz16);
    pi+=sz16;

    memcpy(&hg->nextDistance, pi, sz16);
    pi+=sz16;

    pi = retrieve(pi, hg->displayName);

    quint16 numKeys;
    memcpy(&numKeys, pi, sz16);
    pi +=sz16;

    hg->mphKeys.clear();
    hg->sphKeys.clear();

    QString key;
    for (quint16 i = 0; i < numKeys; i++)
    {
        pi = retrieve(pi, key);
        hg->mphKeys.append(key);
    }

    memcpy(&numKeys, pi, sz16);
    pi +=sz16;

    for (quint16 i = 0; i < numKeys; i++)
    {
        pi = retrieve(pi, key);
        hg->sphKeys.append(key);
    }

     return true;
}
