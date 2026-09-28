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
        QCOMPARE(arena.blocks.first(), QRectF(-0.5, 4.0, 1.0, 2.0));
        QCOMPARE(arenas.obstacleRects(QStringLiteral("test")).size(), 2);

        QVERIFY(arenas.arena(QStringLiteral("missing")).isEmpty());
        QCOMPARE(arenas.randomId(), QStringLiteral("test"));

        QVERIFY(!arenas.loadJson("{"));
        QCOMPARE(arenas.list().size(), 1);
    }
};

QTEST_GUILESS_MAIN(tst_Arenas)
#include "tst_arenas.moc"
