#include "SQLiteCache.h"

#include <QDateTime>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

namespace
{
QString buildIsoTimestamp()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

QString nonNullText(const QString &value, const QString &fallback = QStringLiteral(""))
{
    return value.isNull() ? fallback : value;
}

QJsonObject stateToJson(const DeviceState &state)
{
    QJsonObject json;

    json.insert(QStringLiteral("device_id"), state.deviceId);
    json.insert(QStringLiteral("display_name"), state.displayName);
    json.insert(QStringLiteral("protocol_profile"), state.protocolProfile);
    json.insert(QStringLiteral("online"), state.online);
    json.insert(QStringLiteral("relay1"), state.relay1On);
    json.insert(QStringLiteral("relay2"), state.relay2On);
    json.insert(QStringLiteral("pad_left_state"), state.padLeftState);
    json.insert(QStringLiteral("pad_right_state"), state.padRightState);
    json.insert(QStringLiteral("pad_ready"), state.padReady);
    json.insert(QStringLiteral("pad_occupied"), state.padOccupied);
    json.insert(QStringLiteral("pad_mode"), state.padMode);
    json.insert(QStringLiteral("rssi"), state.rssi);
    json.insert(QStringLiteral("operator"), state.operatorName);
    json.insert(QStringLiteral("ip"), state.ip);
    json.insert(QStringLiteral("state"), state.stateText);
    json.insert(QStringLiteral("tick"), static_cast<double>(state.tick));
    json.insert(QStringLiteral("weather_wind_speed"), state.windSpeed);
    json.insert(QStringLiteral("weather_wind_direction"), state.windDirection);
    json.insert(QStringLiteral("weather_wind_speed_raw"), state.windSpeedRaw);
    json.insert(QStringLiteral("weather_wind_direction_raw"), state.windDirectionRaw);
    json.insert(QStringLiteral("weather_rain_adc_raw"), state.rainAdcRaw);
    json.insert(QStringLiteral("weather_wind_direction_text"), state.windDirectionText);
    json.insert(QStringLiteral("weather_rain_level_text"), state.rainLevelText);
    json.insert(QStringLiteral("weather_temperature"), state.temperature);
    json.insert(QStringLiteral("weather_humidity"), state.humidity);
    json.insert(QStringLiteral("weather_pressure"), state.pressure);
    json.insert(QStringLiteral("weather_visibility"), state.visibility);
    json.insert(QStringLiteral("weather_capability_wind"), state.windCapability);
    json.insert(QStringLiteral("weather_capability_air"), state.airCapability);
    json.insert(QStringLiteral("weather_capability_rain"), state.rainCapability);
    json.insert(QStringLiteral("weather_capability_pressure"), state.pressureCapability);
    json.insert(QStringLiteral("weather_capability_visibility"), state.visibilityCapability);
    json.insert(QStringLiteral("weather_rain_detected"), state.rainDetected);
    json.insert(QStringLiteral("weather_rain_value"), state.rainValue);
    json.insert(QStringLiteral("weather_pm25"), state.pm25);
    json.insert(QStringLiteral("weather_pm10"), state.pm10);
    json.insert(QStringLiteral("weather_co2"), state.co2);
    json.insert(QStringLiteral("weather_tvoc"), state.tvoc);
    json.insert(QStringLiteral("weather_ch2o"), state.ch2o);
    json.insert(QStringLiteral("timestamp"), state.timestamp);
    json.insert(QStringLiteral("last_seen_ms"), static_cast<double>(state.lastSeenMs));
    json.insert(QStringLiteral("active_alarm_count"), state.activeAlarmCount);
    return json;
}

bool tableHasColumn(const QSqlDatabase &database, const QString &tableName, const QString &columnName)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(tableName)))
    {
        return false;
    }

    while (query.next())
    {
        if (query.value(1).toString() == columnName)
        {
            return true;
        }
    }

    return false;
}
}

SQLiteCache::SQLiteCache(QObject *parent)
    : QObject(parent)
{
    const QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataPath);
    m_databasePath = QDir(dataPath).filePath(QStringLiteral("relay_platform.db"));
}

SQLiteCache::~SQLiteCache()
{
    const QString name = connectionName();
    if (m_database.isValid())
    {
        m_database.close();
    }
    QSqlDatabase::removeDatabase(name);
}

bool SQLiteCache::initialize()
{
    const QStringList statements{
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS devices_cache ("
            "device_id TEXT PRIMARY KEY,"
            "display_name TEXT,"
            "protocol_profile TEXT DEFAULT 'relay_v1',"
            "operator_name TEXT,"
            "ip TEXT,"
            "last_seen TEXT,"
            "last_seen_ms INTEGER DEFAULT 0,"
            "rssi INTEGER DEFAULT 0,"
            "relay1 INTEGER DEFAULT 0,"
            "relay2 INTEGER DEFAULT 0,"
            "pad_left_state TEXT DEFAULT 'closed',"
            "pad_right_state TEXT DEFAULT 'closed',"
            "pad_ready INTEGER DEFAULT 0,"
            "pad_occupied INTEGER DEFAULT 0,"
            "pad_mode TEXT DEFAULT 'auto',"
            "online INTEGER DEFAULT 0,"
            "state_text TEXT,"
            "tick INTEGER DEFAULT 0,"
            "wind_speed REAL DEFAULT 0,"
            "wind_direction REAL DEFAULT 0,"
            "wind_speed_raw INTEGER DEFAULT 0,"
            "wind_direction_raw INTEGER DEFAULT 0,"
            "rain_adc_raw INTEGER DEFAULT 0,"
            "wind_direction_text TEXT DEFAULT '',"
            "rain_level_text TEXT DEFAULT '',"
            "temperature REAL DEFAULT 0,"
            "humidity REAL DEFAULT 0,"
            "pressure REAL DEFAULT 0,"
            "visibility REAL DEFAULT 0,"
            "capability_wind INTEGER DEFAULT 1,"
            "capability_air INTEGER DEFAULT 1,"
            "capability_rain INTEGER DEFAULT 1,"
            "capability_pressure INTEGER DEFAULT 0,"
            "capability_visibility INTEGER DEFAULT 0,"
            "rain_detected INTEGER DEFAULT 0,"
            "rain_value REAL DEFAULT 0,"
            "pm25 REAL DEFAULT 0,"
            "pm10 REAL DEFAULT 0,"
            "co2 REAL DEFAULT 0,"
            "tvoc REAL DEFAULT 0,"
            "ch2o REAL DEFAULT 0,"
            "timestamp TEXT,"
            "active_alarm_count INTEGER DEFAULT 0)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS last_state_cache ("
            "device_id TEXT PRIMARY KEY,"
            "payload TEXT NOT NULL,"
            "updated_at TEXT NOT NULL)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS message_cache ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "device_id TEXT,"
            "direction TEXT,"
            "channel TEXT,"
            "topic TEXT,"
            "command TEXT,"
            "payload TEXT,"
            "result TEXT,"
            "operator_name TEXT,"
            "level TEXT,"
            "created_at TEXT NOT NULL)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS command_outbox ("
            "msg_id TEXT PRIMARY KEY,"
            "device_id TEXT,"
            "command TEXT,"
            "payload TEXT,"
            "status TEXT,"
            "detail TEXT,"
            "created_at TEXT NOT NULL,"
            "updated_at TEXT NOT NULL)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS alarm_cache ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "alarm_key TEXT UNIQUE NOT NULL,"
            "device_id TEXT,"
            "code TEXT,"
            "severity TEXT,"
            "message TEXT,"
            "source TEXT,"
            "active INTEGER DEFAULT 1,"
            "created_at TEXT NOT NULL,"
            "resolved_at TEXT)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS app_settings ("
            "key TEXT PRIMARY KEY,"
            "value TEXT NOT NULL)")
    };

    if (!openIfNeeded())
    {
        return false;
    }

    for (const QString &statement : statements)
    {
        if (!execSchemaStatement(statement))
        {
            return false;
        }
    }

    const auto ensureColumn = [this](const QString &tableName, const QString &columnName, const QString &definition) {
        if (tableHasColumn(m_database, tableName, columnName))
        {
            return true;
        }
        return execSchemaStatement(
            QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3").arg(tableName, columnName, definition));
    };

    if (!ensureColumn(QStringLiteral("devices_cache"), QStringLiteral("protocol_profile"), QStringLiteral("TEXT NOT NULL DEFAULT 'relay_v1'")) ||
        !ensureColumn(QStringLiteral("devices_cache"), QStringLiteral("pad_left_state"), QStringLiteral("TEXT NOT NULL DEFAULT 'closed'")) ||
        !ensureColumn(QStringLiteral("devices_cache"), QStringLiteral("pad_right_state"), QStringLiteral("TEXT NOT NULL DEFAULT 'closed'")) ||
        !ensureColumn(QStringLiteral("devices_cache"), QStringLiteral("pad_ready"), QStringLiteral("INTEGER NOT NULL DEFAULT 0")) ||
        !ensureColumn(QStringLiteral("devices_cache"), QStringLiteral("pad_occupied"), QStringLiteral("INTEGER NOT NULL DEFAULT 0")) ||
        !ensureColumn(QStringLiteral("devices_cache"), QStringLiteral("pad_mode"), QStringLiteral("TEXT NOT NULL DEFAULT 'auto'")) ||
        !ensureColumn(QStringLiteral("devices_cache"), QStringLiteral("wind_speed"), QStringLiteral("REAL NOT NULL DEFAULT 0")) ||
        !ensureColumn(QStringLiteral("devices_cache"), QStringLiteral("wind_direction"), QStringLiteral("REAL NOT NULL DEFAULT 0")) ||
        !ensureColumn(QStringLiteral("devices_cache"), QStringLiteral("wind_speed_raw"), QStringLiteral("INTEGER NOT NULL DEFAULT 0")) ||
        !ensureColumn(QStringLiteral("devices_cache"), QStringLiteral("wind_direction_raw"), QStringLiteral("INTEGER NOT NULL DEFAULT 0")) ||
        !ensureColumn(QStringLiteral("devices_cache"), QStringLiteral("rain_adc_raw"), QStringLiteral("INTEGER NOT NULL DEFAULT 0")) ||
        !ensureColumn(QStringLiteral("devices_cache"), QStringLiteral("wind_direction_text"), QStringLiteral("TEXT NOT NULL DEFAULT ''")) ||
        !ensureColumn(QStringLiteral("devices_cache"), QStringLiteral("rain_level_text"), QStringLiteral("TEXT NOT NULL DEFAULT ''")) ||
        !ensureColumn(QStringLiteral("devices_cache"), QStringLiteral("temperature"), QStringLiteral("REAL NOT NULL DEFAULT 0")) ||
        !ensureColumn(QStringLiteral("devices_cache"), QStringLiteral("humidity"), QStringLiteral("REAL NOT NULL DEFAULT 0")) ||
        !ensureColumn(QStringLiteral("devices_cache"), QStringLiteral("pressure"), QStringLiteral("REAL NOT NULL DEFAULT 0")) ||
        !ensureColumn(QStringLiteral("devices_cache"), QStringLiteral("visibility"), QStringLiteral("REAL NOT NULL DEFAULT 0")) ||
        !ensureColumn(QStringLiteral("devices_cache"), QStringLiteral("capability_wind"), QStringLiteral("INTEGER NOT NULL DEFAULT 1")) ||
        !ensureColumn(QStringLiteral("devices_cache"), QStringLiteral("capability_air"), QStringLiteral("INTEGER NOT NULL DEFAULT 1")) ||
        !ensureColumn(QStringLiteral("devices_cache"), QStringLiteral("capability_rain"), QStringLiteral("INTEGER NOT NULL DEFAULT 1")) ||
        !ensureColumn(QStringLiteral("devices_cache"), QStringLiteral("capability_pressure"), QStringLiteral("INTEGER NOT NULL DEFAULT 0")) ||
        !ensureColumn(QStringLiteral("devices_cache"), QStringLiteral("capability_visibility"), QStringLiteral("INTEGER NOT NULL DEFAULT 0")) ||
        !ensureColumn(QStringLiteral("devices_cache"), QStringLiteral("rain_detected"), QStringLiteral("INTEGER NOT NULL DEFAULT 0")) ||
        !ensureColumn(QStringLiteral("devices_cache"), QStringLiteral("rain_value"), QStringLiteral("REAL NOT NULL DEFAULT 0")) ||
        !ensureColumn(QStringLiteral("devices_cache"), QStringLiteral("pm25"), QStringLiteral("REAL NOT NULL DEFAULT 0")) ||
        !ensureColumn(QStringLiteral("devices_cache"), QStringLiteral("pm10"), QStringLiteral("REAL NOT NULL DEFAULT 0")) ||
        !ensureColumn(QStringLiteral("devices_cache"), QStringLiteral("co2"), QStringLiteral("REAL NOT NULL DEFAULT 0")) ||
        !ensureColumn(QStringLiteral("devices_cache"), QStringLiteral("tvoc"), QStringLiteral("REAL NOT NULL DEFAULT 0")) ||
        !ensureColumn(QStringLiteral("devices_cache"), QStringLiteral("ch2o"), QStringLiteral("REAL NOT NULL DEFAULT 0")))
    {
        return false;
    }

    return true;
}

QString SQLiteCache::databasePath() const
{
    return m_databasePath;
}

QString SQLiteCache::lastError() const
{
    return m_lastError;
}

QString SQLiteCache::setting(const QString &key, const QString &defaultValue) const
{
    if (!openIfNeeded())
    {
        return defaultValue;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT value FROM app_settings WHERE key = ?"));
    query.addBindValue(key);
    if (!query.exec())
    {
        return defaultValue;
    }

    return query.next() ? query.value(0).toString() : defaultValue;
}

void SQLiteCache::setSetting(const QString &key, const QString &value)
{
    if (!openIfNeeded())
    {
        return;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "INSERT INTO app_settings(key, value) VALUES(?, ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
    query.addBindValue(key);
    query.addBindValue(value);
    if (!query.exec())
    {
        setLastError(query.lastError().text());
    }
}

void SQLiteCache::removeSetting(const QString &key)
{
    if (!openIfNeeded())
    {
        return;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("DELETE FROM app_settings WHERE key = ?"));
    query.addBindValue(key);
    if (!query.exec())
    {
        setLastError(query.lastError().text());
    }
}

void SQLiteCache::upsertDeviceState(const DeviceState &state)
{
    if (!openIfNeeded() || state.deviceId.isEmpty())
    {
        return;
    }

    const QString now = buildIsoTimestamp();
    QSqlQuery cacheQuery(m_database);
    cacheQuery.prepare(QStringLiteral(
        "INSERT INTO devices_cache(device_id, display_name, protocol_profile, operator_name, ip, last_seen, last_seen_ms, rssi, "
        "relay1, relay2, pad_left_state, pad_right_state, pad_ready, pad_occupied, pad_mode, online, state_text, tick, "
        "wind_speed, wind_direction, wind_speed_raw, wind_direction_raw, rain_adc_raw, wind_direction_text, rain_level_text, "
        "temperature, humidity, pressure, visibility, capability_wind, capability_air, capability_rain, capability_pressure, capability_visibility, "
        "rain_detected, rain_value, pm25, pm10, co2, tvoc, ch2o, timestamp, active_alarm_count) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(device_id) DO UPDATE SET "
        "display_name = excluded.display_name, "
        "protocol_profile = excluded.protocol_profile, "
        "operator_name = excluded.operator_name, "
        "ip = excluded.ip, "
        "last_seen = excluded.last_seen, "
        "last_seen_ms = excluded.last_seen_ms, "
        "rssi = excluded.rssi, "
        "relay1 = excluded.relay1, "
        "relay2 = excluded.relay2, "
        "pad_left_state = excluded.pad_left_state, "
        "pad_right_state = excluded.pad_right_state, "
        "pad_ready = excluded.pad_ready, "
        "pad_occupied = excluded.pad_occupied, "
        "pad_mode = excluded.pad_mode, "
        "online = excluded.online, "
        "state_text = excluded.state_text, "
        "tick = excluded.tick, "
        "wind_speed = excluded.wind_speed, "
        "wind_direction = excluded.wind_direction, "
        "wind_speed_raw = excluded.wind_speed_raw, "
        "wind_direction_raw = excluded.wind_direction_raw, "
        "rain_adc_raw = excluded.rain_adc_raw, "
        "wind_direction_text = excluded.wind_direction_text, "
        "rain_level_text = excluded.rain_level_text, "
        "temperature = excluded.temperature, "
        "humidity = excluded.humidity, "
        "pressure = excluded.pressure, "
        "visibility = excluded.visibility, "
        "capability_wind = excluded.capability_wind, "
        "capability_air = excluded.capability_air, "
        "capability_rain = excluded.capability_rain, "
        "capability_pressure = excluded.capability_pressure, "
        "capability_visibility = excluded.capability_visibility, "
        "rain_detected = excluded.rain_detected, "
        "rain_value = excluded.rain_value, "
        "pm25 = excluded.pm25, "
        "pm10 = excluded.pm10, "
        "co2 = excluded.co2, "
        "tvoc = excluded.tvoc, "
        "ch2o = excluded.ch2o, "
        "timestamp = excluded.timestamp, "
        "active_alarm_count = excluded.active_alarm_count"));
    cacheQuery.addBindValue(nonNullText(state.deviceId));
    cacheQuery.addBindValue(nonNullText(state.displayName.isEmpty() ? state.deviceId : state.displayName, state.deviceId));
    cacheQuery.addBindValue(nonNullText(state.protocolProfile, QStringLiteral("relay_v1")));
    cacheQuery.addBindValue(nonNullText(state.operatorName));
    cacheQuery.addBindValue(nonNullText(state.ip));
    cacheQuery.addBindValue(nonNullText(state.timestamp.isEmpty() ? now : state.timestamp, now));
    cacheQuery.addBindValue(state.lastSeenMs);
    cacheQuery.addBindValue(state.rssi);
    cacheQuery.addBindValue(state.relay1On ? 1 : 0);
    cacheQuery.addBindValue(state.relay2On ? 1 : 0);
    cacheQuery.addBindValue(nonNullText(state.padLeftState, QStringLiteral("closed")));
    cacheQuery.addBindValue(nonNullText(state.padRightState, QStringLiteral("closed")));
    cacheQuery.addBindValue(state.padReady ? 1 : 0);
    cacheQuery.addBindValue(state.padOccupied ? 1 : 0);
    cacheQuery.addBindValue(nonNullText(state.padMode, QStringLiteral("auto")));
    cacheQuery.addBindValue(state.online ? 1 : 0);
    cacheQuery.addBindValue(nonNullText(state.stateText));
    cacheQuery.addBindValue(state.tick);
    cacheQuery.addBindValue(state.windSpeed);
    cacheQuery.addBindValue(state.windDirection);
    cacheQuery.addBindValue(state.windSpeedRaw);
    cacheQuery.addBindValue(state.windDirectionRaw);
    cacheQuery.addBindValue(state.rainAdcRaw);
    cacheQuery.addBindValue(nonNullText(state.windDirectionText));
    cacheQuery.addBindValue(nonNullText(state.rainLevelText));
    cacheQuery.addBindValue(state.temperature);
    cacheQuery.addBindValue(state.humidity);
    cacheQuery.addBindValue(state.pressure);
    cacheQuery.addBindValue(state.visibility);
    cacheQuery.addBindValue(state.windCapability ? 1 : 0);
    cacheQuery.addBindValue(state.airCapability ? 1 : 0);
    cacheQuery.addBindValue(state.rainCapability ? 1 : 0);
    cacheQuery.addBindValue(state.pressureCapability ? 1 : 0);
    cacheQuery.addBindValue(state.visibilityCapability ? 1 : 0);
    cacheQuery.addBindValue(state.rainDetected ? 1 : 0);
    cacheQuery.addBindValue(state.rainValue);
    cacheQuery.addBindValue(state.pm25);
    cacheQuery.addBindValue(state.pm10);
    cacheQuery.addBindValue(state.co2);
    cacheQuery.addBindValue(state.tvoc);
    cacheQuery.addBindValue(state.ch2o);
    cacheQuery.addBindValue(nonNullText(state.timestamp));
    cacheQuery.addBindValue(state.activeAlarmCount);

    if (!cacheQuery.exec())
    {
        setLastError(cacheQuery.lastError().text());
        return;
    }

    QSqlQuery stateQuery(m_database);
    stateQuery.prepare(QStringLiteral(
        "INSERT INTO last_state_cache(device_id, payload, updated_at) VALUES(?, ?, ?) "
        "ON CONFLICT(device_id) DO UPDATE SET payload = excluded.payload, updated_at = excluded.updated_at"));
    stateQuery.addBindValue(state.deviceId);
    stateQuery.addBindValue(QString::fromUtf8(QJsonDocument(stateToJson(state)).toJson(QJsonDocument::Compact)));
    stateQuery.addBindValue(now);
    if (!stateQuery.exec())
    {
        setLastError(stateQuery.lastError().text());
    }
}

QVector<DeviceState> SQLiteCache::loadDeviceStates() const
{
    QVector<DeviceState> states;

    if (!openIfNeeded())
    {
        return states;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT device_id, display_name, protocol_profile, operator_name, ip, rssi, relay1, relay2, "
        "pad_left_state, pad_right_state, pad_ready, pad_occupied, pad_mode, online, state_text, tick, "
        "wind_speed, wind_direction, wind_speed_raw, wind_direction_raw, rain_adc_raw, wind_direction_text, rain_level_text, "
        "temperature, humidity, pressure, visibility, capability_wind, capability_air, capability_rain, capability_pressure, capability_visibility, "
        "rain_detected, rain_value, pm25, pm10, co2, tvoc, ch2o, timestamp, last_seen_ms, active_alarm_count "
        "FROM devices_cache ORDER BY last_seen_ms DESC, device_id ASC"));
    if (!query.exec())
    {
        return states;
    }

    while (query.next())
    {
        DeviceState state;
        state.deviceId = query.value(0).toString();
        state.displayName = query.value(1).toString();
        state.protocolProfile = query.value(2).toString();
        state.operatorName = query.value(3).toString();
        state.ip = query.value(4).toString();
        state.rssi = query.value(5).toInt();
        state.relay1On = query.value(6).toInt() != 0;
        state.relay2On = query.value(7).toInt() != 0;
        state.padLeftState = query.value(8).toString();
        state.padRightState = query.value(9).toString();
        state.padReady = query.value(10).toInt() != 0;
        state.padOccupied = query.value(11).toInt() != 0;
        state.padMode = query.value(12).toString();
        state.online = query.value(13).toInt() != 0;
        state.stateText = query.value(14).toString();
        state.tick = query.value(15).toLongLong();
        state.windSpeed = query.value(16).toDouble();
        state.windDirection = query.value(17).toDouble();
        state.windSpeedRaw = query.value(18).toInt();
        state.windDirectionRaw = query.value(19).toInt();
        state.rainAdcRaw = query.value(20).toInt();
        state.windDirectionText = query.value(21).toString();
        state.rainLevelText = query.value(22).toString();
        state.temperature = query.value(23).toDouble();
        state.humidity = query.value(24).toDouble();
        state.pressure = query.value(25).toDouble();
        state.visibility = query.value(26).toDouble();
        state.windCapability = query.value(27).toInt() != 0;
        state.airCapability = query.value(28).toInt() != 0;
        state.rainCapability = query.value(29).toInt() != 0;
        state.pressureCapability = query.value(30).toInt() != 0;
        state.visibilityCapability = query.value(31).toInt() != 0;
        state.rainDetected = query.value(32).toInt() != 0;
        state.rainValue = query.value(33).toDouble();
        state.pm25 = query.value(34).toDouble();
        state.pm10 = query.value(35).toDouble();
        state.co2 = query.value(36).toDouble();
        state.tvoc = query.value(37).toDouble();
        state.ch2o = query.value(38).toDouble();
        state.timestamp = query.value(39).toString();
        state.lastSeenMs = query.value(40).toLongLong();
        state.activeAlarmCount = query.value(41).toInt();
        states.push_back(state);
    }

    return states;
}

void SQLiteCache::insertMessage(const MessageRecord &record)
{
    if (!openIfNeeded())
    {
        return;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "INSERT INTO message_cache(device_id, direction, channel, topic, command, payload, result, "
        "operator_name, level, created_at) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(record.deviceId);
    query.addBindValue(record.direction);
    query.addBindValue(record.channel);
    query.addBindValue(record.topic);
    query.addBindValue(record.command);
    query.addBindValue(record.payload);
    query.addBindValue(record.result);
    query.addBindValue(record.operatorName);
    query.addBindValue(record.level);
    query.addBindValue(record.createdAt.isEmpty() ? buildIsoTimestamp() : record.createdAt);
    if (!query.exec())
    {
        setLastError(query.lastError().text());
        return;
    }

    pruneMessages(record.deviceId);
}

void SQLiteCache::replaceRemoteMessages(const QString &deviceId, const QVector<MessageRecord> &records)
{
    if (!openIfNeeded())
    {
        return;
    }

    QSqlQuery remove(m_database);
    remove.prepare(QStringLiteral("DELETE FROM message_cache WHERE device_id = ? AND level = 'remote'"));
    remove.addBindValue(deviceId);
    if (!remove.exec())
    {
        setLastError(remove.lastError().text());
        return;
    }

    for (const MessageRecord &record : records)
    {
        MessageRecord stored = record;
        stored.level = QStringLiteral("remote");
        insertMessage(stored);
    }
}

QVector<MessageRecord> SQLiteCache::loadMessages(const QString &deviceId, int limit, bool preferRemote) const
{
    QVector<MessageRecord> records;

    if (!openIfNeeded())
    {
        return records;
    }

    auto executeQuery = [this, &records, &deviceId, limit](const QString &statement, const QString &remoteFlag) {
        QSqlQuery query(m_database);
        query.prepare(statement);
        query.addBindValue(deviceId);
        query.addBindValue(deviceId);
        if (!remoteFlag.isEmpty())
        {
            query.addBindValue(remoteFlag);
        }
        query.addBindValue(limit);
        if (!query.exec())
        {
            return false;
        }

        while (query.next())
        {
            MessageRecord record;
            record.id = query.value(0).toLongLong();
            record.deviceId = query.value(1).toString();
            record.direction = query.value(2).toString();
            record.channel = query.value(3).toString();
            record.topic = query.value(4).toString();
            record.command = query.value(5).toString();
            record.payload = query.value(6).toString();
            record.result = query.value(7).toString();
            record.operatorName = query.value(8).toString();
            record.level = query.value(9).toString();
            record.createdAt = query.value(10).toString();
            records.push_back(record);
        }
        return true;
    };

    if (preferRemote)
    {
        executeQuery(
            QStringLiteral(
                "SELECT id, device_id, direction, channel, topic, command, payload, result, operator_name, level, created_at "
                "FROM message_cache WHERE (? = '' OR device_id = ?) AND level = ? "
                "ORDER BY created_at DESC, id DESC LIMIT ?"),
            QStringLiteral("remote"));
        if (!records.isEmpty())
        {
            return records;
        }
    }

    records.clear();
    executeQuery(
        QStringLiteral(
            "SELECT id, device_id, direction, channel, topic, command, payload, result, operator_name, level, created_at "
            "FROM message_cache WHERE (? = '' OR device_id = ?) AND (level IS NULL OR level != ?) "
            "ORDER BY created_at DESC, id DESC LIMIT ?"),
        QStringLiteral("remote"));

    return records;
}

void SQLiteCache::insertOutboxCommand(const QString &msgId,
                                      const QString &deviceId,
                                      const QString &command,
                                      const QString &payload,
                                      const QString &status,
                                      const QString &detail)
{
    if (!openIfNeeded())
    {
        return;
    }

    const QString now = buildIsoTimestamp();
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "INSERT INTO command_outbox(msg_id, device_id, command, payload, status, detail, created_at, updated_at) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(msg_id) DO UPDATE SET status = excluded.status, detail = excluded.detail, updated_at = excluded.updated_at"));
    query.addBindValue(msgId);
    query.addBindValue(deviceId);
    query.addBindValue(command);
    query.addBindValue(payload);
    query.addBindValue(status);
    query.addBindValue(detail);
    query.addBindValue(now);
    query.addBindValue(now);
    if (!query.exec())
    {
        setLastError(query.lastError().text());
    }
}

void SQLiteCache::updateOutboxStatus(const QString &msgId, const QString &status, const QString &detail)
{
    if (!openIfNeeded())
    {
        return;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("UPDATE command_outbox SET status = ?, detail = ?, updated_at = ? WHERE msg_id = ?"));
    query.addBindValue(status);
    query.addBindValue(detail);
    query.addBindValue(buildIsoTimestamp());
    query.addBindValue(msgId);
    if (!query.exec())
    {
        setLastError(query.lastError().text());
    }
}

AlarmRecord SQLiteCache::upsertAlarm(const AlarmRecord &record)
{
    AlarmRecord stored = record;

    if (!openIfNeeded() || record.alarmKey.isEmpty())
    {
        return stored;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "INSERT INTO alarm_cache(alarm_key, device_id, code, severity, message, source, active, created_at, resolved_at) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(alarm_key) DO UPDATE SET "
        "device_id = excluded.device_id, "
        "code = excluded.code, "
        "severity = excluded.severity, "
        "message = excluded.message, "
        "source = excluded.source, "
        "active = excluded.active, "
        "created_at = excluded.created_at, "
        "resolved_at = excluded.resolved_at"));
    query.addBindValue(record.alarmKey);
    query.addBindValue(record.deviceId);
    query.addBindValue(record.code);
    query.addBindValue(record.severity);
    query.addBindValue(record.message);
    query.addBindValue(record.source);
    query.addBindValue(record.active ? 1 : 0);
    query.addBindValue(record.createdAt.isEmpty() ? buildIsoTimestamp() : record.createdAt);
    query.addBindValue(record.active ? QVariant() : QVariant(record.resolvedAt.isEmpty() ? buildIsoTimestamp() : record.resolvedAt));
    if (!query.exec())
    {
        setLastError(query.lastError().text());
        return stored;
    }

    QSqlQuery fetch(m_database);
    fetch.prepare(QStringLiteral(
        "SELECT id, alarm_key, device_id, code, severity, message, source, active, created_at, resolved_at "
        "FROM alarm_cache WHERE alarm_key = ?"));
    fetch.addBindValue(record.alarmKey);
    if (!fetch.exec() || !fetch.next())
    {
        return stored;
    }

    stored.id = fetch.value(0).toLongLong();
    stored.alarmKey = fetch.value(1).toString();
    stored.deviceId = fetch.value(2).toString();
    stored.code = fetch.value(3).toString();
    stored.severity = fetch.value(4).toString();
    stored.message = fetch.value(5).toString();
    stored.source = fetch.value(6).toString();
    stored.active = fetch.value(7).toInt() != 0;
    stored.createdAt = fetch.value(8).toString();
    stored.resolvedAt = fetch.value(9).toString();
    return stored;
}

void SQLiteCache::resolveAlarm(const QString &alarmKey, const QString &resolvedAt)
{
    if (!openIfNeeded() || alarmKey.isEmpty())
    {
        return;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("UPDATE alarm_cache SET active = 0, resolved_at = ? WHERE alarm_key = ?"));
    query.addBindValue(resolvedAt.isEmpty() ? buildIsoTimestamp() : resolvedAt);
    query.addBindValue(alarmKey);
    if (!query.exec())
    {
        setLastError(query.lastError().text());
    }
}

QVector<AlarmRecord> SQLiteCache::loadAlarms(const QString &deviceId, int limit) const
{
    QVector<AlarmRecord> records;

    if (!openIfNeeded())
    {
        return records;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT id, alarm_key, device_id, code, severity, message, source, active, created_at, resolved_at "
        "FROM alarm_cache WHERE (? = '' OR device_id = ?) ORDER BY active DESC, id DESC LIMIT ?"));
    query.addBindValue(deviceId);
    query.addBindValue(deviceId);
    query.addBindValue(limit);
    if (!query.exec())
    {
        return records;
    }

    while (query.next())
    {
        AlarmRecord record;
        record.id = query.value(0).toLongLong();
        record.alarmKey = query.value(1).toString();
        record.deviceId = query.value(2).toString();
        record.code = query.value(3).toString();
        record.severity = query.value(4).toString();
        record.message = query.value(5).toString();
        record.source = query.value(6).toString();
        record.active = query.value(7).toInt() != 0;
        record.createdAt = query.value(8).toString();
        record.resolvedAt = query.value(9).toString();
        records.push_back(record);
    }

    return records;
}

QHash<QString, int> SQLiteCache::loadActiveAlarmCounts() const
{
    QHash<QString, int> counts;

    if (!openIfNeeded())
    {
        return counts;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT device_id, COUNT(*) FROM alarm_cache WHERE active = 1 GROUP BY device_id"));
    if (!query.exec())
    {
        return counts;
    }

    while (query.next())
    {
        counts.insert(query.value(0).toString(), query.value(1).toInt());
    }

    return counts;
}

bool SQLiteCache::openIfNeeded() const
{
    if (m_database.isValid() && m_database.isOpen())
    {
        return true;
    }

    if (!m_database.isValid())
    {
        m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName());
        m_database.setDatabaseName(m_databasePath);
    }

    if (!m_database.open())
    {
        const_cast<SQLiteCache *>(this)->setLastError(m_database.lastError().text());
        return false;
    }

    return true;
}

bool SQLiteCache::execSchemaStatement(const QString &statement) const
{
    QSqlQuery query(m_database);
    if (!query.exec(statement))
    {
        const_cast<SQLiteCache *>(this)->setLastError(query.lastError().text());
        return false;
    }

    return true;
}

void SQLiteCache::setLastError(const QString &message)
{
    if (m_lastError == message)
    {
        return;
    }

    m_lastError = message;
    if (!message.isEmpty() && openIfNeeded())
    {
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral(
            "INSERT INTO app_settings(key, value) VALUES(?, ?) "
            "ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
        query.addBindValue(QStringLiteral("debug.sqlite.lastError"));
        query.addBindValue(message);
        query.exec();
    }
    emit lastErrorChanged();
}

void SQLiteCache::pruneMessages(const QString &deviceId)
{
    if (!openIfNeeded() || deviceId.isEmpty())
    {
        return;
    }

    QSqlQuery aged(m_database);
    aged.prepare(QStringLiteral("DELETE FROM message_cache WHERE created_at < ?"));
    aged.addBindValue(QDateTime::currentDateTimeUtc().addDays(-7).toString(Qt::ISODateWithMs));
    aged.exec();

    QSqlQuery depth(m_database);
    depth.prepare(QStringLiteral(
        "DELETE FROM message_cache "
        "WHERE device_id = ? AND id NOT IN (SELECT id FROM message_cache WHERE device_id = ? ORDER BY id DESC LIMIT 10000)"));
    depth.addBindValue(deviceId);
    depth.addBindValue(deviceId);
    depth.exec();
}

QString SQLiteCache::connectionName() const
{
    return QStringLiteral("relay_client_%1").arg(reinterpret_cast<quintptr>(this));
}
