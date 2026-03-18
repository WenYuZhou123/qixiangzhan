#include "AppController.h"

#include <QDateTime>

#include "auth/AuthSession.h"
#include "data/ApiClient.h"
#include "data/AlarmRepository.h"
#include "data/DeviceRepository.h"
#include "data/HistoryRepository.h"
#include "data/RemoteSyncService.h"
#include "data/SQLiteCache.h"
#include "domain/CommandTracker.h"
#include "domain/DeviceStateStore.h"
#include "realtime/RealtimeGateway.h"
#include "serial/SerialConsoleService.h"

AppController::AppController(QObject *parent)
    : QObject(parent)
{
    m_cache = new SQLiteCache(this);
    m_cache->initialize();
    m_engineeringMode = m_cache->setting(QStringLiteral("app.engineeringMode"), QStringLiteral("0")) == QStringLiteral("1");

    m_stateStore = new DeviceStateStore(this);
    m_commandTracker = new CommandTracker(this);
    m_deviceRepository = new DeviceRepository(this);
    m_historyRepository = new HistoryRepository(m_cache, this);
    m_alarmRepository = new AlarmRepository(m_cache, this);
    m_authSession = new AuthSession(m_cache, this);
    m_apiClient = new ApiClient(m_authSession, this);
    m_serialConsoleService = new SerialConsoleService(this);
    m_realtimeGateway = new RealtimeGateway(m_cache,
                                            m_stateStore,
                                            m_commandTracker,
                                            m_deviceRepository,
                                            m_historyRepository,
                                            m_alarmRepository,
                                            this);
    m_remoteSyncService = new RemoteSyncService(m_apiClient,
                                                m_cache,
                                                m_authSession,
                                                m_stateStore,
                                                m_commandTracker,
                                                m_deviceRepository,
                                                m_historyRepository,
                                                m_alarmRepository,
                                                m_realtimeGateway,
                                                this);
    m_userAdminService = new UserAdminService(m_apiClient, m_authSession, this);

    const QVector<DeviceState> cachedStates = m_cache->loadDeviceStates();
    m_stateStore->hydrateStates(cachedStates);
    m_deviceRepository->loadStates(cachedStates);

    connect(m_deviceRepository, &DeviceRepository::currentDeviceIdChanged, this, [this]() {
        syncCurrentDeviceSelection(m_deviceRepository->currentDeviceId());
    });

    connect(m_alarmRepository, &AlarmRepository::activeCountsChanged, this, [this](const QHash<QString, int> &counts) {
        m_deviceRepository->setActiveAlarmCounts(counts);
        for (auto it = counts.cbegin(); it != counts.cend(); ++it)
        {
            m_stateStore->updateAlarmCount(it.key(), it.value());
        }
    });

    connect(m_serialConsoleService, &SerialConsoleService::lineCaptured, this, [this](const QString &direction, const QString &text) {
        MessageRecord record;
        record.deviceId = m_deviceRepository->currentDeviceId();
        record.direction = direction;
        record.channel = QStringLiteral("serial");
        record.topic = m_serialConsoleService->selectedPort();
        record.payload = text;
        record.level = QStringLiteral("info");
        record.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        m_historyRepository->appendRecord(record);
    });

    connect(m_authSession, &AuthSession::sessionChanged, this, [this]() {
        if (m_remoteSyncService != nullptr)
        {
            m_remoteSyncService->refreshVisibleData();
        }
        emit controlStateChanged();
    });
    connect(m_realtimeGateway, &RealtimeGateway::connectionStateChanged, this, &AppController::controlStateChanged);
    connect(m_remoteSyncService, &RemoteSyncService::connectionStateChanged, this, &AppController::controlStateChanged);
    connect(m_remoteSyncService, &RemoteSyncService::lastErrorChanged, this, &AppController::controlStateChanged);
    connect(m_commandTracker, &CommandTracker::pendingChanged, this, &AppController::controlStateChanged);
    connect(m_commandTracker, &CommandTracker::retryChanged, this, &AppController::controlStateChanged);
    connect(m_commandTracker, &CommandTracker::lastErrorChanged, this, &AppController::controlStateChanged);

    if (!m_deviceRepository->currentDeviceId().isEmpty())
    {
        syncCurrentDeviceSelection(m_deviceRepository->currentDeviceId());
    }
}

AppController::~AppController() = default;

AuthSession *AppController::authSession() const
{
    return m_authSession;
}

RealtimeGateway *AppController::realtimeGateway() const
{
    return m_realtimeGateway;
}

DeviceRepository *AppController::deviceRepository() const
{
    return m_deviceRepository;
}

HistoryRepository *AppController::historyRepository() const
{
    return m_historyRepository;
}

AlarmRepository *AppController::alarmRepository() const
{
    return m_alarmRepository;
}

SerialConsoleService *AppController::serialConsoleService() const
{
    return m_serialConsoleService;
}

RemoteSyncService *AppController::remoteSyncService() const
{
    return m_remoteSyncService;
}

UserAdminService *AppController::userAdminService() const
{
    return m_userAdminService;
}

QString AppController::localDatabasePath() const
{
    return m_cache != nullptr ? m_cache->databasePath() : QString();
}

bool AppController::androidMode() const
{
    return m_androidMode;
}

void AppController::setAndroidMode(bool androidModeValue)
{
    if (m_androidMode == androidModeValue)
    {
        return;
    }

    m_androidMode = androidModeValue;
    if (m_remoteSyncService != nullptr)
    {
        m_remoteSyncService->setAndroidMode(androidModeValue);
    }
    if (m_realtimeGateway != nullptr)
    {
        m_realtimeGateway->setIgnoreCommandTimeouts(androidModeValue);
    }
    emit androidModeChanged();
    emit controlStateChanged();
}

bool AppController::engineeringMode() const
{
    return m_engineeringMode;
}

void AppController::setEngineeringMode(bool engineeringModeValue)
{
    if (m_engineeringMode == engineeringModeValue)
    {
        return;
    }

    m_engineeringMode = engineeringModeValue;
    if (m_cache != nullptr)
    {
        m_cache->setSetting(QStringLiteral("app.engineeringMode"), engineeringModeValue ? QStringLiteral("1")
                                                                                         : QStringLiteral("0"));
    }
    if (!m_engineeringMode &&
        (m_currentPage == QStringLiteral("engineering") ||
         m_currentPage == QStringLiteral("serial") ||
         m_currentPage == QStringLiteral("logs")))
    {
        setCurrentPage(QStringLiteral("overview"));
    }
    emit engineeringModeChanged();
    emit controlStateChanged();
}

QString AppController::currentPage() const
{
    return m_currentPage;
}

void AppController::setCurrentPage(const QString &page)
{
    const QString normalized = page.trimmed().isEmpty() ? QStringLiteral("overview") : page.trimmed();
    if (m_currentPage == normalized)
    {
        return;
    }

    m_currentPage = normalized;
    if (m_remoteSyncService != nullptr)
    {
        m_remoteSyncService->setCurrentPage(normalized);
    }
    emit currentPageChanged();
}

bool AppController::commandPending() const
{
    return m_commandTracker != nullptr ? m_commandTracker->pending() : false;
}

bool AppController::commandConnected() const
{
    return useRemoteRuntime()
        ? (m_remoteSyncService != nullptr && m_remoteSyncService->connected())
        : (m_realtimeGateway != nullptr && m_realtimeGateway->connected());
}

QString AppController::runtimeConnectionState() const
{
    if (m_authSession != nullptr && m_authSession->guestMode())
    {
        return QStringLiteral("游客预览");
    }
    return useRemoteRuntime()
        ? (m_remoteSyncService != nullptr ? m_remoteSyncService->connectionState() : QStringLiteral("API idle"))
        : (m_realtimeGateway != nullptr ? m_realtimeGateway->connectionState() : QStringLiteral("Disconnected"));
}

bool AppController::runtimeConnected() const
{
    if (m_authSession != nullptr && m_authSession->guestMode())
    {
        return false;
    }
    return useRemoteRuntime()
        ? (m_remoteSyncService != nullptr && m_remoteSyncService->connected())
        : (m_realtimeGateway != nullptr && m_realtimeGateway->connected());
}

QAbstractItemModel *AppController::logModel() const
{
    return m_realtimeGateway != nullptr ? m_realtimeGateway->logModel() : nullptr;
}

void AppController::refreshDevices()
{
    if (m_remoteSyncService != nullptr)
    {
        m_remoteSyncService->refreshDevices();
    }
}

void AppController::refreshCurrentDevice()
{
    if (m_remoteSyncService != nullptr)
    {
        m_remoteSyncService->refreshCurrentDevice();
    }
}

void AppController::refreshHistory()
{
    if (m_remoteSyncService != nullptr && m_authSession != nullptr && m_authSession->remoteAuthenticated())
    {
        m_remoteSyncService->refreshHistory();
        return;
    }
    if (m_historyRepository != nullptr)
    {
        m_historyRepository->setPreferRemote(false);
        m_historyRepository->reload();
    }
}

void AppController::refreshAlarms()
{
    if (m_remoteSyncService != nullptr && m_authSession != nullptr && m_authSession->remoteAuthenticated())
    {
        m_remoteSyncService->refreshAlarms();
        return;
    }
    if (m_alarmRepository != nullptr)
    {
        m_alarmRepository->reload();
    }
}

void AppController::padOpen()
{
    if (useRemoteRuntime() && m_remoteSyncService != nullptr)
    {
        m_remoteSyncService->sendCommand(QStringLiteral("pad_open"),
                                         0,
                                         false,
                                         m_androidMode ? QStringLiteral("qt-android")
                                                       : QStringLiteral("qt-desktop"));
        return;
    }
    if (m_realtimeGateway != nullptr)
    {
        m_realtimeGateway->padOpen();
    }
}

void AppController::padClose()
{
    if (useRemoteRuntime() && m_remoteSyncService != nullptr)
    {
        m_remoteSyncService->sendCommand(QStringLiteral("pad_close"),
                                         0,
                                         false,
                                         m_androidMode ? QStringLiteral("qt-android")
                                                       : QStringLiteral("qt-desktop"));
        return;
    }
    if (m_realtimeGateway != nullptr)
    {
        m_realtimeGateway->padClose();
    }
}

void AppController::padStop()
{
    if (useRemoteRuntime() && m_remoteSyncService != nullptr)
    {
        m_remoteSyncService->sendCommand(QStringLiteral("pad_stop"),
                                         0,
                                         false,
                                         m_androidMode ? QStringLiteral("qt-android")
                                                       : QStringLiteral("qt-desktop"));
        return;
    }
    if (m_realtimeGateway != nullptr)
    {
        m_realtimeGateway->padStop();
    }
}

void AppController::queryPadStatus()
{
    if (useRemoteRuntime() && m_remoteSyncService != nullptr)
    {
        m_remoteSyncService->sendCommand(QStringLiteral("query_pad_status"),
                                         0,
                                         false,
                                         m_androidMode ? QStringLiteral("qt-android")
                                                       : QStringLiteral("qt-desktop"));
        return;
    }
    if (m_realtimeGateway != nullptr)
    {
        m_realtimeGateway->queryPadStatus();
    }
}

void AppController::setRelay1(bool enabled)
{
    if (useRemoteRuntime() && m_remoteSyncService != nullptr)
    {
        m_remoteSyncService->sendCommand(QStringLiteral("set_r1"),
                                         enabled ? 1 : 0,
                                         true,
                                         m_androidMode ? QStringLiteral("qt-android")
                                                       : QStringLiteral("qt-desktop"));
        return;
    }
    if (m_realtimeGateway != nullptr)
    {
        m_realtimeGateway->setRelay1(enabled);
    }
}

void AppController::setRelay2(bool enabled)
{
    if (useRemoteRuntime() && m_remoteSyncService != nullptr)
    {
        m_remoteSyncService->sendCommand(QStringLiteral("set_r2"),
                                         enabled ? 1 : 0,
                                         true,
                                         m_androidMode ? QStringLiteral("qt-android")
                                                       : QStringLiteral("qt-desktop"));
        return;
    }
    if (m_realtimeGateway != nullptr)
    {
        m_realtimeGateway->setRelay2(enabled);
    }
}

void AppController::setAll(bool enabled)
{
    if (useRemoteRuntime() && m_remoteSyncService != nullptr)
    {
        m_remoteSyncService->sendCommand(QStringLiteral("set_all"),
                                         enabled ? 1 : 0,
                                         true,
                                         m_androidMode ? QStringLiteral("qt-android")
                                                       : QStringLiteral("qt-desktop"));
        return;
    }
    if (m_realtimeGateway != nullptr)
    {
        m_realtimeGateway->setAll(enabled);
    }
}

void AppController::queryStatus()
{
    if (useRemoteRuntime() && m_remoteSyncService != nullptr)
    {
        m_remoteSyncService->sendCommand(QStringLiteral("query_status"),
                                         0,
                                         false,
                                         m_androidMode ? QStringLiteral("qt-android")
                                                       : QStringLiteral("qt-desktop"));
        return;
    }
    if (m_realtimeGateway != nullptr)
    {
        m_realtimeGateway->queryStatus();
    }
}

void AppController::retryLastCommand()
{
    if (m_commandTracker == nullptr || !m_commandTracker->canRetry())
    {
        if (m_stateStore != nullptr)
        {
            m_stateStore->setLastError(QStringLiteral("No retry command available"));
        }
        return;
    }

    if (useRemoteRuntime() && m_remoteSyncService != nullptr)
    {
        m_remoteSyncService->sendCommand(m_commandTracker->lastCommand(),
                                         m_commandTracker->lastValue(),
                                         m_commandTracker->lastHasValue(),
                                         m_androidMode ? QStringLiteral("qt-android")
                                                       : QStringLiteral("qt-desktop"));
        return;
    }
    if (m_realtimeGateway != nullptr)
    {
        m_realtimeGateway->retryLastCommand();
    }
}

void AppController::syncCurrentDeviceSelection(const QString &deviceId)
{
    if (deviceId.isEmpty())
    {
        return;
    }

    m_stateStore->setCurrentDeviceId(deviceId);
    m_historyRepository->setCurrentDeviceId(deviceId);
    m_alarmRepository->setCurrentDeviceId(deviceId);
    m_realtimeGateway->setCurrentDeviceId(deviceId);
    if (m_remoteSyncService != nullptr)
    {
        m_remoteSyncService->setCurrentDeviceId(deviceId);
    }
}

bool AppController::useRemoteRuntime() const
{
    return m_authSession != nullptr &&
           m_authSession->remoteAuthenticated() &&
           (!m_engineeringMode || m_androidMode);
}
