#pragma once

#include <QHash>
#include <QObject>
#include <QJsonObject>

#include "models/DeviceModels.h"

class DeviceStateStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentDeviceId READ currentDeviceId WRITE setCurrentDeviceId NOTIFY currentDeviceIdChanged)
    Q_PROPERTY(int deviceCount READ deviceCount NOTIFY deviceListChanged)
    Q_PROPERTY(bool online READ online NOTIFY deviceStateChanged)
    Q_PROPERTY(QString protocolProfile READ protocolProfile NOTIFY deviceStateChanged)
    Q_PROPERTY(bool relay1On READ relay1On NOTIFY deviceStateChanged)
    Q_PROPERTY(bool relay2On READ relay2On NOTIFY deviceStateChanged)
    Q_PROPERTY(QString padLeftState READ padLeftState NOTIFY deviceStateChanged)
    Q_PROPERTY(QString padRightState READ padRightState NOTIFY deviceStateChanged)
    Q_PROPERTY(bool padReady READ padReady NOTIFY deviceStateChanged)
    Q_PROPERTY(bool padOccupied READ padOccupied NOTIFY deviceStateChanged)
    Q_PROPERTY(QString padMode READ padMode NOTIFY deviceStateChanged)
    Q_PROPERTY(int rssi READ rssi NOTIFY deviceStateChanged)
    Q_PROPERTY(QString operatorName READ operatorName NOTIFY deviceStateChanged)
    Q_PROPERTY(QString ip READ ip NOTIFY deviceStateChanged)
    Q_PROPERTY(QString stateText READ stateText NOTIFY deviceStateChanged)
    Q_PROPERTY(qint64 tick READ tick NOTIFY deviceStateChanged)
    Q_PROPERTY(double windSpeed READ windSpeed NOTIFY deviceStateChanged)
    Q_PROPERTY(double windDirection READ windDirection NOTIFY deviceStateChanged)
    Q_PROPERTY(int windSpeedRaw READ windSpeedRaw NOTIFY deviceStateChanged)
    Q_PROPERTY(int windDirectionRaw READ windDirectionRaw NOTIFY deviceStateChanged)
    Q_PROPERTY(int rainAdcRaw READ rainAdcRaw NOTIFY deviceStateChanged)
    Q_PROPERTY(QString windDirectionText READ windDirectionText NOTIFY deviceStateChanged)
    Q_PROPERTY(QString rainLevelText READ rainLevelText NOTIFY deviceStateChanged)
    Q_PROPERTY(double temperature READ temperature NOTIFY deviceStateChanged)
    Q_PROPERTY(double humidity READ humidity NOTIFY deviceStateChanged)
    Q_PROPERTY(double pressure READ pressure NOTIFY deviceStateChanged)
    Q_PROPERTY(double visibility READ visibility NOTIFY deviceStateChanged)
    Q_PROPERTY(bool rainDetected READ rainDetected NOTIFY deviceStateChanged)
    Q_PROPERTY(double rainValue READ rainValue NOTIFY deviceStateChanged)
    Q_PROPERTY(double pm25 READ pm25 NOTIFY deviceStateChanged)
    Q_PROPERTY(double pm10 READ pm10 NOTIFY deviceStateChanged)
    Q_PROPERTY(double co2 READ co2 NOTIFY deviceStateChanged)
    Q_PROPERTY(double tvoc READ tvoc NOTIFY deviceStateChanged)
    Q_PROPERTY(double ch2o READ ch2o NOTIFY deviceStateChanged)
    Q_PROPERTY(QString timestamp READ timestamp NOTIFY deviceStateChanged)
    Q_PROPERTY(int activeAlarmCount READ activeAlarmCount NOTIFY deviceStateChanged)
    Q_PROPERTY(QString lastAckSummary READ lastAckSummary NOTIFY ackChanged)
    Q_PROPERTY(QString lastAckResult READ lastAckResult NOTIFY ackChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit DeviceStateStore(QObject *parent = nullptr);

    QString currentDeviceId() const;
    void setCurrentDeviceId(const QString &deviceId);
    int deviceCount() const;

    bool online() const;
    QString protocolProfile() const;
    bool relay1On() const;
    bool relay2On() const;
    QString padLeftState() const;
    QString padRightState() const;
    bool padReady() const;
    bool padOccupied() const;
    QString padMode() const;
    int rssi() const;
    QString operatorName() const;
    QString ip() const;
    QString stateText() const;
    qint64 tick() const;
    double windSpeed() const;
    double windDirection() const;
    int windSpeedRaw() const;
    int windDirectionRaw() const;
    int rainAdcRaw() const;
    QString windDirectionText() const;
    QString rainLevelText() const;
    double temperature() const;
    double humidity() const;
    double pressure() const;
    double visibility() const;
    bool rainDetected() const;
    double rainValue() const;
    double pm25() const;
    double pm10() const;
    double co2() const;
    double tvoc() const;
    double ch2o() const;
    QString timestamp() const;
    int activeAlarmCount() const;
    QString lastAckSummary() const;
    QString lastAckResult() const;
    QString lastError() const;
    QVector<DeviceState> allStates() const;
    DeviceState stateForDevice(const QString &deviceId) const;

    void updateStatus(const QJsonObject &json);
    void updateAck(const AckMessage &ack);
    void updateOnline(const OnlineMessage &onlineMessage);
    void updateAlarmCount(const QString &deviceId, int activeAlarmCount);
    void hydrateStates(const QVector<DeviceState> &states);
    void noteAckStatus(const QString &summary, const QString &result);
    void setLastError(const QString &errorText);

    Q_INVOKABLE void clearLastError();

signals:
    void currentDeviceIdChanged();
    void deviceListChanged();
    void deviceStateChanged();
    void deviceUpdated(const QString &deviceId);
    void ackChanged();
    void lastErrorChanged();

private:
    DeviceState &ensureState(const QString &deviceId);
    const DeviceState *currentState() const;

    QString m_currentDeviceId;
    QHash<QString, DeviceState> m_states;
    QString m_lastAckSummary;
    QString m_lastAckResult;
    QString m_lastError;
};
