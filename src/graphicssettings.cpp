#include "graphicssettings.h"

#include <type_traits>

namespace {
#if defined(Q_OS_WASM) || defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
constexpr auto defaultGlow = GraphicsSettings::Glow::LowGlow;
constexpr auto defaultAntialiasing = GraphicsSettings::Antialiasing::FastAntialiasing;
#else
constexpr auto defaultGlow = GraphicsSettings::Glow::HighGlow;
constexpr auto defaultAntialiasing = GraphicsSettings::Antialiasing::Multisample4x;
#endif
constexpr auto defaultShading = GraphicsSettings::Shading::Phong;

template<typename Enum>
Enum readEnum(const QSettings& settings, const char* key, Enum fallback, Enum last)
{
    const int value = settings.value(key, int(fallback)).toInt();
    return (value < 0 || value > int(last)) ? fallback : Enum(value);
}
}

GraphicsSettings::GraphicsSettings(QObject* parent):
    QObject(parent),
    m_settings(),
    m_shading(defaultShading),
    m_glow(defaultGlow),
    m_antialiasing(defaultAntialiasing),
    m_stars(true),
    m_floor(true),
    m_shadows(true),
    m_showFps(false)
{
    m_settings.beginGroup(QStringLiteral("graphics"));
    m_shading = readEnum(m_settings, "shading", defaultShading, Shading::Flat);
    m_glow = readEnum(m_settings, "glow", defaultGlow, Glow::HighGlow);
    m_antialiasing = readEnum(m_settings, "antialiasing", defaultAntialiasing,
                              Antialiasing::Multisample4x);
    m_stars = m_settings.value("stars", true).toBool();
    m_floor = m_settings.value("floor", true).toBool();
    m_shadows = m_settings.value("shadows", true).toBool();
    m_showFps = m_settings.value("showFps", false).toBool();
}

void GraphicsSettings::restoreDefaults()
{
    setShading(defaultShading);
    setGlow(defaultGlow);
    setAntialiasing(defaultAntialiasing);
    setStars(true);
    setFloor(true);
    setShadows(true);
    setShowFps(false);
}

template<typename Value>
bool GraphicsSettings::store(Value& member, Value value, const char* key)
{
    if (member == value)
        return false;

    member = value;
    if constexpr (std::is_enum_v<Value>)
        m_settings.setValue(key, int(value));
    else
        m_settings.setValue(key, value);
    return true;
}

void GraphicsSettings::setShading(Shading shading)
{
    if (store(m_shading, shading, "shading"))
        emit shadingChanged(shading);
}

GraphicsSettings::Shading GraphicsSettings::shading() const
{
    return m_shading;
}

void GraphicsSettings::setGlow(Glow glow)
{
    if (store(m_glow, glow, "glow"))
        emit glowChanged(glow);
}

GraphicsSettings::Glow GraphicsSettings::glow() const
{
    return m_glow;
}

void GraphicsSettings::setAntialiasing(Antialiasing antialiasing)
{
    if (store(m_antialiasing, antialiasing, "antialiasing"))
        emit antialiasingChanged(antialiasing);
}

GraphicsSettings::Antialiasing GraphicsSettings::antialiasing() const
{
    return m_antialiasing;
}

void GraphicsSettings::setStars(bool stars)
{
    if (store(m_stars, stars, "stars"))
        emit starsChanged(stars);
}

bool GraphicsSettings::stars() const
{
    return m_stars;
}

void GraphicsSettings::setFloor(bool floor)
{
    if (store(m_floor, floor, "floor"))
        emit floorChanged(floor);
}

bool GraphicsSettings::floor() const
{
    return m_floor;
}

void GraphicsSettings::setShadows(bool shadows)
{
    if (store(m_shadows, shadows, "shadows"))
        emit shadowsChanged(shadows);
}

bool GraphicsSettings::shadows() const
{
    return m_shadows;
}

void GraphicsSettings::setShowFps(bool showFps)
{
    if (store(m_showFps, showFps, "showFps"))
        emit showFpsChanged(showFps);
}

bool GraphicsSettings::showFps() const
{
    return m_showFps;
}
