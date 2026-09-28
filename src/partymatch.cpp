#include "partymatch.h"

#include <QtMath>

#include <algorithm>

PartyMatch::PartyMatch(QObject* parent):
    QObject(parent),
    m_players(minPlayers),
    m_lives(3),
    m_ball(new Ball(false, QVector2D(), this)),
    m_state(State::Idle),
    m_pausedState(State::Idle),
    m_livesLeft(),
    m_winner(-1),
    m_serveSpeed(14.0),
    m_maxSpeed(30.0),
    m_speedUp(1.05),
    m_serveDelay(1.0),
    m_serveCountdown(0.0),
    m_serveDirection(),
    m_rally(0),
    m_random(QRandomGenerator::global()->generate())
{}

QVector2D PartyMatch::normal(int player) const
{
    const qreal angle = qDegreesToRadians(-90.0 + player * 360.0 / m_players);
    return QVector2D(float(qCos(angle)), float(qSin(angle)));
}

QVector2D PartyMatch::tangent(int player) const
{
    const QVector2D n = normal(player);
    return QVector2D(-n.y(), n.x());
}

void PartyMatch::start()
{
    m_livesLeft = QList<int>(m_players, m_lives);
    emit livesLeftChanged();
    if (m_winner != -1) {
        m_winner = -1;
        emit winnerChanged(m_winner);
    }

    m_ball->setVelocity(QVector2D());
    m_ball->setLastTouch(Match::Side::NoSide);
    setRally(0);
    prepareServe(-1);
    setState(State::Serving);
}

void PartyMatch::stop()
{
    m_ball->setVelocity(QVector2D());
    setState(State::Idle);
}

void PartyMatch::pause()
{
    if (m_state != State::Serving && m_state != State::Playing)
        return;

    m_pausedState = m_state;
    setState(State::Paused);
}

void PartyMatch::resume()
{
    if (m_state == State::Paused)
        setState(m_pausedState);
}

void PartyMatch::advance(qreal dt)
{
    if (m_state != State::Serving)
        return;

    m_serveCountdown = std::max(m_serveCountdown - dt, 0.0);
    emit serveCountdownChanged(m_serveCountdown);
    if (m_serveCountdown > 0.0)
        return;

    m_ball->setVelocity(m_serveDirection * float(m_serveSpeed));
    setState(State::Playing);
    emit served();
}

void PartyMatch::paddleHit(int player, qreal offset)
{
    if (m_state != State::Playing || !isAlive(player))
        return;

    // Only balls running into the side, this also swallows repeated
    // contact reports
    const QVector2D n = normal(player);
    const QVector2D velocity = m_ball->velocity();
    if (QVector2D::dotProduct(velocity, n) <= 0.0f)
        return;

    // Back into the field, turned by where the ball met the paddle
    const qreal angle = qDegreesToRadians(std::clamp(offset, -1.0, 1.0) * maxBounceAngle);
    const QVector2D direction = -n * float(qCos(angle)) + tangent(player) * float(qSin(angle));
    const qreal speed = std::min(qreal(velocity.length()) * m_speedUp, m_maxSpeed);
    m_ball->setVelocity(direction * float(speed));
    setRally(m_rally + 1);
    emit paddleHitBall(player);
}

bool PartyMatch::bounce(const QVector2D& normal)
{
    if (m_state != State::Playing || normal.isNull())
        return false;

    const QVector2D n = normal.normalized();
    const QVector2D velocity = m_ball->velocity();
    const float into = QVector2D::dotProduct(velocity, n);
    if (into >= 0.0f)
        return false;

    m_ball->setVelocity(velocity - 2.0f * into * n);
    return true;
}

void PartyMatch::goal(int player)
{
    if (m_state != State::Playing || !isAlive(player))
        return;

    --m_livesLeft[player];
    emit livesLeftChanged();
    emit goalScored(player);

    m_ball->setVelocity(QVector2D());
    setRally(0);

    if (m_livesLeft.at(player) == 0) {
        emit playerOut(player);

        if (alive() == 1) {
            m_winner = int(std::find_if(m_livesLeft.cbegin(), m_livesLeft.cend(),
                                        [](int lives) { return lives > 0; }) - m_livesLeft.cbegin());
            emit winnerChanged(m_winner);
            setState(State::Finished);
            emit finished();
            return;
        }
    }

    prepareServe(player);
    setState(State::Serving);
}

bool PartyMatch::isAlive(int player) const
{
    return player >= 0 && player < m_livesLeft.size() && m_livesLeft.at(player) > 0;
}

void PartyMatch::setPlayers(int players)
{
    players = std::clamp(players, minPlayers, maxPlayers);
    if (m_players == players)
        return;

    m_players = players;
    emit playersChanged(players);
}

int PartyMatch::players() const
{
    return m_players;
}

void PartyMatch::setLives(int lives)
{
    lives = std::max(lives, 1);
    if (m_lives == lives)
        return;

    m_lives = lives;
    emit livesChanged(lives);
}

int PartyMatch::lives() const
{
    return m_lives;
}

Ball* PartyMatch::ball() const
{
    return m_ball;
}

PartyMatch::State PartyMatch::state() const
{
    return m_state;
}

QVariantList PartyMatch::livesLeft() const
{
    QVariantList lives;
    for (int left : m_livesLeft)
        lives.append(left);
    return lives;
}

int PartyMatch::alive() const
{
    return int(std::count_if(m_livesLeft.cbegin(), m_livesLeft.cend(), [](int lives) { return lives > 0; }));
}

int PartyMatch::winner() const
{
    return m_winner;
}

void PartyMatch::setServeSpeed(qreal serveSpeed)
{
    if (m_serveSpeed == serveSpeed)
        return;

    m_serveSpeed = serveSpeed;
    emit serveSpeedChanged(serveSpeed);
}

qreal PartyMatch::serveSpeed() const
{
    return m_serveSpeed;
}

void PartyMatch::setMaxSpeed(qreal maxSpeed)
{
    if (m_maxSpeed == maxSpeed)
        return;

    m_maxSpeed = maxSpeed;
    emit maxSpeedChanged(maxSpeed);
}

qreal PartyMatch::maxSpeed() const
{
    return m_maxSpeed;
}

void PartyMatch::setSpeedUp(qreal speedUp)
{
    if (m_speedUp == speedUp)
        return;

    m_speedUp = speedUp;
    emit speedUpChanged(speedUp);
}

qreal PartyMatch::speedUp() const
{
    return m_speedUp;
}

void PartyMatch::setServeDelay(qreal serveDelay)
{
    if (m_serveDelay == serveDelay)
        return;

    m_serveDelay = serveDelay;
    emit serveDelayChanged(serveDelay);
}

qreal PartyMatch::serveDelay() const
{
    return m_serveDelay;
}

qreal PartyMatch::serveCountdown() const
{
    return m_serveCountdown;
}

QVector2D PartyMatch::serveDirection() const
{
    return m_serveDirection;
}

int PartyMatch::rally() const
{
    return m_rally;
}

void PartyMatch::setSeed(quint32 seed)
{
    m_random.seed(seed);
}

void PartyMatch::setState(State state)
{
    if (m_state == state)
        return;

    m_state = state;
    emit stateChanged(state);
}

void PartyMatch::setRally(int rally)
{
    if (m_rally == rally)
        return;

    m_rally = rally;
    emit rallyChanged(rally);
}

void PartyMatch::prepareServe(int conceder)
{
    QList<int> targets;
    for (int player = 0; player < m_players; ++player) {
        if (isAlive(player) && player != conceder)
            targets.append(player);
    }
    if (targets.isEmpty())
        targets.append(std::max(conceder, 0));

    // Towards a player, a little off the middle of the goal
    const int target = targets.at(m_random.bounded(int(targets.size())));
    const qreal angle = qDegreesToRadians((m_random.bounded(2.0) - 1.0) * maxServeAngle);
    const QVector2D n = normal(target);
    const QVector2D t = tangent(target);
    m_serveDirection = n * float(qCos(angle)) + t * float(qSin(angle));
    emit serveDirectionChanged();

    m_serveCountdown = m_serveDelay;
    emit serveCountdownChanged(m_serveCountdown);
}
