#pragma once

#include <QAbstractListModel>
#include <QVector>

#include "models/DeviceModels.h"

class SQLiteCache;

class HistoryRepository : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString currentDeviceId READ currentDeviceId WRITE setCurrentDeviceId NOTIFY currentDeviceIdChanged)
    Q_PROPERTY(int limit READ limit WRITE setLimit NOTIFY limitChanged)
    Q_PROPERTY(bool preferRemote READ preferRemote WRITE setPreferRemote NOTIFY preferRemoteChanged)

public:
    enum HistoryRoles
    {
        IdRole = Qt::UserRole + 1,
        DeviceIdRole,
        DirectionRole,
        ChannelRole,
        TopicRole,
        CommandRole,
        PayloadRole,
        ResultRole,
        OperatorRole,
        LevelRole,
        CreatedAtRole
    };
    Q_ENUM(HistoryRoles)

    explicit HistoryRepository(SQLiteCache *cache, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString currentDeviceId() const;
    void setCurrentDeviceId(const QString &deviceId);
    int limit() const;
    void setLimit(int limit);
    bool preferRemote() const;
    void setPreferRemote(bool preferRemote);

    void appendRecord(const MessageRecord &record);
    Q_INVOKABLE void reload();

signals:
    void currentDeviceIdChanged();
    void limitChanged();
    void preferRemoteChanged();

private:
    SQLiteCache *m_cache = nullptr;
    QVector<MessageRecord> m_records;
    QString m_currentDeviceId;
    int m_limit = 200;
    bool m_preferRemote = false;
};
