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
#include <cmath>
#include <algorithm>
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
std::vector<bool>          SDL::enabled_;
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
    if (effectiveScaleQuality == "0" && preScaleQuality == "smooth") {
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
    enabled_.clear();

    for (int i = 0; i < numScreens_; ++i)
    {
        // Screen enable flag
        bool enableScreen = true;
        config.getProperty("enableScreen" + std::to_string(i), enableScreen);

        // Primary display
        if (i == 0 && !enableScreen)
        {
            Logger::write(Logger::ZONE_ERROR, "SDL",
                "Primary screen (0) cannot be disabled – ignoring enableScreen0=false");
            enableScreen = true;               // force it on
        }

        enabled_.push_back(enableScreen);      // <-- store for later use

        if (!enableScreen)                     // secondary screen disabled
        {
            Logger::write(Logger::ZONE_INFO, "SDL",
                "Screen " + std::to_string(i) + " disabled by configuration; skipping.");
            windowWidth_.push_back(0);
            windowHeight_.push_back(0);
            displayWidth_.push_back(0);
            displayHeight_.push_back(0);
            window_.push_back(nullptr);
            renderer_.push_back(nullptr);
            fullscreen_.push_back(false);
            rotation_.push_back(0);
            mirror_.push_back(false);
            continue;
        }

        // Secondary display creation
        SDL_DisplayMode mode;
        bool windowBorder = false;
        bool windowResize = false;
        Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;
        config.getProperty("windowBorder", windowBorder);
        if (!windowBorder) windowFlags |= SDL_WINDOW_BORDERLESS;
        config.getProperty("windowResize", windowResize);
        if (windowResize) windowFlags |= SDL_WINDOW_RESIZABLE;

        int screenNum = i;                     // default = logical index
        config.getProperty("screenNum" + std::to_string(i), screenNum);

        if (SDL_GetCurrentDisplayMode(screenNum, &mode) != 0)
        {
            if (i == 0)
            {
                Logger::write(Logger::ZONE_ERROR, "SDL",
                    "Primary display " + std::to_string(screenNum) + " does not exist.");
                return false;
            }
            else
            {
                Logger::write(Logger::ZONE_WARNING, "SDL",
                    "Display " + std::to_string(screenNum) + " does not exist; skipping screen " +
                    std::to_string(i));
                windowWidth_.push_back(0);
                windowHeight_.push_back(0);
                displayWidth_.push_back(0);
                displayHeight_.push_back(0);
                window_.push_back(nullptr);
                renderer_.push_back(nullptr);
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
        window_.push_back(SDL_CreateWindow("RetroFEVLC", SDL_WINDOWPOS_CENTERED_DISPLAY(screenNum), SDL_WINDOWPOS_CENTERED_DISPLAY(screenNum), windowWidth_[i], windowHeight_[i], windowFlags));

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
    if (alpha <= 0.0f || viewInfo.Monitor >= numScreens_ || !renderer_[viewInfo.Monitor] || !texture)
        return true;

    SDL_Renderer* renderer = renderer_[viewInfo.Monitor];
    int renderWidth, renderHeight;
    SDL_GetRendererOutputSize(renderer, &renderWidth, &renderHeight);

    float dpiScaleX = static_cast<float>(renderWidth) / windowWidth_[viewInfo.Monitor];
    float dpiScaleY = static_cast<float>(renderHeight) / windowHeight_[viewInfo.Monitor];
    float winW = static_cast<float>(windowWidth_[viewInfo.Monitor]);
    float winH = static_cast<float>(windowHeight_[viewInfo.Monitor]);
    float layW = static_cast<float>(layoutWidth);
    float layH = static_cast<float>(layoutHeight);

    // Layout scaling
    float scaleX = winW / layW;
    float scaleY = winH / layH;
    float offsetX = 0.0f, offsetY = 0.0f;

    std::string layoutMode = "stretch";
    float layoutScale = 1.0f;
    if (config_) {
        config_->getProperty("layoutScaleMode", layoutMode);
        config_->getProperty("layoutZoomMode", layoutScale);
        std::string modeKey = "layoutScaleMode" + std::to_string(viewInfo.Monitor);
        std::string scaleKey = "layoutZoomMode" + std::to_string(viewInfo.Monitor);
        std::string perMode;
        float perScale = 0.0f;
        if (config_->getProperty(modeKey, perMode)) layoutMode = perMode;
        if (config_->getProperty(scaleKey, perScale) && perScale > 0.0f) layoutScale = perScale;
        if (layoutScale <= 0.0f) layoutScale = 1.0f;
    }

    if (layoutMode == "fit" || layoutMode == "fill") {
        float aL = layW / layH, aW = winW / winH;
        float s = (layoutMode == "fit")
            ? ((aL > aW) ? (winW / layW) : (winH / layH))
            : ((aL < aW) ? (winW / layW) : (winH / layH));
        scaleX = scaleY = s * layoutScale;
        offsetX = (winW - layW * scaleX) * 0.5f;
        offsetY = (winH - layH * scaleY) * 0.5f;
    }
    else if (layoutMode == "none") {
        scaleX = scaleY = layoutScale;
        offsetX = (winW - layW * scaleX) * 0.5f;
        offsetY = (winH - layH * scaleY) * 0.5f;
    }
    else {
        scaleX *= layoutScale;
        scaleY *= layoutScale;
    }

    if (rotation_[viewInfo.Monitor] % 2 == 1)
        std::swap(scaleX, scaleY), std::swap(offsetX, offsetY);
    if (mirror_[viewInfo.Monitor])
        scaleY /= 2.0f;

    if (mirror_[viewInfo.Monitor] && (viewInfo.ContainerWidth < 0 || viewInfo.ContainerHeight < 0)) {
        viewInfo.ContainerX = 0;
        viewInfo.ContainerY = 0;
        viewInfo.ContainerWidth = layW;
        viewInfo.ContainerHeight = layH;
    }

    float baseX = dest->x;
    float baseY = dest->y;
    float baseW = dest->w;
    float baseH = dest->h;

    SDL_Rect srcRect;
    if (src) srcRect = *src;
    else { srcRect = { 0,0,0,0 }; SDL_QueryTexture(texture, nullptr, nullptr, &srcRect.w, &srcRect.h); }

    Uint8 finalAlpha = static_cast<Uint8>(alpha * viewInfo.Alpha * 255.0f + 0.5f);

    // Declare all variables needed for reflections (used later)
    SDL_Rect srcRectOrig = srcRect;
    SDL_Rect dstRectOrig;
    SDL_Rect srcRectCopy = srcRect;
    SDL_Rect dstRectCopy;
    double imageScaleX = 0.0, imageScaleY = 0.0;

	// UNIFIED 3D PATH: flip3DX/Y + roller3DX/Y + wrapLX/LY/RX/RY/TX/TY/BX/BY 
    bool hasTilt = (viewInfo.AngleX != 0.0f || viewInfo.AngleY != 0.0f);
    bool hasSlice = (viewInfo.Slice3DX != 0.0f || viewInfo.Slice3DY != 0.0f);
    bool hasWarp =
        (std::abs(viewInfo.WrapLX) != 0.0f ||
            std::abs(viewInfo.WrapLY) != 0.0f ||
            std::abs(viewInfo.WrapRX) != 0.0f ||
            std::abs(viewInfo.WrapRY) != 0.0f ||
            std::abs(viewInfo.WrapTX) != 0.0f ||
            std::abs(viewInfo.WrapTY) != 0.0f ||
            std::abs(viewInfo.WrapBX) != 0.0f ||
            std::abs(viewInfo.WrapBY) != 0.0f);
    // Flag for the *exact* default 2D rendering path
  // TODO  bool is2DOnly = !(hasTilt || hasSlice || hasWarp); // TODO
    if (hasTilt || hasSlice || hasWarp) {
        bool yAxisTilt = hasTilt && std::abs(viewInfo.AngleY) >= std::abs(viewInfo.AngleX);
        float tiltAngleDeg = yAxisTilt ? viewInfo.AngleY : viewInfo.AngleX;
       
        bool useVerticalStrips = hasSlice && viewInfo.Slice3DY != 0.0f;  
        float sliceAngleDeg = useVerticalStrips ? viewInfo.Slice3DY : viewInfo.Slice3DX;
        bool reverseSlice = sliceAngleDeg < 0.0f;
        float sliceRad = std::abs(sliceAngleDeg) * (float)M_PI / 180.0f;
        const int slices = 96;
        const int BLEED = 2;
        float pivotX = baseX + baseW * 0.5f;
        float pivotY = baseY + baseH * 0.5f;
        double baseAngle = viewInfo.Angle + (mirror_[viewInfo.Monitor] ? 0.0 : rotation_[viewInfo.Monitor] * 90.0);
        SDL_Rect baseDst = {
            static_cast<int>(std::round((pivotX - baseW * 0.5f) * scaleX + offsetX) * dpiScaleX),
            static_cast<int>(std::round((pivotY - baseH * 0.5f) * scaleY + offsetY) * dpiScaleY),
            static_cast<int>(std::round(baseW * scaleX * dpiScaleX)),
            static_cast<int>(std::round(baseH * scaleY * dpiScaleY))
        };
        if (fullscreen_[viewInfo.Monitor]) {
            baseDst.x += static_cast<int>((displayWidth_[viewInfo.Monitor] - windowWidth_[viewInfo.Monitor]) * 0.5f * dpiScaleX);
            baseDst.y += static_cast<int>((displayHeight_[viewInfo.Monitor] - windowHeight_[viewInfo.Monitor]) * 0.5f * dpiScaleY);
        }
        double tempScaleX = (baseDst.w > 0) ? static_cast<double>(srcRectOrig.w) / baseDst.w : 0.0;
        double tempScaleY = (baseDst.h > 0) ? static_cast<double>(srcRectOrig.h) / baseDst.h : 0.0;
        int cx = 0, cy = 0, cw = 0, ch = 0;
        bool hasContainer = (viewInfo.ContainerWidth > 0 && viewInfo.ContainerHeight > 0);
        if (hasContainer) {
            cx = static_cast<int>((viewInfo.ContainerX * scaleX + offsetX) * dpiScaleX);
            cy = static_cast<int>((viewInfo.ContainerY * scaleY + offsetY) * dpiScaleY);
            cw = static_cast<int>(viewInfo.ContainerWidth * scaleX * dpiScaleX);
            ch = static_cast<int>(viewInfo.ContainerHeight * scaleY * dpiScaleY);
        }
        // 3D Tilt transformation
        bool backFaceVisible = false;
        float finalCardW = baseW;
        float finalCardH = baseH;
        float tiltLiftX = 0.0f;
        float tiltLiftY = 0.0f;
        if (hasTilt) {
            // Normalize to -180..+180 
            auto norm = [](float a) {
                a = fmodf(a, 360.0f);
                if (a > 180.0f) a -= 360.0f;
                if (a <= -180.0f) a += 360.0f;
                return a;
                };
            float ax = norm(viewInfo.AngleX);
            float ay = norm(viewInfo.AngleY);
            float radX = ax * (float)M_PI / 180.0f;
            float radY = ay * (float)M_PI / 180.0f;
            float cosX = std::cos(radX);
            float cosY = std::cos(radY);

            // 1. Foreshortening (width/height shrink when edge-on)
            float scaleW = std::fabs(cosY); // horizontal tilt
            float scaleH = std::fabs(cosX); // vertical tilt

            // 2. TINY breathing — based on absolute facing angle (0° and 180° = max size!)
            float facing = std::fabs(cosX * cosY); // 1.0 when front or back, 0.0 when edge-on
            float breathe = 1.0f + 0.08f * (1.0f - facing);

            // 3. Final size — same at 0° and 180°, gentle breathing
            float finalScale = breathe;
            finalCardW *= scaleW * finalScale;
            finalCardH *= scaleH * finalScale;

            // 4. NO movement at all — center perfectly fixed forever
            tiltLiftX = 0.0f;
            tiltLiftY = 0.0f;

            // NEW: ONLY THIS LINE ADDED — PERFECT BACKSIDE SUPPORT FOR NEGATIVE ANGLES
            backFaceVisible = (cosX * cosY < 0.0f);
        }
        auto drawCard = [&](SDL_Rect dst, const SDL_Rect& src, double angle, SDL_RendererFlip f) {
            if (mirror_[viewInfo.Monitor]) {
                if (rotation_[viewInfo.Monitor] % 2 == 0) {
                    dst.y += renderHeight / 2;
                    SDL_RenderCopyEx(renderer, texture, &src, &dst, angle, nullptr, f);
                    dst.x = renderWidth - dst.x - dst.w;
                    dst.y = renderHeight - dst.y - dst.h;
                    SDL_RenderCopyEx(renderer, texture, &src, &dst, angle + 180.0, nullptr, f);
                }
                else {
                    int tmp = dst.x;
                    dst.x = renderWidth / 2 - dst.y - dst.h / 2 - dst.w / 2;
                    dst.y = tmp - dst.h / 2 + dst.w / 2;
                    SDL_RenderCopyEx(renderer, texture, &src, &dst, angle + 90.0, nullptr, f);
                    dst.x = renderWidth - dst.x - dst.w;
                    dst.y = renderHeight - dst.y - dst.h;
                    SDL_RenderCopyEx(renderer, texture, &src, &dst, angle + 270.0, nullptr, f);
                }
            }
            else {
                SDL_Rect d = dst;
                if (rotation_[viewInfo.Monitor] == 1) { int t = d.x; d.x = renderWidth - d.y - d.h / 2 - d.w / 2; d.y = t - d.h / 2 + d.w / 2; }
                else if (rotation_[viewInfo.Monitor] == 2) { d.x = renderWidth - d.x - d.w; d.y = renderHeight - d.y - d.h; }
                else if (rotation_[viewInfo.Monitor] == 3) { int t = d.x; d.x = d.y + d.h / 2 - d.w / 2; d.y = renderHeight - t - d.h / 2 - d.w / 2; }
                SDL_RenderCopyEx(renderer, texture, &src, &d, angle, nullptr, f);
            }
            };
        if (!hasWarp) {
            SDL_RendererFlip flip = SDL_FLIP_NONE;
            if (backFaceVisible) {
                flip = SDL_FLIP_HORIZONTAL;
            }
            if (mirror_[viewInfo.Monitor]) {
                flip = (SDL_RendererFlip)(flip ^ SDL_FLIP_HORIZONTAL);
            }
            //  SPECIAL CASE: NO SLICE, SINGLE RENDER (FIXES ANGLE X/Y ALONE) 
            if (!hasSlice) {
                SDL_Rect srcS = srcRectOrig;
                SDL_Rect dstS = {
                    static_cast<int>(std::round((pivotX + tiltLiftX - finalCardW * 0.5f) * scaleX + offsetX) * dpiScaleX),
                    static_cast<int>(std::round((pivotY + tiltLiftY - finalCardH * 0.5f) * scaleY + offsetY) * dpiScaleY),
                    static_cast<int>(std::round(finalCardW * scaleX * dpiScaleX)),
                    static_cast<int>(std::round(finalCardH * scaleY * dpiScaleY))
                };
                if (fullscreen_[viewInfo.Monitor]) {
                    dstS.x += static_cast<int>((displayWidth_[viewInfo.Monitor] - windowWidth_[viewInfo.Monitor]) * 0.5f * dpiScaleX);
                    dstS.y += static_cast<int>((displayHeight_[viewInfo.Monitor] - windowHeight_[viewInfo.Monitor]) * 0.5f * dpiScaleY);
                }
                double curTempScaleX = (dstS.w > 0) ? static_cast<double>(srcRectOrig.w) / dstS.w : 0.0;
                double curTempScaleY = (dstS.h > 0) ? static_cast<double>(srcRectOrig.h) / dstS.h : 0.0;
                if (hasContainer && dstS.w > 0 && dstS.h > 0) {
                    if (dstS.x < cx) { int d = cx - dstS.x; dstS.x = cx; dstS.w -= d; srcS.x += static_cast<int>(d * curTempScaleX); }
                    if (dstS.x + dstS.w > cx + cw) dstS.w = cx + cw - dstS.x;
                    if (dstS.y < cy) { int d = cy - dstS.y; dstS.y = cy; dstS.h -= d; srcS.y += static_cast<int>(d * curTempScaleY); }
                    if (dstS.y + dstS.h > cy + ch) dstS.h = cy + ch - dstS.y;
                    srcS.w = static_cast<int>(dstS.w * curTempScaleX);
                    srcS.h = static_cast<int>(dstS.h * curTempScaleY);
                }
                if (srcS.w > 0 && srcS.h > 0) {
                    drawCard(dstS, srcS, baseAngle, flip);
                }
            }
            else {
                // Multi-slice rendering loop
                float angleDeg = useVerticalStrips ? viewInfo.Slice3DY : viewInfo.Slice3DX;

                // FULL negative value support + proper 0–360 wrapping
                float normalized = fmodf(angleDeg, 360.0f);
                if (normalized < 0.0f) normalized += 360.0f;

                // Map 180°–360°  180°–0° so it continues folding instead of reversing
                float effectiveDeg = (normalized > 180.0f) ? (360.0f - normalized) : normalized;

                float sliceRad = effectiveDeg * (float)M_PI / 180.0f;  // Always positive 0

                // Determine if we need to reverse strip order (negative angles + second half)
                bool shouldReverse = (angleDeg < 0.0f) != (normalized > 180.0f);  // XOR magic
                bool reverseSliceFinal = reverseSlice ^ shouldReverse;

                for (int i = 0; i < slices; ++i) {
                    int idx = reverseSliceFinal ? (slices - 1 - i) : i;
                    float t = static_cast<float>(idx) / (slices - 1);   //  Critical fix: was / slices before!
                    float nt = static_cast<float>(idx + 1) / (slices - 1);

                    SDL_Rect srcS, dstS;

                    if (useVerticalStrips) {
                        // VERTICAL STRIPS (Slice3DY)
                        // posT: for normal angles uses t (left-anchored, unrolls right)
                        //       for reversed angles uses (1-t) (right-anchored, unrolls left)
                        float posT = reverseSliceFinal ? (1.0f - t) : t;
                        float sliceScale = std::cos(sliceRad * posT) * 0.5f + 0.5f;
                        float depth = std::cos(sliceRad) * (1.0f - std::abs(2.0f * posT - 1.0f)) * 8.0f;

                        int srcX = std::max(0, static_cast<int>(srcRectOrig.w * t - BLEED));
                        int srcW = std::min(srcRectOrig.w - srcX, static_cast<int>(srcRectOrig.w * (nt - t) + BLEED * 2));
                        srcS = { srcRectOrig.x + srcX, srcRectOrig.y, srcW, srcRectOrig.h };

                        float wSlice = finalCardW * (nt - t) * sliceScale + BLEED * 2;
                        float xOffset;
                        if (!reverseSliceFinal) {
                            // Normal: left edge anchored, unrolls rightward
                            xOffset = pivotX + tiltLiftX - finalCardW * 0.5f + finalCardW * posT * sliceScale + depth - BLEED;
                        } else {
                            // Mirrored: right edge anchored, unrolls leftward
                            float rightEdge = pivotX + tiltLiftX + finalCardW * 0.5f - finalCardW * posT * sliceScale + depth;
                            xOffset = rightEdge - wSlice + BLEED;
                        }

                        dstS = {
                            static_cast<int>(std::round((xOffset * scaleX + offsetX) * dpiScaleX)),
                            static_cast<int>(std::round((pivotY + tiltLiftY - finalCardH * 0.5f) * scaleY + offsetY) * dpiScaleY),
                            static_cast<int>(std::round(wSlice * scaleX * dpiScaleX)),
                            static_cast<int>(std::round(finalCardH * scaleY * dpiScaleY))
                        };
                        if (fullscreen_[viewInfo.Monitor])
                            dstS.x += static_cast<int>((displayWidth_[viewInfo.Monitor] - windowWidth_[viewInfo.Monitor]) * 0.5f * dpiScaleX);
                    }
                    else {
                        // HORIZONTAL STRIPS (Slice3DX)
                        // posT: for normal angles uses t (top-anchored, unrolls downward)
                        //       for reversed angles uses (1-t) (bottom-anchored, unrolls upward)
                        float posT = reverseSliceFinal ? (1.0f - t) : t;
                        float sliceScale = std::cos(sliceRad * posT) * 0.5f + 0.5f;
                        float depth = std::cos(sliceRad) * (1.0f - std::abs(2.0f * posT - 1.0f)) * 8.0f;

                        int srcY = std::max(0, static_cast<int>(srcRectOrig.h * t - BLEED));
                        int srcH = std::min(srcRectOrig.h - srcY, static_cast<int>(srcRectOrig.h * (nt - t) + BLEED * 2));
                        srcS = { srcRectOrig.x, srcRectOrig.y + srcY, srcRectOrig.w, srcH };

                        float hSlice = finalCardH * (nt - t) * sliceScale + BLEED * 2;
                        float yOffset;
                        if (!reverseSliceFinal) {
                            // Normal: top edge anchored, unrolls downward
                            yOffset = pivotY + tiltLiftY - finalCardH * 0.5f + finalCardH * posT * sliceScale + depth - BLEED;
                        } else {
                            // Mirrored: bottom edge anchored, unrolls upward
                            float bottomEdge = pivotY + tiltLiftY + finalCardH * 0.5f - finalCardH * posT * sliceScale + depth;
                            yOffset = bottomEdge - hSlice + BLEED;
                        }

                        dstS = {
                            static_cast<int>(std::round((pivotX + tiltLiftX - finalCardW * 0.5f) * scaleX + offsetX) * dpiScaleX),
                            static_cast<int>(std::round((yOffset * scaleY + offsetY) * dpiScaleY)),
                            static_cast<int>(std::round(finalCardW * scaleX * dpiScaleX)),
                            static_cast<int>(std::round(hSlice * scaleY * dpiScaleY))
                        };
                        if (fullscreen_[viewInfo.Monitor])
                            dstS.y += static_cast<int>((displayHeight_[viewInfo.Monitor] - windowHeight_[viewInfo.Monitor]) * 0.5f * dpiScaleY);
                    }
                    // Container clipping
                    if (hasContainer && dstS.w > 0 && dstS.h > 0) {
                        if (dstS.x < cx) { int d = cx - dstS.x; dstS.x = cx; dstS.w -= d; srcS.x += static_cast<int>(d * tempScaleX); srcS.w -= static_cast<int>(d * tempScaleX); }
                        if (dstS.x + dstS.w > cx + cw) { int wd = dstS.x + dstS.w - (cx + cw); srcS.w -= static_cast<int>(wd * tempScaleX); dstS.w -= wd; }
                        if (dstS.y < cy) { int d = cy - dstS.y; dstS.y = cy; dstS.h -= d; srcS.y += static_cast<int>(d * tempScaleY); srcS.h -= static_cast<int>(d * tempScaleY); }
                        if (dstS.y + dstS.h > cy + ch) { int hd = dstS.y + dstS.h - (cy + ch); srcS.h -= static_cast<int>(hd * tempScaleY); dstS.h -= hd; }
                        if (dstS.w <= 0 || dstS.h <= 0) continue;
                    }
                    if (srcS.w > 0 && srcS.h > 0) {
                        drawCard(dstS, srcS, baseAngle, flip);
                    }
                }
            }
        }
        if (hasWarp) {
            int origW, origH;
            SDL_QueryTexture(texture, nullptr, nullptr, &origW, &origH);

            // Texture coordinates (0.0 to 1.0)
            float tx1 = srcRectOrig.x / (float)origW;
            float ty1 = srcRectOrig.y / (float)origH;
            float tx2 = (srcRectOrig.x + srcRectOrig.w) / (float)origW;
            float ty2 = (srcRectOrig.y + srcRectOrig.h) / (float)origH;

            // SCREEN-SPACE corners (in pixels) - MUST use baseX/baseY/baseW/baseH
            float x1 = baseX;
            float y1 = baseY;
            float x2 = x1 + baseW;
            float y2 = y1 + baseH;

            struct Corner { float x, y; };
            Corner tl = { x1 + viewInfo.WrapLX,  y1 + viewInfo.WrapLY };
            Corner tr = { x2 + viewInfo.WrapRX,  y1 + viewInfo.WrapRY };
            Corner br = { x2 + viewInfo.WrapBX,  y2 + viewInfo.WrapBY };
            Corner bl = { x1 + viewInfo.WrapTX,  y2 + viewInfo.WrapTY };

            auto toScreen = [&](float lx, float ly) -> SDL_FPoint {
                return { (lx * scaleX + offsetX) * dpiScaleX, (ly * scaleY + offsetY) * dpiScaleY };
                };

            SDL_FPoint verts[4] = {
                toScreen(tl.x, tl.y),
                toScreen(tr.x, tr.y),
                toScreen(br.x, br.y),
                toScreen(bl.x, bl.y)
            };

            SDL_FPoint texcoords[4] = {
                { tx1, ty1 }, { tx2, ty1 },
                { tx2, ty2 }, { tx1, ty2 }
            };

            if (fullscreen_[viewInfo.Monitor]) {
                float ox = (displayWidth_[viewInfo.Monitor] - windowWidth_[viewInfo.Monitor]) * 0.5f * dpiScaleX;
                float oy = (displayHeight_[viewInfo.Monitor] - windowHeight_[viewInfo.Monitor]) * 0.5f * dpiScaleY;
                for (auto& v : verts) { v.x += ox; v.y += oy; }
            }

            SDL_SetTextureAlphaMod(texture, finalAlpha);

            double finalAngle = viewInfo.Angle + (mirror_[viewInfo.Monitor] ? 0.0 : rotation_[viewInfo.Monitor] * 90.0);

            auto drawWarped = [&]() {
                SDL_Vertex vertices[4];
                SDL_FPoint current[4];
                std::copy(std::begin(verts), std::end(verts), std::begin(current));

                // Apply screen rotation (only if not mirrored)
                if (!mirror_[viewInfo.Monitor] && finalAngle != 0.0) {
                    float cx = renderWidth * 0.5f;
                    float cy = renderHeight * 0.5f;
                    float rad = (float)finalAngle * (float)M_PI / 180.0f;
                    float c = std::cos(rad), s = std::sin(rad);
                    for (int i = 0; i < 4; ++i) {
                        float rx = current[i].x - cx;
                        float ry = current[i].y - cy;
                        current[i].x = cx + rx * c - ry * s;
                        current[i].y = cy + rx * s + ry * c;
                    }
                }

                for (int i = 0; i < 4; ++i) {
                    vertices[i].position = current[i];
                    vertices[i].color = { 255, 255, 255, 255 };
                    vertices[i].tex_coord = texcoords[i];
                }

                const int indices[6] = { 0, 1, 2, 0, 2, 3 };

                SDL_RenderGeometryRaw(renderer, texture,
                    (float*)vertices, sizeof(SDL_Vertex),
                    (SDL_Color*)&vertices[0].color, sizeof(SDL_Vertex),
                    (float*)&vertices[0].tex_coord, sizeof(SDL_Vertex),
                    4, indices, 6, sizeof(int));

                // Mirror support
                if (mirror_[viewInfo.Monitor]) {
                    for (auto& v : current) {
                        v.x = renderWidth - v.x;
                        v.y = renderHeight - v.y;
                    }
                    std::swap(current[0], current[1]);
                    std::swap(current[2], current[3]);
                    for (int i = 0; i < 4; ++i) vertices[i].position = current[i];

                    SDL_RenderGeometryRaw(renderer, texture,
                        (float*)vertices, sizeof(SDL_Vertex),
                        (SDL_Color*)&vertices[0].color, sizeof(SDL_Vertex),
                        (float*)&vertices[0].tex_coord, sizeof(SDL_Vertex),
                        4, indices, 6, sizeof(int));
                }
                };

            drawWarped();

                        if (!viewInfo.Reflection.empty()) {
                SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(viewInfo.ReflectionAlpha * alpha * 255.0f + 0.5f));
                                std::swap(texcoords[0], texcoords[3]);
                std::swap(texcoords[1], texcoords[2]);
                drawWarped();
            }
        }
    }
    else {

        // Standard 2D rendering path
        float scaledX = baseX * scaleX + offsetX;
        float scaledY = baseY * scaleY + offsetY;
        float scaledW = baseW * scaleX;
        float scaledH = baseH * scaleY;

        SDL_Rect dstRect = {
            static_cast<int>(std::round(scaledX * dpiScaleX)),
            static_cast<int>(std::round(scaledY * dpiScaleY)),
            static_cast<int>(std::round(scaledW * dpiScaleX)),
            static_cast<int>(std::round(scaledH * dpiScaleY))
        };

        if (fullscreen_[viewInfo.Monitor]) {
            dstRect.x += static_cast<int>((displayWidth_[viewInfo.Monitor] - windowWidth_[viewInfo.Monitor]) / 2 * dpiScaleX);
            dstRect.y += static_cast<int>((displayHeight_[viewInfo.Monitor] - windowHeight_[viewInfo.Monitor]) / 2 * dpiScaleY);
        }

        srcRectOrig = srcRect;
        dstRectOrig = dstRect;
        srcRectCopy = srcRect;
        dstRectCopy = dstRect;

        imageScaleX = (dstRect.w > 0) ? static_cast<double>(srcRect.w) / dstRect.w : 0.0;
        imageScaleY = (dstRect.h > 0) ? static_cast<double>(srcRect.h) / dstRect.h : 0.0;

        // Pre-clip large textures
        if (srcRect.w > dstRect.w * 2 || srcRect.h > dstRect.h * 2) {
            srcRect.w = static_cast<int>(dstRect.w * imageScaleX);
            srcRect.h = static_cast<int>(dstRect.h * imageScaleY);
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "SDL", "Pre-clipped texture from " + std::to_string(srcRectOrig.w) + "x" + std::to_string(srcRectOrig.h) + " to " + std::to_string(srcRect.w) + "x" + std::to_string(srcRect.h));
#endif
        }
        // Container clipping
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
                srcRect.y += static_cast<int>(delta * imageScaleY);
            }
            if (dstRect.y + dstRect.h > containerY + containerH) {
                dstRect.h = containerY + containerH - dstRect.y;
            }
            srcRect.w = static_cast<int>(dstRect.w * imageScaleX);
            srcRect.h = static_cast<int>(dstRect.h * imageScaleY);
        }

        double angle = viewInfo.Angle + (mirror_[viewInfo.Monitor] ? 0.0 : rotation_[viewInfo.Monitor] * 90.0);
        SDL_SetTextureAlphaMod(texture, finalAlpha);

        // Mirror + rotation
        if (mirror_[viewInfo.Monitor]) {
            if (rotation_[viewInfo.Monitor] % 2 == 0) {
                if (srcRect.h > 0 && srcRect.w > 0) {
                    dstRect.y += renderHeight / 2;
                    SDL_RenderCopyEx(renderer, texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_NONE);
                    dstRect.x = renderWidth - dstRect.x - dstRect.w;
                    dstRect.y = renderHeight - dstRect.y - dstRect.h;
                    angle += 180;
                    SDL_RenderCopyEx(renderer, texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_NONE);
                }
            }
            else {
                if (srcRect.h > 0 && srcRect.w > 0) {
                    int tmp = dstRect.x;
                    dstRect.x = renderWidth / 2 - dstRect.y - dstRect.h / 2 - dstRect.w / 2;
                    dstRect.y = tmp - dstRect.h / 2 + dstRect.w / 2;
                    angle += 90;
                    SDL_RenderCopyEx(renderer, texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_NONE);
                    dstRect.x = renderWidth - dstRect.x - dstRect.w;
                    dstRect.y = renderHeight - dstRect.y - dstRect.h;
                    angle += 180;
                    SDL_RenderCopyEx(renderer, texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_NONE);
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
                SDL_RenderCopyEx(renderer, texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_NONE);
            }
        }

        // Reflections
        bool enableReflections = true;
        int texWidth, texHeight;
        SDL_QueryTexture(texture, nullptr, nullptr, &texWidth, &texHeight);
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

        // Top reflection
        if (viewInfo.Reflection.find("top") != std::string::npos) {
            dstRect = dstRectOrig;
            dstRect.h = static_cast<int>(dstRectOrig.h * viewInfo.ReflectionScale);
            dstRect.y = dstRectOrig.y - dstRect.h - static_cast<int>(viewInfo.ReflectionDistance * scaleY * dpiScaleY);
            imageScaleY = (dstRect.h > 0) ? static_cast<double>(srcRectOrig.h) / dstRect.h : 0.0;
            srcRect = srcRectOrig;

            // Container clipping for reflection
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
                    srcRect.y = srcRectOrig.y + static_cast<int>((dstRectOrig.h - dstRect.h) * imageScaleY);
                }
                srcRect.w = static_cast<int>(dstRect.w * imageScaleX);
                srcRect.h = static_cast<int>(dstRect.h * imageScaleY);
            }

            angle = viewInfo.Angle;
            if (!mirror_[viewInfo.Monitor]) angle += rotation_[viewInfo.Monitor] * 90;

            SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(viewInfo.ReflectionAlpha * alpha * 255.0f));

            if (mirror_[viewInfo.Monitor]) {
                if (rotation_[viewInfo.Monitor] % 2 == 0) {
                    if (srcRect.h > 0 && srcRect.w > 0) {
                        dstRect.y += renderHeight / 2;
                        SDL_RenderCopyEx(renderer, texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_VERTICAL);
                        dstRect.x = renderWidth - dstRect.x - dstRect.w;
                        dstRect.y = renderHeight - dstRect.y - dstRect.h;
                        angle += 180;
                        SDL_RenderCopyEx(renderer, texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_VERTICAL);
                    }
                }
                else {
                    if (srcRect.h > 0 && srcRect.w > 0) {
                        int tmp = dstRect.x;
                        dstRect.x = renderWidth / 2 - dstRect.y - dstRect.h / 2 - dstRect.w / 2;
                        dstRect.y = tmp - dstRect.h / 2 + dstRect.w / 2;
                        angle += 90;
                        SDL_RenderCopyEx(renderer, texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_VERTICAL);
                        dstRect.x = renderWidth - dstRect.x - dstRect.w;
                        dstRect.y = renderHeight - dstRect.y - dstRect.h;
                        angle += 180;
                        SDL_RenderCopyEx(renderer, texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_VERTICAL);
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
                    SDL_RenderCopyEx(renderer, texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_VERTICAL);
                }
            }
        }

        // Bottom reflection (same pattern for all 4 directions)
        if (viewInfo.Reflection.find("bottom") != std::string::npos) {
            dstRect = dstRectOrig;
            dstRect.y = dstRectOrig.y + dstRectOrig.h + static_cast<int>(viewInfo.ReflectionDistance * scaleY * dpiScaleY);
            dstRect.h = static_cast<int>(dstRectOrig.h * viewInfo.ReflectionScale);
            imageScaleY = (dstRect.h > 0) ? static_cast<double>(srcRectOrig.h) / dstRect.h : 0.0;
            srcRect = srcRectOrig;

            // Container clipping for reflection (same as top)
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
                    srcRect.y = srcRectOrig.y + static_cast<int>((dstRectOrig.h - dstRect.h) * imageScaleY);
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

        // Left reflection
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

            srcRect = srcRectOrig;

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
                angle += rotation_[viewInfo.Monitor] * 90.0;

            SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(viewInfo.ReflectionAlpha * alpha * 255.0f + 0.5f));

            if (mirror_[viewInfo.Monitor]) {
                if (rotation_[viewInfo.Monitor] % 2 == 0) {
                    if (srcRect.w > 0 && srcRect.h > 0) {
                        dstRect.y += renderHeight / 2;
                        SDL_RenderCopyEx(renderer, texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_HORIZONTAL);
                        dstRect.x = renderWidth - dstRect.x - dstRect.w;
                        dstRect.y = renderHeight - dstRect.y - dstRect.h;
                        angle += 180.0;
                        SDL_RenderCopyEx(renderer, texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_HORIZONTAL);
                    }
                }
                else {
                    if (srcRect.w > 0 && srcRect.h > 0) {
                        int tmp = dstRect.x;
                        dstRect.x = renderWidth / 2 - dstRect.y - dstRect.h / 2 - dstRect.w / 2;
                        dstRect.y = tmp - dstRect.h / 2 + dstRect.w / 2;
                        angle += 90.0;
                        SDL_RenderCopyEx(renderer, texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_HORIZONTAL);
                        dstRect.x = renderWidth - dstRect.x - dstRect.w;
                        dstRect.y = renderHeight - dstRect.y - dstRect.h;
                        angle += 180.0;
                        SDL_RenderCopyEx(renderer, texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_HORIZONTAL);
                    }
                }
            }
            else {
                // Apply rotation transforms
                SDL_Rect rotatedDst = dstRect;
                if (rotation_[viewInfo.Monitor] == 1) {
                    int tmp = rotatedDst.x;
                    rotatedDst.x = renderWidth - rotatedDst.y - rotatedDst.h / 2 - rotatedDst.w / 2;
                    rotatedDst.y = tmp - rotatedDst.h / 2 + rotatedDst.w / 2;
                }
                else if (rotation_[viewInfo.Monitor] == 2) {
                    rotatedDst.x = renderWidth - rotatedDst.x - rotatedDst.w;
                    rotatedDst.y = renderHeight - rotatedDst.y - rotatedDst.h;
                }
                else if (rotation_[viewInfo.Monitor] == 3) {
                    int tmp = rotatedDst.x;
                    rotatedDst.x = rotatedDst.y + rotatedDst.h / 2 - rotatedDst.w / 2;
                    rotatedDst.y = renderHeight - tmp - rotatedDst.h / 2 - rotatedDst.w / 2;
                }

                if (srcRect.w > 0 && srcRect.h > 0) {
                    SDL_RenderCopyEx(renderer, texture, &srcRect, &rotatedDst, angle, nullptr, SDL_FLIP_HORIZONTAL);
                }
            }
        }

        // Right reflection
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

            srcRect = srcRectOrig;

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
                angle += rotation_[viewInfo.Monitor] * 90.0;

            SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(viewInfo.ReflectionAlpha * alpha * 255.0f + 0.5f));

            if (mirror_[viewInfo.Monitor]) {
                if (rotation_[viewInfo.Monitor] % 2 == 0) {
                    if (srcRect.w > 0 && srcRect.h > 0) {
                        dstRect.y += renderHeight / 2;
                        SDL_RenderCopyEx(renderer, texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_HORIZONTAL);
                        dstRect.x = renderWidth - dstRect.x - dstRect.w;
                        dstRect.y = renderHeight - dstRect.y - dstRect.h;
                        angle += 180.0;
                        SDL_RenderCopyEx(renderer, texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_HORIZONTAL);
                    }
                }
                else {
                    if (srcRect.w > 0 && srcRect.h > 0) {
                        int tmp = dstRect.x;
                        dstRect.x = renderWidth / 2 - dstRect.y - dstRect.h / 2 - dstRect.w / 2;
                        dstRect.y = tmp - dstRect.h / 2 + dstRect.w / 2;
                        angle += 90.0;
                        SDL_RenderCopyEx(renderer, texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_HORIZONTAL);
                        dstRect.x = renderWidth - dstRect.x - dstRect.w;
                        dstRect.y = renderHeight - dstRect.y - dstRect.h;
                        angle += 180.0;
                        SDL_RenderCopyEx(renderer, texture, &srcRect, &dstRect, angle, nullptr, SDL_FLIP_HORIZONTAL);
                    }
                }
            }
            else {
                SDL_Rect rotatedDst = dstRect;
                if (rotation_[viewInfo.Monitor] == 1) {
                    int tmp = rotatedDst.x;
                    rotatedDst.x = renderWidth - rotatedDst.y - rotatedDst.h / 2 - rotatedDst.w / 2;
                    rotatedDst.y = tmp - rotatedDst.h / 2 + rotatedDst.w / 2;
                }
                else if (rotation_[viewInfo.Monitor] == 2) {
                    rotatedDst.x = renderWidth - rotatedDst.x - rotatedDst.w;
                    rotatedDst.y = renderHeight - rotatedDst.y - rotatedDst.h;
                }
                else if (rotation_[viewInfo.Monitor] == 3) {
                    int tmp = rotatedDst.x;
                    rotatedDst.x = rotatedDst.y + rotatedDst.h / 2 - rotatedDst.w / 2;
                    rotatedDst.y = renderHeight - tmp - rotatedDst.h / 2 - rotatedDst.w / 2;
                }

                if (srcRect.w > 0 && srcRect.h > 0) {
                    SDL_RenderCopyEx(renderer, texture, &srcRect, &rotatedDst, angle, nullptr, SDL_FLIP_HORIZONTAL);
                }
            }
        }

        return true;
    }
}

bool SDL::hasActiveMarquee()
{
    // Start from index 1  skip primary monitor (0)
    for (size_t i = 1; i < renderer_.size(); ++i)
    {
        if (renderer_[i] != nullptr)
            return true;
    }
    return false;
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

bool SDL::renderShapeGeometry(const ViewInfo& viewInfo, int layoutWidth, int layoutHeight)
{
    float alpha = viewInfo.BackgroundAlpha * viewInfo.Alpha;
    if (alpha <= 0.001f || viewInfo.Monitor >= static_cast<int>(numScreens_) || !renderer_[viewInfo.Monitor])
        return true;

    SDL_Renderer* renderer = renderer_[viewInfo.Monitor];
    int renderWidth, renderHeight;
    SDL_GetRendererOutputSize(renderer, &renderWidth, &renderHeight);

    float dpiScaleX = static_cast<float>(renderWidth) / windowWidth_[viewInfo.Monitor];
    float dpiScaleY = static_cast<float>(renderHeight) / windowHeight_[viewInfo.Monitor];
    float winW = static_cast<float>(windowWidth_[viewInfo.Monitor]);
    float winH = static_cast<float>(windowHeight_[viewInfo.Monitor]);
    float layW = static_cast<float>(layoutWidth);
    float layH = static_cast<float>(layoutHeight);

    float scaleX = winW / layW;
    float scaleY = winH / layH;
    float offsetX = 0.0f, offsetY = 0.0f;

    std::string layoutMode = "stretch";
    float layoutScale = 1.0f;
    if (config_) {
        config_->getProperty("layoutScaleMode", layoutMode);
        config_->getProperty("layoutZoomMode", layoutScale);
    }
    if (layoutMode == "fit") {
        float s = (scaleX < scaleY ? scaleX : scaleY) * layoutScale;
        scaleX = s;
        scaleY = s;
        offsetX = (winW - layW * scaleX) * 0.5f;
        offsetY = (winH - layH * scaleY) * 0.5f;
    }

    auto toScreen = [&](float lx, float ly) -> SDL_FPoint {
        return { (lx * scaleX + offsetX) * dpiScaleX, (ly * scaleY + offsetY) * dpiScaleY };
    };

    auto clampU8 = [](float v) -> Uint8 {
        if (v < 0.0f) return 0;
        if (v > 255.0f) return 255;
        return static_cast<Uint8>(v);
    };

    Uint8 r8 = clampU8(viewInfo.BackgroundRed * 255.0f);
    Uint8 g8 = clampU8(viewInfo.BackgroundGreen * 255.0f);
    Uint8 b8 = clampU8(viewInfo.BackgroundBlue * 255.0f);
    Uint8 a8 = clampU8(alpha * 255.0f);

    SDL_Color colorSolid = { r8, g8, b8, a8 };
    SDL_Color colorTrans = { r8, g8, b8, 0 };

    std::vector<SDL_FPoint> points;
    std::vector<SDL_Color> colors;
    std::vector<int> indices;

    auto addVertex = [&](float lx, float ly, const SDL_Color& col) -> int {
        int idx = static_cast<int>(points.size());
        points.push_back(toScreen(lx, ly));
        colors.push_back(col);
        return idx;
    };

    auto addQuad = [&](int v00, int v01, int v10, int v11) {
        indices.push_back(v00);
        indices.push_back(v01);
        indices.push_back(v10);

        indices.push_back(v01);
        indices.push_back(v11);
        indices.push_back(v10);
    };

    float fringeX = (scaleX * dpiScaleX > 0.0f) ? (1.2f / (scaleX * dpiScaleX)) : 1.0f;
    float fringeY = (scaleY * dpiScaleY > 0.0f) ? (1.2f / (scaleY * dpiScaleY)) : 1.0f;

    std::string shape = viewInfo.Shape;
    if (shape.empty()) shape = "rect";

    if (shape == "curve" || shape == "wheel") {
        const int N = 256;
        std::string pos = viewInfo.CurvePosition;
        if (pos.empty()) pos = "top";
        std::string mode = viewInfo.CurveMode;
        if (mode.empty()) mode = (viewInfo.Thickness > 0.0f || viewInfo.Height > 0.0f || viewInfo.Width > 0.0f) ? "line" : "shape";
        float depth = viewInfo.CurveDepth;

        if (pos == "top" || pos == "bottom") {
            float startX = (viewInfo.X > 0.0f) ? (viewInfo.X + viewInfo.XOffset) : 0.0f;
            float totalW = (viewInfo.Width > 0.0f) ? viewInfo.Width : layW;
            float baseY = viewInfo.Y + viewInfo.YOffset;
            if (baseY == 0.0f && pos == "bottom") baseY = layH * 0.75f;
            float thick = (viewInfo.Thickness > 0.0f) ? viewInfo.Thickness : ((viewInfo.Height > 0.0f) ? viewInfo.Height : 4.0f);
            float f = fringeY;

            std::vector<std::vector<int>> stepVerts(N + 1);

            for (int i = 0; i <= N; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(N);
                float lx = startX + t * totalW;
                float arc = depth * 4.0f * t * (1.0f - t);
                float ly = (pos == "top") ? (baseY + arc) : (baseY - arc);

                if (mode == "shape" || mode == "fill") {
                    if (pos == "top") {
                        int v0 = addVertex(lx, 0.0f, colorSolid);
                        int v1 = addVertex(lx, ly, colorSolid);
                        int v2 = addVertex(lx, ly + f, colorTrans);
                        stepVerts[i] = { v0, v1, v2 };
                    } else { // bottom
                        int v0 = addVertex(lx, ly - f, colorTrans);
                        int v1 = addVertex(lx, ly, colorSolid);
                        int v2 = addVertex(lx, layH, colorSolid);
                        stepVerts[i] = { v0, v1, v2 };
                    }
                } else { // line
                    if (thick > 1.5f * f) {
                        int v0 = addVertex(lx, ly - f, colorTrans);
                        int v1 = addVertex(lx, ly, colorSolid);
                        int v2 = addVertex(lx, ly + thick, colorSolid);
                        int v3 = addVertex(lx, ly + thick + f, colorTrans);
                        stepVerts[i] = { v0, v1, v2, v3 };
                    } else {
                        int v0 = addVertex(lx, ly - f, colorTrans);
                        int v1 = addVertex(lx, ly + thick * 0.5f, colorSolid);
                        int v2 = addVertex(lx, ly + thick + f, colorTrans);
                        stepVerts[i] = { v0, v1, v2 };
                    }
                }
            }

            for (int i = 0; i < N; ++i) {
                size_t numTracks = stepVerts[i].size() - 1;
                for (size_t tr = 0; tr < numTracks; ++tr) {
                    addQuad(stepVerts[i][tr], stepVerts[i][tr + 1], stepVerts[i + 1][tr], stepVerts[i + 1][tr + 1]);
                }
            }
        } else { // left or right
            float startY = (viewInfo.Y > 0.0f) ? (viewInfo.Y + viewInfo.YOffset) : 0.0f;
            float totalH = (viewInfo.Height > 0.0f) ? viewInfo.Height : layH;
            float baseX = viewInfo.X + viewInfo.XOffset;
            if (baseX == 0.0f && pos == "right") baseX = layW * 0.9f;
            float thick = (viewInfo.Thickness > 0.0f) ? viewInfo.Thickness : ((viewInfo.Width > 0.0f) ? viewInfo.Width : 4.0f);
            float f = fringeX;

            std::vector<std::vector<int>> stepVerts(N + 1);

            for (int i = 0; i <= N; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(N);
                float ly = startY + t * totalH;
                float arc = depth * 4.0f * t * (1.0f - t);
                float lx = (pos == "left") ? (baseX + arc) : (baseX - arc);

                if (mode == "shape" || mode == "fill") {
                    if (pos == "left") {
                        int v0 = addVertex(0.0f, ly, colorSolid);
                        int v1 = addVertex(lx, ly, colorSolid);
                        int v2 = addVertex(lx + f, ly, colorTrans);
                        stepVerts[i] = { v0, v1, v2 };
                    } else { // right
                        int v0 = addVertex(lx - f, ly, colorTrans);
                        int v1 = addVertex(lx, ly, colorSolid);
                        int v2 = addVertex(layW, ly, colorSolid);
                        stepVerts[i] = { v0, v1, v2 };
                    }
                } else { // line
                    if (thick > 1.5f * f) {
                        int v0 = addVertex(lx - f, ly, colorTrans);
                        int v1 = addVertex(lx, ly, colorSolid);
                        int v2 = addVertex(lx + thick, ly, colorSolid);
                        int v3 = addVertex(lx + thick + f, ly, colorTrans);
                        stepVerts[i] = { v0, v1, v2, v3 };
                    } else {
                        int v0 = addVertex(lx - f, ly, colorTrans);
                        int v1 = addVertex(lx + thick * 0.5f, ly, colorSolid);
                        int v2 = addVertex(lx + thick + f, ly, colorTrans);
                        stepVerts[i] = { v0, v1, v2 };
                    }
                }
            }

            for (int i = 0; i < N; ++i) {
                size_t numTracks = stepVerts[i].size() - 1;
                for (size_t tr = 0; tr < numTracks; ++tr) {
                    addQuad(stepVerts[i][tr], stepVerts[i][tr + 1], stepVerts[i + 1][tr], stepVerts[i + 1][tr + 1]);
                }
            }
        }
    }
    else if (shape == "circle" || shape == "ring" || shape == "arc") {
        const int N = 256;
        float cx = viewInfo.X + viewInfo.XOffset;
        float cy = viewInfo.Y + viewInfo.YOffset;
        float r = (viewInfo.Radius > 0.0f) ? viewInfo.Radius : (viewInfo.ScaledWidth() * 0.5f);
        if (r <= 0.0f) r = 50.0f;
        float thick = viewInfo.Thickness;
        float aStart = viewInfo.StartAngle * 3.14159265358979323846f / 180.0f;
        float aEnd = (shape == "arc") ? (viewInfo.EndAngle * 3.14159265358979323846f / 180.0f) : (aStart + 2.0f * 3.14159265358979323846f);
        float f = (fringeX + fringeY) * 0.5f;

        if (thick <= 0.0f || thick >= r) {
            // Solid filled disc with outer AA fringe
            int vc = addVertex(cx, cy, colorSolid);
            std::vector<std::pair<int, int>> rim(N + 1);
            for (int i = 0; i <= N; ++i) {
                float theta = aStart + (aEnd - aStart) * (static_cast<float>(i) / static_cast<float>(N));
                float cosT = std::cos(theta);
                float sinT = std::sin(theta);
                int vInner = addVertex(cx + r * cosT, cy + r * sinT, colorSolid);
                int vOuter = addVertex(cx + (r + f) * cosT, cy + (r + f) * sinT, colorTrans);
                rim[i] = { vInner, vOuter };
            }
            for (int i = 0; i < N; ++i) {
                indices.push_back(vc);
                indices.push_back(rim[i].first);
                indices.push_back(rim[i + 1].first);
                addQuad(rim[i].first, rim[i].second, rim[i + 1].first, rim[i + 1].second);
            }
        } else {
            // Hollow ring / frame with AA fringe on both edges
            float rInner = r - thick;
            std::vector<std::vector<int>> stepVerts(N + 1);
            for (int i = 0; i <= N; ++i) {
                float theta = aStart + (aEnd - aStart) * (static_cast<float>(i) / static_cast<float>(N));
                float cosT = std::cos(theta);
                float sinT = std::sin(theta);

                int v0 = addVertex(cx + (rInner - f) * cosT, cy + (rInner - f) * sinT, colorTrans);
                int v1 = addVertex(cx + rInner * cosT, cy + rInner * sinT, colorSolid);
                int v2 = addVertex(cx + r * cosT, cy + r * sinT, colorSolid);
                int v3 = addVertex(cx + (r + f) * cosT, cy + (r + f) * sinT, colorTrans);
                stepVerts[i] = { v0, v1, v2, v3 };
            }
            for (int i = 0; i < N; ++i) {
                for (size_t tr = 0; tr < 3; ++tr) {
                    addQuad(stepVerts[i][tr], stepVerts[i][tr + 1], stepVerts[i + 1][tr], stepVerts[i + 1][tr + 1]);
                }
            }
        }
    }

    if (points.empty() || indices.empty())
        return true;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometryRaw(renderer, nullptr,
        reinterpret_cast<const float*>(points.data()), sizeof(SDL_FPoint),
        reinterpret_cast<const SDL_Color*>(colors.data()), sizeof(SDL_Color),
        nullptr, 0,
        static_cast<int>(points.size()),
        indices.data(),
        static_cast<int>(indices.size()),
        sizeof(int));

    return true;
}

void SDL::renderCopyEx3D(
    SDL_Texture* texture,
    float renderW, float renderH,
    const SDL_Rect* pSrcRect,
    ViewInfo& viewInfo,
    Page& page)
{
    // ------------------------------------------------------------------
    // 1. Determine dominant axis (Y wins if equal)
    // ------------------------------------------------------------------
    bool yAxis = (std::abs(viewInfo.AngleY) >= std::abs(viewInfo.AngleX));
    float angleDeg = yAxis ? viewInfo.AngleY : viewInfo.AngleX;
    float radians = angleDeg * float(M_PI) / 180.0f;
    float cosAngle = std::cos(radians);
    float absCos = std::fabs(cosAngle);
    float direction = (angleDeg < 0.0f ? -1.0f : 1.0f);

    // ------------------------------------------------------------------
    // 2. Base 3D shrink
    // ------------------------------------------------------------------
    float w3D = renderW;
    float h3D = renderH;

    if (yAxis) {
        // Horizontal rotation  shrink width
        w3D = renderW * absCos;
        h3D = renderH * (0.8f + 0.2f * absCos);
    }
    else {
        // Vertical rotation  shrink height
        h3D = renderH * absCos;
        w3D = renderW * (0.8f + 0.2f * absCos);
    }

    // ------------------------------------------------------------------
    // 3. Z-depth scale
    // ------------------------------------------------------------------
    float ax = viewInfo.AngleX * float(M_PI) / 180.0f;
    float ay = viewInfo.AngleY * float(M_PI) / 180.0f;
    float facing = std::cos(ax) * std::cos(ay);
    float zScale = 1.0f + std::fabs(facing) * 0.5f;
    w3D *= zScale;
    h3D *= zScale;

    // ------------------------------------------------------------------
    // 4. Layout origins (0=left/top, 0.5=center, 1=right/bottom)
    // ------------------------------------------------------------------
    float originX = viewInfo.XOrigin;
    float originY = viewInfo.YOrigin;

    // Effective rotation origin:
    float rotOriginX = yAxis ? originX : 0.5f;
    float rotOriginY = yAxis ? 0.5f : originY;

    // ------------------------------------------------------------------
    // 5. Base pivot (layout position + offset)
    // ------------------------------------------------------------------
    float pivotX = viewInfo.X + viewInfo.XOffset;
    float pivotY = viewInfo.Y + viewInfo.YOffset;

    // ------------------------------------------------------------------
    // 6. Signed rotation offset to simulate front/back movement
    // ------------------------------------------------------------------
    float offsetX = 0.0f;
    float offsetY = 0.0f;

    if (yAxis) {
        // Horizontal rotation (Y-axis)
        float side = 0.0f;
        if (originX < 0.5f) side = -1.0f;      // left origin
        else if (originX > 0.5f) side = 1.0f;  // right origin

        offsetX = (renderW * (1.0f - absCos)) * direction * side;
        offsetX += (renderW * (1.0f - absCos)) * (-direction) * 0.5f; // 3D tilt
    }
    else {
        // Vertical rotation (X-axis)
        float side = 0.0f;
        if (originY < 0.5f) side = -1.0f;      // top origin
        else if (originY > 0.5f) side = 1.0f;  // bottom origin

        offsetY = (renderH * (1.0f - absCos)) * direction * side;
        offsetY += (renderH * (1.0f - absCos)) * (-direction) * 0.5f; // 3D tilt
    }

    pivotX += offsetX;
    pivotY += offsetY;

    // ------------------------------------------------------------------
    // 7. Final placement using effective rotation origin
    // ------------------------------------------------------------------
    float x = pivotX - rotOriginX * w3D;
    float y = pivotY - rotOriginY * h3D;

    // ------------------------------------------------------------------
    // 8. Apply display scaling
    // ------------------------------------------------------------------
    float scaleX = page.getScaleX(viewInfo.Monitor);
    float scaleY = page.getScaleY(viewInfo.Monitor);

    SDL_Rect destRect = {
        int(x * scaleX),
        int(y * scaleY),
        int(w3D * scaleX),
        int(h3D * scaleY)
    };

    // ------------------------------------------------------------------
    // 9. Flip when back-facing
    // ------------------------------------------------------------------
    SDL_RendererFlip flip = SDL_FLIP_NONE;
    if (cosAngle < 0.0f)
        flip = yAxis ? SDL_FLIP_HORIZONTAL : SDL_FLIP_VERTICAL;

    // ------------------------------------------------------------------
    // 10. Render
    // ------------------------------------------------------------------
    SDL_SetTextureAlphaMod(texture, Uint8(viewInfo.Alpha * 255));
    SDL_RenderCopyEx(
        getRenderer(viewInfo.Monitor),
        texture,
        pSrcRect,
        &destRect,
        0.0,
        nullptr,
        flip
    );
}





void SDL::renderCopySlice3D(SDL_Texture* texture,
    int origW, int origH,
    float renderW, float renderH,
    ViewInfo& viewInfo,
    Page& page)
{
    const int slices = 96;
    const int BLEED = 1;

    float pivotX = viewInfo.X + viewInfo.XOffset - viewInfo.XOrigin * renderW + renderW * 0.5f;
    float pivotY = viewInfo.Y + viewInfo.YOffset - viewInfo.YOrigin * renderH + renderH * 0.5f;

    float scaleX = page.getScaleX(viewInfo.Monitor);
    float scaleY = page.getScaleY(viewInfo.Monitor);

    SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(viewInfo.Alpha * 255.0f + 0.5f));

    float angle = (viewInfo.Slice3DX != 0.0f) ? viewInfo.Slice3DX : viewInfo.Slice3DY;
    bool reverse = (angle < 0.0f);

    // Special case: angle near zero draw full image as one slice
    if (std::abs(angle) <= 0.02f)
    {
        SDL_Rect srcSlice = { 0, 0, origW, origH };
        SDL_Rect dst = {
            static_cast<int>((pivotX - renderW * 0.5f) * scaleX + 0.5f),
            static_cast<int>((pivotY - renderH * 0.5f) * scaleY + 0.5f),
            static_cast<int>(renderW * scaleX + 0.5f),
            static_cast<int>(renderH * scaleY + 0.5f)
        };
        SDL_RenderCopy(getRenderer(viewInfo.Monitor), texture, &srcSlice, &dst);
        return;
    }

    auto getIndex = [&](int i) { return reverse ? (slices - 1 - i) : i; };

    if (viewInfo.Slice3DX != 0.0f)
    {
        float ax = angle * static_cast<float>(M_PI) / 180.0f;

        for (int rawI = 0; rawI < slices; ++rawI)
        {
            int i = getIndex(rawI);
            float t = static_cast<float>(i) / slices;
            float nextT = static_cast<float>(i + 1) / slices;

            float sliceScale = std::cos(ax * t) * 0.5f + 0.5f;
            float depth = std::cos(ax) * (1.0f - std::abs(2.0f * t - 1.0f));
            float zOffset = depth * 20.0f;

            int srcX = static_cast<int>(origW * t - BLEED + 0.5f);
            int srcW = static_cast<int>(origW * (nextT - t) + BLEED * 2 + 0.5f);
            srcX = std::max(0, srcX);
            srcW = std::min(srcW, origW - srcX);

            SDL_Rect srcSlice = { srcX, 0, srcW, origH };

            float wSlice = renderW * (nextT - t) * sliceScale + BLEED * 2;
            float xOffset = pivotX - renderW * 0.5f + renderW * t * sliceScale + zOffset - BLEED;
            float yOffset = pivotY - renderH * 0.5f;

            SDL_Rect dst = {
                static_cast<int>(xOffset * scaleX + 0.5f),
                static_cast<int>(yOffset * scaleY + 0.5f),
                static_cast<int>(wSlice * scaleX + 0.5f),
                static_cast<int>(renderH * scaleY + 0.5f)
            };

            SDL_RenderCopy(getRenderer(viewInfo.Monitor), texture, &srcSlice, &dst);
        }
    }
    else if (viewInfo.Slice3DY != 0.0f)
    {
        float ay = angle * static_cast<float>(M_PI) / 180.0f;

        for (int rawI = 0; rawI < slices; ++rawI)
        {
            int i = getIndex(rawI);
            float t = static_cast<float>(i) / slices;
            float nextT = static_cast<float>(i + 1) / slices;

            float sliceScale = std::cos(ay * t) * 0.5f + 0.5f;
            float depth = std::cos(ay) * (1.0f - std::abs(2.0f * t - 1.0f));
            float zOffset = depth * 20.0f;

            int srcY = static_cast<int>(origH * t - BLEED + 0.5f);
            int srcH = static_cast<int>(origH * (nextT - t) + BLEED * 2 + 0.5f);
            srcY = std::max(0, srcY);
            srcH = std::min(srcH, origH - srcY);

            SDL_Rect srcSlice = { 0, srcY, origW, srcH };

            float hSlice = renderH * (nextT - t) * sliceScale + BLEED * 2;
            float xOffset = pivotX - renderW * 0.5f;
            float yOffset = pivotY - renderH * 0.5f + renderH * t * sliceScale + zOffset - BLEED;

            SDL_Rect dst = {
                static_cast<int>(xOffset * scaleX + 0.5f),
                static_cast<int>(yOffset * scaleY + 0.5f),
                static_cast<int>(renderW * scaleX + 0.5f),
                static_cast<int>(hSlice * scaleY + 0.5f)
            };

            SDL_RenderCopy(getRenderer(viewInfo.Monitor), texture, &srcSlice, &dst);
        }
    }
}
