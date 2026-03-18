#include "SerialConsoleService.h"

#include <QDateTime>

#if RELAY_CLIENT_HAS_SERIALPORT
#include <QSerialPortInfo>
#endif

SerialConsoleService::SerialConsoleService(QObject *parent)
    : QObject(parent)
{
#if RELAY_CLIENT_HAS_SERIALPORT
    connect(&m_serialPort, &QSerialPort::readyRead, this, &SerialConsoleService::handleReadyRead);
    connect(&m_serialPort, &QSerialPort::errorOccurred, this, &SerialConsoleService::handleError);
#endif
    refreshPorts();
}

bool SerialConsoleService::supported() const
{
#if RELAY_CLIENT_HAS_SERIALPORT
    return true;
#else
    return false;
#endif
}

QStringList SerialConsoleService::availablePorts() const
{
    return m_availablePorts;
}

QString SerialConsoleService::selectedPort() const
{
    return m_selectedPort;
}

void SerialConsoleService::setSelectedPort(const QString &portName)
{
    if (m_selectedPort == portName)
    {
        return;
    }

    m_selectedPort = portName;
    emit selectedPortChanged();
}

int SerialConsoleService::baudRate() const
{
    return m_baudRate;
}

void SerialConsoleService::setBaudRate(int baudRateValue)
{
    if (m_baudRate == baudRateValue || baudRateValue <= 0)
    {
        return;
    }

    m_baudRate = baudRateValue;
    emit baudRateChanged();
}

bool SerialConsoleService::connected() const
{
#if RELAY_CLIENT_HAS_SERIALPORT
    return m_serialPort.isOpen();
#else
    return false;
#endif
}

QString SerialConsoleService::lastError() const
{
    return m_lastError;
}

QAbstractItemModel *SerialConsoleService::logModel()
{
    return &m_logModel;
}

void SerialConsoleService::refreshPorts()
{
#if RELAY_CLIENT_HAS_SERIALPORT
    QStringList ports;
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos)
    {
        ports.append(info.portName());
    }
    if (m_availablePorts != ports)
    {
        m_availablePorts = ports;
        emit availablePortsChanged();
    }
    if (m_selectedPort.isEmpty() && !m_availablePorts.isEmpty())
    {
        setSelectedPort(m_availablePorts.constFirst());
    }
#else
    if (!m_availablePorts.isEmpty())
    {
        m_availablePorts.clear();
        emit availablePortsChanged();
    }
    setLastError(QStringLiteral("当前 Qt 构建未包含 SerialPort 模块"));
#endif
}

void SerialConsoleService::openPort()
{
#if RELAY_CLIENT_HAS_SERIALPORT
    if (m_selectedPort.isEmpty())
    {
        setLastError(QStringLiteral("请选择串口"));
        return;
    }

    if (m_serialPort.isOpen())
    {
        m_serialPort.close();
        emit connectedChanged();
    }

    m_serialPort.setPortName(m_selectedPort);
    m_serialPort.setBaudRate(m_baudRate);
    if (!m_serialPort.open(QIODevice::ReadWrite))
    {
        setLastError(m_serialPort.errorString());
        return;
    }

    appendLog(QStringLiteral("INFO"), QStringLiteral("串口已连接 %1 @ %2").arg(m_selectedPort).arg(m_baudRate));
    emit connectedChanged();
#else
    setLastError(QStringLiteral("当前 Qt 构建未包含 SerialPort 模块"));
#endif
}

void SerialConsoleService::closePort()
{
#if RELAY_CLIENT_HAS_SERIALPORT
    if (!m_serialPort.isOpen())
    {
        return;
    }

    m_serialPort.close();
    appendLog(QStringLiteral("INFO"), QStringLiteral("串口已关闭"));
    emit connectedChanged();
#endif
}

void SerialConsoleService::sendRaw(const QString &text)
{
#if RELAY_CLIENT_HAS_SERIALPORT
    if (!m_serialPort.isOpen())
    {
        setLastError(QStringLiteral("串口未连接"));
        return;
    }

    QByteArray payload = text.toUtf8();
    if (!payload.endsWith("\r\n"))
    {
        payload.append("\r\n");
    }
    m_serialPort.write(payload);
    appendLog(QStringLiteral("TX"), text);
    emit lineCaptured(QStringLiteral("tx"), text);
#else
    Q_UNUSED(text)
    setLastError(QStringLiteral("当前 Qt 构建未包含 SerialPort 模块"));
#endif
}

void SerialConsoleService::sendPreset(const QString &command)
{
    sendRaw(command);
}

void SerialConsoleService::appendLog(const QString &prefix, const QString &text)
{
    QStringList entries = m_logModel.stringList();
    entries.append(QStringLiteral("[%1] %2 %3")
                       .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")))
                       .arg(prefix, text));
    while (entries.size() > 400)
    {
        entries.removeFirst();
    }
    m_logModel.setStringList(entries);
}

void SerialConsoleService::setLastError(const QString &message)
{
    if (m_lastError == message)
    {
        return;
    }

    m_lastError = message;
    emit lastErrorChanged();
}

#if RELAY_CLIENT_HAS_SERIALPORT
void SerialConsoleService::handleReadyRead()
{
    const QString text = QString::fromUtf8(m_serialPort.readAll());
    if (text.isEmpty())
    {
        return;
    }

    appendLog(QStringLiteral("RX"), text.trimmed());
    emit lineCaptured(QStringLiteral("rx"), text.trimmed());
}

void SerialConsoleService::handleError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError)
    {
        return;
    }

    setLastError(m_serialPort.errorString());
}
#endif
