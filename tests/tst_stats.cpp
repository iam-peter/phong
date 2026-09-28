#include "stats.h"

#include <QCoreApplication>
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
