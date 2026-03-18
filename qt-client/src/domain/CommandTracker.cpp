#include "CommandTracker.h"

CommandTracker::CommandTracker(QObject *parent)
    : QObject(parent)
{
    m_timeoutTimer.setSingleShot(true);
    m_timeoutTimer.setInterval(5000);
    connect(&m_timeoutTimer, &QTimer::timeout, this, &CommandTracker::handleTimeout);
}

bool CommandTracker::pending() const
{
    return m_pending;
}

QString CommandTracker::pendingMsgId() const
{
    return m_pendingMsgId;
}

QString CommandTracker::pendingCommand() const
{
    return m_pendingCommand;
}

QString CommandTracker::lastError() const
{
    return m_lastError;
}

bool CommandTracker::canRetry() const
{
    return m_canRetry;
}

QString CommandTracker::lastCommand() const
{
    return m_lastCommand;
}

int CommandTracker::lastValue() const
{
    return m_lastValue;
}

bool CommandTracker::lastHasValue() const
{
    return m_lastHasValue;
}

void CommandTracker::startPending(const QString &msgId, const QString &command, int value, bool hasValue, int timeoutMs)
{
    rememberLastCommand(command, value, hasValue);
    m_pending = true;
    m_pendingMsgId = msgId;
    m_pendingCommand = command;
    clearLastError();
    m_timeoutTimer.setInterval(timeoutMs > 0 ? timeoutMs : 5000);
    m_timeoutTimer.start();
    emit pendingChanged();
}

void CommandTracker::resolveAck(const QString &msgId)
{
    if (!matchesPending(msgId))
    {
        return;
    }

    m_timeoutTimer.stop();
    m_pending = false;
    m_pendingMsgId.clear();
    m_pendingCommand.clear();
    emit pendingChanged();
}

void CommandTracker::failPending(const QString &message)
{
    m_timeoutTimer.stop();
    m_pending = false;
    m_pendingMsgId.clear();
    m_pendingCommand.clear();
    setLastError(message);
    emit pendingChanged();
}

bool CommandTracker::matchesPending(const QString &msgId) const
{
    return m_pending && !msgId.isEmpty() && m_pendingMsgId == msgId;
}

void CommandTracker::rememberLastCommand(const QString &command, int value, bool hasValue)
{
    m_lastCommand = command;
    m_lastValue = value;
    m_lastHasValue = hasValue;
    if (!m_canRetry)
    {
        m_canRetry = true;
        emit retryChanged();
    }
}

void CommandTracker::clearLastError()
{
    if (m_lastError.isEmpty())
    {
        return;
    }

    m_lastError.clear();
    emit lastErrorChanged();
}

void CommandTracker::handleTimeout()
{
    const QString msgId = m_pendingMsgId;
    const QString command = m_pendingCommand;

    m_pending = false;
    m_pendingMsgId.clear();
    m_pendingCommand.clear();
    setLastError(QStringLiteral("ACK timeout"));
    emit pendingChanged();
    emit commandTimedOut(msgId, command);
}

void CommandTracker::setLastError(const QString &message)
{
    if (m_lastError == message)
    {
        return;
    }

    m_lastError = message;
    emit lastErrorChanged();
}
