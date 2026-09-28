#include "gamesettings.h"

#include <algorithm>

namespace {
constexpr int defaultPointsToWin = 5;
constexpr auto defaultBallSpeed = GameSettings::BallSpeed::Medium;
constexpr auto defaultPaddleSize = GameSettings::PaddleSize::Regular;
constexpr auto defaultDifficulty = ComputerPlayer::Difficulty::Normal;
constexpr bool defaultModifiers = true;
constexpr int defaultSetsToWin = 1;
constexpr bool defaultWinByTwo = false;
const QString defaultArena = QStringLiteral("random");
constexpr bool defaultSound = true;
constexpr bool defaultMusic = true;
constexpr int defaultKickoffTime = 2;

template<typename Enum>
Enum readEnum(const QSettings& settings, const char* key, Enum fallback, Enum last)
{
    const int value = settings.value(key, int(fallback)).toInt();
    return (value < 0 || value > int(last)) ? fallback : Enum(value);
}
}

GameSettings::GameSettings(QObject* parent):
    QObject(parent),
    m_settings(),
    m_pointsToWin(defaultPointsToWin),
    m_ballSpeed(defaultBallSpeed),
    m_paddleSize(defaultPaddleSize),
    m_difficulty(defaultDifficulty),
    m_modifiers(defaultModifiers),
    m_setsToWin(defaultSetsToWin),
    m_winByTwo(defaultWinByTwo),
    m_arena(defaultArena),
    m_sound(defaultSound),
    m_music(defaultMusic),
    m_kickoffTime(defaultKickoffTime),
    m_mode(0)
{
    m_pointsToWin = std::clamp(m_settings.value("pointsToWin", defaultPointsToWin).toInt(),
                               minPointsToWin, maxPointsToWin);
    m_ballSpeed = readEnum(m_settings, "ballSpeed", defaultBallSpeed, BallSpeed::Fast);
    m_paddleSize = readEnum(m_settings, "paddleSize", defaultPaddleSize, PaddleSize::Large);
    m_difficulty = readEnum(m_settings, "difficulty", defaultDifficulty,
                            ComputerPlayer::Difficulty::Hard);
    m_modifiers = m_settings.value("modifiers", defaultModifiers).toBool();
    m_setsToWin = std::clamp(m_settings.value("setsToWin", defaultSetsToWin).toInt(), 1, maxSetsToWin);
    m_winByTwo = m_settings.value("winByTwo", defaultWinByTwo).toBool();
    m_arena = m_settings.value("arena", defaultArena).toString();
    m_sound = m_settings.value("sound", defaultSound).toBool();
    m_music = m_settings.value("music", defaultMusic).toBool();
    m_kickoffTime = std::clamp(m_settings.value("kickoffTime", defaultKickoffTime).toInt(),
                               minKickoffTime, maxKickoffTime);
    m_mode = std::max(m_settings.value("mode", 0).toInt(), 0);
}

void GameSettings::restoreDefaults()
{
    setPointsToWin(defaultPointsToWin);
    setBallSpeed(defaultBallSpeed);
    setPaddleSize(defaultPaddleSize);
    setDifficulty(defaultDifficulty);
    setModifiers(defaultModifiers);
    setSetsToWin(defaultSetsToWin);
    setWinByTwo(defaultWinByTwo);
    setArena(defaultArena);
    setSound(defaultSound);
    setMusic(defaultMusic);
    setKickoffTime(defaultKickoffTime);
}

void GameSettings::setPointsToWin(int pointsToWin)
{
    pointsToWin = std::clamp(pointsToWin, minPointsToWin, maxPointsToWin);
    if (m_pointsToWin == pointsToWin)
        return;

    m_pointsToWin = pointsToWin;
    m_settings.setValue("pointsToWin", pointsToWin);
    emit pointsToWinChanged(pointsToWin);
}

int GameSettings::pointsToWin() const
{
    return m_pointsToWin;
}

void GameSettings::setBallSpeed(BallSpeed ballSpeed)
{
    if (m_ballSpeed == ballSpeed)
        return;

    m_ballSpeed = ballSpeed;
    m_settings.setValue("ballSpeed", int(ballSpeed));
    emit ballSpeedChanged(ballSpeed);
}

GameSettings::BallSpeed GameSettings::ballSpeed() const
{
    return m_ballSpeed;
}

void GameSettings::setPaddleSize(PaddleSize paddleSize)
{
    if (m_paddleSize == paddleSize)
        return;

    m_paddleSize = paddleSize;
    m_settings.setValue("paddleSize", int(paddleSize));
    emit paddleSizeChanged(paddleSize);
}

GameSettings::PaddleSize GameSettings::paddleSize() const
{
    return m_paddleSize;
}

void GameSettings::setDifficulty(ComputerPlayer::Difficulty difficulty)
{
    if (m_difficulty == difficulty)
        return;

    m_difficulty = difficulty;
    m_settings.setValue("difficulty", int(difficulty));
    emit difficultyChanged(difficulty);
}

ComputerPlayer::Difficulty GameSettings::difficulty() const
{
    return m_difficulty;
}

void GameSettings::setModifiers(bool modifiers)
{
    if (m_modifiers == modifiers)
        return;

    m_modifiers = modifiers;
    m_settings.setValue("modifiers", modifiers);
    emit modifiersChanged(modifiers);
}

bool GameSettings::modifiers() const
{
    return m_modifiers;
}

void GameSettings::setSetsToWin(int setsToWin)
{
    setsToWin = std::clamp(setsToWin, 1, maxSetsToWin);
    if (m_setsToWin == setsToWin)
        return;

    m_setsToWin = setsToWin;
    m_settings.setValue("setsToWin", setsToWin);
    emit setsToWinChanged(setsToWin);
}

int GameSettings::setsToWin() const
{
    return m_setsToWin;
}

void GameSettings::setWinByTwo(bool winByTwo)
{
    if (m_winByTwo == winByTwo)
        return;

    m_winByTwo = winByTwo;
    m_settings.setValue("winByTwo", winByTwo);
    emit winByTwoChanged(winByTwo);
}

bool GameSettings::winByTwo() const
{
    return m_winByTwo;
}

void GameSettings::setArena(const QString& arena)
{
    if (m_arena == arena)
        return;

    m_arena = arena;
    m_settings.setValue("arena", arena);
    emit arenaChanged(arena);
}

QString GameSettings::arena() const
{
    return m_arena;
}

void GameSettings::setSound(bool sound)
{
    if (m_sound == sound)
        return;

    m_sound = sound;
    m_settings.setValue("sound", sound);
    emit soundChanged(sound);
}

bool GameSettings::sound() const
{
    return m_sound;
}

void GameSettings::setMusic(bool music)
{
    if (m_music == music)
        return;

    m_music = music;
    m_settings.setValue("music", music);
    emit musicChanged(music);
}

bool GameSettings::music() const
{
    return m_music;
}

void GameSettings::setKickoffTime(int kickoffTime)
{
    kickoffTime = std::clamp(kickoffTime, minKickoffTime, maxKickoffTime);
    if (m_kickoffTime == kickoffTime)
        return;

    m_kickoffTime = kickoffTime;
    m_settings.setValue("kickoffTime", kickoffTime);
    emit kickoffTimeChanged(kickoffTime);
}

int GameSettings::kickoffTime() const
{
    return m_kickoffTime;
}

void GameSettings::setMode(int mode)
{
    if (m_mode == mode)
        return;

    m_mode = mode;
    m_settings.setValue("mode", mode);
    emit modeChanged(mode);
}

int GameSettings::mode() const
{
    return m_mode;
}

qreal GameSettings::serveSpeed() const
{
    switch (m_ballSpeed) {
        case BallSpeed::Slow:
            return 12.0;
        case BallSpeed::Fast:
            return 21.0;
        case BallSpeed::Medium:
        default:
            return 16.0;
    }
}

qreal GameSettings::maxSpeed() const
{
    return 2.0 * serveSpeed();
}

qreal GameSettings::paddleLength() const
{
    switch (m_paddleSize) {
        case PaddleSize::Small:
            return 3.0;
        case PaddleSize::Large:
            return 5.0;
        case PaddleSize::Regular:
        default:
            return 4.0;
    }
}
