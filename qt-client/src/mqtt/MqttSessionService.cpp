#include "MqttSessionService.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

namespace
{
AckMessage parseAck(const QJsonObject &json)
{
    AckMessage ack;

    ack.msgId = json.value(QStringLiteral("msg_id")).toString();
    ack.deviceId = json.value(QStringLiteral("device_id")).toString();
    ack.cmd = json.value(QStringLiteral("cmd")).toString();
    ack.result = json.value(QStringLiteral("result")).toString();
    ack.detail = json.value(QStringLiteral("detail")).toString();
    ack.relay1On = json.value(QStringLiteral("relay1")).toInt() != 0;
    ack.relay2On = json.value(QStringLiteral("relay2")).toInt() != 0;
    ack.timestamp = json.value(QStringLiteral("timestamp")).toString();
    return ack;
}

OnlineMessage parseOnline(const QJsonObject &json)
{
    OnlineMessage message;

    message.deviceId = json.value(QStringLiteral("device_id")).toString();
    message.online = json.value(QStringLiteral("online")).toInt() != 0;
    message.timestamp = json.value(QStringLiteral("timestamp")).toString();
    return message;
}
}

MqttSessionService::MqttSessionService(QObject *parent)
    : QObject(parent)
    , m_stateStore(this)
    , m_commandTracker(this)
{
    m_reconnectTimer.setSingleShot(true);
    m_reconnectTimer.setInterval(2000);

    m_client.setProtocolVersion(QMqttClient::MQTT_3_1_1);
    m_client.setKeepAlive(60);

    connect(&m_client, &QMqttClient::stateChanged, this, &MqttSessionService::handleStateChanged);
    connect(&m_client, &QMqttClient::messageReceived, this, &MqttSessionService::handleMessageReceived);
    connect(&m_client, &QMqttClient::errorChanged, this, &MqttSessionService::handleErrorChanged);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &MqttSessionService::attemptReconnect);
    connect(&m_commandTracker, &CommandTracker::commandTimedOut, this, &MqttSessionService::handleCommandTimeout);

    loadSettings();
    m_stateStore.setCurrentDeviceId(m_deviceId);
    appendLog(QStringLiteral("Client ready for %1").arg(m_deviceId));
}

QString MqttSessionService::brokerHost() const
{
    return m_brokerHost;
}

int MqttSessionService::brokerPort() const
{
    return m_brokerPort;
}

QString MqttSessionService::username() const
{
    return m_username;
}

QString MqttSessionService::password() const
{
    return m_password;
}

QString MqttSessionService::deviceId() const
{
    return m_deviceId;
}

bool MqttSessionService::useTls() const
{
    return m_useTls;
}

QString MqttSessionService::connectionState() const
{
    return stateToText(m_client.state());
}

bool MqttSessionService::connected() const
{
    return m_client.state() == QMqttClient::Connected;
}

DeviceStateStore *MqttSessionService::stateStore()
{
    return &m_stateStore;
}

CommandTracker *MqttSessionService::commandTracker()
{
    return &m_commandTracker;
}

QAbstractItemModel *MqttSessionService::logModel()
{
    return &m_logModel;
}

void MqttSessionService::setBrokerHost(const QString &host)
{
    if (m_brokerHost == host)
    {
        return;
    }

    m_brokerHost = host;
    saveSettings();
    emit brokerSettingsChanged();
}

void MqttSessionService::setBrokerPort(int port)
{
    if (m_brokerPort == port)
    {
        return;
    }

    m_brokerPort = port;
    saveSettings();
    emit brokerSettingsChanged();
}

void MqttSessionService::setUsername(const QString &username)
{
    if (m_username == username)
    {
        return;
    }

    m_username = username;
    saveSettings();
    emit brokerSettingsChanged();
}

void MqttSessionService::setPassword(const QString &password)
{
    if (m_password == password)
    {
        return;
    }

    m_password = password;
    saveSettings();
    emit brokerSettingsChanged();
}

void MqttSessionService::setDeviceId(const QString &deviceId)
{
    if (m_deviceId == deviceId || deviceId.trimmed().isEmpty())
    {
        return;
    }

    m_deviceId = deviceId.trimmed();
    m_stateStore.setCurrentDeviceId(m_deviceId);
    saveSettings();
    emit brokerSettingsChanged();
}

void MqttSessionService::setUseTls(bool useTls)
{
    if (m_useTls == useTls)
    {
        return;
    }

    m_useTls = useTls;
    saveSettings();
    emit brokerSettingsChanged();
}

void MqttSessionService::connectBroker()
{
    if (m_client.state() == QMqttClient::Connected || m_client.state() == QMqttClient::Connecting)
    {
        return;
    }

    m_manualDisconnect = false;
    m_reconnectTimer.stop();
    m_client.setHostname(m_brokerHost);
    m_client.setPort(static_cast<quint16>(m_brokerPort));
    m_client.setUsername(m_username);
    m_client.setPassword(m_password);
    m_client.setClientId(clientId());
    appendLog(QStringLiteral("Connecting %1:%2 TLS=%3")
                  .arg(m_brokerHost)
                  .arg(m_brokerPort)
                  .arg(m_useTls ? QStringLiteral("on") : QStringLiteral("off")));

    if (m_useTls)
    {
        m_client.connectToHostEncrypted();
    }
    else
    {
        m_client.connectToHost();
    }
}

void MqttSessionService::disconnectBroker()
{
    m_manualDisconnect = true;
    m_reconnectTimer.stop();
    appendLog(QStringLiteral("Disconnect requested"));
    m_client.disconnectFromHost();
}

void MqttSessionService::setRelay1(bool enabled)
{
    publishCommand(QStringLiteral("set_r1"), enabled ? 1 : 0, true);
}

void MqttSessionService::setRelay2(bool enabled)
{
    publishCommand(QStringLiteral("set_r2"), enabled ? 1 : 0, true);
}

void MqttSessionService::setAll(bool enabled)
{
    publishCommand(QStringLiteral("set_all"), enabled ? 1 : 0, true);
}

void MqttSessionService::queryStatus()
{
    publishCommand(QStringLiteral("query_status"), 0, false);
}

void MqttSessionService::retryLastCommand()
{
    if (!m_commandTracker.canRetry())
    {
        m_stateStore.setLastError(QStringLiteral("No command to retry"));
        return;
    }

    publishCommand(m_commandTracker.lastCommand(),
                   m_commandTracker.lastValue(),
                   m_commandTracker.lastHasValue());
}

void MqttSessionService::handleStateChanged(QMqttClient::ClientState state)
{
    emit connectionStateChanged();
    appendLog(QStringLiteral("State -> %1").arg(stateToText(state)));

    if (state == QMqttClient::Connected)
    {
        subscribeTopics();
        QTimer::singleShot(250, this, &MqttSessionService::queryStatus);
        return;
    }

    if (state == QMqttClient::Disconnected)
    {
        if (m_commandTracker.pending())
        {
            m_commandTracker.failPending(QStringLiteral("Broker disconnected"));
        }

        if (!m_manualDisconnect && !m_reconnectTimer.isActive())
        {
            appendLog(QStringLiteral("Reconnect scheduled in 2s"));
            m_reconnectTimer.start();
        }
    }
}

void MqttSessionService::handleMessageReceived(const QByteArray &payload, const QMqttTopicName &topic)
{
    QJsonParseError parseError;
    const QString topicName = topic.name();
    const QString payloadText = QString::fromUtf8(payload);
    const QJsonDocument json = QJsonDocument::fromJson(payload, &parseError);

    appendLog(QStringLiteral("RX %1 %2").arg(topicName, payloadText));
    if (parseError.error != QJsonParseError::NoError || !json.isObject())
    {
        appendLog(QStringLiteral("JSON parse failed on %1").arg(topicName));
        return;
    }

    if (topicName == statusTopic())
    {
        m_stateStore.updateStatus(json.object());
        return;
    }

    if (topicName == ackTopic())
    {
        const AckMessage ack = parseAck(json.object());
        m_stateStore.updateAck(ack);
        if (m_commandTracker.matchesPending(ack.msgId))
        {
            m_commandTracker.resolveAck(ack.msgId);
        }
        if (ack.result.compare(QStringLiteral("success"), Qt::CaseInsensitive) != 0)
        {
            m_stateStore.setLastError(QStringLiteral("%1: %2").arg(ack.cmd, ack.detail));
        }
        return;
    }

    if (topicName == onlineTopic())
    {
        m_stateStore.updateOnline(parseOnline(json.object()));
        return;
    }

    if (topicName == eventTopic())
    {
        const QString eventName = json.object().value(QStringLiteral("event")).toString();
        const QString detail = json.object().value(QStringLiteral("detail")).toString();
        appendLog(QStringLiteral("EVENT %1 %2").arg(eventName, detail));
    }
}

void MqttSessionService::handleErrorChanged(QMqttClient::ClientError error)
{
    if (error == QMqttClient::NoError)
    {
        return;
    }

    appendLog(QStringLiteral("MQTT error %1").arg(static_cast<int>(error)));
}

void MqttSessionService::attemptReconnect()
{
    if (m_manualDisconnect || m_client.state() != QMqttClient::Disconnected)
    {
        return;
    }

    appendLog(QStringLiteral("Reconnect attempt"));
    connectBroker();
}

void MqttSessionService::handleCommandTimeout(const QString &msgId, const QString &command)
{
    m_stateStore.setLastError(QStringLiteral("%1 timeout (%2)").arg(command, msgId));
    appendLog(QStringLiteral("ACK timeout msg_id=%1 cmd=%2").arg(msgId, command));
}

QString MqttSessionService::commandTopic() const
{
    return QStringLiteral("device/%1/down/cmd").arg(m_deviceId);
}

QString MqttSessionService::statusTopic() const
{
    return QStringLiteral("device/%1/up/status").arg(m_deviceId);
}

QString MqttSessionService::ackTopic() const
{
    return QStringLiteral("device/%1/up/ack").arg(m_deviceId);
}

QString MqttSessionService::onlineTopic() const
{
    return QStringLiteral("device/%1/up/online").arg(m_deviceId);
}

QString MqttSessionService::eventTopic() const
{
    return QStringLiteral("device/%1/up/event").arg(m_deviceId);
}

QString MqttSessionService::buildMsgId() const
{
    return QStringLiteral("%1-%2-%3")
        .arg(m_deviceId)
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(m_msgSequence + 1U);
}

QString MqttSessionService::clientId() const
{
    return QStringLiteral("qt_%1_%2")
        .arg(m_deviceId)
        .arg(QCoreApplication::applicationPid());
}

QString MqttSessionService::stateToText(QMqttClient::ClientState state) const
{
    switch (state)
    {
    case QMqttClient::Disconnected:
        return QStringLiteral("Disconnected");
    case QMqttClient::Connecting:
        return QStringLiteral("Connecting");
    case QMqttClient::Connected:
        return QStringLiteral("Connected");
    }

    return QStringLiteral("Unknown");
}

void MqttSessionService::loadSettings()
{
    QSettings settings;

    m_brokerHost = settings.value(QStringLiteral("broker/host"),
                                  QStringLiteral("rc11adc1.ala.cn-hangzhou.emqxsl.cn")).toString();
    m_brokerPort = settings.value(QStringLiteral("broker/port"), 8883).toInt();
    m_username = settings.value(QStringLiteral("broker/username"), QStringLiteral("h743")).toString();
    m_password = settings.value(QStringLiteral("broker/password"), QStringLiteral("123456")).toString();
    m_deviceId = settings.value(QStringLiteral("broker/deviceId"), QStringLiteral("relay_h743_001")).toString();
    m_useTls = settings.value(QStringLiteral("broker/useTls"), true).toBool();
}

void MqttSessionService::saveSettings() const
{
    QSettings settings;

    settings.setValue(QStringLiteral("broker/host"), m_brokerHost);
    settings.setValue(QStringLiteral("broker/port"), m_brokerPort);
    settings.setValue(QStringLiteral("broker/username"), m_username);
    settings.setValue(QStringLiteral("broker/password"), m_password);
    settings.setValue(QStringLiteral("broker/deviceId"), m_deviceId);
    settings.setValue(QStringLiteral("broker/useTls"), m_useTls);
}

void MqttSessionService::appendLog(const QString &line)
{
    QStringList entries = m_logModel.stringList();

    entries.append(QStringLiteral("[%1] %2")
                       .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")))
                       .arg(line));
    while (entries.size() > 200)
    {
        entries.removeFirst();
    }
    m_logModel.setStringList(entries);
}

void MqttSessionService::subscribeTopics()
{
    auto subscribeOne = [this](const QString &topic) {
        if (m_client.subscribe(QMqttTopicFilter(topic), 1U) == nullptr)
        {
            appendLog(QStringLiteral("Subscribe failed: %1").arg(topic));
        }
        else
        {
            appendLog(QStringLiteral("Subscribed: %1").arg(topic));
        }
    };

    subscribeOne(statusTopic());
    subscribeOne(ackTopic());
    subscribeOne(onlineTopic());
    subscribeOne(eventTopic());
}

bool MqttSessionService::publishCommand(const QString &command, int value, bool hasValue)
{
    QJsonObject payload;
    QByteArray encoded;
    const QString msgId = buildMsgId();
    const QString topic = commandTopic();

    if (!connected())
    {
        m_stateStore.setLastError(QStringLiteral("Broker not connected"));
        return false;
    }

    if (m_commandTracker.pending())
    {
        m_stateStore.setLastError(QStringLiteral("Wait for current ACK first"));
        return false;
    }

    payload.insert(QStringLiteral("msg_id"), msgId);
    payload.insert(QStringLiteral("device_id"), m_deviceId);
    payload.insert(QStringLiteral("cmd"), command);
    if (hasValue)
    {
        payload.insert(QStringLiteral("value"), value);
    }
    payload.insert(QStringLiteral("operator"), QStringLiteral("qt-desktop"));
    payload.insert(QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    encoded = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    m_msgSequence++;
    if (m_client.publish(QMqttTopicName(topic), encoded, 1U, false) < 0)
    {
        m_stateStore.setLastError(QStringLiteral("Publish failed"));
        appendLog(QStringLiteral("TX failed %1").arg(topic));
        return false;
    }

    m_commandTracker.startPending(msgId, command, value, hasValue);
    m_stateStore.clearLastError();
    appendLog(QStringLiteral("TX %1 %2").arg(topic, QString::fromUtf8(encoded)));
    return true;
}
