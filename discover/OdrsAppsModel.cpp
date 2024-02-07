/*
 *   SPDX-FileCopyrightText: 2022 Aleix Pol Gonzalez <aleixpol@blue-systems.com>
 *
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "OdrsAppsModel.h"
#include <ReviewsBackend/Rating.h>
#include <appstream/OdrsReviewsBackend.h>
#include <utils.h>

using namespace Qt::StringLiterals;

OdrsAppsModel::OdrsAppsModel()
{
    auto backend = OdrsReviewsBackend::global();
    connect(backend.get(), &OdrsReviewsBackend::ratingsReady, this, &OdrsAppsModel::refresh);
    if (!backend->top().isEmpty()) {
        refresh();
    }
}

void OdrsAppsModel::refresh()
{
    const auto top = OdrsReviewsBackend::global()->top();
    setUris(kTransform<QVector<QUrl>>(top, [](auto rating) {
        return QUrl("appstream://"_L1 + rating->packageName());
    }));
}
