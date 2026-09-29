#include "graphicssettings.h"

#include <type_traits>

#if defined(Q_OS_WASM)
#include <emscripten.h>

#include <QTimer>
#endif

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

#if defined(Q_OS_WASM)

// The whole page goes full screen, the canvas of Qt fills it. Browsers
// only allow that during a user gesture, Qt delivers taps and keys while
// the browser is still handling them.
EM_JS(int, phong_full_screen_available, (), {
    return document.fullscreenEnabled || document.webkitFullscreenEnabled ? 1 : 0;
});

EM_JS(int, phong_full_screen, (), {
    return document.fullscreenElement || document.webkitFullscreenElement ? 1 : 0;
});

EM_JS(int, phong_touch_screen, (), {
    return globalThis.matchMedia && matchMedia("(pointer: coarse)").matches ? 1 : 0;
});

EM_JS(void, phong_set_full_screen, (int fullScreen), {
    const current = document.fullscreenElement || document.webkitFullscreenElement ? 1 : 0;
    if (fullScreen === current)
        return;
    const page = document.documentElement;
    let result;
    if (fullScreen && page.requestFullscreen)
        result = page.requestFullscreen({ navigationUI: "hide" });
    else if (fullScreen && page.webkitRequestFullscreen)
        result = page.webkitRequestFullscreen();
    else if (!fullScreen && document.exitFullscreen)
        result = document.exitFullscreen();
    else if (!fullScreen && document.webkitExitFullscreen)
        result = document.webkitExitFullscreen();
    if (!result || !result.then)
        return;
    // Refused without a gesture, the state simply stays. Phones turn to
    // landscape, only possible in full screen and not on every phone.
    result.then(() => {
        const orientation = globalThis.screen && screen.orientation;
        if (fullScreen && orientation && orientation.lock && matchMedia("(pointer: coarse)").matches)
            orientation.lock("landscape").catch(() => {});
    }, () => {});
});

#endif

GraphicsSettings::GraphicsSettings(QObject* parent):
    QObject(parent),
    m_settings(),
    m_theme(Theme::Neon),
    m_shading(defaultShading),
    m_glow(defaultGlow),
    m_antialiasing(defaultAntialiasing),
    m_stars(true),
    m_floor(true),
    m_shadows(true),
    m_showFps(false),
    m_fullScreen(false)
{
    m_settings.beginGroup(QStringLiteral("graphics"));
    m_theme = readEnum(m_settings, "theme", Theme::Neon, Theme::Amber);
    m_shading = readEnum(m_settings, "shading", defaultShading, Shading::Flat);
    m_glow = readEnum(m_settings, "glow", defaultGlow, Glow::MediumGlow);
    m_antialiasing = readEnum(m_settings, "antialiasing", defaultAntialiasing,
                              Antialiasing::Multisample4x);
    m_stars = m_settings.value("stars", true).toBool();
    m_floor = m_settings.value("floor", true).toBool();
    m_shadows = m_settings.value("shadows", true).toBool();
    m_showFps = m_settings.value("showFps", false).toBool();

#if defined(Q_OS_WASM)
    // The browser leaves full screen on its own too, with Esc or the back
    // gesture, and says so only to JavaScript
    auto* poll = new QTimer(this);
    connect(poll, &QTimer::timeout, this, [this] {
        const bool fullScreen = phong_full_screen();
        if (m_fullScreen == fullScreen)
            return;
        m_fullScreen = fullScreen;
        emit fullScreenChanged(fullScreen);
    });
    poll->start(250);
#endif
}

void GraphicsSettings::restoreDefaults()
{
    setTheme(Theme::Neon);
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

void GraphicsSettings::setTheme(Theme theme)
{
    if (store(m_theme, theme, "theme"))
        emit themeChanged(theme);
}

GraphicsSettings::Theme GraphicsSettings::theme() const
{
    return m_theme;
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

void GraphicsSettings::setFullScreen(bool fullScreen)
{
#if defined(Q_OS_WASM)
    // Follows once the browser changed, see the poll
    phong_set_full_screen(fullScreen);
#else
    if (m_fullScreen == fullScreen)
        return;
    m_fullScreen = fullScreen;
    emit fullScreenChanged(fullScreen);
#endif
}

bool GraphicsSettings::fullScreen() const
{
    return m_fullScreen;
}

bool GraphicsSettings::touchScreen() const
{
#if defined(Q_OS_WASM)
    return phong_touch_screen();
#elif defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    return true;
#else
    return false;
#endif
}

bool GraphicsSettings::fullScreenAvailable() const
{
#if defined(Q_OS_WASM)
    return phong_full_screen_available();
#else
    return true;
#endif
}
