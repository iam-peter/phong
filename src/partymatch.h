#ifndef PARTYMATCH_H
#define PARTYMATCH_H

#include "match.h"

#include <QObject>
#include <QRandomGenerator>
#include <QVariantList>
#include <QVector2D>
#include <QtQml/qqmlregistration.h>

// Rules for three to six players on a regular polygon, one side each.
// The middle of a side is its player's goal, posts at its ends keep the
// goals apart. A goal costs a ball, a player without balls is out and
// the last one left wins. The paddles hit like in Match: smashes,
// perfect hits, spin and the power bar with its special.
class PartyMatch : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int players READ players WRITE setPlayers NOTIFY playersChanged)
    Q_PROPERTY(int lives READ lives WRITE setLives NOTIFY livesChanged)
    Q_PROPERTY(Ball* ball READ ball CONSTANT)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    // Balls left for each player, 0 is out
    Q_PROPERTY(QVariantList livesLeft READ livesLeft NOTIFY livesLeftChanged)
    Q_PROPERTY(int alive READ alive NOTIFY livesLeftChanged)
    Q_PROPERTY(int winner READ winner NOTIFY winnerChanged)
    Q_PROPERTY(qreal serveSpeed READ serveSpeed WRITE setServeSpeed NOTIFY serveSpeedChanged)
    Q_PROPERTY(qreal maxSpeed READ maxSpeed WRITE setMaxSpeed NOTIFY maxSpeedChanged)
    Q_PROPERTY(qreal speedUp READ speedUp WRITE setSpeedUp NOTIFY speedUpChanged)
    Q_PROPERTY(qreal serveDelay READ serveDelay WRITE setServeDelay NOTIFY serveDelayChanged)
    Q_PROPERTY(qreal serveCountdown READ serveCountdown NOTIFY serveCountdownChanged)
    Q_PROPERTY(QVector2D serveDirection READ serveDirection NOTIFY serveDirectionChanged)
    Q_PROPERTY(int rally READ rally NOTIFY rallyChanged)
    Q_PROPERTY(int longestRally READ longestRally NOTIFY longestRallyChanged)
    // Paddle speed at which a hit gets the full spin
    Q_PROPERTY(qreal paddleSpeed READ paddleSpeed WRITE setPaddleSpeed NOTIFY paddleSpeedChanged)
    // The player who hit the ball last, -1 after a serve
    Q_PROPERTY(int lastTouch READ lastTouch NOTIFY lastTouchChanged)
    // The player whose paddle holds the ball, -1 for none, where on the
    // paddle and for how many more seconds
    Q_PROPERTY(int heldBy READ heldBy NOTIFY holdChanged)
    Q_PROPERTY(qreal holdOffset READ holdOffset NOTIFY holdChanged)
    Q_PROPERTY(qreal holdTime READ holdTime NOTIFY holdChanged)
    Q_PROPERTY(qreal maxHoldTime READ holdLimit CONSTANT)

public:
    enum State {
        Idle = 0,
        Serving,
        Playing,
        Paused,
        Finished
    };
    Q_ENUM(State)

    static constexpr int minPlayers = 3;
    static constexpr int maxPlayers = 6;
    // Steepest angle (degrees) off a paddle, from the normal of its side
    static constexpr qreal maxBounceAngle = 55.0;
    // Serves never go straight at a player, nor too far off
    static constexpr qreal maxServeAngle = 25.0;

    explicit PartyMatch(QObject* parent = nullptr);

    // Score, hits, power and the effects on a player, the same object for
    // an index all the time
    Q_INVOKABLE Player* player(int player) const;

    // Outward normal of the side of player, player 0 at the bottom and
    // the others counterclockwise
    Q_INVOKABLE QVector2D normal(int player) const;
    // Direction along the side, counterclockwise
    Q_INVOKABLE QVector2D tangent(int player) const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void advance(qreal dt);

    // offset: where the ball hit the paddle, -1 to 1 along the tangent.
    // paddleVelocity along the tangent puts spin on the ball, smash from 0
    // to 1 is how far the player wound up. See Match::paddleHit().
    Q_INVOKABLE void paddleHit(int player, qreal offset, qreal paddleVelocity = 0.0, qreal smash = 0.0);
    // Reflection off a post or wall, normal points from it to the ball.
    // Returns whether the ball bounced.
    Q_INVOKABLE bool bounce(const QVector2D& normal);
    // The shield in front of the goal of player sends the ball back and
    // is gone, returns whether it did
    Q_INVOKABLE bool shieldHit(int player);
    Q_INVOKABLE void scaleBallSpeed(qreal factor);
    // Bends the flight towards a gravity well, see Match::attract()
    Q_INVOKABLE void attract(const QVector2D& position, const QVector2D& well, qreal strength, qreal dt);

    // The special and the catch it gives, see Match::useSpecial()
    Q_INVOKABLE bool useSpecial(int player);
    Q_INVOKABLE bool catchBall(int player, qreal offset);
    Q_INVOKABLE void aimHeldBall(qreal offset);
    Q_INVOKABLE bool releaseBall(qreal smash = 0.0);
    // The ball went into the goal of player
    Q_INVOKABLE void goal(int player);

    Q_INVOKABLE bool isAlive(int player) const;

    // The state for the players on the network, and applying it on their
    // side, where no rules run
    Q_INVOKABLE QVariantMap snapshot() const;
    Q_INVOKABLE void applySnapshot(const QVariantMap& snapshot);

    void setPlayers(int players);
    int players() const;

    void setLives(int lives);
    int lives() const;

    Ball* ball() const;
    State state() const;
    QVariantList livesLeft() const;
    int alive() const;
    int winner() const;

    void setServeSpeed(qreal serveSpeed);
    qreal serveSpeed() const;

    void setMaxSpeed(qreal maxSpeed);
    qreal maxSpeed() const;

    void setSpeedUp(qreal speedUp);
    qreal speedUp() const;

    void setServeDelay(qreal serveDelay);
    qreal serveDelay() const;

    qreal serveCountdown() const;
    QVector2D serveDirection() const;
    int rally() const;
    int longestRally() const;

    void setPaddleSpeed(qreal paddleSpeed);
    qreal paddleSpeed() const;

    int lastTouch() const;
    int heldBy() const;
    qreal holdOffset() const;
    qreal holdTime() const;
    qreal holdLimit() const;

    void setSeed(quint32 seed);

signals:
    void playersChanged(int);
    void livesChanged(int);
    void stateChanged(PartyMatch::State);
    void livesLeftChanged();
    void winnerChanged(int);
    void serveSpeedChanged(qreal);
    void maxSpeedChanged(qreal);
    void speedUpChanged(qreal);
    void serveDelayChanged(qreal);
    void serveCountdownChanged(qreal);
    void serveDirectionChanged();
    void rallyChanged(int);
    void longestRallyChanged(int);
    void paddleSpeedChanged(qreal);
    void lastTouchChanged(int);
    void holdChanged();

    void served();
    void paddleHitBall(int player, qreal smash, bool perfect);
    void ballCaught(int player);
    void specialUsed(int player);
    void shieldUsed(int player);
    void goalScored(int player);
    void playerOut(int player);
    void finished();

private:
    void setState(State state);
    void setRally(int rally);
    void setLastTouch(int player);
    void setHold(int player, qreal offset, qreal time);
    // Back into the field from player, turned by angle (radians) off the normal
    void hit(int player, qreal angle, qreal speed, qreal spin, qreal smash, bool perfect);
    void curve(qreal dt);
    // The countdown to a kickoff at one of the players still in, not
    // conceder unless nobody else is left
    void prepareServe(int conceder);

    int m_players;
    int m_lives;
    Ball* m_ball;
    State m_state;
    State m_pausedState;
    QList<int> m_livesLeft;
    int m_winner;
    qreal m_serveSpeed;
    qreal m_maxSpeed;
    qreal m_speedUp;
    qreal m_serveDelay;
    qreal m_serveCountdown;
    QVector2D m_serveDirection;
    int m_rally;
    int m_longestRally;
    qreal m_paddleSpeed;
    int m_lastTouch;
    int m_heldBy;
    qreal m_holdOffset;
    qreal m_holdTime;
    qreal m_heldSpeed;
    QList<Player*> m_playerObjects;
    QRandomGenerator m_random;
};

#endif // PARTYMATCH_H
