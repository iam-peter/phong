#include "tournament.h"

#include <algorithm>

namespace {
qreal strength(ComputerPlayer::Difficulty difficulty)
{
    switch (difficulty) {
        case ComputerPlayer::Difficulty::Easy:
            return 1.0;
        case ComputerPlayer::Difficulty::Hard:
            return 3.0;
        case ComputerPlayer::Difficulty::Normal:
        default:
            return 2.0;
    }
}
}

Tournament::Tournament(QObject* parent):
    QObject(parent),
    m_draw(),
    m_winners(),
    m_round(0),
    m_eliminated(false),
    m_random(QRandomGenerator::global()->generate())
{}

QList<Tournament::Entrant> Tournament::computerEntrants()
{
    using P = ComputerPlayer::Personality;
    using D = ComputerPlayer::Difficulty;
    return {
        { tr("Rookie"), P::Balanced, D::Easy, false },
        { tr("The Wall"), P::Wall, D::Normal, false },
        { tr("Spinner"), P::Spinner, D::Normal, false },
        { tr("Collector"), P::Collector, D::Normal, false },
        { tr("Pro"), P::Balanced, D::Normal, false },
        { tr("Smasher"), P::Smasher, D::Hard, false },
        { tr("Ace"), P::Balanced, D::Hard, false }
    };
}

void Tournament::start()
{
    QList<Entrant> computers = computerEntrants();
    std::shuffle(computers.begin(), computers.end(), m_random);

    m_draw = { { tr("You"), ComputerPlayer::Personality::Balanced, ComputerPlayer::Difficulty::Normal, true } };
    m_draw += computers;

    // The player opens the bracket at the top
    m_winners.clear();
    for (int round = 0; round < roundCount; ++round)
        m_winners.append(QList<int>(size >> (round + 1), -1));

    m_round = 0;
    m_eliminated = false;
    emit changed();
}

void Tournament::recordResult(bool won)
{
    if (!isRunning())
        return;

    const QList<int> players = entrants(m_round);
    const int slot = playerSlot(m_round);
    decideRound(m_round, won ? players.at(slot) : players.at(slot ^ 1));

    if (!won) {
        // The bracket is played to the end without the player
        m_eliminated = true;
        for (int round = m_round + 1; round < roundCount; ++round)
            decideRound(round, -1);
    }
    else if (m_round < roundCount - 1) {
        ++m_round;
    }
    else {
        m_round = roundCount;
    }

    emit changed();
}

int Tournament::round() const
{
    return m_round;
}

bool Tournament::isRunning() const
{
    return !m_draw.isEmpty() && !m_eliminated && m_round < roundCount;
}

bool Tournament::isChampion() const
{
    return !m_draw.isEmpty() && !m_eliminated && m_round >= roundCount;
}

QVariantMap Tournament::opponent() const
{
    if (!isRunning())
        return QVariantMap();

    const QList<int> players = entrants(m_round);
    const Entrant& entrant = m_draw.at(players.at(playerSlot(m_round) ^ 1));
    return {
        { QStringLiteral("name"), entrant.name },
        { QStringLiteral("personality"), int(entrant.personality) },
        { QStringLiteral("difficulty"), int(entrant.difficulty) }
    };
}

QVariantList Tournament::rounds() const
{
    QVariantList rounds;
    if (m_draw.isEmpty())
        return rounds;

    for (int round = 0; round <= roundCount; ++round) {
        const QList<int> players = entrants(round);
        QVariantList entries;
        for (int slot = 0; slot < players.size(); ++slot) {
            const int index = players.at(slot);
            if (index < 0) {
                entries.append(QVariantMap());
                continue;
            }

            // The champion has no match left to lose
            Result result = round == roundCount ? Result::Won : Result::Open;
            if (round < roundCount && m_winners.at(round).at(slot / 2) >= 0)
                result = m_winners.at(round).at(slot / 2) == index ? Result::Won : Result::Lost;

            entries.append(QVariantMap{
                { QStringLiteral("name"), m_draw.at(index).name },
                { QStringLiteral("player"), m_draw.at(index).player },
                { QStringLiteral("result"), int(result) }
            });
        }
        rounds.append(QVariant(entries));
    }
    return rounds;
}

QList<int> Tournament::entrants(int round) const
{
    if (round <= 0) {
        QList<int> players;
        for (int i = 0; i < m_draw.size(); ++i)
            players.append(i);
        return players;
    }

    // The winners of the round before, -1 where the match is open
    return m_winners.value(round - 1, QList<int>(size >> round, -1));
}

void Tournament::setSeed(quint32 seed)
{
    m_random.seed(seed);
}

int Tournament::simulate(int a, int b)
{
    const qreal strengthA = strength(m_draw.at(a).difficulty);
    const qreal strengthB = strength(m_draw.at(b).difficulty);
    return m_random.bounded(strengthA + strengthB) < strengthA ? a : b;
}

void Tournament::decideRound(int round, int playerWinner)
{
    const QList<int> players = entrants(round);
    for (int match = 0; match < players.size() / 2; ++match) {
        const int a = players.at(2 * match);
        const int b = players.at(2 * match + 1);
        const bool playersMatch = m_draw.at(a).player || m_draw.at(b).player;
        m_winners[round][match] = playersMatch && playerWinner >= 0 ? playerWinner : simulate(a, b);
    }
}

int Tournament::playerSlot(int round) const
{
    const QList<int> players = entrants(round);
    for (int slot = 0; slot < players.size(); ++slot) {
        if (players.at(slot) >= 0 && m_draw.at(players.at(slot)).player)
            return slot;
    }
    return -1;
}
