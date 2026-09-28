#include "fontatlas.h"

#include <QPainterPath>
#include <QTest>

class tst_FontAtlas : public QObject
{
    Q_OBJECT

private slots:
    void distanceField()
    {
        // A square of 16 pixels in the middle of the cell, the path four
        // times larger for the supersampling
        QPainterPath square;
        square.addRect(QRectF(-32, -32, 64, 64));
        const QByteArray field = FontAtlas::distanceField(square, 32, 4, 4, QPointF(16, 16));
        QCOMPARE(field.size(), 32 * 32);
        const auto at = [&field](int x, int y) { return uchar(field.at(y * 32 + x)); };

        // Inside above the edge, outside below, far away at 0
        QVERIFY(at(16, 16) == 255);
        QCOMPARE(at(0, 0), uchar(0));
        QVERIFY(at(9, 16) > 128);
        QVERIFY(at(7, 16) < 128);
        QVERIFY(qAbs(int(at(8, 16)) - 128) < 40);
        // Falling off with the distance
        QVERIFY(at(6, 16) > at(5, 16));
    }

    void layout()
    {
        FontAtlas* atlas = FontAtlas::instance();
        atlas->setFamily(QStringLiteral("monospace"));
        QVERIFY(atlas->cellUnits() > 0.0f);

        // A glyph per letter, left to right, on one line, spaces left out
        const QList<FontAtlas::Placed> glyphs = atlas->layout(QStringLiteral("ab c"));
        QCOMPARE(glyphs.size(), 3);
        QVERIFY(glyphs.at(0).center.x() < glyphs.at(1).center.x());
        QVERIFY(glyphs.at(1).center.x() < glyphs.at(2).center.x());
        QCOMPARE(glyphs.at(0).center.y(), glyphs.at(2).center.y());

        // Capitalized like the texts of the game: the same cells as upper case
        const QList<FontAtlas::Placed> upper = atlas->layout(QStringLiteral("AB C"));
        QCOMPARE(upper.at(0).uv, glyphs.at(0).uv);
        QCOMPARE(upper.at(2).uv, glyphs.at(2).uv);
        QVERIFY(upper.at(0).uv != upper.at(1).uv);

        // The cells are in the texture: inside the strokes, a few pixels
        // thick, the field is clearly above the edge
        const QByteArray& pixels = atlas->pixels();
        QCOMPARE(pixels.size(), FontAtlas::atlasSize * FontAtlas::atlasSize);
        QVERIFY(std::any_of(pixels.cbegin(), pixels.cend(), [](char c) { return uchar(c) > 150; }));
    }
};

QTEST_MAIN(tst_FontAtlas)
#include "tst_fontatlas.moc"
