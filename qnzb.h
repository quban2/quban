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

#ifndef QNZB_H
#define QNZB_H

#include "qmgr.h"

class Configuration;

class QNzb : public QWidget
{
  Q_OBJECT

	Configuration* config;
     QMgr* qmgr;

public:
  QNzb(QMgr* _qmgr, Configuration* _config);
  virtual ~QNzb();
  void OpenNzbFile(QString);

public slots:
  /*$PUBLIC_SLOTS$*/

	void slotAddNzbItem(NzbHeader* nh, bool groupItems, bool first = 0, QString dDir=QString::null);
	void slotNzbAddFinished(bool groupItems, QString fullNzbFilename);
	void slotOpenNzb();
	void slotGroupFullNzb();
};

bool nzbHeaderLessThan(const NzbHeader* s1, const NzbHeader* s2);

#endif
