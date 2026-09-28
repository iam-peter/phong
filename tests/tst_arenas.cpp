#include "arenas.h"

#include <QTest>

class tst_Arenas : public QObject
{
    Q_OBJECT

private slots:
    void shippedConfiguration()
    {
        Arenas arenas;
        QVERIFY(arenas.load(QStringLiteral(PHONG_SOURCE_DIR "/config/arenas.json")));
        QVERIFY(arenas.list().size() >= 2);

        const QVariantMap classic = arenas.arena(QStringLiteral("classic"));
        QCOMPARE(classic.value("name").toString(), QStringLiteral("Classic"));
        QVERIFY(classic.value("bumpers").toList().isEmpty());
        QVERIFY(arenas.obstacleRects(QStringLiteral("classic")).isEmpty());

        // Every arena leaves the serve spot free
        for (const Arenas::Arena& arena : arenas.list()) {
            for (const QVariant& rect : arenas.obstacleRects(arena.id))
                QVERIFY2(!rect.toRectF().contains(QPointF(0, 0)), qPrintable(arena.id));
        }

        // Also compiled in as the default
        Arenas embedded;
        QCOMPARE(embedded.list().size(), arenas.list().size());
    }

    void movingObstacles()
    {
        Arenas arenas;
        QTest::ignoreMessage(QtWarningMsg, "Skipping bumper of \"moving\" outside the field or on the serve spot");
        QVERIFY(arenas.loadJson(R"({ "arenas": [
            { "id": "moving",
              "bumpers": [ { "x": 5, "y": 0, "radius": 1, "move": { "y": 4, "period": 3, "phase": 0.5 } },
                           { "x": 3, "y": 1, "radius": 1, "move": { "x": -4 } } ] }
        ] })"));

        // The second one would sweep over the serve spot
        const Arenas::Arena& arena = arenas.list().first();
        QCOMPARE(arena.bumpers.size(), 1);
        const Arenas::Motion& move = arena.bumpers.first().move;
        QVERIFY(move.isMoving());
        QCOMPARE(move.y, 4.0);
        QCOMPARE(move.period, 3.0);
        QCOMPARE(move.phase, 0.5);

        // Items keep clear of all the room it takes
        const QRectF rect = arenas.obstacleRects(QStringLiteral("moving")).first().toRectF();
        QCOMPARE(rect, QRectF(4.0, -5.0, 2.0, 10.0));

        const QVariantMap map = arenas.arena(QStringLiteral("moving"))
                                    .value("bumpers").toList().first().toMap().value("move").toMap();
        QCOMPARE(map.value("period").toDouble(), 3.0);
    }

    void validation()
    {
        Arenas arenas;
        QTest::ignoreMessage(QtWarningMsg, "Skipping bumper of \"test\" outside the field or on the serve spot");
        QTest::ignoreMessage(QtWarningMsg, "Skipping block of \"test\" outside the field or on the serve spot");
        QTest::ignoreMessage(QtWarningMsg, "Skipping arena without a unique id");
        QVERIFY(arenas.loadJson(R"({ "arenas": [
            { "id": "test", "name": "Test",
              "bumpers": [ { "x": 3, "y": 3, "radius": 1 }, { "x": 0, "y": 0.5 } ],
              "blocks": [ { "x": 0, "y": 5, "width": 1, "height": 2 },
                          { "x": 30, "y": 0 } ] },
            { "id": "test" }
        ] })"));

        QCOMPARE(arenas.list().size(), 1);
        const Arenas::Arena& arena = arenas.list().first();
        QCOMPARE(arena.bumpers.size(), 1);
        QCOMPARE(arena.blocks.size(), 1);
        QCOMPARE(arena.blocks.first().rect, QRectF(-0.5, 4.0, 1.0, 2.0));
        QVERIFY(!arena.blocks.first().move.isMoving());
        QCOMPARE(arenas.obstacleRects(QStringLiteral("test")).size(), 2);

        QVERIFY(arenas.arena(QStringLiteral("missing")).isEmpty());
        QCOMPARE(arenas.randomId(), QStringLiteral("test"));

        QVERIFY(!arenas.loadJson("{"));
        QCOMPARE(arenas.list().size(), 1);
    }
};

QTEST_GUILESS_MAIN(tst_Arenas)
#include "tst_arenas.moc"
