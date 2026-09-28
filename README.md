# P(H)ONG

Pong in a 3D rendered world on a 2D playing field, built with Qt Quick 3D and
Qt Quick 3D Physics. The rules, the computer opponent and the settings are
C++, the scenes are QML.

## Requirements

- Qt 6.9 or newer (`ExtrudedTextGeometry`) with the Qt Quick 3D and
  Qt Quick 3D Physics modules
- Qt Multimedia for sound on the desktop, optional
- CMake 3.21 or newer
- For WebAssembly: the Qt `wasm_singlethread` kit and the Emscripten version
  it was built with (Qt 6.11: 4.0.7, Qt 6.12: 5.0.5)

## Checkout

```
git clone https://github.com/iam-peter/phong.git
```

## Build

### Desktop

```
~/Qt/6.11.1/gcc_64/bin/qt-cmake -S . -B build/desktop -G Ninja
cmake --build build/desktop
ctest --test-dir build/desktop
./build/desktop/phong
```

### WebAssembly

```
source ~/emsdk/emsdk_env.sh
~/Qt/6.11.1/wasm_singlethread/bin/qt-cmake -S . -B build/wasm -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DQT_HOST_PATH=$HOME/Qt/6.11.1/gcc_64
cmake --build build/wasm
python3 -m http.server -d build/wasm
```

Then open <http://localhost:8000/phong.html>. Settings are kept in the
browser's local storage.

## Controls

| | 1 Player | 2 Players |
|---|---|---|
| Left paddle | `W`/`S` or `Up`/`Down` | `W`/`S` |
| Right paddle | computer | `Up`/`Down` |
| Mouse / touch | drag anywhere | drag on your half |

`Esc` pauses, `Esc` again goes back to the menu, `Space` or `P` toggles the
pause. Menus take arrow keys, `Enter` and clicks.

Moving the paddle while it hits the ball puts spin on it: brushed upwards the
ball dips on its way over, brushed downwards it rises.

## Game modes

- **1 Player** against the computer, the level is set in the settings.
- **2 Players** on one keyboard, or with two fingers on a touch screen.
- **Ladder** against Easy, Normal and Hard in a row, a loss can be retried.

On Normal and Hard the computer aims its returns through modifiers it wants,
Hard also plays the ball away from your paddle.

Every point starts with a kickoff countdown, one to three seconds, and an
arrow showing where the ball will go. Like in football the player who was
scored against kicks off, the ball flies towards the scorer.

A match is a single set or best of three or five, optionally won by two
points. The stats screen keeps the record against every computer level, the
best ladder run and the longest rally.

## Modifiers

Now and then a modifier appears on the field. The ball collects it by flying
through, and the player who touched the ball last gets the effect. Right
after a serve the ball flies through without collecting anything.

| | Modifier | Effect |
|---|---|---|
| `>>` | Fast ball | the ball speeds up towards the opponent |
| `+` | Big paddle | the collector's paddle grows for 12 s |
| `\|\|` | Shield | a barrier behind the collector's paddle stops one goal, it stays until hit |
| `-` | Small paddle | curse, the opponent's paddle shrinks for 12 s |
| `@` | Spin curse | curse, the opponent's paddle rotates for 7 s and the ball bounces off its surface |
| `=` | Narrow field | the walls move in for 12 s |
| `oo` | Multi ball | two extra balls fly at the opponent for 12 s, their goals count |

Active effects show next to the player names. Modifiers can be switched off
in the settings.

### Configuration

The modifiers are defined in [config/modifiers.json](config/modifiers.json),
which is compiled into the game. On the desktop another file can be tried
without rebuilding:

```
./build/desktop/phong --modifiers my-modifiers.json
```

The effects are built in, the file decides which modifiers exist and how they
use them:

| Key | Meaning |
|---|---|
| `spawn.minDelay`, `spawn.maxDelay` | seconds between two spawns |
| `spawn.maxItems` | items on the field at the same time |
| `spawn.lifetime` | seconds until an item that nobody collected disappears |
| `spawn.minDistance` | minimum distance between two items |
| `id`, `name`, `glyph`, `color` | identity and look |
| `effect` | `ballSpeed`, `paddleSize`, `shield`, `spin`, `narrowField` or `multiBall` |
| `target` | `collector` (default), `opponent` for curses, or `both` |
| `value` | speed or length factor, spin in degrees per second, inset of the walls, number of extra balls |
| `duration` | seconds the effect lasts, for `paddleSize`, `spin`, `narrowField` and `multiBall` |
| `weight` | relative spawn chance, `0` never spawns |
| `enabled` | `false` skips the entry |

Values are clamped to a playable range, invalid entries are skipped with a
warning. A slow ball, for example, is the ball speed effect below 1:

```json
{ "id": "slowBall", "name": "Slow ball", "glyph": "<<", "color": "#ffff66",
  "effect": "ballSpeed", "value": 0.6 }
```

## Arenas

Every match picks an arena with round bumpers or blocks in the middle of the
field, or plays the one chosen in the settings. They are defined in
[config/arenas.json](config/arenas.json) and can be tried without rebuilding
with `--arenas my-arenas.json`:

```json
{ "id": "bumpers", "name": "Bumpers",
  "bumpers": [ { "x": 0, "y": 5, "radius": 1.3 } ],
  "blocks": [ { "x": 0, "y": -7, "width": 1, "height": 5 } ] }
```

Obstacles outside the field or on the serve spot in the middle are skipped
with a warning.

## Graphics

P(H)ONG is Pong with Phong shading: every surface is lit by a key light from
above in front and shows a specular highlight, the ball is a shiny sphere. On
top of that bright surfaces glow like neon, the field has a floor with a grid
and soft shadows, and stars drift behind all screens.

The graphics screen in the settings switches these on and off: shading
(Phong or flat, unlit colors), glow, anti-aliasing (fast is FXAA, 2x and 4x
multisampling), stars, floor, shadows and a frame rate display. The browser
and phones start with low glow and fast anti-aliasing, the desktop with high
glow and 4x multisampling.

## Sound

The sound effects are synthesized from a few tones, there are no audio files.
The desktop plays them with Qt Multimedia, the browser with the Web Audio API,
which starts after the first key press or click. Sound can be switched off in
the settings. `QT_LOGGING_RULES="phong.sound.debug=true"` logs what the audio
output does.

## Physics

Qt 3D, which the first version was built on, is not available for
WebAssembly, Qt Quick 3D is. That also made Box2D unnecessary: Qt Quick 3D
Physics (PhysX) does the collision detection, continuous collision detection
and goal triggers. The bodies are locked to the z = 0 plane, which turns the
3D engine into a 2D one.

The engine moves the ball, `Match` decides where it bounces to. Pong wants
arcade physics, a constant speed that grows with every hit and an angle that
depends on where the ball hits the paddle, so every contact report is
answered with the velocity the rules ask for.
