#pragma once

#include <QByteArray>
#include <QObject>
#include <QSet>
#include <QTimer>

#if defined(RELAY_CLIENT_HAS_WEBSOCKETS)
#include <QWebSocket>
#endif

class AlarmRepository;
class ApiClient;
class AuthSession;
class CommandTracker;
class DeviceRepository;
class DeviceStateStore;
class HistoryRepository;
class QJsonObject;
class QNetworkReply;
class RealtimeGateway;
class SQLiteCache;

class RemoteSyncService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool androidMode READ androidMode WRITE setAndroidMode NOTIFY androidModeChanged)
    Q_PROPERTY(QString currentPage READ currentPage WRITE setCurrentPage NOTIFY currentPageChanged)
    Q_PROPERTY(QString currentDeviceId READ currentDeviceId WRITE setCurrentDeviceId NOTIFY currentDeviceIdChanged)
    Q_PROPERTY(QString connectionState READ connectionState NOTIFY connectionStateChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectionStateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString systemHealthSummary READ systemHealthSummary NOTIFY systemHealthChanged)
    Q_PROPERTY(QString systemHealthStatus READ systemHealthStatus NOTIFY systemHealthChanged)
    Q_PROPERTY(bool healthApiOk READ healthApiOk NOTIFY systemHealthChanged)
    Q_PROPERTY(bool healthMysqlOk READ healthMysqlOk NOTIFY systemHealthChanged)
    Q_PROPERTY(bool healthMqttConnected READ healthMqttConnected NOTIFY systemHealthChanged)
    Q_PROPERTY(double healthDiskFreePercent READ healthDiskFreePercent NOTIFY systemHealthChanged)
    Q_PROPERTY(QString healthUptimeText READ healthUptimeText NOTIFY systemHealthChanged)
    Q_PROPERTY(QString healthDeviceSummary READ healthDeviceSummary NOTIFY systemHealthChanged)
    Q_PROPERTY(QString healthLastSeenAt READ healthLastSeenAt NOTIFY systemHealthChanged)
    Q_PROPERTY(QString healthLastWeatherAt READ healthLastWeatherAt NOTIFY systemHealthChanged)
    Q_PROPERTY(QString healthDiskSummary READ healthDiskSummary NOTIFY systemHealthChanged)
    Q_PROPERTY(QString healthMqttSummary READ healthMqttSummary NOTIFY systemHealthChanged)
    Q_PROPERTY(QString healthMysqlSummary READ healthMysqlSummary NOTIFY systemHealthChanged)

public:
    explicit RemoteSyncService(ApiClient *apiClient,
                               SQLiteCache *cache,
                               AuthSession *authSession,
                               DeviceStateStore *stateStore,
                               CommandTracker *commandTracker,
                               DeviceRepository *deviceRepository,
                               HistoryRepository *historyRepository,
                               AlarmRepository *alarmRepository,
                               RealtimeGateway *logGateway,
                               QObject *parent = nullptr);

    bool androidMode() const;
    void setAndroidMode(bool androidMode);
    QString currentPage() const;
    void setCurrentPage(const QString &page);
    QString currentDeviceId() const;
    void setCurrentDeviceId(const QString &deviceId);
    QString connectionState() const;
    bool connected() const;
    QString lastError() const;
    QString systemHealthSummary() const;
    QString systemHealthStatus() const;
    bool healthApiOk() const;
    bool healthMysqlOk() const;
    bool healthMqttConnected() const;
    double healthDiskFreePercent() const;
    QString healthUptimeText() const;
    QString healthDeviceSummary() const;
    QString healthLastSeenAt() const;
    QString healthLastWeatherAt() const;
    QString healthDiskSummary() const;
    QString healthMqttSummary() const;
    QString healthMysqlSummary() const;

    Q_INVOKABLE void refreshDevices();
    Q_INVOKABLE void refreshCurrentDevice();
    Q_INVOKABLE void refreshHistory();
    Q_INVOKABLE void refreshAlarms();
    Q_INVOKABLE void refreshCommands();
    Q_INVOKABLE void refreshSystemHealth();
    Q_INVOKABLE void refreshVisibleData();
    Q_INVOKABLE bool sendCommand(const QString &command, int value, bool hasValue, const QString &operatorName);

signals:
    void androidModeChanged();
    void currentPageChanged();
    void currentDeviceIdChanged();
    void connectionStateChanged();
    void lastErrorChanged();
    void systemHealthChanged();

private slots:
    void pollCurrentPage();
    void pollPendingCommand();

private:
    void appendLog(const QString &line);
    void setConnectionState(const QString &state, bool connected);
    void setLastError(const QString &message);
    void reconfigurePolling();
    bool canUseApi() const;
    bool beginRequest(const QString &key);
    void endRequest(const QString &key);
    void handleApiFailure(QNetworkReply *reply,
                          const QByteArray &payload,
                          const QString &context,
                          bool dropRemoteHistory = false);
    void applyDeviceFromJson(const QJsonObject &object);
    void applyAlarmCacheCounts();
    void updateSystemHealthFromJson(const QJsonObject &object);
    void updateDebugSnapshot() const;
    void restartRealtimeSocket();
    void handleRealtimeTextMessage(const QString &message);

    ApiClient *m_apiClient = nullptr;
    SQLiteCache *m_cache = nullptr;
    AuthSession *m_authSession = nullptr;
    DeviceStateStore *m_stateStore = nullptr;
    CommandTracker *m_commandTracker = nullptr;
    DeviceRepository *m_deviceRepository = nullptr;
    HistoryRepository *m_historyRepository = nullptr;
    AlarmRepository *m_alarmRepository = nullptr;
    RealtimeGateway *m_logGateway = nullptr;
    QTimer m_pagePollTimer;
    QTimer m_commandPollTimer;
    QSet<QString> m_inFlight;
    bool m_androidMode = false;
    QString m_currentPage = QStringLiteral("overview");
    QString m_currentDeviceId;
    QString m_connectionState = QStringLiteral("API idle");
    QString m_lastError;
    QString m_systemHealthSummary = QStringLiteral("Health not loaded");
    QString m_systemHealthStatus = QStringLiteral("unknown");
    bool m_healthApiOk = false;
    bool m_healthMysqlOk = false;
    bool m_healthMqttConnected = false;
    double m_healthDiskFreePercent = 0.0;
    QString m_healthUptimeText = QStringLiteral("--");
    QString m_healthDeviceSummary = QStringLiteral("--");
    QString m_healthLastSeenAt = QStringLiteral("--");
    QString m_healthLastWeatherAt = QStringLiteral("--");
    QString m_healthDiskSummary = QStringLiteral("--");
    QString m_healthMqttSummary = QStringLiteral("--");
    QString m_healthMysqlSummary = QStringLiteral("--");
    bool m_connected = false;
    QString m_pendingRemoteMsgId;
    QString m_pendingRemoteCommand;
    qint64 m_pendingStartedMs = 0;
#if defined(RELAY_CLIENT_HAS_WEBSOCKETS)
    QWebSocket m_socket;
#endif
};
