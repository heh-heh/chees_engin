#include "StartPage.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

StartPage::StartPage(QWidget* parent) : QWidget(parent) {
    auto* title = new QLabel("Chess Engine");
    title->setStyleSheet("font-size: 28px; font-weight: bold;");

    auto* subtitle = new QLabel("Lobby-based GUI for multiplayer-ready chess");
    subtitle->setStyleSheet("color: #9aa5b1;");

    auto* description = new QLabel(
        "This is the start screen. Move to the lobby to choose single-player or multiplayer mode.");
    description->setWordWrap(true);

    auto* start_button = new QPushButton("Start");
    start_button->setMinimumSize(200, 50);
    connect(start_button, &QPushButton::clicked, this, &StartPage::startRequested);

    auto* exit_button = new QPushButton("Exit");
    exit_button->setMinimumSize(200, 50);
    connect(exit_button, &QPushButton::clicked, this, &StartPage::exitRequested);

    auto* button_row = new QHBoxLayout();
    button_row->addWidget(start_button);
    button_row->addWidget(exit_button);
    button_row->addStretch();

    auto* layout = new QVBoxLayout(this);
    layout->addSpacing(40);
    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addSpacing(20);
    layout->addWidget(description);
    layout->addSpacing(20);
    layout->addLayout(button_row);
    layout->addStretch();
}
