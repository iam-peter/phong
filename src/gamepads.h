#ifndef GAMEPADS_H
#define GAMEPADS_H

#include <QList>
#include <QObject>
#include <QTimer>
#include <QVector2D>
#include <QtQml/qqmlregistration.h>

#include <array>
#include <memory>

// A gamepad, laid out like an Xbox controller. The left stick and the
// d-pad together give the direction.
class Gamepad : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Gamepads come from Gamepads")
    Q_PROPERTY(int id READ id CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    // -1 to 1 each, up and right positive, without the dead zone
    Q_PROPERTY(QVector2D direction READ direction NOTIFY directionChanged)

public:
    enum Button {
        South = 0,  // A
        East,       // B
        West,       // X
        North,      // Y
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
    Q_ENUM(Button)

    // Stick movement below this share counts as none
    static constexpr qreal deadZone = 0.25;
    // Menu navigation repeats after this delay, at this interval
    static constexpr qreal repeatDelay = 0.4;
    static constexpr qreal repeatInterval = 0.12;

    Gamepad(int id, const QString& name, QObject* parent = nullptr);

    int id() const;
    QString name() const;
    QVector2D direction() const;
    Q_INVOKABLE bool isPressed(Gamepad::Button button) const;

    // For the backends: raw stick position, y down positive like most
    // hardware reports it, and button states
    void setStick(const QVector2D& stick);
    void setButton(Button button, bool pressed);
    // Runs the navigation repeat, call regularly
    void advance(qreal dt);

signals:
    void directionChanged();
    void buttonPressed(Gamepad::Button button);
    void buttonReleased(Gamepad::Button button);
    // Menu navigation as keys: arrows from the direction, Return from A,
    // Escape from B and Back. Start is left to pause a game.
    void navigated(int key);

private:
    void updateDirection();
    int navigationKey() const;

    int m_id;
    QString m_name;
    QVector2D m_stick;
    QVector2D m_direction;
    std::array<bool, ButtonCount> m_buttons;
    int m_heldKey;
    qreal m_repeatIn;
};

// The connected gamepads. On the desktop they come from SDL3 if the game
// was built with it, in the browser from the Gamepad API.
class Gamepads : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QList<Gamepad*> pads READ pads NOTIFY padsChanged)
    Q_PROPERTY(int count READ count NOTIFY padsChanged)
    // Whether gamepads can be used at all
    Q_PROPERTY(bool available READ isAvailable CONSTANT)
    Q_PROPERTY(bool rumbleEnabled READ isRumbleEnabled WRITE setRumbleEnabled NOTIFY rumbleEnabledChanged)

public:
    explicit Gamepads(QObject* parent = nullptr);
    ~Gamepads() override;

    QList<Gamepad*> pads() const;
    int count() const;
    bool isAvailable() const;

    void setRumbleEnabled(bool rumbleEnabled);
    bool isRumbleEnabled() const;
    Q_INVOKABLE Gamepad* pad(int index) const;

    // Reads the backend, the timer does it too
    Q_INVOKABLE void poll();

    // Shakes the pad, strength from 0 to 1, for duration milliseconds, if
    // it can and rumble is enabled
    Q_INVOKABLE void rumble(Gamepad* pad, qreal strength, int duration);

    // For the backends and tests
    Gamepad* add(int id, const QString& name);
    void remove(int id);
    Gamepad* find(int id) const;

signals:
    void padsChanged();
    void rumbleEnabledChanged(bool);
    void navigated(Gamepad* pad, int key);
    void buttonPressed(Gamepad* pad, Gamepad::Button button);
    void buttonReleased(Gamepad* pad, Gamepad::Button button);

private:
    class Backend;

    QList<Gamepad*> m_pads;
    bool m_rumbleEnabled;
    std::unique_ptr<Backend> m_backend;
    QTimer m_timer;
};

#endif // GAMEPADS_H
