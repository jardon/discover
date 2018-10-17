/***************************************************************************
 *   Copyright © 2013 Aleix Pol Gonzalez <aleixpol@blue-systems.com>       *
 *   Copyright © 2018 Abhijeet Sharma <sharma.abhijeet2096@gmail.com>      *
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

#ifndef FWUPDRESOURCE_H
#define FWUPDRESOURCE_H

#include "FwupdBackend.h"

#include <resources/AbstractResource.h>
#include <KLocalizedString>

class AddonList;
class FwupdResource : public AbstractResource
{
Q_OBJECT
public:
    explicit FwupdResource(QString name, AbstractResourcesBackend* parent);

    QList<PackageState> addonsInformation() override;
    QString section() override;
    QString origin() const override;
    QString longDescription() override;
    QString availableVersion() const override;
    QString installedVersion() const override;
    QString license() override;
    int size() override;
    QUrl homepage() override;
    QUrl helpURL() override;
    QUrl bugURL() override;
    QUrl donationURL() override;
    QStringList categories() override;
    AbstractResource::State state() override;
    QVariant icon() const override;
    QString comment() override;
    QString name() const override;
    QString packageName() const override;
    QString vendor() const;
    bool isTechnical() const override { return true; }
    bool canExecute() const override { return false; }
    void invokeApplication() const override;
    void fetchChangelog() override;
    QUrl url() const override;
    QString executeLabel() const override;
    QDate releaseDate() const override { return m_releaseDate; }
    QString sourceIcon() const override { return {}; }
    void setDeviceID(const QString &deviceId) { m_deviceID = deviceId; }
    void setIsDeviceLocked(bool locked) { isDeviceLocked = locked; }
    void setDescription(const QString &description) { m_description = description; }
    void setId(const QString &id) { m_id = id; }

    void setState(AbstractResource::State state);
    void setAddonInstalled(const QString& addon, bool installed);
    void setReleaseDetails(FwupdRelease *release);
    void setDeviceDetails(FwupdDevice* device);

public:
    QString m_id;
    QString m_name;
    QString m_summary;
    QString m_description;
    QString m_version;
    QString m_vendor;
    QStringList m_categories;
    QString m_license;
    QDate m_releaseDate;

    AbstractResource::State m_state;
    QUrl m_homepage;
    QString m_iconName;
    QList<PackageState> m_addons;
    int m_size = 0;

    QString m_deviceID;
    QString m_updateURI;
    QString m_file;
    bool isDeviceLocked = false; // True if device is locked!
    bool isOnlyOffline = false; // True if only offline updates
    bool isLiveUpdatable = false; // True if device is live updatable
    bool needsReboot = false; // True if device needs Reboot
    bool isDeviceRemoval = false; //True if device is Removal
    bool needsBootLoader = false; //True if BootLoader Required
    QString guidString;
    QString m_origin;
};

#endif // FWUPDRESOURCE_H
