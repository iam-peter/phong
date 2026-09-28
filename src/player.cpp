#include "player.h"

Player::Player(QObject* parent):
    QObject(parent),
    m_name(),
    m_score(0),
    m_computer(false)
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
