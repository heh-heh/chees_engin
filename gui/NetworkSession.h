#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>

class QTcpServer;
class QTcpSocket;

// Wraps QTcpServer/QTcpSocket to host or join a LAN match, replacing the
// manual thread+mutex socket handling used by the previous ImGui GUI.
class NetworkSession : public QObject {
    Q_OBJECT

public:
    enum class Mode { None, Host, Client };

    explicit NetworkSession(QObject* parent = nullptr);
    ~NetworkSession() override;

    bool hostGame(quint16 port, QString& error);
    bool joinGame(const QString& host, quint16 port, QString& error);

    // Connects out to a relay server instead of listening/dialing directly,
    // so neither side needs port forwarding. Both sides must use the same
    // roomCode; the relay pairs them and then just forwards raw bytes.
    bool hostViaRelay(const QString& relayHost, quint16 relayPort, const QString& roomCode, QString& error);
    bool joinViaRelay(const QString& relayHost, quint16 relayPort, const QString& roomCode, QString& error);
    void stop();

    bool isConnected() const;
    Mode mode() const { return mode_; }
    bool usingRelay() const { return using_relay_; }
    bool sendMessage(const QString& message);

    static QString localIPv4Address();

signals:
    void opponentConnected();
    void opponentDisconnected();
    void messageReceived(const QString& message);
    void connectionFailed(const QString& error);

private slots:
    void handleNewConnection();
    void handleSocketConnected();
    void handleReadyRead();
    void handleSocketError();
    void handleSocketDisconnected();

private:
    void attachSocket(QTcpSocket* socket);
    bool connectViaRelay(Mode mode, const QString& relayHost, quint16 relayPort, const QString& roomCode, QString& error);
    void handleRelayHandshakeLine(const QByteArray& line);

    Mode mode_ = Mode::None;
    QTcpServer* server_ = nullptr;
    QTcpSocket* socket_ = nullptr;
    QByteArray buffer_;
    bool using_relay_ = false;
    bool paired_ = false;
    QString pending_room_code_;
};
