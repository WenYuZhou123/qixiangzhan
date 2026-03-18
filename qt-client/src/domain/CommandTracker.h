#pragma once

#include <QObject>
#include <QTimer>

class CommandTracker : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool pending READ pending NOTIFY pendingChanged)
    Q_PROPERTY(QString pendingMsgId READ pendingMsgId NOTIFY pendingChanged)
    Q_PROPERTY(QString pendingCommand READ pendingCommand NOTIFY pendingChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(bool canRetry READ canRetry NOTIFY retryChanged)

public:
    explicit CommandTracker(QObject *parent = nullptr);

    bool pending() const;
    QString pendingMsgId() const;
    QString pendingCommand() const;
    QString lastError() const;
    bool canRetry() const;
    QString lastCommand() const;
    int lastValue() const;
    bool lastHasValue() const;

    void startPending(const QString &msgId, const QString &command, int value, bool hasValue, int timeoutMs = 5000);
    void resolveAck(const QString &msgId);
    void failPending(const QString &message);
    bool matchesPending(const QString &msgId) const;
    void rememberLastCommand(const QString &command, int value, bool hasValue);
    void clearLastError();

signals:
    void pendingChanged();
    void lastErrorChanged();
    void retryChanged();
    void commandTimedOut(const QString &msgId, const QString &command);

private slots:
    void handleTimeout();

private:
    void setLastError(const QString &message);

    bool m_pending = false;
    QString m_pendingMsgId;
    QString m_pendingCommand;
    QString m_lastError;
    QString m_lastCommand;
    int m_lastValue = 0;
    bool m_lastHasValue = false;
    bool m_canRetry = false;
    QTimer m_timeoutTimer;
};
