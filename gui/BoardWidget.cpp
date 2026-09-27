#include "BoardWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>

#include <cmath>

namespace {

constexpr int kBoardDimension = 8;

QString piece_glyph(int value) {
    if (value == 0) {
        return QString();
    }
    const bool white = value > 0;
    switch (std::abs(value)) {
        case 1: return QString(QChar(white ? 0x2659 : 0x265F));
        case 2: return QString(QChar(white ? 0x2658 : 0x265E));
        case 3: return QString(QChar(white ? 0x2657 : 0x265D));
        case 4: return QString(QChar(white ? 0x2656 : 0x265C));
        case 5: return QString(QChar(white ? 0x2655 : 0x265B));
        case 6: return QString(QChar(white ? 0x2654 : 0x265A));
        default: return QString();
    }
}

}  // namespace

BoardWidget::BoardWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(320, 320);
}

void BoardWidget::setSession(GameSession* session) {
    session_ = session;
    update();
}

void BoardWidget::setLocked(bool locked) {
    locked_ = locked;
}

QSize BoardWidget::sizeHint() const {
    return QSize(560, 560);
}

int BoardWidget::squareSize() const {
    return std::min(width(), height()) / kBoardDimension;
}

QRect BoardWidget::boardRect() const {
    const int size = squareSize() * kBoardDimension;
    const int x = (width() - size) / 2;
    const int y = (height() - size) / 2;
    return QRect(x, y, size, size);
}

void BoardWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(20, 22, 26));

    if (session_ == nullptr) {
        return;
    }

    const QRect board_rect = boardRect();
    const int size = squareSize();
    const auto king_pos = session_->kingPosition(session_->currentColor());
    const bool current_side_in_check = session_->isKingInCheck(session_->currentColor());

    QFont piece_font = font();
    piece_font.setPointSizeF(size * 0.5);
    piece_font.setBold(true);

    for (int row = 0; row < kBoardDimension; ++row) {
        for (int col = 0; col < kBoardDimension; ++col) {
            const QRect cell(board_rect.x() + col * size, board_rect.y() + row * size, size, size);

            const bool light_square = (row + col) % 2 == 0;
            const bool selected = row == session_->selectedRow() && col == session_->selectedCol();
            const bool legal_target = session_->isLegalTarget(row, col);
            const bool checked_king_square = current_side_in_check && row == king_pos.first && col == king_pos.second;

            QColor cell_color = light_square ? QColor(230, 224, 209) : QColor(143, 122, 89);
            if (checked_king_square) {
                cell_color = QColor(240, 61, 61);
            } else if (selected) {
                cell_color = QColor(240, 204, 56);
            } else if (legal_target) {
                const int target_value = session_->pieceAt(row, col);
                cell_color = target_value == 0 ? QColor(56, 179, 107) : QColor(214, 71, 56);
            }

            painter.fillRect(cell, cell_color);
            painter.setPen(QColor(0, 0, 0, 60));
            painter.drawRect(cell);

            const int value = session_->pieceAt(row, col);
            const QString glyph = piece_glyph(value);
            if (!glyph.isEmpty()) {
                const QColor fill_color = value > 0 ? Qt::white : Qt::black;
                const QColor outline_color = value > 0 ? Qt::black : Qt::white;

                painter.setFont(piece_font);
                for (const auto& offset : {QPoint(-1, 0), QPoint(1, 0), QPoint(0, -1), QPoint(0, 1)}) {
                    painter.setPen(outline_color);
                    painter.drawText(cell.translated(offset), Qt::AlignCenter, glyph);
                }
                painter.setPen(fill_color);
                painter.drawText(cell, Qt::AlignCenter, glyph);
            }
        }
    }
}

void BoardWidget::mousePressEvent(QMouseEvent* event) {
    if (locked_ || session_ == nullptr) {
        return;
    }

    const QRect board_rect = boardRect();
    if (!board_rect.contains(event->pos())) {
        return;
    }

    const int size = squareSize();
    const int col = (event->pos().x() - board_rect.x()) / size;
    const int row = (event->pos().y() - board_rect.y()) / size;
    if (row < 0 || row >= kBoardDimension || col < 0 || col >= kBoardDimension) {
        return;
    }

    emit squareClicked(row, col);
}

void BoardWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    update();
}
