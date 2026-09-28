#include "player.h"

Player::Player(QObject* parent):
    QObject(parent),
    m_name(),
    m_score(0),
    m_computer(false),
    m_paddleScale(1.0),
    m_spinning(false),
    m_shielded(false)
{}

void Player::setName(const QString& name)
{
    if (m_name == name)
        return;

    m_name = name;
    emit nameChanged(name);
}

QString Player::name() const
{
    return m_name;
}

void Player::setScore(int score)
{
    if (m_score == score)
        return;

    m_score = score;
    emit scoreChanged(score);
}

int Player::score() const
{
    return m_score;
}

void Player::setComputer(bool computer)
{
    if (m_computer == computer)
        return;

    m_computer = computer;
    emit computerChanged(computer);
}

bool Player::isComputer() const
{
    return m_computer;
}

void Player::setPaddleScale(qreal paddleScale)
{
    if (m_paddleScale == paddleScale)
        return;

    m_paddleScale = paddleScale;
    emit paddleScaleChanged(paddleScale);
}

qreal Player::paddleScale() const
{
    return m_paddleScale;
}

void Player::setSpinning(bool spinning)
{
    if (m_spinning == spinning)
        return;

    m_spinning = spinning;
    emit spinningChanged(spinning);
}

bool Player::isSpinning() const
{
    return m_spinning;
}

void Player::setShielded(bool shielded)
{
    if (m_shielded == shielded)
        return;

    m_shielded = shielded;
    emit shieldedChanged(shielded);
}

bool Player::isShielded() const
{
    return m_shielded;
}
