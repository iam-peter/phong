#include "keysettings.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

class tst_KeySettings : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("phong-tests"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_keysettings"));
    }

    void cleanup()
    {
        KeySettings().restoreDefaults();
    }

    void defaults()
    {
        KeySettings keys;
        QCOMPARE(keys.key(KeySettings::LeftUp), int(Qt::Key_W));
        QCOMPARE(keys.key(KeySettings::RightSpecial), int(Qt::Key_Right));
        QCOMPARE(keys.action(Qt::Key_D), int(KeySettings::LeftSmash));
        QCOMPARE(keys.action(Qt::Key_Q), -1);
        QCOMPARE(keys.keyName(Qt::Key_Up), QStringLiteral("Up"));
        QCOMPARE(keys.keyName(Qt::Key_W), QStringLiteral("W"));
    }

    void assignedKeysSwap()
    {
        KeySettings keys;
        QSignalSpy changed(&keys, &KeySettings::changed);

        // A free key
        QVERIFY(keys.setKey(KeySettings::LeftSmash, Qt::Key_E));
        QCOMPARE(keys.key(KeySettings::LeftSmash), int(Qt::Key_E));
        QCOMPARE(changed.count(), 1);

        // A key in use: the other action gets the old one
        QVERIFY(keys.setKey(KeySettings::LeftUp, Qt::Key_S));
        QCOMPARE(keys.key(KeySettings::LeftUp), int(Qt::Key_S));
        QCOMPARE(keys.key(KeySettings::LeftDown), int(Qt::Key_W));

        // Menu keys stay with the menus
        QVERIFY(!keys.setKey(KeySettings::Pause, Qt::Key_Escape));
        QVERIFY(!keys.setKey(KeySettings::Pause, Qt::Key_Return));
        QCOMPARE(keys.key(KeySettings::Pause), int(Qt::Key_P));

        // Kept
        KeySettings other;
        QCOMPARE(other.key(KeySettings::LeftSmash), int(Qt::Key_E));
        QCOMPARE(other.key(KeySettings::LeftDown), int(Qt::Key_W));

        other.restoreDefaults();
        QCOMPARE(other.key(KeySettings::LeftSmash), int(Qt::Key_D));
    }
};

QTEST_GUILESS_MAIN(tst_KeySettings)
#include "tst_keysettings.moc"
