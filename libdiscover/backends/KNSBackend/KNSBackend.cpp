/***************************************************************************
 *   Copyright © 2012 Aleix Pol Gonzalez <aleixpol@blue-systems.com>       *
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

// Qt includes
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTimer>
#include <QDirIterator>

// Attica includes
#include <attica/content.h>
#include <attica/providermanager.h>

// KDE includes
#include <KNSCore/Engine>
#include <KNSCore/QuestionManager>
#include <KConfigGroup>
#include <KDesktopFile>
#include <KLocalizedString>

// DiscoverCommon includes
#include "Transaction/Transaction.h"
#include "Transaction/TransactionModel.h"
#include "Category/Category.h"

// Own includes
#include "KNSBackend.h"
#include "KNSResource.h"
#include "KNSReviews.h"
#include <resources/StandardBackendUpdater.h>

class KNSBackendFactory : public AbstractResourcesBackendFactory {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.muon.AbstractResourcesBackendFactory")
    Q_INTERFACES(AbstractResourcesBackendFactory)
    public:
        KNSBackendFactory() {
            connect(KNSCore::QuestionManager::instance(), &KNSCore::QuestionManager::askQuestion, this, [](KNSCore::Question* q) {
                qWarning() << q->question() << q->questionType();
                q->setResponse(KNSCore::Question::InvalidResponse);
            });
        }

        QVector<AbstractResourcesBackend*> newInstance(QObject* parent, const QString &/*name*/) const override
        {
            QVector<AbstractResourcesBackend*> ret;
            for (const QString &path: QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation)) {
                QDirIterator dirIt(path, {QStringLiteral("*.knsrc")}, QDir::Files);
                for(; dirIt.hasNext(); ) {
                    dirIt.next();

                    auto bk = new KNSBackend(parent, QStringLiteral("plasma"), dirIt.filePath());
                    ret += bk;
                }
            }
            return ret;
        }
};

KNSBackend::KNSBackend(QObject* parent, const QString& iconName, const QString &knsrc)
    : AbstractResourcesBackend(parent)
    , m_fetching(false)
    , m_isValid(true)
    , m_page(0)
    , m_reviews(new KNSReviews(this))
    , m_name(knsrc)
    , m_iconName(iconName)
    , m_updater(new StandardBackendUpdater(this))
{
    const QString fileName = QFileInfo(m_name).fileName();
    setName(fileName);
    setObjectName(knsrc);

    const KConfig conf(m_name);
    if (!conf.hasGroup("KNewStuff3")) {
        markInvalid(QStringLiteral("Config group not found! Check your KNS3 installation."));
        return;
    }

    m_categories = QStringList{ fileName };

    const KConfigGroup group = conf.group("KNewStuff3");
    m_extends = group.readEntry("Extends", QStringList());
    m_reviews->setProviderUrl(QUrl(group.readEntry("ProvidersUrl", QString())));

    setFetching(true);

    m_engine = new KNSCore::Engine(this);
    m_engine->init(m_name);
    connect(m_engine, &KNSCore::Engine::signalError, this, [this](const QString &error) { qWarning() << "kns error" << objectName() << error; });
    connect(m_engine, &KNSCore::Engine::signalEntriesLoaded, this, &KNSBackend::receivedEntries);
    connect(m_engine, &KNSCore::Engine::signalEntryChanged, this, &KNSBackend::statusChanged);
    connect(m_engine, &KNSCore::Engine::signalEntryDetailsLoaded, this, &KNSBackend::statusChanged);
    m_page = -1;
    connect(m_engine, &KNSCore::Engine::signalProvidersLoaded, m_engine, &KNSCore::Engine::checkForInstalled);
    m_responsePending = true;

    const QVector<QPair<FilterType, QString>> filters = { {CategoryFilter, fileName } };
    const QSet<QString> backendName = { name() };
    QString displayName = group.readEntry("Name", QString());
    if (displayName.isEmpty()) {
        displayName = fileName.mid(0, fileName.indexOf(QLatin1Char('.')));
        displayName[0] = displayName[0].toUpper();
    }

    static const QSet<QString> knsrcPlasma = {
        QStringLiteral("aurorae.knsrc"), QStringLiteral("icons.knsrc"), QStringLiteral("kfontinst.knsrc"), QStringLiteral("lookandfeel.knsrc"), QStringLiteral("plasma-themes.knsrc"), QStringLiteral("plasmoids.knsrc"),
        QStringLiteral("wallpaper.knsrc"), QStringLiteral("xcursor.knsrc"),

        QStringLiteral("cgcgtk3.knsrc"), QStringLiteral("cgcicon.knsrc"), QStringLiteral("cgctheme.knsrc"), //GTK integration
        QStringLiteral("kwinswitcher.knsrc"), QStringLiteral("kwineffect.knsrc"), QStringLiteral("kwinscripts.knsrc") //KWin
    };
    auto actualCategory = new Category(displayName, QStringLiteral("plasma"), filters, backendName, {}, QUrl(), true);

    const auto topLevelName = knsrcPlasma.contains(fileName)? i18n("Plasma Addons") : i18n("Application Addons");
    const QUrl decoration(knsrcPlasma.contains(fileName)? QStringLiteral("https://c2.staticflickr.com/4/3148/3042248532_20bd2e38f4_b.jpg") : QStringLiteral("https://c2.staticflickr.com/8/7067/6847903539_d9324dcd19_o.jpg"));
    auto addonsCategory = new Category(topLevelName, QStringLiteral("plasma"), filters, backendName, {actualCategory}, decoration, true);
    m_rootCategories = { addonsCategory };
}

KNSBackend::~KNSBackend() = default;

void KNSBackend::markInvalid(const QString &message)
{
    qWarning() << "invalid kns backend!" << m_name << "because:" << message;
    m_isValid = false;
    setFetching(false);
}

void KNSBackend::setFetching(bool f)
{
    if(m_fetching!=f) {
        m_fetching = f;
        emit fetchingChanged();
    }
}

bool KNSBackend::isValid() const
{
    return m_isValid;
}

KNSResource* KNSBackend::resourceForEntry(const KNSCore::EntryInternal& entry)
{
    KNSResource* r = static_cast<KNSResource*>(m_resourcesByName.value(entry.uniqueId()));
    if (!r) {
        r = new KNSResource(entry, m_categories, this);
        m_resourcesByName.insert(entry.uniqueId(), r);
    } else {
        r->setEntry(entry);
    }
    return r;
}

void KNSBackend::receivedEntries(const KNSCore::EntryInternal::List& entries)
{
    m_responsePending = false;

    if(entries.isEmpty()) {
        Q_EMIT searchFinished();
        Q_EMIT availableForQueries();
        setFetching(false);
        return;
    }

    QVector<AbstractResource*> resources;
    resources.reserve(entries.count());
    foreach(const KNSCore::EntryInternal& entry, entries) {
        resources += resourceForEntry(entry);
    }
//     qDebug() << "received" << this << m_page << m_resourcesByName.count();
    Q_EMIT receivedResources(resources);
    if (m_page >= 0 && !m_responsePending) {
        ++m_page;
        m_engine->requestData(m_page, 100);
        m_responsePending = true;
    } else {
        Q_EMIT availableForQueries();
    }
}

void KNSBackend::statusChanged(const KNSCore::EntryInternal& entry)
{
    resourceForEntry(entry);
}

class KNSTransaction : public Transaction
{
public:
    KNSTransaction(QObject* parent, KNSResource* res, Transaction::Role role)
        : Transaction(parent, res, role)
        , m_id(res->entry().uniqueId())
    {
        TransactionModel::global()->addTransaction(this);

        setCancellable(false);

        auto manager = res->knsBackend()->downloadManager();
        connect(manager, &KNSCore::Engine::signalEntryChanged, this, &KNSTransaction::anEntryChanged);
    }

    void anEntryChanged(const KNSCore::EntryInternal& entry) {
        if (entry.uniqueId() == m_id) {
            switch (entry.status()) {
                case KNS3::Entry::Invalid:
                    qWarning() << "invalid status for" << entry.uniqueId() << entry.status();
                    break;
                case KNS3::Entry::Installing:
                case KNS3::Entry::Updating:
                    setStatus(CommittingStatus);
                    break;
                case KNS3::Entry::Downloadable:
                case KNS3::Entry::Installed:
                case KNS3::Entry::Deleted:
                case KNS3::Entry::Updateable:
                    if (status() != DoneStatus) {
                        setStatus(DoneStatus);
                        TransactionModel::global()->removeTransaction(this);
                    }
                    break;
            }
        }
    }

    ~KNSTransaction() override {
        if (TransactionModel::global()->contains(this)) {
            qWarning() << "deleting Transaction before it's done";
                TransactionModel::global()->removeTransaction(this);
        }
    }

    void cancel() override {}

private:
    const QString m_id;
};

void KNSBackend::removeApplication(AbstractResource* app)
{
    auto res = qobject_cast<KNSResource*>(app);
    new KNSTransaction(this, res, Transaction::RemoveRole);
    m_engine->uninstall(res->entry());
}

void KNSBackend::installApplication(AbstractResource* app)
{
    auto res = qobject_cast<KNSResource*>(app);
    m_engine->install(res->entry());
    new KNSTransaction(this, res, Transaction::InstallRole);
}

void KNSBackend::installApplication(AbstractResource* app, const AddonList& /*addons*/)
{
    installApplication(app);
}

int KNSBackend::updatesCount() const
{
    return m_updater->updatesCount();
}

AbstractReviewsBackend* KNSBackend::reviewsBackend() const
{
    return m_reviews;
}

static ResultsStream* voidStream()
{
    return new ResultsStream(QStringLiteral("KNS-void"), {});
}

ResultsStream* KNSBackend::search(const AbstractResourcesBackend::Filters& filter)
{
    if (filter.state >= AbstractResource::Installed) {
        QVector<AbstractResource*> ret;
        foreach(AbstractResource* r, m_resourcesByName) {
            if(r->state()>=filter.state && (r->name().contains(filter.search, Qt::CaseInsensitive) || r->comment().contains(filter.search, Qt::CaseInsensitive)))
                ret += r;
        }
        return new ResultsStream(QStringLiteral("KNS-installed"), ret);
    } else if (filter.category && filter.category->matchesCategoryName(m_categories.first())) {
        return searchStream(filter.search);
    } else /*if (!filter.search.isEmpty())*/ {
        return searchStream(filter.search);
    }
    return voidStream();
}

ResultsStream * KNSBackend::searchStream(const QString &searchText)
{
    Q_EMIT startingSearch();

    auto stream = new ResultsStream(QStringLiteral("KNS-search-")+name());
    auto start = [this, stream, searchText]() {
        m_engine->setSearchTerm(searchText);
        m_engine->requestData(0, 100);
        m_responsePending = true;
        m_page = 0;
        connect(this, &KNSBackend::receivedResources, stream, &ResultsStream::resourcesFound);
        connect(this, &KNSBackend::searchFinished, stream, &ResultsStream::deleteLater);
        connect(this, &KNSBackend::startingSearch, stream, &ResultsStream::deleteLater);
    };
    if (m_responsePending) {
        connect(this, &KNSBackend::availableForQueries, stream, start);
    } else {
        start();
    }
    return stream;
}

ResultsStream * KNSBackend::findResourceByPackageName(const QUrl& search)
{
    if (search.scheme() != QLatin1String("kns") || search.host() != name())
        return voidStream();

    const auto pathParts = search.path().split(QLatin1Char('/'), QString::SkipEmptyParts);
    if (pathParts.size() != 2) {
        passiveMessage(i18n("Wrong KNewStuff URI: %1", search.toString()));
        return voidStream();
    }
    const auto providerid = pathParts.at(0);
    const auto entryid = pathParts.at(1);

    auto stream = new ResultsStream(QStringLiteral("KNS-byname-")+entryid);

    auto start = [this, entryid, stream]() {
        m_responsePending = true;
        m_engine->fetchEntryById(entryid);
        connect(m_engine, &KNSCore::Engine::signalEntryDetailsLoaded, stream, [this, stream, entryid](const KNSCore::EntryInternal &entry) {
            if (entry.uniqueId() == entryid) {
                stream->resourcesFound({resourceForEntry(entry)});
            }
            m_responsePending = false;
            QTimer::singleShot(0, this, &KNSBackend::availableForQueries);
            stream->deleteLater();
        });
    };
    if (m_responsePending) {
        connect(this, &KNSBackend::availableForQueries, stream, start);
    } else {
        start();
    }
    return stream;
}

bool KNSBackend::isFetching() const
{
    return m_fetching;
}

AbstractBackendUpdater* KNSBackend::backendUpdater() const
{
    return m_updater;
}

#include "KNSBackend.moc"
