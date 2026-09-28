#include "modifiers.h"

#include <QSignalSpy>
#include <QTest>

class tst_Modifiers : public QObject
{
    Q_OBJECT

private:
    // Serve and let the player on side return the ball
    static void touch(Match& match, Match::Side side)
    {
        if (match.state() != Match::State::Playing) {
            match.start();
            match.advance(match.serveDelay());
        }

        // Make sure the ball flies towards side before it is returned
        const bool towardsLeft = match.ballVelocity().x() < 0.0f;
        if (towardsLeft != (side == Match::Side::LeftSide))
            match.paddleHit(Match::opponent(side), 0.0);
        match.paddleHit(side, 0.0);
    }

    static const Modifiers::Definition& definition(const Modifiers& modifiers, const char* id)
    {
        return modifiers.definitions().at(modifiers.findDefinition(QString::fromLatin1(id)));
    }

    static int spawn(Modifiers& modifiers, const char* id)
    {
        return modifiers.spawn(modifiers.findDefinition(QString::fromLatin1(id)), QVector2D(0, 0));
    }

private slots:
    void shippedConfiguration()
    {
        Modifiers modifiers;
        QVERIFY(modifiers.load(QStringLiteral(PHONG_SOURCE_DIR "/config/modifiers.json")));

        const QStringList ids = { "fastBall", "bigPaddle", "shield",
                                  "smallPaddle", "spin", "narrowField", "multiBall",
                                  "magnet", "portals", "freeze", "reverse", "ghostBall",
                                  "gravityWell" };
        QCOMPARE(modifiers.definitions().size(), ids.size());
        for (const QString& id : ids)
            QVERIFY2(modifiers.findDefinition(id) >= 0, qPrintable(id));

        QCOMPARE(definition(modifiers, "smallPaddle").target, Modifiers::Target::Opponent);
        QCOMPARE(definition(modifiers, "spin").effect, Modifiers::Effect::Spin);
        QCOMPARE(modifiers.maxFieldInset(), 3.0);

        // Also compiled in as the default
        Modifiers embedded;
        QCOMPARE(embedded.definitions().size(), ids.size());
    }

    void loadCustomConfiguration()
    {
        Modifiers modifiers;
        QTest::ignoreMessage(QtWarningMsg,
                             "Skipping modifier \"broken\" with unknown effect \"teleport\"");
        QTest::ignoreMessage(QtWarningMsg,
                             "Skipping modifier \"lost\" with unknown target \"nobody\"");
        QVERIFY(modifiers.loadJson(R"({
            "spawn": { "minDelay": 1, "maxDelay": 2, "maxItems": 4, "lifetime": 3 },
            "modifiers": [
                { "id": "slow", "name": "Slow ball", "glyph": "<<", "color": "#123456",
                  "effect": "ballSpeed", "value": 0.7, "weight": 2 },
                { "id": "huge", "effect": "paddleSize", "target": "both", "value": 9 },
                { "id": "off", "effect": "shield", "enabled": false },
                { "id": "broken", "effect": "teleport" },
                { "id": "lost", "effect": "shield", "target": "nobody" }
            ]
        })"));

        QCOMPARE(modifiers.definitions().size(), 2);
        QCOMPARE(modifiers.spawnSettings().maxItems, 4);
        QCOMPARE(modifiers.spawnSettings().lifetime, 3.0);

        const Modifiers::Definition& slow = definition(modifiers, "slow");
        QCOMPARE(slow.name, QStringLiteral("Slow ball"));
        QCOMPARE(slow.color, QColor(0x12, 0x34, 0x56));
        QCOMPARE(slow.value, 0.7);
        QCOMPARE(slow.weight, 2.0);

        // Values are kept in a playable range, names default to the id
        const Modifiers::Definition& huge = definition(modifiers, "huge");
        QCOMPARE(huge.value, 2.0);
        QCOMPARE(huge.name, QStringLiteral("huge"));
        QCOMPARE(huge.target, Modifiers::Target::Both);
    }

    void rejectBrokenJson()
    {
        Modifiers modifiers;
        const qsizetype count = modifiers.definitions().size();

        QString error;
        QVERIFY(!modifiers.loadJson("{ \"modifiers\": [", &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!modifiers.loadJson("[]", &error));
        QCOMPARE(modifiers.definitions().size(), count);
    }

    void spawnsWhilePlaying()
    {
        Match match;
        Modifiers modifiers;
        modifiers.setMatch(&match);
        modifiers.setSeed(1);
        const Modifiers::SpawnSettings& spawn = modifiers.spawnSettings();

        // Nothing while the serve counts down
        match.start();
        modifiers.advance(spawn.maxDelay + 1.0);
        QCOMPARE(modifiers.rowCount(), 0);

        match.advance(match.serveDelay());
        for (int i = 0; i < 100; ++i)
            modifiers.advance(0.1);
        QVERIFY(modifiers.rowCount() > 0);
        QVERIFY(modifiers.rowCount() <= spawn.maxItems);

        // Items keep their distance and stay in the spawn area
        const QRectF area = modifiers.spawnArea();
        QList<QVector2D> positions;
        for (int row = 0; row < modifiers.rowCount(); ++row) {
            const QModelIndex index = modifiers.index(row);
            const QVector2D position(index.data(Modifiers::ItemXRole).toFloat(),
                                     index.data(Modifiers::ItemYRole).toFloat());
            QVERIFY(area.contains(position.toPointF()));
            for (const QVector2D& other : positions)
                QVERIFY(other.distanceToPoint(position) >= spawn.minDistance);
            positions.append(position);
        }
    }

    void weightsDecideWhatSpawns()
    {
        Match match;
        Modifiers modifiers;
        modifiers.setMatch(&match);
        modifiers.setSeed(7);
        QVERIFY(modifiers.loadJson(R"({
            "spawn": { "minDelay": 0.5, "maxDelay": 0.5, "maxItems": 8, "lifetime": 100,
                       "minDistance": 0 },
            "modifiers": [
                { "id": "never", "effect": "shield", "weight": 0 },
                { "id": "always", "effect": "ballSpeed" }
            ]
        })"));

        match.start();
        match.advance(match.serveDelay());
        for (int i = 0; i < 40; ++i)
            modifiers.advance(0.1);

        QVERIFY(modifiers.rowCount() > 1);
        const int always = modifiers.findDefinition("always");
        for (int row = 0; row < modifiers.rowCount(); ++row)
            QCOMPARE(modifiers.index(row).data(Modifiers::DefinitionRole).toInt(), always);
    }

    void disabledSpawnsNothing()
    {
        Match match;
        Modifiers modifiers;
        modifiers.setMatch(&match);
        modifiers.setEnabled(false);

        match.start();
        match.advance(match.serveDelay());
        for (int i = 0; i < 300; ++i)
            modifiers.advance(0.1);
        QCOMPARE(modifiers.rowCount(), 0);
    }

    void itemsExpire()
    {
        Match match;
        Modifiers modifiers;
        modifiers.setMatch(&match);
        modifiers.setEnabled(false);

        match.start();
        match.advance(match.serveDelay());
        spawn(modifiers, "shield");

        modifiers.advance(modifiers.spawnSettings().lifetime - 1.0);
        QCOMPARE(modifiers.rowCount(), 1);
        modifiers.advance(1.0);
        QCOMPARE(modifiers.rowCount(), 0);
    }

    void untouchedBallCollectsNothing()
    {
        Match match;
        Modifiers modifiers;
        modifiers.setMatch(&match);

        match.start();
        match.advance(match.serveDelay());
        const int id = spawn(modifiers, "bigPaddle");

        QVERIFY(!modifiers.collect(id));
        QCOMPARE(modifiers.rowCount(), 1);
    }

    void boostsGoToLastTouch()
    {
        Match match;
        Modifiers modifiers;
        modifiers.setMatch(&match);
        QSignalSpy collected(&modifiers, &Modifiers::collected);
        const Modifiers::Definition& big = definition(modifiers, "bigPaddle");

        touch(match, Match::Side::RightSide);
        const int id = spawn(modifiers, "bigPaddle");
        QVERIFY(modifiers.collect(id));
        QVERIFY(!modifiers.collect(id));

        QCOMPARE(modifiers.rowCount(), 0);
        QCOMPARE(match.right()->paddleScale(), big.value);
        QCOMPARE(match.left()->paddleScale(), 1.0);
        QCOMPARE(collected.count(), 1);
        QCOMPARE(collected.at(0).at(1).value<Match::Side>(), Match::Side::RightSide);

        // Shows as active effect until it runs out
        QCOMPARE(modifiers.activeEffects(Match::Side::RightSide).size(), 1);
        QCOMPARE(modifiers.activeEffects(Match::Side::RightSide).at(0).toMap().value("id"),
                 QVariant(QStringLiteral("bigPaddle")));
        QVERIFY(modifiers.activeEffects(Match::Side::LeftSide).isEmpty());

        modifiers.advance(big.duration - 0.5);
        QCOMPARE(match.right()->paddleScale(), big.value);
        modifiers.advance(1.0);
        QCOMPARE(match.right()->paddleScale(), 1.0);
        QVERIFY(modifiers.activeEffects(Match::Side::RightSide).isEmpty());
    }

    void cursesGoToOpponent()
    {
        Match match;
        Modifiers modifiers;
        modifiers.setMatch(&match);
        QSignalSpy collected(&modifiers, &Modifiers::collected);

        touch(match, Match::Side::LeftSide);
        modifiers.collect(spawn(modifiers, "smallPaddle"));
        modifiers.collect(spawn(modifiers, "spin"));

        QCOMPARE(match.left()->paddleScale(), 1.0);
        QCOMPARE(match.left()->spinSpeed(), 0.0);
        QCOMPARE(match.right()->paddleScale(), definition(modifiers, "smallPaddle").value);
        QCOMPARE(match.right()->spinSpeed(), definition(modifiers, "spin").value);
        QCOMPARE(collected.at(1).at(1).value<Match::Side>(), Match::Side::RightSide);

        modifiers.advance(definition(modifiers, "spin").duration + 0.1);
        QCOMPARE(match.right()->spinSpeed(), 0.0);
    }

    void bothPlayersAffected()
    {
        Match match;
        Modifiers modifiers;
        modifiers.setMatch(&match);
        QSignalSpy collected(&modifiers, &Modifiers::collected);
        QVERIFY(modifiers.loadJson(R"({ "modifiers": [
            { "id": "tiny", "effect": "paddleSize", "target": "both", "value": 0.5 }
        ] })"));

        touch(match, Match::Side::LeftSide);
        modifiers.collect(spawn(modifiers, "tiny"));
        QCOMPARE(match.left()->paddleScale(), 0.5);
        QCOMPARE(match.right()->paddleScale(), 0.5);
        QCOMPARE(collected.at(0).at(1).value<Match::Side>(), Match::Side::NoSide);
    }

    void fastBallAndNarrowField()
    {
        Match match;
        Modifiers modifiers;
        modifiers.setMatch(&match);
        const Modifiers::Definition& fast = definition(modifiers, "fastBall");
        const Modifiers::Definition& narrow = definition(modifiers, "narrowField");

        touch(match, Match::Side::LeftSide);
        const float speed = match.ballVelocity().length();
        modifiers.collect(spawn(modifiers, "fastBall"));
        QCOMPARE(match.ballVelocity().length(), speed * float(fast.value));
        QVERIFY(match.ballVelocity().x() > 0.0f);

        modifiers.collect(spawn(modifiers, "narrowField"));
        QCOMPARE(modifiers.fieldInset(), narrow.value);
        modifiers.advance(narrow.duration + 0.1);
        QCOMPARE(modifiers.fieldInset(), 0.0);
    }

    void shieldStopsOneGoal()
    {
        Match match;
        Modifiers modifiers;
        modifiers.setMatch(&match);

        touch(match, Match::Side::LeftSide);
        modifiers.collect(spawn(modifiers, "shield"));
        QVERIFY(match.left()->isShielded());

        // Ball on its way to the right, the left shield has nothing to do
        QVERIFY(!modifiers.shieldHit(Match::Side::LeftSide));

        // The right player misses, but has no shield
        QVERIFY(!modifiers.shieldHit(Match::Side::RightSide));

        // Back towards the left goal
        match.paddleHit(Match::Side::RightSide, 0.0);
        QVERIFY(modifiers.shieldHit(Match::Side::LeftSide));
        QVERIFY(match.ballVelocity().x() > 0.0f);
        QVERIFY(!match.left()->isShielded());

        // The right player returned the ball last. The shield stays until
        // hit, also across points.
        modifiers.collect(spawn(modifiers, "shield"));
        QVERIFY(match.right()->isShielded());
        match.goal(Match::Side::LeftSide);
        modifiers.advance(60.0);
        QVERIFY(match.right()->isShielded());
    }

    void magnetCatches()
    {
        Match match;
        Modifiers modifiers;
        modifiers.setMatch(&match);
        const Modifiers::Definition& magnet = definition(modifiers, "magnet");
        QSignalSpy effects(&modifiers, &Modifiers::effectsChanged);

        touch(match, Match::Side::LeftSide);
        QVERIFY(modifiers.collect(spawn(modifiers, "magnet")));
        QCOMPARE(match.left()->catches(), int(magnet.value));
        QCOMPARE(modifiers.activeEffects(Match::Side::LeftSide).size(), 1);

        // Every catch uses one up, the magnet is gone with the last
        for (int i = 0; i < int(magnet.value); ++i) {
            match.paddleHit(Match::Side::RightSide, 0.5);
            QVERIFY(match.catchBall(match.ball(), Match::Side::LeftSide, 0.0));
            QVERIFY(match.releaseBall(match.ball()));
        }
        effects.clear();
        modifiers.advance(0.1);
        QCOMPARE(effects.count(), 1);
        QVERIFY(modifiers.activeEffects(Match::Side::LeftSide).isEmpty());

        // Or after its time
        QVERIFY(modifiers.collect(spawn(modifiers, "magnet")));
        modifiers.advance(magnet.duration + 0.1);
        QCOMPARE(match.left()->catches(), 0);
        QVERIFY(modifiers.activeEffects(Match::Side::LeftSide).isEmpty());
    }

    void cursesOnControls()
    {
        Match match;
        Modifiers modifiers;
        modifiers.setMatch(&match);
        const Modifiers::Definition& freeze = definition(modifiers, "freeze");
        const Modifiers::Definition& reverse = definition(modifiers, "reverse");

        touch(match, Match::Side::LeftSide);
        QVERIFY(modifiers.collect(spawn(modifiers, "freeze")));
        QVERIFY(modifiers.collect(spawn(modifiers, "reverse")));
        QVERIFY(match.right()->isFrozen());
        QVERIFY(match.right()->isReversed());
        QVERIFY(!match.left()->isFrozen());
        QCOMPARE(modifiers.activeEffects(Match::Side::RightSide).size(), 2);

        // The freeze is short, the reversal lasts longer
        modifiers.advance(freeze.duration + 0.1);
        QVERIFY(!match.right()->isFrozen());
        QVERIFY(match.right()->isReversed());
        modifiers.advance(reverse.duration);
        QVERIFY(!match.right()->isReversed());
        QVERIFY(modifiers.activeEffects(Match::Side::RightSide).isEmpty());
    }

    void fieldEffects()
    {
        Match match;
        Modifiers modifiers;
        modifiers.setMatch(&match);
        modifiers.setSpawnArea(QRectF(-8, -5, 16, 10));
        QSignalSpy portalsChanged(&modifiers, &Modifiers::portalsChanged);

        touch(match, Match::Side::RightSide);
        QVERIFY(modifiers.portals().isEmpty());
        QVERIFY(modifiers.collect(spawn(modifiers, "portals")));

        // One on each half, inside the spawn area
        const QVariantList portals = modifiers.portals();
        QCOMPARE(portals.size(), 2);
        const QVector2D a = portals.at(0).value<QVector2D>();
        const QVector2D b = portals.at(1).value<QVector2D>();
        QVERIFY(a.x() * b.x() < 0.0f);
        for (const QVector2D& portal : { a, b }) {
            QVERIFY(std::abs(portal.x()) >= 3.0f && std::abs(portal.x()) <= 8.0f);
            QVERIFY(std::abs(portal.y()) <= 5.0f);
        }

        QVERIFY(modifiers.collect(spawn(modifiers, "ghostBall")));
        QVERIFY(modifiers.isGhostBall());
        QVERIFY(modifiers.collect(spawn(modifiers, "gravityWell")));
        QCOMPARE(modifiers.gravityStrength(), definition(modifiers, "gravityWell").value);
        QVERIFY(modifiers.spawnArea().contains(modifiers.gravityWell().toPointF()));
        // Field effects don't show next to the names
        QVERIFY(modifiers.activeEffects(Match::Side::LeftSide).isEmpty());
        QVERIFY(modifiers.activeEffects(Match::Side::RightSide).isEmpty());

        // Items keep away from the well and the portals
        for (int i = 0; i < 200; ++i)
            modifiers.advance(0.5);
        QVERIFY(!modifiers.isGhostBall());
        QVERIFY(modifiers.portals().isEmpty());
        QCOMPARE(modifiers.gravityStrength(), 0.0);
        QCOMPARE(portalsChanged.count(), 2);

        modifiers.collect(spawn(modifiers, "portals"));
        modifiers.collect(spawn(modifiers, "gravityWell"));
        modifiers.reset();
        QVERIFY(modifiers.portals().isEmpty());
        QCOMPARE(modifiers.gravityStrength(), 0.0);
    }

    void spawnAtAPlace()
    {
        Modifiers modifiers;
        const int id = modifiers.spawnAt(QVector2D(1, 2));
        QVERIFY(id > 0);
        QCOMPARE(modifiers.rowCount(), 1);
        const QModelIndex index = modifiers.index(0);
        QCOMPARE(modifiers.data(index, Modifiers::ItemXRole).toReal(), 1.0);
        QCOMPARE(modifiers.data(index, Modifiers::ItemYRole).toReal(), 2.0);

        // Only what may spawn
        QVERIFY(modifiers.loadJson(R"({ "modifiers": [ { "id": "a", "effect": "shield", "weight": 0 } ] })"));
        QCOMPARE(modifiers.spawnAt(QVector2D(0, 0)), -1);
    }

    void multiBallFromTheItem()
    {
        Match match;
        Modifiers modifiers;
        modifiers.setMatch(&match);
        const Modifiers::Definition& multi = definition(modifiers, "multiBall");

        touch(match, Match::Side::LeftSide);
        const int id = modifiers.spawn(modifiers.findDefinition("multiBall"), QVector2D(2, 3));
        QVERIFY(modifiers.collect(id));

        // The new balls come from the item and fly at the opponent
        QCOMPARE(match.extraBalls()->rowCount(), int(multi.value));
        for (Ball* ball : match.extraBalls()->balls()) {
            QCOMPARE(ball->spawnPosition(), QVector2D(2, 3));
            QVERIFY(ball->velocity().x() > 0.0f);
            QCOMPARE(ball->lastTouch(), Match::Side::LeftSide);
            QCOMPARE(ball->lifetime(), multi.duration);
        }
    }

    void extraBallCollectsForItsPlayer()
    {
        Match match;
        Modifiers modifiers;
        modifiers.setMatch(&match);

        touch(match, Match::Side::LeftSide);
        Ball* extra = match.addBall(QVector2D(0, 0), Match::Side::LeftSide, 10.0,
                                    Match::Side::RightSide);

        // The main ball was touched by left, the extra one by right
        modifiers.collect(spawn(modifiers, "bigPaddle"), extra);
        QCOMPARE(match.right()->paddleScale(), definition(modifiers, "bigPaddle").value);
        QCOMPARE(match.left()->paddleScale(), 1.0);

        // A fast ball speeds up the ball that collected it
        const float main = match.ballVelocity().length();
        const float speed = extra->velocity().length();
        modifiers.collect(spawn(modifiers, "fastBall"), extra);
        QCOMPARE(extra->velocity().length(), speed * float(definition(modifiers, "fastBall").value));
        QCOMPARE(match.ballVelocity().length(), main);
    }

    void itemsAvoidObstacles()
    {
        Match match;
        Modifiers modifiers;
        modifiers.setMatch(&match);
        modifiers.setSeed(3);
        QVERIFY(modifiers.loadJson(R"({
            "spawn": { "minDelay": 0.5, "maxDelay": 0.5, "maxItems": 8, "lifetime": 100,
                       "minDistance": 0 },
            "modifiers": [ { "id": "any", "effect": "ballSpeed" } ]
        })"));

        // Most of the spawn area is blocked
        const QRectF obstacle(-8.0, -4.5, 12.0, 9.0);
        modifiers.setObstacles({ obstacle });

        match.start();
        match.advance(match.serveDelay());
        for (int i = 0; i < 60; ++i)
            modifiers.advance(0.1);

        QVERIFY(modifiers.rowCount() > 0);
        const QVariantList positions = modifiers.itemPositions();
        QCOMPARE(positions.size(), modifiers.rowCount());
        for (const QVariant& position : positions)
            QVERIFY(!obstacle.adjusted(-1.2, -1.2, 1.2, 1.2).contains(position.value<QVector2D>().toPointF()));
    }

    void resetClearsEverything()
    {
        Match match;
        Modifiers modifiers;
        modifiers.setMatch(&match);

        touch(match, Match::Side::LeftSide);
        for (const char* id : { "bigPaddle", "shield", "spin", "narrowField" })
            modifiers.collect(spawn(modifiers, id));
        spawn(modifiers, "fastBall");

        modifiers.reset();
        QCOMPARE(modifiers.rowCount(), 0);
        QCOMPARE(match.left()->paddleScale(), 1.0);
        QVERIFY(!match.left()->isShielded());
        QCOMPARE(match.right()->spinSpeed(), 0.0);
        QCOMPARE(modifiers.fieldInset(), 0.0);
        QVERIFY(modifiers.activeEffects(Match::Side::LeftSide).isEmpty());
    }
};

QTEST_GUILESS_MAIN(tst_Modifiers)
#include "tst_modifiers.moc"
