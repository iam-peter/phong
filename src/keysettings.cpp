#include "keysettings.h"

#include <QKeySequence>
#include <QMetaEnum>

namespace {
// Gamepad::Button, the header of the gamepads needn't be here
enum Button {
    South = 0,
    East,
    West,
    North,
    Back,
    Start,
    LeftShoulder,
    RightShoulder,
    DpadUp,
    DpadDown,
    DpadLeft,
    DpadRight,
    ButtonCount
};
}

KeySettings::KeySettings(QObject* parent):
    QObject(parent),
    m_settings(),
    m_keys(),
    m_padButtons(),
    m_partyKeys()
{
    m_settings.beginGroup(QStringLiteral("keys"));
    const QMetaEnum actions = QMetaEnum::fromType<Action>();
    for (int action = 0; action < ActionCount; ++action) {
        const int key = m_settings.value(QString::fromLatin1(actions.valueToKey(action)),
                                         defaultKey(Action(action))).toInt();
        m_keys.append(key);
    }

    const QMetaEnum padActions = QMetaEnum::fromType<PadAction>();
    for (int action = 0; action < PadActionCount; ++action) {
        m_padButtons.append(m_settings.value(QString::fromLatin1(padActions.valueToKey(action)),
                                             defaultPadButton(PadAction(action))).toInt());
    }
    for (int action = 0; action < PadActionCount; ++action) {
        if (isReservedButton(m_padButtons.at(action)) || m_padButtons.count(m_padButtons.at(action)) > 1) {
            for (int other = 0; other < PadActionCount; ++other)
                m_padButtons[other] = defaultPadButton(PadAction(other));
            break;
        }
    }

    const QMetaEnum partyActions = QMetaEnum::fromType<PartyAction>();
    for (int action = 0; action < PartyActionCount; ++action) {
        m_partyKeys.append(m_settings.value(QString::fromLatin1(partyActions.valueToKey(action)),
                                            defaultPartyKey(PartyAction(action))).toInt());
    }
    for (int action = 0; action < PartyActionCount; ++action) {
        if (isReserved(m_partyKeys.at(action)) || m_partyKeys.count(m_partyKeys.at(action)) > 1) {
            for (int other = 0; other < PartyActionCount; ++other)
                m_partyKeys[other] = defaultPartyKey(PartyAction(other));
            break;
        }
    }

    // An edited file with doubled or reserved keys starts over
    for (int action = 0; action < ActionCount; ++action) {
        if (isReserved(m_keys.at(action)) || m_keys.count(m_keys.at(action)) > 1) {
            for (int other = 0; other < ActionCount; ++other)
                m_keys[other] = defaultKey(Action(other));
            break;
        }
    }
}

int KeySettings::defaultKey(Action action)
{
    switch (action) {
        case Action::LeftUp:
            return Qt::Key_W;
        case Action::LeftDown:
            return Qt::Key_S;
        case Action::LeftSmash:
            return Qt::Key_D;
        case Action::LeftSpecial:
            return Qt::Key_A;
        case Action::RightUp:
            return Qt::Key_Up;
        case Action::RightDown:
            return Qt::Key_Down;
        case Action::RightSmash:
            return Qt::Key_Left;
        case Action::RightSpecial:
            return Qt::Key_Right;
        case Action::Pause:
            return Qt::Key_P;
        default:
            return 0;
    }
}

bool KeySettings::isReserved(int key)
{
    return key == 0 || key == Qt::Key_Escape || key == Qt::Key_Return || key == Qt::Key_Enter
           || key == Qt::Key_unknown;
}

int KeySettings::key(Action action) const
{
    return action >= 0 && action < ActionCount ? m_keys.at(action) : 0;
}

bool KeySettings::setKey(Action action, int key)
{
    if (action < 0 || action >= ActionCount || isReserved(key))
        return false;

    const int previous = m_keys.at(action);
    if (previous == key)
        return true;

    const int other = this->action(key);
    if (other >= 0)
        m_keys[other] = previous;
    m_keys[action] = key;
    store();
    emit changed();
    return true;
}

int KeySettings::action(int key) const
{
    return int(m_keys.indexOf(key));
}

QString KeySettings::keyName(int key) const
{
    switch (key) {
        case Qt::Key_Space:
            return tr("Space");
        case Qt::Key_Up:
            return tr("Up");
        case Qt::Key_Down:
            return tr("Down");
        case Qt::Key_Left:
            return tr("Left");
        case Qt::Key_Right:
            return tr("Right");
        default:
            return QKeySequence(key).toString(QKeySequence::NativeText);
    }
}

QString KeySettings::actionKeyName(Action action) const
{
    return keyName(key(action));
}

QStringList KeySettings::keyNames() const
{
    QStringList names;
    for (int action = 0; action < ActionCount; ++action)
        names.append(actionKeyName(Action(action)));
    return names;
}

int KeySettings::defaultPadButton(PadAction action)
{
    switch (action) {
        case PadAction::PadSmash:
            return South;
        case PadAction::PadSpecial:
            return East;
        case PadAction::PadDash:
            return West;
        case PadAction::PadPause:
            return Start;
        default:
            return -1;
    }
}

bool KeySettings::isReservedButton(int button)
{
    return button < 0 || button >= ButtonCount || button == Back || button >= DpadUp;
}

int KeySettings::padButton(PadAction action) const
{
    return action >= 0 && action < PadActionCount ? m_padButtons.at(action) : -1;
}

bool KeySettings::setPadButton(PadAction action, int button)
{
    if (action < 0 || action >= PadActionCount || isReservedButton(button))
        return false;

    const int previous = m_padButtons.at(action);
    if (previous == button)
        return true;

    const int other = padAction(button);
    if (other >= 0)
        m_padButtons[other] = previous;
    m_padButtons[action] = button;
    store();
    emit changed();
    return true;
}

int KeySettings::padAction(int button) const
{
    return int(m_padButtons.indexOf(button));
}

QString KeySettings::buttonName(int button) const
{
    switch (button) {
        case South: return QStringLiteral("A");
        case East: return QStringLiteral("B");
        case West: return QStringLiteral("X");
        case North: return QStringLiteral("Y");
        case Back: return tr("Back");
        case Start: return tr("Start");
        case LeftShoulder: return QStringLiteral("LB");
        case RightShoulder: return QStringLiteral("RB");
        default: return QStringLiteral("?");
    }
}

QStringList KeySettings::padButtonNames() const
{
    QStringList names;
    for (int action = 0; action < PadActionCount; ++action)
        names.append(buttonName(m_padButtons.at(action)));
    return names;
}

int KeySettings::defaultPartyKey(PartyAction action)
{
    switch (action) {
        case PartyAction::PartyOneLeft: return Qt::Key_A;
        case PartyAction::PartyOneRight: return Qt::Key_D;
        case PartyAction::PartyOneUp: return Qt::Key_W;
        case PartyAction::PartyOneDown: return Qt::Key_S;
        case PartyAction::PartyOneSmash: return Qt::Key_Space;
        case PartyAction::PartyOneSpecial: return Qt::Key_E;
        case PartyAction::PartyTwoLeft: return Qt::Key_Left;
        case PartyAction::PartyTwoRight: return Qt::Key_Right;
        case PartyAction::PartyTwoUp: return Qt::Key_Up;
        case PartyAction::PartyTwoDown: return Qt::Key_Down;
        case PartyAction::PartyTwoSmash: return Qt::Key_Period;
        case PartyAction::PartyTwoSpecial: return Qt::Key_Comma;
        default: return 0;
    }
}

int KeySettings::partyKey(PartyAction action) const
{
    return action >= 0 && action < PartyActionCount ? m_partyKeys.at(action) : 0;
}

bool KeySettings::setPartyKey(PartyAction action, int key)
{
    if (action < 0 || action >= PartyActionCount || isReserved(key))
        return false;

    const int previous = m_partyKeys.at(action);
    if (previous == key)
        return true;

    const int other = partyAction(key);
    if (other >= 0)
        m_partyKeys[other] = previous;
    m_partyKeys[action] = key;
    store();
    emit changed();
    return true;
}

int KeySettings::partyAction(int key) const
{
    return int(m_partyKeys.indexOf(key));
}

QString KeySettings::partySetName(int set) const
{
    if (set < 0 || set > 1)
        return QString();

    // Up, left, down, right, the way WASD reads
    const int base = set * PartyTwoLeft;
    const QList<int> keys = { m_partyKeys.at(base + 2), m_partyKeys.at(base), m_partyKeys.at(base + 3),
                              m_partyKeys.at(base + 1) };
    if (keys == QList<int>{ Qt::Key_Up, Qt::Key_Left, Qt::Key_Down, Qt::Key_Right })
        return tr("Arrows");

    QStringList names;
    bool short_ = true;
    for (int key : keys) {
        names.append(keyName(key));
        short_ = short_ && names.last().size() == 1;
    }
    return names.join(short_ ? QString() : QStringLiteral("/"));
}

QStringList KeySettings::partyKeyNames() const
{
    QStringList names;
    for (int action = 0; action < PartyActionCount; ++action)
        names.append(keyName(m_partyKeys.at(action)));
    return names;
}

void KeySettings::restoreDefaults()
{
    bool changed = false;
    for (int action = 0; action < ActionCount; ++action) {
        changed = changed || m_keys.at(action) != defaultKey(Action(action));
        m_keys[action] = defaultKey(Action(action));
    }
    for (int action = 0; action < PadActionCount; ++action) {
        changed = changed || m_padButtons.at(action) != defaultPadButton(PadAction(action));
        m_padButtons[action] = defaultPadButton(PadAction(action));
    }
    for (int action = 0; action < PartyActionCount; ++action) {
        changed = changed || m_partyKeys.at(action) != defaultPartyKey(PartyAction(action));
        m_partyKeys[action] = defaultPartyKey(PartyAction(action));
    }
    if (!changed)
        return;

    store();
    emit this->changed();
}

void KeySettings::store()
{
    const QMetaEnum actions = QMetaEnum::fromType<Action>();
    for (int action = 0; action < ActionCount; ++action)
        m_settings.setValue(QString::fromLatin1(actions.valueToKey(action)), m_keys.at(action));
    const QMetaEnum padActions = QMetaEnum::fromType<PadAction>();
    for (int action = 0; action < PadActionCount; ++action)
        m_settings.setValue(QString::fromLatin1(padActions.valueToKey(action)), m_padButtons.at(action));
    const QMetaEnum partyActions = QMetaEnum::fromType<PartyAction>();
    for (int action = 0; action < PartyActionCount; ++action)
        m_settings.setValue(QString::fromLatin1(partyActions.valueToKey(action)), m_partyKeys.at(action));
}
