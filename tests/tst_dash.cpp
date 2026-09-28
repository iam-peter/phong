#include "dash.h"

#include <QSignalSpy>
#include <QTest>

class tst_Dash : public QObject
{
    Q_OBJECT

private slots:
    void doubleTapDashes()
    {
        Dash dash;
        QSignalSpy dashed(&dash, &Dash::dashed);

        QVERIFY(!dash.tap(1));
        dash.advance(0.5 * Dash::doubleTapTime);
        QVERIFY(dash.tap(1));
        QCOMPARE(dashed.count(), 1);
        QCOMPARE(dashed.last().at(0).toInt(), 1);
        QVERIFY(dash.isActive());
        QCOMPARE(dash.direction(), 1);
        QVERIFY(!dash.isReady());
        QCOMPARE(dash.cooldown(), 1.0);

        // Over after its duration, ready again after the cooldown
        dash.advance(Dash::duration);
        QVERIFY(!dash.isActive());
        QCOMPARE(dash.direction(), 0);
        dash.advance(Dash::cooldownTime);
        QVERIFY(dash.isReady());
        QCOMPARE(dash.cooldown(), 0.0);
    }

    void slowTapsDontDash()
    {
        Dash dash;
        QVERIFY(!dash.tap(-1));
        dash.advance(2.0 * Dash::doubleTapTime);
        QVERIFY(!dash.tap(-1));
        QVERIFY(!dash.isActive());

        // Nor do taps in different directions
        dash.advance(0.05);
        QVERIFY(!dash.tap(1));
        QVERIFY(!dash.isActive());

        // But the last tap starts a new pair
        dash.advance(0.05);
        QVERIFY(dash.tap(1));
    }

    void cooldownBlocks()
    {
        Dash dash;
        QVERIFY(dash.trigger(-1));
        QCOMPARE(dash.direction(), -1);
        dash.advance(Dash::duration);

        QVERIFY(!dash.trigger(-1));
        QVERIFY(!dash.tap(1));
        dash.advance(0.05);
        QVERIFY(!dash.tap(1));
        QVERIFY(!dash.isActive());

        dash.reset();
        QVERIFY(dash.isReady());
        QVERIFY(!dash.trigger(0));
        QVERIFY(dash.trigger(1));
    }
};

QTEST_GUILESS_MAIN(tst_Dash)
#include "tst_dash.moc"
