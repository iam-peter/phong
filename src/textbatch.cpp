#include "textbatch.h"

#include <QColor>
#include <QMatrix4x4>
#include <QMetaProperty>

#include <algorithm>
#include <cmath>

TextBatch::TextBatch(QQuick3DObject* parent):
    QQuick3DInstancing(parent),
    m_origin(),
    m_entries()
{
    setHasTransparency(true);
    // A new font lays everything out again
    connect(FontAtlas::instance(), &FontAtlas::familyChanged, this, [this] {
        for (Entry& entry : m_entries) {
            if (entry.text)
                entry.glyphs = FontAtlas::instance()->layout(entry.text->property("text").toString());
        }
        markDirty();
    });
}

void TextBatch::add(QQuick3DObject* text)
{
    if (!text || std::any_of(m_entries.cbegin(), m_entries.cend(), [text](const Entry& e) { return e.text == text; }))
        return;

    Entry entry;
    entry.text = text;
    follow(entry, text, "text", SLOT(textChanged()));
    for (const char* property : { "color", "glow", "depth", "alignX", "alignY", "sceneTransform" })
        follow(entry, text, property, SLOT(changed()));
    // Hidden or faded with an ancestor too
    for (QQuick3DObject* node = text; node && node != m_origin; node = node->parentItem()) {
        follow(entry, node, "visible", SLOT(changed()));
        follow(entry, node, "opacity", SLOT(changed()));
    }
    entry.connections.append(connect(text, &QObject::destroyed, this, [this, text] { remove(text); }));
    // Laid out here rather than while rendering, new glyphs change the atlas
    entry.glyphs = FontAtlas::instance()->layout(text->property("text").toString());
    entry.laidOut = true;
    m_entries.append(entry);
    markDirty();
}

void TextBatch::remove(QQuick3DObject* text)
{
    for (qsizetype i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).text == text || m_entries.at(i).text.isNull()) {
            disconnectEntry(m_entries[i]);
            m_entries.removeAt(i--);
        }
    }
    markDirty();
}

void TextBatch::setOrigin(QQuick3DObject* origin)
{
    if (m_origin == origin)
        return;

    m_origin = origin;
    emit originChanged();
    markDirty();
}

QQuick3DObject* TextBatch::origin() const
{
    return m_origin;
}

void TextBatch::textChanged()
{
    for (Entry& entry : m_entries) {
        if (entry.text == sender()) {
            entry.glyphs = FontAtlas::instance()->layout(entry.text->property("text").toString());
            entry.laidOut = true;
        }
    }
    markDirty();
}

void TextBatch::changed()
{
    markDirty();
}

void TextBatch::follow(Entry& entry, QObject* object, const char* property, const char* slot)
{
    const QMetaObject* meta = object->metaObject();
    const int index = meta->indexOfProperty(property);
    if (index < 0 || !meta->property(index).hasNotifySignal())
        return;
    const QMetaMethod signal = meta->property(index).notifySignal();
    const QMetaMethod handler = metaObject()->method(metaObject()->indexOfSlot(QMetaObject::normalizedSignature(slot + 1)));
    entry.connections.append(connect(object, signal, this, handler));
}

void TextBatch::disconnectEntry(Entry& entry)
{
    for (const QMetaObject::Connection& connection : std::as_const(entry.connections))
        disconnect(connection);
    entry.connections.clear();
}

bool TextBatch::shown(QQuick3DObject* text, float* opacity) const
{
    float product = 1.0f;
    for (QQuick3DObject* node = text; node && node != m_origin; node = node->parentItem()) {
        if (!node->property("visible").toBool())
            return false;
        const QVariant value = node->property("opacity");
        if (value.isValid())
            product *= value.toFloat();
    }
    *opacity = product;
    return product > 0.01f;
}

QByteArray TextBatch::getInstanceBuffer(int* instanceCount)
{
    FontAtlas* atlas = FontAtlas::instance();
    const QMatrix4x4 originInverse = m_origin ? m_origin->property("sceneTransform").value<QMatrix4x4>().inverted()
                                              : QMatrix4x4();
    // The quad of #Rectangle is 100 units wide, a cell is cellUnits
    const float quadScale = atlas->cellUnits() / 100.0f;

    QList<InstanceTableEntry> table;
    for (Entry& entry : m_entries) {
        QQuick3DObject* text = entry.text;
        float opacity = 1.0f;
        if (!text || !shown(text, &opacity))
            continue;

        if (!entry.laidOut) {
            entry.glyphs = atlas->layout(text->property("text").toString());
            entry.laidOut = true;
        }
        if (entry.glyphs.isEmpty())
            continue;

        const QMatrix4x4 transform = originInverse * text->property("sceneTransform").value<QMatrix4x4>();
        QColor color = text->property("color").value<QColor>();
        color.setAlphaF(color.alphaF() * opacity);
        QColor side = QColor::fromRgbF(color.redF() * sideShade, color.greenF() * sideShade,
                                       color.blueF() * sideShade, color.alphaF());
        const float glow = text->property("glow").toFloat();
        const float depth = std::max(text->property("depth").toFloat(), 0.0f);
        const float alignX = text->property("alignX").toFloat();
        const float alignY = text->property("alignY").toFloat();
        // The copies from the back at 0 to the front at depth, the front
        // last so it covers them
        const int layers = std::max(2, int(std::ceil(depth / layerStep)) + 1);

        const InstanceTableEntry front = calculateTableEntry(QVector3D(), QVector3D(1, 1, 1), QVector3D(), color);
        const InstanceTableEntry back = calculateTableEntry(QVector3D(), QVector3D(1, 1, 1), QVector3D(), side);
        for (int layer = 0; layer < layers; ++layer) {
            const bool isFront = layer == layers - 1;
            const float z = depth * float(layer) / float(layers - 1);
            for (const FontAtlas::Placed& glyph : std::as_const(entry.glyphs)) {
                QMatrix4x4 matrix = transform;
                matrix.translate(alignX + glyph.center.x(), alignY + glyph.center.y(), z);
                matrix.scale(quadScale);
                InstanceTableEntry instance = isFront ? front : back;
                instance.row0 = matrix.row(0);
                instance.row1 = matrix.row(1);
                instance.row2 = matrix.row(2);
                instance.instanceData = QVector4D(glyph.uv.x(), glyph.uv.y(), isFront ? glow : glow * sideShade, 0.0f);
                table.append(instance);
            }
        }
    }

    // Without instances Qt draws the quad once as it is, one of no size
    // draws nothing
    if (table.isEmpty()) {
        InstanceTableEntry nothing = calculateTableEntry(QVector3D(), QVector3D(0, 0, 0), QVector3D(), Qt::transparent);
        table.append(nothing);
    }

    const int count = int(table.size());
    if (instanceCount)
        *instanceCount = count;
    return QByteArray(reinterpret_cast<const char*>(table.constData()), count * qsizetype(sizeof(InstanceTableEntry)));
}
