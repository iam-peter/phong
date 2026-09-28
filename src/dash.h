#ifndef DASH_H
#define DASH_H

#include <QObject>
#include <QtQml/qqmlregistration.h>

// A short burst of paddle speed, started by tapping a direction twice.
// After a dash the paddle needs a moment before the next one.
class Dash : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
    // 1 up, -1 down, 0 while not dashing
    Q_PROPERTY(int direction READ direction NOTIFY activeChanged)
    Q_PROPERTY(bool ready READ isReady NOTIFY readyChanged)
    // Share of the cooldown left, 1 right after a dash, 0 when ready
    Q_PROPERTY(qreal cooldown READ cooldown NOTIFY cooldownChanged)
    // Paddle speed factor while dashing
    Q_PROPERTY(qreal boost READ boost CONSTANT)

public:
    // Seconds between the two taps
    static constexpr qreal doubleTapTime = 0.25;
    static constexpr qreal duration = 0.15;
    static constexpr qreal speedFactor = 2.5;
    static constexpr qreal cooldownTime = 1.2;

    explicit Dash(QObject* parent = nullptr);

    // A key press in direction, the second one in a row within
    // doubleTapTime dashes. Returns whether it did.
    Q_INVOKABLE bool tap(int direction);
    // Dashes right away if ready, e.g. for the computer
    Q_INVOKABLE bool trigger(int direction);
    // Call once per simulation step
    Q_INVOKABLE void advance(qreal dt);
    Q_INVOKABLE void reset();

    bool isActive() const;
    int direction() const;
    bool isReady() const;
    qreal cooldown() const;
    qreal boost() const;

signals:
    void activeChanged();
    void readyChanged(bool);
    void cooldownChanged(qreal);
    void dashed(int direction);

private:
    void setTimeLeft(qreal timeLeft, int direction);
    void setCooldownLeft(qreal cooldownLeft);

    qreal m_clock;
    qreal m_lastTap;
    int m_lastTapDirection;
    qreal m_timeLeft;
    int m_direction;
    qreal m_cooldownLeft;
};

#endif // DASH_H
