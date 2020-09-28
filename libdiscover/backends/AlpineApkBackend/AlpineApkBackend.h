/***************************************************************************
 *   Copyright © 2020 Alexey Min <alexey.min@gmail.com>                    *
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

#ifndef AlpineApkBackend_H
#define AlpineApkBackend_H

#include <resources/AbstractResourcesBackend.h>
#include <QVariantList>
#include <QFutureWatcher>

#include <QtApk>

#include <AppStreamQt/component.h>

class AlpineApkReviewsBackend;
class AlpineApkUpdater;
class AlpineApkResource;
class KJob;
class QTimer;

class AlpineApkBackend : public AbstractResourcesBackend
{
    Q_OBJECT

public:
    explicit AlpineApkBackend(QObject *parent = nullptr);

    QVector<Category *> category() const override;
    int updatesCount() const override;
    AbstractBackendUpdater *backendUpdater() const override;
    AbstractReviewsBackend *reviewsBackend() const override;
    ResultsStream *search(const AbstractResourcesBackend::Filters &filter) override;
    ResultsStream *findResourceByPackageName(const QUrl &search);
    QHash<QString, AlpineApkResource *> resources() const { return m_resources; }
    QHash<QString, AlpineApkResource *> *resourcesPtr() { return &m_resources; }
    bool isValid() const override { return true; } // No external file dependencies that could cause runtime errors

    Transaction *installApplication(AbstractResource *app) override;
    Transaction *installApplication(AbstractResource *app, const AddonList &addons) override;
    Transaction *removeApplication(AbstractResource *app) override;
    bool isFetching() const override { return m_fetching; }
    int fetchingUpdatesProgress() const override;
    void checkForUpdates() override;
    QString displayName() const override;
    bool hasApplications() const override;

public Q_SLOTS:
    void setFetchingUpdatesProgress(int percent);

private Q_SLOTS:
    void finishCheckForUpdates();
    void loadResources();
    void onLoadResourcesFinished();

public:
    QtApk::Database *apkdb() { return &m_apkdb; }

private:
    QHash<QString, AlpineApkResource *> m_resources;
    QHash<QString, AppStream::Component> m_resourcesAppstreamData;
    AlpineApkUpdater *m_updater;
    AlpineApkReviewsBackend *m_reviews;
    QtApk::Database m_apkdb;
    QVector<QtApk::Package> m_availablePackages;
    QVector<QtApk::Package> m_installedPackages;
    bool m_fetching = false;
    int m_fetchProgress = 0;
    QTimer *m_updatesTimeoutTimer;
    QList<AppStream::Component> m_appStreamComponents;
    // QVector<QString> m_collectedCategories;
    QFutureWatcher<void> m_voidFutureWatcher;
};

#endif // AlpineApkBackend_H
