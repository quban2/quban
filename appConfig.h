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

#ifndef APPCONFIG_H_
#define APPCONFIG_H_

#include <QString>
#include <QFont>
#include <QList>

class QSettings;

typedef struct
{
	bool    enabled;
	int     appId;
	int     appType;
	QString suffixes;
	QString reSuffixes;
	QString fullFilePath;
	bool    filePathValid;
	QString parameters;
	int     priority;
} t_ExternalApp;

class Configuration
{
public:
		Configuration();
		Configuration(const Configuration&);
		~Configuration();

	    static Configuration* getConfig();

		void read();
		void write() const;
		enum DeleteViewedFile {Ask=0, Yes, No};
		//General options
		bool configured;
		QString dbDir;
		QString decodeDir;
		QString nzbDir;
		QString tmpDir;

		int maxShutDownWait;
		bool renameNzbFiles;
		bool noQueueSavePrompt;
        bool minAvailEnabled;
        int  minAvailValue;

		QFont appFont;

        int mainTab;
        int jobTab;

		//Decoding options
		bool overwriteExisting;
		bool deleteFailed;

		//View options.
		int dViewed;
		bool singleViewTab;

		QString textSuffixes;

		bool activateTab;
		bool showStatusBar;

		//View options
		bool showQueueBar;
		bool showServerBar;
		bool showGroupBar;
		bool showHeaderBar;
		bool showJobBar;

		//HeaderList options
		bool showFrom;
		bool showDetails;
		bool showDate;
		bool ignoreNoPartArticles;
		bool alwaysDoubleClick;
		//remember sort column
		bool rememberSort;
		//don't automatically resize columns (and remember the manual sizes)
		bool rememberWidth;
		bool downloadCompressed;
		int markOpt;
		int  checkDaysValue;
        int bulkLoadThreshold;
        int bulkDeleteThreshold;
        int groupCheckCount;

		// Unpack general options
		bool autoUnpack;
		QString otherFileTypes;
		int     numRepairers;
		int     numUnpackers;
		bool deleteGroupFiles;
		bool deleteRepairFiles;
		bool deleteCompressedFiles;
		bool deleteOtherFiles;
		bool merge001Files;
		bool dontShowTypeConfirmUnlessNeeded;

		// version db backup
		int groupDbVersion;
		int headerDbVersion;
		int availableGroupsDbVersion;
		int serverDbVersion;
		int queueDbVersion;
		int unpackDbVersion;
		int schedulerDbVersion;
		int versionDbVersion;

		int maxAppId;
        QList<t_ExternalApp *> repairAppsList;
        QList<t_ExternalApp *> unpackAppsList;

private:
		QSettings *conf;
};

#endif /* APPCONFIG_H_ */
