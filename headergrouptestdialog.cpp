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

#include <string.h>
#include "helpdialog.h"
#include "headergroupingwidget.h"
#include "headergrouptestdialog.h"

HeaderGroupTestDialog::HeaderGroupTestDialog(NewsGroup *_ng, HeaderGroupingSettings &settings, QWidget *parent) :
    QDialog(parent)
{
    setupUi(this);

    ng = _ng;   

    config = Configuration::getConfig();
    numUngroupedArticles = config->groupCheckCount;

    headerGroupingWidget = new HeaderGroupingWidget();
    regexLayout->addWidget(headerGroupingWidget);

    headerGroupingWidget->regroupButton->setHidden(true);
    headerGroupingWidget->testGroupingPushButton->setHidden(true);

    previousSettings = settings;
    currentSettings  = settings;

    setSettings();

    buildTrees();

    connect(applyPushButton, SIGNAL(clicked()), this, SLOT(apply()));
    connect(resetPushButton, SIGNAL(clicked()), this, SLOT(reset()));

    connect(acceptPushButton, SIGNAL(clicked()), this, SLOT(accept()));
    connect(closePushButton, SIGNAL(clicked()), this, SLOT(reject()));

    connect(helpPushButton, SIGNAL(clicked()), this, SLOT(help()));
}

HeaderGroupTestDialog::~HeaderGroupTestDialog()
{

}

void HeaderGroupTestDialog::reject()
{
  QDialog::reject();
}

void HeaderGroupTestDialog::getSettings()
{
    currentSettings.maxMatchDistance = headerGroupingWidget->maxDistanceSpinBox->value();
    currentSettings.dontUseREs = headerGroupingWidget->noRegexCheckBox->isChecked();
    currentSettings.advancedGrouping = headerGroupingWidget->advanceGroupingCheckBox->isChecked();
    currentSettings.RE1 = headerGroupingWidget->groupRE1_Value->text();
    currentSettings.indexRE1 = headerGroupingWidget->groupRE1_Combo->currentIndex();
    currentSettings.RE2 = headerGroupingWidget->groupRE2_Value->text();
    currentSettings.indexRE2 = headerGroupingWidget->groupRE2_Combo->currentIndex();
    currentSettings.RE3 = headerGroupingWidget->groupRE3_Value->text();
    currentSettings.indexRE3 = headerGroupingWidget->groupRE3_Combo->currentIndex();
}

void HeaderGroupTestDialog::setSettings()
{
    headerGroupingWidget->maxDistanceSpinBox->setValue(currentSettings.maxMatchDistance);
    headerGroupingWidget->noRegexCheckBox->setChecked(currentSettings.dontUseREs);
    headerGroupingWidget->advanceGroupingCheckBox->setChecked(currentSettings.advancedGrouping);
    headerGroupingWidget->groupRE1_Value->setText(currentSettings.RE1);
    headerGroupingWidget->groupRE1_Combo->setCurrentIndex(currentSettings.indexRE1);
    headerGroupingWidget->groupRE2_Value->setText(currentSettings.RE2);
    headerGroupingWidget->groupRE2_Combo->setCurrentIndex(currentSettings.indexRE2);
    headerGroupingWidget->groupRE3_Value->setText(currentSettings.RE3);
    headerGroupingWidget->groupRE3_Combo->setCurrentIndex(currentSettings.indexRE3);
}

void HeaderGroupTestDialog::buildTrees()
{
    ungroupedArticles.reserve(numUngroupedArticles);
    ungroupedFrom.reserve(numUngroupedArticles);

    SinglePartHeader sph;
    Dbt key, data;
    key.set_flags(DB_DBT_USERMEM);
    key.set_ulen(KEYMEM_SIZE);
    key.set_data(keymem);

    data.set_flags(DB_DBT_USERMEM);
    data.set_ulen(DATAMEM_SIZE);
    data.set_data(datamem);

    Dbc *cursor;
    ng->getDb()->cursor(0, &cursor, 0);

    int i=0;

    char *splitpoint, *keyData, *dataData;
    quint32 keySize;

    QString articleName,
            from;

    while((cursor->get(&key, &data, DB_NEXT))==0 && i <numUngroupedArticles)
    {
        keyData = (char *)(key.get_data());
        keySize = (quint32)key.get_size();
        dataData = (char *)(data.get_data());

        if ((splitpoint = (char *)memchr(keyData, '\n', keySize)))
        {
            articleName = QString::fromLocal8Bit(keyData, (splitpoint - keyData));
            if (*((char *)dataData) == 'm') // multiPart
            {
                from = QString::fromLocal8Bit(splitpoint + 1, keyData + (keySize - 1) - splitpoint);
            }
            else if (*((char *)dataData) == 's') // SinglePart
            {
                SinglePartHeader::getSinglePartHeader((unsigned int)keySize, (char *)keyData, (char *)dataData, &sph);
                from = sph.getFrom();
            }
            else
                from = articleName;
        }
        else
        {
            articleName = QString::fromLocal8Bit(keyData, keySize);
            from = articleName;
        }     

        if (articleName.length())
        {
            ++i;
            ungroupedArticles.append(articleName);
            ungroupedFrom.append(from);

            QTreeWidgetItem *nameItem = new QTreeWidgetItem(ungroupedTreeWidget);
            nameItem->setText(0, articleName);
        }
    }

    cursor->close();

    ungroupedLabel->setText(tr("Number ungrouped articles : ") + QString::number(i));

    buildGroupedTree();
}

void HeaderGroupTestDialog::buildGroupedTree()
{
    groupedTreeWidget->clear();

    QString subj =  "MDQuban", from = "MDQuban";

    //QString rs1 = "^(.*)(\".*\")";
    //QString rs2 = "^(.*)\\s-\\s(.*)$";
    //QString rs3 = "^(\\S+.*)\\[.*\\].*(\".*\")";

    //QString rs3 = "^(.*)\\s-\\s.*\\s-\\s(.*)$";

    QRegExp rx[3];
    bool    rxPosBack[3];
    bool    noRegexpGrouping;

    QString prevSubj = "MDQuban2", prevFrom = "MDQuban2";

    int pos;
    bool newGroup = false;

    quint32 numGroups = 0;

    quint16 stringDiff = 9999,
            minGrouped = 9999,
            maxGrouped = 0,
            numGrouped = 0;

    QTreeWidgetItem *nameItem = 0;

    noRegexpGrouping = currentSettings.dontUseREs;

    if (noRegexpGrouping == false) // need regex for grouping
    {
        rx[0].setPattern(currentSettings.RE1);
        rx[1].setPattern(currentSettings.RE2);
        rx[2].setPattern(currentSettings.RE3);

        rxPosBack[0] = currentSettings.indexRE1;
        rxPosBack[1] = currentSettings.indexRE2;
        rxPosBack[2] = currentSettings.indexRE3;
    }

    for (int i = 0; i < ungroupedArticles.size(); ++i)
    {
        prevSubj = subj;
        prevFrom = from;

        subj = ungroupedArticles.at(i);
        from = ungroupedFrom.at(i);

        if (noRegexpGrouping == false) // need regex for grouping
        {
            for (int i=0; i<3; ++i)
            {
                if (rx[i].isEmpty() == false)
                {
                    if (rxPosBack[i] == true) // from the back
                    {
                        pos = subj.lastIndexOf(rx[i]);
                        if (pos != -1)
                            subj.truncate(pos);
                    }
                    else // from the front
                    {
                        pos = rx[i].indexIn(subj);
                        if (pos > -1)
                            subj = rx[i].cap(0);
                    }
                }
            }
        }

        //qDebug() << "Stripped down to: " << subj;

        if (prevFrom != from) // change of contributor
        {
            newGroup = true;
            stringDiff = 9999;
        }
        else // same contributor
        {
            if ((stringDiff = levenshteinDistance(prevSubj, subj)) > currentSettings.maxMatchDistance) // no match ...
                newGroup = true;
            else
                newGroup = false;

            //qDebug() << "Diff between " << prevSubj << " and " << subj << " is " << stringDiff;
        }

        ++numGrouped;

        if (newGroup)
        {
            minGrouped = qMin(minGrouped, numGrouped);
            maxGrouped = qMax(maxGrouped, numGrouped);
            numGrouped = 0;

            nameItem = new QTreeWidgetItem(groupedTreeWidget);
            nameItem->setText(1, subj);
            nameItem->setText(0, QString::number(stringDiff));

            numGroups++;
        }
        else
        {
            // pin it to the top level item
            QTreeWidgetItem *subItem = new QTreeWidgetItem(nameItem);
            subItem->setText(1, subj);
            subItem->setText(0, QString::number(stringDiff));
        }
    }

    groupedLabel->setText(tr("Number grouped articles : ") + QString::number(numGroups));
    minArticlesLabel->setText(tr("Minimum articles per Group : ") + QString::number(minGrouped));
    maxArticlesLabel->setText(tr("Maximum articles per Group : ") + QString::number(maxGrouped));
}

qint16 HeaderGroupTestDialog::levenshteinDistance(const QString& a_compare1, const QString& a_compare2)
{
    const qint16 length1 = a_compare1.size();
    const qint16 length2 = a_compare2.size();
    QVector<qint16> curr_col(length2 + 1);
    QVector<qint16> prev_col(length2 + 1);

    // Prime the previous column for use in the following loop:
    for (qint16 idx2 = 0; idx2 < length2 + 1; ++idx2)
    {
        prev_col[idx2] = idx2;
    }

    for (qint16 idx1 = 0; idx1 < length1; ++idx1)
    {
        curr_col[0] = idx1 + 1;

        for (qint16 idx2 = 0; idx2 < length2; ++idx2)
        {
            const qint16 compare = a_compare1[idx1] == a_compare2[idx2] ? 0 : 1;

            curr_col[idx2 + 1] = qMin(qMin(curr_col[idx2] + 1, prev_col[idx2 + 1] + 1),
                    prev_col[idx2] + compare);
        }

#if defined(Q_OS_OS2) || defined(Q_OS_OS2EMX) // In qglobal.h
    QVector<qint16> swap_col(length2 + 1);

    swap_col = prev_col;
    prev_col = curr_col;
    curr_col = swap_col;
#else
     curr_col.swap(prev_col);
#endif
    }

    return prev_col[length2];
}

void HeaderGroupTestDialog::apply()
{
    previousSettings = currentSettings;
    getSettings();
    buildGroupedTree();
}

void HeaderGroupTestDialog::reset()
{
    currentSettings = previousSettings;
    setSettings();
    buildGroupedTree();
}

void HeaderGroupTestDialog::accept()
{
    emit sigAccept(currentSettings);
    QDialog::accept();
}

void HeaderGroupTestDialog::help()
{
  HelpDialog* helpDialog = new HelpDialog(this);
  QString htmlText;

  htmlText =
  "<html>"
  "<body style=\"font-family:'Cantarell'; font-size:11pt; font-weight:400; font-style:normal;\">"
  "<p align=\"center\"><span style=\"font-weight:600; text-decoration: underline;\">Article Grouping Display</span></p>"
  "<p></p>"
  "<p><ul>"

  "<li>Ungrouped articles"
  "<ul><li>This list contains the first <nnnn> (default 1000) articles from the newsgroup</li>"
  "<li>The number of articles can be adjusted in the Header List preferences</li></ul>"
  "</li>"

  "<li>Grouped articles"
  "<ul><li>This list contains the result of grouping the 'ungrouped article' list using the grouping parameters below this list</li>"
  "<li>The distance shown is the computed match distance (a value of 9999 signifies a change of author the 'Max distance for match'</li>"
  "<li>To include more items in the groupings increase the 'Max distance for match' : less selective matching</li>"
  "<li>To reduce the number of items in the groupings decrease the 'Max distance for match' : more selective matching</li></ul>"
  "</li>"

  "<li>Grouping Details"
  "<ul><li>This area displays a number of statistics relating to the grouping :</li>"
  "<li>The number of ungrouped articles</li>"
  "<li>The number of grouped articles</li>"
  "<li>The minimum number of articles per group</li>"
  "<li>The maximum number of articles per group</li></ul>"
  "</li>"

  "<li>Group related articles"
  "<ul><li>Quban always groups article parts together</li>"
  "<li>This 'beta' option allows related articles to be grouped together for display purposes  ... to reduce the number of items displayed</li>"
  "<li>This has performance benefits as well as providing a more concise list .. when done properly</li></ul>"
  "</li>"

  "<li>Max distance for match"
  "<ul><li>This shows the tolerance in subject matching. The lower the number the closer the match must be.</li></ul>"
  "</li>"
  "<li>Test grouping ..."
  "<ul><li>Display a window to show the results of various user defined grouping options</li></ul>"
  "</li>"

  "<li>Regroup now"
  "<ul><li>Run a batch job to regroup articles now based on the current settings</li></ul>"
  "</li>"

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

  "<li>Button details"
  "<ul><li>Help : display this information</li>"
  "<li>Apply : apply the current grouping settings to the ungrouped list (window stays open)</li>"
  "<li>Reset : apply the previous grouping settings to the ungrouped list (window stays open)</li>"
  "<li>Accept : close the window and keep any changes</li>"
  "<li>Close : close the window and discard any changes</li></ul>"
  "</li>"

  "</ul></p>"
  "</body></html>";

  helpDialog->setHtmlText(htmlText);
  helpDialog->show();
}
