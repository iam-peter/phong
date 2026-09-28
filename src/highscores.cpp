#include "highscores.h"

#include "lan.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QWebSocket>

HighScores::HighScores(QObject* parent):
    QObject(parent),
    m_serverUrl(),
    m_endless(),
    m_squash(),
    m_socket(nullptr),
    m_timeout(),
    m_error()
{
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(timeout);
    connect(&m_timeout, &QTimer::timeout, this, [this] { finish(tr("The server doesn't answer")); });
}

HighScores::~HighScores()
{
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
    }
}

void HighScores::submit(const QString& board, const QString& name, int score)
{
    request({ { QStringLiteral("t"), QStringLiteral("score") },
              { QStringLiteral("board"), board },
              { QStringLiteral("name"), name.trimmed().left(12) },
              { QStringLiteral("score"), score } });
}

void HighScores::refresh(const QString& board)
{
    request({ { QStringLiteral("t"), QStringLiteral("scores") }, { QStringLiteral("board"), board } });
}

int HighScores::place(const QString& board, const QString& name, int score) const
{
    const QVariantList& list = board == QLatin1String("squash") ? m_squash : m_endless;
    for (qsizetype i = 0; i < list.size(); ++i) {
        const QVariantMap entry = list.at(i).toMap();
        if (entry.value(QStringLiteral("name")).toString() == name.trimmed()
            && entry.value(QStringLiteral("score")).toInt() == score)
            return int(i);
    }
    return -1;
}

void HighScores::setServerUrl(const QString& serverUrl)
{
    if (m_serverUrl == serverUrl)
        return;

    m_serverUrl = serverUrl;
    emit serverUrlChanged();
}

QString HighScores::serverUrl() const
{
    return m_serverUrl;
}

bool HighScores::isAvailable() const
{
    return !m_serverUrl.trimmed().isEmpty();
}

QVariantList HighScores::endless() const
{
    return m_endless;
}

QVariantList HighScores::squash() const
{
    return m_squash;
}

bool HighScores::isBusy() const
{
    return m_socket;
}

QString HighScores::error() const
{
    return m_error;
}

void HighScores::request(const QVariantMap& message)
{
    const QUrl url = Lan::serverUrl(m_serverUrl, 45460);
    if (!url.isValid() || url.host().isEmpty())
        return;

    // The latest request wins
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
        m_socket->deleteLater();
    }

    QWebSocket* socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    m_socket = socket;
    const QString text =
        QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(message)).toJson(QJsonDocument::Compact));
    connect(socket, &QWebSocket::connected, this, [socket, text] { socket->sendTextMessage(text); });
    connect(socket, &QWebSocket::textMessageReceived, this, [this](const QString& answer) {
        const QVariantMap scores = QJsonDocument::fromJson(answer.toUtf8()).object().toVariantMap();
        if (scores.value(QStringLiteral("t")).toString() != QLatin1String("scores"))
            return;
        const QString board = scores.value(QStringLiteral("board")).toString();
        const QVariantList list = scores.value(QStringLiteral("list")).toList();
        if (board == QLatin1String("endless"))
            m_endless = list;
        else if (board == QLatin1String("squash"))
            m_squash = list;
        emit scoresChanged();
        finish(QString());
    });
    connect(socket, &QWebSocket::disconnected, this, [this, socket] {
        if (socket == m_socket)
            finish(socket->errorString().isEmpty() ? tr("The connection to the server is gone") : socket->errorString());
    });
    m_timeout.start();
    emit busyChanged(true);
    socket->open(url);
}

void HighScores::finish(const QString& error)
{
    m_timeout.stop();
    if (m_socket) {
        QWebSocket* socket = m_socket;
        m_socket = nullptr;
        socket->disconnect(this);
        socket->close();
        socket->deleteLater();
        emit busyChanged(false);
    }
    setError(error);
}

void HighScores::setError(const QString& error)
{
    if (m_error == error)
        return;

    m_error = error;
    emit errorChanged();
}
