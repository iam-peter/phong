#ifndef COMPUTERPLAYER_H
#define COMPUTERPLAYER_H

#include <QObject>
#include <QVector2D>
#include <QtQml/qqmlregistration.h>

// Steers a paddle towards where the ball will cross the paddle line.
// The difficulty decides whether bounces are foreseen, how often the plan
// is refreshed, how accurate it is and how fast the paddle may move.
class ComputerPlayer : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(Difficulty difficulty READ difficulty WRITE setDifficulty NOTIFY difficultyChanged)
    Q_PROPERTY(qreal paddleX READ paddleX WRITE setPaddleX NOTIFY paddleXChanged)
    Q_PROPERTY(qreal paddleReach READ paddleReach WRITE setPaddleReach NOTIFY paddleReachChanged)
    Q_PROPERTY(qreal fieldTop READ fieldTop WRITE setFieldTop NOTIFY fieldTopChanged)
    Q_PROPERTY(qreal fieldBottom READ fieldBottom WRITE setFieldBottom NOTIFY fieldBottomChanged)
    Q_PROPERTY(qreal target READ target NOTIFY targetChanged)
    Q_PROPERTY(qreal direction READ direction NOTIFY directionChanged)

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

    qreal target() const;

    // Paddle input from -1 (full speed down) to 1 (full speed up)
    qreal direction() const;

signals:
    void difficultyChanged(ComputerPlayer::Difficulty);
    void paddleXChanged(qreal);
    void paddleReachChanged(qreal);
    void fieldTopChanged(qreal);
    void fieldBottomChanged(qreal);
    void targetChanged(qreal);
    void directionChanged(qreal);

private:
    struct Profile {
        qreal reactionTime; // seconds between two plans
        qreal aimError;     // random aim error, fraction of the paddle reach
        qreal maxInput;     // fraction of the paddle speed
        bool predicts;      // plans for bounces or just chases the ball
    };

    Profile profile() const;
    void plan(const QVector2D& ballPosition, const QVector2D& ballVelocity);
    void setTarget(qreal target);
    void setDirection(qreal direction);

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
};

#endif // COMPUTERPLAYER_H
