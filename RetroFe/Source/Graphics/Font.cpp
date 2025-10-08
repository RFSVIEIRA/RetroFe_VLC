#include "Font.h"
#include "../SDL.h"
#include "../Utility/Log.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <cstdio>
#include <cstring>

Font::Font(std::string fontPath, int fontSize, SDL_Color color, int monitor)
    : texture(NULL), fontPath_(fontPath), fontSize_(fontSize), color_(color), monitor_(monitor) {
}

Font::~Font() {
    deInitialize();
}

SDL_Texture* Font::getTexture() {
    return texture;
}

int Font::getHeight() {
    return height;
}

int Font::getAscent() {
    return ascent;
}

bool Font::getRect(unsigned int charCode, GlyphInfo& glyph) {
    std::map<unsigned int, GlyphInfoBuild*>::iterator it = atlas.find(charCode);
    if (it != atlas.end()) {
        GlyphInfoBuild* info = it->second;
        glyph = info->glyph;
        return true;
    }
    return false;
}

bool Font::initialize(SDL_Renderer* renderer) {
    TTF_Font* font = TTF_OpenFont(fontPath_.c_str(), fontSize_);
    if (!font) {
        std::stringstream ss;
        ss << "Could not open font: " << TTF_GetError();
        Logger::write(Logger::ZONE_ERROR, "Font", ss.str());
        return false;
    }

    int x = 0;
    int y = 0;
    int atlasHeight = 0;
    int atlasWidth = 0;
    height = TTF_FontHeight(font);
    ascent = TTF_FontAscent(font);

    for (unsigned short int i = 32; i < 128; ++i) {
        GlyphInfoBuild* info = new GlyphInfoBuild;
        memset(info, 0, sizeof(GlyphInfoBuild));

        color_.a = 255;
        info->surface = TTF_RenderGlyph_Blended(font, i, color_);
        TTF_GlyphMetrics(font, i,
            &info->glyph.minX, &info->glyph.maxX,
            &info->glyph.minY, &info->glyph.maxY,
            &info->glyph.advance);

        if (x + info->surface->w >= 1024) {
            atlasHeight += y;
            atlasWidth = (atlasWidth >= x) ? atlasWidth : x;
            x = 0;
            y = 0;
        }

        info->glyph.rect.w = info->surface->w;
        info->glyph.rect.h = info->surface->h;
        info->glyph.rect.x = x;
        info->glyph.rect.y = atlasHeight;
        atlas[i] = info;

        x += info->glyph.rect.w;
        y = (y > info->glyph.rect.h) ? y : info->glyph.rect.h;
    }

    atlasWidth = (atlasWidth >= x) ? atlasWidth : x;
    atlasHeight += y;

    unsigned int rmask, gmask, bmask, amask;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    rmask = 0xff000000;
    gmask = 0x00ff0000;
    bmask = 0x0000ff00;
    amask = 0x000000ff;
#else
    rmask = 0x000000ff;
    gmask = 0x0000ff00;
    bmask = 0x00ff0000;
    amask = 0xff000000;
#endif

    SDL_Surface* atlasSurface = SDL_CreateRGBSurface(0, atlasWidth, atlasHeight, 32, rmask, gmask, bmask, amask);
    std::map<unsigned int, GlyphInfoBuild*>::iterator it;
    for (it = atlas.begin(); it != atlas.end(); it++) {
        GlyphInfoBuild* info = it->second;
        SDL_BlitSurface(info->surface, NULL, atlasSurface, &info->glyph.rect);
        SDL_FreeSurface(info->surface);
        info->surface = NULL;
    }

    SDL_LockMutex(SDL::getMutex());
    SDL_Renderer* targetRenderer = renderer ? renderer : SDL::getRenderer(monitor_);
    if (!targetRenderer) {
        Logger::write(Logger::ZONE_ERROR, "Font", "No valid renderer for texture creation");
        TTF_CloseFont(font);
        SDL_FreeSurface(atlasSurface);
        return false;
    }
    texture = SDL_CreateTextureFromSurface(targetRenderer, atlasSurface);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_FreeSurface(atlasSurface);
    SDL_UnlockMutex(SDL::getMutex());

    TTF_CloseFont(font);

    if (!texture) {
        Logger::write(Logger::ZONE_ERROR, "Font", "SDL_CreateTextureFromSurface failed: " + std::string(SDL_GetError()));
        return false;
    }

    return true;
}

void Font::deInitialize() {
    if (texture) {
        SDL_LockMutex(SDL::getMutex());
        SDL_DestroyTexture(texture);
        texture = NULL;
        SDL_UnlockMutex(SDL::getMutex());
    }

    std::map<unsigned int, GlyphInfoBuild*>::iterator atlasIt = atlas.begin();
    while (atlasIt != atlas.end()) {
        delete atlasIt->second;
        atlas.erase(atlasIt);
        atlasIt = atlas.begin();
    }
}