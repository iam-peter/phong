#include "partymodifiers.h"

#include <QtMath>

#include <algorithm>

namespace {
// The well keeps this far from the items, and the items from the middle,
// where the ball waits for the kickoff
constexpr qreal wellClearance = 1.6;
constexpr qreal middleClearance = 1.5;
}

bool PartyModifiers::isSupported(Modifiers::Effect effect)
{
    switch (effect) {
        case Modifiers::Effect::BallSpeed:
        case Modifiers::Effect::PaddleSize:
        case Modifiers::Effect::Shield:
        case Modifiers::Effect::Magnet:
        case Modifiers::Effect::Freeze:
        case Modifiers::Effect::Reverse:
        case Modifiers::Effect::GhostBall:
        case Modifiers::Effect::GravityWell:
            return true;
        default:
            return false;
    }
}

PartyModifiers::PartyModifiers(QObject* parent):
    QAbstractListModel(parent),
    m_match(nullptr),
    m_enabled(true),
    m_spawnRadius(6.0),
    m_definitions(),
    m_spawn(),
    m_items(),
    m_nextId(1),
    m_spawnCountdown(0.0),
    m_effects(PartyMatch::maxPlayers),
    m_ghostTime(0.0),
    m_ghostBall(false),
    m_gravityTime(0.0),
    m_gravityWell(),
    m_gravityStrength(0.0),
    m_random(QRandomGenerator::global()->generate())
{
    // The configuration of the classic field, what works here
    const Modifiers catalog;
    for (const Modifiers::Definition& definition : catalog.definitions()) {
        if (isSupported(definition.effect))
            m_definitions.append(definition);
    }
    m_spawn = catalog.spawnSettings();
    resetSpawnCountdown();
}

int PartyModifiers::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_items.size());
}

QVariant PartyModifiers::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return QVariant();

    const Item& item = m_items.at(index.row());
    const Modifiers::Definition& definition = m_definitions.at(item.definition);
    switch (role) {
        case Role::DefinitionRole:
            return item.definition;
        case Role::ItemIdRole:
            return item.id;
        case Role::ItemXRole:
            return item.position.x();
        case Role::ItemYRole:
            return item.position.y();
        case Role::ItemNameRole:
            return definition.name;
        case Role::ItemGlyphRole:
            return definition.glyph;
        case Role::ItemColorRole:
            return definition.color;
        default:
            return QVariant();
    }
}

QHash<int, QByteArray> PartyModifiers::roleNames() const
{
    return {
        { Role::DefinitionRole, "definition" },
        { Role::ItemIdRole, "itemId" },
        { Role::ItemXRole, "itemX" },
        { Role::ItemYRole, "itemY" },
        { Role::ItemNameRole, "itemName" },
        { Role::ItemGlyphRole, "itemGlyph" },
        { Role::ItemColorRole, "itemColor" }
    };
}

const QList<Modifiers::Definition>& PartyModifiers::definitions() const
{
    return m_definitions;
}

int PartyModifiers::findDefinition(const QString& id) const
{
    const auto it = std::find_if(m_definitions.cbegin(), m_definitions.cend(),
                                 [&id](const Modifiers::Definition& definition) { return definition.id == id; });
    return it == m_definitions.cend() ? -1 : int(it - m_definitions.cbegin());
}

QVariantMap PartyModifiers::definition(int index) const
{
    if (index < 0 || index >= m_definitions.size())
        return QVariantMap();

    const Modifiers::Definition& definition = m_definitions.at(index);
    return {
        { QStringLiteral("id"), definition.id },
        { QStringLiteral("name"), definition.name },
        { QStringLiteral("glyph"), definition.glyph },
        { QStringLiteral("color"), definition.color },
        { QStringLiteral("effect"), int(definition.effect) },
        { QStringLiteral("target"), int(definition.target) }
    };
}

void PartyModifiers::reset()
{
    beginResetModel();
    m_items.clear();
    endResetModel();

    m_effects = QList<Effects>(PartyMatch::maxPlayers);
    m_ghostTime = 0.0;
    setGhostBall(false);
    m_gravityTime = 0.0;
    setGravityWell(QVector2D(), 0.0);

    if (m_match) {
        for (int index = 0; index < PartyMatch::maxPlayers; ++index) {
            Player* player = m_match->player(index);
            player->setPaddleScale(1.0);
            player->setShielded(false);
            player->setCatches(0);
            player->setFrozen(false);
            player->setReversed(false);
        }
    }

    resetSpawnCountdown();
}

void PartyModifiers::advance(qreal dt)
{
    if (!m_match)
        return;

    // Effects run out
    for (int index = 0; index < PartyMatch::maxPlayers; ++index) {
        Effects& effects = m_effects[index];
        Player* player = m_match->player(index);

        if (effects.paddleTime > 0.0) {
            effects.paddleTime -= dt;
            if (effects.paddleTime <= 0.0)
                player->setPaddleScale(1.0);
        }
        if (effects.freezeTime > 0.0) {
            effects.freezeTime -= dt;
            if (effects.freezeTime <= 0.0)
                player->setFrozen(false);
        }
        if (effects.reverseTime > 0.0) {
            effects.reverseTime -= dt;
            if (effects.reverseTime <= 0.0)
                player->setReversed(false);
        }
        // The magnet is gone after its time or its catches
        if (effects.magnetTime > 0.0) {
            effects.magnetTime -= dt;
            if (effects.magnetTime <= 0.0 || player->catches() <= 0) {
                effects.magnetTime = 0.0;
                player->setCatches(0);
            }
        }
    }

    if (m_ghostTime > 0.0) {
        m_ghostTime -= dt;
        if (m_ghostTime <= 0.0)
            setGhostBall(false);
    }

    if (m_gravityTime > 0.0) {
        m_gravityTime -= dt;
        if (m_gravityTime <= 0.0)
            setGravityWell(QVector2D(), 0.0);
    }

    // Items only come and go while the ball is in play
    if (m_match->state() != PartyMatch::State::Playing)
        return;

    for (int row = int(m_items.size()) - 1; row >= 0; --row) {
        m_items[row].age += dt;
        if (m_items[row].age >= m_spawn.lifetime)
            removeItem(row);
    }

    if (!m_enabled || m_definitions.isEmpty())
        return;

    m_spawnCountdown -= dt;
    if (m_spawnCountdown <= 0.0) {
        if (m_items.size() < m_spawn.maxItems) {
            qreal total = 0.0;
            for (const Modifiers::Definition& definition : std::as_const(m_definitions))
                total += definition.weight;
            qreal pick = m_random.bounded(std::max(total, 0.001));
            int definition = 0;
            while (definition < m_definitions.size() - 1 && pick >= m_definitions.at(definition).weight) {
                pick -= m_definitions.at(definition).weight;
                ++definition;
            }

            bool found = false;
            const QVector2D position = freePosition(m_spawnRadius, &found);
            if (found)
                spawn(definition, position);
        }
        resetSpawnCountdown();
    }
}

bool PartyModifiers::collect(int itemId)
{
    if (!m_match || !m_match->isAlive(m_match->lastTouch()))
        return false;

    const auto it = std::find_if(m_items.cbegin(), m_items.cend(),
                                 [itemId](const Item& item) { return item.id == itemId; });
    if (it == m_items.cend())
        return false;

    const Item item = *it;
    removeItem(int(it - m_items.cbegin()));

    const int collector = m_match->lastTouch();
    apply(item.definition, collector);
    emit collected(item.definition, collector, item.position);
    return true;
}

int PartyModifiers::spawn(int definition, const QVector2D& position)
{
    if (definition < 0 || definition >= m_definitions.size())
        return -1;

    const int row = int(m_items.size());
    beginInsertRows(QModelIndex(), row, row);
    m_items.append({ m_nextId++, definition, position, 0.0 });
    endInsertRows();

    return m_items.last().id;
}

QVariantMap PartyModifiers::snapshot() const
{
    QVariantList items;
    for (const Item& item : m_items) {
        items.append(QVariantMap{ { QStringLiteral("id"), item.id },
                                  { QStringLiteral("def"), m_definitions.at(item.definition).id },
                                  { QStringLiteral("x"), item.position.x() },
                                  { QStringLiteral("y"), item.position.y() } });
    }

    return { { QStringLiteral("items"), items },
             { QStringLiteral("ghost"), m_ghostBall },
             { QStringLiteral("well"), QVariantList{ m_gravityWell.x(), m_gravityWell.y() } },
             { QStringLiteral("strength"), m_gravityStrength } };
}

void PartyModifiers::applySnapshot(const QVariantMap& snapshot)
{
    // Items the host no longer has go, new ones come with its ids
    QList<Item> wanted;
    for (const QVariant& entry : snapshot.value(QStringLiteral("items")).toList()) {
        const QVariantMap item = entry.toMap();
        const int definition = findDefinition(item.value(QStringLiteral("def")).toString());
        if (definition >= 0) {
            wanted.append({ item.value(QStringLiteral("id")).toInt(), definition,
                            QVector2D(item.value(QStringLiteral("x")).toFloat(),
                                      item.value(QStringLiteral("y")).toFloat()),
                            0.0 });
        }
    }
    for (int row = int(m_items.size()) - 1; row >= 0; --row) {
        const int id = m_items.at(row).id;
        if (std::none_of(wanted.cbegin(), wanted.cend(), [id](const Item& item) { return item.id == id; }))
            removeItem(row);
    }
    for (const Item& item : std::as_const(wanted)) {
        if (std::any_of(m_items.cbegin(), m_items.cend(), [&item](const Item& other) { return other.id == item.id; }))
            continue;
        const int row = int(m_items.size());
        beginInsertRows(QModelIndex(), row, row);
        m_items.append(item);
        endInsertRows();
    }

    setGhostBall(snapshot.value(QStringLiteral("ghost")).toBool());
    const QVariantList well = snapshot.value(QStringLiteral("well")).toList();
    setGravityWell(well.size() == 2 ? QVector2D(well.at(0).toFloat(), well.at(1).toFloat()) : QVector2D(),
                   snapshot.value(QStringLiteral("strength")).toDouble());
}

void PartyModifiers::setSeed(quint32 seed)
{
    m_random.seed(seed);
}

void PartyModifiers::setMatch(PartyMatch* match)
{
    if (m_match == match)
        return;

    m_match = match;
    emit matchChanged(match);
}

PartyMatch* PartyModifiers::match() const
{
    return m_match;
}

void PartyModifiers::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;

    m_enabled = enabled;
    emit enabledChanged(enabled);
}

bool PartyModifiers::isEnabled() const
{
    return m_enabled;
}

void PartyModifiers::setSpawnRadius(qreal spawnRadius)
{
    if (m_spawnRadius == spawnRadius)
        return;

    m_spawnRadius = spawnRadius;
    emit spawnRadiusChanged(spawnRadius);
}

qreal PartyModifiers::spawnRadius() const
{
    return m_spawnRadius;
}

bool PartyModifiers::isGhostBall() const
{
    return m_ghostBall;
}

QVector2D PartyModifiers::gravityWell() const
{
    return m_gravityWell;
}

qreal PartyModifiers::gravityStrength() const
{
    return m_gravityStrength;
}

void PartyModifiers::apply(int index, int collector)
{
    const Modifiers::Definition& definition = m_definitions.at(index);

    // Effects on the ball and the field don't care about the target
    switch (definition.effect) {
        case Modifiers::Effect::BallSpeed:
            m_match->scaleBallSpeed(definition.value);
            return;
        case Modifiers::Effect::GhostBall:
            m_ghostTime = definition.duration;
            setGhostBall(true);
            return;
        case Modifiers::Effect::GravityWell:
            setGravityWell(QVector2D(), 0.0);
            m_gravityTime = definition.duration;
            setGravityWell(freePosition(0.6 * m_spawnRadius), definition.value);
            return;
        default:
            break;
    }

    QList<int> players;
    switch (definition.target) {
        case Modifiers::Target::Collector:
            players = { collector };
            break;
        case Modifiers::Target::Opponent:
            for (int player = 0; player < m_match->players(); ++player) {
                if (player != collector && m_match->isAlive(player))
                    players.append(player);
            }
            break;
        case Modifiers::Target::Both:
            for (int player = 0; player < m_match->players(); ++player) {
                if (m_match->isAlive(player))
                    players.append(player);
            }
            break;
    }

    for (int index : std::as_const(players)) {
        Player* player = m_match->player(index);
        Effects& effects = m_effects[index];

        switch (definition.effect) {
            case Modifiers::Effect::PaddleSize:
                player->setPaddleScale(definition.value);
                effects.paddleTime = definition.duration;
                break;
            case Modifiers::Effect::Shield:
                player->setShielded(true);
                break;
            case Modifiers::Effect::Magnet:
                player->setCatches(int(definition.value));
                effects.magnetTime = definition.duration;
                break;
            case Modifiers::Effect::Freeze:
                player->setFrozen(true);
                effects.freezeTime = definition.duration;
                break;
            case Modifiers::Effect::Reverse:
                player->setReversed(true);
                effects.reverseTime = definition.duration;
                break;
            default:
                break;
        }
    }
}

void PartyModifiers::removeItem(int row)
{
    beginRemoveRows(QModelIndex(), row, row);
    m_items.removeAt(row);
    endRemoveRows();
}

void PartyModifiers::resetSpawnCountdown()
{
    m_spawnCountdown = m_spawn.minDelay + m_random.bounded(std::max(m_spawn.maxDelay - m_spawn.minDelay, 0.01));
}

QVector2D PartyModifiers::freePosition(qreal radius, bool* found)
{
    const auto isFree = [this](const QVector2D& position) {
        if (position.length() < middleClearance)
            return false;
        if (m_gravityStrength > 0.0 && position.distanceToPoint(m_gravityWell) < m_spawn.minDistance + wellClearance)
            return false;
        return std::none_of(m_items.cbegin(), m_items.cend(), [&](const Item& item) {
            return item.position.distanceToPoint(position) < m_spawn.minDistance;
        });
    };

    QVector2D position;
    for (int attempt = 0; attempt < 10; ++attempt) {
        // Evenly over the disc
        const qreal distance = radius * qSqrt(m_random.bounded(1.0));
        const qreal angle = m_random.bounded(2.0 * M_PI);
        position = QVector2D(float(distance * qCos(angle)), float(distance * qSin(angle)));
        if (isFree(position)) {
            if (found)
                *found = true;
            return position;
        }
    }

    if (found)
        *found = false;
    return position;
}

void PartyModifiers::setGhostBall(bool ghostBall)
{
    if (m_ghostBall == ghostBall)
        return;

    m_ghostBall = ghostBall;
    emit ghostBallChanged(ghostBall);
}

void PartyModifiers::setGravityWell(const QVector2D& position, qreal strength)
{
    if (m_gravityWell == position && m_gravityStrength == strength)
        return;

    m_gravityWell = position;
    m_gravityStrength = strength;
    emit gravityWellChanged();
}
