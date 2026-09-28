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

    void padButtons()
    {
        KeySettings keys;
        QCOMPARE(keys.padButton(KeySettings::PadSmash), 0);
        QCOMPARE(keys.padButtonNames().at(KeySettings::PadSpecial), QStringLiteral("B"));
        QCOMPARE(keys.padAction(5), int(KeySettings::PadPause));

        // A shoulder button for the special, the smash takes over B
        QVERIFY(keys.setPadButton(KeySettings::PadSpecial, 6));
        QCOMPARE(keys.padButtonNames().at(KeySettings::PadSpecial), QStringLiteral("LB"));
        QVERIFY(keys.setPadButton(KeySettings::PadSmash, 6));
        QCOMPARE(keys.padButton(KeySettings::PadSpecial), 0);

        // The d-pad and Back belong to moving and the menus
        QVERIFY(!keys.setPadButton(KeySettings::PadDash, 8));
        QVERIFY(!keys.setPadButton(KeySettings::PadDash, 4));
        QCOMPARE(KeySettings().padButton(KeySettings::PadSmash), 6);

        keys.restoreDefaults();
        QCOMPARE(keys.padButton(KeySettings::PadSmash), 0);
    }

    void partyKeys()
    {
        KeySettings keys;
        QCOMPARE(keys.partyKey(KeySettings::PartyOneLeft), int(Qt::Key_A));
        QCOMPARE(keys.partyAction(Qt::Key_Up), int(KeySettings::PartyTwoUp));
        QCOMPARE(keys.partyKeyNames().at(KeySettings::PartyOneSmash), QStringLiteral("Space"));
        QCOMPARE(keys.partySetName(0), QStringLiteral("WASD"));
        QCOMPARE(keys.partySetName(1), QStringLiteral("Arrows"));

        // The party keys swap among themselves, the others stay
        QVERIFY(keys.setPartyKey(KeySettings::PartyOneSpecial, Qt::Key_Left));
        QCOMPARE(keys.partyKey(KeySettings::PartyTwoLeft), int(Qt::Key_E));
        QCOMPARE(keys.key(KeySettings::RightSmash), int(Qt::Key_Left));
        QVERIFY(!keys.setPartyKey(KeySettings::PartyTwoSmash, Qt::Key_Escape));
        QCOMPARE(KeySettings().partyKey(KeySettings::PartyOneSpecial), int(Qt::Key_Left));

        keys.restoreDefaults();
        QCOMPARE(keys.partyKey(KeySettings::PartyOneSpecial), int(Qt::Key_E));
    }
};

QTEST_GUILESS_MAIN(tst_KeySettings)
#include "tst_keysettings.moc"
