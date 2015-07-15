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

#include "headerlist.h"
#include "appConfig.h"
#include <QCloseEvent>
#include <QList>
#include <QMap>
#include <QIcon>
#include <QDebug>
#include <QtDebug>
#include <QMessageBox>
#include <QHeaderView>
#include <qregexp.h>
#include <qstringlist.h>
#include <QStringBuilder>
#include "quban.h"
#include "qmgr.h"
#include "common.h"
#include "bulkDelete.h"
#include "bulkLoad.h"
#include "headerGroup.h"
#include "autounpackconfirmation.h"

extern Quban* quban;
extern DbEnv *g_dbenv;

/* Review the database in 5MB chunks. */

HeaderList::HeaderList(NewsGroup *_ng, Servers *_servers, QString, QWidget *parent)
: QTabWidget (parent), ng(_ng), servers(_servers)
{
    config = Configuration::getConfig();

	keymem=new uchar[KEYMEM_SIZE];
	datamem=new uchar[DATAMEM_SIZE];

	key.set_flags(DB_DBT_USERMEM);
	key.set_data(keymem);
	key.set_ulen(KEYMEM_SIZE);

	data.set_flags(DB_DBT_USERMEM);
	data.set_ulen(DATAMEM_SIZE);
	data.set_data(datamem);

	mphDeletionsList = 0;
	sphDeletionsList = 0;

	pd = 0;

	bulkLoadSeq = 0;
	bulkDeleteSeq = 0;

	showFrom=config->showFrom;
	showDetails=config->showDetails;
	showDate=config->showDate;

	QVBoxLayout *topHLayout = new QVBoxLayout(this);
	headerListWidget = new HeaderListWidget();
	topHLayout->addWidget(headerListWidget);

	headerListWidget->h_layout->setContentsMargins(0, 36, 0, 0);
    toolBarTab_4 = new QToolBar(this);
    toolBarTab_4->setObjectName(QString::fromUtf8("toolBarTab_4"));

    toolBarTab_4->setMovable(false);
    toolBarTab_4->addAction(quban->actionDownload_selected);
    toolBarTab_4->addAction(quban->actionView_article);
    toolBarTab_4->addSeparator();
    toolBarTab_4->addAction(quban->actionShow_only_complete_articles);
    toolBarTab_4->addAction(quban->actionShow_only_unread_articles);
    toolBarTab_4->addSeparator();
    toolBarTab_4->addAction(quban->actionFilter_articles);
    toolBarTab_4->addSeparator();
    toolBarTab_4->resize(200, 36);
    slotHeaders_Toolbar(config->showHeaderBar);

    headerListWidget->filterColCombo->addItem(tr("Subject"), CommonDefs::Subj_Col);
    headerListWidget->filterColCombo->addItem(tr("Parts"), CommonDefs::Parts_Col);
    headerListWidget->filterColCombo->addItem(tr("KBytes"), CommonDefs::KBytes_Col);
    if (showFrom)
        headerListWidget->filterColCombo->addItem(tr("From"), CommonDefs::From_Col);
    if (showDate)
    headerListWidget->filterColCombo->addItem(tr("Posting Date"), CommonDefs::Date_Col);
    headerListWidget->filterColCombo->addItem(tr("Download Date"), CommonDefs::Download_Col);

    headerListWidget->filterSyntaxComboBox->addItem(tr("Fixed string"), QRegExp::FixedString);
    headerListWidget->filterSyntaxComboBox->addItem(tr("Regular expression"), QRegExp::RegExp);
    headerListWidget->filterSyntaxComboBox->addItem(tr("Wildcard"), QRegExp::Wildcard);

    m_headerList=new HeaderTreeView(headerListWidget);
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
    m_headerList->header()->setMovable(false);
#else
    m_headerList->header()->setSectionsMovable(false);
#endif

	headerListWidget->listLayout->addWidget(m_headerList);

    headerTreeModel = new HeaderTreeModel(servers, ng->getDb(), ng->getPartsDb(), ng->getGroupingDb(), this,
                                          ng->areHeadersGrouped() ? ng->getTotalGroups() : ng->getTotal());

	m_headerList->setAllColumnsShowFocus(true);
	m_headerList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_headerList->setUniformRowHeights(true);

	if (!showDetails)
		servers = NULL;

	showOnlyNew=ng->onlyUnread();
	showOnlyComplete=ng->onlyComplete();

	headerProxy = new HeaderSortFilterProxyModel(showOnlyNew, showOnlyComplete, this);
	headerProxy->setSourceModel(headerTreeModel);
    headerProxy->setPerformAnd(true);

    if (!ng->areHeadersGrouped())
        loadHeaders(ng->getDb());
    else
        loadGroups(ng->getGroupingDb());

	if (!bulkLoading)
	{        
		m_headerList->header()->setSortIndicatorShown(true);
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
		m_headerList->header()->setClickable(true);
#else
        m_headerList->header()->setSectionsClickable(true);
#endif

        if (config->rememberSort && !((ng->getSortColumn() == CommonDefs::Subj_Col) && (ng->getSortOrder() == true)))
		{
			if (ng->getSortOrder() == true)
				sortOrder = Qt::AscendingOrder;
			else
				sortOrder = Qt::DescendingOrder;

			sortColumn = ng->getSortColumn();
			m_headerList->header()->setSortIndicator(sortColumn, sortOrder);

			headerProxy->sort(sortColumn, sortOrder);
		}
		else
		{
            sortColumn = CommonDefs::Subj_Col;
			sortOrder  = Qt::AscendingOrder;
			m_headerList->header()->setSortIndicator(sortColumn, sortOrder);
		}

		qDebug() << "At load, will sort headers by col " <<  sortColumn << " and order " << sortOrder;

		if (showOnlyComplete || showOnlyNew)
			filter(headerListWidget->m_filterEdit->text());

		m_headerList->setModel(headerProxy);

		connect(m_headerList->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)), this, SLOT(slotSelectionChanged()));
	}

	connect(m_headerList->header(), SIGNAL(sectionClicked(int)), this, SLOT(slotSortClicked(int)));
	connect(headerListWidget->m_filterEdit, SIGNAL(returnPressed()), this, SLOT(slotEnterButtonClicked()));
	connect(headerListWidget->m_enterButton, SIGNAL(clicked()), this, SLOT(slotEnterButtonClicked()));
    connect(headerListWidget->advancedFilterButton, SIGNAL(clicked()), this, SLOT(slotAdvancedFilterButtonClicked()));
    connect(headerListWidget->m_clearButton, SIGNAL(clicked()), this, SLOT(slotClearButtonClicked()));

	connect(m_headerList->header(), SIGNAL(sectionMoved(int, int, int)), this, SLOT(slotIndexChanged(int, int, int)));
	connect(m_headerList->header(), SIGNAL(sectionResized(int, int, int)), this, SLOT(slotColSizeChanged(int, int, int)));

	if (!config->showFrom)
        m_headerList->hideColumn(CommonDefs::From_Col);
	if (!config->showDate)
        m_headerList->hideColumn(CommonDefs::Date_Col);
	if (!config->showDetails)
	{
        for (int i=5; i<=headerTreeModel->columnCount()+1; i++)
        	m_headerList->hideColumn(i);
	}

	if( config->rememberWidth )
	{
        adjColSize(CommonDefs::Subj_Col);
        adjColSize(CommonDefs::Parts_Col);
        adjColSize(CommonDefs::KBytes_Col);
		if (showFrom)
            adjColSize(CommonDefs::From_Col);
		if (showDate)
            adjColSize(CommonDefs::Date_Col);
        adjColSize(CommonDefs::Download_Col);
	}
	else
	{
	    for (int j=0; j < headerTreeModel->columnCount(); ++j)
	    	m_headerList->resizeColumnToContents(j);
	}

	contextMenu = new QMenu(m_headerList);
	m_headerList->setContextMenuPolicy(Qt::ActionsContextMenu);

    m_headerList->setUpdatesEnabled(true);

	headerListWidget->m_filterEdit->setEnabled(true);

	m_headerList->setAutoScroll(false);
	m_headerList->setFocus();
}

HeaderList::~ HeaderList( )
{
	qDebug() << "In HeaderList::~ HeaderList for " << ng->getAlias();

	ng->setView(NULL);
	ng->getDb()->sync(0);

    Q_DELETE(contextMenu);
	headerTreeModel->removeAllRows();
	headerTreeModel->deleteLater();
    Q_DELETE(m_headerList);

    Q_DELETE_ARRAY(keymem);
    Q_DELETE_ARRAY(datamem);
}

void HeaderList::slotSortClicked(int col)
{
	if (bulkLoading)
	{
		qDebug() << "INTERNAL ERROR: bulk load is trying to sort ...";
		return;
	}

	qDebug() << "Request to sort by col " << col;

	if (sortColumn == col) // same column so reverse sort, otherwise keep order and change column
	{
		if (sortOrder == Qt::AscendingOrder)
			sortOrder = Qt::DescendingOrder;
		else
			sortOrder = Qt::AscendingOrder;
	}

	sortColumn = col;

	qDebug() << "About to sort headers by col " <<  sortColumn << " and order " << sortOrder;

	m_headerList->header()->setSortIndicator(sortColumn, sortOrder);

	headerProxy->sort(sortColumn, sortOrder);
}

void HeaderList::loadHeaders(Db* db)
{
 	bulkLoading = false;

#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
        m_headerList->header()->setClickable(false);
#else
        m_headerList->header()->setSectionsClickable(false);
#endif
	m_headerList->setRootIsDecorated(true);
    m_headerList->setUpdatesEnabled(false);

	mphDeletionsList = 0;
	sphDeletionsList = 0;

    QString progMessage(tr("Loading %1 ...").arg(ng->getAlias()));
 	quban->statusBar()->showMessage(progMessage);

    pd = new QProgressDialog(tr("Loaded ") +
            "0 of " + QString("%L1").arg(ng->getTotal()) +
            " articles.", tr("Cancel"), 0, ng->getTotal());
	pd->setWindowTitle(tr("Load articles"));

    pd->show();

    if (ng->getTotal() > (quint64)(config->bulkLoadThreshold))
 	{
 		bulkLoadSeq = quban->getQMgr()->startBulkLoad(ng, this);
 		bulkLoading = true;
        connect(pd, SIGNAL(canceled()), this, SLOT(slotCancelCurrentBulkLoad()));
 		return;
 	}

 	connect(pd, SIGNAL(canceled()), this, SLOT(cancelLoad()));

    DBC *dbcp = 0;
	DBT ckey, cdata;

	MultiPartHeader mph;
	SinglePartHeader sph;
    HeaderBase* hb = 0;

	memset(&ckey, 0, sizeof(ckey));
	memset(&cdata, 0, sizeof(cdata));

    size_t retklen, retdlen;
    void *retkey = 0, *retdata = 0;
    int ret, t_ret;
    void *p = 0;

    quint64 count=0;

 	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	QTime start = QTime::currentTime();
	qDebug() << "Loading started: " << start.toString();
	loadCancelled = false;

    cdata.data = (void *) new char[HEADER_BULK_BUFFER_LENGTH];
    cdata.ulen = HEADER_BULK_BUFFER_LENGTH;
    cdata.flags = DB_DBT_USERMEM;

    ckey.data = (void *) new char[HEADER_BULK_BUFFER_LENGTH];
    ckey.ulen = HEADER_BULK_BUFFER_LENGTH;
    ckey.flags = DB_DBT_USERMEM;

    /* Acquire a cursor for the database. */
    if ((ret = db->get_DB()->cursor(db->get_DB(), NULL, &dbcp, DB_CURSOR_BULK)) != 0)
    {
        db->err(ret, "DB->cursor");
        Q_DELETE_ARRAY_NO_LVALUE((char*)(ckey.data));
        Q_DELETE_ARRAY_NO_LVALUE((char*)(cdata.data));
        return;
    }

    quint32 numIgnored = 0;
    bool    mphFound = false;

	for (;;)
	{
		/*
		 * Acquire the next set of key/data pairs.  This code
		 * does not handle single key/data pairs that won't fit
		 * in a BUFFER_LENGTH size buffer, instead returning
		 * DB_BUFFER_SMALL to our caller.
		 */
		if ((ret = dbcp->get(dbcp, &ckey, &cdata, DB_MULTIPLE_KEY | DB_NEXT)) != 0)
		{
			if (ret != DB_NOTFOUND)
				db->err(ret, "DBcursor->get");
			break;
		}

		for (DB_MULTIPLE_INIT(p, &cdata);;)
		{
			DB_MULTIPLE_KEY_NEXT(p, &cdata, retkey, retklen, retdata, retdlen);
			if (p == NULL)
				break;

            if (retdlen){;} // MD TODO compiler .... unused variable

			if (*((char *)retdata) == 'm')
			{
                MultiPartHeader::getMultiPartHeader((unsigned int)retklen, (char *)retkey, (char *)retdata, &mph);
			    hb = (HeaderBase*)&mph;

			    mphFound = true;
			}
			else if (*((char *)retdata) == 's')
			{
                SinglePartHeader::getSinglePartHeader((unsigned int)retklen, (char *)retkey, (char *)retdata, &sph);
				hb = (HeaderBase*)&sph;
				mphFound = false;
				// qDebug() << "Single index = " << sph.getIndex();
			}
			else
			{
				// What have we found ?????
				qDebug() << "Found unexpected identifier for header : " << (char)*((char *)retdata);
				continue;
			}

            if (hb->getStatus() & HeaderBase::MarkedForDeletion)
			{
				// ignore this header, garbage collection will remove it ...
				++numIgnored;
				++count; // ... but still count it as bulk delete will reduce the count

				if (numIgnored == 1)
				{
					// These two will be freed by bulk delete
					mphDeletionsList = new QList<QString>;
					sphDeletionsList = new QList<QString>;
				}

				if (mphFound)
					mphDeletionsList->append(hb->getIndex());
				else
					sphDeletionsList->append(hb->getIndex());

				continue;
			}

			headerTreeModel->setupTopLevelItem(hb);
			++count;

            if (count % 40 == 0)
			{
				pd->setValue(count);
                pd->setLabelText(tr("Loaded ") + QString("%L1").arg(count) +
                                    " of " + QString("%L1").arg(ng->getTotal()) +
                                    " articles.");
				QCoreApplication::processEvents();
			}

			if (loadCancelled)
				break;
		}

		if (loadCancelled)
			break;
	}

	if ((t_ret = dbcp->close(dbcp)) != 0)
	{
		db->err(ret, "DBcursor->close");
		if (ret == 0)
			ret = t_ret;
	}

    Q_DELETE_ARRAY_NO_LVALUE((char*)(ckey.data));
    Q_DELETE_ARRAY_NO_LVALUE((char*)(cdata.data));

	qDebug() << "Loading finished. Seconds: " << start.secsTo(QTime::currentTime());
	qDebug() << "Ignored " << numIgnored << " articles";
	quban->statusBar()->showMessage(progMessage + " : 100%", 3000);

	pd->setValue(count);
    pd->setLabelText(tr("Loaded ") + QString("%L1").arg(count) +
                        " of " + QString("%L1").arg(ng->getTotal()) +
                        " articles.");
	QCoreApplication::processEvents();

	if (!loadCancelled)
	{
		quint64 totalArticles = ng->getTotal();
        if (totalArticles != count || ng->unread() > count)
		{
			qint64 difference = count - totalArticles; // may be negative
			quint64 unread = (quint64)qMax((qint64)(ng->unread() + difference), (qint64)0);

            if (unread > count)
                unread = count;

			ng->setTotal(count);
			ng->setUnread(unread);

			emit updateArticleCounts(ng);
		}
	}

	if (mphDeletionsList) // need bulk deletion
		bulkDeleteSeq = quban->getQMgr()->startBulkDelete(ng, this, mphDeletionsList, sphDeletionsList);

 	QApplication::restoreOverrideCursor();

    Q_DELETE(pd);
 	pd = 0;
}

void HeaderList::loadGroups(Db* db)
{
    bulkLoading = false;

#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
        m_headerList->header()->setClickable(false);
#else
        m_headerList->header()->setSectionsClickable(false);
#endif
    m_headerList->setRootIsDecorated(true);
    m_headerList->setUpdatesEnabled(false);

    QString progMessage(tr("Loading %1 groups ...").arg(ng->getAlias()));
    quban->statusBar()->showMessage(progMessage);

    pd = new QProgressDialog(tr("Loaded ") +
            "0 of " + QString("%L1").arg(ng->getTotalGroups()) +
            " groups.", tr("Cancel"), 0, ng->getTotalGroups());
    pd->setWindowTitle(tr("Load article groups"));

    pd->show();

/* MD TODO possibly ???
 *
    if (ng->getTotalGroups() > (quint64)(config->bulkLoadThreshold))
    {
        bulkLoadSeq = quban->getQMgr()->startBulkLoad(ng, this);
        bulkLoading = true;
        connect(pd, SIGNAL(canceled()), this, SLOT(slotCancelCurrentBulkLoad()));
        return;
    }
*/

    connect(pd, SIGNAL(canceled()), this, SLOT(cancelLoad()));

    DBC *dbcp = 0;
    DBT ckey, cdata;

    HeaderGroup headerGroup;

    memset(&ckey, 0, sizeof(ckey));
    memset(&cdata, 0, sizeof(cdata));

    size_t retklen, retdlen;
    void *retkey = 0, *retdata = 0;
    int ret, t_ret;
    void *p = 0;

    quint64 count=0;

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    QTime start = QTime::currentTime();
    qDebug() << "Loading started: " << start.toString();
    loadCancelled = false;

    cdata.data = (void *) new char[HEADER_BULK_BUFFER_LENGTH];
    cdata.ulen = HEADER_BULK_BUFFER_LENGTH;
    cdata.flags = DB_DBT_USERMEM;

    ckey.data = (void *) new char[HEADER_BULK_BUFFER_LENGTH];
    ckey.ulen = HEADER_BULK_BUFFER_LENGTH;
    ckey.flags = DB_DBT_USERMEM;

    /* Acquire a cursor for the database. */
    if ((ret = db->get_DB()->cursor(db->get_DB(), NULL, &dbcp, DB_CURSOR_BULK)) != 0)
    {
        db->err(ret, "DB->cursor");
        Q_DELETE_ARRAY_NO_LVALUE((char*)(ckey.data));
        Q_DELETE_ARRAY_NO_LVALUE((char*)(cdata.data));
        return;
    }

    for (;;)
    {
        /*
         * Acquire the next set of key/data pairs.  This code
         * does not handle single key/data pairs that won't fit
         * in a BUFFER_LENGTH size buffer, instead returning
         * DB_BUFFER_SMALL to our caller.
         */
        if ((ret = dbcp->get(dbcp, &ckey, &cdata, DB_MULTIPLE_KEY | DB_NEXT)) != 0)
        {
            if (ret != DB_NOTFOUND)
                db->err(ret, "DBcursor->get");
            break;
        }

        for (DB_MULTIPLE_INIT(p, &cdata);;)
        {
            DB_MULTIPLE_KEY_NEXT(p, &cdata, retkey, retklen, retdata, retdlen);
            if (p == NULL)
                break;

            if (retdlen){;} // MD TODO compiler .... unused variable

            HeaderGroup::getHeaderGroup((unsigned int)retklen, (char *)retkey, (char *)retdata, &headerGroup);

            headerTreeModel->setupTopLevelGroupItem(&headerGroup);
            ++count;

            if (count % 40 == 0)
            {
                pd->setValue(count);
                pd->setLabelText(tr("Loaded ") + QString("%L1").arg(count) +
                                    " of " + QString("%L1").arg(ng->getTotalGroups()) +
                                    " groups.");
                QCoreApplication::processEvents();
            }

            if (loadCancelled)
                break;
        }

        if (loadCancelled)
            break;
    }

    if ((t_ret = dbcp->close(dbcp)) != 0)
    {
        db->err(ret, "DBcursor->close");
        if (ret == 0)
            ret = t_ret;
    }

    Q_DELETE_ARRAY_NO_LVALUE((char*)(ckey.data));
    Q_DELETE_ARRAY_NO_LVALUE((char*)(cdata.data));

    qDebug() << "Loading finished. Seconds: " << start.secsTo(QTime::currentTime());
    quban->statusBar()->showMessage(progMessage + " : 100%", 3000);

    pd->setValue(count);
    pd->setLabelText(tr("Loaded ") + QString("%L1").arg(count) +
                        " of " + QString("%L1").arg(ng->getTotalGroups()) +
                        " groups.");
    QCoreApplication::processEvents();

    /*
    if (!loadCancelled)
    {
        quint64 totalArticles = ng->getTotal();
        if (totalArticles != count || ng->unread() > count)
        {
            qint64 difference = count - totalArticles; // may be negative
            quint64 unread = (quint64)qMax((qint64)(ng->unread() + difference), (qint64)0);

            if (unread > count)
                unread = count;

            ng->setTotal(count);
            ng->setUnread(unread);

            emit updateArticleCounts(ng);
        }
    }
    */

    QApplication::restoreOverrideCursor();

    Q_DELETE(pd);
    pd = 0;
}

void HeaderList::slotProgress(quint64 seq, quint64 count, QString newLabelText)
{
	if (bulkLoadSeq == seq && pd)
    {
		pd->setValue(count);
        pd->setLabelText(newLabelText);
    }
}

void HeaderList::slotCancelCurrentBulkLoad()
{
    quban->getQMgr()->slotCancelBulkLoad(bulkLoadSeq);

    Q_DELETE(pd);
}

void HeaderList::bulkLoadFinished(quint64 seq)
{
	if (bulkLoadSeq == seq)
	{
        Q_DELETE(pd);

		QString progMessage(tr("Loading %1 ...").arg(ng->getAlias()));
		quban->statusBar()->showMessage(progMessage + " : 100%", 3000);

		quban->getLogEventList()->logEvent(tr("Bulk load of ") + ng->getAlias() + tr(" completed successfully"));

		m_headerList->header()->setSortIndicatorShown(true);

#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
        m_headerList->header()->setClickable(true);
#else
        m_headerList->header()->setSectionsClickable(true);
#endif

		Configuration* config = Configuration::getConfig();

        if (config->rememberSort && !((ng->getSortColumn() == CommonDefs::Subj_Col) && (ng->getSortOrder() == true)))
		{
			if (ng->getSortOrder() == true)
				sortOrder = Qt::AscendingOrder;
			else
				sortOrder = Qt::DescendingOrder;

			sortColumn = ng->getSortColumn();
			m_headerList->header()->setSortIndicator(sortColumn, sortOrder);

			headerProxy->sort(sortColumn, sortOrder);
		}
		else
		{
            sortColumn = CommonDefs::Subj_Col;
			sortOrder  = Qt::AscendingOrder;
			m_headerList->header()->setSortIndicator(sortColumn, sortOrder);
		}

		qDebug() << "At load, will sort headers by col " <<  sortColumn << " and order " << sortOrder;

		if (showOnlyComplete || showOnlyNew)
        {
            headerProxy->setPerformAnd(true);
			filter(headerListWidget->m_filterEdit->text());
        }

	    for (int j=0; j < headerTreeModel->columnCount(); ++j)
	    	m_headerList->resizeColumnToContents(j);

	    bulkLoading = false;
	}
}

void HeaderList::bulkLoadFailed(quint64 seq)
{
	if (bulkLoadSeq == seq)
	{
        Q_DELETE(pd);

		quban->getLogAlertList()->logMessage(LogAlertList::Error, tr("Error during bulk load of ") + ng->getAlias());
	}
}

void HeaderList::bulkLoadCancelled(quint64 seq)
{
    if (bulkLoadSeq == seq)
    {
        Q_DELETE(pd);

        quban->getLogEventList()->logEvent(tr("Bulk load of ") + ng->getAlias() + " cancelled");
    }
}

void HeaderList::slotBulkLoadReady(quint64 seq)
{
	if (bulkLoadSeq != seq)
		return;

	m_headerList->setModel(headerProxy);
	connect(m_headerList->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)), this, SLOT(slotSelectionChanged()));

    for (int j=0; j < headerTreeModel->columnCount(); ++j)
    	m_headerList->resizeColumnToContents(j);
}

void HeaderList::slotHeaders_Toolbar(bool checked)
{
    if (checked)
    {
		toolBarTab_4->setHidden(false);
		toolBarTab_4->setEnabled(true);
		headerListWidget->h_layout->setContentsMargins(0, 36, 0, 0);
    }
    else
    {
		toolBarTab_4->setHidden(true);
		toolBarTab_4->setEnabled(false);
		headerListWidget->h_layout->setContentsMargins(0, 0, 0, 0);
    }
}

void HeaderList::cancelLoad()
{
	loadCancelled = true;
}

void HeaderList::slotListParts()
{
	if (quban->getMainTab()->currentWidget() != this)
		return;

    QList<QModelIndex> selectedSubjects = m_headerList->selectionModel()->selectedRows(CommonDefs::Subj_Col);
    QModelIndex sourceIndex;

    //m_headerList->setSortingEnabled(false);

#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
        m_headerList->header()->setClickable(false);
#else
        m_headerList->header()->setSectionsClickable(false);
#endif

    for (int i = 0; i < selectedSubjects.size(); ++i)
    {
    	sourceIndex = headerProxy->mapToSource(selectedSubjects.at(i));

        if (headerTreeModel->getItem(sourceIndex)->getType() == HeaderTreeItem::HEADER ||
            headerTreeModel->getItem(sourceIndex)->getType() == HeaderTreeItem::HEADER_GROUP)
		{
			headerTreeModel->setupChildItems(headerTreeModel->getItem(sourceIndex));
			m_headerList->setExpanded(selectedSubjects.at(i), true);
		}
	}

    if (selectedSubjects.size())
	    m_headerList->setCurrentIndex(selectedSubjects.at(0));
	m_headerList->resizeColumnToContents(0);
	//m_headerList->setSortingEnabled(true);

#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
        m_headerList->header()->setClickable(true);
#else
        m_headerList->header()->setSectionsClickable(true);
#endif
}

HeaderGroup* HeaderList::getGroup(QString& articleIndex)
{
    HeaderGroup *hg = 0;

    int ret;
    Dbt groupkey;
    Dbt groupdata;
    memset(&groupkey, 0, sizeof(groupkey));
    memset(&groupdata, 0, sizeof(groupdata));
    groupdata.set_flags(DB_DBT_MALLOC);

    QByteArray ba = articleIndex.toLocal8Bit();
    const char *k= ba.constData();
    groupkey.set_data((void*)k);
    groupkey.set_size(articleIndex.length());

    Db* groupsDb = ng->getGroupingDb();
    ret=groupsDb->get(NULL, &groupkey, &groupdata, 0);
    if (ret != 0) //key not found
    {
        qDebug() << "Failed to find group with key " << articleIndex;
    }
    else
    {
        qDebug() << "Found group with key " << articleIndex;

        hg=new HeaderGroup(articleIndex.length(), (char*)k, (char*)groupdata.get_data());
        void* ptr = groupdata.get_data();
        Q_FREE(ptr);
    }

    return hg;
}

void HeaderList::slotDownloadSelected(bool first, bool view, QString dir)
{
    if (quban->getMainTab()->currentWidget() != this)
        return;

    // belt and braces
    key.set_data(keymem);
    data.set_data(datamem);

    QList<QModelIndex> selectedSubjects = m_headerList->selectionModel()->selectedRows(CommonDefs::Subj_Col);
    QList<QModelIndex> selectedFroms    = m_headerList->selectionModel()->selectedRows(CommonDefs::From_Col);
    QModelIndex sourceSubjIndex;
    QModelIndex sourceFromIndex;

    int i = 0;

    if (first)
        i = selectedSubjects.size() - 1;

    QString progMessage(tr("Adding items to download queue"));
    QString index;
    QString msgId;
    bool multiPartHeader = false;
    QList<QString>hbKeys;

    quban->statusBar()->showMessage(progMessage, 3000);

    while ((!first && i < selectedSubjects.size()) || (first && i >= 0))
    {
        sourceSubjIndex = headerProxy->mapToSource(selectedSubjects.at(i));
        sourceFromIndex = headerProxy->mapToSource(selectedFroms.at(i));

        if (headerTreeModel->getItem(sourceSubjIndex)->getType() == HeaderTreeItem::HEADER)
        {
            msgId = headerTreeModel->getItem(sourceSubjIndex)->getMsgId();

            if (msgId.isNull()) // it's a multi part header
            {
                multiPartHeader = true;
                index = sourceSubjIndex.data().toString() % "\n" % sourceFromIndex.data().toString();
            }
            else // it's a single part header
            {
                multiPartHeader = false;
                index = sourceSubjIndex.data().toString() % "\n" % msgId;
            }

            downloadSelectedHeader(first, view, dir, sourceSubjIndex, index, multiPartHeader);
        }
        else if (headerTreeModel->getItem(sourceSubjIndex)->getType() == HeaderTreeItem::HEADER_GROUP)
        {
            QString articleIndex = sourceSubjIndex.data().toString() % "\n" % sourceFromIndex.data().toString();

            HeaderGroup *hg = getGroup(articleIndex);
            if (!hg)
                return;

            hbKeys = hg->getSphKeys();

            for (int h=0; h<hbKeys.count(); ++h)
            {
                qDebug() << "Found sph with key " << hbKeys.at(h);

                downloadSelectedHeader(first, view, dir, sourceSubjIndex, hbKeys.at(h), false);
            }

            hbKeys = hg->getMphKeys();

            for (int h=0; h<hbKeys.count(); ++h)
            {
                qDebug() << "Found mph with key " << hbKeys.at(h);

                downloadSelectedHeader(first, view, dir, sourceSubjIndex, hbKeys.at(h), true);
            }
        }
        else
            qDebug() << "Tried to download msg part - ignored";

        if (!first)
            ++i;
        else
            --i;
    }

    m_headerList->resizeColumnToContents(0);
    quban->statusBar()->showMessage(tr("Finished adding items to download queue"), 3000);

    emit updateFinished(ng);
}

void HeaderList::downloadSelectedHeader(bool first, bool view, QString dir, QModelIndex sourceSubjIndex, QString index, bool multiPartHeader)
{
    MultiPartHeader *mph = 0;
    SinglePartHeader* sph = 0;
    HeaderBase* hb = 0;

    uint count=0;
    char *p = 0;
    QByteArray ba;

    int ret;
    count++;

    // qDebug() << "Index = " << index;
    ba = index.toLocal8Bit();
    const char *k= ba.constData();
    memcpy(keymem, k, index.length());
    key.set_size(index.length());
    ret=ng->getDb()->get(NULL, &key, &data, 0);

    if (ret == DB_BUFFER_SMALL)
    {
        //Grow key and repeat the query
        qDebug("Insufficient memory");
        qDebug("Size is: %d", data.get_size());
        uchar *p=datamem;
        datamem=new uchar[data.get_size()+1000];
        data.set_ulen(data.get_size()+1000);
        data.set_data(datamem);
        Q_DELETE_ARRAY(p);
        qDebug("Exiting growing array cycle");
        ret=ng->getDb()->get(0,&key, &data, 0);
    }
    if (ret==0)
    {
        if (multiPartHeader)
        {
            mph = new MultiPartHeader(index.length(), (char*)k, (char*)datamem);
            hb = (HeaderBase*)mph;
        }
        else
        {
            sph = new SinglePartHeader(index.length(), (char*)k, (char*)datamem);
            hb = (HeaderBase*)sph;
        }

        if (view)
            emit viewPost(hb, ng);
        else
            emit downloadPost(hb, ng, first, false, dir);

        //Now, mark the header as read, save it and change the appearance...
        if (hb->getStatusIgnoreMark() == HeaderBase::bh_new)
            ng->decUnread();
        hb->setStatus(HeaderBase::bh_downloaded );
        headerTreeModel->getItem(sourceSubjIndex)->setStatus(HeaderBase::bh_downloaded, headerTreeModel);

        p=hb->data();
        data.set_data(p);
        data.set_size(hb->getRecordSize());
        ret=ng->getDb()->put(NULL, &key, &data, 0);
        if (ret!=0)
            qDebug("Error updating record: %d", ret);

        data.set_data(datamem);
        Q_DELETE_ARRAY(p);
    }
    else
        qDebug() << "Error retrieving record: " << ret;
}

bool HeaderList::prepareToClose(bool confirmation)
{
	if (!confirmation)
	{
		ng->setView(NULL);
		return true;
	}

	QString dbQuestion = tr("Do you want to mark all messages in\n ") + ng->name() + tr("\nas read ?");
	int result = QMessageBox::question(this, tr("Question"), dbQuestion, QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel);

    switch (result)
    {
		case QMessageBox::Yes:
			//Mark all messages as read
			markAllAsRead( ng->getDb()  );
			qDebug("Marked all as read");
		case QMessageBox::No:
			//do nothing
			ng->setView(NULL);
			return true;
			break;
		case QMessageBox::Cancel:
			//don't exit (return false?)
			return false;
			break;
	}

	return false;
}

void HeaderList::slotDelSelected()
{
	if (quban->getMainTab()->currentWidget() != this)
		return;

    QList<QModelIndex> selectedSubjects = m_headerList->selectionModel()->selectedRows(CommonDefs::Subj_Col);
    QList<QModelIndex> selectedFroms    = m_headerList->selectionModel()->selectedRows(CommonDefs::From_Col);
    QModelIndex sourceSubjIndex;
    QModelIndex sourceFromIndex;

	if (selectedSubjects.size() == 0)
	{
		quban->getLogAlertList()->logMessage(LogAlertList::Alert, tr("No articles selected for deletion"));
		return;
	}

    if (selectedSubjects.size() < config->bulkDeleteThreshold)
		return delSelectedSmall();

	ng->articlesNeedDeleting(true);

	QString index;

	//m_headerList->setSortingEnabled(false);

#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
        m_headerList->header()->setClickable(false);
#else
        m_headerList->header()->setSectionsClickable(false);
#endif

	//These are used to get Header details
	uchar* keymem2=new uchar[KEYMEM_SIZE/2];
	uchar* datamem2=new uchar[DATAMEM_SIZE];

	Dbt key2;
	key2.set_flags(DB_DBT_USERMEM);
	key2.set_data((void*)keymem2);
	key2.set_ulen(KEYMEM_SIZE);

	Dbt data2;
	data2.set_flags(DB_DBT_USERMEM);
	data2.set_ulen(DATAMEM_SIZE);
	data2.set_data((void*)datamem2);

	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	int numDeleted = 0;

	deleteCancelled = false;
	QProgressDialog* pd = new QProgressDialog(tr("Deleting ") + QString::number(selectedSubjects.size()) +
			" articles.", tr("Cancel"), 0, selectedSubjects.size());
	pd->setWindowTitle(tr("Delete articles"));

    connect(pd, SIGNAL(canceled()), this, SLOT(cancelDelete()));
    pd->show();

    m_headerList->setUpdatesEnabled(false);

    QString msgId;
    int i;
    bool multiPart = false;
    QList<QString>hbKeys;

    for (i=0; i<selectedSubjects.size(); ++i)
    {
		sourceSubjIndex = headerProxy->mapToSource(selectedSubjects.at(i));

		//qDebug() << "About to delete row " << thisRow << " from database";

		sourceSubjIndex = headerProxy->mapToSource(selectedSubjects.at(i));
		sourceFromIndex = headerProxy->mapToSource(selectedFroms.at(i));

        if (headerTreeModel->getItem(sourceSubjIndex)->getType() == HeaderTreeItem::HEADER)
		{
			msgId = headerTreeModel->getItem(sourceSubjIndex)->getMsgId();

			if (msgId.isNull()) // it's a multi part header
			{
			    index = sourceSubjIndex.data().toString() % "\n" % sourceFromIndex.data().toString();
			    //qDebug() << "Multi part index is " << index;
			    multiPart = true;
			}
			else // it's a single part header
			{
				index = sourceSubjIndex.data().toString() % "\n" % msgId;
				//qDebug() << "Single part index is " << index;
				multiPart = false;
			}

            markForDeletion(index, multiPart, keymem2, datamem2, key2, data2, sourceSubjIndex);
		}
        else if (headerTreeModel->getItem(sourceSubjIndex)->getType() == HeaderTreeItem::HEADER_GROUP)
        {
            index = sourceSubjIndex.data().toString() % "\n" % sourceFromIndex.data().toString();

            HeaderGroup *hg = getGroup(index);
            if (!hg)
                return;

            hbKeys = hg->getSphKeys();

            for (int h=0; h<hbKeys.count(); ++h)
            {
                qDebug() << "Found sph with key " << hbKeys.at(h);

                QString thisIndex = hbKeys.at(h);
                markForDeletion(thisIndex, false, keymem2, datamem2, key2, data2, sourceSubjIndex);
            }

            hbKeys = hg->getMphKeys();

            for (int h=0; h<hbKeys.count(); ++h)
            {
                qDebug() << "Found mph with key " << hbKeys.at(h);

                QString thisIndex = hbKeys.at(h);
                markForDeletion(thisIndex, true, keymem2, datamem2, key2, data2, sourceSubjIndex);
            }
        }

        if ((++numDeleted) % 50 == 0)
        {
            pd->setValue(numDeleted);
            QCoreApplication::processEvents();
        }

		if (deleteCancelled)
			break;
	}

	//m_headerList->setSortingEnabled(true);

#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
        m_headerList->header()->setClickable(true);
#else
        m_headerList->header()->setSectionsClickable(true);
#endif

	pd->setValue(numDeleted);
	QCoreApplication::processEvents();

	m_headerList->setUpdatesEnabled(true);
	QApplication::restoreOverrideCursor();

    Q_DELETE(pd);

	emit updateFinished(ng);
}

void HeaderList::markForDeletion(QString& index, bool multiPart, uchar* keymem2, uchar* datamem2, Dbt& key2,
                                 Dbt& data2, QModelIndex& sourceSubjIndex)
{
    int ret = 0;
    QByteArray ba;
    const char* k = 0;
    MultiPartHeader* mph = 0;
    SinglePartHeader* sph = 0;
    HeaderBase* hb = 0;
    char* pbh = 0;

    ba = index.toLocal8Bit();
    k= ba.data();

    memcpy(keymem2, k, index.length());
    key2.set_size(index.length());

    ret=ng->getDb()->get(NULL, &key2, &data2, 0);

    //qDebug() << "Get record returned " << ret << " for index " << index;

    if (ret == DB_BUFFER_SMALL)
    {
        //Grow key and repeat the query
        qDebug("Insufficient memory");
        qDebug("Size is: %d", data2.get_size());
        uchar *p=datamem2;
        datamem2=new uchar[data2.get_size()+1000];
        data2.set_ulen(data2.get_size()+1000);
        data2.set_data(datamem2);
        Q_DELETE_ARRAY(p);
        qDebug("Exiting growing array cycle");
        ret=ng->getDb()->get(0,&key2, &data2, 0);
    }

    //qDebug() << "Get record (check 2) returned " << ret << " for index " << index;

    if (ret==0)
    {
        if (multiPart)
        {
            mph = new MultiPartHeader(index.length(), (char*)k, (char*)datamem2);
            hb = (HeaderBase*)mph;
        }
        else
        {
            sph = new SinglePartHeader(index.length(), (char*)k, (char*)datamem2);
            hb = (HeaderBase*)sph;
        }

        ng->decTotal();
        if (headerTreeModel->getItem(sourceSubjIndex)->getStatus() == HeaderBase::bh_new)
            ng->decUnread();

        hb->setStatus(hb->getStatus() | HeaderBase::MarkedForDeletion);
        headerTreeModel->getItem(sourceSubjIndex)->setStatus(HeaderBase::MarkedForDeletion, headerTreeModel);

        if (data2.get_size() != hb->getRecordSize()) // This should always be true ...
        {
            qDebug() << "Mismatch in article deletion ... Read size = " << data2.get_size() << ", but write size = " << hb->getRecordSize();
            return;
        }

        pbh=hb->data();
        memcpy(datamem2, pbh, hb->getRecordSize());

        Q_DELETE_ARRAY(pbh);
        Q_DELETE(hb);

        // key and data sizes are unchanged
        ret=ng->getDb()->put(0,&key2, &data2, 0);
        if(ret != 0)
            qDebug() << "Error marking header for deletion: " << g_dbenv->strerror(ret);
    }
}

void HeaderList::delSelectedSmall()
{
	if (quban->getMainTab()->currentWidget() != this)
		return;

	QMap<int, int> rowsToDelete; // row num, array index
    QList<QModelIndex> selectedSubjects = m_headerList->selectionModel()->selectedRows(CommonDefs::Subj_Col);
    QList<QModelIndex> selectedFroms    = m_headerList->selectionModel()->selectedRows(CommonDefs::From_Col);
    QModelIndex sourceSubjIndex;
    QModelIndex sourceFromIndex;

	int thisRow = 0,
	    delRow = 0,
		rowCount = 0;

    bool multiPart = false;
    QList<QString>hbKeys;

	QString index;
	int ret;

	//m_headerList->setSortingEnabled(false);

#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
        m_headerList->header()->setClickable(false);
#else
        m_headerList->header()->setSectionsClickable(false);
#endif

	Dbt mkey;

    char* keymem = 0;

    if ((keymem = (char*)malloc(HEADER_BULK_BUFFER_LENGTH)) == NULL)
    {
    	qDebug() << "Failed to alloc 5Mb buffer for bulk delete";
    	return;
    }

    memset(keymem, 0, HEADER_BULK_BUFFER_LENGTH);

	mkey.set_flags(DB_DBT_USERMEM);
	mkey.set_ulen(HEADER_BULK_BUFFER_LENGTH);
	mkey.set_data((void*)keymem);

    DbMultipleDataBuilder* keybuilder = new DbMultipleDataBuilder(mkey);

	// Build a list of all rows, we need to delete from the back and they could be sorted by sender etc, so in a random order
	for (int i = 0; i < selectedSubjects.size(); ++i)
	{
		sourceSubjIndex = headerProxy->mapToSource(selectedSubjects.at(i));
		thisRow = sourceSubjIndex.row();
		rowsToDelete.insert(thisRow, i);
		//qDebug() << "Just added row " << thisRow << " with selected array index " << i << " to rowsToDelete";
	}

	//These are used to get Header details
	uchar* keymem2=new uchar[KEYMEM_SIZE/2];
	uchar* datamem2=new uchar[DATAMEM_SIZE];

	Dbt key2;
	key2.set_flags(DB_DBT_USERMEM);
	key2.set_data((void*)keymem2);
	key2.set_ulen(KEYMEM_SIZE);

	Dbt data2;
	data2.set_flags(DB_DBT_USERMEM);
	data2.set_ulen(DATAMEM_SIZE);
	data2.set_data((void*)datamem2);

	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	int numDeleted = 0;

	deleteCancelled = false;
	QProgressDialog* pd = new QProgressDialog(tr("Deleting ") + QString::number(selectedSubjects.size()) +
			" articles.", tr("Cancel"), 0, selectedSubjects.size());
	pd->setWindowTitle(tr("Delete articles"));

    connect(pd, SIGNAL(canceled()), this, SLOT(cancelDelete()));
    pd->show();

    m_headerList->setUpdatesEnabled(false);

    QString msgId;
    int i;

    QMapIterator<int, int> it(rowsToDelete); // row num, array index
    it.toBack(); // We need to delete from the back and they could be sorted by sender etc, so in a random order
    while (it.hasPrevious())
    {
        it.previous();

        thisRow = it.key();
        i = it.value();

		//qDebug() << "About to delete row " << thisRow << " from database";

		sourceSubjIndex = headerProxy->mapToSource(selectedSubjects.at(i));
		sourceFromIndex = headerProxy->mapToSource(selectedFroms.at(i));

        if (headerTreeModel->getItem(sourceSubjIndex)->getType() == HeaderTreeItem::HEADER)
        // if (headerTreeModel->parent(sourceSubjIndex) == QModelIndex()) // top level
		{
			msgId = headerTreeModel->getItem(sourceSubjIndex)->getMsgId();     

			if (msgId.isNull()) // it's a multi part header
			{
			    index = sourceSubjIndex.data().toString() % "\n" % sourceFromIndex.data().toString();
			    //qDebug() << "Multi part index is " << index;
			    multiPart = true;
			}
			else // it's a single part header
			{
				index = sourceSubjIndex.data().toString() % "\n" % msgId;
				//qDebug() << "Single part index is " << index;
				multiPart = false;
			}

            deleteHeader(index, multiPart, true, keymem2, datamem2, key2, data2, sourceSubjIndex,
                         keybuilder, mkey, thisRow, delRow, rowCount);

            if ((++numDeleted) % 50 == 0)
            {
                pd->setValue(numDeleted);
                QCoreApplication::processEvents();
            }
        }
        else if (headerTreeModel->getItem(sourceSubjIndex)->getType() == HeaderTreeItem::HEADER_GROUP)
        {
            index = sourceSubjIndex.data().toString() % "\n" % sourceFromIndex.data().toString();

            HeaderGroup *hg = getGroup(index);
            if (!hg)
                return;

            hbKeys = hg->getSphKeys();

            for (int h=0; h<hbKeys.count(); ++h)
            {
                qDebug() << "Found sph with key " << hbKeys.at(h);

                QString thisIndex = hbKeys.at(h);
                deleteHeader(thisIndex, false, false, keymem2, datamem2, key2, data2, sourceSubjIndex,
                             keybuilder, mkey, thisRow, delRow, rowCount);
            }

            hbKeys = hg->getMphKeys();

            for (int h=0; h<hbKeys.count(); ++h)
            {
                qDebug() << "Found mph with key " << hbKeys.at(h);

                QString thisIndex = hbKeys.at(h);
                deleteHeader(thisIndex, true, false, keymem2, datamem2, key2, data2, sourceSubjIndex,
                             keybuilder, mkey, thisRow, delRow, rowCount);
            }

            thisRow = it.key();
            rowCount = 1;
            headerTreeModel->removeRows(thisRow, rowCount);
            rowCount = 0;

            if ((++numDeleted) % 50 == 0)
            {
                pd->setValue(numDeleted);
                QCoreApplication::processEvents();
            }
		}

		if (deleteCancelled)
			break;
	}

    //qDebug() << "At end, about to delete from db";

	if ((ret=ng->getDb()->del(NULL, &mkey, DB_MULTIPLE)) != 0)
	{
		ng->getDb()->err(ret, "Db::del");
		QApplication::restoreOverrideCursor();
		m_headerList->setUpdatesEnabled(true);
        Q_DELETE(pd);
        Q_DELETE(keybuilder);
        Q_FREE(keymem);
        Q_DELETE_ARRAY(keymem2);
        Q_DELETE_ARRAY(datamem2);
		emit updateFinished(ng);
		return;
	}

    //qDebug() << "At end, db deletion complete";
    //qDebug() << "At end: About to delete " << rowCount << " rows starting at " << delRow;

	if (rowCount)
	    headerTreeModel->removeRows(delRow, rowCount); // last set encountered

    Q_DELETE(keybuilder);
    Q_FREE(keymem);
    Q_DELETE_ARRAY(keymem2);
    Q_DELETE_ARRAY(datamem2);

	//m_headerList->setSortingEnabled(true);

#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
        m_headerList->header()->setClickable(true);
#else
        m_headerList->header()->setSectionsClickable(true);
#endif

	pd->setValue(numDeleted);
	QCoreApplication::processEvents();

	m_headerList->setUpdatesEnabled(true);
	QApplication::restoreOverrideCursor();

    Q_DELETE(pd);

	emit updateFinished(ng);
}

void HeaderList::deleteHeader(QString& index, bool multiPart, bool deleteFromTree, uchar* keymem2, uchar* datamem2, Dbt& key2,
                              Dbt& data2, QModelIndex& sourceSubjIndex, DbMultipleDataBuilder* keybuilder, Dbt& mkey, int& thisRow,
                              int& 	delRow, int& rowCount)
{
    int ret = 0;
    QByteArray ba;
    const char* k = 0;

    MultiPartHeader* mph = 0;
    SinglePartHeader* sph = 0;
    HeaderBase* hb = 0;

    quint16 serverId;

    //typedef QMap<quint16,quint64> ServerNumMap;     // Holds server number, article num
    //typedef QMap<quint16, ServerNumMap> PartNumMap; //Hold <part number 1, 2 etc, <serverid, articlenum>>
    PartNumMap serverArticleNos;
    QMap<quint16, quint64> partSize;
    QMap<quint16, QString> partMsgId;

    ServerNumMap::Iterator servit;
    PartNumMap::iterator pnmit;

    ba = index.toLocal8Bit();
    k= ba.data();

    memcpy(keymem2, k, index.length());
    key2.set_size(index.length());

    ret=ng->getDb()->get(NULL, &key2, &data2, 0);

    //qDebug() << "Get record returned " << ret << " for index " << index;

    if (ret == DB_BUFFER_SMALL)
    {
        //Grow key and repeat the query
        qDebug("Insufficient memory");
        qDebug("Size is: %d", data2.get_size());
        uchar *p=datamem2;
        datamem2=new uchar[data2.get_size()+1000];
        data2.set_ulen(data2.get_size()+1000);
        data2.set_data(datamem2);
        Q_DELETE_ARRAY(p);
        qDebug("Exiting growing array cycle");
        ret=ng->getDb()->get(0,&key2, &data2, 0);
    }

    //qDebug() << "Get record (check 2) returned " << ret << " for index " << index;

    if (ret==0)
    {
        if (multiPart)
        {
            mph = new MultiPartHeader(index.length(), (char*)k, (char*)datamem2);
            hb = (HeaderBase*)mph;
            hb->getAllArticleNums(ng->getPartsDb(), &serverArticleNos, &partSize, &partMsgId);
            //qDebug() << "Deleting parts for " << mph->getSubj();
            mph->deleteAllParts(ng->getPartsDb());
        }
        else
        {
            sph = new SinglePartHeader(index.length(), (char*)k, (char*)datamem2);
            hb = (HeaderBase*)sph;
            hb->getAllArticleNums(ng->getPartsDb(), &serverArticleNos, &partSize, &partMsgId);
        }

        // Don't need these two ...
        partSize.clear();
        partMsgId.clear();

        // walk through the parts ...
        for (pnmit = serverArticleNos.begin(); pnmit != serverArticleNos.end(); ++pnmit)
        {
            for (servit = pnmit.value().begin(); servit != pnmit.value().end(); ++servit)
            {
                serverId = servit.key();
                ng->reduceParts(serverId, 1);
                //qDebug() << "Reducing parts for server " << serverId << " by 1";
            }
        }

        serverArticleNos.clear();

        Q_DELETE(hb);

        if (keybuilder->append((void*)k, index.length()) == false) // failed to add to buffer
        {
            qDebug() << "In loop, about to delete from db";

            if ((ret=ng->getDb()->del(NULL, &mkey, DB_MULTIPLE)) != 0)
            {
                ng->getDb()->err(ret, "Db::del");
                QApplication::restoreOverrideCursor();
                m_headerList->setUpdatesEnabled(true);
                Q_DELETE(pd);
                Q_DELETE(keybuilder);
                Q_FREE(keymem);
                Q_DELETE_ARRAY(keymem2);
                Q_DELETE_ARRAY(datamem2);
                emit updateFinished(ng);
                return;
            }

            //qDebug() << "In loop, db deletion complete";
            Q_DELETE(keybuilder);
            memset(&keymem, 0, sizeof(keymem));
            keybuilder = new DbMultipleDataBuilder(mkey);
            if (keybuilder->append((void*)k, index.length()) == false) // failed to add single index to buffer
            {
                qDebug() << "ERROR: the following header key is too long for the buffer: " << index;
            }
        }

        ng->decTotal();
        if (headerTreeModel->getItem(sourceSubjIndex)->getStatus() == MultiPartHeader::bh_new)
            ng->decUnread();

        if (deleteFromTree)
        {
            if (thisRow == delRow - 1) // consecutive rows
            {
                ++rowCount;
            }
            else
            {
                if (rowCount)
                {
                    //qDebug() << "In loop: About to delete " << rowCount << " rows starting at " << delRow;
                    headerTreeModel->removeRows(delRow, rowCount);
                }
                rowCount = 1;
            }
            delRow = thisRow;
        }
    }
}

void HeaderList::cancelDelete()
{
	deleteCancelled = true;
}

void HeaderList::markSelectedAs( int what)
{
	if (quban->getMainTab()->currentWidget() != this)
		return;

    QList<QModelIndex> selectedSubjects = m_headerList->selectionModel()->selectedRows(CommonDefs::Subj_Col);
    QList<QModelIndex> selectedFroms    = m_headerList->selectionModel()->selectedRows(CommonDefs::From_Col);
    QModelIndex sourceSubjIndex;
    QModelIndex sourceFromIndex;

	if (selectedSubjects.size() == 0)
		return;

	// belt and braces
	key.set_data(keymem);
	data.set_data(datamem);

	QByteArray ba;

    MultiPartHeader *mph = 0;
    SinglePartHeader* sph = 0;
    HeaderBase* hb = 0;

	QString index;
    QString msgId;
    bool multiPartHeader = false;

    for (int i = 0; i < selectedSubjects.size(); ++i)
    {
		sourceSubjIndex = headerProxy->mapToSource(selectedSubjects.at(i));
		sourceFromIndex = headerProxy->mapToSource(selectedFroms.at(i));

		if (headerTreeModel->parent(sourceSubjIndex) == QModelIndex()) // top level
		{
			msgId = headerTreeModel->getItem(sourceSubjIndex)->getMsgId();

			if (msgId.isNull()) // it's a multi part header
			{
				multiPartHeader = true;
			    index = sourceSubjIndex.data().toString() % "\n" % sourceFromIndex.data().toString();
			}
			else // it's a single part header
			{
				multiPartHeader = false;
				index = sourceSubjIndex.data().toString() % "\n" % msgId;
			}

			int ret;

			ba = index.toLocal8Bit();
			const char *k= ba.constData();
			memcpy(keymem, k, index.length());
			key.set_size(index.length());

			ret=ng->getDb()->get(NULL, &key, &data, 0);

			if (ret == DB_BUFFER_SMALL)
			{
				//Grow key and repeat the query
				qDebug("Insufficient memory");
				qDebug("Size is: %d", data.get_size());
				uchar *p=datamem;
				datamem=new uchar[data.get_size()+1000];
				data.set_ulen(data.get_size()+1000);
				data.set_data(datamem);
                Q_DELETE_ARRAY(p);
				qDebug("Exiting growing array cycle");
				ret=ng->getDb()->get(0,&key, &data, 0);
			}

			if (ret==0)
			{
				if (multiPartHeader)
				{
					mph = new MultiPartHeader(index.length(), (char*)k, (char*)datamem);
				    hb = (HeaderBase*)mph;
				}
				else
				{
					sph = new SinglePartHeader(index.length(), (char*)k, (char*)datamem);
					hb = (HeaderBase*)sph;
				}

				//Now, mark the header as read, save it and change the appearence...
                if ( (hb->getStatusIgnoreMark() == HeaderBase::bh_new) && (what != HeaderBase::bh_new) )
					ng->decUnread();
                if ( (what == HeaderBase::bh_new ) && (hb->getStatusIgnoreMark() != HeaderBase::bh_new)) {
// 					qDebug("Increase");
					ng->incUnread();
				}
                hb->setStatus(what);
                headerTreeModel->getItem(sourceSubjIndex)->setStatus(what, headerTreeModel);
				char *p=hb->data();
				data.set_data(p);
				data.set_size(hb->getRecordSize());
				ret=ng->getDb()->put(NULL, &key, &data, 0);

				if (ret!=0)
					qDebug("Error updating record: %d", ret);

				data.set_data(datamem);
                Q_DELETE_ARRAY(p);
                Q_DELETE(hb);
				//Take item out of the list...
                if ( (what == MultiPartHeader::bh_read) && showOnlyNew )
					m_headerList->setRowHidden(selectedSubjects.at(i).row(), QModelIndex(), true);
			}
			else
				qDebug("Error retrieving record: %d", ret);
		}
	}

	m_headerList->resizeColumnToContents(0);
	emit updateFinished(ng);
}

void HeaderList::slotMarkSelectedAsRead()
{
    markSelectedAs(MultiPartHeader::bh_read);
}

void HeaderList::slotMarkSelectedAsUnread()
{
    markSelectedAs(MultiPartHeader::bh_new);
}

void HeaderList::slotShowOnlyComplete()
{
	//toggle
	if (showOnlyComplete)
		showOnlyComplete=false;
	else showOnlyComplete=true;

	emit sigSaveSettings(ng, showOnlyNew, showOnlyComplete);
	headerProxy->setShowOnlyComplete(showOnlyComplete);
    headerProxy->setPerformAnd(true);
	filter(headerListWidget->m_filterEdit->text());
}

void HeaderList::slotShowOnlyNew()
{
	if (showOnlyNew)
		showOnlyNew=false;
	else showOnlyNew=true;
	emit sigSaveSettings(ng, showOnlyNew, showOnlyComplete);
	headerProxy->setShowOnlyNew(showOnlyNew);
    headerProxy->setPerformAnd(true);
	filter(headerListWidget->m_filterEdit->text());
}

void HeaderList::filter(const QString &s)
{
//    qDebug() << "Request to filter";
	if (quban->getMainTab()->currentWidget() != this)
		return;

	int rowCount = headerTreeModel->fullRowCount();

	if (rowCount == 0)
		return;

	quint16 filterCol = headerListWidget->filterColCombo->itemData(
    		headerListWidget->filterColCombo->currentIndex()).toInt();

    QRegExp::PatternSyntax syntax =
            QRegExp::PatternSyntax(headerListWidget->filterSyntaxComboBox->itemData(
            		headerListWidget->filterSyntaxComboBox->currentIndex()).toInt());
    Qt::CaseSensitivity caseSensitivity =
    		headerListWidget->caseCheck->isChecked() ? Qt::CaseSensitive
                                   : Qt::CaseInsensitive;

    QRegExp rx(s, caseSensitivity, syntax);
	if (!rx.isValid())
		QMessageBox::warning(this,tr("Error"), tr("Invalid regular expression!") );

	headerListWidget->m_filterEdit->setEnabled(false);
	m_headerList->setEnabled(false);
    QList<qint32>filterColumns;
    filterColumns << filterCol;
    headerProxy->setFilterKeyColumns(filterColumns);
    headerProxy->addFilterRegExp(filterCol, rx, CommonDefs::CONTAINS);
    headerProxy->setFilterRegExp(rx); // runs the filtering now ...

    m_headerList->setEnabled(true);
	headerListWidget->m_filterEdit->setEnabled(true);
}

void HeaderList::compoundFilter(FilterComponents &s)
{
    if (s.andComponents) // AND
        headerProxy->setPerformAnd(true);
    else // OR
        headerProxy->setPerformAnd(false);

    QList<qint32>filterColumns;
    quint8  columnNo;
    QRegExp rx;

    for (int i = 0; i < s.filterLines.size(); ++i)
    {
        columnNo = s.filterLines.at(i)->columnNo;
        filterColumns << columnNo;
    }

    headerProxy->setFilterKeyColumns(filterColumns);

    for (int i = s.filterLines.size() - 1; i >= 0; --i)
    {
        columnNo = s.filterLines.at(i)->columnNo;

        if (columnNo != CommonDefs::No_Col)
        {
            rx.setPattern(s.filterLines.at(i)->filter);
            rx.setPatternSyntax(QRegExp::FixedString);
            if (s.filterLines.at(i)->caseSensitive)
                rx.setCaseSensitivity(Qt::CaseSensitive);
            else
                rx.setCaseSensitivity(Qt::CaseInsensitive);
            headerProxy->addFilterRegExp(columnNo, rx, s.filterLines.at(i)->filterType);
        }
        else // READ / NO_READ
        {
            rx.setPattern("");
            headerProxy->addFilter(columnNo, s.filterLines.at(i)->filterType);
        }

        Q_DELETE_NO_LVALUE(s.filterLines.at(i));
        s.filterLines.removeAt(i);
    }

    headerProxy->setFilterRegExp(rx); // runs the filtering now ...
}

void HeaderList::markAllAsRead( Db* db)
{
	if (quban->getMainTab()->currentWidget() != this)
		return;

	// belt and braces
	key.set_data(keymem);
	data.set_data(datamem);

    Dbc *cursor = 0;
	db->cursor(NULL, &cursor, DB_WRITECURSOR);

	int ret= 0;
	QTime current, previous;
	previous=QTime::currentTime();
    MultiPartHeader *mph = 0;
    SinglePartHeader* sph = 0;
    HeaderBase* hb = 0;

	while (ret == 0)
	{
		if ((ret=cursor->get(&key, &data, DB_NEXT)) != DB_NOTFOUND)
		{
			if (ret==0)
			{
				if (*((char *)data.get_data()) == 'm')
				{
					mph = new MultiPartHeader(key.get_size(), (char*)key.get_data(), (char*)data.get_data());
				    hb = (HeaderBase*)mph;
				}
				else if (*((char *)data.get_data()) == 's')
				{
					sph = new SinglePartHeader(key.get_size(), (char*)key.get_data(), (char*)data.get_data());
					hb = (HeaderBase*)sph;
				}
				else
				{
					// What have we found ?????
					qDebug() << "Found unexpected identifier for header : " << (char)*((char *)data.get_data());
					continue;
				}

				//Now, mark the header as read, save it and change the appearance...
                if (hb->getStatusIgnoreMark() == HeaderBase::bh_new)
					ng->decUnread();

                hb->setStatus(HeaderBase::bh_read);

				char *p=hb->data();
				data.set_data(p);
				data.set_size(hb->getRecordSize());

				ret=cursor->put(NULL, &data, DB_CURRENT);
				if (ret!=0)
					qDebug("Error updating record: %d", ret);

				data.set_data(datamem);
                Q_DELETE_ARRAY(p);
                Q_DELETE(hb);

				current=QTime::currentTime();
				if (previous.secsTo(current) > 1)
				{
				    QCoreApplication::processEvents();
					previous=QTime::currentTime();

				}
			}else qDebug("Error retrieving key: %d", ret);
		}
		else if (ret == DB_BUFFER_SMALL)
		{
			ret = 0;
			qDebug("Insufficient memory");
			qDebug("Size required is: %d", data.get_size());
			uchar *p=datamem;
			datamem=new uchar[data.get_size()+1000];
			data.set_ulen(data.get_size()+1000);
			data.set_data(datamem);
            Q_DELETE_ARRAY(p);
		}
	}

	cursor->close();
	ng->setUnread(0);

	emit updateFinished(ng);
}

void HeaderList::slotViewArticle( )
{
	slotDownloadSelected(true, true);
}

void HeaderList::closeAndMark( )
{
	saveSettings();
	QCloseEvent *e = new QCloseEvent();
	ng->setView(NULL);
	markGroupAsRead();
	QTabWidget::closeEvent(e);
}

void HeaderList::closeAndNoMark( )
{
	saveSettings();
	QCloseEvent *e = new QCloseEvent();
	ng->setView(NULL);
	QTabWidget::closeEvent(e);
    Q_DELETE(e);
}

void HeaderList::closeEvent( QCloseEvent * e )
{
    if (ng->getDownloadingServersCount() || ng->unread() == 0 )
    {
		ng->setView(NULL);
		saveSettings();
		QTabWidget::closeEvent(e);
	}
	else
	{
		Qt::SortOrder so;
		Configuration* config = Configuration::getConfig();
		switch (config->markOpt) {
			case Configuration::Yes:
				markGroupAsRead();
			case Configuration::No:
				saveSettings();
				ng->setView(NULL);
				QTabWidget::closeEvent(e);
				break;
			case Configuration::Ask:
				QString dbQuestion = tr("Do you want to mark all messages in\n ") + ng->name() + tr("\nas read ?");
				int result = QMessageBox::question(this, tr("Question"), dbQuestion, QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel);

			    switch (result)
			    {
					case QMessageBox::Yes:

				//Mark all messages as read
	// 				markAllAsRead( ng->getDb()  );

						markGroupAsRead();

					case QMessageBox::No:
				//do nothing
						saveSettings();
						ng->setView(NULL);
						// ng->setSortColumn( m_headerList->sortColumn());
						ng->setSortColumn(0); // MD TODO
						so = m_headerList->header()->sortIndicatorOrder();
						ng->setSortOrder( so == Qt::AscendingOrder ? true : false);
						emit sigSaveSettings(ng, showOnlyNew, showOnlyComplete);
						QTabWidget::closeEvent(e);
						break;
					case QMessageBox::Cancel:
				//don't exit (return false?)

						break;
				}
		}
	}
}

void HeaderList::slotDownloadSelectedFirst( )
{
	slotDownloadSelected(true);
}

void HeaderList::slotClearButtonClicked( )
{
//    m_headerList->setEnabled(false);
//    headerListWidget->m_filterEdit->setEnabled(false);
	headerListWidget->m_filterEdit->clear();
    headerProxy->clearFilter();
    filter("");
//    m_headerList->setEnabled(true); // run filtering now ...
//    headerListWidget->m_filterEdit->setEnabled(true);
}

//New version of markAllAsRead(): scan the articles in memory, and mark only the unread articles...

void HeaderList::markGroupAsRead( )
{
	if (quban->getMainTab()->currentWidget() != this)
		return;

	// 	qDebug("Mark group as read");
	// belt and braces
	key.set_data(keymem);
	data.set_data(datamem);

	int ret;
	QString index;

	int rowCount = headerTreeModel->fullRowCount();
	QModelIndex subjItem;
	QModelIndex fromItem;

    MultiPartHeader *mph = 0;
    SinglePartHeader* sph = 0;
    HeaderBase* hb = 0;

    QString msgId;
    bool multiPartHeader = false;

    QByteArray ba;
	for  (int i=0; i<rowCount; ++i)
	{
        subjItem = headerTreeModel->index(i, CommonDefs::Subj_Col);
        fromItem = headerTreeModel->index(i, CommonDefs::From_Col);

        if (headerTreeModel->getItem(subjItem)->getStatus() == HeaderBase::bh_new)
		{
			msgId = headerTreeModel->getItem(subjItem)->getMsgId();

			if (msgId.isNull()) // it's a multi part header
			{
				multiPartHeader = true;
			    index =  subjItem.data().toString() % "\n" % fromItem.data().toString();
			}
			else // it's a single part header
			{
				multiPartHeader = false;
				index = subjItem.data().toString() % "\n" % msgId;
			}

			ba = index.toLocal8Bit();
			const char *k= ba.constData();;
			memcpy(keymem, k, index.length());
			key.set_size(index.length());

			ret=ng->getDb()->get(NULL, &key, &data, 0);

			if (ret == DB_BUFFER_SMALL)
			{
				//Grow key and repeat the query
				qDebug("Insufficient memory");
				qDebug("Size is: %d", data.get_size());
				uchar *p=datamem;
				datamem=new uchar[data.get_size()+1000];
				data.set_ulen(data.get_size()+1000);
				data.set_data(datamem);
                Q_DELETE_ARRAY(p);
				qDebug("Exiting growing array cycle");
				ret=ng->getDb()->get(0,&key, &data, 0);
			}

			if (ret==0)
			{
				if (multiPartHeader)
				{
					mph = new MultiPartHeader(index.length(), (char*)k, (char*)datamem);
				    hb = (HeaderBase*)mph;
				}
				else
				{
					sph = new SinglePartHeader(index.length(), (char*)k, (char*)datamem);
					hb = (HeaderBase*)sph;
				}

				ng->decUnread();
                hb->setStatus(HeaderBase::bh_read);
				char *p=hb->data();
				data.set_data(p);
				data.set_size(hb->getRecordSize());
				ret=ng->getDb()->put(NULL, &key, &data, 0);

				if (ret!=0)
					qDebug("Error updating record: %d", ret);

				data.set_data(datamem);
                Q_DELETE_ARRAY(p);
                Q_DELETE(hb);
			}
			else
				qDebug("Error retrieving record: %d", ret);
		}
	}

	emit updateFinished(ng);
}

void HeaderList::slotDownloadToDir( )
{
	DownLoadSelect *ds=new DownLoadSelect(ng->getSaveDir(), this);
	connect(ds, SIGNAL(download(QString, bool )), this, SLOT(slotDownloadToFirst(QString, bool )));
	ds->exec();
}

void HeaderList::slotDownloadToFirst( QString dir, bool first)
{
	slotDownloadSelected(first, false, dir);
}

void HeaderList::slotGroupAndDownload()
{
	if (quban->getMainTab()->currentWidget() != this)
		return;

	// belt and braces
	key.set_data(keymem);
	data.set_data(datamem);

	GroupedDownloads* groupedDownloads = &(quban->getQMgr()->groupedDownloads);

    qDebug() << "Group and download selected";

	if (quban->getMainTab()->currentWidget() != this)
		return;

    QList<QModelIndex> selectedSubjects = m_headerList->selectionModel()->selectedRows(CommonDefs::Subj_Col);
    QList<QModelIndex> selectedFroms    = m_headerList->selectionModel()->selectedRows(CommonDefs::From_Col);
    QModelIndex sourceSubjIndex;
    QModelIndex sourceFromIndex;

	if (selectedSubjects.size() == 0)
		return;

    QList<QString>hbKeys;
	QString msgId;
	QString index;
	bool multiPartHeader = false;

	 quban->getQMgr()->maxGroupId++;
	 headerGroupId = quban->getQMgr()->maxGroupId;

	 GroupManager* groupManager = new GroupManager(quban->getQMgr()->getUnpackDb(), ng, headerGroupId);
	 groupedDownloads->insert(headerGroupId, groupManager);
	 groupManager->dbSave();

	 UnpackConfirmation* unpackConfirmation = new UnpackConfirmation;
	 quban->getQMgr()->unpackConfirmation = unpackConfirmation;
	 unpackConfirmation->groupId = headerGroupId;
	 unpackConfirmation->ng = ng;
	 unpackConfirmation->first = false;
	 unpackConfirmation->dDir = ng->getSaveDir();

	for (int i = 0; i < selectedSubjects.size(); ++i)
	{
		sourceSubjIndex = headerProxy->mapToSource(selectedSubjects.at(i));
		sourceFromIndex = headerProxy->mapToSource(selectedFroms.at(i));

        if (headerTreeModel->getItem(sourceSubjIndex)->getType() == HeaderTreeItem::HEADER)
        {
            msgId = headerTreeModel->getItem(sourceSubjIndex)->getMsgId();

            if (msgId.isNull()) // it's a multi part header
            {
                multiPartHeader = true;
                index = sourceSubjIndex.data().toString() % "\n" % sourceFromIndex.data().toString();
            }
            else // it's a single part header
            {
                multiPartHeader = false;
                index = sourceSubjIndex.data().toString() % "\n" % msgId;
            }

            confirmSelectedHeader(sourceSubjIndex, index, multiPartHeader, unpackConfirmation);
        }
        else if (headerTreeModel->getItem(sourceSubjIndex)->getType() == HeaderTreeItem::HEADER_GROUP)
        {
            int ret;
            QString articleIndex = sourceSubjIndex.data().toString() % "\n" % sourceFromIndex.data().toString();

            Dbt groupkey;
            Dbt groupdata;
            memset(&groupkey, 0, sizeof(groupkey));
            memset(&groupdata, 0, sizeof(groupdata));
            groupdata.set_flags(DB_DBT_MALLOC);

            QByteArray ba = articleIndex.toLocal8Bit();
            const char *k= ba.constData();
            groupkey.set_data((void*)k);
            groupkey.set_size(articleIndex.length());

            Db* groupsDb = ng->getGroupingDb();
            ret=groupsDb->get(NULL, &groupkey, &groupdata, 0);
            if (ret != 0) //key not found
            {
                qDebug() << "Failed to find group with key " << articleIndex;
                return;
            }

            qDebug() << "Found group with key " << articleIndex;

            HeaderGroup *hg=new HeaderGroup(articleIndex.length(), (char*)k, (char*)groupdata.get_data());
            void* ptr = groupdata.get_data();
            Q_FREE(ptr);

            hbKeys = hg->getSphKeys();
            multiPartHeader = false;

            for (int h=0; h<hbKeys.count(); ++h)
            {
                qDebug() << "Found sph with key " << hbKeys.at(h);
                confirmSelectedHeader(sourceSubjIndex, hbKeys.at(h), multiPartHeader, unpackConfirmation);
            }

            hbKeys = hg->getMphKeys();
            multiPartHeader = true;

            for (int h=0; h<hbKeys.count(); ++h)
            {
                qDebug() << "Found mph with key " << hbKeys.at(h);
                confirmSelectedHeader(sourceSubjIndex, hbKeys.at(h), multiPartHeader, unpackConfirmation);
            }
        }
        else
            qDebug() << "Tried to download msg part - ignored";

	}

	m_headerList->resizeColumnToContents(0);
	qDebug() << "There are " << unpackConfirmation->headers.count() << " headers to display";
	quban->getQMgr()->completeUnpackDetails(unpackConfirmation, QString::null);
}

void HeaderList::confirmSelectedHeader(QModelIndex sourceSubjIndex, QString index, bool multiPartHeader, UnpackConfirmation* unpackConfirmation)
{
    MultiPartHeader *mph = 0;
    SinglePartHeader* sph = 0;
    HeaderBase* hb = 0;

    uint count=0;
    char *p = 0;
    QByteArray ba;

    int ret;
    count++;

    // qDebug() << "Index = " << index;
    ba = index.toLocal8Bit();
    const char *k= ba.constData();
    memcpy(keymem, k, index.length());
    key.set_size(index.length());
    ret=ng->getDb()->get(NULL, &key, &data, 0);

    if (ret == DB_BUFFER_SMALL)
    {
        //Grow key and repeat the query
        qDebug("Insufficient memory");
        qDebug("Size is: %d", data.get_size());
        uchar *p=datamem;
        datamem=new uchar[data.get_size()+1000];
        data.set_ulen(data.get_size()+1000);
        data.set_data(datamem);
        Q_DELETE_ARRAY(p);
        qDebug("Exiting growing array cycle");
        ret=ng->getDb()->get(0,&key, &data, 0);
    }
    if (ret==0)
    {
        if (multiPartHeader)
        {
            mph = new MultiPartHeader(index.length(), (char*)k, (char*)datamem);
            hb = (HeaderBase*)mph;
        }
        else
        {
            sph = new SinglePartHeader(index.length(), (char*)k, (char*)datamem);
            hb = (HeaderBase*)sph;
        }

        unpackConfirmation->headers.append(new ConfirmationHeader(hb));

        //Now, mark the header as read, save it and change the appearance...
        if (hb->getStatusIgnoreMark() == HeaderBase::bh_new)
            ng->decUnread();
        hb->setStatus(HeaderBase::bh_downloaded );
        headerTreeModel->getItem(sourceSubjIndex)->setStatus(HeaderBase::bh_downloaded, headerTreeModel);

        p=hb->data();
        data.set_data(p);
        data.set_size(hb->getRecordSize());
        ret=ng->getDb()->put(NULL, &key, &data, 0);
        if (ret!=0)
            qDebug("Error updating record: %d", ret);

        data.set_data(datamem);
        Q_DELETE_ARRAY(p);
    }
    else
        qDebug() << "Error retrieving record: " << ret;
}

void HeaderList::slotItemExecuted( )
{
	slotViewArticle();
}

void HeaderList::slotEnterButtonClicked( )
{
    headerProxy->setPerformAnd(true);
	filter(headerListWidget->m_filterEdit->text());
}

void HeaderList::slotSelectionChanged()
{
    QString progMessage = QString::number(m_headerList->selectionModel()->selectedRows(CommonDefs::Subj_Col).count()) + tr(" items selected");
	quban->statusBar()->showMessage(progMessage, 3000);
}

void HeaderList::slotGroupArticles()
{
}



int HeaderList::area( QString s )
{
	int area = 0;
    QByteArray ba = s.toLocal8Bit();
    const char *as = ba.data();
	for (int i = 0; i < s.length(); i++) {
		if (!s.at(i).isDigit()) {
			area += as[i];
		}
	}
	return area;
}

void HeaderList::slotColSizeChanged( int section, int, int newSize)
{
	//qDebug() << "Section " << section << " Changed its size from " << oldSize << " to " << newSize;
	if (newSize == 0) // hidden
		return;

// 	ng->clearColSize();
	switch( section ){
		case 0:
            ng->setColSize( CommonDefs::Subj_Col, newSize);
			break;
		case 1:
            ng->setColSize(CommonDefs::Parts_Col, newSize);
			break;
		case 2:
            ng->setColSize(CommonDefs::KBytes_Col, newSize);
			break;
		case 3:

			if(showFrom)
			{
                ng->setColSize(CommonDefs::From_Col, newSize);

			} else if (showDate) {
                ng->setColSize(CommonDefs::Date_Col, newSize);
			}
			//else it's a server col, won't save the size of this
			break;
		case 4:
			if (showFrom && showDate)
			{
				//Date col
                ng->setColSize(CommonDefs::Date_Col, newSize);
			}
			break;
		default:
			qDebug() << "Resize of a non-important column";
			break;
	}
}

void HeaderList::slotIndexChanged( int section, int fromIndex, int toIndex)
{
	qDebug() << "Section " << section << " Moved from index " << fromIndex << " to index " << toIndex;
	ng->clearColOrder();
// TODO - the ordering below ...
	ng->setColIndex(0, 0);
// 	qDebug() << "Subj Index:  " << ng->getColIndex( Subj_Col);

	ng->setColIndex(1, 1);
// 	qDebug() << "Parts Index:  " << ng->getColIndex( Parts_Col);
	ng->setColIndex(2, 2);
// 	qDebug() << "Bytes Index:  " << ng->getColIndex( KBytes_Col);
	if (showDate) {
		ng->setColIndex(3, 3);
// 		qDebug() << "From Index:  " << ng->getColIndex( Date_Col);
	}
	if (showFrom)
		ng->setColIndex(4, 4);

// 	ng->setColIndex( section, toIndex);
	emit sigSaveSettings( ng, showOnlyNew, showOnlyComplete);
}

void HeaderList::slotClearFilter( )
{
	headerListWidget->m_filterEdit->clear();
	slotEnterButtonClicked();
}

void HeaderList::slotCopyToFilter()
{
    QModelIndexList indexList = m_headerList->selectionModel()->selectedIndexes();
    if (indexList.isEmpty())
        return;

    QModelIndex sourceIndex = headerProxy->mapToSource(indexList[0]);

    QString data = headerTreeModel->data(sourceIndex).toString();

    headerListWidget->m_filterEdit->setText(data);
}

void HeaderList::slotFocusFilter( )
{
	headerListWidget->m_filterEdit->setFocus();
}

void HeaderList::saveSettings( )
{
	/*

	ng->setSortColumn( m_headerList->sortColumn());
// 	qDebug() << "Sort column: " << ng->getSortColumn() << endl;
	Qt::SortOrder so = m_headerList->header()->sortIndicatorOrder();
	ng->setSortOrder( so == Qt::AscendingOrder ? true : false);
	emit sigSaveSettings(ng, showOnlyNew, showOnlyComplete);
	*/
}

void HeaderList::adjColSize( int col )
{
	int w;
// TODO	m_headerList->setColumnWidthMode(col, QTreeWidget::Manual);//

    if ((w = ng->getColSize( col )) != -1)
    {
		m_headerList->setColumnWidth(col, w);
	}
}

void HeaderList::slotFilterArticles(bool)
{
    ArticleFilterDialog* af = new ArticleFilterDialog(ng->getAlias(), config->showFrom, config->showDate, this);

    connect(af, SIGNAL(multifilter(FilterComponents&)), this, SLOT(compoundFilter(FilterComponents&)));

    af->setModal(false);
    af->show();
    af->raise();
}

void HeaderList::slotAdvancedFilterButtonClicked()
{
    slotFilterArticles(false);
}
