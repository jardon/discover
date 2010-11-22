/***************************************************************************
 *   Copyright © 2010 Jonathan Thomas <echidnaman@kubuntu.org>             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation; either version 2 of        *
 *   the License or (at your option) version 3 or any later version        *
 *   accepted by the membership of KDE e.V. (or its successor approved     *
 *   by the membership of KDE e.V.), which shall act as a proxy            *
 *   defined in Section 14 of version 3 of the license.                    *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "ApplicationBackend.h"

#include <QtCore/QDir>
#include <QtCore/QStringList>

#include <KLocale>
#include <KMessageBox>

#include <LibQApt/Backend>

#include "Application.h"

ApplicationBackend::ApplicationBackend(QObject *parent)
    : QObject(parent)
    , m_backend(0)
{
}

ApplicationBackend::~ApplicationBackend()
{
}

void ApplicationBackend::setBackend(QApt::Backend *backend)
{
    m_backend = backend;
    m_backend->setUndoRedoCacheSize(1);
    init();

    connect(m_backend, SIGNAL(workerEvent(QApt::WorkerEvent)),
            this, SLOT(workerEvent(QApt::WorkerEvent)));
}

void ApplicationBackend::init()
{
    QList<int> popconScores;
    QDir appDir("/usr/share/app-install/desktop/");
    QStringList fileList = appDir.entryList(QDir::Files);
    foreach(const QString &fileName, fileList) {
        Application *app = new Application("/usr/share/app-install/desktop/" + fileName, m_backend);
        if (app->isValid()) {
            m_appList << app;
            popconScores << app->popconScore();
        } else {
            // Invalid .desktop file
            // kDebug() << fileName;
        }
    }
    qSort(popconScores);

    m_maxPopconScore = popconScores.last();
}

void ApplicationBackend::reload()
{
    qDeleteAll(m_appList);
    m_appList.clear();
    m_queue.clear();
    m_backend->reloadCache();

    init();
    emit reloaded();
}

void ApplicationBackend::workerEvent(QApt::WorkerEvent event)
{
    m_workerState.first = event;
    m_workerState.second = m_queue.first().application;
    switch (event) {
    case QApt::PackageDownloadStarted:
        emit workerEvent(QApt::PackageDownloadStarted, m_queue.first().application);
        connect(m_backend, SIGNAL(downloadProgress(int, int, int)),
                this, SLOT(updateDownloadProgress(int)));
        break;
    case QApt::PackageDownloadFinished:
        emit workerEvent(QApt::PackageDownloadFinished, m_queue.first().application);
        disconnect(m_backend, SIGNAL(downloadProgress(int, int, int)),
                   this, SLOT(updateDownloadProgress(int)));
        break;
    case QApt::CommitChangesStarted:
        emit workerEvent(QApt::CommitChangesStarted, m_queue.first().application);
        connect(m_backend, SIGNAL(commitProgress(const QString &, int)),
                this, SLOT(updateCommitProgress(const QString &, int)));
        break;
    case QApt::CommitChangesFinished:
        emit workerEvent(QApt::CommitChangesFinished, m_queue.first().application);
        disconnect(m_backend, SIGNAL(commitProgress(const QString &, int)),
                   this, SLOT(updateCommitProgress(const QString &, int)));

        m_workerState.first = QApt::InvalidEvent;
        m_workerState.second = 0;
        m_queue.removeFirst();

        if (m_queue.isEmpty()) {
            reload();
        } else {
            runNextTransaction();
        }
        break;
    default:
        break;
    }
}

void ApplicationBackend::updateDownloadProgress(int percentage)
{
    Application *app = m_queue.first().application;
    emit progress(app, percentage);
}

void ApplicationBackend::updateCommitProgress(const QString &text, int percentage)
{
    Q_UNUSED(text);

    Application *app = m_queue.first().application;
    emit progress(app, percentage);
}

void ApplicationBackend::addTransaction(Transaction transaction)
{
    m_queue.append(transaction);

    if (m_queue.count() == 1) {
        runNextTransaction();
    }
}

void ApplicationBackend::runNextTransaction()
{
    QList<int> oldCacheState = m_backend->currentCacheState();
    m_backend->saveCacheState();

    Transaction transaction = m_queue.first();
    Application *app = transaction.application;

    switch (transaction.action) {
    case QApt::Package::ToInstall:
    case QApt::Package::ToUpgrade:
        app->package()->setInstall();
        break;
    case QApt::Package::ToRemove:
        app->package()->setRemove();
        break;
    default:
        break;
    }

    if (app->package()->wouldBreak()) {
        m_backend->restoreCacheState(oldCacheState);
        //TODO Notify of error
    }

    QApt::PackageList markedPackages = m_backend->markedPackages();
    bool commitChanges = false;

    switch (transaction.action) {
    case QApt::Package::ToInstall:
    case QApt::Package::ToUpgrade:
        commitChanges = shouldInstallAdditionalPackages(markedPackages);
        break;
    case QApt::Package::ToRemove:
        commitChanges = true;
        break;
    default:
        break;
    }

    if (commitChanges) {
        m_backend->commitChanges();
    } else {
        m_backend->restoreCacheState(oldCacheState);
    }
}

bool ApplicationBackend::shouldInstallAdditionalPackages(QApt::PackageList &list)
{
    int diskSpaceUsed = 0;
    int result = KMessageBox::Cancel;

    foreach (QApt::Package *package, list) {
        diskSpaceUsed += package->availableInstalledSize();
    }

    QString text = i18nc("@label", "Installing will require %1 of disk space.\n"
                                   "Do you wish to continue?",
                                   KGlobal::locale()->formatByteSize(diskSpaceUsed));
    QString title = i18nc("@title:window", "Confirm Installation");

    result = KMessageBox::questionYesNo(0, text, title, KStandardGuiItem::cont(),
                                        KStandardGuiItem::cancel(), QLatin1String("AskForDependencies"));

    return (result == KMessageBox::Yes);
}

QList<Application *> ApplicationBackend::applicationList() const
{
    return m_appList;
}

QPair<QApt::WorkerEvent, Application *> ApplicationBackend::workerState() const
{
    return m_workerState;
}

int ApplicationBackend::maxPopconScore() const
{
    return m_maxPopconScore;
}
