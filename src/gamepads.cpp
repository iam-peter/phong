#include "gamepads.h"

#include <QElapsedTimer>
#include <QLoggingCategory>

#include <algorithm>
#include <cmath>

#if defined(Q_OS_WASM)
#include <emscripten.h>
#elif defined(PHONG_HAVE_SDL3)
#include <SDL3/SDL.h>
#endif

Q_LOGGING_CATEGORY(lcGamepads, "phong.gamepads", QtWarningMsg)

Gamepad::Gamepad(int id, const QString& name, QObject* parent):
    QObject(parent),
    m_id(id),
    m_name(name),
    m_stick(),
    m_direction(),
    m_buttons(),
    m_heldKey(0),
    m_repeatIn(0.0)
{
    m_buttons.fill(false);
}

int Gamepad::id() const
{
    return m_id;
}

QString Gamepad::name() const
{
    return m_name;
}

QVector2D Gamepad::direction() const
{
    return m_direction;
}

bool Gamepad::isPressed(Button button) const
{
    return button >= 0 && button < ButtonCount && m_buttons.at(button);
}

void Gamepad::setStick(const QVector2D& stick)
{
    if (m_stick == stick)
        return;

    m_stick = stick;
    updateDirection();
}

void Gamepad::setButton(Button button, bool pressed)
{
    if (button < 0 || button >= ButtonCount || m_buttons.at(button) == pressed)
        return;

    m_buttons[button] = pressed;
    if (button >= DpadUp)
        updateDirection();

    if (pressed) {
        emit buttonPressed(button);
        if (button == South)
            emit navigated(Qt::Key_Return);
        else if (button == East || button == Back)
            emit navigated(Qt::Key_Escape);
    }
    else {
        emit buttonReleased(button);
    }
}

void Gamepad::advance(qreal dt)
{
    if (m_heldKey == 0)
        return;

    m_repeatIn -= dt;
    if (m_repeatIn <= 0.0) {
        m_repeatIn += repeatInterval;
        emit navigated(m_heldKey);
    }
}

void Gamepad::updateDirection()
{
    // The d-pad wins over the stick
    QVector2D direction(float(m_buttons.at(DpadRight)) - float(m_buttons.at(DpadLeft)),
                        float(m_buttons.at(DpadUp)) - float(m_buttons.at(DpadDown)));
    if (direction.isNull()) {
        // Up positive, and the dead zone taken out so small movements
        // start at zero
        const QVector2D stick(m_stick.x(), -m_stick.y());
        const float length = std::min(stick.length(), 1.0f);
        if (length > deadZone)
            direction = stick.normalized() * float((length - deadZone) / (1.0 - deadZone));
    }

    if (m_direction != direction) {
        m_direction = direction;
        emit directionChanged();
    }

    const int key = navigationKey();
    if (key != m_heldKey) {
        m_heldKey = key;
        m_repeatIn = repeatDelay;
        if (key != 0)
            emit navigated(key);
    }
}

int Gamepad::navigationKey() const
{
    // Past the middle of the way, along the stronger axis
    const float x = m_direction.x();
    const float y = m_direction.y();
    if (std::max(std::abs(x), std::abs(y)) < 0.5f)
        return 0;
    if (std::abs(x) > std::abs(y))
        return x > 0.0f ? Qt::Key_Right : Qt::Key_Left;
    return y > 0.0f ? Qt::Key_Up : Qt::Key_Down;
}

#if defined(Q_OS_WASM)

// The browser's Gamepad API with the standard mapping. Polling snapshots
// the pads, the getters read the snapshot.
EM_JS(int, phong_gamepads_poll, (), {
    const pads = navigator.getGamepads ? Array.from(navigator.getGamepads()) : [];
    globalThis.phongPads = pads.filter((pad) => pad && pad.connected);
    return globalThis.phongPads.length;
});

EM_JS(int, phong_gamepad_index, (int slot), {
    return globalThis.phongPads[slot].index;
});

EM_JS(double, phong_gamepad_axis, (int slot, int axis), {
    const axes = globalThis.phongPads[slot].axes;
    return axis < axes.length ? axes[axis] : 0;
});

EM_JS(int, phong_gamepad_buttons, (int slot), {
    let mask = 0;
    globalThis.phongPads[slot].buttons.forEach((button, index) => {
        if (index < 31 && button.pressed)
            mask |= 1 << index;
    });
    return mask;
});

EM_JS(void, phong_gamepad_rumble, (int index, double strength, int duration), {
    const pad = navigator.getGamepads ? navigator.getGamepads()[index] : null;
    const actuator = pad && pad.vibrationActuator;
    if (actuator && actuator.playEffect)
        actuator.playEffect("dual-rumble", { duration: duration, strongMagnitude: strength,
                                              weakMagnitude: Math.min(1, strength * 1.5) }).catch(() => {});
});

class Gamepads::Backend
{
public:
    bool isAvailable() const
    {
        return true;
    }

    void rumble(int id, qreal strength, int duration)
    {
        phong_gamepad_rumble(id, strength, duration);
    }

    void poll(Gamepads* gamepads)
    {
        // Standard mapping indices of our buttons
        static constexpr std::array<int, Gamepad::ButtonCount> indices = { 0, 1, 2, 3, 8, 9, 4, 5, 12, 13, 14, 15 };

        const int count = phong_gamepads_poll();
        QList<int> seen;
        for (int slot = 0; slot < count; ++slot) {
            const int id = phong_gamepad_index(slot);
            seen.append(id);
            Gamepad* pad = gamepads->find(id);
            if (!pad)
                pad = gamepads->add(id, QStringLiteral("Gamepad %1").arg(id + 1));

            pad->setStick(QVector2D(float(phong_gamepad_axis(slot, 0)), float(phong_gamepad_axis(slot, 1))));
            const int buttons = phong_gamepad_buttons(slot);
            for (int button = 0; button < Gamepad::ButtonCount; ++button)
                pad->setButton(Gamepad::Button(button), buttons & (1 << indices.at(button)));
        }

        for (Gamepad* pad : gamepads->pads()) {
            if (!seen.contains(pad->id()))
                gamepads->remove(pad->id());
        }
    }
};

#elif defined(PHONG_HAVE_SDL3)

class Gamepads::Backend
{
public:
    Backend()
    {
        // There is no SDL window to have the focus, the events come anyway
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        // SDL would turn SIGINT and SIGTERM into its quit event, which
        // nobody reads, and the game wouldn't end any more
        SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
        m_available = SDL_Init(SDL_INIT_GAMEPAD);
        if (!m_available)
            qCWarning(lcGamepads) << "No gamepads:" << SDL_GetError();
    }

    ~Backend()
    {
        for (SDL_Gamepad* gamepad : std::as_const(m_open))
            SDL_CloseGamepad(gamepad);
        if (m_available)
            SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    }

    bool isAvailable() const
    {
        return m_available;
    }

    void rumble(int id, qreal strength, int duration)
    {
        // The low frequency motor is the strong one
        if (SDL_Gamepad* gamepad = SDL_GetGamepadFromID(SDL_JoystickID(id))) {
            const Uint16 low = Uint16(std::clamp(strength, 0.0, 1.0) * 0xffff);
            const Uint16 high = Uint16(std::clamp(strength * 1.5, 0.0, 1.0) * 0xffff);
            SDL_RumbleGamepad(gamepad, low, high, Uint32(std::max(duration, 0)));
        }
    }

    void poll(Gamepads* gamepads)
    {
        if (!m_available)
            return;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_GAMEPAD_ADDED: {
                    const SDL_JoystickID id = event.gdevice.which;
                    if (SDL_Gamepad* gamepad = SDL_OpenGamepad(id)) {
                        m_open.append(gamepad);
                        const char* name = SDL_GetGamepadName(gamepad);
                        gamepads->add(int(id), name ? QString::fromUtf8(name) : QStringLiteral("Gamepad"));
                        qCDebug(lcGamepads) << "Added" << id << name;
                    }
                    break;
                }
                case SDL_EVENT_GAMEPAD_REMOVED: {
                    const SDL_JoystickID id = event.gdevice.which;
                    if (SDL_Gamepad* gamepad = SDL_GetGamepadFromID(id)) {
                        m_open.removeAll(gamepad);
                        SDL_CloseGamepad(gamepad);
                    }
                    gamepads->remove(int(id));
                    break;
                }
                case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                case SDL_EVENT_GAMEPAD_BUTTON_UP:
                    if (Gamepad* pad = gamepads->find(int(event.gbutton.which))) {
                        const int button = buttonOf(event.gbutton.button);
                        if (button >= 0)
                            pad->setButton(Gamepad::Button(button), event.gbutton.down);
                    }
                    break;
                case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                    if (Gamepad* pad = gamepads->find(int(event.gaxis.which))) {
                        const float value = std::max(event.gaxis.value / 32767.0f, -1.0f);
                        QVector2D stick = m_sticks.value(pad->id());
                        if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX)
                            stick.setX(value);
                        else if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY)
                            stick.setY(value);
                        else
                            break;
                        m_sticks.insert(pad->id(), stick);
                        pad->setStick(stick);
                    }
                    break;
                default:
                    break;
            }
        }
    }

private:
    static int buttonOf(Uint8 button)
    {
        switch (button) {
            case SDL_GAMEPAD_BUTTON_SOUTH: return Gamepad::South;
            case SDL_GAMEPAD_BUTTON_EAST: return Gamepad::East;
            case SDL_GAMEPAD_BUTTON_WEST: return Gamepad::West;
            case SDL_GAMEPAD_BUTTON_NORTH: return Gamepad::North;
            case SDL_GAMEPAD_BUTTON_BACK: return Gamepad::Back;
            case SDL_GAMEPAD_BUTTON_START: return Gamepad::Start;
            case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return Gamepad::LeftShoulder;
            case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return Gamepad::RightShoulder;
            case SDL_GAMEPAD_BUTTON_DPAD_UP: return Gamepad::DpadUp;
            case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return Gamepad::DpadDown;
            case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return Gamepad::DpadLeft;
            case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return Gamepad::DpadRight;
            default: return -1;
        }
    }

    bool m_available = false;
    QList<SDL_Gamepad*> m_open;
    QHash<int, QVector2D> m_sticks;
};

#else

class Gamepads::Backend
{
public:
    bool isAvailable() const
    {
        return false;
    }

    void rumble(int, qreal, int)
    {}

    void poll(Gamepads*)
    {}
};

#endif

Gamepads::Gamepads(QObject* parent):
    QObject(parent),
    m_pads(),
    m_rumbleEnabled(true),
    m_backend(std::make_unique<Backend>()),
    m_timer()
{
    if (!m_backend->isAvailable())
        return;

    m_timer.setInterval(8);
    connect(&m_timer, &QTimer::timeout, this, &Gamepads::poll);
    m_timer.start();
}

Gamepads::~Gamepads() = default;

QList<Gamepad*> Gamepads::pads() const
{
    return m_pads;
}

int Gamepads::count() const
{
    return int(m_pads.size());
}

bool Gamepads::isAvailable() const
{
    return m_backend->isAvailable();
}

void Gamepads::rumble(Gamepad* pad, qreal strength, int duration)
{
    if (m_rumbleEnabled && pad && m_pads.contains(pad) && strength > 0.0 && duration > 0)
        m_backend->rumble(pad->id(), strength, duration);
}

void Gamepads::setRumbleEnabled(bool rumbleEnabled)
{
    if (m_rumbleEnabled == rumbleEnabled)
        return;

    m_rumbleEnabled = rumbleEnabled;
    emit rumbleEnabledChanged(rumbleEnabled);
}

bool Gamepads::isRumbleEnabled() const
{
    return m_rumbleEnabled;
}

Gamepad* Gamepads::pad(int index) const
{
    return m_pads.value(index, nullptr);
}

void Gamepads::poll()
{
    static QElapsedTimer clock;
    const qreal dt = clock.isValid() ? clock.restart() / 1000.0 : 0.0;
    if (!clock.isValid())
        clock.start();

    m_backend->poll(this);
    for (Gamepad* pad : std::as_const(m_pads))
        pad->advance(std::min(dt, 0.1));
}

Gamepad* Gamepads::add(int id, const QString& name)
{
    if (Gamepad* pad = find(id))
        return pad;

    Gamepad* pad = new Gamepad(id, name, this);
    connect(pad, &Gamepad::navigated, this, [this, pad](int key) { emit navigated(pad, key); });
    connect(pad, &Gamepad::buttonPressed, this, [this, pad](Gamepad::Button button) { emit buttonPressed(pad, button); });
    connect(pad, &Gamepad::buttonReleased, this, [this, pad](Gamepad::Button button) { emit buttonReleased(pad, button); });
    m_pads.append(pad);
    emit padsChanged();
    return pad;
}

void Gamepads::remove(int id)
{
    Gamepad* pad = find(id);
    if (!pad)
        return;

    m_pads.removeAll(pad);
    emit padsChanged();
    pad->deleteLater();
}

Gamepad* Gamepads::find(int id) const
{
    const auto it = std::find_if(m_pads.cbegin(), m_pads.cend(), [id](Gamepad* pad) { return pad->id() == id; });
    return it == m_pads.cend() ? nullptr : *it;
}
