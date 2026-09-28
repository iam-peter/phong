#ifndef PARTYMODIFIERS_H
#define PARTYMODIFIERS_H

#include "modifiers.h"
#include "partymatch.h"

#include <QAbstractListModel>
#include <QPointer>
#include <QRandomGenerator>
#include <QVariantList>
#include <QVariantMap>
#include <QVector2D>
#include <QtQml/qqmlregistration.h>

// The modifiers of the polygon, collected like in Modifiers by the player
// who hit the ball last. They come from the same configuration, but only
// the effects that make sense for more than two sides: the ball speed,
// paddle sizes, shields, magnets, freezing, reversed controls, the ghost
// ball and the gravity well. A curse hits every other player still in.
class PartyModifiers : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(PartyMatch* match READ match WRITE setMatch NOTIFY matchChanged)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    // Items spawn within this distance from the middle
    Q_PROPERTY(qreal spawnRadius READ spawnRadius WRITE setSpawnRadius NOTIFY spawnRadiusChanged)
    Q_PROPERTY(bool ghostBall READ isGhostBall NOTIFY ghostBallChanged)
    Q_PROPERTY(QVector2D gravityWell READ gravityWell NOTIFY gravityWellChanged)
    Q_PROPERTY(qreal gravityStrength READ gravityStrength NOTIFY gravityWellChanged)

public:
    // The roles of Modifiers, the delegates are the same
    enum Role {
        DefinitionRole = Qt::UserRole + 1,
        ItemIdRole,
        ItemXRole,
        ItemYRole,
        ItemNameRole,
        ItemGlyphRole,
        ItemColorRole
    };

    static bool isSupported(Modifiers::Effect effect);

    explicit PartyModifiers(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    const QList<Modifiers::Definition>& definitions() const;
    Q_INVOKABLE int findDefinition(const QString& id) const;
    // name, glyph, color, effect and target of a definition
    Q_INVOKABLE QVariantMap definition(int index) const;

    // Removes all items and effects, call when a game starts
    Q_INVOKABLE void reset();
    // Spawns and expires items and runs the effect timers, call once per
    // simulation step
    Q_INVOKABLE void advance(qreal dt);
    // The ball flew through the item, returns whether it was collected,
    // nobody collects before the ball was hit after the serve
    Q_INVOKABLE bool collect(int itemId);

    // Places an item, for tests and debugging
    Q_INVOKABLE int spawn(int definition, const QVector2D& position);

    // Items and field effects for the others on the network, the effects
    // on the players travel with the match
    Q_INVOKABLE QVariantMap snapshot() const;
    Q_INVOKABLE void applySnapshot(const QVariantMap& snapshot);

    void setSeed(quint32 seed);

    void setMatch(PartyMatch* match);
    PartyMatch* match() const;

    void setEnabled(bool enabled);
    bool isEnabled() const;

    void setSpawnRadius(qreal spawnRadius);
    qreal spawnRadius() const;

    bool isGhostBall() const;
    QVector2D gravityWell() const;
    qreal gravityStrength() const;

signals:
    void matchChanged(PartyMatch*);
    void enabledChanged(bool);
    void spawnRadiusChanged(qreal);
    void ghostBallChanged(bool);
    void gravityWellChanged();
    // collector is the player who got it
    void collected(int definition, int collector, const QVector2D& position);

private:
    struct Item {
        int id;
        int definition;
        QVector2D position;
        qreal age;
    };

    // The timed effects on one player
    struct Effects {
        qreal paddleTime = 0.0;
        qreal magnetTime = 0.0;
        qreal freezeTime = 0.0;
        qreal reverseTime = 0.0;
    };

    void apply(int definition, int collector);
    void removeItem(int row);
    void resetSpawnCountdown();
    // A random place within radius of the middle, clear of items and the well
    QVector2D freePosition(qreal radius, bool* found = nullptr);
    void setGhostBall(bool ghostBall);
    void setGravityWell(const QVector2D& position, qreal strength);

    QPointer<PartyMatch> m_match;
    bool m_enabled;
    qreal m_spawnRadius;
    QList<Modifiers::Definition> m_definitions;
    Modifiers::SpawnSettings m_spawn;

    QList<Item> m_items;
    int m_nextId;
    qreal m_spawnCountdown;
    QList<Effects> m_effects;
    qreal m_ghostTime;
    bool m_ghostBall;
    qreal m_gravityTime;
    QVector2D m_gravityWell;
    qreal m_gravityStrength;

    QRandomGenerator m_random;
};

#endif // PARTYMODIFIERS_H
