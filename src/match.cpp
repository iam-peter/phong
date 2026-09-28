#include "match.h"

#include <QRandomGenerator>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace {
// Smallest share of the speed that goes across the field, keeps the ball
// from bouncing between the walls forever
float minAcross()
{
    return float(qCos(qDegreesToRadians(Match::maxBounceAngle)));
}

// Unit direction with at least minAcross() going across the field, keeping
// the signs of direction. x decides where zero components go.
QVector2D acrossField(const QVector2D& direction, float x)
{
    const float across = std::max(std::abs(direction.x()), minAcross());
    const float y = std::copysign(std::sqrt(1.0f - across * across),
                                  direction.y() == 0.0f ? 1.0f : direction.y());
    return QVector2D(std::copysign(across, direction.x() == 0.0f ? x : direction.x()), y);
}
}

Match::Match(QObject* parent):
    QObject(parent),
    m_left(new Player(this)),
    m_right(new Player(this)),
    m_ball(new Ball(false, QVector2D(), this)),
    m_extraBalls(new BallModel(this)),
    m_state(State::Idle),
    m_pausedState(State::Idle),
    m_winner(nullptr),
    m_serveTo(Side::LeftSide),
    m_serveDirection(),
    m_pointsToWin(5),
    m_setsToWin(1),
    m_winByTwo(false),
    m_serveSpeed(16.0),
    m_maxSpeed(34.0),
    m_speedUp(1.06),
    m_paddleSpeed(24.0),
    m_serveDelay(1.0),
    m_serveCountdown(0.0),
    m_rally(0),
    m_longestRally(0),
    m_totalHits(0),
    m_playTime(0.0)
{
    connect(m_ball, &Ball::velocityChanged, this, &Match::ballVelocityChanged);
}

void Match::start()
{
    for (Player* player : { m_left, m_right }) {
        player->setScore(0);
        player->setSets(0);
    }
    setWinner(nullptr);

    m_ball->setVelocity(QVector2D());
    m_ball->setSpin(0.0);
    m_ball->setLastTouch(Side::NoSide);
    removeExtraBalls();

    setRally(0);
    setPlayTime(0.0);
    m_longestRally = 0;
    emit longestRallyChanged(m_longestRally);
    m_totalHits = 0;
    emit totalHitsChanged(m_totalHits);

    // A coin toss for the first kickoff
    prepareServe(QRandomGenerator::global()->bounded(2) ? Side::RightSide : Side::LeftSide);
    setState(State::Serving);
}

void Match::stop()
{
    m_ball->setVelocity(QVector2D());
    m_ball->setSpin(0.0);
    removeExtraBalls();
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
        return;
    }

    if (m_state != State::Playing)
        return;

    setPlayTime(m_playTime + dt);

    for (Ball* ball : balls())
        curve(ball, dt);

    // Extra balls only stay for a while
    const QList<Ball*> extras = m_extraBalls->balls();
    for (Ball* ball : extras) {
        ball->setLifetime(ball->lifetime() - dt);
        if (ball->lifetime() <= 0.0)
            m_extraBalls->remove(ball);
    }
}

void Match::paddleHit(Ball* ball, Side side, qreal offset, qreal paddleVelocity)
{
    if (m_state != State::Playing || !isActive(ball))
        return;

    // A paddle only returns balls travelling towards it. This also swallows
    // repeated contact reports while the ball is still touching the paddle.
    const qreal direction = side == Side::LeftSide ? 1.0 : -1.0;
    if (ball->velocity().x() * direction > 0.0f)
        return;

    const qreal speed = std::min(qreal(ball->velocity().length()) * m_speedUp, m_maxSpeed);
    const qreal angle = qDegreesToRadians(std::clamp(offset, -1.0, 1.0) * maxBounceAngle);

    // Brushing the ball upwards makes it dip on its way over
    const qreal brush = m_paddleSpeed > 0.0 ? std::clamp(paddleVelocity / m_paddleSpeed, -1.0, 1.0) : 0.0;
    const qreal spin = -direction * brush * maxSpin;

    hit(ball, side, QVector2D(direction * speed * qCos(angle), speed * qSin(angle)), spin);
}

void Match::deflect(Ball* ball, Side side, const QVector2D& normal)
{
    if (m_state != State::Playing || !isActive(ball) || normal.isNull())
        return;

    // Only balls running into the paddle surface bounce
    const QVector2D n = normal.normalized();
    const float into = QVector2D::dotProduct(ball->velocity(), n);
    if (into >= 0.0f)
        return;

    // The surface decides the angle, but a paddle never scores against its
    // own player, and the ball keeps crossing the field instead of bouncing
    // between the walls
    const QVector2D reflected = (ball->velocity() - 2.0f * into * n).normalized();
    const float awayFromGoal = side == Side::LeftSide ? 1.0f : -1.0f;
    const QVector2D direction = acrossField(QVector2D(awayFromGoal * std::abs(reflected.x()),
                                                      reflected.y()), awayFromGoal);

    const qreal speed = std::min(qreal(ball->velocity().length()) * m_speedUp, m_maxSpeed);
    hit(ball, side, direction * float(speed), 0.0);
}

bool Match::bounce(Ball* ball, const QVector2D& normal)
{
    if (m_state != State::Playing || !isActive(ball) || normal.isNull())
        return false;

    const QVector2D n = normal.normalized();
    const float into = QVector2D::dotProduct(ball->velocity(), n);
    if (into >= 0.0f)
        return false;

    const QVector2D reflected = (ball->velocity() - 2.0f * into * n).normalized();
    const float speed = ball->velocity().length();
    ball->setVelocity(acrossField(reflected, ball->velocity().x() < 0.0f ? -1.0f : 1.0f) * speed);
    ball->setSpin(0.5 * ball->spin());
    return true;
}

bool Match::shieldHit(Ball* ball, Side side)
{
    if (m_state != State::Playing || !isActive(ball) || side == Side::NoSide)
        return false;

    // The left shield guards against balls moving left and vice versa
    const float towardsGoal = side == Side::LeftSide ? -1.0f : 1.0f;
    const QVector2D velocity = ball->velocity();
    if (velocity.x() * towardsGoal <= 0.0f)
        return false;

    ball->setVelocity(QVector2D(-velocity.x(), velocity.y()));
    ball->setSpin(0.0);
    return true;
}

void Match::scaleBallSpeed(Ball* ball, qreal factor)
{
    if (m_state != State::Playing || !isActive(ball) || factor <= 0.0)
        return;

    ball->setVelocity(ball->velocity() * float(factor));
}

void Match::wallHit(Ball* ball, bool top)
{
    if (m_state != State::Playing || !isActive(ball))
        return;

    const QVector2D velocity = ball->velocity();
    const float vy = std::abs(velocity.y());
    ball->setVelocity(QVector2D(velocity.x(), top ? -vy : vy));
    ball->setSpin(0.5 * ball->spin());
}

void Match::goal(Ball* ball, Side scorer)
{
    Player* player = this->player(scorer);
    if (m_state != State::Playing || !player || !isActive(ball))
        return;

    player->setScore(player->score() + 1);

    const int lead = player->score() - this->player(opponent(scorer))->score();
    const bool setWon = player->score() >= m_pointsToWin && (!m_winByTwo || lead >= 2);

    // An extra ball scores and is gone, the rally goes on with the others
    if (ball->isExtra()) {
        m_extraBalls->remove(ball);
        emit pointScored(scorer, ball);
        if (!setWon)
            return;
    }
    else {
        emit pointScored(scorer, ball);
    }

    m_ball->setVelocity(QVector2D());
    m_ball->setSpin(0.0);
    removeExtraBalls();
    setRally(0);

    // Like in football the player who conceded kicks off, the ball flies
    // towards the scorer. After a set the loser of it kicks off.
    prepareServe(scorer);

    if (setWon) {
        player->setSets(player->sets() + 1);
        emit setFinished(scorer);

        if (player->sets() >= m_setsToWin) {
            setWinner(player);
            setState(State::Finished);
            emit finished();
            return;
        }

        m_left->setScore(0);
        m_right->setScore(0);
    }

    setState(State::Serving);
}

Ball* Match::addBall(const QVector2D& position, Side towards, qreal lifetime, Side lastTouch)
{
    if (m_state != State::Playing || towards == Side::NoSide)
        return nullptr;

    QRandomGenerator* random = QRandomGenerator::global();
    const qreal speed = std::max(qreal(m_ball->velocity().length()), m_serveSpeed);
    const qreal direction = towards == Side::LeftSide ? -1.0 : 1.0;
    const qreal vertical = random->bounded(2) ? 1.0 : -1.0;
    const qreal angle = qDegreesToRadians(minServeAngle
                                          + random->bounded(maxServeAngle - minServeAngle));

    Ball* ball = new Ball(true, position, this);
    ball->setVelocity(QVector2D(direction * speed * qCos(angle), vertical * speed * qSin(angle)));
    ball->setLifetime(lifetime);
    ball->setLastTouch(lastTouch);
    m_extraBalls->append(ball);
    return ball;
}

void Match::paddleHit(Side side, qreal offset, qreal paddleVelocity)
{
    paddleHit(m_ball, side, offset, paddleVelocity);
}

void Match::deflect(Side side, const QVector2D& normal)
{
    deflect(m_ball, side, normal);
}

bool Match::shieldHit(Side side)
{
    return shieldHit(m_ball, side);
}

void Match::scaleBallSpeed(qreal factor)
{
    scaleBallSpeed(m_ball, factor);
}

void Match::wallHit(bool top)
{
    wallHit(m_ball, top);
}

void Match::goal(Side scorer)
{
    goal(m_ball, scorer);
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

Ball* Match::ball() const
{
    return m_ball;
}

BallModel* Match::extraBalls() const
{
    return m_extraBalls;
}

QList<Ball*> Match::balls() const
{
    return QList<Ball*>{ m_ball } + m_extraBalls->balls();
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

void Match::setSetsToWin(int setsToWin)
{
    setsToWin = std::max(setsToWin, 1);
    if (m_setsToWin == setsToWin)
        return;

    m_setsToWin = setsToWin;
    emit setsToWinChanged(setsToWin);
}

int Match::setsToWin() const
{
    return m_setsToWin;
}

void Match::setWinByTwo(bool winByTwo)
{
    if (m_winByTwo == winByTwo)
        return;

    m_winByTwo = winByTwo;
    emit winByTwoChanged(winByTwo);
}

bool Match::winByTwo() const
{
    return m_winByTwo;
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

void Match::setPaddleSpeed(qreal paddleSpeed)
{
    if (m_paddleSpeed == paddleSpeed)
        return;

    m_paddleSpeed = paddleSpeed;
    emit paddleSpeedChanged(paddleSpeed);
}

qreal Match::paddleSpeed() const
{
    return m_paddleSpeed;
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

Match::Side Match::serveTo() const
{
    return m_serveTo;
}

QVector2D Match::serveDirection() const
{
    return m_serveDirection;
}

QVector2D Match::ballVelocity() const
{
    return m_ball->velocity();
}

Match::Side Match::lastTouch() const
{
    return m_ball->lastTouch();
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

void Match::hit(Ball* ball, Side side, const QVector2D& velocity, qreal spin)
{
    ball->setVelocity(velocity);
    ball->setSpin(spin);
    ball->setLastTouch(side);

    setRally(m_rally + 1);
    ++m_totalHits;
    emit totalHitsChanged(m_totalHits);
    emit paddleHitBall(ball, side);
}

void Match::curve(Ball* ball, qreal dt)
{
    const qreal spin = ball->spin();
    if (spin == 0.0)
        return;

    // Turn the flight direction, but not so far that the ball stops
    // crossing the field
    const QVector2D velocity = ball->velocity();
    const float angle = float(spin * dt);
    const QVector2D turned(velocity.x() * std::cos(angle) - velocity.y() * std::sin(angle),
                           velocity.x() * std::sin(angle) + velocity.y() * std::cos(angle));
    if (std::abs(turned.x()) < minAcross() * turned.length()) {
        ball->setSpin(0.0);
        return;
    }

    ball->setVelocity(turned);

    const qreal decayed = spin * std::exp(-spinDecay * dt);
    ball->setSpin(std::abs(decayed) < 0.02 ? 0.0 : decayed);
}

void Match::removeExtraBalls()
{
    m_extraBalls->clear();
}

bool Match::isActive(Ball* ball) const
{
    return ball && (ball == m_ball || m_extraBalls->balls().contains(ball));
}

void Match::prepareServe(Side towards)
{
    QRandomGenerator* random = QRandomGenerator::global();

    const qreal direction = towards == Side::LeftSide ? -1.0 : 1.0;
    const qreal vertical = random->bounded(2) ? 1.0 : -1.0;
    const qreal angle = qDegreesToRadians(minServeAngle
                                          + random->bounded(maxServeAngle - minServeAngle));

    m_serveTo = towards;
    m_serveDirection = QVector2D(direction * qCos(angle), vertical * qSin(angle));
    emit serveDirectionChanged();
    setServeCountdown(m_serveDelay);
}

void Match::serve()
{
    m_ball->setVelocity(m_serveDirection * float(m_serveSpeed));
    m_ball->setSpin(0.0);
    m_ball->setLastTouch(Side::NoSide);
    setState(State::Playing);
    emit served();
}

Ball::Ball(bool extra, const QVector2D& spawnPosition, QObject* parent):
    QObject(parent),
    m_velocity(),
    m_spin(0.0),
    m_lastTouch(Match::Side::NoSide),
    m_extra(extra),
    m_spawnPosition(spawnPosition),
    m_lifetime(0.0)
{}

QVector2D Ball::velocity() const
{
    return m_velocity;
}

qreal Ball::spin() const
{
    return m_spin;
}

Match::Side Ball::lastTouch() const
{
    return m_lastTouch;
}

bool Ball::isExtra() const
{
    return m_extra;
}

QVector2D Ball::spawnPosition() const
{
    return m_spawnPosition;
}

qreal Ball::lifetime() const
{
    return m_lifetime;
}

void Ball::setVelocity(const QVector2D& velocity)
{
    if (m_velocity == velocity)
        return;

    m_velocity = velocity;
    emit velocityChanged(velocity);
}

void Ball::setSpin(qreal spin)
{
    if (m_spin == spin)
        return;

    m_spin = spin;
    emit spinChanged(spin);
}

void Ball::setLastTouch(Match::Side lastTouch)
{
    if (m_lastTouch == lastTouch)
        return;

    m_lastTouch = lastTouch;
    emit lastTouchChanged(lastTouch);
}

void Ball::setLifetime(qreal lifetime)
{
    if (m_lifetime == lifetime)
        return;

    m_lifetime = lifetime;
    emit lifetimeChanged(lifetime);
}

BallModel::BallModel(QObject* parent):
    QAbstractListModel(parent),
    m_balls()
{}

int BallModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_balls.size());
}

QVariant BallModel::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)
        || role != Role::BallRole)
        return QVariant();

    return QVariant::fromValue(m_balls.at(index.row()));
}

QHash<int, QByteArray> BallModel::roleNames() const
{
    return { { Role::BallRole, "ball" } };
}

const QList<Ball*>& BallModel::balls() const
{
    return m_balls;
}

void BallModel::append(Ball* ball)
{
    const int row = int(m_balls.size());
    beginInsertRows(QModelIndex(), row, row);
    m_balls.append(ball);
    endInsertRows();
    emit countChanged();
}

void BallModel::remove(Ball* ball)
{
    const int row = int(m_balls.indexOf(ball));
    if (row < 0)
        return;

    beginRemoveRows(QModelIndex(), row, row);
    m_balls.removeAt(row);
    endRemoveRows();
    emit countChanged();

    ball->deleteLater();
}

void BallModel::clear()
{
    if (m_balls.isEmpty())
        return;

    beginResetModel();
    const QList<Ball*> balls = m_balls;
    m_balls.clear();
    endResetModel();
    emit countChanged();

    for (Ball* ball : balls)
        ball->deleteLater();
}
