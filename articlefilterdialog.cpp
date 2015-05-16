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
#include "articlefilterdialog.h"

ArticleFilterDialog::ArticleFilterDialog(QString& alias, bool _showFrom, bool _showDate, QWidget *parent) :
    QDialog(parent)
{
    setupUi(this);

    filterComponents.andComponents = true;

    numberFilters = 0;
    showFrom = _showFrom;
    showDate = _showDate;

    fullGroupBox->setTitle(QString("Newsgroup : " + alias));

    connect(filterPushButton, SIGNAL(clicked()), this, SLOT(filter()));
    connect(cancelPushButton, SIGNAL(clicked()), this, SLOT(reject()));
    connect(subjectCheckBox, SIGNAL(toggled(bool)), this, SLOT(slotSubjectChecked(bool)));
    if (showFrom)
    {
        connect(authorCheckBox, SIGNAL(toggled(bool)), this, SLOT(slotAuthorChecked(bool)));
    }
    else
    {
        authorCheckBox->setHidden(true);
        authorComboBox->setHidden(true);
        authorLineEdit->setHidden(true);
        fromCaseSensitiveCheckBox->setHidden(true);
    }
    connect(kbytesCheckBox, SIGNAL(toggled(bool)), this, SLOT(slotBytesChecked(bool)));
    if (showDate)
    {
        connect(ageCheckBox, SIGNAL(toggled(bool)), this, SLOT(slotAgeChecked(bool)));
    }
    else
    {
        ageCheckBox->setHidden(true);
        ageComboBox->setHidden(true);
        ageLineEdit->setHidden(true);
    }
    connect(downloadAgeCheckBox, SIGNAL(toggled(bool)), this, SLOT(slotDownloadAgeChecked(bool)));
    connect(readCheckBox, SIGNAL(toggled(bool)), this, SLOT(slotReadChecked(bool)));
}

ArticleFilterDialog::~ArticleFilterDialog()
{

}

void ArticleFilterDialog::reject()
{
  QDialog::reject();
}

void ArticleFilterDialog::slotSubjectChecked(bool checked)
{
        commonCheckBox(checked);

        if (checked)
        {
            subjectComboBox->setEnabled(true);
            subjectLineEdit->setEnabled(true);
            subjectCaseSensitiveCheckBox->setEnabled(true);
        }
        else
        {
            subjectComboBox->setEnabled(false);
            subjectLineEdit->setEnabled(false);
            subjectCaseSensitiveCheckBox->setEnabled(false);
        }
}

void ArticleFilterDialog::slotAuthorChecked(bool checked)
{
    commonCheckBox(checked);

    if (checked)
    {
        authorComboBox->setEnabled(true);
        authorLineEdit->setEnabled(true);
        fromCaseSensitiveCheckBox->setEnabled(true);
    }
    else
    {
        authorComboBox->setEnabled(false);
        authorComboBox->setEnabled(false);
        fromCaseSensitiveCheckBox->setEnabled(false);
    }
}

void ArticleFilterDialog::slotBytesChecked(bool checked)
{
    commonCheckBox(checked);

    if (checked)
    {
        kbytesComboBox->setEnabled(true);
        kbytesLineEdit->setEnabled(true);
    }
    else
    {
        kbytesComboBox->setEnabled(false);
        kbytesLineEdit->setEnabled(false);
    }
}

void ArticleFilterDialog::slotAgeChecked(bool checked)
{
    commonCheckBox(checked);

    if (checked)
    {
        ageComboBox->setEnabled(true);
        ageLineEdit->setEnabled(true);
    }
    else
    {
        ageComboBox->setEnabled(false);
        ageLineEdit->setEnabled(false);
    }
}


void ArticleFilterDialog::slotDownloadAgeChecked(bool checked)
{
    commonCheckBox(checked);

    if (checked)
    {
        downloadAgeComboBox->setEnabled(true);
        downloadAgeLineEdit->setEnabled(true);
    }
    else
    {
        downloadAgeComboBox->setEnabled(false);
        downloadAgeLineEdit->setEnabled(false);
    }
}

void ArticleFilterDialog::slotReadChecked(bool checked)
{
    commonCheckBox(checked);

    if (checked)
        readComboBox->setEnabled(true);
    else
        readComboBox->setEnabled(false);
}

void ArticleFilterDialog::commonCheckBox(bool checked)
{
    quint8 prevFilters = numberFilters;

    if (checked)
        ++numberFilters;
    else
        --numberFilters;

    if (numberFilters == 1 && prevFilters == 0) // enable the common bits of the dialog
    {
        filterPushButton->setEnabled(true);
    }

    if (numberFilters == 0 && prevFilters == 1) // disable the common bits of the dialog
    {
        filterPushButton->setEnabled(false);
    }

    if (numberFilters < 2) // switch off the and/or
    {
        AndComboBox->setEnabled(false);
        andOrLabel->setEnabled(false);
    }
    else
    {
        AndComboBox->setEnabled(true);
        andOrLabel->setEnabled(true);
    }
}

void ArticleFilterDialog::filter()
{
    FilterLine* filterLine;

    if (AndComboBox->currentIndex() == 0)
        filterComponents.andComponents = true;
    else
        filterComponents.andComponents = false;

    filterComponents.filterLines.clear();

    if (subjectCheckBox->isChecked())
    {
        filterLine = new FilterLine;
        filterLine->columnNo = CommonDefs::Subj_Col;
        if (subjectComboBox->currentIndex() == 0)
            filterLine->filterType = CommonDefs::CONTAINS;
        else
            filterLine->filterType = CommonDefs::EQUALS;
        filterLine->filter = subjectLineEdit->text();
        if (subjectCaseSensitiveCheckBox->isChecked())
            filterLine->caseSensitive = true;
        else
            filterLine->caseSensitive = false;
        filterComponents.filterLines.append(filterLine);
    }

    if (authorCheckBox->isChecked())
    {
        filterLine = new FilterLine;
        filterLine->columnNo = CommonDefs::From_Col;
        if (authorComboBox->currentIndex() == 0)
            filterLine->filterType = CommonDefs::CONTAINS;
        else
            filterLine->filterType = CommonDefs::EQUALS;
        filterLine->filter = authorLineEdit->text();
        if (fromCaseSensitiveCheckBox->isChecked())
            filterLine->caseSensitive = true;
        else
            filterLine->caseSensitive = false;
        filterComponents.filterLines.append(filterLine);
    }

    if (kbytesCheckBox->isChecked())
    {
        filterLine = new FilterLine;
        filterLine->columnNo = CommonDefs::KBytes_Col;
        if (kbytesComboBox->currentIndex() == 0)
            filterLine->filterType = CommonDefs::LESS_THAN;
        else if (kbytesComboBox->currentIndex() == 1)
            filterLine->filterType = CommonDefs::MORE_THAN;
        else
            filterLine->filterType = CommonDefs::EQUALS;
        filterLine->filter = kbytesLineEdit->text();
        filterLine->caseSensitive = true;
        filterComponents.filterLines.append(filterLine);
    }

    if (ageCheckBox->isChecked())
    {
        filterLine = new FilterLine;
        filterLine->columnNo = CommonDefs::Date_Col;
        if (ageComboBox->currentIndex() == 0)
            filterLine->filterType = CommonDefs::MORE_THAN;
        else if (ageComboBox->currentIndex() == 1)
            filterLine->filterType = CommonDefs::LESS_THAN;
        else
            filterLine->filterType = CommonDefs::EQUALS;
        filterLine->filter = ageLineEdit->text();
        filterLine->caseSensitive = true;
        filterComponents.filterLines.append(filterLine);
    }

    if (downloadAgeCheckBox->isChecked())
    {
        filterLine = new FilterLine;
        filterLine->columnNo = CommonDefs::Download_Col;
        if (downloadAgeComboBox->currentIndex() == 0)
            filterLine->filterType = CommonDefs::MORE_THAN;
        else if (downloadAgeComboBox->currentIndex() == 1)
            filterLine->filterType = CommonDefs::LESS_THAN;
        else
            filterLine->filterType = CommonDefs::EQUALS;
        filterLine->filter = downloadAgeLineEdit->text();
        filterLine->caseSensitive = true;
        filterComponents.filterLines.append(filterLine);
    }

    if (readCheckBox->isChecked())
    {
        filterLine = new FilterLine;
        filterLine->columnNo = CommonDefs::No_Col;
        if (readComboBox->currentIndex() == 0)
            filterLine->filterType = CommonDefs::READ;
        else
            filterLine->filterType = CommonDefs::NOT_READ;
        filterComponents.filterLines.append(filterLine);
    }

    emit multifilter(filterComponents);
}
