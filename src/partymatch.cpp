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
    m_longestRally(0),
    m_paddleSpeed(22.0),
    m_lastTouch(-1),
    m_heldBy(-1),
    m_holdOffset(0.0),
    m_holdTime(0.0),
    m_heldSpeed(0.0),
    m_playerObjects(),
    m_random(QRandomGenerator::global()->generate())
{
    for (int player = 0; player < maxPlayers; ++player)
        m_playerObjects.append(new Player(this));
}

Player* PartyMatch::player(int player) const
{
    return player >= 0 && player < maxPlayers ? m_playerObjects.at(player) : nullptr;
}

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

    for (Player* player : std::as_const(m_playerObjects)) {
        player->setHits(0);
        player->setPower(0.0);
        player->setCatches(0);
    }
    m_ball->setVelocity(QVector2D());
    m_ball->setSpin(0.0);
    m_ball->setSmashed(false);
    setHold(-1, 0.0, 0.0);
    setLastTouch(-1);
    setRally(0);
    if (m_longestRally != 0) {
        m_longestRally = 0;
        emit longestRallyChanged(0);
    }
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
    if (m_state == State::Playing) {
        curve(dt);
        // A held ball goes when the time is up
        if (m_heldBy >= 0) {
            setHold(m_heldBy, m_holdOffset, std::max(m_holdTime - dt, 0.0));
            if (m_holdTime <= 0.0)
                releaseBall();
        }
        return;
    }
    if (m_state != State::Serving)
        return;

    m_serveCountdown = std::max(m_serveCountdown - dt, 0.0);
    emit serveCountdownChanged(m_serveCountdown);
    if (m_serveCountdown > 0.0)
        return;

    m_ball->setVelocity(m_serveDirection * float(m_serveSpeed));
    m_ball->setSpin(0.0);
    m_ball->setSmashed(false);
    setLastTouch(-1);
    setState(State::Playing);
    emit served();
}

void PartyMatch::paddleHit(int player, qreal offset, qreal paddleVelocity, qreal smash)
{
    if (m_state != State::Playing || !isAlive(player) || m_heldBy >= 0)
        return;

    // Only balls running into the side, this also swallows repeated
    // contact reports
    const QVector2D velocity = m_ball->velocity();
    if (QVector2D::dotProduct(velocity, normal(player)) <= 0.0f)
        return;

    // A smash or a perfect hit is faster and may go beyond the max speed
    smash = std::clamp(smash, 0.0, 1.0);
    const bool perfect = std::abs(offset) <= Match::perfectZone
                         && std::abs(paddleVelocity) <= Match::perfectStillness * m_paddleSpeed;
    const qreal boost = (1.0 + Match::smashBoost * smash) * (perfect ? 1.0 + Match::perfectBoost : 1.0);
    const qreal limit = m_maxSpeed * (1.0 + Match::smashOverspeed * smash
                                      + (perfect ? Match::perfectOverspeed : 0.0));
    const qreal speed = std::min(qreal(velocity.length()) * m_speedUp * boost, limit);

    // Brushed along the tangent the ball curves back the other way
    const qreal brush = m_paddleSpeed > 0.0 ? std::clamp(paddleVelocity / m_paddleSpeed, -1.0, 1.0) : 0.0;
    hit(player, qDegreesToRadians(std::clamp(offset, -1.0, 1.0) * maxBounceAngle), speed,
        brush * Match::maxSpin, smash, perfect);
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
    m_ball->setSpin(0.5 * m_ball->spin());
    return true;
}

bool PartyMatch::shieldHit(int player)
{
    Player* shielded = this->player(player);
    if (m_state != State::Playing || !isAlive(player) || !shielded->isShielded())
        return false;

    const QVector2D n = normal(player);
    const QVector2D velocity = m_ball->velocity();
    const float into = QVector2D::dotProduct(velocity, n);
    if (into <= 0.0f)
        return false;

    m_ball->setVelocity(velocity - 2.0f * into * n);
    m_ball->setSpin(0.0);
    m_ball->setSmashed(false);
    shielded->setShielded(false);
    emit shieldUsed(player);
    return true;
}

void PartyMatch::scaleBallSpeed(qreal factor)
{
    if (m_state != State::Playing || factor <= 0.0)
        return;

    m_ball->setVelocity(m_ball->velocity() * float(factor));
    m_heldSpeed *= factor;
}

void PartyMatch::attract(const QVector2D& position, const QVector2D& well, qreal strength, qreal dt)
{
    if (m_state != State::Playing || m_heldBy >= 0 || strength <= 0.0)
        return;

    const QVector2D velocity = m_ball->velocity();
    const QVector2D towards = well - position;
    const float distance = towards.length();
    if (distance < 0.01f || velocity.isNull())
        return;

    // Like Match::attract(), stronger for a faster ball, the speed stays
    constexpr float core = 2.0f;
    const float speed = velocity.length() / float(Match::gravitySpeed);
    const float pull = float(strength) * speed * speed / std::max(distance * distance, core * core);
    const QVector2D bent = (velocity + towards / distance * pull * float(dt)).normalized();
    m_ball->setVelocity(bent * velocity.length());
}

bool PartyMatch::useSpecial(int player)
{
    Player* user = this->player(player);
    if ((m_state != State::Playing && m_state != State::Serving) || !isAlive(player) || user->power() < 1.0)
        return false;

    user->setPower(0.0);
    user->setCatches(user->catches() + 1);
    emit specialUsed(player);
    return true;
}

bool PartyMatch::catchBall(int player, qreal offset)
{
    Player* catcher = this->player(player);
    if (m_state != State::Playing || !isAlive(player) || catcher->catches() <= 0 || m_heldBy >= 0)
        return false;

    // Like a hit, only balls running into the side
    if (QVector2D::dotProduct(m_ball->velocity(), normal(player)) <= 0.0f)
        return false;

    catcher->setCatches(catcher->catches() - 1);
    m_heldSpeed = m_ball->velocity().length();
    m_ball->setVelocity(QVector2D());
    m_ball->setSpin(0.0);
    m_ball->setSmashed(false);
    setLastTouch(player);
    setHold(player, std::clamp(offset, -1.0, 1.0), Match::maxHoldTime);
    emit ballCaught(player);
    return true;
}

void PartyMatch::aimHeldBall(qreal offset)
{
    if (m_heldBy < 0)
        return;

    setHold(m_heldBy, std::clamp(offset, -1.0, 1.0), m_holdTime);
}

bool PartyMatch::releaseBall(qreal smash)
{
    if (m_state != State::Playing || m_heldBy < 0)
        return false;

    const int player = m_heldBy;
    const qreal offset = m_holdOffset;
    setHold(-1, 0.0, 0.0);

    // Like a hit at the offset, but without spin
    smash = std::clamp(smash, 0.0, 1.0);
    const qreal speed = std::min(m_heldSpeed * m_speedUp * (1.0 + Match::smashBoost * smash),
                                 m_maxSpeed * (1.0 + Match::smashOverspeed * smash));
    hit(player, qDegreesToRadians(offset * maxBounceAngle), speed, 0.0, smash, false);
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
    m_ball->setSpin(0.0);
    m_ball->setSmashed(false);
    setHold(-1, 0.0, 0.0);
    setRally(0);

    if (m_livesLeft.at(player) == 0) {
        Player* out = this->player(player);
        out->setPower(0.0);
        out->setCatches(0);
        out->setShielded(false);
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

QVariantMap PartyMatch::snapshot() const
{
    const QVector2D velocity = m_ball->velocity();
    QVariantList power;
    QVariantList catches;
    QVariantList hits;
    // Paddle size, shield, frozen, reversed: what the modifiers did
    QVariantList effects;
    for (int player = 0; player < m_players; ++player) {
        const Player* object = m_playerObjects.at(player);
        power.append(object->power());
        catches.append(object->catches());
        hits.append(object->hits());
        effects.append(QVariant(QVariantList{ object->paddleScale(), object->isShielded(), object->isFrozen(),
                                              object->isReversed() }));
    }
    return {
        { QStringLiteral("players"), m_players },
        { QStringLiteral("state"), int(m_state) },
        { QStringLiteral("lives"), livesLeft() },
        { QStringLiteral("winner"), m_winner },
        { QStringLiteral("countdown"), m_serveCountdown },
        { QStringLiteral("serve"), QVariantList{ m_serveDirection.x(), m_serveDirection.y() } },
        { QStringLiteral("rally"), m_rally },
        { QStringLiteral("longest"), m_longestRally },
        { QStringLiteral("velocity"), QVariantList{ velocity.x(), velocity.y() } },
        { QStringLiteral("spin"), m_ball->spin() },
        { QStringLiteral("smashed"), m_ball->isSmashed() },
        { QStringLiteral("touch"), m_lastTouch },
        { QStringLiteral("hold"), QVariantList{ m_heldBy, m_holdOffset, m_holdTime } },
        { QStringLiteral("power"), power },
        { QStringLiteral("catches"), catches },
        { QStringLiteral("hits"), hits },
        { QStringLiteral("effects"), effects }
    };
}

void PartyMatch::applySnapshot(const QVariantMap& snapshot)
{
    setPlayers(snapshot.value(QStringLiteral("players"), m_players).toInt());

    QList<int> lives;
    for (const QVariant& left : snapshot.value(QStringLiteral("lives")).toList())
        lives.append(left.toInt());
    if (lives != m_livesLeft) {
        m_livesLeft = lives;
        emit livesLeftChanged();
    }

    const int winner = snapshot.value(QStringLiteral("winner"), -1).toInt();
    if (winner != m_winner) {
        m_winner = winner;
        emit winnerChanged(winner);
    }

    const qreal countdown = snapshot.value(QStringLiteral("countdown")).toDouble();
    if (countdown != m_serveCountdown) {
        m_serveCountdown = countdown;
        emit serveCountdownChanged(countdown);
    }

    const QVariantList serve = snapshot.value(QStringLiteral("serve")).toList();
    const QVector2D direction = serve.size() == 2 ? QVector2D(serve.at(0).toFloat(), serve.at(1).toFloat())
                                                  : QVector2D();
    if (direction != m_serveDirection) {
        m_serveDirection = direction;
        emit serveDirectionChanged();
    }

    const QVariantList velocity = snapshot.value(QStringLiteral("velocity")).toList();
    if (velocity.size() == 2)
        m_ball->setVelocity(QVector2D(velocity.at(0).toFloat(), velocity.at(1).toFloat()));

    m_ball->setSpin(snapshot.value(QStringLiteral("spin")).toDouble());
    m_ball->setSmashed(snapshot.value(QStringLiteral("smashed")).toBool());
    setLastTouch(snapshot.value(QStringLiteral("touch"), -1).toInt());
    const QVariantList hold = snapshot.value(QStringLiteral("hold")).toList();
    if (hold.size() == 3)
        setHold(hold.at(0).toInt(), hold.at(1).toDouble(), hold.at(2).toDouble());

    const QVariantList power = snapshot.value(QStringLiteral("power")).toList();
    const QVariantList catches = snapshot.value(QStringLiteral("catches")).toList();
    const QVariantList hits = snapshot.value(QStringLiteral("hits")).toList();
    const QVariantList effects = snapshot.value(QStringLiteral("effects")).toList();
    for (int player = 0; player < m_players; ++player) {
        Player* object = m_playerObjects.at(player);
        object->setPower(power.value(player).toDouble());
        object->setCatches(catches.value(player).toInt());
        object->setHits(hits.value(player).toInt());
        const QVariantList effect = effects.value(player).toList();
        object->setPaddleScale(effect.value(0, 1.0).toDouble());
        object->setShielded(effect.value(1).toBool());
        object->setFrozen(effect.value(2).toBool());
        object->setReversed(effect.value(3).toBool());
    }

    setRally(snapshot.value(QStringLiteral("rally")).toInt());
    const int longest = snapshot.value(QStringLiteral("longest")).toInt();
    if (longest != m_longestRally) {
        m_longestRally = longest;
        emit longestRallyChanged(longest);
    }
    setState(State(std::clamp(snapshot.value(QStringLiteral("state")).toInt(), 0, int(State::Finished))));
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

int PartyMatch::longestRally() const
{
    return m_longestRally;
}

void PartyMatch::setPaddleSpeed(qreal paddleSpeed)
{
    if (m_paddleSpeed == paddleSpeed)
        return;

    m_paddleSpeed = paddleSpeed;
    emit paddleSpeedChanged(paddleSpeed);
}

qreal PartyMatch::paddleSpeed() const
{
    return m_paddleSpeed;
}

int PartyMatch::lastTouch() const
{
    return m_lastTouch;
}

int PartyMatch::heldBy() const
{
    return m_heldBy;
}

qreal PartyMatch::holdOffset() const
{
    return m_holdOffset;
}

qreal PartyMatch::holdTime() const
{
    return m_holdTime;
}

qreal PartyMatch::holdLimit() const
{
    return Match::maxHoldTime;
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

    if (rally > m_longestRally) {
        m_longestRally = rally;
        emit longestRallyChanged(rally);
    }
}

void PartyMatch::setLastTouch(int player)
{
    if (m_lastTouch == player)
        return;

    m_lastTouch = player;
    emit lastTouchChanged(player);
}

void PartyMatch::setHold(int player, qreal offset, qreal time)
{
    if (m_heldBy == player && m_holdOffset == offset && m_holdTime == time)
        return;

    m_heldBy = player;
    m_holdOffset = offset;
    m_holdTime = time;
    emit holdChanged();
}

void PartyMatch::hit(int player, qreal angle, qreal speed, qreal spin, qreal smash, bool perfect)
{
    const QVector2D direction = -normal(player) * float(qCos(angle)) + tangent(player) * float(qSin(angle));
    m_ball->setVelocity(direction * float(speed));
    m_ball->setSpin(spin);
    m_ball->setSmashed(smash >= Match::smashThreshold);
    setLastTouch(player);

    Player* hitter = this->player(player);
    hitter->setHits(hitter->hits() + 1);
    hitter->setPower(hitter->power() + (perfect ? 2.0 : 1.0) * Match::powerPerHit);

    setRally(m_rally + 1);
    emit paddleHitBall(player, smash, perfect);
}

void PartyMatch::curve(qreal dt)
{
    const qreal spin = m_ball->spin();
    if (spin == 0.0)
        return;

    const QVector2D velocity = m_ball->velocity();
    const float angle = float(spin * dt);
    m_ball->setVelocity(QVector2D(velocity.x() * std::cos(angle) - velocity.y() * std::sin(angle),
                                  velocity.x() * std::sin(angle) + velocity.y() * std::cos(angle)));

    const qreal decayed = spin * std::exp(-Match::spinDecay * dt);
    m_ball->setSpin(std::abs(decayed) < 0.02 ? 0.0 : decayed);
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
