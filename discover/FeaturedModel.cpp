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

static QString featuredURL()
{
    // kwriteconfig5 --file discoverrc --group Software --key FeaturedListingURL https://autoconfig.kde.org/discover/featured-5.9.json
    KConfigGroup grp(KSharedConfig::openConfig(), u"Software"_s);
    if (grp.hasKey("FeaturedListingURL")) {
        return grp.readEntry("FeaturedListingURL", QString());
    }
    return QStringLiteral("https://autoconfig.kde.org/discover/featured-5.9.json");
}

FeaturedModel::FeaturedModel()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(dir);

    static const QString url = featuredURL();
    *featuredCache = dir + QLatin1Char('/featured.json');
    const QUrl featuredUrl(url);
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
