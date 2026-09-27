#include "MainWindow.h"

#include <QMessageBox>
#include <QPushButton>
#include <QRandomGenerator>
#include <QStackedWidget>
#include <QTimer>

#include "../src/network/chess_protocol.hpp"
#include "BoardWidget.h"
#include "GamePage.h"
#include "LobbyPage.h"
#include "ModeSelectPage.h"
#include "StartPage.h"
#include "WaitingRoomPage.h"

namespace {

struct RemoteMove {
    int from_row = -1;
    int from_col = -1;
    int to_row = -1;
    int to_col = -1;
    int color = 0;
    bool valid = false;
};

QString build_remote_move_message(int fromRow, int fromCol, int toRow, int toCol, int color) {
    return QString("MOVE|%1|%2|%3|%4|%5").arg(fromRow).arg(fromCol).arg(toRow).arg(toCol).arg(color);
}

bool parse_remote_move_message(const QString& message, RemoteMove& move) {
    if (!message.startsWith("MOVE|")) {
        return false;
    }
    const QStringList parts = message.mid(5).split('|');
    if (parts.size() != 5) {
        return false;
    }

    bool ok = true;
    move.from_row = parts[0].toInt(&ok);
    if (!ok) return false;
    move.from_col = parts[1].toInt(&ok);
    if (!ok) return false;
    move.to_row = parts[2].toInt(&ok);
    if (!ok) return false;
    move.to_col = parts[3].toInt(&ok);
    if (!ok) return false;
    move.color = parts[4].toInt(&ok);
    if (!ok) return false;

    move.valid = true;
    return true;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Chess Engine GUI");
    resize(1280, 800);

    stack_ = new QStackedWidget(this);
    start_page_ = new StartPage();
    lobby_page_ = new LobbyPage();
    mode_select_page_ = new ModeSelectPage();
    waiting_room_page_ = new WaitingRoomPage();
    game_page_ = new GamePage();

    stack_->addWidget(start_page_);
    stack_->addWidget(lobby_page_);
    stack_->addWidget(mode_select_page_);
    stack_->addWidget(waiting_room_page_);
    stack_->addWidget(game_page_);
    setCentralWidget(stack_);

    connect(start_page_, &StartPage::startRequested, this, [this]() { showScreen(Screen::Lobby); });
    connect(start_page_, &StartPage::exitRequested, this, &QMainWindow::close);

    connect(lobby_page_, &LobbyPage::continueRequested, this, [this]() { showScreen(Screen::ModeSelect); });
    connect(lobby_page_, &LobbyPage::backRequested, this, [this]() { showScreen(Screen::Start); });

    connect(mode_select_page_, &ModeSelectPage::singlePlayerRequested, this, &MainWindow::onModeSelectSinglePlayer);
    connect(mode_select_page_, &ModeSelectPage::hostRequested, this, &MainWindow::onModeSelectHost);
    connect(mode_select_page_, &ModeSelectPage::joinRequested, this, &MainWindow::onModeSelectJoin);
    connect(mode_select_page_, &ModeSelectPage::hostViaRelayRequested, this, &MainWindow::onModeSelectHostRelay);
    connect(mode_select_page_, &ModeSelectPage::joinViaRelayRequested, this, &MainWindow::onModeSelectJoinRelay);
    connect(mode_select_page_, &ModeSelectPage::backRequested, this, [this]() { showScreen(Screen::Lobby); });

    connect(waiting_room_page_, &WaitingRoomPage::readyToggled, this, &MainWindow::onWaitingRoomReadyToggled);
    connect(waiting_room_page_, &WaitingRoomPage::startMatchRequested, this, &MainWindow::onWaitingRoomStartMatch);
    connect(waiting_room_page_, &WaitingRoomPage::chatMessageSent, this, &MainWindow::onWaitingRoomChatSent);
    connect(waiting_room_page_, &WaitingRoomPage::backRequested, this, &MainWindow::onWaitingRoomBack);

    connect(game_page_, &GamePage::squareClicked, this, &MainWindow::onGameSquareClicked);
    connect(game_page_, &GamePage::newGameRequested, this, &MainWindow::onGameNewGame);
    connect(game_page_, &GamePage::lobbyRequested, this, &MainWindow::onGameLobby);
    connect(game_page_, &GamePage::chatMessageSent, this, &MainWindow::onGameChatSent);

    game_page_->boardWidget()->setSession(&session_);

    ui_timer_ = new QTimer(this);
    connect(ui_timer_, &QTimer::timeout, this, &MainWindow::onUiTick);
    ui_timer_->start(200);

    state_broadcast_timer_.start();
    last_remote_state_timer_.start();

    showScreen(Screen::Start);
}

void MainWindow::showScreen(Screen screen) {
    switch (screen) {
        case Screen::Start:
            stack_->setCurrentWidget(start_page_);
            break;
        case Screen::Lobby:
            stack_->setCurrentWidget(lobby_page_);
            break;
        case Screen::ModeSelect:
            stack_->setCurrentWidget(mode_select_page_);
            break;
        case Screen::WaitingRoom:
            refreshWaitingRoomPage();
            stack_->setCurrentWidget(waiting_room_page_);
            break;
        case Screen::Game:
            game_page_->setMultiplayer(session_.isMultiplayer());
            game_page_->setBoardLocked(false);
            game_over_dialog_shown_ = false;
            refreshGamePage();
            stack_->setCurrentWidget(game_page_);
            break;
    }
}

void MainWindow::onModeSelectSinglePlayer() {
    multiplayer_mode_ = MultiplayerMode::None;
    local_player_color_ = 1;
    stopNetworkSession();
    session_.reset(false);
    showScreen(Screen::Game);
}

NetworkSession* MainWindow::createNetworkSession() {
    stopNetworkSession();
    auto* network = new NetworkSession(this);
    connect(network, &NetworkSession::opponentConnected, this, &MainWindow::onNetworkOpponentConnected);
    connect(network, &NetworkSession::opponentDisconnected, this, &MainWindow::onNetworkOpponentDisconnected);
    connect(network, &NetworkSession::messageReceived, this, &MainWindow::onNetworkMessage);
    connect(network, &NetworkSession::connectionFailed, this, &MainWindow::onNetworkConnectionFailed);
    return network;
}

void MainWindow::onModeSelectHost(quint16 port) {
    multiplayer_mode_ = MultiplayerMode::Host;
    local_player_color_ = 1;
    using_relay_ = false;
    network_port_ = port;
    room_code_ = QString("ROOM-%1").arg(port);

    network_ = createNetworkSession();

    QString error;
    if (network_->hostGame(port, error)) {
        mode_select_page_->setModeLabel("Mode: Host");
        waiting_room_page_->setHostMode(true);
        waiting_room_page_->clearChat();
        local_ready_ = false;
        opponent_ready_ = false;
        remote_start_requested_ = false;
        has_remote_match_state_ = false;
        showScreen(Screen::WaitingRoom);
        waiting_room_page_->setStatusMessage("Room created. Waiting for opponent...");
    } else {
        mode_select_page_->setError("Host failed: " + error);
    }
}

void MainWindow::onModeSelectJoin(const QString& host, const QString& roomCode, quint16 port) {
    multiplayer_mode_ = MultiplayerMode::Client;
    local_player_color_ = -1;
    using_relay_ = false;
    network_host_ = host.isEmpty() ? "127.0.0.1" : host;
    network_port_ = port;
    room_code_ = roomCode.isEmpty() ? QString("ROOM-%1").arg(port) : roomCode;

    network_ = createNetworkSession();

    QString error;
    if (network_->joinGame(network_host_, port, error)) {
        waiting_room_page_->setHostMode(false);
        waiting_room_page_->clearChat();
        local_ready_ = false;
        opponent_ready_ = false;
        remote_start_requested_ = false;
        has_remote_match_state_ = false;
        showScreen(Screen::WaitingRoom);
        waiting_room_page_->setStatusMessage("Connecting to host...");
    } else {
        mode_select_page_->setError("Connection failed: " + error);
    }
}

void MainWindow::onModeSelectHostRelay(const QString& relayHost, quint16 relayPort, const QString& roomCode) {
    multiplayer_mode_ = MultiplayerMode::Host;
    local_player_color_ = 1;
    using_relay_ = true;
    network_host_ = relayHost;
    network_port_ = relayPort;
    room_code_ = roomCode.isEmpty() ? QString("ROOM-%1").arg(QRandomGenerator::global()->bounded(1000, 9999)) : roomCode;

    network_ = createNetworkSession();

    QString error;
    if (network_->hostViaRelay(relayHost, relayPort, room_code_, error)) {
        mode_select_page_->setModeLabel("Mode: Host (Relay)");
        waiting_room_page_->setHostMode(true);
        waiting_room_page_->clearChat();
        local_ready_ = false;
        opponent_ready_ = false;
        remote_start_requested_ = false;
        has_remote_match_state_ = false;
        showScreen(Screen::WaitingRoom);
        waiting_room_page_->setStatusMessage("Connected to relay server. Waiting for opponent...");
    } else {
        mode_select_page_->setError(error);
    }
}

void MainWindow::onModeSelectJoinRelay(const QString& relayHost, quint16 relayPort, const QString& roomCode) {
    multiplayer_mode_ = MultiplayerMode::Client;
    local_player_color_ = -1;
    using_relay_ = true;
    network_host_ = relayHost;
    network_port_ = relayPort;
    room_code_ = roomCode;

    network_ = createNetworkSession();

    QString error;
    if (network_->joinViaRelay(relayHost, relayPort, roomCode, error)) {
        waiting_room_page_->setHostMode(false);
        waiting_room_page_->clearChat();
        local_ready_ = false;
        opponent_ready_ = false;
        remote_start_requested_ = false;
        has_remote_match_state_ = false;
        showScreen(Screen::WaitingRoom);
        waiting_room_page_->setStatusMessage("Connecting to relay server...");
    } else {
        mode_select_page_->setError(error);
    }
}

void MainWindow::onNetworkOpponentConnected() {
    waiting_room_page_->setStatusMessage(
        multiplayer_mode_ == MultiplayerMode::Host ? "Opponent joined. Match is ready." : "Connected to host. Match is ready.");
    refreshWaitingRoomPage();
}

void MainWindow::onNetworkOpponentDisconnected() {
    if (stack_->currentWidget() == game_page_) {
        session_.setStatusMessage("Opponent disconnected.");
        refreshGamePage();
    } else {
        waiting_room_page_->setStatusMessage("Opponent disconnected.");
        refreshWaitingRoomPage();
    }
}

void MainWindow::onNetworkConnectionFailed(const QString& error) {
    if (stack_->currentWidget() == mode_select_page_) {
        mode_select_page_->setError(error);
    } else {
        waiting_room_page_->setStatusMessage("Connection failed: " + error);
    }
}

void MainWindow::onNetworkMessage(const QString& message) {
    RemoteMove move;
    if (parse_remote_move_message(message, move)) {
        handleRemoteMove(message);
        return;
    }

    bool ready = false;
    std::string ready_std = message.toStdString();
    if (chess::parse_ready_message(ready_std, ready)) {
        opponent_ready_ = ready;
        waiting_room_page_->setStatusMessage(ready ? "Opponent is ready." : "Opponent is not ready.");
        refreshWaitingRoomPage();
        return;
    }

    if (chess::is_match_start_message(ready_std)) {
        if (multiplayer_mode_ == MultiplayerMode::Client) {
            session_.reset(true);
            remote_start_requested_ = false;
            showScreen(Screen::Game);
        }
        return;
    }

    std::string chat_text;
    if (chess::parse_chat_message(ready_std, chat_text)) {
        appendChatMessage("Opponent", QString::fromStdString(chat_text));
        return;
    }

    if (multiplayer_mode_ == MultiplayerMode::Client) {
        chess::MatchStateMessage state;
        if (chess::parse_match_state_message(ready_std, state)) {
            synced_turn_elapsed_ = std::chrono::milliseconds(state.turn_elapsed_ms);
            synced_score_ = state.score;
            last_remote_state_timer_.restart();
            has_remote_match_state_ = true;
        }
    }
}

void MainWindow::handleRemoteMove(const QString& message) {
    RemoteMove move;
    if (!parse_remote_move_message(message, move) || !move.valid) {
        return;
    }

    const int expected_color = multiplayer_mode_ == MultiplayerMode::Host ? -1 : 1;
    if (move.color != expected_color) {
        return;
    }

    if (session_.applyRemoteMove(move.from_row, move.from_col, move.to_row, move.to_col, move.color)) {
        if (multiplayer_mode_ == MultiplayerMode::Host) {
            broadcastMatchState(true);
        }
        refreshGamePage();
    }
}

void MainWindow::onWaitingRoomReadyToggled(bool ready) {
    local_ready_ = ready;
    sendNetworkMessage(QString::fromStdString(chess::build_ready_message(ready)));
    waiting_room_page_->setStatusMessage(ready ? "You are ready." : "You are not ready.");
    refreshWaitingRoomPage();
}

void MainWindow::onWaitingRoomStartMatch() {
    session_.reset(true);
    sendNetworkMessage(QString::fromStdString(chess::build_match_start_message()));
    broadcastMatchState(true);
    showScreen(Screen::Game);
}

void MainWindow::onWaitingRoomChatSent(const QString& text) {
    if (sendNetworkMessage(QString::fromStdString(chess::build_chat_message(text.toStdString())))) {
        appendChatMessage("You", text);
    }
}

void MainWindow::onWaitingRoomBack() {
    stopNetworkSession();
    showScreen(Screen::Lobby);
}

void MainWindow::onGameSquareClicked(int row, int col) {
    if (session_.isGameOver()) {
        return;
    }

    const ClickResult result = session_.handleClick(row, col, local_player_color_);
    if (result.outcome == MoveOutcome::Moved) {
        if (session_.isMultiplayer() && network_ != nullptr && network_->isConnected()) {
            sendNetworkMessage(build_remote_move_message(result.fromRow, result.fromCol, result.toRow, result.toCol, result.movingColor));
        }
    }

    refreshGamePage();
    showGameOverDialogIfNeeded();
}

void MainWindow::onGameNewGame() {
    session_.reset(session_.isMultiplayer());
    game_over_dialog_shown_ = false;
    game_page_->setBoardLocked(false);
    refreshGamePage();
}

void MainWindow::onGameLobby() {
    session_.setStatusMessage("Returned to lobby.");
    stopNetworkSession();
    showScreen(Screen::Lobby);
}

void MainWindow::onGameChatSent(const QString& text) {
    if (sendNetworkMessage(QString::fromStdString(chess::build_chat_message(text.toStdString())))) {
        appendChatMessage("You", text);
    }
}

void MainWindow::onUiTick() {
    if (stack_->currentWidget() == game_page_) {
        refreshGamePage();
        if (session_.isMultiplayer()) {
            broadcastMatchState(false);
        }
    } else if (stack_->currentWidget() == waiting_room_page_) {
        refreshWaitingRoomPage();
    }
}

void MainWindow::stopNetworkSession() {
    if (network_ != nullptr) {
        network_->stop();
        network_->deleteLater();
        network_ = nullptr;
    }
    local_ready_ = false;
    opponent_ready_ = false;
    remote_start_requested_ = false;
    has_remote_match_state_ = false;
    waiting_room_page_->clearChat();
    game_page_->clearChat();
}

bool MainWindow::sendNetworkMessage(const QString& message) {
    if (network_ == nullptr || !network_->isConnected() || !network_->sendMessage(message)) {
        session_.setStatusMessage("Network message could not be sent.");
        return false;
    }
    return true;
}

void MainWindow::broadcastMatchState(bool force) {
    if (!session_.isMultiplayer() || multiplayer_mode_ != MultiplayerMode::Host || network_ == nullptr || !network_->isConnected()) {
        return;
    }

    if (!force && state_broadcast_timer_.isValid() && state_broadcast_timer_.elapsed() < 1000) {
        return;
    }

    const QString state_message = QString::fromStdString(chess::build_match_state_message(
        session_.turn(), session_.currentColor(), session_.turnElapsed().count(), session_.evaluateBoard()));
    if (sendNetworkMessage(state_message)) {
        state_broadcast_timer_.restart();
    }
}

void MainWindow::appendChatMessage(const QString& sender, const QString& text) {
    const QString line = sender + ": " + text;
    waiting_room_page_->appendChatMessage(line);
    game_page_->appendChatMessage(line);
}

void MainWindow::refreshWaitingRoomPage() {
    const bool is_host = multiplayer_mode_ == MultiplayerMode::Host;
    waiting_room_page_->setHostMode(is_host);
    const QString lan_address = NetworkSession::localIPv4Address();
    waiting_room_page_->setRoomInfo(room_code_, lan_address, network_host_, network_port_, using_relay_);

    const bool connected = network_ != nullptr && network_->isConnected();
    waiting_room_page_->setOpponentConnected(connected);

    const bool can_start = is_host && connected && local_ready_ && opponent_ready_;
    waiting_room_page_->setReadyState(local_ready_, opponent_ready_, can_start);
}

void MainWindow::refreshGamePage() {
    std::chrono::milliseconds turn_elapsed = session_.turnElapsed();
    int score = session_.evaluateBoard();

    if (session_.isMultiplayer() && multiplayer_mode_ == MultiplayerMode::Client && has_remote_match_state_) {
        turn_elapsed = synced_turn_elapsed_ + std::chrono::milliseconds(last_remote_state_timer_.elapsed());
        score = synced_score_;
    }

    game_page_->refresh(session_, turn_elapsed, score);
}

void MainWindow::showGameOverDialogIfNeeded() {
    if (!session_.isGameOver() || game_over_dialog_shown_) {
        return;
    }

    game_over_dialog_shown_ = true;
    game_page_->setBoardLocked(true);

    QMessageBox box(this);
    box.setWindowTitle("Game Over");
    box.setText(QString("Winner: %1\nCheckmate was reached. Choose your next action.")
        .arg(session_.winnerColor() == 1 ? "White" : "Black"));
    QPushButton* play_again = box.addButton("Play Again", QMessageBox::AcceptRole);
    box.addButton("Back to Lobby", QMessageBox::RejectRole);
    box.exec();

    if (box.clickedButton() == play_again) {
        session_.reset(session_.isMultiplayer());
        game_over_dialog_shown_ = false;
        game_page_->setBoardLocked(false);
        refreshGamePage();
    } else {
        session_.setStatusMessage("Returned to lobby.");
        stopNetworkSession();
        showScreen(Screen::Lobby);
    }
}
