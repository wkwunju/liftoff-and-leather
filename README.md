# Liftoff & Leather

A fan-made parody vertical shmup, written in C and compiled to WebAssembly with Emscripten and raylib. Two cartoon pilots — one with dark hair, a black t-shirt, and a chunky rocket, one with grey hair, glasses, and a black leather jacket — fly against alien bugs and little robots.

**Fan-made parody. Not affiliated with or endorsed by Elon Musk, Jensen Huang, or their companies.**

Nothing here is a photo, a logo, or a trademarked asset. Ships, bugs, and the boss are drawn with shapes in code. The pilots only fight bugs and drones.

The public site is [https://wkwunju.github.io/liftoff-and-leather/](https://wkwunju.github.io/liftoff-and-leather/) after GitHub Pages finishes a deploy from `main`.

## Play the bundle

The static site is already built in `dist/`:

- `dist/index.html`
- `dist/index.js`
- `dist/index.wasm`

Serve that folder from any static host (GitHub Pages, Cloudflare Pages, or a local file server). Browsers will not load the wasm module from a `file://` URL. Python’s server sends the right wasm type:

```bash
python3 -m http.server 8747 --directory dist
```

Open http://127.0.0.1:8747/

## Build

You need the Emscripten SDK (`emcc` on `PATH`), GNU make, and git. Raylib 5.5 is cloned into `third_party/raylib` on the first build and is not committed.

```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

From this repo:

```bash
./build.sh
```

`make` does the same thing once `emcc` is on `PATH`. Output:

```
dist/index.html
dist/index.js
dist/index.wasm
```

Tested with Emscripten 7.0 and raylib 5.5.

## Controls

Hangar:

- Up / Down or Left / Right to highlight a pilot
- Enter, Space, or Z to launch. 1 is Elon, 2 is Jensen, 3 is local co-op
- 4 or B starts boss practice with the highlighted pilot
- Click or tap a card to launch that pilot. Tap “Boss practice” for the beetle

Flight:

- Move with the arrow keys or WASD. In a solo run, either set works
- Hold Space or Z to shoot
- Co-op, one keyboard: pilot 1 (rocket) uses WASD and Space or Z. Pilot 2 (lasers) uses the arrow keys and Right Shift, Right Ctrl, or L
- P pauses, R restarts the run, M mutes the beeps, Esc returns to the hangar
- A gamepad’s left stick and south button (or right trigger) fly pilot 1

Touch:

- Tap a card on the title screen
- Drag anywhere on the playfield to fly. The ship fires while a finger is down
- The II button in the corner pauses

## The run

- Elon is a bit slower and fires heavy rockets
- Jensen is quicker and fires a stream of green chip-shaped lasers
- Wave 1 is skitter bugs, wave 2 adds dive drones, wave 3 adds pod aliens, then Queen Beetle
- Pickups: F is a fan shot, Z is faster fire, H is a one-hit shield
- Three lives each, a shared score, then either “Out of fuel” or “Bugs cleared”. Enter or the on-screen button flies again

Sound is a handful of synthesized beeps. Browsers stay silent until the first key or tap.

## Licenses

The game code in this repo is original. [raylib](https://github.com/raysan5/raylib) is by Ramon Santamaria, zlib licensed, and is downloaded at build time.

## Disclaimer

Fan-made parody. Not affiliated with or endorsed by Elon Musk, Jensen Huang, or their companies.
