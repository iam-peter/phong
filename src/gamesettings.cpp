#include "gamesettings.h"

#include <algorithm>

namespace {
constexpr int defaultPointsToWin = 5;
constexpr auto defaultBallSpeed = GameSettings::BallSpeed::Medium;
constexpr auto defaultPaddleSize = GameSettings::PaddleSize::Regular;
constexpr auto defaultDifficulty = ComputerPlayer::Difficulty::Normal;
constexpr bool defaultModifiers = true;

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
    m_modifiers(defaultModifiers)
{
    m_pointsToWin = std::clamp(m_settings.value("pointsToWin", defaultPointsToWin).toInt(),
                               minPointsToWin, maxPointsToWin);
    m_ballSpeed = readEnum(m_settings, "ballSpeed", defaultBallSpeed, BallSpeed::Fast);
    m_paddleSize = readEnum(m_settings, "paddleSize", defaultPaddleSize, PaddleSize::Large);
    m_difficulty = readEnum(m_settings, "difficulty", defaultDifficulty,
                            ComputerPlayer::Difficulty::Hard);
    m_modifiers = m_settings.value("modifiers", defaultModifiers).toBool();
}

void GameSettings::restoreDefaults()
{
    setPointsToWin(defaultPointsToWin);
    setBallSpeed(defaultBallSpeed);
    setPaddleSize(defaultPaddleSize);
    setDifficulty(defaultDifficulty);
    setModifiers(defaultModifiers);
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
