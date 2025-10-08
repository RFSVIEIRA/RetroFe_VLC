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


#include "SDL.h"
#include "Database/Configuration.h"
#include "Utility/Log.h"
#include <SDL_mixer.h>
#include <sstream>
#include "Graphics/Page.h"
// #include "Utility/Utils.h"



// Static member definitions
Configuration*             SDL::config_ = nullptr;
std::vector<SDL_Window*>   SDL::window_;
std::vector<SDL_Renderer*> SDL::renderer_;
SDL_mutex*                 SDL::mutex_ = NULL;
std::vector<int>           SDL::displayWidth_;
std::vector<int>           SDL::displayHeight_;
std::vector<int>           SDL::windowWidth_;
std::vector<int>           SDL::windowHeight_;
std::vector<bool>          SDL::fullscreen_;
std::vector<int>           SDL::rotation_;
std::vector<bool>          SDL::mirror_;
int                        SDL::numScreens_ = 1;
int                        SDL::numDisplays_ = 1;


bool SDL::initialize(Configuration& config)
{
    config_ = &config;
    int audioRate = MIX_DEFAULT_FREQUENCY;
    Uint16 audioFormat = MIX_DEFAULT_FORMAT;
    int audioChannels = 1;
    int audioBuffers = 4096;
    bool hideMouse;
    std::string scaleQuality = "0";
    std::string preScaleQuality = "off";

    config.getProperty("scaleQuality", scaleQuality);
    config.getProperty("4kPreScaleQuality", preScaleQuality);
   

    Logger::write(Logger::ZONE_INFO, "SDL", "Initializing");

    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");

    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0)
    {
        std::string error = SDL_GetError();
        Logger::write(Logger::ZONE_ERROR, "SDL", "Initialize failed: " + error);
        return false;
    }

    if (config.getProperty("hideMouse", hideMouse))
    {
        SDL_ShowCursor(hideMouse ? SDL_FALSE : SDL_TRUE);
    }

    // Determine effective scale quality
    std::string effectiveScaleQuality = scaleQuality;
    bool is4K = false;
    numDisplays_ = SDL_GetNumVideoDisplays();
    for (int i = 0; i < numDisplays_; i++)
    {
        SDL_Rect bounds;
        if (SDL_GetDisplayBounds(i, &bounds) == 0 && (bounds.w >= 3840 || bounds.h >= 2160))
        {
            is4K = true;
            break;
        }
    }

    if (scaleQuality == "auto") {
        effectiveScaleQuality = is4K ? "2" : "0";
        Logger::write(Logger::ZONE_INFO, "SDL", "scaleQuality=auto resolved to " + effectiveScaleQuality);
    }

    // Apply preScaleQuality overrides
    if (preScaleQuality == "smooth") {
        effectiveScaleQuality = "1";
        Logger::write(Logger::ZONE_INFO, "SDL", "scaleQuality overridden to 1 due to 4kPreScaleQuality=smooth");
    }
    else if (effectiveScaleQuality == "2") {
        preScaleQuality = "off";
        Logger::write(Logger::ZONE_DEBUG, "SDL", "4kPreScaleQuality overridden to off due to scaleQuality=2");
    }
    else if (effectiveScaleQuality == "0" && preScaleQuality == "smooth") {
        preScaleQuality = "fast";
        Logger::write(Logger::ZONE_DEBUG, "SDL", "4kPreScaleQuality overridden to fast due to scaleQuality=0");
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, effectiveScaleQuality.c_str());

    Logger::write(Logger::ZONE_INFO, "SDL", "Final scale quality set to " + effectiveScaleQuality);
    Logger::write(Logger::ZONE_INFO, "SDL", "Final 4kPreScaleQuality set to " + preScaleQuality);

    // Auto detect displays
    numDisplays_ = SDL_GetNumVideoDisplays();
    Logger::write(Logger::ZONE_INFO, "SDL", "Number of displays found: " + std::to_string(numDisplays_));
    numScreens_ = numDisplays_; // Default to detected displays
    config.getProperty("numScreens", numScreens_); // Override if specified
    if (numScreens_ <= 0)
    {
        Logger::write(Logger::ZONE_ERROR, "SDL", "Number of requested displays <= 0; setting to 1");
        numScreens_ = 1;
    }
    if (numScreens_ > numDisplays_)
    {
        Logger::write(Logger::ZONE_WARNING, "SDL", "numScreens (" + std::to_string(numScreens_) + ") exceeds available displays (" + std::to_string(numDisplays_) + "); capping to " + std::to_string(numDisplays_));
        numScreens_ = numDisplays_;
    }
    Logger::write(Logger::ZONE_INFO, "SDL", "Number of displays requested: " + std::to_string(numScreens_));

    // Clear vectors
    window_.clear();
    renderer_.clear();
    displayWidth_.clear();
    displayHeight_.clear();
    windowWidth_.clear();
    windowHeight_.clear();
    fullscreen_.clear();
    rotation_.clear();
    mirror_.clear();

    // Initialize windows and renderers
    for (int i = 0; i < numScreens_; ++i)
    {
        SDL_DisplayMode mode;
        bool windowBorder = false;
        bool windowResize = false;
        Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;

        config.getProperty("windowBorder", windowBorder);
        if (!windowBorder) windowFlags |= SDL_WINDOW_BORDERLESS;
        config.getProperty("windowResize", windowResize);
        if (windowResize) windowFlags |= SDL_WINDOW_RESIZABLE;

        int screenNum = i;
        config.getProperty("screenNum" + std::to_string(i), screenNum);

        if (SDL_GetCurrentDisplayMode(screenNum, &mode) != 0)
        {
            if (i == 0)
            {
                Logger::write(Logger::ZONE_ERROR, "SDL", "Display " + std::to_string(screenNum) + " does not exist.");
                return false;
            }
            else
            {
                Logger::write(Logger::ZONE_WARNING, "SDL", "Display " + std::to_string(screenNum) + " does not exist; skipping.");
                windowWidth_.push_back(0);
                windowHeight_.push_back(0);
                displayWidth_.push_back(0);
                displayHeight_.push_back(0);
                window_.push_back(NULL);
                renderer_.push_back(NULL);
                fullscreen_.push_back(false);
                rotation_.push_back(0);
                mirror_.push_back(false);
                continue;
            }
        }

        windowWidth_.push_back(mode.w);
        displayWidth_.push_back(mode.w);
        std::string hString = "stretch"; // Default for horizontal
        if (i == 0) config.getProperty("horizontal", hString); // Try global first for primary
        config.getProperty("horizontal" + std::to_string(i), hString); // Override with per-monitor
        if (hString != "stretch" && !config.getProperty("horizontal" + std::to_string(i), windowWidth_[i]) && (i != 0 || !config.getProperty("horizontal", windowWidth_[i])))
        {
            Logger::write(Logger::ZONE_ERROR, "Configuration", "Invalid property value for \"horizontal\"" + std::to_string(i) + "; must be 'stretch' or a valid width");
            return false;
        }

        windowHeight_.push_back(mode.h);
        displayHeight_.push_back(mode.h);
        std::string vString = "stretch"; // Default for vertical
        if (i == 0) config.getProperty("vertical", vString); // Try global first for primary
        config.getProperty("vertical" + std::to_string(i), vString); // Override with per-monitor
        if (vString != "stretch" && !config.getProperty("vertical" + std::to_string(i), windowHeight_[i]) && (i != 0 || !config.getProperty("vertical", windowHeight_[i])))
        {
            Logger::write(Logger::ZONE_ERROR, "Configuration", "Invalid property value for \"vertical\"" + std::to_string(i) + "; must be 'stretch' or a valid height");
            return false;
        }

        bool fullscreen = false; // Default to no fullscreen
        if (!config.getProperty("fullscreen" + std::to_string(i), fullscreen)) // Try per-monitor first
            config.getProperty("fullscreen", fullscreen); // Fall back to global
        fullscreen_.push_back(fullscreen);

        if (fullscreen_[i])
        {
#if defined(WIN32) || (_WIN64)
            windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
#else
            windowFlags |= SDL_WINDOW_FULLSCREEN;
#endif
        }

        int rotation = 0;
        config.getProperty("rotation" + std::to_string(i), rotation);
        Logger::write(Logger::ZONE_INFO, "Configuration", "Setting rotation for screen " + std::to_string(i) + " to " + std::to_string(rotation * 90) + " degrees.");
        rotation_.push_back(rotation);

        bool mirror = false;
        config.getProperty("mirror" + std::to_string(i), mirror);
        if (mirror)
            Logger::write(Logger::ZONE_INFO, "Configuration", "Setting mirror mode for screen " + std::to_string(i) + ".");
        mirror_.push_back(mirror);

        std::string fullscreenStr = fullscreen_[i] ? "yes" : "no";
        std::stringstream ss;
        ss << "Creating " << windowWidth_[i] << "x" << windowHeight_[i] << " window (fullscreen: " << fullscreenStr << ") on display " << screenNum;
        Logger::write(Logger::ZONE_INFO, "SDL", ss.str());
        window_.push_back(SDL_CreateWindow("RetroFE", SDL_WINDOWPOS_CENTERED_DISPLAY(screenNum), SDL_WINDOWPOS_CENTERED_DISPLAY(screenNum), windowWidth_[i], windowHeight_[i], windowFlags));

        if (!window_[i])
        {
            std::string error = SDL_GetError();
            if (i == 0)
            {
                Logger::write(Logger::ZONE_ERROR, "SDL", "Create window " + std::to_string(i) + " on display " + std::to_string(screenNum) + " failed: " + error);
                return false;
            }
            else
            {
                Logger::write(Logger::ZONE_WARNING, "SDL", "Create window " + std::to_string(i) + " on display " + std::to_string(screenNum) + " failed: " + error);
                window_[i] = NULL;
            }
        }
        else
        {
            bool vSync = false;
            config.getProperty("vSync", vSync);
            renderer_.push_back(SDL_CreateRenderer(window_[i], -1, SDL_RENDERER_ACCELERATED | (vSync ? SDL_RENDERER_PRESENTVSYNC : 0)));
            if (!renderer_[i])
            {
                std::string error = SDL_GetError();
                Logger::write(Logger::ZONE_ERROR, "SDL", "Create renderer " + std::to_string(i) + " failed: " + error);
                return false;
            }

            SDL_RendererInfo info;
            if (SDL_GetRendererInfo(renderer_[i], &info) == 0)
            {
                Logger::write(Logger::ZONE_INFO, "SDL", "Renderer " + std::to_string(i) + ": " + info.name + " (accelerated: " + (info.flags & SDL_RENDERER_ACCELERATED ? "yes" : "no") + ")");
            }

            SDL_RenderSetLogicalSize(renderer_[i], windowWidth_[i], windowHeight_[i]);
        }
    }

    bool minimize_on_focus_loss_ = false;
    config.getProperty("minimize_on_focus_loss", minimize_on_focus_loss_);
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, minimize_on_focus_loss_ ? "1" : "0", SDL_HINT_OVERRIDE);

    mutex_ = SDL_CreateMutex();
    if (!mutex_)
    {
        std::string error = SDL_GetError();
        Logger::write(Logger::ZONE_ERROR, "SDL", "Mutex creation failed: " + error);
        return false;
    }

    if (Mix_OpenAudio(audioRate, audioFormat, audioChannels, audioBuffers) == -1)
    {
        std::string error = Mix_GetError();
        Logger::write(Logger::ZONE_WARNING, "SDL", "Audio initialize failed: " + error);
    }

    return true;
}

bool SDL::deInitialize()
{
    Logger::write(Logger::ZONE_INFO, "SDL", "DeInitializing");
    config_ = nullptr;

    Mix_CloseAudio();
    Mix_Quit();

    if (mutex_)
    {
        SDL_DestroyMutex(mutex_);
        mutex_ = NULL;
    }

    for (size_t i = 0; i < renderer_.size(); ++i)
    {
        if (renderer_[i]) SDL_DestroyRenderer(renderer_[i]);
    }
    for (size_t i = 0; i < window_.size(); ++i)
    {
        if (window_[i]) SDL_DestroyWindow(window_[i]);
    }

    renderer_.clear();
    window_.clear();
    displayWidth_.clear();
    displayHeight_.clear();
    windowWidth_.clear();
    windowHeight_.clear();
    fullscreen_.clear();
    rotation_.clear();
    mirror_.clear();

    SDL_ShowCursor(SDL_TRUE);
    SDL_Quit();

    return true;
}

SDL_Renderer* SDL::getRenderer(int index)
{
    return (index < numDisplays_ ? renderer_[index] : renderer_[0]);
}

SDL_mutex* SDL::getMutex()
{
    return mutex_;
}

SDL_Window* SDL::getWindow(int index)
{
    return (index < numDisplays_ ? window_[index] : window_[0]);
}


// Render a copy of a texture
bool SDL::renderCopy(SDL_Texture* texture, float alpha, SDL_Rect* src, SDL_Rect* dest, ViewInfo& viewInfo, int layoutWidth, int layoutHeight)
{
    if (alpha == 0 || viewInfo.Monitor >= numScreens_ || !renderer_[viewInfo.Monitor])
        return true;

    int renderWidth, renderHeight;
    SDL_GetRendererOutputSize(renderer_[viewInfo.Monitor], &renderWidth, &renderHeight);

    float dpiScaleX = (float)renderWidth / windowWidth_[viewInfo.Monitor];
    float dpiScaleY = (float)renderHeight / windowHeight_[viewInfo.Monitor];

    float windowWidth = (float)windowWidth_[viewInfo.Monitor];
    float windowHeight = (float)windowHeight_[viewInfo.Monitor];
    float layoutWidthF = (float)layoutWidth;
    float layoutHeightF = (float)layoutHeight;

    // Initialize scaling and offsets
    float scaleX = windowWidth / layoutWidthF;
    float scaleY = windowHeight / layoutHeightF;
    float offsetX = 0;
    float offsetY = 0;

    // Apply layout scaling and mode
    std::string layoutMode = "stretch";
    float layoutScale = 1.0f;
    if (config_) {
        config_->getProperty("layoutScaleMode", layoutMode);
        config_->getProperty("layoutZoomMode", layoutScale);

        // Check for per-monitor overrides
        std::string layoutModeKey = "layoutScaleMode" + std::to_string(viewInfo.Monitor);
        std::string layoutScaleKey = "layoutZoomMode" + std::to_string(viewInfo.Monitor);
        std::string perMonitorLayoutMode;
        float perMonitorLayoutScale = 0.0f;

        if (config_->getProperty(layoutModeKey, perMonitorLayoutMode)) {
            layoutMode = perMonitorLayoutMode;
        }
        if (config_->getProperty(layoutScaleKey, perMonitorLayoutScale) && perMonitorLayoutScale > 0) {
            layoutScale = perMonitorLayoutScale;
        }

        if (layoutScale <= 0) {
            Logger::write(Logger::ZONE_WARNING, "SDL", "Invalid layoutScale " + std::to_string(layoutScale) + " for monitor " + std::to_string(viewInfo.Monitor) + ", defaulting to 1.0");
            layoutScale = 1.0f;
        }
    }

    // Adjust scaling based on layout mode
    if (layoutMode == "stretch") {
        scaleX = (windowWidth / layoutWidthF) * layoutScale;
        scaleY = (windowHeight / layoutHeightF) * layoutScale;
        offsetX = 0;
        offsetY = 0;
    }
    else if (layoutMode == "fit") {
        float aspectLayout = layoutWidthF / layoutHeightF;
        float aspectWindow = windowWidth / windowHeight;
        float scale = (aspectLayout > aspectWindow) ? (windowWidth / layoutWidthF) : (windowHeight / layoutHeightF);
        scale *= layoutScale;
        scaleX = scale;
        scaleY = scale;
        offsetX = (windowWidth - layoutWidthF * scale) / 2;
        offsetY = (windowHeight - layoutHeightF * scale) / 2;
    }
    else if (layoutMode == "fill") {
        float aspectLayout = layoutWidthF / layoutHeightF;
        float aspectWindow = windowWidth / windowHeight;
        float scale = (aspectLayout < aspectWindow) ? (windowWidth / layoutWidthF) : (windowHeight / layoutHeightF);
        scale *= layoutScale;
        scaleX = scale;
        scaleY = scale;
        offsetX = (windowWidth - layoutWidthF * scale) / 2;
        offsetY = (windowHeight - layoutHeightF * scale) / 2;
    }
    else if (layoutMode == "none") {
        scaleX = layoutScale;
        scaleY = layoutScale;
        offsetX = (windowWidth - layoutWidthF * layoutScale) / 2;
        offsetY = (windowHeight - layoutHeightF * layoutScale) / 2;
    }
    else {
        Logger::write(Logger::ZONE_WARNING, "SDL", "Unknown layoutMode '" + layoutMode + "' for monitor " + std::to_string(viewInfo.Monitor) + ", defaulting to stretch");
        scaleX = (windowWidth / layoutWidthF) * layoutScale;
        scaleY = (windowHeight / layoutHeightF) * layoutScale;
        offsetX = 0;
        offsetY = 0;
    }

    // Adjust for rotation and mirroring
    if (rotation_[viewInfo.Monitor] % 2 == 1) {
        std::swap(scaleX, scaleY);
        std::swap(offsetX, offsetY);
    }

    if (mirror_[viewInfo.Monitor])
        scaleY /= 2;

    if (mirror_[viewInfo.Monitor] && (viewInfo.ContainerWidth < 0 || viewInfo.ContainerHeight < 0)) {
        viewInfo.ContainerX = 0;
        viewInfo.ContainerY = 0;
        viewInfo.ContainerWidth = layoutWidthF;
        viewInfo.ContainerHeight = layoutHeightF;
    }

    SDL_Rect srcRect, dstRect, srcRectCopy, dstRectCopy, srcRectOrig, dstRectOrig;
    double imageScaleX, imageScaleY;

    // Set destination rectangle
    dstRect.w = static_cast<int>(dest->w * scaleX * dpiScaleX);
    dstRect.h = static_cast<int>(dest->h * scaleY * dpiScaleY);
    dstRect.x = static_cast<int>(std::round(dest->x * scaleX * dpiScaleX + offsetX * dpiScaleX));
    dstRect.y = static_cast<int>(std::round(dest->y * scaleY * dpiScaleY + offsetY * dpiScaleY));

    // Adjust for container if present
    if (viewInfo.ContainerWidth > 0 && viewInfo.ContainerHeight > 0) {
        // Calculate scaled container position
        float containerXScaled = viewInfo.ContainerX * scaleX * dpiScaleX + offsetX * dpiScaleX;
        float containerYScaled = viewInfo.ContainerY * scaleY * dpiScaleY + offsetY * dpiScaleY;

        // Position texture relative to container
        float relX = (dest->x - viewInfo.ContainerX) * scaleX * dpiScaleX;
        float relY = (dest->y - viewInfo.ContainerY) * scaleY * dpiScaleY;
        dstRect.x = static_cast<int>(std::round(containerXScaled + relX));
        dstRect.y = static_cast<int>(std::round(containerYScaled + relY));
    }

    if (fullscreen_[viewInfo.Monitor]) {
        dstRect.x += static_cast<int>((displayWidth_[viewInfo.Monitor] - windowWidth_[viewInfo.Monitor]) / 2 * dpiScaleX);
        dstRect.y += static_cast<int>((displayHeight_[viewInfo.Monitor] - windowHeight_[viewInfo.Monitor]) / 2 * dpiScaleY);
    }

    if (src) {
        srcRect.x = src->x;
        srcRect.y = src->y;
        srcRect.w = src->w;
        srcRect.h = src->h;
    }
    else {
        srcRect.x = 0;
        srcRect.y = 0;
        SDL_QueryTexture(texture, NULL, NULL, &srcRect.w, &srcRect.h);
    }

    imageScaleX = (dstRect.w > 0) ? static_cast<double>(srcRect.w) / dstRect.w : 0.0;
    imageScaleY = (dstRect.h > 0) ? static_cast<double>(srcRect.h) / dstRect.h : 0.0;

    // Pre-clip large textures
    srcRectOrig = srcRect;
    if (srcRect.w > dstRect.w * 2 || srcRect.h > dstRect.h * 2) {
        srcRect.w = static_cast<int>(dstRect.w * imageScaleX);
        srcRect.h = static_cast<int>(dstRect.h * imageScaleY);
#ifdef _DEBUG
        Logger::write(Logger::ZONE_DEBUG, "SDL", "Pre-clipped texture from " + std::to_string(srcRectOrig.w) + "x" + std::to_string(srcRectOrig.h) + " to " + std::to_string(srcRect.w) + "x" + std::to_string(srcRect.h));
#endif
    }

    srcRectOrig = srcRect;
    dstRectOrig = dstRect;
    srcRectCopy = srcRect;
    dstRectCopy = dstRect;

    // Apply container clipping
    if (viewInfo.ContainerWidth > 0 && viewInfo.ContainerHeight > 0 && dstRect.w > 0 && dstRect.h > 0) {
        int containerX = static_cast<int>(viewInfo.ContainerX * scaleX * dpiScaleX + offsetX * dpiScaleX);
        int containerY = static_cast<int>(viewInfo.ContainerY * scaleY * dpiScaleY + offsetY * dpiScaleY);
        int containerW = static_cast<int>(viewInfo.ContainerWidth * scaleX * dpiScaleX);
        int containerH = static_cast<int>(viewInfo.ContainerHeight * scaleY * dpiScaleY);

        if (dstRect.x < containerX) {
            int delta = containerX - dstRect.x;
            dstRect.x = containerX;
            dstRect.w -= delta;
            srcRect.x += static_cast<int>(delta * imageScaleX);
        }
        if (dstRect.x + dstRect.w > containerX + containerW) {
            dstRect.w = containerX + containerW - dstRect.x;
        }
        if (dstRect.y < containerY) {
            int containerY = static_cast<int>(viewInfo.ContainerY * scaleY * dpiScaleY + offsetY * dpiScaleY);
            int delta = containerY - dstRect.y;
            dstRect.y = containerY;
            dstRect.h -= delta;
            srcRect.y += static_cast<int>(delta * imageScaleY);
        }
        if (dstRect.y + dstRect.h > containerY + containerH) {
            dstRect.h = containerY + containerH - dstRect.y;
        }

        srcRect.w = static_cast<int>(dstRect.w * imageScaleX);
        srcRect.h = static_cast<int>(dstRect.h * imageScaleY);
    }

    double angle = viewInfo.Angle;
    if (!mirror_[viewInfo.Monitor])
        angle += rotation_[viewInfo.Monitor] * 90;

    if (mirror_[viewInfo.Monitor]) {
        if (rotation_[viewInfo.Monitor] % 2 == 0) {
            if (srcRect.h > 0 && srcRect.w > 0) {
                dstRect.y += renderHeight / 2;
                SDL_SetTextureAlphaMod(texture, static_cast<char>(alpha * 255));
                SDL_RenderCopyEx(renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, NULL, SDL_FLIP_NONE);
                dstRect.x = renderWidth - dstRect.x - dstRect.w;
                dstRect.y = renderHeight - dstRect.y - dstRect.h;
                angle += 180;
                SDL_RenderCopyEx(renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, NULL, SDL_FLIP_NONE);
            }
        }
        else {
            if (srcRect.h > 0 && srcRect.w > 0) {
                int tmp = dstRect.x;
                dstRect.x = renderWidth / 2 - dstRect.y - dstRect.h / 2 - dstRect.w / 2;
                dstRect.y = tmp - dstRect.h / 2 + dstRect.w / 2;
                angle += 90;
                SDL_SetTextureAlphaMod(texture, static_cast<char>(alpha * 255));
                SDL_RenderCopyEx(renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, NULL, SDL_FLIP_NONE);
                dstRect.x = renderWidth - dstRect.x - dstRect.w;
                dstRect.y = renderHeight - dstRect.y - dstRect.h;
                angle += 180;
                SDL_RenderCopyEx(renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, NULL, SDL_FLIP_NONE);
            }
        }
    }
    else {
        if (rotation_[viewInfo.Monitor] == 1) {
            int tmp = dstRect.x;
            dstRect.x = renderWidth - dstRect.y - dstRect.h / 2 - dstRect.w / 2;
            dstRect.y = tmp - dstRect.h / 2 + dstRect.w / 2;
        }
        if (rotation_[viewInfo.Monitor] == 2) {
            dstRect.x = renderWidth - dstRect.x - dstRect.w;
            dstRect.y = renderHeight - dstRect.y - dstRect.h;
        }
        if (rotation_[viewInfo.Monitor] == 3) {
            int tmp = dstRect.x;
            dstRect.x = dstRect.y + dstRect.h / 2 - dstRect.w / 2;
            dstRect.y = renderHeight - tmp - dstRect.h / 2 - dstRect.w / 2;
        }

        if (srcRect.h > 0 && srcRect.w > 0) {
            SDL_SetTextureAlphaMod(texture, static_cast<char>(alpha * 255));
            SDL_RenderCopyEx(renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, NULL, SDL_FLIP_NONE);
        }
    }

    // Handle reflections
    bool enableReflections = true;
    int texWidth, texHeight;
    SDL_QueryTexture(texture, NULL, NULL, &texWidth, &texHeight);
    if (texWidth >= 3840 || texHeight >= 2160) {
        if (config_) {
            std::string reflectionsKey = "enableReflectionsFor4K" + std::to_string(viewInfo.Monitor);
            if (!config_->getProperty(reflectionsKey, enableReflections)) {
                config_->getProperty("enableReflectionsFor4K", enableReflections);
            }
        }
        if (!enableReflections) {
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "SDL", "Skipping reflections for 4K texture (" + std::to_string(texWidth) + "x" + std::to_string(texHeight) + ") on monitor " + std::to_string(viewInfo.Monitor));
#endif
            return true;
        }
    }

    if (viewInfo.Reflection.find("top") != std::string::npos) {
        dstRect.h = static_cast<int>(dstRectOrig.h * viewInfo.ReflectionScale);
        dstRect.y = dstRectOrig.y - dstRect.h - static_cast<int>(viewInfo.ReflectionDistance * scaleY * dpiScaleY);
        imageScaleY = (dstRect.h > 0) ? static_cast<double>(srcRectOrig.h) / dstRect.h : 0.0;
        dstRectCopy.y = dstRect.y;
        dstRectCopy.h = dstRect.h;

        if (viewInfo.ContainerWidth > 0 && viewInfo.ContainerHeight > 0 && dstRect.w > 0 && dstRect.h > 0) {
            int containerX = static_cast<int>(viewInfo.ContainerX * scaleX * dpiScaleX + offsetX * dpiScaleX);
            int containerY = static_cast<int>(viewInfo.ContainerY * scaleY * dpiScaleY + offsetY * dpiScaleY);
            int containerW = static_cast<int>(viewInfo.ContainerWidth * scaleX * dpiScaleX);
            int containerH = static_cast<int>(viewInfo.ContainerHeight * scaleY * dpiScaleY);

            if (dstRect.x < containerX) {
                int delta = containerX - dstRect.x;
                dstRect.x = containerX;
                dstRect.w -= delta;
                srcRect.x += static_cast<int>(delta * imageScaleX);
            }
            if (dstRect.x + dstRect.w > containerX + containerW) {
                dstRect.w = containerX + containerW - dstRect.x;
            }
            if (dstRect.y < containerY) {
                int delta = containerY - dstRect.y;
                dstRect.y = containerY;
                dstRect.h -= delta;
            }
            if (dstRect.y + dstRect.h > containerY + containerH) {
                dstRect.h = containerY + containerH - dstRect.y;
                srcRect.y = srcRectCopy.y + static_cast<int>((dstRectCopy.h - dstRect.h) * imageScaleY);
            }

            srcRect.w = static_cast<int>(dstRect.w * imageScaleX);
            srcRect.h = static_cast<int>(dstRect.h * imageScaleY);
        }

        angle = viewInfo.Angle;
        if (!mirror_[viewInfo.Monitor])
            angle += rotation_[viewInfo.Monitor] * 90;

        if (mirror_[viewInfo.Monitor]) {
            if (rotation_[viewInfo.Monitor] % 2 == 0) {
                if (srcRect.h > 0 && srcRect.w > 0) {
                    dstRect.y += renderHeight / 2;
                    SDL_SetTextureAlphaMod(texture, static_cast<char>(viewInfo.ReflectionAlpha * alpha * 255));
                    SDL_RenderCopyEx(renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, NULL, SDL_FLIP_VERTICAL);
                    dstRect.x = renderWidth - dstRect.x - dstRect.w;
                    dstRect.y = renderHeight - dstRect.y - dstRect.h;
                    angle += 180;
                    SDL_RenderCopyEx(renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, NULL, SDL_FLIP_VERTICAL);
                }
            }
            else {
                if (srcRect.h > 0 && srcRect.w > 0) {
                    int tmp = dstRect.x;
                    dstRect.x = renderWidth / 2 - dstRect.y - dstRect.h / 2 - dstRect.w / 2;
                    dstRect.y = tmp - dstRect.h / 2 + dstRect.w / 2;
                    angle += 90;
                    SDL_SetTextureAlphaMod(texture, static_cast<char>(viewInfo.ReflectionAlpha * alpha * 255));
                    SDL_RenderCopyEx(renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, NULL, SDL_FLIP_VERTICAL);
                    dstRect.x = renderWidth - dstRect.x - dstRect.w;
                    dstRect.y = renderHeight - dstRect.y - dstRect.h;
                    angle += 180;
                    SDL_RenderCopyEx(renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, NULL, SDL_FLIP_VERTICAL);
                }
            }
        }
        else {
            if (rotation_[viewInfo.Monitor] == 1) {
                int tmp = dstRect.x;
                dstRect.x = renderWidth - dstRect.y - dstRect.h / 2 - dstRect.w / 2;
                dstRect.y = tmp - dstRect.h / 2 + dstRect.w / 2;
            }
            if (rotation_[viewInfo.Monitor] == 2) {
                dstRect.x = renderWidth - dstRect.x - dstRect.w;
                dstRect.y = renderHeight - dstRect.y - dstRect.h;
            }
            if (rotation_[viewInfo.Monitor] == 3) {
                int tmp = dstRect.x;
                dstRect.x = dstRect.y + dstRect.h / 2 - dstRect.w / 2;
                dstRect.y = renderHeight - tmp - dstRect.h / 2 - dstRect.w / 2;
            }

            if (srcRect.h > 0 && srcRect.w > 0) {
                SDL_SetTextureAlphaMod(texture, static_cast<char>(viewInfo.ReflectionAlpha * alpha * 255));
                SDL_RenderCopyEx(renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, NULL, SDL_FLIP_VERTICAL);
            }
        }
    }

    srcRect = srcRectOrig;
    dstRect = dstRectOrig;
    srcRectCopy = srcRectOrig;
    dstRectCopy = dstRectOrig;

    if (viewInfo.Reflection.find("bottom") != std::string::npos) {
        dstRect.y = dstRectOrig.y + dstRectOrig.h + static_cast<int>(viewInfo.ReflectionDistance * scaleY * dpiScaleY);
        dstRect.h = static_cast<int>(dstRectOrig.h * viewInfo.ReflectionScale);
        imageScaleY = (dstRect.h > 0) ? static_cast<double>(srcRectOrig.h) / dstRect.h : 0.0;
        dstRectCopy.y = dstRect.y;
        dstRectCopy.h = dstRect.h;

        if (viewInfo.ContainerWidth > 0 && viewInfo.ContainerHeight > 0 && dstRect.w > 0 && dstRect.h > 0) {
            int containerX = static_cast<int>(viewInfo.ContainerX * scaleX * dpiScaleX + offsetX * dpiScaleX);
            int containerY = static_cast<int>(viewInfo.ContainerY * scaleY * dpiScaleY + offsetY * dpiScaleY);
            int containerW = static_cast<int>(viewInfo.ContainerWidth * scaleX * dpiScaleX);
            int containerH = static_cast<int>(viewInfo.ContainerHeight * scaleY * dpiScaleY);

            if (dstRect.x < containerX) {
                int delta = containerX - dstRect.x;
                dstRect.x = containerX;
                dstRect.w -= delta;
                srcRect.x += static_cast<int>(delta * imageScaleX);
            }
            if (dstRect.x + dstRect.w > containerX + containerW) {
                dstRect.w = containerX + containerW - dstRect.x;
            }
            if (dstRect.y < containerY) {
                int delta = containerY - dstRect.y;
                dstRect.y = containerY;
                dstRect.h -= delta;
            }
            if (dstRect.y + dstRect.h > containerY + containerH) {
                dstRect.h = containerY + containerH - dstRect.y;
                srcRect.y = srcRectCopy.y + static_cast<int>((dstRectCopy.h - dstRect.h) * imageScaleY);
            }

            srcRect.w = static_cast<int>(dstRect.w * imageScaleX);
            srcRect.h = static_cast<int>(dstRect.h * imageScaleY);
        }

        angle = viewInfo.Angle;
        if (!mirror_[viewInfo.Monitor])
            angle += rotation_[viewInfo.Monitor] * 90;

        if (mirror_[viewInfo.Monitor]) {
            if (rotation_[viewInfo.Monitor] % 2 == 0) {
                if (srcRect.h > 0 && srcRect.w > 0) {
                    dstRect.y += renderHeight / 2;
                    SDL_SetTextureAlphaMod(texture, static_cast<char>(viewInfo.ReflectionAlpha * alpha * 255));
                    SDL_RenderCopyEx(renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, NULL, SDL_FLIP_VERTICAL);
                    dstRect.x = renderWidth - dstRect.x - dstRect.w;
                    dstRect.y = renderHeight - dstRect.y - dstRect.h;
                    angle += 180;
                    SDL_RenderCopyEx(renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, NULL, SDL_FLIP_VERTICAL);
                }
            }
            else {
                if (srcRect.h > 0 && srcRect.w > 0) {
                    int tmp = dstRect.x;
                    dstRect.x = renderWidth / 2 - dstRect.y - dstRect.h / 2 - dstRect.w / 2;
                    dstRect.y = tmp - dstRect.h / 2 + dstRect.w / 2;
                    angle += 90;
                    SDL_SetTextureAlphaMod(texture, static_cast<char>(viewInfo.ReflectionAlpha * alpha * 255));
                    SDL_RenderCopyEx(renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, NULL, SDL_FLIP_VERTICAL);
                    dstRect.x = renderWidth - dstRect.x - dstRect.w;
                    dstRect.y = renderHeight - dstRect.y - dstRect.h;
                    angle += 180;
                    SDL_RenderCopyEx(renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, NULL, SDL_FLIP_VERTICAL);
                }
            }
        }
        else {
            if (rotation_[viewInfo.Monitor] == 1) {
                int tmp = dstRect.x;
                dstRect.x = renderWidth - dstRect.y - dstRect.h / 2 - dstRect.w / 2;
                dstRect.y = tmp - dstRect.h / 2 + dstRect.w / 2;
            }
            if (rotation_[viewInfo.Monitor] == 2) {
                dstRect.x = renderWidth - dstRect.x - dstRect.w;
                dstRect.y = renderHeight - dstRect.y - dstRect.h;
            }
            if (rotation_[viewInfo.Monitor] == 3) {
                int tmp = dstRect.x;
                dstRect.x = dstRect.y + dstRect.h / 2 - dstRect.w / 2;
                dstRect.y = renderHeight - tmp - dstRect.h / 2 - dstRect.w / 2;
            }

            if (srcRect.h > 0 && srcRect.w > 0) {
                SDL_SetTextureAlphaMod(texture, static_cast<char>(viewInfo.ReflectionAlpha * alpha * 255));
                SDL_RenderCopyEx(renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, NULL, SDL_FLIP_VERTICAL);
            }
        }
    }

    srcRect = srcRectOrig;
    dstRect = dstRectOrig;
    srcRectCopy = srcRectOrig;
    dstRectCopy = dstRectOrig;

    if (viewInfo.Reflection.find("left") != std::string::npos) {
        dstRect.w = static_cast<int>(dstRectOrig.w * viewInfo.ReflectionScale);
        dstRect.x = dstRectOrig.x - dstRect.w - static_cast<int>(viewInfo.ReflectionDistance * scaleX * dpiScaleX);
        imageScaleX = (dstRect.w > 0) ? static_cast<double>(srcRectOrig.w) / dstRect.w : 0.0;
        dstRectCopy.x = dstRect.x;
        dstRectCopy.w = dstRect.w;

        if (viewInfo.ContainerWidth > 0 && viewInfo.ContainerHeight > 0 && dstRect.w > 0 && dstRect.h > 0) {
            int containerX = static_cast<int>(viewInfo.ContainerX * scaleX * dpiScaleX + offsetX * dpiScaleX);
            int containerY = static_cast<int>(viewInfo.ContainerY * scaleY * dpiScaleY + offsetY * dpiScaleY);
            int containerW = static_cast<int>(viewInfo.ContainerWidth * scaleX * dpiScaleX);
            int containerH = static_cast<int>(viewInfo.ContainerHeight * scaleY * dpiScaleY);

            if (dstRect.x < containerX) {
                int delta = containerX - dstRect.x;
                dstRect.x = containerX;
                dstRect.w -= delta;
            }
            if (dstRect.x + dstRect.w > containerX + containerW) {
                dstRect.w = containerX + containerW - dstRect.x;
                srcRect.x = srcRectCopy.x + static_cast<int>((dstRectCopy.w - dstRect.w) * imageScaleX);
            }
            if (dstRect.y < containerY) {
                int delta = containerY - dstRect.y;
                dstRect.y = containerY;
                dstRect.h -= delta;
                srcRect.y = static_cast<int>(delta * imageScaleY);
            }
            if (dstRect.y + dstRect.h > containerY + containerH) {
                dstRect.h = containerY + containerH - dstRect.y;
            }

            srcRect.w = static_cast<int>(dstRect.w * imageScaleX);
            srcRect.h = static_cast<int>(dstRect.h * imageScaleY);
        }

        angle = viewInfo.Angle;
        if (!mirror_[viewInfo.Monitor])
            angle += rotation_[viewInfo.Monitor] * 90;

        if (mirror_[viewInfo.Monitor]) {
            if (rotation_[viewInfo.Monitor] % 2 == 0) {
                if (srcRect.h > 0 && srcRect.w > 0) {
                    dstRect.y += renderHeight / 2;
                    SDL_SetTextureAlphaMod(texture, static_cast<char>(viewInfo.ReflectionAlpha * alpha * 255));
                    SDL_RenderCopyEx(renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, NULL, SDL_FLIP_HORIZONTAL);
                    dstRect.x = renderWidth - dstRect.x - dstRect.w;
                    dstRect.y = renderHeight - dstRect.y - dstRect.h;
                    angle += 180;
                    SDL_RenderCopyEx(renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, NULL, SDL_FLIP_HORIZONTAL);
                }
            }
            else {
                if (srcRect.h > 0 && srcRect.w > 0) {
                    int tmp = dstRect.x;
                    dstRect.x = renderWidth / 2 - dstRect.y - dstRect.h / 2 - dstRect.w / 2;
                    dstRect.y = tmp - dstRect.h / 2 + dstRect.w / 2;
                    angle += 90;
                    SDL_SetTextureAlphaMod(texture, static_cast<char>(viewInfo.ReflectionAlpha * alpha * 255));
                    SDL_RenderCopyEx(renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, NULL, SDL_FLIP_HORIZONTAL);
                    dstRect.x = renderWidth - dstRect.x - dstRect.w;
                    dstRect.y = renderHeight - dstRect.y - dstRect.h;
                    angle += 180;
                    SDL_RenderCopyEx(renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, NULL, SDL_FLIP_HORIZONTAL);
                }
            }
        }
        else {
            if (rotation_[viewInfo.Monitor] == 1) {
                int tmp = dstRect.x;
                dstRect.x = renderWidth - dstRect.y - dstRect.h / 2 - dstRect.w / 2;
                dstRect.y = tmp - dstRect.h / 2 + dstRect.w / 2;
            }
            if (rotation_[viewInfo.Monitor] == 2) {
                dstRect.x = renderWidth - dstRect.x - dstRect.w;
                dstRect.y = renderHeight - dstRect.y - dstRect.h;
            }
            if (rotation_[viewInfo.Monitor] == 3) {
                int tmp = dstRect.x;
                dstRect.x = dstRect.y + dstRect.h / 2 - dstRect.w / 2;
                dstRect.y = renderHeight - tmp - dstRect.h / 2 - dstRect.w / 2;
            }

            if (srcRect.h > 0 && srcRect.w > 0) {
                SDL_SetTextureAlphaMod(texture, static_cast<char>(viewInfo.ReflectionAlpha * alpha * 255));
                SDL_RenderCopyEx(renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, NULL, SDL_FLIP_HORIZONTAL);
            }
        }
    }

    srcRect = srcRectOrig;
    dstRect = dstRectOrig;
    srcRectCopy = srcRectOrig;
    dstRectCopy = dstRectOrig;

    if (viewInfo.Reflection.find("right") != std::string::npos) {
        dstRect.x = dstRectOrig.x + dstRectOrig.w + static_cast<int>(viewInfo.ReflectionDistance * scaleX * dpiScaleX);
        dstRect.w = static_cast<int>(dstRectOrig.w * viewInfo.ReflectionScale);
        imageScaleX = (dstRect.w > 0) ? static_cast<double>(srcRectOrig.w) / dstRect.w : 0.0;
        dstRectCopy.x = dstRect.x;
        dstRectCopy.w = dstRect.w;

        if (viewInfo.ContainerWidth > 0 && viewInfo.ContainerHeight > 0 && dstRect.w > 0 && dstRect.h > 0) {
            int containerX = static_cast<int>(viewInfo.ContainerX * scaleX * dpiScaleX + offsetX * dpiScaleX);
            int containerY = static_cast<int>(viewInfo.ContainerY * scaleY * dpiScaleY + offsetY * dpiScaleY);
            int containerW = static_cast<int>(viewInfo.ContainerWidth * scaleX * dpiScaleX);
            int containerH = static_cast<int>(viewInfo.ContainerHeight * scaleY * dpiScaleY);

            if (dstRect.x < containerX) {
                int delta = containerX - dstRect.x;
                dstRect.x = containerX;
                dstRect.w -= delta;
            }
            if (dstRect.x + dstRect.w > containerX + containerW) {
                dstRect.w = containerX + containerW - dstRect.x;
                srcRect.x = srcRectCopy.x + static_cast<int>((dstRectCopy.w - dstRect.w) * imageScaleX);
            }
            if (dstRect.y < containerY) {
                int delta = containerY - dstRect.y;
                dstRect.y = containerY;
                dstRect.h -= delta;
                srcRect.y = static_cast<int>(delta * imageScaleY);
            }
            if (dstRect.y + dstRect.h > containerY + containerH) {
                dstRect.h = containerY + containerH - dstRect.y;
            }

            srcRect.w = static_cast<int>(dstRect.w * imageScaleX);
            srcRect.h = static_cast<int>(dstRect.h * imageScaleY);
        }

        angle = viewInfo.Angle;
        if (!mirror_[viewInfo.Monitor])
            angle += rotation_[viewInfo.Monitor] * 90;

        if (mirror_[viewInfo.Monitor]) {
            if (rotation_[viewInfo.Monitor] % 2 == 0) {
                if (srcRect.h > 0 && srcRect.w > 0) {
                    dstRect.y += renderHeight / 2;
                    SDL_SetTextureAlphaMod(texture, static_cast<char>(viewInfo.ReflectionAlpha * alpha * 255));
                    SDL_RenderCopyEx(renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, NULL, SDL_FLIP_HORIZONTAL);
                    dstRect.x = renderWidth - dstRect.x - dstRect.w;
                    dstRect.y = renderHeight - dstRect.y - dstRect.h;
                    angle += 180;
                    SDL_RenderCopyEx(renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, NULL, SDL_FLIP_HORIZONTAL);
                }
            }
            else {
                if (srcRect.h > 0 && srcRect.w > 0) {
                    int tmp = dstRect.x;
                    dstRect.x = renderWidth / 2 - dstRect.y - dstRect.h / 2 - dstRect.w / 2;
                    dstRect.y = tmp - dstRect.h / 2 + dstRect.w / 2;
                    angle += 90;
                    SDL_SetTextureAlphaMod(texture, static_cast<char>(viewInfo.ReflectionAlpha * alpha * 255));
                    SDL_RenderCopyEx(renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, NULL, SDL_FLIP_HORIZONTAL);
                    dstRect.x = renderWidth - dstRect.x - dstRect.w;
                    dstRect.y = renderHeight - dstRect.y - dstRect.h;
                    angle += 180;
                    SDL_RenderCopyEx(renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, NULL, SDL_FLIP_HORIZONTAL);
                }
            }
        }
        else {
            if (rotation_[viewInfo.Monitor] == 1) {
                int tmp = dstRect.x;
                dstRect.x = renderWidth - dstRect.y - dstRect.h / 2 - dstRect.w / 2;
                dstRect.y = tmp - dstRect.h / 2 + dstRect.w / 2;
            }
            if (rotation_[viewInfo.Monitor] == 2) {
                dstRect.x = renderWidth - dstRect.x - dstRect.w;
                dstRect.y = renderHeight - dstRect.y - dstRect.h;
            }
            if (rotation_[viewInfo.Monitor] == 3) {
                int tmp = dstRect.x;
                dstRect.x = dstRect.y + dstRect.h / 2 - dstRect.w / 2;
                dstRect.y = renderHeight - tmp - dstRect.h / 2 - dstRect.w / 2;
            }

            if (srcRect.h > 0 && srcRect.w > 0) {
                SDL_SetTextureAlphaMod(texture, static_cast<char>(viewInfo.ReflectionAlpha * alpha * 255));
                SDL_RenderCopyEx(renderer_[viewInfo.Monitor], texture, &srcRect, &dstRect, angle, NULL, SDL_FLIP_HORIZONTAL);
            }
        }
    }

    return true;
}

bool SDL::is4KDetected()
{
    for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
        SDL_Rect bounds;
        if (SDL_GetDisplayBounds(i, &bounds) == 0 && (bounds.w >= 3840 || bounds.h >= 2160)) {
            return true;
        }
    }
    return false;
}
std::string SDL::resolveAutoScaleQuality()
{
    if (!config_) {
        Logger::write(Logger::ZONE_ERROR, "SDL", "Configuration is not initialized.");
        return "0"; // Default to nearest scaling
    }

    std::string scaleQuality = "auto";
    config_->getProperty("scaleQuality", scaleQuality);

    if (scaleQuality == "auto") {
        return SDL::is4KDetected() ? "2" : "0";
    }

    return scaleQuality;
}
