#ifndef MATCH_H
#define MATCH_H

#include "player.h"

#include <QAbstractListModel>
#include <QObject>
#include <QVector2D>
#include <QtQml/qqmlregistration.h>

class Ball;
class BallModel;

// Rules of a single match: score, sets, serve, win condition and the arcade
// bounce behaviour of the balls. The physics engine detects the collisions
// and moves the balls, the match decides where they go next.
class Match : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(Player* left READ left CONSTANT)
    Q_PROPERTY(Player* right READ right CONSTANT)
    Q_PROPERTY(Ball* ball READ ball CONSTANT)
    Q_PROPERTY(BallModel* extraBalls READ extraBalls CONSTANT)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(Player* winner READ winner NOTIFY winnerChanged)
    Q_PROPERTY(int pointsToWin READ pointsToWin WRITE setPointsToWin NOTIFY pointsToWinChanged)
    Q_PROPERTY(int setsToWin READ setsToWin WRITE setSetsToWin NOTIFY setsToWinChanged)
    Q_PROPERTY(bool winByTwo READ winByTwo WRITE setWinByTwo NOTIFY winByTwoChanged)
    // Only the right player can win, reaching pointsToWin, the left one
    // plays for points
    Q_PROPERTY(bool endless READ isEndless WRITE setEndless NOTIFY endlessChanged)
    // The next point can decide the match
    Q_PROPERTY(bool matchPoint READ isMatchPoint NOTIFY matchPointChanged)
    Q_PROPERTY(qreal serveSpeed READ serveSpeed WRITE setServeSpeed NOTIFY serveSpeedChanged)
    Q_PROPERTY(qreal maxSpeed READ maxSpeed WRITE setMaxSpeed NOTIFY maxSpeedChanged)
    Q_PROPERTY(qreal speedUp READ speedUp WRITE setSpeedUp NOTIFY speedUpChanged)
    Q_PROPERTY(qreal paddleSpeed READ paddleSpeed WRITE setPaddleSpeed NOTIFY paddleSpeedChanged)
    Q_PROPERTY(qreal serveDelay READ serveDelay WRITE setServeDelay NOTIFY serveDelayChanged)
    Q_PROPERTY(qreal serveCountdown READ serveCountdown NOTIFY serveCountdownChanged)
    // Where the next kickoff goes, known from the start of the countdown
    Q_PROPERTY(Side serveTo READ serveTo NOTIFY serveDirectionChanged)
    Q_PROPERTY(QVector2D serveDirection READ serveDirection NOTIFY serveDirectionChanged)
    Q_PROPERTY(QVector2D ballVelocity READ ballVelocity NOTIFY ballVelocityChanged)
    Q_PROPERTY(int rally READ rally NOTIFY rallyChanged)
    Q_PROPERTY(int longestRally READ longestRally NOTIFY longestRallyChanged)
    Q_PROPERTY(int totalHits READ totalHits NOTIFY totalHitsChanged)
    Q_PROPERTY(qreal playTime READ playTime NOTIFY playTimeChanged)
    // Seconds a magnetic paddle holds a caught ball at most
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

    enum Side {
        NoSide = -1,
        LeftSide = 0,
        RightSide
    };
    Q_ENUM(Side)

    // Steepest angle (degrees) a paddle can send the ball off at
    static constexpr qreal maxBounceAngle = 60.0;
    // Serve angle range (degrees), both up and down
    static constexpr qreal minServeAngle = 10.0;
    static constexpr qreal maxServeAngle = 30.0;
    // Curve (radians per second) of a ball hit by a paddle at full speed,
    // and how fast the spin wears off
    static constexpr qreal maxSpin = 1.2;
    static constexpr qreal spinDecay = 0.6;
    // A fully charged smash is this much faster, also beyond the max speed
    static constexpr qreal smashBoost = 0.6;
    static constexpr qreal smashOverspeed = 0.25;
    // From this wind up on a hit counts as a smash
    static constexpr qreal smashThreshold = 0.25;
    // A hit in the middle of a paddle that stands still is perfect, and
    // faster, also beyond the max speed
    static constexpr qreal perfectZone = 0.15;
    static constexpr qreal perfectStillness = 0.1;
    static constexpr qreal perfectBoost = 0.15;
    static constexpr qreal perfectOverspeed = 0.1;
    // Seconds a magnetic paddle holds a caught ball at most
    static constexpr qreal maxHoldTime = 1.2;

    explicit Match(QObject* parent = nullptr);

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();

    // Advances serve countdown, play time, curves and extra ball lifetimes,
    // call once per simulation step
    Q_INVOKABLE void advance(qreal dt);

    // offset: where the ball hit the paddle, -1 (bottom edge) to 1 (top
    // edge). A moving paddle puts spin on the ball, smash from 0 to 1 is
    // how far the player wound up. A centered hit with a still paddle is
    // perfect.
    Q_INVOKABLE void paddleHit(Ball* ball, Match::Side side, qreal offset, qreal paddleVelocity = 0.0,
                               qreal smash = 0.0);
    // A paddle that isn't upright reflects the ball off its surface, but
    // always away from its own goal. normal points from the paddle to the ball.
    Q_INVOKABLE void deflect(Ball* ball, Match::Side side, const QVector2D& normal);
    // Plain reflection off an obstacle, normal points from it to the ball.
    // Returns whether the ball bounced.
    Q_INVOKABLE bool bounce(Ball* ball, const QVector2D& normal);
    // The shield in front of the goal of side sends the ball back,
    // returns whether it did
    Q_INVOKABLE bool shieldHit(Ball* ball, Match::Side side);
    Q_INVOKABLE void scaleBallSpeed(Ball* ball, qreal factor);
    Q_INVOKABLE void wallHit(Ball* ball, bool top);
    Q_INVOKABLE void goal(Ball* ball, Match::Side scorer);

    // A magnetic paddle catches a ball running into it at offset (see
    // paddleHit), returns whether it did. The paddle holds it until
    // released, at most maxHoldTime.
    Q_INVOKABLE bool catchBall(Ball* ball, Match::Side side, qreal offset);
    // Where on the paddle a held ball sits, aims the release
    Q_INVOKABLE void aimHeldBall(Ball* ball, qreal offset);
    // Sends a held ball off like a paddle hit at its offset
    Q_INVOKABLE bool releaseBall(Ball* ball, qreal smash = 0.0);

    // An extra ball flying from position towards side, gone after lifetime
    // seconds or its goal
    Q_INVOKABLE Ball* addBall(const QVector2D& position, Match::Side towards, qreal lifetime,
                              Match::Side lastTouch);

    // The same for the main ball
    void paddleHit(Side side, qreal offset, qreal paddleVelocity = 0.0, qreal smash = 0.0);
    void deflect(Side side, const QVector2D& normal);
    bool shieldHit(Side side);
    void scaleBallSpeed(qreal factor);
    void wallHit(bool top);
    void goal(Side scorer);

    // Whether side wins the match with its next point
    Q_INVOKABLE bool winsWithNextPoint(Match::Side side) const;

    Player* left() const;
    Player* right() const;
    Player* player(Side side) const;
    static Side opponent(Side side);

    Ball* ball() const;
    BallModel* extraBalls() const;
    QList<Ball*> balls() const;

    State state() const;
    Player* winner() const;

    void setPointsToWin(int pointsToWin);
    int pointsToWin() const;

    // Sets a player needs, 2 is best of three
    void setSetsToWin(int setsToWin);
    int setsToWin() const;

    // A set needs a lead of two points
    void setWinByTwo(bool winByTwo);
    bool winByTwo() const;

    void setEndless(bool endless);
    bool isEndless() const;

    bool isMatchPoint() const;

    void setServeSpeed(qreal serveSpeed);
    qreal serveSpeed() const;

    void setMaxSpeed(qreal maxSpeed);
    qreal maxSpeed() const;

    void setSpeedUp(qreal speedUp);
    qreal speedUp() const;

    // Paddle speed at which a hit gets the full spin
    void setPaddleSpeed(qreal paddleSpeed);
    qreal paddleSpeed() const;

    void setServeDelay(qreal serveDelay);
    qreal serveDelay() const;

    qreal serveCountdown() const;
    Side serveTo() const;
    QVector2D serveDirection() const;
    QVector2D ballVelocity() const;
    Side lastTouch() const;

    int rally() const;
    int longestRally() const;
    int totalHits() const;
    qreal playTime() const;
    qreal holdLimit() const;

signals:
    void stateChanged(Match::State);
    void winnerChanged(Player*);
    void pointsToWinChanged(int);
    void setsToWinChanged(int);
    void winByTwoChanged(bool);
    void endlessChanged(bool);
    void matchPointChanged(bool);
    void serveSpeedChanged(qreal);
    void maxSpeedChanged(qreal);
    void speedUpChanged(qreal);
    void paddleSpeedChanged(qreal);
    void serveDelayChanged(qreal);
    void serveCountdownChanged(qreal);
    void serveDirectionChanged();
    void ballVelocityChanged(const QVector2D&);
    void rallyChanged(int);
    void longestRallyChanged(int);
    void totalHitsChanged(int);
    void playTimeChanged(qreal);

    void served();
    void paddleHitBall(Ball* ball, Match::Side side, qreal smash, bool perfect);
    void ballCaught(Ball* ball, Match::Side side);
    void pointScored(Match::Side scorer, Ball* ball);
    void setFinished(Match::Side winner);
    void finished();

private:
    void setState(State state);
    void setWinner(Player* winner);
    void setServeCountdown(qreal serveCountdown);
    void setRally(int rally);
    void setPlayTime(qreal playTime);
    void hit(Ball* ball, Side side, const QVector2D& velocity, qreal spin, qreal smash = 0.0,
             bool perfect = false);
    // Ends the rally after a point of scorer, setWon if it won the set
    void endRally(Side scorer, bool setWon);
    // Adds a point, returns whether it wins the set
    bool addPoint(Side scorer);
    void updateMatchPoint();
    void curve(Ball* ball, qreal dt);
    void removeExtraBalls();
    bool isActive(Ball* ball) const;
    // The countdown to the kickoff towards side
    void prepareServe(Side towards);
    void serve();

    Player* m_left;
    Player* m_right;
    Ball* m_ball;
    BallModel* m_extraBalls;

    State m_state;
    State m_pausedState;
    Player* m_winner;
    Side m_serveTo;
    QVector2D m_serveDirection;

    int m_pointsToWin;
    int m_setsToWin;
    bool m_winByTwo;
    bool m_endless;
    bool m_matchPoint;
    qreal m_serveSpeed;
    qreal m_maxSpeed;
    qreal m_speedUp;
    qreal m_paddleSpeed;
    qreal m_serveDelay;
    qreal m_serveCountdown;

    int m_rally;
    int m_longestRally;
    int m_totalHits;
    qreal m_playTime;
};

// A ball in play. The match owns the balls and changes them.
class Ball : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Balls are owned by a Match")
    Q_PROPERTY(QVector2D velocity READ velocity NOTIFY velocityChanged)
    Q_PROPERTY(qreal spin READ spin NOTIFY spinChanged)
    Q_PROPERTY(Match::Side lastTouch READ lastTouch NOTIFY lastTouchChanged)
    Q_PROPERTY(bool extra READ isExtra CONSTANT)
    Q_PROPERTY(QVector2D spawnPosition READ spawnPosition CONSTANT)
    Q_PROPERTY(qreal lifetime READ lifetime NOTIFY lifetimeChanged)
    // The last hit was a smash
    Q_PROPERTY(bool smashed READ isSmashed NOTIFY smashedChanged)
    // The side whose magnetic paddle holds the ball, where on the paddle
    // and for how many more seconds
    Q_PROPERTY(Match::Side heldBy READ heldBy NOTIFY holdChanged)
    Q_PROPERTY(qreal holdOffset READ holdOffset NOTIFY holdChanged)
    Q_PROPERTY(qreal holdTime READ holdTime NOTIFY holdChanged)

public:
    explicit Ball(bool extra, const QVector2D& spawnPosition, QObject* parent = nullptr);

    QVector2D velocity() const;
    // Radians per second the flight curves with, positive is counterclockwise
    qreal spin() const;
    Match::Side lastTouch() const;
    bool isExtra() const;
    QVector2D spawnPosition() const;
    // Seconds left for an extra ball
    qreal lifetime() const;
    bool isSmashed() const;
    Match::Side heldBy() const;
    qreal holdOffset() const;
    qreal holdTime() const;

signals:
    void velocityChanged(const QVector2D&);
    void spinChanged(qreal);
    void lastTouchChanged(Match::Side);
    void lifetimeChanged(qreal);
    void smashedChanged(bool);
    void holdChanged();

private:
    friend class Match;

    void setVelocity(const QVector2D& velocity);
    void setSpin(qreal spin);
    void setLastTouch(Match::Side lastTouch);
    void setLifetime(qreal lifetime);
    void setSmashed(bool smashed);
    void setHold(Match::Side heldBy, qreal offset, qreal time);

    QVector2D m_velocity;
    qreal m_spin;
    Match::Side m_lastTouch;
    bool m_extra;
    QVector2D m_spawnPosition;
    qreal m_lifetime;
    bool m_smashed;
    Match::Side m_heldBy;
    qreal m_holdOffset;
    qreal m_holdTime;
    qreal m_heldSpeed;
};

// The extra balls, a model so a new ball doesn't recreate the others
class BallModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by a Match")
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        BallRole = Qt::UserRole + 1
    };

    explicit BallModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    const QList<Ball*>& balls() const;
    void append(Ball* ball);
    // Removes and deletes the ball later, it may still be reporting
    void remove(Ball* ball);
    void clear();

signals:
    void countChanged();

private:
    QList<Ball*> m_balls;
};

#endif // MATCH_H
