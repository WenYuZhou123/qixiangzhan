#include "DeviceRepository.h"

#include <algorithm>

namespace
{
bool deviceSort(const DeviceState &left, const DeviceState &right)
{
    if (left.online != right.online)
    {
        return left.online;
    }

    if (left.lastSeenMs != right.lastSeenMs)
    {
        return left.lastSeenMs > right.lastSeenMs;
    }

    return left.deviceId < right.deviceId;
}
}

DeviceRepository::DeviceRepository(QObject *parent)
    : QAbstractListModel(parent)
{
}

int DeviceRepository::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant DeviceRepository::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
    {
        return QVariant();
    }

    const DeviceState &item = m_items.at(index.row());
    switch (role)
    {
    case DeviceIdRole:
        return item.deviceId;
    case DisplayNameRole:
        return item.displayName.isEmpty() ? item.deviceId : item.displayName;
    case OnlineRole:
        return item.online;
    case ProtocolProfileRole:
        return item.protocolProfile;
    case Relay1OnRole:
        return item.relay1On;
    case Relay2OnRole:
        return item.relay2On;
    case PadLeftStateRole:
        return item.padLeftState;
    case PadRightStateRole:
        return item.padRightState;
    case PadReadyRole:
        return item.padReady;
    case PadOccupiedRole:
        return item.padOccupied;
    case PadModeRole:
        return item.padMode;
    case RssiRole:
        return item.rssi;
    case OperatorRole:
        return item.operatorName;
    case IpRole:
        return item.ip;
    case StateTextRole:
        return item.stateText;
    case TickRole:
        return item.tick;
    case WindSpeedRole:
        return item.windSpeed;
    case WindDirectionRole:
        return item.windDirection;
    case TemperatureRole:
        return item.temperature;
    case HumidityRole:
        return item.humidity;
    case PressureRole:
        return item.pressure;
    case VisibilityRole:
        return item.visibility;
    case WindCapabilityRole:
        return item.windCapability;
    case AirCapabilityRole:
        return item.airCapability;
    case RainCapabilityRole:
        return item.rainCapability;
    case PressureCapabilityRole:
        return item.pressureCapability;
    case VisibilityCapabilityRole:
        return item.visibilityCapability;
    case RainDetectedRole:
        return item.rainDetected;
    case RainValueRole:
        return item.rainValue;
    case Pm25Role:
        return item.pm25;
    case Pm10Role:
        return item.pm10;
    case Co2Role:
        return item.co2;
    case TvocRole:
        return item.tvoc;
    case Ch2oRole:
        return item.ch2o;
    case TimestampRole:
        return item.timestamp;
    case AlarmCountRole:
        return item.activeAlarmCount;
    case SelectedRole:
        return item.deviceId == m_currentDeviceId;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> DeviceRepository::roleNames() const
{
    return {
        {DeviceIdRole, "deviceId"},
        {DisplayNameRole, "displayName"},
        {OnlineRole, "online"},
        {ProtocolProfileRole, "protocolProfile"},
        {Relay1OnRole, "relay1On"},
        {Relay2OnRole, "relay2On"},
        {PadLeftStateRole, "padLeftState"},
        {PadRightStateRole, "padRightState"},
        {PadReadyRole, "padReady"},
        {PadOccupiedRole, "padOccupied"},
        {PadModeRole, "padMode"},
        {RssiRole, "rssi"},
        {OperatorRole, "operatorName"},
        {IpRole, "ip"},
        {StateTextRole, "stateText"},
        {TickRole, "tick"},
        {WindSpeedRole, "windSpeed"},
        {WindDirectionRole, "windDirection"},
        {TemperatureRole, "temperature"},
        {HumidityRole, "humidity"},
        {PressureRole, "pressure"},
        {VisibilityRole, "visibility"},
        {WindCapabilityRole, "windCapability"},
        {AirCapabilityRole, "airCapability"},
        {RainCapabilityRole, "rainCapability"},
        {PressureCapabilityRole, "pressureCapability"},
        {VisibilityCapabilityRole, "visibilityCapability"},
        {RainDetectedRole, "rainDetected"},
        {RainValueRole, "rainValue"},
        {Pm25Role, "pm25"},
        {Pm10Role, "pm10"},
        {Co2Role, "co2"},
        {TvocRole, "tvoc"},
        {Ch2oRole, "ch2o"},
        {TimestampRole, "timestamp"},
        {AlarmCountRole, "alarmCount"},
        {SelectedRole, "selected"}
    };
}

int DeviceRepository::count() const
{
    return m_items.size();
}

QString DeviceRepository::currentDeviceId() const
{
    return m_currentDeviceId;
}

void DeviceRepository::setCurrentDeviceId(const QString &deviceId)
{
    if (m_currentDeviceId == deviceId)
    {
        return;
    }

    const int previousIndex = currentIndex();
    m_currentDeviceId = deviceId;
    const int newIndex = currentIndex();

    if (previousIndex >= 0)
    {
        emit dataChanged(index(previousIndex, 0), index(previousIndex, 0), {SelectedRole});
    }
    if (newIndex >= 0)
    {
        emit dataChanged(index(newIndex, 0), index(newIndex, 0), {SelectedRole});
    }
    emit currentDeviceIdChanged();
}

int DeviceRepository::currentIndex() const
{
    return indexOfDevice(m_currentDeviceId);
}

void DeviceRepository::loadStates(const QVector<DeviceState> &states)
{
    beginResetModel();
    m_items = states;
    std::sort(m_items.begin(), m_items.end(), deviceSort);
    endResetModel();
    emit countChanged();

    if ((!m_currentDeviceId.isEmpty() && indexOfDevice(m_currentDeviceId) >= 0) || m_items.isEmpty())
    {
        return;
    }

    if (!m_items.isEmpty())
    {
        m_currentDeviceId = m_items.constFirst().deviceId;
        emit currentDeviceIdChanged();
    }
}

void DeviceRepository::upsertState(const DeviceState &state)
{
    if (state.deviceId.isEmpty())
    {
        return;
    }

    const int existingIndex = indexOfDevice(state.deviceId);
    if (existingIndex < 0)
    {
        beginResetModel();
        m_items.push_back(state);
        std::sort(m_items.begin(), m_items.end(), deviceSort);
        endResetModel();
        emit countChanged();
        if (m_currentDeviceId.isEmpty())
        {
            setCurrentDeviceId(state.deviceId);
        }
        return;
    }

    m_items[existingIndex] = state;
    std::sort(m_items.begin(), m_items.end(), deviceSort);
    emit dataChanged(index(0, 0), index(m_items.size() - 1, 0));
}

void DeviceRepository::setActiveAlarmCounts(const QHash<QString, int> &counts)
{
    for (int row = 0; row < m_items.size(); ++row)
    {
        DeviceState &item = m_items[row];
        const int count = counts.value(item.deviceId, 0);
        if (item.activeAlarmCount == count)
        {
            continue;
        }

        item.activeAlarmCount = count;
        emit dataChanged(index(row, 0), index(row, 0), {AlarmCountRole});
    }
}

QVector<DeviceState> DeviceRepository::states() const
{
    return m_items;
}

void DeviceRepository::selectIndex(int indexValue)
{
    if (indexValue < 0 || indexValue >= m_items.size())
    {
        return;
    }

    setCurrentDeviceId(m_items.at(indexValue).deviceId);
}

void DeviceRepository::selectDevice(const QString &deviceId)
{
    setCurrentDeviceId(deviceId);
}

int DeviceRepository::indexOfDevice(const QString &deviceId) const
{
    for (int row = 0; row < m_items.size(); ++row)
    {
        if (m_items.at(row).deviceId == deviceId)
        {
            return row;
        }
    }

    return -1;
}
