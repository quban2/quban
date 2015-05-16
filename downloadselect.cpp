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

#include "downloadselect.h"
#include <QFileDialog>
#include <qmessagebox.h>
#include <qdir.h>
#include "common.h"

DownLoadSelect::DownLoadSelect(QString dir, QWidget* parent)
: QDialog(parent)
{
	setupUi(this);

	dirEdit->setText(dir);
// TODO	buttonSelectDir->setIconSet(KGlobal::iconLoader()->loadIcon("fileopen", KIcon::Small, 0, false));
// TODO	buttonOk->setIconSet(KGlobal::iconLoader()->loadIcon("button_ok", KIcon::Small, 0, false));
// TODO	buttonCancel->setIconSet(KGlobal::iconLoader()->loadIcon("button_cancel", KIcon::Small, 0, false));
	connect(buttonSelectDir, SIGNAL(clicked()), this, SLOT(selectDir()));

}

DownLoadSelect::~DownLoadSelect()
{
}

/*$SPECIALIZATION$*/
void DownLoadSelect::reject()
{
  QDialog::reject();
}

void DownLoadSelect::accept()
{
	if (!dirEdit->text().trimmed().isEmpty()) {
		//Check & create the dir
		if (!checkAndCreateDir(dirEdit->text()))
			QMessageBox::warning(this, tr("Error"), tr("Cannot create %1 or directory not writable").arg(dirEdit->text())  );
		else {
			emit download(QDir::cleanPath(dirEdit->text()) + '/', downloadFirstCheck->isChecked());
			QDialog::accept();
		}
	} else QMessageBox::warning(this, tr("Error"), tr("The directory name can not be empty"));

}

void DownLoadSelect::selectDir( )
{
	 QString dir = QFileDialog::getExistingDirectory(this, tr("Please select a directory"),
			 dirEdit->text().trimmed(),
			 QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	if (!dir.isEmpty())
		dirEdit->setText(dir);
}
