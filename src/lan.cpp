#include "lan.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QNetworkInterface>
#include <QSysInfo>
#include <QUrl>
#include <QWebSocket>

#if !defined(Q_OS_WASM)
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
    m_server(nullptr),
    m_announcer(nullptr),
    m_announceTimer(),
    m_name(),
    m_info(),
    m_peers(),
    m_pending(),
    m_nextId(1),
    m_socket(nullptr),
    m_clientId(-1),
    m_listener(nullptr),
    m_expireTimer(),
    m_games()
{
    m_announceTimer.setInterval(1000);
    connect(&m_announceTimer, &QTimer::timeout, this, &Lan::announce);
    m_expireTimer.setInterval(1000);
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
    const QString text = encode(message);
    for (const Peer& p : std::as_const(m_peers)) {
        if (p.id == peer && p.socket)
            p.socket->sendTextMessage(text);
    }
}

void Lan::sendAll(const QVariantMap& message)
{
    if (m_peers.isEmpty())
        return;

    const QString text = encode(message);
    for (const Peer& peer : std::as_const(m_peers)) {
        if (peer.socket)
            peer.socket->sendTextMessage(text);
    }
}

void Lan::kick(int peer)
{
    for (const Peer& p : std::as_const(m_peers)) {
        if (p.id == peer && p.socket)
            p.socket->close();
    }
}

void Lan::join(const QString& url, const QString& name)
{
    leave();

    QUrl target = QUrl::fromUserInput(url.contains(QLatin1String("://")) ? url : QStringLiteral("ws://") + url);
    target.setScheme(QStringLiteral("ws"));
    if (target.port() < 0)
        target.setPort(defaultPort);

    m_socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    connect(m_socket, &QWebSocket::connected, this, [this, name] {
        sendToHost({ { QStringLiteral("t"), QStringLiteral("hello") },
                     { QStringLiteral("name"), name },
                     { QStringLiteral("version"), protocolVersion } });
    });
    connect(m_socket, &QWebSocket::textMessageReceived, this, &Lan::clientReceived);
    connect(m_socket, &QWebSocket::disconnected, this, [this] {
        const bool wasJoined = m_role == Role::Client || m_role == Role::Joining;
        QWebSocket* socket = m_socket;
        m_socket = nullptr;
        if (socket) {
            // The error may come after the disconnect
            if (m_error.isEmpty() && socket->error() != QAbstractSocket::UnknownSocketError)
                setError(socket->errorString());
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
    qCDebug(lcLan) << "Joining" << target;
    m_socket->open(target);
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
    const QVariantMap message = decode(text);

    // The first message says who it is
    if (m_pending.contains(socket)) {
        if (message.value(QStringLiteral("t")).toString() != QLatin1String("hello"))
            return;
        m_pending.removeAll(socket);

        if (message.value(QStringLiteral("version")).toInt() != protocolVersion) {
            socket->sendTextMessage(encode({ { QStringLiteral("t"), QStringLiteral("refused") },
                                             { QStringLiteral("reason"), tr("The game versions differ") } }));
            socket->close();
            return;
        }

        const Peer peer{ m_nextId++, message.value(QStringLiteral("name")).toString().left(16), socket };
        m_peers.append(peer);
        socket->sendTextMessage(encode({ { QStringLiteral("t"), QStringLiteral("welcome") },
                                         { QStringLiteral("id"), peer.id } }));
        emit peersChanged();
        emit peerJoined(peer.id, peer.name);
        return;
    }

    for (const Peer& peer : std::as_const(m_peers)) {
        if (peer.socket == socket) {
            emit received(peer.id, message);
            return;
        }
    }
}

void Lan::dropSocket(QWebSocket* socket)
{
    m_pending.removeAll(socket);
    for (qsizetype i = 0; i < m_peers.size(); ++i) {
        if (m_peers.at(i).socket == socket) {
            const int id = m_peers.at(i).id;
            m_peers.removeAt(i);
            emit peersChanged();
            emit peerLeft(id);
            break;
        }
    }
    socket->deleteLater();
}

void Lan::clientReceived(const QString& text)
{
    const QVariantMap message = decode(text);
    const QString type = message.value(QStringLiteral("t")).toString();

    if (type == QLatin1String("welcome")) {
        m_clientId = message.value(QStringLiteral("id")).toInt();
        setRole(Role::Client);
        emit joined();
        return;
    }
    if (type == QLatin1String("refused")) {
        setError(message.value(QStringLiteral("reason")).toString());
        if (m_socket)
            m_socket->close();
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
