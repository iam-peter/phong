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
    Q_PROPERTY(bool music READ music WRITE setMusic NOTIFY musicChanged)
    // Percent
    Q_PROPERTY(int musicVolume READ musicVolume WRITE setMusicVolume NOTIFY musicVolumeChanged)
    // Gamepads shake on hits and goals
    Q_PROPERTY(bool rumble READ rumble WRITE setRumble NOTIFY rumbleChanged)
    // Seconds of countdown before a kickoff
    Q_PROPERTY(int kickoffTime READ kickoffTime WRITE setKickoffTime NOTIFY kickoffTimeChanged)
    // The game mode last chosen in the menu, see GameScene.Mode
    Q_PROPERTY(int mode READ mode WRITE setMode NOTIFY modeChanged)
    // Players on the polygon, 3 to 6
    Q_PROPERTY(int partyPlayers READ partyPlayers WRITE setPartyPlayers NOTIFY partyPlayersChanged)
    // The host last joined by address
    Q_PROPERTY(QString lanAddress READ lanAddress WRITE setLanAddress NOTIFY lanAddressChanged)
    // The name the others see online and on the high scores, empty for none
    Q_PROPERTY(QString playerName READ playerName WRITE setPlayerName NOTIFY playerNameChanged)
    // phong-server for games over the internet and the shared high scores,
    // the build's default until one is set, empty for none
    Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(QString defaultServer READ defaultServerUrl CONSTANT)

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
    static constexpr int minKickoffTime = 1;
    static constexpr int maxKickoffTime = 5;

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

    void setMusic(bool music);
    bool music() const;

    void setMusicVolume(int musicVolume);
    int musicVolume() const;

    void setRumble(bool rumble);
    bool rumble() const;

    void setKickoffTime(int kickoffTime);
    int kickoffTime() const;

    void setMode(int mode);
    int mode() const;

    void setPartyPlayers(int partyPlayers);
    int partyPlayers() const;

    void setLanAddress(const QString& lanAddress);
    QString lanAddress() const;

    void setPlayerName(const QString& playerName);
    QString playerName() const;

    void setServerUrl(const QString& serverUrl);
    QString serverUrl() const;
    static QString defaultServerUrl();

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
    void musicChanged(bool);
    void musicVolumeChanged(int);
    void rumbleChanged(bool);
    void kickoffTimeChanged(int);
    void modeChanged(int);
    void partyPlayersChanged(int);
    void lanAddressChanged(const QString&);
    void playerNameChanged(const QString&);
    void serverUrlChanged(const QString&);

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
    bool m_music;
    int m_musicVolume;
    bool m_rumble;
    int m_kickoffTime;
    int m_mode;
    int m_partyPlayers;
    QString m_lanAddress;
    QString m_playerName;
    QString m_serverUrl;
};

#endif // GAMESETTINGS_H
