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

QList<Stats::Achievement> Stats::achievementList()
{
    return {
        { QStringLiteral("firstWin"), tr("First win"), tr("Beat the computer") },
        { QStringLiteral("beatHard"), tr("Top level"), tr("Beat the computer on Hard") },
        { QStringLiteral("shutout"), tr("Shutout"), tr("Win without losing a point") },
        { QStringLiteral("comeback"), tr("Comeback"), tr("Win after trailing by three") },
        { QStringLiteral("purist"), tr("Purist"), tr("Beat the computer with modifiers off") },
        { QStringLiteral("smashGoal"), tr("Smash hit"), tr("Score with a smash") },
        { QStringLiteral("perfectionist"), tr("Perfectionist"), tr("Five perfect hits in a match") },
        { QStringLiteral("goingUp"), tr("Going up"), tr("A rally of 20 on Elevators") },
        { QStringLiteral("champion"), tr("Champion"), tr("Win the tournament") },
        { QStringLiteral("demolition"), tr("Demolition"), tr("Break the last brick of a wall") },
        { QStringLiteral("squashPro"), tr("Squash pro"), tr("A rally of 25 in squash") },
        { QStringLiteral("endurance"), tr("Endurance"), tr("Score 100 in endless") }
    };
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

bool Stats::recordSquash(int rally)
{
    if (rally <= squashBest())
        return false;

    setValue(QStringLiteral("stats/squashBest"), rally);
    emit changed();
    return true;
}

bool Stats::unlock(const QString& id)
{
    const QList<Achievement> list = achievementList();
    const auto it = std::find_if(list.cbegin(), list.cend(),
                                 [&id](const Achievement& achievement) { return achievement.id == id; });
    if (it == list.cend() || isUnlocked(id))
        return false;

    m_settings.setValue(QStringLiteral("stats/achievements"), unlocked() << id);
    emit changed();
    emit achievementUnlocked(it->name);
    return true;
}

bool Stats::isUnlocked(const QString& id) const
{
    return unlocked().contains(id);
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

int Stats::squashBest() const
{
    return value(QStringLiteral("stats/squashBest"));
}

QVariantList Stats::achievements() const
{
    const QStringList done = unlocked();
    QVariantList achievements;
    for (const Achievement& achievement : achievementList()) {
        achievements.append(QVariantMap{
            { QStringLiteral("id"), achievement.id },
            { QStringLiteral("name"), achievement.name },
            { QStringLiteral("description"), achievement.description },
            { QStringLiteral("unlocked"), done.contains(achievement.id) }
        });
    }
    return achievements;
}

int Stats::unlockedCount() const
{
    const QStringList done = unlocked();
    const QList<Achievement> list = achievementList();
    return int(std::count_if(list.cbegin(), list.cend(),
                             [&done](const Achievement& achievement) { return done.contains(achievement.id); }));
}

QStringList Stats::unlocked() const
{
    return m_settings.value(QStringLiteral("stats/achievements")).toStringList();
}

int Stats::value(const QString& key) const
{
    return std::max(m_settings.value(key, 0).toInt(), 0);
}

void Stats::setValue(const QString& key, int value)
{
    m_settings.setValue(key, value);
}
