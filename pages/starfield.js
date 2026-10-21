// The stars behind the game's scenes, see qml/Starfield.qml: 900 small
// cubes in a box far behind the field, turning once around the z axis in
// 15 minutes, seen from where the camera looks at the menu. Scrolling the
// page moves the camera down like a flight to the next scene.
//
//   const field = startStarfield(canvas)
//   field.stop()
"use strict";

function startStarfield(canvas, options = {}) {
    const count = options.count ?? 900;
    const period = 900; // seconds per turn, like the NumberAnimation
    const background = "#05091a";
    const starFrom = [0x73, 0x89, 0xff];
    const starTo = [0xff, 0xe6, 0xff];
    const tanHalfFieldOfView = Math.tan(22.5 * Math.PI / 180);

    // RandomInstancing with its own generator, seeded all the same
    let seed = options.seed ?? 7;
    function random() {
        seed = (seed + 0x6d2b79f5) | 0;
        let t = Math.imul(seed ^ (seed >>> 15), 1 | seed);
        t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
        return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    }
    const range = (from, to) => from + (to - from) * random();

    // The outline of a turned cube seen from the front, its faces are all
    // the same unlit color
    function outline(half, rx, ry, rz) {
        const [sx, cx, sy, cy, sz, cz] = [Math.sin(rx), Math.cos(rx), Math.sin(ry),
                                          Math.cos(ry), Math.sin(rz), Math.cos(rz)];
        const points = [];
        for (const x of [-half, half]) {
            for (const y of [-half, half]) {
                for (const z of [-half, half]) {
                    const y1 = y * cx - z * sx, z1 = y * sx + z * cx;
                    const x2 = x * cy + z1 * sy;
                    points.push([x2 * cz - y1 * sz, x2 * sz + y1 * cz]);
                }
            }
        }
        points.sort((a, b) => a[0] - b[0] || a[1] - b[1]);
        const cross = (o, a, b) => (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0]);
        const hull = [];
        for (const pass of [points, points.slice().reverse()]) {
            const start = hull.length;
            for (const p of pass) {
                while (hull.length >= start + 2 && cross(hull[hull.length - 2], hull[hull.length - 1], p) <= 0)
                    hull.pop();
                hull.push(p);
            }
            hull.pop();
        }
        return hull.flat();
    }

    const stars = [];
    for (let i = 0; i < count; ++i) {
        const x = range(-260, 260), y = range(-200, 200), z = range(-300, -70);
        // A #Cube is 100 units, scaled by 0.004 and then 0.4 to 2
        const half = 0.5 * 100 * 0.004 * range(0.4, 2.0);
        const shape = outline(half, range(0, 2 * Math.PI), range(0, 2 * Math.PI), range(0, 2 * Math.PI));
        const color = starFrom.map((from, c) => Math.round(range(from, starTo[c])));
        stars.push({ x, y, z, shape, color: `rgb(${color})` });
    }
    // Far ones first
    stars.sort((a, b) => a.z - b.z);

    const context = canvas.getContext("2d");
    const reducedMotion = window.matchMedia("(prefers-reduced-motion: reduce)");
    let width = 0, height = 0, ratio = 1;
    let frame = 0;
    const startTime = performance.now();
    let lastTime = startTime;

    function resize() {
        ratio = Math.min(window.devicePixelRatio || 1, 2);
        width = canvas.clientWidth;
        height = canvas.clientHeight;
        canvas.width = Math.round(width * ratio);
        canvas.height = Math.round(height * ratio);
    }

    function draw(now) {
        lastTime = now;
        if (canvas.clientWidth !== width || canvas.clientHeight !== height)
            resize();

        // The camera of Main.qml in front of the menu scene
        const aspect = Math.max(width, 1) / Math.max(height, 1);
        const distance = Math.max(20, 23 / (tanHalfFieldOfView * aspect), 13 / tanHalfFieldOfView);
        const cameraX = -Math.max(40, distance * tanHalfFieldOfView * aspect + 23 + 1);
        // One pixel of scrolling is one pixel on the plane of the scenes
        const unitsPerPixel = 2 * distance * tanHalfFieldOfView / Math.max(height, 1);
        const cameraY = -(options.scroll?.() ?? window.scrollY) * unitsPerPixel;
        const focal = 0.5 * height / tanHalfFieldOfView;

        const angle = 2 * Math.PI * ((now - startTime) / 1000) / period;
        const sin = Math.sin(angle), cos = Math.cos(angle);

        context.setTransform(ratio, 0, 0, ratio, 0, 0);
        context.fillStyle = background;
        context.fillRect(0, 0, width, height);

        for (const star of stars) {
            const scale = focal / (distance - star.z);
            const sx = 0.5 * width + (star.x * cos - star.y * sin - cameraX) * scale;
            const sy = 0.5 * height - (star.x * sin + star.y * cos - cameraY) * scale;
            if (sx < -8 || sx > width + 8 || sy < -8 || sy > height + 8)
                continue;

            // The cube turns with the field
            const shape = star.shape;
            context.beginPath();
            for (let p = 0; p < shape.length; p += 2) {
                const px = sx + (shape[p] * cos - shape[p + 1] * sin) * scale;
                const py = sy - (shape[p] * sin + shape[p + 1] * cos) * scale;
                if (p === 0)
                    context.moveTo(px, py);
                else
                    context.lineTo(px, py);
            }
            context.fillStyle = star.color;
            context.fill();
        }
    }

    function tick(now) {
        draw(now);
        frame = reducedMotion.matches ? 0 : requestAnimationFrame(tick);
    }

    // Without motion the stars still follow scrolling and resizing
    function redraw() {
        if (!frame)
            draw(lastTime);
    }
    window.addEventListener("scroll", redraw, { passive: true });
    window.addEventListener("resize", redraw);
    reducedMotion.addEventListener("change", () => {
        if (!reducedMotion.matches && !frame)
            frame = requestAnimationFrame(tick);
    });

    resize();
    tick(performance.now());

    return {
        stop() {
            cancelAnimationFrame(frame);
            frame = -1;
            window.removeEventListener("scroll", redraw);
            window.removeEventListener("resize", redraw);
        }
    };
}
