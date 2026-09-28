#ifndef COMPUTERPLAYER_H
#define COMPUTERPLAYER_H

#include <QObject>
#include <QRandomGenerator>
#include <QVariantList>
#include <QVector2D>
#include <QtQml/qqmlregistration.h>

// Steers a paddle towards where the ball will cross the paddle line.
// The difficulty decides whether bounces are foreseen, how often the plan
// is refreshed, how accurate it is, how fast the paddle may move and
// whether the return is aimed at a target or away from the opponent.
class ComputerPlayer : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(Difficulty difficulty READ difficulty WRITE setDifficulty NOTIFY difficultyChanged)
    Q_PROPERTY(qreal paddleX READ paddleX WRITE setPaddleX NOTIFY paddleXChanged)
    Q_PROPERTY(qreal paddleReach READ paddleReach WRITE setPaddleReach NOTIFY paddleReachChanged)
    Q_PROPERTY(qreal fieldTop READ fieldTop WRITE setFieldTop NOTIFY fieldTopChanged)
    Q_PROPERTY(qreal fieldBottom READ fieldBottom WRITE setFieldBottom NOTIFY fieldBottomChanged)
    // Points worth sending the ball through, e.g. modifiers
    Q_PROPERTY(QVariantList targets READ targets WRITE setTargets NOTIFY targetsChanged)
    Q_PROPERTY(qreal opponentY READ opponentY WRITE setOpponentY NOTIFY opponentYChanged)
    Q_PROPERTY(qreal target READ target NOTIFY targetChanged)
    Q_PROPERTY(qreal direction READ direction NOTIFY directionChanged)
    // Winds up a smash for the next return
    Q_PROPERTY(bool charging READ isCharging NOTIFY chargingChanged)
    // Full paddle speed, to know when only a dash makes it in time
    Q_PROPERTY(qreal paddleSpeed READ paddleSpeed WRITE setPaddleSpeed NOTIFY paddleSpeedChanged)
    // Only a dash reaches the ball in time
    Q_PROPERTY(bool wantsDash READ wantsDash NOTIFY wantsDashChanged)
    // Reversed controls, 0 to 1: slower to react, less accurate
    Q_PROPERTY(qreal confusion READ confusion WRITE setConfusion NOTIFY confusionChanged)
    // Can't see the ball, keeps following the last plan
    Q_PROPERTY(bool blind READ isBlind WRITE setBlind NOTIFY blindChanged)

public:
    enum Difficulty {
        Easy = 0,
        Normal,
        Hard
    };
    Q_ENUM(Difficulty)

    explicit ComputerPlayer(QObject* parent = nullptr);

    // Where a ball at position, moving with velocity, crosses x = lineX.
    // Top and bottom are the limits for the ball center, bounces between
    // them are folded in.
    static qreal predictY(const QVector2D& position, const QVector2D& velocity,
                          qreal lineX, qreal top, qreal bottom);

    // Paddle offset (see Match::paddleHit) that sends a ball leaving the
    // paddle line at hitY straight through the easiest of the targets. NaN
    // if none can be reached. A shot off a wall is always steeper.
    static qreal aimOffset(qreal hitY, qreal lineX, const QList<QVector2D>& targets);

    // Offset that sends the ball to the wall away from the opponent
    static qreal awayOffset(qreal hitY, qreal lineX, qreal opponentY, qreal top, qreal bottom);

    // predictY() within this player's field
    Q_INVOKABLE qreal predictCrossing(const QVector2D& position, const QVector2D& velocity,
                                      qreal lineX) const;

    // Where on the paddle to put a caught ball held at hitY before
    // releasing it, see Match::aimHeldBall()
    Q_INVOKABLE qreal holdAim(qreal hitY) const;

    // Forget the current plan, e.g. after a point was scored
    Q_INVOKABLE void reset();

    // Call once per simulation step, afterwards direction holds the input
    Q_INVOKABLE void update(qreal dt, const QVector2D& ballPosition,
                            const QVector2D& ballVelocity, qreal paddleY);

    void setDifficulty(Difficulty difficulty);
    Difficulty difficulty() const;

    void setPaddleX(qreal paddleX);
    qreal paddleX() const;

    // Distance from the paddle center to its edge plus the ball radius
    void setPaddleReach(qreal paddleReach);
    qreal paddleReach() const;

    void setFieldTop(qreal fieldTop);
    qreal fieldTop() const;

    void setFieldBottom(qreal fieldBottom);
    qreal fieldBottom() const;

    void setTargets(const QVariantList& targets);
    QVariantList targets() const;

    void setOpponentY(qreal opponentY);
    qreal opponentY() const;

    void setPaddleSpeed(qreal paddleSpeed);
    qreal paddleSpeed() const;

    bool wantsDash() const;

    void setConfusion(qreal confusion);
    qreal confusion() const;

    void setBlind(bool blind);
    bool isBlind() const;

    void setSeed(quint32 seed);

    qreal target() const;

    // Paddle input from -1 (full speed down) to 1 (full speed up)
    qreal direction() const;

    bool isCharging() const;

signals:
    void difficultyChanged(ComputerPlayer::Difficulty);
    void paddleXChanged(qreal);
    void paddleReachChanged(qreal);
    void fieldTopChanged(qreal);
    void fieldBottomChanged(qreal);
    void targetsChanged();
    void opponentYChanged(qreal);
    void targetChanged(qreal);
    void directionChanged(qreal);
    void chargingChanged(bool);
    void paddleSpeedChanged(qreal);
    void wantsDashChanged(bool);
    void confusionChanged(qreal);
    void blindChanged(bool);

private:
    struct Profile {
        qreal reactionTime; // seconds between two plans
        qreal aimError;     // random aim error, fraction of the paddle reach
        qreal maxInput;     // fraction of the paddle speed
        bool predicts;      // plans for bounces or just chases the ball
        qreal aimChance;    // share of returns aimed at a target
        bool tactics;       // otherwise plays away from the opponent
        qreal smashChance;  // share of returns smashed
        bool dashes;        // dashes when the ball is out of reach otherwise
    };

    // Seconds the wind up of a smash takes
    static constexpr qreal smashWindUp = 0.9;

    // The profile of the difficulty, and with the confusion
    Profile baseProfile() const;
    Profile profile() const;
    void plan(const QVector2D& ballPosition, const QVector2D& ballVelocity);
    void setTarget(qreal target);
    void setDirection(qreal direction);
    void setCharging(bool charging);
    void setWantsDash(bool wantsDash);

    Difficulty m_difficulty;
    qreal m_paddleX;
    qreal m_paddleReach;
    qreal m_fieldTop;
    qreal m_fieldBottom;

    qreal m_target;
    qreal m_direction;
    qreal m_sincePlan;
    bool m_approaching;
    qreal m_aimError; // -1 to 1, rolled once per approach
    bool m_aiming;    // rolled once per approach
    bool m_smashing;  // rolled once per approach
    bool m_charging;
    qreal m_paddleSpeed;
    bool m_wantsDash;
    qreal m_confusion;
    bool m_blind;
    QList<QVector2D> m_targets;
    qreal m_opponentY;
    QRandomGenerator m_random;
};

#endif // COMPUTERPLAYER_H
