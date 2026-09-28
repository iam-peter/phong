#include "match.h"

#include <QRandomGenerator>
#include <QtMath>

#include <algorithm>
#include <cmath>

Match::Match(QObject* parent):
    QObject(parent),
    m_left(new Player(this)),
    m_right(new Player(this)),
    m_state(State::Idle),
    m_pausedState(State::Idle),
    m_winner(nullptr),
    m_serveTo(Side::LeftSide),
    m_pointsToWin(5),
    m_serveSpeed(16.0),
    m_maxSpeed(34.0),
    m_speedUp(1.06),
    m_serveDelay(1.0),
    m_serveCountdown(0.0),
    m_ballVelocity(),
    m_rally(0),
    m_longestRally(0),
    m_totalHits(0),
    m_playTime(0.0),
    m_lastTouch(Side::NoSide)
{}

void Match::start()
{
    m_left->setScore(0);
    m_right->setScore(0);
    setWinner(nullptr);
    setBallVelocity(QVector2D());
    setRally(0);
    setPlayTime(0.0);
    setLastTouch(Side::NoSide);

    m_longestRally = 0;
    emit longestRallyChanged(m_longestRally);
    m_totalHits = 0;
    emit totalHitsChanged(m_totalHits);

    m_serveTo = QRandomGenerator::global()->bounded(2) ? Side::RightSide : Side::LeftSide;
    setServeCountdown(m_serveDelay);
    setState(State::Serving);
}

void Match::stop()
{
    setBallVelocity(QVector2D());
    setState(State::Idle);
}

void Match::pause()
{
    if (m_state != State::Serving && m_state != State::Playing)
        return;

    m_pausedState = m_state;
    setState(State::Paused);
}

void Match::resume()
{
    if (m_state != State::Paused)
        return;

    setState(m_pausedState);
}

void Match::advance(qreal dt)
{
    if (m_state == State::Serving) {
        setServeCountdown(std::max(m_serveCountdown - dt, 0.0));
        if (m_serveCountdown <= 0.0)
            serve();
    }
    else if (m_state == State::Playing) {
        setPlayTime(m_playTime + dt);
    }
}

void Match::paddleHit(Side side, qreal offset)
{
    if (m_state != State::Playing)
        return;

    // A paddle only returns balls travelling towards it. This also swallows
    // repeated contact reports while the ball is still touching the paddle.
    const qreal direction = side == Side::LeftSide ? 1.0 : -1.0;
    if (m_ballVelocity.x() * direction > 0.0f)
        return;

    const qreal speed = std::min(qreal(m_ballVelocity.length()) * m_speedUp, m_maxSpeed);
    const qreal angle = qDegreesToRadians(std::clamp(offset, -1.0, 1.0) * maxBounceAngle);

    hit(side, QVector2D(direction * speed * qCos(angle), speed * qSin(angle)));
}

void Match::deflect(Side side, const QVector2D& normal)
{
    if (m_state != State::Playing || normal.isNull())
        return;

    // Only balls running into the paddle surface bounce
    const QVector2D n = normal.normalized();
    const float into = QVector2D::dotProduct(m_ballVelocity, n);
    if (into >= 0.0f)
        return;

    const QVector2D reflected = (m_ballVelocity - 2.0f * into * n).normalized();

    // The surface decides the angle, but a paddle never scores against its
    // own player, and the ball keeps crossing the field instead of bouncing
    // between the walls
    const float awayFromGoal = side == Side::LeftSide ? 1.0f : -1.0f;
    const float minX = qCos(qDegreesToRadians(maxBounceAngle));
    const float x = std::max(std::abs(reflected.x()), minX);
    const float y = std::copysign(std::sqrt(1.0f - x * x),
                                  reflected.y() == 0.0f ? 1.0f : reflected.y());
    const QVector2D direction(awayFromGoal * x, y);

    const qreal speed = std::min(qreal(m_ballVelocity.length()) * m_speedUp, m_maxSpeed);
    hit(side, direction * float(speed));
}

bool Match::shieldHit(Side side)
{
    if (m_state != State::Playing || side == Side::NoSide)
        return false;

    // The left shield guards against balls moving left and vice versa
    const float towardsGoal = side == Side::LeftSide ? -1.0f : 1.0f;
    if (m_ballVelocity.x() * towardsGoal <= 0.0f)
        return false;

    setBallVelocity(QVector2D(-m_ballVelocity.x(), m_ballVelocity.y()));
    return true;
}

void Match::scaleBallSpeed(qreal factor)
{
    if (m_state != State::Playing || factor <= 0.0)
        return;

    setBallVelocity(m_ballVelocity * float(factor));
}

void Match::wallHit(bool top)
{
    if (m_state != State::Playing)
        return;

    const float vy = std::abs(m_ballVelocity.y());
    setBallVelocity(QVector2D(m_ballVelocity.x(), top ? -vy : vy));
}

void Match::goal(Side scorer)
{
    Player* player = this->player(scorer);
    if (m_state != State::Playing || !player)
        return;

    player->setScore(player->score() + 1);

    setBallVelocity(QVector2D());
    setRally(0);

    emit pointScored(scorer);

    if (player->score() >= m_pointsToWin) {
        setWinner(player);
        setState(State::Finished);
        emit finished();
        return;
    }

    // Serve towards the player who conceded the point
    m_serveTo = scorer == Side::LeftSide ? Side::RightSide : Side::LeftSide;
    setServeCountdown(m_serveDelay);
    setState(State::Serving);
}

Player* Match::left() const
{
    return m_left;
}

Player* Match::right() const
{
    return m_right;
}

Player* Match::player(Side side) const
{
    switch (side) {
        case Side::LeftSide:
            return m_left;
        case Side::RightSide:
            return m_right;
        default:
            return nullptr;
    }
}

Match::Side Match::opponent(Side side)
{
    switch (side) {
        case Side::LeftSide:
            return Side::RightSide;
        case Side::RightSide:
            return Side::LeftSide;
        default:
            return Side::NoSide;
    }
}

Match::State Match::state() const
{
    return m_state;
}

Player* Match::winner() const
{
    return m_winner;
}

void Match::setPointsToWin(int pointsToWin)
{
    pointsToWin = std::max(pointsToWin, 1);
    if (m_pointsToWin == pointsToWin)
        return;

    m_pointsToWin = pointsToWin;
    emit pointsToWinChanged(pointsToWin);
}

int Match::pointsToWin() const
{
    return m_pointsToWin;
}

void Match::setServeSpeed(qreal serveSpeed)
{
    if (m_serveSpeed == serveSpeed)
        return;

    m_serveSpeed = serveSpeed;
    emit serveSpeedChanged(serveSpeed);
}

qreal Match::serveSpeed() const
{
    return m_serveSpeed;
}

void Match::setMaxSpeed(qreal maxSpeed)
{
    if (m_maxSpeed == maxSpeed)
        return;

    m_maxSpeed = maxSpeed;
    emit maxSpeedChanged(maxSpeed);
}

qreal Match::maxSpeed() const
{
    return m_maxSpeed;
}

void Match::setSpeedUp(qreal speedUp)
{
    if (m_speedUp == speedUp)
        return;

    m_speedUp = speedUp;
    emit speedUpChanged(speedUp);
}

qreal Match::speedUp() const
{
    return m_speedUp;
}

void Match::setServeDelay(qreal serveDelay)
{
    if (m_serveDelay == serveDelay)
        return;

    m_serveDelay = serveDelay;
    emit serveDelayChanged(serveDelay);
}

qreal Match::serveDelay() const
{
    return m_serveDelay;
}

qreal Match::serveCountdown() const
{
    return m_serveCountdown;
}

QVector2D Match::ballVelocity() const
{
    return m_ballVelocity;
}

int Match::rally() const
{
    return m_rally;
}

int Match::longestRally() const
{
    return m_longestRally;
}

int Match::totalHits() const
{
    return m_totalHits;
}

qreal Match::playTime() const
{
    return m_playTime;
}

Match::Side Match::lastTouch() const
{
    return m_lastTouch;
}

void Match::setState(State state)
{
    if (m_state == state)
        return;

    m_state = state;
    emit stateChanged(state);
}

void Match::setWinner(Player* winner)
{
    if (m_winner == winner)
        return;

    m_winner = winner;
    emit winnerChanged(winner);
}

void Match::setServeCountdown(qreal serveCountdown)
{
    if (m_serveCountdown == serveCountdown)
        return;

    m_serveCountdown = serveCountdown;
    emit serveCountdownChanged(serveCountdown);
}

void Match::setBallVelocity(const QVector2D& ballVelocity)
{
    if (m_ballVelocity == ballVelocity)
        return;

    m_ballVelocity = ballVelocity;
    emit ballVelocityChanged(ballVelocity);
}

void Match::setRally(int rally)
{
    if (m_rally == rally)
        return;

    m_rally = rally;
    emit rallyChanged(rally);

    if (rally > m_longestRally) {
        m_longestRally = rally;
        emit longestRallyChanged(rally);
    }
}

void Match::setPlayTime(qreal playTime)
{
    if (m_playTime == playTime)
        return;

    m_playTime = playTime;
    emit playTimeChanged(playTime);
}

void Match::setLastTouch(Side lastTouch)
{
    if (m_lastTouch == lastTouch)
        return;

    m_lastTouch = lastTouch;
    emit lastTouchChanged(lastTouch);
}

void Match::hit(Side side, const QVector2D& velocity)
{
    setBallVelocity(velocity);
    setLastTouch(side);

    setRally(m_rally + 1);
    ++m_totalHits;
    emit totalHitsChanged(m_totalHits);
}

void Match::serve()
{
    QRandomGenerator* random = QRandomGenerator::global();

    const qreal direction = m_serveTo == Side::LeftSide ? -1.0 : 1.0;
    const qreal vertical = random->bounded(2) ? 1.0 : -1.0;
    const qreal angle = qDegreesToRadians(minServeAngle
                                          + random->bounded(maxServeAngle - minServeAngle));

    setBallVelocity(QVector2D(direction * m_serveSpeed * qCos(angle),
                              vertical * m_serveSpeed * qSin(angle)));
    setLastTouch(Side::NoSide);
    setState(State::Playing);
    emit served();
}
