#include "match.h"

#include <QSignalSpy>
#include <QTest>

class tst_Match : public QObject
{
    Q_OBJECT

private:
    // Start a match and run the serve countdown down
    static void serve(Match& match)
    {
        if (match.state() != Match::State::Serving)
            match.start();
        match.advance(match.serveDelay());
    }

private slots:
    void startsIdle()
    {
        Match match;
        QCOMPARE(match.state(), Match::State::Idle);
        QCOMPARE(match.ballVelocity(), QVector2D());
    }

    void serveAfterDelay()
    {
        Match match;
        QSignalSpy served(&match, &Match::served);

        match.start();
        QCOMPARE(match.state(), Match::State::Serving);

        match.advance(0.5 * match.serveDelay());
        QCOMPARE(match.state(), Match::State::Serving);
        QCOMPARE(served.count(), 0);

        match.advance(match.serveDelay());
        QCOMPARE(match.state(), Match::State::Playing);
        QCOMPARE(served.count(), 1);
        QCOMPARE(match.ballVelocity().length(), float(match.serveSpeed()));

        // Never straight across and never too steep
        const QVector2D v = match.ballVelocity();
        const qreal angle = qRadiansToDegrees(qAtan2(std::abs(v.y()), std::abs(v.x())));
        QVERIFY(angle >= Match::minServeAngle - 0.01);
        QVERIFY(angle <= Match::maxServeAngle + 0.01);
    }

    void paddleSendsBallBack()
    {
        Match match;
        match.setSpeedUp(1.1);

        for (int i = 0; i < 20; ++i) {
            match.start();
            serve(match);

            const QVector2D before = match.ballVelocity();
            const Match::Side side = before.x() < 0.0f ? Match::Side::LeftSide
                                                       : Match::Side::RightSide;
            match.paddleHit(side, 0.0);

            const QVector2D after = match.ballVelocity();
            QVERIFY(after.x() * before.x() < 0.0f);
            QCOMPARE(after.y(), 0.0f);
            QCOMPARE(after.length(), before.length() * 1.1f);
            QCOMPARE(match.rally(), 1);
        }
    }

    void paddleIgnoresBallMovingAway()
    {
        Match match;
        serve(match);

        const QVector2D velocity = match.ballVelocity();
        const Match::Side behind = velocity.x() < 0.0f ? Match::Side::RightSide
                                                       : Match::Side::LeftSide;
        match.paddleHit(behind, 0.5);

        QCOMPARE(match.ballVelocity(), velocity);
        QCOMPARE(match.rally(), 0);
    }

    void paddleOffsetSetsAngle()
    {
        Match match;
        match.setSpeedUp(1.0);
        serve(match);

        const Match::Side side = match.ballVelocity().x() < 0.0f ? Match::Side::LeftSide
                                                                 : Match::Side::RightSide;
        // Hitting the top edge sends the ball up at the steepest angle,
        // offsets beyond the edge are clamped
        match.paddleHit(side, 3.0);

        const QVector2D v = match.ballVelocity();
        const qreal angle = qRadiansToDegrees(qAtan2(v.y(), std::abs(v.x())));
        QVERIFY(qAbs(angle - Match::maxBounceAngle) < 0.01);
    }

    void speedIsCapped()
    {
        Match match;
        match.setSpeedUp(2.0);
        match.setMaxSpeed(20.0);
        serve(match);

        for (int i = 0; i < 5; ++i) {
            const Match::Side side = match.ballVelocity().x() < 0.0f ? Match::Side::LeftSide
                                                                     : Match::Side::RightSide;
            match.paddleHit(side, 0.3);
        }

        QVERIFY(qAbs(match.ballVelocity().length() - 20.0f) < 0.001f);
        QCOMPARE(match.rally(), 5);
        QCOMPARE(match.longestRally(), 5);
        QCOMPARE(match.totalHits(), 5);
    }

    void wallsReflect()
    {
        Match match;
        serve(match);

        match.wallHit(true);
        QVERIFY(match.ballVelocity().y() < 0.0f);

        // Repeated reports of the same contact don't flip it back
        match.wallHit(true);
        QVERIFY(match.ballVelocity().y() < 0.0f);

        match.wallHit(false);
        QVERIFY(match.ballVelocity().y() > 0.0f);
    }

    void goalScoresAndServesToLoser()
    {
        Match match;
        QSignalSpy scored(&match, &Match::pointScored);
        serve(match);

        match.paddleHit(match.ballVelocity().x() < 0.0f ? Match::Side::LeftSide
                                                        : Match::Side::RightSide, 0.0);
        match.goal(Match::Side::LeftSide);

        QCOMPARE(match.left()->score(), 1);
        QCOMPARE(match.right()->score(), 0);
        QCOMPARE(scored.count(), 1);
        QCOMPARE(match.state(), Match::State::Serving);
        QCOMPARE(match.ballVelocity(), QVector2D());
        QCOMPARE(match.rally(), 0);
        QCOMPARE(match.longestRally(), 1);

        match.advance(match.serveDelay());
        QVERIFY(match.ballVelocity().x() > 0.0f); // towards the right player

        // A goal while nobody plays doesn't count
        match.pause();
        match.goal(Match::Side::LeftSide);
        QCOMPARE(match.left()->score(), 1);
    }

    void winnerFinishesMatch()
    {
        Match match;
        match.setPointsToWin(3);
        QSignalSpy finished(&match, &Match::finished);

        match.start();
        for (int i = 0; i < 3; ++i) {
            serve(match);
            match.goal(Match::Side::RightSide);
        }

        QCOMPARE(match.state(), Match::State::Finished);
        QCOMPARE(match.winner(), match.right());
        QCOMPARE(finished.count(), 1);

        // Starting again resets everything
        match.start();
        QCOMPARE(match.winner(), nullptr);
        QCOMPARE(match.left()->score(), 0);
        QCOMPARE(match.right()->score(), 0);
        QCOMPARE(match.totalHits(), 0);
    }

    void lastTouch()
    {
        Match match;
        serve(match);
        QCOMPARE(match.lastTouch(), Match::Side::NoSide);

        const Match::Side side = match.ballVelocity().x() < 0.0f ? Match::Side::LeftSide
                                                                 : Match::Side::RightSide;
        match.paddleHit(side, 0.0);
        QCOMPARE(match.lastTouch(), side);

        // A new serve belongs to nobody
        match.goal(side);
        match.advance(match.serveDelay());
        QCOMPARE(match.lastTouch(), Match::Side::NoSide);
    }

    void deflectReflects()
    {
        Match match;
        match.setSpeedUp(1.0);
        serve(match);

        // Towards the left paddle, tilted by 45 degrees
        if (match.ballVelocity().x() > 0.0f)
            match.paddleHit(Match::Side::RightSide, 0.0);
        const QVector2D before = match.ballVelocity();
        const int rally = match.rally();
        const QVector2D normal = QVector2D(1, 1).normalized();

        match.deflect(Match::Side::LeftSide, normal);
        const QVector2D after = match.ballVelocity();
        QCOMPARE(after.length(), before.length());
        QVERIFY(QVector2D::dotProduct(after, normal) > 0.0f);
        QCOMPARE(match.lastTouch(), Match::Side::LeftSide);
        QCOMPARE(match.rally(), rally + 1);

        // Moving away from the surface already, repeated reports change nothing
        match.deflect(Match::Side::LeftSide, normal);
        QCOMPARE(match.ballVelocity(), after);
    }

    void deflectKeepsBallCrossing()
    {
        Match match;
        match.setSpeedUp(1.0);
        serve(match);
        if (match.ballVelocity().x() > 0.0f)
            match.paddleHit(Match::Side::RightSide, 0.0);

        // A paddle lying flat would send the ball straight up
        const QVector2D v = match.ballVelocity();
        match.deflect(Match::Side::LeftSide, v.y() < 0.0f ? QVector2D(0, 1) : QVector2D(0, -1));

        const QVector2D after = match.ballVelocity().normalized();
        QVERIFY(std::abs(after.x()) >= qCos(qDegreesToRadians(Match::maxBounceAngle)) - 1e-4);
    }

    void deflectNeverTowardsOwnGoal()
    {
        Match match;
        match.setSpeedUp(1.0);

        for (int i = 0; i < 20; ++i) {
            match.start();
            match.advance(match.serveDelay());
            const Match::Side side = match.ballVelocity().x() < 0.0f ? Match::Side::LeftSide
                                                                     : Match::Side::RightSide;

            // Glancing off a paddle lying flat, a mirror would keep the ball
            // flying towards the goal
            const QVector2D v = match.ballVelocity();
            match.deflect(side, QVector2D(0.0f, v.y() > 0.0f ? -1.0f : 1.0f));

            const float awayFromGoal = side == Match::Side::LeftSide ? 1.0f : -1.0f;
            QVERIFY(match.ballVelocity().x() * awayFromGoal > 0.0f);
            QVERIFY(match.ballVelocity().y() * v.y() < 0.0f);
            QCOMPARE(match.ballVelocity().length(), v.length());
        }
    }

    void shieldAndBoost()
    {
        Match match;
        serve(match);

        const Match::Side goalSide = match.ballVelocity().x() < 0.0f ? Match::Side::LeftSide
                                                                     : Match::Side::RightSide;
        QVERIFY(!match.shieldHit(Match::opponent(goalSide)));
        const QVector2D before = match.ballVelocity();
        QVERIFY(match.shieldHit(goalSide));
        QCOMPARE(match.ballVelocity(), QVector2D(-before.x(), before.y()));
        QCOMPARE(match.rally(), 0);

        match.scaleBallSpeed(1.5);
        QCOMPARE(match.ballVelocity().length(), before.length() * 1.5f);
    }

    void pauseKeepsState()
    {
        Match match;
        match.start();

        match.pause();
        QCOMPARE(match.state(), Match::State::Paused);
        match.advance(10.0);
        QCOMPARE(match.state(), Match::State::Paused);
        match.resume();
        QCOMPARE(match.state(), Match::State::Serving);

        match.advance(match.serveDelay());
        match.advance(2.0);
        match.pause();
        match.advance(5.0);
        match.resume();
        QCOMPARE(match.state(), Match::State::Playing);
        QCOMPARE(match.playTime(), 2.0);
    }
};

QTEST_GUILESS_MAIN(tst_Match)
#include "tst_match.moc"
