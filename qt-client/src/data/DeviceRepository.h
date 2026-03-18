#pragma once

#include <QAbstractListModel>
#include <QVector>

#include "models/DeviceModels.h"

class DeviceRepository : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString currentDeviceId READ currentDeviceId WRITE setCurrentDeviceId NOTIFY currentDeviceIdChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentDeviceIdChanged)

public:
    enum DeviceRoles
    {
        DeviceIdRole = Qt::UserRole + 1,
        DisplayNameRole,
        OnlineRole,
        ProtocolProfileRole,
        Relay1OnRole,
        Relay2OnRole,
        PadLeftStateRole,
        PadRightStateRole,
        PadReadyRole,
        PadOccupiedRole,
        PadModeRole,
        RssiRole,
        OperatorRole,
        IpRole,
        StateTextRole,
        TickRole,
        WindSpeedRole,
        WindDirectionRole,
        TemperatureRole,
        HumidityRole,
        PressureRole,
        VisibilityRole,
        TimestampRole,
        AlarmCountRole,
        SelectedRole
    };
    Q_ENUM(DeviceRoles)

    explicit DeviceRepository(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const;

    QString currentDeviceId() const;
    void setCurrentDeviceId(const QString &deviceId);
    int currentIndex() const;

    void loadStates(const QVector<DeviceState> &states);
    void upsertState(const DeviceState &state);
    void setActiveAlarmCounts(const QHash<QString, int> &counts);
    QVector<DeviceState> states() const;

    Q_INVOKABLE void selectIndex(int index);
    Q_INVOKABLE void selectDevice(const QString &deviceId);

signals:
    void countChanged();
    void currentDeviceIdChanged();

private:
    int indexOfDevice(const QString &deviceId) const;

    QVector<DeviceState> m_items;
    QString m_currentDeviceId;
};
