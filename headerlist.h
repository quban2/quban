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

#ifndef HEADERLIST_H
#define HEADERLIST_H

#include <QTabWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QMultiHash>
#include <QMenu>
#include <QList>
#include <QProgressDialog>
#include "downloadselect.h"
#include "unpack.h"
#include "newsgroup.h"
#include "SinglePartHeader.h"
#include "headerlistwidget.h"
#include "articlefilterdialog.h"
#include "hlistviewitem.h"

class QToolBar;

class HeaderList: public QTabWidget
{
	Q_OBJECT

	friend class BulkLoad;

    void loadHeaders(Db* db);
    void loadGroups(Db* db);
	QStringList rxList;
  	NewsGroup *ng;
  	Servers *servers;
    HeaderTreeView *m_headerList;
	HeaderTreeModel* headerTreeModel;
	HeaderSortFilterProxyModel* headerProxy;

	Dbt key, data;
	uchar *keymem;
	uchar *datamem;

	bool loadCancelled;
	bool deleteCancelled;

	void markSelectedAs(int);
	bool showOnlyNew, showOnlyComplete;
	void filter(const QString &);
	void markAllAsRead(Db*);
	void markGroupAsRead();
	void saveSettings();

	//Keep these options here because these *need* to be synchronized with the bhlistviewitem...
	bool showFrom;
	bool showDetails;
	bool showDate;
	int area(QString s);
	void adjColSize(int col);

private:

	//char headerIndex[MPH_MAX_STRING_SIZE + 1];

	// Auto unpack data
	quint32             headerGroupId;

    HeaderListWidget* headerListWidget;
    QToolBar           *toolBarTab_4;

    int                sortColumn;
    Qt::SortOrder      sortOrder;

    // These two will be freed by the bulk delete thread if used
    QList<QString>* mphDeletionsList;
    QList<QString>* sphDeletionsList;

    bool bulkLoading;

    QProgressDialog* pd;

    Configuration* config;

    void downloadSelectedHeader(bool first, bool view, QString dir, QModelIndex sourceSubjIndex, QString index, bool multiPartHeader);
    void confirmSelectedHeader(QModelIndex sourceSubjIndex, QString index, bool multiPartHeader, UnpackConfirmation *unpackConfirmation);

public:
	HeaderList(NewsGroup *_ng, Servers *_servers, QString caption, QWidget* parent=0);
	virtual ~HeaderList();
	bool prepareToClose(bool confirmation=true);
    bool onlyNew() {return showOnlyNew;}
	bool onlyComplete() {return showOnlyComplete;}
	void closeAndMark();
	void closeAndNoMark();
	NewsGroup *getNg() {return ng;}
	QTreeView *getTree() { return m_headerList;}
	QMenu* getMenu() {return contextMenu;}
    HeaderListWidget* getHeaderListWidget() { return headerListWidget; }

	virtual void closeEvent(QCloseEvent *e);

	QMenu* contextMenu;

	// MD TODO make these private ...
    quint64 bulkLoadSeq;
    quint64 bulkDeleteSeq;

private slots:
	void slotClearButtonClicked();
	void slotEnterButtonClicked();
    void slotAdvancedFilterButtonClicked();

public slots:
	void slotViewArticle();
	void slotDownloadSelected(bool first=false, bool view=false, QString dir=QString::null);
	void slotDownloadSelectedFirst();
	void slotMarkSelectedAsRead();
	void slotDelSelected();
	void slotDownloadToDir();
	void slotGroupAndDownload();
	void slotMarkSelectedAsUnread();
	void slotShowOnlyComplete();
	void slotShowOnlyNew();
	void slotDownloadToFirst(QString, bool);
	void slotListParts();
	void slotGroupArticles();
	void slotIndexChanged(int, int, int);
	void slotClearFilter();
    void slotCopyToFilter();
	void slotFocusFilter();
	void slotColSizeChanged(int, int, int);
	void cancelLoad();
	void cancelDelete();

	void slotSelectionChanged();
	void slotHeaders_Toolbar(bool);

	void slotSortClicked(int);

	void slotProgress(quint64, quint64);
    void slotCancelCurrentBulkLoad();

	void bulkLoadFinished(quint64);
	void bulkLoadFailed(quint64);
    void bulkLoadCancelled(quint64 seq);
	void slotBulkLoadStarted(quint64);

    void slotFilterArticles(bool);

    void compoundFilter(FilterComponents &);

protected:
	void delSelectedSmall();

protected slots:
  /*$PROTECTED_SLOTS$*/
// 	void slotActivateFilter(const QString&);
	void slotItemExecuted();

signals:
	void downloadPost(HeaderBase*, NewsGroup*, bool first, bool view, QString);
	void viewPost(HeaderBase*, NewsGroup*);
	void updateFinished(NewsGroup*);
	void sigSaveSettings(NewsGroup *, bool, bool);
	void updateArticleCounts(NewsGroup*);
};

#endif

