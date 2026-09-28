#include "computerplayer.h"

#include <QRandomGenerator>

#include <algorithm>
#include <cmath>

ComputerPlayer::ComputerPlayer(QObject* parent):
    QObject(parent),
    m_difficulty(Difficulty::Normal),
    m_paddleX(0.0),
    m_paddleReach(1.0),
    m_fieldTop(1.0),
    m_fieldBottom(-1.0),
    m_target(0.0),
    m_direction(0.0),
    m_sincePlan(0.0),
    m_approaching(false),
    m_aimError(0.0)
{}

qreal ComputerPlayer::predictY(const QVector2D& position, const QVector2D& velocity,
                               qreal lineX, qreal top, qreal bottom)
{
    const qreal height = top - bottom;
    if (velocity.x() == 0.0f || height <= 0.0)
        return std::clamp(qreal(position.y()), bottom, top);

    const qreal time = (lineX - position.x()) / velocity.x();
    if (time < 0.0)
        return std::clamp(qreal(position.y()), bottom, top);

    // Unfold the bounces: the ball travels a zig-zag with a period of
    // twice the field height.
    const qreal y = position.y() + velocity.y() * time - bottom;
    qreal folded = std::fmod(y, 2.0 * height);
    if (folded < 0.0)
        folded += 2.0 * height;
    if (folded > height)
        folded = 2.0 * height - folded;

    return bottom + folded;
}

void ComputerPlayer::reset()
{
    m_sincePlan = 0.0;
    m_approaching = false;
    m_aimError = 0.0;
    setTarget(0.5 * (m_fieldTop + m_fieldBottom));
    setDirection(0.0);
}

void ComputerPlayer::update(qreal dt, const QVector2D& ballPosition,
                            const QVector2D& ballVelocity, qreal paddleY)
{
    const Profile profile = this->profile();

    const qreal towardsPaddle = m_paddleX - ballPosition.x();
    const bool approaching = ballVelocity.x() != 0.0f
                             && (towardsPaddle > 0.0) == (ballVelocity.x() > 0.0f);

    // Once the ball changes direction it takes a moment to notice, and every
    // approach gets its own aim error so the paddle doesn't wobble.
    if (approaching != m_approaching) {
        m_approaching = approaching;
        m_sincePlan = 0.0;
        m_aimError = QRandomGenerator::global()->bounded(2.0) - 1.0;
    }
    else {
        m_sincePlan += dt;
    }

    if (m_sincePlan >= profile.reactionTime) {
        plan(ballPosition, ballVelocity);
        m_sincePlan = 0.0;
    }

    // Slow down close to the target instead of jittering around it
    const qreal distance = m_target - paddleY;
    const qreal deadZone = 0.1 * m_paddleReach;
    if (std::abs(distance) < deadZone)
        setDirection(0.0);
    else
        setDirection(std::clamp(distance / m_paddleReach, -1.0, 1.0) * profile.maxInput);
}

void ComputerPlayer::setDifficulty(Difficulty difficulty)
{
    if (m_difficulty == difficulty)
        return;

    m_difficulty = difficulty;
    emit difficultyChanged(difficulty);
}

ComputerPlayer::Difficulty ComputerPlayer::difficulty() const
{
    return m_difficulty;
}

void ComputerPlayer::setPaddleX(qreal paddleX)
{
    if (m_paddleX == paddleX)
        return;

    m_paddleX = paddleX;
    emit paddleXChanged(paddleX);
}

qreal ComputerPlayer::paddleX() const
{
    return m_paddleX;
}

void ComputerPlayer::setPaddleReach(qreal paddleReach)
{
    paddleReach = std::max(paddleReach, 0.01);
    if (m_paddleReach == paddleReach)
        return;

    m_paddleReach = paddleReach;
    emit paddleReachChanged(paddleReach);
}

qreal ComputerPlayer::paddleReach() const
{
    return m_paddleReach;
}

void ComputerPlayer::setFieldTop(qreal fieldTop)
{
    if (m_fieldTop == fieldTop)
        return;

    m_fieldTop = fieldTop;
    emit fieldTopChanged(fieldTop);
}

qreal ComputerPlayer::fieldTop() const
{
    return m_fieldTop;
}

void ComputerPlayer::setFieldBottom(qreal fieldBottom)
{
    if (m_fieldBottom == fieldBottom)
        return;

    m_fieldBottom = fieldBottom;
    emit fieldBottomChanged(fieldBottom);
}

qreal ComputerPlayer::fieldBottom() const
{
    return m_fieldBottom;
}

qreal ComputerPlayer::target() const
{
    return m_target;
}

qreal ComputerPlayer::direction() const
{
    return m_direction;
}

ComputerPlayer::Profile ComputerPlayer::profile() const
{
    switch (m_difficulty) {
        case Difficulty::Easy:
            return { 0.35, 1.6, 0.5, false };
        case Difficulty::Hard:
            return { 0.08, 0.6, 1.0, true };
        case Difficulty::Normal:
        default:
            return { 0.2, 1.15, 0.75, true };
    }
}

void ComputerPlayer::plan(const QVector2D& ballPosition, const QVector2D& ballVelocity)
{
    if (!m_approaching) {
        // Wait in the middle for the next return
        setTarget(0.5 * (m_fieldTop + m_fieldBottom));
        return;
    }

    const Profile profile = this->profile();
    const qreal y = profile.predicts
        ? predictY(ballPosition, ballVelocity, m_paddleX, m_fieldTop, m_fieldBottom)
        : std::clamp(qreal(ballPosition.y()), m_fieldBottom, m_fieldTop);
    setTarget(y + m_aimError * profile.aimError * m_paddleReach);
}

void ComputerPlayer::setTarget(qreal target)
{
    if (m_target == target)
        return;

    m_target = target;
    emit targetChanged(target);
}

void ComputerPlayer::setDirection(qreal direction)
{
    if (m_direction == direction)
        return;

    m_direction = direction;
    emit directionChanged(direction);
}
