#include "HistoryRepository.h"

#include "data/SQLiteCache.h"

HistoryRepository::HistoryRepository(SQLiteCache *cache, QObject *parent)
    : QAbstractListModel(parent)
    , m_cache(cache)
{
}

int HistoryRepository::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_records.size();
}

QVariant HistoryRepository::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_records.size())
    {
        return QVariant();
    }

    const MessageRecord &record = m_records.at(index.row());
    switch (role)
    {
    case IdRole:
        return record.id;
    case DeviceIdRole:
        return record.deviceId;
    case DirectionRole:
        return record.direction;
    case ChannelRole:
        return record.channel;
    case TopicRole:
        return record.topic;
    case CommandRole:
        return record.command;
    case PayloadRole:
        return record.payload;
    case ResultRole:
        return record.result;
    case OperatorRole:
        return record.operatorName;
    case LevelRole:
        return record.level;
    case CreatedAtRole:
        return record.createdAt;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> HistoryRepository::roleNames() const
{
    return {
        {IdRole, "rowId"},
        {DeviceIdRole, "deviceId"},
        {DirectionRole, "direction"},
        {ChannelRole, "channel"},
        {TopicRole, "topic"},
        {CommandRole, "command"},
        {PayloadRole, "payload"},
        {ResultRole, "result"},
        {OperatorRole, "operatorName"},
        {LevelRole, "level"},
        {CreatedAtRole, "createdAt"}
    };
}

QString HistoryRepository::currentDeviceId() const
{
    return m_currentDeviceId;
}

void HistoryRepository::setCurrentDeviceId(const QString &deviceId)
{
    if (m_currentDeviceId == deviceId)
    {
        return;
    }

    m_currentDeviceId = deviceId;
    emit currentDeviceIdChanged();
    reload();
}

int HistoryRepository::limit() const
{
    return m_limit;
}

bool HistoryRepository::preferRemote() const
{
    return m_preferRemote;
}

void HistoryRepository::setLimit(int limitValue)
{
    if (m_limit == limitValue || limitValue <= 0)
    {
        return;
    }

    m_limit = limitValue;
    emit limitChanged();
    reload();
}

void HistoryRepository::setPreferRemote(bool preferRemoteValue)
{
    if (m_preferRemote == preferRemoteValue)
    {
        return;
    }

    m_preferRemote = preferRemoteValue;
    emit preferRemoteChanged();
    reload();
}

void HistoryRepository::appendRecord(const MessageRecord &record)
{
    if (m_cache == nullptr)
    {
        return;
    }

    m_cache->insertMessage(record);
    if (!m_currentDeviceId.isEmpty() && record.deviceId != m_currentDeviceId)
    {
        return;
    }

    beginResetModel();
    m_records.prepend(record);
    if (m_records.size() > m_limit)
    {
        m_records.resize(m_limit);
    }
    endResetModel();
}

void HistoryRepository::reload()
{
    beginResetModel();
    m_records = m_cache != nullptr ? m_cache->loadMessages(m_currentDeviceId, m_limit, m_preferRemote) : QVector<MessageRecord>();
    endResetModel();
}
