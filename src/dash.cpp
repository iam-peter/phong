#include "dash.h"

#include <algorithm>

Dash::Dash(QObject* parent):
    QObject(parent),
    m_clock(0.0),
    m_lastTap(-1.0),
    m_lastTapDirection(0),
    m_timeLeft(0.0),
    m_direction(0),
    m_cooldownLeft(0.0)
{}

bool Dash::tap(int direction)
{
    direction = direction > 0 ? 1 : direction < 0 ? -1 : 0;
    if (direction == 0)
        return false;

    const bool doubleTap = direction == m_lastTapDirection && m_lastTap >= 0.0
                           && m_clock - m_lastTap <= doubleTapTime;
    if (doubleTap && trigger(direction)) {
        // A third tap starts counting anew
        m_lastTap = -1.0;
        m_lastTapDirection = 0;
        return true;
    }

    m_lastTap = m_clock;
    m_lastTapDirection = direction;
    return false;
}

bool Dash::trigger(int direction)
{
    direction = direction > 0 ? 1 : direction < 0 ? -1 : 0;
    if (direction == 0 || !isReady())
        return false;

    setTimeLeft(duration, direction);
    setCooldownLeft(cooldownTime);
    emit dashed(direction);
    return true;
}

void Dash::advance(qreal dt)
{
    m_clock += dt;
    if (m_timeLeft > 0.0) {
        const qreal timeLeft = std::max(m_timeLeft - dt, 0.0);
        setTimeLeft(timeLeft, timeLeft > 0.0 ? m_direction : 0);
    }
    if (m_cooldownLeft > 0.0)
        setCooldownLeft(std::max(m_cooldownLeft - dt, 0.0));
}

void Dash::reset()
{
    m_lastTap = -1.0;
    m_lastTapDirection = 0;
    setTimeLeft(0.0, 0);
    setCooldownLeft(0.0);
}

bool Dash::isActive() const
{
    return m_timeLeft > 0.0;
}

int Dash::direction() const
{
    return m_direction;
}

bool Dash::isReady() const
{
    return m_cooldownLeft <= 0.0;
}

qreal Dash::cooldown() const
{
    return m_cooldownLeft / cooldownTime;
}

qreal Dash::boost() const
{
    return speedFactor;
}

void Dash::setTimeLeft(qreal timeLeft, int direction)
{
    const bool wasActive = isActive();
    const int oldDirection = m_direction;
    m_timeLeft = timeLeft;
    m_direction = direction;
    if (wasActive != isActive() || oldDirection != direction)
        emit activeChanged();
}

void Dash::setCooldownLeft(qreal cooldownLeft)
{
    if (m_cooldownLeft == cooldownLeft)
        return;

    const bool wasReady = isReady();
    m_cooldownLeft = cooldownLeft;
    emit cooldownChanged(cooldown());
    if (wasReady != isReady())
        emit readyChanged(isReady());
}
