#include "AlarmRepository.h"

#include <QDateTime>

#include "data/SQLiteCache.h"

namespace
{
QString buildAlarmKey(const QString &deviceId, const QString &code)
{
    return QStringLiteral("%1::%2").arg(deviceId, code);
}
}

AlarmRepository::AlarmRepository(SQLiteCache *cache, QObject *parent)
    : QAbstractListModel(parent)
    , m_cache(cache)
{
    refreshCounts();
}

int AlarmRepository::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_records.size();
}

QVariant AlarmRepository::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_records.size())
    {
        return QVariant();
    }

    const AlarmRecord &record = m_records.at(index.row());
    switch (role)
    {
    case IdRole:
        return record.id;
    case AlarmKeyRole:
        return record.alarmKey;
    case DeviceIdRole:
        return record.deviceId;
    case CodeRole:
        return record.code;
    case SeverityRole:
        return record.severity;
    case MessageRole:
        return record.message;
    case SourceRole:
        return record.source;
    case ActiveRole:
        return record.active;
    case CreatedAtRole:
        return record.createdAt;
    case ResolvedAtRole:
        return record.resolvedAt;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> AlarmRepository::roleNames() const
{
    return {
        {IdRole, "rowId"},
        {AlarmKeyRole, "alarmKey"},
        {DeviceIdRole, "deviceId"},
        {CodeRole, "code"},
        {SeverityRole, "severity"},
        {MessageRole, "message"},
        {SourceRole, "source"},
        {ActiveRole, "active"},
        {CreatedAtRole, "createdAt"},
        {ResolvedAtRole, "resolvedAt"}
    };
}

QString AlarmRepository::currentDeviceId() const
{
    return m_currentDeviceId;
}

void AlarmRepository::setCurrentDeviceId(const QString &deviceId)
{
    if (m_currentDeviceId == deviceId)
    {
        return;
    }

    m_currentDeviceId = deviceId;
    emit currentDeviceIdChanged();
    reload();
}

void AlarmRepository::raiseAlarm(const QString &deviceId,
                                 const QString &code,
                                 const QString &severity,
                                 const QString &message,
                                 const QString &source)
{
    if (m_cache == nullptr || deviceId.isEmpty() || code.isEmpty())
    {
        return;
    }

    AlarmRecord record;
    record.alarmKey = buildAlarmKey(deviceId, code);
    record.deviceId = deviceId;
    record.code = code;
    record.severity = severity;
    record.message = message;
    record.source = source;
    record.active = true;
    record.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    m_cache->upsertAlarm(record);
    refreshCounts();
    if (m_currentDeviceId.isEmpty() || m_currentDeviceId == deviceId)
    {
        reload();
    }
}

void AlarmRepository::resolveAlarm(const QString &deviceId, const QString &code)
{
    if (m_cache == nullptr || deviceId.isEmpty() || code.isEmpty())
    {
        return;
    }

    m_cache->resolveAlarm(buildAlarmKey(deviceId, code), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    refreshCounts();
    if (m_currentDeviceId.isEmpty() || m_currentDeviceId == deviceId)
    {
        reload();
    }
}

QHash<QString, int> AlarmRepository::activeCounts() const
{
    return m_activeCounts;
}

void AlarmRepository::reload()
{
    refreshCounts();
    beginResetModel();
    m_records = m_cache != nullptr ? m_cache->loadAlarms(m_currentDeviceId, 200) : QVector<AlarmRecord>();
    endResetModel();
}

void AlarmRepository::refreshCounts()
{
    m_activeCounts = m_cache != nullptr ? m_cache->loadActiveAlarmCounts() : QHash<QString, int>();
    emit activeCountsChanged(m_activeCounts);
}
