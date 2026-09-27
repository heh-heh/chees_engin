#pragma once

#include <QWidget>

#include "GameSession.h"

class BoardWidget;
class QLabel;
class QProgressBar;
class QTextEdit;
class QLineEdit;
class QPushButton;

// Main gameplay screen: board on the left, status/chat panel on the right.
class GamePage : public QWidget {
    Q_OBJECT

public:
    explicit GamePage(QWidget* parent = nullptr);

    BoardWidget* boardWidget() const { return board_widget_; }

    void setMultiplayer(bool multiplayer);
    void setBoardLocked(bool locked);
    void refresh(const GameSession& session, std::chrono::milliseconds turnElapsed, int boardScore);
    void appendChatMessage(const QString& line);
    void clearChat();

signals:
    void squareClicked(int row, int col);
    void newGameRequested();
    void lobbyRequested();
    void chatMessageSent(const QString& text);

private:
    BoardWidget* board_widget_ = nullptr;
    QLabel* mode_label_ = nullptr;
    QLabel* turn_label_ = nullptr;
    QLabel* color_label_ = nullptr;
    QLabel* timer_label_ = nullptr;
    QLabel* score_label_ = nullptr;
    QProgressBar* score_bar_ = nullptr;
    QLabel* game_status_label_ = nullptr;
    QLabel* castling_label_ = nullptr;
    QLabel* status_message_label_ = nullptr;
    QLabel* selected_label_ = nullptr;
    QWidget* chat_panel_ = nullptr;
    QTextEdit* chat_view_ = nullptr;
    QLineEdit* chat_input_ = nullptr;
};
