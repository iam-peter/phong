#include "onlineservice.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

Q_LOGGING_CATEGORY(lcOnline, "phong.online")

OnlineService::OnlineService(QObject* parent):
    QObject(parent),
    m_settings(),
    m_network(new QNetworkAccessManager(this)),
    m_reply(),
    m_directory(defaultDirectory()),
    m_enabled(true),
    m_customServer(),
    m_publishedServer(m_settings.value(QStringLiteral("online/published"), defaultServer()).toString())
{
    m_network->setTransferTimeout(timeout);
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
    // Always the current one, not a cached copy
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::finished, this, [this, reply = m_reply.data()] { answered(reply); });
    emit busyChanged(true);
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
