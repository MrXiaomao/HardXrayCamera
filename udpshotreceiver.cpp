#include "udpshotreceiver.h"

#include <QHostAddress>
#include <QNetworkDatagram>
#include <QUdpSocket>

UdpShotReceiver::UdpShotReceiver(QObject *parent)
    : QObject(parent)
    , m_socket(new QUdpSocket(this))
{
    connect(m_socket, &QUdpSocket::readyRead, this, &UdpShotReceiver::onReadyRead);
}

UdpShotReceiver::~UdpShotReceiver()
{
    if (m_socket->isOpen())
        m_socket->close();
}

bool UdpShotReceiver::isListening() const
{
    return m_socket->state() == QAbstractSocket::BoundState;
}

bool UdpShotReceiver::startListening(quint16 port)
{
    if (m_socket->state() == QAbstractSocket::BoundState)
        m_socket->close();

    m_port = port;
    const bool ok = m_socket->bind(QHostAddress::AnyIPv4, m_port,
                                   QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    if (ok) {
        emit bindStateChanged(true,
                              tr("UDP 炮号广播监听已启动，端口 %1").arg(m_port));
    } else {
        emit bindStateChanged(false,
                              tr("UDP 绑定端口 %1 失败: %2")
                                  .arg(m_port)
                                  .arg(m_socket->errorString()));
    }
    return ok;
}

void UdpShotReceiver::stopListening()
{
    if (m_socket->state() == QAbstractSocket::BoundState) {
        m_socket->close();
        emit bindStateChanged(false, tr("UDP 炮号广播监听已关闭"));
    }
    m_lastShotNumber.clear();
}

QString UdpShotReceiver::parseShotNumber(const QByteArray &payload)
{
    if (payload.size() < 10)
        return {};

    if (payload.at(0) != '+' || payload.at(1) != 'P' || payload.at(2) != 'L' || payload.at(3) != 'S')
        return {};

    QString shotId;
    shotId.reserve(5);
    for (int i = 5; i < 10 && i < payload.size(); ++i) {
        const char c = payload.at(i);
        if (c < '0' || c > '9')
            return {};
        shotId.append(QLatin1Char(c));
    }
    if (shotId.size() != 5)
        return {};
    return shotId;
}

void UdpShotReceiver::onReadyRead()
{
    while (m_socket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_socket->receiveDatagram();
        const QByteArray payload = datagram.data();
        const QString asciiText = QString::fromUtf8(payload);

        const QString senderInfo = QStringLiteral("%1:%2")
                                       .arg(datagram.senderAddress().toString())
                                       .arg(datagram.senderPort());

        emit datagramReceived(asciiText, senderInfo);

        const QString shotNumber = parseShotNumber(payload);
        if (shotNumber.isEmpty())
            continue;

        if (shotNumber == m_lastShotNumber)
            continue;

        m_lastShotNumber = shotNumber;
        emit shotNumberChanged(shotNumber);
    }
}
