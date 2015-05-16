#include "prefstartup.h"
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

#include <QDir>
#include <QMessageBox>
#include "appConfig.h"
#include "common.h"
#include "quban.h"
#include "prefgeneral.h"
#include "prefstartup.h"

extern Quban* quban;

PrefStartup::PrefStartup(QWidget *parent)
    : QDialog(parent)
{
	setupUi(this);

	config = Configuration::getConfig();

	prefGeneral = new PrefGeneral(generalFrame);
    generalLayout->addWidget(prefGeneral);

    // These are not essential at this stage - get them in the wizard
    prefGeneral->label_2->hide();
    prefGeneral->shutdownWait->hide();
    prefGeneral->NoQueueSavePromptCheck->hide();
    prefGeneral->renameNzbFiles->hide();
    prefGeneral->availableGroupsFrame->hide();

	QObject::connect(okButton, SIGNAL(clicked()), this, SLOT(acceptPrefs()));
	QObject::connect(cancelButton, SIGNAL(clicked()), this, SLOT(rejectPrefs()));
}

PrefStartup::~PrefStartup()
{

}

void PrefStartup::acceptPrefs()
{
	if (prefGeneral->dbDir().isEmpty())
	{
		QMessageBox::warning ( this, tr("Invalid Directory"), tr("Invalid db Directory"));
	}
	else if (prefGeneral->tmpDir().isEmpty())
	{
		QMessageBox::warning ( this, tr("Invalid Directory"), tr("Invalid tmp Directory"));
	}
	else if (prefGeneral->decodeDir().isEmpty())
	{
		QMessageBox::warning ( this, tr("Invalid Directory"), tr("Invalid decode Directory"));
	}
	else if (prefGeneral->nzbDir().isEmpty())
	{
		QMessageBox::warning ( this, tr("Invalid Directory"), tr("Invalid nzb Directory"));
	}
    // directories check/creation
	else if (!checkAndCreateDir(prefGeneral->dbDir()))
	{
		QMessageBox::warning ( this, tr("Invalid Directory"), tr("Could not create db Directory"));
    }
	else if (! checkAndCreateDir(prefGeneral->decodeDir()))
	{
		QMessageBox::warning ( this, tr("Invalid Directory"), tr("Could not create decode Directory"));
    }
	else if (! checkAndCreateDir(prefGeneral->tmpDir()))
	{
		QMessageBox::warning ( this, tr("Invalid Directory"), tr("Could not create tmp Directory"));
	}
	else if (! checkAndCreateDir(prefGeneral->nzbDir()))
	{
		QMessageBox::warning ( this, tr("Invalid Directory"), tr("Could not create nzb Directory"));
	}
	else
	{
		//write configuration
		config->configured=false; // the wizard completes the configuration
		config->dbDir=QDir::cleanPath(prefGeneral->dbDir());
		config->tmpDir=QDir::cleanPath(prefGeneral->tmpDir());
		config->decodeDir=QDir::cleanPath(prefGeneral->decodeDir());
		config->nzbDir=QDir::cleanPath(prefGeneral->nzbDir());

		config->write();

        quban=new Quban;
		quban->startup();
		quban->show();
		quban->showWizard(true);
	}

    accept();
}

void PrefStartup::rejectPrefs( )
{
	qDebug("Rejected");
	if (config->configured == false)
		QMessageBox::critical ( this, tr("Invalid configuration"), tr("Configuration was cancelled"));

	QDialog::reject();
}
