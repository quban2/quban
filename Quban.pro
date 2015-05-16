TEMPLATE = app
TARGET = quban
target.path = /usr/bin
INSTALLS += target
LIBS += -L/usr/local/BerkeleyDB/lib \
    -ldb_cxx \
    -lz
QT += core \
    gui \
    network \
    xml
INCLUDEPATH += /usr/include/db5 \
    /usr/include/db4 \
    /usr/local/BerkeleyDB/include
CONFIG(release):DEFINES += QT_NO_DEBUG_OUTPUT
HEADERS += bulkLoad.h \
    bulkDelete.h \
    headerlistwidget.h \
    logEventList.h \
    logAlertList.h \
    qubanDbEnv.h \
    availablegroupswidget.h \
    JobList.h \
    SinglePartHeader.h \
    HeaderBase.h \
    MultiPartHeader.h \
    RawHeader.h \
    sslerrorchecker.h \
    IdListWidgetItem.h \
    AvailableGroupsView.h \
    newsgroupcontents.h \
    ratecontroldialog.h \
    addschedule.h \
    QueueScheduler.h \
    SaveQItems.h \
    about.h \
    addserver.h \
    addserverwidget.h \
    appConfig.h \
    autounpackconfirmation.h \
    availablegroups.h \
    catman.h \
    common.h \
    decodeManager.h \
    decoder.h \
    downloadselect.h \
    droptree.h \
    expireDb.h \
    fileviewer.h \
    grouplist.h \
    groupprogress.h \
    header.h \
    headerlist.h \
    hlistviewitem.h \
    maintabwidget.h \
    newsgroup.h \
    newsgroupdialog.h \
    nntphost.h \
    nntpthreadsocket.h \
    nzbform.h \
    prefgeneral.h \
    prefstartup.h \
    prefunpack.h \
    prefunpackexternal.h \
    qmgr.h \
    qnzb.h \
    quban.h \
    qubanwizard.h \
    queueparts.h \
    queueschedulerdialog.h \
    rateController.h \
    serverconnection.h \
    serverslist.h \
    unpack.h \
    uudecoder.h \
    yydecoder.h \
    headerGroup.h \
    bulkHeaderGroup.h \
    headerReadWorker.h \
    headerReadXfeatGzip.h \
    headerReadXzVer.h \
    headerReadNoCompression.h \
    headerQueue.h \
    articlefilterdialog.h \
    headergrouptestdialog.h \
    headergroupingwidget.h \
    helpdialog.h
SOURCES += bulkLoad.cpp \
    bulkDelete.cpp \
    headerlistwidget.cpp \
    logEventList.cpp \
    logAlertList.cpp \
    qubanDbEnv.cpp \
    availablegroupswidget.cpp \
    JobList.cpp \
    SinglePartHeader.cpp \
    HeaderBase.cpp \
    MultiPartHeader.cpp \
    RawHeader.cpp \
    sslerrorchecker.cpp \
    IdListWidgetItem.cpp \
    AvailableGroupsView.cpp \
    newsgroupcontents.cpp \
    ratecontroldialog.cpp \
    addschedule.cpp \
    rateController.cpp \
    decodeManager.cpp \
    expireDb.cpp \
    SaveQItems.cpp \
    QueueScheduler.cpp \
    queueschedulerdialog.cpp \
    addserverwidget.cpp \
    qubanwizard.cpp \
    newsgroupdialog.cpp \
    droptree.cpp \
    prefstartup.cpp \
    qnzb.cpp \
    groupprogress.cpp \
    autounpackconfirmation.cpp \
    prefunpackexternal.cpp \
    unpack.cpp \
    prefunpack.cpp \
    serverconnection.cpp \
    maintabwidget.cpp \
    qmgr.cpp \
    grouplist.cpp \
    downloadselect.cpp \
    catman.cpp \
    headerlist.cpp \
    fileviewer.cpp \
    queueparts.cpp \
    nntpthreadsocket.cpp \
    yydecoder.cpp \
    uudecoder.cpp \
    addserver.cpp \
    availablegroups.cpp \
    newsgroup.cpp \
    hlistviewitem.cpp \
    serverslist.cpp \
    common.cpp \
    header.cpp \
    nzbform.cpp \
    nntphost.cpp \
    decoder.cpp \
    prefgeneral.cpp \
    appConfig.cpp \
    about.cpp \
    main.cpp \
    quban.cpp \
    headerGroup.cpp \
    bulkHeaderGroup.cpp \
    headerReadWorker.cpp \
    headerReadXfeatGzip.cpp \
    headerReadXzVer.cpp \
    headerReadNoCompression.cpp \
    articlefilterdialog.cpp \
    headergrouptestdialog.cpp \
    headergroupingwidget.cpp \
    helpdialog.cpp
FORMS += headerlistwidget.ui \
    availablegroupswidget.ui \
    sslerrorchecker.ui \
    newsgroupcontents.ui \
    ratecontroldialog.ui \
    addschedule.ui \
    addserverwidget.ui \
    queueschedulerdialog.ui \
    qubanwizard.ui \
    newsgroupdialog.ui \
    prefstartup.ui \
    groupprogress.ui \
    autounpackconfirmation.ui \
    prefunpackexternal.ui \
    prefunpack.ui \
    serverconnection.ui \
    downloadselect.ui \
    catman.ui \
    quban.ui \
    about.ui \
    addserver.ui \
    nzbform.ui \
    prefdialog.ui \
    prefgeneral.ui \
    articlefilterdialog.ui \
    headergrouptestdialog.ui \
    headergroupingwidget.ui \
    helpdialog.ui
RESOURCES += quban.qrc
