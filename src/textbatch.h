#ifndef TEXTBATCH_H
#define TEXTBATCH_H

#include "fontatlas.h"

#include <QList>
#include <QPointer>
#include <QtQml/qqmlregistration.h>
#include <QtQuick3D/qquick3dinstancing.h>

// All the texts of a scene as one instanced model: every glyph is a quad
// with its cell of the FontAtlas, placed where its Text3D is, relative to
// origin. A few darker copies behind it give the text its depth, like the
// sides of the extruded text did. The texts register themselves, the
// batch follows their text, color, glow, depth, alignment, transform,
// visibility and opacity, those of their ancestors up to origin included.
class TextBatch : public QQuick3DInstancing
{
    Q_OBJECT
    QML_ELEMENT
    // The node the model with this table sits in, untransformed
    Q_PROPERTY(QQuick3DObject* origin READ origin WRITE setOrigin NOTIFY originChanged)

public:
    // Distance between two of the copies that make the depth
    static constexpr float layerStep = 0.07f;
    // How bright the copies behind the front are
    static constexpr float sideShade = 0.45f;

    explicit TextBatch(QQuick3DObject* parent = nullptr);

    // text is a Text3D: text, color, glow, depth, alignX and alignY
    Q_INVOKABLE void add(QQuick3DObject* text);
    Q_INVOKABLE void remove(QQuick3DObject* text);

    void setOrigin(QQuick3DObject* origin);
    QQuick3DObject* origin() const;

signals:
    void originChanged();

protected:
    QByteArray getInstanceBuffer(int* instanceCount) override;

private slots:
    void textChanged();
    void changed();

private:
    struct Entry {
        QPointer<QQuick3DObject> text;
        QList<QMetaObject::Connection> connections;
        QList<FontAtlas::Placed> glyphs;
        bool laidOut = false;
    };

    void follow(Entry& entry, QObject* object, const char* property, const char* slot);
    void disconnectEntry(Entry& entry);
    // Visible up to origin, and the opacity there
    bool shown(QQuick3DObject* text, float* opacity) const;

    QPointer<QQuick3DObject> m_origin;
    QList<Entry> m_entries;
};

#endif // TEXTBATCH_H
