#include "highscores.h"
#include "relayserver.h"

#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

class tst_HighScores : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Lan keeps a token in the settings
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("phong-tests"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_highscores"));
    }

    void submitAndRefresh()
    {
        RelayServer server;
        QVERIFY(server.listen(0));

        HighScores scores;
        QVERIFY(!scores.isAvailable());
        scores.submit(QStringLiteral("endless"), QStringLiteral("Ace"), 10);
        QVERIFY(!scores.isBusy());

        scores.setServerUrl(QStringLiteral("127.0.0.1:%1").arg(server.port()));
        QVERIFY(scores.isAvailable());
        scores.submit(QStringLiteral("endless"), QStringLiteral("Ace"), 42);
        QVERIFY(scores.isBusy());
        QTRY_VERIFY(!scores.isBusy());
        QVERIFY(scores.error().isEmpty());
        QCOMPARE(scores.endless().size(), 1);
        QCOMPARE(scores.place(QStringLiteral("endless"), QStringLiteral("Ace"), 42), 0);
        QCOMPARE(scores.place(QStringLiteral("endless"), QStringLiteral("Ace"), 41), -1);

        // Somebody else's, seen with the next refresh
        HighScores other;
        other.setServerUrl(scores.serverUrl());
        other.submit(QStringLiteral("endless"), QStringLiteral("Bob"), 50);
        QTRY_VERIFY(!other.isBusy());
        scores.refresh(QStringLiteral("endless"));
        QTRY_VERIFY(!scores.isBusy());
        QCOMPARE(scores.endless().size(), 2);
        QCOMPARE(scores.place(QStringLiteral("endless"), QStringLiteral("Ace"), 42), 1);
        QVERIFY(scores.squash().isEmpty());
    }

    void nobodyThere()
    {
        HighScores scores;
        scores.setServerUrl(QStringLiteral("127.0.0.1:46499"));
        scores.refresh(QStringLiteral("squash"));
        QTRY_VERIFY_WITH_TIMEOUT(!scores.isBusy(), HighScores::timeout + 1000);
        QVERIFY(!scores.error().isEmpty());
    }
};

QTEST_GUILESS_MAIN(tst_HighScores)
#include "tst_highscores.moc"
