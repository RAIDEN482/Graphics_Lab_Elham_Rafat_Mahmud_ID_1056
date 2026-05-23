#include "glad.h"
#include "glfw3.h"
#include <cmath>
#include <ctime>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include "TextRenderer.h"

// ── Window ────────────────────────────────────────────────────────────────────
const unsigned int SCR_WIDTH  = 800;
const unsigned int SCR_HEIGHT = 600;
void framebuffer_size_callback(GLFWwindow* window, int width, int height);

// ── Player ────────────────────────────────────────────────────────────────────
float playerX = 0.0f;
float playerY = -0.6f;
const float PLAYER_Y_HOME    = -0.6f;   // resting Y position
const float playerHalfWidth  =  0.05f;
const float playerHalfHeight =  0.05f;
float playerSpeed = 1.2f;               // grows over time

// ── Dash ability (charge-based) ───────────────────────────────────────────────
// Dodge 5 obstacles to fully charge. Press UP when ready to dash.
enum class DashState { IDLE, LUNGING, RETURNING };
DashState   dashState        = DashState::IDLE;
const float DASH_DIST        =  0.45f;  // NDC units lunged upward (longer dash)
const float DASH_SPEED       =  4.0f;   // lunge speed (NDC/s)
const float RETURN_SPEED     =  1.8f;   // return speed (NDC/s) for longer air time
float       dashTargetY      =  0.0f;
bool        dashKeyHeld      = false;
// Charge counter
const int   DASH_CHARGE_MAX  = 5;       // dodges needed for full charge
int         dashCharge       = 0;       // 0 .. DASH_CHARGE_MAX
bool        dashReady        = false;

// ── Obstacles ─────────────────────────────────────────────────────────────────
struct Obstacle {
    float x, y, speed;
    float spawnDelay;
    bool  active;
    bool  isRectangle;
    float halfWidth;
    float halfHeight;
    float r, g, b;
    bool  isPowerUp;
    float angle;
    float baseScale;
};
std::vector<Obstacle> obstacles;
Obstacle powerUp;
bool      powerUpActive      = false;
float     nextPowerUpSpawn   = 18.0f;
const float obstacleHalfSize  = 0.07f;
const int   INITIAL_OBSTACLES = 8;
const int   MAX_OBSTACLES     = 30;
float nextObstacleSpawn       = 12.0f;
int   obstacleSpawnInterval   = 10;
const float POWERUP_ROT_SPEED   = 2.2f;
const float POWERUP_SCALE_SPEED = 0.3f;
const float POWERUP_BASE_SIZE   = 0.08f;

// ── Speed ─────────────────────────────────────────────────────────────────────
float speedMultiplier   = 1.0f;
float nextSpeedIncrease = 20.0f;  // ramp every 20 s

// ── State ─────────────────────────────────────────────────────────────────────
bool gameOver  = false;
bool beatHighScore = false;
bool surpassedHighScore = false;
float previousHighScore = 0.0f;

// ── Leaderboard ───────────────────────────────────────────────────────────────
const std::string SCORE_FILE = "scores.txt";
const int         MAX_SCORES = 5;
std::vector<int>  highScores;

void loadScores()
{
    highScores.clear();
    std::ifstream f(SCORE_FILE);
    if (!f.is_open()) return;
    int s;
    while (f >> s && (int)highScores.size() < MAX_SCORES)
        highScores.push_back(s);
}

void saveScores()
{
    std::ofstream f(SCORE_FILE);
    for (int s : highScores) f << s << "\n";
}

bool submitScore(int seconds)
{
    bool beaten = highScores.empty() || seconds > highScores[0];
    highScores.push_back(seconds);
    std::sort(highScores.begin(), highScores.end(), std::greater<int>());
    if ((int)highScores.size() > MAX_SCORES) highScores.resize(MAX_SCORES);
    saveScores();
    return beaten;
}

// ── Shaders ───────────────────────────────────────────────────────────────────
const char *vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
uniform vec2 offset;
uniform vec2 scale;
uniform float angle;
void main() {
    vec2 scaled = aPos * scale;
    float c = cos(angle);
    float s = sin(angle);
    vec2 rotated = vec2(scaled.x * c - scaled.y * s,
                        scaled.x * s + scaled.y * c);
    gl_Position = vec4(rotated + offset, 0.0, 1.0);
}
)";

const char *fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
uniform vec4 ourColor;
void main() { FragColor = ourColor; }
)";

// ── Pixel-font 5x7 bitmaps ────────────────────────────────────────────────────
// Each row of bits: bit6 = top pixel, bit0 = bottom pixel (correct orientation)
// Moved to TextRenderer.cpp for shared access
// ── Text quad ─────────────────────────────────────────────────────────────────
// moved to TextRenderer.h for shared access

// Builds a list of NDC quads for the given glyph array.
// startX/startY = top-left of first character.
// scale = NDC size of one pixel.
// moved to TextRenderer.cpp for shared access
// ── Obstacle helpers ──────────────────────────────────────────────────────────
void randomObstacleColor(float& r, float& g, float& b)
{
    switch (rand() % 5)
    {
        case 0: r = 0.0f; g = 0.9f; b = 0.3f; break; // green
        case 1: r = 0.0f; g = 0.5f; b = 1.0f; break; // blue
        case 2: r = 0.0f; g = 0.8f; b = 0.9f; break; // cyan
        case 3: r = 0.0f; g = 0.7f; b = 0.4f; break; // teal
        default: r = 0.0f; g = 0.9f; b = 0.2f; break; // lime
    }
}

bool tooCloseToOthers(float x, float y, float halfWidth, float halfHeight)
{
    const float MARGIN = 0.08f;
    for (auto& o : obstacles)
    {
        if (!o.active) continue;
        float requiredX = o.halfWidth + halfWidth + MARGIN;
        float requiredY = o.halfHeight + halfHeight + MARGIN;
        if (fabs(o.x - x) < requiredX && fabs(o.y - y) < requiredY)
            return true;
    }
    return false;
}

bool sampleObstaclePosition(Obstacle& ob)
{
    const float MIN_X = -0.88f;
    const float MAX_X =  0.88f;
    const float MIN_Y =  1.2f;
    const float MAX_Y =  4.0f;

    const float xSamples[] = {-0.80f, -0.56f, -0.32f, -0.08f, 0.16f, 0.40f, 0.64f, 0.88f};
    const float ySamples[] = {1.25f, 1.60f, 1.95f, 2.30f, 2.65f, 3.00f, 3.35f, 3.70f};
    int candidates[64][2];
    int total = 0;
    for (int yi = 0; yi < 8; ++yi)
        for (int xi = 0; xi < 8; ++xi)
            candidates[total][0] = xi, candidates[total++][1] = yi;

    for (int i = 0; i < total; ++i) {
        int j = rand() % total;
        std::swap(candidates[i][0], candidates[j][0]);
        std::swap(candidates[i][1], candidates[j][1]);
    }

    for (int i = 0; i < total; ++i)
    {
        int xi = candidates[i][0];
        int yi = candidates[i][1];
        ob.x = xSamples[xi] + ((rand() % 61) - 30) / 500.0f;
        ob.y = ySamples[yi] + ((rand() % 61) - 30) / 500.0f;
        if (!tooCloseToOthers(ob.x, ob.y, ob.halfWidth, ob.halfHeight))
            return true;
    }

    for (int i = 0; i < 200; ++i)
    {
        ob.x = MIN_X + ((rand() % 100) / 100.0f) * (MAX_X - MIN_X);
        ob.y = MIN_Y + ((rand() % 280) / 100.0f);
        if (!tooCloseToOthers(ob.x, ob.y, ob.halfWidth, ob.halfHeight))
            return true;
    }

    return false;
}

Obstacle createObstacle(float delay = 0.0f)
{
    Obstacle ob;
    ob.isRectangle = (rand() % 4 == 0);
    if (ob.isRectangle) {
        ob.halfWidth  = obstacleHalfSize * (1.4f + (rand() % 80) / 100.0f);
        ob.halfHeight = obstacleHalfSize * (1.1f + (rand() % 160) / 100.0f);
    } else {
        float size = obstacleHalfSize * (0.8f + (rand() % 80) / 100.0f);
        ob.halfWidth  = size;
        ob.halfHeight = size;
    }
    sampleObstaclePosition(ob);
    ob.speed      = 0.30f + (rand() % 160) / 400.0f;
    ob.spawnDelay = delay;
    ob.active     = (delay <= 0.0f);
    ob.isPowerUp  = false;
    randomObstacleColor(ob.r, ob.g, ob.b);
    ob.angle     = 0.0f;
    ob.baseScale = 1.0f;
    return ob;
}

void resetObstacle(Obstacle& ob)
{
    ob.isRectangle = (rand() % 4 == 0);
    if (ob.isRectangle) {
        ob.halfWidth  = obstacleHalfSize * (1.4f + (rand() % 80) / 100.0f);
        ob.halfHeight = obstacleHalfSize * (1.1f + (rand() % 160) / 100.0f);
    } else {
        float size = obstacleHalfSize * (0.8f + (rand() % 80) / 100.0f);
        ob.halfWidth  = size;
        ob.halfHeight = size;
    }
    sampleObstaclePosition(ob);
    ob.speed      = 0.30f + (rand() % 160) / 400.0f;
    ob.active     = true;
    randomObstacleColor(ob.r, ob.g, ob.b);
}

Obstacle createPowerUp(float delay = 0.0f)
{
    Obstacle ob;
    ob.x          = ((rand() % 160) - 80) / 100.0f;
    ob.y          = 1.1f + (rand() % 120) / 100.0f;
    ob.speed      = 0.18f + (rand() % 80) / 800.0f;
    ob.spawnDelay = delay;
    ob.active     = (delay <= 0.0f);
    ob.isPowerUp  = true;
    ob.isRectangle = true;
    ob.halfWidth  = POWERUP_BASE_SIZE * 1.5f;
    ob.halfHeight = POWERUP_BASE_SIZE * 2.0f;
    ob.r          = 0.0f;
    ob.g          = 1.0f;
    ob.b          = 1.0f;
    ob.angle      = 0.0f;
    ob.baseScale  = 1.0f;
    return ob;
}

bool checkCollision(float px, float py, const Obstacle& ob)
{
    return fabs(px - ob.x) < (playerHalfWidth + ob.halfWidth)
        && fabs(py - ob.y) < (playerHalfHeight + ob.halfHeight);
}

// ── main ──────────────────────────────────────────────────────────────────────
int main()
{
    srand((unsigned int)time(NULL));
    loadScores();
    if (!highScores.empty())
        previousHighScore = (float)highScores[0];

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
                                          "Dodge Me If You Can", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    // Shaders
    auto mkShader = [](GLenum type, const char* src) {
        unsigned int s = glCreateShader(type);
        glShaderSource(s, 1, &src, NULL);
        glCompileShader(s);
        return s;
    };
    unsigned int vs = mkShader(GL_VERTEX_SHADER,   vertexShaderSource);
    unsigned int fs = mkShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs); glDeleteShader(fs);

    int offsetLoc = glGetUniformLocation(prog, "offset");
    int scaleLoc  = glGetUniformLocation(prog, "scale");
    int angleLoc  = glGetUniformLocation(prog, "angle");
    int colorLoc  = glGetUniformLocation(prog, "ourColor");

    // Player VAO
    float playerVerts[] = {
        -playerHalfWidth, -playerHalfHeight,
         playerHalfWidth, -playerHalfHeight,
         0.0f,             playerHalfHeight
    };
    unsigned int triVAO, triVBO;
    glGenVertexArrays(1,&triVAO); glGenBuffers(1,&triVBO);
    glBindVertexArray(triVAO);
    glBindBuffer(GL_ARRAY_BUFFER, triVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(playerVerts), playerVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);

    // Obstacle VAO
    float sqVerts[] = {
        -obstacleHalfSize,-obstacleHalfSize,
         obstacleHalfSize,-obstacleHalfSize,
         obstacleHalfSize, obstacleHalfSize,
         obstacleHalfSize, obstacleHalfSize,
        -obstacleHalfSize, obstacleHalfSize,
        -obstacleHalfSize,-obstacleHalfSize
    };
    unsigned int sqVAO, sqVBO;
    glGenVertexArrays(1,&sqVAO); glGenBuffers(1,&sqVBO);
    glBindVertexArray(sqVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sqVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(sqVerts), sqVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);

    // Dynamic text VAO
    float dummy[12] = {};
    unsigned int textVAO, textVBO;
    glGenVertexArrays(1,&textVAO); glGenBuffers(1,&textVBO);
    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(dummy), dummy, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);

    // Spawn obstacles
    obstacles.reserve(MAX_OBSTACLES);
    for (int i = 0; i < INITIAL_OBSTACLES; ++i)
        obstacles.push_back(createObstacle(i * (0.6f + (rand() % 80) / 100.0f)));
    nextObstacleSpawn = 10.0f + (rand() % 11);
    obstacleSpawnInterval = 10 + (rand() % 11);
    nextPowerUpSpawn = 18.0f + (rand() % 11);

    // Build text quads
    // ── "GAME OVER" centred horizontally, just below screen centre
    const float PIX     = 0.026f;
    const float CHAR_W  = 6.0f * PIX;  // 5 cols + 1 gap

    float goW      = 9 * CHAR_W;
    float goStartX = -goW / 2.0f;
    float goStartY = 0.75f;            // top of GAME OVER text near the screen top

    float ywWinW   = 7 * CHAR_W;
    float ywLoseW  = 8 * CHAR_W;
    float ywWinX   = -ywWinW  / 2.0f;
    float ywLoseX  = -ywLoseW / 2.0f;
    float gap      = 8.0f * PIX;        // gap between lines
    float ywStartY = goStartY - 7.0f * PIX - gap; // top of YOU WIN / YOU LOSE below GAME OVER

    std::vector<PixelQuad> gameOverQuads, youWinQuads, youLoseQuads;
    std::vector<PixelQuad> restartQuads;
    prepareGameOverText(gameOverQuads, youWinQuads, youLoseQuads, 0.026f);

    const float INFO_PIX    = 0.015f;
    const float INFO_CHARW  = 6.0f * INFO_PIX;
    const char* restartText = "PRESS R TO PLAY AGAIN";
    const int   restartLen  = 22;
    buildText(restartText, -INFO_CHARW * restartLen / 2.0f + 0.05f, 0.0f, INFO_PIX, restartQuads);

    double startTime = glfwGetTime();
    double lastTime  = startTime;
    int    finalTime = 0;

    // ── Game loop ─────────────────────────────────────────────────────────────
    while (!glfwWindowShouldClose(window))
    {
        double now       = glfwGetTime();
        float  dt        = (float)(now - lastTime);
        lastTime = now;
        float  elapsed   = (float)(now - startTime);

        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS
            || glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS
            || glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, true);
        }

        if (gameOver && glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
        {
            gameOver         = false;
            beatHighScore    = false;
            surpassedHighScore = false;
            playerX          = 0.0f;
            playerY          = PLAYER_Y_HOME;
            playerSpeed      = 1.2f;
            dashState        = DashState::IDLE;
            dashCharge       = 0;
            dashReady        = false;
            dashKeyHeld      = false;
            speedMultiplier  = 1.0f;
            nextSpeedIncrease = 20.0f;
            nextObstacleSpawn = 10.0f + (rand() % 11);
            obstacleSpawnInterval = 10 + (rand() % 11);
            nextPowerUpSpawn = 18.0f + (rand() % 11);
            powerUpActive = false;
            obstacles.clear();
            for (int i = 0; i < INITIAL_OBSTACLES; ++i)
                obstacles.push_back(createObstacle(i * (0.6f + (rand() % 80) / 100.0f)));
            startTime = now;
            lastTime  = now;
        }

        if (!gameOver)
        {
            // Player movement speed grows over time
            playerSpeed = 1.2f + std::min(elapsed / 10.0f, 14.0f) * 0.1f;

            // ── Left / Right ──────────────────────────────────────────────────
            if (glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS) playerX -= playerSpeed * dt;
            if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) playerX += playerSpeed * dt;
            playerX = std::max(playerX, -1.0f + playerHalfWidth);
            playerX = std::min(playerX,  1.0f - playerHalfWidth);

            // ── Dash (UP key) — only fires when charge is full ────────────────
            bool upPressed = (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS);

            if (dashState == DashState::IDLE)
            {
                if (upPressed && !dashKeyHeld && dashReady)
                {
                    // Spend the charge
                    dashCharge = 0;
                    dashReady  = false;
                    dashState  = DashState::LUNGING;
                    float dashDistance = surpassedHighScore ? DASH_DIST * 1.2f : DASH_DIST;
                    dashTargetY = PLAYER_Y_HOME + dashDistance;
                }
                dashKeyHeld = upPressed;
            }
            else if (dashState == DashState::LUNGING)
            {
                playerY += DASH_SPEED * dt;
                if (playerY >= dashTargetY)
                {
                    playerY   = dashTargetY;
                    dashState = DashState::RETURNING;
                }
            }
            else if (dashState == DashState::RETURNING)
            {
                float returnSpeed = surpassedHighScore ? RETURN_SPEED * 0.75f : RETURN_SPEED;
                playerY -= returnSpeed * dt;
                if (playerY <= PLAYER_Y_HOME)
                {
                    playerY   = PLAYER_Y_HOME;
                    dashState = DashState::IDLE;
                    dashKeyHeld = upPressed;
                }
            }

            if (!surpassedHighScore && previousHighScore > 0.0f && elapsed > previousHighScore) {
                surpassedHighScore = true;
            }

            // Obstacle speed ramp
            if (elapsed >= nextSpeedIncrease) {
                nextSpeedIncrease += 20.0f;
                speedMultiplier   += 0.25f;
                std::cout << "Speed up! x" << speedMultiplier << "\n";
            }

            // New obstacle spawn interval varies randomly
            if (elapsed >= nextObstacleSpawn && (int)obstacles.size() < MAX_OBSTACLES) {
                obstacles.push_back(createObstacle(0.0f));
                obstacleSpawnInterval = 10 + (rand() % 11);
                nextObstacleSpawn += obstacleSpawnInterval;
                std::cout << "Obstacles: " << obstacles.size() << "\n";
            }

            if (!powerUpActive && elapsed >= nextPowerUpSpawn) {
                powerUp      = createPowerUp(0.0f);
                powerUpActive = true;
                std::cout << "Power-up incoming!\n";
            }

            // Move & collide
            for (auto& ob : obstacles) {
                if (!ob.active) { ob.spawnDelay -= dt; if (ob.spawnDelay <= 0) ob.active = true; continue; }
                float prevY = ob.y;
                ob.y -= ob.speed * speedMultiplier * dt;

                // Charge dash: count obstacle as dodged when it passes below player
                if (!dashReady && prevY >= playerY && ob.y < playerY)
                {
                    // Only charge if obstacle was near player horizontally (close call)
                    if (fabs(ob.x - playerX) < 0.55f)
                    {
                        dashCharge = std::min(dashCharge + 1, DASH_CHARGE_MAX);
                        if (dashCharge >= DASH_CHARGE_MAX) dashReady = true;
                    }
                }

                if (ob.y < -1.1f) resetObstacle(ob);
                if (checkCollision(playerX, playerY, ob)) {
                    gameOver     = true;
                    finalTime    = (int)elapsed;
                    beatHighScore = submitScore(finalTime);
                    if (beatHighScore) {
                        std::cout << "Game Over! You win! " << finalTime << " s\n";
                    } else {
                        std::cout << "Game Over! You lose! " << finalTime << " s\n";
                    }
                }
            }

            if (powerUpActive) {
                if (!powerUp.active) {
                    powerUp.spawnDelay -= dt;
                    if (powerUp.spawnDelay <= 0) powerUp.active = true;
                }
                if (powerUp.active) {
                    powerUp.y += -powerUp.speed * speedMultiplier * dt;
                    powerUp.angle  += POWERUP_ROT_SPEED * dt;
                    powerUp.baseScale += POWERUP_SCALE_SPEED * dt;
                    if (powerUp.baseScale > 2.0f) powerUp.baseScale = 2.0f;
                    float t = fmod((float)now / 3.0f, 1.0f);
                    powerUp.r = 0.6f * t;
                    powerUp.g = 1.0f - 1.0f * t;
                    powerUp.b = 1.0f;

                    if (powerUp.y < -1.1f) {
                        powerUpActive = false;
                        nextPowerUpSpawn = elapsed + 16.0f + (rand() % 12);
                    } else if (checkCollision(playerX, playerY, powerUp)) {
                        dashCharge = DASH_CHARGE_MAX;
                        dashReady  = true;
                        powerUpActive = false;
                        nextPowerUpSpawn = elapsed + 16.0f + (rand() % 12);
                    }
                }
            }
        }

        // ── Draw ────────────────────────────────────────────────────────────
        glClearColor(0.05f,0.05f,0.08f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(prog);

        // Player
        glUniform2f(scaleLoc, 1.0f, 1.0f);
        glUniform1f(angleLoc, 0.0f);
        glUniform2f(offsetLoc, playerX, playerY);
        glUniform4f(colorLoc, 1.0f, 0.0f, 0.0f, 1.0f);
        glBindVertexArray(triVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // Obstacles
        glBindVertexArray(sqVAO);
        for (auto& ob : obstacles) {
            if (!ob.active) continue;
            glUniform2f(scaleLoc, ob.halfWidth / obstacleHalfSize, ob.halfHeight / obstacleHalfSize);
            glUniform1f(angleLoc, 0.0f);
            glUniform2f(offsetLoc, ob.x, ob.y);
            glUniform4f(colorLoc, ob.r, ob.g, ob.b, 1.0f);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        if (powerUpActive && powerUp.active) {
            glUniform2f(scaleLoc,
                        powerUp.baseScale * powerUp.halfWidth / obstacleHalfSize,
                        powerUp.baseScale * powerUp.halfHeight / obstacleHalfSize);
            glUniform1f(angleLoc, powerUp.angle);
            glUniform2f(offsetLoc, powerUp.x, powerUp.y);
            glUniform4f(colorLoc, powerUp.r, powerUp.g, powerUp.b, 1.0f);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        // ── Dash charge bar (bottom-centre) ──────────────────────────────────
        // Shows 5 segments; filled segments = dashCharge
        {
            glBindVertexArray(textVAO);
            float segW   = 0.06f;
            float segH   = 0.025f;
            float gap2   = 0.012f;
            float totalW = DASH_CHARGE_MAX * segW + (DASH_CHARGE_MAX - 1) * gap2;
            float startX2 = -totalW / 2.0f;
            float barY2  = -0.88f;

            for (int i = 0; i < DASH_CHARGE_MAX; ++i)
            {
                float sx = startX2 + i * (segW + gap2);
                bool filled = (i < dashCharge);
                // Pulse gold when fully charged, else dim blue/bright blue
                float cr, cg, cb;
                if (dashReady) {
                    float pulse = 0.6f + 0.4f * sinf((float)glfwGetTime() * 6.0f);
                    cr = pulse; cg = pulse * 0.85f; cb = 0.0f;
                } else {
                    cr = filled ? 0.2f : 0.08f;
                    cg = filled ? 0.6f : 0.12f;
                    cb = filled ? 1.0f : 0.25f;
                }
                float sv[] = {
                    sx,      barY2,
                    sx+segW, barY2,
                    sx+segW, barY2+segH,
                    sx+segW, barY2+segH,
                    sx,      barY2+segH,
                    sx,      barY2
                };
                glBindBuffer(GL_ARRAY_BUFFER, textVBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(sv), sv);
                glUniform2f(scaleLoc, 1.0f, 1.0f);
                glUniform1f(angleLoc, 0.0f);
                glUniform2f(offsetLoc, 0.0f, 0.0f);
                glUniform4f(colorLoc, cr, cg, cb, 1.0f);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
        }

        // Overlay text
        if (gameOver) {
            glBindVertexArray(textVAO);
            glUniform2f(scaleLoc, 1.0f, 1.0f);
            glUniform1f(angleLoc, 0.0f);
            for (auto& q : gameOverQuads)
                drawPixelQuad(textVBO, offsetLoc, colorLoc, q, 1.0f, 0.15f, 0.15f);
            if (beatHighScore) {
                for (auto& q : youWinQuads)
                    drawPixelQuad(textVBO, offsetLoc, colorLoc, q, 0.2f, 1.0f, 0.4f);
            } else {
                for (auto& q : youLoseQuads)
                    drawPixelQuad(textVBO, offsetLoc, colorLoc, q, 1.0f, 0.4f, 0.2f);
            }
            for (auto& q : restartQuads)
                drawPixelQuad(textVBO, offsetLoc, colorLoc, q, 0.4f, 0.8f, 1.0f);
        }

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}