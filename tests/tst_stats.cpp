#include "stats.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

class tst_Stats : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("phong-tests"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_stats"));
    }

    void init()
    {
        Stats().reset();
    }

    void cleanupTestCase()
    {
        Stats().reset();
    }

    void recordsMatches()
    {
        Stats stats;
        stats.recordMatch(true, 1, true, 7);
        stats.recordMatch(true, 1, false, 12);
        stats.recordMatch(true, 2, false, 3);
        stats.recordMatch(false, 0, true, 5);

        QCOMPARE(stats.wins(1), 1);
        QCOMPARE(stats.losses(1), 1);
        QCOMPARE(stats.losses(2), 1);
        QCOMPARE(stats.wins(0), 0);
        QCOMPARE(stats.twoPlayerMatches(), 1);
        QCOMPARE(stats.longestRally(), 12);
        QCOMPARE(stats.records().at(1).toMap().value("wins").toInt(), 1);

        // Persisted
        Stats again;
        QCOMPARE(again.wins(1), 1);
        QCOMPARE(again.longestRally(), 12);
    }

    void endlessHighScore()
    {
        Stats stats;
        QVERIFY(stats.recordEndless(40));
        QVERIFY(!stats.recordEndless(30));
        QVERIFY(!stats.recordEndless(40));
        QVERIFY(stats.recordEndless(41));
        QCOMPARE(stats.endlessBest(), 41);
    }

    void tournamentWins()
    {
        Stats stats;
        QCOMPARE(stats.tournamentsWon(), 0);
        stats.recordTournamentWin();
        stats.recordTournamentWin();
        QCOMPARE(Stats().tournamentsWon(), 2);
        stats.reset();
        QCOMPARE(stats.tournamentsWon(), 0);
    }

    void squashBest()
    {
        Stats stats;
        QVERIFY(stats.recordSquash(12));
        QVERIFY(!stats.recordSquash(12));
        QVERIFY(!stats.recordSquash(3));
        QVERIFY(stats.recordSquash(20));
        QCOMPARE(Stats().squashBest(), 20);
    }

    void partyGames()
    {
        Stats stats;
        stats.recordParty(false, 4);
        stats.recordParty(true, 30);
        QCOMPARE(Stats().partyGames(), 2);
        QCOMPARE(Stats().partyWins(), 1);
        QCOMPARE(stats.longestRally(), 30);
        stats.reset();
        QCOMPARE(stats.partyGames(), 0);
    }

    void achievements()
    {
        Stats stats;
        const QList<Stats::Achievement> list = Stats::achievementList();
        QCOMPARE(stats.achievements().size(), list.size());
        QCOMPARE(stats.unlockedCount(), 0);

        QSignalSpy unlocked(&stats, &Stats::achievementUnlocked);
        QVERIFY(stats.unlock(QStringLiteral("smashGoal")));
        QVERIFY(!stats.unlock(QStringLiteral("smashGoal")));
        QVERIFY(!stats.unlock(QStringLiteral("noSuchThing")));
        QCOMPARE(unlocked.count(), 1);
        QCOMPARE(unlocked.last().at(0).toString(), QStringLiteral("Smash hit"));

        // Kept, and reset with the rest
        Stats other;
        QVERIFY(other.isUnlocked(QStringLiteral("smashGoal")));
        QCOMPARE(other.unlockedCount(), 1);
        const QVariantMap entry = other.achievements().at(5).toMap();
        QCOMPARE(entry.value("id").toString(), QStringLiteral("smashGoal"));
        QVERIFY(entry.value("unlocked").toBool());

        other.reset();
        QCOMPARE(stats.unlockedCount(), 0);
    }

    void ladderKeepsTheBest()
    {
        Stats stats;
        stats.recordLadder(2);
        stats.recordLadder(1);
        QCOMPARE(stats.ladderBest(), 2);
        stats.recordLadder(5);
        QCOMPARE(stats.ladderBest(), Stats::difficulties);

        stats.reset();
        QCOMPARE(stats.ladderBest(), 0);
    }
};

QTEST_GUILESS_MAIN(tst_Stats)
#include "tst_stats.moc"
