#ifndef ARENAS_H
#define ARENAS_H

#include <QObject>
#include <QRandomGenerator>
#include <QRectF>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// Layouts of obstacles on the playing field, defined in JSON, see
// config/arenas.json. Bumpers are round, blocks are rectangles.
class Arenas : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // Maps with id, name, bumpers ({x, y, radius}) and blocks ({x, y, width, height})
    Q_PROPERTY(QVariantList arenas READ arenas NOTIFY arenasChanged)

public:
    struct Bumper {
        qreal x;
        qreal y;
        qreal radius;
    };

    struct Arena {
        QString id;
        QString name;
        QList<Bumper> bumpers;
        QList<QRectF> blocks;
    };

    // The embedded configuration unless overridden, e.g. from the command line
    static QString defaultSource();
    static void setDefaultSource(const QString& fileName);

    explicit Arenas(QObject* parent = nullptr);

    // Replaces the arenas, invalid entries are skipped with a warning.
    // Returns false and keeps the old arenas if the JSON is unusable.
    bool load(const QString& fileName);
    bool loadJson(const QByteArray& json, QString* error = nullptr);

    const QList<Arena>& list() const;
    QVariantList arenas() const;

    // The arena with id, an empty map if there is none
    Q_INVOKABLE QVariantMap arena(const QString& id) const;
    Q_INVOKABLE QString randomId();
    // Bounding rectangles of all obstacles of the arena with id
    Q_INVOKABLE QVariantList obstacleRects(const QString& id) const;

signals:
    void arenasChanged();

private:
    int find(const QString& id) const;
    static QVariantMap toMap(const Arena& arena);

    QList<Arena> m_arenas;
    QRandomGenerator m_random;
};

#endif // ARENAS_H
