#ifndef MODIFIERS_H
#define MODIFIERS_H

#include "match.h"

#include <QAbstractListModel>
#include <QPointer>
#include <QRandomGenerator>
#include <QRectF>
#include <QVector2D>
#include <QtQml/qqmlregistration.h>

// Collectible modifiers on the playing field. The ball collects them by
// flying through, the player who touched the ball last gets the effect:
// boosts help that player, curses hit the opponent.
class Modifiers : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(Match* match READ match WRITE setMatch NOTIFY matchChanged)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QRectF spawnArea READ spawnArea WRITE setSpawnArea NOTIFY spawnAreaChanged)
    Q_PROPERTY(bool fieldNarrowed READ isFieldNarrowed NOTIFY fieldNarrowedChanged)

public:
    enum Kind {
        FastBall = 0,   // boost, the ball speeds up towards the opponent
        BigPaddle,      // boost, longer paddle for a while
        Shield,         // boost, a barrier behind the paddle stops one goal
        SmallPaddle,    // curse, the opponent's paddle shrinks for a while
        SpinPaddle,     // curse, the opponent's paddle rotates for a while
        NarrowField     // both, the walls move in for a while
    };
    Q_ENUM(Kind)

    enum Role {
        KindRole = Qt::UserRole + 1,
        ItemIdRole,
        ItemXRole,
        ItemYRole
    };

    static constexpr int maxItems = 2;
    static constexpr qreal minSpawnDelay = 5.0;
    static constexpr qreal maxSpawnDelay = 9.0;
    static constexpr qreal itemLifetime = 15.0;
    static constexpr qreal minItemDistance = 3.0;

    static constexpr qreal fastBallFactor = 1.4;
    static constexpr qreal bigPaddleScale = 1.5;
    static constexpr qreal smallPaddleScale = 0.6;
    static constexpr qreal paddleDuration = 12.0;
    static constexpr qreal spinDuration = 7.0;
    static constexpr qreal narrowDuration = 12.0;

    explicit Modifiers(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Removes all items and effects, call when a match starts
    Q_INVOKABLE void reset();

    // Spawns and expires items and runs the effect timers, call once per
    // simulation step
    Q_INVOKABLE void advance(qreal dt);

    // The ball flew through the item, returns whether it was collected.
    // Nobody gets it before the ball was touched after the serve.
    Q_INVOKABLE bool collect(int itemId);

    // The ball hit the shield in front of the goal of side
    Q_INVOKABLE bool shieldHit(Match::Side side);

    Q_INVOKABLE static bool isCurse(Modifiers::Kind kind);

    // Places an item, for tests and debugging
    Q_INVOKABLE int spawn(Modifiers::Kind kind, const QVector2D& position);

    void setSeed(quint32 seed);

    void setMatch(Match* match);
    Match* match() const;

    void setEnabled(bool enabled);
    bool isEnabled() const;

    void setSpawnArea(const QRectF& spawnArea);
    QRectF spawnArea() const;

    bool isFieldNarrowed() const;

signals:
    void matchChanged(Match*);
    void enabledChanged(bool);
    void spawnAreaChanged(const QRectF&);
    void fieldNarrowedChanged(bool);

    // side is the player affected by the effect
    void collected(Modifiers::Kind kind, Match::Side side, const QVector2D& position);

private:
    struct Item {
        int id;
        Kind kind;
        QVector2D position;
        qreal age;
    };

    // Effect timers of one player
    struct Effects {
        qreal paddleTime;
        qreal spinTime;
    };

    void spawnRandom();
    void apply(Kind kind, Match::Side side);
    void removeItem(int row);
    void resetSpawnCountdown();
    void setFieldNarrowed(bool fieldNarrowed);
    Effects& effects(Match::Side side);

    QPointer<Match> m_match;
    bool m_enabled;
    QRectF m_spawnArea;

    QList<Item> m_items;
    int m_nextId;
    qreal m_spawnCountdown;

    Effects m_left;
    Effects m_right;
    qreal m_narrowTime;
    bool m_fieldNarrowed;

    QRandomGenerator m_random;
};

#endif // MODIFIERS_H
