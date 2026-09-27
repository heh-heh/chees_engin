#pragma once

#include <QWidget>

// Lobby screen; both play-mode buttons proceed to the mode-select screen,
// matching the original behavior where the actual mode is chosen there.
class LobbyPage : public QWidget {
    Q_OBJECT

public:
    explicit LobbyPage(QWidget* parent = nullptr);

signals:
    void continueRequested();
    void backRequested();
};
