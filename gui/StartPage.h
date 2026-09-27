#pragma once

#include <QWidget>

// Landing page shown when the application starts.
class StartPage : public QWidget {
    Q_OBJECT

public:
    explicit StartPage(QWidget* parent = nullptr);

signals:
    void startRequested();
    void exitRequested();
};
