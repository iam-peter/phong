#ifndef FONTATLAS_H
#define FONTATLAS_H

#include <QByteArray>
#include <QFont>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QRawFont>
#include <QVector2D>
#include <QtQml/qqmlregistration.h>
#include <QtQuick3D/qquick3dtexturedata.h>

class QJSEngine;
class QPainterPath;
class QQmlEngine;


// The letters of the game in one texture, for the text of a whole scene
// in one draw call, see TextBatch. Every glyph gets a cell with its signed
// distance field: 0.5 is the edge, more is inside. That keeps the edges
// sharp at any size and angle. Glyphs are added as texts need them.
class FontAtlas : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // The font of the texts, set once before they are laid out
    Q_PROPERTY(QString family READ family WRITE setFamily NOTIFY familyChanged)
    // The Texture with a FontAtlasTexture, set by the scene that has it
    Q_PROPERTY(QObject* texture READ texture WRITE setTexture NOTIFY textureChanged)
    // Size of a cell in the texture coordinates
    Q_PROPERTY(float cellUV READ cellUV CONSTANT)

public:
    static constexpr int atlasSize = 1024;
    static constexpr int cellSize = 64;
    // Pixels of the distance field on either side of the edge
    static constexpr int spread = 8;
    // Size of an em in the cells, where the baseline and the origin sit
    static constexpr int emPixels = 40;
    static constexpr int baseline = 44;
    static constexpr int originX = 8;
    // The glyphs are rendered this much larger for the distance field
    static constexpr int supersample = 4;
    // Texts are measured with this point size, positions come in points
    // like with ExtrudedTextGeometry
    static constexpr qreal pointSize = 20.0;

    // A glyph of a laid out text: where the middle of its cell goes, in
    // the units of Text3D, and the corner of the cell in the texture
    struct Placed {
        QVector2D center;
        QVector2D uv;
    };

    // The one atlas, QML gets the same
    static FontAtlas* instance();
    static FontAtlas* create(QQmlEngine*, QJSEngine*);

    ~FontAtlas() override;

    // The glyphs of text, capitalized like the texts of the game
    QList<Placed> layout(const QString& text);
    // Width and height of a cell in the units of Text3D
    float cellUnits() const;

    void setFamily(const QString& family);
    QString family() const;
    void setTexture(QObject* texture);
    QObject* texture() const;
    // The single channel texture, atlasSize by atlasSize
    const QByteArray& pixels() const;
    float cellUV() const;

    // The distance field of a path into a cell of size by size pixels,
    // origin in the cell. The path is in pixels times supersample.
    static QByteArray distanceField(const QPainterPath& path, int size, int spread, int supersample,
                                    const QPointF& origin);

signals:
    void familyChanged();
    void textureChanged();
    // New glyphs are in the texture
    void glyphsAdded();

private:
    explicit FontAtlas(QObject* parent = nullptr);

    struct Key {
        QString font;
        quint32 glyph;
        bool operator==(const Key& other) const { return font == other.font && glyph == other.glyph; }
    };
    friend size_t qHash(const Key& key, size_t seed) { return qHashMulti(seed, key.font, key.glyph); }

    int cellFor(const QRawFont& font, quint32 glyph);

    QString m_family;
    QFont m_font;
    float m_cellUnits;
    QByteArray m_pixels;
    QHash<Key, int> m_cells;
    int m_nextCell;
    bool m_warned;
    QPointer<QObject> m_texture;
};

// The pixels of the FontAtlas for a Texture, it follows new glyphs. It has
// to be created in the scene, one is enough.
class FontAtlasTexture : public QQuick3DTextureData
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit FontAtlasTexture(QQuick3DObject* parent = nullptr);
};

#endif // FONTATLAS_H
