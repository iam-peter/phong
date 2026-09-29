#ifndef GRAPHICSSETTINGS_H
#define GRAPHICSSETTINGS_H

#include <QObject>
#include <QSettings>
#include <QtQml/qqmlregistration.h>

// How the game looks and what that may cost, persisted with QSettings.
// The defaults are lighter in the browser and on phones.
class GraphicsSettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(Theme theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(Shading shading READ shading WRITE setShading NOTIFY shadingChanged)
    Q_PROPERTY(Glow glow READ glow WRITE setGlow NOTIFY glowChanged)
    Q_PROPERTY(Antialiasing antialiasing READ antialiasing WRITE setAntialiasing NOTIFY antialiasingChanged)
    Q_PROPERTY(bool stars READ stars WRITE setStars NOTIFY starsChanged)
    Q_PROPERTY(bool floor READ floor WRITE setFloor NOTIFY floorChanged)
    Q_PROPERTY(bool shadows READ shadows WRITE setShadows NOTIFY shadowsChanged)
    Q_PROPERTY(bool showFps READ showFps WRITE setShowFps NOTIFY showFpsChanged)
    // Not stored, a browser only goes full screen after a tap or a key.
    // Main applies it to the window on the desktop.
    Q_PROPERTY(bool fullScreen READ fullScreen WRITE setFullScreen NOTIFY fullScreenChanged)
    // Safari on the iPhone has no full screen for pages
    Q_PROPERTY(bool fullScreenAvailable READ fullScreenAvailable CONSTANT)
    // Played with fingers, on a phone or a tablet: landscape only and the
    // buttons for fingers from the start
    Q_PROPERTY(bool touchScreen READ touchScreen CONSTANT)

public:
    // Color palettes, see Theme.qml
    enum Theme {
        Neon = 0,
        Classic,
        Paper,
        GameBoy,
        Amber
    };
    Q_ENUM(Theme)

    // Phong lights with highlights, Flat just shows the colors
    enum Shading {
        Phong = 0,
        Flat
    };
    Q_ENUM(Shading)

    // Medium came later, the stored values of the others stay
    enum Glow {
        NoGlow = 0,
        LowGlow,
        HighGlow,
        MediumGlow
    };
    Q_ENUM(Glow)

    enum Antialiasing {
        NoAntialiasing = 0,
        FastAntialiasing,   // FXAA, a cheap filter
        Multisample2x,
        Multisample4x
    };
    Q_ENUM(Antialiasing)

    explicit GraphicsSettings(QObject* parent = nullptr);

    Q_INVOKABLE void restoreDefaults();

    void setTheme(Theme theme);
    Theme theme() const;

    void setShading(Shading shading);
    Shading shading() const;

    void setGlow(Glow glow);
    Glow glow() const;

    void setAntialiasing(Antialiasing antialiasing);
    Antialiasing antialiasing() const;

    void setStars(bool stars);
    bool stars() const;

    void setFloor(bool floor);
    bool floor() const;

    void setShadows(bool shadows);
    bool shadows() const;

    void setShowFps(bool showFps);
    bool showFps() const;

    void setFullScreen(bool fullScreen);
    bool fullScreen() const;
    bool fullScreenAvailable() const;
    bool touchScreen() const;

signals:
    void themeChanged(GraphicsSettings::Theme);
    void shadingChanged(GraphicsSettings::Shading);
    void glowChanged(GraphicsSettings::Glow);
    void antialiasingChanged(GraphicsSettings::Antialiasing);
    void starsChanged(bool);
    void floorChanged(bool);
    void shadowsChanged(bool);
    void showFpsChanged(bool);
    void fullScreenChanged(bool);

private:
    template<typename Value>
    bool store(Value& member, Value value, const char* key);

    QSettings m_settings;

    Theme m_theme;
    Shading m_shading;
    Glow m_glow;
    Antialiasing m_antialiasing;
    bool m_stars;
    bool m_floor;
    bool m_shadows;
    bool m_showFps;
    bool m_fullScreen;
};

#endif // GRAPHICSSETTINGS_H
