#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>

#include "RelayServer.h"

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("chess_relay_server");

    QCommandLineParser parser;
    parser.setApplicationDescription("Relay server that pairs chess_gui hosts and clients by room code.");
    parser.addHelpOption();
    QCommandLineOption port_option({"p", "port"}, "TCP port to listen on.", "port", "9100");
    parser.addOption(port_option);
    parser.process(app);

    bool ok = false;
    const quint16 port = parser.value(port_option).toUShort(&ok);
    if (!ok || port == 0) {
        qCritical("Invalid port value.");
        return 1;
    }

    RelayServer server(port);
    return app.exec();
}
