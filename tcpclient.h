#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <QObject>
#include <QtNetwork>

// 通信核心线程（继承QObject用于信号槽）
class TcpClientThread : public QObject {
    Q_OBJECT
public:
    explicit TcpClientThread(QObject* parent = nullptr);

    void setServerInfo(const QString& host, quint16 port) {
        m_host = host;
        m_port = port;
    }

public slots:
    void start() {
        connectToHost();
        m_heartbeatTimer->start(5000); // 5秒心跳
    }

    void stop() {
        if(m_socket) m_socket->disconnectFromHost();
        m_reconnectTimer->stop();
        m_heartbeatTimer->stop();
    }

    void sendData(const QByteArray& data);

private slots:
    void connectToHost();

    void onConnected() {
        m_reconnectAttempts = 0;
        qInfo() << "Connected to server";
        emit connectionStatusChanged(true);
    }

    void onError(QAbstractSocket::SocketError error) {
        qWarning() << "Socket error:" << error << m_socket->errorString();
        emit sigErrorOccurred(error);
        scheduleReconnect();
    }

    void onDisconnected() {
        qInfo() << "Disconnected from server";
        emit connectionStatusChanged(false);
        // scheduleReconnect();
    }

    //数据分包处理
    void processData();


private:
    void scheduleReconnect();

signals:
    void dataReceived(const QByteArray& data);
    // 连接异常
    void sigErrorOccurred(QAbstractSocket::SocketError error);
    // 连接状态改变，上线/下线
    void connectionStatusChanged(bool connected);

private:
    QTcpSocket* m_socket;
    QTimer* m_reconnectTimer;
    QTimer* m_heartbeatTimer;
    QString m_host;
    quint16 m_port;
    int m_reconnectAttempts = 0;
    QByteArray m_recvBuffer;
};

// 客户端管理类（主线程使用）
class TcpClient : public QObject
{
    Q_OBJECT
public:
    explicit TcpClient(QObject *parent = nullptr);

    ~TcpClient();

    void connectToHost(const QString& host, quint16 port);

    void disconnectFromHost();

    void send(const QByteArray& data);

signals:
    void startSignal();
    void stopSignal();
    void sigconnectError(QAbstractSocket::SocketError error);
    void sigconnectStatusChanged(bool connected);
    void sendDataSignal(const QByteArray&);
    void dataReceived(const QByteArray&);

private:
    QThread* m_thread;
    TcpClientThread* m_worker;
};

#endif // TCPCLIENT_H
