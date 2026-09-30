#include "gamesettings.h"

#include <algorithm>

namespace {
constexpr int defaultPointsToWin = 5;
constexpr auto defaultBallSpeed = GameSettings::BallSpeed::Medium;
constexpr auto defaultPaddleSize = GameSettings::PaddleSize::Regular;
constexpr auto defaultDifficulty = ComputerPlayer::Difficulty::Normal;
constexpr bool defaultModifiers = true;
constexpr int defaultSetsToWin = 1;
constexpr bool defaultWinByTwo = false;
const QString defaultArena = QStringLiteral("random");
constexpr bool defaultSound = true;
constexpr bool defaultMusic = true;
constexpr int defaultMusicVolume = 75;
constexpr bool defaultRumble = true;
constexpr int defaultKickoffTime = 2;

qreal serveSpeed(GameSettings::BallSpeed speed)
{
    switch (speed) {
        case GameSettings::BallSpeed::Slow:
            return 12.0;
        case GameSettings::BallSpeed::Fast:
            return 21.0;
        case GameSettings::BallSpeed::Medium:
        default:
            return 16.0;
    }
}

qreal paddleLength(GameSettings::PaddleSize size)
{
    switch (size) {
        case GameSettings::PaddleSize::Small:
            return 3.0;
        case GameSettings::PaddleSize::Large:
            return 5.0;
        case GameSettings::PaddleSize::Regular:
        default:
            return 4.0;
    }
}

template<typename Enum>
Enum readEnum(const QSettings& settings, const char* key, Enum fallback, Enum last)
{
    const int value = settings.value(key, int(fallback)).toInt();
    return (value < 0 || value > int(last)) ? fallback : Enum(value);
}
}

GameSettings::GameSettings(QObject* parent):
    QObject(parent),
    m_settings(),
    m_pointsToWin(defaultPointsToWin),
    m_ballSpeed(defaultBallSpeed),
    m_paddleSize(defaultPaddleSize),
    m_difficulty(defaultDifficulty),
    m_modifiers(defaultModifiers),
    m_setsToWin(defaultSetsToWin),
    m_winByTwo(defaultWinByTwo),
    m_arena(defaultArena),
    m_sound(defaultSound),
    m_music(defaultMusic),
    m_musicVolume(defaultMusicVolume),
    m_rumble(defaultRumble),
    m_kickoffTime(defaultKickoffTime),
    m_mode(0),
    m_lobbyPlayers(2),
    m_network()
{
    m_pointsToWin = std::clamp(m_settings.value("pointsToWin", defaultPointsToWin).toInt(),
                               minPointsToWin, maxPointsToWin);
    m_ballSpeed = readEnum(m_settings, "ballSpeed", defaultBallSpeed, BallSpeed::Fast);
    m_paddleSize = readEnum(m_settings, "paddleSize", defaultPaddleSize, PaddleSize::Large);
    m_difficulty = readEnum(m_settings, "difficulty", defaultDifficulty,
                            ComputerPlayer::Difficulty::Hard);
    m_modifiers = m_settings.value("modifiers", defaultModifiers).toBool();
    m_setsToWin = std::clamp(m_settings.value("setsToWin", defaultSetsToWin).toInt(), 1, maxSetsToWin);
    m_winByTwo = m_settings.value("winByTwo", defaultWinByTwo).toBool();
    m_arena = m_settings.value("arena", defaultArena).toString();
    m_sound = m_settings.value("sound", defaultSound).toBool();
    m_music = m_settings.value("music", defaultMusic).toBool();
    m_musicVolume = std::clamp(m_settings.value("musicVolume", defaultMusicVolume).toInt(), 0, 100);
    m_rumble = m_settings.value("rumble", defaultRumble).toBool();
    m_kickoffTime = std::clamp(m_settings.value("kickoffTime", defaultKickoffTime).toInt(),
                               minKickoffTime, maxKickoffTime);
    m_mode = std::max(m_settings.value("mode", 0).toInt(), 0);
    m_lobbyPlayers = std::clamp(m_settings.value("lobbyPlayers", 2).toInt(), 2, 6);
    m_network = m_settings.value("network").toString() == QLatin1String("lan") ? QStringLiteral("lan")
                                                                                : QStringLiteral("internet");
    m_lanAddress = m_settings.value("lanAddress").toString();
    m_playerName = m_settings.value("playerName").toString().left(12);
    m_online = m_settings.value("online", true).toBool();
    m_serverUrl = m_settings.value("serverUrl").toString();
}

void GameSettings::restoreDefaults()
{
    setPointsToWin(defaultPointsToWin);
    setBallSpeed(defaultBallSpeed);
    setPaddleSize(defaultPaddleSize);
    setDifficulty(defaultDifficulty);
    setModifiers(defaultModifiers);
    setSetsToWin(defaultSetsToWin);
    setWinByTwo(defaultWinByTwo);
    setArena(defaultArena);
    setSound(defaultSound);
    setMusic(defaultMusic);
    setMusicVolume(defaultMusicVolume);
    setRumble(defaultRumble);
    setKickoffTime(defaultKickoffTime);
}

void GameSettings::restoreAppDefaults()
{
    setSound(defaultSound);
    setMusic(defaultMusic);
    setMusicVolume(defaultMusicVolume);
    setRumble(defaultRumble);
}

QVariantMap GameSettings::rules(int mode) const
{
    const QString prefix = QStringLiteral("rules/%1/").arg(std::max(mode, 0));
    const auto value = [this, &prefix](const char* name, const QVariant& fallback) {
        return m_settings.value(prefix + QLatin1String(name), fallback);
    };

    const int points = std::clamp(value("pointsToWin", m_pointsToWin).toInt(),
                                  minPointsToWin, maxPointsToWin);
    const int sets = std::clamp(value("setsToWin", m_setsToWin).toInt(), 1, maxSetsToWin);
    const auto speed = BallSpeed(std::clamp(value("ballSpeed", int(m_ballSpeed)).toInt(),
                                            int(BallSpeed::Slow), int(BallSpeed::Fast)));
    const auto size = PaddleSize(std::clamp(value("paddleSize", int(m_paddleSize)).toInt(),
                                            int(PaddleSize::Small), int(PaddleSize::Large)));
    const auto cpu = ComputerPlayer::Difficulty(std::clamp(
        value("difficulty", int(m_difficulty)).toInt(), int(ComputerPlayer::Difficulty::Easy),
        int(ComputerPlayer::Difficulty::Hard)));
    const int kickoff = std::clamp(value("kickoffTime", m_kickoffTime).toInt(),
                                   minKickoffTime, maxKickoffTime);

    return {
        {QStringLiteral("pointsToWin"), points},
        {QStringLiteral("setsToWin"), sets},
        {QStringLiteral("winByTwo"), value("winByTwo", m_winByTwo).toBool()},
        {QStringLiteral("kickoffTime"), kickoff},
        {QStringLiteral("ballSpeed"), int(speed)},
        {QStringLiteral("serveSpeed"), ::serveSpeed(speed)},
        {QStringLiteral("maxSpeed"), 2.0 * ::serveSpeed(speed)},
        {QStringLiteral("paddleSize"), int(size)},
        {QStringLiteral("paddleLength"), ::paddleLength(size)},
        {QStringLiteral("difficulty"), int(cpu)},
        {QStringLiteral("arena"), value("arena", m_arena).toString()},
        {QStringLiteral("modifiers"), value("modifiers", m_modifiers).toBool()}
    };
}

void GameSettings::setRule(int mode, const QString& name, const QVariant& value)
{
    const QString prefix = QStringLiteral("rules/%1/").arg(std::max(mode, 0));
    QVariant validated;
    if (name == QLatin1String("pointsToWin"))
        validated = std::clamp(value.toInt(), minPointsToWin, maxPointsToWin);
    else if (name == QLatin1String("setsToWin"))
        validated = std::clamp(value.toInt(), 1, maxSetsToWin);
    else if (name == QLatin1String("winByTwo") || name == QLatin1String("modifiers"))
        validated = value.toBool();
    else if (name == QLatin1String("kickoffTime"))
        validated = std::clamp(value.toInt(), minKickoffTime, maxKickoffTime);
    else if (name == QLatin1String("ballSpeed"))
        validated = std::clamp(value.toInt(), int(BallSpeed::Slow), int(BallSpeed::Fast));
    else if (name == QLatin1String("paddleSize"))
        validated = std::clamp(value.toInt(), int(PaddleSize::Small), int(PaddleSize::Large));
    else if (name == QLatin1String("difficulty"))
        validated = std::clamp(value.toInt(), int(ComputerPlayer::Difficulty::Easy),
                               int(ComputerPlayer::Difficulty::Hard));
    else if (name == QLatin1String("arena"))
        validated = value.toString();
    else
        return;

    if (m_settings.value(prefix + name) == validated)
        return;
    m_settings.setValue(prefix + name, validated);
    emit rulesChanged(std::max(mode, 0));
}

void GameSettings::restoreRules(int mode)
{
    const QString prefix = QStringLiteral("rules/%1/").arg(std::max(mode, 0));
    m_settings.setValue(prefix + QStringLiteral("pointsToWin"), defaultPointsToWin);
    m_settings.setValue(prefix + QStringLiteral("setsToWin"), defaultSetsToWin);
    m_settings.setValue(prefix + QStringLiteral("winByTwo"), defaultWinByTwo);
    m_settings.setValue(prefix + QStringLiteral("kickoffTime"), defaultKickoffTime);
    m_settings.setValue(prefix + QStringLiteral("ballSpeed"), int(defaultBallSpeed));
    m_settings.setValue(prefix + QStringLiteral("paddleSize"), int(defaultPaddleSize));
    m_settings.setValue(prefix + QStringLiteral("difficulty"), int(defaultDifficulty));
    m_settings.setValue(prefix + QStringLiteral("arena"), defaultArena);
    m_settings.setValue(prefix + QStringLiteral("modifiers"), defaultModifiers);
    emit rulesChanged(std::max(mode, 0));
}

void GameSettings::setPointsToWin(int pointsToWin)
{
    pointsToWin = std::clamp(pointsToWin, minPointsToWin, maxPointsToWin);
    if (m_pointsToWin == pointsToWin)
        return;

    m_pointsToWin = pointsToWin;
    m_settings.setValue("pointsToWin", pointsToWin);
    emit pointsToWinChanged(pointsToWin);
}

int GameSettings::pointsToWin() const
{
    return m_pointsToWin;
}

void GameSettings::setBallSpeed(BallSpeed ballSpeed)
{
    if (m_ballSpeed == ballSpeed)
        return;

    m_ballSpeed = ballSpeed;
    m_settings.setValue("ballSpeed", int(ballSpeed));
    emit ballSpeedChanged(ballSpeed);
}

GameSettings::BallSpeed GameSettings::ballSpeed() const
{
    return m_ballSpeed;
}

void GameSettings::setPaddleSize(PaddleSize paddleSize)
{
    if (m_paddleSize == paddleSize)
        return;

    m_paddleSize = paddleSize;
    m_settings.setValue("paddleSize", int(paddleSize));
    emit paddleSizeChanged(paddleSize);
}

GameSettings::PaddleSize GameSettings::paddleSize() const
{
    return m_paddleSize;
}

void GameSettings::setDifficulty(ComputerPlayer::Difficulty difficulty)
{
    if (m_difficulty == difficulty)
        return;

    m_difficulty = difficulty;
    m_settings.setValue("difficulty", int(difficulty));
    emit difficultyChanged(difficulty);
}

ComputerPlayer::Difficulty GameSettings::difficulty() const
{
    return m_difficulty;
}

void GameSettings::setModifiers(bool modifiers)
{
    if (m_modifiers == modifiers)
        return;

    m_modifiers = modifiers;
    m_settings.setValue("modifiers", modifiers);
    emit modifiersChanged(modifiers);
}

bool GameSettings::modifiers() const
{
    return m_modifiers;
}

void GameSettings::setSetsToWin(int setsToWin)
{
    setsToWin = std::clamp(setsToWin, 1, maxSetsToWin);
    if (m_setsToWin == setsToWin)
        return;

    m_setsToWin = setsToWin;
    m_settings.setValue("setsToWin", setsToWin);
    emit setsToWinChanged(setsToWin);
}

int GameSettings::setsToWin() const
{
    return m_setsToWin;
}

void GameSettings::setWinByTwo(bool winByTwo)
{
    if (m_winByTwo == winByTwo)
        return;

    m_winByTwo = winByTwo;
    m_settings.setValue("winByTwo", winByTwo);
    emit winByTwoChanged(winByTwo);
}

bool GameSettings::winByTwo() const
{
    return m_winByTwo;
}

void GameSettings::setArena(const QString& arena)
{
    if (m_arena == arena)
        return;

    m_arena = arena;
    m_settings.setValue("arena", arena);
    emit arenaChanged(arena);
}

QString GameSettings::arena() const
{
    return m_arena;
}

void GameSettings::setSound(bool sound)
{
    if (m_sound == sound)
        return;

    m_sound = sound;
    m_settings.setValue("sound", sound);
    emit soundChanged(sound);
}

bool GameSettings::sound() const
{
    return m_sound;
}

void GameSettings::setMusic(bool music)
{
    if (m_music == music)
        return;

    m_music = music;
    m_settings.setValue("music", music);
    emit musicChanged(music);
}

bool GameSettings::music() const
{
    return m_music;
}

void GameSettings::setMusicVolume(int musicVolume)
{
    musicVolume = std::clamp(musicVolume, 0, 100);
    if (m_musicVolume == musicVolume)
        return;

    m_musicVolume = musicVolume;
    m_settings.setValue("musicVolume", musicVolume);
    emit musicVolumeChanged(musicVolume);
}

int GameSettings::musicVolume() const
{
    return m_musicVolume;
}

void GameSettings::setRumble(bool rumble)
{
    if (m_rumble == rumble)
        return;

    m_rumble = rumble;
    m_settings.setValue("rumble", rumble);
    emit rumbleChanged(rumble);
}

bool GameSettings::rumble() const
{
    return m_rumble;
}

void GameSettings::setKickoffTime(int kickoffTime)
{
    kickoffTime = std::clamp(kickoffTime, minKickoffTime, maxKickoffTime);
    if (m_kickoffTime == kickoffTime)
        return;

    m_kickoffTime = kickoffTime;
    m_settings.setValue("kickoffTime", kickoffTime);
    emit kickoffTimeChanged(kickoffTime);
}

int GameSettings::kickoffTime() const
{
    return m_kickoffTime;
}

void GameSettings::setMode(int mode)
{
    if (m_mode == mode)
        return;

    m_mode = mode;
    m_settings.setValue("mode", mode);
    emit modeChanged(mode);
}

int GameSettings::mode() const
{
    return m_mode;
}

void GameSettings::setLobbyPlayers(int lobbyPlayers)
{
    lobbyPlayers = std::clamp(lobbyPlayers, 2, 6);
    if (m_lobbyPlayers == lobbyPlayers)
        return;

    m_lobbyPlayers = lobbyPlayers;
    m_settings.setValue("lobbyPlayers", lobbyPlayers);
    emit lobbyPlayersChanged(lobbyPlayers);
}

int GameSettings::lobbyPlayers() const
{
    return m_lobbyPlayers;
}

void GameSettings::setNetwork(const QString& network)
{
    const QString value = network == QLatin1String("lan") ? QStringLiteral("lan") : QStringLiteral("internet");
    if (m_network == value)
        return;

    m_network = value;
    m_settings.setValue("network", value);
    emit networkChanged(value);
}

QString GameSettings::network() const
{
    return m_network;
}

void GameSettings::setLanAddress(const QString& lanAddress)
{
    if (m_lanAddress == lanAddress)
        return;

    m_lanAddress = lanAddress;
    m_settings.setValue("lanAddress", lanAddress);
    emit lanAddressChanged(lanAddress);
}

QString GameSettings::lanAddress() const
{
    return m_lanAddress;
}

void GameSettings::setPlayerName(const QString& playerName)
{
    const QString name = playerName.trimmed().left(12);
    if (m_playerName == name)
        return;

    m_playerName = name;
    m_settings.setValue("playerName", name);
    emit playerNameChanged(name);
}

QString GameSettings::playerName() const
{
    return m_playerName;
}

void GameSettings::setServerUrl(const QString& serverUrl)
{
    const QString url = serverUrl.trimmed();
    if (m_serverUrl == url)
        return;

    m_serverUrl = url;
    m_settings.setValue("serverUrl", url);
    emit serverUrlChanged(url);
}

QString GameSettings::serverUrl() const
{
    return m_serverUrl;
}

void GameSettings::setOnline(bool online)
{
    if (m_online == online)
        return;

    m_online = online;
    m_settings.setValue("online", online);
    emit onlineChanged(online);
}

bool GameSettings::online() const
{
    return m_online;
}

qreal GameSettings::serveSpeed() const
{
    return ::serveSpeed(m_ballSpeed);
}

qreal GameSettings::maxSpeed() const
{
    return 2.0 * serveSpeed();
}

qreal GameSettings::paddleLength() const
{
    return ::paddleLength(m_paddleSize);
}
