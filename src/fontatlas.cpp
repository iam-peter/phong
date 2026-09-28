#include "fontatlas.h"

#include <QGlyphRun>
#include <QImage>
#include <QJSEngine>
#include <QLoggingCategory>
#include <QPainter>
#include <QPainterPath>
#include <QTextLayout>
#include <QtQuick3D/qquick3dtexturedata.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

Q_LOGGING_CATEGORY(lcFontAtlas, "phong.fontatlas")

namespace {
constexpr float far = 1e20f;

// The squared distance transform of one row or column, Felzenszwalb and
// Huttenlocher: d[q] = min over p of (q - p)^2 + f[p]
void transform1d(const float* f, float* d, int n, std::vector<int>& v, std::vector<float>& z)
{
    int k = 0;
    v[0] = 0;
    z[0] = -far;
    z[1] = far;
    for (int q = 1; q < n; ++q) {
        float s = ((f[q] + float(q * q)) - (f[v[k]] + float(v[k] * v[k]))) / float(2 * q - 2 * v[k]);
        while (s <= z[k]) {
            --k;
            s = ((f[q] + float(q * q)) - (f[v[k]] + float(v[k] * v[k]))) / float(2 * q - 2 * v[k]);
        }
        ++k;
        v[k] = q;
        z[k] = s;
        z[k + 1] = far;
    }
    k = 0;
    for (int q = 0; q < n; ++q) {
        while (z[k + 1] < float(q))
            ++k;
        d[q] = float((q - v[k]) * (q - v[k])) + f[v[k]];
    }
}

// Squared distances to the nearest pixel where seed is set, in place
void transform2d(std::vector<float>& grid, int size)
{
    std::vector<float> f(size);
    std::vector<float> d(size);
    std::vector<int> v(size);
    std::vector<float> z(size + 1);
    for (int x = 0; x < size; ++x) {
        for (int y = 0; y < size; ++y)
            f[y] = grid[y * size + x];
        transform1d(f.data(), d.data(), size, v, z);
        for (int y = 0; y < size; ++y)
            grid[y * size + x] = d[y];
    }
    for (int y = 0; y < size; ++y) {
        transform1d(&grid[y * size], d.data(), size, v, z);
        std::copy(d.begin(), d.end(), grid.begin() + y * size);
    }
}

FontAtlas* s_instance = nullptr;
}

FontAtlas* FontAtlas::instance()
{
    if (!s_instance)
        s_instance = new FontAtlas();
    return s_instance;
}

FontAtlas* FontAtlas::create(QQmlEngine*, QJSEngine*)
{
    FontAtlas* atlas = instance();
    QJSEngine::setObjectOwnership(atlas, QJSEngine::CppOwnership);
    return atlas;
}

FontAtlas::FontAtlas(QObject* parent):
    QObject(parent),
    m_family(),
    m_font(),
    m_cellUnits(1.0f),
    m_pixels(atlasSize * atlasSize, char(0)),
    m_cells(),
    m_nextCell(0),
    m_warned(false),
    m_texture()
{
    setFamily(QString());
}

FontAtlas::~FontAtlas()
{
    if (s_instance == this)
        s_instance = nullptr;
}

QList<FontAtlas::Placed> FontAtlas::layout(const QString& text)
{
    QList<Placed> placed;
    if (text.isEmpty())
        return placed;

    QTextLayout layout(text.toUpper(), m_font);
    layout.beginLayout();
    QTextLine line = layout.createLine();
    line.setLineWidth(1e6);
    layout.endLayout();

    const int before = m_nextCell;
    const qreal ascent = line.ascent();
    for (const QGlyphRun& run : layout.glyphRuns()) {
        const QRawFont font = run.rawFont();
        // Units per em of this font, a fallback may differ
        const float em = float(font.pixelSize() / pointSize);
        const QList<quint32> glyphs = run.glyphIndexes();
        const QList<QPointF> positions = run.positions();
        for (qsizetype i = 0; i < glyphs.size(); ++i) {
            const int cell = cellFor(font, glyphs.at(i));
            if (cell < 0)
                continue;
            // The cell's middle from the glyph's origin on the baseline
            const float cx = float(cellSize / 2 - originX) / emPixels * em;
            const float cy = float(baseline - cellSize / 2) / emPixels * em;
            const QPointF at = positions.at(i);
            placed.append({ QVector2D(float(at.x() / pointSize) + cx, float((ascent - at.y()) / pointSize) + cy),
                            QVector2D(float((cell % (atlasSize / cellSize)) * cellSize) / atlasSize,
                                      float((cell / (atlasSize / cellSize)) * cellSize) / atlasSize) });
        }
    }

    if (m_nextCell != before)
        emit glyphsAdded();
    return placed;
}

float FontAtlas::cellUnits() const
{
    return m_cellUnits;
}

void FontAtlas::setFamily(const QString& family)
{
    if (m_family == family && !m_family.isEmpty())
        return;

    m_family = family;
    m_font = family.isEmpty() ? QFont() : QFont(family);
    m_font.setPointSizeF(pointSize);
    const QRawFont raw = QRawFont::fromFont(m_font);
    m_cellUnits = float(cellSize) / emPixels * float((raw.isValid() ? raw.pixelSize() : pointSize) / pointSize);
    emit familyChanged();
}

QString FontAtlas::family() const
{
    return m_family;
}

void FontAtlas::setTexture(QObject* texture)
{
    if (m_texture == texture)
        return;

    m_texture = texture;
    emit textureChanged();
}

QObject* FontAtlas::texture() const
{
    return m_texture;
}

const QByteArray& FontAtlas::pixels() const
{
    return m_pixels;
}

float FontAtlas::cellUV() const
{
    return float(cellSize) / atlasSize;
}

int FontAtlas::cellFor(const QRawFont& font, quint32 glyph)
{
    const Key key{ font.familyName() + QLatin1Char('/') + font.styleName(), glyph };
    const auto found = m_cells.constFind(key);
    if (found != m_cells.cend())
        return found.value();

    // The outline at the size of the distance field, times the supersampling
    QRawFont raster = font;
    raster.setPixelSize(emPixels * supersample);
    const QPainterPath path = raster.pathForGlyph(glyph);
    // Spaces have nothing to draw
    if (path.isEmpty()) {
        m_cells.insert(key, -1);
        return -1;
    }

    const int cells = (atlasSize / cellSize) * (atlasSize / cellSize);
    if (m_nextCell >= cells) {
        if (!m_warned)
            qCWarning(lcFontAtlas) << "The font atlas is full, further glyphs are left out";
        m_warned = true;
        return -1;
    }

    const QByteArray field = distanceField(path, cellSize, spread, supersample, QPointF(originX, baseline));

    const int cell = m_nextCell++;
    const int column = cell % (atlasSize / cellSize);
    const int row = cell / (atlasSize / cellSize);
    for (int y = 0; y < cellSize; ++y) {
        std::copy_n(field.constData() + y * cellSize, cellSize,
                    m_pixels.data() + (row * cellSize + y) * atlasSize + column * cellSize);
    }
    m_cells.insert(key, cell);
    return cell;
}

QByteArray FontAtlas::distanceField(const QPainterPath& path, int size, int spread, int supersample,
                                    const QPointF& origin)
{
    const int large = size * supersample;
    QImage image(large, large, QImage::Format_Grayscale8);
    image.fill(0);
    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.translate(origin * supersample);
        painter.fillPath(path, Qt::white);
    }

    // Squared distances to the nearest inside and outside pixel
    std::vector<float> toInside(large * large);
    std::vector<float> toOutside(large * large);
    for (int y = 0; y < large; ++y) {
        const uchar* line = image.constScanLine(y);
        for (int x = 0; x < large; ++x) {
            const bool inside = line[x] >= 128;
            toInside[y * large + x] = inside ? 0.0f : far;
            toOutside[y * large + x] = inside ? far : 0.0f;
        }
    }
    transform2d(toInside, large);
    transform2d(toOutside, large);

    // Signed, positive inside, averaged over each pixel of the cell and
    // mapped so that 0.5 is the edge and spread pixels reach 0 and 1
    QByteArray field(size * size, char(0));
    const float scale = 1.0f / float(supersample * supersample);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float sum = 0.0f;
            for (int dy = 0; dy < supersample; ++dy) {
                for (int dx = 0; dx < supersample; ++dx) {
                    const int i = (y * supersample + dy) * large + x * supersample + dx;
                    sum += toOutside[i] > 0.0f ? std::sqrt(toOutside[i]) - 0.5f : 0.5f - std::sqrt(toInside[i]);
                }
            }
            const float distance = sum * scale / float(supersample);
            const float value = std::clamp(0.5f + distance / float(2 * spread), 0.0f, 1.0f);
            field[y * size + x] = char(uchar(std::lround(value * 255.0f)));
        }
    }
    return field;
}

FontAtlasTexture::FontAtlasTexture(QQuick3DObject* parent):
    QQuick3DTextureData(parent)
{
    setFormat(QQuick3DTextureData::R8);
    setSize(QSize(FontAtlas::atlasSize, FontAtlas::atlasSize));
    setHasTransparency(false);
    FontAtlas* atlas = FontAtlas::instance();
    setTextureData(atlas->pixels());
    connect(atlas, &FontAtlas::glyphsAdded, this, [this, atlas] { setTextureData(atlas->pixels()); });
}
