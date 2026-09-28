#include "lan.h"

#include <QSignalSpy>
#include <QTest>

class tst_Lan : public QObject
{
    Q_OBJECT

private slots:
    void hostAndJoin()
    {
        Lan host;
        QSignalSpy joined(&host, &Lan::peerJoined);
        QSignalSpy received(&host, &Lan::received);
        QSignalSpy peerLeft(&host, &Lan::peerLeft);
        QVERIFY(host.host(QStringLiteral("Host"), { { QStringLiteral("mode"), QStringLiteral("party") } }, 46455));
        QCOMPARE(host.role(), Lan::Host);
        QVERIFY(host.port() >= 46455);
        for (const QString& address : host.addresses())
            QVERIFY(address.endsWith(QStringLiteral(":%1").arg(host.port())));

        Lan client;
        QSignalSpy clientJoined(&client, &Lan::joined);
        QSignalSpy fromHost(&client, &Lan::receivedFromHost);
        client.join(QStringLiteral("127.0.0.1:%1").arg(host.port()), QStringLiteral("Guest"));
        QCOMPARE(client.role(), Lan::Joining);

        QTRY_COMPARE(clientJoined.count(), 1);
        QCOMPARE(client.role(), Lan::Client);
        QTRY_COMPARE(joined.count(), 1);
        const int id = joined.last().at(0).toInt();
        QCOMPARE(joined.last().at(1).toString(), QStringLiteral("Guest"));
        QCOMPARE(client.clientId(), id);
        QCOMPARE(host.peers().size(), 1);

        // Both ways
        client.sendToHost({ { QStringLiteral("t"), QStringLiteral("input") }, { QStringLiteral("move"), 0.5 } });
        QTRY_COMPARE(received.count(), 1);
        QCOMPARE(received.last().at(0).toInt(), id);
        QCOMPARE(received.last().at(1).toMap().value(QStringLiteral("move")).toDouble(), 0.5);

        host.sendAll({ { QStringLiteral("t"), QStringLiteral("state") }, { QStringLiteral("rally"), 3 } });
        host.send(id, { { QStringLiteral("t"), QStringLiteral("only") } });
        QTRY_COMPARE(fromHost.count(), 2);
        QCOMPARE(fromHost.at(0).at(0).toMap().value(QStringLiteral("rally")).toInt(), 3);

        // The client leaves
        client.leave();
        QTRY_COMPARE(peerLeft.count(), 1);
        QCOMPARE(peerLeft.last().at(0).toInt(), id);
        QVERIFY(host.peers().isEmpty());
    }

    void hostGoesAway()
    {
        Lan host;
        QVERIFY(host.host(QStringLiteral("Host"), {}, 46465));

        Lan client;
        QSignalSpy left(&client, &Lan::left);
        client.join(QStringLiteral("ws://127.0.0.1:%1").arg(host.port()), QStringLiteral("Guest"));
        QTRY_COMPARE(client.role(), Lan::Client);

        host.leave();
        QTRY_COMPARE(left.count(), 1);
        QCOMPARE(client.role(), Lan::NoRole);
    }

    void nobodyThere()
    {
        Lan client;
        QSignalSpy left(&client, &Lan::left);
        client.join(QStringLiteral("127.0.0.1:46475"), QStringLiteral("Guest"));
        QTRY_COMPARE(left.count(), 1);
        QVERIFY(!client.error().isEmpty());
    }

    void discovery()
    {
        Lan browser;
        browser.startBrowsing();

        Lan host;
        QVERIFY(host.host(QStringLiteral("Found me"), { { QStringLiteral("players"), 4 } }, 46485));

        QTRY_VERIFY_WITH_TIMEOUT(!browser.games().isEmpty(), 5000);
        const QVariantMap game = browser.games().first().toMap();
        QCOMPARE(game.value(QStringLiteral("name")).toString(), QStringLiteral("Found me"));
        QVERIFY(game.value(QStringLiteral("url")).toString().endsWith(QStringLiteral(":%1").arg(host.port())));
        QCOMPARE(game.value(QStringLiteral("info")).toMap().value(QStringLiteral("players")).toInt(), 4);

        // Gone after a while without announcements
        host.leave();
        QTRY_VERIFY_WITH_TIMEOUT(browser.games().isEmpty(), 8000);
    }
};

QTEST_GUILESS_MAIN(tst_Lan)
#include "tst_lan.moc"
