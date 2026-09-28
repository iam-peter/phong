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
    Q_PROPERTY(bool modifiers READ modifiers WRITE setModifiers NOTIFY modifiersChanged)
    // Sets a player needs, 2 is best of three
    Q_PROPERTY(int setsToWin READ setsToWin WRITE setSetsToWin NOTIFY setsToWinChanged)
    Q_PROPERTY(bool winByTwo READ winByTwo WRITE setWinByTwo NOTIFY winByTwoChanged)
    // Id of an arena, see Arenas, or "random" for a new one every match
    Q_PROPERTY(QString arena READ arena WRITE setArena NOTIFY arenaChanged)
    Q_PROPERTY(bool sound READ sound WRITE setSound NOTIFY soundChanged)

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
    static constexpr int maxSetsToWin = 3;

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

    void setModifiers(bool modifiers);
    bool modifiers() const;

    void setSetsToWin(int setsToWin);
    int setsToWin() const;

    void setWinByTwo(bool winByTwo);
    bool winByTwo() const;

    void setArena(const QString& arena);
    QString arena() const;

    void setSound(bool sound);
    bool sound() const;

    qreal serveSpeed() const;
    qreal maxSpeed() const;
    qreal paddleLength() const;

signals:
    void pointsToWinChanged(int);
    void ballSpeedChanged(GameSettings::BallSpeed);
    void paddleSizeChanged(GameSettings::PaddleSize);
    void difficultyChanged(ComputerPlayer::Difficulty);
    void modifiersChanged(bool);
    void setsToWinChanged(int);
    void winByTwoChanged(bool);
    void arenaChanged(const QString&);
    void soundChanged(bool);

private:
    QSettings m_settings;

    int m_pointsToWin;
    BallSpeed m_ballSpeed;
    PaddleSize m_paddleSize;
    ComputerPlayer::Difficulty m_difficulty;
    bool m_modifiers;
    int m_setsToWin;
    bool m_winByTwo;
    QString m_arena;
    bool m_sound;
};

#endif // GAMESETTINGS_H
