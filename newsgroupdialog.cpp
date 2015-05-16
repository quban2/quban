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
#include <QMessageBox>
#include <QFileDialog>
#include <QDate>
#include <QDebug>
#include <QtDebug>
#include <QDesktopWidget>
#include "appConfig.h"
#include "grouplist.h"
#include "headerlist.h"
#include "catman.h"
#include "quban.h"
#include "helpdialog.h"
#include "headergroupingwidget.h"
#include "newsgroupdialog.h"

extern Quban* quban;

NewsGroupDialog::NewsGroupDialog(QString ngName, QStringList _cats, AvailableGroup * _g, QWidget* parent)
	: QDialog(parent), cats(_cats), g(_g)
{
	setupUi(this);

	config = Configuration::getConfig();

	creating = true;
	refreshed = false;
    groupingUpdated = false;
	ng = 0;

    headerGroupingWidget = new HeaderGroupingWidget();
    regexLayout->addWidget(headerGroupingWidget);

    headerGroupingWidget->testGroupingPushButton->setHidden(true);
    headerGroupingWidget->regroupButton->setHidden(true);

	connect(buttonSelectDir, SIGNAL(clicked()), this, SLOT(selectDir()));
	connect(buttonOk, SIGNAL(clicked()), this, SLOT(accept()));
	connect(buttonCancel, SIGNAL(clicked()), this, SLOT(reject()));
    connect(m_alias, SIGNAL(textChanged(const QString&)), this, SLOT(aliasChanged(const QString&)));
	connect(m_addCatBtn, SIGNAL(clicked()), this, SLOT(slotAddClicked()));
    connect(numDaysRetention, SIGNAL(toggled(bool)), this, SLOT(slotToggleDay(bool)));
    connect(numDaysRetention_from_download, SIGNAL(toggled(bool)), this, SLOT(slotToggleDownloadDay(bool)));

    connect(groupArticlesCheck, SIGNAL(stateChanged(int)), this, SLOT(slotGroupHeadersStateChanged(int)));
    connect(helpPushButton, SIGNAL(clicked()), this, SLOT(help()));

	m_name->setReadOnly(true);
	m_name->setText(ngName);
	m_alias->setText(ngName);
	m_catCombo->insertItems(0, cats);
	m_catCombo->setCurrentIndex(m_catCombo->findText("None"));
	m_saveDir->setReadOnly(false);
	numDaysRetention_from_download->setChecked(true);
	unsequencedCheck->setChecked(false);
	dbNamedAsAliasCheck->setChecked(false);
    groupArticlesCheck->setChecked(false);
    headerGroupingWidget->headerGroupBox->setEnabled(false);
	shouldBeNamedAsAlias = false;

    headerGroupingWidget->groupRE1_Value->setText("-.*$");
    headerGroupingWidget->groupRE2_Value->setText("");
    headerGroupingWidget->groupRE3_Value->setText("");

    headerGroupingWidget->groupRE1_Combo->setCurrentIndex(1);
    headerGroupingWidget->groupRE2_Combo->setCurrentIndex(1);
    headerGroupingWidget->groupRE3_Combo->setCurrentIndex(1);

    headerGroupingWidget->maxDistanceSpinBox->setValue(12);

	slotToggleDownloadDay(true);
}

NewsGroupDialog::NewsGroupDialog( NewsGroup * _ng, Servers *_servers, QStringList _cats, QWidget * parent)
      : QDialog(parent) , cats(_cats), servers(_servers),  ng(_ng)
{
	setupUi(this);

	g = 0;

	config = Configuration::getConfig();

	creating = false;
	refreshed = false;
    groupingUpdated = false;

    headerGroupingWidget = new HeaderGroupingWidget();
    regexLayout->addWidget(headerGroupingWidget);

	connect(buttonSelectDir, SIGNAL(clicked()), this, SLOT(selectDir()));
	connect(buttonOk, SIGNAL(clicked()), this, SLOT(accept()));
	connect(buttonCancel, SIGNAL(clicked()), this, SLOT(reject()));
	connect(m_addCatBtn, SIGNAL(clicked()), this, SLOT(slotAddClicked()));
	connect(numDaysRetention, SIGNAL(toggled(bool)), this, SLOT(slotToggleDay(bool)));
	connect(numDaysRetention_from_download, SIGNAL(toggled(bool)), this, SLOT(slotToggleDownloadDay(bool)));
    connect(groupArticlesCheck, SIGNAL(stateChanged(int)), this, SLOT(slotGroupHeadersStateChanged(int)));
    connect(headerGroupingWidget->regroupButton, SIGNAL(clicked()), this, SLOT(slotRegroup()));
    connect(headerGroupingWidget->testGroupingPushButton, SIGNAL(clicked()), this, SLOT(slotTestGrouping()));
    connect(helpPushButton, SIGNAL(clicked()), this, SLOT(help()));

	m_name->setReadOnly(true);
	m_name->setText(ng->getName());
	savedName = ng->getName();
	m_alias->setText(ng->getAlias());
	savedAlias = ng->getAlias();
	//New and TODO: load categories into the combobox.
	//set the current category active
	m_catCombo->insertItems(0, cats);
	m_catCombo->setCurrentIndex(m_catCombo->findText(ng->getCategory()));
	savedCategory = ng->getCategory();
	qDebug("deleteOlderPosting: %d", ng->getDeleteOlderPosting());

	if (ng->getDeleteOlderPosting() != 0)
	{
		numDaysRetention->setChecked(true);
		daySpin->setValue(ng->getDeleteOlderPosting());
	}
	else if (ng->getDeleteOlderDownload() != 0)
	{
		numDaysRetention_from_download->setChecked(true);
		daySpin_2->setValue(ng->getDeleteOlderDownload());
	}
    else
        retainForeverButton->setChecked(true);

    savedDeleteOlder = ng->getDeleteOlderPosting();
    savedDeleteOlder2 = ng->getDeleteOlderDownload();

	m_saveDir->setText(ng->getSaveDir());
	m_saveDir->setReadOnly(false);
	savedSaveDir = ng->getSaveDir();

	unsequencedCheck->setChecked(ng->areUnsequencedArticlesIgnored());
	dbNamedAsAliasCheck->setChecked(ng->shouldDbBeNamedAsAlias());
	shouldBeNamedAsAlias = ng->shouldDbBeNamedAsAlias();

    savedGroupArticles = ng->areHeadersGrouped();
    groupArticlesCheck->setChecked(ng->areHeadersGrouped());
    if (ng->areHeadersGrouped())
        headerGroupingWidget->headerGroupBox->setEnabled(true);
    else
        headerGroupingWidget->headerGroupBox->setEnabled(false);

    headerGroupingWidget->groupRE1_Value->setText(ng->getGroupRE1());
    headerGroupingWidget->groupRE2_Value->setText(ng->getGroupRE2());
    headerGroupingWidget->groupRE3_Value->setText(ng->getGroupRE3());

    headerGroupingWidget->groupRE1_Combo->setCurrentIndex(ng->getGroupRE1Back());
    headerGroupingWidget->groupRE2_Combo->setCurrentIndex(ng->getGroupRE2Back());
    headerGroupingWidget->groupRE3_Combo->setCurrentIndex(ng->getGroupRE3Back());

    headerGroupingWidget->maxDistanceSpinBox->setValue(ng->getMatchDistance());

	buttonOk->setText(tr("Modify"));

    connect(m_alias, SIGNAL(textChanged(const QString&)), this, SLOT(aliasChanged(const QString&)));
}

NewsGroupDialog::~NewsGroupDialog()
{
}

void NewsGroupDialog::reject()
{
  QDialog::reject();
}

void NewsGroupDialog::accept( )
{
	int saveDays, saveDays2;

	if (numDaysRetention->isChecked())
		saveDays = daySpin->value();
	else
		saveDays = 0;

	if (numDaysRetention_from_download->isChecked())
		saveDays2 = daySpin_2->value();
	else
		saveDays2 = 0;

    // If changed to save as alias make sure it's unique
    if ((!ng || ng->shouldDbBeNamedAsAlias() != dbNamedAsAliasCheck->isChecked()) &&
            dbNamedAsAliasCheck->isChecked())
    {
        GroupList* gl = quban->getSubGroupList();
        if (gl->isAliasUsedForDbName(m_alias->text()))
        {
            QMessageBox::warning(this, tr("Error"), tr("The group alias cannot be used as database name as it's not unique"));
            return;
        }
    }

    qDebug() << "Group created or modified";
    if (m_name->text().trimmed().isEmpty() )
    {
        QMessageBox::warning(this, tr("Error"), tr("The group name cannot be null"));
    }
    else if ((m_saveDir->text().trimmed().isEmpty()) )
    {
        QMessageBox::warning(this, tr("Error"), tr("The directory name cannot be empty"));
    }
    else if (!checkAndCreateDir(m_saveDir->text()))
    {
        QMessageBox::warning(this, tr("Error"), tr("Cannot create %1 or directory not writable").arg(m_saveDir->text()));
    }
    else if (groupArticlesCheck->isChecked() && headerGroupingWidget->noRegexCheckBox->isChecked() == false &&
             (headerGroupingWidget->groupRE1_Value->text().isEmpty() ||headerGroupingWidget-> groupRE1_Value->text().isNull()))
    {
        QMessageBox::warning(this, tr("Error"),
            tr("Cannot have an empty first regex unless 'Don't use any regular expressions ...' is checked"));
    }
    else if (m_alias->text().trimmed().isEmpty())
    {
        //Allow empty alias, setting it to the ng name
        m_alias->setText(m_name->text());
    }
    else
    {
        entries.append(m_name->text());
        entries.append(QDir::cleanPath(m_saveDir->text()));
        entries.append(m_alias->text());
        entries.append(m_catCombo->currentText());
        entries.append(QString::number(saveDays));
        entries.append(QString::number(saveDays2));
        entries.append(unsequencedCheck->isChecked() ? "Y" : "N");
        entries.append(dbNamedAsAliasCheck->isChecked() ? "Y" : "N");
        entries.append(groupArticlesCheck->isChecked() ? "Y" : "N");
        entries.append(headerGroupingWidget->groupRE1_Value->text());
        entries.append(headerGroupingWidget->groupRE2_Value->text());
        entries.append(headerGroupingWidget->groupRE3_Value->text());
        entries.append(QString::number(headerGroupingWidget->groupRE1_Combo->currentIndex()));
        entries.append(QString::number(headerGroupingWidget->groupRE2_Combo->currentIndex()));
        entries.append(QString::number(headerGroupingWidget->groupRE3_Combo->currentIndex()));
        entries.append(QString::number(headerGroupingWidget->maxDistanceSpinBox->value()));
        entries.append(headerGroupingWidget->noRegexCheckBox->isChecked() ? "Y" : "N");
        entries.append(groupingUpdated ? "Y" : "N");

        if (creating == false)
            emit updateGroup(entries);
        else
            emit newGroup(entries, g);

        QDialog::accept();

    }
}

void NewsGroupDialog::selectDir( )
{
	 QString dir = QFileDialog::getExistingDirectory(this, tr("Please select a directory"),
			 m_saveDir->text().trimmed(),
			 QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	if (!dir.isEmpty())
		m_saveDir->setText(dir);
}

void NewsGroupDialog::aliasChanged(const QString& aliasText)
{
	m_saveDir->setText(config->decodeDir + '/' + aliasText);
}

void NewsGroupDialog::slotAddClicked( )
{
	CatMan *catMan = new CatMan(this);
	connect(catMan, SIGNAL(newCat(QString)), this, SLOT(slotNewCat(QString)));
	catMan->exec();
}

void NewsGroupDialog::slotNewCat( QString cat)
{
	if (!cats.contains(cat) && cat != "None")
	{
		emit addCat(cat);
		cats.append(cat);
		m_catCombo->insertItem(m_catCombo->count()+1, cat);
		m_catCombo->setCurrentIndex(m_catCombo->findText(cat));
	}
	else
		QMessageBox::warning(this, tr("Error"), tr("Duplicate category"));
}

void NewsGroupDialog::slotToggleDay(bool )
{
	if (numDaysRetention->isChecked() )
		daySpin->setEnabled(true);
	else
		daySpin->setEnabled(false);
}

void NewsGroupDialog::slotToggleDownloadDay(bool )
{
	if (numDaysRetention_from_download->isChecked() )
		daySpin_2->setEnabled(true);
	else
		daySpin_2->setEnabled(false);
}

void NewsGroupDialog::slotGroupHeadersStateChanged(int newState)
{
    if (newState == Qt::Checked)
    {
        headerGroupingWidget->headerGroupBox->setEnabled(true);
        if (creating)
            headerGroupingWidget->regroupButton->setEnabled(false); // Can't do this until the group is created
    }
    else
    {
        headerGroupingWidget->headerGroupBox->setEnabled(false);
    }
}

void NewsGroupDialog::slotRegroup()
{
    ng->groupHeaders();

    // Launch a bulk job to group headers
    emit sigGroupHeaders(ng);
}

void NewsGroupDialog::slotTestGrouping()
{
    HeaderGroupingSettings settings;

    settings.maxMatchDistance = headerGroupingWidget->maxDistanceSpinBox->value();
    settings.dontUseREs = headerGroupingWidget->noRegexCheckBox->isChecked();
    settings.RE1 = headerGroupingWidget->groupRE1_Value->text();
    settings.indexRE1 = headerGroupingWidget->groupRE1_Combo->currentIndex();
    settings.RE2 = headerGroupingWidget->groupRE2_Value->text();
    settings.indexRE2 = headerGroupingWidget->groupRE2_Combo->currentIndex();
    settings.RE3 = headerGroupingWidget->groupRE3_Value->text();
    settings.indexRE3 = headerGroupingWidget->groupRE3_Combo->currentIndex();

    HeaderGroupTestDialog* headerGroupTestDialog = new HeaderGroupTestDialog(ng, settings);

    connect(headerGroupTestDialog, SIGNAL(sigAccept(HeaderGroupingSettings&)), this, SLOT(amendGrouping(HeaderGroupingSettings&)));

    headerGroupTestDialog->exec();
}

void NewsGroupDialog::amendGrouping(HeaderGroupingSettings& settings)
{
    headerGroupingWidget->maxDistanceSpinBox->setValue(settings.maxMatchDistance);
    headerGroupingWidget->noRegexCheckBox->setChecked(settings.dontUseREs);
    headerGroupingWidget->groupRE1_Value->setText(settings.RE1);
    headerGroupingWidget->groupRE1_Combo->setCurrentIndex(settings.indexRE1);
    headerGroupingWidget->groupRE2_Value->setText(settings.RE2);
    headerGroupingWidget->groupRE2_Combo->setCurrentIndex(settings.indexRE2);
    headerGroupingWidget->groupRE3_Value->setText(settings.RE3);
    headerGroupingWidget->groupRE3_Combo->setCurrentIndex(settings.indexRE3);

    groupingUpdated = true;
}

void NewsGroupDialog::help()
{
  HelpDialog* helpDialog = new HelpDialog();
  QString htmlText;

  if (creating)
  {
      htmlText =
              "<html>"
              "<body style=\"font-family:'Cantarell'; font-size:11pt; font-weight:400; font-style:normal;\">"
              "<p align=\"center\"><span style=\"font-weight:600; text-decoration: underline;\">Create Newsgroup</span></p>";
  }
  else // updating
  {
      htmlText =
              "<html>"
              "<body style=\"font-family:'Cantarell'; font-size:11pt; font-weight:400; font-style:normal;\">"
              "<p align=\"center\"><span style=\"font-weight:600; text-decoration: underline;\">Update Newsgroup</span></p>";
  }

  htmlText.append(
  "<p></p>"
  "<p><ul>"

  "<li>Name (Read Only)"
  "<ul><li>This is the name of the subscribed to newsgroup - it cannot be edited</li></ul>"
  "</li>"

  "<li>Alias"
  "<ul><li>This is an alias for the newsgroup. It can be edited and will be used as the 'display' name within the application</li></ul>"
  "</li>"

  "<li>Category"
  "<ul><li>A category is like a folder to group together related groups.</li>"
  "<li>Categories can be created here or a previously created category can be selected</li></ul>"
  "</li>"

  "<li>Save Directory"
  "<ul><li>The save directory is the place that downloaded articles are stored.</li>"
  "<li>By default it is set to the download directory from your config settings plus the alias name, but this can be amended</li></ul>"
  "</li>"

  "<li>Ignore unsequenced articles"
  "<ul><li>Ignore articles that don't appear to be part of a sequence such as '001/101'</li></ul>"
  "</li>"

  "<li>Name database using alias"
  "<ul><li>The newsgroup database(s) will be named using the alias rather than the newsgroup name</li></ul>"
  "</li>"

  "<li>Group related articles"
  "<ul><li>Quban always groups article parts together</li>"
  "<li>This 'beta' option allows related articles to be grouped together for display purposes  ... to reduce the number of items displayed</li>"
  "<li>This has performance benefits as well as providing a more concise list .. when done properly</li></ul>"
  "</li>"

  "<li>Max distance for match"
  "<ul><li>This shows the tolerance in subject matching. The lower the number the closer the match must be.</li></ul>"
  "</li>");


  if (!creating)  // updating
  {
      htmlText.append(
  "<li>Test grouping ..."
  "<ul><li>Display a window to show the results of various user defined grouping options</li></ul>"
  "</li>"

  "<li>Regroup now"
  "<ul><li>Run a batch job to regroup articles now based on the current settings</li></ul>"
  "</li>"
                  );
  }

   htmlText.append(

  "<li>Don't use regular expressions"
  "<ul><li>Attempt to match article subject names exactly as they appear rather than a subset of the name</li></ul>"
  "</li>"

  "<li>Grouping regular expression 1"
  "<ul><li>The first regular expression used to determine the match string</li></ul>"
  "</li>"

   "<li>Grouping regular expression 2"
   "<ul><li>The second regular expression used to determine the match string. Applied to the result of regular expression 1 filtering</li>"
   "<li>May be empty.</li></ul>"
   "</li>"

   "<li>Grouping regular expression 3"
   "<ul><li>The third regular expression used to determine the match string. Applied to the result of regular expression 2 filtering</li>"
   "<li>May be empty.</li></ul>"
   "</li>"

   "<li>Default retention period"
   "<ul><li>Specify how long articles should be retained for. Either so many days from posting or download date or forever</li></ul>"
   "</li>"

  "</ul></p>"
  "</body></html>");

  helpDialog->setHtmlText(htmlText);
  helpDialog->show();
}
