#ifndef QUBANWIZARD_H
#define QUBANWIZARD_H

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
#include "ui_qubanwizard.h"

class PrefHeader;
class PrefGeneral;
class PrefDecode;
class PrefView;
class PrefUnpack;
class Configuration;

class QubanWizard : public QDialog, public Ui::QubanWizard
{
    Q_OBJECT

public:
    QubanWizard(QWidget* parent = 0);
    ~QubanWizard();

private:
	PrefGeneral *prefGeneral;
	PrefDecode  *prefDecode;
	PrefView    *prefView;
	PrefHeader  *prefHeader;
	PrefUnpack  *prefUnpack;
	Configuration* config;

protected slots:
    void slotOkClicked();
    void slotCancelClicked();

    void slotToggleDay(bool);

   signals:
  	void newServer();
	void sigGetGroups();
};

#endif // QUBANWIZARD_H
