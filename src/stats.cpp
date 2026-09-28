#include "stats.h"

#include <QVariantMap>

#include <algorithm>

namespace {
QString winsKey(int difficulty)
{
    return QStringLiteral("stats/wins%1").arg(difficulty);
}

QString lossesKey(int difficulty)
{
    return QStringLiteral("stats/losses%1").arg(difficulty);
}
}

Stats::Stats(QObject* parent):
    QObject(parent),
    m_settings()
{}

void Stats::recordMatch(bool againstComputer, int difficulty, bool won, int longestRally)
{
    if (againstComputer && difficulty >= 0 && difficulty < difficulties) {
        const QString key = won ? winsKey(difficulty) : lossesKey(difficulty);
        setValue(key, value(key) + 1);
    }
    else if (!againstComputer) {
        setValue(QStringLiteral("stats/twoPlayerMatches"), twoPlayerMatches() + 1);
    }

    setValue(QStringLiteral("stats/longestRally"), std::max(this->longestRally(), longestRally));
    emit changed();
}

void Stats::recordLadder(int levelsBeaten)
{
    if (levelsBeaten <= ladderBest())
        return;

    setValue(QStringLiteral("stats/ladderBest"), std::min(levelsBeaten, difficulties));
    emit changed();
}

bool Stats::recordEndless(int score)
{
    if (score <= endlessBest())
        return false;

    setValue(QStringLiteral("stats/endlessBest"), score);
    emit changed();
    return true;
}

void Stats::recordTournamentWin()
{
    setValue(QStringLiteral("stats/tournamentsWon"), tournamentsWon() + 1);
    emit changed();
}

void Stats::reset()
{
    m_settings.remove(QStringLiteral("stats"));
    emit changed();
}

int Stats::wins(int difficulty) const
{
    return value(winsKey(difficulty));
}

int Stats::losses(int difficulty) const
{
    return value(lossesKey(difficulty));
}

QVariantList Stats::records() const
{
    QVariantList records;
    for (int difficulty = 0; difficulty < difficulties; ++difficulty)
        records.append(QVariantMap{ { "wins", wins(difficulty) }, { "losses", losses(difficulty) } });
    return records;
}

int Stats::twoPlayerMatches() const
{
    return value(QStringLiteral("stats/twoPlayerMatches"));
}

int Stats::longestRally() const
{
    return value(QStringLiteral("stats/longestRally"));
}

int Stats::ladderBest() const
{
    return value(QStringLiteral("stats/ladderBest"));
}

int Stats::endlessBest() const
{
    return value(QStringLiteral("stats/endlessBest"));
}

int Stats::tournamentsWon() const
{
    return value(QStringLiteral("stats/tournamentsWon"));
}

int Stats::value(const QString& key) const
{
    return std::max(m_settings.value(key, 0).toInt(), 0);
}

void Stats::setValue(const QString& key, int value)
{
    m_settings.setValue(key, value);
}
