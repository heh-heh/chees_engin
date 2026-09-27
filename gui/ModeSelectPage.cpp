#include "ModeSelectPage.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

ModeSelectPage::ModeSelectPage(QWidget* parent) : QWidget(parent) {
    auto* title = new QLabel("Game Mode Selection");
    title->setStyleSheet("font-size: 24px; font-weight: bold;");

    auto* subtitle = new QLabel("Choose how the match will begin.");
    subtitle->setStyleSheet("color: #9aa5b1;");

    auto* single_player_button = new QPushButton("Start Single Player");
    single_player_button->setMinimumSize(270, 90);
    connect(single_player_button, &QPushButton::clicked, this, &ModeSelectPage::singlePlayerRequested);

    auto* host_button = new QPushButton("Host Match");
    host_button->setMinimumSize(200, 90);

    auto* join_button = new QPushButton("Join Match");
    join_button->setMinimumSize(200, 90);

    auto* button_row = new QHBoxLayout();
    button_row->addWidget(single_player_button);
    button_row->addWidget(host_button);
    button_row->addWidget(join_button);
    button_row->addStretch();

    host_input_ = new QLineEdit("127.0.0.1");
    room_code_input_ = new QLineEdit("ROOM-9001");
    port_input_ = new QSpinBox();
    port_input_->setRange(1, 65535);
    port_input_->setValue(9001);

    auto* network_form = new QGroupBox("Network Options (Direct LAN)");
    auto* form_layout = new QVBoxLayout(network_form);
    form_layout->addWidget(new QLabel("Host"));
    form_layout->addWidget(host_input_);
    form_layout->addWidget(new QLabel("Room Code"));
    form_layout->addWidget(room_code_input_);
    form_layout->addWidget(new QLabel("Port"));
    form_layout->addWidget(port_input_);

    use_relay_checkbox_ = new QCheckBox("Use relay server (no port forwarding needed)");
    relay_host_input_ = new QLineEdit();
    relay_host_input_->setPlaceholderText("relay.example.com");
    relay_port_input_ = new QSpinBox();
    relay_port_input_->setRange(1, 65535);
    relay_port_input_->setValue(9100);

    auto* relay_form = new QGroupBox("Relay Server (over the internet)");
    auto* relay_layout = new QVBoxLayout(relay_form);
    relay_layout->addWidget(use_relay_checkbox_);
    relay_layout->addWidget(new QLabel("Relay Address"));
    relay_layout->addWidget(relay_host_input_);
    relay_layout->addWidget(new QLabel("Relay Port"));
    relay_layout->addWidget(relay_port_input_);
    relay_layout->addWidget(new QLabel("Room Code is shared with Network Options above."));

    status_label_ = new QLabel("Mode: None");

    auto* info_box = new QGroupBox("Current Setup");
    auto* info_layout = new QVBoxLayout(info_box);
    info_layout->addWidget(new QLabel("- Single Player: local board and turn logic"));
    info_layout->addWidget(new QLabel("- Multiplayer: host and client with TCP socket sync"));
    info_layout->addWidget(new QLabel("- Waiting room appears until the other player connects"));

    auto* back_button = new QPushButton("Back to Lobby");
    back_button->setMinimumSize(200, 40);
    connect(back_button, &QPushButton::clicked, this, &ModeSelectPage::backRequested);

    connect(host_button, &QPushButton::clicked, this, [this]() {
        if (use_relay_checkbox_->isChecked()) {
            emit hostViaRelayRequested(relay_host_input_->text(), static_cast<quint16>(relay_port_input_->value()), room_code_input_->text());
        } else {
            emit hostRequested(static_cast<quint16>(port_input_->value()));
        }
    });
    connect(join_button, &QPushButton::clicked, this, [this]() {
        if (use_relay_checkbox_->isChecked()) {
            emit joinViaRelayRequested(relay_host_input_->text(), static_cast<quint16>(relay_port_input_->value()), room_code_input_->text());
        } else {
            emit joinRequested(host_input_->text(), room_code_input_->text(), static_cast<quint16>(port_input_->value()));
        }
    });

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addSpacing(10);
    layout->addLayout(button_row);
    layout->addSpacing(10);
    layout->addWidget(network_form);
    layout->addWidget(relay_form);
    layout->addWidget(status_label_);
    layout->addWidget(info_box);
    layout->addStretch();
    layout->addWidget(back_button, 0, Qt::AlignLeft);
}

void ModeSelectPage::setError(const QString& error) {
    if (error.isEmpty()) {
        return;
    }
    status_label_->setStyleSheet("color: #ff6666;");
    status_label_->setText(error);
}

void ModeSelectPage::setModeLabel(const QString& modeText) {
    status_label_->setStyleSheet("color: #80e6b3;");
    status_label_->setText(modeText);
}
