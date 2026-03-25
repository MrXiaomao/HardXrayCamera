#include "tcpclient.h"

TcpClientThread::TcpClientThread(QObject* parent)
    : QObject(parent), m_socket(nullptr), m_reconnectTimer(new QTimer(this)) {
    // 初始化重连定时器（指数退避策略）
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &TcpClientThread::connectToHost);

    // 心跳定时器
    m_heartbeatTimer = new QTimer(this);
    connect(m_heartbeatTimer, &QTimer::timeout, [this](){
        if(m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
            m_socket->write("HEARTBEAT");
        }
    });
}

// 发送数据，并且把发送数据按 4096 字节切块，每块前面加 4 字节长度头
void TcpClientThread::sendData(const QByteArray& data) {
    if(!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "Attempt to send data when disconnected";
        return;
    }

    // 分包处理（假设协议头包含4字节长度）
    const int chunkSize = 4096;
    for(int i=0; i<data.size(); i+=chunkSize) {
        QByteArray chunk = data.mid(i, chunkSize);
        QByteArray header;
        QDataStream ds(&header, QIODevice::WriteOnly);
        ds << qToBigEndian<quint32>(chunk.size());
        m_socket->write(header + chunk);
    }
}

// 每次连接前先删旧 socket，再创建新 socket，然后连接
void TcpClientThread::connectToHost() {
    if(m_socket) {
        m_socket->deleteLater();
        m_socket = nullptr;
    }

    m_socket = new QTcpSocket(this);
    int bufferSize = 4 * 1024 * 1024;
    m_socket->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption, bufferSize);
    m_socket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, bufferSize);

    // 连接信号槽
    connect(m_socket, &QTcpSocket::connected, this, &TcpClientThread::onConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &TcpClientThread::processData);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &TcpClientThread::onError);
    connect(m_socket, &QTcpSocket::disconnected, this, &TcpClientThread::onDisconnected);

    m_socket->connectToHost(m_host, m_port);
}

// 把收到的数据追加到缓冲区，然后按“4 字节长度头 + 包体”的格式拆包，拆出完整包后发
void TcpClientThread::processData() {
    m_recvBuffer.append(m_socket->readAll());
    // 协议解析（基于长度头）
    while(m_recvBuffer.size() >= 4) {
        quint32 packetSize = qFromBigEndian<quint32>(m_recvBuffer.left(4));
        if(m_recvBuffer.size() < packetSize + 4) break;

        QByteArray packet = m_recvBuffer.mid(4, packetSize);
        m_recvBuffer.remove(0, packetSize + 4);

        emit dataReceived(packet);
    }
}

void TcpClientThread::scheduleReconnect() {
    const int maxDelay = 30000; // 最大重试间隔30秒
    int delay = qMin(1000 * (1 << m_reconnectAttempts), maxDelay);
    m_reconnectTimer->start(delay);
    m_reconnectAttempts++;
    qInfo() << "Will reconnect in" << delay << "ms";
}

TcpClient::TcpClient(QObject *parent)
    : QObject{parent}
{
    m_thread = new QThread;
    m_worker = new TcpClientThread;
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(this, &TcpClient::startSignal, m_worker, &TcpClientThread::start);
    connect(this, &TcpClient::stopSignal, m_worker, &TcpClientThread::stop);
    connect(this, &TcpClient::sendDataSignal, m_worker, &TcpClientThread::sendData);
    connect(m_worker, &TcpClientThread::dataReceived, this, &TcpClient::dataReceived);
    connect(m_worker, &TcpClientThread::connectionStatusChanged, this, &TcpClient::sigconnectStatusChanged);
    connect(m_worker, &TcpClientThread::sigErrorOccurred, this, &TcpClient::sigconnectError);
    
    m_thread->start();
}

TcpClient::~TcpClient() {
    emit stopSignal();
    m_thread->quit();
    m_thread->wait();
    delete m_thread;
}

void TcpClient::connectToHost(const QString& host, quint16 port) {
    m_worker->setServerInfo(host, port);
    emit startSignal();
}

void TcpClient::disconnectFromHost() {
    emit stopSignal();
}

void TcpClient::send(const QByteArray& data) {
    emit sendDataSignal(data);
}
