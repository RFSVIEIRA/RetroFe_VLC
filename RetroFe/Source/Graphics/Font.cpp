#include "Font.h"
#include "../SDL.h"
#include "../Utility/Log.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>
#include <cstdlib>

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

bool Font::initialize(SDL_Renderer* renderer) 
{
    TTF_Font* font = TTF_OpenFont(fontPath_.c_str(), fontSize_);
    if (!font) {
        // Fallback font resolution and auto-creation from system fonts
        try {
            std::filesystem::path fp(fontPath_);
            std::string filename = fp.filename().string();

            const char* win = getenv("WINDIR");
            if (!win) win = getenv("SystemRoot");
            std::string winFonts = std::string(win ? win : "C:\\Windows") + "\\Fonts\\";

            std::vector<std::string> candidates;
            if (!filename.empty()) {
                candidates.push_back(winFonts + filename);
            }
            candidates.push_back(winFonts + "Arial.ttf");
            candidates.push_back(winFonts + "arial.ttf");
            candidates.push_back(winFonts + "segoeui.ttf");
            candidates.push_back(winFonts + "calibri.ttf");
            candidates.push_back(winFonts + "tahoma.ttf");
            candidates.push_back(winFonts + "verdana.ttf");
            candidates.push_back("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
            candidates.push_back("/usr/share/fonts/TTF/DejaVuSans.ttf");
            candidates.push_back("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf");
            candidates.push_back("/System/Library/Fonts/Supplemental/Arial.ttf");

            std::string fallbackPath;
            for (const auto& cand : candidates) {
                std::error_code ec;
                if (std::filesystem::exists(cand, ec)) {
                    fallbackPath = cand;
                    break;
                }
            }

            if (!fallbackPath.empty()) {
                Logger::write(Logger::ZONE_INFO, "Font", "Using fallback font from system: " + fallbackPath);
                font = TTF_OpenFont(fallbackPath.c_str(), fontSize_);
            }
        } catch (...) {
        }
    }

    if (!font) {
        Logger::write(Logger::ZONE_ERROR, "Font", std::string("Could not open font: ") + TTF_GetError());
        return false;
    }

    height = TTF_FontHeight(font);
    ascent = TTF_FontAscent(font);

    int x = 0;
    int y = 0;
    int atlasWidth = 0;
    int atlasHeight = 0;

    auto loadRange = [&](Uint32 start, Uint32 end)
    {
        for (Uint32 cp = start; cp <= end; ++cp)
        {
            if (!TTF_GlyphIsProvided32(font, cp)) continue;

            GlyphInfoBuild* info = new GlyphInfoBuild{};
            info->surface = TTF_RenderGlyph32_Blended(font, cp, color_);

            if (!info->surface) {
                Logger::write(Logger::ZONE_WARNING, "Font", "Render failed for code " + std::to_string(cp));
                delete info;
                continue;
            }

            TTF_GlyphMetrics32(
                font, cp,
                &info->glyph.minX, &info->glyph.maxX,
                &info->glyph.minY, &info->glyph.maxY,
                &info->glyph.advance
            );

            // New row?
            if (x + info->surface->w >= 1024) {
                atlasHeight += y;
                atlasWidth = std::max(atlasWidth, x);
                x = 0;
                y = 0;
            }

            info->glyph.rect.x = x;
            info->glyph.rect.y = atlasHeight;
            info->glyph.rect.w = info->surface->w;
            info->glyph.rect.h = info->surface->h;

            atlas[cp] = info;

            x += info->surface->w;
            y = std::max(y, info->surface->h);
        }
    };

    // Load ASCII
    loadRange(0x0020, 0x007F);

    // Load Latin-1 Supplement ( is here!)
    loadRange(0x00A0, 0x00FF);

    atlasWidth = std::max(atlasWidth, x);
    atlasHeight += y;

    // Create atlas surface
    SDL_Surface* atlasSurface = SDL_CreateRGBSurfaceWithFormat(0, atlasWidth, atlasHeight, 32, SDL_PIXELFORMAT_RGBA32);

    for (auto& kv : atlas)
    {
        GlyphInfoBuild* info = kv.second;
        SDL_BlitSurface(info->surface, nullptr, atlasSurface, &info->glyph.rect);
        SDL_FreeSurface(info->surface);
        info->surface = nullptr;
    }

    SDL_Renderer* targetRenderer = renderer ? renderer : SDL::getRenderer(monitor_);
    if (!targetRenderer) {
        targetRenderer = SDL::getRenderer(0);
    }
    if (!targetRenderer) {
        for (int i = 0; i < SDL::getNumScreens(); ++i) {
            targetRenderer = SDL::getRenderer(i);
            if (targetRenderer) break;
        }
    }
    if (!targetRenderer) {
        Logger::write(Logger::ZONE_ERROR, "Font", "No valid renderer");
        SDL_FreeSurface(atlasSurface);
        TTF_CloseFont(font);
        return false;
    }

    texture = SDL_CreateTextureFromSurface(targetRenderer, atlasSurface);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_FreeSurface(atlasSurface);

    TTF_CloseFont(font);

    return texture != nullptr;
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