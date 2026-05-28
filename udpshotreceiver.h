#ifndef UDPSHOTRECEIVER_H
#define UDPSHOTRECEIVER_H

#include <QObject>
#include <QString>

class QUdpSocket;

class UdpShotReceiver : public QObject
{
    Q_OBJECT
public:
    explicit UdpShotReceiver(QObject *parent = nullptr);
    ~UdpShotReceiver() override;

    bool startListening(quint16 port);
    void stopListening();
    bool isListening() const;
    quint16 port() const { return m_port; }

    // 解析 +PLS_12345 格式，成功返回 5 位炮号数字串
    static QString parseShotNumber(const QByteArray &payload);

signals:
    void datagramReceived(const QString &asciiText, const QString &senderInfo);
    void shotNumberChanged(const QString &shotNumber);
    void bindStateChanged(bool bound, const QString &message);

private slots:
    void onReadyRead();

private:
    QUdpSocket *m_socket = nullptr;
    quint16 m_port = 0;
    QString m_lastShotNumber;
};

#endif // UDPSHOTRECEIVER_H
