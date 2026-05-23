#ifndef TEXT_RENDERER_H
#define TEXT_RENDERER_H

#include <vector>

// Forward declaration
struct PixelQuad {
    float x, y, w, h;
};

// Functions
void buildText(const unsigned char glyphs[][5], int numChars,
               float startX, float startY, float scale,
               std::vector<PixelQuad>& quads);

void buildText(const char* text,
               float startX, float startY, float scale,
               std::vector<PixelQuad>& quads);

void drawPixelQuad(unsigned int textVBO,
                   int offsetLoc, int colorLoc,
                   const PixelQuad& q,
                   float r, float g, float b);

void prepareGameOverText(std::vector<PixelQuad>& gameOverQuads,
                         std::vector<PixelQuad>& youWinQuads,
                         std::vector<PixelQuad>& youLoseQuads,
                         float PIX);

#endif
