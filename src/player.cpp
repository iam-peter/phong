#include "player.h"

#include <algorithm>

Player::Player(QObject* parent):
    QObject(parent),
    m_name(),
    m_score(0),
    m_sets(0),
    m_hits(0),
    m_computer(false),
    m_paddleScale(1.0),
    m_spinSpeed(0.0),
    m_shielded(false),
    m_catches(0)
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

void Player::setSets(int sets)
{
    if (m_sets == sets)
        return;

    m_sets = sets;
    emit setsChanged(sets);
}

int Player::sets() const
{
    return m_sets;
}

void Player::setHits(int hits)
{
    if (m_hits == hits)
        return;

    m_hits = hits;
    emit hitsChanged(hits);
}

int Player::hits() const
{
    return m_hits;
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

void Player::setSpinSpeed(qreal spinSpeed)
{
    if (m_spinSpeed == spinSpeed)
        return;

    m_spinSpeed = spinSpeed;
    emit spinSpeedChanged(spinSpeed);
}

qreal Player::spinSpeed() const
{
    return m_spinSpeed;
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

void Player::setCatches(int catches)
{
    catches = std::max(catches, 0);
    if (m_catches == catches)
        return;

    m_catches = catches;
    emit catchesChanged(catches);
}

int Player::catches() const
{
    return m_catches;
}
