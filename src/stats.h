#ifndef STATS_H
#define STATS_H

#include <QObject>
#include <QSettings>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// Results across matches, persisted with QSettings
class Stats : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // Wins and losses against the computer, one map per difficulty
    Q_PROPERTY(QVariantList records READ records NOTIFY changed)
    Q_PROPERTY(int twoPlayerMatches READ twoPlayerMatches NOTIFY changed)
    Q_PROPERTY(int longestRally READ longestRally NOTIFY changed)
    // Ladder levels beaten in one run, 3 is all of them
    Q_PROPERTY(int ladderBest READ ladderBest NOTIFY changed)
    Q_PROPERTY(int endlessBest READ endlessBest NOTIFY changed)
    Q_PROPERTY(int tournamentsWon READ tournamentsWon NOTIFY changed)
    Q_PROPERTY(int squashBest READ squashBest NOTIFY changed)

public:
    static constexpr int difficulties = 3;

    explicit Stats(QObject* parent = nullptr);

    Q_INVOKABLE void recordMatch(bool againstComputer, int difficulty, bool won, int longestRally);
    Q_INVOKABLE void recordLadder(int levelsBeaten);
    // Returns whether score is a new high score
    Q_INVOKABLE bool recordEndless(int score);
    Q_INVOKABLE void recordTournamentWin();
    // Returns whether rally is a new best
    Q_INVOKABLE bool recordSquash(int rally);
    Q_INVOKABLE void reset();

    int wins(int difficulty) const;
    int losses(int difficulty) const;
    QVariantList records() const;
    int twoPlayerMatches() const;
    int longestRally() const;
    int ladderBest() const;
    int endlessBest() const;
    int tournamentsWon() const;
    int squashBest() const;

signals:
    void changed();

private:
    int value(const QString& key) const;
    void setValue(const QString& key, int value);

    QSettings m_settings;
};

#endif // STATS_H
