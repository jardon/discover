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

#include "AlpineApkBackend.h"
#include "AlpineApkResource.h"
#include "AlpineApkReviewsBackend.h"
#include "AlpineApkTransaction.h"
#include "AlpineApkSourcesBackend.h"
#include "AlpineApkUpdater.h"
#include "alpineapk_backend_logging.h"  // generated by ECM

#include "resources/SourcesModel.h"
#include "Transaction/Transaction.h"
#include "Category/Category.h"

#include <KLocalizedString>

#include <QDebug>
#include <QLoggingCategory>
#include <QThread>
#include <QTimer>
#include <QAction>

DISCOVER_BACKEND_PLUGIN(AlpineApkBackend)

AlpineApkBackend::AlpineApkBackend(QObject *parent)
    : AbstractResourcesBackend(parent)
    , m_updater(new AlpineApkUpdater(this))
    , m_reviews(new AlpineApkReviewsBackend(this))
    , m_updatesTimeoutTimer(new QTimer(this))
{
#ifndef QT_DEBUG
    const_cast<QLoggingCategory &>(LOG_ALPINEAPK()).setEnabled(QtDebugMsg, false);
#endif

    // schedule checking for updates
    QTimer::singleShot(1000, this, &AlpineApkBackend::checkForUpdates);

    // connections with our updater
    QObject::connect(m_updater, &AlpineApkUpdater::updatesCountChanged,
                     this, &AlpineApkBackend::updatesCountChanged);
    QObject::connect(m_updater, &AlpineApkUpdater::checkForUpdatesFinished,
                     this, &AlpineApkBackend::finishCheckForUpdates);
    QObject::connect(m_updater, &AlpineApkUpdater::fetchingUpdatesProgressChanged,
                     this, &AlpineApkBackend::setFetchingUpdatesProgress);

    // safety measure: make sure update check process can finish in some finite time
    QObject::connect(m_updatesTimeoutTimer, &QTimer::timeout,
                     this, &AlpineApkBackend::finishCheckForUpdates);
    m_updatesTimeoutTimer->setTimerType(Qt::CoarseTimer);
    m_updatesTimeoutTimer->setSingleShot(true);
    m_updatesTimeoutTimer->setInterval(2 * 60 * 1000); // 2minutes

    qCDebug(LOG_ALPINEAPK) << "backend: populating resources...";

    if (m_apkdb.open(QtApk::Database::QTAPK_OPENF_READONLY)) {
        m_availablePackages = m_apkdb.getAvailablePackages();
        m_installedPackages = m_apkdb.getInstalledPackages();
        m_apkdb.close();
    }

    if (m_availablePackages.size() > 0) {
        for (const QtApk::Package &pkg: m_availablePackages) {
            AlpineApkResource *res = new AlpineApkResource(pkg, this);
            res->setCategoryName(QStringLiteral("all"));
            res->setOriginSource(QStringLiteral("apk"));
            res->setSection(QStringLiteral("dummy"));
            const QString key = pkg.name.toLower();
            m_resources.insert(key, res);
            connect(res, &AlpineApkResource::stateChanged, this, &AlpineApkBackend::updatesCountChanged);
        }
        qCDebug(LOG_ALPINEAPK) << "  available" << m_availablePackages.size()
                               << "packages";
    }
    if (m_installedPackages.size() > 0) {
        for (const QtApk::Package &pkg: m_installedPackages) {
            const QString key = pkg.name.toLower();
            if (m_resources.contains(key)) {
                m_resources.value(key)->setState(AbstractResource::Installed);
            }
        }
        qCDebug(LOG_ALPINEAPK) << "  installed" << m_installedPackages.size()
                               << "packages";
    }

    SourcesModel::global()->addSourcesBackend(new AlpineApkSourcesBackend(this));
}

QVector<Category *> AlpineApkBackend::category() const
{
    // single root category
    // we could add more, but Alpine apk does not have this concept
    static Category *cat = new Category(
                i18nc("Root category name", "Alpine packages"),
                QStringLiteral("package-x-generic"), // icon
                {},                // orFilters
                { displayName() }, // pluginName
                {},                // subCategories
                QUrl(),            // decoration (what is it?)
                false              // isAddons
    );
    return { cat };
}

int AlpineApkBackend::updatesCount() const
{
    return m_updater->updatesCount();
}

ResultsStream *AlpineApkBackend::search(const AbstractResourcesBackend::Filters &filter)
{
    QVector<AbstractResource*> ret;
    if (!filter.resourceUrl.isEmpty()) {
        return findResourceByPackageName(filter.resourceUrl);
    } else {
        for (AbstractResource *r: qAsConst(m_resources)) {
            if (r->type() == AbstractResource::Technical
                    && filter.state != AbstractResource::Upgradeable) {
                continue;
            }
            if (r->state() < filter.state) {
                continue;
            }
            if(r->name().contains(filter.search, Qt::CaseInsensitive)
                    || r->comment().contains(filter.search, Qt::CaseInsensitive)) {
                ret += r;
            }
        }
    }
    return new ResultsStream(QStringLiteral("AlpineApkStream"), ret);
}

ResultsStream *AlpineApkBackend::findResourceByPackageName(const QUrl &searchUrl)
{
//    if (search.isLocalFile()) {
//        AlpineApkResource* res = new AlpineApkResource(
//                    search.fileName(), AbstractResource::Technical, this);
//        res->setSize(666);
//        res->setState(AbstractResource::None);
//        m_resources.insert(res->packageName(), res);
//        connect(res, &AlpineApkResource::stateChanged, this, &AlpineApkBackend::updatesCountChanged);
//        return new ResultsStream(QStringLiteral("AlpineApkStream-local"), { res });
//    }

    AlpineApkResource *result = nullptr;

    // QUrl("appstream://org.kde.krita.desktop")
    // smart workaround for appstream URLs
    if (searchUrl.scheme()  == QLatin1String("appstream")) {
        // remove leading "org.kde."
        QString pkgName = searchUrl.host();
        if (pkgName.startsWith(QLatin1String("org.kde."))) {
            pkgName = pkgName.mid(8);
        }
        // remove trailing ".desktop"
        if (pkgName.endsWith(QLatin1String(".desktop"))) {
            pkgName = pkgName.left(pkgName.length() - 8);
        }
        // now we can search for "krita" package
        result = m_resources.value(pkgName);
    }

    if (!result) {
        return new ResultsStream(QStringLiteral("AlpineApkStream"), {});
    }
    return new ResultsStream(QStringLiteral("AlpineApkStream"), { result });
}

AbstractBackendUpdater *AlpineApkBackend::backendUpdater() const
{
    return m_updater;
}

AbstractReviewsBackend *AlpineApkBackend::reviewsBackend() const
{
    return m_reviews;
}

Transaction* AlpineApkBackend::installApplication(AbstractResource *app, const AddonList &addons)
{
    return new AlpineApkTransaction(qobject_cast<AlpineApkResource *>(app), addons, Transaction::InstallRole);
}

Transaction* AlpineApkBackend::installApplication(AbstractResource *app)
{
    return new AlpineApkTransaction(qobject_cast<AlpineApkResource *>(app), Transaction::InstallRole);
}

Transaction* AlpineApkBackend::removeApplication(AbstractResource *app)
{
    return new AlpineApkTransaction(qobject_cast<AlpineApkResource *>(app), Transaction::RemoveRole);
}

int AlpineApkBackend::fetchingUpdatesProgress() const
{
    if (!m_fetching) return 100;
    return m_fetchProgress;
}

void AlpineApkBackend::checkForUpdates()
{
    if (m_fetching) {
        qCDebug(LOG_ALPINEAPK) << "backend: checkForUpdates(): already fetching";
        return;
    }

    qCDebug(LOG_ALPINEAPK) << "backend: start checkForUpdates()";

    // safety measure - finish updates check in some time
    m_updatesTimeoutTimer->start();

    // let our updater do the job
    m_updater->startCheckForUpdates();

    // update UI
    m_fetching = true;
    m_fetchProgress = 0;
    emit fetchingChanged();
    emit fetchingUpdatesProgressChanged();
}

void AlpineApkBackend::finishCheckForUpdates()
{
    m_updatesTimeoutTimer->stop(); // stop safety timer
    // update UI
    m_fetching = false;
    emit fetchingChanged();
    emit fetchingUpdatesProgressChanged();
}

QString AlpineApkBackend::displayName() const
{
    return i18nc("Backend plugin display name", "Alpine APK backend");
}

bool AlpineApkBackend::hasApplications() const
{
    return true;
}

void AlpineApkBackend::setFetchingUpdatesProgress(int percent)
{
    m_fetchProgress = percent;
    emit fetchingUpdatesProgressChanged();
}

#include "AlpineApkBackend.moc"
