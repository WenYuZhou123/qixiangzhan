#pragma once

#include <QObject>
#include <QStringListModel>

#if RELAY_CLIENT_HAS_SERIALPORT
#include <QSerialPort>
#endif

class SerialConsoleService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool supported READ supported CONSTANT)
    Q_PROPERTY(QStringList availablePorts READ availablePorts NOTIFY availablePortsChanged)
    Q_PROPERTY(QString selectedPort READ selectedPort WRITE setSelectedPort NOTIFY selectedPortChanged)
    Q_PROPERTY(int baudRate READ baudRate WRITE setBaudRate NOTIFY baudRateChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QAbstractItemModel *logModel READ logModel CONSTANT)

public:
    explicit SerialConsoleService(QObject *parent = nullptr);

    bool supported() const;
    QStringList availablePorts() const;
    QString selectedPort() const;
    void setSelectedPort(const QString &portName);
    int baudRate() const;
    void setBaudRate(int baudRate);
    bool connected() const;
    QString lastError() const;
    QAbstractItemModel *logModel();

    Q_INVOKABLE void refreshPorts();
    Q_INVOKABLE void openPort();
    Q_INVOKABLE void closePort();
    Q_INVOKABLE void sendRaw(const QString &text);
    Q_INVOKABLE void sendPreset(const QString &command);

signals:
    void availablePortsChanged();
    void selectedPortChanged();
    void baudRateChanged();
    void connectedChanged();
    void lastErrorChanged();
    void lineCaptured(const QString &direction, const QString &text);

private:
    void appendLog(const QString &prefix, const QString &text);
    void setLastError(const QString &message);

#if RELAY_CLIENT_HAS_SERIALPORT
    void handleReadyRead();
    void handleError(QSerialPort::SerialPortError error);
    QSerialPort m_serialPort;
#endif

    QStringListModel m_logModel;
    QStringList m_availablePorts;
    QString m_selectedPort;
    QString m_lastError;
    int m_baudRate = 115200;
};
