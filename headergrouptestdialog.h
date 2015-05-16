#ifndef HEADERGROUPTESTDIALOG_H
#define HEADERGROUPTESTDIALOG_H
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

#include <QDialog>
#include "newsgroup.h"
#include "appConfig.h"
#include "ui_headergrouptestdialog.h"

typedef struct
{
    qint16  maxMatchDistance;
    bool    dontUseREs;
    QString RE1;
    quint8  indexRE1;
    QString RE2;
    quint8  indexRE2;
    QString RE3;
    quint8  indexRE3;
} HeaderGroupingSettings;

class HeaderGroupingWidget;

class HeaderGroupTestDialog : public QDialog, private Ui::HeaderGroupTestDialog
{
    Q_OBJECT

public:
    explicit HeaderGroupTestDialog(NewsGroup *_ng, HeaderGroupingSettings& settings, QWidget *parent = 0);
    ~HeaderGroupTestDialog();

private:

    void getSettings();
    void setSettings();
    void buildTrees();

    void buildGroupedTree();
    qint16 levenshteinDistance(const QString& a_compare1, const QString& a_compare2);

    NewsGroup* ng;
    Configuration* config;
    qint32 numUngroupedArticles;

    HeaderGroupingWidget* headerGroupingWidget;

    HeaderGroupingSettings previousSettings;
    HeaderGroupingSettings currentSettings;

    QList<QString> ungroupedArticles;
    QList<QString> ungroupedFrom;

    char datamem[DATAMEM_SIZE];
    char keymem[KEYMEM_SIZE];

protected slots:
    void accept();
    void reset();

    void apply();
    void reject();

    void help();

signals:
    void sigAccept(HeaderGroupingSettings&);

};

#endif // HEADERGROUPTESTDIALOG_H
