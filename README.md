# P(H)ONG

Pong in a 3D rendered world on a 2D playing field, built with Qt Quick 3D and
Qt Quick 3D Physics. The rules, the computer opponent and the settings are
C++, the scenes are QML.

![Multi-ball on the Pinball arena](docs/screenshots/multi-ball.png)

| | |
|---|---|
| ![A rally heating up](docs/screenshots/rally.png) | ![Kickoff countdown and direction](docs/screenshots/kickoff.png) |
| ![Main menu](docs/screenshots/menu.png) | ![Graphics settings](docs/screenshots/graphics.png) |

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

| | Against the computer | 2 Players |
|---|---|---|
| Left paddle | `W`/`S` or `Up`/`Down` | `W`/`S` |
| Left smash | hold `Space`, `D` or `Left` | hold `D` |
| Right paddle | computer | `Up`/`Down` |
| Right smash | computer | hold `Left` |
| Dash | tap a direction twice | tap a direction twice |
| Special | `A` or `Right` | `A`, `Right` |
| Mouse / touch | drag anywhere | drag on your half |

`Esc` or `P` pauses and resumes, the pause menu also leads back to the main
menu. Menus take arrow keys, `Enter` and clicks.

Moving the paddle while it hits the ball puts spin on it: brushed upwards the
ball dips on its way over, brushed downwards it rises.

Holding the smash key winds up the paddle, it glows and slows down. The next
return is faster, a full wind up takes 0.6 seconds and even goes beyond the
top speed. The computer smashes too, on Normal now and then, on Hard often.

Tapping a direction twice dashes, the paddle shoots a few units that way.
After a dash it takes a second before the next one, a bar on the back of the
paddle fills up again meanwhile. On Normal and Hard the computer dashes for
balls it wouldn't reach otherwise.

Every hit fills the power bar under the field by a segment, a perfect hit by
two. A full bar pulses and allows the special, like a super move in a
fighting game: the paddle catches the next ball and holds it to aim, see the
Magnet below. The computer uses its special as soon as it can.

A ball met with the middle of a paddle that stands still is a perfect hit,
it rings higher and flies faster, also beyond the top speed.

Rallies heat up: every hit climbs a musical scale, the walls and the grid glow
hotter, and every fifth hit gets a cheer. When a ball is about to decide the
match, the game slows down and moves in closer.

Callouts cheer the big moments: a shield stopping a goal is a save, a hard
smash returned with a hard smash a double smash, and levelling the score
after trailing by three a comeback. The rally that decided the match is
replayed before the results, its last second in slow motion, any key skips
it.

## Game modes

- **1 Player** against the computer, the level is set in the settings.
- **2 Players** on one keyboard, or with two fingers on a touch screen.
- **3 to 6 Players**, a test: you against the computer on a regular polygon
  with a side for everybody, a triangle for three up to a hexagon for six.
  The middle of each side is its player's goal, posts at the ends keep the
  goals apart. Everybody has three balls to lose, a player who is out gets a
  wall instead of the goal, and the last one left wins. `Left`/`Right` or
  `A`/`D` move your paddle at the bottom.
- **Ladder** against Easy, Normal and Hard in a row, a loss can be retried.
- **Endless** against a computer getting harder and faster the longer you
  last, with three balls to lose. Every return scores a point, every ball the
  computer misses ten, the best score is kept.
- **Tournament**, a knockout bracket of eight against computer players with
  their own ways: the Rookie, the Pro and the Ace play like Easy, Normal and
  Hard, the Wall returns everything straight and never smashes, the Spinner
  brushes every ball to curve it, the Smasher winds up almost every return
  and the Collector sends the ball through the modifiers. The other matches
  of a round are decided by the strength of the two.
- **Bricks** against the computer, with a wall of bricks in the middle and a
  gap for the kickoff. A brick breaks when hit and gives the player who sent
  the ball a point, every third one drops a modifier instead. Once the wall
  is down a new one is built for the next kickoff.
- **Squash** alone against a closed wall with three balls to lose, the
  longest rally is the score.

The menu cycles through the modes with `Left`/`Right`, `Enter` plays.

On Normal and Hard the computer aims its returns through modifiers it wants,
Hard also plays the ball away from your paddle.

Every point starts with a kickoff countdown, one to three seconds, and an
arrow showing where the ball will go. Like in football the player who was
scored against kicks off, the ball flies towards the scorer.

A match is a single set or best of three or five, optionally won by two
points. The stats screen keeps the record against every computer level, the
best ladder run, the endless and squash high scores, the tournaments won and
the longest rally.

### Achievements

Twelve challenges wait on the achievements screen, next to the stats, from
the first win against the computer to a rally of 20 on Elevators, a goal
with a smash, five perfect hits in a match, a win after trailing by three
or 100 points in endless play. They count for the player against the
computer or the wall, the results show the ones a match brought. Resetting
the stats resets them too.

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
| `U` | Magnet | the collector's paddle catches the next three balls within 12 s |
| `()` | Portals | two linked portals open for 10 s, a ball flying into one comes out of the other |
| `*` | Freeze | curse, the opponent's paddle freezes for 1.5 s |
| `?` | Reversed | curse, the opponent's controls are swapped for 7 s |
| `~` | Ghost ball | the balls can't be seen in the middle third of the field for 8 s |
| `O` | Gravity well | a well bends the flight of the balls for 10 s |

A magnetic paddle holds a caught ball for up to 1.2 seconds. Meanwhile the
paddle stands and the movement keys slide the ball along it, the ball leaves
at the angle of where it sits. Letting go of the smash key throws it, wound
up as long as the key was held.

Active effects show next to the player names. Modifiers can be switched off
in the settings. Curses get to the computer as well: reversed it reacts
slower and less accurately, and it can't follow a ghost ball either.

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
| `effect` | `ballSpeed`, `paddleSize`, `shield`, `spin`, `narrowField`, `multiBall`, `magnet`, `portals`, `freeze`, `reverse`, `ghostBall` or `gravityWell` |
| `target` | `collector` (default), `opponent` for curses, or `both` |
| `value` | speed or length factor, spin in degrees per second, inset of the walls, number of extra balls, catches, strength of the well |
| `duration` | seconds the effect lasts, all but `ballSpeed` and `shield` |
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

Bumpers and blocks may move, `move` swings them back and forth around their
position, `period` is the time of a swing in seconds and `phase` where it
starts, from 0 to 1:

```json
{ "x": -5, "y": 0, "radius": 1.1, "move": { "y": 5, "period": 4, "phase": 0.5 } }
```

Obstacles outside the field or on the serve spot in the middle, also while
moving, are skipped with a warning.

## Graphics

P(H)ONG is Pong with Phong shading: every surface is lit by a key light from
above in front and shows a specular highlight, the ball is a shiny sphere. On
top of that bright surfaces glow like neon, the field has a floor with a grid
and soft shadows, and stars drift behind all screens.

Five themes set the colors: Neon, Classic in black, grey and white, Paper,
the four greens of the Game Boy and an amber monitor. The classic ones map
the colors of the modifiers onto their few shades.

![The classic themes](docs/screenshots/themes.png)

The graphics screen in the settings picks the theme and switches the rest on
and off: shading
(Phong or flat, unlit colors), glow (low, medium, high), anti-aliasing (fast is FXAA, 2x and 4x
multisampling), stars, floor, shadows and a frame rate display. The browser
and phones start with low glow and fast anti-aliasing, the desktop with high
glow and 4x multisampling.

## Sound

The sound effects are synthesized from a few tones, there are no audio files.
The desktop plays them with Qt Multimedia, the browser with the Web Audio API,
which starts after the first key press or click.

The music is made of the same tones, a chiptune loop over four chords that
the game sequences as it plays. It starts with a bass line at the kickoff,
and as the rally grows hi-hats, drums, an arpeggio and a lead come in and
the tempo rises. In slow motion it slows down too. The desktop sequences it
on the audio thread, the browser schedules it ahead on the audio clock, so
the beat keeps its time when the game is busy.

Sound and music can be switched off separately in the settings. `QT_LOGGING_RULES="phong.sound.debug=true"` logs what the audio
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
