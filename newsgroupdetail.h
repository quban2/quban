#ifndef NEWSGROUPDETAIL_H
#define NEWSGROUPDETAIL_H

/***************************************************************************
     Copyright (C) 2011 by Martin Demet
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

#include <QtGui/QWidget>
#include <QList>
#include <QStringList>
#include "common.h"
#include "newsgroup.h"
#include "ui_newsgroupdetail.h"

class Group;

class NewsGroupDetail : public QWidget, public Ui::NewsGroupDetail
{
    Q_OBJECT

public:
    NewsGroupDetail(QString ngName, QString saveDir, QStringList _cats, Group *_g, QWidget* parent = 0);
    NewsGroupDetail(NewsGroup *_ng, Servers *_servers, QStringList _cats, QWidget* parent = 0);
    virtual ~NewsGroupDetail();

    void updateGroupStats(NewsGroup *ng, NntpHost *nh, uint lowWater, uint highWater, uint articleParts);
    void updateDetails(NewsGroup *_ng, Servers *servers, QWidget * parent);
    void refreshDetails();

    QStringList entries;
    QWidget* parent;
    QStringList cats;
    Servers *servers;
    NewsGroup *ng;
    Group *g;

    QList <int> serverIdList;

private:
    void buildDetails();

private slots:
	void slotGetClicked();
	void slotExpireClicked();

  signals:
  	void newGroup(QStringList, Group*);
  	void resetServerInGroup(NewsGroup *, int);
  	void saveGroup(NewsGroup*);
  	void addCat(QString);
  	void sigGetHeaders(uint, uint);
  	void sigExpireHeaders(uint, uint);
};

#endif // NEWSGROUPDETAIL_H
