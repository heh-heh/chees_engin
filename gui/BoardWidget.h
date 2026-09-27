#pragma once

#include <QWidget>

#include "GameSession.h"

// Custom-painted 8x8 chess board. Rendering state (pieces, selection,
// highlights) is read directly from the attached GameSession on each repaint.
class BoardWidget : public QWidget {
    Q_OBJECT

public:
    explicit BoardWidget(QWidget* parent = nullptr);

    void setSession(GameSession* session);
    void setLocked(bool locked);
    QSize sizeHint() const override;

signals:
    void squareClicked(int row, int col);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    int squareSize() const;
    QRect boardRect() const;

    GameSession* session_ = nullptr;
    bool locked_ = false;
};
