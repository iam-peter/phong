#ifndef LAN_H
#define LAN_H

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QUdpSocket;
class QWebSocket;
class QWebSocketServer;

// Games on the local network. A host runs the game and takes the others
// in over WebSockets, they send their input and get the state back. Hosts
// announce themselves by UDP broadcast, browsers can't receive those or
// host, but they can join a host by its address.
//
// Over the internet the same game goes through phong-server: the host
// opens a room there and gets a code, the others join with the code, and
// the server passes the messages. Browsers can host those too.
class Lan : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(Role role READ role NOTIFY roleChanged)
    Q_PROPERTY(bool canHost READ canHost CONSTANT)
    // The players joined, { id, name }, while hosting
    Q_PROPERTY(QVariantList peers READ peers NOTIFY peersChanged)
    // Addresses to join this host at, e.g. from a browser
    Q_PROPERTY(QStringList addresses READ addresses NOTIFY roleChanged)
    Q_PROPERTY(int port READ port NOTIFY roleChanged)
    // Hosts found on the network while browsing: { name, url, info }
    Q_PROPERTY(QVariantList games READ games NOTIFY gamesChanged)
    // The id the host gave this client, -1 before
    Q_PROPERTY(int clientId READ clientId NOTIFY roleChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    // A name for this machine's games and players
    Q_PROPERTY(QString machineName READ machineName CONSTANT)
    // Random and kept, it tells a host that a player is back
    Q_PROPERTY(QString token READ token CONSTANT)
    // The name the player chose, empty for none
    Q_PROPERTY(QString playerName READ playerName WRITE setPlayerName NOTIFY nameChanged)
    // The name the others see: the chosen one, or the machine's on the
    // LAN, never the machine's over the internet
    Q_PROPERTY(QString localName READ localName NOTIFY nameChanged)
    // Hosting or joined through the server
    Q_PROPERTY(bool online READ isOnline NOTIFY roleChanged)
    // The code of the room while hosting online
    Q_PROPERTY(QString roomCode READ roomCode NOTIFY roleChanged)
    // Milliseconds to the host and back, -1 before the first answer
    Q_PROPERTY(int latency READ latency NOTIFY latencyChanged)

public:
    enum Role {
        NoRole = 0,
        Host,
        Joining,
        Client
    };
    Q_ENUM(Role)

    static constexpr quint16 defaultPort = 45455;
    static constexpr quint16 discoveryPort = 45454;
    // Bumped when the messages change, older clients are turned away
    static constexpr int protocolVersion = 2;
    // Seconds between two latency checks
    static constexpr int pingInterval = 1000;

    explicit Lan(QObject* parent = nullptr);
    ~Lan() override;

    // Opens a game on the first free port from port on. info is
    // announced to browsing clients, e.g. the mode and the free sides.
    Q_INVOKABLE bool host(const QString& name, const QVariantMap& info, quint16 port = defaultPort);
    Q_INVOKABLE void setInfo(const QVariantMap& info);
    // To one peer, or to all of them
    Q_INVOKABLE void send(int peer, const QVariantMap& message);
    Q_INVOKABLE void sendAll(const QVariantMap& message);
    Q_INVOKABLE void kick(int peer);
    // A player who chose another name
    Q_INVOKABLE void renamePeer(int peer, const QString& name);

    // Opens a room on the server at serverUrl, like ws://example.com:45460
    Q_INVOKABLE void hostOnline(const QString& serverUrl, const QString& name, const QVariantMap& info);

    // url like ws://192.168.1.5:45455
    Q_INVOKABLE void join(const QString& url, const QString& name);
    // The room with code on the server
    Q_INVOKABLE void joinOnline(const QString& serverUrl, const QString& code, const QString& name);
    // Client to host
    Q_INVOKABLE void sendToHost(const QVariantMap& message);

    // Hosting or joined, both end
    Q_INVOKABLE void leave();

    Q_INVOKABLE void startBrowsing();
    Q_INVOKABLE void stopBrowsing();

    Role role() const;
    bool canHost() const;
    QVariantList peers() const;
    QStringList addresses() const;
    int port() const;
    QVariantList games() const;
    int clientId() const;
    QString error() const;
    QString machineName() const;
    QString token() const;
    void setPlayerName(const QString& playerName);
    QString playerName() const;
    QString localName() const;
    bool isOnline() const;
    QString roomCode() const;
    int latency() const;

    // ws:// if the url has no scheme, the port if it has none
    static QUrl serverUrl(const QString& url, quint16 port);

signals:
    void roleChanged();
    void peersChanged();
    void gamesChanged();
    void errorChanged();
    void nameChanged();
    void latencyChanged(int);

    // Host side
    void peerJoined(int peer, const QString& name, const QString& token);
    void peerLeft(int peer);
    void received(int peer, const QVariantMap& message);

    // Client side
    void joined();
    // The connection is gone, reason for the player
    void left(const QString& reason);
    void receivedFromHost(const QVariantMap& message);

private:
    struct Peer {
        int id;
        QString name;
        QString token;
        // A connection of its own on the LAN, or a peer of the room on
        // the server
        QPointer<QWebSocket> socket;
        int relayPeer;
    };

    void setRole(Role role);
    void setError(const QString& error);
    void announce();
    void readAnnouncements();
    void expireGames();
    void acceptConnection();
    void hostReceived(QWebSocket* socket, const QString& text);
    // A message from a peer, on the LAN or through the server
    void peerMessage(QWebSocket* socket, int relayPeer, const QVariantMap& message);
    void deliver(QWebSocket* socket, int relayPeer, const QVariantMap& message);
    void dropSocket(QWebSocket* socket);
    void dropPeer(qsizetype index);
    void relayReceived(const QString& text);
    void openClient(const QUrl& url, const QString& name, const QString& code);
    void ping();
    void clientReceived(const QString& text);
    static QString encode(const QVariantMap& message);
    static QVariantMap decode(const QString& text);

    Role m_role;
    QString m_error;
    QString m_token;
    QString m_playerName;

    // Host
    QWebSocketServer* m_server;
    QUdpSocket* m_announcer;
    QTimer m_announceTimer;
    QString m_name;
    QVariantMap m_info;
    QList<Peer> m_peers;
    QList<QWebSocket*> m_pending;
    int m_nextId;
    // Online: the connection to the server, the code of the room and the
    // peers of the room that haven't said hello yet
    QWebSocket* m_relay;
    QString m_roomCode;
    QList<int> m_pendingRelay;

    // Client
    QWebSocket* m_socket;
    int m_clientId;
    bool m_joinedOnline;
    QTimer m_pingTimer;
    int m_latency;

    // Browsing
    QUdpSocket* m_listener;
    QTimer m_expireTimer;
    struct Game {
        QString name;
        QString url;
        QVariantMap info;
        qint64 seen;
    };
    QList<Game> m_games;
};

#endif // LAN_H
