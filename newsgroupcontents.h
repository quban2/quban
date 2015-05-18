#ifndef NEWSGROUPCONTENTS_H
#define NEWSGROUPCONTENTS_H

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
#include <QDialog>
#include <QList>
#include "common.h"
#include "newsgroup.h"
#include "ui_newsgroupcontents.h"

class NewsGroupContents : public QDialog, private Ui::NewsGroupContents
{
    Q_OBJECT

public:
    NewsGroupContents(NewsGroup *_ng, Servers *_servers, QWidget* parent = 0);
    virtual ~NewsGroupContents();

    void updateGroupStats(NewsGroup *ng, NntpHost *nh, uint lowWater, uint highWater, uint articleParts);
    void updateDetails(NewsGroup *_ng, Servers *servers, QWidget * parent);
    void refreshDetails();

private:

    QStringList entries;
    QWidget* parent;
    Servers *servers;
    NewsGroup *ng;

    QList <quint16> serverIdList;
    QList <quint16> delayedServerIdList;
	quint64 maxLocalLow;
	quint64 maxLocalHigh;
	quint64 maxLocalParts;

private:
    void buildDetails();
    void getHeaders(quint16 hostId);

public slots:
    void slotLimitsUpdate(NewsGroup *, NntpHost *, uint, uint, uint);
    void slotHeaderDownloadProgress(NewsGroup *, NntpHost *, quint64, quint64, quint64);
    void slotRefreshServers();

protected slots:
  /*$PROTECTED_SLOTS$*/
  virtual void          reject();

private slots:
	void slotGetClicked();
	void slotExpireClicked();
	void slotLocalServerClicked();
  	void slotGetHeaders(quint16, quint64, quint64);
  	void slotExpireHeaders(uint, uint);
  	void slotUpdateFinished(NewsGroup *);
    void slotMoveToLast();
    void slotMoveToNext();
    void slotMoveToDays();
    void slotMoveToDownloadDays();

  signals:

  	void sigGetHeaders(quint16, quint64, quint64);
  	void sigExpireHeaders(uint, uint);
  	void sigGetGroupLimits(NewsGroup *);
  	void sigGetGroupLimits(NewsGroup *, quint16, NntpHost *);
  	void sigGetHeaders(quint16, NewsGroup *, quint64, quint64);
  	void sigExpireNewsgroup(NewsGroup *, uint, uint);
};

#endif // NEWSGROUPCONTENTS_H
