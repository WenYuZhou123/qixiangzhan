#include "DeviceStateStore.h"

#include <QDateTime>
#include <QJsonValue>

namespace
{
QString readString(const QJsonObject &json, const char *key, const QString &fallback = QString())
{
    const QJsonValue value = json.value(QLatin1String(key));
    return value.isString() ? value.toString() : fallback;
}

int readInt(const QJsonObject &json, const char *key, int fallback = 0)
{
    const QJsonValue value = json.value(QLatin1String(key));
    bool ok = false;

    if (value.isDouble())
    {
        return value.toInt();
    }
    if (value.isString())
    {
        const int parsed = value.toString().toInt(&ok);
        return ok ? parsed : fallback;
    }

    return fallback;
}

double readDouble(const QJsonObject &json, const char *key, double fallback = 0.0)
{
    const QJsonValue value = json.value(QLatin1String(key));
    bool ok = false;

    if (value.isDouble())
    {
        return value.toDouble();
    }
    if (value.isString())
    {
        const double parsed = value.toString().toDouble(&ok);
        return ok ? parsed : fallback;
    }

    return fallback;
}

qint64 readLongLong(const QJsonObject &json, const char *key, qint64 fallback = 0)
{
    const QJsonValue value = json.value(QLatin1String(key));
    bool ok = false;

    if (value.isDouble())
    {
        return static_cast<qint64>(value.toDouble());
    }
    if (value.isString())
    {
        const qint64 parsed = value.toString().toLongLong(&ok);
        return ok ? parsed : fallback;
    }

    return fallback;
}

bool readBoolLike(const QJsonObject &json, const char *key, bool fallback = false)
{
    const QJsonValue value = json.value(QLatin1String(key));
    const QString text = value.toString().trimmed();

    if (value.isBool())
    {
        return value.toBool();
    }
    if (value.isDouble())
    {
        return value.toInt() != 0;
    }
    if (text.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0 ||
        text.compare(QStringLiteral("on"), Qt::CaseInsensitive) == 0 ||
        text == QStringLiteral("1"))
    {
        return true;
    }
    if (text.compare(QStringLiteral("false"), Qt::CaseInsensitive) == 0 ||
        text.compare(QStringLiteral("off"), Qt::CaseInsensitive) == 0 ||
        text == QStringLiteral("0"))
    {
        return false;
    }

    return fallback;
}

void applyWeatherCapabilities(DeviceState &state, const QJsonObject &json)
{
    state.windCapability = readBoolLike(json, "wind", state.windCapability);
    state.airCapability = readBoolLike(json, "air", state.airCapability);
    state.rainCapability = readBoolLike(json, "rain", state.rainCapability);
    state.pressureCapability = readBoolLike(json, "pressure", state.pressureCapability);
    state.visibilityCapability = readBoolLike(json, "visibility", state.visibilityCapability);
}

void applySensorStatus(DeviceState &state, const QJsonObject &json)
{
    state.windSensorOnline = readBoolLike(json, "wind_online", state.windSensorOnline);
    state.airSensorOnline = readBoolLike(json, "air_online", state.airSensorOnline);
    state.rainSensorOnline = readBoolLike(json, "rain_online", state.rainSensorOnline);
    state.sensorFailureCount = readInt(json, "failure_count", state.sensorFailureCount);
    state.sensorLastOkTick = readLongLong(json, "last_ok_tick", state.sensorLastOkTick);
    state.sensorLastError = readString(json, "last_error", state.sensorLastError);
}
}

DeviceStateStore::DeviceStateStore(QObject *parent)
    : QObject(parent)
{
}

QString DeviceStateStore::currentDeviceId() const
{
    return m_currentDeviceId;
}

void DeviceStateStore::setCurrentDeviceId(const QString &deviceId)
{
    if (m_currentDeviceId == deviceId)
    {
        return;
    }

    m_currentDeviceId = deviceId;
    emit currentDeviceIdChanged();
    emit deviceStateChanged();
}

int DeviceStateStore::deviceCount() const
{
    return m_states.size();
}

bool DeviceStateStore::online() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->online : false;
}

QString DeviceStateStore::protocolProfile() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->protocolProfile : QStringLiteral("relay_v1");
}

bool DeviceStateStore::relay1On() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->relay1On : false;
}

bool DeviceStateStore::relay2On() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->relay2On : false;
}

QString DeviceStateStore::padLeftState() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->padLeftState : QStringLiteral("closed");
}

QString DeviceStateStore::padRightState() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->padRightState : QStringLiteral("closed");
}

bool DeviceStateStore::padReady() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->padReady : false;
}

bool DeviceStateStore::padOccupied() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->padOccupied : false;
}

QString DeviceStateStore::padMode() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->padMode : QStringLiteral("auto");
}

int DeviceStateStore::rssi() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->rssi : 0;
}

QString DeviceStateStore::operatorName() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->operatorName : QString();
}

QString DeviceStateStore::ip() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->ip : QString();
}

QString DeviceStateStore::stateText() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->stateText : QString();
}

qint64 DeviceStateStore::tick() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->tick : 0;
}

double DeviceStateStore::windSpeed() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->windSpeed : 0.0;
}

double DeviceStateStore::windDirection() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->windDirection : 0.0;
}

int DeviceStateStore::windSpeedRaw() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->windSpeedRaw : 0;
}

int DeviceStateStore::windDirectionRaw() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->windDirectionRaw : 0;
}

int DeviceStateStore::rainAdcRaw() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->rainAdcRaw : 0;
}

QString DeviceStateStore::windDirectionText() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->windDirectionText : QString();
}

QString DeviceStateStore::rainLevelText() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->rainLevelText : QString();
}

double DeviceStateStore::temperature() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->temperature : 0.0;
}

double DeviceStateStore::humidity() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->humidity : 0.0;
}

double DeviceStateStore::pressure() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->pressure : 0.0;
}

double DeviceStateStore::visibility() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->visibility : 0.0;
}

bool DeviceStateStore::windCapability() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->windCapability : true;
}

bool DeviceStateStore::airCapability() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->airCapability : true;
}

bool DeviceStateStore::rainCapability() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->rainCapability : true;
}

bool DeviceStateStore::pressureCapability() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->pressureCapability : false;
}

bool DeviceStateStore::visibilityCapability() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->visibilityCapability : false;
}

bool DeviceStateStore::rainDetected() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->rainDetected : false;
}

double DeviceStateStore::rainValue() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->rainValue : 0.0;
}

double DeviceStateStore::pm25() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->pm25 : 0.0;
}

double DeviceStateStore::pm10() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->pm10 : 0.0;
}

double DeviceStateStore::co2() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->co2 : 0.0;
}

double DeviceStateStore::tvoc() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->tvoc : 0.0;
}

double DeviceStateStore::ch2o() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->ch2o : 0.0;
}

bool DeviceStateStore::windSensorOnline() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->windSensorOnline : true;
}

bool DeviceStateStore::airSensorOnline() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->airSensorOnline : true;
}

bool DeviceStateStore::rainSensorOnline() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->rainSensorOnline : true;
}

int DeviceStateStore::sensorFailureCount() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->sensorFailureCount : 0;
}

qint64 DeviceStateStore::sensorLastOkTick() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->sensorLastOkTick : 0;
}

QString DeviceStateStore::sensorLastError() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->sensorLastError : QString();
}

QString DeviceStateStore::timestamp() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->timestamp : QString();
}

int DeviceStateStore::activeAlarmCount() const
{
    const DeviceState *state = currentState();
    return state != nullptr ? state->activeAlarmCount : 0;
}

QString DeviceStateStore::lastAckSummary() const
{
    return m_lastAckSummary;
}

QString DeviceStateStore::lastAckResult() const
{
    return m_lastAckResult;
}

QString DeviceStateStore::lastError() const
{
    return m_lastError;
}

QVector<DeviceState> DeviceStateStore::allStates() const
{
    return m_states.values().toVector();
}

DeviceState DeviceStateStore::stateForDevice(const QString &deviceId) const
{
    return m_states.value(deviceId);
}

void DeviceStateStore::updateStatus(const QJsonObject &json)
{
    const QString deviceId = readString(json, "device_id", m_currentDeviceId);
    DeviceState &state = ensureState(deviceId);

    state.deviceId = deviceId;
    state.protocolProfile = readString(json, "protocol_profile", state.protocolProfile);
    state.online = readBoolLike(json, "online", state.online);
    state.relay1On = readBoolLike(json, "relay1", state.relay1On);
    state.relay2On = readBoolLike(json, "relay2", state.relay2On);
    if (json.contains(QStringLiteral("pad")) && json.value(QStringLiteral("pad")).isObject())
    {
        const QJsonObject pad = json.value(QStringLiteral("pad")).toObject();
        state.padLeftState = readString(pad, "left_state", state.padLeftState);
        state.padRightState = readString(pad, "right_state", state.padRightState);
        state.padReady = readBoolLike(pad, "ready", state.padReady);
        state.padOccupied = readBoolLike(pad, "occupied", state.padOccupied);
        state.padMode = readString(pad, "mode", state.padMode);
    }
    else
    {
        state.padLeftState = readString(json, "pad_left_state", state.padLeftState);
        state.padRightState = readString(json, "pad_right_state", state.padRightState);
        state.padReady = readBoolLike(json, "pad_ready", state.padReady);
        state.padOccupied = readBoolLike(json, "pad_occupied", state.padOccupied);
        state.padMode = readString(json, "pad_mode", state.padMode);
    }
    state.rssi = readInt(json, "rssi", state.rssi);
    state.operatorName = readString(json, "operator", state.operatorName);
    state.ip = readString(json, "ip", state.ip);
    state.stateText = readString(json, "state", state.stateText);
    state.tick = readLongLong(json, "tick", state.tick);
    if (json.contains(QStringLiteral("weather")) && json.value(QStringLiteral("weather")).isObject())
    {
        const QJsonObject weather = json.value(QStringLiteral("weather")).toObject();
        state.windSpeed = readDouble(weather, "wind_speed", state.windSpeed);
        state.windDirection = readDouble(weather, "wind_direction", state.windDirection);
        state.windSpeedRaw = readInt(weather, "wind_speed_raw", state.windSpeedRaw);
        state.windDirectionRaw = readInt(weather, "wind_direction_raw", state.windDirectionRaw);
        state.rainAdcRaw = readInt(weather, "rain_adc_raw", state.rainAdcRaw);
        state.windDirectionText = readString(weather, "wind_direction_text", state.windDirectionText);
        state.rainLevelText = readString(weather, "rain_level_text", state.rainLevelText);
        state.temperature = readDouble(weather, "temperature", state.temperature);
        state.humidity = readDouble(weather, "humidity", state.humidity);
        state.pressure = readDouble(weather, "pressure", state.pressure);
        state.visibility = readDouble(weather, "visibility", state.visibility);
        if (weather.contains(QStringLiteral("capabilities")) &&
            weather.value(QStringLiteral("capabilities")).isObject())
        {
            applyWeatherCapabilities(state, weather.value(QStringLiteral("capabilities")).toObject());
        }
        state.rainDetected = readBoolLike(weather, "rain_detected", state.rainDetected);
        state.rainValue = readDouble(weather, "rain_value", state.rainValue);
        state.pm25 = readDouble(weather, "pm25", state.pm25);
        state.pm10 = readDouble(weather, "pm10", state.pm10);
        state.co2 = readDouble(weather, "co2", state.co2);
        state.tvoc = readDouble(weather, "tvoc", state.tvoc);
        state.ch2o = readDouble(weather, "ch2o", state.ch2o);
        if (weather.contains(QStringLiteral("sensor_status")) &&
            weather.value(QStringLiteral("sensor_status")).isObject())
        {
            applySensorStatus(state, weather.value(QStringLiteral("sensor_status")).toObject());
        }
    }
    else
    {
        state.windSpeed = readDouble(json, "weather_wind_speed", state.windSpeed);
        state.windDirection = readDouble(json, "weather_wind_direction", state.windDirection);
        state.windSpeedRaw = readInt(json, "weather_wind_speed_raw", state.windSpeedRaw);
        state.windDirectionRaw = readInt(json, "weather_wind_direction_raw", state.windDirectionRaw);
        state.rainAdcRaw = readInt(json, "weather_rain_adc_raw", state.rainAdcRaw);
        state.windDirectionText = readString(json, "weather_wind_direction_text", state.windDirectionText);
        state.rainLevelText = readString(json, "weather_rain_level_text", state.rainLevelText);
        state.temperature = readDouble(json, "weather_temperature", state.temperature);
        state.humidity = readDouble(json, "weather_humidity", state.humidity);
        state.pressure = readDouble(json, "weather_pressure", state.pressure);
        state.visibility = readDouble(json, "weather_visibility", state.visibility);
        state.windCapability = readBoolLike(json, "weather_capability_wind", state.windCapability);
        state.airCapability = readBoolLike(json, "weather_capability_air", state.airCapability);
        state.rainCapability = readBoolLike(json, "weather_capability_rain", state.rainCapability);
        state.pressureCapability = readBoolLike(json, "weather_capability_pressure", state.pressureCapability);
        state.visibilityCapability = readBoolLike(json, "weather_capability_visibility", state.visibilityCapability);
        state.rainDetected = readBoolLike(json, "weather_rain_detected", state.rainDetected);
        state.rainValue = readDouble(json, "weather_rain_value", state.rainValue);
        state.pm25 = readDouble(json, "weather_pm25", state.pm25);
        state.pm10 = readDouble(json, "weather_pm10", state.pm10);
        state.co2 = readDouble(json, "weather_co2", state.co2);
        state.tvoc = readDouble(json, "weather_tvoc", state.tvoc);
        state.ch2o = readDouble(json, "weather_ch2o", state.ch2o);
    }
    state.timestamp = readString(json, "timestamp", state.timestamp);
    state.lastSeenMs = QDateTime::currentMSecsSinceEpoch();
    if (state.displayName.isEmpty())
    {
        state.displayName = state.deviceId;
    }

    if (deviceId == m_currentDeviceId)
    {
        emit deviceStateChanged();
    }
    emit deviceUpdated(deviceId);
}

void DeviceStateStore::updateAck(const AckMessage &ack)
{
    QString summary;

    if (!ack.detail.isEmpty())
    {
        summary = QStringLiteral("%1 -> %2 (%3)").arg(ack.cmd, ack.result, ack.detail);
    }
    else
    {
        summary = QStringLiteral("%1 -> %2").arg(ack.cmd, ack.result);
    }

    m_lastAckSummary = summary;
    m_lastAckResult = ack.result;
    emit ackChanged();

    if (ack.deviceId.isEmpty() || ack.deviceId == m_currentDeviceId)
    {
        DeviceState &state = ensureState(ack.deviceId.isEmpty() ? m_currentDeviceId : ack.deviceId);
        state.relay1On = ack.relay1On;
        state.relay2On = ack.relay2On;
        if (!ack.timestamp.isEmpty())
        {
            state.timestamp = ack.timestamp;
        }
        state.lastSeenMs = QDateTime::currentMSecsSinceEpoch();
        emit deviceStateChanged();
    }
    emit deviceUpdated(ack.deviceId.isEmpty() ? m_currentDeviceId : ack.deviceId);
}

void DeviceStateStore::updateOnline(const OnlineMessage &onlineMessage)
{
    DeviceState &state = ensureState(onlineMessage.deviceId);

    state.deviceId = onlineMessage.deviceId;
    state.online = onlineMessage.online;
    if (!onlineMessage.timestamp.isEmpty())
    {
        state.timestamp = onlineMessage.timestamp;
    }
    state.lastSeenMs = QDateTime::currentMSecsSinceEpoch();
    if (state.displayName.isEmpty())
    {
        state.displayName = state.deviceId;
    }

    if (onlineMessage.deviceId == m_currentDeviceId)
    {
        emit deviceStateChanged();
    }
    emit deviceUpdated(onlineMessage.deviceId);
}

void DeviceStateStore::updateAlarmCount(const QString &deviceId, int activeAlarmCount)
{
    DeviceState &state = ensureState(deviceId);

    if (state.activeAlarmCount == activeAlarmCount)
    {
        return;
    }

    state.activeAlarmCount = activeAlarmCount;
    if (deviceId == m_currentDeviceId)
    {
        emit deviceStateChanged();
    }
    emit deviceUpdated(deviceId);
}

void DeviceStateStore::hydrateStates(const QVector<DeviceState> &states)
{
    bool changed = false;

    for (const DeviceState &state : states)
    {
        if (state.deviceId.isEmpty())
        {
            continue;
        }

        const bool wasKnown = m_states.contains(state.deviceId);
        m_states.insert(state.deviceId, state);
        changed = true;
        if (!wasKnown)
        {
            emit deviceListChanged();
        }
        emit deviceUpdated(state.deviceId);
    }

    if (m_currentDeviceId.isEmpty() && !states.isEmpty())
    {
        m_currentDeviceId = states.constFirst().deviceId;
        emit currentDeviceIdChanged();
    }

    if (changed)
    {
        emit deviceStateChanged();
    }
}

void DeviceStateStore::noteAckStatus(const QString &summary, const QString &result)
{
    if (m_lastAckSummary == summary && m_lastAckResult == result)
    {
        return;
    }

    m_lastAckSummary = summary;
    m_lastAckResult = result;
    emit ackChanged();
}

void DeviceStateStore::setLastError(const QString &errorText)
{
    if (m_lastError == errorText)
    {
        return;
    }

    m_lastError = errorText;
    emit lastErrorChanged();
}

void DeviceStateStore::applyPredictedCommand(const QString &deviceId,
                                             const QString &command,
                                             int value,
                                             bool hasValue)
{
    const QString resolvedDeviceId = deviceId.isEmpty() ? m_currentDeviceId : deviceId;
    if (resolvedDeviceId.isEmpty())
    {
        return;
    }

    DeviceState &state = ensureState(resolvedDeviceId);
    const bool enabled = hasValue && value != 0;
    const QString pendingText = QStringLiteral("%1 pending").arg(command);

    if (command == QStringLiteral("set_r1") && hasValue)
    {
        state.relay1On = enabled;
        state.padLeftState = enabled ? QStringLiteral("open") : QStringLiteral("closed");
    }
    else if (command == QStringLiteral("set_r2") && hasValue)
    {
        state.relay2On = enabled;
        state.padRightState = enabled ? QStringLiteral("open") : QStringLiteral("closed");
    }
    else if (command == QStringLiteral("set_all") && hasValue)
    {
        state.relay1On = enabled;
        state.relay2On = enabled;
        state.padLeftState = enabled ? QStringLiteral("open") : QStringLiteral("closed");
        state.padRightState = enabled ? QStringLiteral("open") : QStringLiteral("closed");
    }
    else if (command == QStringLiteral("pad_open"))
    {
        state.relay1On = true;
        state.relay2On = true;
        state.padLeftState = QStringLiteral("opening");
        state.padRightState = QStringLiteral("opening");
    }
    else if (command == QStringLiteral("pad_close"))
    {
        state.relay1On = false;
        state.relay2On = false;
        state.padLeftState = QStringLiteral("closing");
        state.padRightState = QStringLiteral("closing");
    }
    else if (command == QStringLiteral("pad_stop"))
    {
        state.padLeftState = QStringLiteral("stopped");
        state.padRightState = QStringLiteral("stopped");
    }
    else if (command != QStringLiteral("query_status") && command != QStringLiteral("query_pad_status"))
    {
        return;
    }

    state.stateText = pendingText;
    state.timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    state.lastSeenMs = QDateTime::currentMSecsSinceEpoch();

    if (resolvedDeviceId == m_currentDeviceId)
    {
        emit deviceStateChanged();
    }
    emit deviceUpdated(resolvedDeviceId);
}

void DeviceStateStore::clearLastError()
{
    setLastError(QString());
}

DeviceState &DeviceStateStore::ensureState(const QString &deviceId)
{
    const QString key = deviceId.isEmpty() ? m_currentDeviceId : deviceId;
    const bool isNewDevice = !m_states.contains(key);
    DeviceState &state = m_states[key];
    if (state.deviceId.isEmpty())
    {
        state.deviceId = key;
    }
    if (state.displayName.isEmpty())
    {
        state.displayName = state.deviceId;
    }
    if (isNewDevice)
    {
        emit deviceListChanged();
        if (m_currentDeviceId.isEmpty())
        {
            m_currentDeviceId = key;
            emit currentDeviceIdChanged();
        }
    }
    return state;
}

const DeviceState *DeviceStateStore::currentState() const
{
    const auto it = m_states.constFind(m_currentDeviceId);
    return (it != m_states.cend()) ? &it.value() : nullptr;
}
