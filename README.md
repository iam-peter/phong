# P(H)ONG

Pong in a 3D rendered world on a 2D playing field, built with Qt Quick 3D and
Qt Quick 3D Physics. The rules, the computer opponent and the settings are
C++, the scenes are QML.

## Requirements

- Qt 6.9 or newer (`ExtrudedTextGeometry`) with the Qt Quick 3D and
  Qt Quick 3D Physics modules
- CMake 3.21 or newer
- For WebAssembly: the Qt `wasm_singlethread` kit and the Emscripten version
  it was built with (Qt 6.11: 4.0.7, Qt 6.12: 5.0.5)

## Checkout

```
git clone https://github.com/iam-peter/phong-cpp.git
```

## Build

### Desktop

```
~/Qt/6.11.1/gcc_64/bin/qt-cmake -S . -B build/desktop -G Ninja
cmake --build build/desktop
ctest --test-dir build/desktop
./build/desktop/phong-cpp
```

### WebAssembly

```
source ~/emsdk/emsdk_env.sh
~/Qt/6.11.1/wasm_singlethread/bin/qt-cmake -S . -B build/wasm -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DQT_HOST_PATH=$HOME/Qt/6.11.1/gcc_64
cmake --build build/wasm
python3 -m http.server -d build/wasm
```

Then open <http://localhost:8000/phong-cpp.html>. Settings are kept in the
browser's local storage.

## Controls

| | 1 Player | 2 Players |
|---|---|---|
| Left paddle | `W`/`S` or `Up`/`Down` | `W`/`S` |
| Right paddle | computer | `Up`/`Down` |
| Mouse / touch | drag anywhere | drag on your half |

`Esc` pauses, `Esc` again goes back to the menu, `Space` or `P` toggles the
pause. Menus take arrow keys, `Enter` and clicks.

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

## Task

> Let's start series of simple Qt challenges in which we'll try to build often well known mini games or mechanisms by using our green framework.
>
> __#1 Qt task - Pong__
>
> In task number one the goal is to create Pong game. Probably all of you are familiar with this game in which two players are trying to get the point by buncing back the dot so that the opponent does not.
>
> Requirements:
> - Simple interface, built without using assets
> - Counting and displaying points
> - Steering for at least one player
>
> This task is rather simple one, which has to be a kind of warmup. But if you need something more sophisticated - just free your imagination and tune pong game as you wish!
Share your solution as a git link to start discussion about used Qt features and compare it to the others!
