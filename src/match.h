#ifndef MATCH_H
#define MATCH_H

#include "player.h"

#include <QObject>
#include <QVector2D>
#include <QtQml/qqmlregistration.h>

// Rules of a single match: score, serve, win condition and the arcade
// bounce behaviour of the ball. The physics engine detects the collisions
// and moves the ball, the match decides where the ball goes next.
class Match : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(Player* left READ left CONSTANT)
    Q_PROPERTY(Player* right READ right CONSTANT)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(Player* winner READ winner NOTIFY winnerChanged)
    Q_PROPERTY(int pointsToWin READ pointsToWin WRITE setPointsToWin NOTIFY pointsToWinChanged)
    Q_PROPERTY(qreal serveSpeed READ serveSpeed WRITE setServeSpeed NOTIFY serveSpeedChanged)
    Q_PROPERTY(qreal maxSpeed READ maxSpeed WRITE setMaxSpeed NOTIFY maxSpeedChanged)
    Q_PROPERTY(qreal speedUp READ speedUp WRITE setSpeedUp NOTIFY speedUpChanged)
    Q_PROPERTY(qreal serveDelay READ serveDelay WRITE setServeDelay NOTIFY serveDelayChanged)
    Q_PROPERTY(qreal serveCountdown READ serveCountdown NOTIFY serveCountdownChanged)
    Q_PROPERTY(QVector2D ballVelocity READ ballVelocity NOTIFY ballVelocityChanged)
    Q_PROPERTY(int rally READ rally NOTIFY rallyChanged)
    Q_PROPERTY(int longestRally READ longestRally NOTIFY longestRallyChanged)
    Q_PROPERTY(int totalHits READ totalHits NOTIFY totalHitsChanged)
    Q_PROPERTY(qreal playTime READ playTime NOTIFY playTimeChanged)

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
        LeftSide = 0,
        RightSide
    };
    Q_ENUM(Side)

    // Steepest angle (degrees) a paddle can send the ball off at
    static constexpr qreal maxBounceAngle = 60.0;
    // Serve angle range (degrees), both up and down
    static constexpr qreal minServeAngle = 10.0;
    static constexpr qreal maxServeAngle = 30.0;

    explicit Match(QObject* parent = nullptr);

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();

    // Advances serve countdown and play time, call once per simulation step
    Q_INVOKABLE void advance(qreal dt);

    // offset: where the ball hit the paddle, -1 (bottom edge) to 1 (top edge)
    Q_INVOKABLE void paddleHit(Match::Side side, qreal offset);
    Q_INVOKABLE void wallHit(bool top);
    Q_INVOKABLE void goal(Match::Side scorer);

    Player* left() const;
    Player* right() const;
    Player* player(Side side) const;

    State state() const;
    Player* winner() const;

    void setPointsToWin(int pointsToWin);
    int pointsToWin() const;

    void setServeSpeed(qreal serveSpeed);
    qreal serveSpeed() const;

    void setMaxSpeed(qreal maxSpeed);
    qreal maxSpeed() const;

    void setSpeedUp(qreal speedUp);
    qreal speedUp() const;

    void setServeDelay(qreal serveDelay);
    qreal serveDelay() const;

    qreal serveCountdown() const;
    QVector2D ballVelocity() const;

    int rally() const;
    int longestRally() const;
    int totalHits() const;
    qreal playTime() const;

signals:
    void stateChanged(Match::State);
    void winnerChanged(Player*);
    void pointsToWinChanged(int);
    void serveSpeedChanged(qreal);
    void maxSpeedChanged(qreal);
    void speedUpChanged(qreal);
    void serveDelayChanged(qreal);
    void serveCountdownChanged(qreal);
    void ballVelocityChanged(const QVector2D&);
    void rallyChanged(int);
    void longestRallyChanged(int);
    void totalHitsChanged(int);
    void playTimeChanged(qreal);

    void served();
    void pointScored(Match::Side scorer);
    void finished();

private:
    void setState(State state);
    void setWinner(Player* winner);
    void setServeCountdown(qreal serveCountdown);
    void setBallVelocity(const QVector2D& ballVelocity);
    void setRally(int rally);
    void setPlayTime(qreal playTime);
    void serve();

    Player* m_left;
    Player* m_right;

    State m_state;
    State m_pausedState;
    Player* m_winner;
    Side m_serveTo;

    int m_pointsToWin;
    qreal m_serveSpeed;
    qreal m_maxSpeed;
    qreal m_speedUp;
    qreal m_serveDelay;
    qreal m_serveCountdown;

    QVector2D m_ballVelocity;

    int m_rally;
    int m_longestRally;
    int m_totalHits;
    qreal m_playTime;
};

#endif // MATCH_H
