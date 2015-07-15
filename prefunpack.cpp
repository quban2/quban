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

#include "common.h"
#include <stdio.h>
#include <QDebug>
#include <QtDebug>
#include <QFile>
#include <QFileDialog>
#include "appConfig.h"
#include "prefunpack.h"

PrefUnpack::PrefUnpack(QWidget *parent)
    : QWidget(parent)
{
	setupUi(this);

	//Check if configuration entries exist, else put defaults...
	config = Configuration::getConfig();

    numRepairers = config->numRepairers;
    numUnpackers = config->numUnpackers;

	autoUnpackCheckBox->setChecked(config->autoUnpack);

	excludeLineEdit->setText(config->otherFileTypes);
	numRepairersSpinBox->setValue(config->numRepairers);
	numUnpackersSpinBox->setValue(config->numUnpackers);
	delGroupCheckBox->setChecked(config->deleteGroupFiles);
	delRepairCheckBox->setChecked(config->deleteRepairFiles);
	delCompressedCheckBox->setChecked(config->deleteCompressedFiles);
	delOtherCheckBox->setChecked(config->deleteOtherFiles);
	merge001CheckBox->setChecked(config->merge001Files);
	alwaysConfirmCheckBox->setChecked(config->dontShowTypeConfirmUnlessNeeded);

	connect(numRepairersSpinBox, SIGNAL(valueChanged(int)), this, SLOT(numRepairersChanged(int)));
	connect(numUnpackersSpinBox, SIGNAL(valueChanged(int)), this, SLOT(numUnpackersChanged(int)));

	if (config->numRepairers)
	{
        PrefUnpackExternal* repairWidget = 0;

		for (int i=0; i<config->numRepairers; i++)
		{
			repairWidget = new PrefUnpackExternal;
			repairWidgetList.append(repairWidget);

			repairWidget->pageTitle->setText("Repair Application");
			repairWidget->typeComboBox->addItem("Par2");
			repairWidget->typeComboBox->addItem("Other");

			repairWidget->priorityCombo->addItem("Lowest");
			repairWidget->priorityCombo->addItem("Low");
			repairWidget->priorityCombo->addItem("Normal");
			repairWidget->priorityCombo->addItem("As parent");

			if (i==0 && config->repairAppsList.at(i)->fullFilePath.isEmpty()) // set defaults for repair(0)
			{
				repairWidget->enabledCheckBox->setChecked(true);
				repairWidget->typeComboBox->setCurrentIndex(0);
				repairWidget->suffixesLineEdit->setText("par2");
				repairWidget->locationLineEdit->setText("/usr/bin/par2");
				checkAppValidity(repairWidget);
				repairWidget->paramsLineEdit->setText("r ${DRIVING_FILE}");
				repairWidget->priorityCombo->setCurrentIndex(3);
				repairWidget->appId = 1;
			}
			else
			{
				repairWidget->enabledCheckBox->setChecked(config->repairAppsList.at(i)->enabled);
				repairWidget->typeComboBox->setCurrentIndex(config->repairAppsList.at(i)->appType - 1);
				repairWidget->suffixesLineEdit->setText(config->repairAppsList.at(i)->suffixes);
				repairWidget->reSuffixesLineEdit->setText(config->repairAppsList.at(i)->reSuffixes);
				repairWidget->locationLineEdit->setText(config->repairAppsList.at(i)->fullFilePath);
				checkAppValidity(repairWidget);
				repairWidget->paramsLineEdit->setText(config->repairAppsList.at(i)->parameters);
				repairWidget->priorityCombo->setCurrentIndex(config->repairAppsList.at(i)->priority);
				if (config->repairAppsList.at(i)->appId == 0) // for 0.2.3 to 0.3.0 migration
				{
					if (i==0)
						repairWidget->appId = 1;
					else
						repairWidget->appId = ++(config->maxAppId);
				}
				else
				    repairWidget->appId = config->repairAppsList.at(i)->appId;
			}

			connect(repairWidget->locationPushButton, SIGNAL(clicked()), this, SLOT(locationSearch()));
			connect(repairWidget->removeButton, SIGNAL(clicked()), this, SLOT(removeExternalApp()));
			connect(repairWidget->locationLineEdit, SIGNAL(textChanged (const QString &)), this, SLOT(checkAppValidity()));

			tabWidget->addTab(repairWidget, repairWidget->locationLineEdit->text().section("/", -1));
		}
	}

	if (config->numUnpackers)
	{
        PrefUnpackExternal* unpackWidget = 0;

		for (int i=0; i<config->numUnpackers; i++)
		{
			unpackWidget = new PrefUnpackExternal;
			unpackWidgetList.append(unpackWidget);

			unpackWidget->pageTitle->setText("Unpack Application");
			unpackWidget->typeComboBox->addItem("Rar");
			unpackWidget->typeComboBox->addItem("7z");
			unpackWidget->typeComboBox->addItem("Zip");
			unpackWidget->typeComboBox->addItem("Other");

			unpackWidget->priorityCombo->addItem("Lowest");
			unpackWidget->priorityCombo->addItem("Low");
			unpackWidget->priorityCombo->addItem("Normal");
			unpackWidget->priorityCombo->addItem("As parent");

			if (i==0 && config->unpackAppsList.at(i)->fullFilePath.isEmpty()) // set defaults for unpack(0)
			{
				unpackWidget->enabledCheckBox->setChecked(true);
				unpackWidget->typeComboBox->setCurrentIndex(0);
				unpackWidget->suffixesLineEdit->setText("rar");
				unpackWidget->reSuffixesLineEdit->setText("r[0-9]{2}");
				unpackWidget->locationLineEdit->setText("/usr/bin/unrar");
				checkAppValidity(unpackWidget);
				unpackWidget->paramsLineEdit->setText("x -o+ -p- ${DRIVING_FILE}");
				unpackWidget->priorityCombo->setCurrentIndex(3);
				unpackWidget->appId = 2;
			}
			else if (i==1 && config->unpackAppsList.at(i)->fullFilePath.isEmpty()) // set defaults for unpack(1)
			{
				unpackWidget->enabledCheckBox->setChecked(true);
				unpackWidget->typeComboBox->setCurrentIndex(1);
				unpackWidget->suffixesLineEdit->setText("7z");
				unpackWidget->locationLineEdit->setText("/usr/bin/7za");
				checkAppValidity(unpackWidget);
				unpackWidget->paramsLineEdit->setText("x ${DRIVING_FILE}");
				unpackWidget->priorityCombo->setCurrentIndex(3);
				unpackWidget->appId = 3;
			}
			else if (i==2 && config->unpackAppsList.at(i)->fullFilePath.isEmpty()) // set defaults for unpack(2)
			{
				unpackWidget->enabledCheckBox->setChecked(true);
				unpackWidget->typeComboBox->setCurrentIndex(2);
				unpackWidget->suffixesLineEdit->setText("zip");
				unpackWidget->locationLineEdit->setText("/usr/bin/unzip");
				checkAppValidity(unpackWidget);
				unpackWidget->paramsLineEdit->setText("${DRIVING_FILE}");
				unpackWidget->priorityCombo->setCurrentIndex(3);
				unpackWidget->appId = 4;
			}
			else
			{
				unpackWidget->enabledCheckBox->setChecked(config->unpackAppsList.at(i)->enabled);
				unpackWidget->typeComboBox->setCurrentIndex(config->unpackAppsList.at(i)->appType - 1);
				unpackWidget->suffixesLineEdit->setText(config->unpackAppsList.at(i)->suffixes);
				unpackWidget->reSuffixesLineEdit->setText(config->unpackAppsList.at(i)->reSuffixes);
				unpackWidget->locationLineEdit->setText(config->unpackAppsList.at(i)->fullFilePath);
				checkAppValidity(unpackWidget);
				unpackWidget->paramsLineEdit->setText(config->unpackAppsList.at(i)->parameters);
				unpackWidget->priorityCombo->setCurrentIndex(config->unpackAppsList.at(i)->priority);
				if (config->unpackAppsList.at(i)->appId == 0) // for 0.2.3 to 0.3.0 migration
				{
					if (i==0)
					{
						unpackWidget->appId = 2;
					    unpackWidget->reSuffixesLineEdit->setText("r[0-9]{2}");
					}
					else if (i==1)
						unpackWidget->appId = 3;
					else if (i==2)
						unpackWidget->appId = 4;
					else
						unpackWidget->appId = ++(config->maxAppId);
				}
				else
				    unpackWidget->appId = config->unpackAppsList.at(i)->appId;
			}

			connect(unpackWidget->locationPushButton, SIGNAL(clicked()), this, SLOT(locationSearch()));
			connect(unpackWidget->removeButton, SIGNAL(clicked()), this, SLOT(removeExternalApp()));
			connect(unpackWidget->locationLineEdit, SIGNAL(textChanged (const QString &)), this, SLOT(checkAppValidity()));

			tabWidget->addTab(unpackWidget, unpackWidget->locationLineEdit->text().section("/", -1));
		}
	}

	if (!autoUnpackCheckBox->isChecked())
	{
		numAppsGroupBox->setEnabled(false);
		actionsGroupBox->setEnabled(false);
		excludeLabel->setEnabled(false);
		excludeLineEdit->setEnabled(false);
	}

	connect(autoUnpackCheckBox, SIGNAL(stateChanged(int)), this, SLOT(slotEnableConfig()));
}

PrefUnpack::~PrefUnpack()
{

}

void PrefUnpack::slotEnableConfig()
{
	if (autoUnpackCheckBox->isChecked())
	{
		numAppsGroupBox->setEnabled(true);
		actionsGroupBox->setEnabled(true);
		excludeLabel->setEnabled(true);
		excludeLineEdit->setEnabled(true);
	}
	else
	{
		numAppsGroupBox->setEnabled(false);
		actionsGroupBox->setEnabled(false);
		excludeLabel->setEnabled(false);
		excludeLineEdit->setEnabled(false);
	}
}

void PrefUnpack::numRepairersChanged(int newVal)
{
	if (numRepairers > newVal) // removal
	{
		for (int i=(numRepairers-1); i>(newVal-1); i--)
		{
			PrefUnpackExternal* activeWidget = repairWidgetList.takeLast();
			tabWidget->removeTab(tabWidget->indexOf(activeWidget));
			numRepairers--;
		}
	}
	else if (numRepairers < newVal) // addition
	{
        PrefUnpackExternal* repairWidget = 0;

		for (int i=(numRepairers-1); i<(newVal-1); i++)
		{
			repairWidget = new PrefUnpackExternal;
			repairWidgetList.append(repairWidget);

			repairWidget->pageTitle->setText("Repair Application");
			repairWidget->typeComboBox->addItem("Par2");
			repairWidget->typeComboBox->addItem("Other");
			repairWidget->typeComboBox->setCurrentIndex(1);
			repairWidget->priorityCombo->addItem("Lowest");
			repairWidget->priorityCombo->addItem("Low");
			repairWidget->priorityCombo->addItem("Normal");
			repairWidget->priorityCombo->addItem("As parent");
			repairWidget->priorityCombo->setCurrentIndex(3);
			repairWidget->appId = ++(config->maxAppId);
			connect(repairWidget->locationPushButton, SIGNAL(clicked()), this, SLOT(locationSearch()));
			connect(repairWidget->removeButton, SIGNAL(clicked()), this, SLOT(removeExternalApp()));
			connect(repairWidget->locationLineEdit, SIGNAL(textChanged (const QString &)), this, SLOT(checkAppValidity()));

			int tabIndex = tabWidget->addTab(repairWidget, QString("New Repairer"));
			tabWidget->setCurrentIndex(tabIndex);

			numRepairers++;
		}
	}
}

void PrefUnpack::numUnpackersChanged(int newVal)
{
	if (numUnpackers > newVal) // removal
	{
		for (int i=(numUnpackers-1); i>(newVal-1); i--)
		{
			PrefUnpackExternal* activeWidget = unpackWidgetList.takeLast();
			tabWidget->removeTab(tabWidget->indexOf(activeWidget));
			numUnpackers--;
		}
	}
	else if (numUnpackers < newVal) // addition
	{
        PrefUnpackExternal* unpackWidget = 0;

		for (int i=(numUnpackers-1); i<(newVal-1); i++)
		{
			unpackWidget = new PrefUnpackExternal;
			unpackWidgetList.append(unpackWidget);

			unpackWidget->pageTitle->setText("Unpack Application");
			unpackWidget->typeComboBox->addItem("Rar");
			unpackWidget->typeComboBox->addItem("7z");
			unpackWidget->typeComboBox->addItem("Zip");
			unpackWidget->typeComboBox->addItem("Other");
			unpackWidget->typeComboBox->setCurrentIndex(3);
			unpackWidget->priorityCombo->addItem("Lowest");
			unpackWidget->priorityCombo->addItem("Low");
			unpackWidget->priorityCombo->addItem("Normal");
			unpackWidget->priorityCombo->addItem("As parent");
			unpackWidget->priorityCombo->setCurrentIndex(3);
			unpackWidget->appId = ++(config->maxAppId);
			connect(unpackWidget->locationPushButton, SIGNAL(clicked()), this, SLOT(locationSearch()));
			connect(unpackWidget->removeButton, SIGNAL(clicked()), this, SLOT(removeExternalApp()));
			connect(unpackWidget->locationLineEdit, SIGNAL(textChanged (const QString &)), this, SLOT(checkAppValidity()));

			int tabIndex = tabWidget->addTab(unpackWidget, QString("New Unpacker"));
			tabWidget->setCurrentIndex(tabIndex);

			numUnpackers++;
		}
	}
}

void PrefUnpack::locationSearch()
{
	PrefUnpackExternal* activeWidget = (PrefUnpackExternal*)tabWidget->currentWidget();

	 QString fileName = QFileDialog::getOpenFileName(this, tr("Please select application"),
	                                                 "/usr/bin");

	if (!fileName.isEmpty())
		activeWidget->locationLineEdit->setText(fileName);
}

void PrefUnpack::removeExternalApp()
{
	int activeIndex = tabWidget->currentIndex();
	PrefUnpackExternal* activeWidget = (PrefUnpackExternal*)tabWidget->currentWidget();

	if ( activeWidget->pageTitle->text().startsWith("Repair"))
	{
		numRepairersSpinBox->setValue(numRepairersSpinBox->value() - 1);
		repairWidgetList.removeOne(activeWidget);
	}
	else
	{
		numUnpackersSpinBox->setValue(numUnpackersSpinBox->value() - 1);
		unpackWidgetList.removeOne(activeWidget);
	}

	tabWidget->removeTab(activeIndex);
}

void PrefUnpack::checkAppValidity()
{
	PrefUnpackExternal* activeWidget = (PrefUnpackExternal*)tabWidget->currentWidget();

	checkAppValidity(activeWidget);
}

void PrefUnpack::checkAppValidity(PrefUnpackExternal* activeWidget)
{
	if (!QFile::exists(activeWidget->locationLineEdit->text()))
	{
	    QIcon icon(QString::fromUtf8(":/quban/images/fatCow/Cross.png"));
	    activeWidget->validButton->setIcon(icon);
	    activeWidget->validButton->setToolTip("File name or path is incorrect");
	}
	else
	{
	    QIcon icon(QString::fromUtf8(":/quban/images/fatCow/Tick.png"));
	    activeWidget->validButton->setIcon(icon);
	    activeWidget->validButton->setToolTip("File is valid");
	}
}
