#include "FontCache.h"
#include "Font.h"
#include "../Utility/Log.h"
#include "../SDL.h"
#include <SDL_ttf.h>
#include <sstream>

FontCache::FontCache() : defaultRenderer_(nullptr) {
}

FontCache::~FontCache() {
    deInitialize();
}

void FontCache::initialize() {
    if (TTF_Init() < 0) {
        Logger::write(Logger::ZONE_ERROR, "FontCache", "TTF_Init failed: " + std::string(TTF_GetError()));
    }
    defaultRenderer_ = SDL::getRenderer(0);
    if (!defaultRenderer_) {
        Logger::write(Logger::ZONE_WARNING, "FontCache", "No default renderer available");
    }
}

void FontCache::deInitialize() {
    std::map<std::string, Font*>::iterator it = fontFaceMap_.begin();
    while (it != fontFaceMap_.end()) {
        delete it->second;
        fontFaceMap_.erase(it);
        it = fontFaceMap_.begin();
    }

    SDL_LockMutex(SDL::getMutex());
    TTF_Quit();
    SDL_UnlockMutex(SDL::getMutex());
}

Font* FontCache::getFont(std::string fontPath, int fontSize, SDL_Color color)
{
    Font* t = NULL;

    std::map<std::string, Font*>::iterator it = fontFaceMap_.find(buildFontKey(fontPath, fontSize, color));

    if (it != fontFaceMap_.end())
    {
        t = it->second;
    }

    return t;
}


bool FontCache::loadFont(std::string fontPath, int fontSize, SDL_Color color, int monitor)
{
    std::string key = buildFontKey(fontPath, fontSize, color);
    std::map<std::string, Font*>::iterator it = fontFaceMap_.find(key);

    if (it == fontFaceMap_.end())
    {
        Font* f = new Font(fontPath, fontSize, color, monitor);
        f->initialize();
        fontFaceMap_[key] = f;
    }

    return true;
}

std::string FontCache::buildFontKey(std::string font, int fontSize, SDL_Color color) {
    std::stringstream ss;
    ss << font << "_SIZE=" << fontSize << "_RGB=" << (int)color.r << "." << (int)color.g << "." << (int)color.b;
    return ss.str();
}