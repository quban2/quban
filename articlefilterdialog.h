#ifndef ARTICLEFILTERDIALOG_H
#define ARTICLEFILTERDIALOG_H
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
#include <QList>
#include "common.h"
#include "ui_articlefilterdialog.h"

typedef struct
{
    quint8  columnNo;
    quint8  filterType;
    QString filter;
    bool    caseSensitive;
}
FilterLine;

typedef struct
{
    bool andComponents;
    QList<FilterLine*> filterLines;
}
FilterComponents;

class ArticleFilterDialog : public QDialog, private Ui::ArticleFilterDialog
{
    Q_OBJECT

public:
    explicit ArticleFilterDialog(QString& alias, bool _showFrom, bool _showDate, QWidget *parent = 0);
    ~ArticleFilterDialog();

private:
    quint8 numberFilters;
    bool showFrom;
    bool showDate;

    FilterComponents filterComponents;

    void commonCheckBox(bool);

protected slots:
    void filter();
    void reject();
    void slotSubjectChecked(bool);
    void slotAuthorChecked(bool);
    void slotBytesChecked(bool);
    void slotAgeChecked(bool);
    void slotDownloadAgeChecked(bool);
    void slotReadChecked(bool);

signals:
    void multifilter(FilterComponents&);

};

#endif // ARTICLEFILTERDIALOG_H
