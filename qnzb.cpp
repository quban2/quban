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
#include "qnzb.h"
#include <QMessageBox>
#include <QFile>
#include <QDebug>
#include <QtDebug>
#include <QStringBuilder>
#include <QFileDialog>
#include <QDomDocument>
#include <QtAlgorithms>
#include "header.h"
#include "nzbform.h"

QNzb::QNzb(QMgr* _qmgr, Configuration* _config):
    config(_config), qmgr(_qmgr)
{
}

QNzb::~QNzb()
{
}

void QNzb::slotGroupFullNzb()
{
	Dbt key, data;
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	QByteArray ba;
	const char *c_str;
	QString sindex;

	QString nzbFilename =
			QFileDialog::getOpenFileName(this, tr("Open a .nzb file"),
					config->nzbDir, tr("Newzbin Files (*.nzb)"));
	qDebug() << "Opened " << nzbFilename;

	if (nzbFilename.isNull())
		return;

	//Parse nzb and add posts to the auto group download
	//Code taken (and adapted with permission) from knzb, (C) 2004 David Pye.

	QFile nzbFile(nzbFilename);
	if (!nzbFile.open(QIODevice::ReadOnly))
	{
		QMessageBox::warning(this, nzbFilename, tr("Failed to open nzb file ..."));
		return;
	}

	qmgr->nzbGroupId = ++(qmgr->maxGroupId);

	GroupManager* groupManager = new GroupManager(qmgr->unpackDb, qmgr->nzbGroup, qmgr->nzbGroupId);
	qmgr->groupedDownloads.insert(qmgr->nzbGroupId, groupManager);
	groupManager->dbSave();

	qmgr->unpackConfirmation = new UnpackConfirmation;
	qmgr->unpackConfirmation->groupId = qmgr->nzbGroupId;
	qmgr->unpackConfirmation->ng = qmgr->nzbGroup;
	qmgr->unpackConfirmation->first = false;
	QFileInfo fi(nzbFilename);
	qmgr->unpackConfirmation->dDir = qmgr->nzbGroup->getSaveDir() + fi.completeBaseName();

	if (!checkAndCreateDir(qmgr->unpackConfirmation->dDir))
	{
		QMessageBox::warning ( this, qmgr->unpackConfirmation->dDir, tr("Could not create directory"));
		return;
	}

	int i = 0;

	QDomDocument doc("nzbfile");
	MultiPartHeader *mph = NULL;
	doc.setContent(&nzbFile);
	nzbFile.close();
	QFileInfo qf(nzbFilename);
	QRegExp rx("\\((\\d+)/(\\d+)\\)");
	QDomElement docElem = doc.documentElement();
	QDomNode n = docElem.firstChild();
	int ret;
	quint64 mps;
	Header* h;
	char *phead;
	Dbt pkey;
	Dbt pdata;

	while (!n.isNull())
	{
		QDomElement e = n.toElement();
		if (!e.isNull())
		{
			if (e.tagName().compare("file") == 0)
			{
				//Create a MultiPartHeader
				i++;
				mph = new MultiPartHeader();
				QString subject = e.attribute("subject");
				int pos = 0;
				int capTotal = 0;
				int index = -1; // = rx.search(h->m_subj, -1);
				while ((pos = rx.indexIn(subject, pos)) != -1)
				{
					index = pos;
					pos += rx.matchedLength();
					capTotal = rx.cap(2).toInt();
				}
				subject = subject.left(index).simplified();
				mph->setSubj(subject);

				mph->setFrom(e.attribute("poster"));
				QDateTime qdt;
				qdt.setTime_t(e.attribute("date").toUInt());
				mph->setPostingDate(qdt.toTime_t());
				mph->setDownloadDate(QDateTime::currentDateTime().toTime_t());
                mph->setStatus(MultiPartHeader::bh_new);
				mph->setNumParts(capTotal);
				mps = qmgr->nzbGroup->getMultiPartSequence() + 1;
				mph->setKey(mps);
				qmgr->nzbGroup->setMultiPartSequence(mps);

				QDomNode fileSubNode = e.firstChild();
				while (!fileSubNode.isNull())
				{
					QDomElement fileElement = fileSubNode.toElement();
					if (!fileElement.isNull())
					{
						if (fileElement.tagName() == "segments")
						{
							QDomNode segmentsSubNode = fileSubNode.firstChild();
							while (!segmentsSubNode.isNull())
							{
								QDomElement segmentsElement =
										segmentsSubNode.toElement();
								if (!segmentsElement.isNull())
								{
									if (segmentsElement.tagName() == "segment")
									{
										int bytes = segmentsElement.attribute(
												"bytes").toInt();
										int part = segmentsElement.attribute(
												"number").toInt();
										QString mid = segmentsElement.text();


										h = new Header(mps, part, bytes);
										h->setMessageId("<" + mid + ">");
										mph->increaseSize(bytes);

										Servers::Iterator sit;
										for (sit = qmgr->servers->begin(); sit
												!= qmgr->servers->end(); ++sit)
										{
											if (sit.value()->getServerType()!= NntpHost::dormantServer)
												h->setServerPartNum((quint16)(sit.key()), (quint64)part);
										}

										//save in the parts db : will not be omitted, so safe to save to db straight away
										pkey.set_data(&mps);
										pkey.set_size(sizeof(quint64));

										phead=h->data();

										pdata.set_data(phead);
										pdata.set_size(h->getRecordSize());

										ret = qmgr->nzbGroup->getPartsDb()->put(0, &pkey, &pdata, 0);
										if (ret)
										{
											qDebug() << "Unable to create part " << part << " for key " << mps << ", result = " << ret;
										}
										else
											mph->missingParts = mph->missingParts - 1;

										delete [] phead;
										delete h;
									}
								}
								segmentsSubNode = segmentsSubNode.nextSibling();
							}
						}
					}

					fileSubNode = fileSubNode.nextSibling();
				}

				qmgr->unpackConfirmation->headers.append(new ConfirmationHeader(mph));

				sindex = mph->getSubj().simplified() % '\n' % mph->getFrom();
				qDebug() << "Saving " << index;

				ba = sindex.toLocal8Bit();
				c_str = ba.constData();
				key.set_data((void*) c_str);
				key.set_size(sindex.length());
				data.set_data(mph->data());
				data.set_size(mph->getRecordSize());
				int ret;
				if ((ret = qmgr->nzbGroup->getDb()->put(0, &key, &data, 0)) != 0)
					qDebug() << "Error inserting binheader into nzbGroup: "
							<< qmgr->dbEnv->strerror(ret);

                void* ptr = data.get_data();
                Q_FREE(ptr);
			}
		}
		n = n.nextSibling();
	}

	qmgr->nzbGroupId = 0;

	qDebug() << "There are " << qmgr->unpackConfirmation->headers.count() << " headers to display";
	qmgr->completeUnpackDetails(qmgr->unpackConfirmation, nzbFilename);
}

bool nzbHeaderLessThan(const NzbHeader* s1, const NzbHeader* s2)
{
    return s1->subj < s2->subj;
}

void QNzb::slotOpenNzb()
{
	QString nzbFilename =
			QFileDialog::getOpenFileName(this, tr("Open a .nzb file"),
					config->nzbDir, tr("Newzbin Files (*.nzb)"));
	qDebug() << "Opened " << nzbFilename;

	if (nzbFilename.isNull())
		return;

	OpenNzbFile(nzbFilename);
}

void QNzb::OpenNzbFile(QString nzbFilename)
{
	//Parse nzb and add posts to the queue
	//Code taken (and adapted with permission) from knzb, (C) 2004 David Pye.

	QFile nzbFile(nzbFilename);
	if (!nzbFile.open(QIODevice::ReadOnly))
	{
		QMessageBox::warning(this, nzbFilename,
				tr("Failed to open nzb file ..."));
		return;
	}
	QList<NzbHeader*> headerList;
	int i = 0;

	QDomDocument doc("nzbfile");
	NzbHeader *nh = NULL;
	doc.setContent(&nzbFile);
	nzbFile.close();
	QFileInfo qf(nzbFilename);
	QRegExp rx("\\((\\d+)/(\\d+)\\)");
	QDomElement docElem = doc.documentElement();
	QDomNode n = docElem.firstChild();
	quint64 mps;
	Header* h;

    QString groupName;
    QString category;
    QString type;

	while (!n.isNull())
	{
		QDomElement e = n.toElement();
		if (!e.isNull())
        {
			if (e.tagName().compare("file") == 0)
			{
				//Create a NzbHeader
				i++;
				nh = new NzbHeader(qmgr->nzbGroup->getPartsDb());
				QString subject = e.attribute("subject");
				int pos = 0;
				int capTotal = 0;
				int index = -1; // = rx.search(h->m_subj, -1);
				while ((pos = rx.indexIn(subject, pos)) != -1)
				{
					index = pos;
					pos += rx.matchedLength();
					capTotal = rx.cap(2).toInt();
				}
				subject = subject.left(index).simplified();
				nh->setSubj(subject);
				// qDebug() << "Setting subject to " << subject;

				nh->setFrom(e.attribute("poster"));
				QDateTime qdt;
				qdt.setTime_t(e.attribute("date").toUInt());
				nh->setPostingDate(qdt.toTime_t());
				nh->setDownloadDate(QDateTime::currentDateTime().toTime_t());
                nh->setStatus(HeaderBase::bh_new);
				nh->setNumParts(capTotal);
				mps = qmgr->nzbGroup->getMultiPartSequence() + 1;
				nh->setKey(mps);
				qmgr->nzbGroup->setMultiPartSequence(mps);

				QDomNode fileSubNode = e.firstChild();
				while (!fileSubNode.isNull())
				{
					QDomElement fileElement = fileSubNode.toElement();
					if (!fileElement.isNull())
					{
						if (fileElement.tagName() == "segments")
						{
							QDomNode segmentsSubNode = fileSubNode.firstChild();
							while (!segmentsSubNode.isNull())
							{
								QDomElement segmentsElement =
										segmentsSubNode.toElement();
								if (!segmentsElement.isNull())
								{
									if (segmentsElement.tagName() == "segment")
									{
										int bytes = segmentsElement.attribute(
												"bytes").toInt();
										int part = segmentsElement.attribute(
												"number").toInt();
										QString mid = segmentsElement.text();

										h = new Header(mps, part, bytes);
										h->setMessageId("<" + mid + ">");
										nh->increaseSize(bytes);

										Servers::Iterator sit;
										for (sit = qmgr->servers->begin(); sit
												!= qmgr->servers->end(); ++sit)
										{
                                            if (sit.value() && sit.value()->getServerType()!= NntpHost::dormantServer)
												h->setServerPartNum((quint16)(sit.key()), (quint64)part);
										}

										//save in the Map
										nh->addNzbPart(part, h);
									}
								}
								segmentsSubNode = segmentsSubNode.nextSibling();
							}
						}
					}

					fileSubNode = fileSubNode.nextSibling();
				}

				headerList.append(nh);
				if (!nh)
					qDebug() << "Found a duff NzbHeader";

			}
            else if (e.tagName().compare("head") == 0)
            {
                QDomNode metaSubNode = e.firstChild();
                while (!metaSubNode.isNull())
                {
                    QDomElement metaElement = metaSubNode.toElement();
                    if (!metaElement.isNull())
                    {
                        if (metaElement.tagName() == "meta")
                        {
                            type = metaElement.attribute("type");
                            if (!type.isNull())
                            {
                                if (type == "category")
                                {
                                    qDebug() << "Category " << metaElement.text();
                                    category = metaElement.text();
                                }
                                else if (type == "name")
                                {
                                    qDebug() << "Name " << metaElement.text();
                                    groupName = metaElement.text();
                                }
                            }
                        }
                    }
                    metaSubNode = metaSubNode.nextSibling();
                }
            }
		}
		n = n.nextSibling();
	}
	QFileInfo fi(nzbFilename);

	// qDebug() << "Got " << headerList.count() << " headers";

	qSort(headerList.begin(), headerList.end(), nzbHeaderLessThan);

	QByteArray ba = nzbFilename.toLocal8Bit();
	const char *c_nzbFilename = ba.constData();
	NzbForm *nzbForm =
            new NzbForm(&headerList, qmgr->nzbGroup->getSaveDir() + fi.completeBaseName(), groupName, category, this, c_nzbFilename);

	connect(nzbForm,
			SIGNAL(sigDownloadNzbPost(NzbHeader*, bool, bool, QString)), this,
			SLOT(slotAddNzbItem(NzbHeader*, bool, bool, QString)));
	connect(nzbForm, SIGNAL(sigFinishedNzbPosts(bool, QString)), qmgr, SLOT(slotAdjustQueueWidth()));
	connect(nzbForm, SIGNAL(sigFinishedNzbPosts(bool, QString)), this, SLOT(slotNzbAddFinished(bool, QString)));
	nzbForm->exec();

	qmgr->nzbGroup->getPartsDb()->sync(0);
	qmgr->nzbGroup->getDb()->sync(0);
/*
 * TODO - fix this, it may be partly cleared in nzbform.cpp (Cancel etc)
 *        and in other areas where the nh is passed on to ConfirmationHeader (see below), freed on cancel,
 *        and then in qmgr.cpp to PendingHeader where it uses the nh pointer!!! Also sent to QMgr::slotAddPostItem()
 *        that's used by header downloads as well ...
 *
  	qDeleteAll(headerList);
*/
	headerList.clear();
}

void QNzb::slotAddNzbItem(NzbHeader* nh, bool groupItems, bool first, QString dDir)
{
	Dbt key, data;
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	QString index = nh->getSubj().simplified() % '\n' % nh->getFrom();
	qDebug() << "Saving " << index;

	QByteArray ba = index.toLocal8Bit();
	const char *c_str = ba.constData();
	key.set_data((void*) c_str);
	key.set_size(index.length());
	data.set_data(nh->data());
	data.set_size(nh->getRecordSize());
	int ret;
	if ((ret = qmgr->nzbGroup->getDb()->put(0, &key, &data, 0)) != 0)
		qDebug() << "Error inserting nzb header into nzbGroup: "
				<< qmgr->dbEnv->strerror(ret);

    void* ptr = data.get_data();
    Q_FREE(ptr);

	if ((ret = nh->saveNzbParts()) != 0)
			qDebug() << "Error inserting nzb parts into nzbGroup: "
					<< qmgr->dbEnv->strerror(ret);

	if (groupItems)
	{
		if (qmgr->nzbGroupId == 0) // need to allocate group
		{
			qmgr->nzbGroupId = ++(qmgr->maxGroupId);

			GroupManager* groupManager = new GroupManager(qmgr->unpackDb, qmgr->nzbGroup, qmgr->nzbGroupId);
			qmgr->groupedDownloads.insert(qmgr->nzbGroupId, groupManager);
			groupManager->dbSave();

			qmgr->unpackConfirmation = new UnpackConfirmation;
			qmgr->unpackConfirmation->groupId = qmgr->nzbGroupId;
			qmgr->unpackConfirmation->ng = qmgr->nzbGroup;
			qmgr->unpackConfirmation->first = first;
			qmgr->unpackConfirmation->dDir = dDir;
		}

		qmgr->unpackConfirmation->headers.append(new ConfirmationHeader(nh));
	}
	else
	{
		qmgr->slotAddPostItem(nh, qmgr->nzbGroup, first, false, dDir, 0);
	}
}

void QNzb::slotNzbAddFinished(bool groupItems, QString fullNzbFilename)
{
	if (groupItems && qmgr->unpackConfirmation)
	{
		qmgr->nzbGroupId = 0;

		qDebug() << "There are " << qmgr->unpackConfirmation->headers.count()
				<< " headers to display";

		if (qmgr->unpackConfirmation->headers.count())
			qmgr->completeUnpackDetails(qmgr->unpackConfirmation, fullNzbFilename);
	}
	else
	{
		if (qmgr->droppedNzbFiles.size() > 0)
		{
			qmgr->processNextDroppedFile();
		}

	    if (config->renameNzbFiles == true)
	    {
		    QFile::rename(fullNzbFilename, fullNzbFilename + tr(NZB_QUEUED_SUFFIX));
	    }
	}
}
