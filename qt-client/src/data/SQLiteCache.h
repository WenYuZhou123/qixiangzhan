#pragma once

#include <QHash>
#include <QObject>
#include <QSqlDatabase>
#include <QVector>

#include "models/DeviceModels.h"

class SQLiteCache : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString databasePath READ databasePath CONSTANT)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit SQLiteCache(QObject *parent = nullptr);
    ~SQLiteCache() override;

    bool initialize();
    QString databasePath() const;
    QString lastError() const;

    QString setting(const QString &key, const QString &defaultValue = QString()) const;
    void setSetting(const QString &key, const QString &value);
    void removeSetting(const QString &key);

    void upsertDeviceState(const DeviceState &state);
    QVector<DeviceState> loadDeviceStates() const;

    void insertMessage(const MessageRecord &record);
    void replaceRemoteMessages(const QString &deviceId, const QVector<MessageRecord> &records);
    QVector<MessageRecord> loadMessages(const QString &deviceId, int limit = 200, bool preferRemote = false) const;

    void insertOutboxCommand(const QString &msgId,
                            const QString &deviceId,
                            const QString &command,
                            const QString &payload,
                            const QString &status,
                            const QString &detail = QString());
    void updateOutboxStatus(const QString &msgId, const QString &status, const QString &detail = QString());

    AlarmRecord upsertAlarm(const AlarmRecord &record);
    void resolveAlarm(const QString &alarmKey, const QString &resolvedAt);
    QVector<AlarmRecord> loadAlarms(const QString &deviceId, int limit = 200) const;
    QHash<QString, int> loadActiveAlarmCounts() const;

signals:
    void lastErrorChanged();

private:
    bool openIfNeeded() const;
    bool execSchemaStatement(const QString &statement) const;
    void setLastError(const QString &message);
    void pruneMessages(const QString &deviceId);
    QString connectionName() const;

    mutable QSqlDatabase m_database;
    QString m_databasePath;
    QString m_lastError;
};
