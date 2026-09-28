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
    Q_PROPERTY(Shading shading READ shading WRITE setShading NOTIFY shadingChanged)
    Q_PROPERTY(Glow glow READ glow WRITE setGlow NOTIFY glowChanged)
    Q_PROPERTY(Antialiasing antialiasing READ antialiasing WRITE setAntialiasing NOTIFY antialiasingChanged)
    Q_PROPERTY(bool stars READ stars WRITE setStars NOTIFY starsChanged)
    Q_PROPERTY(bool floor READ floor WRITE setFloor NOTIFY floorChanged)
    Q_PROPERTY(bool shadows READ shadows WRITE setShadows NOTIFY shadowsChanged)
    Q_PROPERTY(bool showFps READ showFps WRITE setShowFps NOTIFY showFpsChanged)

public:
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

signals:
    void shadingChanged(GraphicsSettings::Shading);
    void glowChanged(GraphicsSettings::Glow);
    void antialiasingChanged(GraphicsSettings::Antialiasing);
    void starsChanged(bool);
    void floorChanged(bool);
    void shadowsChanged(bool);
    void showFpsChanged(bool);

private:
    template<typename Value>
    bool store(Value& member, Value value, const char* key);

    QSettings m_settings;

    Shading m_shading;
    Glow m_glow;
    Antialiasing m_antialiasing;
    bool m_stars;
    bool m_floor;
    bool m_shadows;
    bool m_showFps;
};

#endif // GRAPHICSSETTINGS_H
