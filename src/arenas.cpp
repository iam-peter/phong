#include "arenas.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

#include <algorithm>

Q_LOGGING_CATEGORY(lcArenas, "phong.arenas")

namespace {
const QString builtInSource = QStringLiteral(":/qt/qml/Phong/config/arenas.json");
QString s_defaultSource = builtInSource;

// Obstacles stay between the walls and paddles and away from the serve spot
const QRectF field(-10.0, -9.5, 20.0, 19.0);
constexpr qreal serveClearance = 1.8;

bool blocksServe(const QRectF& rect)
{
    return rect.adjusted(-serveClearance, -serveClearance, serveClearance, serveClearance)
        .contains(QPointF(0.0, 0.0));
}
}

QString Arenas::defaultSource()
{
    return s_defaultSource;
}

void Arenas::setDefaultSource(const QString& fileName)
{
    s_defaultSource = fileName;
}

Arenas::Arenas(QObject* parent):
    QObject(parent),
    m_arenas(),
    m_random(QRandomGenerator::global()->generate())
{
    // A broken custom configuration shouldn't take the arenas away
    if (!load(s_defaultSource) && s_defaultSource != builtInSource) {
        qCWarning(lcArenas) << "Using the built-in arenas instead";
        load(builtInSource);
    }
}

bool Arenas::load(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcArenas) << "Cannot read" << fileName << file.errorString();
        return false;
    }

    QString error;
    if (!loadJson(file.readAll(), &error)) {
        qCWarning(lcArenas).noquote() << fileName << error;
        return false;
    }

    return true;
}

bool Arenas::loadJson(const QByteArray& json, QString* error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (error)
            *error = QStringLiteral("offset %1: %2").arg(parseError.offset).arg(parseError.errorString());
        return false;
    }

    if (!document.isObject() || !document.object().value("arenas").isArray()) {
        if (error)
            *error = QStringLiteral("expected an object with an \"arenas\" array");
        return false;
    }

    QList<Arena> arenas;
    const QJsonArray entries = document.object().value("arenas").toArray();
    for (const QJsonValue& value : entries) {
        const QJsonObject entry = value.toObject();

        Arena arena;
        arena.id = entry.value("id").toString();
        if (arena.id.isEmpty() || std::any_of(arenas.cbegin(), arenas.cend(),
                                              [&](const Arena& other) { return other.id == arena.id; })) {
            qCWarning(lcArenas) << "Skipping arena without a unique id";
            continue;
        }
        arena.name = entry.value("name").toString(arena.id);

        for (const QJsonValue& bumperValue : entry.value("bumpers").toArray()) {
            const QJsonObject object = bumperValue.toObject();
            const Bumper bumper = { object.value("x").toDouble(), object.value("y").toDouble(),
                                    std::clamp(object.value("radius").toDouble(1.0), 0.3, 3.0) };
            const QRectF rect(bumper.x - bumper.radius, bumper.y - bumper.radius,
                              2.0 * bumper.radius, 2.0 * bumper.radius);
            if (!field.contains(rect) || blocksServe(rect)) {
                qCWarning(lcArenas) << "Skipping bumper of" << arena.id << "outside the field or on the serve spot";
                continue;
            }
            arena.bumpers.append(bumper);
        }

        for (const QJsonValue& blockValue : entry.value("blocks").toArray()) {
            const QJsonObject object = blockValue.toObject();
            const qreal width = std::clamp(object.value("width").toDouble(1.0), 0.2, 10.0);
            const qreal height = std::clamp(object.value("height").toDouble(1.0), 0.2, 10.0);
            const QRectF rect(object.value("x").toDouble() - 0.5 * width,
                              object.value("y").toDouble() - 0.5 * height, width, height);
            if (!field.contains(rect) || blocksServe(rect)) {
                qCWarning(lcArenas) << "Skipping block of" << arena.id << "outside the field or on the serve spot";
                continue;
            }
            arena.blocks.append(rect);
        }

        arenas.append(arena);
    }

    m_arenas = arenas;
    emit arenasChanged();
    return true;
}

const QList<Arenas::Arena>& Arenas::list() const
{
    return m_arenas;
}

QVariantList Arenas::arenas() const
{
    QVariantList arenas;
    for (const Arena& arena : m_arenas)
        arenas.append(toMap(arena));
    return arenas;
}

QVariantMap Arenas::arena(const QString& id) const
{
    const int index = find(id);
    return index < 0 ? QVariantMap() : toMap(m_arenas.at(index));
}

QString Arenas::randomId()
{
    return m_arenas.isEmpty() ? QString() : m_arenas.at(m_random.bounded(int(m_arenas.size()))).id;
}

QVariantList Arenas::obstacleRects(const QString& id) const
{
    QVariantList rects;
    const int index = find(id);
    if (index < 0)
        return rects;

    const Arena& arena = m_arenas.at(index);
    for (const Bumper& bumper : arena.bumpers)
        rects.append(QRectF(bumper.x - bumper.radius, bumper.y - bumper.radius,
                            2.0 * bumper.radius, 2.0 * bumper.radius));
    for (const QRectF& block : arena.blocks)
        rects.append(block);
    return rects;
}

int Arenas::find(const QString& id) const
{
    const auto it = std::find_if(m_arenas.cbegin(), m_arenas.cend(),
                                 [&id](const Arena& arena) { return arena.id == id; });
    return it == m_arenas.cend() ? -1 : int(it - m_arenas.cbegin());
}

QVariantMap Arenas::toMap(const Arena& arena)
{
    QVariantList bumpers;
    for (const Bumper& bumper : arena.bumpers)
        bumpers.append(QVariantMap{ { "x", bumper.x }, { "y", bumper.y }, { "radius", bumper.radius } });

    QVariantList blocks;
    for (const QRectF& block : arena.blocks)
        blocks.append(QVariantMap{ { "x", block.center().x() }, { "y", block.center().y() },
                                   { "width", block.width() }, { "height", block.height() } });

    return {
        { "id", arena.id },
        { "name", arena.name },
        { "bumpers", bumpers },
        { "blocks", blocks }
    };
}
