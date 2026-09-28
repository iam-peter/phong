#include "relayserver.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QWebSocket>
#include <QWebSocketServer>

#include <algorithm>

Q_LOGGING_CATEGORY(lcRelay, "phong.server")

namespace {
// No 0 and O, no 1 and I
const QString codeLetters = QStringLiteral("ABCDEFGHJKLMNPQRSTUVWXYZ");

QVariantMap decode(const QString& text)
{
    return QJsonDocument::fromJson(text.toUtf8()).object().toVariantMap();
}

QString encode(const QVariantMap& message)
{
    return QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(message)).toJson(QJsonDocument::Compact));
}

// Letters, digits and a few signs, no control characters
QString cleanName(const QString& name)
{
    QString clean;
    for (const QChar c : name.trimmed()) {
        if (c.isLetterOrNumber() || c == QLatin1Char(' ') || c == QLatin1Char('-') || c == QLatin1Char('_')
            || c == QLatin1Char('.'))
            clean.append(c);
        if (clean.size() >= RelayServer::maxNameLength)
            break;
    }
    return clean.trimmed();
}
}

RelayServer::RelayServer(const QString& dataFile, QObject* parent):
    QObject(parent),
    m_server(new QWebSocketServer(QStringLiteral("phong"), QWebSocketServer::NonSecureMode, this)),
    m_dataFile(dataFile),
    m_rooms(),
    m_places(),
    m_scores(),
    m_keepAlive(),
    m_lag(0),
    m_random(QRandomGenerator::global()->generate())
{
    connect(m_server, &QWebSocketServer::newConnection, this, &RelayServer::accept);
    loadScores();

    // Proxies close quiet connections, e.g. a lobby waiting for players
    m_keepAlive.setInterval(20000);
    connect(&m_keepAlive, &QTimer::timeout, this, [this] {
        for (auto it = m_places.cbegin(); it != m_places.cend(); ++it)
            it.key()->ping();
    });
    m_keepAlive.start();
}

RelayServer::~RelayServer()
{
    m_server->close();
}

bool RelayServer::listen(quint16 port)
{
    if (!m_server->listen(QHostAddress::Any, port)) {
        qCWarning(lcRelay) << "Can't listen on port" << port << m_server->errorString();
        return false;
    }
    return true;
}

quint16 RelayServer::port() const
{
    return m_server->serverPort();
}

int RelayServer::roomCount() const
{
    return int(m_rooms.size());
}

QVariantList RelayServer::scores(const QString& board) const
{
    return m_scores.value(board);
}

bool RelayServer::isBoard(const QString& board)
{
    return board == QLatin1String("endless") || board == QLatin1String("squash");
}

void RelayServer::setSeed(quint32 seed)
{
    m_random.seed(seed);
}

void RelayServer::setLag(int ms)
{
    m_lag = std::max(ms, 0);
}

void RelayServer::pass(QWebSocket* socket, const QString& text)
{
    if (!socket)
        return;
    if (m_lag <= 0) {
        socket->sendTextMessage(text);
        return;
    }
    // Timers of the same length go off in order, so do the messages
    QPointer<QWebSocket> target(socket);
    QTimer::singleShot(m_lag, this, [target, text] {
        if (target)
            target->sendTextMessage(text);
    });
}

void RelayServer::accept()
{
    while (QWebSocket* socket = m_server->nextPendingConnection()) {
        socket->setMaxAllowedIncomingMessageSize(maxMessageSize);
        socket->setParent(this);
        connect(socket, &QWebSocket::textMessageReceived, this,
                [this, socket](const QString& text) { received(socket, text); });
        connect(socket, &QWebSocket::disconnected, this, [this, socket] { closed(socket); });
    }
}

void RelayServer::received(QWebSocket* socket, const QString& text)
{
    const QVariantMap message = decode(text);
    const QString type = message.value(QStringLiteral("t")).toString();

    // In a room the messages go through
    const auto place = m_places.constFind(socket);
    if (place != m_places.cend()) {
        const auto room = m_rooms.find(place->code);
        if (room == m_rooms.end())
            return;
        if (place->peer == 0) {
            fromHost(*room, message);
        }
        else if (room->host) {
            pass(room->host, encode({ { QStringLiteral("t"), QStringLiteral("from") },
                                      { QStringLiteral("peer"), place->peer },
                                      { QStringLiteral("m"), message } }));
        }
        return;
    }

    if (type == QLatin1String("create"))
        create(socket);
    else if (type == QLatin1String("join"))
        join(socket, message.value(QStringLiteral("code")).toString().trimmed().toUpper());
    else if (type == QLatin1String("score"))
        addScore(socket, message);
    else if (type == QLatin1String("scores"))
        sendScores(socket, message.value(QStringLiteral("board")).toString());
}

void RelayServer::closed(QWebSocket* socket)
{
    const auto place = m_places.constFind(socket);
    if (place != m_places.cend()) {
        const QString code = place->code;
        const int peer = place->peer;
        m_places.erase(place);

        const auto room = m_rooms.find(code);
        if (room != m_rooms.end()) {
            if (peer == 0) {
                // The host is gone and the game with it
                const QList<QPointer<QWebSocket>> peers = room->peers.values();
                m_rooms.erase(room);
                for (const QPointer<QWebSocket>& other : peers) {
                    if (other) {
                        m_places.remove(other);
                        other->close(QWebSocketProtocol::CloseCodeNormal, QStringLiteral("The host is gone"));
                    }
                }
                qCInfo(lcRelay) << "Room" << code << "closed," << m_rooms.size() << "open";
            }
            else {
                room->peers.remove(peer);
                if (room->host)
                    send(room->host, { { QStringLiteral("t"), QStringLiteral("gone") },
                                       { QStringLiteral("peer"), peer } });
            }
        }
    }
    socket->deleteLater();
}

void RelayServer::create(QWebSocket* socket)
{
    if (m_rooms.size() >= maxRooms) {
        send(socket, { { QStringLiteral("t"), QStringLiteral("error") },
                       { QStringLiteral("reason"), tr("The server is full") } });
        socket->close();
        return;
    }

    Room room;
    room.code = newCode();
    room.host = socket;
    m_rooms.insert(room.code, room);
    m_places.insert(socket, { room.code, 0 });
    send(socket, { { QStringLiteral("t"), QStringLiteral("room") }, { QStringLiteral("code"), room.code } });
    qCInfo(lcRelay) << "Room" << room.code << "opened," << m_rooms.size() << "open";
}

void RelayServer::join(QWebSocket* socket, const QString& code)
{
    const auto room = m_rooms.find(code);
    if (room == m_rooms.end() || !room->host) {
        send(socket, { { QStringLiteral("t"), QStringLiteral("error") },
                       { QStringLiteral("reason"), tr("There is no game with that code") } });
        socket->close();
        return;
    }
    if (room->peers.size() >= maxPeers) {
        send(socket, { { QStringLiteral("t"), QStringLiteral("error") },
                       { QStringLiteral("reason"), tr("The game is full") } });
        socket->close();
        return;
    }

    const int peer = room->nextPeer++;
    room->peers.insert(peer, socket);
    m_places.insert(socket, { code, peer });
    send(socket, { { QStringLiteral("t"), QStringLiteral("joined") } });
    send(room->host, { { QStringLiteral("t"), QStringLiteral("open") }, { QStringLiteral("peer"), peer } });
}

void RelayServer::fromHost(Room& room, const QVariantMap& message)
{
    const QString type = message.value(QStringLiteral("t")).toString();
    const QVariantMap inner = message.value(QStringLiteral("m")).toMap();

    if (type == QLatin1String("all")) {
        const QString text = encode(inner);
        for (const QPointer<QWebSocket>& peer : std::as_const(room.peers))
            pass(peer, text);
        return;
    }

    QWebSocket* peer = room.peers.value(message.value(QStringLiteral("peer")).toInt());
    if (!peer)
        return;
    if (type == QLatin1String("to"))
        pass(peer, encode(inner));
    else if (type == QLatin1String("close"))
        peer->close();
}

void RelayServer::addScore(QWebSocket* socket, const QVariantMap& message)
{
    const QString board = message.value(QStringLiteral("board")).toString();
    const QString name = cleanName(message.value(QStringLiteral("name")).toString());
    bool valid = false;
    const int score = message.value(QStringLiteral("score")).toInt(&valid);
    if (!isBoard(board) || name.isEmpty() || !valid || score <= 0 || score > maxScore) {
        sendScores(socket, board);
        return;
    }

    // Highest first, the older one first among equals
    QVariantList& list = m_scores[board];
    const auto at = std::find_if(list.begin(), list.end(), [score](const QVariant& entry) {
        return entry.toMap().value(QStringLiteral("score")).toInt() < score;
    });
    if (at - list.begin() < maxScores) {
        list.insert(at, QVariantMap{ { QStringLiteral("name"), name }, { QStringLiteral("score"), score } });
        while (list.size() > maxScores)
            list.removeLast();
        saveScores();
    }
    sendScores(socket, board);
}

void RelayServer::sendScores(QWebSocket* socket, const QString& board)
{
    send(socket, { { QStringLiteral("t"), QStringLiteral("scores") },
                   { QStringLiteral("board"), board },
                   { QStringLiteral("list"), isBoard(board) ? m_scores.value(board) : QVariantList() } });
}

QString RelayServer::newCode()
{
    QString code;
    do {
        code.clear();
        for (int i = 0; i < codeLength; ++i)
            code.append(codeLetters.at(m_random.bounded(int(codeLetters.size()))));
    } while (m_rooms.contains(code));
    return code;
}

void RelayServer::loadScores()
{
    if (m_dataFile.isEmpty())
        return;

    QFile file(m_dataFile);
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QVariantMap boards = QJsonDocument::fromJson(file.readAll()).object().toVariantMap();
    for (auto it = boards.cbegin(); it != boards.cend(); ++it) {
        if (isBoard(it.key()))
            m_scores.insert(it.key(), it.value().toList().mid(0, maxScores));
    }
}

void RelayServer::saveScores() const
{
    if (m_dataFile.isEmpty())
        return;

    QVariantMap boards;
    for (auto it = m_scores.cbegin(); it != m_scores.cend(); ++it)
        boards.insert(it.key(), it.value());

    QSaveFile file(m_dataFile);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(lcRelay) << "Can't write" << m_dataFile;
        return;
    }
    file.write(QJsonDocument(QJsonObject::fromVariantMap(boards)).toJson());
    file.commit();
}

void RelayServer::send(QWebSocket* socket, const QVariantMap& message)
{
    if (socket)
        socket->sendTextMessage(encode(message));
}
