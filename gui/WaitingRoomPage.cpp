#include "WaitingRoomPage.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

WaitingRoomPage::WaitingRoomPage(QWidget* parent) : QWidget(parent) {
    mode_title_ = new QLabel();
    mode_title_->setStyleSheet("font-size: 22px; font-weight: bold;");
    mode_subtitle_ = new QLabel();
    mode_subtitle_->setStyleSheet("color: #9aa5b1;");

    room_info_label_ = new QLabel();
    room_info_label_->setWordWrap(true);

    status_label_ = new QLabel();
    status_label_->setWordWrap(true);

    readiness_bar_ = new QProgressBar();
    readiness_bar_->setRange(0, 100);
    readiness_bar_->setTextVisible(true);
    readiness_bar_->setFormat("Searching");
    readiness_bar_->setValue(65);

    ready_checkbox_ = new QCheckBox("Ready");
    ready_checkbox_->setEnabled(false);
    connect(ready_checkbox_, &QCheckBox::toggled, this, &WaitingRoomPage::readyToggled);

    opponent_ready_label_ = new QLabel("Opponent: Not ready");

    start_match_button_ = new QPushButton("Start Match");
    start_match_button_->setMinimumSize(220, 45);
    start_match_button_->setEnabled(false);
    connect(start_match_button_, &QPushButton::clicked, this, &WaitingRoomPage::startMatchRequested);

    auto* ready_row = new QHBoxLayout();
    ready_row->addWidget(ready_checkbox_);
    ready_row->addWidget(opponent_ready_label_);
    ready_row->addStretch();
    ready_row->addWidget(start_match_button_);

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

    auto* back_button = new QPushButton("Back to Lobby");
    back_button->setMinimumSize(180, 45);
    connect(back_button, &QPushButton::clicked, this, &WaitingRoomPage::backRequested);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(mode_title_);
    layout->addWidget(mode_subtitle_);
    layout->addSpacing(8);
    layout->addWidget(room_info_label_);
    layout->addSpacing(8);
    layout->addWidget(new QLabel("Status"));
    layout->addWidget(status_label_);
    layout->addWidget(new QLabel("Match readiness"));
    layout->addWidget(readiness_bar_);
    layout->addLayout(ready_row);
    layout->addSpacing(8);
    layout->addWidget(new QLabel("MATCH CHAT"));
    layout->addWidget(chat_view_);
    layout->addLayout(chat_row);
    layout->addStretch();
    layout->addWidget(back_button, 0, Qt::AlignLeft);
}

void WaitingRoomPage::setHostMode(bool isHost) {
    is_host_ = isHost;
    if (isHost) {
        mode_title_->setText("Room Host");
        mode_subtitle_->setText("Waiting for another player to join this room.");
    } else {
        mode_title_->setText("Join Room");
        mode_subtitle_->setText("Connecting to the host and waiting for match setup.");
    }
}

void WaitingRoomPage::setRoomInfo(const QString& roomCode, const QString& lanAddress, const QString& targetHost, quint16 port, bool isRelay) {
    if (isRelay) {
        room_info_label_->setText(QString("Room code: %1\nRelay server: %2:%3\nShare only the room code with the other player.")
            .arg(roomCode, targetHost, QString::number(port)));
    } else if (is_host_) {
        room_info_label_->setText(QString("Room code: %1\nListening on: 0.0.0.0 (all network interfaces)\nLAN address: %2\nPort: %3\nGive the LAN address and port to the other player.")
            .arg(roomCode, lanAddress, QString::number(port)));
    } else {
        room_info_label_->setText(QString("Room code: %1\nTarget host: %2\nPort: %3")
            .arg(roomCode, targetHost, QString::number(port)));
    }
}

void WaitingRoomPage::setStatusMessage(const QString& message) {
    status_label_->setText(message);
}

void WaitingRoomPage::setOpponentConnected(bool connected) {
    readiness_bar_->setValue(connected ? 100 : 65);
    readiness_bar_->setFormat(connected ? "Opponent connected" : "Searching");
    ready_checkbox_->setEnabled(connected);
}

void WaitingRoomPage::setReadyState(bool localReady, bool opponentReady, bool canStart) {
    QSignalBlocker blocker(ready_checkbox_);
    ready_checkbox_->setChecked(localReady);
    opponent_ready_label_->setText(opponentReady ? "Opponent: Ready" : "Opponent: Not ready");
    start_match_button_->setVisible(is_host_);
    start_match_button_->setEnabled(is_host_ && canStart);
}

void WaitingRoomPage::appendChatMessage(const QString& line) {
    chat_view_->append(line);
}

void WaitingRoomPage::clearChat() {
    chat_view_->clear();
}
