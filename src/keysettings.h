#ifndef KEYSETTINGS_H
#define KEYSETTINGS_H

#include <QList>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QtQml/qqmlregistration.h>

// The keys of the players, persisted with QSettings. Every action has a
// key and every key at most one action. Escape and Enter belong to the
// menus.
class KeySettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // Names of the keys of all actions, in the order of Action
    Q_PROPERTY(QStringList keyNames READ keyNames NOTIFY changed)

public:
    enum Action {
        LeftUp = 0,
        LeftDown,
        LeftSmash,
        LeftSpecial,
        RightUp,
        RightDown,
        RightSmash,
        RightSpecial,
        Pause,
        ActionCount
    };
    Q_ENUM(Action)

    explicit KeySettings(QObject* parent = nullptr);

    static int defaultKey(Action action);
    // Keys an action can't have
    static bool isReserved(int key);

    Q_INVOKABLE int key(KeySettings::Action action) const;
    // Another action with that key gets the key of this one. Returns
    // whether the key could be used.
    Q_INVOKABLE bool setKey(KeySettings::Action action, int key);
    // The action of a key, -1 for none
    Q_INVOKABLE int action(int key) const;
    Q_INVOKABLE QString keyName(int key) const;
    Q_INVOKABLE QString actionKeyName(KeySettings::Action action) const;
    Q_INVOKABLE void restoreDefaults();

    QStringList keyNames() const;

signals:
    void changed();

private:
    void store();

    QSettings m_settings;
    QList<int> m_keys;
};

#endif // KEYSETTINGS_H
