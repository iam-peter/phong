#ifndef ONLINESERVICE_H
#define ONLINESERVICE_H

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QSettings>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

class QNetworkAccessManager;
class QNetworkReply;
class QWebSocket;

// Which phong-server games over the internet and the shared high scores
// use, if any. The game asks a small directory for it, online.json in
// the repository: { "server": "wss://..." }, an empty server
// switches online play off for everybody. So the server can move to
// another host, or be switched off, without a new build. A server set
// in the settings goes first, and the player can switch it all off.
//
// Free hosts put a server to sleep when nobody uses it, and waking it
// takes a while. check() asks the server for a list of scores and tells
// how long the answer takes, so the screens can say it's waking up.
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
    // How the server is, from the last check
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    // Seconds the running check waits for an answer
    Q_PROPERTY(int waited READ waited NOTIFY stateChanged)
    // Seconds the server took to wake up last time, 0 if never seen asleep
    Q_PROPERTY(int lastWake READ lastWake NOTIFY stateChanged)
    // For the player, empty while there is nothing to say
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)

public:
    enum State {
        Unknown = 0,
        Checking,
        // No answer for a moment, a sleeping server wakes up
        Waking,
        Ready,
        Unreachable
    };
    Q_ENUM(State)

    static constexpr int timeout = 8000;
    // After this a check says the server is waking up
    static constexpr int wakingAfter = 2000;
    static constexpr int wakeTimeout = 120000;
    // A server that answered this recently isn't asked again
    static constexpr int freshFor = 5 * 60 * 1000;

    explicit OnlineService(QObject* parent = nullptr);

    static QUrl defaultDirectory();
    static QString defaultServer();

    // Asks the directory again, e.g. when a lobby opens
    Q_INVOKABLE void refresh();
    // Asks the server, which also wakes it up, unless it answered a
    // moment ago or is being asked
    Q_INVOKABLE void check();

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
    State state() const;
    int waited() const;
    int lastWake() const;
    QString status() const;

signals:
    void serverChanged();
    void busyChanged(bool);
    void stateChanged();

private:
    void answered(QNetworkReply* reply);
    void setPublishedServer(const QString& server);
    void setState(State state);
    // The check is over, ready or not
    void finishCheck(bool ready);

    QSettings m_settings;
    QNetworkAccessManager* m_network;
    QPointer<QNetworkReply> m_reply;
    QUrl m_directory;
    bool m_enabled;
    QString m_customServer;
    QString m_publishedServer;

    QPointer<QWebSocket> m_probe;
    QString m_probed;
    QElapsedTimer m_checkTime;
    QElapsedTimer m_lastAnswer;
    QTimer m_tick;
    State m_state;
};

#endif // ONLINESERVICE_H
