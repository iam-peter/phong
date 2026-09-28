#include "partymodifiers.h"

#include <QSignalSpy>
#include <QTest>

class tst_PartyModifiers : public QObject
{
    Q_OBJECT

private:
    // The player the ball is flying at most directly
    static int target(const PartyMatch& match)
    {
        int best = 0;
        for (int player = 1; player < match.players(); ++player) {
            if (QVector2D::dotProduct(match.ball()->velocity(), match.normal(player))
                > QVector2D::dotProduct(match.ball()->velocity(), match.normal(best)))
                best = player;
        }
        return best;
    }

    // A game in play, the ball last hit by the returned player
    static int play(PartyMatch& match, PartyModifiers& modifiers)
    {
        match.setPlayers(4);
        modifiers.setMatch(&match);
        match.start();
        modifiers.reset();
        match.advance(match.serveDelay());
        const int player = target(match);
        match.paddleHit(player, 0.5, match.paddleSpeed());
        return player;
    }

    static int spawn(PartyModifiers& modifiers, const QString& id)
    {
        const int definition = modifiers.findDefinition(id);
        return definition < 0 ? -1 : modifiers.spawn(definition, QVector2D(2, 2));
    }

private slots:
    void onlyWhatFitsThePolygon()
    {
        PartyModifiers modifiers;
        QVERIFY(!modifiers.definitions().isEmpty());
        for (const Modifiers::Definition& definition : modifiers.definitions())
            QVERIFY(PartyModifiers::isSupported(definition.effect));
        QVERIFY(modifiers.findDefinition(QStringLiteral("shield")) >= 0);
        QCOMPARE(modifiers.findDefinition(QStringLiteral("multiBall")), -1);
        QCOMPARE(modifiers.findDefinition(QStringLiteral("portals")), -1);
    }

    void nobodyCollectsBeforeAHit()
    {
        PartyMatch match;
        PartyModifiers modifiers;
        modifiers.setMatch(&match);
        match.start();
        match.advance(match.serveDelay());
        const int item = spawn(modifiers, QStringLiteral("bigPaddle"));
        QVERIFY(!modifiers.collect(item));
        QCOMPARE(modifiers.rowCount(), 1);
    }

    void collectorGrows()
    {
        PartyMatch match;
        PartyModifiers modifiers;
        QSignalSpy collected(&modifiers, &PartyModifiers::collected);
        const int player = play(match, modifiers);

        QVERIFY(modifiers.collect(spawn(modifiers, QStringLiteral("bigPaddle"))));
        QCOMPARE(collected.count(), 1);
        QCOMPARE(collected.last().at(1).toInt(), player);
        QVERIFY(match.player(player)->paddleScale() > 1.0);
        QCOMPARE(modifiers.rowCount(), 0);

        // Until the time is up
        modifiers.advance(60.0);
        QCOMPARE(match.player(player)->paddleScale(), 1.0);
    }

    void cursesHitEverybodyElse()
    {
        PartyMatch match;
        PartyModifiers modifiers;
        const int player = play(match, modifiers);

        QVERIFY(modifiers.collect(spawn(modifiers, QStringLiteral("freeze"))));
        for (int other = 0; other < match.players(); ++other)
            QCOMPARE(match.player(other)->isFrozen(), other != player);

        modifiers.reset();
        for (int other = 0; other < match.players(); ++other)
            QVERIFY(!match.player(other)->isFrozen());
    }

    void shieldSavesTheCollector()
    {
        PartyMatch match;
        PartyModifiers modifiers;
        const int player = play(match, modifiers);

        QVERIFY(modifiers.collect(spawn(modifiers, QStringLiteral("shield"))));
        QVERIFY(match.player(player)->isShielded());
    }

    void fieldEffectsTravel()
    {
        PartyMatch match;
        PartyModifiers modifiers;
        play(match, modifiers);
        QVERIFY(modifiers.collect(spawn(modifiers, QStringLiteral("gravityWell"))));
        QVERIFY(modifiers.gravityStrength() > 0.0);
        spawn(modifiers, QStringLiteral("ghostBall"));

        PartyModifiers replica;
        replica.applySnapshot(modifiers.snapshot());
        QCOMPARE(replica.rowCount(), 1);
        QCOMPARE(replica.gravityWell(), modifiers.gravityWell());
        QCOMPARE(replica.gravityStrength(), modifiers.gravityStrength());

        // The effects on the players go with the match
        match.player(2)->setReversed(true);
        PartyMatch client;
        client.applySnapshot(match.snapshot());
        QVERIFY(client.player(2)->isReversed());
        QVERIFY(!client.player(1)->isReversed());
    }

    void itemsSpawnInsideTheRadius()
    {
        PartyMatch match;
        PartyModifiers modifiers;
        modifiers.setSeed(7);
        modifiers.setSpawnRadius(5.0);
        play(match, modifiers);
        for (int i = 0; i < 20; ++i)
            modifiers.advance(1.0);
        QVERIFY(modifiers.rowCount() > 0);
        for (int row = 0; row < modifiers.rowCount(); ++row) {
            const QModelIndex index = modifiers.index(row);
            const QVector2D position(modifiers.data(index, PartyModifiers::ItemXRole).toFloat(),
                                     modifiers.data(index, PartyModifiers::ItemYRole).toFloat());
            QVERIFY(position.length() <= 5.0f);
        }
    }
};

QTEST_GUILESS_MAIN(tst_PartyModifiers)
#include "tst_partymodifiers.moc"
