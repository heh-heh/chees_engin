#include "GamePage.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

#include "BoardWidget.h"

namespace {

QString piece_symbol(int value) {
    if (value == 0) {
        return QString();
    }
    const bool white = value > 0;
    switch (std::abs(value)) {
        case 1: return white ? "P" : "p";
        case 2: return white ? "N" : "n";
        case 3: return white ? "B" : "b";
        case 4: return white ? "R" : "r";
        case 5: return white ? "Q" : "q";
        case 6: return white ? "K" : "k";
        default: return QString();
    }
}

QString color_name(int color) {
    return color == 1 ? "White" : "Black";
}

QString format_turn_timer(std::chrono::milliseconds elapsed) {
    const auto total_seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
    const int seconds = static_cast<int>(total_seconds % 60);
    const int minutes = static_cast<int>(total_seconds / 60);
    return QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0'));
}

QString format_score_value(int score) {
    const QString sign = score >= 0 ? "+" : "-";
    const int magnitude = std::abs(score);
    return QString("%1%2.%3").arg(sign).arg(magnitude / 100).arg(magnitude % 100, 2, 10, QChar('0'));
}

}  // namespace

GamePage::GamePage(QWidget* parent) : QWidget(parent) {
    board_widget_ = new BoardWidget();
    connect(board_widget_, &BoardWidget::squareClicked, this, &GamePage::squareClicked);

    mode_label_ = new QLabel("Single Player Match");
    mode_label_->setStyleSheet("font-size: 18px; font-weight: bold;");

    turn_label_ = new QLabel();
    color_label_ = new QLabel();
    timer_label_ = new QLabel();
    score_label_ = new QLabel();

    score_bar_ = new QProgressBar();
    score_bar_->setRange(0, 100);
    score_bar_->setTextVisible(true);

    game_status_label_ = new QLabel();
    castling_label_ = new QLabel();
    status_message_label_ = new QLabel();
    status_message_label_->setWordWrap(true);
    selected_label_ = new QLabel("Selected: none");

    chat_view_ = new QTextEdit();
    chat_view_->setReadOnly(true);
    chat_view_->setFixedHeight(120);
    chat_input_ = new QLineEdit();
    chat_input_->setPlaceholderText("Type a chat message...");
    auto* send_button = new QPushButton("Send");
    connect(send_button, &QPushButton::clicked, this, [this]() {
        const QString text = chat_input_->text();
        if (!text.isEmpty()) {
            emit chatMessageSent(text);
            chat_input_->clear();
        }
    });
    connect(chat_input_, &QLineEdit::returnPressed, send_button, &QPushButton::click);

    auto* chat_row = new QHBoxLayout();
    chat_row->addWidget(chat_input_);
    chat_row->addWidget(send_button);

    chat_panel_ = new QWidget();
    auto* chat_layout = new QVBoxLayout(chat_panel_);
    chat_layout->setContentsMargins(0, 0, 0, 0);
    chat_layout->addWidget(new QLabel("MATCH CHAT"));
    chat_layout->addWidget(chat_view_);
    chat_layout->addLayout(chat_row);
    chat_panel_->setVisible(false);

    auto* new_game_button = new QPushButton("New Game");
    auto* lobby_button = new QPushButton("Lobby");
    auto* reset_button = new QPushButton("Reset");
    for (QPushButton* button : {new_game_button, lobby_button, reset_button}) {
        button->setMinimumSize(140, 35);
    }
    connect(new_game_button, &QPushButton::clicked, this, &GamePage::newGameRequested);
    connect(reset_button, &QPushButton::clicked, this, &GamePage::newGameRequested);
    connect(lobby_button, &QPushButton::clicked, this, &GamePage::lobbyRequested);

    auto* button_row = new QHBoxLayout();
    button_row->addWidget(new_game_button);
    button_row->addWidget(lobby_button);
    button_row->addWidget(reset_button);
    button_row->addStretch();

    auto* status_box = new QGroupBox("STATUS");
    auto* status_layout = new QVBoxLayout(status_box);
    status_layout->addWidget(turn_label_);
    status_layout->addWidget(color_label_);
    status_layout->addWidget(timer_label_);
    status_layout->addWidget(score_label_);
    status_layout->addWidget(score_bar_);
    status_layout->addWidget(game_status_label_);
    status_layout->addWidget(castling_label_);
    status_layout->addWidget(new QLabel("Message"));
    status_layout->addWidget(status_message_label_);
    status_layout->addWidget(chat_panel_);
    status_layout->addWidget(selected_label_);
    status_layout->addStretch();
    status_layout->addLayout(button_row);

    auto* board_row = new QHBoxLayout();
    board_row->addWidget(board_widget_, 3);
    board_row->addWidget(status_box, 2);

    auto* top_level_layout = new QVBoxLayout(this);
    top_level_layout->addWidget(mode_label_);
    top_level_layout->addLayout(board_row);
}

void GamePage::setMultiplayer(bool multiplayer) {
    mode_label_->setText(multiplayer ? "Multiplayer Match" : "Single Player Match");
    chat_panel_->setVisible(multiplayer);
}

void GamePage::setBoardLocked(bool locked) {
    board_widget_->setLocked(locked);
}

void GamePage::refresh(const GameSession& session, std::chrono::milliseconds turnElapsed, int boardScore) {
    board_widget_->update();

    turn_label_->setText(QString("Turn: %1").arg(session.turn()));
    color_label_->setText(QString("Color: %1").arg(color_name(session.currentColor())));
    timer_label_->setText(QString("Timer: %1").arg(format_turn_timer(turnElapsed)));
    score_label_->setText(QString("Score: %1").arg(format_score_value(boardScore)));

    const float normalized = std::clamp((boardScore + 1200.0f) / 2400.0f, 0.0f, 1.0f);
    score_bar_->setValue(static_cast<int>(normalized * 100));
    score_bar_->setFormat(boardScore >= 0 ? "White favor" : "Black favor");

    const bool in_check = session.isKingInCheck(session.currentColor());
    if (session.isGameOver()) {
        game_status_label_->setText("Status: Game Over");
        game_status_label_->setStyleSheet("color: #ff4d4d;");
    } else if (in_check) {
        game_status_label_->setText("Status: Check");
        game_status_label_->setStyleSheet("color: #ff6666;");
    } else {
        game_status_label_->setText("Status: Normal");
        game_status_label_->setStyleSheet("color: #6ecb84;");
    }

    const bool kingside = session.canCastleKingside(session.currentColor());
    const bool queenside = session.canCastleQueenside(session.currentColor());
    if (kingside || queenside) {
        castling_label_->setText(QString("Castling: %1%2").arg(kingside ? "K" : "").arg(queenside ? "Q" : ""));
        castling_label_->setVisible(true);
    } else {
        castling_label_->setVisible(false);
    }

    status_message_label_->setText(QString::fromStdString(session.statusMessage()));

    if (session.selectedRow() >= 0 && session.selectedCol() >= 0) {
        const int value = session.pieceAt(session.selectedRow(), session.selectedCol());
        QString text = QString("Selected (%1, %2)").arg(session.selectedRow()).arg(session.selectedCol());
        if (value != 0) {
            text += QString(" - Piece: %1, Color: %2").arg(piece_symbol(value), color_name(value > 0 ? 1 : -1));
        } else {
            text += " - Empty square";
        }
        selected_label_->setText(text);
    } else {
        selected_label_->setText("Selected: none");
    }
}

void GamePage::appendChatMessage(const QString& line) {
    chat_view_->append(line);
}

void GamePage::clearChat() {
    chat_view_->clear();
}
