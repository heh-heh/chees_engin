#pragma once

#include <QMainWindow>
#include <QElapsedTimer>

#include <chrono>

#include "GameSession.h"
#include "NetworkSession.h"

class QStackedWidget;
class StartPage;
class LobbyPage;
class ModeSelectPage;
class WaitingRoomPage;
class GamePage;
class QTimer;

// Top-level window that owns the game/network state and wires the
// Start -> Lobby -> ModeSelect -> WaitingRoom -> Game screen flow.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onModeSelectSinglePlayer();
    void onModeSelectHost(quint16 port);
    void onModeSelectJoin(const QString& host, const QString& roomCode, quint16 port);
    void onModeSelectHostRelay(const QString& relayHost, quint16 relayPort, const QString& roomCode);
    void onModeSelectJoinRelay(const QString& relayHost, quint16 relayPort, const QString& roomCode);

    void onNetworkOpponentConnected();
    void onNetworkOpponentDisconnected();
    void onNetworkMessage(const QString& message);
    void onNetworkConnectionFailed(const QString& error);

    void onWaitingRoomReadyToggled(bool ready);
    void onWaitingRoomStartMatch();
    void onWaitingRoomChatSent(const QString& text);
    void onWaitingRoomBack();

    void onGameSquareClicked(int row, int col);
    void onGameNewGame();
    void onGameLobby();
    void onGameChatSent(const QString& text);

    void onUiTick();

private:
    enum class Screen { Start, Lobby, ModeSelect, WaitingRoom, Game };
    enum class MultiplayerMode { None, Host, Client };

    void showScreen(Screen screen);
    NetworkSession* createNetworkSession();
    void stopNetworkSession();
    bool sendNetworkMessage(const QString& message);
    void broadcastMatchState(bool force);
    void appendChatMessage(const QString& sender, const QString& text);
    void refreshWaitingRoomPage();
    void refreshGamePage();
    void handleRemoteMove(const QString& message);
    void showGameOverDialogIfNeeded();

    QStackedWidget* stack_ = nullptr;
    StartPage* start_page_ = nullptr;
    LobbyPage* lobby_page_ = nullptr;
    ModeSelectPage* mode_select_page_ = nullptr;
    WaitingRoomPage* waiting_room_page_ = nullptr;
    GamePage* game_page_ = nullptr;

    QTimer* ui_timer_ = nullptr;
    QElapsedTimer state_broadcast_timer_;

    GameSession session_;
    NetworkSession* network_ = nullptr;

    MultiplayerMode multiplayer_mode_ = MultiplayerMode::None;
    int local_player_color_ = 1;
    QString room_code_ = "ROOM-9001";
    QString network_host_ = "127.0.0.1";
    quint16 network_port_ = 9001;
    bool using_relay_ = false;

    bool local_ready_ = false;
    bool opponent_ready_ = false;
    bool remote_start_requested_ = false;
    bool has_remote_match_state_ = false;
    std::chrono::milliseconds synced_turn_elapsed_{0};
    int synced_score_ = 0;
    QElapsedTimer last_remote_state_timer_;
    bool game_over_dialog_shown_ = false;
};
