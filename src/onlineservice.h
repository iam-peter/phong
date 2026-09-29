#ifndef ONLINESERVICE_H
#define ONLINESERVICE_H

#include <QObject>
#include <QPointer>
#include <QSettings>
#include <QString>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

class QNetworkAccessManager;
class QNetworkReply;

// Which phong-server games over the internet and the shared high scores
// use, if any. The game asks a small directory for it, online.json next
// to the browser version: { "server": "wss://..." }, an empty server
// switches online play off for everybody. So the server can move to
// another host, or be switched off, without a new build. A server set
// in the settings goes first, and the player can switch it all off.
class OnlineService : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // The player's switch for internet play and the shared scores
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY serverChanged)
    // A server of the player's own, empty for the published one
    Q_PROPERTY(QString customServer READ customServer WRITE setCustomServer NOTIFY serverChanged)
    // What the directory says, the last answer is kept for a start
    // without a connection, the build's default before any answer
    Q_PROPERTY(QString publishedServer READ publishedServer NOTIFY serverChanged)
    // The server to use, empty while online play is off or unknown
    Q_PROPERTY(QString server READ server NOTIFY serverChanged)
    Q_PROPERTY(bool available READ isAvailable NOTIFY serverChanged)
    // Asking the directory
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)

public:
    static constexpr int timeout = 8000;

    explicit OnlineService(QObject* parent = nullptr);

    static QUrl defaultDirectory();
    static QString defaultServer();

    // Asks the directory again, e.g. when a lobby opens
    Q_INVOKABLE void refresh();

    // Where to ask, for tests, empty for no directory
    void setDirectory(const QUrl& directory);
    QUrl directory() const;

    void setEnabled(bool enabled);
    bool isEnabled() const;
    void setCustomServer(const QString& customServer);
    QString customServer() const;
    QString publishedServer() const;
    QString server() const;
    bool isAvailable() const;
    bool isBusy() const;

signals:
    void serverChanged();
    void busyChanged(bool);

private:
    void answered(QNetworkReply* reply);
    void setPublishedServer(const QString& server);

    QSettings m_settings;
    QNetworkAccessManager* m_network;
    QPointer<QNetworkReply> m_reply;
    QUrl m_directory;
    bool m_enabled;
    QString m_customServer;
    QString m_publishedServer;
};

#endif // ONLINESERVICE_H
