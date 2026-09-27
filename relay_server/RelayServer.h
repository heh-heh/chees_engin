#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>

class QTcpServer;
class QTcpSocket;

// Pure byte relay: pairs a hosting socket and a joining socket by room code,
// then forwards raw bytes between them unmodified. It never parses or plays
// chess; the two chess_gui clients speak the existing protocol to each other
// as if directly connected.
class RelayServer : public QObject {
    Q_OBJECT

public:
    explicit RelayServer(quint16 port, QObject* parent = nullptr);

private slots:
    void onNewConnection();
    void onStatusConnection();
    void onReadyRead();
    void onSocketDisconnected();

private:
    struct Connection {
        QByteArray buffer;
        QString roomCode;
        bool paired = false;
    };

    void handleHandshakeLine(QTcpSocket* socket, const QByteArray& line);
    void pairSockets(QTcpSocket* hostSocket, QTcpSocket* clientSocket);
    void teardown(QTcpSocket* socket);
    QByteArray buildStatusPage() const;

    QTcpServer* server_;
    QTcpServer* status_server_;
    QHash<QTcpSocket*, Connection> connections_;
    QHash<QString, QTcpSocket*> waitingHosts_;
    QHash<QTcpSocket*, QTcpSocket*> peers_;
};
