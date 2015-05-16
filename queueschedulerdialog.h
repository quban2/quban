#ifndef QUEUESCHEDULERDIALOG_H
#define QUEUESCHEDULERDIALOG_H

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

#include <QtGui/QDialog>
#include <QList>
#include <QMap>
#include <QPixmap>
#include <QCursor>
#include <QStandardItemModel>
#include "QueueScheduler.h"
#include "ui_queueschedulerdialog.h"


class QueueSchedulerDialog : public QDialog, private Ui::QueueSchedulerDialog
{
    Q_OBJECT

public:
    QueueSchedulerDialog(QueueScheduler* _queueScheduler, QWidget *parent = 0);
    ~QueueSchedulerDialog();

protected slots:
  /*$PROTECTED_SLOTS$*/
  virtual void reject();
  virtual void addSchedule();
  virtual void saveSchedule();
  virtual void saveAsSchedule();
  virtual void deleteSchedule();
  virtual void SelectionChanged(const QItemSelection &, const QItemSelection &);
  virtual void modifyCurrentPeriod();
  virtual void deleteCurrentPeriod();
  virtual void deleteAllPeriods();
  virtual void PeriodModeChanged(const QString &);
  virtual void ScheduleChanged(const QString &);
  virtual void enabledChecked(bool);

private slots:
  void timeSet();

public slots:
  virtual void addSchedule(QString &);
  virtual void saveAsSchedule(QString &);

private:
  virtual void clearScreen();
  virtual void buildScreen(QueueSchedule* newSchedule);
  virtual void enableWidgets(bool);
  quint32 setStartTime(quint32, QueueScheduleElement*);
  quint32 setEndTime(quint32, QueueScheduleElement*);
  bool isPeriodInvalid(quint32, quint32, QueueScheduleElement*);

  QString greenButton;
  QString orangeButton;
  QString redButton;
  QList<QPushButton*> pbList;
  int                 startIndex; // used when awaiting user's selection of end period;

  bool changeDetected;
  bool screenAlreadyUpdated;
  quint16 elementId;

  QPixmap* leftArrow;
  QCursor* leftC;

  QPixmap* rightArrow;
  QCursor* rightC;

  QueueScheduler* queueScheduler;
  QueueSchedule* queueSchedule;

  QStandardItemModel *model;
};

#endif // QUEUESCHEDULERDIALOG_H
