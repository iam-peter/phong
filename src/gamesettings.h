#ifndef GAMESETTINGS_H
#define GAMESETTINGS_H

#include "computerplayer.h"

#include <QObject>
#include <QSettings>
#include <QtQml/qqmlregistration.h>

// User settings, persisted with QSettings (browser local storage on wasm)
class GameSettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(int pointsToWin READ pointsToWin WRITE setPointsToWin NOTIFY pointsToWinChanged)
    Q_PROPERTY(BallSpeed ballSpeed READ ballSpeed WRITE setBallSpeed NOTIFY ballSpeedChanged)
    Q_PROPERTY(PaddleSize paddleSize READ paddleSize WRITE setPaddleSize NOTIFY paddleSizeChanged)
    Q_PROPERTY(ComputerPlayer::Difficulty difficulty READ difficulty WRITE setDifficulty NOTIFY difficultyChanged)

    Q_PROPERTY(qreal serveSpeed READ serveSpeed NOTIFY ballSpeedChanged)
    Q_PROPERTY(qreal maxSpeed READ maxSpeed NOTIFY ballSpeedChanged)
    Q_PROPERTY(qreal paddleLength READ paddleLength NOTIFY paddleSizeChanged)

public:
    enum BallSpeed {
        Slow = 0,
        Medium,
        Fast
    };
    Q_ENUM(BallSpeed)

    enum PaddleSize {
        Small = 0,
        Regular,
        Large
    };
    Q_ENUM(PaddleSize)

    static constexpr int minPointsToWin = 1;
    static constexpr int maxPointsToWin = 21;

    explicit GameSettings(QObject* parent = nullptr);

    Q_INVOKABLE void restoreDefaults();

    void setPointsToWin(int pointsToWin);
    int pointsToWin() const;

    void setBallSpeed(BallSpeed ballSpeed);
    BallSpeed ballSpeed() const;

    void setPaddleSize(PaddleSize paddleSize);
    PaddleSize paddleSize() const;

    void setDifficulty(ComputerPlayer::Difficulty difficulty);
    ComputerPlayer::Difficulty difficulty() const;

    qreal serveSpeed() const;
    qreal maxSpeed() const;
    qreal paddleLength() const;

signals:
    void pointsToWinChanged(int);
    void ballSpeedChanged(GameSettings::BallSpeed);
    void paddleSizeChanged(GameSettings::PaddleSize);
    void difficultyChanged(ComputerPlayer::Difficulty);

private:
    QSettings m_settings;

    int m_pointsToWin;
    BallSpeed m_ballSpeed;
    PaddleSize m_paddleSize;
    ComputerPlayer::Difficulty m_difficulty;
};

#endif // GAMESETTINGS_H
