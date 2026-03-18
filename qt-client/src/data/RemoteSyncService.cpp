#include "RemoteSyncService.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

#include "auth/AuthSession.h"
#include "data/AlarmRepository.h"
#include "data/ApiClient.h"
#include "data/DeviceRepository.h"
#include "data/HistoryRepository.h"
#include "data/SQLiteCache.h"
#include "domain/CommandTracker.h"
#include "domain/DeviceStateStore.h"
#include "models/DeviceModels.h"
#include "realtime/RealtimeGateway.h"

namespace
{
constexpr int kAndroidDevicesPollIntervalMs = 600;
constexpr int kDesktopDevicesPollIntervalMs = 1200;
constexpr int kAndroidSecondaryPollIntervalMs = 1400;
constexpr int kDesktopSecondaryPollIntervalMs = 2800;
constexpr int kPendingCommandPollIntervalMs = 250;
constexpr int kPendingCommandTimeoutMs = 6000;

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
        const QString text = value.toString().trimmed().toLower();
        if (text == QStringLiteral("1") || text == QStringLiteral("true") || text == QStringLiteral("on"))
        {
            return true;
        }
        if (text == QStringLiteral("0") || text == QStringLiteral("false") || text == QStringLiteral("off"))
        {
            return false;
        }
    }
    return fallback;
}

QString payloadText(const QJsonValue &value)
{
    if (value.isUndefined() || value.isNull())
    {
        return QString();
    }
    if (value.isString())
    {
        return value.toString();
    }
    if (value.isObject())
    {
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    }
    if (value.isArray())
    {
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    }
    return value.toVariant().toString();
}

double readNumberLike(const QJsonValue &value, double fallback = 0.0)
{
    if (value.isDouble())
    {
        return value.toDouble();
    }
    if (value.isString())
    {
        bool ok = false;
        const double parsed = value.toString().toDouble(&ok);
        return ok ? parsed : fallback;
    }
    return fallback;
}

QString responseErrorText(QNetworkReply *reply, const QByteArray &payload)
{
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error == QJsonParseError::NoError && json.isObject())
    {
        const QString detail = json.object().value(QStringLiteral("detail")).toString();
        if (!detail.isEmpty())
        {
            return detail;
        }
    }
    return reply != nullptr ? reply->errorString() : QStringLiteral("Network error");
}

DeviceState deviceStateFromJson(const QJsonObject &object)
{
    DeviceState state;
    state.deviceId = object.value(QStringLiteral("device_id")).toString();
    state.displayName = object.value(QStringLiteral("display_name")).toString(state.deviceId);
    state.online = readBoolLike(object.value(QStringLiteral("online")), false);
    state.protocolProfile = object.value(QStringLiteral("protocol_profile")).toString(QStringLiteral("relay_v1"));
    state.relay1On = readBoolLike(object.value(QStringLiteral("relay1")), false);
    state.relay2On = readBoolLike(object.value(QStringLiteral("relay2")), false);
    state.rssi = object.value(QStringLiteral("rssi")).toInt();
    state.operatorName = object.value(QStringLiteral("operator_name")).toString();
    state.ip = object.value(QStringLiteral("ip")).toString();
    state.stateText = object.value(QStringLiteral("state_text")).toString();
    state.tick = static_cast<qint64>(object.value(QStringLiteral("tick")).toDouble());
    const QJsonObject pad = object.value(QStringLiteral("pad")).toObject();
    state.padLeftState = pad.value(QStringLiteral("left_state")).toString(QStringLiteral("closed"));
    state.padRightState = pad.value(QStringLiteral("right_state")).toString(QStringLiteral("closed"));
    state.padReady = readBoolLike(pad.value(QStringLiteral("ready")), false);
    state.padOccupied = readBoolLike(pad.value(QStringLiteral("occupied")), false);
    state.padMode = pad.value(QStringLiteral("mode")).toString(QStringLiteral("auto"));
    const QJsonObject weather = object.value(QStringLiteral("weather")).toObject();
    state.windSpeed = readNumberLike(weather.value(QStringLiteral("wind_speed")));
    state.windDirection = readNumberLike(weather.value(QStringLiteral("wind_direction")));
    state.temperature = readNumberLike(weather.value(QStringLiteral("temperature")));
    state.humidity = readNumberLike(weather.value(QStringLiteral("humidity")));
    state.pressure = readNumberLike(weather.value(QStringLiteral("pressure")));
    state.visibility = readNumberLike(weather.value(QStringLiteral("visibility")));
    state.timestamp = object.value(QStringLiteral("last_seen_at")).toString(
        object.value(QStringLiteral("updated_at")).toString());
    state.lastSeenMs = QDateTime::currentMSecsSinceEpoch();
    state.activeAlarmCount = object.value(QStringLiteral("active_alarm_count")).toInt();
    return state;
}

MessageRecord historyRecordFromJson(const QJsonObject &object)
{
    MessageRecord record;
    record.id = static_cast<qint64>(object.value(QStringLiteral("id")).toDouble());
    record.deviceId = object.value(QStringLiteral("device_id")).toString();
    record.direction = object.value(QStringLiteral("direction")).toString();
    record.channel = object.value(QStringLiteral("channel")).toString();
    record.topic = object.value(QStringLiteral("topic")).toString();
    record.command = object.value(QStringLiteral("command")).toString();
    record.payload = payloadText(object.value(QStringLiteral("payload")));
    record.result = object.value(QStringLiteral("result")).toString();
    record.operatorName = object.value(QStringLiteral("operator_name")).toString();
    record.level = QStringLiteral("remote");
    record.createdAt = object.value(QStringLiteral("created_at")).toString();
    return record;
}

AlarmRecord alarmRecordFromJson(const QJsonObject &object)
{
    AlarmRecord record;
    record.id = static_cast<qint64>(object.value(QStringLiteral("id")).toDouble());
    record.alarmKey = object.value(QStringLiteral("alarm_key")).toString();
    record.deviceId = object.value(QStringLiteral("device_id")).toString();
    record.code = object.value(QStringLiteral("code")).toString();
    record.severity = object.value(QStringLiteral("severity")).toString();
    record.message = object.value(QStringLiteral("message")).toString();
    record.source = object.value(QStringLiteral("source")).toString();
    record.active = readBoolLike(object.value(QStringLiteral("active")), true);
    record.createdAt = object.value(QStringLiteral("created_at")).toString();
    record.resolvedAt = object.value(QStringLiteral("resolved_at")).toString();
    return record;
}

bool isPendingStatus(const QString &status)
{
    return status == QStringLiteral("pending") ||
           status == QStringLiteral("queued") ||
           status == QStringLiteral("sent");
}

bool isSuccessStatus(const QString &status)
{
    return status == QStringLiteral("success") ||
           status == QStringLiteral("ack_success") ||
           status == QStringLiteral("ack");
}
}

RemoteSyncService::RemoteSyncService(ApiClient *apiClient,
                                     SQLiteCache *cache,
                                     AuthSession *authSession,
                                     DeviceStateStore *stateStore,
                                     CommandTracker *commandTracker,
                                     DeviceRepository *deviceRepository,
                                     HistoryRepository *historyRepository,
                                     AlarmRepository *alarmRepository,
                                     RealtimeGateway *logGateway,
                                     QObject *parent)
    : QObject(parent)
    , m_apiClient(apiClient)
    , m_cache(cache)
    , m_authSession(authSession)
    , m_stateStore(stateStore)
    , m_commandTracker(commandTracker)
    , m_deviceRepository(deviceRepository)
    , m_historyRepository(historyRepository)
    , m_alarmRepository(alarmRepository)
    , m_logGateway(logGateway)
{
    m_pagePollTimer.setSingleShot(false);
    m_commandPollTimer.setSingleShot(false);
    m_commandPollTimer.setInterval(kPendingCommandPollIntervalMs);

    connect(&m_pagePollTimer, &QTimer::timeout, this, &RemoteSyncService::pollCurrentPage);
    connect(&m_commandPollTimer, &QTimer::timeout, this, &RemoteSyncService::pollPendingCommand);
    connect(m_authSession, &AuthSession::sessionChanged, this, [this]() {
        restartRealtimeSocket();
        refreshVisibleData();
    });
    connect(m_authSession, &AuthSession::apiBaseUrlChanged, this, [this]() {
        restartRealtimeSocket();
    });

#if defined(RELAY_CLIENT_HAS_WEBSOCKETS)
    connect(&m_socket, &QWebSocket::connected, this, [this]() {
        appendLog(QStringLiteral("WS connected"));
        setConnectionState(QStringLiteral("Cloud Live"), true);
        setLastError(QString());
        refreshVisibleData();
    });
    connect(&m_socket, &QWebSocket::disconnected, this, [this]() {
        appendLog(QStringLiteral("WS disconnected"));
        if (canUseApi())
        {
            setConnectionState(QStringLiteral("Cloud Fallback"), true);
        }
    });
    connect(&m_socket, &QWebSocket::textMessageReceived, this, &RemoteSyncService::handleRealtimeTextMessage);
    connect(
        &m_socket,
        qOverload<QAbstractSocket::SocketError>(&QWebSocket::errorOccurred),
        this,
        [this](QAbstractSocket::SocketError) {
            appendLog(QStringLiteral("WS error: %1").arg(m_socket.errorString()));
            if (canUseApi())
            {
                setConnectionState(QStringLiteral("Cloud Fallback"), true);
            }
        });
#endif

    restartRealtimeSocket();
    reconfigurePolling();
}

bool RemoteSyncService::androidMode() const
{
    return m_androidMode;
}

void RemoteSyncService::setAndroidMode(bool androidModeValue)
{
    if (m_androidMode == androidModeValue)
    {
        return;
    }

    m_androidMode = androidModeValue;
    emit androidModeChanged();
    reconfigurePolling();
}

QString RemoteSyncService::currentPage() const
{
    return m_currentPage;
}

void RemoteSyncService::setCurrentPage(const QString &page)
{
    const QString normalized = page.trimmed().isEmpty() ? QStringLiteral("overview") : page.trimmed();
    if (m_currentPage == normalized)
    {
        return;
    }

    m_currentPage = normalized;
    emit currentPageChanged();
    refreshVisibleData();
    reconfigurePolling();
}

QString RemoteSyncService::currentDeviceId() const
{
    return m_currentDeviceId;
}

void RemoteSyncService::setCurrentDeviceId(const QString &deviceId)
{
    const QString normalized = deviceId.trimmed();
    if (m_currentDeviceId == normalized)
    {
        return;
    }

    m_currentDeviceId = normalized;
    emit currentDeviceIdChanged();
    if (!m_currentDeviceId.isEmpty())
    {
        refreshVisibleData();
    }
}

QString RemoteSyncService::connectionState() const
{
    return m_connectionState;
}

bool RemoteSyncService::connected() const
{
    return m_connected;
}

QString RemoteSyncService::lastError() const
{
    return m_lastError;
}

void RemoteSyncService::refreshDevices()
{
    if (!canUseApi() || !beginRequest(QStringLiteral("devices")))
    {
        return;
    }

    appendLog(QStringLiteral("GET /devices"));
    QNetworkReply *reply = m_apiClient->get(QStringLiteral("/devices"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray payload = reply->readAll();
        endRequest(QStringLiteral("devices"));

        if (reply->error() != QNetworkReply::NoError)
        {
            handleApiFailure(reply, payload, QStringLiteral("GET /devices"));
            reply->deleteLater();
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument json = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !json.isArray())
        {
            setLastError(QStringLiteral("Invalid /devices payload"));
            setConnectionState(QStringLiteral("Cloud Parse Error"), false);
            reply->deleteLater();
            return;
        }

        const QJsonArray items = json.array();
        for (const QJsonValue &value : items)
        {
            if (value.isObject())
            {
                applyDeviceFromJson(value.toObject());
            }
        }

        setConnectionState(QStringLiteral("Cloud Ready"), true);
        setLastError(QString());
        appendLog(QStringLiteral("OK /devices (%1)").arg(items.size()));
        reply->deleteLater();
    });
}

void RemoteSyncService::refreshCurrentDevice()
{
    if (!canUseApi() || m_currentDeviceId.isEmpty() || !beginRequest(QStringLiteral("device")))
    {
        return;
    }

    appendLog(QStringLiteral("GET /devices/%1").arg(m_currentDeviceId));
    QNetworkReply *reply = m_apiClient->get(QStringLiteral("/devices/%1").arg(m_currentDeviceId));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray payload = reply->readAll();
        endRequest(QStringLiteral("device"));

        if (reply->error() != QNetworkReply::NoError)
        {
            handleApiFailure(reply, payload, QStringLiteral("GET /devices/%1").arg(m_currentDeviceId));
            reply->deleteLater();
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument json = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !json.isObject())
        {
            setLastError(QStringLiteral("Invalid /devices/{id} payload"));
            setConnectionState(QStringLiteral("Cloud Parse Error"), false);
            reply->deleteLater();
            return;
        }

        applyDeviceFromJson(json.object());
        setConnectionState(QStringLiteral("Cloud Ready"), true);
        setLastError(QString());
        reply->deleteLater();
    });
}

void RemoteSyncService::refreshHistory()
{
    if (!canUseApi() || m_currentDeviceId.isEmpty() || !beginRequest(QStringLiteral("history")))
    {
        return;
    }

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("200"));
    appendLog(QStringLiteral("GET /devices/%1/history").arg(m_currentDeviceId));
    QNetworkReply *reply = m_apiClient->get(QStringLiteral("/devices/%1/history").arg(m_currentDeviceId), query);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray payload = reply->readAll();
        endRequest(QStringLiteral("history"));

        if (reply->error() != QNetworkReply::NoError)
        {
            handleApiFailure(reply, payload, QStringLiteral("GET /devices/%1/history").arg(m_currentDeviceId), true);
            reply->deleteLater();
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument json = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !json.isArray())
        {
            if (m_historyRepository != nullptr)
            {
                m_historyRepository->setPreferRemote(false);
                m_historyRepository->reload();
            }
            setLastError(QStringLiteral("Invalid /history payload"));
            setConnectionState(QStringLiteral("Cloud Parse Error"), false);
            reply->deleteLater();
            return;
        }

        QVector<MessageRecord> records;
        const QJsonArray items = json.array();
        records.reserve(items.size());
        for (const QJsonValue &value : items)
        {
            if (value.isObject())
            {
                records.push_back(historyRecordFromJson(value.toObject()));
            }
        }

        if (m_cache != nullptr)
        {
            m_cache->replaceRemoteMessages(m_currentDeviceId, records);
        }
        if (m_historyRepository != nullptr)
        {
            m_historyRepository->setPreferRemote(true);
            m_historyRepository->reload();
        }

        setConnectionState(QStringLiteral("Cloud Ready"), true);
        setLastError(QString());
        reply->deleteLater();
    });
}

void RemoteSyncService::refreshAlarms()
{
    if (!canUseApi() || m_currentDeviceId.isEmpty() || !beginRequest(QStringLiteral("alarms")))
    {
        return;
    }

    appendLog(QStringLiteral("GET /devices/%1/alarms").arg(m_currentDeviceId));
    QNetworkReply *reply = m_apiClient->get(QStringLiteral("/devices/%1/alarms").arg(m_currentDeviceId));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray payload = reply->readAll();
        endRequest(QStringLiteral("alarms"));

        if (reply->error() != QNetworkReply::NoError)
        {
            handleApiFailure(reply, payload, QStringLiteral("GET /devices/%1/alarms").arg(m_currentDeviceId));
            reply->deleteLater();
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument json = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !json.isArray())
        {
            setLastError(QStringLiteral("Invalid /alarms payload"));
            setConnectionState(QStringLiteral("Cloud Parse Error"), false);
            reply->deleteLater();
            return;
        }

        if (m_cache != nullptr)
        {
            for (const QJsonValue &value : json.array())
            {
                if (value.isObject())
                {
                    m_cache->upsertAlarm(alarmRecordFromJson(value.toObject()));
                }
            }
        }

        if (m_alarmRepository != nullptr)
        {
            m_alarmRepository->reload();
        }
        applyAlarmCacheCounts();
        setConnectionState(QStringLiteral("Cloud Ready"), true);
        setLastError(QString());
        reply->deleteLater();
    });
}

void RemoteSyncService::refreshCommands()
{
    if (!canUseApi() || m_currentDeviceId.isEmpty() || !beginRequest(QStringLiteral("commands")))
    {
        return;
    }

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("device_id"), m_currentDeviceId);
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("200"));
    if (!m_pendingRemoteMsgId.isEmpty())
    {
        query.addQueryItem(QStringLiteral("msg_id"), m_pendingRemoteMsgId);
    }

    appendLog(QStringLiteral("GET /commands?device_id=%1").arg(m_currentDeviceId));
    QNetworkReply *reply = m_apiClient->get(QStringLiteral("/commands"), query);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray payload = reply->readAll();
        endRequest(QStringLiteral("commands"));

        if (reply->error() != QNetworkReply::NoError)
        {
            handleApiFailure(reply, payload, QStringLiteral("GET /commands"));
            reply->deleteLater();
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument json = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !json.isArray())
        {
            setLastError(QStringLiteral("Invalid /commands payload"));
            setConnectionState(QStringLiteral("Cloud Parse Error"), false);
            reply->deleteLater();
            return;
        }

        for (const QJsonValue &value : json.array())
        {
            if (!value.isObject())
            {
                continue;
            }

            const QJsonObject object = value.toObject();
            const QString msgId = object.value(QStringLiteral("msg_id")).toString();
            const QString command = object.value(QStringLiteral("command")).toString();
            const QString status = object.value(QStringLiteral("status")).toString();
            const QString detail = object.value(QStringLiteral("detail")).toString();

            if (m_cache != nullptr && !msgId.isEmpty())
            {
                m_cache->insertOutboxCommand(msgId,
                                             object.value(QStringLiteral("device_id")).toString(),
                                             command,
                                             payloadText(object.value(QStringLiteral("payload"))),
                                             status,
                                             detail);
            }

            if (msgId != m_pendingRemoteMsgId)
            {
                continue;
            }

            if (isPendingStatus(status))
            {
                continue;
            }

            const QString summary = QStringLiteral("%1 -> %2").arg(command, status);
            if (m_stateStore != nullptr)
            {
                m_stateStore->noteAckStatus(summary, status);
            }

            if (isSuccessStatus(status))
            {
                if (m_commandTracker != nullptr)
                {
                    m_commandTracker->resolveAck(msgId);
                }
                if (m_stateStore != nullptr)
                {
                    m_stateStore->clearLastError();
                }
                m_pendingRemoteMsgId.clear();
                m_pendingRemoteCommand.clear();
                m_pendingStartedMs = 0;
                m_commandPollTimer.stop();
                refreshCurrentDevice();
                refreshHistory();
                refreshAlarms();
            }
            else
            {
                const QString errorText = detail.isEmpty() ? status : detail;
                if (m_commandTracker != nullptr)
                {
                    m_commandTracker->failPending(errorText);
                }
                if (m_stateStore != nullptr)
                {
                    m_stateStore->setLastError(errorText);
                }
                m_pendingRemoteMsgId.clear();
                m_pendingRemoteCommand.clear();
                m_pendingStartedMs = 0;
                m_commandPollTimer.stop();
                refreshHistory();
                refreshAlarms();
            }
        }

        setConnectionState(QStringLiteral("Cloud Ready"), true);
        setLastError(QString());
        reply->deleteLater();
    });
}

void RemoteSyncService::refreshVisibleData()
{
    if (!canUseApi())
    {
        if (m_historyRepository != nullptr)
        {
            m_historyRepository->setPreferRemote(false);
            m_historyRepository->reload();
        }
        if (m_alarmRepository != nullptr)
        {
            m_alarmRepository->reload();
        }
        return;
    }

    refreshDevices();

    if (m_currentPage == QStringLiteral("overview") ||
        m_currentPage == QStringLiteral("pad") ||
        m_currentPage == QStringLiteral("weather") ||
        m_currentPage == QStringLiteral("engineering"))
    {
        refreshCurrentDevice();
        return;
    }
    if (m_currentPage == QStringLiteral("history"))
    {
        refreshHistory();
        refreshCommands();
        return;
    }
    if (m_currentPage == QStringLiteral("alarms"))
    {
        refreshAlarms();
        return;
    }

    refreshCurrentDevice();
}

bool RemoteSyncService::sendCommand(const QString &command, int value, bool hasValue, const QString &operatorName)
{
    if (!canUseApi())
    {
        setLastError(QStringLiteral("Cloud API is not ready"));
        return false;
    }
    if (m_currentDeviceId.isEmpty())
    {
        setLastError(QStringLiteral("Please select a device first"));
        return false;
    }
    if (m_commandTracker != nullptr && m_commandTracker->pending())
    {
        if (m_stateStore != nullptr)
        {
            m_stateStore->setLastError(QStringLiteral("Please wait for the previous ACK"));
        }
        return false;
    }

    QJsonObject body;
    body.insert(QStringLiteral("device_id"), m_currentDeviceId);
    body.insert(QStringLiteral("command"), command);
    if (hasValue)
    {
        body.insert(QStringLiteral("value"), value);
    }
    body.insert(QStringLiteral("operator"), operatorName);

    appendLog(QStringLiteral("POST /devices/%1/commands").arg(m_currentDeviceId));
    QNetworkReply *reply = m_apiClient->post(
        QStringLiteral("/devices/%1/commands").arg(m_currentDeviceId),
        QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, command, value, hasValue]() {
        const QByteArray payload = reply->readAll();
        if (reply->error() != QNetworkReply::NoError)
        {
            handleApiFailure(reply, payload, QStringLiteral("POST /devices/%1/commands").arg(m_currentDeviceId));
            reply->deleteLater();
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument json = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !json.isObject())
        {
            setLastError(QStringLiteral("Invalid command response"));
            setConnectionState(QStringLiteral("Cloud Parse Error"), false);
            reply->deleteLater();
            return;
        }

        const QJsonObject object = json.object();
        const QString msgId = object.value(QStringLiteral("msg_id")).toString();
        const QString status = object.value(QStringLiteral("status")).toString(QStringLiteral("pending"));
        const QString detail = object.value(QStringLiteral("detail")).toString();
        if (msgId.isEmpty())
        {
            setLastError(QStringLiteral("Command response missing msg_id"));
            setConnectionState(QStringLiteral("Cloud Parse Error"), false);
            reply->deleteLater();
            return;
        }

        const QString summary = QStringLiteral("%1 -> %2").arg(command, status);
        if (!isPendingStatus(status))
        {
            if (m_stateStore != nullptr)
            {
                m_stateStore->noteAckStatus(summary, status);
            }

            if (isSuccessStatus(status))
            {
                if (m_stateStore != nullptr)
                {
                    m_stateStore->clearLastError();
                }
                setConnectionState(QStringLiteral("Cloud Ready"), true);
                setLastError(QString());
            }
            else
            {
                const QString errorText = detail.isEmpty() ? status : detail;
                if (m_stateStore != nullptr)
                {
                    m_stateStore->setLastError(errorText);
                }
                setLastError(errorText);
            }

            refreshCommands();
            refreshCurrentDevice();
            refreshHistory();
            refreshAlarms();
            reply->deleteLater();
            return;
        }

        if (m_commandTracker != nullptr)
        {
            m_commandTracker->startPending(msgId, command, value, hasValue, kPendingCommandTimeoutMs);
        }
        if (m_stateStore != nullptr)
        {
            m_stateStore->clearLastError();
            m_stateStore->noteAckStatus(summary, status);
        }
        m_pendingRemoteMsgId = msgId;
        m_pendingRemoteCommand = command;
        m_pendingStartedMs = QDateTime::currentMSecsSinceEpoch();
        m_commandPollTimer.start();
        setConnectionState(QStringLiteral("Cloud Ready"), true);
        setLastError(QString());
        refreshCurrentDevice();
        refreshDevices();
        refreshCommands();
        reply->deleteLater();
    });
    return true;
}

void RemoteSyncService::pollCurrentPage()
{
    if (m_currentPage == QStringLiteral("overview") ||
        m_currentPage == QStringLiteral("pad") ||
        m_currentPage == QStringLiteral("weather") ||
        m_currentPage == QStringLiteral("engineering"))
    {
        refreshDevices();
        refreshCurrentDevice();
        return;
    }
    if (m_currentPage == QStringLiteral("history"))
    {
        refreshHistory();
        refreshCommands();
        return;
    }
    if (m_currentPage == QStringLiteral("alarms"))
    {
        refreshAlarms();
        return;
    }
    refreshCurrentDevice();
}

void RemoteSyncService::pollPendingCommand()
{
    if (m_pendingRemoteMsgId.isEmpty())
    {
        m_commandPollTimer.stop();
        return;
    }

    if (m_pendingStartedMs > 0 && (QDateTime::currentMSecsSinceEpoch() - m_pendingStartedMs) > kPendingCommandTimeoutMs)
    {
        m_commandPollTimer.stop();
        const QString timeoutText = QStringLiteral("%1 ACK timeout").arg(m_pendingRemoteCommand);
        if (m_commandTracker != nullptr)
        {
            m_commandTracker->failPending(timeoutText);
        }
        if (m_stateStore != nullptr)
        {
            m_stateStore->noteAckStatus(timeoutText, QStringLiteral("ack_timeout"));
            m_stateStore->setLastError(timeoutText);
        }
        m_pendingRemoteMsgId.clear();
        m_pendingRemoteCommand.clear();
        m_pendingStartedMs = 0;
        refreshHistory();
        refreshAlarms();
        return;
    }

    refreshCurrentDevice();
    refreshCommands();
}

void RemoteSyncService::appendLog(const QString &line)
{
    if (m_logGateway != nullptr)
    {
        m_logGateway->appendExternalLog(QStringLiteral("[API] %1").arg(line));
    }
}

void RemoteSyncService::setConnectionState(const QString &state, bool connectedValue)
{
    bool changed = false;
    if (m_connectionState != state)
    {
        m_connectionState = state;
        changed = true;
    }
    if (m_connected != connectedValue)
    {
        m_connected = connectedValue;
        changed = true;
    }
    if (changed)
    {
        emit connectionStateChanged();
    }
}

void RemoteSyncService::setLastError(const QString &message)
{
    if (m_lastError == message)
    {
        return;
    }

    m_lastError = message;
    emit lastErrorChanged();
}

void RemoteSyncService::reconfigurePolling()
{
    if (!canUseApi())
    {
        m_pagePollTimer.stop();
        return;
    }

    bool shouldPoll = m_androidMode;
#if defined(RELAY_CLIENT_HAS_WEBSOCKETS)
    shouldPoll = shouldPoll || m_socket.state() != QAbstractSocket::ConnectedState;
#else
    shouldPoll = true;
#endif
    if (!shouldPoll)
    {
        m_pagePollTimer.stop();
        return;
    }

    int intervalMs = 0;
    if (m_currentPage == QStringLiteral("overview") ||
        m_currentPage == QStringLiteral("pad") ||
        m_currentPage == QStringLiteral("weather") ||
        m_currentPage == QStringLiteral("engineering"))
    {
        intervalMs = m_androidMode ? kAndroidDevicesPollIntervalMs : kDesktopDevicesPollIntervalMs;
    }
    else if (m_currentPage == QStringLiteral("history") ||
             m_currentPage == QStringLiteral("alarms") ||
             m_currentPage == QStringLiteral("logs") ||
             m_currentPage == QStringLiteral("settings"))
    {
        intervalMs = m_androidMode ? kAndroidSecondaryPollIntervalMs : kDesktopSecondaryPollIntervalMs;
    }

    if (intervalMs <= 0)
    {
        m_pagePollTimer.stop();
        return;
    }

    m_pagePollTimer.start(intervalMs);
}

bool RemoteSyncService::canUseApi() const
{
    return m_apiClient != nullptr && m_apiClient->ready();
}

bool RemoteSyncService::beginRequest(const QString &key)
{
    if (m_inFlight.contains(key))
    {
        return false;
    }
    m_inFlight.insert(key);
    return true;
}

void RemoteSyncService::endRequest(const QString &key)
{
    m_inFlight.remove(key);
}

void RemoteSyncService::handleApiFailure(QNetworkReply *reply,
                                         const QByteArray &payload,
                                         const QString &context,
                                         bool dropRemoteHistory)
{
    const int statusCode = reply != nullptr
        ? reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()
        : 0;
    if (statusCode == 401 && m_authSession != nullptr && m_authSession->ensureFreshToken(0))
    {
        appendLog(QStringLiteral("%1 token refreshed").arg(context));
        return;
    }
    if (statusCode == 401 && m_authSession != nullptr)
    {
        appendLog(QStringLiteral("%1 unauthorized, clearing remote session").arg(context));
        m_authSession->invalidateRemoteSession(QStringLiteral("Session expired. Please log in again."));
        setConnectionState(QStringLiteral("Authentication Required"), false);
        if (dropRemoteHistory && m_historyRepository != nullptr)
        {
            m_historyRepository->setPreferRemote(false);
            m_historyRepository->reload();
        }
        return;
    }

    const QString errorText = responseErrorText(reply, payload);
    appendLog(QStringLiteral("%1 failed: %2").arg(context, errorText));
    setLastError(errorText);
    setConnectionState(QStringLiteral("Cloud Error"), false);

    if (dropRemoteHistory && m_historyRepository != nullptr)
    {
        m_historyRepository->setPreferRemote(false);
        m_historyRepository->reload();
    }
}

void RemoteSyncService::applyDeviceFromJson(const QJsonObject &object)
{
    const DeviceState state = deviceStateFromJson(object);
    if (state.deviceId.isEmpty())
    {
        return;
    }

    if (m_cache != nullptr)
    {
        m_cache->upsertDeviceState(state);
    }
    if (m_stateStore != nullptr)
    {
        m_stateStore->hydrateStates({state});
    }
    if (m_deviceRepository != nullptr)
    {
        m_deviceRepository->upsertState(state);
        if (m_deviceRepository->currentDeviceId().isEmpty())
        {
            m_deviceRepository->setCurrentDeviceId(state.deviceId);
        }
    }
    if (m_currentDeviceId.isEmpty())
    {
        m_currentDeviceId = state.deviceId;
        emit currentDeviceIdChanged();
    }
}

void RemoteSyncService::restartRealtimeSocket()
{
#if defined(RELAY_CLIENT_HAS_WEBSOCKETS)
    if (m_socket.state() != QAbstractSocket::UnconnectedState)
    {
        m_socket.abort();
    }
    if (!canUseApi())
    {
        reconfigurePolling();
        return;
    }

    QString wsBase = m_apiClient->baseUrl().trimmed();
    if (wsBase.endsWith(QStringLiteral("/api/v1")))
    {
        wsBase.chop(QStringLiteral("/api/v1").size());
    }
    if (wsBase.startsWith(QStringLiteral("https://")))
    {
        wsBase.replace(0, 8, QStringLiteral("wss://"));
    }
    else if (wsBase.startsWith(QStringLiteral("http://")))
    {
        wsBase.replace(0, 7, QStringLiteral("ws://"));
    }
    const QUrl url(wsBase + QStringLiteral("/api/v1/ws/realtime"));
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + m_authSession->token().toUtf8());
    appendLog(QStringLiteral("WS connect %1").arg(url.toString()));
    m_socket.open(request);
#else
    reconfigurePolling();
#endif
}

void RemoteSyncService::handleRealtimeTextMessage(const QString &message)
{
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(message.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject())
    {
        return;
    }

    const QJsonObject object = json.object();
    const QString type = object.value(QStringLiteral("type")).toString();
    const QJsonObject payload = object.value(QStringLiteral("payload")).toObject();

    if (type == QStringLiteral("heartbeat"))
    {
        setConnectionState(QStringLiteral("Cloud Live"), true);
        reconfigurePolling();
        return;
    }
    if (type == QStringLiteral("device_state"))
    {
        applyDeviceFromJson(payload);
        setConnectionState(QStringLiteral("Cloud Live"), true);
        return;
    }
    if (type == QStringLiteral("alarm_update"))
    {
        const QJsonArray items = payload.value(QStringLiteral("items")).toArray();
        if (m_cache != nullptr)
        {
            for (const QJsonValue &value : items)
            {
                if (value.isObject())
                {
                    m_cache->upsertAlarm(alarmRecordFromJson(value.toObject()));
                }
            }
        }
        if (m_alarmRepository != nullptr)
        {
            m_alarmRepository->reload();
        }
        applyAlarmCacheCounts();
        return;
    }
    if (type == QStringLiteral("command_update"))
    {
        const QString msgId = payload.value(QStringLiteral("msg_id")).toString();
        const QString status = payload.value(QStringLiteral("status")).toString();
        const QString detail = payload.value(QStringLiteral("detail")).toString();
        const QString command = payload.value(QStringLiteral("command")).toString();
        if (m_cache != nullptr && !msgId.isEmpty())
        {
            m_cache->insertOutboxCommand(
                msgId,
                payload.value(QStringLiteral("device_id")).toString(),
                command,
                payloadText(payload.value(QStringLiteral("payload"))),
                status,
                detail);
        }
        if (msgId == m_pendingRemoteMsgId && !isPendingStatus(status))
        {
            if (isSuccessStatus(status))
            {
                if (m_commandTracker != nullptr)
                {
                    m_commandTracker->resolveAck(msgId);
                }
                if (m_stateStore != nullptr)
                {
                    m_stateStore->clearLastError();
                }
            }
            else
            {
                const QString errorText = detail.isEmpty() ? status : detail;
                if (m_commandTracker != nullptr)
                {
                    m_commandTracker->failPending(errorText);
                }
                if (m_stateStore != nullptr)
                {
                    m_stateStore->setLastError(errorText);
                }
            }
            m_pendingRemoteMsgId.clear();
            m_pendingRemoteCommand.clear();
            m_pendingStartedMs = 0;
            m_commandPollTimer.stop();
            refreshCurrentDevice();
            refreshHistory();
            refreshAlarms();
        }
    }
}

void RemoteSyncService::applyAlarmCacheCounts()
{
    if (m_cache == nullptr || m_alarmRepository == nullptr || m_deviceRepository == nullptr || m_stateStore == nullptr)
    {
        return;
    }

    const QHash<QString, int> counts = m_cache->loadActiveAlarmCounts();
    m_deviceRepository->setActiveAlarmCounts(counts);
    for (auto it = counts.cbegin(); it != counts.cend(); ++it)
    {
        m_stateStore->updateAlarmCount(it.key(), it.value());
    }
}
