#ifndef MODIFIERS_H
#define MODIFIERS_H

#include "match.h"

#include <QAbstractListModel>
#include <QColor>
#include <QPointer>
#include <QRandomGenerator>
#include <QRectF>
#include <QVariantList>
#include <QVariantMap>
#include <QVector2D>
#include <QtQml/qqmlregistration.h>

// Collectible modifiers on the playing field. The ball collects them by
// flying through, the player who touched the ball last is the collector.
//
// The effects are built in, which modifiers exist and how they use the
// effects is defined in JSON, see config/modifiers.json.
class Modifiers : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(Match* match READ match WRITE setMatch NOTIFY matchChanged)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QRectF spawnArea READ spawnArea WRITE setSpawnArea NOTIFY spawnAreaChanged)
    // Rectangles items keep clear of, e.g. the bumpers of an arena
    Q_PROPERTY(QVariantList obstacles READ obstacles WRITE setObstacles NOTIFY obstaclesChanged)
    Q_PROPERTY(qreal fieldInset READ fieldInset NOTIFY fieldInsetChanged)
    Q_PROPERTY(qreal maxFieldInset READ maxFieldInset NOTIFY definitionsChanged)
    // Two linked portals while they are open, empty otherwise
    Q_PROPERTY(QVariantList portals READ portals NOTIFY portalsChanged)
    // The ball can't be seen in the middle third of the field
    Q_PROPERTY(bool ghostBall READ isGhostBall NOTIFY ghostBallChanged)
    // A gravity well bends the flight of the balls, strength 0 while there is none
    Q_PROPERTY(QVector2D gravityWell READ gravityWell NOTIFY gravityWellChanged)
    Q_PROPERTY(qreal gravityStrength READ gravityStrength NOTIFY gravityWellChanged)

public:
    enum Effect {
        BallSpeed = 0,  // value: speed factor
        PaddleSize,     // value: length factor, for duration seconds
        Shield,         // a barrier behind the paddle stops one goal
        Spin,           // value: degrees per second, for duration seconds
        NarrowField,    // value: how far the walls move in, for duration seconds
        MultiBall,      // value: extra balls, they stay for duration seconds
        Magnet,         // value: balls the paddle catches, for duration seconds
        Portals,        // two linked portals open for duration seconds
        Freeze,         // the paddle can't move for duration seconds
        Reverse,        // the controls are swapped for duration seconds
        GhostBall,      // the ball is invisible in the middle for duration seconds
        GravityWell     // value: strength of a well bending the flight, for duration seconds
    };
    Q_ENUM(Effect)

    enum Target {
        Collector = 0,
        Opponent,       // a curse
        Both
    };
    Q_ENUM(Target)

    enum Role {
        DefinitionRole = Qt::UserRole + 1,
        ItemIdRole,
        ItemXRole,
        ItemYRole,
        ItemNameRole,
        ItemGlyphRole,
        ItemColorRole
    };

    struct Definition {
        QString id;
        QString name;
        QString glyph;
        QColor color;
        Effect effect;
        Target target;
        qreal value;
        qreal duration;
        qreal weight;       // relative spawn chance
    };

    struct SpawnSettings {
        qreal minDelay;
        qreal maxDelay;
        int maxItems;
        qreal lifetime;
        qreal minDistance;
    };

    // The embedded configuration unless overridden, e.g. from the command line
    static QString defaultSource();
    static void setDefaultSource(const QString& fileName);

    explicit Modifiers(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Replaces the definitions, invalid entries are skipped with a warning.
    // Returns false and keeps the old definitions if the JSON is unusable.
    bool load(const QString& fileName);
    bool loadJson(const QByteArray& json, QString* error = nullptr);

    const QList<Definition>& definitions() const;
    const SpawnSettings& spawnSettings() const;
    Q_INVOKABLE int findDefinition(const QString& id) const;
    // name, glyph, color, effect and target of a definition
    Q_INVOKABLE QVariantMap definition(int index) const;

    // Definitions of the effects active on the player of side
    Q_INVOKABLE QVariantList activeEffects(Match::Side side) const;

    // Where the items are, e.g. for the computer player to aim at
    Q_INVOKABLE QVariantList itemPositions() const;

    // Removes all items and effects, call when a match starts
    Q_INVOKABLE void reset();

    // Spawns and expires items and runs the effect timers, call once per
    // simulation step
    Q_INVOKABLE void advance(qreal dt);

    // The ball flew through the item, returns whether it was collected.
    // The player who touched that ball last gets it, nobody before the
    // ball was touched after the serve.
    Q_INVOKABLE bool collect(int itemId, Ball* ball);
    bool collect(int itemId);

    // The ball hit the shield in front of the goal of side
    Q_INVOKABLE bool shieldHit(Ball* ball, Match::Side side);
    bool shieldHit(Match::Side side);

    // Places an item, for tests and debugging
    Q_INVOKABLE int spawn(int definition, const QVector2D& position);

    void setSeed(quint32 seed);

    void setMatch(Match* match);
    Match* match() const;

    void setEnabled(bool enabled);
    bool isEnabled() const;

    void setSpawnArea(const QRectF& spawnArea);
    QRectF spawnArea() const;

    void setObstacles(const QVariantList& obstacles);
    QVariantList obstacles() const;

    qreal fieldInset() const;
    qreal maxFieldInset() const;

    QVariantList portals() const;
    bool isGhostBall() const;
    QVector2D gravityWell() const;
    qreal gravityStrength() const;

signals:
    void matchChanged(Match*);
    void enabledChanged(bool);
    void spawnAreaChanged(const QRectF&);
    void obstaclesChanged();
    void fieldInsetChanged(qreal);
    void definitionsChanged();
    void effectsChanged();
    void portalsChanged();
    void ghostBallChanged(bool);
    void gravityWellChanged();

    // side is the player affected by the effect, NoSide for both
    void collected(int definition, Match::Side side, const QVector2D& position);

private:
    struct Item {
        int id;
        int definition;
        QVector2D position;
        qreal age;
    };

    // Effects on one player and the definitions that caused them
    struct Effects {
        qreal paddleTime;
        int paddleDefinition;
        qreal spinTime;
        int spinDefinition;
        int shieldDefinition;
        qreal magnetTime;
        int magnetDefinition;
        qreal freezeTime;
        int freezeDefinition;
        qreal reverseTime;
        int reverseDefinition;
    };

    static Effects noEffects();

    void spawnRandom();
    void apply(int definition, Match::Side collector, Ball* ball, const QVector2D& position);
    void removeItem(int row);
    void resetSpawnCountdown();
    void setFieldInset(qreal fieldInset);
    // A random free place in the spawn area within x from minX to maxX
    QVector2D freePosition(qreal minX, qreal maxX, qreal clearance, bool* found = nullptr);
    bool isFree(const QVector2D& position, qreal clearance) const;
    void setPortals(const QList<QVector2D>& portals);
    void setGhostBall(bool ghostBall);
    void setGravityWell(const QVector2D& position, qreal strength);
    Effects& effects(Match::Side side);
    const Effects& effects(Match::Side side) const;

    QPointer<Match> m_match;
    bool m_enabled;
    QRectF m_spawnArea;
    QList<QRectF> m_obstacles;

    QList<Definition> m_definitions;
    SpawnSettings m_spawn;

    QList<Item> m_items;
    int m_nextId;
    qreal m_spawnCountdown;

    Effects m_left;
    Effects m_right;
    qreal m_narrowTime;
    qreal m_fieldInset;
    qreal m_portalTime;
    QList<QVector2D> m_portals;
    qreal m_ghostTime;
    bool m_ghostBall;
    qreal m_gravityTime;
    QVector2D m_gravityWell;
    qreal m_gravityStrength;

    QRandomGenerator m_random;
};

#endif // MODIFIERS_H
