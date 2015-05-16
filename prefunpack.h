#ifndef PREFUNPACK_H
#define PREFUNPACK_H
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

#include <QtGui/QWidget>
#include <QList>
#include "ui_prefunpack.h"
#include "prefunpackexternal.h"

class Configuration;

class PrefUnpack : public QWidget, public Ui::PrefUnpackClass
{
    Q_OBJECT

public:
    PrefUnpack(QWidget *parent = 0);
    virtual ~PrefUnpack();

protected slots:
	void slotEnableConfig();

public slots:
	void numRepairersChanged(int);
	void numUnpackersChanged(int);
	void locationSearch();
	void removeExternalApp();
	void checkAppValidity();
	void checkAppValidity(PrefUnpackExternal*);

public:
    QList<PrefUnpackExternal *> repairWidgetList;
    QList<PrefUnpackExternal *> unpackWidgetList;

private:
    int numRepairers;
    int numUnpackers;

private:
	Configuration* config;
};

#endif // PREFUNPACK_H
