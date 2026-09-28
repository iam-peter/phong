#include "tournament.h"

#include <QSignalSpy>
#include <QTest>

class tst_Tournament : public QObject
{
    Q_OBJECT

private:
    static QVariantMap entry(const Tournament& tournament, int round, int slot)
    {
        return tournament.rounds().at(round).toList().at(slot).toMap();
    }

private slots:
    void draw()
    {
        Tournament tournament;
        QVERIFY(!tournament.isRunning());
        QVERIFY(tournament.rounds().isEmpty());

        tournament.setSeed(1);
        tournament.start();
        QVERIFY(tournament.isRunning());
        QCOMPARE(tournament.round(), 0);

        // Eight different entrants, the player at the top
        const QVariantList first = tournament.rounds().at(0).toList();
        QCOMPARE(first.size(), Tournament::size);
        QVERIFY(first.at(0).toMap().value("player").toBool());
        QStringList names;
        for (const QVariant& entrant : first)
            names.append(entrant.toMap().value("name").toString());
        names.removeDuplicates();
        QCOMPARE(names.size(), Tournament::size);

        // Four, two and one open places after that
        QCOMPARE(tournament.rounds().size(), Tournament::roundCount + 1);
        QCOMPARE(tournament.rounds().at(1).toList().size(), 4);
        QVERIFY(entry(tournament, 3, 0).isEmpty());

        // The first opponent is the player's neighbour
        QCOMPARE(tournament.opponent().value("name"), first.at(1).toMap().value("name"));
    }

    void winningThrough()
    {
        Tournament tournament;
        QSignalSpy changed(&tournament, &Tournament::changed);
        tournament.start();

        for (int round = 0; round < Tournament::roundCount; ++round) {
            QVERIFY(tournament.isRunning());
            QCOMPARE(tournament.round(), round);
            const QString opponent = tournament.opponent().value("name").toString();
            tournament.recordResult(true);

            // The player goes on, the opponent is out, the other matches
            // of the round are decided
            QCOMPARE(entry(tournament, round + 1, 0).value("player").toBool(), true);
            const QVariantList entrants = tournament.rounds().at(round).toList();
            for (int slot = 0; slot < entrants.size(); ++slot) {
                const QVariantMap entrant = entrants.at(slot).toMap();
                QVERIFY(entrant.value("result").toInt() != Tournament::Result::Open);
                if (entrant.value("name").toString() == opponent)
                    QCOMPARE(entrant.value("result").toInt(), int(Tournament::Result::Lost));
            }
        }

        QVERIFY(tournament.isChampion());
        QVERIFY(!tournament.isRunning());
        QVERIFY(tournament.opponent().isEmpty());
        QVERIFY(entry(tournament, 3, 0).value("player").toBool());
        QCOMPARE(changed.count(), 1 + Tournament::roundCount);

        // Nothing more to record
        tournament.recordResult(false);
        QVERIFY(tournament.isChampion());
    }

    void knockedOut()
    {
        Tournament tournament;
        tournament.start();
        tournament.recordResult(true);
        tournament.recordResult(false);

        QVERIFY(!tournament.isRunning());
        QVERIFY(!tournament.isChampion());
        QCOMPARE(entry(tournament, 1, 0).value("result").toInt(), int(Tournament::Result::Lost));

        // The rest is played without the player, somebody else wins
        const QVariantMap champion = entry(tournament, 3, 0);
        QVERIFY(!champion.isEmpty());
        QVERIFY(!champion.value("player").toBool());
        QCOMPARE(entry(tournament, 2, 0).value("result").toInt() == int(Tournament::Result::Won)
                     || entry(tournament, 2, 1).value("result").toInt() == int(Tournament::Result::Won),
                 true);
    }

    void strongerComputersWinMore()
    {
        // Rookie is easy, Ace hard, over many draws Ace goes further
        int rookie = 0;
        int ace = 0;
        for (int i = 0; i < 300; ++i) {
            Tournament tournament;
            tournament.setSeed(i);
            tournament.start();
            tournament.recordResult(false);
            const QString name = entry(tournament, 3, 0).value("name").toString();
            rookie += name == QLatin1String("Rookie");
            ace += name == QLatin1String("Ace");
        }
        QVERIFY2(ace > 3 * rookie, qPrintable(QStringLiteral("%1 %2").arg(ace).arg(rookie)));
    }
};

QTEST_GUILESS_MAIN(tst_Tournament)
#include "tst_tournament.moc"
