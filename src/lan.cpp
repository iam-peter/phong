#include "lan.h"

#include <QDateTime>
#include <QSettings>
#include <QUuid>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QSysInfo>
#include <QUrl>
#include <QWebSocket>

#if !defined(Q_OS_WASM)
#include <QNetworkInterface>
#include <QUdpSocket>
#include <QWebSocketServer>
#endif

Q_LOGGING_CATEGORY(lcLan, "phong.lan", QtWarningMsg)

namespace {
// Hosts not heard of for this long are gone
constexpr qint64 gameTimeout = 3500;
}

Lan::Lan(QObject* parent):
    QObject(parent),
    m_role(Role::NoRole),
    m_error(),
    m_token(),
    m_playerName(),
    m_server(nullptr),
    m_announcer(nullptr),
    m_announceTimer(),
    m_name(),
    m_info(),
    m_peers(),
    m_pending(),
    m_nextId(1),
    m_relay(nullptr),
    m_roomCode(),
    m_pendingRelay(),
    m_socket(nullptr),
    m_clientId(-1),
    m_joinedOnline(false),
    m_pingTimer(),
    m_latency(-1),
    m_listener(nullptr),
    m_expireTimer(),
    m_games()
{
    QSettings settings;
    m_token = settings.value(QStringLiteral("lan/token")).toString();
    if (m_token.isEmpty()) {
        m_token = QUuid::createUuid().toString(QUuid::WithoutBraces);
        settings.setValue(QStringLiteral("lan/token"), m_token);
    }

    m_announceTimer.setInterval(1000);
    connect(&m_announceTimer, &QTimer::timeout, this, &Lan::announce);
    m_expireTimer.setInterval(1000);
    m_pingTimer.setInterval(pingInterval);
    connect(&m_pingTimer, &QTimer::timeout, this, &Lan::ping);
    connect(&m_expireTimer, &QTimer::timeout, this, &Lan::expireGames);
}

Lan::~Lan()
{
    leave();
}

bool Lan::host(const QString& name, const QVariantMap& info, quint16 port)
{
#if defined(Q_OS_WASM)
    Q_UNUSED(name)
    Q_UNUSED(info)
    Q_UNUSED(port)
    setError(tr("A browser can't host a game"));
    return false;
#else
    leave();

    m_server = new QWebSocketServer(QStringLiteral("phong"), QWebSocketServer::NonSecureMode, this);
    // A few ports on, e.g. a second instance on the same machine
    bool listening = false;
    for (quint16 candidate = port; candidate < port + 10 && !listening; ++candidate)
        listening = m_server->listen(QHostAddress::Any, candidate);
    if (!listening) {
        setError(tr("Can't open a port: %1").arg(m_server->errorString()));
        delete m_server;
        m_server = nullptr;
        return false;
    }
    connect(m_server, &QWebSocketServer::newConnection, this, &Lan::acceptConnection);

    m_name = name;
    m_info = info;
    m_announcer = new QUdpSocket(this);
    m_announceTimer.start();
    setError(QString());
    setRole(Role::Host);
    announce();
    qCDebug(lcLan) << "Hosting on port" << m_server->serverPort();
    return true;
#endif
}

void Lan::setInfo(const QVariantMap& info)
{
    m_info = info;
}

void Lan::send(int peer, const QVariantMap& message)
{
    for (const Peer& p : std::as_const(m_peers)) {
        if (p.id == peer)
            deliver(p.socket, p.relayPeer, message);
    }
}

void Lan::sendAll(const QVariantMap& message)
{
    if (m_peers.isEmpty())
        return;

    // The server passes one message on to the whole room
    if (m_relay) {
        m_relay->sendTextMessage(encode({ { QStringLiteral("t"), QStringLiteral("all") },
                                          { QStringLiteral("m"), message } }));
        return;
    }

    const QString text = encode(message);
    for (const Peer& peer : std::as_const(m_peers)) {
        if (peer.socket)
            peer.socket->sendTextMessage(text);
    }
}

void Lan::kick(int peer)
{
    for (const Peer& p : std::as_const(m_peers)) {
        if (p.id != peer)
            continue;
        if (p.socket)
            p.socket->close();
        else if (m_relay)
            m_relay->sendTextMessage(encode({ { QStringLiteral("t"), QStringLiteral("close") },
                                              { QStringLiteral("peer"), p.relayPeer } }));
    }
}

void Lan::hostOnline(const QString& serverUrl, const QString& name, const QVariantMap& info)
{
    leave();

    const QUrl url = Lan::serverUrl(serverUrl, 45460);
    if (!url.isValid() || url.host().isEmpty()) {
        setError(tr("No server for games over the internet, see the settings"));
        return;
    }

    m_name = name;
    m_info = info;
    m_relay = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    connect(m_relay, &QWebSocket::connected, this, [this] {
        m_relay->sendTextMessage(encode({ { QStringLiteral("t"), QStringLiteral("create") },
                                          { QStringLiteral("version"), protocolVersion } }));
    });
    connect(m_relay, &QWebSocket::textMessageReceived, this, &Lan::relayReceived);
    connect(m_relay, &QWebSocket::disconnected, this, [this] {
        QWebSocket* relay = m_relay;
        if (!relay)
            return;
        if (m_error.isEmpty())
            setError(relay->error() != QAbstractSocket::UnknownSocketError ? relay->errorString()
                                                                           : tr("The connection to the server is gone"));
        // Everybody in the room is gone with it
        m_relay = nullptr;
        relay->deleteLater();
        leave();
    });
    setError(QString());
    setRole(Role::Joining);
    qCDebug(lcLan) << "Opening a room on" << url;
    m_relay->open(url);
}

void Lan::join(const QString& url, const QString& name)
{
    openClient(serverUrl(url, defaultPort), name, QString());
}

void Lan::joinOnline(const QString& serverUrl, const QString& code, const QString& name)
{
    const QUrl url = Lan::serverUrl(serverUrl, 45460);
    if (!url.isValid() || url.host().isEmpty()) {
        leave();
        setError(tr("No server for games over the internet, see the settings"));
        emit left(m_error);
        return;
    }
    openClient(url, name, code.trimmed().toUpper());
}

void Lan::openClient(const QUrl& target, const QString& name, const QString& code)
{
    leave();

    // Online the server first needs the room, the host then the hello
    m_joinedOnline = !code.isEmpty();
    m_socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    const QVariantMap hello{ { QStringLiteral("t"), QStringLiteral("hello") },
                             { QStringLiteral("name"), name },
                             { QStringLiteral("token"), m_token },
                             { QStringLiteral("version"), protocolVersion } };
    connect(m_socket, &QWebSocket::connected, this, [this, hello, code] {
        if (code.isEmpty())
            sendToHost(hello);
        else
            sendToHost({ { QStringLiteral("t"), QStringLiteral("join") }, { QStringLiteral("code"), code } });
    });
    connect(m_socket, &QWebSocket::textMessageReceived, this, [this, hello](const QString& text) {
        // The server found the room
        if (m_role == Role::Joining && m_joinedOnline
            && decode(text).value(QStringLiteral("t")).toString() == QLatin1String("joined")) {
            sendToHost(hello);
            return;
        }
        clientReceived(text);
    });
    connect(m_socket, &QWebSocket::disconnected, this, [this] {
        const bool wasJoined = m_role == Role::Client || m_role == Role::Joining;
        QWebSocket* socket = m_socket;
        m_socket = nullptr;
        m_pingTimer.stop();
        if (socket) {
            // The error may come after the disconnect
            if (m_error.isEmpty() && socket->error() != QAbstractSocket::UnknownSocketError)
                setError(socket->errorString());
            if (m_error.isEmpty() && !socket->closeReason().isEmpty())
                setError(socket->closeReason());
            socket->deleteLater();
        }
        m_clientId = -1;
        setRole(Role::NoRole);
        if (wasJoined)
            emit left(m_error.isEmpty() ? tr("The connection to the host is gone") : m_error);
    });
    connect(m_socket, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (m_socket)
            setError(m_socket->errorString());
    });

    setError(QString());
    setRole(Role::Joining);
    qCDebug(lcLan) << "Joining" << target << code;
    m_socket->open(target);
}

QUrl Lan::serverUrl(const QString& url, quint16 port)
{
    const QString trimmed = url.trimmed();
    if (trimmed.isEmpty())
        return QUrl();
    QUrl target(trimmed.contains(QLatin1String("://")) ? trimmed : QStringLiteral("ws://") + trimmed);
    if (target.scheme() == QLatin1String("http"))
        target.setScheme(QStringLiteral("ws"));
    else if (target.scheme() == QLatin1String("https"))
        target.setScheme(QStringLiteral("wss"));
    // wss usually goes through a proxy on the default port
    if (target.port() < 0 && target.scheme() == QLatin1String("ws"))
        target.setPort(port);
    return target;
}

void Lan::sendToHost(const QVariantMap& message)
{
    if (m_socket)
        m_socket->sendTextMessage(encode(message));
}

void Lan::leave()
{
#if !defined(Q_OS_WASM)
    m_announceTimer.stop();
    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
    if (m_announcer) {
        m_announcer->deleteLater();
        m_announcer = nullptr;
    }
#endif
    const QList<Peer> peers = m_peers;
    m_peers.clear();
    for (const Peer& peer : peers) {
        if (peer.socket) {
            peer.socket->disconnect(this);
            peer.socket->close();
            peer.socket->deleteLater();
        }
    }
    for (QWebSocket* socket : std::as_const(m_pending)) {
        socket->disconnect(this);
        socket->close();
        socket->deleteLater();
    }
    m_pending.clear();
    m_pendingRelay.clear();
    if (m_relay) {
        QWebSocket* relay = m_relay;
        m_relay = nullptr;
        relay->disconnect(this);
        relay->close();
        relay->deleteLater();
    }
    m_roomCode.clear();
    if (!peers.isEmpty())
        emit peersChanged();

    if (m_socket) {
        QWebSocket* socket = m_socket;
        m_socket = nullptr;
        socket->disconnect(this);
        socket->close();
        socket->deleteLater();
    }
    m_clientId = -1;
    m_joinedOnline = false;
    m_pingTimer.stop();
    if (m_latency != -1) {
        m_latency = -1;
        emit latencyChanged(m_latency);
    }
    setRole(Role::NoRole);
}

void Lan::startBrowsing()
{
#if !defined(Q_OS_WASM)
    if (m_listener)
        return;

    m_listener = new QUdpSocket(this);
    if (!m_listener->bind(QHostAddress::AnyIPv4, discoveryPort,
                          QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        qCWarning(lcLan) << "Can't listen for games:" << m_listener->errorString();
        delete m_listener;
        m_listener = nullptr;
        return;
    }
    connect(m_listener, &QUdpSocket::readyRead, this, &Lan::readAnnouncements);
    m_expireTimer.start();
#endif
}

void Lan::stopBrowsing()
{
#if !defined(Q_OS_WASM)
    m_expireTimer.stop();
    if (m_listener) {
        m_listener->deleteLater();
        m_listener = nullptr;
    }
#endif
    if (!m_games.isEmpty()) {
        m_games.clear();
        emit gamesChanged();
    }
}

Lan::Role Lan::role() const
{
    return m_role;
}

bool Lan::canHost() const
{
#if defined(Q_OS_WASM)
    return false;
#else
    return true;
#endif
}

QVariantList Lan::peers() const
{
    QVariantList peers;
    for (const Peer& peer : m_peers)
        peers.append(QVariantMap{ { QStringLiteral("id"), peer.id }, { QStringLiteral("name"), peer.name } });
    return peers;
}

QStringList Lan::addresses() const
{
    QStringList addresses;
#if !defined(Q_OS_WASM)
    if (m_role != Role::Host)
        return addresses;

    // Home networks first, then other private ones, VPNs and containers
    // add their own
    QList<QHostAddress> found;
    for (const QHostAddress& address : QNetworkInterface::allAddresses()) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback())
            found.append(address);
    }
    const auto rank = [](const QHostAddress& address) {
        if (address.isInSubnet(QHostAddress(QStringLiteral("192.168.0.0")), 16))
            return 0;
        if (address.isInSubnet(QHostAddress(QStringLiteral("10.0.0.0")), 8))
            return 1;
        if (address.isInSubnet(QHostAddress(QStringLiteral("172.16.0.0")), 12))
            return 2;
        return 3;
    };
    std::stable_sort(found.begin(), found.end(),
                     [&rank](const QHostAddress& a, const QHostAddress& b) { return rank(a) < rank(b); });
    for (const QHostAddress& address : std::as_const(found))
        addresses.append(QStringLiteral("%1:%2").arg(address.toString()).arg(port()));
#endif
    return addresses;
}

int Lan::port() const
{
#if defined(Q_OS_WASM)
    return 0;
#else
    return m_server ? m_server->serverPort() : 0;
#endif
}

QVariantList Lan::games() const
{
    QVariantList games;
    for (const Game& game : m_games) {
        games.append(QVariantMap{ { QStringLiteral("name"), game.name },
                                  { QStringLiteral("url"), game.url },
                                  { QStringLiteral("info"), game.info } });
    }
    return games;
}

int Lan::clientId() const
{
    return m_clientId;
}

QString Lan::error() const
{
    return m_error;
}

QString Lan::token() const
{
    return m_token;
}

void Lan::setPlayerName(const QString& playerName)
{
    const QString name = playerName.trimmed().left(12);
    if (m_playerName == name)
        return;

    m_playerName = name;
    emit nameChanged();
}

QString Lan::playerName() const
{
    return m_playerName;
}

QString Lan::localName() const
{
    if (!m_playerName.isEmpty())
        return m_playerName;
    return isOnline() ? tr("Player") : machineName();
}

bool Lan::isOnline() const
{
    return m_relay || m_joinedOnline;
}

QString Lan::roomCode() const
{
    return m_roomCode;
}

int Lan::latency() const
{
    return m_latency;
}

QString Lan::machineName() const
{
#if defined(Q_OS_WASM)
    return tr("Browser");
#else
    const QString name = QSysInfo::machineHostName();
    return name.isEmpty() ? tr("Player") : name.left(16);
#endif
}

void Lan::setRole(Role role)
{
    if (m_role == role)
        return;

    m_role = role;
    emit roleChanged();
}

void Lan::setError(const QString& error)
{
    if (m_error == error)
        return;

    m_error = error;
    emit errorChanged();
}

void Lan::announce()
{
#if !defined(Q_OS_WASM)
    if (!m_announcer || !m_server)
        return;

    const QJsonObject announcement{ { QStringLiteral("phong"), protocolVersion },
                                    { QStringLiteral("name"), m_name },
                                    { QStringLiteral("port"), int(m_server->serverPort()) },
                                    { QStringLiteral("info"), QJsonObject::fromVariantMap(m_info) } };
    const QByteArray datagram = QJsonDocument(announcement).toJson(QJsonDocument::Compact);
    m_announcer->writeDatagram(datagram, QHostAddress::Broadcast, discoveryPort);
    // Loopback too, the broadcast doesn't reach this machine everywhere
    m_announcer->writeDatagram(datagram, QHostAddress::LocalHost, discoveryPort);
#endif
}

void Lan::readAnnouncements()
{
#if !defined(Q_OS_WASM)
    bool changed = false;
    while (m_listener && m_listener->hasPendingDatagrams()) {
        QByteArray datagram(int(m_listener->pendingDatagramSize()), Qt::Uninitialized);
        QHostAddress sender;
        m_listener->readDatagram(datagram.data(), datagram.size(), &sender);

        const QJsonObject announcement = QJsonDocument::fromJson(datagram).object();
        if (announcement.value(QStringLiteral("phong")).toInt() != protocolVersion)
            continue;

        bool ok = false;
        const QHostAddress ipv4(sender.toIPv4Address(&ok));
        const QString host = ok ? ipv4.toString() : sender.toString();
        const QString url = QStringLiteral("ws://%1:%2").arg(host).arg(announcement.value(QStringLiteral("port")).toInt());
        const Game game{ announcement.value(QStringLiteral("name")).toString(), url,
                         announcement.value(QStringLiteral("info")).toObject().toVariantMap(),
                         QDateTime::currentMSecsSinceEpoch() };

        // The same game over loopback and the network is one game
        const auto same = std::find_if(m_games.begin(), m_games.end(), [&](const Game& other) {
            return other.url == url
                   || (other.name == game.name && other.url.section(QLatin1Char(':'), -1) == url.section(QLatin1Char(':'), -1)
                       && (sender.isLoopback() || other.url.contains(QLatin1String("127.0.0.1"))));
        });
        if (same == m_games.end()) {
            m_games.append(game);
            changed = true;
        }
        else {
            // Prefer the network address, it works from other machines too
            const bool better = same->url.contains(QLatin1String("127.0.0.1")) && !sender.isLoopback();
            changed = changed || same->info != game.info || better;
            if (better)
                same->url = url;
            same->info = game.info;
            same->seen = game.seen;
        }
    }
    if (changed)
        emit gamesChanged();
#endif
}

void Lan::expireGames()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qsizetype before = m_games.size();
    m_games.removeIf([now](const Game& game) { return now - game.seen > gameTimeout; });
    if (m_games.size() != before)
        emit gamesChanged();
}

void Lan::acceptConnection()
{
#if !defined(Q_OS_WASM)
    while (m_server && m_server->hasPendingConnections()) {
        QWebSocket* socket = m_server->nextPendingConnection();
        m_pending.append(socket);
        connect(socket, &QWebSocket::textMessageReceived, this,
                [this, socket](const QString& text) { hostReceived(socket, text); });
        connect(socket, &QWebSocket::disconnected, this, [this, socket] { dropSocket(socket); });
    }
#endif
}

void Lan::hostReceived(QWebSocket* socket, const QString& text)
{
    peerMessage(socket, 0, decode(text));
}

void Lan::peerMessage(QWebSocket* socket, int relayPeer, const QVariantMap& message)
{
    const QString type = message.value(QStringLiteral("t")).toString();

    // The first message says who it is
    const bool pending = socket ? m_pending.contains(socket) : m_pendingRelay.contains(relayPeer);
    if (pending) {
        if (type != QLatin1String("hello"))
            return;
        m_pending.removeAll(socket);
        m_pendingRelay.removeAll(relayPeer);

        if (message.value(QStringLiteral("version")).toInt() != protocolVersion) {
            deliver(socket, relayPeer, { { QStringLiteral("t"), QStringLiteral("refused") },
                                         { QStringLiteral("reason"), tr("The game versions differ") } });
            if (socket)
                socket->close();
            else if (m_relay)
                m_relay->sendTextMessage(encode({ { QStringLiteral("t"), QStringLiteral("close") },
                                                  { QStringLiteral("peer"), relayPeer } }));
            return;
        }

        const Peer peer{ m_nextId++, message.value(QStringLiteral("name")).toString().left(16),
                         message.value(QStringLiteral("token")).toString().left(64), socket, relayPeer };
        m_peers.append(peer);
        deliver(socket, relayPeer, { { QStringLiteral("t"), QStringLiteral("welcome") },
                                     { QStringLiteral("id"), peer.id } });
        emit peersChanged();
        emit peerJoined(peer.id, peer.name, peer.token);
        return;
    }

    // The latency is measured here, the game needn't know
    if (type == QLatin1String("ping")) {
        deliver(socket, relayPeer, { { QStringLiteral("t"), QStringLiteral("pong") },
                                     { QStringLiteral("c"), message.value(QStringLiteral("c")) } });
        return;
    }

    for (const Peer& peer : std::as_const(m_peers)) {
        if (socket ? peer.socket == socket : (!peer.socket && peer.relayPeer == relayPeer)) {
            emit received(peer.id, message);
            return;
        }
    }
}

void Lan::deliver(QWebSocket* socket, int relayPeer, const QVariantMap& message)
{
    if (socket) {
        socket->sendTextMessage(encode(message));
    }
    else if (m_relay) {
        m_relay->sendTextMessage(encode({ { QStringLiteral("t"), QStringLiteral("to") },
                                          { QStringLiteral("peer"), relayPeer },
                                          { QStringLiteral("m"), message } }));
    }
}

void Lan::dropSocket(QWebSocket* socket)
{
    m_pending.removeAll(socket);
    for (qsizetype i = 0; i < m_peers.size(); ++i) {
        if (m_peers.at(i).socket == socket) {
            dropPeer(i);
            break;
        }
    }
    socket->deleteLater();
}

void Lan::dropPeer(qsizetype index)
{
    const int id = m_peers.at(index).id;
    m_peers.removeAt(index);
    emit peersChanged();
    emit peerLeft(id);
}

void Lan::relayReceived(const QString& text)
{
    const QVariantMap message = decode(text);
    const QString type = message.value(QStringLiteral("t")).toString();
    const int relayPeer = message.value(QStringLiteral("peer")).toInt();

    if (type == QLatin1String("room")) {
        m_roomCode = message.value(QStringLiteral("code")).toString();
        setRole(Role::Host);
        qCDebug(lcLan) << "Hosting room" << m_roomCode;
    }
    else if (type == QLatin1String("error")) {
        setError(message.value(QStringLiteral("reason")).toString());
    }
    else if (type == QLatin1String("open")) {
        m_pendingRelay.append(relayPeer);
    }
    else if (type == QLatin1String("from")) {
        peerMessage(nullptr, relayPeer, message.value(QStringLiteral("m")).toMap());
    }
    else if (type == QLatin1String("gone")) {
        m_pendingRelay.removeAll(relayPeer);
        for (qsizetype i = 0; i < m_peers.size(); ++i) {
            if (!m_peers.at(i).socket && m_peers.at(i).relayPeer == relayPeer) {
                dropPeer(i);
                break;
            }
        }
    }
}

void Lan::ping()
{
    sendToHost({ { QStringLiteral("t"), QStringLiteral("ping") },
                 { QStringLiteral("c"), QDateTime::currentMSecsSinceEpoch() } });
}

void Lan::clientReceived(const QString& text)
{
    const QVariantMap message = decode(text);
    const QString type = message.value(QStringLiteral("t")).toString();

    if (type == QLatin1String("welcome")) {
        m_clientId = message.value(QStringLiteral("id")).toInt();
        setRole(Role::Client);
        m_pingTimer.start();
        ping();
        emit joined();
        return;
    }
    if (type == QLatin1String("refused") || type == QLatin1String("error")) {
        setError(message.value(QStringLiteral("reason")).toString());
        if (m_socket)
            m_socket->close();
        return;
    }
    if (type == QLatin1String("pong")) {
        const qint64 sent = message.value(QStringLiteral("c")).toLongLong();
        const int measured = int(std::clamp<qint64>(QDateTime::currentMSecsSinceEpoch() - sent, 0, 10000));
        // Smoothed, a single slow answer doesn't count much
        const int latency = m_latency < 0 ? measured : (3 * m_latency + measured) / 4;
        if (latency != m_latency) {
            m_latency = latency;
            emit latencyChanged(latency);
        }
        return;
    }

    emit receivedFromHost(message);
}

QString Lan::encode(const QVariantMap& message)
{
    return QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(message)).toJson(QJsonDocument::Compact));
}

QVariantMap Lan::decode(const QString& text)
{
    return QJsonDocument::fromJson(text.toUtf8()).object().toVariantMap();
}
