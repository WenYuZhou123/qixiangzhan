#pragma once

#include <QAbstractItemModel>
#include <QHash>
#include <QObject>
#include <QJsonObject>
#include <QStringListModel>
#include <QTimer>
#include <QtMqtt/QMqttClient>

#include "domain/CommandTracker.h"
#include "domain/DeviceStateStore.h"

class AlarmRepository;
class DeviceRepository;
class HistoryRepository;
class SQLiteCache;

class RealtimeGateway : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString brokerHost READ brokerHost WRITE setBrokerHost NOTIFY brokerSettingsChanged)
    Q_PROPERTY(int brokerPort READ brokerPort WRITE setBrokerPort NOTIFY brokerSettingsChanged)
    Q_PROPERTY(QString username READ username WRITE setUsername NOTIFY brokerSettingsChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY brokerSettingsChanged)
    Q_PROPERTY(QString currentDeviceId READ currentDeviceId WRITE setCurrentDeviceId NOTIFY currentDeviceIdChanged)
    Q_PROPERTY(bool useTls READ useTls WRITE setUseTls NOTIFY brokerSettingsChanged)
    Q_PROPERTY(QString connectionState READ connectionState NOTIFY connectionStateChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectionStateChanged)
    Q_PROPERTY(DeviceStateStore *stateStore READ stateStore CONSTANT)
    Q_PROPERTY(CommandTracker *commandTracker READ commandTracker CONSTANT)
    Q_PROPERTY(QAbstractItemModel *logModel READ logModel CONSTANT)

public:
    explicit RealtimeGateway(SQLiteCache *cache,
                             DeviceStateStore *stateStore,
                             CommandTracker *commandTracker,
                             DeviceRepository *deviceRepository,
                             HistoryRepository *historyRepository,
                             AlarmRepository *alarmRepository,
                             QObject *parent = nullptr);

    QString brokerHost() const;
    int brokerPort() const;
    QString username() const;
    QString password() const;
    QString currentDeviceId() const;
    bool useTls() const;
    QString connectionState() const;
    bool connected() const;
    DeviceStateStore *stateStore();
    CommandTracker *commandTracker();
    QAbstractItemModel *logModel();

    void setBrokerHost(const QString &host);
    void setBrokerPort(int port);
    void setUsername(const QString &username);
    void setPassword(const QString &password);
    void setCurrentDeviceId(const QString &deviceId);
    void setUseTls(bool useTls);
    void setIgnoreCommandTimeouts(bool ignore);

    Q_INVOKABLE void connectBroker();
    Q_INVOKABLE void disconnectBroker();
    Q_INVOKABLE void setRelay1(bool enabled);
    Q_INVOKABLE void setRelay2(bool enabled);
    Q_INVOKABLE void setAll(bool enabled);
    Q_INVOKABLE void queryStatus();
    Q_INVOKABLE void padOpen();
    Q_INVOKABLE void padClose();
    Q_INVOKABLE void padStop();
    Q_INVOKABLE void queryPadStatus();
    Q_INVOKABLE void retryLastCommand();
    Q_INVOKABLE void appendExternalLog(const QString &line);

signals:
    void brokerSettingsChanged();
    void connectionStateChanged();
    void currentDeviceIdChanged();

private slots:
    void handleStateChanged(QMqttClient::ClientState state);
    void handleMessageReceived(const QByteArray &payload, const QMqttTopicName &topic);
    void handleErrorChanged(QMqttClient::ClientError error);
    void attemptReconnect();
    void handleCommandTimeout(const QString &msgId, const QString &command);
    void checkDeviceTimeouts();

private:
    QString currentCommandTopic() const;
    QString commandTopic(const QString &deviceId) const;
    QString buildMsgId() const;
    QString clientId() const;
    QString stateToText(QMqttClient::ClientState state) const;
    void loadSettings();
    void saveSettings() const;
    void appendLog(const QString &line);
    void subscribeTopics();
    bool publishCommand(const QString &command, int value, bool hasValue);
    bool publishLegacyCommand(const QString &deviceId, const QString &command, int value, bool hasValue);
    void persistStateForDevice(const QString &deviceId);
    void syncAlarmCounts();
    void handleStateRules(const DeviceState &state);
    QString extractDeviceId(const QString &topicName) const;
    void rememberLegacyTopic(const QString &deviceId, const QString &topicName);
    void rememberPendingCommandState(const QString &deviceId,
                                    const QString &msgId,
                                    const QString &command,
                                    int value,
                                    bool hasValue);
    void clearPendingCommandState();
    bool tryResolvePendingCommandFromStatus(const QJsonObject &statusObject);

    QMqttClient m_client;
    QStringListModel m_logModel;
    QTimer m_reconnectTimer;
    QTimer m_deviceMonitorTimer;
    SQLiteCache *m_cache = nullptr;
    DeviceStateStore *m_stateStore = nullptr;
    CommandTracker *m_commandTracker = nullptr;
    DeviceRepository *m_deviceRepository = nullptr;
    HistoryRepository *m_historyRepository = nullptr;
    AlarmRepository *m_alarmRepository = nullptr;
    QString m_brokerHost;
    int m_brokerPort = 8883;
    QString m_username;
    QString m_password;
    bool m_useTls = true;
    bool m_manualDisconnect = false;
    quint32 m_msgSequence = 0U;
    QString m_lastCommandDeviceId;
    QHash<QString, QString> m_legacyCommandTopics;
    int m_lowRssiThreshold = 10;
    qint64 m_offlineThresholdMs = 60000;
    bool m_ignoreCommandTimeouts = false;
    QString m_pendingCommandDeviceId;
    QString m_pendingCommandMsgId;
    QString m_pendingCommandName;
    int m_pendingCommandValue = 0;
    bool m_pendingCommandHasValue = false;
    qint64 m_pendingCommandStartedMs = 0;
    qint64 m_pendingBaselineTick = 0;
};
