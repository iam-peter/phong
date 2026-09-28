#include "modifiers.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

#include <algorithm>

Q_LOGGING_CATEGORY(lcModifiers, "phong.modifiers")

namespace {
const QString builtInSource = QStringLiteral(":/qt/qml/Phong/config/modifiers.json");
QString s_defaultSource = builtInSource;

const QHash<QString, Modifiers::Effect> effectNames = {
    { QStringLiteral("ballSpeed"), Modifiers::Effect::BallSpeed },
    { QStringLiteral("paddleSize"), Modifiers::Effect::PaddleSize },
    { QStringLiteral("shield"), Modifiers::Effect::Shield },
    { QStringLiteral("spin"), Modifiers::Effect::Spin },
    { QStringLiteral("narrowField"), Modifiers::Effect::NarrowField },
    { QStringLiteral("multiBall"), Modifiers::Effect::MultiBall },
    { QStringLiteral("magnet"), Modifiers::Effect::Magnet }
};

const QHash<QString, Modifiers::Target> targetNames = {
    { QStringLiteral("collector"), Modifiers::Target::Collector },
    { QStringLiteral("opponent"), Modifiers::Target::Opponent },
    { QStringLiteral("both"), Modifiers::Target::Both }
};

// Default value and the range that keeps the game playable
struct ValueRange {
    qreal fallback;
    qreal min;
    qreal max;
};

ValueRange valueRange(Modifiers::Effect effect)
{
    switch (effect) {
        case Modifiers::Effect::BallSpeed:
            return { 1.4, 0.3, 3.0 };
        case Modifiers::Effect::PaddleSize:
            return { 1.5, 0.3, 2.0 };
        case Modifiers::Effect::Spin:
            return { 150.0, 10.0, 720.0 };
        case Modifiers::Effect::NarrowField:
            return { 3.0, 0.5, 5.0 };
        case Modifiers::Effect::MultiBall:
            return { 1.0, 1.0, 3.0 };
        case Modifiers::Effect::Magnet:
            return { 3.0, 1.0, 5.0 };
        case Modifiers::Effect::Shield:
        default:
            return { 0.0, 0.0, 0.0 };
    }
}

Modifiers::SpawnSettings defaultSpawnSettings()
{
    return { 5.0, 9.0, 2, 15.0, 3.0 };
}
}

QString Modifiers::defaultSource()
{
    return s_defaultSource;
}

void Modifiers::setDefaultSource(const QString& fileName)
{
    s_defaultSource = fileName;
}

Modifiers::Modifiers(QObject* parent):
    QAbstractListModel(parent),
    m_match(nullptr),
    m_enabled(true),
    m_spawnArea(-8.0, -4.5, 16.0, 9.0),
    m_obstacles(),
    m_definitions(),
    m_spawn(defaultSpawnSettings()),
    m_items(),
    m_nextId(1),
    m_spawnCountdown(0.0),
    m_left(noEffects()),
    m_right(noEffects()),
    m_narrowTime(0.0),
    m_fieldInset(0.0),
    m_random(QRandomGenerator::global()->generate())
{
    // A broken custom configuration shouldn't take the modifiers away
    if (!load(s_defaultSource) && s_defaultSource != builtInSource) {
        qCWarning(lcModifiers) << "Using the built-in modifiers instead";
        load(builtInSource);
    }
    resetSpawnCountdown();
}

int Modifiers::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_items.size());
}

QVariant Modifiers::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return QVariant();

    const Item& item = m_items.at(index.row());
    const Definition& definition = m_definitions.at(item.definition);
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

QHash<int, QByteArray> Modifiers::roleNames() const
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

bool Modifiers::load(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcModifiers) << "Cannot read" << fileName << file.errorString();
        return false;
    }

    QString error;
    if (!loadJson(file.readAll(), &error)) {
        qCWarning(lcModifiers).noquote() << fileName << error;
        return false;
    }

    return true;
}

bool Modifiers::loadJson(const QByteArray& json, QString* error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (error)
            *error = QStringLiteral("offset %1: %2").arg(parseError.offset).arg(parseError.errorString());
        return false;
    }

    if (!document.isObject() || !document.object().value("modifiers").isArray()) {
        if (error)
            *error = QStringLiteral("expected an object with a \"modifiers\" array");
        return false;
    }

    const QJsonObject root = document.object();

    SpawnSettings spawn = defaultSpawnSettings();
    const QJsonObject spawnObject = root.value("spawn").toObject();
    spawn.minDelay = std::max(spawnObject.value("minDelay").toDouble(spawn.minDelay), 0.5);
    spawn.maxDelay = std::max(spawnObject.value("maxDelay").toDouble(spawn.maxDelay), spawn.minDelay);
    spawn.maxItems = std::clamp(spawnObject.value("maxItems").toInt(spawn.maxItems), 0, 8);
    spawn.lifetime = std::max(spawnObject.value("lifetime").toDouble(spawn.lifetime), 1.0);
    spawn.minDistance = std::max(spawnObject.value("minDistance").toDouble(spawn.minDistance), 0.0);

    QList<Definition> definitions;
    const QJsonArray entries = root.value("modifiers").toArray();
    for (qsizetype i = 0; i < entries.size(); ++i) {
        const QJsonObject entry = entries.at(i).toObject();
        const QString id = entry.value("id").toString(QStringLiteral("#%1").arg(i));

        if (!entry.value("enabled").toBool(true))
            continue;

        const QString effectName = entry.value("effect").toString();
        if (!effectNames.contains(effectName)) {
            qCWarning(lcModifiers) << "Skipping modifier" << id << "with unknown effect" << effectName;
            continue;
        }

        const QString targetName = entry.value("target").toString(QStringLiteral("collector"));
        if (!targetNames.contains(targetName)) {
            qCWarning(lcModifiers) << "Skipping modifier" << id << "with unknown target" << targetName;
            continue;
        }

        const QColor color = QColor::fromString(entry.value("color").toString(QStringLiteral("white")));
        if (!color.isValid()) {
            qCWarning(lcModifiers) << "Skipping modifier" << id << "with invalid color";
            continue;
        }

        const Effect effect = effectNames.value(effectName);
        const ValueRange range = valueRange(effect);

        Definition definition;
        definition.id = id;
        definition.name = entry.value("name").toString(id);
        definition.glyph = entry.value("glyph").toString(QStringLiteral("?"));
        definition.color = color;
        definition.effect = effect;
        definition.target = targetNames.value(targetName);
        definition.value = std::clamp(entry.value("value").toDouble(range.fallback), range.min, range.max);
        definition.duration = std::max(entry.value("duration").toDouble(10.0), 0.5);
        definition.weight = std::max(entry.value("weight").toDouble(1.0), 0.0);
        definitions.append(definition);
    }

    // Items refer to definitions by index
    beginResetModel();
    m_items.clear();
    m_definitions = definitions;
    m_spawn = spawn;
    endResetModel();

    resetSpawnCountdown();
    emit definitionsChanged();
    return true;
}

const QList<Modifiers::Definition>& Modifiers::definitions() const
{
    return m_definitions;
}

const Modifiers::SpawnSettings& Modifiers::spawnSettings() const
{
    return m_spawn;
}

int Modifiers::findDefinition(const QString& id) const
{
    const auto it = std::find_if(m_definitions.cbegin(), m_definitions.cend(),
                                 [&id](const Definition& definition) { return definition.id == id; });
    return it == m_definitions.cend() ? -1 : int(it - m_definitions.cbegin());
}

QVariantMap Modifiers::definition(int index) const
{
    if (index < 0 || index >= m_definitions.size())
        return QVariantMap();

    const Definition& definition = m_definitions.at(index);
    return {
        { QStringLiteral("id"), definition.id },
        { QStringLiteral("name"), definition.name },
        { QStringLiteral("glyph"), definition.glyph },
        { QStringLiteral("color"), definition.color },
        { QStringLiteral("effect"), int(definition.effect) },
        { QStringLiteral("target"), int(definition.target) }
    };
}

QVariantList Modifiers::activeEffects(Match::Side side) const
{
    Player* player = m_match ? m_match->player(side) : nullptr;
    if (!player)
        return QVariantList();

    const Effects& effects = this->effects(side);
    QVariantList active;
    if (effects.paddleTime > 0.0)
        active.append(definition(effects.paddleDefinition));
    if (player->isShielded())
        active.append(definition(effects.shieldDefinition));
    if (effects.spinTime > 0.0)
        active.append(definition(effects.spinDefinition));
    if (effects.magnetTime > 0.0)
        active.append(definition(effects.magnetDefinition));
    return active;
}

QVariantList Modifiers::itemPositions() const
{
    QVariantList positions;
    for (const Item& item : m_items)
        positions.append(item.position);
    return positions;
}

void Modifiers::reset()
{
    beginResetModel();
    m_items.clear();
    endResetModel();

    m_left = noEffects();
    m_right = noEffects();
    m_narrowTime = 0.0;
    setFieldInset(0.0);

    if (m_match) {
        for (Player* player : { m_match->left(), m_match->right() }) {
            player->setPaddleScale(1.0);
            player->setSpinSpeed(0.0);
            player->setShielded(false);
            player->setCatches(0);
        }
    }

    resetSpawnCountdown();
    emit effectsChanged();
}

void Modifiers::advance(qreal dt)
{
    if (!m_match)
        return;

    // Effects run out
    bool changed = false;
    for (Match::Side side : { Match::Side::LeftSide, Match::Side::RightSide }) {
        Effects& effects = this->effects(side);
        Player* player = m_match->player(side);

        if (effects.paddleTime > 0.0) {
            effects.paddleTime -= dt;
            if (effects.paddleTime <= 0.0) {
                player->setPaddleScale(1.0);
                changed = true;
            }
        }

        if (effects.spinTime > 0.0) {
            effects.spinTime -= dt;
            if (effects.spinTime <= 0.0) {
                player->setSpinSpeed(0.0);
                changed = true;
            }
        }

        // The magnet is gone after its time or its catches
        if (effects.magnetTime > 0.0) {
            effects.magnetTime -= dt;
            if (effects.magnetTime <= 0.0 || player->catches() <= 0) {
                effects.magnetTime = 0.0;
                player->setCatches(0);
                changed = true;
            }
        }
    }

    if (changed)
        emit effectsChanged();

    if (m_narrowTime > 0.0) {
        m_narrowTime -= dt;
        if (m_narrowTime <= 0.0)
            setFieldInset(0.0);
    }

    // Items only come and go while the ball is in play
    if (m_match->state() != Match::State::Playing)
        return;

    for (int row = int(m_items.size()) - 1; row >= 0; --row) {
        m_items[row].age += dt;
        if (m_items[row].age >= m_spawn.lifetime)
            removeItem(row);
    }

    if (!m_enabled)
        return;

    m_spawnCountdown -= dt;
    if (m_spawnCountdown <= 0.0) {
        if (m_items.size() < m_spawn.maxItems)
            spawnRandom();
        resetSpawnCountdown();
    }
}

bool Modifiers::collect(int itemId)
{
    return collect(itemId, m_match ? m_match->ball() : nullptr);
}

bool Modifiers::collect(int itemId, Ball* ball)
{
    if (!m_match || !ball || ball->lastTouch() == Match::Side::NoSide)
        return false;

    const auto it = std::find_if(m_items.cbegin(), m_items.cend(),
                                 [itemId](const Item& item) { return item.id == itemId; });
    if (it == m_items.cend())
        return false;

    const Item item = *it;
    removeItem(int(it - m_items.cbegin()));

    const Match::Side collector = ball->lastTouch();
    apply(item.definition, collector, ball, item.position);

    Match::Side affected = collector;
    switch (m_definitions.at(item.definition).target) {
        case Target::Opponent:
            affected = Match::opponent(collector);
            break;
        case Target::Both:
            affected = Match::Side::NoSide;
            break;
        case Target::Collector:
            break;
    }

    emit collected(item.definition, affected, item.position);
    return true;
}

bool Modifiers::shieldHit(Match::Side side)
{
    return shieldHit(m_match ? m_match->ball() : nullptr, side);
}

bool Modifiers::shieldHit(Ball* ball, Match::Side side)
{
    Player* player = m_match ? m_match->player(side) : nullptr;
    if (!player || !player->isShielded())
        return false;

    if (!m_match->shieldHit(ball, side))
        return false;

    player->setShielded(false);
    emit effectsChanged();
    return true;
}

int Modifiers::spawn(int definition, const QVector2D& position)
{
    if (definition < 0 || definition >= m_definitions.size())
        return -1;

    const int row = int(m_items.size());
    beginInsertRows(QModelIndex(), row, row);
    m_items.append({ m_nextId++, definition, position, 0.0 });
    endInsertRows();

    return m_items.last().id;
}

void Modifiers::setSeed(quint32 seed)
{
    m_random.seed(seed);
}

void Modifiers::setMatch(Match* match)
{
    if (m_match == match)
        return;

    m_match = match;
    emit matchChanged(match);
}

Match* Modifiers::match() const
{
    return m_match;
}

void Modifiers::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;

    m_enabled = enabled;
    emit enabledChanged(enabled);
}

bool Modifiers::isEnabled() const
{
    return m_enabled;
}

void Modifiers::setSpawnArea(const QRectF& spawnArea)
{
    if (m_spawnArea == spawnArea)
        return;

    m_spawnArea = spawnArea;
    emit spawnAreaChanged(spawnArea);
}

QRectF Modifiers::spawnArea() const
{
    return m_spawnArea;
}

void Modifiers::setObstacles(const QVariantList& obstacles)
{
    QList<QRectF> rects;
    for (const QVariant& obstacle : obstacles)
        rects.append(obstacle.toRectF());

    if (m_obstacles == rects)
        return;

    m_obstacles = rects;
    emit obstaclesChanged();
}

QVariantList Modifiers::obstacles() const
{
    QVariantList obstacles;
    for (const QRectF& rect : m_obstacles)
        obstacles.append(rect);
    return obstacles;
}

qreal Modifiers::fieldInset() const
{
    return m_fieldInset;
}

qreal Modifiers::maxFieldInset() const
{
    qreal inset = 0.0;
    for (const Definition& definition : m_definitions) {
        if (definition.effect == Effect::NarrowField)
            inset = std::max(inset, definition.value);
    }
    return inset;
}

Modifiers::Effects Modifiers::noEffects()
{
    return { 0.0, -1, 0.0, -1, -1, 0.0, -1 };
}

void Modifiers::spawnRandom()
{
    // Pick a definition by weight
    qreal total = 0.0;
    for (const Definition& definition : m_definitions)
        total += definition.weight;
    if (total <= 0.0)
        return;

    qreal pick = m_random.bounded(total);
    int definition = 0;
    while (definition < m_definitions.size() - 1 && pick >= m_definitions.at(definition).weight) {
        pick -= m_definitions.at(definition).weight;
        ++definition;
    }

    // Keep items apart so a single pass doesn't collect two
    for (int attempt = 0; attempt < 10; ++attempt) {
        const QVector2D position(m_spawnArea.left() + m_random.bounded(m_spawnArea.width()),
                                 m_spawnArea.top() + m_random.bounded(m_spawnArea.height()));

        const bool free = std::all_of(m_items.cbegin(), m_items.cend(), [&](const Item& item) {
            return item.position.distanceToPoint(position) >= m_spawn.minDistance;
        }) && std::none_of(m_obstacles.cbegin(), m_obstacles.cend(), [&](const QRectF& rect) {
            // Leave room for the item itself
            return rect.adjusted(-1.2, -1.2, 1.2, 1.2).contains(position.toPointF());
        });

        if (free) {
            spawn(definition, position);
            return;
        }
    }
}

void Modifiers::apply(int index, Match::Side collector, Ball* ball, const QVector2D& position)
{
    const Definition& definition = m_definitions.at(index);

    QList<Match::Side> sides;
    switch (definition.target) {
        case Target::Collector:
            sides = { collector };
            break;
        case Target::Opponent:
            sides = { Match::opponent(collector) };
            break;
        case Target::Both:
            sides = { Match::Side::LeftSide, Match::Side::RightSide };
            break;
    }

    // Effects on the ball and the field don't care about the target
    switch (definition.effect) {
        case Effect::BallSpeed:
            m_match->scaleBallSpeed(ball, definition.value);
            return;
        case Effect::MultiBall:
            // The new balls fly at the collector's opponent
            for (int i = 0; i < int(definition.value); ++i)
                m_match->addBall(position, Match::opponent(collector), definition.duration, collector);
            return;
        case Effect::NarrowField:
            m_narrowTime = definition.duration;
            setFieldInset(definition.value);
            return;
        default:
            break;
    }

    for (Match::Side side : sides) {
        Player* player = m_match->player(side);
        Effects& effects = this->effects(side);

        switch (definition.effect) {
            case Effect::PaddleSize:
                player->setPaddleScale(definition.value);
                effects.paddleTime = definition.duration;
                effects.paddleDefinition = index;
                break;
            case Effect::Shield:
                player->setShielded(true);
                effects.shieldDefinition = index;
                break;
            case Effect::Spin:
                player->setSpinSpeed(definition.value);
                effects.spinTime = definition.duration;
                effects.spinDefinition = index;
                break;
            case Effect::Magnet:
                player->setCatches(int(definition.value));
                effects.magnetTime = definition.duration;
                effects.magnetDefinition = index;
                break;
            default:
                break;
        }
    }

    emit effectsChanged();
}

void Modifiers::removeItem(int row)
{
    beginRemoveRows(QModelIndex(), row, row);
    m_items.removeAt(row);
    endRemoveRows();
}

void Modifiers::resetSpawnCountdown()
{
    m_spawnCountdown = m_spawn.minDelay + m_random.bounded(m_spawn.maxDelay - m_spawn.minDelay);
}

void Modifiers::setFieldInset(qreal fieldInset)
{
    if (m_fieldInset == fieldInset)
        return;

    m_fieldInset = fieldInset;
    emit fieldInsetChanged(fieldInset);
}

Modifiers::Effects& Modifiers::effects(Match::Side side)
{
    return side == Match::Side::LeftSide ? m_left : m_right;
}

const Modifiers::Effects& Modifiers::effects(Match::Side side) const
{
    return side == Match::Side::LeftSide ? m_left : m_right;
}
