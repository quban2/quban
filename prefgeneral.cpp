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

#include <QFileDialog>
#include <QFontDialog>
#include <QDebug>
#include <QtDebug>
#include "appConfig.h"
#include "prefgeneral.h"

PrefGeneral::PrefGeneral(QWidget *parent)
    : QWidget(parent)
{
	setupUi(this);

	//Check if configuration entries exist, else put defaults...
	config = Configuration::getConfig();
	m_dbEdit->setText(config->dbDir );
	m_decodeEdit->setText(config->decodeDir);
	m_nzbEdit->setText(config->nzbDir);
	m_tmpEdit->setText(config->tmpDir);
	shutdownWait->setValue(config->maxShutDownWait);
	renameFiles=config->renameNzbFiles;
	renameNzbFiles->setChecked(renameNzbFiles);
	NoQueueSavePromptCheck->setChecked(config->noQueueSavePrompt);
	fontNameComboBox->setCurrentFont(config->appFont);
	fontSizeSpinBox->setValue(config->appFont.pointSize());

    minArticlesCheckBox->setChecked(config->minAvailEnabled);
    minArticlesSpinBox->setValue(config->minAvailValue);

    if (config->minAvailEnabled)
        minArticlesSpinBox->setEnabled(true);
    else
        minArticlesSpinBox->setEnabled(false);

	connect(m_dbChooseBtn, SIGNAL(clicked()), this, SLOT(slotSelectDbDir()));
	connect(m_decodeChooseBtn, SIGNAL(clicked()), this, SLOT(slotSelectDecodeDir()));
	connect(m_nzbChooseBtn, SIGNAL(clicked()), this, SLOT(slotSelectNzbDir()));
	connect(m_tmpChooseBtn, SIGNAL(clicked()), this, SLOT(slotSelectTmpDir()));
	connect(fontDialogButton, SIGNAL(clicked()), this, SLOT(slotFontSet()));

    connect(minArticlesCheckBox, SIGNAL(stateChanged(int)), this, SLOT(minArticlesEnabledChanged(int)));
}

PrefGeneral::~PrefGeneral()
{
}

void PrefGeneral::slotSelectDbDir( )
{
	 QString dir = QFileDialog::getExistingDirectory(this, tr("Please select Db Directory"),
			 m_dbEdit->text().trimmed(),
			 QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	if (!dir.isEmpty())
		m_dbEdit->setText(dir);
}
void PrefGeneral::slotSelectDecodeDir( )
{
	 QString dir = QFileDialog::getExistingDirectory(this, tr("Please select Decode Directory"),
			 m_decodeEdit->text().trimmed(),
			 QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	if (!dir.isEmpty())
		m_decodeEdit->setText(dir);
}

void PrefGeneral::slotSelectNzbDir( )
{
	 QString dir = QFileDialog::getExistingDirectory(this, tr("Please select Nzb Directory"),
			 m_nzbEdit->text().trimmed(),
			 QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	if (!dir.isEmpty())
		m_nzbEdit->setText(dir);
}

void PrefGeneral::slotSelectTmpDir( )
{
	 QString dir = QFileDialog::getExistingDirectory(this, tr("Please select Tmp Directory"),
			 m_tmpEdit->text().trimmed(),
			 QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	if (!dir.isEmpty())
		m_tmpEdit->setText(dir);
}

void PrefGeneral::slotFontSet()
{
	 bool ok;
	 QFont font = QFontDialog::getFont(&ok, config->appFont, this);
	 qDebug() << "Font dialog returned " << ok;
	 if (ok)
	 {
		fontNameComboBox->setCurrentFont(font);
		fontSizeSpinBox->setValue(font.pointSize());
	 }
}

void PrefGeneral::minArticlesEnabledChanged(int state)
{
    if (state)
        minArticlesSpinBox->setEnabled(true);
    else
        minArticlesSpinBox->setEnabled(false);
}

