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
#include "ratecontroldialog.h"

RateControlDialog::RateControlDialog(QWidget *parent)
    : QDialog(parent)
{
	setupUi(this);

	connect(buttonCancel, SIGNAL(clicked()), this, SLOT(reject()));
	connect(buttonOk, SIGNAL(clicked()), this, SLOT(accept()));
}

RateControlDialog::~RateControlDialog()
{
}

void RateControlDialog::reject()
{
	QDialog::reject();
}

void RateControlDialog::accept()
{
	qint64 speed;

	if (restrictedButton->isChecked())
	{
		if (mbButton->isChecked())
			speed = speedSpinBox->value() * 1024 * 1024;
		else
			speed = speedSpinBox->value() * 1024;
	}
	else
		speed = -1; // unrestricted

	emit setSpeed(speed);

	QDialog::accept();
}
