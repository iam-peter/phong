#include "computerplayer.h"
#include "match.h"

#include <QtMath>

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
    m_aimError(0.0),
    m_aiming(false),
    m_smashing(false),
    m_charging(false),
    m_paddleSpeed(24.0),
    m_wantsDash(false),
    m_confusion(0.0),
    m_blind(false),
    m_targets(),
    m_opponentY(0.0),
    m_random(QRandomGenerator::global()->generate())
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

qreal ComputerPlayer::aimOffset(qreal hitY, qreal lineX, const QList<QVector2D>& targets)
{
    // Aim no steeper than this, the paddle edges are risky
    constexpr qreal maxAim = 50.0;

    qreal best = qQNaN();
    for (const QVector2D& target : targets) {
        // Only targets in front of the paddle
        const qreal distance = (lineX - target.x()) * (lineX > 0.0 ? 1.0 : -1.0);
        if (distance <= 0.0)
            continue;

        const qreal angle = qRadiansToDegrees(std::atan2(target.y() - hitY, distance));
        if (std::abs(angle) <= maxAim && (qIsNaN(best) || std::abs(angle) < std::abs(best)))
            best = angle;
    }

    return qIsNaN(best) ? best : best / Match::maxBounceAngle;
}

qreal ComputerPlayer::awayOffset(qreal hitY, qreal lineX, qreal opponentY, qreal top, qreal bottom)
{
    // Towards the half of the far side the opponent is not in
    const qreal middle = 0.5 * (top + bottom);
    const qreal y = opponentY > middle ? bottom + 0.2 * (top - bottom)
                                       : top - 0.2 * (top - bottom);
    const qreal angle = qRadiansToDegrees(std::atan2(y - hitY, 2.0 * std::abs(lineX)));
    return std::clamp(angle, -40.0, 40.0) / Match::maxBounceAngle;
}

qreal ComputerPlayer::predictCrossing(const QVector2D& position, const QVector2D& velocity,
                                      qreal lineX) const
{
    return predictY(position, velocity, lineX, m_fieldTop, m_fieldBottom);
}

qreal ComputerPlayer::holdAim(qreal hitY) const
{
    // Through a target if there is one, otherwise away from the opponent
    const qreal offset = aimOffset(hitY, m_paddleX, m_targets);
    return qIsNaN(offset) ? awayOffset(hitY, m_paddleX, m_opponentY, m_fieldTop, m_fieldBottom) : offset;
}

void ComputerPlayer::reset()
{
    m_sincePlan = 0.0;
    m_approaching = false;
    m_aimError = 0.0;
    setTarget(0.5 * (m_fieldTop + m_fieldBottom));
    setDirection(0.0);
    setCharging(false);
    setWantsDash(false);
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
        m_aimError = m_random.bounded(2.0) - 1.0;
        m_aiming = m_random.bounded(1.0) < profile.aimChance;
        m_smashing = m_random.bounded(1.0) < profile.smashChance;
    }
    else {
        m_sincePlan += dt;
    }

    if (m_sincePlan >= profile.reactionTime && !m_blind) {
        plan(ballPosition, ballVelocity);
        m_sincePlan = 0.0;
    }

    // Wind up the smash just before the ball arrives
    const qreal arrival = approaching ? towardsPaddle / ballVelocity.x() : qInf();
    setCharging(m_smashing && approaching && arrival < smashWindUp);

    // Slow down close to the target instead of jittering around it
    const qreal distance = m_target - paddleY;
    const qreal deadZone = 0.1 * m_paddleReach;
    if (std::abs(distance) < deadZone)
        setDirection(0.0);
    else
        setDirection(std::clamp(distance / m_paddleReach, -1.0, 1.0) * profile.maxInput);

    // A dash when the ball comes soon and too far away for the paddle
    const qreal reach = arrival * m_paddleSpeed * profile.maxInput + deadZone;
    setWantsDash(profile.dashes && approaching && arrival < 0.6 && std::abs(distance) > reach);
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

void ComputerPlayer::setTargets(const QVariantList& targets)
{
    QList<QVector2D> points;
    for (const QVariant& target : targets)
        points.append(target.value<QVector2D>());

    if (m_targets == points)
        return;

    m_targets = points;
    emit targetsChanged();
}

QVariantList ComputerPlayer::targets() const
{
    QVariantList targets;
    for (const QVector2D& point : m_targets)
        targets.append(point);
    return targets;
}

void ComputerPlayer::setOpponentY(qreal opponentY)
{
    if (m_opponentY == opponentY)
        return;

    m_opponentY = opponentY;
    emit opponentYChanged(opponentY);
}

qreal ComputerPlayer::opponentY() const
{
    return m_opponentY;
}

void ComputerPlayer::setSeed(quint32 seed)
{
    m_random.seed(seed);
}

qreal ComputerPlayer::target() const
{
    return m_target;
}

qreal ComputerPlayer::direction() const
{
    return m_direction;
}

bool ComputerPlayer::isCharging() const
{
    return m_charging;
}

void ComputerPlayer::setPaddleSpeed(qreal paddleSpeed)
{
    if (m_paddleSpeed == paddleSpeed)
        return;

    m_paddleSpeed = paddleSpeed;
    emit paddleSpeedChanged(paddleSpeed);
}

qreal ComputerPlayer::paddleSpeed() const
{
    return m_paddleSpeed;
}

bool ComputerPlayer::wantsDash() const
{
    return m_wantsDash;
}

void ComputerPlayer::setWantsDash(bool wantsDash)
{
    if (m_wantsDash == wantsDash)
        return;

    m_wantsDash = wantsDash;
    emit wantsDashChanged(wantsDash);
}

void ComputerPlayer::setConfusion(qreal confusion)
{
    confusion = std::clamp(confusion, 0.0, 1.0);
    if (m_confusion == confusion)
        return;

    m_confusion = confusion;
    emit confusionChanged(confusion);
}

qreal ComputerPlayer::confusion() const
{
    return m_confusion;
}

void ComputerPlayer::setBlind(bool blind)
{
    if (m_blind == blind)
        return;

    m_blind = blind;
    emit blindChanged(blind);
}

bool ComputerPlayer::isBlind() const
{
    return m_blind;
}

void ComputerPlayer::setCharging(bool charging)
{
    if (m_charging == charging)
        return;

    m_charging = charging;
    emit chargingChanged(charging);
}

ComputerPlayer::Profile ComputerPlayer::profile() const
{
    Profile profile = baseProfile();
    profile.reactionTime *= 1.0 + 2.0 * m_confusion;
    profile.aimError *= 1.0 + m_confusion;
    profile.maxInput *= 1.0 - 0.3 * m_confusion;
    return profile;
}

ComputerPlayer::Profile ComputerPlayer::baseProfile() const
{
    switch (m_difficulty) {
        case Difficulty::Easy:
            return { 0.35, 1.6, 0.5, false, 0.0, false, 0.0, false };
        case Difficulty::Hard:
            return { 0.08, 0.6, 1.0, true, 1.0, true, 0.5, true };
        case Difficulty::Normal:
        default:
            return { 0.2, 1.15, 0.75, true, 0.5, false, 0.25, true };
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

    // Meet the ball off center to send it where it should go. Aiming
    // players concentrate, their aim is steadier.
    qreal offset = qQNaN();
    if (m_aiming) {
        offset = aimOffset(y, m_paddleX, m_targets);
        if (qIsNaN(offset) && profile.tactics)
            offset = awayOffset(y, m_paddleX, m_opponentY, m_fieldTop, m_fieldBottom);
    }

    if (qIsNaN(offset)) {
        setTarget(y + m_aimError * profile.aimError * m_paddleReach);
        return;
    }

    setTarget(y - offset * m_paddleReach + 0.5 * m_aimError * profile.aimError * m_paddleReach);
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
