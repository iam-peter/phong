# P(H)ONG

## Requirements

Qt 5.13.0 (`minExtend` and `maxExtended`  for `QGeometry`)

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

## Issues
- Couldn't use `onStarted`, `onFinished` and `onRunningChanged` signals in a nested `Vector3dAnimation` inside of a `Transition`. Solved by using a `SequentialAnimation` combined with two `ScriptAction`s at the beginning and end.

- Can't use `Camera`s `void translate(vLocal, enumeration option)` together with `Transition`. This is why setting of the camera `position` and `viewCenter` is done manually.

- I had trouble using `KeyboardHandler` together with `KeyboardDevice` in each of the different `Scene` types I've created. Using `focus` or `enabled` wasn't work toggle each individual on and off. Once focused each `KeyboardHandler` received the same event even though it was `accepted`.

- `KeyboardHandler` `onPressed` and `onEscapePressed` will bothe be triggered even pressing the escape key even though it was accepted in the `onPressed` call already.
    ```
        KeyboardHandler {
            id: keyboardHandler
            sourceDevice: keyboardDevice
            focus: true
            onPressed: {
                console.log("KeyboardHandler | onPressed")
                console.log("event " + event + " " + event.accepted)
                Logic.currentScene().onPressed(event)
            }
            onEscapePressed: {
                console.log("KeyboardHandler | onEscapePressed")
                console.log("event " + event + " " + event.accepted)
                Logic.currentScene().onEscapePressed(event)
            }
        }
    ```
    Has strange side effects when used together with `FirstPersonCameraController`. The camera `position` and `viewCenter` will be set to a strange value, not set by the scene logic.

TODO: Hint about **resetting code model** `Tools > QML/JS > Reset Code Model`
