/***************************************************************************
 *   Copyright © 2016 Aleix Pol Gonzalez <aleixpol@blue-systems.com>       *
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

#include "FeaturedModel.h"

#include <QDebug>
#include <QStandardPaths>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>
#include <KIO/FileCopyJob>

#include <resources/ResourcesModel.h>
#include <resources/StoredResultsStream.h>

Q_GLOBAL_STATIC(QString, featuredCache)

FeaturedModel::FeaturedModel()
{
    connect(ResourcesModel::global(), &ResourcesModel::backendsChanged, this, &FeaturedModel::refresh);
    connect(ResourcesModel::global(), &ResourcesModel::resourceRemoved, this, &FeaturedModel::removeResource);

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(dir);
    *featuredCache = dir+QLatin1String("/featured-5.9.json");

    const QUrl featuredUrl(QStringLiteral("https://autoconfig.kde.org/discover/featured-5.9.json"));
    KIO::FileCopyJob *getJob = KIO::file_copy(featuredUrl, QUrl::fromLocalFile(*featuredCache), -1, KIO::Overwrite | KIO::HideProgressInfo);
    connect(getJob, &KIO::FileCopyJob::result, this, &FeaturedModel::refresh);
}

void FeaturedModel::refresh()
{
    QSet<ResultsStream*> streams;

    QFile f(*featuredCache);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "couldn't open file" << *featuredCache;
        return;
    }
    QJsonParseError error;
    const auto array = QJsonDocument::fromJson(f.readAll(), &error).array();
    if (error.error) {
        qWarning() << "couldn't parse" << *featuredCache << ". error:" << error.errorString();
        return;
    }

    foreach(const QJsonValue &uri, array) {
        foreach(auto backend, ResourcesModel::global()->backends()) {
            streams << backend->findResourceByPackageName(QUrl(uri.toString()));
        }
    }
    auto stream = new StoredResultsStream(streams);
    connect(stream, &StoredResultsStream::finishedResources, this, &FeaturedModel::setResources);
}

void FeaturedModel::setResources(const QVector<AbstractResource *>& resources)
{
    //TODO: sort like in the json files

    beginResetModel();
    m_resources = resources;
    endResetModel();
}

void FeaturedModel::removeResource(AbstractResource* resource)
{
    int index = m_resources.indexOf(resource);
    if (index<0)
        return;

    beginRemoveRows({}, index, 0);
    m_resources.removeAt(index);
    endRemoveRows();
}

QVariant FeaturedModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || role!=Qt::UserRole)
        return {};

    auto res = m_resources.value(index.row());
    if (!res)
        return {};

    return QVariant::fromValue<QObject*>(res);
}

int FeaturedModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_resources.count();
}

QHash<int, QByteArray> FeaturedModel::roleNames() const
{
    return {{Qt::UserRole, "application"}};
}
