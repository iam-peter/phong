#include "partymatch.h"

#include <QSignalSpy>
#include <QTest>
#include <QtMath>

class tst_PartyMatch : public QObject
{
    Q_OBJECT

private:
    static void serve(PartyMatch& match)
    {
        match.advance(match.serveDelay());
        QCOMPARE(match.state(), PartyMatch::State::Playing);
    }

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

private slots:
    void geometry()
    {
        PartyMatch match;
        match.setPlayers(4);

        // Player 0 at the bottom, then counterclockwise
        QVERIFY((match.normal(0) - QVector2D(0, -1)).length() < 1e-5f);
        QVERIFY((match.normal(1) - QVector2D(1, 0)).length() < 1e-5f);
        QVERIFY((match.tangent(0) - QVector2D(1, 0)).length() < 1e-5f);

        match.setPlayers(1);
        QCOMPARE(match.players(), PartyMatch::minPlayers);
        match.setPlayers(10);
        QCOMPARE(match.players(), PartyMatch::maxPlayers);
    }

    void servesAtAPlayer()
    {
        for (int players = PartyMatch::minPlayers; players <= PartyMatch::maxPlayers; ++players) {
            PartyMatch match;
            match.setPlayers(players);
            match.start();
            QCOMPARE(match.state(), PartyMatch::State::Serving);
            QCOMPARE(match.alive(), players);
            serve(match);

            // Towards a goal, never straight
            const QVector2D v = match.ball()->velocity();
            QVERIFY(qAbs(v.length() - float(match.serveSpeed())) < 1e-3f);
            const qreal angle = qRadiansToDegrees(qAcos(QVector2D::dotProduct(v.normalized(),
                                                                              match.normal(target(match)))));
            QVERIFY(angle <= PartyMatch::maxServeAngle + 0.01);
        }
    }

    void paddleSendsBallBack()
    {
        PartyMatch match;
        match.setPlayers(5);
        match.setSpeedUp(1.1);
        match.start();
        serve(match);

        const int player = target(match);
        const float speed = match.ball()->velocity().length();

        // Not from a side the ball moves away from
        for (int other = 0; other < 5; ++other) {
            if (QVector2D::dotProduct(match.ball()->velocity(), match.normal(other)) < 0.0f)
                match.paddleHit(other, 0.0);
        }
        QCOMPARE(match.rally(), 0);

        match.paddleHit(player, 0.0);
        const QVector2D v = match.ball()->velocity();
        QVERIFY((v.normalized() + match.normal(player)).length() < 1e-4f);
        QVERIFY(qAbs(v.length() - speed * 1.1f) < 1e-3f);
        QCOMPARE(match.rally(), 1);

        // Moving away now, a second report changes nothing
        match.paddleHit(player, 1.0);
        QCOMPARE(match.ball()->velocity(), v);
    }

    void bounceOffPosts()
    {
        PartyMatch match;
        match.start();
        serve(match);

        const QVector2D v = match.ball()->velocity();
        QVERIFY(!match.bounce(v.normalized()));
        QVERIFY(match.bounce(-v.normalized()));
        QVERIFY((match.ball()->velocity() + v).length() < 1e-4f);
    }

    void snapshotTravels()
    {
        PartyMatch host;
        host.setPlayers(5);
        host.setLives(2);
        host.start();
        serve(host);
        host.goal(3);
        host.paddleHit(0, 0.0);

        PartyMatch client;
        QSignalSpy lives(&client, &PartyMatch::livesLeftChanged);
        client.applySnapshot(host.snapshot());
        QCOMPARE(client.players(), 5);
        QCOMPARE(client.state(), host.state());
        QCOMPARE(client.livesLeft(), host.livesLeft());
        QCOMPARE(client.alive(), 5);
        QCOMPARE(client.serveCountdown(), host.serveCountdown());
        QCOMPARE(client.serveDirection(), host.serveDirection());
        QCOMPARE(client.rally(), host.rally());
        QCOMPARE(lives.count(), 1);

        serve(host);
        client.applySnapshot(host.snapshot());
        QCOMPARE(client.state(), PartyMatch::State::Playing);
        QCOMPARE(client.ball()->velocity(), host.ball()->velocity());

        // The same snapshot again changes nothing
        client.applySnapshot(host.snapshot());
        QCOMPARE(lives.count(), 1);
    }

    void lastOneLeftWins()
    {
        PartyMatch match;
        match.setPlayers(3);
        match.setLives(2);
        QSignalSpy out(&match, &PartyMatch::playerOut);
        QSignalSpy finished(&match, &PartyMatch::finished);
        match.start();

        // A goal costs a ball, the next kickoff goes to somebody else
        serve(match);
        match.goal(0);
        QCOMPARE(match.livesLeft().at(0).toInt(), 1);
        QCOMPARE(match.state(), PartyMatch::State::Serving);
        QCOMPARE(match.ball()->velocity(), QVector2D());
        serve(match);
        QVERIFY(target(match) != 0);

        match.goal(0);
        QCOMPARE(out.count(), 1);
        QVERIFY(!match.isAlive(0));
        QCOMPARE(match.alive(), 2);

        // Out is out
        serve(match);
        QVERIFY(target(match) != 0);
        match.goal(0);
        QCOMPARE(match.alive(), 2);
        match.paddleHit(0, 0.0);
        QCOMPARE(match.rally(), 0);

        match.goal(2);
        serve(match);
        match.goal(2);
        QCOMPARE(finished.count(), 1);
        QCOMPARE(match.state(), PartyMatch::State::Finished);
        QCOMPARE(match.winner(), 1);
    }
};

QTEST_GUILESS_MAIN(tst_PartyMatch)
#include "tst_partymatch.moc"
