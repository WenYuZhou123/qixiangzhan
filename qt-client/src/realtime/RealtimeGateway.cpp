#include "RealtimeGateway.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSslConfiguration>

#include "data/AlarmRepository.h"
#include "data/DeviceRepository.h"
#include "data/HistoryRepository.h"
#include "data/SQLiteCache.h"

namespace
{
constexpr int kDirectCommandTimeoutMs = 18000;
constexpr qint64 kStatusTimestampSlackMs = 1500;

bool isLegacyStatusTopic(const QString &topicName)
{
    return topicName.startsWith(QStringLiteral("device/")) &&
           topicName.endsWith(QStringLiteral("/status")) &&
           !topicName.endsWith(QStringLiteral("/up/status"));
}

QString readStringLike(const QJsonValue &value, const QString &fallback = QString())
{
    if (value.isString())
    {
        return value.toString();
    }

    return fallback;
}

bool readBoolLike(const QJsonValue &value, bool fallback = false)
{
    if (value.isBool())
    {
        return value.toBool();
    }
    if (value.isDouble())
    {
        return value.toInt() != 0;
    }
    if (value.isString())
    {
        const QString lowered = value.toString().trimmed().toLower();
        if (lowered == QStringLiteral("1") ||
            lowered == QStringLiteral("true") ||
            lowered == QStringLiteral("yes") ||
            lowered == QStringLiteral("on") ||
            lowered == QStringLiteral("open"))
        {
            return true;
        }
        if (lowered == QStringLiteral("0") ||
            lowered == QStringLiteral("false") ||
            lowered == QStringLiteral("no") ||
            lowered == QStringLiteral("off") ||
            lowered == QStringLiteral("closed"))
        {
            return false;
        }
    }

    return fallback;
}

qint64 readLongLongLike(const QJsonValue &value, qint64 fallback = 0)
{
    if (value.isDouble())
    {
        return static_cast<qint64>(value.toDouble());
    }
    if (value.isString())
    {
        bool ok = false;
        const qint64 parsed = value.toString().toLongLong(&ok);
        return ok ? parsed : fallback;
    }

    return fallback;
}

QDateTime parseIsoTimestamp(const QString &value)
{
    QDateTime parsed = QDateTime::fromString(value, Qt::ISODateWithMs);
    if (!parsed.isValid())
    {
        parsed = QDateTime::fromString(value, Qt::ISODate);
    }
    if (parsed.isValid())
    {
        parsed = parsed.toUTC();
    }
    return parsed;
}

QString readPadState(const QJsonObject &status, const QString &key)
{
    if (status.contains(QStringLiteral("pad")) && status.value(QStringLiteral("pad")).isObject())
    {
        return readStringLike(status.value(QStringLiteral("pad")).toObject().value(key));
    }

    return readStringLike(status.value(key));
}

bool statusMatchesCommandOutcome(const QJsonObject &status,
                                 const QString &command,
                                 int value,
                                 bool hasValue)
{
    const bool relay1On = readBoolLike(status.value(QStringLiteral("relay1")));
    const bool relay2On = readBoolLike(status.value(QStringLiteral("relay2")));
    const QString leftState = readPadState(status, QStringLiteral("left_state")).toLower();
    const QString rightState = readPadState(status, QStringLiteral("right_state")).toLower();

    if (command == QStringLiteral("query_status") || command == QStringLiteral("query_pad_status"))
    {
        return true;
    }
    if (command == QStringLiteral("set_r1") && hasValue)
    {
        return relay1On == (value != 0);
    }
    if (command == QStringLiteral("set_r2") && hasValue)
    {
        return relay2On == (value != 0);
    }
    if (command == QStringLiteral("set_all") && hasValue)
    {
        const bool enabled = value != 0;
        return relay1On == enabled && relay2On == enabled;
    }
    if (command == QStringLiteral("pad_open"))
    {
        return (relay1On && relay2On) || (leftState == QStringLiteral("open") && rightState == QStringLiteral("open"));
    }
    if (command == QStringLiteral("pad_close"))
    {
        return ((!relay1On && !relay2On) ||
                (leftState == QStringLiteral("closed") && rightState == QStringLiteral("closed")));
    }
    if (command == QStringLiteral("pad_stop"))
    {
        return true;
    }

    return false;
}

bool statusIsFreshEnough(const QJsonObject &status, qint64 baselineTick, qint64 pendingStartedMs)
{
    const qint64 statusTick = readLongLongLike(status.value(QStringLiteral("tick")), 0);
    if (statusTick > 0 && baselineTick > 0)
    {
        return statusTick > baselineTick;
    }
    if (statusTick > 0)
    {
        return true;
    }

    const QDateTime timestamp = parseIsoTimestamp(readStringLike(status.value(QStringLiteral("timestamp"))));
    if (timestamp.isValid() && pendingStartedMs > 0)
    {
        return timestamp.toMSecsSinceEpoch() >= (pendingStartedMs - kStatusTimestampSlackMs);
    }

    return false;
}

QJsonDocument parseJsonPayloadCompat(const QByteArray &payload, QJsonParseError *outError, bool *usedFallback = nullptr)
{
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error == QJsonParseError::NoError && document.isObject())
    {
        if (usedFallback != nullptr)
        {
            *usedFallback = false;
        }
        if (outError != nullptr)
        {
            *outError = parseError;
        }
        return document;
    }

    QByteArray normalized = payload.trimmed();
    if (normalized.size() >= 2 && normalized.startsWith('"') && normalized.endsWith('"'))
    {
        normalized = normalized.mid(1, normalized.size() - 2);
    }
    normalized.replace("\\\"", "\"");
    normalized.replace("\\\\", "\\");

    QJsonParseError fallbackError;
    document = QJsonDocument::fromJson(normalized, &fallbackError);
    if (usedFallback != nullptr)
    {
        *usedFallback = (fallbackError.error == QJsonParseError::NoError && document.isObject());
    }
    if (outError != nullptr)
    {
        *outError = fallbackError;
    }
    return document;
}

QString legacyCommandTopicForStatusTopic(const QString &topicName)
{
    if (!isLegacyStatusTopic(topicName))
    {
        return QString();
    }

    const QString prefix = topicName.left(topicName.size() - QStringLiteral("/status").size());
    return prefix + QStringLiteral("/cmd");
}

QJsonObject normalizeStatusObject(const QJsonObject &json)
{
    QJsonObject object = json;
    const QString deviceId = object.value(QStringLiteral("device_id")).toString(
        object.value(QStringLiteral("client_id")).toString());

    if (!deviceId.isEmpty())
    {
        object.insert(QStringLiteral("device_id"), deviceId);
    }

    if (!object.contains(QStringLiteral("online")))
    {
        object.insert(QStringLiteral("online"), 1);
    }

    if (object.contains(QStringLiteral("net")) && object.value(QStringLiteral("net")).isObject())
    {
        const QJsonObject net = object.value(QStringLiteral("net")).toObject();
        if (!object.contains(QStringLiteral("rssi")) && net.contains(QStringLiteral("rssi")))
        {
            object.insert(QStringLiteral("rssi"), net.value(QStringLiteral("rssi")));
        }
        if (!object.contains(QStringLiteral("operator")) && net.contains(QStringLiteral("operator")))
        {
            object.insert(QStringLiteral("operator"), net.value(QStringLiteral("operator")));
        }
        if (!object.contains(QStringLiteral("ip")) && net.contains(QStringLiteral("ip")))
        {
            object.insert(QStringLiteral("ip"), net.value(QStringLiteral("ip")));
        }
    }

    if (!object.contains(QStringLiteral("timestamp")))
    {
        object.insert(QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    }

    if (!object.contains(QStringLiteral("protocol_profile")))
    {
        object.insert(QStringLiteral("protocol_profile"),
                      object.contains(QStringLiteral("pad"))
                          ? QStringLiteral("airport_pad_v1")
                          : QStringLiteral("relay_v1"));
    }

    return object;
}

QString legacyCommandToken(const QString &command, int value, bool hasValue)
{
    if (command == QStringLiteral("set_r1") && hasValue)
    {
        return value != 0 ? QStringLiteral("R1_ON") : QStringLiteral("R1_OFF");
    }
    if (command == QStringLiteral("set_r2") && hasValue)
    {
        return value != 0 ? QStringLiteral("R2_ON") : QStringLiteral("R2_OFF");
    }
    if (command == QStringLiteral("set_all") && hasValue)
    {
        return value != 0 ? QStringLiteral("ALL_ON") : QStringLiteral("ALL_OFF");
    }
    if (command == QStringLiteral("query_status"))
    {
        return QStringLiteral("STATUS");
    }

    return QString();
}

AckMessage parseAck(const QJsonObject &json, const QString &fallbackDeviceId)
{
    AckMessage ack;

    ack.msgId = json.value(QStringLiteral("msg_id")).toString();
    ack.deviceId = json.value(QStringLiteral("device_id")).toString(fallbackDeviceId);
    ack.cmd = json.value(QStringLiteral("cmd")).toString();
    ack.result = json.value(QStringLiteral("result")).toString();
    ack.detail = json.value(QStringLiteral("detail")).toString();
    ack.relay1On = json.value(QStringLiteral("relay1")).toInt() != 0;
    ack.relay2On = json.value(QStringLiteral("relay2")).toInt() != 0;
    ack.timestamp = json.value(QStringLiteral("timestamp")).toString();
    return ack;
}

OnlineMessage parseOnline(const QJsonObject &json, const QString &fallbackDeviceId)
{
    OnlineMessage message;

    message.deviceId = json.value(QStringLiteral("device_id")).toString(fallbackDeviceId);
    message.online = json.value(QStringLiteral("online")).toInt() != 0;
    message.timestamp = json.value(QStringLiteral("timestamp")).toString();
    return message;
}
}

RealtimeGateway::RealtimeGateway(SQLiteCache *cache,
                                 DeviceStateStore *stateStore,
                                 CommandTracker *commandTracker,
                                 DeviceRepository *deviceRepository,
                                 HistoryRepository *historyRepository,
                                 AlarmRepository *alarmRepository,
                                 QObject *parent)
    : QObject(parent)
    , m_cache(cache)
    , m_stateStore(stateStore)
    , m_commandTracker(commandTracker)
    , m_deviceRepository(deviceRepository)
    , m_historyRepository(historyRepository)
    , m_alarmRepository(alarmRepository)
{
    m_reconnectTimer.setSingleShot(true);
    m_reconnectTimer.setInterval(3000);
    m_deviceMonitorTimer.setInterval(5000);

    m_client.setProtocolVersion(QMqttClient::MQTT_3_1_1);
    m_client.setKeepAlive(60);

    connect(&m_client, &QMqttClient::stateChanged, this, &RealtimeGateway::handleStateChanged);
    connect(&m_client, &QMqttClient::messageReceived, this, &RealtimeGateway::handleMessageReceived);
    connect(&m_client, &QMqttClient::errorChanged, this, &RealtimeGateway::handleErrorChanged);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &RealtimeGateway::attemptReconnect);
    connect(&m_deviceMonitorTimer, &QTimer::timeout, this, &RealtimeGateway::checkDeviceTimeouts);
    connect(m_commandTracker, &CommandTracker::commandTimedOut, this, &RealtimeGateway::handleCommandTimeout);
    connect(m_stateStore, &DeviceStateStore::currentDeviceIdChanged, this, &RealtimeGateway::currentDeviceIdChanged);

    loadSettings();
    appendLog(QStringLiteral("实时链路已初始化"));
    m_deviceMonitorTimer.start();
}

QString RealtimeGateway::brokerHost() const
{
    return m_brokerHost;
}

int RealtimeGateway::brokerPort() const
{
    return m_brokerPort;
}

QString RealtimeGateway::username() const
{
    return m_username;
}

QString RealtimeGateway::password() const
{
    return m_password;
}

QString RealtimeGateway::currentDeviceId() const
{
    return m_stateStore != nullptr ? m_stateStore->currentDeviceId() : QString();
}

bool RealtimeGateway::useTls() const
{
    return m_useTls;
}

QString RealtimeGateway::connectionState() const
{
    return stateToText(m_client.state());
}

bool RealtimeGateway::connected() const
{
    return m_client.state() == QMqttClient::Connected;
}

DeviceStateStore *RealtimeGateway::stateStore()
{
    return m_stateStore;
}

CommandTracker *RealtimeGateway::commandTracker()
{
    return m_commandTracker;
}

QAbstractItemModel *RealtimeGateway::logModel()
{
    return &m_logModel;
}

void RealtimeGateway::setBrokerHost(const QString &host)
{
    if (m_brokerHost == host)
    {
        return;
    }

    m_brokerHost = host.trimmed();
    saveSettings();
    emit brokerSettingsChanged();
}

void RealtimeGateway::setBrokerPort(int port)
{
    if (m_brokerPort == port)
    {
        return;
    }

    m_brokerPort = port;
    saveSettings();
    emit brokerSettingsChanged();
}

void RealtimeGateway::setUsername(const QString &usernameValue)
{
    if (m_username == usernameValue)
    {
        return;
    }

    m_username = usernameValue;
    saveSettings();
    emit brokerSettingsChanged();
}

void RealtimeGateway::setPassword(const QString &passwordValue)
{
    if (m_password == passwordValue)
    {
        return;
    }

    m_password = passwordValue;
    saveSettings();
    emit brokerSettingsChanged();
}

void RealtimeGateway::setCurrentDeviceId(const QString &deviceId)
{
    if (m_stateStore == nullptr || m_stateStore->currentDeviceId() == deviceId || deviceId.trimmed().isEmpty())
    {
        return;
    }

    m_stateStore->setCurrentDeviceId(deviceId.trimmed());
    saveSettings();
    emit currentDeviceIdChanged();
}

void RealtimeGateway::setUseTls(bool useTlsValue)
{
    if (m_useTls == useTlsValue)
    {
        return;
    }

    m_useTls = useTlsValue;
    saveSettings();
    emit brokerSettingsChanged();
}

void RealtimeGateway::setIgnoreCommandTimeouts(bool ignore)
{
    m_ignoreCommandTimeouts = ignore;
}

void RealtimeGateway::connectBroker()
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
    appendLog(QStringLiteral("连接 MQTT %1:%2 TLS=%3")
                  .arg(m_brokerHost)
                  .arg(m_brokerPort)
                  .arg(m_useTls ? QStringLiteral("on") : QStringLiteral("off")));

    if (m_useTls)
    {
        m_client.connectToHostEncrypted(QSslConfiguration::defaultConfiguration());
    }
    else
    {
        m_client.connectToHost();
    }
}

void RealtimeGateway::disconnectBroker()
{
    m_manualDisconnect = true;
    m_reconnectTimer.stop();
    appendLog(QStringLiteral("请求断开 MQTT"));
    m_client.disconnectFromHost();
}

void RealtimeGateway::setRelay1(bool enabled)
{
    publishCommand(QStringLiteral("set_r1"), enabled ? 1 : 0, true);
}

void RealtimeGateway::setRelay2(bool enabled)
{
    publishCommand(QStringLiteral("set_r2"), enabled ? 1 : 0, true);
}

void RealtimeGateway::setAll(bool enabled)
{
    publishCommand(QStringLiteral("set_all"), enabled ? 1 : 0, true);
}

void RealtimeGateway::queryStatus()
{
    publishCommand(QStringLiteral("query_status"), 0, false);
}

void RealtimeGateway::padOpen()
{
    publishCommand(QStringLiteral("pad_open"), 0, false);
}

void RealtimeGateway::padClose()
{
    publishCommand(QStringLiteral("pad_close"), 0, false);
}

void RealtimeGateway::padStop()
{
    publishCommand(QStringLiteral("pad_stop"), 0, false);
}

void RealtimeGateway::queryPadStatus()
{
    publishCommand(QStringLiteral("query_pad_status"), 0, false);
}

void RealtimeGateway::retryLastCommand()
{
    if (!m_commandTracker->canRetry())
    {
        m_stateStore->setLastError(QStringLiteral("没有可重试命令"));
        return;
    }

    if (!m_lastCommandDeviceId.isEmpty())
    {
        setCurrentDeviceId(m_lastCommandDeviceId);
    }

    publishCommand(m_commandTracker->lastCommand(),
                   m_commandTracker->lastValue(),
                   m_commandTracker->lastHasValue());
}

void RealtimeGateway::appendExternalLog(const QString &line)
{
    appendLog(line);
}

void RealtimeGateway::handleStateChanged(QMqttClient::ClientState state)
{
    emit connectionStateChanged();
    appendLog(QStringLiteral("MQTT 状态 -> %1").arg(stateToText(state)));

    const QString alarmDeviceId = currentDeviceId();
    if (state == QMqttClient::Connected)
    {
        subscribeTopics();
        if (!alarmDeviceId.isEmpty())
        {
            m_alarmRepository->resolveAlarm(alarmDeviceId, QStringLiteral("client_mqtt_disconnect"));
            syncAlarmCounts();
        }
        QTimer::singleShot(500, this, &RealtimeGateway::queryStatus);
        return;
    }

    if (state == QMqttClient::Disconnected)
    {
        if (m_commandTracker->pending())
        {
            m_commandTracker->failPending(QStringLiteral("Broker disconnected"));
            clearPendingCommandState();
        }

        if (!alarmDeviceId.isEmpty())
        {
            m_alarmRepository->raiseAlarm(alarmDeviceId,
                                          QStringLiteral("client_mqtt_disconnect"),
                                          QStringLiteral("warning"),
                                          QStringLiteral("桌面端与 MQTT 连接已断开"),
                                          QStringLiteral("qt-client"));
            syncAlarmCounts();
        }

        if (!m_manualDisconnect && !m_reconnectTimer.isActive())
        {
            appendLog(QStringLiteral("3 秒后自动重连"));
            m_reconnectTimer.start();
        }
    }
}

void RealtimeGateway::handleMessageReceived(const QByteArray &payload, const QMqttTopicName &topic)
{
    QJsonParseError parseError;
    const QString topicName = topic.name();
    const QString payloadText = QString::fromUtf8(payload);
    bool usedFallback = false;
    const QJsonDocument json = parseJsonPayloadCompat(payload, &parseError, &usedFallback);
    const QString topicDeviceId = extractDeviceId(topicName);

    appendLog(QStringLiteral("RX %1 %2").arg(topicName, payloadText));

    MessageRecord inboundRecord;
    inboundRecord.deviceId = topicDeviceId;
    inboundRecord.direction = QStringLiteral("in");
    inboundRecord.channel = QStringLiteral("mqtt");
    inboundRecord.topic = topicName;
    inboundRecord.payload = payloadText;
    inboundRecord.level = QStringLiteral("info");
    inboundRecord.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    m_historyRepository->appendRecord(inboundRecord);

    if (parseError.error != QJsonParseError::NoError || !json.isObject())
    {
        appendLog(QStringLiteral("JSON 解析失败: %1").arg(topicName));
        return;
    }

    if (usedFallback)
    {
        appendLog(QStringLiteral("Parsed escaped JSON payload: %1").arg(topicName));
    }

    if (topicName.endsWith(QStringLiteral("/up/status")) || isLegacyStatusTopic(topicName))
    {
        QJsonObject object = normalizeStatusObject(json.object());
        if (!object.contains(QStringLiteral("device_id")))
        {
            object.insert(QStringLiteral("device_id"), topicDeviceId);
        }
        if (isLegacyStatusTopic(topicName))
        {
            const QString legacyDeviceId = object.value(QStringLiteral("device_id")).toString();
            if (!legacyDeviceId.isEmpty())
            {
                rememberLegacyTopic(legacyDeviceId, topicName);
            }
        }
        m_stateStore->updateStatus(object);
        persistStateForDevice(object.value(QStringLiteral("device_id")).toString(topicDeviceId));
        tryResolvePendingCommandFromStatus(object);
        return;
    }

    if (topicName.endsWith(QStringLiteral("/up/ack")))
    {
        const AckMessage ack = parseAck(json.object(), topicDeviceId);
        m_stateStore->updateAck(ack);
        persistStateForDevice(ack.deviceId);
        m_cache->updateOutboxStatus(ack.msgId,
                                    ack.result.compare(QStringLiteral("success"), Qt::CaseInsensitive) == 0
                                        ? QStringLiteral("ack_success")
                                        : QStringLiteral("ack_error"),
                                    ack.detail);
        if (m_commandTracker->matchesPending(ack.msgId))
        {
            m_commandTracker->resolveAck(ack.msgId);
            clearPendingCommandState();
        }
        if (ack.result.compare(QStringLiteral("success"), Qt::CaseInsensitive) != 0)
        {
            m_stateStore->setLastError(QStringLiteral("%1: %2").arg(ack.cmd, ack.detail));
            m_alarmRepository->raiseAlarm(ack.deviceId,
                                          QStringLiteral("command_error"),
                                          QStringLiteral("critical"),
                                          QStringLiteral("%1 失败: %2").arg(ack.cmd, ack.detail),
                                          QStringLiteral("mqtt"));
        }
        else
        {
            m_alarmRepository->resolveAlarm(ack.deviceId, QStringLiteral("ack_timeout"));
            m_alarmRepository->resolveAlarm(ack.deviceId, QStringLiteral("command_error"));
        }
        syncAlarmCounts();
        return;
    }

    if (topicName.endsWith(QStringLiteral("/up/online")))
    {
        const OnlineMessage online = parseOnline(json.object(), topicDeviceId);
        m_stateStore->updateOnline(online);
        persistStateForDevice(online.deviceId);
        if (online.online)
        {
            m_alarmRepository->resolveAlarm(online.deviceId, QStringLiteral("device_offline"));
        }
        else
        {
            m_alarmRepository->raiseAlarm(online.deviceId,
                                          QStringLiteral("device_offline"),
                                          QStringLiteral("critical"),
                                          QStringLiteral("设备离线"),
                                          QStringLiteral("mqtt"));
        }
        syncAlarmCounts();
        return;
    }

    if (topicName.endsWith(QStringLiteral("/up/event")))
    {
        const QString eventName = json.object().value(QStringLiteral("event")).toString();
        const QString detail = json.object().value(QStringLiteral("detail")).toString();
        appendLog(QStringLiteral("EVENT %1 %2").arg(eventName, detail));
    }
}

void RealtimeGateway::handleErrorChanged(QMqttClient::ClientError error)
{
    if (error == QMqttClient::NoError)
    {
        return;
    }

    appendLog(QStringLiteral("MQTT 错误 %1").arg(static_cast<int>(error)));
}

void RealtimeGateway::attemptReconnect()
{
    if (m_manualDisconnect || m_client.state() != QMqttClient::Disconnected)
    {
        return;
    }

    appendLog(QStringLiteral("执行自动重连"));
    connectBroker();
}

void RealtimeGateway::handleCommandTimeout(const QString &msgId, const QString &command)
{
    if (m_ignoreCommandTimeouts)
    {
        appendLog(QStringLiteral("Ignore tracker timeout for API command %1").arg(msgId));
        return;
    }

    clearPendingCommandState();
    const QString deviceId = m_lastCommandDeviceId.isEmpty() ? currentDeviceId() : m_lastCommandDeviceId;
    m_stateStore->setLastError(QStringLiteral("%1 超时 (%2)").arg(command, msgId));
    m_cache->updateOutboxStatus(msgId, QStringLiteral("ack_timeout"), QStringLiteral("ACK timeout"));
    if (!deviceId.isEmpty())
    {
        m_alarmRepository->raiseAlarm(deviceId,
                                      QStringLiteral("ack_timeout"),
                                      QStringLiteral("critical"),
                                      QStringLiteral("%1 ACK 超时").arg(command),
                                      QStringLiteral("mqtt"));
        syncAlarmCounts();
    }
    appendLog(QStringLiteral("ACK 超时 msg_id=%1 cmd=%2").arg(msgId, command));
}

void RealtimeGateway::checkDeviceTimeouts()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const QVector<DeviceState> states = m_stateStore->allStates();

    for (const DeviceState &state : states)
    {
        if (state.deviceId.isEmpty() || state.lastSeenMs <= 0)
        {
            continue;
        }

        if ((now - state.lastSeenMs) > m_offlineThresholdMs)
        {
            DeviceState updated = state;
            updated.online = false;
            updated.stateText = QStringLiteral("offline timeout");
            m_stateStore->hydrateStates({updated});
            m_deviceRepository->upsertState(updated);
            m_cache->upsertDeviceState(updated);
            m_alarmRepository->raiseAlarm(updated.deviceId,
                                          QStringLiteral("device_offline"),
                                          QStringLiteral("critical"),
                                          QStringLiteral("设备超过 60 秒未上报"),
                                          QStringLiteral("qt-client"));
            syncAlarmCounts();
        }
    }
}

QString RealtimeGateway::currentCommandTopic() const
{
    return commandTopic(currentDeviceId());
}

QString RealtimeGateway::commandTopic(const QString &deviceId) const
{
    return QStringLiteral("device/%1/down/cmd").arg(deviceId);
}

QString RealtimeGateway::buildMsgId() const
{
    return QStringLiteral("%1-%2-%3")
        .arg(currentDeviceId())
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(m_msgSequence + 1U);
}

QString RealtimeGateway::clientId() const
{
    return QStringLiteral("qt_platform_%1_%2")
        .arg(QCoreApplication::applicationPid())
        .arg(QDateTime::currentMSecsSinceEpoch());
}

QString RealtimeGateway::stateToText(QMqttClient::ClientState state) const
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

void RealtimeGateway::loadSettings()
{
    const auto readSetting = [this](const QString &key, const QString &fallback) {
        return m_cache != nullptr ? m_cache->setting(key, fallback) : fallback;
    };

    m_brokerHost = readSetting(QStringLiteral("broker.host"),
                               QStringLiteral("rc11adc1.ala.cn-hangzhou.emqxsl.cn"));
    m_brokerPort = readSetting(QStringLiteral("broker.port"), QStringLiteral("8883")).toInt();
    m_username = readSetting(QStringLiteral("broker.username"), QStringLiteral("h743"));
    m_password = readSetting(QStringLiteral("broker.password"), QStringLiteral("123456"));
    m_useTls = readSetting(QStringLiteral("broker.useTls"), QStringLiteral("true")) == QStringLiteral("true");
    setCurrentDeviceId(readSetting(QStringLiteral("broker.currentDeviceId"), QStringLiteral("relay_h743_001")));
}

void RealtimeGateway::saveSettings() const
{
    if (m_cache == nullptr)
    {
        return;
    }

    m_cache->setSetting(QStringLiteral("broker.host"), m_brokerHost);
    m_cache->setSetting(QStringLiteral("broker.port"), QString::number(m_brokerPort));
    m_cache->setSetting(QStringLiteral("broker.username"), m_username);
    m_cache->setSetting(QStringLiteral("broker.password"), m_password);
    m_cache->setSetting(QStringLiteral("broker.useTls"), m_useTls ? QStringLiteral("true") : QStringLiteral("false"));
    m_cache->setSetting(QStringLiteral("broker.currentDeviceId"), currentDeviceId());
}

void RealtimeGateway::appendLog(const QString &line)
{
    QStringList entries = m_logModel.stringList();
    entries.append(QStringLiteral("[%1] %2")
                       .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")))
                       .arg(line));
    while (entries.size() > 400)
    {
        entries.removeFirst();
    }
    m_logModel.setStringList(entries);
}

void RealtimeGateway::subscribeTopics()
{
    const QStringList topics{
        QStringLiteral("device/+/up/status"),
        QStringLiteral("device/+/up/ack"),
        QStringLiteral("device/+/up/online"),
        QStringLiteral("device/+/up/event"),
        QStringLiteral("device/+/status")
    };

    for (const QString &topic : topics)
    {
        if (m_client.subscribe(QMqttTopicFilter(topic), 1U) == nullptr)
        {
            appendLog(QStringLiteral("订阅失败: %1").arg(topic));
        }
        else
        {
            appendLog(QStringLiteral("已订阅: %1").arg(topic));
        }
    }
}

bool RealtimeGateway::publishCommand(const QString &command, int value, bool hasValue)
{
    if (!connected())
    {
        m_stateStore->setLastError(QStringLiteral("Broker 未连接"));
        return false;
    }

    if (m_commandTracker->pending())
    {
        m_stateStore->setLastError(QStringLiteral("请等待上一个 ACK 完成"));
        return false;
    }

    const QString deviceId = currentDeviceId();
    if (deviceId.isEmpty())
    {
        m_stateStore->setLastError(QStringLiteral("请先选择设备"));
        return false;
    }

    if (m_legacyCommandTopics.contains(deviceId))
    {
        return publishLegacyCommand(deviceId, command, value, hasValue);
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("msg_id"), buildMsgId());
    payload.insert(QStringLiteral("device_id"), deviceId);
    payload.insert(QStringLiteral("cmd"), command);
    if (hasValue)
    {
        payload.insert(QStringLiteral("value"), value);
    }
    payload.insert(QStringLiteral("operator"), QStringLiteral("qt-desktop"));
    payload.insert(QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));

    const QByteArray encoded = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    const QString topic = currentCommandTopic();
    const QString msgId = payload.value(QStringLiteral("msg_id")).toString();
    m_msgSequence++;

    if (m_client.publish(QMqttTopicName(topic), encoded, 1U, false) < 0)
    {
        m_stateStore->setLastError(QStringLiteral("命令发布失败"));
        appendLog(QStringLiteral("TX 失败 %1").arg(topic));
        return false;
    }

    m_lastCommandDeviceId = deviceId;
    rememberPendingCommandState(deviceId, msgId, command, value, hasValue);
    m_commandTracker->startPending(msgId, command, value, hasValue, kDirectCommandTimeoutMs);
    m_stateStore->applyPredictedCommand(deviceId, command, value, hasValue);
    if (m_deviceRepository != nullptr)
    {
        const DeviceState predicted = m_stateStore->stateForDevice(deviceId);
        if (!predicted.deviceId.isEmpty())
        {
            m_deviceRepository->upsertState(predicted);
        }
    }
    m_historyRepository->appendRecord(
        MessageRecord{0,
                      deviceId,
                      QStringLiteral("out"),
                      QStringLiteral("mqtt"),
                      topic,
                      command,
                      QString::fromUtf8(encoded),
                      QStringLiteral("pending"),
                      QStringLiteral("qt-desktop"),
                      QStringLiteral("info"),
                      QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)});
    m_cache->insertOutboxCommand(msgId, deviceId, command, QString::fromUtf8(encoded), QStringLiteral("pending"));
    m_stateStore->clearLastError();
    appendLog(QStringLiteral("TX %1 %2").arg(topic, QString::fromUtf8(encoded)));
    return true;
}

bool RealtimeGateway::publishLegacyCommand(const QString &deviceId, const QString &command, int value, bool hasValue)
{
    const QString topic = m_legacyCommandTopics.value(deviceId);
    const QString token = legacyCommandToken(command, value, hasValue);
    if (topic.isEmpty() || token.isEmpty())
    {
        m_stateStore->setLastError(QStringLiteral("Legacy MQTT mapping is incomplete"));
        return false;
    }

    if (m_client.publish(QMqttTopicName(topic), token.toUtf8(), 1U, false) < 0)
    {
        m_stateStore->setLastError(QStringLiteral("Legacy command publish failed"));
        appendLog(QStringLiteral("TX legacy failed %1").arg(topic));
        return false;
    }

    m_lastCommandDeviceId = deviceId;
    m_commandTracker->rememberLastCommand(command, value, hasValue);
    m_commandTracker->clearLastError();
    m_historyRepository->appendRecord(
        MessageRecord{0,
                      deviceId,
                      QStringLiteral("out"),
                      QStringLiteral("mqtt"),
                      topic,
                      command,
                      token,
                      QStringLiteral("legacy_sent"),
                      QStringLiteral("qt-desktop"),
                      QStringLiteral("info"),
                      QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)});
    m_stateStore->clearLastError();
    appendLog(QStringLiteral("TX legacy %1 %2").arg(topic, token));

    if (command != QStringLiteral("query_status"))
    {
        QTimer::singleShot(800, this, [this, deviceId]() {
            if (!connected() || !m_legacyCommandTopics.contains(deviceId))
            {
                return;
            }
            publishLegacyCommand(deviceId, QStringLiteral("query_status"), 0, false);
        });
    }

    return true;
}

void RealtimeGateway::persistStateForDevice(const QString &deviceId)
{
    if (deviceId.isEmpty())
    {
        return;
    }

    const DeviceState state = m_stateStore->stateForDevice(deviceId);
    if (state.deviceId.isEmpty())
    {
        return;
    }

    m_cache->upsertDeviceState(state);
    m_deviceRepository->upsertState(state);
    handleStateRules(state);
    syncAlarmCounts();
}

void RealtimeGateway::syncAlarmCounts()
{
    const QHash<QString, int> counts = m_alarmRepository->activeCounts();
    m_deviceRepository->setActiveAlarmCounts(counts);
    const QVector<DeviceState> states = m_deviceRepository->states();
    for (const DeviceState &device : states)
    {
        m_stateStore->updateAlarmCount(device.deviceId, counts.value(device.deviceId, 0));
        DeviceState refreshed = m_stateStore->stateForDevice(device.deviceId);
        if (!refreshed.deviceId.isEmpty())
        {
            m_cache->upsertDeviceState(refreshed);
            m_deviceRepository->upsertState(refreshed);
        }
    }
}

void RealtimeGateway::handleStateRules(const DeviceState &state)
{
    if (state.deviceId.isEmpty())
    {
        return;
    }

    if (state.rssi > 0 && state.rssi < m_lowRssiThreshold)
    {
        m_alarmRepository->raiseAlarm(state.deviceId,
                                      QStringLiteral("low_rssi"),
                                      QStringLiteral("warning"),
                                      QStringLiteral("设备 RSSI 低于阈值: %1").arg(state.rssi),
                                      QStringLiteral("mqtt"));
    }
    else
    {
        m_alarmRepository->resolveAlarm(state.deviceId, QStringLiteral("low_rssi"));
    }

    if (state.online)
    {
        m_alarmRepository->resolveAlarm(state.deviceId, QStringLiteral("device_offline"));
    }
}

QString RealtimeGateway::extractDeviceId(const QString &topicName) const
{
    const QStringList parts = topicName.split('/');
    if (parts.size() >= 4 && parts.constFirst() == QStringLiteral("device"))
    {
        return parts.at(1);
    }

    return QString();
}

void RealtimeGateway::rememberLegacyTopic(const QString &deviceId, const QString &topicName)
{
    if (deviceId.isEmpty() || topicName.isEmpty())
    {
        return;
    }

    const QString commandTopic = legacyCommandTopicForStatusTopic(topicName);
    if (commandTopic.isEmpty())
    {
        return;
    }

    const QString existing = m_legacyCommandTopics.value(deviceId);
    if (existing == commandTopic)
    {
        return;
    }

    m_legacyCommandTopics.insert(deviceId, commandTopic);
    appendLog(QStringLiteral("Detected legacy topic for %1 -> %2").arg(deviceId, commandTopic));
}

void RealtimeGateway::rememberPendingCommandState(const QString &deviceId,
                                                  const QString &msgId,
                                                  const QString &command,
                                                  int value,
                                                  bool hasValue)
{
    m_pendingCommandDeviceId = deviceId;
    m_pendingCommandMsgId = msgId;
    m_pendingCommandName = command;
    m_pendingCommandValue = value;
    m_pendingCommandHasValue = hasValue;
    m_pendingCommandStartedMs = QDateTime::currentMSecsSinceEpoch();
    m_pendingBaselineTick = 0;

    if (m_stateStore != nullptr && !deviceId.isEmpty())
    {
        m_pendingBaselineTick = m_stateStore->stateForDevice(deviceId).tick;
    }
}

void RealtimeGateway::clearPendingCommandState()
{
    m_pendingCommandDeviceId.clear();
    m_pendingCommandMsgId.clear();
    m_pendingCommandName.clear();
    m_pendingCommandValue = 0;
    m_pendingCommandHasValue = false;
    m_pendingCommandStartedMs = 0;
    m_pendingBaselineTick = 0;
}

bool RealtimeGateway::tryResolvePendingCommandFromStatus(const QJsonObject &statusObject)
{
    if (m_commandTracker == nullptr || !m_commandTracker->pending())
    {
        return false;
    }

    const QString deviceId = readStringLike(statusObject.value(QStringLiteral("device_id")));
    const QString pendingMsgId = m_commandTracker->pendingMsgId();
    if (deviceId.isEmpty() ||
        pendingMsgId.isEmpty() ||
        m_pendingCommandDeviceId.isEmpty() ||
        m_pendingCommandMsgId.isEmpty() ||
        pendingMsgId != m_pendingCommandMsgId ||
        deviceId != m_pendingCommandDeviceId)
    {
        return false;
    }

    if (!statusIsFreshEnough(statusObject, m_pendingBaselineTick, m_pendingCommandStartedMs))
    {
        return false;
    }

    if (!statusMatchesCommandOutcome(statusObject,
                                     m_pendingCommandName,
                                     m_pendingCommandValue,
                                     m_pendingCommandHasValue))
    {
        return false;
    }

    m_commandTracker->resolveAck(pendingMsgId);
    if (m_cache != nullptr)
    {
        m_cache->updateOutboxStatus(pendingMsgId,
                                    QStringLiteral("ack_success"),
                                    QStringLiteral("Resolved from status report"));
    }
    if (m_stateStore != nullptr)
    {
        m_stateStore->noteAckStatus(QStringLiteral("%1 -> success (status)")
                                        .arg(m_pendingCommandName),
                                    QStringLiteral("success"));
        m_stateStore->clearLastError();
    }
    if (m_alarmRepository != nullptr)
    {
        m_alarmRepository->resolveAlarm(deviceId, QStringLiteral("ack_timeout"));
        m_alarmRepository->resolveAlarm(deviceId, QStringLiteral("command_error"));
        syncAlarmCounts();
    }

    appendLog(QStringLiteral("Resolved pending command from status msg_id=%1 cmd=%2")
                  .arg(pendingMsgId, m_pendingCommandName));
    clearPendingCommandState();
    return true;
}
