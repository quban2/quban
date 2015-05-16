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

#include "appConfig.h"
#include "prefview.h"

PrefView::PrefView(QWidget *parent)
    : QWidget(parent)
{
	setupUi(this);

	Configuration* config = Configuration::getConfig();

	dViewed=config->dViewed;

	singleViewTab=config->singleViewTab;
	if (singleViewTab)
	{
		oneTabBtn->setChecked(true);
		askBtn->setEnabled(false);
	}
	else
		multiTabBtn->setChecked(true);

	if (dViewed == Configuration::Ask)
	{
		if (!singleViewTab)
			askBtn->setChecked(true);
		else
			keepBtn->setChecked(true);
	}
	else if (dViewed == Configuration::No)
		keepBtn->setChecked(true);
	else
		deleteBtn->setChecked(true);

	connect(oneTabBtn, SIGNAL(toggled(bool)), this, SLOT(slotSingleTabBtnToggled(bool )));

	textFileSuffixes->setText(config->textSuffixes);

	activateTab=config->activateTab;

	activateBtn->setChecked(activateTab);

	showStatusBar=config->showStatusBar;
	statusBtn->setChecked(showStatusBar);
}

PrefView::~PrefView()
{

}

void PrefView::slotSingleTabBtnToggled( bool on)
{
	if (on) {
		askBtn->setEnabled(false);
		keepBtn->setChecked(true);
	} else askBtn->setEnabled(true);

}
