#ifndef LAN_H
#define LAN_H

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QTimer>
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
    static constexpr int protocolVersion = 1;

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

    // url like ws://192.168.1.5:45455
    Q_INVOKABLE void join(const QString& url, const QString& name);
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

signals:
    void roleChanged();
    void peersChanged();
    void gamesChanged();
    void errorChanged();

    // Host side
    void peerJoined(int peer, const QString& name);
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
        QPointer<QWebSocket> socket;
    };

    void setRole(Role role);
    void setError(const QString& error);
    void announce();
    void readAnnouncements();
    void expireGames();
    void acceptConnection();
    void hostReceived(QWebSocket* socket, const QString& text);
    void dropSocket(QWebSocket* socket);
    void clientReceived(const QString& text);
    static QString encode(const QVariantMap& message);
    static QVariantMap decode(const QString& text);

    Role m_role;
    QString m_error;

    // Host
    QWebSocketServer* m_server;
    QUdpSocket* m_announcer;
    QTimer m_announceTimer;
    QString m_name;
    QVariantMap m_info;
    QList<Peer> m_peers;
    QList<QWebSocket*> m_pending;
    int m_nextId;

    // Client
    QWebSocket* m_socket;
    int m_clientId;

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
