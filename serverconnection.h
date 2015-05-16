#ifndef SERVERCONNECTION_H
#define SERVERCONNECTION_H

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
#include "ui_serverconnection.h"

class ServerConnection : public QDialog, private Ui::ServerConnection
{
    Q_OBJECT

public:
    ServerConnection(QWidget *parent = 0);
    virtual ~ServerConnection();

    void setAddress(const QString&);
    void setPort(const QString&);
    void setMode(const QString&);
    void setEncryption(const QString&);

protected slots:
    virtual void          accept();
};

#endif // SERVERCONNECTION_H
