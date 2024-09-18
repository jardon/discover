/*
 *   SPDX-FileCopyrightText: 2016 Aleix Pol Gonzalez <aleixpol@blue-systems.com>
 *
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "FeaturedModel.h"

#include "discover_debug.h"
#include <KConfigGroup>
#include <KIO/StoredTransferJob>
#include <KSharedConfig>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QtGlobal>

#include <resources/ResourcesModel.h>
#include <resources/StoredResultsStream.h>
#include <utils.h>

using namespace Qt::StringLiterals;

Q_GLOBAL_STATIC(QString, featuredCache)

static QString featuredFileName()
{
    // kwriteconfig5 --file discoverrc --group Software --key FeaturedListingFileName featured-5.9.json
    KConfigGroup grp(KSharedConfig::openConfig(), u"Software"_s);
    if (grp.hasKey("FeaturedListingFileName")) {
        return grp.readEntry("FeaturedListingFileName", QString());
    }
    static const bool isMobile = QByteArrayList{"1", "true"}.contains(qgetenv("QT_QUICK_CONTROLS_MOBILE"));
    return isMobile ? QLatin1String("featured-mobile-5.9.json") : QLatin1String("featured-5.9.json");
}

static QString featuredBaseURL()
{
    // kwriteconfig5 --file discoverrc --group Software --key FeaturedListingBaseURL https://autoconfig.kde.org/discover/
    KConfigGroup grp(KSharedConfig::openConfig(), u"Software"_s);
    if (grp.hasKey("FeaturedListingBaseURL")) {
        return grp.readEntry("FeaturedListingBaseURL", QString());
    }
    return QLatin1String("https://autoconfig.kde.org/discover/");
}


FeaturedModel::FeaturedModel()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(dir);

    static const QString fileName = featuredFileName();
    static const QString baseURL = featuredBaseURL();
    *featuredCache = dir + QLatin1Char('/') + fileName;
    const QUrl featuredUrl(baseURL + fileName);
    const bool shouldBlock = !QFileInfo::exists(*featuredCache);
    auto *fetchJob = KIO::storedGet(featuredUrl, KIO::NoReload, KIO::HideProgressInfo);
    if (shouldBlock) {
        acquireFetching(true);
    }
    connect(fetchJob, &KIO::StoredTransferJob::result, this, [this, fetchJob, shouldBlock]() {
        const auto dest = qScopeGuard([this, shouldBlock] {
            if (shouldBlock) {
                acquireFetching(false);
            }
            refresh();
        });
        if (fetchJob->error() != 0)
            return;

        QFile f(*featuredCache);
        if (!f.open(QIODevice::WriteOnly))
            qCWarning(DISCOVER_LOG) << "could not open" << *featuredCache << f.errorString();
        f.write(fetchJob->data());
        f.close();
    });
    if (!shouldBlock) {
        refresh();
    }
}

void FeaturedModel::refresh()
{
    // usually only useful if launching just fwupd or kns backends
    if (!currentApplicationBackend())
        return;

    acquireFetching(true);
    const auto dest = qScopeGuard([this] {
        acquireFetching(false);
    });
    QFile f(*featuredCache);
    if (!f.open(QIODevice::ReadOnly)) {
        qCWarning(DISCOVER_LOG) << "couldn't open file" << *featuredCache << f.errorString();
        return;
    }
    QJsonParseError error;
    const auto array = QJsonDocument::fromJson(f.readAll(), &error).array();
    if (error.error) {
        qCWarning(DISCOVER_LOG) << "couldn't parse" << *featuredCache << ". error:" << error.errorString();
        return;
    }

    const auto uris = kTransform<QVector<QUrl>>(array, [](const QJsonValue &uri) {
        return QUrl(uri.toString());
    });
    setUris(uris);
}

#include "moc_FeaturedModel.cpp"
