#include "modifiers.h"

#include <algorithm>

Modifiers::Modifiers(QObject* parent):
    QAbstractListModel(parent),
    m_match(nullptr),
    m_enabled(true),
    m_spawnArea(-8.0, -4.5, 16.0, 9.0),
    m_items(),
    m_nextId(1),
    m_spawnCountdown(0.0),
    m_left({ 0.0, 0.0 }),
    m_right({ 0.0, 0.0 }),
    m_narrowTime(0.0),
    m_fieldNarrowed(false),
    m_random(QRandomGenerator::global()->generate())
{
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
    switch (role) {
        case Role::KindRole:
            return int(item.kind);
        case Role::ItemIdRole:
            return item.id;
        case Role::ItemXRole:
            return item.position.x();
        case Role::ItemYRole:
            return item.position.y();
        default:
            return QVariant();
    }
}

QHash<int, QByteArray> Modifiers::roleNames() const
{
    return {
        { Role::KindRole, "kind" },
        { Role::ItemIdRole, "itemId" },
        { Role::ItemXRole, "itemX" },
        { Role::ItemYRole, "itemY" }
    };
}

void Modifiers::reset()
{
    beginResetModel();
    m_items.clear();
    endResetModel();

    m_left = { 0.0, 0.0 };
    m_right = { 0.0, 0.0 };
    m_narrowTime = 0.0;
    setFieldNarrowed(false);

    if (m_match) {
        for (Player* player : { m_match->left(), m_match->right() }) {
            player->setPaddleScale(1.0);
            player->setSpinning(false);
            player->setShielded(false);
        }
    }

    resetSpawnCountdown();
}

void Modifiers::advance(qreal dt)
{
    if (!m_match)
        return;

    // Effects run out
    for (Match::Side side : { Match::Side::LeftSide, Match::Side::RightSide }) {
        Effects& effects = this->effects(side);
        Player* player = m_match->player(side);

        if (effects.paddleTime > 0.0) {
            effects.paddleTime -= dt;
            if (effects.paddleTime <= 0.0)
                player->setPaddleScale(1.0);
        }

        if (effects.spinTime > 0.0) {
            effects.spinTime -= dt;
            if (effects.spinTime <= 0.0)
                player->setSpinning(false);
        }
    }

    if (m_narrowTime > 0.0) {
        m_narrowTime -= dt;
        if (m_narrowTime <= 0.0)
            setFieldNarrowed(false);
    }

    // Items only come and go while the ball is in play
    if (m_match->state() != Match::State::Playing)
        return;

    for (int row = int(m_items.size()) - 1; row >= 0; --row) {
        m_items[row].age += dt;
        if (m_items[row].age >= itemLifetime)
            removeItem(row);
    }

    if (!m_enabled)
        return;

    m_spawnCountdown -= dt;
    if (m_spawnCountdown <= 0.0) {
        if (m_items.size() < maxItems)
            spawnRandom();
        resetSpawnCountdown();
    }
}

bool Modifiers::collect(int itemId)
{
    if (!m_match || m_match->lastTouch() == Match::Side::NoSide)
        return false;

    const auto it = std::find_if(m_items.cbegin(), m_items.cend(),
                                 [itemId](const Item& item) { return item.id == itemId; });
    if (it == m_items.cend())
        return false;

    const Item item = *it;
    removeItem(int(it - m_items.cbegin()));

    const Match::Side collector = m_match->lastTouch();
    apply(item.kind, collector);

    const Match::Side affected = isCurse(item.kind) ? Match::opponent(collector) : collector;
    emit collected(item.kind, affected, item.position);
    return true;
}

bool Modifiers::shieldHit(Match::Side side)
{
    Player* player = m_match ? m_match->player(side) : nullptr;
    if (!player || !player->isShielded())
        return false;

    if (!m_match->shieldHit(side))
        return false;

    player->setShielded(false);
    return true;
}

bool Modifiers::isCurse(Kind kind)
{
    return kind == Kind::SmallPaddle || kind == Kind::SpinPaddle;
}

int Modifiers::spawn(Kind kind, const QVector2D& position)
{
    const int row = int(m_items.size());
    beginInsertRows(QModelIndex(), row, row);
    m_items.append({ m_nextId++, kind, position, 0.0 });
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

bool Modifiers::isFieldNarrowed() const
{
    return m_fieldNarrowed;
}

void Modifiers::spawnRandom()
{
    const Kind kind = Kind(m_random.bounded(int(Kind::NarrowField) + 1));

    // Keep items apart so a single pass doesn't collect two
    for (int attempt = 0; attempt < 10; ++attempt) {
        const QVector2D position(m_spawnArea.left() + m_random.bounded(m_spawnArea.width()),
                                 m_spawnArea.top() + m_random.bounded(m_spawnArea.height()));

        const bool free = std::all_of(m_items.cbegin(), m_items.cend(), [&](const Item& item) {
            return item.position.distanceToPoint(position) >= minItemDistance;
        });

        if (free) {
            spawn(kind, position);
            return;
        }
    }
}

void Modifiers::apply(Kind kind, Match::Side side)
{
    const Match::Side opponent = Match::opponent(side);

    switch (kind) {
        case Kind::FastBall:
            m_match->scaleBallSpeed(fastBallFactor);
            break;
        case Kind::BigPaddle:
            m_match->player(side)->setPaddleScale(bigPaddleScale);
            effects(side).paddleTime = paddleDuration;
            break;
        case Kind::Shield:
            m_match->player(side)->setShielded(true);
            break;
        case Kind::SmallPaddle:
            m_match->player(opponent)->setPaddleScale(smallPaddleScale);
            effects(opponent).paddleTime = paddleDuration;
            break;
        case Kind::SpinPaddle:
            m_match->player(opponent)->setSpinning(true);
            effects(opponent).spinTime = spinDuration;
            break;
        case Kind::NarrowField:
            m_narrowTime = narrowDuration;
            setFieldNarrowed(true);
            break;
    }
}

void Modifiers::removeItem(int row)
{
    beginRemoveRows(QModelIndex(), row, row);
    m_items.removeAt(row);
    endRemoveRows();
}

void Modifiers::resetSpawnCountdown()
{
    m_spawnCountdown = minSpawnDelay + m_random.bounded(maxSpawnDelay - minSpawnDelay);
}

void Modifiers::setFieldNarrowed(bool fieldNarrowed)
{
    if (m_fieldNarrowed == fieldNarrowed)
        return;

    m_fieldNarrowed = fieldNarrowed;
    emit fieldNarrowedChanged(fieldNarrowed);
}

Modifiers::Effects& Modifiers::effects(Match::Side side)
{
    return side == Match::Side::LeftSide ? m_left : m_right;
}
