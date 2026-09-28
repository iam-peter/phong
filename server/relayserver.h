#ifndef RELAYSERVER_H
#define RELAYSERVER_H

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QRandomGenerator>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

class QWebSocket;
class QWebSocketServer;

// Games over the internet and the shared high scores. A host opens a room
// and gets a code, the others join the room with the code and the server
// passes the messages between them. It doesn't look into the game, the
// host runs it like on the LAN.
//
// Every connection starts with one of:
//   { t: "create" }                  opens a room: { t: "room", code }
//   { t: "join", code }              joins it: { t: "joined" }, the host
//                                    gets { t: "open", peer }
//   { t: "score", board, name, score } adds a score: { t: "scores", ... }
//   { t: "scores", board }           the best: { t: "scores", board, list }
// In a room the host sends { t: "to", peer, m }, { t: "all", m } or
// { t: "close", peer } and gets { t: "from", peer, m } and { t: "gone",
// peer }. The others send and get the plain messages.
class RelayServer : public QObject
{
    Q_OBJECT

public:
    static constexpr int codeLength = 4;
    static constexpr int maxRooms = 500;
    static constexpr int maxPeers = 8;
    static constexpr int maxScores = 10;
    static constexpr int maxNameLength = 12;
    static constexpr int maxScore = 1000000;
    static constexpr qint64 maxMessageSize = 64 * 1024;

    // Scores are kept in dataFile, none if it's empty
    explicit RelayServer(const QString& dataFile = QString(), QObject* parent = nullptr);
    ~RelayServer() override;

    bool listen(quint16 port);
    quint16 port() const;
    int roomCount() const;

    // The best scores of a board, { name, score }
    QVariantList scores(const QString& board) const;
    static bool isBoard(const QString& board);

    void setSeed(quint32 seed);

private:
    struct Room {
        QString code;
        QPointer<QWebSocket> host;
        QHash<int, QPointer<QWebSocket>> peers;
        int nextPeer = 1;
    };

    void accept();
    void received(QWebSocket* socket, const QString& text);
    void closed(QWebSocket* socket);
    void create(QWebSocket* socket);
    void join(QWebSocket* socket, const QString& code);
    void fromHost(Room& room, const QVariantMap& message);
    void addScore(QWebSocket* socket, const QVariantMap& message);
    void sendScores(QWebSocket* socket, const QString& board);
    QString newCode();
    void loadScores();
    void saveScores() const;
    static void send(QWebSocket* socket, const QVariantMap& message);

    QWebSocketServer* m_server;
    QString m_dataFile;
    QHash<QString, Room> m_rooms;
    // Where each connection belongs: the code of its room and its peer
    // id there, 0 for the host
    struct Place {
        QString code;
        int peer;
    };
    QHash<QWebSocket*, Place> m_places;
    QHash<QString, QVariantList> m_scores;
    QTimer m_keepAlive;
    QRandomGenerator m_random;
};

#endif // RELAYSERVER_H
