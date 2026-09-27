#pragma once

#include <QWidget>

class QLabel;
class QCheckBox;
class QPushButton;
class QTextEdit;
class QLineEdit;
class QProgressBar;

// Waiting room shown while a hosted/joined match waits for both players
// to connect and mark themselves ready.
class WaitingRoomPage : public QWidget {
    Q_OBJECT

public:
    explicit WaitingRoomPage(QWidget* parent = nullptr);

    void setHostMode(bool isHost);
    void setRoomInfo(const QString& roomCode, const QString& lanAddress, const QString& targetHost, quint16 port, bool isRelay = false);
    void setStatusMessage(const QString& message);
    void setOpponentConnected(bool connected);
    void setReadyState(bool localReady, bool opponentReady, bool canStart);
    void appendChatMessage(const QString& line);
    void clearChat();

signals:
    void readyToggled(bool ready);
    void startMatchRequested();
    void chatMessageSent(const QString& text);
    void backRequested();

private:
    QLabel* mode_title_ = nullptr;
    QLabel* mode_subtitle_ = nullptr;
    QLabel* room_info_label_ = nullptr;
    QLabel* status_label_ = nullptr;
    QProgressBar* readiness_bar_ = nullptr;
    QCheckBox* ready_checkbox_ = nullptr;
    QLabel* opponent_ready_label_ = nullptr;
    QPushButton* start_match_button_ = nullptr;
    QTextEdit* chat_view_ = nullptr;
    QLineEdit* chat_input_ = nullptr;
    bool is_host_ = false;
};
