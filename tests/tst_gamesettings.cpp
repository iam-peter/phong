#include "gamesettings.h"

#include <QCoreApplication>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

class tst_GameSettings : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("phong-tests"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_gamesettings"));
        QSettings().clear();
    }

    void cleanup()
    {
        QSettings().clear();
    }

    void ruleDefaults()
    {
        GameSettings settings;
        const QVariantMap rules = settings.rules(0);

        QCOMPARE(rules.value("pointsToWin").toInt(), 5);
        QCOMPARE(rules.value("setsToWin").toInt(), 1);
        QCOMPARE(rules.value("ballSpeed").toInt(), int(GameSettings::BallSpeed::Medium));
        QCOMPARE(rules.value("serveSpeed").toReal(), 16.0);
        QCOMPARE(rules.value("maxSpeed").toReal(), 32.0);
        QCOMPARE(rules.value("paddleLength").toReal(), 4.0);
    }

    void rulesAreIsolatedByMode()
    {
        GameSettings settings;
        settings.setRule(0, QStringLiteral("pointsToWin"), 3);
        settings.setRule(3, QStringLiteral("pointsToWin"), 11);

        QCOMPARE(settings.rules(0).value("pointsToWin").toInt(), 3);
        QCOMPARE(settings.rules(3).value("pointsToWin").toInt(), 11);

        GameSettings again;
        QCOMPARE(again.rules(0).value("pointsToWin").toInt(), 3);
        QCOMPARE(again.rules(3).value("pointsToWin").toInt(), 11);
    }

    void validatesAndComputesRules()
    {
        GameSettings settings;
        QSignalSpy changed(&settings, &GameSettings::rulesChanged);

        settings.setRule(2, QStringLiteral("pointsToWin"), -4);
        settings.setRule(2, QStringLiteral("kickoffTime"), 99);
        settings.setRule(2, QStringLiteral("ballSpeed"), GameSettings::BallSpeed::Fast);
        settings.setRule(2, QStringLiteral("paddleSize"), GameSettings::PaddleSize::Large);

        const QVariantMap rules = settings.rules(2);
        QCOMPARE(rules.value("pointsToWin").toInt(), GameSettings::minPointsToWin);
        QCOMPARE(rules.value("kickoffTime").toInt(), GameSettings::maxKickoffTime);
        QCOMPARE(rules.value("serveSpeed").toReal(), 21.0);
        QCOMPARE(rules.value("maxSpeed").toReal(), 42.0);
        QCOMPARE(rules.value("paddleLength").toReal(), 5.0);
        QCOMPARE(changed.count(), 4);
    }

    void restoresOneMode()
    {
        GameSettings settings;
        settings.setRule(0, QStringLiteral("pointsToWin"), 21);
        settings.setRule(1, QStringLiteral("pointsToWin"), 11);

        settings.restoreRules(0);

        QCOMPARE(settings.rules(0).value("pointsToWin").toInt(), 5);
        QCOMPARE(settings.rules(1).value("pointsToWin").toInt(), 11);
    }
};

QTEST_GUILESS_MAIN(tst_GameSettings)
#include "tst_gamesettings.moc"