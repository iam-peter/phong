#ifndef HIGHSCORES_H
#define HIGHSCORES_H

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QWebSocket;

// The high scores of endless and squash that everybody shares, kept by
// phong-server. Each request opens a connection, gets the list and closes
// it again.
class HighScores : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // The server, empty for none
    Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(bool available READ isAvailable NOTIFY serverUrlChanged)
    // The best scores, { name, score }, highest first
    Q_PROPERTY(QVariantList endless READ endless NOTIFY scoresChanged)
    Q_PROPERTY(QVariantList squash READ squash NOTIFY scoresChanged)
    // A request is on its way
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    // A sleeping server, e.g. on a free plan, takes a while to wake up
    static constexpr int timeout = 60000;

    explicit HighScores(QObject* parent = nullptr);
    ~HighScores() override;

    // Adds the score of name to board, "endless" or "squash", and gets
    // the list back
    Q_INVOKABLE void submit(const QString& board, const QString& name, int score);
    Q_INVOKABLE void refresh(const QString& board);
    // Where the entry is on the list, -1 if it isn't
    Q_INVOKABLE int place(const QString& board, const QString& name, int score) const;

    void setServerUrl(const QString& serverUrl);
    QString serverUrl() const;
    bool isAvailable() const;
    QVariantList endless() const;
    QVariantList squash() const;
    bool isBusy() const;
    QString error() const;

signals:
    void serverUrlChanged();
    void scoresChanged();
    void busyChanged(bool);
    void errorChanged();

private:
    void request(const QVariantMap& message);
    void finish(const QString& error);
    void setError(const QString& error);

    QString m_serverUrl;
    QVariantList m_endless;
    QVariantList m_squash;
    QPointer<QWebSocket> m_socket;
    QTimer m_timeout;
    QString m_error;
};

#endif // HIGHSCORES_H
