#include "onlineservice.h"

#include "lan.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QWebSocket>

Q_LOGGING_CATEGORY(lcOnline, "phong.online")

OnlineService::OnlineService(QObject* parent):
    QObject(parent),
    m_settings(),
    m_network(new QNetworkAccessManager(this)),
    m_reply(),
    m_directory(defaultDirectory()),
    m_enabled(true),
    m_customServer(),
    m_publishedServer(m_settings.value(QStringLiteral("online/published"), defaultServer()).toString()),
    m_probe(),
    m_probed(),
    m_checkTime(),
    m_lastAnswer(),
    m_tick(),
    m_state(State::Unknown)
{
    m_network->setTransferTimeout(timeout);

    // The seconds of a check count up
    m_tick.setInterval(1000);
    connect(&m_tick, &QTimer::timeout, this, [this] {
        if (m_checkTime.elapsed() >= wakeTimeout) {
            finishCheck(false);
            return;
        }
        if (m_state == State::Checking && m_checkTime.elapsed() >= wakingAfter)
            setState(State::Waking);
        else
            emit stateChanged();
    });

    // Another server is another question
    connect(this, &OnlineService::serverChanged, this, [this] {
        if (m_probed != server()) {
            if (m_probe)
                finishCheck(false);
            m_lastAnswer.invalidate();
            setState(State::Unknown);
        }
    });
    refresh();
}

QUrl OnlineService::defaultDirectory()
{
#if defined(PHONG_DIRECTORY_URL)
    return QUrl(QStringLiteral(PHONG_DIRECTORY_URL));
#else
    return QUrl();
#endif
}

QString OnlineService::defaultServer()
{
#if defined(PHONG_SERVER_URL)
    return QStringLiteral(PHONG_SERVER_URL);
#else
    return QString();
#endif
}

void OnlineService::refresh()
{
    if (m_reply || !m_directory.isValid() || m_directory.isEmpty())
        return;

    QNetworkRequest request(m_directory);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    // Always the current one, not a cached copy, also not one of the
    // CDN in front of GitHub's raw files
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setRawHeader("Cache-Control", "no-cache");
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::finished, this, [this, reply = m_reply.data()] { answered(reply); });
    emit busyChanged(true);
}

void OnlineService::check()
{
    const QString server = this->server();
    if (server.isEmpty() || m_probe)
        return;
    if (m_state == State::Ready && m_lastAnswer.isValid() && m_lastAnswer.elapsed() < freshFor)
        return;

    const QUrl url = Lan::serverUrl(server, 45460);
    if (!url.isValid() || url.host().isEmpty()) {
        setState(State::Unreachable);
        return;
    }

    m_probed = server;
    QWebSocket* probe = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    m_probe = probe;
    connect(probe, &QWebSocket::connected, this, [probe] {
        probe->sendTextMessage(QStringLiteral("{\"t\":\"scores\",\"board\":\"endless\"}"));
    });
    // Any answer means it's up
    connect(probe, &QWebSocket::textMessageReceived, this, [this, probe] {
        if (probe == m_probe)
            finishCheck(true);
    });
    connect(probe, &QWebSocket::disconnected, this, [this, probe] {
        if (probe == m_probe)
            finishCheck(false);
    });
    m_checkTime.start();
    m_tick.start();
    setState(State::Checking);
    probe->open(url);
}

void OnlineService::finishCheck(bool ready)
{
    m_tick.stop();
    const qint64 took = m_checkTime.isValid() ? m_checkTime.elapsed() : 0;
    if (m_probe) {
        QWebSocket* probe = m_probe;
        m_probe = nullptr;
        probe->disconnect(this);
        probe->close();
        probe->deleteLater();
    }

    if (ready) {
        m_lastAnswer.start();
        // Remembered for the next time it sleeps
        if (took >= wakingAfter)
            m_settings.setValue(QStringLiteral("online/wake/") + m_probed, int((took + 500) / 1000));
    }
    setState(ready ? State::Ready : State::Unreachable);
}

void OnlineService::setDirectory(const QUrl& directory)
{
    m_directory = directory;
}

QUrl OnlineService::directory() const
{
    return m_directory;
}

void OnlineService::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;

    m_enabled = enabled;
    emit serverChanged();
}

bool OnlineService::isEnabled() const
{
    return m_enabled;
}

void OnlineService::setCustomServer(const QString& customServer)
{
    const QString server = customServer.trimmed();
    if (m_customServer == server)
        return;

    m_customServer = server;
    emit serverChanged();
}

QString OnlineService::customServer() const
{
    return m_customServer;
}

QString OnlineService::publishedServer() const
{
    return m_publishedServer;
}

QString OnlineService::server() const
{
    if (!m_enabled)
        return QString();
    return m_customServer.isEmpty() ? m_publishedServer : m_customServer;
}

bool OnlineService::isAvailable() const
{
    return !server().isEmpty();
}

bool OnlineService::isBusy() const
{
    return m_reply;
}

OnlineService::State OnlineService::state() const
{
    return m_state;
}

int OnlineService::waited() const
{
    return (m_state == State::Checking || m_state == State::Waking) && m_checkTime.isValid()
               ? int(m_checkTime.elapsed() / 1000) : 0;
}

int OnlineService::lastWake() const
{
    const QString server = m_probe ? m_probed : this->server();
    return m_settings.value(QStringLiteral("online/wake/") + server, 0).toInt();
}

QString OnlineService::status() const
{
    switch (m_state) {
        case State::Waking:
            return lastWake() > 0 ? tr("Waking up the server, %1 s, last time it took %2 s").arg(waited()).arg(lastWake())
                                  : tr("Waking up the server, %1 s, that can take a minute").arg(waited());
        case State::Unreachable:
            return tr("The server doesn't answer");
        default:
            return QString();
    }
}

void OnlineService::setState(State state)
{
    if (m_state == state)
        return;

    m_state = state;
    emit stateChanged();
}

void OnlineService::answered(QNetworkReply* reply)
{
    reply->deleteLater();
    m_reply = nullptr;
    emit busyChanged(false);

    // Without an answer the last one stays
    if (reply->error() != QNetworkReply::NoError) {
        qCInfo(lcOnline) << "No directory at" << m_directory << reply->errorString();
        return;
    }
    const QJsonObject directory = QJsonDocument::fromJson(reply->readAll()).object();
    if (!directory.contains(QStringLiteral("server"))) {
        qCInfo(lcOnline) << "The directory at" << m_directory << "names no server";
        return;
    }
    setPublishedServer(directory.value(QStringLiteral("server")).toString().trimmed());
}

void OnlineService::setPublishedServer(const QString& server)
{
    if (m_publishedServer == server)
        return;

    m_publishedServer = server;
    m_settings.setValue(QStringLiteral("online/published"), server);
    emit serverChanged();
}
