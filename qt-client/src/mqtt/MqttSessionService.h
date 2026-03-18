#pragma once

#include <QAbstractItemModel>
#include <QObject>
#include <QStringListModel>
#include <QTimer>
#include <QtMqtt/QMqttClient>

#include "domain/CommandTracker.h"
#include "domain/DeviceStateStore.h"

class MqttSessionService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString brokerHost READ brokerHost WRITE setBrokerHost NOTIFY brokerSettingsChanged)
    Q_PROPERTY(int brokerPort READ brokerPort WRITE setBrokerPort NOTIFY brokerSettingsChanged)
    Q_PROPERTY(QString username READ username WRITE setUsername NOTIFY brokerSettingsChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY brokerSettingsChanged)
    Q_PROPERTY(QString deviceId READ deviceId WRITE setDeviceId NOTIFY brokerSettingsChanged)
    Q_PROPERTY(bool useTls READ useTls WRITE setUseTls NOTIFY brokerSettingsChanged)
    Q_PROPERTY(QString connectionState READ connectionState NOTIFY connectionStateChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectionStateChanged)
    Q_PROPERTY(DeviceStateStore *stateStore READ stateStore CONSTANT)
    Q_PROPERTY(CommandTracker *commandTracker READ commandTracker CONSTANT)
    Q_PROPERTY(QAbstractItemModel *logModel READ logModel CONSTANT)

public:
    explicit MqttSessionService(QObject *parent = nullptr);

    QString brokerHost() const;
    int brokerPort() const;
    QString username() const;
    QString password() const;
    QString deviceId() const;
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
    void setDeviceId(const QString &deviceId);
    void setUseTls(bool useTls);

    Q_INVOKABLE void connectBroker();
    Q_INVOKABLE void disconnectBroker();
    Q_INVOKABLE void setRelay1(bool enabled);
    Q_INVOKABLE void setRelay2(bool enabled);
    Q_INVOKABLE void setAll(bool enabled);
    Q_INVOKABLE void queryStatus();
    Q_INVOKABLE void retryLastCommand();

signals:
    void brokerSettingsChanged();
    void connectionStateChanged();

private slots:
    void handleStateChanged(QMqttClient::ClientState state);
    void handleMessageReceived(const QByteArray &payload, const QMqttTopicName &topic);
    void handleErrorChanged(QMqttClient::ClientError error);
    void attemptReconnect();
    void handleCommandTimeout(const QString &msgId, const QString &command);

private:
    QString commandTopic() const;
    QString statusTopic() const;
    QString ackTopic() const;
    QString onlineTopic() const;
    QString eventTopic() const;
    QString buildMsgId() const;
    QString clientId() const;
    QString stateToText(QMqttClient::ClientState state) const;
    void loadSettings();
    void saveSettings() const;
    void appendLog(const QString &line);
    void subscribeTopics();
    bool publishCommand(const QString &command, int value, bool hasValue);

    QMqttClient m_client;
    DeviceStateStore m_stateStore;
    CommandTracker m_commandTracker;
    QStringListModel m_logModel;
    QTimer m_reconnectTimer;
    QString m_brokerHost;
    int m_brokerPort = 8883;
    QString m_username;
    QString m_password;
    QString m_deviceId;
    bool m_useTls = true;
    bool m_manualDisconnect = false;
    quint32 m_msgSequence = 0U;
};
