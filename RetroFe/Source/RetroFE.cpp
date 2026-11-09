/* This file is part of RetroFE.
 *
 * RetroFE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * RetroFE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with RetroFE.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "RetroFE.h"
#include "Collection/CollectionInfoBuilder.h"
#include "Collection/CollectionInfo.h"
#include "Database/Configuration.h"
#include "Database/MetadataDatabase.h"
#include "Collection/Item.h"
#include "Execute/Launcher.h"
#include "Menu/Menu.h"
#include "Utility/Log.h"
#include "Utility/Utils.h"
#include "Collection/MenuParser.h"
#include "SDL.h"
#include "Control/UserInput.h"
#include "Graphics/PageBuilder.h"
#include "Graphics/Page.h"
#include "Graphics/Component/ScrollingList.h"
#include "Graphics/Component/Image.h"
#include "Graphics/Component/Video.h"
#include <vlc/vlc.h>
#include <SDL_image.h>
#include "Graphics/Component/JxlLoader.h" 
#include "Video/VideoFactory.h"
#include <algorithm>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>
#include <set>
#include <unordered_set>
#include <SDL_ttf.h>

#if defined(__linux) || defined(__APPLE__)
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <cstring>
#endif

#ifdef WIN32
#include <Windows.h>
#include <SDL_syswm.h>
#include <SDL_thread.h>
#include <iostream>
#endif
#include <numeric>
#include <functional>
RetroFE::RetroFE(Configuration& c)
    : initialized(false)
    , initializeError(false)
    , initializeThread(NULL)
    , config_(c)
    , db_(NULL)
    , metadb_(NULL)
    , input_(config_)
    , currentPage_(NULL)
    , keyInputDisable_(0)
    , currentTime_(0)
    , lastLaunchReturnTime_(0)
    , keyLastTime_(0)
    , keyDelayTime_(.3f)
    , reboot_(false)
    , layoutFilePath_()  
    , lastLayoutModTime_(0)
    , layoutCheckTimer_(0.0f)
    , layoutCheckInterval_(1.0f) // Check every 1 second
    , enterCollectionAfterReload_(false)
	, enterNumberOfTimes_(0)
    , isInCriticalState_(false)
    , launchStartTime_(0.0f)
    , preSearchMaxModTime_(0)
	, pendingBackToMain_(false)
	, jumpNextBack_(false)
    , screenDuplicatorHandles()
	, isReload_(false)
    , isReloading_(false)
    , loadingFont_(nullptr)
    , loadingFontSize_(0)
    , loadingTextTexture_(nullptr)
    , loadingTextW_(0)
    , loadingTextH_(0)
    , cachedLoadingTextColor_({ 0, 0, 0, 0 })
    , isAutoNavigating_(false)
    , isJukebox_(false)
    , cib_(NULL)
{
    Image::config_ = &config_;
    menuMode_ = false;
    attractMode_ = false;
    attractModePlaylistCollectionNumber_ = 0;
    firstPlaylist_ = "all";

	// dynamic layout updating
    std::string layoutName;
    config_.getProperty("layout", layoutName);
    layoutFilePath_ = Utils::combinePath(Configuration::absolutePath, "layouts", layoutName, "layout.xml");
    reloadSavedScroll_ = static_cast<unsigned int>(-1);
    searchPage_        = NULL;
    searchLaunchItem_  = NULL;
    searchFadeAlpha_   = 0;
    searchTargetItem_  = NULL;
    searchPendingItem_ = NULL;
    searchJumpIndex_   = -1;
    preSearchMenuDepth_ = 0;
    searchPageCollection_ = NULL;
    pendingSearchAfterGameExit_ = false;
    searchIndexBuilt_ = false;
    suppressNextBack_ = false;
}


RetroFE::~RetroFE()
{
    deInitialize();
    for (std::map<std::string, CollectionInfo*>::iterator it = searchCollections_.begin(); it != searchCollections_.end(); ++it)
    {
        if (it->second) delete it->second;
    }
    searchCollections_.clear();
    if (searchPage_)
    {
        searchPage_->playlists.clear();
        searchPage_->ownsItems = false;
        delete searchPage_;
        searchPage_ = NULL;
    }
    if (searchPageCollection_)
    {
        searchPageCollection_->playlists.clear();
        searchPageCollection_->ownsItems = false;
        delete searchPageCollection_;
        searchPageCollection_ = NULL;
    }
    if (searchTargetItem_)
    {
        delete searchTargetItem_;
        searchTargetItem_ = NULL;
    }
    searchPendingItem_ = NULL;
    freeLoadingResources();
}


// Render the current page to the screen
void RetroFE::render()
{

    SDL_LockMutex(SDL::getMutex());
    for (int i = 0; i < SDL::getNumDisplays(); ++i)
    {
        SDL_SetRenderDrawColor(SDL::getRenderer(i), 0x0, 0x0, 0x00, 0xFF);
        SDL_RenderClear(SDL::getRenderer(i));
    }

    if (currentPage_)
    {
        currentPage_->draw();
    }

    // Search overlay (pop-up keyboard + results) is drawn above the page
    if ( ( search_.isActive( ) || search_.hasStatus( ) ) && SDL::getNumDisplays( ) > 0 )
    {
        search_.draw( SDL::getRenderer( 0 ) );
    }

    // Options menu overlay
    if ( activeOptionsMenu_ && activeOptionsMenu_->isActive( ) && SDL::getNumDisplays( ) > 0 )
    {
        activeOptionsMenu_->draw( SDL::getRenderer( 0 ) );
    }

    // List Search launch: fade to black above all page and search graphics.
    if ( searchFadeAlpha_ > 0 )
    {
        for ( int i = 0; i < SDL::getNumDisplays( ); ++i )
        {
            SDL_Renderer *renderer = SDL::getRenderer( i );
            SDL_BlendMode previousBlend;
            SDL_GetRenderDrawBlendMode( renderer, &previousBlend );
            SDL_SetRenderDrawBlendMode( renderer, SDL_BLENDMODE_BLEND );
            SDL_SetRenderDrawColor( renderer, 0, 0, 0, static_cast<Uint8>( searchFadeAlpha_ ) );
            SDL_RenderFillRect( renderer, NULL );
            SDL_SetRenderDrawBlendMode( renderer, previousBlend );
        }
    }

    for (int i = 0; i < SDL::getNumDisplays(); ++i)
    {
        SDL_RenderPresent(SDL::getRenderer(i));
    }
    SDL_UnlockMutex(SDL::getMutex());

}

void RetroFE::checkAndReloadLayout()
{
    if (isInCriticalState_) return;
    time_t currentModTime = Utils::getFileModificationTime(layoutFilePath_);
    if (currentModTime == 0) {
        Logger::write(Logger::ZONE_ERROR, "RetroFeVLC", "Failed to get modification time for layout file: " + layoutFilePath_);
        return;
    }

    if (currentModTime > lastLayoutModTime_) {
        Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Layout file changed, reloading: " + layoutFilePath_);
        reloadLayout();
        lastLayoutModTime_ = currentModTime;
    }
}

#ifdef WIN32
// Function to enable shutdown privilege for ExitWindowsEx
bool RetroFE::EnableShutdownPrivilege() {
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        Logger::write(Logger::ZONE_ERROR, "RetroFeVLC", "Failed to open process token: " + std::to_string(GetLastError()));
        return false;
    }
    LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);
    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, 0)) {
        Logger::write(Logger::ZONE_ERROR, "RetroFeVLC", "Failed to adjust token privileges: " + std::to_string(GetLastError()));
        CloseHandle(hToken);
        return false;
    }
    CloseHandle(hToken);
    return true;
}
#endif

// Reload the layout by rebuilding the current page

SDL_Color RetroFE::parseHexColor(const std::string& hex, SDL_Color defaultColor)
{
    std::string s = hex;
    s.erase(0, s.find_first_not_of(" \t\r\n"));
    s.erase(s.find_last_not_of(" \t\r\n") + 1);

    if (s.empty()) return defaultColor;

    // Support decimal RGB format from Color.h ("255,255,255" or "255,255,255,255")
    if (s.find(',') != std::string::npos) {
        std::vector<std::string> parts = Utils::split(s, ',');
        try {
            SDL_Color c = defaultColor;
            if (parts.size() > 0) c.r = static_cast<Uint8>((std::min)(255, (std::max)(0, std::stoi(parts[0]))));
            if (parts.size() > 1) c.g = static_cast<Uint8>((std::min)(255, (std::max)(0, std::stoi(parts[1]))));
            if (parts.size() > 2) c.b = static_cast<Uint8>((std::min)(255, (std::max)(0, std::stoi(parts[2]))));
            if (parts.size() > 3) c.a = static_cast<Uint8>((std::min)(255, (std::max)(0, std::stoi(parts[3]))));
            else c.a = 255;
            return c;
        } catch (...) {
            return defaultColor;
        }
    }

    if (s[0] == '#') s = s.substr(1);
    else if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0) s = s.substr(2);

    if (s.length() == 6) {
        try {
            unsigned long val = std::stoul(s, nullptr, 16);
            SDL_Color c;
            c.r = static_cast<Uint8>((val >> 16) & 0xFF);
            c.g = static_cast<Uint8>((val >> 8) & 0xFF);
            c.b = static_cast<Uint8>(val & 0xFF);
            c.a = 255;
            return c;
        } catch (...) {}
    } else if (s.length() == 8) {
        try {
            unsigned long val = std::stoul(s, nullptr, 16);
            SDL_Color c;
            c.r = static_cast<Uint8>((val >> 24) & 0xFF);
            c.g = static_cast<Uint8>((val >> 16) & 0xFF);
            c.b = static_cast<Uint8>((val >> 8) & 0xFF);
            c.a = static_cast<Uint8>(val & 0xFF);
            return c;
        } catch (...) {}
    }
    return defaultColor;
}

SDL_Color RetroFE::hsvToRgb(float h, float s, float v, Uint8 alpha)
{
    float c = v * s;
    float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float r = 0, g = 0, b = 0;
    if (h < 60.0f) { r = c; g = x; b = 0; }
    else if (h < 120.0f) { r = x; g = c; b = 0; }
    else if (h < 180.0f) { r = 0; g = c; b = x; }
    else if (h < 240.0f) { r = 0; g = x; b = c; }
    else if (h < 300.0f) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }
    SDL_Color out;
    out.r = static_cast<Uint8>((r + m) * 255.0f);
    out.g = static_cast<Uint8>((g + m) * 255.0f);
    out.b = static_cast<Uint8>((b + m) * 255.0f);
    out.a = alpha;
    return out;
}

void RetroFE::drawLoadingCircle(SDL_Renderer* renderer, int cx, int cy, int radius, SDL_Color color)
{
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int dy = -radius; dy <= radius; ++dy) {
        int dx = static_cast<int>(std::sqrt(radius * radius - dy * dy));
        SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

static float resolveCoord(const std::string& str, float totalSize, float defaultVal)
{
    if (str.empty()) return defaultVal;
    std::string s = str;
    s.erase(0, s.find_first_not_of(" \t\r\n"));
    s.erase(s.find_last_not_of(" \t\r\n") + 1);
    if (s.empty()) return defaultVal;

    std::string lower = Utils::toLower(s);
    float base = 0.0f;
    size_t offsetPos = std::string::npos;
    if (lower.rfind("center", 0) == 0) {
        base = totalSize * 0.5f;
        offsetPos = 6;
    } else if (lower.rfind("left", 0) == 0) {
        base = 0.0f;
        offsetPos = 4;
    } else if (lower.rfind("right", 0) == 0) {
        base = totalSize;
        offsetPos = 5;
    } else if (lower.rfind("stretch", 0) == 0) {
        base = totalSize;
        offsetPos = 7;
    } else if (lower.rfind("top", 0) == 0) {
        base = 0.0f;
        offsetPos = 3;
    } else if (lower.rfind("bottom", 0) == 0) {
        base = totalSize;
        offsetPos = 6;
    } else {
        try {
            return std::stof(s);
        } catch (...) {
            return defaultVal;
        }
    }

    if (offsetPos != std::string::npos && offsetPos < s.size()) {
        std::string rem = s.substr(offsetPos);
        rem.erase(std::remove(rem.begin(), rem.end(), ' '), rem.end());
        if (!rem.empty()) {
            try {
                base += std::stof(rem);
            } catch (...) {}
        }
    }
    return base;
}

static float resolveOrigin(const std::string& originStr, const std::string& posStr, float defaultOrigin = -1.0f)
{
    if (!originStr.empty()) {
        std::string s = Utils::toLower(originStr);
        s.erase(0, s.find_first_not_of(" \t\r\n"));
        s.erase(s.find_last_not_of(" \t\r\n") + 1);
        if (s == "center") return 0.5f;
        if (s == "left" || s == "top") return 0.0f;
        if (s == "right" || s == "bottom") return 1.0f;
        try {
            float val = std::stof(s);
            if (val > 1.0f && val <= 100.0f) return val / 100.0f;
            return val;
        } catch (...) {}
    }

    if (defaultOrigin >= 0.0f) return defaultOrigin;

    std::string lowerPos = Utils::toLower(posStr);
    if (lowerPos.find("center") != std::string::npos) return 0.5f;
    if (lowerPos.find("right") != std::string::npos || lowerPos.find("bottom") != std::string::npos) return 1.0f;
    return 0.0f;
}

static std::string resolveLoadingPath(const std::string& layoutDir, const std::string& relativePath, const std::string& defaultLayoutName)
{
    if (relativePath.empty()) return "";
    if (Utils::fileExists(relativePath)) return relativePath;

    // 1. Relative to layoutDir
    if (!layoutDir.empty()) {
        std::string p = Utils::combinePath(layoutDir, relativePath);
        if (Utils::fileExists(p)) return p;
    }

    // 2. Relative to default layout folder (layouts/<defaultLayoutName>)
    if (!defaultLayoutName.empty()) {
        std::string p = Utils::combinePath(Configuration::absolutePath, "layouts", defaultLayoutName, relativePath);
        if (Utils::fileExists(p)) return p;
    }

    // 3. Relative to RetroFE root
    std::string p = Utils::combinePath(Configuration::absolutePath, relativePath);
    if (Utils::fileExists(p)) return p;

    // Fallback: return preferred path
    if (!layoutDir.empty()) return Utils::combinePath(layoutDir, relativePath);
    if (!defaultLayoutName.empty()) return Utils::combinePath(Configuration::absolutePath, "layouts", defaultLayoutName, relativePath);
    return Utils::combinePath(Configuration::absolutePath, relativePath);
}

void RetroFE::freeLoadingResources()
{
    for (auto& pair : loadingImageCache_) {
        for (auto& tPair : pair.second.textures) {
            if (tPair.second) SDL_DestroyTexture(tPair.second);
        }
        if (pair.second.surface) SDL_FreeSurface(pair.second.surface);
    }
    loadingImageCache_.clear();

    for (auto& pair : loadingFontCache_) {
        if (pair.second.font) TTF_CloseFont(pair.second.font);
    }
    loadingFontCache_.clear();

    for (auto& pair : loadingTextTextureCache_) {
        for (auto& tPair : pair.second) {
            if (tPair.second) SDL_DestroyTexture(tPair.second);
        }
    }
    loadingTextTextureCache_.clear();

    if (loadingFont_) {
        TTF_CloseFont(loadingFont_);
        loadingFont_ = nullptr;
    }
    if (loadingTextTexture_) {
        SDL_DestroyTexture(loadingTextTexture_);
        loadingTextTexture_ = nullptr;
    }
}

void RetroFE::updateLoadingConfig(const Page::LoadingConfig& config)
{
    loadingConfig_ = config;
    std::stable_sort(loadingConfig_.elements.begin(), loadingConfig_.elements.end(),
        [](const Page::LoadingElement& a, const Page::LoadingElement& b) {
            return a.layer < b.layer;
        });

    std::string defaultLayoutName;
    config_.getProperty("layout", defaultLayoutName);

    std::set<std::string> activeImages;
    std::set<std::pair<std::string, int>> activeFonts;

    for (const auto& el : config.elements) {
        if (el.type == Page::LoadingElement::Image && !el.src.empty()) {
            std::string fullPath = resolveLoadingPath(config.layoutDir, el.src, defaultLayoutName);
            activeImages.insert(fullPath);
        }
        else if (el.type == Page::LoadingElement::Text) {
            std::string fontPath = el.font.empty() ? "" : resolveLoadingPath(config.layoutDir, el.font, defaultLayoutName);
            activeFonts.insert({ fontPath, el.fontSize });
        }
    }

    // Clean up cached images no longer used
    for (auto it = loadingImageCache_.begin(); it != loadingImageCache_.end(); ) {
        if (activeImages.find(it->first) == activeImages.end()) {
            for (auto& tPair : it->second.textures) {
                if (tPair.second) SDL_DestroyTexture(tPair.second);
            }
            if (it->second.surface) SDL_FreeSurface(it->second.surface);
            it = loadingImageCache_.erase(it);
        } else {
            ++it;
        }
    }

    // Pre-load surfaces for new images
    for (const auto& path : activeImages) {
        if (loadingImageCache_.find(path) == loadingImageCache_.end()) {
            LoadingImageCache cache;
            cache.surface = JxlLoader::IsJxlFile(path) ? JxlLoader::LoadSurface(path) : IMG_Load(path.c_str());
            if (cache.surface) {
                Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Preloaded loading image: " + path);
            } else {
                Logger::write(Logger::ZONE_WARNING, "RetroFeVLC", "Failed to load loading image: " + path);
            }
            loadingImageCache_[path] = cache;
        }
    }

    // Invalidate text texture cache since config changed
    for (auto& pair : loadingTextTextureCache_) {
        for (auto& tPair : pair.second) {
            if (tPair.second) SDL_DestroyTexture(tPair.second);
        }
    }
    loadingTextTextureCache_.clear();
}

void RetroFE::renderLoadingScreen()
{
    SDL_LockMutex(SDL::getMutex());
    for (int d = 0; d < SDL::getNumDisplays(); ++d)
    {
        SDL_Renderer* renderer = SDL::getRenderer(d);
        if (!renderer) continue;

        int winW = 1920, winH = 1080;
        SDL_GetRendererOutputSize(renderer, &winW, &winH);

        // Default loading screen
        if (!loadingConfig_.hasCustomElements())
        {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);

            std::string text = "Loading";
            SDL_Color textColor = { 255, 255, 255, 255 };

            int targetFontSize = (std::max)(22, winH / 26);
            if (!loadingFont_ || loadingFontSize_ != targetFontSize) {
                if (loadingFont_) {
                    TTF_CloseFont(loadingFont_);
                    loadingFont_ = nullptr;
                }
                if (loadingTextTexture_) {
                    SDL_DestroyTexture(loadingTextTexture_);
                    loadingTextTexture_ = nullptr;
                }
                if (TTF_WasInit()) {
                    loadingFont_ = TTF_OpenFont("C:\\Windows\\Fonts\\segoeui.ttf", targetFontSize);
                    if (!loadingFont_) {
                        loadingFont_ = TTF_OpenFont("C:\\Windows\\Fonts\\arial.ttf", targetFontSize);
                    }
                }
                loadingFontSize_ = targetFontSize;
            }

            int textW = 0, textH = 0;
            if (loadingFont_) {
                TTF_SizeUTF8(loadingFont_, text.c_str(), &textW, &textH);
            } else {
                textW = static_cast<int>(text.size()) * targetFontSize * 6 / 10;
                textH = targetFontSize;
            }

            int numDots = 6;
            int dotRadius = (std::max)(4, winH / 220);
            int dotSpacing = (std::max)(18, winH / 55);
            int dotsTotalW = (numDots - 1) * dotSpacing;
            int gap = (std::max)(22, winH / 50);
            int totalW = textW + gap + dotsTotalW;

            int startX = (winW - totalW) / 2;
            int startY = (winH - textH) / 2;

            if (loadingFont_) {
                if (!loadingTextTexture_ || cachedLoadingText_ != text ||
                    cachedLoadingTextColor_.r != textColor.r ||
                    cachedLoadingTextColor_.g != textColor.g ||
                    cachedLoadingTextColor_.b != textColor.b)
                {
                    if (loadingTextTexture_) {
                        SDL_DestroyTexture(loadingTextTexture_);
                        loadingTextTexture_ = nullptr;
                    }
                    SDL_Surface* surface = TTF_RenderUTF8_Blended(loadingFont_, text.c_str(), textColor);
                    if (surface) {
                        loadingTextTexture_ = SDL_CreateTextureFromSurface(renderer, surface);
                        loadingTextW_ = surface->w;
                        loadingTextH_ = surface->h;
                        SDL_FreeSurface(surface);
                        cachedLoadingText_ = text;
                        cachedLoadingTextColor_ = textColor;
                    }
                }
                if (loadingTextTexture_) {
                    SDL_Rect dest = { startX, startY, loadingTextW_, loadingTextH_ };
                    SDL_RenderCopy(renderer, loadingTextTexture_, NULL, &dest);
                }
            }

            // Animated forward dots
            Uint32 ticks = SDL_GetTicks();
            float waveSpeed = 800.0f;
            float waveProgress = std::fmod(static_cast<float>(ticks), waveSpeed) / waveSpeed;
            float waveHead = waveProgress * static_cast<float>(numDots);

            int dotsStartX = startX + textW + gap;
            int dotCenterY = startY + (textH / 2);

            for (int i = 0; i < numDots; ++i) {
                int cx = dotsStartX + i * dotSpacing;
                int cy = dotCenterY;

                float diff = static_cast<float>(i) - waveHead;
                while (diff < -numDots / 2.0f) diff += numDots;
                while (diff > numDots / 2.0f) diff -= numDots;
                float dist = std::fabs(diff);
                float intensity = (std::max)(0.0f, 1.0f - dist / 1.3f);

                SDL_Color dotCol;
                if (intensity > 0.05f) {
                    float hue = std::fmod((ticks * 0.12f) + (i * 40.0f), 360.0f);
                    SDL_Color waveColor = hsvToRgb(hue, 0.75f, 1.0f);
                    dotCol.r = static_cast<Uint8>(textColor.r * (1.0f - intensity) + waveColor.r * intensity);
                    dotCol.g = static_cast<Uint8>(textColor.g * (1.0f - intensity) + waveColor.g * intensity);
                    dotCol.b = static_cast<Uint8>(textColor.b * (1.0f - intensity) + waveColor.b * intensity);
                    dotCol.a = 255;
                } else {
                    dotCol.r = static_cast<Uint8>(textColor.r * 0.25f);
                    dotCol.g = static_cast<Uint8>(textColor.g * 0.25f);
                    dotCol.b = static_cast<Uint8>(textColor.b * 0.25f);
                    dotCol.a = 255;
                }

                int r = dotRadius + static_cast<int>(intensity * (dotRadius * 0.5f));
                drawLoadingCircle(renderer, cx, cy, r, dotCol);
            }

            SDL_RenderPresent(renderer);
            continue;
        }

        // Custom loading elements
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        std::string defaultLayoutName;
        config_.getProperty("layout", defaultLayoutName);

        bool hasText = false;
        int lastTextEndX = winW / 2;
        int lastTextBaselineY = winH / 2;
        int lastTextH = (std::max)(22, winH / 26);
        SDL_Color dotsTextColor = { 255, 255, 255, 255 };

        for (const auto& el : loadingConfig_.elements)
        {
            if (el.monitor != 0 && el.monitor != d) continue;

            if (el.type == Page::LoadingElement::Container)
            {
                SDL_SetRenderDrawColor(renderer, el.color.r, el.color.g, el.color.b, static_cast<Uint8>(el.alpha * 255));
                float rw = (el.wStr == "-1" || el.wStr.empty()) ? static_cast<float>(winW) : resolveCoord(el.wStr, static_cast<float>(winW), static_cast<float>(winW));
                float rh = (el.hStr == "-1" || el.hStr.empty()) ? static_cast<float>(winH) : resolveCoord(el.hStr, static_cast<float>(winH), static_cast<float>(winH));
                float rawX = resolveCoord(el.xStr, static_cast<float>(winW), 0.0f) + resolveCoord(el.xOffsetStr, static_cast<float>(winW), 0.0f);
                float rawY = resolveCoord(el.yStr, static_cast<float>(winH), 0.0f) + resolveCoord(el.yOffsetStr, static_cast<float>(winH), 0.0f);
                float originX = resolveOrigin(el.xOriginStr, el.xStr, 0.0f);
                float originY = resolveOrigin(el.yOriginStr, el.yStr, 0.0f);
                float rx = rawX - (originX * rw);
                float ry = rawY - (originY * rh);
                SDL_Rect r = { static_cast<int>(rx), static_cast<int>(ry), static_cast<int>(rw), static_cast<int>(rh) };
                SDL_RenderFillRect(renderer, &r);
            }
            else if (el.type == Page::LoadingElement::Image && !el.src.empty())
            {
                std::string fullPath = resolveLoadingPath(loadingConfig_.layoutDir, el.src, defaultLayoutName);
                auto it = loadingImageCache_.find(fullPath);
                if (it != loadingImageCache_.end() && it->second.surface) {
                    SDL_Surface* surf = it->second.surface;
                    if (it->second.textures.find(d) == it->second.textures.end() || !it->second.textures[d]) {
                        it->second.textures[d] = SDL_CreateTextureFromSurface(renderer, surf);
                    }
                    SDL_Texture* tex = it->second.textures[d];
                    if (tex) {
                        float boxW = (el.wStr == "-1" || el.wStr.empty()) ? static_cast<float>(surf->w) : resolveCoord(el.wStr, static_cast<float>(winW), static_cast<float>(surf->w));
                        float boxH = (el.hStr == "-1" || el.hStr.empty()) ? static_cast<float>(surf->h) : resolveCoord(el.hStr, static_cast<float>(winH), static_cast<float>(surf->h));

                        SDL_Rect srcRect = { 0, 0, surf->w, surf->h };
                        SDL_Rect* pSrcRect = nullptr;
                        float imgW = static_cast<float>(surf->w);
                        float imgH = static_cast<float>(surf->h);
                        float renderW = boxW;
                        float renderH = boxH;

                        if (el.scaleMode == "fit") {
                            float imgAspect = imgW / imgH;
                            float boxAspect = boxW / boxH;
                            if (imgAspect > boxAspect) {
                                renderW = boxW;
                                renderH = boxW / imgAspect;
                            } else {
                                renderH = boxH;
                                renderW = boxH * imgAspect;
                            }
                        } else if (el.scaleMode == "fill") {
                            float imgAspect = imgW / imgH;
                            float boxAspect = boxW / boxH;
                            float scale = (imgAspect < boxAspect) ? (boxW / imgW) : (boxH / imgH);
                            float srcW = boxW / scale;
                            float srcH = boxH / scale;
                            srcRect.x = static_cast<int>((imgW - srcW) * 0.5f);
                            srcRect.y = static_cast<int>((imgH - srcH) * 0.5f);
                            srcRect.w = static_cast<int>(srcW);
                            srcRect.h = static_cast<int>(srcH);
                            renderW = boxW;
                            renderH = boxH;
                            pSrcRect = &srcRect;
                        } else if (el.scaleMode == "none") {
                            renderW = imgW;
                            renderH = imgH;
                        } else { // stretch
                            renderW = boxW;
                            renderH = boxH;
                        }

                        float rawX = resolveCoord(el.xStr, static_cast<float>(winW), static_cast<float>(winW) * 0.5f) + resolveCoord(el.xOffsetStr, static_cast<float>(winW), 0.0f);
                        float rawY = resolveCoord(el.yStr, static_cast<float>(winH), static_cast<float>(winH) * 0.5f) + resolveCoord(el.yOffsetStr, static_cast<float>(winH), 0.0f);

                        float originX = resolveOrigin(el.xOriginStr, el.xStr);
                        float originY = resolveOrigin(el.yOriginStr, el.yStr);

                        float dstX = rawX - (originX * renderW);
                        float dstY = rawY - (originY * renderH);

                        SDL_Rect dstRect = {
                            static_cast<int>(dstX),
                            static_cast<int>(dstY),
                            static_cast<int>(renderW),
                            static_cast<int>(renderH)
                        };
                        SDL_SetTextureAlphaMod(tex, static_cast<Uint8>(el.alpha * 255));
                        SDL_RenderCopy(renderer, tex, pSrcRect, &dstRect);
                    }
                }
            }
            else if (el.type == Page::LoadingElement::Text && !el.text.empty())
            {
                std::string displayText = el.text;
                while (!displayText.empty() && (displayText.back() == '.' || displayText.back() == ' ')) {
                    displayText.pop_back();
                }
                if (displayText.empty()) displayText = el.text;

                int targetFontSize = el.fontSize > 0 ? el.fontSize : (std::max)(22, winH / 26);
                std::string fontKey = el.font + "_" + std::to_string(targetFontSize);

                TTF_Font* font = nullptr;
                auto fIt = loadingFontCache_.find(fontKey);
                if (fIt != loadingFontCache_.end()) {
                    font = fIt->second.font;
                } else {
                    if (TTF_WasInit()) {
                        if (!el.font.empty()) {
                            std::string fullFont = resolveLoadingPath(loadingConfig_.layoutDir, el.font, defaultLayoutName);
                            font = TTF_OpenFont(fullFont.c_str(), targetFontSize);
                        }
                        if (!font) {
                            font = TTF_OpenFont("C:\\Windows\\Fonts\\segoeui.ttf", targetFontSize);
                            if (!font) font = TTF_OpenFont("C:\\Windows\\Fonts\\arial.ttf", targetFontSize);
                        }
                    }
                    LoadingFontCache lfc;
                    lfc.font = font;
                    lfc.fontSize = targetFontSize;
                    loadingFontCache_[fontKey] = lfc;
                }

                int textW = 0, textH = 0;
                if (font) {
                    TTF_SizeUTF8(font, displayText.c_str(), &textW, &textH);
                } else {
                    textW = static_cast<int>(displayText.size()) * targetFontSize * 6 / 10;
                    textH = targetFontSize;
                }

                float rawX = resolveCoord(el.xStr, static_cast<float>(winW), static_cast<float>(winW) * 0.5f) + resolveCoord(el.xOffsetStr, static_cast<float>(winW), 0.0f);
                float rawY = resolveCoord(el.yStr, static_cast<float>(winH), static_cast<float>(winH) * 0.5f) + resolveCoord(el.yOffsetStr, static_cast<float>(winH), 0.0f);

                float originX = resolveOrigin(el.xOriginStr, el.xStr);
                float originY = resolveOrigin(el.yOriginStr, el.yStr);

                float dstX = rawX - (originX * textW);
                float dstY = rawY - (originY * textH);

                if (font) {
                    std::string texKey = displayText + "_" + fontKey + "_" +
                                         std::to_string(el.color.r) + "_" + std::to_string(el.color.g) + "_" + std::to_string(el.color.b);
                    SDL_Texture* tex = loadingTextTextureCache_[texKey][d];
                    if (!tex) {
                        SDL_Surface* surf = TTF_RenderUTF8_Blended(font, displayText.c_str(), el.color);
                        if (surf) {
                            tex = SDL_CreateTextureFromSurface(renderer, surf);
                            SDL_FreeSurface(surf);
                            loadingTextTextureCache_[texKey][d] = tex;
                        }
                    }
                    if (tex) {
                        SDL_Rect dest = { static_cast<int>(dstX), static_cast<int>(dstY), textW, textH };
                        SDL_SetTextureAlphaMod(tex, static_cast<Uint8>(el.alpha * 255));
                        SDL_RenderCopy(renderer, tex, NULL, &dest);
                    }
                }

                hasText = true;
                lastTextEndX = static_cast<int>(dstX + textW);
                int fontAscent = font ? TTF_FontAscent(font) : (textH * 8 / 10);
                lastTextBaselineY = static_cast<int>(dstY + fontAscent);
                lastTextH = textH;
                dotsTextColor = el.color;
            }
        }
        // Animated synchronized forward wave dots
        int numDots = 6;
        int dotRadius = 0;
        int dotSpacing = 0;
        int gap = 0;
        int dotsStartX = 0;
        int dotCenterY = 0;

        if (hasText) {
            dotRadius = (std::max)(3, lastTextH / 11);
            dotSpacing = (std::max)(12, dotRadius * 4);
            gap = (std::max)(8, dotRadius * 2);
            dotsStartX = lastTextEndX + gap;
            // Position dots directly on the typographic baseline (bottom of text, like normal ellipsis)
            dotCenterY = lastTextBaselineY - dotRadius - 1;
        } else {
            dotRadius = (std::max)(4, winH / 220);
            dotSpacing = (std::max)(18, winH / 55);
            gap = (std::max)(18, winH / 60);
            int dotsTotalW = (numDots - 1) * dotSpacing;
            dotsStartX = (winW - dotsTotalW) / 2;
            dotCenterY = winH - (std::max)(60, winH / 12);
        }

        Uint32 ticks = SDL_GetTicks();
        float waveSpeed = 800.0f;
        float waveProgress = std::fmod(static_cast<float>(ticks), waveSpeed) / waveSpeed;
        float waveHead = waveProgress * static_cast<float>(numDots);

        for (int i = 0; i < numDots; ++i) {
            int cx = dotsStartX + i * dotSpacing;
            int cy = dotCenterY;

            float diff = static_cast<float>(i) - waveHead;
            while (diff < -numDots / 2.0f) diff += numDots;
            while (diff > numDots / 2.0f) diff -= numDots;
            float dist = std::fabs(diff);
            float intensity = (std::max)(0.0f, 1.0f - dist / 1.3f);

            SDL_Color dotCol;
            if (intensity > 0.05f) {
                float hue = std::fmod((ticks * 0.12f) + (i * 40.0f), 360.0f);
                SDL_Color waveColor = hsvToRgb(hue, 0.75f, 1.0f);
                dotCol.r = static_cast<Uint8>(dotsTextColor.r * (1.0f - intensity) + waveColor.r * intensity);
                dotCol.g = static_cast<Uint8>(dotsTextColor.g * (1.0f - intensity) + waveColor.g * intensity);
                dotCol.b = static_cast<Uint8>(dotsTextColor.b * (1.0f - intensity) + waveColor.b * intensity);
                dotCol.a = 255;
            } else {
                dotCol.r = static_cast<Uint8>(dotsTextColor.r * 0.25f);
                dotCol.g = static_cast<Uint8>(dotsTextColor.g * 0.25f);
                dotCol.b = static_cast<Uint8>(dotsTextColor.b * 0.25f);
                dotCol.a = 255;
            }

            int r = dotRadius + static_cast<int>(intensity * (dotRadius * 0.5f));
            drawLoadingCircle(renderer, cx, cy, r, dotCol);
        }

        SDL_RenderPresent(renderer);
    }
    SDL_UnlockMutex(SDL::getMutex());
}

void RetroFE::reloadLayout()
{
    if (!currentPage_) return;

    updateLoadingConfig(currentPage_->getLoadingConfig());

    isReloading_ = true;
    renderLoadingScreen();

    std::string startMainBuild = "Main";
    config_.getProperty("startMainBuild", startMainBuild);
    // Save state before layout reload
    std::string currentCol = currentPage_->getCollectionName();
    std::string currentPlaylist = currentPage_->getPlaylistName();
    unsigned int currentScrollOffset = currentPage_->getScrollOffsetIndex();
    Item* currentSelectedItem = currentPage_->getSelectedItem();
    std::string selectedItemName = (currentSelectedItem ? currentSelectedItem->name : "");
    reloadSavedItem_ = selectedItemName;
    reloadSavedPlaylist_ = currentPlaylist;
    reloadSavedScroll_ = currentScrollOffset;
    Logger::write(Logger::ZONE_INFO, "RetroFEVLC", "Saved state for collection: " + currentCol + ", playlist: " + currentPlaylist + ", scroll offset: " + std::to_string(currentScrollOffset) + ", selected item: " + selectedItemName);
    // Free current page
    Logger::write(Logger::ZONE_INFO, "RetroFEVLC", "Freeing current page...");
    currentPage_->freeGraphicsMemory();
    currentPage_->deInitialize();
    delete currentPage_;
    currentPage_ = nullptr;
    renderLoadingScreen();
    // Reload layout
    Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Reloading layout...");
    currentPage_ = loadPage();
    if (!currentPage_) {
        Logger::write(Logger::ZONE_ERROR, "RetroFeVLC", "Failed to reload layout!");
        isReloading_ = false;
        return;
    }
    renderLoadingScreen();
    // Restore previous collection
    bool rememberMenu = false;
    config_.getProperty("rememberMenu", rememberMenu);
    Logger::write(Logger::ZONE_INFO, "RetroFeVLC",
        "Navigating back to collection: " + currentCol +
        " (rememberMenu: " + std::to_string(rememberMenu) + ")");
    // ---- CRITICAL: autoEnter logic -------------------------------------------------
    bool autoEnter = (currentCol != startMainBuild) || jumpNextBack_;
    // Log for clarity
    if (currentCol == startMainBuild) {
        if (jumpNextBack_) {
            Logger::write(Logger::ZONE_INFO, "RetroFeVLC",
                "In " + startMainBuild + " + jumpNextBack=true will enter sub-collection and return (arture reload)");
        }
        else {
            Logger::write(Logger::ZONE_INFO, "RetroFeVLC",
                "In " + startMainBuild + " + jumpNextBack=false stay in " + startMainBuild + ", restore selection only");
        }
    }
    else {
        Logger::write(Logger::ZONE_INFO, "RetroFeVLC",
            "In sub-collection '" + currentCol + "' will re-enter to restore state");
    }
    // ---- Perform navigation -------------------------------------------------------
    isReload_ = true;
    navigateToCollection(currentCol, autoEnter, selectedItemName, currentPlaylist, currentScrollOffset);
    // Refresh input mapping
    SDL_PumpEvents();
    input_.resetStates();
    Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Layout reloaded successfully!");
}

//void RetroFE::reloadLayout()
//{
//    if (!currentPage_) return;
//
//    std::string currentCol = currentPage_->getCollectionName();
//    std::string currentPlaylist = currentPage_->getPlaylistName();
//    unsigned int currentScrollOffset = currentPage_->getScrollOffsetIndex();
//    Item* currentSelectedItem = currentPage_->getSelectedItem();
//    std::string selectedItemName = (currentSelectedItem ? currentSelectedItem->name : "");
//
//    Logger::write(Logger::ZONE_INFO, "RetroFeVLC",
//        "Reloading layout - Saved: " + currentCol +
//        " | playlist: " + currentPlaylist +
//        " | offset: " + std::to_string(currentScrollOffset));
//
//    // 1. Free old page
//    currentPage_->freeGraphicsMemory();
//    currentPage_->deInitialize();
//    delete currentPage_;
//    currentPage_ = nullptr;
//
//    // 2. Load new page with new layout
//    currentPage_ = loadPage();
//    if (!currentPage_)
//    {
//        Logger::write(Logger::ZONE_ERROR, "RetroFeVLC", "Failed to load page after reload!");
//        return;
//    }
//
//    // 3. Navigate back
//    isReload_ = true;
//    if (!navigateToCollection(currentCol, false, selectedItemName, currentPlaylist, currentScrollOffset))
//    {
//        Logger::write(Logger::ZONE_ERROR, "RetroFeVLC", "navigateToCollection failed after reload");
//    }
//
//    // 4. Safe final refresh after layout change (list <-> grid)
//    if (currentPage_ && currentPage_->getActiveMenu())
//    {
//        ScrollingList* menu = currentPage_->getActiveMenu();
//
//        // Clean old state
//        menu->freeGraphicsMemory();
//        menu->destroyItems();
//
//        // Rebuild with new layout configuration
//        menu->allocateSpritePoints();
//
//        // Safe index restore
//        unsigned int safeIndex = currentScrollOffset;
//        if (menu->getSize() > 0)
//        {
//            safeIndex = std::fmin(safeIndex, menu->getSize() - 1);
//        }
//
//        menu->setSelectedIndex(safeIndex);
//        menu->allocateGraphicsMemory();
//
//        currentPage_->onNewItemSelected();
//    }
//
//    isReload_ = false;
//    Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Layout reloaded successfully (list/grid transition handled)");
//}
// Initialize the configuration and database
int RetroFE::initialize(void* context)
{


    RetroFE* instance = static_cast<RetroFE*>(context);
    Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Initializing");

    if (!instance->input_.initialize())
    {
        Logger::write(Logger::ZONE_ERROR, "RetroFeVLC", "Could not initialize user controls");
        instance->initializeError = true;
        return -1;
    }

    instance->db_ = new DB(Utils::combinePath(Configuration::absolutePath, "meta.db"));

    if (!instance->db_->initialize())
    {
        Logger::write(Logger::ZONE_ERROR, "RetroFeVLC", "Could not initialize database");
        instance->initializeError = true;
        return -1;
    }

    instance->metadb_ = new MetadataDatabase(*(instance->db_), instance->config_);

    if (!instance->metadb_->initialize())
    {
        Logger::write(Logger::ZONE_ERROR, "RetroFeVLC", "Could not initialize meta database");
        instance->initializeError = true;
        return -1;
    }
    instance->cib_ = new CollectionInfoBuilder(instance->config_, *(instance->metadb_)); // Initialize cib_
    instance->buildSearchEntries(nullptr, instance->cachedSearchEntries_);
    instance->cachedSearchGroups_ = instance->searchGroups_;
    instance->searchIndexBuilt_ = true;
    instance->initialized = true;
    return 0;

}


// Launch a game/program
void RetroFE::launchEnter()
{

    // Record launch start time
    launchStartTime_ = static_cast<float>(SDL_GetTicks()) / 1000.0f;
    // Disable window focus
    SDL_SetWindowGrab(SDL::getWindow(0), SDL_FALSE);

    // Free the textures, and optionally take down SDL
    freeGraphicsMemory(false);

    bool hideMouse = false;
    int  mouseX = 5000;
    int  mouseY = 5000;
    config_.getProperty("hideMouse", hideMouse);
    config_.getProperty("mouseX", mouseX);
    config_.getProperty("mouseY", mouseY);
    if (hideMouse)
        SDL_WarpMouseGlobal(mouseX, mouseY);
}


// Return from the launch of a game/program
void RetroFE::launchExit(bool allocateGraphics)
{
    // Calculate play duration
    float launchEndTime = static_cast<float>(SDL_GetTicks()) / 1000.0f;
    float playDurationSeconds = launchEndTime - launchStartTime_;
    int playDurationMinutes = static_cast<int>(playDurationSeconds / 60.0f); // Convert to minutes
    Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Game session duration: " + std::to_string(playDurationMinutes) + " minutes");

    // Optionally set up SDL, and load the textures
    if (allocateGraphics)
    {
        allocateGraphicsMemory(false);
    }

    // Restore the SDL settings
    SDL_RestoreWindow(SDL::getWindow(0));
    SDL_RaiseWindow(SDL::getWindow(0));
    SDL_SetWindowGrab(SDL::getWindow(0), SDL_TRUE);


    // Empty event queue, but handle joystick add/remove events
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_JOYDEVICEADDED || e.type == SDL_JOYDEVICEREMOVED)
        {
            input_.update(e);
        }
    }
    input_.resetStates();
    attract_.reset();

    // Restore time settings
    currentTime_ = static_cast<float>(SDL_GetTicks()) / 1000;
    keyLastTime_ = currentTime_;
    lastLaunchReturnTime_ = currentTime_;

    bool hideMouse = false;
    int  mouseX = 5000;
    int  mouseY = 5000;
    config_.getProperty("hideMouse", hideMouse);
    config_.getProperty("mouseX", mouseX);
    config_.getProperty("mouseY", mouseY);
    if (hideMouse)
        SDL_WarpMouseGlobal(mouseX, mouseY);

}


// Free the textures, and optionall take down SDL
void RetroFE::freeGraphicsMemory(bool freeAll)
{

    // Free textures
    if (currentPage_)
    {
        currentPage_->freeGraphicsMemory(freeAll);
    }

    // Close down SDL
    bool unloadSDL = false;
    config_.getProperty("unloadSDL", unloadSDL);
    if (unloadSDL)
    {
        currentPage_->deInitializeFonts();
        SDL::deInitialize();
        input_.clearJoysticks();
    }

}


// Optionally set up SDL, and load the textures
void RetroFE::allocateGraphicsMemory(bool allocateSecundary)
{

    // Reopen SDL
    bool unloadSDL = false;
    config_.getProperty("unloadSDL", unloadSDL);
    if (unloadSDL)
    {
        SDL::initialize(config_);
        currentPage_->initializeFonts();
    }

    // Allocate textures
    if (currentPage_)
    {
        currentPage_->allocateGraphicsMemory(allocateSecundary);
    }

}


// Deinitialize RetroFE
bool RetroFE::deInitialize()
{

    bool retVal = true;

    // Free textures
    Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Freeing graphics memory");
    freeGraphicsMemory();


    // Delete page
    if (currentPage_)
    {
        Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Deinitializing current page");
        currentPage_->deInitialize();
        delete currentPage_;
        currentPage_ = NULL;
    }
    
    // Delete databases and CollectionInfoBuilder
    if (cib_)
    {
        delete cib_;
        cib_ = NULL;
    }

    // Delete databases
    if (metadb_)
    {
        delete metadb_;
        metadb_ = NULL;
    }

    if (db_)
    {
        delete db_;
        db_ = NULL;
    }

    initialized = false;

    if (reboot_)
    {
        Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Rebooting");
        VideoFactory::resetAll();
    }
    else
    {
        Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Exiting");
        SDL::deInitialize();
        // Replace gst_deinit()
    }

    return retVal;
}


// Run RetroFE
bool RetroFE::run()
{

    // Clear the RA cache at the start of EVERY RetroFE run
    // This ensures one-time adds per session only
    addedToRaCache_.clear();
    Logger::write(Logger::ZONE_DEBUG, "RetroRa", "Session RA cache cleared");
    static bool sdlInitialized = false;
    // Initialize SDL
    if (!sdlInitialized)
    {
        if (!SDL::initialize(config_))
        {
            Logger::write(Logger::ZONE_ERROR, "RetroFeVLC", "Failed to initialize SDL");
            return false;
        }
        sdlInitialized = true;
    }
    fontcache_.initialize();

    // Define control configuration
    std::string controlsConfPath = Utils::combinePath(Configuration::absolutePath, "controls.conf");
    if (!config_.import("controls", controlsConfPath))
    {
        Logger::write(Logger::ZONE_ERROR, "RetroFeVLC", "Could not import \"" + controlsConfPath + "\"");
        return false;
    }
    // Input repeat delay (seconds) - 0.0 = disabled / continuous repeat while held
    keyDelayTime_ = 0.3f;  // default if not set or invalid

    bool hasKeyDelay = config_.getProperty("keyDelayTime", keyDelayTime_);

    if (!hasKeyDelay) {
        // Key is missing treat as disabled
        keyDelayTime_ = 0.0f;
    }
    else if (keyDelayTime_ <= 0.0f) {
        // User explicitly set 0 or negative treat as disabled
        keyDelayTime_ = 0.0f;
    }
    else {
        // Positive value clamp to sensible range
        if (keyDelayTime_ < 0.05f) {
            keyDelayTime_ = 0.05f;
            Logger::write(Logger::ZONE_WARNING, "RetroFeVLC",
                "keyDelayTime too low - clamped to 0.05 seconds");
        }
        if (keyDelayTime_ > 2.0f) {
            keyDelayTime_ = 2.0f;
            Logger::write(Logger::ZONE_WARNING, "RetroFeVLC",
                "keyDelayTime too high - clamped to 2.0 seconds");
        }
    }

    // Log the effective value
    std::string status = (keyDelayTime_ <= 0.0f)
        ? "DISABLED (continuous repeat while holding)"
        : std::to_string(keyDelayTime_) + " seconds";

    Logger::write(Logger::ZONE_INFO, "RetroFeVLC",
        "Key repeat delay set to: " + status);

    float preloadTime = 0;

    // Initialize video
    bool videoEnable = true;
    int  videoLoop = 0;
    config_.getProperty("videoEnable", videoEnable);
    config_.getProperty("videoLoop", videoLoop);
    VideoFactory::setEnabled(videoEnable);
    VideoFactory::setNumLoops(videoLoop);
    Video::setEnabled(videoEnable);
    config_.getProperty("jumpNextBack", jumpNextBack_);
    initializeThread = SDL_CreateThread(initialize, "RetroFEInit", (void*)this);

    if (!initializeThread)
    {
        Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Could not initialize RetroFE");
        return false;
    }

    int numScreens = SDL::getNumScreens();
    marqueePauseScreen_.resize(numScreens, true);       // default = paused
    marqueePauseScreen_[0] = true;                      // primary is *always* paused

    for (int i = 1; i < numScreens; ++i)
    {
        std::ostringstream key;
        key << "marqueePauseScreen" << i;
        bool pause = true;
        if (!config_.getProperty(key.str(), pause))
            pause = true;                               // missing pause
        marqueePauseScreen_[i] = pause;
        Logger::write(Logger::ZONE_INFO, "RetroFeVLC",
            "marqueePauseScreen" + std::to_string(i) +
            (pause ? " = PAUSED" : " = RUNNING"));
    }

    // Preload layouts 
    std::string layoutName;
    config_.getProperty("layout", layoutName);
    PageBuilder pb(layoutName, "layout", config_, &fontcache_);
    pb.preloadLayouts(); // Preload base and first collection layouts

    // Initialize layout monitoring
    lastLayoutModTime_ = Utils::getFileModificationTime(layoutFilePath_);
    if (lastLayoutModTime_ == 0) {
        Logger::write(Logger::ZONE_ERROR, "RetroFeVLC", "Failed to get initial modification time for layout file: " + layoutFilePath_);
        
    }


    int attractModeTime = 0;
    int attractModeNextTime = 0;
    int attractModePlaylistTime = 0;
    int attractModeCollectionTime = 0;
    int attractModeMinTime = 1000;
    int attractModeMaxTime = 5000;
    std::string firstCollection = "Main";
    bool running = true;
    RETROFE_STATE state = RETROFE_NEW;

    config_.getProperty("attractModeTime", attractModeTime);
    config_.getProperty("attractModeNextTime", attractModeNextTime);
    config_.getProperty("attractModePlaylistTime", attractModePlaylistTime);
    config_.getProperty("attractModeCollectionTime", attractModeCollectionTime);
    config_.getProperty("attractModeMinTime", attractModeMinTime);
    config_.getProperty("attractModeMaxTime", attractModeMaxTime);
    config_.getProperty("firstCollection", firstCollection);

    attract_.idleTime = static_cast<float>(attractModeTime);
    attract_.idleNextTime = static_cast<float>(attractModeNextTime);
    attract_.idlePlaylistTime = static_cast<float>(attractModePlaylistTime);
    attract_.idleCollectionTime = static_cast<float>(attractModeCollectionTime);
    attract_.minTime = attractModeMinTime;
    attract_.maxTime = attractModeMaxTime;

    int fps = 60;
    int fpsIdle = 60;
    config_.getProperty("fps", fps);
    config_.getProperty("fpsIdle", fpsIdle);
    double fpsTime = 1000.0 / static_cast<double>(fps);
    double fpsIdleTime = 1000.0 / static_cast<double>(fpsIdle);

    int initializeStatus = 0;
    bool inputClear = false;

    isJukebox_ = false;
    config_.getProperty("isJukebox", isJukebox_);

    // load the initial splash screen, unload it once it is complete
    currentPage_ = loadSplashPage();
    bool splashMode = false;
    bool exitSplashMode = false;
    if (currentPage_)
    {
        state = RETROFE_ENTER;
        splashMode = true;
    }
    else
    {
        Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "No splash page found; waiting for background initialization");
        SDL_WaitThread(initializeThread, &initializeStatus);
        if (initializeError)
        {
            state = RETROFE_QUIT_REQUEST;
            running = false;
        }
        else
        {
            currentPage_ = loadPage();
            state = RETROFE_ENTER;
            splashMode = false;
        }
    }

    Launcher l(config_, this);
    Menu     m(config_, input_);
    preloadTime = static_cast<float>(SDL_GetTicks()) / 1000;
    l.LEDBlinky(1);

    while (running)
    {

        float lastTime = 0;
        float deltaTime = 0;

        // Exit splash mode when an active key is pressed
        SDL_Event e;
        if (splashMode && SDL_PollEvent(&e))
        {
            if (input_.update(e) && input_.keystate(UserInput::KeyCodeSelect))
            {
                exitSplashMode = true;
                while (SDL_PollEvent(&e))
                {
                    if (e.type == SDL_JOYDEVICEADDED || e.type == SDL_JOYDEVICEREMOVED)
                    {
                        input_.update(e);
                    }
                }
                input_.resetStates();
                attract_.reset();
            }
        }

        if (!currentPage_)
        {
            Logger::write(Logger::ZONE_WARNING, "RetroFeVLC", "Could not load page");
            running = false;
            break;
        }

        switch (state)
        {

            // Idle state; waiting for input
        case RETROFE_IDLE:
            currentPage_->cleanup();

            // Handle end of splash mode first
            if ((initialized || initializeError) && splashMode && (exitSplashMode || (currentPage_->getMinShowTime() <= (currentTime_ - preloadTime) && !(currentPage_->isPlaying()))))
            {
                SDL_WaitThread(initializeThread, &initializeStatus);

                if (initializeError)
                {
                    state = RETROFE_QUIT_REQUEST;
                    break;
                }

                currentPage_->stop();
                state = RETROFE_SPLASH_EXIT;
                break;
            }

            // Process startup queue
            if (!splashMode && currentPage_->isIdle()) {
                // Handle item selection without entering
                if (!startupSelectItem_.empty()) {
                    Item* selectItem = nullptr;
                    std::vector<Item*>& collItems = currentPage_->getCollection()->items;
                    for (auto* item : collItems) {
                        if (item->name == startupSelectItem_ && !item->leaf) {
                            selectItem = item;
                            break;
                        }
                    }
                    if (selectItem) {
                        currentPage_->selectItem(selectItem);
                        currentPage_->onNewItemSelected();
                        currentPage_->allocateGraphicsMemory();
                        currentPage_->start();
                        Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Selected collection: " + startupSelectItem_);
                    }
                    else {
                        Logger::write(Logger::ZONE_WARNING, "RetroFeVLC", "Could not find item for collection: " + startupSelectItem_ + " in " + currentPage_->getCollectionName());
                    }
                    startupSelectItem_ = "";
                    state = RETROFE_IDLE;
                }
                // Handle collection entry
                else if (!startupEnterQueue_.empty()) {
                    std::string nextCol = startupEnterQueue_.front();
                    startupEnterQueue_.erase(startupEnterQueue_.begin());
                    Item* nextItem = nullptr;
                    std::vector<Item*>& collItems = currentPage_->getCollection()->items;
                    for (auto* item : collItems) {
                        if (item->name == nextCol && !item->leaf) {
                            nextItem = item;
                            break;
                        }
                    }
                    if (nextItem) {
                        currentPage_->selectItem(nextItem);
                        nextPageItem_ = nextItem;
                        enterNumberOfTimes_ = 1;
                        state = RETROFE_NEXT_PAGE_REQUEST;
                        Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Startup: Entering collection " + nextCol);
                    }
                    else {
                        Logger::write(Logger::ZONE_WARNING, "RetroFeVLC", "Startup: Could not find item for collection " + nextCol + " in " + currentPage_->getCollectionName());
                        state = RETROFE_IDLE;
                    }
                }
                // END OF COLLECTION ENTRY

              // Handle back navigation to Main
                else if (pendingBackToMain_ && currentPage_->getMenuDepth() > 1 && jumpNextBack_)
                {
                    pendingBackToMain_ = false;
                    state = RETROFE_BACK_REQUEST;
                    Logger::write(Logger::ZONE_INFO, "RetroFeVLC",
                        "jumpNextBack: Triggering back to Main after auto-enter");
                }
                else if (pendingBackToMain_)
                {
                    pendingBackToMain_ = false;   // clear when disabled
                    Logger::write(Logger::ZONE_INFO, "RetroFeVLC",
                        "jumpNextBack=false: Cancelled pending back to Main");
                }
                // FINAL SELECTION RESTORATION AFTER QUEUE IS EMPTY
                else if ((isAutoNavigating_ || !reloadSavedItem_.empty()) && startupEnterQueue_.empty()) {
                    bool rememberMenu = false;
                    config_.getProperty("rememberMenu", rememberMenu);

                    if (rememberMenu || isReload_) {
                        bool skipSelect = false;

                        // Verifica se ja estamos no item e offset corretos
                        Item* currentSelected = currentPage_->getSelectedItem();
                        unsigned int currentOffset = currentPage_->getScrollOffsetIndex();

                        if (currentSelected &&
                            currentSelected->name == reloadSavedItem_ &&
                            (reloadSavedScroll_ == static_cast<unsigned int>(-1) || currentOffset == reloadSavedScroll_))
                        {
                            skipSelect = true;
                            Logger::write(Logger::ZONE_INFO, "RetroFeVLC",
                                "Already at correct item ('" + reloadSavedItem_ + "') skipping select to avoid jump");
                        }

                        // Restore scroll (so se for diferente)
                        if (!skipSelect && reloadSavedScroll_ != static_cast<unsigned int>(-1)) {
                            currentPage_->setScrollOffsetIndex(reloadSavedScroll_);
                            Logger::write(Logger::ZONE_INFO, "RetroFeVLC",
                                "Restored scroll offset: " + std::to_string(reloadSavedScroll_));
                        }

                        // Restore selected item (so se for necessario)
                        if (!skipSelect && !reloadSavedItem_.empty()) {
                            std::vector<Item*>* playlist = currentPage_->getCurrentPlaylist();
                            if (playlist) {
                                auto it = std::find_if(playlist->begin(), playlist->end(),
                                    [this](const Item* item) { return item->name == reloadSavedItem_; });

                                if (it != playlist->end()) {
                                    currentPage_->selectItem(*it);
                                    currentPage_->onNewItemSelected();
                                    currentPage_->reallocateMenuSpritePoints();
                                    Logger::write(Logger::ZONE_INFO, "RetroFeVLC",
                                        "RESTORED ITEM: '" + reloadSavedItem_ +
                                        "' in collection: " + currentPage_->getCollectionName());
                                }
                                else {
                                    Logger::write(Logger::ZONE_WARNING, "RetroFeVLC",
                                        "Could not restore item: '" + reloadSavedItem_ + "' -- not found in playlist");
                                }
                            }
                        }

                        // Restore playlist
                        if (!reloadSavedPlaylist_.empty() &&
                            currentPage_->getPlaylistName() != reloadSavedPlaylist_) {
                            currentPage_->selectPlaylist(reloadSavedPlaylist_);
                            Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Restored playlist: " + reloadSavedPlaylist_);
                        }
                    }

                    // ONE-TIME ONLY
                    reloadSavedItem_.clear();
                    reloadSavedPlaylist_.clear();
                    reloadSavedScroll_ = static_cast<unsigned int>(-1);

                    // End auto-navigation
                    isAutoNavigating_ = false;
                    if (currentPage_) currentPage_->setAutoNavigating(false);
                    isReload_ = false;
                    input_.resetStates();

                    // Force target artwork update
                    if (currentPage_) {
                        currentPage_->onNewItemSelected();
                        currentPage_->reallocateMenuSpritePoints();
                        if (currentPage_->getMenuDepth() == 1)
                        {
                            currentPage_->start();
                        }
                        else
                        {
                            currentPage_->enterMenu();
                        }
                        currentPage_->highlightEnter();
                    }
                }
                // Handle reload entry
                else if (enterNumberOfTimes_ == 0 && enterCollectionAfterReload_) {
                    nextPageItem_ = currentPage_->getSelectedItem();
                    if (nextPageItem_ && !nextPageItem_->leaf) {
                        Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Entering collection after reload");
                        state = RETROFE_NEXT_PAGE_REQUEST;
                        enterNumberOfTimes_ = 1;
                    }
                    enterCollectionAfterReload_ = false;
                }
                // Normal idle processing
                else if (lastLaunchReturnTime_ == 0 || (currentTime_ - lastLaunchReturnTime_ > 0.3)) {
                    if (currentPage_->isIdle()) {
                        state = processUserInput(currentPage_);
                    }
                    lastLaunchReturnTime_ = 0;
                }
            }

            break;

            // Load art on entering RetroFE
        case RETROFE_LOAD_ART:
            currentPage_->start();
            state = RETROFE_ENTER;
            break;

            // Wait for onEnter animation to finish
      // Wait for onEnter animation to finish
        case RETROFE_ENTER:
            if (currentPage_->isIdle())
            {
                bool startCollectionEnter = false;
                config_.getProperty("startCollectionEnter", startCollectionEnter);
                nextPageItem_ = currentPage_->getSelectedItem();
                if (!splashMode && startCollectionEnter && nextPageItem_ && !nextPageItem_->leaf &&
                    startupEnterQueue_.empty() && startupSelectItem_.empty())
                {
                    state = RETROFE_NEXT_PAGE_REQUEST;
                }
                // **OLD hard-coded jump – now respects jumpNextBack_**
                else if (!splashMode && !startCollectionEnter &&
                    currentPage_->getCollectionName() == "Main" && nextPageItem_ && !nextPageItem_->leaf &&
                    startupEnterQueue_.empty() && startupSelectItem_.empty() && jumpNextBack_)
                {
                    Logger::write(Logger::ZONE_INFO, "RetroFeVLC",
                        "jumpNextBack=true: Auto-entering Main sub-collection to reload artwork");
                    pendingBackToMain_ = true;
                    isAutoNavigating_ = true;
                    if (currentPage_) currentPage_->setAutoNavigating(true);
                    state = RETROFE_NEXT_PAGE_REQUEST;
                }
                else
                {
                    state = RETROFE_IDLE;
                }
            }
            break;

            // Handle end of splash mode
        case RETROFE_SPLASH_EXIT:
            if (currentPage_->isIdle())
            {
                // Delete the splash screen
                Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Deinitializing splash page");
                currentPage_->deInitialize();
                delete currentPage_;
                currentPage_ = nullptr;

                // Load new page
                Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Loading initial page");
                currentPage_ = loadPage();
                splashMode = false;
                if (!currentPage_)
                {
                    Logger::write(Logger::ZONE_ERROR, "RetroFeVLC", "Failed to load initial page");
                    state = RETROFE_QUIT_REQUEST;
                    break;
                }

                // Clear renderer to avoid black screen
                SDL_LockMutex(SDL::getMutex());
                for (int i = 0; i < SDL::getNumDisplays(); ++i)
                {
                    SDL_Renderer* renderer = SDL::getRenderer(i);
                    if (renderer) {
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Black background
                        SDL_RenderClear(renderer);
                        SDL_RenderPresent(renderer);
                        Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC", "Cleared renderer for display " + std::to_string(i));
                    }
                }
                SDL_UnlockMutex(SDL::getMutex());

                std::string firstCollection = "Main";
                config_.getProperty("firstCollection", firstCollection);
                Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "First collection set to: " + firstCollection);

                bool startCollectionEnter = false;
                config_.getProperty("startCollectionEnter", startCollectionEnter);

                // Navigate to firstCollection
                if (!navigateToCollection(firstCollection, startCollectionEnter)) {
                    Logger::write(Logger::ZONE_ERROR, "RetroFeVLC", "Failed to navigate to first collection: " + firstCollection);
                    state = RETROFE_QUIT_REQUEST;
                    break;
                }

                state = RETROFE_LOAD_ART;
            }
            break;

            // Switch playlist; start onHighlightExit animation
        case RETROFE_PLAYLIST_REQUEST:
            inputClear = false;
            config_.getProperty("playlistInputClear", inputClear);
            if (inputClear)
            {
                // Empty event queue
                SDL_Event e;
                while (SDL_PollEvent(&e))
                    input_.update(e);
                input_.resetStates();
            }
            currentPage_->highlightExit();
            currentPage_->playlistExit();
            currentPage_->setScrolling(Page::ScrollDirectionIdle);
            state = RETROFE_PLAYLIST_EXIT;
            break;

            // Switch playlist; wait for onHighlightExit animation to finish; load art
        case RETROFE_PLAYLIST_EXIT:
            if (currentPage_->isIdle())
            {
                currentPage_->onNewItemSelected();
                state = RETROFE_PLAYLIST_LOAD_ART;
            }
            break;

            // Switch playlist; start onHighlightEnter animation
        case RETROFE_PLAYLIST_LOAD_ART:
            if (currentPage_->isIdle())
            {
                currentPage_->reallocateMenuSpritePoints();
                currentPage_->playlistEnter();
                state = RETROFE_PLAYLIST_ENTER;
            }
            break;

            // Switch playlist; wait for onHighlightEnter animation to finish
        case RETROFE_PLAYLIST_ENTER:
            if (currentPage_->isIdle())
            {
                currentPage_->highlightEnter();
                state = RETROFE_IDLE;
            }
            break;

            // Jump in menu; start onMenuJumpExit animation
        case RETROFE_MENUJUMP_REQUEST:
            inputClear = false;
            config_.getProperty("jumpInputClear", inputClear);
            if (inputClear)
            {
                // Empty event queue
                SDL_Event e;
                while (SDL_PollEvent(&e))
                    input_.update(e);
                input_.resetStates();
            }
            currentPage_->menuJumpExit();
            currentPage_->setScrolling(Page::ScrollDirectionIdle);
            state = RETROFE_MENUJUMP_EXIT;
            break;

            // Jump in menu; wait for onMenuJumpExit animation to finish; load art
        case RETROFE_MENUJUMP_EXIT:
            if (currentPage_->isIdle())
            {
                currentPage_->onNewItemSelected();
                state = RETROFE_MENUJUMP_LOAD_ART;
            }
            break;

            // Jump in menu; start onMenuJumpEnter animation
        case RETROFE_MENUJUMP_LOAD_ART:
            if (currentPage_->isIdle())
            {
                currentPage_->reallocateMenuSpritePoints();
                currentPage_->menuJumpEnter();
                state = RETROFE_MENUJUMP_ENTER;
            }
            break;

            // Jump in menu; wait for onMenuJump animation to finish
        case RETROFE_MENUJUMP_ENTER:
            if (currentPage_->isIdle())
            {
                currentPage_->highlightEnter();
                state = RETROFE_IDLE;
            }
            break;

            // Start onHighlightExit animation
        case RETROFE_HIGHLIGHT_REQUEST:
            currentPage_->setScrolling(Page::ScrollDirectionIdle);
            currentPage_->highlightExit();
            state = RETROFE_HIGHLIGHT_EXIT;
            break;

            // Wait for onHighlightExit animation to finish; load art
        case RETROFE_HIGHLIGHT_EXIT:
            if (currentPage_->isIdle())
            {
                currentPage_->highlightLoadArt();
                state = RETROFE_HIGHLIGHT_LOAD_ART;
            }
            break;

            // Start onHighlightEnter animation
        case RETROFE_HIGHLIGHT_LOAD_ART:
            currentPage_->highlightEnter();
            if (currentPage_->getSelectedItem())
                l.LEDBlinky(9, currentPage_->getSelectedItem()->collectionInfo->name, currentPage_->getSelectedItem());
            state = RETROFE_HIGHLIGHT_ENTER;
            break;

            // Wait for onHighlightEnter animation to finish
        case RETROFE_HIGHLIGHT_ENTER:
            RETROFE_STATE state_tmp;
            if (currentPage_->isMenuIdle() &&
                ((state_tmp = processUserInput(currentPage_)) == RETROFE_HIGHLIGHT_REQUEST ||
                    state_tmp == RETROFE_MENUJUMP_REQUEST ||
                    state_tmp == RETROFE_PLAYLIST_REQUEST))
            {
                state = state_tmp;
            }
            else if (currentPage_->isIdle())
            {
                state = RETROFE_IDLE;
            }
            break;

            // Next page; start onMenuExit animation
        case RETROFE_NEXT_PAGE_REQUEST:
            currentPage_->exitMenu();
            if (isAutoNavigating_ && currentPage_)
            {
                currentPage_->completeAnimations();
            }
            state = RETROFE_NEXT_PAGE_MENU_EXIT;
            break;

            // Wait for onMenuExit animation to finish; load new page if applicable; load art
        case RETROFE_NEXT_PAGE_MENU_EXIT:
            isInCriticalState_ = true;
            if (currentPage_->isIdle())
            {
                if (currentPage_->getSelectedItem())
                    l.LEDBlinky(8, currentPage_->getSelectedItem()->name, currentPage_->getSelectedItem());
                lastMenuOffsets_[currentPage_->getCollectionName()] = currentPage_->getScrollOffsetIndex();
                lastMenuPlaylists_[currentPage_->getCollectionName()] = currentPage_->getPlaylistName();
                if (!nextPageItem_)
                {
                    isInCriticalState_ = false;
                    state = RETROFE_IDLE;
                    break;
                }
                std::string nextPageName = nextPageItem_->name;
                bool searchResultsNext = ( searchPageCollection_ && nextPageName == "_search" );
                if (!searchResultsNext && !collectionExists(nextPageName, menuMode_))
                {
                    Logger::write(Logger::ZONE_WARNING, "RetroFeVLC",
                        "Cannot load " + std::string(menuMode_ ? "menu: '" : "collection: '") +
                        nextPageName + "' (does not exist)");
                    isInCriticalState_ = false;
                    state = RETROFE_IDLE;
                    break;
                }
                if (!menuMode_)
                {
                    // Load new layout if available
                    std::string layoutName;
                    config_.getProperty("layout", layoutName);
                    if (!searchResultsNext)
                    {
                        PageBuilder pb(layoutName, "layout", config_, &fontcache_);
                        Page* page = pb.buildPage(nextPageItem_->name);
                        if (page)
                        {
                            currentPage_->freeGraphicsMemory();
                            pages_.push(currentPage_);
                            currentPage_ = page;
                            if (isAutoNavigating_ && currentPage_) currentPage_->setAutoNavigating(true);
                        }
                    }
                }

                if (searchResultsNext)
                {
                    if (currentPage_->getCollectionName() == "TempGameCollection" && !pages_.empty())
                    {
                        Page* gamePage = currentPage_;
                        currentPage_->stop();
                        currentPage_->freeGraphicsMemory();
                        currentPage_ = pages_.top();
                        pages_.pop();
                        delete gamePage;
                        currentPage_->allocateGraphicsMemory(false);
                    }
                    config_.setProperty("currentCollection", "SearchResults");
                    preSearchMenuDepth_ = currentPage_->getMenuDepth();
                    currentPage_->pushSearchCollection(searchPageCollection_, searchJumpIndex_ >= 0 ? static_cast<unsigned int>(searchJumpIndex_) : 0);
                    searchJumpIndex_ = -1;
                    if (searchTargetItem_)
                    {
                        delete searchTargetItem_;
                        searchTargetItem_ = NULL;
                    }
                }
                else
                {
                    config_.setProperty("currentCollection", nextPageName);

                    CollectionInfo* info;
                    if (menuMode_)
                        info = getMenuCollection(nextPageName);
                    else
                        info = getCollection(nextPageName);

                    currentPage_->pushCollection(info);

                    bool rememberMenu = false;
                    config_.getProperty("rememberMenu", rememberMenu);

                    std::string autoPlaylist = "all";
                    // Check for collection-specific autoPlaylist first
                    std::string collectionAutoPlaylistKey = "collections." + currentPage_->getCollectionName() + ".list.autoPlaylist";
                    if (!config_.getProperty(collectionAutoPlaylistKey, autoPlaylist))
                    {
                        // Fall back to global autoPlaylist if collection-specific one isn’t set
                        config_.getProperty("autoPlaylist", autoPlaylist);
                    }

                    if (rememberMenu && lastMenuPlaylists_.find(nextPageName) != lastMenuPlaylists_.end())
                    {
                        currentPage_->selectPlaylist(lastMenuPlaylists_[nextPageName]); // Switch to last playlist
                    }
                    else
                    {
                        currentPage_->selectPlaylist(autoPlaylist);
                        if (currentPage_->getPlaylistName() != autoPlaylist)
                            currentPage_->selectPlaylist("all");
                    }

                    if (rememberMenu && lastMenuOffsets_.find(nextPageName) != lastMenuOffsets_.end())
                    {
                        currentPage_->setScrollOffsetIndex(lastMenuOffsets_[nextPageName]);
                    }
                }

                currentPage_->onNewItemSelected();
                currentPage_->reallocateMenuSpritePoints();

                state = RETROFE_NEXT_PAGE_MENU_LOAD_ART;

                // Check if we've entered an empty collection and need to go back automatically
                if (currentPage_->getCollectionSize() == 0)
                {
                    bool backOnEmpty = false;
                    config_.getProperty("backOnEmpty", backOnEmpty);
                    if (backOnEmpty)
                        state = RETROFE_BACK_MENU_EXIT;
                }

                isInCriticalState_ = false;
            }
            break;

            // Start onMenuEnter animation
        case RETROFE_NEXT_PAGE_MENU_LOAD_ART:
            if (currentPage_->getMenuDepth() != 1)
            {
                currentPage_->enterMenu();
            }
            else
            {
                currentPage_->start();
            }
            if (isAutoNavigating_ && currentPage_)
            {
                currentPage_->completeAnimations();
            }
            if (currentPage_->getSelectedItem())
                l.LEDBlinky(9, currentPage_->getSelectedItem()->collectionInfo->name, currentPage_->getSelectedItem());
            state = RETROFE_NEXT_PAGE_MENU_ENTER;
            break;

            // Wait for onMenuEnter animation to finish
        case RETROFE_NEXT_PAGE_MENU_ENTER:
            if (currentPage_->isIdle())
            {
                inputClear = false;
                config_.getProperty("collectionInputClear", inputClear);
                if (inputClear)
                {
                    // Empty event queue
                    SDL_Event e;
                    while (SDL_PollEvent(&e))
                        input_.update(e);
                    input_.resetStates();
                }
                enterNumberOfTimes_ = 0;
                state = RETROFE_IDLE;
            }
            break;

            // Start exit animation
        case RETROFE_COLLECTION_DOWN_REQUEST:
            if (!pages_.empty() && currentPage_->getMenuDepth() == 1) // Inside a collection with a different layout
            {
                currentPage_->stop();
                m.clearPage();
                menuMode_ = false;
                state = RETROFE_COLLECTION_DOWN_EXIT;
            }
            else if (currentPage_->getMenuDepth() > 1) // Inside a collection with the same layout
            {
                currentPage_->exitMenu();
                state = RETROFE_COLLECTION_DOWN_EXIT;
            }
            else // Not in a collection
            {
                state = RETROFE_COLLECTION_DOWN_ENTER;

                if (attractMode_) // Check playlist change in attract mode
                {
                    attractModePlaylistCollectionNumber_ += 1;
                    int attractModePlaylistCollectionNumber = 0;
                    config_.getProperty("attractModePlaylistCollectionNumber", attractModePlaylistCollectionNumber);
                    // Check if playlist should be changed
                    if (attractModePlaylistCollectionNumber_ > 0 && attractModePlaylistCollectionNumber_ >= attractModePlaylistCollectionNumber)
                    {
                        attractModePlaylistCollectionNumber_ = 0;
                        currentPage_->nextPlaylist();
                        std::string attractModeSkipPlaylist = "";
                        config_.getProperty("attractModeSkipPlaylist", attractModeSkipPlaylist);
                        if (currentPage_->getPlaylistName() == attractModeSkipPlaylist)
                            currentPage_->nextPlaylist();
                        state = RETROFE_PLAYLIST_REQUEST;
                    }
                }
            }
            break;

            // Wait for the menu exit animation to finish
        case RETROFE_COLLECTION_DOWN_EXIT:
            if (currentPage_->isIdle())
            {
                lastMenuOffsets_[currentPage_->getCollectionName()] = currentPage_->getScrollOffsetIndex();
                lastMenuPlaylists_[currentPage_->getCollectionName()] = currentPage_->getPlaylistName();
                if (currentPage_->getMenuDepth() == 1) // Inside a collection with a different layout
                {
                    currentPage_->deInitialize();
                    delete currentPage_;
                    currentPage_ = pages_.top();
                    pages_.pop();
                    if (isAutoNavigating_ && currentPage_) currentPage_->setAutoNavigating(true);
                    currentPage_->allocateGraphicsMemory();
                }
                else // Inside a collection with the same layout
                {
                    currentPage_->popCollection();
                }
                config_.setProperty("currentCollection", currentPage_->getCollectionName());

                bool rememberMenu = false;
                config_.getProperty("rememberMenu", rememberMenu);

                std::string autoPlaylist = "all";
                // Check for collection-specific autoPlaylist first
                std::string collectionAutoPlaylistKey = "collections." + currentPage_->getCollectionName() + ".list.autoPlaylist";
                if (!config_.getProperty(collectionAutoPlaylistKey, autoPlaylist))
                {
                    // Fall back to global autoPlaylist if collection-specific one isn’t set
                    config_.getProperty("autoPlaylist", autoPlaylist);
                }

                if (rememberMenu && lastMenuPlaylists_.find(currentPage_->getCollectionName()) != lastMenuPlaylists_.end())
                {
                    currentPage_->selectPlaylist(lastMenuPlaylists_[currentPage_->getCollectionName()]); // Switch to last playlist
                }
                else
                {
                    currentPage_->selectPlaylist(autoPlaylist);
                    if (currentPage_->getPlaylistName() != autoPlaylist)
                        currentPage_->selectPlaylist("all");
                }

                if (rememberMenu && lastMenuOffsets_.find(currentPage_->getCollectionName()) != lastMenuOffsets_.end())
                {
                    currentPage_->setScrollOffsetIndex(lastMenuOffsets_[currentPage_->getCollectionName()]);
                }

                state = RETROFE_COLLECTION_DOWN_MENU_ENTER;
                currentPage_->onNewItemSelected();

                if (attractMode_) // Check playlist change in attract mode
                {
                    attractModePlaylistCollectionNumber_ += 1;
                    int attractModePlaylistCollectionNumber = 0;
                    config_.getProperty("attractModePlaylistCollectionNumber", attractModePlaylistCollectionNumber);
                    // Check if playlist should be changed
                    if (attractModePlaylistCollectionNumber_ > 0 && attractModePlaylistCollectionNumber_ >= attractModePlaylistCollectionNumber)
                    {
                        attractModePlaylistCollectionNumber_ = 0;
                        currentPage_->nextPlaylist();
                        std::string attractModeSkipPlaylist = "";
                        config_.getProperty("attractModeSkipPlaylist", attractModeSkipPlaylist);
                        if (currentPage_->getPlaylistName() == attractModeSkipPlaylist)
                            currentPage_->nextPlaylist();
                        state = RETROFE_PLAYLIST_REQUEST;
                    }
                }

            }
            break;


            // Start menu enter animation
        case RETROFE_COLLECTION_DOWN_MENU_ENTER:
            currentPage_->enterMenu();
            state = RETROFE_COLLECTION_DOWN_ENTER;
            break;


            // Waiting for enter animation to stop
        case RETROFE_COLLECTION_DOWN_ENTER:
            if (currentPage_->isIdle())
            {
                int attractModePlaylistCollectionNumber = 0;
                config_.getProperty("attractModePlaylistCollectionNumber", attractModePlaylistCollectionNumber);
                if (!(attractMode_ && attractModePlaylistCollectionNumber > 0 && attractModePlaylistCollectionNumber_ == 0))
                {
                    currentPage_->setScrolling(Page::ScrollDirectionForward);
                    currentPage_->scroll(true);
                    currentPage_->updateScrollPeriod();
                }
                state = RETROFE_COLLECTION_DOWN_SCROLL;
            }
            break;

            // Waiting for scrolling animation to stop
        case RETROFE_COLLECTION_DOWN_SCROLL:
            if (currentPage_->isMenuIdle())
            {
                std::string attractModeSkipCollection = "";
                config_.getProperty("attractModeSkipCollection", attractModeSkipCollection);
                // Check if we need to skip this collection in attract mode or if we can select it
                if (attractMode_ && currentPage_->getSelectedItem()->name == attractModeSkipCollection)
                {
                    currentPage_->setScrolling(Page::ScrollDirectionForward);
                    currentPage_->scroll(true);
                    currentPage_->updateScrollPeriod();
                }
                else
                {
                    RETROFE_STATE state_tmp = processUserInput(currentPage_);
                    if (state_tmp == RETROFE_COLLECTION_DOWN_REQUEST)
                    {
                        state = RETROFE_COLLECTION_DOWN_REQUEST;
                    }
                    else if (state_tmp == RETROFE_COLLECTION_UP_REQUEST)
                    {
                        state = RETROFE_COLLECTION_UP_REQUEST;
                    }
                    else
                    {
                        currentPage_->setScrolling(Page::ScrollDirectionIdle); // Stop scrolling
                        nextPageItem_ = currentPage_->getSelectedItem();
                        bool enterOnCollection = true;
                        config_.getProperty("enterOnCollection", enterOnCollection);
                        if (!nextPageItem_ || nextPageItem_->leaf || (!attractMode_ && !enterOnCollection)) // Current selection is a game or enterOnCollection is not set
                        {
                            state = RETROFE_HIGHLIGHT_REQUEST;
                        }
                        else // Current selection is a menu
                        {
                            state = RETROFE_COLLECTION_HIGHLIGHT_EXIT;
                        }
                    }
                }
            }
            break;


            // Start onHighlightExit animation
        case RETROFE_COLLECTION_HIGHLIGHT_REQUEST:
            currentPage_->highlightExit();
            state = RETROFE_COLLECTION_HIGHLIGHT_EXIT;
            break;

            // Wait for onHighlightExit animation to finish; load art
        case RETROFE_COLLECTION_HIGHLIGHT_EXIT:
            if (currentPage_->isIdle())
            {
                currentPage_->highlightLoadArt();
                state = RETROFE_COLLECTION_HIGHLIGHT_LOAD_ART;
            }
            break;

            // Start onHighlightEnter animation
        case RETROFE_COLLECTION_HIGHLIGHT_LOAD_ART:
            currentPage_->highlightEnter();
            if (currentPage_->getSelectedItem())
                l.LEDBlinky(9, currentPage_->getSelectedItem()->collectionInfo->name, currentPage_->getSelectedItem());
            state = RETROFE_COLLECTION_HIGHLIGHT_ENTER;
            break;

            // Wait for onHighlightEnter animation to finish
        case RETROFE_COLLECTION_HIGHLIGHT_ENTER:
            if (currentPage_->isIdle())
            {
                RETROFE_STATE state_tmp = processUserInput(currentPage_);
                if (state_tmp == RETROFE_COLLECTION_DOWN_REQUEST)
                {
                    state = RETROFE_COLLECTION_DOWN_REQUEST;
                }
                else if (state_tmp == RETROFE_COLLECTION_UP_REQUEST)
                {
                    state = RETROFE_COLLECTION_UP_REQUEST;
                }
                else
                {
                    if (nextPageItem_)
                        state = RETROFE_NEXT_PAGE_REQUEST;
                    else
                        state = RETROFE_IDLE;
                }
            }
            break;

            // Start exit animation
        case RETROFE_COLLECTION_UP_REQUEST:
            if (!pages_.empty() && currentPage_->getMenuDepth() == 1) // Inside a collection with a different layout
            {
                currentPage_->stop();
                m.clearPage();
                menuMode_ = false;
                state = RETROFE_COLLECTION_UP_EXIT;
            }
            else if (currentPage_->getMenuDepth() > 1) // Inside a collection with the same layout
            {
                currentPage_->exitMenu();
                state = RETROFE_COLLECTION_UP_EXIT;
            }
            else // Not in a collection
            {
                state = RETROFE_COLLECTION_UP_ENTER;
            }
            break;

            // Wait for the menu exit animation to finish
        case RETROFE_COLLECTION_UP_EXIT:
            if (currentPage_->isIdle())
            {
                lastMenuOffsets_[currentPage_->getCollectionName()] = currentPage_->getScrollOffsetIndex();
                lastMenuPlaylists_[currentPage_->getCollectionName()] = currentPage_->getPlaylistName();
                if (currentPage_->getMenuDepth() == 1) // Inside a collection with a different layout
                {
                    currentPage_->deInitialize();
                    delete currentPage_;
                    currentPage_ = pages_.top();
                    pages_.pop();
                    currentPage_->allocateGraphicsMemory();
                }
                else // Inside a collection with the same layout
                {
                    currentPage_->popCollection();
                }
                config_.setProperty("currentCollection", currentPage_->getCollectionName());

                bool rememberMenu = false;
                config_.getProperty("rememberMenu", rememberMenu);

                std::string autoPlaylist = "all";
                // Check for collection-specific autoPlaylist first
                std::string collectionAutoPlaylistKey = "collections." + currentPage_->getCollectionName() + ".list.autoPlaylist";
                if (!config_.getProperty(collectionAutoPlaylistKey, autoPlaylist))
                {
                    // Fall back to global autoPlaylist if collection-specific one isn’t set
                    config_.getProperty("autoPlaylist", autoPlaylist);
                }

                if (rememberMenu && lastMenuPlaylists_.find(currentPage_->getCollectionName()) != lastMenuPlaylists_.end())
                {
                    currentPage_->selectPlaylist(lastMenuPlaylists_[currentPage_->getCollectionName()]); // Switch to last playlist
                }
                else
                {
                    currentPage_->selectPlaylist(autoPlaylist);
                    if (currentPage_->getPlaylistName() != autoPlaylist)
                        currentPage_->selectPlaylist("all");
                }

                if (rememberMenu && lastMenuOffsets_.find(currentPage_->getCollectionName()) != lastMenuOffsets_.end())
                {
                    currentPage_->setScrollOffsetIndex(lastMenuOffsets_[currentPage_->getCollectionName()]);
                }

                currentPage_->onNewItemSelected();
                state = RETROFE_COLLECTION_UP_MENU_ENTER;
            }
            break;


            // Start menu enter animation
        case RETROFE_COLLECTION_UP_MENU_ENTER:
            currentPage_->enterMenu();
            state = RETROFE_COLLECTION_UP_ENTER;
            break;


            // Waiting for enter animation to stop
        case RETROFE_COLLECTION_UP_ENTER:
            if (currentPage_->isIdle())
            {
                currentPage_->setScrolling(Page::ScrollDirectionBack);
                currentPage_->scroll(false);
                currentPage_->updateScrollPeriod();
                state = RETROFE_COLLECTION_UP_SCROLL;
            }
            break;

            // Waiting for scrolling animation to stop
        case RETROFE_COLLECTION_UP_SCROLL:
            if (currentPage_->isMenuIdle())
            {
                RETROFE_STATE state_tmp;
                state_tmp = processUserInput(currentPage_);
                if (state_tmp == RETROFE_COLLECTION_DOWN_REQUEST)
                {
                    state = RETROFE_COLLECTION_DOWN_REQUEST;
                }
                else if (state_tmp == RETROFE_COLLECTION_UP_REQUEST)
                {
                    state = RETROFE_COLLECTION_UP_REQUEST;
                }
                else
                {
                    currentPage_->setScrolling(Page::ScrollDirectionIdle); // Stop scrolling
                    nextPageItem_ = currentPage_->getSelectedItem();
                    bool enterOnCollection = true;
                    config_.getProperty("enterOnCollection", enterOnCollection);
                    if (currentPage_->getSelectedItem()->leaf || !enterOnCollection) // Current selection is a game or enterOnCollection is not set
                    {
                        state = RETROFE_HIGHLIGHT_REQUEST;
                    }
                    else // Current selection is a menu
                    {
                        state = RETROFE_COLLECTION_HIGHLIGHT_EXIT;
                    }
                }
            }
            break;


            // Launching a menu entry
        case RETROFE_HANDLE_MENUENTRY:

            // Empty event queue
            SDL_Event e;
            while (SDL_PollEvent(&e))
                input_.update(e);
            input_.resetStates();

            // Handle menu entry
            m.handleEntry(currentPage_->getSelectedItem());

            // Empty event queue
            while (SDL_PollEvent(&e))
                input_.update(e);
            input_.resetStates();

            state = RETROFE_IDLE;
            break;

            // Launching game; start onGameEnter animation
        case RETROFE_LAUNCH_ENTER:
            if (!searchLaunchItem_)
            {
                currentPage_->enterGame();  // Start onGameEnter animation
            }
            currentPage_->playSelect(); // Play launch sound
            state = RETROFE_LAUNCH_REQUEST;
            break;

            // Wait for onGameEnter animation to finish; launch game; start onGameExit animation
        case RETROFE_LAUNCH_REQUEST:
            if (currentPage_->isIdle() && !currentPage_->isSelectPlaying())
            {
                nextPageItem_ = searchLaunchItem_ ? searchLaunchItem_ : currentPage_->getSelectedItem();
                if (!nextPageItem_)
                {
                    state = RETROFE_IDLE;
                    break;
                }
                bool hasLayer20Background = false;
                if (currentPage_)
                {
                    hasLayer20Background = currentPage_->hasComponentOnLayer(20);
                }

                if (!hasLayer20Background)
                {
                    SDL_Renderer* renderer = SDL::getRenderer(0);
                    if (renderer)
                    {
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                        SDL_RenderClear(renderer);
                        SDL_RenderPresent(renderer);
                        Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Screen cleared to black (no layer 20 background found)");
                    }
                }
                else
                {
                    Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Layer 20 background detected - keeping image visible (no black clear)");
                }
                launchEnter();
                CollectionInfoBuilder cib(config_, *metadb_);
                std::string attractModeSkipPlaylist = "";
                std::string lastPlayedSkipCollection = "";
                int size = 0;
                config_.getProperty("attractModeSkipPlaylist", attractModeSkipPlaylist);
                config_.getProperty("lastPlayedSkipCollection", lastPlayedSkipCollection);
                config_.getProperty("lastplayedSize", size);
                CollectionInfo* targetCollection = currentPage_->getCollection();
                if (targetCollection->name == "TempGameCollection" && targetCollection->originalCollection)
                {
                    targetCollection = targetCollection->originalCollection;
                }
                if (targetCollection->name == "SearchResults" && nextPageItem_->collectionInfo)
                {
                    targetCollection = nextPageItem_->collectionInfo;
                }
                if (currentPage_->getPlaylistName() != attractModeSkipPlaylist &&
                    nextPageItem_->collectionInfo->name != lastPlayedSkipCollection)
                    cib.updateLastPlayedPlaylist(targetCollection, nextPageItem_, size);
       
                l.LEDBlinky(3, nextPageItem_->collectionInfo->name, nextPageItem_);
              
                bool enableSD = true;
                config_.getProperty("screenDuplicator", enableSD);

                // Only launch if enabled AND at least one marquee is actually running
                if (enableSD && SDL::hasActiveMarquee())
                {
#ifdef WIN32
                    // Kill old instances
                    for (HANDLE h : screenDuplicatorHandles)
                    {
                        if (h != NULL)
                        {
                            WaitForSingleObject(h, 1000);
                            TerminateProcess(h, 0);
                            CloseHandle(h);
                        }
                    }
                    screenDuplicatorHandles.clear();
#endif

                    screenDuplicatorHandles = l.ScreenDuplicatorWithHandle(nextPageItem_, l.getLastRomPath());
                    Logger::write(Logger::ZONE_INFO, "ScreenDuplicator",
                        "Launched " + std::to_string(screenDuplicatorHandles.size()) +
                        " instance(s) for " + nextPageItem_->name);
                }
                else
                {
                    // Clean up any old handles
                    if (!screenDuplicatorHandles.empty())
                    {
                        for (HANDLE h : screenDuplicatorHandles)
                        {
                            if (h != NULL)
                            {
                                TerminateProcess(h, 0);
                                CloseHandle(h);
                            }
                        }
                        screenDuplicatorHandles.clear();
                    }

                    // Log why it was skipped
                    if (!enableSD)
                        Logger::write(Logger::ZONE_INFO, "ScreenDuplicator", "Disabled in settings");
                    else
                        Logger::write(Logger::ZONE_INFO, "ScreenDuplicator", "No active marquee screens – not launching");
                }

                bool isFromSearch = (searchLaunchItem_ != NULL);
                bool launched = l.run(nextPageItem_->collectionInfo->name, nextPageItem_);
                searchLaunchItem_ = NULL;
                searchFadeAlpha_  = 0;

                if (launched)
                {
                    attract_.reset();
                    reboot_ = true;
                    state = RETROFE_QUIT_REQUEST;
                }
                else
                {
                    launchExit();

                    // KILL ALL DUPLICATOR INSTANCES IF GAME FAILED
                    if (!screenDuplicatorHandles.empty())
                    {
                        for (HANDLE h : screenDuplicatorHandles)
                        {
                            if (h != NULL)
                            {
                                TerminateProcess(h, 0);
                                CloseHandle(h);
                            }
                        }
                        screenDuplicatorHandles.clear();
                        Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "ScreenDuplicator killed (game failed)");
                    }


                    // Calculate play duration
                    float launchEndTime = static_cast<float>(SDL_GetTicks()) / 1000.0f;
                    float playDurationSeconds = launchEndTime - launchStartTime_;
                    int playDurationMinutes = static_cast<int>(playDurationSeconds / 60.0f);
                    if (cib_)
                    {
                        CollectionInfo* statsCollection = targetCollection;
                        if (statsCollection && statsCollection->name == "SearchResults" && nextPageItem_->collectionInfo)
                        {
                            statsCollection = nextPageItem_->collectionInfo;
                        }
                        cib_->saveGameStats(statsCollection, nextPageItem_, playDurationMinutes);
                        cib_->saveTopGameStats(statsCollection);
                        // RetroRa call — only if RetroRa = true in settings.conf
                        bool useRetroRa = false;
                        config_.getProperty("retroachievements", useRetroRa);

                        if (useRetroRa && playDurationSeconds >= 60.0f)
                        {
                      
                            Item* item = currentPage_->getSelectedItem();
                            if (item)
                            {
                                std::string gameName = item->name;
                                std::string systemName = item->collectionInfo->name;

                                // Basic escaping — replace " with \"
                                gameName = Utils::replace(gameName, "\"", "\\\"");
                                systemName = Utils::replace(systemName, "\"", "\\\"");

                                // Build command (adjust path if needed)
                                std::string exe = "RetroRa.exe";                         // or full path: "C:\\Tools\\RetroRa.exe"
                                std::string args = "-- \"" + gameName + "\" -- \"" + systemName + "\"";

                                std::string cmd = "\"" + exe + "\" " + args;
                                std::string workDir = Configuration::absolutePath;       // or where RetroRa.exe lives

                                Logger::write(Logger::ZONE_DEBUG, "RetroRa",
                                    "Running: " + cmd);
                                Logger::write(Logger::ZONE_INFO, "RetroRa",
                                    "Sending \"" + item->name + "\" -- \"" + item->collectionInfo->name + "\"");
                                // Use the existing execute() method — fire and forget (no wait)
                                Sleep(1000); // Small delay to ensure RetroRa is ready to receive the message
                                Launcher l(config_, this);
                                l.launchRetroRa(item->name, item->collectionInfo->name, true);
                            }
                            else
                            {
                                Logger::write(Logger::ZONE_WARNING, "RetroRa",
                                    "No selected item after game exit — cannot run RetroRa");
                            }
                        }
                        // Determine the collection to reload stats from
                        std::string collectionName = getCorrectCollectionName(nextPageItem_, targetCollection);
                    
                        // Reload stats for the launched game after exit game
                        std::string statsPath = Utils::combinePath(Configuration::absolutePath, "collections", collectionName, "info", nextPageItem_->name + ".conf");
                        Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC", "Reloading stats for " + nextPageItem_->name + " from " + statsPath);
                        try
                        {
                            nextPageItem_->info_.clear();
                            nextPageItem_->loadInfo(statsPath);
                            std::string value;
                            if (nextPageItem_->getInfo("playCount", value))
                                Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC", "Reloaded playCount=" + value + " for " + nextPageItem_->name);
                            if (nextPageItem_->getInfo("totalPlayTime", value))
                                Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC", "Reloaded totalPlayTime=" + value + " for " + nextPageItem_->name);
                            if (nextPageItem_->getInfo("lastPlayedDate", value))
                                Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC", "Reloaded lastPlayedDate=" + value + " for " + nextPageItem_->name);
                            if (nextPageItem_->getInfo("lastPlayDuration", value))
                                Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC", "Reloaded lastPlayDuration=" + value + " for " + nextPageItem_->name);
                            if (nextPageItem_->getInfo("isRa", value))
								Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC", "Reloaded isRa=" + value + " for " + nextPageItem_->name);
                        }
                        catch (const std::exception& e)
                        {
                            Logger::write(Logger::ZONE_ERROR, "RetroFeVLC", "Exception while reloading stats for " + nextPageItem_->name + " from " + statsPath + ": " + e.what());
                        }
                    }
                    l.LEDBlinky(4);
                    if (!isFromSearch)
                    {
                        currentPage_->exitGame();
                    }
                    // Trigger UI refresh
                    currentPage_->onNewItemSelected();
                    // Force reload of all reloadable components
                    currentPage_->restart();
                    // Avoid saving scroll offset for LASTPLAYED
                    if (currentPage_->getCollectionName() != "LASTPLAYED") {
                        lastMenuOffsets_[currentPage_->getCollectionName()] = currentPage_->getScrollOffsetIndex();
                        lastMenuPlaylists_[currentPage_->getCollectionName()] = currentPage_->getPlaylistName();
                    }
                    state = RETROFE_LAUNCH_EXIT;
                }
            }
            break;

            // Wait for onGameExit animation to finish
      case RETROFE_LAUNCH_EXIT:
          if (currentPage_->isIdle())
          {

             

              if (currentPage_->getCollectionName() == "LASTPLAYED") {
                  // Reset to first item in LASTPLAYED collection
                  currentPage_->setScrollOffsetIndex(0);
                  std::vector<Item*>* playlist = currentPage_->getCurrentPlaylist();
                  if (playlist && !playlist->empty()) {
                      currentPage_->selectItem(playlist->front());
                      currentPage_->onNewItemSelected();
                      currentPage_->reallocateMenuSpritePoints();
                      currentPage_->highlightEnter();
                      Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Selected first item in LASTPLAYED collection after game launch");
                  }
                  else {
                      Logger::write(Logger::ZONE_WARNING, "RetroFeVLC", "LASTPLAYED playlist is empty after game launch");
                  }
              }
              state = RETROFE_IDLE;
          }
          break;

            // Go back a page; start onMenuExit animation
        case RETROFE_BACK_REQUEST:
            if (!currentPage_->isSearchActive() && currentPage_->getMenuDepth() == 1)
            {
                currentPage_->stop();
                m.clearPage();
                menuMode_ = false;
            }
            else
            {
                currentPage_->exitMenu();
            }
            if (isAutoNavigating_ && currentPage_)
            {
                currentPage_->completeAnimations();
            }
            state = RETROFE_BACK_MENU_EXIT;
            break;

            // Wait for onMenuExit animation to finish; load previous page; load art
        case RETROFE_BACK_MENU_EXIT:
            if (currentPage_->isIdle())
            {
                lastMenuOffsets_[currentPage_->getCollectionName()] = currentPage_->getScrollOffsetIndex();
                lastMenuPlaylists_[currentPage_->getCollectionName()] = currentPage_->getPlaylistName();
                unsigned int savedScroll = static_cast<unsigned int>(-1);
                if (currentPage_->isSearchActive())
                {
                    unsigned int targetDepth = preSearchMenuDepth_;
                    preSearchMenuDepth_ = 0;
                    std::string savedItem = currentPage_->getSearchSavedRootItem();
                    savedScroll = currentPage_->getSearchSavedRootScroll();

                    currentPage_->freeGraphicsMemory();
                    currentPage_->popSearchCollection(targetDepth);

                    if (searchPageCollection_)
                    {
                        searchPageCollection_->playlists.clear();
                        searchPageCollection_->ownsItems = false;
                        delete searchPageCollection_;
                        searchPageCollection_ = NULL;
                    }

                    currentPage_->allocateGraphicsMemory(false);
                    currentPage_->restart();

                    if (savedScroll != static_cast<unsigned int>(-1))
                    {
                        lastMenuOffsets_[currentPage_->getCollectionName()] = savedScroll;
                    }
                }
                else if (currentPage_->getMenuDepth() == 1)
                {
                    currentPage_->deInitialize();
                    delete currentPage_;
                    currentPage_ = pages_.top();
                    pages_.pop();
                    currentPage_->allocateGraphicsMemory();
                }
                else
                {
                    currentPage_->popCollection();
                }
                config_.setProperty("currentCollection", currentPage_->getCollectionName());

                bool rememberMenu = false;
                config_.getProperty("rememberMenu", rememberMenu);

                std::string autoPlaylist = "all";
                // Check for collection-specific autoPlaylist first
                std::string collectionAutoPlaylistKey = "collections." + currentPage_->getCollectionName() + ".list.autoPlaylist";
                if (!config_.getProperty(collectionAutoPlaylistKey, autoPlaylist))
                {
                    // Fall back to global autoPlaylist if collection-specific one isn’t set
                    config_.getProperty("autoPlaylist", autoPlaylist);
                }

                if (rememberMenu && lastMenuPlaylists_.find(currentPage_->getCollectionName()) != lastMenuPlaylists_.end())
                {
                    currentPage_->selectPlaylist(lastMenuPlaylists_[currentPage_->getCollectionName()]); // Switch to last playlist
                }
                else
                {
                    currentPage_->selectPlaylist(autoPlaylist);
                    if (currentPage_->getPlaylistName() != autoPlaylist)
                        currentPage_->selectPlaylist("all");
                }

                if (rememberMenu && lastMenuOffsets_.find(currentPage_->getCollectionName()) != lastMenuOffsets_.end())
                {
                    currentPage_->setScrollOffsetIndex(lastMenuOffsets_[currentPage_->getCollectionName()]);
                }
                else if (savedScroll != static_cast<unsigned int>(-1))
                {
                    currentPage_->setScrollOffsetIndex(savedScroll);
                }

                currentPage_->onNewItemSelected();
                currentPage_->reallocateMenuSpritePoints();
                state = RETROFE_BACK_MENU_LOAD_ART;
            }
            break;

            // Start onMenuEnter animation
        case RETROFE_BACK_MENU_LOAD_ART:
            currentPage_->enterMenu();
            if (isAutoNavigating_ && currentPage_)
            {
                currentPage_->completeAnimations();
                        }
            state = RETROFE_BACK_MENU_ENTER;
            break;

            // Wait for onMenuEnter animation to finish
        case RETROFE_BACK_MENU_ENTER:
                    if (currentPage_->isIdle())
            {
                // Empty event queue and reset input states so Back doesn't leak
                SDL_Event e;
                while (SDL_PollEvent(&e))
                    input_.update(e);
                input_.resetStates();
                state = RETROFE_IDLE;
            }
            break;

            // Start menu mode
        case RETROFE_MENUMODE_START_REQUEST:
            if (currentPage_->isIdle())
            {
                lastMenuOffsets_[currentPage_->getCollectionName()] = currentPage_->getScrollOffsetIndex();
                lastMenuPlaylists_[currentPage_->getCollectionName()] = currentPage_->getPlaylistName();
                std::string layoutName;
                config_.getProperty("layout", layoutName);
                PageBuilder pb(layoutName, "layout", config_, &fontcache_, true);
                Page* page = pb.buildPage();
                if (page)
                {
                    currentPage_->freeGraphicsMemory();
                    pages_.push(currentPage_);
                    currentPage_ = page;
                    menuMode_ = true;
                    m.setPage(page);
                }
                config_.setProperty("currentCollection", "menu");
                CollectionInfo* info = getMenuCollection("menu");
                currentPage_->pushCollection(info);
                currentPage_->onNewItemSelected();
                currentPage_->reallocateMenuSpritePoints();
                state = RETROFE_MENUMODE_START_LOAD_ART;
            }
            break;

        case RETROFE_MENUMODE_START_LOAD_ART:
            currentPage_->start();
            state = RETROFE_MENUMODE_START_ENTER;
            break;

        case RETROFE_MENUMODE_START_ENTER:
            if (currentPage_->isIdle())
            {
                SDL_Event e;
                while (SDL_PollEvent(&e))
                    input_.update(e);
                input_.resetStates();
                state = RETROFE_IDLE;
            }
            break;

            // Wait for splash mode animation to finish
        case RETROFE_NEW:
            if (currentPage_->isIdle())
            {
                state = RETROFE_IDLE;
            }
            break;

        //Reload Layout
            case RETROFE_RELOAD_REQUEST:
           
                if (currentPage_->getCollectionName() == "TempGameCollection") 
                {
                    state = RETROFE_GAME_PAGE_EXIT; // Trigger exit from game page
                }
                else {
                    isReloading_ = true;
                    renderLoadingScreen();
                    reloadLayout();
                    input_.resetStates();
                    state = RETROFE_IDLE;
                }
			break;

        //Option Menu (handled cleanly via runOptionsMenu, exactly like runSearch)
            case RETROFE_CONFIGMENU_REQUEST:
                if (currentPage_->isIdle()) {
                    Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Opening options menu overlay");
                    state = runOptionsMenu(currentPage_);
                }
                break;

            case RETROFE_CONFIGMENU_ENTER:
            case RETROFE_CONFIGMENU_EXIT:
                state = RETROFE_IDLE;
                break;

// Collection sorting

// Handle next sort request
case RETROFE_NEXT_SORT_REQUEST:
    if (currentPage_ && currentPage_->getCollection()) {
        CollectionInfo* coll = currentPage_->getCollection();
        bool up = sortDirectionUp_;
         /*if (coll) {
            coll->cycleSort(sortDirectionUp_);
            currentPage_->setScrollOffsetIndex(0);
        }*/
        coll->cycleSort(up);

        // Refresh UI
        ScrollingList* menu = currentPage_->getActiveMenu();
        if (menu) {
            menu->freeGraphicsMemory();
            menu->destroyItems();
            menu->setItems(&coll->items);
            menu->allocateSpritePoints();
            menu->allocateGraphicsMemory();
        }
        currentPage_->setScrollOffsetIndex(0);
        currentPage_->onNewItemSelected();
        currentPage_->reallocateMenuSpritePoints();
        currentPage_->highlightEnter();
    }
    state = RETROFE_IDLE;
    break;

    // Sort Exit
case RETROFE_NEXT_SORT_EXIT:
    if (currentPage_->isIdle()) {
        if (currentPage_->getCollection()) {                    // <-- use getCollection()
            CollectionInfo* coll = currentPage_->getCollection();
            if (coll) {
                coll->cycleSort(sortDirectionUp_);
                currentPage_->setScrollOffsetIndex(0);
            }
            currentPage_->getCollection()->cycleSort(sortDirectionUp_);
         
            currentPage_->setScrollOffsetIndex(0);
        }
        currentPage_->onNewItemSelected();
        state = RETROFE_PLAYLIST_LOAD_ART;
    }
    break;
// ─────────────────────────────────────────────────────────────────────
//  SEARCH REQUEST  –  decide whether we can open the search UI
// ─────────────────────────────────────────────────────────────────────
case RETROFE_SEARCH_REQUEST:
            if (currentPage_->isIdle()) {
                Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Opening search overlay");
                state = runSearch(currentPage_);
            }
            break;

        case RETROFE_SEARCH_ENTER:
        case RETROFE_SEARCH_EXIT:
            state = RETROFE_IDLE;
            break;

        case RETROFE_GAME_PAGE_ENTER:
            currentPage_->allocateGraphicsMemory();
            currentPage_->start();
            state = RETROFE_IDLE;
            break;

        case RETROFE_GAME_PAGE_EXIT:
            if (currentPage_->isIdle()) 
            {
                if (isJukebox_)
                {
                    // === JUKEBOX MODE - Full reload ===
                    Logger::write(Logger::ZONE_INFO, "RetroFeVLC",
                        "Exiting game layout forcing full reload + re-enter (Jukebox Mode)");

                    currentPage_->stop();
                    currentPage_->freeGraphicsMemory();

                    if (pages_.empty())
                    {
                        Logger::write(Logger::ZONE_ERROR, "RetroFeVLC", "No previous page!");
                        state = RETROFE_IDLE;
                        break;
                    }

                    currentPage_ = pages_.top();
                    pages_.pop();

                    std::string collName = currentPage_->getCollectionName();

                    // Save state for reloadLayout
                    lastMenuOffsets_[collName] = currentPage_->getScrollOffsetIndex();
                    lastMenuPlaylists_[collName] = currentPage_->getPlaylistName();

                    // Force full reload
                    isReload_ = true;
                    reloadLayout();

                    // After reloadLayout finishes, force the long animation again
                    if (currentPage_)
                    {
                        currentPage_->onNewItemSelected();
                        currentPage_->reallocateMenuSpritePoints();
                        currentPage_->start();
                    }

                    isReload_ = false;
                    Logger::write(Logger::ZONE_INFO, "RetroFeVLC",
                        "Game exit: reload + full re-enter sequence done (Jukebox Mode)");
                }
                else
                {
                    // === NORMAL MODE ===
                    currentPage_->stop();
                    currentPage_->freeGraphicsMemory(); // Free graphics memory

                    currentPage_ = pages_.top();
                    pages_.pop();

                    currentPage_->allocateGraphicsMemory();
                    // currentPage_->start();   // usually not needed here
                }

                state = RETROFE_IDLE;
            }
            break;

            // Start the onExit animation
        case RETROFE_QUIT_REQUEST:
            currentPage_->stop();
            state = RETROFE_QUIT;
            break;

            // Wait for onExit animation to finish before quitting RetroFE
        case RETROFE_QUIT:
            bool LEDBlinkyCloseOnExit = true;
            config_.getProperty("LEDBlinkyCloseOnExit", LEDBlinkyCloseOnExit);
            if (currentPage_->isGraphicsIdle())
            {
                if (LEDBlinkyCloseOnExit == true) {
                    l.LEDBlinky(2);
                    running = false;
                }
                else running = false;
            }
            break;
        }
        bool vSync = false;
        config_.getProperty("vSync", vSync);
        // Handle screen updates and attract mode
        if (running)
        {
            lastTime = currentTime_;
            currentTime_ = static_cast<float>(SDL_GetTicks()) / 1000;

            if (currentTime_ < lastTime)
            {
                currentTime_ = lastTime;
            }

            deltaTime = currentTime_ - lastTime;

            // dynamic layout check
            layoutCheckTimer_ += deltaTime;
            if (layoutCheckTimer_ >= layoutCheckInterval_)
            {
                checkAndReloadLayout();
                layoutCheckTimer_ = 0.0f;
            }

            double sleepTime;
            if (state == RETROFE_IDLE)
                sleepTime = fpsIdleTime - deltaTime * 1000;
            else
                sleepTime = fpsTime - deltaTime * 1000;
            if (isAutoNavigating_)
            {
                sleepTime = 0;
            }
            if (sleepTime > 0 && sleepTime < 1000 && !vSync && !isAutoNavigating_)
            {
                SDL_Delay(static_cast<unsigned int>(sleepTime));
            }

            if (currentPage_)
            {
                if (!splashMode)
                {
                    int attractReturn = attract_.update(deltaTime, *currentPage_);
                    if (attractReturn == 1) // Change playlist
                    {
                        attract_.reset(attract_.isSet());

                        bool cyclePlaylist = false;
                        config_.getProperty("attractModeCyclePlaylist", cyclePlaylist);

                        std::string cycleString;
                        config_.getProperty("cyclePlaylist", cycleString);
                        std::vector<std::string> cycleVector;
                        Utils::listToVector(cycleString, cycleVector, ',');

                        if (cyclePlaylist)
                            currentPage_->nextCyclePlaylist(cycleVector);
                        else
                            currentPage_->nextPlaylist();

                        std::string attractModeSkipPlaylist = "";
                        config_.getProperty("attractModeSkipPlaylist", attractModeSkipPlaylist);
                        if (currentPage_->getPlaylistName() == attractModeSkipPlaylist)
                        {
                            if (cyclePlaylist)
                                currentPage_->nextCyclePlaylist(cycleVector);
                            else
                                currentPage_->nextPlaylist();
                        }
                        state = RETROFE_PLAYLIST_REQUEST;
                    }
                    if (attractReturn == 2) // Change collection
                    {
                        attract_.reset(attract_.isSet());
                        state = RETROFE_COLLECTION_DOWN_REQUEST;
                    }
                }
                if (menuMode_)
                {
                    attract_.reset();
                }
                currentPage_->update(deltaTime);
                SDL_PumpEvents();
                input_.updateKeystate();
        // ────────────────────────────────────────────────────────────────
       // Lazy RetroRa update: trigger ~2 seconds after user stops on a leaf item
      // ────────────────────────────────────────────────────────────────
                bool useRetroRa = false;
                config_.getProperty("retroachievements", useRetroRa);

                if (currentPage_ && useRetroRa)
                {
                    Item* selected = currentPage_->getSelectedItem();
                    if (selected && selected->leaf)
                    {
                        std::string currentGame = selected->name;
                        std::string currentSystem = selected->collectionInfo->name;  //system/sub file
                        Uint32 now = SDL_GetTicks();

                        // Only skip RetroRa logic when in the special collection
                        bool skipRetroRa = (currentPage_->getCollectionName() == "RetroAchievements Collection");

                        if (skipRetroRa)
                        {
                            if (!loggedRaCollectionSkip_)
                            {
#ifdef _DEBUG
                                Logger::write(Logger::ZONE_INFO, "RetroRa",
                                    "SKIP: inside RetroAchievements Collection no RetroRa processing (logged once)");
#endif
                                loggedRaCollectionSkip_ = true;
                            }

                            if (!lastRetroRaGame_.empty())
                            {
                                lastRetroRaGame_.clear();
                                lastRetroRaSystem_.clear();
                                lastRetroRaTriggered_ = false;
                                pendingRetroRaReload_ = false;
                            }
                            
                        }
                        else
                        {
                            // Normal collections full RetroRa + add logic with 2s delay

                            bool selectionChanged = (currentGame != lastRetroRaGame_ || currentSystem != lastRetroRaSystem_);
                            if (selectionChanged)
                            {
                                lastRetroRaSelectionTime_ = now;
                                lastRetroRaTriggered_ = false;
                                lastRetroRaGame_ = currentGame;
                                lastRetroRaSystem_ = currentSystem;
                                pendingRetroRaReload_ = false;
                            }

                            std::string value;
                            bool hasRa = selected->getInfo("0", value) && value == "isRA";

                            std::string collectionName = getCorrectCollectionName(selected, currentPage_->getCollection());

                            bool useRaCollection = false;
                            config_.getProperty("retroachievementsCollection", useRaCollection);

                            // Only run after 2 seconds
                            if (!lastRetroRaTriggered_ &&
                                now - lastRetroRaSelectionTime_ >= 2000)
                            {
                                lastRetroRaTriggered_ = true;

                                // Add if already has RA
                                if (useRaCollection && hasRa && !collectionName.empty() && !currentGame.empty())
                                {
                                    auto cacheKey = std::make_pair(collectionName, currentGame);
                                    if (addedToRaCache_.find(cacheKey) == addedToRaCache_.end())
                                    {
                                        addToRaCollection(collectionName, currentGame);
                                        addedToRaCache_.insert(cacheKey);
#ifdef _DEBUG
                                        Logger::write(Logger::ZONE_INFO, "RetroRa",
                                            "Added after 2-second hover: " + currentGame + " (" + collectionName + ")");
#endif
                                    }
                                }

                                // Or discover if unknown
                                if (!hasRa)
                                {
                                    Launcher l(config_, this);
                                    l.launchRetroRa(currentGame, currentSystem, false);
#ifdef _DEBUG
                                    Logger::write(Logger::ZONE_INFO, "RetroRa",
                                        "Update triggered after 2s: " + currentGame + " (" + currentSystem + ")");
#endif
                                    lastRetroRaCallTime_ = now;
                                    pendingRetroRaReload_ = true;
                                }
                                loggedRaCollectionSkip_ = false;
                            }

                            // Delayed check for new discoveries
                            if (pendingRetroRaReload_ && now - lastRetroRaCallTime_ >= 1200)
                            {
                                pendingRetroRaReload_ = false;
                                std::string statsPath = Utils::combinePath(
                                    Configuration::absolutePath,
                                    "collections", collectionName, "info",
                                    selected->name + ".conf"
                                );

                                try
                                {
                                    selected->info_.clear();
                                    selected->loadInfo(statsPath);

                                    std::string newValue;
                                    bool nowHasRa = selected->getInfo("0", newValue) && newValue == "isRA";

                                    if (nowHasRa)
                                    {
                                        if (currentPage_)
                                        {
                                            currentPage_->refreshBadges();
                                        }

                                        if (useRaCollection && !collectionName.empty() && !currentGame.empty())
                                        {
                                            auto cacheKey = std::make_pair(collectionName, currentGame);
                                            if (addedToRaCache_.find(cacheKey) == addedToRaCache_.end())
                                            {
                                                addToRaCollection(collectionName, currentGame);
                                                addedToRaCache_.insert(cacheKey);
                                                Logger::write(Logger::ZONE_INFO, "RetroRa",
                                                    "Added after RetroRa discovery: " + currentGame + " (" + collectionName + ")");
                                            }
                                        }
                                    }
                                }
                                catch (const std::exception& e)
                                {
                                    Logger::write(Logger::ZONE_ERROR, "RetroRa",
                                        "Delayed reload failed: " + std::string(e.what()));
                                }
                            }
                        }
                    }
                    else
                    {
                        // Reset when leaving leaf item
                        if (!lastRetroRaGame_.empty())
                        {
                            lastRetroRaGame_.clear();
                            lastRetroRaSystem_.clear();
                            lastRetroRaTriggered_ = false;
                            pendingRetroRaReload_ = false;
                        }
                    }
                }
                if (!splashMode)
                {
                    if (currentPage_->isAttractIdle())
                    {
                        if (!attractMode_ && attract_.isSet())
                        {
                            currentPage_->attractEnter();
                            l.LEDBlinky(5);
                        }
                        else if (attractMode_ && !attract_.isSet())
                        {
                            currentPage_->attractExit();
                            l.LEDBlinky(6);
                        }
                        else if (attract_.isSet())
                        {
                            currentPage_->attract();
                        }
                        attractMode_ = attract_.isSet();
                    }
                }
            }

            if (!isAutoNavigating_ && reloadSavedItem_.empty())
            {
                if (isReloading_)
                {
                    isReloading_ = false;
                    if (loadingTextTexture_) {
                        SDL_DestroyTexture(loadingTextTexture_);
                        loadingTextTexture_ = nullptr;
                    }
                }
                render();
            }
            else if (isReloading_)
            {
                renderLoadingScreen();
            }
        }
    }
    return reboot_;
}
std::string RetroFE::getCorrectCollectionName(Item* item, CollectionInfo* currentCollection) const
{
    if (!item || !currentCollection) {
        return currentCollection ? currentCollection->name : "unknown";
    }

    std::string collectionName = item->collectionInfo ? item->collectionInfo->name : currentCollection->name;

    if (currentCollection->originalCollection) {
        collectionName = currentCollection->name;
#ifdef _DEBUG
        Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC", "Using original collection: " + collectionName);
#endif
        return collectionName;
    }

    if (collectionName != currentCollection->name) {
        return collectionName;
    }

    std::string collectionDir = Utils::combinePath(Configuration::absolutePath, "collections", currentCollection->name);
    DIR* dp = opendir(collectionDir.c_str());
    if (dp) {
        struct dirent* dirp;
        while ((dirp = readdir(dp)) != NULL) {
            std::string file = dirp->d_name;
            size_t position = file.find_last_of(".");
            std::string extension = (position != std::string::npos) ? file.substr(position) : "";
            if (extension == ".sub") {
                std::string subFilePath = Utils::combinePath(collectionDir, file);
                std::ifstream subFile(subFilePath);
                std::string line;
                while (std::getline(subFile, line)) {
                    line = Utils::filterComments(line);
                    if (!line.empty() && line == item->name) {
                        collectionName = file.substr(0, position);
                        Logger::write(Logger::ZONE_DEBUG, "RetroFEVLC",
                            "Detected original collection via .sub: " + collectionName + " for " + item->name);
                        subFile.close();
                        closedir(dp);
                        return collectionName;
                    }
                }
                subFile.close();
            }
        }
        closedir(dp);
    }

    return collectionName;
}

// add the currently selected game to the RetroAchievements collection (if enabled) when we trigger an update check
void RetroFE::addToRaCollection(const std::string& system, const std::string& game)
{
    // Path to the RetroAchievements collection folder
    std::string raDir = Utils::combinePath(Configuration::absolutePath, "collections", "RetroAchievements Collection");

    // Create the folder if it doesn't exist yet
    struct stat info;
    if (stat(raDir.c_str(), &info) != 0 || !(info.st_mode & S_IFDIR))
    {
#if defined(_WIN32)
        if (!CreateDirectory(raDir.c_str(), NULL))
        {
            if (GetLastError() != ERROR_ALREADY_EXISTS)
            {
                Logger::write(Logger::ZONE_ERROR, "RetroRa", "Failed to create RetroAchievements folder: " + raDir);
                return;
            }
        }
#else
        if (mkdir(raDir.c_str(), 0755) != 0 && errno != EEXIST)
        {
            Logger::write(Logger::ZONE_ERROR, "RetroRa", "Failed to create RetroAchievements folder: " + raDir + " (" + strerror(errno) + ")");
            return;
        }
#endif
        Logger::write(Logger::ZONE_INFO, "RetroRa", "Created RetroAchievements collection folder");
    }

    // The .sub file for this system (e.g. NES.sub, SNES.sub, Mega Drive.sub, etc.)
    std::string subFilePath = Utils::combinePath(raDir, system + ".sub");

    // Read existing entries to avoid duplicates
    std::set<std::string> existingGames;
    std::ifstream in(subFilePath);
    std::string line;
    while (std::getline(in, line))
    {
        line = Utils::trimEnds(Utils::filterComments(line));
        if (!line.empty())
        {
            existingGames.insert(line);
        }
    }
    in.close();

    // If the game is not already there append it
    if (existingGames.find(game) == existingGames.end())
    {
        std::ofstream out(subFilePath, std::ios::app);
        if (out.is_open())
        {
            out << game << std::endl;
            out.close();
            Logger::write(Logger::ZONE_INFO, "RetroRa",
                "Added game to RetroAchievements " + system + ".sub : " + game);
        }
        else
        {
            Logger::write(Logger::ZONE_ERROR, "RetroRa", "Could not write to " + subFilePath);
        }
    }
    else
    {
        Logger::write(Logger::ZONE_DEBUG, "RetroRa",
            "Game already in RetroAchievements/" + system + ".sub : " + game);
    }
}

// Check if we can go back a page or quite RetroFE
bool RetroFE::back(bool& exit)
{
    bool canGoBack = false;
    bool exitOnBack = false;
    config_.getProperty("exitOnFirstPageBack", exitOnBack);
    exit = false;

    if (currentPage_->getMenuDepth() <= 1 && pages_.empty())
    {
        exit = exitOnBack;
    }
    else
    {
        canGoBack = true;
    }

    return canGoBack;
}

void RetroFE::resetInput() {
    input_.resetStates();  // Reset all input states (e.g., up, down, left, right)
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        // Clear the event queue, keeping joystick add/remove events
        if (e.type == SDL_JOYDEVICEADDED || e.type == SDL_JOYDEVICEREMOVED) {
            input_.update(e);
        }
    }
}
// Process the user input
RetroFE::RETROFE_STATE RetroFE::processUserInput(Page* page)
{
    bool exit = false;
    RETROFE_STATE state = RETROFE_IDLE;

    // Poll all events until we find an active one
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        input_.update(e); // Update input state for each event
    }

    if (suppressNextBack_)
    {
        if (input_.keystate(UserInput::KeyCodeBack))
        {
            return state;
        }
        else
        {
            suppressNextBack_ = false;
        }
    }
    // Early exit conditions: no action if page isnt idle or key delay isnt met
    if (!page || !page->isIdle() || (currentTime_ - keyLastTime_ <= keyDelayTime_))
    {
        return state;
    }
    // Handle next/previous game inputs
    ScrollingList* activeMenu = page->getActiveMenu();
    if (!activeMenu) {
        Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC", "No active menu");
        return state;
    }
#ifdef _DEBUG
    Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC", "Active menu found, isGrid=" + std::to_string(activeMenu->isGrid) +
        ", rows=" + std::to_string(activeMenu->rows) + ", columns=" + std::to_string(activeMenu->columns));
#endif
    if (activeMenu->isGrid) {
        bool moved = false;
        int previousIndex = activeMenu->getSelectedIndex();

        if (input_.keystate(UserInput::KeyCodeUp)) {
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC", "Grid: Up input, currentIndex=" + std::to_string(previousIndex));
#endif
            activeMenu->moveUp();
            moved = true;
        }
        else if (input_.keystate(UserInput::KeyCodeDown)) {
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC", "Grid: Down input, currentIndex=" + std::to_string(previousIndex));
#endif
            activeMenu->moveDown();
            moved = true;
        }
        else if (input_.keystate(UserInput::KeyCodeLeft)) {
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC", "Grid: Left input, currentIndex=" + std::to_string(previousIndex));
#endif
            activeMenu->moveLeft();
            moved = true;
        }
        else if (input_.keystate(UserInput::KeyCodeRight)) {
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC", "Grid: Right input, currentIndex=" + std::to_string(previousIndex));
#endif
            activeMenu->moveRight();
            moved = true;
        }

        if (moved && activeMenu->getSelectedIndex() != previousIndex) {
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC", "Grid: Moved from index " + std::to_string(previousIndex) +
                " to " + std::to_string(activeMenu->getSelectedIndex()));
#endif
            attract_.reset();
            page->onNewItemSelected();
            // page->playSound("highlight"); // uncoment if sound feedback is desired
            keyLastTime_ = currentTime_;
            resetInput();  // Reset inputs after movement
            return state;
        }
    }
    else if (page->isHorizontalScroll())
    {
        if (input_.keystate(UserInput::KeyCodeRight))
        {
            attract_.reset();
            page->setScrolling(Page::ScrollDirectionForward);
            page->scroll(true);
            page->updateScrollPeriod();
            keyLastTime_ = currentTime_;
            return state;
        }
        else if (input_.keystate(UserInput::KeyCodeLeft))
        {
            attract_.reset();
            page->setScrolling(Page::ScrollDirectionBack);
            page->scroll(false);
            page->updateScrollPeriod();
            keyLastTime_ = currentTime_;
            return state;
        }
    }
    else
    {
        if (input_.keystate(UserInput::KeyCodeDown))
        {
            attract_.reset();
            page->setScrolling(Page::ScrollDirectionForward);
            page->scroll(true);
            page->updateScrollPeriod();
            keyLastTime_ = currentTime_;
            return state;
        }
        else if (input_.keystate(UserInput::KeyCodeUp))
        {
            attract_.reset();
            page->setScrolling(Page::ScrollDirectionBack);
            page->scroll(false);
            page->updateScrollPeriod();
            keyLastTime_ = currentTime_;
            return state;
        }
    }

    // Ignore other keys while the menu is scrolling
    if (page->isIdle() && currentTime_ - keyLastTime_ > keyDelayTime_)
    {
        if (input_.keystate(UserInput::KeyCodeMenu) && !menuMode_ && (!currentPage_ || !currentPage_->isSearchActive()))
        {
            state = RETROFE_MENUMODE_START_REQUEST;
        }
        else if (input_.keystate(UserInput::KeyCodeConfigMenu))
        {
            attract_.reset();
            state = RETROFE_CONFIGMENU_REQUEST;
        }
        else if (input_.keystate(UserInput::KeyCodeSearch))
        {
            attract_.reset();
			state = RETROFE_SEARCH_REQUEST;
        }
        else if (input_.keystate(UserInput::KeyCodeRefresh))
        {
            attract_.reset();
            state = RETROFE_RELOAD_REQUEST;
        }

        // Handle Collection Up/Down keys
        else if ((input_.keystate(UserInput::KeyCodeCollectionUp) && (page->isHorizontalScroll() || !input_.keystate(UserInput::KeyCodeUp))) ||
            (input_.keystate(UserInput::KeyCodeCollectionLeft) && (!page->isHorizontalScroll() || !input_.keystate(UserInput::KeyCodeLeft))))
        {
            attract_.reset();
            if (page && page->isSearchActive())
            {
                state = RETROFE_BACK_REQUEST;
            }
            else
            {
                bool backOnCollection = false;
                config_.getProperty("backOnCollection", backOnCollection);
                if (page->getMenuDepth() == 1 || !backOnCollection)
                    state = RETROFE_COLLECTION_UP_REQUEST;
                else
                    state = RETROFE_BACK_REQUEST;
            }
        }

        else if ((input_.keystate(UserInput::KeyCodeCollectionDown) && (page->isHorizontalScroll() || !input_.keystate(UserInput::KeyCodeDown))) ||
            (input_.keystate(UserInput::KeyCodeCollectionRight) && (!page->isHorizontalScroll() || !input_.keystate(UserInput::KeyCodeRight))))
        {
            attract_.reset();
            if (page && page->isSearchActive())
            {
                state = RETROFE_BACK_REQUEST;
            }
            else
            {
                bool backOnCollection = false;
                config_.getProperty("backOnCollection", backOnCollection);
                if (page->getMenuDepth() == 1 || !backOnCollection)
                    state = RETROFE_COLLECTION_DOWN_REQUEST;
                else
                    state = RETROFE_BACK_REQUEST;
            }
        }

        else if (input_.keystate(UserInput::KeyCodePageUp))
        {
            attract_.reset();
            page->pageScroll(Page::ScrollDirectionBack);
            state = RETROFE_MENUJUMP_REQUEST;
        }

        else if (input_.keystate(UserInput::KeyCodePageDown))
        {
            attract_.reset();
            page->pageScroll(Page::ScrollDirectionForward);
            state = RETROFE_MENUJUMP_REQUEST;
        }

        else if (input_.keystate(UserInput::KeyCodeLetterUp))
        {
            attract_.reset();
            bool cfwLetterSub;
            config_.getProperty("cfwLetterSub", cfwLetterSub);
            if (cfwLetterSub && page->hasSubs())
                page->cfwLetterSubScroll(Page::ScrollDirectionBack);
            else
                page->letterScroll(Page::ScrollDirectionBack);
            state = RETROFE_MENUJUMP_REQUEST;
        }

        else if (input_.keystate(UserInput::KeyCodeLetterDown))
        {
            attract_.reset();
            bool cfwLetterSub;
            config_.getProperty("cfwLetterSub", cfwLetterSub);
            if (cfwLetterSub && page->hasSubs())
                page->cfwLetterSubScroll(Page::ScrollDirectionForward);
            else
                page->letterScroll(Page::ScrollDirectionForward);
            state = RETROFE_MENUJUMP_REQUEST;
        }

        else if (input_.keystate(UserInput::KeyCodeFavPlaylist))
        {
            attract_.reset();
            page->favPlaylist();
            state = RETROFE_PLAYLIST_REQUEST;
        }

        else if (input_.keystate(UserInput::KeyCodeNextPlaylist) ||
            (input_.keystate(UserInput::KeyCodePlaylistDown) && page->isHorizontalScroll()) ||
            (input_.keystate(UserInput::KeyCodePlaylistRight) && !page->isHorizontalScroll()))
        {
            attract_.reset();
            page->nextPlaylist();
            state = RETROFE_PLAYLIST_REQUEST;
        }

        else if (input_.keystate(UserInput::KeyCodePrevPlaylist) ||
            (input_.keystate(UserInput::KeyCodePlaylistUp) && page->isHorizontalScroll()) ||
            (input_.keystate(UserInput::KeyCodePlaylistLeft) && !page->isHorizontalScroll()))
        {
            attract_.reset();
            page->prevPlaylist();
            state = RETROFE_PLAYLIST_REQUEST;
        }

        else if (input_.keystate(UserInput::KeyCodeCyclePlaylist) ||
            input_.keystate(UserInput::KeyCodeNextCyclePlaylist))
        {
            attract_.reset();
            std::string cycleString;
            config_.getProperty("cyclePlaylist", cycleString);
            std::vector<std::string> cycleVector;
            Utils::listToVector(cycleString, cycleVector, ',');
            page->nextCyclePlaylist(cycleVector);
            state = RETROFE_PLAYLIST_REQUEST;
        }

        else if (input_.keystate(UserInput::KeyCodePrevCyclePlaylist))
        {
            attract_.reset();
            std::string cycleString;
            config_.getProperty("cyclePlaylist", cycleString);
            std::vector<std::string> cycleVector;
            Utils::listToVector(cycleString, cycleVector, ',');
            page->prevCyclePlaylist(cycleVector);
            state = RETROFE_PLAYLIST_REQUEST;
        }

        else if (input_.keystate(UserInput::KeyCodeRemovePlaylist))
        {
            attract_.reset();
            bool wasInFavoritesPlaylist = (page->getPlaylistName() == "favorites");
            page->removePlaylist();
            if (wasInFavoritesPlaylist)
                state = RETROFE_PLAYLIST_REQUEST;
            else
                page->refreshBadges();
        }

        else if (input_.keystate(UserInput::KeyCodeAddPlaylist))
        {
            attract_.reset();
            bool wasInFavoritesPlaylist = (page->getPlaylistName() == "favorites");
            page->addPlaylist();
            if (wasInFavoritesPlaylist)
                state = RETROFE_PLAYLIST_REQUEST;
            else
                page->refreshBadges();
        }

        else if (input_.keystate(UserInput::KeyCodeTogglePlaylist))
        {
            attract_.reset();
            bool wasInFavoritesPlaylist = (page->getPlaylistName() == "favorites");
            page->togglePlaylist();
            if (wasInFavoritesPlaylist)
                state = RETROFE_PLAYLIST_REQUEST;
            else
                page->refreshBadges();
        }

        else if (input_.keystate(UserInput::KeyCodeSkipForward))
        {
            attract_.reset();
            page->skipForward();
            page->jukeboxJump();
            keyLastTime_ = currentTime_;
        }

        else if (input_.keystate(UserInput::KeyCodeSkipBackward))
        {
            attract_.reset();
            page->skipBackward();
            page->jukeboxJump();
            keyLastTime_ = currentTime_;
        }

        else if (input_.keystate(UserInput::KeyCodeSkipForwardp))
        {
            attract_.reset();
            page->skipForwardp();
            page->jukeboxJump();
            keyLastTime_ = currentTime_;
        }

        else if (input_.keystate(UserInput::KeyCodeSkipBackwardp))
        {
            attract_.reset();
            page->skipBackwardp();
            page->jukeboxJump();
            keyLastTime_ = currentTime_;
        }

        else if (input_.keystate(UserInput::KeyCodePause))
        {
            attract_.reset();
            page->pause();
            page->jukeboxJump();
            keyLastTime_ = currentTime_;
            }
			// SORT by UP/DOWN keys (only work when a playlist is active)
   // Dynamic sorting keys
        else if (input_.keystate(UserInput::KeyCodeSortUp))
        {
            attract_.reset();
            sortDirectionUp_ = true;
            state = RETROFE_NEXT_SORT_REQUEST;
            keyLastTime_ = currentTime_;
            Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC", "SortUp key detected");
            }
        else if (input_.keystate(UserInput::KeyCodeSortDown))
        {
            attract_.reset();
            sortDirectionUp_ = false;
            state = RETROFE_NEXT_SORT_REQUEST;
            keyLastTime_ = currentTime_;
            Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC", "SortDown key detected");
            }
            // Volume controls
        else if (input_.keystate(UserInput::KeyCodeVolumeDown))
        {
            attract_.reset();
            if (currentPage_)
                currentPage_->adjustVolume(-0.05f);
            keyLastTime_ = currentTime_ - 0.05f;   // fast repeat when holding
            }
        else if (input_.keystate(UserInput::KeyCodeVolumeUp))
        {
            attract_.reset();
            if (currentPage_)
                currentPage_->adjustVolume(0.05f);
            keyLastTime_ = currentTime_ - 0.05f;
            }
        else if (input_.keystate(UserInput::KeyCodeAudioMute))
        {
            attract_.reset();
            if (currentPage_)
                currentPage_->toggleMute();
            keyLastTime_ = currentTime_;
        }
        // Queued playlists

      // 1. ADD to queue (1,2,3,4,5...)
        else if (isQueuedKey(UserInput::KeyCodeJbAddQueuedBase, DefaultMaxQueued))
        {
            int idx = getQueuedIndex(UserInput::KeyCodeJbAddQueuedBase, DefaultMaxQueued);
            if (idx > 0)
            {
                attract_.reset();
#ifdef _DEBUG
                Logger::write(Logger::ZONE_INFO, "RetroFeVLC",
                    "[ADD QUEUE] Key detected adding to queue " + std::to_string(idx));
#endif
                page->addToQueued(idx);
                state = RETROFE_PLAYLIST_REQUEST;
                keyLastTime_ = currentTime_;
                
            }
            }

            // 2. CALL queue (Z=1, X=2, C=3...)
        else if (isQueuedKey(UserInput::KeyCodeJbCallQueuedBase, DefaultMaxQueued))
        {
            int idx = getQueuedIndex(UserInput::KeyCodeJbCallQueuedBase, DefaultMaxQueued);
            if (idx > 0)
            {
                attract_.reset();
#ifdef _DEBUG
                Logger::write(Logger::ZONE_INFO, "RetroFeVLC",
                    "=== CALL QUEUED DETECTED === Key for queue " + std::to_string(idx));
#endif
                page->callQueued(idx);
                state = RETROFE_PLAYLIST_REQUEST;
                keyLastTime_ = currentTime_;
            }
            }

        else if (input_.keystate(UserInput::KeyCodeJbisShuffle)) {
            attract_.reset();
            page->toggleShuffle();         
            keyLastTime_ = currentTime_;
        }
        else if (input_.keystate(UserInput::KeyCodeJbRepeatTrackLoop)) {
            attract_.reset();
            page->toggleRepeatLoop();
            keyLastTime_ = currentTime_;
        }
		// inside a queued playlist, allow removing from queue, clearing queue, and moving items up/down in the queue
        else if (page->isCurrentPlaylistQueued())
        {
            if (input_.keystate(UserInput::KeyCodeJbRemoveFromQueue)) {
                attract_.reset();
                page->removeFromCurrentQueue();
                keyLastTime_ = currentTime_;
            }
            else if (input_.keystate(UserInput::KeyCodeJbClearCurrentQueue)) {
                attract_.reset();
                page->clearCurrentQueue();
                keyLastTime_ = currentTime_;
            }
            else if (input_.keystate(UserInput::KeyCodeJbMoveUpInQueue)) {
                attract_.reset();
                page->moveInCurrentPlaylist(true); //inverse direction for moving up vs down
                keyLastTime_ = currentTime_;
            }
            else if (input_.keystate(UserInput::KeyCodeJbMoveDownInQueue)) {
                attract_.reset();
                page->moveInCurrentPlaylist(false);
                keyLastTime_ = currentTime_;
            }
			else if (input_.keystate(UserInput::KeyCodeQuit)) // force quit if trying to exit while in a queued playlist, to avoid potential issues with the queue state
            {
                attract_.reset();
                state = RETROFE_QUIT_REQUEST;
            }
        }

        //##################################

        else if (input_.keystate(UserInput::KeyCodeRestart))
        {
            attract_.reset();
            page->restart();
            keyLastTime_ = currentTime_;
        }

        else if (input_.keystate(UserInput::KeyCodeRandom))
        {
            attract_.reset();
            page->selectRandom();
            state = RETROFE_MENUJUMP_REQUEST;
        }

        else if (input_.keystate(UserInput::KeyCodeAdminMode))
        {
            //todo: add admin mode support
        }
        else if (input_.keystate(UserInput::KeyCodeBack))
        {
            attract_.reset();
            if (page && page->isSearchActive())
            {
                state = RETROFE_BACK_REQUEST;
            }
            else if (back(exit))
            {
                // Check if the current page is a game layout page
                if (currentPage_->getCollectionName() == "TempGameCollection")
                {
                    state = RETROFE_GAME_PAGE_EXIT; // New state to exit game page
                }
                else
                {
                    state = RETROFE_BACK_REQUEST; // Normal back behavior
                }
            }
            else if (exit)
            {
                state = RETROFE_QUIT_REQUEST; // Exit the program
            }
        }

        else if (input_.keystate(UserInput::KeyCodeQuit))
        {
            attract_.reset();
            state = RETROFE_QUIT_REQUEST;
        }

        else if (input_.keystate(UserInput::KeyCodeReboot))
        {
            attract_.reset();
            reboot_ = true;
            state = RETROFE_QUIT_REQUEST;
        }

        else if (input_.keystate(UserInput::KeyCodeSaveFirstPlaylist))
        {
            attract_.reset();
            if (page->getMenuDepth() == 1)
            {
                firstPlaylist_ = page->getPlaylistName();
                saveRetroFEState();
            }
        }
        else if (input_.keystate(UserInput::KeyCodeSelect))
{
                attract_.reset();
                nextPageItem_ = page->getSelectedItem();

            if (nextPageItem_)
            {
                if (nextPageItem_->leaf)
                {
                    if (menuMode_)
                    {
                        state = RETROFE_HANDLE_MENUENTRY;
                    }
                    else
                    {
                                    std::string layoutName;
                            config_.getProperty("layout", layoutName);
                            std::string collectionName = (nextPageItem_->collectionInfo && !nextPageItem_->collectionInfo->name.empty()) ? nextPageItem_->collectionInfo->name : page->getCollectionName();
                            std::string gameName = nextPageItem_->name;

                            bool gameLayoutExists = checkGameLayoutExists(layoutName, collectionName, gameName);
                            bool defaultLayoutExists = checkGameLayoutExists(layoutName, collectionName, "default");

                            if (gameLayoutExists || defaultLayoutExists)
                            {
                                CollectionInfo* originalCollection = page->getCollection();
                                CollectionInfo* tempCollection = createTempCollection(nextPageItem_, originalCollection);
                                if (!tempCollection)
                                {
                                    Logger::write(Logger::ZONE_ERROR, "RetroFeVLC", "Failed to create temp collection for item: " + gameName);
                                    state = RETROFE_LAUNCH_ENTER;
                                }
                                else
                                {
                                    Page* gamePage = nullptr;
                                    if (gameLayoutExists)
                                    {
                                        Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Attempting to load game-specific layout for: " + gameName);
                                        gamePage = buildGamePage(tempCollection, layoutName, collectionName, gameName);
                                    }
                                    else if (defaultLayoutExists)
                                    {
                                        Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Game-specific layout not found, falling back to default layout");
                                        gamePage = buildGamePage(tempCollection, layoutName, collectionName, "default");
                                    }

                                    if (gamePage)
                                    {
                                        currentPage_->stop();
                                        currentPage_->freeGraphicsMemory();
                                        pages_.push(page);
                                        currentPage_ = gamePage;
                                        currentPage_->pushCollection(tempCollection);
                                        currentPage_->onNewItemSelected();
                                        currentPage_->reallocateMenuSpritePoints();
                                        state = RETROFE_GAME_PAGE_ENTER;
                                    }
                                    else
                                    {
                                        Logger::write(Logger::ZONE_ERROR, "RetroFeVLC", "Failed to build game page for layout: " + (gameLayoutExists ? gameName : "default"));
                                        delete tempCollection;
                                        state = RETROFE_LAUNCH_ENTER;
                                    }
                                }
                            }
                            else
                            {
                                Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "No layout found for game '" + gameName + "' or default. Launching directly.");
                                state = RETROFE_LAUNCH_ENTER;
                            }
                        }
                    }
                else
                {
                    if (!collectionExists(nextPageItem_->name, menuMode_))
                    {
                        Logger::write(Logger::ZONE_WARNING, "RetroFeVLC",
                            "Cannot enter " + std::string(menuMode_ ? "menu: '" : "collection: '") +
                            nextPageItem_->name + "' (does not exist)");
                        nextPageItem_ = nullptr;
                    }
                    else
                    {
                        CollectionInfoBuilder cib(config_, *metadb_);
                        std::string attractModeSkipPlaylist = "";
                        std::string lastPlayedSkipCollection = "";
                        int size = 0;
                        config_.getProperty("attractModeSkipPlaylist", attractModeSkipPlaylist);
                        config_.getProperty("lastPlayedSkipCollection", lastPlayedSkipCollection);
                        config_.getProperty("lastplayedCollectionSize", size);

                        if (currentPage_->getPlaylistName() != attractModeSkipPlaylist &&
                            nextPageItem_->collectionInfo->name != lastPlayedSkipCollection)
                        {
                            cib.updateLastPlayedPlaylist(currentPage_->getCollection(), nextPageItem_, size);
                        }
                        state = RETROFE_NEXT_PAGE_REQUEST;
                    }
                }
            }
        }
    }

    if (state != RETROFE_IDLE)
    {
        keyLastTime_ = currentTime_;
        return state;
    }

    // Check if we're done scrolling
    if (!input_.keystate(UserInput::KeyCodeUp) &&
        !input_.keystate(UserInput::KeyCodeLeft) &&
        !input_.keystate(UserInput::KeyCodeDown) &&
        !input_.keystate(UserInput::KeyCodeRight) &&
        !input_.keystate(UserInput::KeyCodePlaylistUp) &&
        !input_.keystate(UserInput::KeyCodePlaylistLeft) &&
        !input_.keystate(UserInput::KeyCodePlaylistDown) &&
        !input_.keystate(UserInput::KeyCodePlaylistRight) &&
        !input_.keystate(UserInput::KeyCodeCollectionUp) &&
        !input_.keystate(UserInput::KeyCodeCollectionLeft) &&
        !input_.keystate(UserInput::KeyCodeCollectionDown) &&
        !input_.keystate(UserInput::KeyCodeCollectionRight) &&
        !input_.keystate(UserInput::KeyCodePageUp) &&
        !input_.keystate(UserInput::KeyCodePageDown) &&
        !input_.keystate(UserInput::KeyCodeLetterUp) &&
        !input_.keystate(UserInput::KeyCodeLetterDown) &&
        !input_.keystate(UserInput::KeyCodeCollectionUp) &&
        !input_.keystate(UserInput::KeyCodeCollectionDown) &&
        !attract_.isActive())
    {
        page->resetScrollPeriod();
        if (page->isMenuScrolling())
        {
            attract_.reset(attract_.isSet());
            state = RETROFE_HIGHLIGHT_REQUEST;
        }
    }

    return state;
}

// Helper to check if any queued key in range is pressed
bool RetroFE::isQueuedKey(UserInput::KeyCode_E base, int maxQueued)
{
    for (int i = 0; i < maxQueued; ++i)
    {
        if (input_.keystate(static_cast<UserInput::KeyCode_E>(base + i)))
            return true;
    }
    return false;
}

int RetroFE::getQueuedIndex(UserInput::KeyCode_E base, int maxQueued)
{
    for (int i = 0; i < maxQueued; ++i)
    {
        if (input_.keystate(static_cast<UserInput::KeyCode_E>(base + i)))
            return i + 1;
    }
    return 0;
}
// Load a page
Page* RetroFE::loadPage()
{
    std::string layoutName;

    config_.getProperty("layout", layoutName);

    PageBuilder pb(layoutName, "layout", config_, &fontcache_);
    Page* page = pb.buildPage();

   if (page) {
        updateLoadingConfig(page->getLoadingConfig());
        page->setMetadataDatabase(metadb_); // Set the database here
    } else {
        Logger::write(Logger::ZONE_ERROR, "RetroFeVLC", "Could not create page");
    }

    return page;
}


// Load the splash page
Page* RetroFE::loadSplashPage()
{
    std::string layoutName;
    config_.getProperty("layout", layoutName);

    PageBuilder pb(layoutName, "splash", config_, &fontcache_);
    Page* page = pb.buildPage();
    if (page) {
        page->start();
    }

    return page;
}

CollectionInfo* RetroFE::getCollection(std::string collectionName) {
    // Check if subcollections should be merged or split
    bool subsSplit = false;
    config_.getProperty("subsSplit", subsSplit);

    // Build the collection
    CollectionInfoBuilder cib(config_, *metadb_);
    CollectionInfo* collection = cib.buildCollection(collectionName);
    collection->subsSplit = subsSplit;
    cib.injectMetadata(collection);

    // Load subcollections 
    DIR* dp;
    struct dirent* dirp;
    std::string path = Utils::combinePath(Configuration::absolutePath, "collections", collectionName);
    dp = opendir(path.c_str());
    if (dp) {
        while ((dirp = readdir(dp)) != NULL) {
            std::string file = dirp->d_name;
            size_t position = file.find_last_of(".");
            std::string basename = (std::string::npos == position) ? file : file.substr(0, position);
            std::string comparator = ".sub";
            size_t start = file.length() >= comparator.length() ? file.length() - comparator.length() : 0;
            if (start >= 0 && file.compare(start, comparator.length(), comparator) == 0) {
                Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Loading subcollection into menu: " + basename);
                CollectionInfo* subcollection = cib.buildCollection(basename, collectionName);
                collection->addSubcollection(subcollection);
                subcollection->subsSplit = subsSplit;
                cib.injectMetadata(subcollection);
                collection->hasSubs = true;
            }
        }
        closedir(dp);
    }
    cib.addSubcollections(collection);

    // Load sorting settings (menuSort and new options)

    std::string prefix = "collections." + collectionName + ".list.";
    bool menuSort = true; // Global default
    config_.getProperty("menuSort", menuSort); // Global setting
    config_.getProperty(prefix + "menuSort", menuSort); // Per-collection override
    collection->menusort = menuSort;

    // Load new settings
    bool lastplayedByLastAccess = false; // Default
    config_.getProperty("lastplayedByLastAccess", lastplayedByLastAccess);
    config_.getProperty(prefix + "lastplayedByLastAccess", lastplayedByLastAccess);
    collection->lastplayedByLastAccess = lastplayedByLastAccess;

    bool favoritesByLastAccess = false; // Default
    config_.getProperty("favoritesByLastAccess", favoritesByLastAccess);
    config_.getProperty(prefix + "favoritesByLastAccess", favoritesByLastAccess);
    collection->favoritesByLastAccess = favoritesByLastAccess;

    bool favoritesCollectionByAsc = false; // Default
    config_.getProperty("favoritesCollectionByAsc", favoritesCollectionByAsc);
    config_.getProperty(prefix + "favoritesCollectionByAsc", favoritesCollectionByAsc);
    collection->favoritesCollectionByAsc = favoritesCollectionByAsc;

    // Deduplicate items
    std::map<std::string, Item*> uniqueItems;
    std::vector<Item*> newItems;
    for (auto* item : collection->items) {
        std::string key = Utils::toLower(item->name);
        auto it = uniqueItems.find(key);
        if (it == uniqueItems.end()) {
            uniqueItems[key] = item;
            newItems.push_back(item);
        }
        else {
#ifdef _DEBUG
            Logger::write(Logger::ZONE_WARNING, "RetroFeVLC", "Duplicate item found in collection " + collectionName + ": " + item->name + " (keeping first occurrence: " + it->second->name + ")");
#endif
            delete item; // Free duplicate item
        }
    }
    collection->items = newItems;

    // Load playlists and apply sorting
    if (collection->menusort) {
        collection->sortItems();
    }

    // Existing metadata and menu parsing code...
    MenuParser mp;
    mp.buildMenuItems(collection, menuSort);
    for (int i = 1; i <= DefaultMaxQueued; ++i) {
        std::string key = "customizePlaylist" + std::to_string(i);
        std::string playlistFile;
        config_.getProperty(prefix + key, playlistFile);
        if (!playlistFile.empty()) {
            // Strip .txt if present (handle user input like "History.txt")
            if (playlistFile.size() > 4 && playlistFile.substr(playlistFile.size() - 4) == ".txt") {
                playlistFile = playlistFile.substr(0, playlistFile.size() - 4);
            }
            std::string orderKey = key + "FileOrder";
            bool userOrder = false; // Default to sorted
            config_.getProperty(prefix + orderKey, userOrder);
            collection->playlistByFileOrder[playlistFile] = userOrder;
            Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Loaded custom playlist: " + playlistFile + " with FileOrder=" + (userOrder ? "true" : "false"));
        }
    }

    cib.addPlaylists(collection);
    collection->sortPlaylists();

    // Add extra info, if available
    std::string statsDir = Utils::combinePath(Configuration::absolutePath, "collections", collectionName, "info");
    struct stat dirStat;
    if (stat(statsDir.c_str(), &dirStat) == 0 && (dirStat.st_mode & S_IFDIR))
    {
        for (std::vector<Item*>::iterator it = collection->items.begin(); it != collection->items.end(); it++)
        {
            std::string path = Utils::combinePath(statsDir, (*it)->name + ".conf");
            (*it)->loadInfo(path);
        }
    }

    // Remove parenthesis and brackets, if so configured
    bool showParenthesis = true;
    bool showSquareBrackets = true;

    (void)config_.getProperty("showParenthesis", showParenthesis);
    (void)config_.getProperty("showSquareBrackets", showSquareBrackets);

   

    typedef std::map<std::string, std::vector <Item*>*> Playlists_T;
    for (Playlists_T::iterator itP = collection->playlists.begin(); itP != collection->playlists.end(); itP++)
    {
        for (std::vector <Item*>::iterator itI = itP->second->begin(); itI != itP->second->end(); itI++)
        {
            if (!showParenthesis)
            {
                std::string::size_type firstPos = (*itI)->title.find_first_of("(");
                std::string::size_type secondPos = (*itI)->title.find_first_of(")", firstPos);

                while (firstPos != std::string::npos && secondPos != std::string::npos)
                {
                    firstPos = (*itI)->title.find_first_of("(");
                    secondPos = (*itI)->title.find_first_of(")", firstPos);

                    if (firstPos != std::string::npos)
                    {
                        (*itI)->title.erase(firstPos, (secondPos - firstPos) + 1);
                    }
                }
            }
            if (!showSquareBrackets)
            {
                std::string::size_type firstPos = (*itI)->title.find_first_of("[");
                std::string::size_type secondPos = (*itI)->title.find_first_of("]", firstPos);

                while (firstPos != std::string::npos && secondPos != std::string::npos)
                {
                    firstPos = (*itI)->title.find_first_of("[");
                    secondPos = (*itI)->title.find_first_of("]", firstPos);

                    if (firstPos != std::string::npos && secondPos != std::string::npos)
                    {
                        (*itI)->title.erase(firstPos, (secondPos - firstPos) + 1);
                    }
                }
            }
        }
    }

    return collection;
}



// Load a menu
CollectionInfo* RetroFE::getMenuCollection(std::string collectionName)
{
    std::string menuPath = Utils::combinePath(Configuration::absolutePath, "menu");
    std::string menuFile = Utils::combinePath(menuPath, collectionName + ".txt");
    std::vector<Item*> menuVector;
    CollectionInfoBuilder cib(config_, *metadb_);
    CollectionInfo* collection = new CollectionInfo(collectionName, menuPath, "", "", "");
    cib.ImportBasicList(collection, menuFile, menuVector);
    for (std::vector<Item*>::iterator it = menuVector.begin(); it != menuVector.end(); ++it)
    {
        (*it)->leaf = false;
        size_t position = (*it)->name.find("=");
        if (position != std::string::npos)
        {
            (*it)->ctrlType = Utils::trimEnds((*it)->name.substr(position + 1, (*it)->name.size() - 1));
            (*it)->name = Utils::trimEnds((*it)->name.substr(0, position));
            (*it)->title = (*it)->name;
            (*it)->fullTitle = (*it)->name;
            (*it)->leaf = true;
        }
        (*it)->collectionInfo = collection;
        collection->items.push_back(*it);
    }
    collection->playlists["all"] = &collection->items;
    return collection;
}


void RetroFE::saveRetroFEState()
{
    std::string file = Utils::combinePath(Configuration::absolutePath, "settings_saved.conf");
    Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Saving settings_saved.conf");
    std::ofstream filestream;
    try
    {
        filestream.open(file.c_str());
        filestream << "firstPlaylist = " << firstPlaylist_ << std::endl;
        filestream.close();
    }
    catch (std::exception&)
    {
        Logger::write(Logger::ZONE_ERROR, "RetroFeVLC", "Save failed: " + file);
    }
}

DWORD RetroFE::GetProcId(const wchar_t* procName)
{
    PROCESSENTRY32W procEntry;
    procEntry.dwSize = sizeof(procEntry);

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
    if (hSnap == INVALID_HANDLE_VALUE)
        return 0;

    if (Process32FirstW(hSnap, &procEntry))
    {
        do
        {
            if (std::wcscmp(procEntry.szExeFile, procName) == 0)
            {
                CloseHandle(hSnap);
                return procEntry.th32ProcessID;
            }
        } while (Process32NextW(hSnap, &procEntry));
    }

    CloseHandle(hSnap);
    return 0;
}

// Check if a custom game layout file exists
bool RetroFE::isLayoutActive(const std::string& content, const std::string& layoutFile)
{
    std::unique_ptr<rapidxml::xml_document<>> doc = std::make_unique<rapidxml::xml_document<>>();
    try {
        doc->parse<0>(const_cast<char*>(content.c_str()));
    }
    catch (rapidxml::parse_error& e) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_ERROR, "RetroFeVLC", "Failed to parse XML in layout file: " + layoutFile + " - " + e.what());
#endif
        return false;
    }

    rapidxml::xml_node<>* root = doc->first_node("layout");
    if (!root) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC", "Missing <layout> root node in: " + layoutFile);
#endif
        return false;
    }

    rapidxml::xml_attribute<>* activeAttr = root->first_attribute("active");
    if (activeAttr) {
        std::string rawValue = activeAttr->value();
        std::string activeValue = PageBuilder::substituteGlobalVariables(rawValue);
#ifdef _DEBUG
        Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC", "Active attribute raw: " + rawValue + ", substituted: " + activeValue);
#endif
        if (Utils::toLower(activeValue) != "true") {
#ifdef _DEBUG
            Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Layout is not active: " + activeValue);
#endif
            return false;
        }
    }
    else {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC", "No active attribute found in: " + layoutFile);
#endif
    }
#ifdef _DEBUG
    Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC", "Layout exists and is active: " + layoutFile);

#endif
    return true;
}

bool RetroFE::checkGameLayoutExists(const std::string& layoutName, const std::string& collectionName, const std::string& gameName)
{
    std::string layoutPath = Utils::combinePath(Configuration::absolutePath, "layouts", layoutName, "collections", collectionName, "layout");
    std::string layoutFile = Utils::combinePath(layoutPath, gameName + ".xml");
#ifdef _DEBUG
    Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Checking game layout file: " + layoutFile);
#endif

    std::ifstream file(layoutFile);
    if (!file.good()) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC", "Layout file does not exist or cannot be opened: " + layoutFile);
#endif
        return false;
    }

    file.seekg(0, std::ios::end);
    if (file.tellg() == 0) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC", "Layout file is empty: " + layoutFile);
#endif
        return false;
    }
    file.seekg(0, std::ios::beg);

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    return isLayoutActive(content, layoutFile);
}


// Create a temporary collection with only the selected game
CollectionInfo* RetroFE::createTempCollection(Item* gameItem, CollectionInfo* originalCollection)
{
    CollectionInfo* tempCollection = new CollectionInfo("TempGameCollection", "", "", "", "");
    tempCollection->items.push_back(gameItem);
    tempCollection->playlists["all"] = &tempCollection->items;
    tempCollection->originalCollection = originalCollection; // Store reference to original collection
    return tempCollection;
}

// Build a page for the game using its custom layout
Page* RetroFE::buildGamePage(CollectionInfo* tempCollection, const std::string& layoutName, const std::string& collectionName, const std::string& gameName)
{
    PageBuilder pb(layoutName, gameName, config_, &fontcache_);
    Page* page = pb.buildPage(collectionName);
    return page;
}

bool RetroFE::collectionExists(const std::string& name, bool isMenuMode)
{
    if (name.empty()) return false;

    if (isMenuMode)
    {
        std::string menuFile = Utils::combinePath(Configuration::absolutePath, "menu", name + ".txt");
        if (Utils::fileExists(menuFile)) return true;
        std::string colMenuTxt = Utils::combinePath(Configuration::absolutePath, "collections", name, "menu.txt");
        if (Utils::fileExists(colMenuTxt)) return true;
        std::string colMenuDir = Utils::combinePath(Configuration::absolutePath, "collections", name, "menu");
        struct stat st;
        if (stat(colMenuDir.c_str(), &st) == 0 && (st.st_mode & S_IFDIR)) return true;
        return false;
    }
    else
    {
        std::string colDir = Utils::combinePath(Configuration::absolutePath, "collections", name);
        struct stat st;
        if (stat(colDir.c_str(), &st) == 0 && (st.st_mode & S_IFDIR))
        {
            return true;
        }
        std::string listPath;
        if (config_.getProperty("collections." + name + ".list.path", listPath) && !listPath.empty())
        {
            return true;
        }
        return false;
    }
}

void RetroFE::buildMenuTree() {
    collectionParents_.clear();
    std::string collectionsDir = Utils::combinePath(Configuration::absolutePath, "collections");
    // Helper: read menu file (folder .txt or menu.txt)
    auto processMenuFile = [&](const std::string& menuFilePath, const std::string& parentCol) {
        std::ifstream menuFile(menuFilePath);
        if (!menuFile.is_open()) {
#ifdef _DEBUG
            Logger::write(Logger::ZONE_WARNING, "RetroFeVLC", "Could not open menu file: " + menuFilePath);
#endif
            return;
        }
        std::string line;
        int lineNumber = 0;
        bool isEmpty = true;
        while (std::getline(menuFile, line)) {
            ++lineNumber;
            line = Utils::filterComments(line);
            line = Utils::trimEnds(line);
            if (!line.empty()) {
                isEmpty = false;
                std::string childCol = line;
                size_t pos = line.find("=");
                if (pos != std::string::npos)
                    childCol = Utils::trimEnds(line.substr(0, pos));
                if (childCol.empty()) continue;
                std::string childColPath = Utils::combinePath(collectionsDir, childCol);
                struct stat st;
                if (stat(childColPath.c_str(), &st) == 0 && (st.st_mode & S_IFDIR)) {
                    collectionParents_[childCol].push_back(parentCol);
#ifdef _DEBUG
                    Logger::write(Logger::ZONE_DEBUG, "RetroFEVLC",
                        "Mapped collection '" + childCol + "' to parent '" + parentCol +
                        "' from " + menuFilePath + " (line " + std::to_string(lineNumber) + ")");
#endif
                }
            }
        }
        menuFile.close();
        // If empty, also scan sub menu folder (legacy behavior)
        if (isEmpty) {
            std::string subMenuDir = Utils::combinePath(collectionsDir, parentCol, "menu");
            DIR* subDp = opendir(subMenuDir.c_str());
            if (subDp) {
                struct dirent* subDirp;
                while ((subDirp = readdir(subDp)) != NULL) {
                    std::string subFile = subDirp->d_name;
                    size_t subPos = subFile.find_last_of(".");
                    std::string subExt = (subPos != std::string::npos) ? subFile.substr(subPos) : "";
                    if (subExt == ".txt") {
                        std::string childCol = subFile.substr(0, subPos);
                        std::string childColPath = Utils::combinePath(collectionsDir, childCol);
                        struct stat st;
                        if (stat(childColPath.c_str(), &st) == 0 && (st.st_mode & S_IFDIR)) {
                            collectionParents_[childCol].push_back(parentCol);
#ifdef _DEBUG
                            Logger::write(Logger::ZONE_DEBUG, "RetroFEVLC",
                                "Mapped collection '" + childCol + "' to parent '" + parentCol +
                                "' from submenu file " + subFile);
#endif
                        }
                    }
                }
                closedir(subDp);
            }
        }
        };
    // Main scanning loop
    DIR* dp = opendir(collectionsDir.c_str());
    if (dp) {
        struct dirent* dirp;
        while ((dirp = readdir(dp)) != NULL) {
            std::string colName = dirp->d_name;
            if (colName == "." || colName == ".." || dirp->d_type != DT_DIR)
                continue;
            std::string menuDir = Utils::combinePath(collectionsDir, colName, "menu");
            std::string menuTxt = Utils::combinePath(collectionsDir, colName, "menu.txt"); // NEW
            // Priority: folder first, then single menu.txt file
            DIR* menuDp = opendir(menuDir.c_str());
            if (menuDp) {
                // Case 1: traditional menu/ folder
                std::vector<std::string> menuFiles;
                struct dirent* menuDirp;
                while ((menuDirp = readdir(menuDp)) != NULL) {
                    std::string file = menuDirp->d_name;
                    size_t pos = file.find_last_of(".");
                    std::string ext = (pos != std::string::npos) ? file.substr(pos) : "";
                    if (ext == ".txt")
                        menuFiles.push_back(file);
                }
                std::sort(menuFiles.begin(), menuFiles.end());
                for (const auto& file : menuFiles) {
                    std::string menuFilePath = Utils::combinePath(menuDir, file);
                    processMenuFile(menuFilePath, colName);
                }
                closedir(menuDp);
            }
            else if (Utils::fileExists(menuTxt)) {
                // Case 2: single menu.txt file (NEW)
#ifdef _DEBUG
                Logger::write(Logger::ZONE_DEBUG, "RetroFEVLC",
                    "Found single menu.txt for collection: " + colName);
#endif
                processMenuFile(menuTxt, colName);
            }
            else {
#ifdef _DEBUG
                Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC",
                    "No menu or menu.txt found for collection: " + colName);
#endif
            }
        }
        closedir(dp);
    }
    else {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_ERROR, "RetroFeVLC",
            "Could not open collections directory: " + collectionsDir);
#endif
    }
    // Log result
#ifdef _DEBUG
    Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Collection parent mappings:");
    if (collectionParents_.empty()) {
        Logger::write(Logger::ZONE_WARNING, "RetroFeVLC",
            "No collection parent mappings created");
    }
    else {
        for (const auto& pair : collectionParents_) {
            std::string parents;
            for (const auto& p : pair.second) {
                if (!parents.empty()) parents += ", ";
                parents += p;
            }
            Logger::write(Logger::ZONE_INFO, "RetroFeVLC",
                " " + pair.first + " -> [" + parents + "]");
        }
    }
#endif
}
bool RetroFE::navigateToCollection(const std::string& targetCollection, bool autoEnter, const std::string& savedSelectedItem, const std::string& savedPlaylist, unsigned int savedScrollOffset) {
    std::string startMainBuild = "Main";
    config_.getProperty("startMainBuild", startMainBuild);
#ifdef _DEBUG
    Logger::write(Logger::ZONE_INFO, "RetroFEVLC", "Navigating to collection: " + targetCollection + ", savedSelectedItem: " + (savedSelectedItem.empty() ? "none" : savedSelectedItem) + ", savedPlaylist: " + (savedPlaylist.empty() ? "all" : savedPlaylist) + ", savedScrollOffset: " + std::to_string(savedScrollOffset));
#endif
    if (!currentPage_) {
        Logger::write(Logger::ZONE_ERROR, "RetroFeVLC", "Current page is null, cannot navigate");
        return false;
    }
    // Reset back navigation flag
    pendingBackToMain_ = false;
    // Rebuild menu tree
    buildMenuTree();
    // Build path to target collection (suporta nomes duplicados)
    std::vector<std::string> path;
    bool foundParent = false;

    if (targetCollection != startMainBuild)
    {
        // Função recursiva que procura um caminho até ao startMainBuild
        std::function<bool(const std::string&, std::vector<std::string>&, std::set<std::string>&)> findPathToRoot;

        findPathToRoot = [&](const std::string& current, std::vector<std::string>& currentPath, std::set<std::string>& visited) -> bool
            {
                if (current == startMainBuild)
                    return true;

                if (visited.count(current))
                    return false;

                visited.insert(current);

                auto it = collectionParents_.find(current);
                if (it == collectionParents_.end())
                    return false;

                // Tenta todos os pais possíveis
                for (const std::string& parent : it->second)
                {
                    currentPath.push_back(current);
                    if (findPathToRoot(parent, currentPath, visited))
                        return true;
                    currentPath.pop_back();
                }
                return false;
            };

        std::vector<std::string> tempPath;
        std::set<std::string> visited;

        if (findPathToRoot(targetCollection, tempPath, visited))
        {
            // tempPath está do filho pai. Invertemos para ficar na ordem correta
            path.assign(tempPath.rbegin(), tempPath.rend());
            foundParent = true;
        }
        else
        {
            // Tenta também case-insensitive
            for (const auto& pair : collectionParents_)
            {
                if (Utils::toLower(pair.first) == Utils::toLower(targetCollection))
                {
                    tempPath.clear();
                    visited.clear();
                    if (findPathToRoot(pair.first, tempPath, visited))
                    {
                        path.assign(tempPath.rbegin(), tempPath.rend());
                        foundParent = true;
                        break;
                    }
                }
            }
        }
    }
    // Fallback: Search startMainBuild/menu files and their subdirectories for the target collection
    if (!foundParent && targetCollection != startMainBuild) {
        std::string mainMenuDir = Utils::combinePath(Configuration::absolutePath, "collections", startMainBuild, "menu");
        DIR* dp = opendir(mainMenuDir.c_str());
        if (dp) {
            struct dirent* dirp;
            while ((dirp = readdir(dp)) != NULL) {
                std::string file = dirp->d_name;
                size_t pos = file.find_last_of(".");
                std::string ext = (pos != std::string::npos) ? file.substr(pos) : "";
                if (ext == ".txt") {
                    std::string intermediate = file.substr(0, pos); // e.g., "ARCADES"
                    std::string menuFilePath = Utils::combinePath(mainMenuDir, file);
                    std::ifstream menuFile(menuFilePath);
                    bool isEmpty = true;
                    if (menuFile.is_open()) {
                        std::string line;
                        while (std::getline(menuFile, line)) {
#ifdef _DEBUG
                            Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC", "Raw line in " + menuFilePath + ": '" + line + "'");
#endif
                            line = Utils::filterComments(line);
                            line = Utils::trimEnds(line);
                            if (!line.empty()) {
                                isEmpty = false;
                                std::string childCol = line;
                                size_t eqPos = line.find("=");
                                if (eqPos != std::string::npos) {
                                    childCol = Utils::trimEnds(line.substr(0, eqPos));
                                }
                                if (Utils::toLower(childCol) == Utils::toLower(targetCollection)) {
                                    path = { intermediate, targetCollection };
#ifdef _DEBUG
                                    Logger::write(Logger::ZONE_INFO, "RetroFEVLC", "Found '" + targetCollection + "' in menu file '" + file + "'; path: " + startMainBuild + " -> " + intermediate + " -> " + targetCollection);
#endif
                                    foundParent = true;
                                    break;
                                }
                            }
                        }
                        menuFile.close();
                    }
                    if (isEmpty && !foundParent) {
                        std::string subMenuDir = Utils::combinePath(Configuration::absolutePath, "collections", intermediate, "menu");
                        DIR* subDp = opendir(subMenuDir.c_str());
                        if (subDp) {
                            struct dirent* subDirp;
                            while ((subDirp = readdir(subDp)) != NULL) {
                                std::string subFile = subDirp->d_name;
                                size_t subPos = subFile.find_last_of(".");
                                std::string subExt = (subPos != std::string::npos) ? subFile.substr(subPos) : "";
                                if (subExt == ".txt") {
                                    std::string subCol = subFile.substr(0, subPos);
#ifdef _DEBUG
                                    Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC", "Checking submenu file '" + subFile + "' in " + subMenuDir);
#endif
                                    if (Utils::toLower(subCol) == Utils::toLower(targetCollection)) {
                                        path = { intermediate, targetCollection };
#ifdef _DEBUG
                                        Logger::write(Logger::ZONE_INFO, "RetroFEVLC", "Found '" + targetCollection + "' in submenu file '" + subFile + "' under '" + intermediate + "'; path: " + startMainBuild + " -> " + intermediate + " -> " + targetCollection);
#endif
                                        foundParent = true;
                                        break;
                                    }
                                }
                            }
                            closedir(subDp);
                            if (foundParent) break;
                        }
                        else {
#ifdef _DEBUG
                            Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC", "No menu directory found for collection: " + intermediate);
#endif
                        }
                    }
                    if (foundParent) break;
                }
            }
            closedir(dp);
        }
        if (!foundParent) {
            std::string colPath = Utils::combinePath(Configuration::absolutePath, "collections", targetCollection);
            struct stat st;
            if (stat(colPath.c_str(), &st) == 0 && (st.st_mode & S_IFDIR)) {
#ifdef _DEBUG
                Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Target collection '" + targetCollection + "' not in menu; treating as top-level");
#endif
                path.push_back(targetCollection);
                foundParent = true;
            }
            else {
#ifdef _DEBUG
                Logger::write(Logger::ZONE_ERROR, "RetroFeVLC", "Cannot find '" + targetCollection + "' in any " + startMainBuild + " menu file or submenu; staying in " + startMainBuild);
#endif
                return true;
            }
        }
    }
    // Log the path
    std::string pathStr = path.empty() ? startMainBuild : std::accumulate(path.begin(), path.end(), std::string(startMainBuild),
        [](const std::string& a, const std::string& b) { return a + " -> " + b; });
#ifdef _DEBUG
    Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Navigation path: " + pathStr);
#endif
    // Load startMainBuild collection
    CollectionInfo* mainInfo = getCollection(startMainBuild);
    if (!mainInfo) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_ERROR, "RetroFeVLC", "Failed to load " + startMainBuild + " collection");
#endif
        return false;
    }
    // Clear menu stack and push startMainBuild
    while (currentPage_->getMenuDepth() > 0) {
        currentPage_->popCollection();
    }
    currentPage_->pushCollection(mainInfo);
    config_.setProperty("currentCollection", startMainBuild);
    // Select initial playlist
    bool rememberMenu = false;
    config_.getProperty("rememberMenu", rememberMenu);
    std::string firstPlaylist = savedPlaylist.empty() ? "all" : savedPlaylist;
    if (!rememberMenu) {
        config_.getProperty("firstPlaylist", firstPlaylist);
    }
    currentPage_->selectPlaylist(firstPlaylist);
    if (currentPage_->getPlaylistName() != firstPlaylist) {
        currentPage_->selectPlaylist("all");
    }

    // If target is startMainBuild (menuIndex 0), restore selection and scroll offset immediately
    // so item 0 is never selected or rendered on reload
    if (targetCollection == startMainBuild) {
        if (savedScrollOffset != static_cast<unsigned int>(-1)) {
            currentPage_->setScrollOffsetIndex(savedScrollOffset);
            Logger::write(Logger::ZONE_INFO, "RetroFeVLC",
                "navigateToCollection: Restored scroll offset for " + startMainBuild + ": " + std::to_string(savedScrollOffset));
        }
        if (!savedSelectedItem.empty()) {
            std::vector<Item*>* playlist = currentPage_->getCurrentPlaylist();
            if (playlist) {
                auto it = std::find_if(playlist->begin(), playlist->end(),
                    [&savedSelectedItem](const Item* item) { return item->name == savedSelectedItem; });
                if (it != playlist->end()) {
                    currentPage_->selectItem(*it);
                    Logger::write(Logger::ZONE_INFO, "RetroFeVLC",
                        "navigateToCollection: Selected item in " + startMainBuild + ": " + savedSelectedItem);
                }
            }
        }
    }

    // Populate startup queue
    startupEnterQueue_.clear();
    startupSelectItem_.clear();
    if (targetCollection == startMainBuild && !savedSelectedItem.empty() && autoEnter) {
        std::vector<Item*>* playlist = currentPage_->getCurrentPlaylist();
        bool isNonLeaf = false;
        if (playlist) {
            auto it = std::find_if(playlist->begin(), playlist->end(),
                [&savedSelectedItem](const Item* item) { return item->name == savedSelectedItem; });
            if (it != playlist->end() && !(*it)->leaf) {
                isNonLeaf = true;
            }
        }
        if (isNonLeaf) {
            Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC", "jumpNextBack enabled: Queueing enter-then-back");
            startupEnterQueue_.push_back(savedSelectedItem);
            pendingBackToMain_ = true;
        }
        else {
            startupEnterQueue_.assign(path.begin(), path.end());
            if (!savedSelectedItem.empty()) {
                startupSelectItem_ = savedSelectedItem;
#ifdef _DEBUG
                Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Selecting item without entering: " + startupSelectItem_);
#endif
            }
        }
    }
    else if (!autoEnter && !path.empty()) {
        startupSelectItem_ = path.back();
        path.pop_back();
#ifdef _DEBUG
        Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Selecting collection without entering: " + startupSelectItem_);
#endif
        startupEnterQueue_.assign(path.begin(), path.end());
    }
    else {
        startupEnterQueue_.assign(path.begin(), path.end());
    }
    isAutoNavigating_ = !startupEnterQueue_.empty() || pendingBackToMain_;
    if (currentPage_) currentPage_->setAutoNavigating(isAutoNavigating_);

    // Initialize page
    currentPage_->allocateGraphicsMemory();
    currentPage_->onNewItemSelected();
    currentPage_->reallocateMenuSpritePoints();
    Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC", "Triggering start() for onEnterAction");
    currentPage_->start(); // Triggers onEnterAction
    if (isAutoNavigating_ && currentPage_) {
        currentPage_->completeAnimations();
    }
    // Skip enterMenu and highlightEnter during reload to avoid unwanted artwork
    if (!isReload_) {
        Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC", "Triggering enterMenu() for onMenuEnterAction");
        currentPage_->enterMenu(); // Triggers onMenuEnterAction
        Logger::write(Logger::ZONE_DEBUG, "RetroFeVLC", "Triggering highlightEnter() for onHighlightEnterAction");
        currentPage_->highlightEnter(); // Triggers onHighlightEnterAction
    }
    // Temporarily disable auto-entry if queue is not empty
    enterCollectionAfterReload_ = false;
    enterNumberOfTimes_ = 0;
    // Log queue state
    std::string queueStr = std::accumulate(startupEnterQueue_.begin(), startupEnterQueue_.end(), std::string(),
        [](const std::string& a, const std::string& b) { return a.empty() ? b : a + ", " + b; });
    Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Startup enter queue: [" + (queueStr.empty() ? "empty" : queueStr) + "]");
    Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Startup select item: " + (startupSelectItem_.empty() ? "none" : startupSelectItem_));
    Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "Pending back to " + startMainBuild + ": " + std::to_string(pendingBackToMain_));
   
    //  // Final safety after navigation (critical for list <-> grid changes)
    //if (currentPage_ && currentPage_->getActiveMenu())
    //{
    //    ScrollingList* menu = currentPage_->getActiveMenu();

    //    // Rebuild sprite points for the new layout
    //    menu->freeGraphicsMemory();
    //    menu->destroyItems();
    //    menu->allocateSpritePoints();

    //    // Safe index restore
    //    unsigned int safeIndex = savedScrollOffset;
    //    if (menu->getSize() > 0)
    //    {
    //        safeIndex = std::fmin(safeIndex, menu->getSize() - 1);
    //    }

    //    menu->setSelectedIndex(safeIndex);
    //    menu->allocateGraphicsMemory();

    //    currentPage_->onNewItemSelected();
    //}

    // Reset reload flag
    isReload_ = false;
    return true;
}

void RetroFE::updateSecondaryScreens(float dt)
{
    if (!currentPage_) return;

    for (int i = 1; i < SDL::getNumScreens(); ++i)
    {
        if (!marqueePauseScreen_[i])          // false *run*
            currentPage_->update(dt, true, i);   // new overload: skipPrimary + monitor filter
    }
}

void RetroFE::renderSecondaryScreens()
{
    if (!currentPage_) return;

    SDL_LockMutex(SDL::getMutex());
    for (int i = 1; i < SDL::getNumScreens(); ++i) {
        SDL_Renderer* r = SDL::getRenderer(i);
        if (!r) continue;

        SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
        SDL_RenderClear(r);
        currentPage_->draw(i);  // Only this monitor
        SDL_RenderPresent(r);
    }
    SDL_UnlockMutex(SDL::getMutex());
}

// =====================================================================
//  NATIVE SEARCH OVERLAY IMPLEMENTATION
// =====================================================================

CollectionInfo *RetroFE::getSearchCollection( std::string collectionName )
{
    std::map<std::string, CollectionInfo *>::iterator it = searchCollections_.find( collectionName );
    if ( it != searchCollections_.end( ) )
    {
        return it->second;
    }

    CollectionInfo *info = NULL;
    std::string     path = Utils::combinePath( Configuration::absolutePath, "collections", collectionName );
    struct stat     st;
    if ( metadb_ && stat( path.c_str( ), &st ) == 0 )
    {
        info = getCollection( collectionName );
    }

    searchCollections_[collectionName] = info;
    return info;
}


void RetroFE::searchProgress( const std::string &text )
{
    Logger::write( Logger::ZONE_INFO, "Search", text );
}


void RetroFE::buildSearchEntries( Page *page, std::vector<SearchMatcher::Entry> &entries )
{
    if ( searchIndexBuilt_ && !cachedSearchEntries_.empty() )
    {
        entries = cachedSearchEntries_;
        searchGroups_ = cachedSearchGroups_;
        return;
    }

    entries.clear( );
    searchGroups_.clear( );

    std::vector<std::string> excluded;

    // 1. Read Launcher.json "ExcludedFolders" if present
    std::string launcherJsonPath = Utils::combinePath(Configuration::absolutePath, "Launcher.json");
    std::ifstream jfile(launcherJsonPath.c_str());
    if (jfile.is_open())
    {
        std::string line;
        bool inExcluded = false;
        while (std::getline(jfile, line))
        {
            if (!inExcluded)
            {
                if (line.find("\"ExcludedFolders\"") != std::string::npos)
                {
                    inExcluded = true;
                }
            }
            else
            {
                if (line.find(']') != std::string::npos)
                {
                    inExcluded = false;
                    break;
                }
                size_t q1 = line.find('"');
                if (q1 != std::string::npos)
                {
                    size_t q2 = line.find('"', q1 + 1);
                    if (q2 != std::string::npos)
                    {
                        std::string folder = line.substr(q1 + 1, q2 - q1 - 1);
                        folder = Utils::trimEnds(folder);
                        if (!folder.empty())
                        {
                            excluded.push_back(folder);
                        }
                    }
                }
            }
        }
        jfile.close();
    }

    // 2. Collections listed in settings.conf (search.exclude = A, B, C)
    std::string excludedCsv;
    config_.getProperty( "search.exclude", excludedCsv );
    std::vector<std::string> confExcluded;
    Utils::listToVector( excludedCsv, confExcluded, ',' );
    for ( size_t e = 0; e < confExcluded.size( ); ++e )
    {
        std::string ex = Utils::trimEnds( confExcluded[e] );
        if (!ex.empty()) excluded.push_back(ex);
    }

    // Pattern rules: search.excludePattern = *Collection*, *Settings* etc.

    std::string patternCsv;
    config_.getProperty( "search.excludePattern", patternCsv );
    std::vector<std::string> patterns;
    Utils::listToVector( patternCsv, patterns, ',' );
    for ( size_t e = 0; e < patterns.size( ); ++e )
        patterns[e] = Utils::trimEnds( patterns[e] );
    patterns.erase(
        std::remove( patterns.begin(), patterns.end(), std::string() ),
        patterns.end() );

    bool excludeCollectionName = false;
    config_.getProperty( "search.excludeCollectionName", excludeCollectionName );

    std::string current;
    if ( page && page->getCollection( ) )
    {
        current = page->getCollection( )->name;
    }

    // Wildcard matcher: '*' matches any run, case-insensitive.
    auto wildcardMatch = []( const std::string &name, const std::string &pat )
    {
        std::string n = Utils::toLower( name );
        std::string p = Utils::toLower( pat );
        std::vector<std::string> toks;
        size_t start = 0;
        while ( start <= p.size( ) )
        {
            size_t pos = p.find( '*', start );
            if ( pos == std::string::npos )
            {
                toks.push_back( p.substr( start ) );
                break;
            }
            toks.push_back( p.substr( start, pos - start ) );
            start = pos + 1;
        }
        size_t idx = 0;
        bool first = true;
        for ( size_t t = 0; t < toks.size( ); ++t )
        {
            const std::string &tok = toks[t];
            if ( tok.empty( ) ) continue;
            size_t found = n.find( tok, idx );
            if ( found == std::string::npos ) return false;
            if ( first && !p.empty( ) && p[0] != '*' && found != 0 ) return false;
            idx = found + tok.size( );
            first = false;
        }
        if ( !p.empty( ) && p[p.size( ) - 1] != '*' && idx != n.size( ) ) return false;
        return true;
    };

    // All collection folders (skip names starting with '_')
    std::vector<std::string> names;
    std::string collectionsPath = Utils::combinePath( Configuration::absolutePath, "collections" );
    DIR *dp = opendir( collectionsPath.c_str( ) );
    if ( dp )
    {
        struct dirent *dirp;
        while ( ( dirp = readdir( dp ) ) != NULL )
        {
            std::string name = dirp->d_name;
            if ( name != "." && name != ".." && !name.empty( ) && name[0] != '_' )
            {
                bool skip = false;
                std::string nameLC = Utils::toLower( name );
                for ( size_t e = 0; e < excluded.size( ); ++e )
                {
                    const std::string &ex = excluded[e];
                    if ( ex.find( '*' ) != std::string::npos )
                    {
                        if ( wildcardMatch( name, ex ) ) { skip = true; break; }
                    }
                    else if ( nameLC == Utils::toLower( ex ) ) { skip = true; break; }
                }
                if ( !skip )
                {
                    for ( size_t e = 0; e < patterns.size( ) && !skip; ++e )
                    {
                        if ( wildcardMatch( name, patterns[e] ) ) skip = true;
                    }
                }
                if ( !skip && excludeCollectionName )
                {
                    if ( Utils::toLower( name ).find( "collection" ) != std::string::npos ) skip = true;
                }
                if ( skip ) continue;

                std::string dirPath = Utils::combinePath( collectionsPath, name );
                struct stat collectionStat;
                if ( stat( dirPath.c_str(), &collectionStat ) != 0 ||
                     ( collectionStat.st_mode & S_IFMT ) != S_IFDIR )
                    continue;

                // Sub-collections or mirrors lacking settings.conf (e.g. ALL GAMES, Ard Denshi, Search Collection) must be ignored
                std::string settingsFile = Utils::combinePath( dirPath, "settings.conf" );
                if ( !Utils::fileExists( settingsFile ) )
                    continue;

                std::ifstream menu( Utils::combinePath( dirPath, "menu.txt" ).c_str( ) );
                if ( menu.good( ) ) continue;
                {
                    DIR *mdp;
                    struct dirent *mdirp;
                    bool menuSubdir = false;
                    mdp = opendir( Utils::combinePath( dirPath, "menu" ).c_str( ) );
                    if ( mdp )
                    {
                        while ( ( mdirp = readdir( mdp ) ) != NULL )
                        {
                            std::string m = mdirp->d_name;
                            if ( m != "." && m != ".." && m.size() > 4 &&
                                 Utils::toLower( m.substr( m.size() - 4 ) ) == ".txt" )
                            {
                                menuSubdir = true;
                                break;
                            }
                        }
                        closedir( mdp );
                    }
                    if ( menuSubdir ) continue;
                }

                names.push_back( name );
            }
        }
        closedir( dp );
    }
    std::sort( names.begin( ), names.end( ) );

    // current collection first
    std::vector<std::string>::iterator cur = std::find( names.begin( ), names.end( ), current );
    if ( cur != names.end( ) )
    {
        std::string c = *cur;
        names.erase( cur );
        names.insert( names.begin( ), c );
    }

    // Load Favorites & Last Played sets for priority ranking
    std::unordered_set<std::string> favSet;
    std::string favPath = Utils::combinePath( Configuration::absolutePath, "collections", "FAVORITES", "playlists", "favorites.txt" );
    std::ifstream favFile( favPath.c_str( ) );
    if ( favFile.is_open( ) )
    {
        std::string line;
        while ( std::getline( favFile, line ) )
        {
            std::string t = SearchMatcher::trim( line );
            if ( !t.empty( ) )
            {
                favSet.insert( t );
                size_t colon = t.find( ':' );
                if ( colon != std::string::npos ) favSet.insert( t.substr( colon + 1 ) );
            }
        }
    }

    std::unordered_set<std::string> recentSet;
    std::string recPath = Utils::combinePath( Configuration::absolutePath, "collections", "LASTPLAYED", "playlists", "lastplayed.txt" );
    std::ifstream recFile( recPath.c_str( ) );
    if ( recFile.is_open( ) )
    {
        std::string line;
        while ( std::getline( recFile, line ) )
        {
            std::string t = SearchMatcher::trim( line );
            if ( !t.empty( ) )
            {
                recentSet.insert( t );
                size_t colon = t.find( ':' );
                if ( colon != std::string::npos ) recentSet.insert( t.substr( colon + 1 ) );
            }
        }
    }

    for ( size_t n = 0; n < names.size( ); ++n )
    {
        const std::string &name = names[n];

        if ( searchCollections_.find( name ) == searchCollections_.end( ) )
        {
            searchProgress( "SEARCH: LOADING " + name + " (" + std::to_string( n + 1 ) + "/" + std::to_string( names.size( ) ) + ")" );
        }

        CollectionInfo *info = getSearchCollection( name );
        if ( !info || info->items.empty( ) ) continue;

        int group = -1;
        for ( unsigned int j = 0; j < info->items.size( ); ++j )
        {
            Item *game = info->items[j];
            if ( !game || !game->leaf ) continue;

            if ( group < 0 )
            {
                group = static_cast<int>( searchGroups_.size( ) );
                searchGroups_.push_back( name );
            }
            std::string origCol = ( game->collectionInfo && !game->collectionInfo->name.empty() )
                                  ? game->collectionInfo->name
                                  : name;

            bool isFav = ( favSet.count( game->name ) > 0 || favSet.count( "_" + origCol + ":" + game->name ) > 0 );
            bool isRec = ( recentSet.count( game->name ) > 0 || recentSet.count( "_" + origCol + ":" + game->name ) > 0 );
            SearchMatcher::Entry en = SearchMatcher::makeEntry( j, game->title, game->fullTitle, game->name,
                                                                 game->manufacturer, game->year, game->genre,
                                                                 group, origCol, isFav, isRec );
            entries.push_back( en );
        }
    }

    cachedSearchEntries_ = entries;
    cachedSearchGroups_ = searchGroups_;
    searchIndexBuilt_ = true;
    search_.setStatus( "" );
}


RetroFE::RETROFE_STATE RetroFE::runOptionsMenu(Page* page)
{
    RETROFE_STATE nextState = RETROFE_IDLE;

    bool menuPaused = false;
    if (currentPage_ && !currentPage_->isPaused()) {
        currentPage_->pause();
        menuPaused = true;
    }

    OptionsMenu optionsMenu(config_, currentPage_);
    activeOptionsMenu_ = &optionsMenu;

    bool settingsChanged = optionsMenu.show(SDL::getRenderer(0), [this]() {
        render();
    }, &input_);

    activeOptionsMenu_ = nullptr;

    if (currentPage_ && menuPaused && currentPage_->isPaused()) {
        currentPage_->pause(); // Unpause! Resume video and animations seamlessly!
        menuPaused = false;
    }

    // Flush any pending events from the menu session
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        input_.update(e);
    }
    input_.resetStates();
    currentTime_ = static_cast<float>(SDL_GetTicks()) / 1000.0f;
    keyLastTime_ = currentTime_;
    suppressNextBack_ = true;

    if (optionsMenu.isQuitSelected()) {
        Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "OptionsMenu signaled quit");
        reboot_ = false;
        attract_.reset();
        if (currentPage_) currentPage_->stop();
        nextState = RETROFE_QUIT_REQUEST;
    }
    else if (optionsMenu.isSignOutSelected()) {
        Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "OptionsMenu signaled Sign Out");
#ifdef WIN32
        if (EnableShutdownPrivilege()) {
            ExitWindowsEx(EWX_LOGOFF | EWX_FORCE, 0);
        }
#endif
        reboot_ = false;
        if (currentPage_) currentPage_->stop();
        nextState = RETROFE_QUIT_REQUEST;
    }
    else if (optionsMenu.isRestartSelected()) {
        Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "OptionsMenu signaled Restart");
#ifdef WIN32
        if (EnableShutdownPrivilege()) {
            ExitWindowsEx(EWX_REBOOT | EWX_FORCE, 0);
        }
#endif
        reboot_ = true;
        if (currentPage_) currentPage_->stop();
        nextState = RETROFE_QUIT_REQUEST;
    }
    else if (optionsMenu.isShutDownSelected()) {
        Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "OptionsMenu signaled Shut Down");
#ifdef WIN32
        if (EnableShutdownPrivilege()) {
            ExitWindowsEx(EWX_POWEROFF | EWX_FORCE, 0);
        }
#endif
        reboot_ = false;
        if (currentPage_) currentPage_->stop();
        nextState = RETROFE_QUIT_REQUEST;
    }
    else if (optionsMenu.hasSavedChanges()) {
        Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "OptionsMenu: layout changes saved, reloading layout");
        isReloading_ = true;
        renderLoadingScreen();

        reloadLayout();
        lastLayoutModTime_ = Utils::getFileModificationTime(layoutFilePath_);
        layoutCheckTimer_ = 0.0f;
        input_.resetStates();
        nextState = RETROFE_IDLE;
    }
    else if (settingsChanged) {
        Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "OptionsMenu requested reboot due to settings change");
        reboot_ = true;
        if (currentPage_) currentPage_->stop();
        nextState = RETROFE_QUIT_REQUEST;
    }
    else {
        Logger::write(Logger::ZONE_INFO, "RetroFeVLC", "OptionsMenu exited without changes");
        nextState = RETROFE_IDLE;
    }

    return nextState;
}


RetroFE::RETROFE_STATE RetroFE::runSearch( Page *page )
{
    RETROFE_STATE state = RETROFE_IDLE;

    if ( ( currentPage_ && currentPage_->isSearchActive( ) ) || ( currentPage_->getCollection( ) && ( currentPage_->getCollection( )->name == "SearchResults" || currentPage_->getCollection( )->name == "Search results" ) ) )
    {
        Logger::write( Logger::ZONE_WARNING, "RetroFE", "Search requested while already on a search results page; ignoring" );
        return RETROFE_IDLE;
    }

    bool searchPaused = false;
    if ( currentPage_ && !currentPage_->isPaused( ) )
    {
        currentPage_->pause( );
        searchPaused = true;
    }

    searchPendingItem_ = NULL;
    searchJumpIndex_   = -1;

    std::string searchMode = "list";
    config_.getProperty( "searchMode", searchMode );
    search_.setListMode( searchMode == "list" );

    std::string searchFont;
    if ( config_.getProperty( "searchFont", searchFont ) && !searchFont.empty( ) )
    {
        bool absolute = ( searchFont.size( ) > 1 && searchFont[1] == ':' ) || searchFont[0] == '/' || searchFont[0] == '\\';
        if ( !absolute ) searchFont = Utils::combinePath( Configuration::absolutePath, searchFont );
    }
    search_.setFontPath( searchFont );

    Page *activePage = page ? page : currentPage_;
    if ( activePage )
    {
        const SearchConfig &searchCfg = activePage->getSearchConfig( );
        search_.setConfig( searchCfg );
        if ( !searchCfg.font.empty( ) )
        {
            search_.setFontPath( searchCfg.font );
        }
    }

    std::vector<SearchMatcher::Entry> entries;
    buildSearchEntries( page, entries );
    search_.open( entries );
    search_.setCurrentCollection( activePage ? activePage->getCollectionName( ) : "" );
    Logger::write( Logger::ZONE_INFO, "RetroFE", "Search overlay opened (" + std::to_string( search_.itemCount( ) ) +
                   " items, " + std::to_string( searchGroups_.size( ) ) + " collections searched)" );

    Uint32 searchFadeStart = 0;
    bool searchFadeRunning = false;

    while ( search_.isActive( ) )
    {
        SDL_Event e;
        while ( SDL_PollEvent( &e ) )
        {
            search_.handleEvent( e, input_ );
        }

        // Keep page animations & videos frozen while search overlay is active (matching OptionsMenu)

        if ( search_.isListMode( ) && search_.isClosing( ) )
        {
            if ( !searchFadeRunning )
            {
                searchFadeRunning = true;
                searchFadeStart = SDL_GetTicks( );
            }
            const Uint32 elapsed = SDL_GetTicks( ) - searchFadeStart;
            searchFadeAlpha_ = elapsed >= 200 ? 255 : static_cast<int>( elapsed * 255 / 200 );
        }

        render( );

        if ( searchFadeRunning && searchFadeAlpha_ == 255 )
        {
            search_.completeClose( );
        }
        SDL_Delay( 16 );
    }

    // Persist mode choice for subsequent runs
    config_.setProperty( "searchMode", search_.isListMode( ) ? "list" : "view" );

    if ( ( SearchMatcher::trim( search_.query( ) ).size( ) >= 3 || !search_.getActiveGenreFilter( ).empty( ) ) && search_.resultCount( ) > 0 )
    {
        saveSearchCollectionSubs( );
    }

    if ( search_.isListMode( ) )
    {
        searchLaunchItem_ = NULL;

        if ( search_.wasChosen( ) )
        {
            const int group = search_.chosenGroup( );
            const unsigned int index = search_.chosenIndex( );

            if ( group >= 0 && group < static_cast<int>( searchGroups_.size( ) ) )
            {
                CollectionInfo *info = getSearchCollection( searchGroups_[group] );

                if ( info && index < info->items.size( ) )
                {
                    searchLaunchItem_ = info->items[index];
                }
            }
        }

        if ( !searchLaunchItem_ )
        {
            searchFadeAlpha_ = 0;
        }

        if ( searchLaunchItem_ )
        {
            state = RETROFE_LAUNCH_ENTER;
        }
        else
        {
            if ( currentPage_ && searchPaused && currentPage_->isPaused( ) )
            {
                currentPage_->pause( );
                searchPaused = false;
            }
            state = RETROFE_IDLE;
        }
    }
    else if ( search_.wasChosen( ) && ( SearchMatcher::trim( search_.query( ) ).size( ) >= 3 || !search_.getActiveGenreFilter( ).empty( ) ) )
    {
        std::string query = search_.query( );
        std::vector<Item *> hits;
        for ( size_t i = 0; i < search_.resultCount( ); ++i )
        {
            const SearchMatcher::Entry &en = search_.resultEntry( i );
            if ( en.group < 0 ) continue;
            if ( en.group >= static_cast<int>( searchGroups_.size( ) ) ) continue;

            CollectionInfo *info = getSearchCollection( searchGroups_[en.group] );
            if ( !info ) continue;

            if ( en.index < info->items.size( ) )
            {
                Item *item = info->items[en.index];
                if ( item )
                {
                    if ( !item->collectionInfo )
                        item->collectionInfo = info;
                    hits.push_back( item );
                }
            }
        }

        if ( !hits.empty( ) )
        {
            unsigned int selectedHitIndex = 0;
            if ( search_.wasChosen( ) )
            {
                int chosenGrp = search_.chosenGroup( );
                unsigned int chosenIdx = search_.chosenIndex( );
                for ( size_t i = 0; i < search_.resultCount( ); ++i )
                {
                    const SearchMatcher::Entry &en = search_.resultEntry( i );
                    if ( en.group == chosenGrp && en.index == chosenIdx )
                    {
                        selectedHitIndex = static_cast<unsigned int>( i );
                        break;
                    }
                }
            }
            if ( selectedHitIndex >= hits.size( ) )
            {
                selectedHitIndex = 0;
            }

            if ( searchPageCollection_ )
            {
                searchPageCollection_->playlists.clear( );
                searchPageCollection_->ownsItems = false;
                delete searchPageCollection_;
                searchPageCollection_ = NULL;
            }
            searchPageCollection_ = new CollectionInfo( "SearchResults", "", "", "", "" );
            searchPageCollection_->items = hits;
            searchPageCollection_->menusort   = false;
            searchPageCollection_->ownsItems  = false;
            searchPageCollection_->subsSplit  = false;
            searchPageCollection_->playlists[ "all" ] = & searchPageCollection_->items;
            searchPageCollection_->playlists[ "favorites" ] = new std::vector<Item *>( );

            if ( currentPage_ && searchPaused && currentPage_->isPaused( ) )
            {
                currentPage_->pause( );
                searchPaused = false;
            }

            if ( searchTargetItem_ ) delete searchTargetItem_;
            searchTargetItem_ = new Item( );
            searchTargetItem_->name = "_search";
            std::string searchDisplayTitle = "Search results";
            if ( !search_.getActiveGenreFilter( ).empty( ) )
            {
                if ( !search_.query( ).empty( ) ) searchDisplayTitle = search_.getActiveGenreFilter( ) + " (" + search_.query( ) + ")";
                else searchDisplayTitle = "Genre: " + search_.getActiveGenreFilter( );
            }
            searchTargetItem_->title = searchDisplayTitle;
            searchTargetItem_->fullTitle = searchDisplayTitle;
            searchTargetItem_->leaf = false;
            searchTargetItem_->collectionInfo = currentPage_->getCollection( );

            searchJumpIndex_ = static_cast<int>( selectedHitIndex );
            nextPageItem_    = searchTargetItem_;
            state            = RETROFE_NEXT_PAGE_REQUEST;
        }
        else
        {
            Logger::write( Logger::ZONE_INFO, "RetroFE", "Search had no matches; closing" );
            if ( currentPage_ && searchPaused && currentPage_->isPaused( ) )
            {
                currentPage_->pause( );
                searchPaused = false;
            }
            state = RETROFE_IDLE;
        }
    }
    else
    {
        Logger::write( Logger::ZONE_INFO, "RetroFE", "Search overlay closed without a selection" );
        if ( currentPage_ && searchPaused && currentPage_->isPaused( ) )
        {
            currentPage_->pause( );
            searchPaused = false;
        }
        state = RETROFE_IDLE;
    }

    SDL_Event e;
    while ( SDL_PollEvent( &e ) )
    {
        input_.update( e );
    }
    input_.resetStates( );
    currentTime_ = static_cast<float>( SDL_GetTicks( ) ) / 1000.0f;
    keyLastTime_ = currentTime_;
    suppressNextBack_ = true;

    return state;
}


void RetroFE::saveSearchCollectionSubs( )
{
    if ( ( SearchMatcher::trim( search_.query( ) ).size( ) < 3 && search_.getActiveGenreFilter( ).empty( ) ) || search_.resultCount( ) == 0 )
        return;

    std::string searchColDir = Utils::combinePath( Configuration::absolutePath, "collections", "Search Collection" );
    struct stat st;
    if ( stat( searchColDir.c_str( ), &st ) != 0 || ( st.st_mode & S_IFMT ) != S_IFDIR )
    {
        Logger::write( Logger::ZONE_WARNING, "RetroFE", "Search Collection directory not found: " + searchColDir );
        return;
    }

    // 1. Delete all existing *.sub files in collections/Search Collection/
    DIR *dp = opendir( searchColDir.c_str( ) );
    if ( dp )
    {
        struct dirent *dirp;
        std::vector<std::string> subFilesToDelete;
        while ( ( dirp = readdir( dp ) ) != NULL )
        {
            std::string fname = dirp->d_name;
            if ( fname.size( ) > 4 && Utils::toLower( fname.substr( fname.size( ) - 4 ) ) == ".sub" )
            {
                subFilesToDelete.push_back( Utils::combinePath( searchColDir, fname ) );
            }
        }
        closedir( dp );

        for ( size_t i = 0; i < subFilesToDelete.size( ); ++i )
        {
#ifdef WIN32
            DeleteFileA( subFilesToDelete[i].c_str( ) );
#else
            remove( subFilesToDelete[i].c_str( ) );
#endif
        }
    }

    // 2. Group matching games by their original system/collection
    std::map<std::string, std::vector<std::string>> systemGames;
    std::set<std::pair<std::string, std::string>> seen;

    for ( size_t i = 0; i < search_.resultCount( ); ++i )
    {
        const SearchMatcher::Entry &en = search_.resultEntry( i );
        if ( en.group < 0 || en.group >= static_cast<int>( searchGroups_.size( ) ) ) continue;

        CollectionInfo *info = getSearchCollection( searchGroups_[en.group] );
        if ( !info || en.index >= info->items.size( ) ) continue;

        Item *item = info->items[en.index];
        if ( !item || !item->leaf ) continue;

        std::string systemName = ( item->collectionInfo && !item->collectionInfo->name.empty( ) )
                                 ? item->collectionInfo->name
                                 : searchGroups_[en.group];
        if ( systemName.empty( ) ) continue;

        if ( seen.insert( std::make_pair( systemName, item->name ) ).second )
        {
            systemGames[systemName].push_back( item->name );
        }
    }

    // 3. Write each <systemName>.sub file
    for ( auto it = systemGames.begin( ); it != systemGames.end( ); ++it )
    {
        const std::string &sysName = it->first;
        const std::vector<std::string> &games = it->second;
        if ( games.empty( ) ) continue;

        std::string subFilePath = Utils::combinePath( searchColDir, sysName + ".sub" );
        std::ofstream subFile( subFilePath.c_str( ) );
        if ( subFile.is_open( ) )
        {
            for ( size_t g = 0; g < games.size( ); ++g )
            {
                subFile << games[g] << "\n";
            }
            subFile.close( );
        }
    }

    Logger::write( Logger::ZONE_INFO, "RetroFE", "Search Collection updated with " +
                   std::to_string( systemGames.size( ) ) + " system .sub files (" +
                   std::to_string( search_.resultCount( ) ) + " items)" );
}
