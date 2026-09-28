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

    void goalKicksOffTowardsScorer()
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

        // Like in football the right player, who conceded, kicks off: the
        // ball flies towards the scorer, in the announced direction
        QCOMPARE(match.serveTo(), Match::Side::LeftSide);
        const QVector2D announced = match.serveDirection();
        QVERIFY(announced.x() < 0.0f);
        match.advance(match.serveDelay());
        QVERIFY(match.ballVelocity().distanceToPoint(announced * float(match.serveSpeed())) < 1e-4f);

        // And the other way round
        match.goal(Match::Side::RightSide);
        QCOMPARE(match.serveTo(), Match::Side::RightSide);
        QVERIFY(match.serveDirection().x() > 0.0f);

        // A goal while nobody plays doesn't count
        match.advance(match.serveDelay());
        match.pause();
        match.goal(Match::Side::LeftSide);
        QCOMPARE(match.left()->score(), 1);
    }

    void announcedServe()
    {
        Match match;
        match.setServeDelay(2.0);
        match.start();

        // Known during the whole countdown, within the serve angles
        const QVector2D direction = match.serveDirection();
        QVERIFY(qAbs(direction.length() - 1.0f) < 1e-5f);
        QCOMPARE(direction.x() < 0.0f, match.serveTo() == Match::Side::LeftSide);
        const qreal angle = qRadiansToDegrees(qAtan2(std::abs(direction.y()), std::abs(direction.x())));
        QVERIFY(angle >= Match::minServeAngle - 0.01 && angle <= Match::maxServeAngle + 0.01);

        match.advance(1.5);
        QCOMPARE(match.serveDirection(), direction);
        QCOMPARE(match.serveCountdown(), 0.5);
        match.advance(0.5);
        QCOMPARE(match.state(), Match::State::Playing);
        QVERIFY(match.ballVelocity().distanceToPoint(direction * float(match.serveSpeed())) < 1e-4f);
    }

    void setLoserKicksOff()
    {
        Match match;
        match.setPointsToWin(1);
        match.setSetsToWin(2);
        serve(match);
        match.goal(Match::Side::RightSide);
        QCOMPARE(match.right()->sets(), 1);
        QCOMPARE(match.serveTo(), Match::Side::RightSide);
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

    void movingPaddleCurvesTheBall()
    {
        Match match;
        match.setSpeedUp(1.0);
        serve(match);
        if (match.ballVelocity().x() > 0.0f)
            match.paddleHit(Match::Side::RightSide, 0.0);

        // Brushing upwards, the ball dips on its way to the right
        match.paddleHit(Match::Side::LeftSide, 0.0, match.paddleSpeed());
        QCOMPARE(match.ball()->spin(), -Match::maxSpin);

        const float speed = match.ballVelocity().length();
        for (int i = 0; i < 10; ++i)
            match.advance(0.05);
        QVERIFY(match.ballVelocity().y() < 0.0f);
        QVERIFY(match.ballVelocity().x() > 0.0f);
        QVERIFY(qAbs(match.ballVelocity().length() - speed) < 0.01f);

        // The spin wears off
        const qreal spin = match.ball()->spin();
        QVERIFY(spin > -Match::maxSpin && spin < 0.0);

        // Walls take some spin away, a still paddle takes all of it
        match.wallHit(false);
        QVERIFY(qAbs(match.ball()->spin() - 0.5 * spin) < 1e-6);
        match.paddleHit(Match::Side::RightSide, 0.0, 0.0);
        QCOMPARE(match.ball()->spin(), 0.0);
    }

    void curveKeepsBallCrossing()
    {
        Match match;
        match.setSpeedUp(1.0);
        serve(match);
        if (match.ballVelocity().x() > 0.0f)
            match.paddleHit(Match::Side::RightSide, 0.0);

        // Steep return with spin turning it further
        match.paddleHit(Match::Side::LeftSide, 1.0, -match.paddleSpeed());
        for (int i = 0; i < 100; ++i)
            match.advance(0.05);

        const QVector2D direction = match.ballVelocity().normalized();
        QVERIFY(std::abs(direction.x()) >= qCos(qDegreesToRadians(Match::maxBounceAngle)) - 1e-3);
    }

    void bestOfThree()
    {
        Match match;
        match.setPointsToWin(2);
        match.setSetsToWin(2);
        QSignalSpy sets(&match, &Match::setFinished);

        match.start();
        for (int i = 0; i < 2; ++i) {
            serve(match);
            match.goal(Match::Side::LeftSide);
        }
        QCOMPARE(sets.count(), 1);
        QCOMPARE(match.left()->sets(), 1);
        QCOMPARE(match.left()->score(), 0);
        QCOMPARE(match.state(), Match::State::Serving);

        for (int i = 0; i < 2; ++i) {
            serve(match);
            match.goal(Match::Side::RightSide);
        }
        QCOMPARE(match.right()->sets(), 1);

        for (int i = 0; i < 2; ++i) {
            serve(match);
            match.goal(Match::Side::LeftSide);
        }
        QCOMPARE(match.state(), Match::State::Finished);
        QCOMPARE(match.winner(), match.left());
        QCOMPARE(match.left()->sets(), 2);
        // The final set score stays for the results
        QCOMPARE(match.left()->score(), 2);
    }

    void winByTwo()
    {
        Match match;
        match.setPointsToWin(3);
        match.setWinByTwo(true);
        match.start();

        const auto point = [&match](Match::Side side) {
            serve(match);
            match.goal(side);
        };

        point(Match::Side::LeftSide);
        point(Match::Side::LeftSide);
        point(Match::Side::RightSide);
        point(Match::Side::RightSide);
        point(Match::Side::LeftSide);
        QCOMPARE(match.left()->score(), 3);
        QCOMPARE(match.state(), Match::State::Serving);

        point(Match::Side::LeftSide);
        QCOMPARE(match.state(), Match::State::Finished);
        QCOMPARE(match.winner(), match.left());
    }

    void extraBalls()
    {
        Match match;
        match.setPointsToWin(3);
        QVERIFY(!match.addBall(QVector2D(0, 0), Match::Side::LeftSide, 5.0, Match::Side::RightSide));

        serve(match);
        Ball* extra = match.addBall(QVector2D(1, 2), Match::Side::LeftSide, 5.0,
                                    Match::Side::RightSide);
        QVERIFY(extra);
        QVERIFY(extra->isExtra());
        QCOMPARE(extra->spawnPosition(), QVector2D(1, 2));
        QCOMPARE(extra->lastTouch(), Match::Side::RightSide);
        QVERIFY(extra->velocity().x() < 0.0f);
        QCOMPARE(match.extraBalls()->rowCount(), 1);
        QCOMPARE(match.balls().size(), 2);

        // Each ball bounces on its own
        const QVector2D main = match.ballVelocity();
        match.paddleHit(extra, Match::Side::LeftSide, 0.0);
        QVERIFY(extra->velocity().x() > 0.0f);
        QCOMPARE(match.ballVelocity(), main);

        // An extra goal scores but play goes on
        QSignalSpy scored(&match, &Match::pointScored);
        match.goal(extra, Match::Side::LeftSide);
        QCOMPARE(match.left()->score(), 1);
        QCOMPARE(match.state(), Match::State::Playing);
        QCOMPARE(match.extraBalls()->rowCount(), 0);
        QCOMPARE(scored.count(), 1);

        // Gone after their lifetime
        match.addBall(QVector2D(0, 0), Match::Side::RightSide, 1.0, Match::Side::LeftSide);
        match.advance(0.6);
        QCOMPARE(match.extraBalls()->rowCount(), 1);
        match.advance(0.6);
        QCOMPARE(match.extraBalls()->rowCount(), 0);

        // The main ball's goal clears the others
        match.addBall(QVector2D(0, 0), Match::Side::RightSide, 10.0, Match::Side::LeftSide);
        match.goal(Match::Side::RightSide);
        QCOMPARE(match.extraBalls()->rowCount(), 0);
        QCOMPARE(match.state(), Match::State::Serving);
    }

    void bounceOffObstacles()
    {
        Match match;
        serve(match);
        const QVector2D before = match.ballVelocity();

        // Head-on into a bumper
        const QVector2D normal = QVector2D(before.x() > 0.0f ? -1.0f : 1.0f, 0.0f);
        match.bounce(match.ball(), normal);
        QVERIFY(match.ballVelocity().distanceToPoint(QVector2D(-before.x(), before.y())) < 1e-4f);
        QCOMPARE(match.rally(), 0);
        QCOMPARE(match.lastTouch(), Match::Side::NoSide);

        // Grazing the top of a block would send it straight up, it keeps crossing
        match.bounce(match.ball(), QVector2D(0.0f, match.ballVelocity().y() > 0.0f ? -1.0f : 1.0f));
        const QVector2D direction = match.ballVelocity().normalized();
        QVERIFY(std::abs(direction.x()) >= qCos(qDegreesToRadians(Match::maxBounceAngle)) - 1e-4);
        QVERIFY(qAbs(match.ballVelocity().length() - before.length()) < 1e-3f);
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
