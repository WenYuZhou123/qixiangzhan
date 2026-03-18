#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QVector>

#include "models/DeviceModels.h"

class SQLiteCache;

class AlarmRepository : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString currentDeviceId READ currentDeviceId WRITE setCurrentDeviceId NOTIFY currentDeviceIdChanged)

public:
    enum AlarmRoles
    {
        IdRole = Qt::UserRole + 1,
        AlarmKeyRole,
        DeviceIdRole,
        CodeRole,
        SeverityRole,
        MessageRole,
        SourceRole,
        ActiveRole,
        CreatedAtRole,
        ResolvedAtRole
    };
    Q_ENUM(AlarmRoles)

    explicit AlarmRepository(SQLiteCache *cache, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString currentDeviceId() const;
    void setCurrentDeviceId(const QString &deviceId);

    void raiseAlarm(const QString &deviceId,
                    const QString &code,
                    const QString &severity,
                    const QString &message,
                    const QString &source);
    void resolveAlarm(const QString &deviceId, const QString &code);
    QHash<QString, int> activeCounts() const;

    Q_INVOKABLE void reload();

signals:
    void currentDeviceIdChanged();
    void activeCountsChanged(const QHash<QString, int> &counts);

private:
    void refreshCounts();

    SQLiteCache *m_cache = nullptr;
    QVector<AlarmRecord> m_records;
    QHash<QString, int> m_activeCounts;
    QString m_currentDeviceId;
};
