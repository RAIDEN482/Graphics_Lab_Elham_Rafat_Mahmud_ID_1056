#include "TextRenderer.h"
#include "glad.h"
#include <cstring>

// Generic uppercase 5x7 font for titles and menu strings.
static const unsigned char FONT_BASIC[26][5] = {
    {0x7E,0x11,0x11,0x11,0x7E}, // A
    {0x7F,0x49,0x49,0x49,0x36}, // B
    {0x3E,0x41,0x41,0x41,0x22}, // C
    {0x7F,0x41,0x41,0x22,0x1C}, // D
    {0x7F,0x49,0x49,0x49,0x41}, // E
    {0x7F,0x09,0x09,0x09,0x01}, // F
    {0x3E,0x41,0x49,0x49,0x3A}, // G
    {0x7F,0x08,0x08,0x08,0x7F}, // H
    {0x00,0x41,0x7F,0x41,0x00}, // I
    {0x20,0x40,0x41,0x3F,0x01}, // J
    {0x7F,0x08,0x14,0x22,0x41}, // K
    {0x7F,0x40,0x40,0x40,0x40}, // L
    {0x7F,0x04,0x18,0x04,0x7F}, // M
    {0x7F,0x04,0x08,0x10,0x7F}, // N
    {0x3E,0x41,0x41,0x41,0x3E}, // O
    {0x7F,0x09,0x09,0x09,0x06}, // P
    {0x3E,0x41,0x51,0x21,0x5E}, // Q
    {0x7F,0x09,0x19,0x29,0x46}, // R
    {0x46,0x49,0x49,0x49,0x31}, // S
    {0x01,0x01,0x7F,0x01,0x01}, // T
    {0x3F,0x40,0x40,0x40,0x3F}, // U
    {0x1F,0x20,0x40,0x20,0x1F}, // V
    {0x7F,0x20,0x18,0x20,0x7F}, // W
    {0x63,0x14,0x08,0x14,0x63}, // X
    {0x07,0x08,0x70,0x08,0x07}, // Y
    {0x61,0x51,0x49,0x45,0x43}, // Z
};

static const unsigned char FONT_SPACE[5] = { 0x00, 0x00, 0x00, 0x00, 0x00 };

static const unsigned char* getGlyphForChar(char c)
{
    if (c == ' ') return FONT_SPACE;
    if (c >= 'A' && c <= 'Z') return FONT_BASIC[c - 'A'];
    if (c >= 'a' && c <= 'z') return FONT_BASIC[c - 'a'];
    return FONT_SPACE;
}

// Font arrays (moved here)
static const unsigned char FONT_GAMEOVER[][5] = {
    {0x3E,0x41,0x49,0x49,0x3A}, // G
    {0x7E,0x11,0x11,0x11,0x7E}, // A
    {0x7F,0x02,0x0C,0x02,0x7F}, // M
    {0x7F,0x49,0x49,0x49,0x41}, // E
    {0x00,0x00,0x00,0x00,0x00}, // (space)
    {0x3E,0x41,0x41,0x41,0x3E}, // O
    {0x1F,0x30,0x40,0x30,0x1F}, // V
    {0x7F,0x49,0x49,0x49,0x41}, // E
    {0x7F,0x09,0x19,0x29,0x46}, // R
};



static const unsigned char FONT_YOUWIN[][5] = {
    {0x07,0x08,0x70,0x08,0x07}, // Y
    {0x3E,0x41,0x41,0x41,0x3E}, // O
    {0x3F,0x40,0x40,0x40,0x3F}, // U
    {0x00,0x00,0x00,0x00,0x00}, // (space)
    {0x7F,0x20,0x18,0x20,0x7F}, // W
    {0x00,0x41,0x7F,0x41,0x00}, // I  
    {0x7F,0x04,0x08,0x10,0x7F}, // N
};

static const unsigned char FONT_YOULOSE[][5] = {
    {0x07,0x08,0x70,0x08,0x07}, // Y
    {0x3E,0x41,0x41,0x41,0x3E}, // O
    {0x3F,0x40,0x40,0x40,0x3F}, // U
    {0x00,0x00,0x00,0x00,0x00}, // (space)
    {0x7F,0x40,0x40,0x40,0x40}, // L
    {0x3E,0x41,0x41,0x41,0x3E}, // O
    {0x46,0x49,0x49,0x49,0x31}, // S
    {0x7F,0x49,0x49,0x49,0x41}, // E
};

void buildText(const unsigned char glyphs[][5], int numChars,
               float startX, float startY, float scale,
               std::vector<PixelQuad>& quads)
{
    quads.clear();
    float cx = startX;
    for (int c = 0; c < numChars; ++c) {
        for (int col = 0; col < 5; ++col) {
            unsigned char bits = glyphs[c][col];
            for (int row = 0; row < 7; ++row) {
                // Correct: bit6 = top, bit0 = bottom
                if (bits & (1 << (6 - row))) {
                    PixelQuad q;
                    q.x = cx + col * scale;
                    q.y = startY + row * scale;  // top-down text: startY is the top of the glyph
                    q.w = scale;
                    q.h = scale;
                    quads.push_back(q);
                }
            }
        }
        cx += 6.0f * scale;
    }
}
void drawPixelQuad(unsigned int textVBO,
                   int offsetLoc, int colorLoc,
                   const PixelQuad& q,
                   float r, float g, float b)
{
    float x0 = q.x,        y1 = q.y;
    float x1 = q.x + q.w,  y0 = q.y - q.h;
    float v[] = {
        x0,y0,  x1,y0,  x1,y1,
        x1,y1,  x0,y1,  x0,y0,
    };
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
    glUniform2f(offsetLoc, 0.0f, 0.0f);
    glUniform4f(colorLoc, r, g, b, 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void prepareGameOverText(std::vector<PixelQuad>& gameOverQuads,
                         std::vector<PixelQuad>& youWinQuads,
                         std::vector<PixelQuad>& youLoseQuads,
                         float PIX)
{
    const float CHAR_W  = 6.0f * PIX;
    float goW      = 9 * CHAR_W;
    float goStartX = -goW / 2.0f;
    float goStartY = 0.75f;

    float ywWinW   = 7 * CHAR_W;
    float ywLoseW  = 8 * CHAR_W;
    float ywWinX   = -ywWinW  / 2.0f;
    float ywLoseX  = -ywLoseW / 2.0f;
    float gap      = 8.0f * PIX;
    float ywStartY = goStartY - 7.0f * PIX - gap;

    buildText(FONT_GAMEOVER, 9, goStartX, goStartY, PIX, gameOverQuads);
    buildText(FONT_YOUWIN,  7, ywWinX,  ywStartY, PIX, youWinQuads);
    buildText(FONT_YOULOSE, 8, ywLoseX, ywStartY, PIX, youLoseQuads);
}

void buildText(const char* text,
               float startX, float startY, float scale,
               std::vector<PixelQuad>& quads)
{
    quads.clear();
    float cx = startX;
    int len = (int)std::strlen(text);
    for (int i = 0; i < len; ++i) {
        const unsigned char* glyph = getGlyphForChar(text[i]);
        for (int col = 0; col < 5; ++col) {
            unsigned char bits = glyph[col];
            for (int row = 0; row < 7; ++row) {
                if (bits & (1 << (6 - row))) {
                    PixelQuad q;
                    q.x = cx + col * scale;
                    q.y = startY + row * scale;
                    q.w = scale;
                    q.h = scale;
                    quads.push_back(q);
                }
            }
        }
        cx += 6.0f * scale;
    }
}
