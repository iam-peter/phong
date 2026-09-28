#include "lan.h"

#if defined(PHONG_HAVE_RELAY)
#include "relayserver.h"
#endif

#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

class tst_Lan : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // The token is kept in the settings
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("phong-tests"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_lan"));
    }

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
        QVERIFY(!client.token().isEmpty());
        QCOMPARE(joined.last().at(2).toString(), client.token());
        QCOMPARE(Lan().token(), client.token());
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

#if defined(PHONG_HAVE_RELAY)
    void throughTheServer()
    {
        RelayServer server;
        QVERIFY(server.listen(0));
        const QString url = QStringLiteral("127.0.0.1:%1").arg(server.port());

        Lan host;
        QSignalSpy joined(&host, &Lan::peerJoined);
        QSignalSpy received(&host, &Lan::received);
        QSignalSpy peerLeft(&host, &Lan::peerLeft);
        host.hostOnline(url, QStringLiteral("Host"), {});
        QTRY_COMPARE(host.role(), Lan::Host);
        QVERIFY(host.isOnline());
        QCOMPARE(host.roomCode().size(), RelayServer::codeLength);

        // A wrong code
        Lan lost;
        QSignalSpy lostLeft(&lost, &Lan::left);
        lost.joinOnline(url, QStringLiteral("QQQQ"), QStringLiteral("Lost"));
        QTRY_COMPARE(lostLeft.count(), 1);
        QVERIFY(!lost.error().isEmpty());

        Lan client;
        QSignalSpy fromHost(&client, &Lan::receivedFromHost);
        QSignalSpy latency(&client, &Lan::latencyChanged);
        client.joinOnline(url, host.roomCode().toLower(), QStringLiteral("Guest"));
        QTRY_COMPARE(client.role(), Lan::Client);
        QVERIFY(client.isOnline());
        QTRY_COMPARE(joined.count(), 1);
        QCOMPARE(joined.last().at(1).toString(), QStringLiteral("Guest"));
        const int id = joined.last().at(0).toInt();
        QCOMPARE(client.clientId(), id);

        // Both ways, and the latency is measured without the game seeing it
        client.sendToHost({ { QStringLiteral("t"), QStringLiteral("input") }, { QStringLiteral("move"), 1 } });
        QTRY_COMPARE(received.count(), 1);
        QCOMPARE(received.last().at(0).toInt(), id);
        host.send(id, { { QStringLiteral("t"), QStringLiteral("one") } });
        host.sendAll({ { QStringLiteral("t"), QStringLiteral("all") } });
        QTRY_COMPARE(fromHost.count(), 2);
        QCOMPARE(fromHost.at(0).at(0).toMap().value(QStringLiteral("t")).toString(), QStringLiteral("one"));
        QTRY_VERIFY(latency.count() > 0);
        QVERIFY(client.latency() >= 0);
        QCOMPARE(received.count(), 1);

        // Kicked through the server
        QSignalSpy left(&client, &Lan::left);
        host.kick(id);
        QTRY_COMPARE(left.count(), 1);
        QTRY_COMPARE(peerLeft.count(), 1);

        // The host goes, so does the room
        host.leave();
        QTRY_COMPARE(server.roomCount(), 0);
    }

    void onlineNeverShowsTheMachine()
    {
        Lan lan;
        QCOMPARE(lan.localName(), lan.machineName());
        lan.setPlayerName(QStringLiteral("  Ace of spades and more  "));
        QCOMPARE(lan.localName(), QStringLiteral("Ace of spade"));
        QCOMPARE(Lan::serverUrl(QStringLiteral("example.com"), 45460).toString(),
                 QStringLiteral("ws://example.com:45460"));
        QCOMPARE(Lan::serverUrl(QStringLiteral("https://example.com/phong"), 45460).toString(),
                 QStringLiteral("wss://example.com/phong"));
    }
#endif

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
