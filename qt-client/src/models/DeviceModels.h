#pragma once

#include <QtGlobal>
#include <QString>

struct DeviceState
{
    QString deviceId;
    QString displayName;
    bool online = false;
    QString protocolProfile = QStringLiteral("relay_v1");
    bool relay1On = false;
    bool relay2On = false;
    QString padLeftState = QStringLiteral("closed");
    QString padRightState = QStringLiteral("closed");
    bool padReady = false;
    bool padOccupied = false;
    QString padMode = QStringLiteral("auto");
    int rssi = 0;
    QString operatorName;
    QString ip;
    QString stateText;
    qint64 tick = 0;
    double windSpeed = 0.0;
    double windDirection = 0.0;
    int windSpeedRaw = 0;
    int windDirectionRaw = 0;
    int rainAdcRaw = 0;
    QString windDirectionText;
    QString rainLevelText;
    double temperature = 0.0;
    double humidity = 0.0;
    double pressure = 0.0;
    double visibility = 0.0;
    bool windCapability = true;
    bool airCapability = true;
    bool rainCapability = true;
    bool pressureCapability = false;
    bool visibilityCapability = false;
    bool rainDetected = false;
    double rainValue = 0.0;
    double pm25 = 0.0;
    double pm10 = 0.0;
    double co2 = 0.0;
    double tvoc = 0.0;
    double ch2o = 0.0;
    bool windSensorOnline = true;
    bool airSensorOnline = true;
    bool rainSensorOnline = true;
    int sensorFailureCount = 0;
    qint64 sensorLastOkTick = 0;
    QString sensorLastError;
    QString timestamp;
    qint64 lastSeenMs = 0;
    int activeAlarmCount = 0;
};

struct AckMessage
{
    QString msgId;
    QString deviceId;
    QString cmd;
    QString result;
    QString detail;
    bool relay1On = false;
    bool relay2On = false;
    QString timestamp;
};

struct OnlineMessage
{
    QString deviceId;
    bool online = false;
    QString timestamp;
};

struct MessageRecord
{
    qint64 id = 0;
    QString deviceId;
    QString direction;
    QString channel;
    QString topic;
    QString command;
    QString payload;
    QString result;
    QString operatorName;
    QString level;
    QString createdAt;
};

struct AlarmRecord
{
    qint64 id = 0;
    QString alarmKey;
    QString deviceId;
    QString code;
    QString severity;
    QString message;
    QString source;
    bool active = true;
    QString createdAt;
    QString resolvedAt;
};

struct UserSession
{
    QString username;
    QString displayName;
    QString token;
    QString refreshToken;
    QString role;
    QString accessExpiresAt;
    QString refreshExpiresAt;
    bool localFallback = false;
};
