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
#include <QMessageBox>
#include <QDateTime>
#include <QStringBuilder>
#include "MultiPartHeader.h"
#include "appConfig.h"
#include "JobList.h"
#include "bulkHeaderGroup.h"

BulkHeaderGroup::BulkHeaderGroup()
{
    m_cancel = false;
    running  = false;
    job = 0;
}

BulkHeaderGroup::~BulkHeaderGroup()
{
}

void BulkHeaderGroup::addBulkGroup(quint64 seq, NewsGroup* ng)
{
    BulkHeaderGroupDetails* j = new BulkHeaderGroupDetails;
    j->seq = seq;
    j->ng  = ng;

    jobList.append(j);

    if (!running)
    {
        running = true;
        BulkGroup();
    }
}

void BulkHeaderGroup::BulkGroup()
{
    keymem=new uchar[KEYMEM_SIZE];
    datamem=new uchar[DATAMEM_SIZE];

    key.set_flags(DB_DBT_USERMEM);
    key.set_data(keymem);
    key.set_ulen(KEYMEM_SIZE);

    data.set_flags(DB_DBT_USERMEM);
    data.set_ulen(DATAMEM_SIZE);
    data.set_data(datamem);

    while (!jobList.isEmpty())
    {
        job = jobList.first();

        if (BulkHeaderGroupBody())
        {
            qDebug() << "Bulk grouping successful for " << job->ng->getAlias();
            if (!jobList.isEmpty())
                jobList.removeAll(jobList.first());
            emit bulkGroupFinished(job->seq);
        }
        else if (m_cancel)
        {
            qDebug() << "Cancelled bulk grouping for " << job->ng->getAlias();
            if (!jobList.isEmpty())
                jobList.removeAll(jobList.first());
            m_cancel = false;
            emit bulkGroupCancelled(job->seq);
        }
        else
        {
            // Db error...
            qDebug() << "Bulk grouping failed for " << job->ng->getAlias();
            if (!jobList.isEmpty())
                jobList.removeAll(jobList.first());
            //set the error and exit...
            emit bulkGroupError(job->seq);
            break;
        }

        // 		qDebug("Updated, now the queue has %d items", jobList.count());
    }
    // 	qDebug("Update done");

    running = false;
    Q_DELETE_ARRAY(keymem);
    Q_DELETE_ARRAY(datamem);
}

bool BulkHeaderGroup::BulkHeaderGroupBody()
{
    // belt and braces
    key.set_data(keymem);
    data.set_data(datamem);

    if (m_cancel)
    {
        emit updateJob(JobList::BulkHeaderGroup, JobList::Cancelled, job->seq);
        return false;
    }

    emit updateJob(JobList::BulkHeaderGroup, JobList::Running, job->seq);

    NewsGroup* ng = job->ng;
    Db* db = ng->getDb();

    MultiPartHeader mph;
    SinglePartHeader sph;
    HeaderBase* hb = 0;
    HeaderGroup* headerGroup = 0;
    HeaderGroup* advancedHeaderGroup = 0;

    // typedef QMap<QString, QString> HeaderGroupIndexes; // subj, headerGroup index
    // typedef QMap<QString, HeaderGroup*> HeaderGroups; //  headerGroup index, headerGroup *

    HeaderGroupIndexes headerGroupIndexes;
    HeaderGroups       headerGroups;

    DBC *dbcp = 0;
    DBT ckey, cdata;

    memset(&ckey, 0, sizeof(ckey));
    memset(&cdata, 0, sizeof(cdata));

    size_t retklen, retdlen;
    void *retkey = 0, *retdata = 0;
    int ret, t_ret;
    void *p = 0;

    quint64 count=0;

    cdata.data = (void *) new char[HEADER_BULK_BUFFER_LENGTH];
    cdata.ulen = HEADER_BULK_BUFFER_LENGTH;
    cdata.flags = DB_DBT_USERMEM;

    ckey.data = (void *) new char[HEADER_BULK_BUFFER_LENGTH];
    ckey.ulen = HEADER_BULK_BUFFER_LENGTH;
    ckey.flags = DB_DBT_USERMEM;

    /* Acquire a cursor for the database. */
    if ((ret = db->get_DB()->cursor(db->get_DB(), NULL, &dbcp, DB_CURSOR_BULK)) != 0)
    {
        db->err(ret, "DB->cursor");
        char* ptr = 0;

        ptr = (char*)(ckey.data);
        Q_DELETE_ARRAY(ptr);
        ptr = (char*)(cdata.data);
        Q_DELETE_ARRAY(ptr);
        return false;
    }

    // To save the group records
    ng->articlesNeedDeleting(false);

    // Store the data in the database - flush first ...

    u_int32_t delCount;

    uchar keymem[KEYMEM_SIZE];
    uchar datamem[DATAMEM_SIZE];
    Dbt key, data;
    char* p2 = 0;
    QByteArray ba;
    const char *k;

    key.set_flags(DB_DBT_USERMEM);
    key.set_data(&keymem);
    key.set_ulen(KEYMEM_SIZE);

    data.set_flags(DB_DBT_USERMEM);
    data.set_ulen(DATAMEM_SIZE);
    data.set_data(&datamem);

    QString subj =  "MDQuban", from = "MDQuban";

    //QString rs1 = "^(.*)(\".*\")";
    //QString rs2 = "^(.*)\\s-\\s(.*)$";
    //QString rs3 = "^(\\S+.*)\\[.*\\].*(\".*\")";

    //QString rs3 = "^(.*)\\s-\\s.*\\s-\\s(.*)$";
    QRegExp rx[3];
    bool    rxPosBack[3];
    bool    noRegexpGrouping;

    QString recKey, storeIndex;
    QString prevSubj = "MDQuban", prevFrom = "MDQuban";

    int pos;
    bool newGroup = false;

    bool mphFound = false;

    quint32 grouped = 0,
            single = 0,
            numGroups = 0;

    qint16 stringDiff = -1;

    bool prevGroup = false;
    bool advancedPlacement = false;
    bool skipAdvanced = false;

    noRegexpGrouping = ng->isThereNoRegexOnGrouping();

    if (noRegexpGrouping == false) // need regex for grouping
    {
        rx[0].setPattern(ng->getGroupRE1());
        rx[1].setPattern(ng->getGroupRE2());
        rx[2].setPattern(ng->getGroupRE3());

        rxPosBack[0] = ng->getGroupRE1Back();
        rxPosBack[1] = ng->getGroupRE2Back();
        rxPosBack[2] = ng->getGroupRE3Back();
    }

    ng->getGroupingDb()->truncate(0, &delCount, 0);
    qDebug() << "Deleted " << delCount << " records from group db";

    QMapIterator<QString, QString> it(headerGroupIndexes);
    QString advancedIndex;

    for (;;)
    {
        /*
         * Acquire the next set of key/data pairs.  This code
         * does not handle single key/data pairs that won't fit
         * in a BUFFER_LENGTH size buffer, instead returning
         * DB_BUFFER_SMALL to our caller.
         */
        if ((ret = dbcp->get(dbcp, &ckey, &cdata, DB_MULTIPLE_KEY | DB_NEXT)) != 0)
        {
            if (ret != DB_NOTFOUND)
                db->err(ret, "DBcursor->get");
            break;
        }

        for (DB_MULTIPLE_INIT(p, &cdata);;)
        {
            DB_MULTIPLE_KEY_NEXT(p, &cdata, retkey, retklen, retdata, retdlen);
            if (p == NULL)
                break;

            if (retdlen){;} // MD TODO compiler .... unused variable

            recKey = QString::fromLocal8Bit((char*)retkey, retklen);

            if (*((char *)retdata) == 'm')
            {
                MultiPartHeader::getMultiPartHeader((unsigned int)retklen, (char *)retkey, (char *)retdata, &mph);
                hb = (HeaderBase*)&mph;
                mphFound = true;
            }
            else if (*((char *)retdata) == 's')
            {
                SinglePartHeader::getSinglePartHeader((unsigned int)retklen, (char *)retkey, (char *)retdata, &sph);
                hb = (HeaderBase*)&sph;
                mphFound = false;
            }
            else
            {
                // What have we found ?????
                qDebug() << "Found unexpected identifier for header : " << (char)*((char *)retdata);
                continue;
            }

            ++count;

            prevSubj = subj;

            prevFrom = from;

            subj = hb->getSubj();
            from = hb->getFrom();

            if (noRegexpGrouping == false) // need regex for grouping
            {
                for (int i=0; i<3; ++i)
                {
                    if (rx[i].isEmpty() == false)
                    {
                        if (rxPosBack[i] == true) // from the back
                        {
                            pos = subj.lastIndexOf(rx[i]);
                            if (pos != -1)
                                subj.truncate(pos);
                        }
                        else // from the front
                        {
                            pos = rx[i].indexIn(subj);
                            if (pos > -1)
                                subj = rx[i].cap(0);
                        }
                    }
                }
            }

            //qDebug() << "Stripped down to: " << subj;

            stringDiff = -1;

            if (prevFrom != from) // change of contributor
            {
                newGroup = true;
            }
            else // same contributor
            {
               if ((stringDiff = levenshteinDistance(prevSubj, subj)) > ng->getMatchDistance()) // no match ...
                   newGroup = true;
               else
                   newGroup = false;

               //qDebug() << "Diff between " << prevSubj << " and " << subj << " is " << stringDiff;
            }

            if (newGroup)
            {
                if (ng->isThereAdvancedGrouping())
                {
                    it.toFront();

                    // decide if we can match to a previous group
                    while (it.hasNext())
                    {
                        it.next();
                        if ((stringDiff = levenshteinDistance(it.key(), subj)) <= ng->getMatchDistance()) // match ...
                        {
                            // The index for this group is in it.value()
                            // See if we have the HeaderGroup in our cache headerGroups)

                            if (headerGroups.contains(it.value()))
                            {
                                advancedHeaderGroup = headerGroups.value(it.value());
                            }
                            else // not in cache
                            {
                                advancedIndex = it.value();
                                advancedHeaderGroup = getGroup(ng, advancedIndex);
                                if (advancedHeaderGroup)
                                {
                                    headerGroups.insert(advancedIndex, advancedHeaderGroup);
                                }
                                else // db read failed ..
                                {
                                    skipAdvanced = true;
                                }
                            }

                            if (skipAdvanced == false)
                            {
                                if (mphFound)
                                    advancedHeaderGroup->addMphKey(recKey);
                                else
                                    advancedHeaderGroup->addSphKey(recKey);

                                advancedPlacement = true;
                                subj = prevSubj; // ignore this header as it's been placed out of sequence
                                from = prevFrom;
                                newGroup = false; // as we managed to relocate to an existing group

                                break; // stop looking at previous groups
                            }
                            else
                                skipAdvanced = false;
                        }
                    }
                }
            }

            if (newGroup)
            {
                if (prevGroup) // save before moving on
                {
                    ba = storeIndex.toLocal8Bit();
                    k= ba.constData();
                    memcpy(keymem, k, storeIndex.length());
                    key.set_size(storeIndex.length());

                    p2=headerGroup->data();
                    data.set_data(p2);
                    data.set_size(headerGroup->getRecordSize());
                    ret=ng->getGroupingDb()->put(NULL, &key, &data, 0);
                    if (ret!=0)
                        qDebug("Error updating record: %d", ret);

                    if (ng->isThereAdvancedGrouping())
                        headerGroupIndexes.insert(storeIndex.section('\n', 0, 0), storeIndex);

                    Q_DELETE_ARRAY(p2);
                    Q_DELETE(headerGroup);
                    numGroups++;
                }

                prevGroup = true;

                storeIndex = subj % "\n" % from;

                headerGroup = new HeaderGroup();

                headerGroup->setDisplayName(subj);
                headerGroup->setPostingDate(hb->getPostingDate());
                headerGroup->setDownloadDate(hb->getDownloadDate());
                headerGroup->setStatus(hb->getStatus());
                headerGroup->setNextDistance(stringDiff);
            }

            // if we've found somewhere else to place this header then don't add again
            if (!advancedPlacement)
            {
                if (mphFound)
                    headerGroup->addMphKey(recKey);
                else
                    headerGroup->addSphKey(recKey);
            }
            else
                advancedPlacement = false;

            if (count % 250 == 0)
            {
                QCoreApplication::processEvents();

                emit updateJob(JobList::BulkHeaderGroup, tr("Header bulk grouping for newsgroup ") + job->ng->getAlias() + ": " +
                    QString::number(count) + " out of " + QString::number(ng->getTotal()) + tr(" grouped"), job->seq);
            }

            if (m_cancel)
            {
                emit updateJob(JobList::BulkHeaderGroup, JobList::Cancelled, job->seq);
                return false;
            }
        }

        if (m_cancel)
        {
            emit updateJob(JobList::BulkHeaderGroup, JobList::Cancelled, job->seq);
            return false;
        }
    }

    if ((t_ret = dbcp->close(dbcp)) != 0)
    {
        db->err(ret, "DBcursor->close");
        if (ret == 0)
            ret = t_ret;
    }

    char* ptr = ((char*)ckey.data);
    Q_DELETE_ARRAY(ptr);
    ptr = ((char*)cdata.data);
    Q_DELETE_ARRAY(ptr);
    if (headerGroups.count())
    {
        qDeleteAll(headerGroups);
        headerGroups.clear();
    }

    qDebug() << "Multi = " << grouped << ", single = " << single;

    ng->setHeadersNeedGrouping(false);
    // Finally update the newsgroup
    emit saveGroup(ng);

    emit updateJob(JobList::BulkHeaderGroup, tr("Header bulk grouping for newsgroup ") + job->ng->getAlias() + ": " +
                  QString::number(count) + " out of " + QString::number(ng->getTotal()) + tr(" grouped"), job->seq);

    if (m_cancel)
    {
        emit updateJob(JobList::BulkHeaderGroup, JobList::Cancelled, job->seq);
        return false;
    }

    emit logEvent(tr("Bulk grouping of ") + ng->getTotal() + tr(" articles completed successfully."));

    emit updateJob(JobList::BulkHeaderGroup, JobList::Finished_Ok, job->seq);

    ng->setTotalGroups(numGroups);

    Q_DELETE(headerGroup);

    return true;
}

HeaderGroup* BulkHeaderGroup::getGroup(NewsGroup* ng, QString& articleIndex)
{
    HeaderGroup *hg = 0;

    int ret;
    Dbt groupkey;
    Dbt groupdata;
    memset(&groupkey, 0, sizeof(groupkey));
    memset(&groupdata, 0, sizeof(groupdata));
    groupdata.set_flags(DB_DBT_MALLOC);

    QByteArray ba = articleIndex.toLocal8Bit();
    const char *k= ba.constData();
    groupkey.set_data((void*)k);
    groupkey.set_size(articleIndex.length());

    Db* groupsDb = ng->getGroupingDb();
    ret=groupsDb->get(NULL, &groupkey, &groupdata, 0);
    if (ret != 0) //key not found
    {
        qDebug() << "Failed to find group with key " << articleIndex;
    }
    else
    {
        qDebug() << "Found group with key " << articleIndex;

        hg=new HeaderGroup(articleIndex.length(), (char*)k, (char*)groupdata.get_data());
        void* ptr = groupdata.get_data();
        Q_FREE(ptr);
    }

    return hg;
}

void BulkHeaderGroup::cancel(quint64 seq)
{
    if (job->seq == seq)
    {
        m_cancel = true;
    }
    else
    {
        QLinkedList<BulkHeaderGroupDetails*>::iterator it;
        for (it = jobList.begin(); it != jobList.end(); ++it)
        {
            if ((*it)->seq == seq)
            {
                emit updateJob(JobList::BulkHeaderGroup, JobList::Cancelled, seq);
                //emit BulkHeaderGroupCancelled(*it);
                jobList.erase(it);
                break;
            }
        }
    }

    return;
}

void BulkHeaderGroup::shutdown()
{
    QLinkedList<BulkHeaderGroupDetails*>::iterator it;
    for (it = jobList.begin(); it != jobList.end(); ++it)
        jobList.erase(it);

    m_cancel = true;
}

qint16 BulkHeaderGroup::levenshteinDistance(const QString& a_compare1, const QString& a_compare2)
{
    const qint16 length1 = a_compare1.size();
    const qint16 length2 = a_compare2.size();
    QVector<qint16> curr_col(length2 + 1);
    QVector<qint16> prev_col(length2 + 1);

    // Prime the previous column for use in the following loop:
    for (qint16 idx2 = 0; idx2 < length2 + 1; ++idx2)
    {
        prev_col[idx2] = idx2;
    }

    for (qint16 idx1 = 0; idx1 < length1; ++idx1)
    {
        curr_col[0] = idx1 + 1;

        for (qint16 idx2 = 0; idx2 < length2; ++idx2)
        {
            const qint16 compare = a_compare1[idx1] == a_compare2[idx2] ? 0 : 1;

            curr_col[idx2 + 1] = qMin(qMin(curr_col[idx2] + 1, prev_col[idx2 + 1] + 1),
                    prev_col[idx2] + compare);
        }

#if defined(Q_OS_OS2) || defined(Q_OS_OS2EMX) // In qglobal.h
    QVector<qint16> swap_col(length2 + 1);

    swap_col = prev_col;
    prev_col = curr_col;
    curr_col = swap_col;
#else
     curr_col.swap(prev_col);
#endif
    }

    return prev_col[length2];
}



