# Dodge Me If You Can — Project Instruction Manual

**Graphics Lab Project | OpenGL / GLFW / C++**

---

## Table of Contents
1. [What Is This Game?](#what-is-this-game)
2. [Project File Structure](#project-file-structure)
3. [How to Build & Run](#how-to-build--run)
4. [Controls](#controls)
5. [Game Rules & Scoring](#game-rules--scoring)
6. [Source Code Walkthrough](#source-code-walkthrough)
   - [main.cpp — Global State & Constants](#maincpp--global-state--constants)
   - [Shader System](#shader-system)
   - [VAO / VBO Setup](#vao--vbo-setup)
   - [Game Loop](#game-loop)
   - [Obstacle System](#obstacle-system)
   - [Dash / Charge System](#dash--charge-system)
   - [Power-Up System](#power-up-system)
   - [Collision Detection](#collision-detection)
   - [Score / Leaderboard System](#score--leaderboard-system)
   - [TextRenderer.cpp — Pixel Font Rendering](#textrenderercpp--pixel-font-rendering)
   - [glad.c — OpenGL Loader](#gladc--opengl-loader)

---

## What Is This Game?

**Dodge Me If You Can** is a 2D arcade dodger built from scratch using raw OpenGL (version 3.3 Core Profile). There are no game engines or helper libraries — every shape is a hand-coded VAO, every letter is a hand-drawn 5×7 pixel bitmap, and every animation is driven by a custom game loop delta-time system.

You control a red triangle at the bottom of the screen. Coloured blocks fall from the top. Your goal: survive as long as possible and beat your own high score.

---

## Project File Structure

```
project/
├── main.cpp            ← Main game logic, game loop, rendering
├── TextRenderer.cpp    ← Pixel-font bitmap definitions and rendering helpers
├── TextRenderer.h      ← Header: PixelQuad struct, function declarations
├── glad.c              ← Auto-generated OpenGL function loader (do not edit)
├── glad.h              ← Header for glad.c
├── glfw3.dll           ← GLFW runtime DLL (must be in same folder as .exe)
├── main.exe            ← Compiled executable
└── scores.txt          ← Auto-created at runtime; stores top 5 high scores
```

---

## How to Build & Run

### Prerequisites
- A C++ compiler (MinGW g++ on Windows recommended)
- GLFW3 library (static `.a` or dynamic `.dll`)
- glad loader (included as `glad.c`)

### Compile Command (MinGW / Windows)
```bash
g++ main.cpp glad.c TextRenderer.cpp -o main.exe -lglfw3 -lopengl32 -lgdi32
```

### Run
```bash
./main.exe
```
Make sure `glfw3.dll` is in the **same folder** as `main.exe`.

> See the VS Code section below for running with F5.

---

## Controls

| Key | Action |
|-----|--------|
| `←` Left Arrow | Move player left |
| `→` Right Arrow | Move player right |
| `↑` Up Arrow | **Dash** upward (only when charge bar is full) |
| `R` | Restart after Game Over |
| `Escape` / `E` / `N` | Quit the game |

---

## Game Rules & Scoring

- Obstacles fall from the top of the screen continuously.
- Your **score = how many seconds you survive**.
- Every **20 seconds**, all obstacle speeds increase by 25% (`speedMultiplier += 0.25`).
- The game stores the **top 5 scores** in `scores.txt`. If you beat the #1 score, you see **"YOU WIN"**; otherwise **"YOU LOSE"**.
- **Dash charge**: Dodge 5 close obstacles to fill the charge bar. Press UP to dash. Collecting a Power-Up instantly fills the bar.
- **After beating your high score**, your return speed from a dash slows down (making it harder — the game punishes you for being good!).

---

## Source Code Walkthrough

---

### `main.cpp` — Global State & Constants

At the top of the file, all game state is declared as global variables. This is a common pattern in small OpenGL projects.

| Variable | What It Does |
|----------|-------------|
| `SCR_WIDTH / SCR_HEIGHT` | Window size: 800×600 |
| `playerX / playerY` | Player's current NDC position |
| `PLAYER_Y_HOME` | The resting Y position of the player (`-0.6`) |
| `playerSpeed` | Starts at 1.2, grows slowly with elapsed time |
| `dashState` | An enum: `IDLE`, `LUNGING`, or `RETURNING` |
| `dashCharge / dashReady` | Current charge count (0–5) and whether the dash is ready to fire |
| `speedMultiplier` | Global multiplier applied to all obstacle speeds |
| `gameOver / beatHighScore` | Flags to control what overlay text is shown |
| `obstacles` | A `std::vector<Obstacle>` holding all active falling blocks |
| `powerUp / powerUpActive` | A single special pickup object and its active flag |

---

### Shader System

Two GLSL shaders are embedded directly as C++ string literals.

**Vertex Shader** (`vertexShaderSource`)
- Takes a 2D position (`aPos`)
- Applies `scale` (to resize shapes), `angle` (to rotate — used for the spinning power-up), and `offset` (to position in NDC space)
- This single shader handles the player, all obstacles, the power-up, and all text quads

**Fragment Shader** (`fragmentShaderSource`)
- Simply outputs whatever colour is passed via the `ourColor` uniform
- All colour changes (obstacle colours, text colours, pulse effects) happen by calling `glUniform4f(colorLoc, r, g, b, 1.0f)` before each draw

**Shader compilation** happens inside `main()` using a small lambda:
```cpp
auto mkShader = [](GLenum type, const char* src) { ... };
```
The two shaders are compiled, linked into a program (`prog`), and then the four uniform locations (`offset`, `scale`, `angle`, `ourColor`) are queried once and reused every frame.

---

### VAO / VBO Setup

Three separate VAO/VBO pairs are created:

| VAO | Shape | Usage |
|-----|-------|-------|
| `triVAO / triVBO` | Triangle (3 vertices) | The player character |
| `sqVAO / sqVBO` | Quad / 2-triangle rectangle | All obstacles and the power-up |
| `textVAO / textVBO` | Dynamic quad (12 floats, updated every draw) | Each pixel in the pixel-font text |

The text VAO is special — it's declared with `GL_DYNAMIC_DRAW` because its vertex data is overwritten every frame using `glBufferSubData` for each character pixel.

---

### Game Loop

The main `while (!glfwWindowShouldClose(window))` loop runs every frame. Each iteration:

1. **Time** — `dt` (delta time in seconds) is calculated from `glfwGetTime()`. All movement uses `dt` so the game runs at the same speed regardless of frame rate.
2. **Input** — `glfwPollEvents()` + `glfwGetKey()` reads keyboard state.
3. **Quit / Restart** — Escape/E/N quits. R restarts by resetting all state variables and re-spawning obstacles.
4. **Player movement** — Left/Right arrows move the player, clamped to screen edges.
5. **Dash state machine** — Handles `IDLE → LUNGING → RETURNING` transitions (see below).
6. **Speed ramp** — Every 20 seconds `speedMultiplier` increases.
7. **Obstacle spawning** — New obstacles are added on a random 10–21 second interval.
8. **Power-up spawning** — One power-up at a time, 18+ seconds apart.
9. **Obstacle movement & collision** — Each obstacle moves down by `speed × speedMultiplier × dt`. Collision triggers game over.
10. **Draw** — Clear screen, draw player, draw obstacles, draw charge bar, draw overlay text if game over.

---

### Obstacle System

**`struct Obstacle`** holds: position (x, y), speed, shape (rectangle or square), half-dimensions, RGB colour, rotation angle, scale, spawn delay, and flags (`active`, `isPowerUp`).

| Function | What It Does |
|----------|-------------|
| `createObstacle(delay)` | Randomly generates a new obstacle. 25% chance of being a rectangle (taller), 75% square. Calls `sampleObstaclePosition()` to find a non-crowded spawn point. |
| `resetObstacle(ob)` | Called when an obstacle goes below the screen (`y < -1.1`). Regenerates it above the screen with new random properties. |
| `sampleObstaclePosition(ob)` | Tries up to 64 grid positions + 200 random positions to find a spot that isn't too close to existing obstacles (using `tooCloseToOthers()`). |
| `randomObstacleColor(r,g,b)` | Picks a random colour from 5 cool-toned options (green, blue, cyan, teal, lime). |

Obstacles start above the screen (y = 1.2 to 4.0) and fall downward. Each has its own `speed` value so they fall at different rates.

---

### Dash / Charge System

The dash is a **charge-based special move**:

1. **Charging**: Each time an obstacle passes below the player AND is within 0.55 NDC units horizontally, `dashCharge` increases by 1.
2. **Ready**: When `dashCharge == 5`, `dashReady = true` and the charge bar pulses gold.
3. **Firing**: Pressing UP triggers `IDLE → LUNGING`. The player moves upward at `DASH_SPEED = 4.0` NDC/s toward `dashTargetY = PLAYER_Y_HOME + DASH_DIST` (0.45 units above home).
4. **Returning**: Once the target Y is reached, state switches to `RETURNING`. The player drops back at `RETURN_SPEED = 1.8` NDC/s (slower than the lunge, giving "air time").
5. **Surpassed high score penalty**: If you've already beaten your previous high score, the return speed is slowed further (`× 0.75`) making the game harder.

The dash charge bar is drawn as 5 rectangular segments at the bottom-centre of the screen. Filled = bright blue. All filled = pulsing gold (using `sinf(time * 6.0)` for the pulse).

---

### Power-Up System

A single power-up can be active at one time, spawning every 16–28 seconds.

- Created by `createPowerUp()`: spawns above the screen, falls slowly (speed 0.18–0.28).
- Visually animated: it **rotates** (`angle += POWERUP_ROT_SPEED * dt`) and its colour **cycles** through a teal-to-blue gradient using `fmod(time/3.0, 1.0)`.
- Its scale pulses up to 2× its base size.
- On **collision with player**: instantly sets `dashCharge = 5` and `dashReady = true` — effectively a free dash.
- If it exits the bottom of the screen without being collected, it resets and schedules the next one.

---

### Collision Detection

```cpp
bool checkCollision(float px, float py, const Obstacle& ob)
{
    return fabs(px - ob.x) < (playerHalfWidth  + ob.halfWidth)
        && fabs(py - ob.y) < (playerHalfHeight + ob.halfHeight);
}
```

Simple **AABB (Axis-Aligned Bounding Box)** collision. Checks if the distance between centres on both axes is less than the sum of their half-sizes. Called every frame for every active obstacle and the power-up.

---

### Score / Leaderboard System

| Function | What It Does |
|----------|-------------|
| `loadScores()` | Reads `scores.txt` on startup into `highScores` vector (max 5 entries) |
| `saveScores()` | Writes the current `highScores` vector back to `scores.txt` |
| `submitScore(seconds)` | Adds the current run's time, sorts descending, trims to 5, saves. Returns `true` if this run beats the #1 score. |

The return value of `submitScore()` determines whether the game over screen says **"YOU WIN"** (green) or **"YOU LOSE"** (orange-red).

---

### `TextRenderer.cpp` — Pixel Font Rendering

All on-screen text is drawn using **hand-coded 5×7 pixel bitmaps** — no font files or FreeType library needed.

**How bitmaps work**: Each character is stored as 5 bytes (one per column). Each byte is 7 bits tall. Bit 6 = top pixel, Bit 0 = bottom pixel. For example, `0x7F` = `1111111` in binary = a full vertical bar (all 7 pixels lit).

| Array | Content |
|-------|---------|
| `FONT_BASIC[26][5]` | Full A–Z alphabet for generic text |
| `FONT_GAMEOVER[9][5]` | Pre-baked "GAME OVER" glyph sequence |
| `FONT_YOUWON[8][5]` | Pre-baked "YOU WON!" glyphs |
| `FONT_YOUWIN[7][5]` | Pre-baked "YOU WIN" glyphs |
| `FONT_YOULOSE[8][5]` | Pre-baked "YOU LOSE" glyphs |

| Function | What It Does |
|----------|-------------|
| `getGlyphForChar(c)` | Returns the 5-byte bitmap for a given ASCII character |
| `buildText(glyphs, n, x, y, scale, quads)` | Converts a glyph array into a list of `PixelQuad` structs — one quad per lit pixel |
| `buildText(text, x, y, scale, quads)` | Overload that takes a plain `const char*` string |
| `drawPixelQuad(vbo, offsetLoc, colorLoc, q, r, g, b)` | Uploads one quad's vertices to the GPU via `glBufferSubData` and issues a `glDrawArrays` call |
| `prepareGameOverText(...)` | Convenience function that pre-builds all game-over overlay quads at startup so they don't need to be rebuilt every frame |

Each `PixelQuad` is just `{ x, y, w, h }` — the NDC position and size of one "pixel" in the font. At render time, `drawPixelQuad` converts that into 6 vertices (2 triangles) and draws them.

---

### `glad.c` — OpenGL Loader

This is an **auto-generated file** (by the GLAD tool at https://glad.dav1d.de). Do not edit it manually.

Its job: at runtime, dynamically load all modern OpenGL function pointers from the GPU driver DLL (`opengl32.dll` on Windows). Without it, you can only call very old OpenGL 1.1 functions.

It is initialized in `main()` with:
```cpp
gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)
```

After that single call, all `gl*` functions (like `glGenVertexArrays`, `glDrawArrays`, etc.) are available.

---

## Quick Reference — What Happens When You Press Each Key

| Key | Code Location | Effect |
|-----|--------------|--------|
| `←` | Game loop, line ~442 | `playerX -= playerSpeed * dt`, clamped to left edge |
| `→` | Game loop, line ~443 | `playerX += playerSpeed * dt`, clamped to right edge |
| `↑` | Game loop, line ~448–458 | If `dashReady && !dashKeyHeld`, fires dash: sets state to LUNGING |
| `R` | Game loop, line ~411–433 | Full game reset: clears obstacles, resets all timers and state |
| `Esc/E/N` | Game loop, line ~404–409 | Calls `glfwSetWindowShouldClose(window, true)` |
