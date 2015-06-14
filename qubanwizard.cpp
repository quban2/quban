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
#include <QDir>
#include <QApplication>
#include "appConfig.h"
#include "common.h"
#include "quban.h"
#include "prefgeneral.h"
#include "prefunpack.h"
#include "qubanwizard.h"

extern Quban* quban;

QubanWizard::QubanWizard(QWidget* parent)
    : QDialog(parent)
{
	setupUi(this);

	config = Configuration::getConfig();

	prefGeneral = new PrefGeneral(generalFrame);
    generalLayout->addWidget(prefGeneral);
	prefUnpack = new PrefUnpack(unpackFrame);
    unpackLayout2->addWidget(prefUnpack);

	fromCheck->setChecked(config->showFrom);
	detailsCheck->setChecked(config->showDetails);
	dateCheck->setChecked(config->showDate);
	sortCheck->setChecked(config->rememberSort);
	sizeCheck->setChecked(config->rememberWidth);
	compressedCheck->setChecked(config->downloadCompressed);
    groupingSizeSpinBox->setValue(config->groupCheckCount);

    deleteBtn->setChecked(config->deleteFailed);
    keepBtn->setChecked(!config->deleteFailed);
    overwriteBtn->setChecked(config->overwriteExisting);
    renameBtn->setChecked(!config->overwriteExisting);

    if (config->singleViewTab)
    {
            oneTabBtn->setChecked(true);
            askBtn->setEnabled(false);
    }
    else
            multiTabBtn->setChecked(true);

    if (config->dViewed == Configuration::Ask)
    {
            if (!config->singleViewTab)
                    askBtn_2->setChecked(true);
            else
                    keepBtn_2->setChecked(true);
    }
    else if (config->dViewed == Configuration::No)
            keepBtn_2->setChecked(true);
    else
            deleteBtn_2->setChecked(true);

    textFileSuffixes->setText(config->textSuffixes);

    activateBtn->setChecked(config->activateTab);

    statusBtn->setChecked(config->showStatusBar);

	if (config->markOpt == Configuration::Ask)
		askBtn->setChecked(true);
	else if (config->markOpt == Configuration::No)
		nullBtn->setChecked(true);
	else markBtn->setChecked(true);

	if (config->checkDaysValue)
	{
		checkDaysValue->setValue(config->checkDaysValue);
		checkHeadersCheck->setChecked(config->checkDaysValue);
	}

    bulkLoadValue->setValue(config->bulkLoadThreshold);
    bulkDeleteValue->setValue(config->bulkDeleteThreshold);

	connect(okButton, SIGNAL(clicked()), this, SLOT(slotOkClicked()));
	connect(cancelButton, SIGNAL(clicked()), this, SLOT(slotCancelClicked()));
	connect(addServerButton, SIGNAL(clicked()), this, SIGNAL(newServer()));
	connect(getGroupListButton, SIGNAL(clicked()), this, SIGNAL(sigGetGroups()));

	connect(checkHeadersCheck, SIGNAL(toggled(bool)), this, SLOT(slotToggleDay(bool)));
}

QubanWizard::~QubanWizard()
{
}

void QubanWizard::slotToggleDay(bool)
{
	if (checkHeadersCheck->isChecked() )
		checkDaysValue->setEnabled(true);
	else
		checkDaysValue->setEnabled(false);
}

void QubanWizard::slotOkClicked()
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
		config->configured=true;
		config->dbDir=QDir::cleanPath(prefGeneral->dbDir());
		config->tmpDir=QDir::cleanPath(prefGeneral->tmpDir());
		config->decodeDir=QDir::cleanPath(prefGeneral->decodeDir());
		config->nzbDir=QDir::cleanPath(prefGeneral->nzbDir());
		config->maxShutDownWait=prefGeneral->shutdownWait->value();
		config->renameNzbFiles=prefGeneral->renameNzbFiles->isChecked();
		config->noQueueSavePrompt=prefGeneral->NoQueueSavePromptCheck->isChecked();
		config->appFont.setFamily(prefGeneral->fontNameComboBox->currentFont().family());
		config->appFont.setPointSize(prefGeneral->fontSizeSpinBox->value());
		QApplication::setFont(config->appFont);

        config->minAvailEnabled=prefGeneral->minArticlesCheckBox->isChecked();
        config->minAvailValue = prefGeneral->minArticlesSpinBox->value();

		//Decoding options
		config->overwriteExisting=overwriteBtn->isChecked();
		config->deleteFailed=deleteBtn->isChecked();

		//View
		config->activateTab=activateBtn->isChecked();
		config->singleViewTab=oneTabBtn->isChecked();
		config->showStatusBar=statusBtn->isChecked();
		config->textSuffixes=textFileSuffixes->text();

        if ( deleteBtn_2->isChecked())
		{
			config->dViewed=Configuration::Yes;
		}
        else if (keepBtn_2->isChecked())
		{
			config->dViewed=Configuration::No;
		} else
		{
			config->dViewed=Configuration::Ask;
		}

		//now create/delete the viewer tab based on the configuration...

		config->showFrom=fromCheck->isChecked();
		config->showDetails=detailsCheck->isChecked();
		config->showDate = dateCheck->isChecked();
		config->rememberSort = sortCheck->isChecked();
		config->rememberWidth = sizeCheck->isChecked();
		config->downloadCompressed = compressedCheck->isChecked();
        config->groupCheckCount = groupingSizeSpinBox->value();

		if (markBtn->isChecked())
			config->markOpt=Configuration::Yes;
		else if (askBtn->isChecked())
			config->markOpt=Configuration::Ask;
		else config->markOpt = Configuration::No;

		if (checkHeadersCheck->isChecked())
			config->checkDaysValue=checkDaysValue->value();
		else
			config->checkDaysValue=0;

        config->bulkLoadThreshold = bulkLoadValue->value();
        config->bulkDeleteThreshold = bulkDeleteValue->value();

		// unpack config
		bool oldAutoUnpack = config->autoUnpack;
		config->autoUnpack=prefUnpack->autoUnpackCheckBox->isChecked();
		config->deleteGroupFiles=prefUnpack->delGroupCheckBox->isChecked();
		config->deleteRepairFiles=prefUnpack->delRepairCheckBox->isChecked();
		config->deleteCompressedFiles=prefUnpack->delCompressedCheckBox->isChecked();
		config->deleteOtherFiles=prefUnpack->delOtherCheckBox->isChecked();
		config->merge001Files=prefUnpack->merge001CheckBox->isChecked();
		config->dontShowTypeConfirmUnlessNeeded=prefUnpack->alwaysConfirmCheckBox->isChecked();
		config->otherFileTypes=prefUnpack->excludeLineEdit->text();
		config->numRepairers=prefUnpack->numRepairersSpinBox->value();
		config->numUnpackers=prefUnpack->numUnpackersSpinBox->value();

		if (config->numRepairers > 0)
		{
			if (config->numRepairers > config->repairAppsList.count()) // need to add to list
			{
				t_ExternalApp* repairExternalApp;

				for (int i=config->repairAppsList.count(); i<config->numRepairers; i++)
				{
					repairExternalApp = new t_ExternalApp;
					config->repairAppsList.append(repairExternalApp);
				}
			}
			else if (config->numRepairers < config->repairAppsList.count()) // can reduce list
			{
				for (int i=config->numRepairers; i<config->repairAppsList.count(); i++)
				{
					config->repairAppsList.removeLast();
				}
			}
		}

		PrefUnpackExternal* repairWidget;

		for (int i=0; i<config->numRepairers; i++)
		{
			repairWidget = prefUnpack->repairWidgetList.at(i);
			config->repairAppsList.at(i)->enabled = repairWidget->enabledCheckBox->isChecked();
			config->repairAppsList.at(i)->appType = repairWidget->typeComboBox->currentIndex() + 1;
			config->repairAppsList.at(i)->suffixes = repairWidget->suffixesLineEdit->text();
			config->repairAppsList.at(i)->reSuffixes = repairWidget->reSuffixesLineEdit->text();
			config->repairAppsList.at(i)->fullFilePath = repairWidget->locationLineEdit->text();
			config->repairAppsList.at(i)->parameters = repairWidget->paramsLineEdit->text();
			config->repairAppsList.at(i)->priority = repairWidget->priorityCombo->currentIndex();
			config->repairAppsList.at(i)->appId = repairWidget->appId;
		}

		if (config->numUnpackers > 0)
		{
			if (config->numUnpackers > config->unpackAppsList.count()) // need to add to list
			{
				t_ExternalApp* unpackExternalApp;

				for (int i=config->unpackAppsList.count(); i<config->numUnpackers; i++)
				{
					unpackExternalApp = new t_ExternalApp;
					config->unpackAppsList.append(unpackExternalApp);
				}
			}
			else if (config->numUnpackers < config->unpackAppsList.count()) // can reduce list
			{
				for (int i=config->numUnpackers; i<config->unpackAppsList.count(); i++)
				{
					config->unpackAppsList.removeLast();
				}
			}
		}

		PrefUnpackExternal* unpackWidget;

		for (int i=0; i<config->numUnpackers; i++)
		{
			unpackWidget = prefUnpack->unpackWidgetList.at(i);
			config->unpackAppsList.at(i)->enabled = unpackWidget->enabledCheckBox->isChecked();
			config->unpackAppsList.at(i)->appType = unpackWidget->typeComboBox->currentIndex() + 1;
			config->unpackAppsList.at(i)->suffixes = unpackWidget->suffixesLineEdit->text();
			config->unpackAppsList.at(i)->reSuffixes = unpackWidget->reSuffixesLineEdit->text();
			config->unpackAppsList.at(i)->fullFilePath = unpackWidget->locationLineEdit->text();
			config->unpackAppsList.at(i)->parameters = unpackWidget->paramsLineEdit->text();
			config->unpackAppsList.at(i)->priority = unpackWidget->priorityCombo->currentIndex();
			config->unpackAppsList.at(i)->appId = unpackWidget->appId;
		}

		config->write();

		if (oldAutoUnpack != config->autoUnpack)
			quban->enableUnpack(config->autoUnpack);
	}

    accept();
}

void QubanWizard::slotCancelClicked()
{
	qDebug("Rejected");
	if (config->configured == false)
		QMessageBox::critical ( this, tr("Invalid configuration"), tr("Configuration was cancelled"));

	QDialog::reject();
}
