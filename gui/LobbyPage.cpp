#include "LobbyPage.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

LobbyPage::LobbyPage(QWidget* parent) : QWidget(parent) {
    auto* title = new QLabel("Lobby");
    title->setStyleSheet("font-size: 24px; font-weight: bold;");

    auto* subtitle = new QLabel("Choose a game mode and prepare a match.");
    subtitle->setStyleSheet("color: #9aa5b1;");

    auto* single_player_button = new QPushButton("Single Player");
    single_player_button->setMinimumSize(230, 90);
    connect(single_player_button, &QPushButton::clicked, this, &LobbyPage::continueRequested);

    auto* multiplayer_button = new QPushButton("Multiplayer");
    multiplayer_button->setMinimumSize(230, 90);
    connect(multiplayer_button, &QPushButton::clicked, this, &LobbyPage::continueRequested);

    auto* mode_row = new QHBoxLayout();
    mode_row->addWidget(single_player_button);
    mode_row->addWidget(multiplayer_button);
    mode_row->addStretch();

    auto* status_box = new QGroupBox("Match Lobby");
    auto* status_layout = new QVBoxLayout(status_box);
    status_layout->addWidget(new QLabel("- Quick start: local single-player"));
    status_layout->addWidget(new QLabel("- Multiplayer: host and join a match"));
    status_layout->addWidget(new QLabel("- Status: waiting in lobby"));

    auto* back_button = new QPushButton("Back");
    back_button->setMinimumSize(160, 40);
    connect(back_button, &QPushButton::clicked, this, &LobbyPage::backRequested);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addSpacing(10);
    layout->addLayout(mode_row);
    layout->addSpacing(10);
    layout->addWidget(status_box);
    layout->addStretch();
    layout->addWidget(back_button, 0, Qt::AlignLeft);
}
