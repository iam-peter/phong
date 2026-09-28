#ifndef ARENAS_H
#define ARENAS_H

#include <QJsonValue>
#include <QObject>
#include <QRandomGenerator>
#include <QRectF>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// Layouts of obstacles on the playing field, defined in JSON, see
// config/arenas.json. Bumpers are round, blocks are rectangles, both may
// move back and forth.
class Arenas : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // Maps with id, name, bumpers ({x, y, radius, move}) and blocks
    // ({x, y, width, height, move}), move is {x, y, period, phase} or null
    Q_PROPERTY(QVariantList arenas READ arenas NOTIFY arenasChanged)

public:
    // Swings between -offset and +offset around the position,
    // offset * sin(2 pi (t / period + phase))
    struct Motion {
        qreal x = 0.0;
        qreal y = 0.0;
        qreal period = 4.0;
        qreal phase = 0.0;

        bool isMoving() const { return x != 0.0 || y != 0.0; }
        // All the room rect takes while moving
        QRectF sweep(const QRectF& rect) const;
    };

    struct Bumper {
        qreal x;
        qreal y;
        qreal radius;
        Motion move;
    };

    struct Block {
        QRectF rect;
        Motion move;
    };

    struct Arena {
        QString id;
        QString name;
        QList<Bumper> bumpers;
        QList<Block> blocks;
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
    // Bounding rectangles of all obstacles of the arena with id, with all
    // the room moving ones take
    Q_INVOKABLE QVariantList obstacleRects(const QString& id) const;

signals:
    void arenasChanged();

private:
    int find(const QString& id) const;
    static QVariantMap toMap(const Arena& arena);
    static Motion readMotion(const QJsonValue& value);
    static QVariant toVariant(const Motion& motion);

    QList<Arena> m_arenas;
    QRandomGenerator m_random;
};

#endif // ARENAS_H
