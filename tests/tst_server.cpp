#include "relayserver.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QWebSocket>

// A connection to the server that keeps what it gets
class Connection : public QObject
{
    Q_OBJECT

public:
    explicit Connection(quint16 port)
    {
        connect(&socket, &QWebSocket::textMessageReceived, this, [this](const QString& text) {
            messages.append(QJsonDocument::fromJson(text.toUtf8()).object().toVariantMap());
        });
        socket.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(port)));
    }

    bool waitConnected()
    {
        if (socket.state() == QAbstractSocket::ConnectedState)
            return true;
        QSignalSpy connected(&socket, &QWebSocket::connected);
        return connected.wait(3000);
    }

    void send(const QVariantMap& message)
    {
        socket.sendTextMessage(
            QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(message)).toJson(QJsonDocument::Compact)));
    }

    // The next message, after the ones read before
    QVariantMap next()
    {
        (void)QTest::qWaitFor([this] { return read < messages.size(); }, 3000);
        return read < messages.size() ? messages.at(read++) : QVariantMap();
    }

    QWebSocket socket;
    QList<QVariantMap> messages;
    qsizetype read = 0;
};

class tst_Server : public QObject
{
    Q_OBJECT

private slots:
    void roomsPassMessages()
    {
        RelayServer server;
        QVERIFY(server.listen(0));

        Connection host(server.port());
        QVERIFY(host.waitConnected());
        host.send({ { "t", "create" } });
        const QVariantMap room = host.next();
        QCOMPARE(room.value("t").toString(), QStringLiteral("room"));
        const QString code = room.value("code").toString();
        QCOMPARE(code.size(), RelayServer::codeLength);
        QCOMPARE(server.roomCount(), 1);

        // A wrong code is turned away
        Connection stranger(server.port());
        QVERIFY(stranger.waitConnected());
        stranger.send({ { "t", "join" }, { "code", "ZZZZ" } });
        QCOMPARE(stranger.next().value("t").toString(), QStringLiteral("error"));

        // The code in lower case works too
        Connection guest(server.port());
        QVERIFY(guest.waitConnected());
        guest.send({ { "t", "join" }, { "code", code.toLower() } });
        QCOMPARE(guest.next().value("t").toString(), QStringLiteral("joined"));
        const QVariantMap open = host.next();
        QCOMPARE(open.value("t").toString(), QStringLiteral("open"));
        const int peer = open.value("peer").toInt();

        // Plain from the guest, wrapped for the host, and back
        guest.send({ { "t", "hello" }, { "name", "Guest" } });
        const QVariantMap from = host.next();
        QCOMPARE(from.value("t").toString(), QStringLiteral("from"));
        QCOMPARE(from.value("peer").toInt(), peer);
        QCOMPARE(from.value("m").toMap().value("name").toString(), QStringLiteral("Guest"));

        host.send({ { "t", "to" }, { "peer", peer }, { "m", QVariantMap{ { "t", "welcome" }, { "id", 7 } } } });
        QCOMPARE(guest.next().value("id").toInt(), 7);
        host.send({ { "t", "all" }, { "m", QVariantMap{ { "t", "state" } } } });
        QCOMPARE(guest.next().value("t").toString(), QStringLiteral("state"));

        // The guest leaves, then the host and the room goes
        guest.socket.close();
        const QVariantMap gone = host.next();
        QCOMPARE(gone.value("t").toString(), QStringLiteral("gone"));
        QCOMPARE(gone.value("peer").toInt(), peer);

        Connection late(server.port());
        QVERIFY(late.waitConnected());
        late.send({ { "t", "join" }, { "code", code } });
        QCOMPARE(late.next().value("t").toString(), QStringLiteral("joined"));
        host.next();
        QSignalSpy lateClosed(&late.socket, &QWebSocket::disconnected);
        host.socket.close();
        QVERIFY(lateClosed.wait(3000));
        QTRY_COMPARE(server.roomCount(), 0);
    }

    void highScores()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath(QStringLiteral("scores.json"));
        {
            RelayServer server(file);
            QVERIFY(server.listen(0));
            Connection player(server.port());
            QVERIFY(player.waitConnected());

            for (int score : { 30, 50, 10 }) {
                player.send({ { "t", "score" }, { "board", "endless" }, { "name", "ACE" }, { "score", score } });
                player.next();
            }
            // Names are cleaned, silly scores and boards ignored
            player.send({ { "t", "score" }, { "board", "endless" }, { "name", " <b>Bob</b>\n " }, { "score", 40 } });
            player.next();
            player.send({ { "t", "score" }, { "board", "endless" }, { "name", "X" }, { "score", -5 } });
            player.next();
            player.send({ { "t", "score" }, { "board", "nope" }, { "name", "X" }, { "score", 5 } });
            player.next();

            player.send({ { "t", "scores" }, { "board", "endless" } });
            const QVariantList list = player.next().value("list").toList();
            QCOMPARE(list.size(), 4);
            QCOMPARE(list.at(0).toMap().value("score").toInt(), 50);
            QCOMPARE(list.at(1).toMap().value("name").toString(), QStringLiteral("bBobb"));
            QCOMPARE(list.at(3).toMap().value("score").toInt(), 10);
            QVERIFY(server.scores(QStringLiteral("squash")).isEmpty());

            // Only the best ten stay
            for (int score = 100; score < 120; ++score) {
                player.send({ { "t", "score" }, { "board", "endless" }, { "name", "Z" }, { "score", score } });
                player.next();
            }
            QCOMPARE(server.scores(QStringLiteral("endless")).size(), RelayServer::maxScores);
            QCOMPARE(server.scores(QStringLiteral("endless")).first().toMap().value("score").toInt(), 119);
        }

        // Kept in the file
        RelayServer again(file);
        QCOMPARE(again.scores(QStringLiteral("endless")).size(), RelayServer::maxScores);
    }
};

QTEST_GUILESS_MAIN(tst_Server)
#include "tst_server.moc"
