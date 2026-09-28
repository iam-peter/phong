#include "graphicssettings.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

class tst_GraphicsSettings : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("phong-tests"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_graphicssettings"));
    }

    void cleanup()
    {
        GraphicsSettings().restoreDefaults();
    }

    void persisted()
    {
        GraphicsSettings settings;
        QSignalSpy glow(&settings, &GraphicsSettings::glowChanged);

        settings.setShading(GraphicsSettings::Shading::Flat);
        settings.setGlow(GraphicsSettings::Glow::NoGlow);
        settings.setAntialiasing(GraphicsSettings::Antialiasing::Multisample2x);
        settings.setStars(false);
        settings.setShowFps(true);
        QCOMPARE(glow.count(), 1);

        // Setting the same value again changes nothing
        settings.setGlow(GraphicsSettings::Glow::NoGlow);
        QCOMPARE(glow.count(), 1);

        GraphicsSettings again;
        QCOMPARE(again.shading(), GraphicsSettings::Shading::Flat);
        QCOMPARE(again.glow(), GraphicsSettings::Glow::NoGlow);
        QCOMPARE(again.antialiasing(), GraphicsSettings::Antialiasing::Multisample2x);
        QVERIFY(!again.stars());
        QVERIFY(again.showFps());
    }

    void defaults()
    {
        GraphicsSettings settings;
        settings.setShadows(false);
        settings.restoreDefaults();
        QCOMPARE(settings.shading(), GraphicsSettings::Shading::Phong);
        QVERIFY(settings.shadows());
        QVERIFY(settings.floor());
        QVERIFY(!settings.showFps());
    }
};

QTEST_GUILESS_MAIN(tst_GraphicsSettings)
#include "tst_graphicssettings.moc"
