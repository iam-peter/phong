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
            // Moving a little, a still paddle would make it a perfect hit
            match.paddleHit(side, 0.0, 2.0 * Match::perfectStillness * match.paddleSpeed());

            const QVector2D after = match.ballVelocity();
            QVERIFY(after.x() * before.x() < 0.0f);
            QCOMPARE(after.y(), 0.0f);
            QVERIFY(qAbs(after.length() - before.length() * 1.1f) < 1e-3f);
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

    void smashIsFaster()
    {
        Match match;
        match.setSpeedUp(1.0);
        match.setMaxSpeed(100.0);
        QSignalSpy hits(&match, &Match::paddleHitBall);
        serve(match);

        const Match::Side first = match.ballVelocity().x() < 0.0f ? Match::Side::LeftSide
                                                                  : Match::Side::RightSide;
        const float speed = match.ballVelocity().length();
        match.paddleHit(first, 0.5, 0.0, 1.0);
        QVERIFY(qAbs(match.ballVelocity().length() - speed * float(1.0 + Match::smashBoost)) < 1e-3f);
        QCOMPARE(hits.last().at(2).toReal(), 1.0);
        QCOMPARE(match.player(first)->hits(), 1);
        QVERIFY(match.ball()->isSmashed());

        // Beyond the max speed, but only a little
        match.setMaxSpeed(20.0);
        match.paddleHit(Match::opponent(first), 0.5, 0.0, 1.0);
        QVERIFY(qAbs(match.ballVelocity().length() - 20.0f * float(1.0 + Match::smashOverspeed)) < 1e-3f);
    }

    void perfectHit()
    {
        Match match;
        match.setSpeedUp(1.0);
        match.setMaxSpeed(100.0);
        QSignalSpy hits(&match, &Match::paddleHitBall);
        serve(match);

        // In the middle with a still paddle
        Match::Side side = match.ballVelocity().x() < 0.0f ? Match::Side::LeftSide : Match::Side::RightSide;
        float speed = match.ballVelocity().length();
        match.paddleHit(side, 0.5 * Match::perfectZone, 0.0);
        QVERIFY(hits.last().at(3).toBool());
        QVERIFY(qAbs(match.ballVelocity().length() - speed * float(1.0 + Match::perfectBoost)) < 1e-3f);
        QVERIFY(!match.ball()->isSmashed());

        // Off center or with a moving paddle it's a normal hit
        side = Match::opponent(side);
        speed = match.ballVelocity().length();
        match.paddleHit(side, 2.0 * Match::perfectZone, 0.0);
        QVERIFY(!hits.last().at(3).toBool());
        QVERIFY(qAbs(match.ballVelocity().length() - speed) < 1e-3f);

        side = Match::opponent(side);
        match.paddleHit(side, 0.0, 0.5 * match.paddleSpeed());
        QVERIFY(!hits.last().at(3).toBool());

        // At the top speed a perfect hit still goes a little faster
        match.setMaxSpeed(20.0);
        side = Match::opponent(side);
        match.paddleHit(side, 0.0, 0.0);
        side = Match::opponent(side);
        match.paddleHit(side, 0.0, 0.0);
        QVERIFY(qAbs(match.ballVelocity().length() - 20.0f * float(1.0 + Match::perfectOverspeed)) < 1e-3f);
    }

    void catchAndRelease()
    {
        Match match;
        match.setSpeedUp(1.0);
        QSignalSpy caught(&match, &Match::ballCaught);
        QSignalSpy hits(&match, &Match::paddleHitBall);
        serve(match);

        const Match::Side side = match.ballVelocity().x() < 0.0f ? Match::Side::LeftSide
                                                                 : Match::Side::RightSide;
        const Match::Side other = Match::opponent(side);
        const float speed = match.ballVelocity().length();

        // Only a magnetic paddle catches, and only balls coming at it
        QVERIFY(!match.catchBall(match.ball(), side, 0.0));
        match.player(side)->setCatches(2);
        match.player(other)->setCatches(2);
        QVERIFY(!match.catchBall(match.ball(), other, 0.0));

        QVERIFY(match.catchBall(match.ball(), side, 0.2));
        QCOMPARE(caught.count(), 1);
        QCOMPARE(match.player(side)->catches(), 1);
        QCOMPARE(match.ball()->heldBy(), side);
        QCOMPARE(match.ball()->lastTouch(), side);
        QCOMPARE(match.ballVelocity(), QVector2D());

        // A held ball ignores the paddle and isn't caught twice
        match.paddleHit(side, 0.0);
        QCOMPARE(match.ballVelocity(), QVector2D());
        QVERIFY(!match.catchBall(match.ball(), side, 0.0));
        QCOMPARE(hits.count(), 0);

        // Aimed at the top edge and released like a hit there
        match.aimHeldBall(match.ball(), 5.0);
        QCOMPARE(match.ball()->holdOffset(), 1.0);
        QVERIFY(match.releaseBall(match.ball()));
        QCOMPARE(match.ball()->heldBy(), Match::Side::NoSide);
        QCOMPARE(hits.count(), 1);
        QCOMPARE(match.rally(), 1);

        const QVector2D v = match.ballVelocity();
        QVERIFY(qAbs(v.length() - speed) < 1e-3f);
        QVERIFY(v.x() * (side == Match::Side::LeftSide ? 1.0f : -1.0f) > 0.0f);
        const qreal angle = qRadiansToDegrees(qAtan2(v.y(), std::abs(v.x())));
        QVERIFY(qAbs(angle - Match::maxBounceAngle) < 0.01);

        // The other side catches and holds until the time is up
        QVERIFY(match.catchBall(match.ball(), other, 0.0));
        match.advance(0.5 * Match::maxHoldTime);
        QCOMPARE(match.ball()->heldBy(), other);
        match.advance(0.6 * Match::maxHoldTime);
        QCOMPARE(match.ball()->heldBy(), Match::Side::NoSide);
        QVERIFY(match.ballVelocity().x() * (other == Match::Side::LeftSide ? 1.0f : -1.0f) > 0.0f);
        QCOMPARE(match.rally(), 2);

        // A goal ends any hold
        match.player(side)->setCatches(1);
        QVERIFY(match.catchBall(match.ball(), side, 0.0));
        match.goal(other);
        QCOMPARE(match.ball()->heldBy(), Match::Side::NoSide);
    }

    void gravityBends()
    {
        Match match;
        serve(match);

        const QVector2D before = match.ballVelocity();
        const QVector2D position(0.0f, 0.0f);
        // A well above the ball pulls it up, the speed stays
        for (int i = 0; i < 30; ++i)
            match.attract(match.ball(), position, QVector2D(before.x() > 0.0f ? 3.0f : -3.0f, 4.0f), 200.0, 1.0 / 60.0);

        const QVector2D after = match.ballVelocity();
        QVERIFY(after.y() > before.y());
        QVERIFY(qAbs(after.length() - before.length()) < 1e-3f);
        QVERIFY(after.x() * before.x() > 0.0f);

        // However strong, the ball keeps crossing the field
        for (int i = 0; i < 300; ++i)
            match.attract(match.ball(), position, QVector2D(0.0f, 5.0f), 400.0, 1.0 / 60.0);
        const QVector2D direction = match.ballVelocity().normalized();
        QVERIFY(std::abs(direction.x()) >= qCos(qDegreesToRadians(Match::maxBounceAngle)) - 1e-4);
        QVERIFY(direction.x() * before.x() > 0.0f);

        // No pull without strength
        const QVector2D velocity = match.ballVelocity();
        match.attract(match.ball(), position, QVector2D(0.0f, -5.0f), 0.0, 1.0);
        QCOMPARE(match.ballVelocity(), velocity);
    }

    // A ball twice as fast passes the well in half the time, it turns
    // twice as much meanwhile, so every ball is bent alike
    void gravityBendsAnySpeed()
    {
        const auto turn = [](qreal speed) {
            Match match;
            match.setServeSpeed(speed);
            serve(match);
            const QVector2D before = match.ballVelocity();
            // The well beside the flight
            const QVector2D side = QVector2D(-before.y(), before.x()).normalized();
            match.attract(match.ball(), QVector2D(), side * 4.0f, 250.0, 1.0 / 60.0);
            const QVector2D after = match.ballVelocity();
            return qAcos(std::clamp(qreal(QVector2D::dotProduct(before, after) / (before.length() * after.length())),
                                    qreal(-1.0), qreal(1.0)));
        };
        const qreal slow = turn(Match::gravitySpeed);
        const qreal fast = turn(2.0 * Match::gravitySpeed);
        QVERIFY(slow > 0.0);
        QVERIFY(qAbs(fast / slow - 2.0) < 0.05);
    }

    void awardedPoints()
    {
        Match match;
        match.setPointsToWin(3);
        QSignalSpy awarded(&match, &Match::pointAwarded);
        QSignalSpy scored(&match, &Match::pointScored);

        // Nothing while the ball isn't in play
        match.start();
        match.awardPoint(Match::Side::LeftSide);
        QCOMPARE(match.left()->score(), 0);

        // The rally goes on after a point
        serve(match);
        match.paddleHit(match.ballVelocity().x() < 0.0f ? Match::Side::LeftSide : Match::Side::RightSide, 0.5);
        const QVector2D velocity = match.ballVelocity();
        match.awardPoint(Match::Side::LeftSide);
        match.awardPoint(Match::Side::LeftSide);
        QCOMPARE(match.left()->score(), 2);
        QCOMPARE(awarded.count(), 2);
        QCOMPARE(scored.count(), 0);
        QCOMPARE(match.state(), Match::State::Playing);
        QCOMPARE(match.ballVelocity(), velocity);
        QCOMPARE(match.rally(), 1);
        QVERIFY(match.isMatchPoint());

        // Unless it wins
        match.awardPoint(Match::Side::LeftSide);
        QCOMPARE(match.state(), Match::State::Finished);
        QCOMPARE(match.winner(), match.left());
        QCOMPARE(match.ballVelocity(), QVector2D());
    }

    void powerBar()
    {
        Match match;
        QSignalSpy used(&match, &Match::specialUsed);
        serve(match);

        // Not before it's full
        const Match::Side side = match.ballVelocity().x() < 0.0f ? Match::Side::LeftSide
                                                                 : Match::Side::RightSide;
        QVERIFY(!match.useSpecial(side));

        // A normal hit fills an eighth, a perfect one a quarter
        match.paddleHit(side, 0.5, 5.0);
        QCOMPARE(match.player(side)->power(), Match::powerPerHit);
        match.paddleHit(Match::opponent(side), 0.5, 5.0);
        match.paddleHit(side, 0.0, 0.0);
        QCOMPARE(match.player(side)->power(), 3.0 * Match::powerPerHit);

        for (int i = 0; i < 10; ++i) {
            match.paddleHit(Match::opponent(side), 0.5, 5.0);
            match.paddleHit(side, 0.5, 5.0);
        }
        QCOMPARE(match.player(side)->power(), 1.0);

        // The special is a catch
        QVERIFY(match.useSpecial(side));
        QCOMPARE(used.count(), 1);
        QCOMPARE(match.player(side)->power(), 0.0);
        QCOMPARE(match.player(side)->catches(), 1);
        QVERIFY(!match.useSpecial(side));

        // A new match starts empty
        match.start();
        QCOMPARE(match.player(Match::opponent(side))->power(), 0.0);
    }

    void snapshotTravels()
    {
        Match host;
        host.setPointsToWin(7);
        host.setSetsToWin(2);
        host.left()->setName(QStringLiteral("Ping"));
        serve(host);
        host.left()->setCatches(2);
        host.right()->setShielded(true);
        host.paddleHit(host.ballVelocity().x() < 0.0f ? Match::Side::LeftSide : Match::Side::RightSide, 0.4, 5.0);
        host.addBall(QVector2D(1, 2), Match::Side::LeftSide, 5.0, Match::Side::RightSide);
        host.addBall(QVector2D(-1, 2), Match::Side::RightSide, 4.0, Match::Side::LeftSide);
        host.advance(0.5);

        Match client;
        QSignalSpy rally(&client, &Match::rallyChanged);
        client.applySnapshot(host.snapshot());
        QCOMPARE(client.state(), Match::State::Playing);
        QCOMPARE(client.pointsToWin(), 7);
        QCOMPARE(client.setsToWin(), 2);
        QCOMPARE(client.left()->name(), QStringLiteral("Ping"));
        QCOMPARE(client.left()->catches(), 2);
        QVERIFY(client.right()->isShielded());
        QCOMPARE(client.rally(), 1);
        QCOMPARE(rally.count(), 1);
        QCOMPARE(client.ballVelocity(), host.ballVelocity());
        QCOMPARE(client.ball()->lastTouch(), host.ball()->lastTouch());
        QCOMPARE(client.playTime(), host.playTime());

        // The extra balls as well, and when one is gone
        QCOMPARE(client.extraBalls()->rowCount(), 2);
        QCOMPARE(client.extraBalls()->balls().at(0)->spawnPosition(), QVector2D(1, 2));
        QCOMPARE(client.extraBalls()->balls().at(1)->lifetime(), host.extraBalls()->balls().at(1)->lifetime());
        host.goal(host.extraBalls()->balls().at(0), Match::Side::RightSide);
        client.applySnapshot(host.snapshot());
        QCOMPARE(client.extraBalls()->rowCount(), 1);
        QCOMPARE(client.right()->score(), 1);

        // Through to the winner
        host.setPointsToWin(1);
        host.setSetsToWin(1);
        host.goal(Match::Side::LeftSide);
        client.applySnapshot(host.snapshot());
        QCOMPARE(client.state(), Match::State::Finished);
        QCOMPARE(client.winner(), client.left());
    }

    void matchPoint()
    {
        Match match;
        match.setPointsToWin(3);
        match.setSetsToWin(2);
        QSignalSpy changed(&match, &Match::matchPointChanged);
        match.start();
        QVERIFY(!match.isMatchPoint());

        const auto point = [&match](Match::Side side) {
            serve(match);
            match.goal(side);
        };

        // Set point only, the first set doesn't decide the match
        point(Match::Side::LeftSide);
        point(Match::Side::LeftSide);
        QVERIFY(!match.isMatchPoint());
        point(Match::Side::LeftSide);

        // The right player only has a set point, left one set up
        point(Match::Side::RightSide);
        point(Match::Side::RightSide);
        QVERIFY(!match.isMatchPoint());

        point(Match::Side::LeftSide);
        point(Match::Side::LeftSide);
        QVERIFY(match.isMatchPoint());
        QVERIFY(changed.count() > 0);

        // 2 : 2 isn't enough with win by two
        match.setWinByTwo(true);
        QVERIFY(!match.isMatchPoint());
        match.setWinByTwo(false);

        point(Match::Side::LeftSide);
        QCOMPARE(match.state(), Match::State::Finished);
        QVERIFY(!match.isMatchPoint());
    }

    void endless()
    {
        Match match;
        match.setEndless(true);
        match.setPointsToWin(3);
        match.start();

        // The left player scores as much as they can, it never ends
        for (int i = 0; i < 10; ++i) {
            serve(match);
            match.goal(Match::Side::LeftSide);
        }
        QCOMPARE(match.left()->score(), 10);
        QCOMPARE(match.state(), Match::State::Serving);
        QVERIFY(!match.isMatchPoint());

        // Three lost balls do
        serve(match);
        match.goal(Match::Side::RightSide);
        serve(match);
        match.goal(Match::Side::RightSide);
        QVERIFY(match.isMatchPoint());
        serve(match);
        match.goal(Match::Side::RightSide);
        QCOMPARE(match.state(), Match::State::Finished);
        QCOMPARE(match.winner(), match.right());
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
