#include "computerplayer.h"

#include <QTest>
#include <QtMath>

#include <cmath>

class tst_ComputerPlayer : public QObject
{
    Q_OBJECT

private slots:
    void predictY_data()
    {
        QTest::addColumn<QVector2D>("position");
        QTest::addColumn<QVector2D>("velocity");
        QTest::addColumn<qreal>("expected");

        // Field from -10 to 10, paddle line at x = 10
        QTest::newRow("straight") << QVector2D(0, 3) << QVector2D(5, 0) << 3.0;
        QTest::newRow("no bounce") << QVector2D(0, 0) << QVector2D(10, 5) << 5.0;
        QTest::newRow("top bounce") << QVector2D(0, 5) << QVector2D(10, 10) << 5.0;
        QTest::newRow("bottom bounce") << QVector2D(0, -5) << QVector2D(10, -10) << -5.0;
        QTest::newRow("two bounces") << QVector2D(-10, 0) << QVector2D(10, 30) << -0.0;
        QTest::newRow("corner") << QVector2D(0, 0) << QVector2D(10, 10) << 10.0;
        QTest::newRow("moving away") << QVector2D(0, 4) << QVector2D(-10, 10) << 4.0;
        QTest::newRow("standing still") << QVector2D(0, 4) << QVector2D(0, 10) << 4.0;
        QTest::newRow("clamped") << QVector2D(0, 12) << QVector2D(0, 0) << 10.0;
    }

    void predictY()
    {
        QFETCH(QVector2D, position);
        QFETCH(QVector2D, velocity);
        QFETCH(qreal, expected);

        const qreal y = ComputerPlayer::predictY(position, velocity, 10.0, 10.0, -10.0);
        QVERIFY2(qAbs(y - expected) < 1e-4, qPrintable(QString::number(y)));
    }

    void waitsInTheMiddle()
    {
        ComputerPlayer player;
        player.setPaddleX(12.0);
        player.setPaddleReach(2.8);
        player.setFieldTop(8.0);
        player.setFieldBottom(-8.0);

        // Ball flies towards the other side
        for (int i = 0; i < 60; ++i)
            player.update(1.0 / 60.0, QVector2D(0, 0), QVector2D(-10, 0), 5.0);

        QCOMPARE(player.target(), 0.0);
        QVERIFY(player.direction() < 0.0);
    }

    void followsTheBall_data()
    {
        QTest::addColumn<ComputerPlayer::Difficulty>("difficulty");
        QTest::addColumn<qreal>("maxInput");
        QTest::addColumn<qreal>("maxError");
        QTest::addColumn<qreal>("expected");

        // Easy chases the ball, the others foresee the bounce
        QTest::newRow("easy") << ComputerPlayer::Difficulty::Easy << 0.5 << 1.6 << 0.0;
        QTest::newRow("normal") << ComputerPlayer::Difficulty::Normal << 0.75 << 1.15 << 6.0;
        QTest::newRow("hard") << ComputerPlayer::Difficulty::Hard << 1.0 << 0.6 << 6.0;
    }

    void followsTheBall()
    {
        QFETCH(ComputerPlayer::Difficulty, difficulty);
        QFETCH(qreal, maxInput);
        QFETCH(qreal, maxError);
        QFETCH(qreal, expected);

        const qreal reach = 2.8;

        for (int i = 0; i < 50; ++i) {
            ComputerPlayer player;
            player.setDifficulty(difficulty);
            player.setPaddleX(12.0);
            player.setPaddleReach(reach);
            player.setFieldTop(8.0);
            player.setFieldBottom(-8.0);

            // Ball at y = 0 on its way over, arrives at y = 6 after one bounce
            const QVector2D position(-12, 0);
            const QVector2D velocity(12, 5);
            for (int frame = 0; frame < 30; ++frame)
                player.update(1.0 / 60.0, position, velocity, -6.0);

            QVERIFY(qAbs(player.target() - expected) <= maxError * reach + 1e-6);
            QVERIFY(qAbs(player.direction()) <= maxInput + 1e-6);
        }
    }

    void aimOffset()
    {
        // Right paddle line at x = 12, field from -8 to 8
        const QList<QVector2D> ahead = { QVector2D(0, 5) };
        const qreal offset = ComputerPlayer::aimOffset(0.0, 12.0, ahead);
        const qreal expected = qRadiansToDegrees(std::atan2(5.0, 12.0)) / 60.0;
        QVERIFY(qAbs(offset - expected) < 1e-6);

        // Behind the paddle doesn't count
        QVERIFY(qIsNaN(ComputerPlayer::aimOffset(0.0, 12.0, { QVector2D(14, 0) })));

        // Too steep, and the easier of two targets wins
        QVERIFY(qIsNaN(ComputerPlayer::aimOffset(6.0, 12.0, { QVector2D(9, -6) })));
        const qreal easier = ComputerPlayer::aimOffset(0.0, 12.0, { QVector2D(9, -6), QVector2D(0, 5) });
        QVERIFY(qAbs(easier - expected) < 1e-6);

        // The left paddle aims to the right
        const qreal left = ComputerPlayer::aimOffset(0.0, -12.0, { QVector2D(0, -5) });
        QVERIFY(qAbs(left + expected) < 1e-6);
    }

    void awayFromOpponent()
    {
        // Opponent high up, go low, and the other way round
        QVERIFY(ComputerPlayer::awayOffset(0.0, 12.0, 5.0, 8.0, -8.0) < 0.0);
        QVERIFY(ComputerPlayer::awayOffset(0.0, 12.0, -5.0, 8.0, -8.0) > 0.0);
        QVERIFY(std::abs(ComputerPlayer::awayOffset(0.0, 12.0, 5.0, 8.0, -8.0)) <= 40.0 / 60.0);
    }

    void hardAimsAtTargets()
    {
        // With a target high up the paddle meets the ball below its center
        qreal sum = 0.0;
        for (quint32 seed = 1; seed <= 40; ++seed) {
            ComputerPlayer player;
            player.setSeed(seed);
            player.setDifficulty(ComputerPlayer::Difficulty::Hard);
            player.setPaddleX(12.0);
            player.setPaddleReach(2.8);
            player.setFieldTop(8.0);
            player.setFieldBottom(-8.0);
            player.setTargets({ QVariant::fromValue(QVector2D(0, 5)) });

            for (int frame = 0; frame < 30; ++frame)
                player.update(1.0 / 60.0, QVector2D(-12, 0), QVector2D(12, 0), 0.0);
            sum += player.target();
        }

        const qreal offset = qRadiansToDegrees(std::atan2(5.0, 12.0)) / 60.0;
        QVERIFY(qAbs(sum / 40.0 + offset * 2.8) < 0.5);
    }

    void reactsWithDelay()
    {
        ComputerPlayer player;
        player.setDifficulty(ComputerPlayer::Difficulty::Easy);
        player.setPaddleX(12.0);
        player.setPaddleReach(2.8);
        player.setFieldTop(8.0);
        player.setFieldBottom(-8.0);

        // The ball just turned around, easy needs 0.35 s to notice
        player.update(0.1, QVector2D(0, 0), QVector2D(10, 5), 0.0);
        QCOMPARE(player.target(), 0.0);
        player.update(0.1, QVector2D(1, 0.5), QVector2D(10, 5), 0.0);
        QCOMPARE(player.target(), 0.0);
        player.update(0.25, QVector2D(3.5, 1.75), QVector2D(10, 5), 0.0);
        QVERIFY(player.target() != 0.0);
    }
};

QTEST_GUILESS_MAIN(tst_ComputerPlayer)
#include "tst_computerplayer.moc"
