#pragma once

#include <QWidget>

class QLineEdit;
class QSpinBox;
class QLabel;
class QCheckBox;

// Screen for choosing single-player, hosting, or joining a match.
class ModeSelectPage : public QWidget {
    Q_OBJECT

public:
    explicit ModeSelectPage(QWidget* parent = nullptr);

    void setError(const QString& error);
    void setModeLabel(const QString& modeText);

signals:
    void singlePlayerRequested();
    void hostRequested(quint16 port);
    void joinRequested(const QString& host, const QString& roomCode, quint16 port);

    // Emitted instead of the signals above when "use relay server" is checked;
    // both sides only need outbound access to relayHost/relayPort.
    void hostViaRelayRequested(const QString& relayHost, quint16 relayPort, const QString& roomCode);
    void joinViaRelayRequested(const QString& relayHost, quint16 relayPort, const QString& roomCode);
    void backRequested();

private:
    QLineEdit* host_input_ = nullptr;
    QLineEdit* room_code_input_ = nullptr;
    QSpinBox* port_input_ = nullptr;
    QLabel* status_label_ = nullptr;

    QCheckBox* use_relay_checkbox_ = nullptr;
    QLineEdit* relay_host_input_ = nullptr;
    QSpinBox* relay_port_input_ = nullptr;
};
