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

private slots:
    void spawnsWhilePlaying()
    {
        Match match;
        Modifiers modifiers;
        modifiers.setMatch(&match);
        modifiers.setSeed(1);

        // Nothing while the serve counts down
        match.start();
        modifiers.advance(Modifiers::maxSpawnDelay + 1.0);
        QCOMPARE(modifiers.rowCount(), 0);

        match.advance(match.serveDelay());
        for (int i = 0; i < 100; ++i)
            modifiers.advance(0.1);
        QVERIFY(modifiers.rowCount() > 0);
        QVERIFY(modifiers.rowCount() <= Modifiers::maxItems);

        // Items keep their distance and stay in the spawn area
        const QRectF area = modifiers.spawnArea();
        QList<QVector2D> positions;
        for (int row = 0; row < modifiers.rowCount(); ++row) {
            const QModelIndex index = modifiers.index(row);
            const QVector2D position(index.data(Modifiers::ItemXRole).toFloat(),
                                     index.data(Modifiers::ItemYRole).toFloat());
            QVERIFY(area.contains(position.toPointF()));
            for (const QVector2D& other : positions)
                QVERIFY(other.distanceToPoint(position) >= Modifiers::minItemDistance);
            positions.append(position);
        }
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
        modifiers.spawn(Modifiers::Kind::Shield, QVector2D(0, 0));

        modifiers.advance(Modifiers::itemLifetime - 1.0);
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
        const int id = modifiers.spawn(Modifiers::Kind::BigPaddle, QVector2D(0, 0));

        QVERIFY(!modifiers.collect(id));
        QCOMPARE(modifiers.rowCount(), 1);
    }

    void boostsGoToLastTouch()
    {
        Match match;
        Modifiers modifiers;
        modifiers.setMatch(&match);
        QSignalSpy collected(&modifiers, &Modifiers::collected);

        touch(match, Match::Side::RightSide);
        const int id = modifiers.spawn(Modifiers::Kind::BigPaddle, QVector2D(1, 2));
        QVERIFY(modifiers.collect(id));
        QVERIFY(!modifiers.collect(id));

        QCOMPARE(modifiers.rowCount(), 0);
        QCOMPARE(match.right()->paddleScale(), Modifiers::bigPaddleScale);
        QCOMPARE(match.left()->paddleScale(), 1.0);
        QCOMPARE(collected.count(), 1);
        QCOMPARE(collected.at(0).at(1).value<Match::Side>(), Match::Side::RightSide);

        // Runs out after a while
        modifiers.advance(Modifiers::paddleDuration - 0.5);
        QCOMPARE(match.right()->paddleScale(), Modifiers::bigPaddleScale);
        modifiers.advance(1.0);
        QCOMPARE(match.right()->paddleScale(), 1.0);
    }

    void cursesGoToOpponent()
    {
        Match match;
        Modifiers modifiers;
        modifiers.setMatch(&match);
        QSignalSpy collected(&modifiers, &Modifiers::collected);

        touch(match, Match::Side::LeftSide);
        modifiers.collect(modifiers.spawn(Modifiers::Kind::SmallPaddle, QVector2D(0, 0)));
        modifiers.collect(modifiers.spawn(Modifiers::Kind::SpinPaddle, QVector2D(0, 0)));

        QCOMPARE(match.left()->paddleScale(), 1.0);
        QVERIFY(!match.left()->isSpinning());
        QCOMPARE(match.right()->paddleScale(), Modifiers::smallPaddleScale);
        QVERIFY(match.right()->isSpinning());
        QCOMPARE(collected.at(1).at(1).value<Match::Side>(), Match::Side::RightSide);

        modifiers.advance(Modifiers::spinDuration + 0.1);
        QVERIFY(!match.right()->isSpinning());
    }

    void fastBallAndNarrowField()
    {
        Match match;
        Modifiers modifiers;
        modifiers.setMatch(&match);

        touch(match, Match::Side::LeftSide);
        const float speed = match.ballVelocity().length();
        modifiers.collect(modifiers.spawn(Modifiers::Kind::FastBall, QVector2D(0, 0)));
        QCOMPARE(match.ballVelocity().length(), speed * float(Modifiers::fastBallFactor));
        QVERIFY(match.ballVelocity().x() > 0.0f);

        modifiers.collect(modifiers.spawn(Modifiers::Kind::NarrowField, QVector2D(0, 0)));
        QVERIFY(modifiers.isFieldNarrowed());
        modifiers.advance(Modifiers::narrowDuration + 0.1);
        QVERIFY(!modifiers.isFieldNarrowed());
    }

    void shieldStopsOneGoal()
    {
        Match match;
        Modifiers modifiers;
        modifiers.setMatch(&match);

        touch(match, Match::Side::LeftSide);
        modifiers.collect(modifiers.spawn(Modifiers::Kind::Shield, QVector2D(0, 0)));
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
        modifiers.collect(modifiers.spawn(Modifiers::Kind::Shield, QVector2D(0, 0)));
        QVERIFY(match.right()->isShielded());
        match.goal(Match::Side::LeftSide);
        modifiers.advance(60.0);
        QVERIFY(match.right()->isShielded());
    }

    void resetClearsEverything()
    {
        Match match;
        Modifiers modifiers;
        modifiers.setMatch(&match);

        touch(match, Match::Side::LeftSide);
        for (Modifiers::Kind kind : { Modifiers::Kind::BigPaddle, Modifiers::Kind::Shield,
                                      Modifiers::Kind::SpinPaddle, Modifiers::Kind::NarrowField })
            modifiers.collect(modifiers.spawn(kind, QVector2D(0, 0)));
        modifiers.spawn(Modifiers::Kind::FastBall, QVector2D(0, 0));

        modifiers.reset();
        QCOMPARE(modifiers.rowCount(), 0);
        QCOMPARE(match.left()->paddleScale(), 1.0);
        QVERIFY(!match.left()->isShielded());
        QVERIFY(!match.right()->isSpinning());
        QVERIFY(!modifiers.isFieldNarrowed());
    }
};

QTEST_GUILESS_MAIN(tst_Modifiers)
#include "tst_modifiers.moc"
