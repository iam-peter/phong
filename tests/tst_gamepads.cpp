#include "gamepads.h"

#include <QSignalSpy>
#include <QTest>

#if defined(PHONG_HAVE_SDL3)
#include <SDL3/SDL.h>
#endif

class tst_Gamepads : public QObject
{
    Q_OBJECT

private slots:
    void directionFromStickAndDpad()
    {
        Gamepad pad(1, QStringLiteral("Test"));
        QSignalSpy changed(&pad, &Gamepad::directionChanged);

        // Inside the dead zone nothing moves
        pad.setStick(QVector2D(0.1f, -0.2f));
        QCOMPARE(pad.direction(), QVector2D());
        QCOMPARE(changed.count(), 0);

        // Stick up is up, full tilt is full speed
        pad.setStick(QVector2D(0.0f, -1.0f));
        QVERIFY((pad.direction() - QVector2D(0.0f, 1.0f)).length() < 1e-5f);

        // The d-pad wins
        pad.setButton(Gamepad::DpadRight, true);
        QCOMPARE(pad.direction(), QVector2D(1.0f, 0.0f));
        pad.setButton(Gamepad::DpadRight, false);
        QVERIFY(pad.direction().y() > 0.99f);
    }

    void navigation()
    {
        Gamepad pad(1, QStringLiteral("Test"));
        QSignalSpy navigated(&pad, &Gamepad::navigated);

        pad.setButton(Gamepad::DpadDown, true);
        QCOMPARE(navigated.count(), 1);
        QCOMPARE(navigated.last().at(0).toInt(), int(Qt::Key_Down));

        // Held, it repeats after a delay
        pad.advance(0.5 * Gamepad::repeatDelay);
        QCOMPARE(navigated.count(), 1);
        pad.advance(0.5 * Gamepad::repeatDelay + 0.01);
        QCOMPARE(navigated.count(), 2);
        pad.advance(Gamepad::repeatInterval);
        QCOMPARE(navigated.count(), 3);

        pad.setButton(Gamepad::DpadDown, false);
        pad.advance(1.0);
        QCOMPARE(navigated.count(), 3);

        // The stick past the middle, and the buttons of a menu
        pad.setStick(QVector2D(-0.9f, 0.1f));
        QCOMPARE(navigated.last().at(0).toInt(), int(Qt::Key_Left));
        pad.setButton(Gamepad::South, true);
        QCOMPARE(navigated.last().at(0).toInt(), int(Qt::Key_Return));
        const int count = int(navigated.count());
        pad.setButton(Gamepad::Start, true);
        QCOMPARE(navigated.count(), count);
        pad.setButton(Gamepad::East, true);
        QCOMPARE(navigated.last().at(0).toInt(), int(Qt::Key_Escape));
    }

    void padsComeAndGo()
    {
        Gamepads gamepads;
        const int before = gamepads.count();
        QSignalSpy changed(&gamepads, &Gamepads::padsChanged);
        QSignalSpy pressed(&gamepads, &Gamepads::buttonPressed);

        Gamepad* pad = gamepads.add(1000, QStringLiteral("Test"));
        QCOMPARE(gamepads.add(1000, QStringLiteral("Again")), pad);
        QCOMPARE(gamepads.count(), before + 1);
        QCOMPARE(gamepads.find(1000), pad);

        pad->setButton(Gamepad::West, true);
        QCOMPARE(pressed.count(), 1);
        QCOMPARE(pressed.last().at(0).value<Gamepad*>(), pad);

        gamepads.remove(1000);
        QCOMPARE(gamepads.count(), before);
        QCOMPARE(changed.count(), 2);
    }

#if defined(PHONG_HAVE_SDL3)
    // A virtual SDL gamepad stands in for the hardware
    void sdlGamepad()
    {
        Gamepads gamepads;
        QVERIFY(gamepads.isAvailable());

        SDL_VirtualJoystickDesc desc;
        SDL_INIT_INTERFACE(&desc);
        desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
        desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        desc.name = "Virtual pad";
        const SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
        QVERIFY2(id != 0, SDL_GetError());
        SDL_Joystick* joystick = SDL_OpenJoystick(id);
        QVERIFY(joystick);

        QTRY_COMPARE(gamepads.find(int(id)) != nullptr, true);
        Gamepad* pad = gamepads.find(int(id));
        QSignalSpy pressed(pad, &Gamepad::buttonPressed);

        SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, true);
        QTRY_COMPARE(pad->isPressed(Gamepad::South), true);
        QCOMPARE(pressed.last().at(0).value<Gamepad::Button>(), Gamepad::South);

        SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFTY, -32767);
        QTRY_VERIFY(pad->direction().y() > 0.99f);

        SDL_CloseJoystick(joystick);
        SDL_DetachVirtualJoystick(id);
        QTRY_COMPARE(gamepads.find(int(id)) == nullptr, true);
    }
#endif
};

QTEST_GUILESS_MAIN(tst_Gamepads)
#include "tst_gamepads.moc"
