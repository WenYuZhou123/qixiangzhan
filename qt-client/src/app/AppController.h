#pragma once

#include <QAbstractItemModel>
#include <QObject>

#include "auth/AuthSession.h"
#include "data/ApiClient.h"
#include "data/AlarmRepository.h"
#include "data/DeviceRepository.h"
#include "data/HistoryRepository.h"
#include "data/RemoteSyncService.h"
#include "data/SQLiteCache.h"
#include "data/UserAdminService.h"
#include "domain/CommandTracker.h"
#include "domain/DeviceStateStore.h"
#include "realtime/RealtimeGateway.h"
#include "serial/SerialConsoleService.h"

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(AuthSession *authSession READ authSession CONSTANT)
    Q_PROPERTY(RealtimeGateway *realtimeGateway READ realtimeGateway CONSTANT)
    Q_PROPERTY(DeviceRepository *deviceRepository READ deviceRepository CONSTANT)
    Q_PROPERTY(HistoryRepository *historyRepository READ historyRepository CONSTANT)
    Q_PROPERTY(AlarmRepository *alarmRepository READ alarmRepository CONSTANT)
    Q_PROPERTY(SerialConsoleService *serialConsoleService READ serialConsoleService CONSTANT)
    Q_PROPERTY(RemoteSyncService *remoteSyncService READ remoteSyncService CONSTANT)
    Q_PROPERTY(UserAdminService *userAdminService READ userAdminService CONSTANT)
    Q_PROPERTY(QString localDatabasePath READ localDatabasePath CONSTANT)
    Q_PROPERTY(bool androidMode READ androidMode WRITE setAndroidMode NOTIFY androidModeChanged)
    Q_PROPERTY(bool engineeringMode READ engineeringMode WRITE setEngineeringMode NOTIFY engineeringModeChanged)
    Q_PROPERTY(QString currentPage READ currentPage WRITE setCurrentPage NOTIFY currentPageChanged)
    Q_PROPERTY(bool commandPending READ commandPending NOTIFY controlStateChanged)
    Q_PROPERTY(bool commandConnected READ commandConnected NOTIFY controlStateChanged)
    Q_PROPERTY(QString runtimeConnectionState READ runtimeConnectionState NOTIFY controlStateChanged)
    Q_PROPERTY(bool runtimeConnected READ runtimeConnected NOTIFY controlStateChanged)
    Q_PROPERTY(QAbstractItemModel *logModel READ logModel CONSTANT)

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

    AuthSession *authSession() const;
    RealtimeGateway *realtimeGateway() const;
    DeviceRepository *deviceRepository() const;
    HistoryRepository *historyRepository() const;
    AlarmRepository *alarmRepository() const;
    SerialConsoleService *serialConsoleService() const;
    RemoteSyncService *remoteSyncService() const;
    UserAdminService *userAdminService() const;
    QString localDatabasePath() const;
    bool androidMode() const;
    void setAndroidMode(bool androidMode);
    bool engineeringMode() const;
    void setEngineeringMode(bool engineeringMode);
    QString currentPage() const;
    void setCurrentPage(const QString &page);
    bool commandPending() const;
    bool commandConnected() const;
    QString runtimeConnectionState() const;
    bool runtimeConnected() const;
    QAbstractItemModel *logModel() const;

    Q_INVOKABLE void refreshDevices();
    Q_INVOKABLE void refreshCurrentDevice();
    Q_INVOKABLE void refreshHistory();
    Q_INVOKABLE void refreshAlarms();
    Q_INVOKABLE void padOpen();
    Q_INVOKABLE void padClose();
    Q_INVOKABLE void padStop();
    Q_INVOKABLE void queryPadStatus();
    Q_INVOKABLE void setRelay1(bool enabled);
    Q_INVOKABLE void setRelay2(bool enabled);
    Q_INVOKABLE void setAll(bool enabled);
    Q_INVOKABLE void queryStatus();
    Q_INVOKABLE void retryLastCommand();

signals:
    void androidModeChanged();
    void engineeringModeChanged();
    void currentPageChanged();
    void controlStateChanged();

private:
    void syncCurrentDeviceSelection(const QString &deviceId);
    bool useRemoteRuntime() const;

    SQLiteCache *m_cache = nullptr;
    DeviceStateStore *m_stateStore = nullptr;
    CommandTracker *m_commandTracker = nullptr;
    DeviceRepository *m_deviceRepository = nullptr;
    HistoryRepository *m_historyRepository = nullptr;
    AlarmRepository *m_alarmRepository = nullptr;
    AuthSession *m_authSession = nullptr;
    ApiClient *m_apiClient = nullptr;
    SerialConsoleService *m_serialConsoleService = nullptr;
    RealtimeGateway *m_realtimeGateway = nullptr;
    RemoteSyncService *m_remoteSyncService = nullptr;
    UserAdminService *m_userAdminService = nullptr;
    bool m_androidMode = false;
    bool m_engineeringMode = false;
    QString m_currentPage = QStringLiteral("overview");
};
