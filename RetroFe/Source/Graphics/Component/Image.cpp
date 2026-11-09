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
#include "Image.h"
#include "JxlLoader.h"
#include "../ViewInfo.h"
#include "../../SDL.h"
#include "../../Utility/Log.h"
#include <SDL_image.h>
#include <SDL2_rotozoom.h>
#include "../../Database/Configuration.h"
#include <random>
#include <filesystem>
#include <cmath>
#include <cstring>
#include <vector>
#include <dirent.h>

Configuration* Image::config_ = nullptr;


Image::Image(std::string file, std::string altFile, Page &p, int monitor, bool random, bool slideShow, int slideShowTimer, int slideNumber, std::string randomSrc, std::string altRandomSrc)
    : Component(p)
    , texture_(NULL)
    , file_(file)
    , altFile_(altFile)
    , random_(random)
    , slideShow_(slideShow)
    , slideShowTimer_(slideShowTimer)
    , slideNumber_(slideNumber)
    , randomSrc_(randomSrc)
    , altRandomSrc_(altRandomSrc)
    , currentImageIndex_(0)
    , lastSwitchTime_(0)
{
    baseViewInfo.Monitor = monitor;

    // If random is true and randomSrc is provided, load the image list and set initial image
    if (random_ && !randomSrc_.empty()) {
        loadImageList();
        if (!imageList_.empty()) {
            file_ = imageList_[currentImageIndex_]; // Set initial image from list
            Logger::write(Logger::ZONE_INFO, "Image", "Initial random image set to: " + file_);
        }
        else {
            Logger::write(Logger::ZONE_WARNING, "Image", "No images found in randomSrc: " + randomSrc_);
        }
    }

    allocateGraphicsMemory();
}

Image::~Image()
{
    freeGraphicsMemory();

    }

void Image::freeGraphicsMemory()
{
    Component::freeGraphicsMemory();
    
    SDL_LockMutex(SDL::getMutex());
    if (texture_ != NULL)
    {
        SDL_DestroyTexture(texture_);
        texture_ = NULL;
        
    }
    SDL_UnlockMutex(SDL::getMutex());
}


static SDL_Surface* resampleAntiAliased(SDL_Surface* src, int dstW, int dstH, int pad, bool useLanczos)
{
    int srcW = src->w;
    int srcH = src->h;
    if (dstW < 1) dstW = 1;
    if (dstH < 1) dstH = 1;

    int outW = dstW + pad * 2;
    int outH = dstH + pad * 2;

    SDL_Surface* out = SDL_CreateRGBSurfaceWithFormat(0, outW, outH, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!out) return src;
    std::memset(out->pixels, 0, static_cast<size_t>(out->pitch) * outH);

    const uint8_t* srcBase = static_cast<const uint8_t*>(src->pixels);
    int srcPitch = src->pitch;

    // Horizontal pass into temporary premultiplied buffer (dstW * srcH * 4 channels: R*A, G*A, B*A, A)
    std::vector<float> tmp(static_cast<size_t>(dstW) * srcH * 4);
    float scaleX = static_cast<float>(srcW) / static_cast<float>(dstW);
    float supportX = (scaleX > 1.0f) ? scaleX : 1.0f;
    float radius = useLanczos ? 2.0f : 1.0f;

    auto kernelWeight = [useLanczos](float x) -> float {
        x = std::fabs(x);
        if (useLanczos)
        {
            if (x < 1e-5f) return 1.0f;
            if (x >= 2.0f) return 0.0f;
            const float pi = 3.14159265358979323846f;
            float px = pi * x;
            float px2 = px * 0.5f;
            return (std::sin(px) / px) * (std::sin(px2) / px2);
        }
        else
        {
            return (x < 1.0f) ? (1.0f - x) : 0.0f;
        }
    };

    struct Tap { int idx; float weight; };
    std::vector<std::vector<Tap>> xTaps(dstW);
    for (int dx = 0; dx < dstW; ++dx)
    {
        float center = (dx + 0.5f) * scaleX - 0.5f;
        int left = static_cast<int>(std::floor(center - radius * supportX));
        int right = static_cast<int>(std::ceil(center + radius * supportX));
        float sumW = 0.0f;
        auto& taps = xTaps[dx];
        taps.reserve(right - left + 1);
        for (int sx = left; sx <= right; ++sx)
        {
            float w = kernelWeight((sx - center) / supportX);
            if (std::fabs(w) > 1e-6f)
            {
                int clampedX = (sx < 0) ? 0 : ((sx >= srcW) ? (srcW - 1) : sx);
                taps.push_back({ clampedX, w });
                sumW += w;
            }
        }
        if (std::fabs(sumW) > 1e-6f)
        {
            float invSum = 1.0f / sumW;
            for (auto& t : taps) t.weight *= invSum;
        }
    }

    for (int sy = 0; sy < srcH; ++sy)
    {
        const uint32_t* sRow = reinterpret_cast<const uint32_t*>(srcBase + sy * srcPitch);
        float* tRow = &tmp[static_cast<size_t>(sy) * dstW * 4];
        for (int dx = 0; dx < dstW; ++dx)
        {
            float accR = 0.0f, accG = 0.0f, accB = 0.0f, accA = 0.0f;
            for (const auto& tap : xTaps[dx])
            {
                uint32_t p = sRow[tap.idx];
                float a = static_cast<float>((p >> 24) & 0xFF);
                float aw = a * tap.weight;
                accR += static_cast<float>((p >> 16) & 0xFF) * aw;
                accG += static_cast<float>((p >> 8) & 0xFF) * aw;
                accB += static_cast<float>(p & 0xFF) * aw;
                accA += aw;
            }
            float* dstPx = &tRow[dx * 4];
            dstPx[0] = accR;
            dstPx[1] = accG;
            dstPx[2] = accB;
            dstPx[3] = accA;
        }
    }

    // Vertical pass from tmp into out
    float scaleY = static_cast<float>(srcH) / static_cast<float>(dstH);
    float supportY = (scaleY > 1.0f) ? scaleY : 1.0f;
    uint8_t* outBase = static_cast<uint8_t*>(out->pixels);
    int outPitch = out->pitch;

    std::vector<Tap> yTaps;
    for (int dy = 0; dy < dstH; ++dy)
    {
        float center = (dy + 0.5f) * scaleY - 0.5f;
        int top = static_cast<int>(std::floor(center - radius * supportY));
        int bot = static_cast<int>(std::ceil(center + radius * supportY));
        float sumW = 0.0f;
        yTaps.clear();
        for (int sy = top; sy <= bot; ++sy)
        {
            float w = kernelWeight((sy - center) / supportY);
            if (std::fabs(w) > 1e-6f)
            {
                int clampedY = (sy < 0) ? 0 : ((sy >= srcH) ? (srcH - 1) : sy);
                yTaps.push_back({ clampedY, w });
                sumW += w;
            }
        }
        if (std::fabs(sumW) > 1e-6f)
        {
            float invSum = 1.0f / sumW;
            for (auto& t : yTaps) t.weight *= invSum;
        }

        uint32_t* dRow = reinterpret_cast<uint32_t*>(outBase + (dy + pad) * outPitch) + pad;
        for (int dx = 0; dx < dstW; ++dx)
        {
            float accR = 0.0f, accG = 0.0f, accB = 0.0f, accA = 0.0f;
            for (const auto& tap : yTaps)
            {
                const float* tPx = &tmp[(static_cast<size_t>(tap.idx) * dstW + dx) * 4];
                accR += tPx[0] * tap.weight;
                accG += tPx[1] * tap.weight;
                accB += tPx[2] * tap.weight;
                accA += tPx[3] * tap.weight;
            }

            int outA = static_cast<int>(std::round(accA));
            if (outA <= 0)
            {
                dRow[dx] = 0;
            }
            else
            {
                if (outA > 255) outA = 255;
                float invA = 1.0f / accA;
                int outR = static_cast<int>(std::round(accR * invA));
                int outG = static_cast<int>(std::round(accG * invA));
                int outB = static_cast<int>(std::round(accB * invA));
                outR = (outR < 0) ? 0 : ((outR > 255) ? 255 : outR);
                outG = (outG < 0) ? 0 : ((outG > 255) ? 255 : outG);
                outB = (outB < 0) ? 0 : ((outB > 255) ? 255 : outB);
                dRow[dx] = (static_cast<uint32_t>(outA) << 24) |
                           (static_cast<uint32_t>(outR) << 16) |
                           (static_cast<uint32_t>(outG) << 8) |
                           static_cast<uint32_t>(outB);
            }
        }
    }

    // Bleed RGB into fully transparent border neighbors so GPU bilinear filtering never samples black fringes
    for (int y = 0; y < outH; ++y)
    {
        int ym1 = (y > 0) ? (y - 1) : y;
        int yp1 = (y + 1 < outH) ? (y + 1) : y;
        const uint32_t* rPrev = reinterpret_cast<const uint32_t*>(outBase + ym1 * outPitch);
        uint32_t* rCurr = reinterpret_cast<uint32_t*>(outBase + y * outPitch);
        const uint32_t* rNext = reinterpret_cast<const uint32_t*>(outBase + yp1 * outPitch);
        for (int x = 0; x < outW; ++x)
        {
            uint32_t c = rCurr[x];
            if (((c >> 24) & 0xFF) == 0)
            {
                int xm1 = (x > 0) ? (x - 1) : x;
                int xp1 = (x + 1 < outW) ? (x + 1) : x;
                uint32_t pN = rPrev[x], pS = rNext[x], pW = rCurr[xm1], pE = rCurr[xp1];
                uint32_t aN = (pN >> 24) & 0xFF, aS = (pS >> 24) & 0xFF, aW = (pW >> 24) & 0xFF, aE = (pE >> 24) & 0xFF;
                uint32_t sumA = aN + aS + aW + aE;
                if (sumA > 0)
                {
                    uint32_t r = (((pN >> 16) & 0xFF) * aN + ((pS >> 16) & 0xFF) * aS + ((pW >> 16) & 0xFF) * aW + ((pE >> 16) & 0xFF) * aE) / sumA;
                    uint32_t g = (((pN >> 8) & 0xFF) * aN + ((pS >> 8) & 0xFF) * aS + ((pW >> 8) & 0xFF) * aW + ((pE >> 8) & 0xFF) * aE) / sumA;
                    uint32_t b = ((pN & 0xFF) * aN + (pS & 0xFF) * aS + (pW & 0xFF) * aW + (pE & 0xFF) * aE) / sumA;
                    rCurr[x] = (r << 16) | (g << 8) | b;
                }
            }
        }
    }

    SDL_FreeSurface(src);
    return out;
}

static SDL_Surface* prepareAntiAliasedSurface(SDL_Surface* surface, int monitor)
{
    if (!surface || surface->w < 4 || surface->h < 4) return surface;

    std::string aaMode = "false";
    if (Image::config_)
    {
        if (!Image::config_->getProperty("antiAliasing", aaMode))
        {
            if (!Image::config_->getProperty("antialiasing", aaMode))
            {
                Image::config_->getProperty("antialianse", aaMode);
            }
        }
    }
    aaMode = Utils::toLower(Utils::trimEnds(aaMode));

    if (aaMode.empty() || aaMode == "false" || aaMode == "off" || aaMode == "no" || aaMode == "0")
    {
        return surface;
    }

    bool useLanczos = (aaMode == "lanczos2" || aaMode == "lanczos" || aaMode == "true" || aaMode == "on" || aaMode == "yes" || aaMode == "1");
    bool useTent = (aaMode == "tent" || aaMode == "area" || aaMode == "smooth");
    if (!useLanczos && !useTent)
    {
        return surface;
    }

    // Never modify full-screen images / overlays (e.g. 1920x1080 fanarts, bezels, shadows)
    if (surface->w >= 1200)
    {
        return surface;
    }

    SDL_Surface* argb = surface;
    if (surface->format->format != SDL_PIXELFORMAT_ARGB8888)
    {
        argb = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ARGB8888, 0);
        if (!argb) return surface;
        SDL_FreeSurface(surface);
    }

    int w = argb->w;
    int h = argb->h;
    const uint8_t* pxBase = static_cast<const uint8_t*>(argb->pixels);
    int pitch = argb->pitch;

    const uint32_t* topRow = reinterpret_cast<const uint32_t*>(pxBase);
    const uint32_t* botRow = reinterpret_cast<const uint32_t*>(pxBase + (h - 1) * pitch);
    uint8_t a00 = (topRow[0] >> 24) & 0xFF;
    uint8_t a10 = (topRow[w - 1] >> 24) & 0xFF;
    uint8_t a01 = (botRow[0] >> 24) & 0xFF;
    uint8_t a11 = (botRow[w - 1] >> 24) & 0xFF;
    bool hasTransparentCorners = (a00 < 250 || a10 < 250 || a01 < 250 || a11 < 250);

    if (!hasTransparentCorners)
    {
        return argb;
    }

    int winH = SDL::getWindowHeight(monitor);
    float dispScale = (winH > 0) ? (static_cast<float>(winH) / 1080.0f) : 1.0f;
    if (dispScale < 1.0f) dispScale = 1.0f;

    int dstW = w;
    int dstH = h;

    int maxCoverH = static_cast<int>(std::round(500.0f * dispScale));
    int maxLogoW = static_cast<int>(std::round((useLanczos ? 500.0f : 300.0f) * dispScale));

    if (h > w && h > maxCoverH)
    {
        dstH = maxCoverH;
        dstW = static_cast<int>(std::round(static_cast<float>(w) * dstH / static_cast<float>(h)));
    }
    else if (w >= h && w > maxLogoW)
    {
        dstW = maxLogoW;
        dstH = static_cast<int>(std::round(static_cast<float>(h) * dstW / static_cast<float>(w)));
    }

    return resampleAntiAliased(argb, dstW, dstH, 2, useLanczos);
}

void Image::allocateGraphicsMemory() {
    int width, height;
    if (!texture_) {
        SDL_Surface* surface = JxlLoader::IsJxlFile(file_) ? JxlLoader::LoadSurface(file_) : IMG_Load(file_.c_str());
        if (!surface && !altFile_.empty()) surface = JxlLoader::IsJxlFile(altFile_) ? JxlLoader::LoadSurface(altFile_) : IMG_Load(altFile_.c_str());

        if (surface) {
            int targetWidth = static_cast<int>(baseViewInfo.Width * SDL::getWindowWidth(baseViewInfo.Monitor) / page.getLayoutWidth(baseViewInfo.Monitor));
            int targetHeight = static_cast<int>(baseViewInfo.Height * SDL::getWindowHeight(baseViewInfo.Monitor) / page.getLayoutHeight(baseViewInfo.Monitor));

            // Check if the image is 4K and needs scaling
            if ((surface->w >= 3840 || surface->h >= 2160) && (surface->w > targetWidth || surface->h > targetHeight) && targetWidth > 0 && targetHeight > 0) {
                SDL_Surface* scaled = nullptr;
                std::string preScaleQuality = "off";
                if (config_) {
                    config_->getProperty("4kPreScaleQuality", preScaleQuality);
                }
                else {
                    Logger::write(Logger::ZONE_WARNING, "Image", "Configuration is null; using default 'off' for 4kPreScaleQuality");
                }

                if (preScaleQuality == "smooth") {
                    double scaleX = static_cast<double>(targetWidth) / surface->w;
                    double scaleY = static_cast<double>(targetHeight) / surface->h;
                    scaled = zoomSurface(surface, scaleX, scaleY, 1);
                    if (scaled) {
                        Logger::write(Logger::ZONE_INFO, "Image", "Pre-scaled 4K image with zoomSurface to " +
                            std::to_string(targetWidth) + "x" + std::to_string(targetHeight));
                    }
                }
                else if (preScaleQuality == "fast") {
                    scaled = SDL_CreateRGBSurfaceWithFormat(0, targetWidth, targetHeight, 32, surface->format->format);
                    if (scaled) {
                        SDL_BlitScaled(surface, nullptr, scaled, nullptr);
                    }
                }
                else {
                    Logger::write(Logger::ZONE_INFO, "Image", "4K pre-scaling is off; using original image size " +
                        std::to_string(surface->w) + "x" + std::to_string(surface->h));
                }

                if (scaled) {
                    SDL_FreeSurface(surface);
                    surface = scaled;
                }
                else if (preScaleQuality != "off") {
                    Logger::write(Logger::ZONE_WARNING, "Image", "Failed to scale 4K image: " + std::string(SDL_GetError()));
                }
            }

            surface = prepareAntiAliasedSurface(surface, baseViewInfo.Monitor);

            SDL_LockMutex(SDL::getMutex());
            texture_ = SDL_CreateTextureFromSurface(SDL::getRenderer(baseViewInfo.Monitor), surface);
            if (texture_) {
                SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_BLEND);
                SDL_QueryTexture(texture_, nullptr, nullptr, &width, &height);
                baseViewInfo.ImageWidth = static_cast<float>(width);
                baseViewInfo.ImageHeight = static_cast<float>(height);
#ifdef _DEBUG
                Logger::write(Logger::ZONE_DEBUG, "Image", "Set ImageWidth=" + std::to_string(baseViewInfo.ImageWidth) +
                    ", ImageHeight=" + std::to_string(baseViewInfo.ImageHeight));
#endif
            }
            else {
#ifdef _DEBUG
                Logger::write(Logger::ZONE_ERROR, "Image", "Texture creation failed: " + std::string(SDL_GetError()));
#endif
            }
            SDL_UnlockMutex(SDL::getMutex());
            SDL_FreeSurface(surface);
        }
        else {
            Logger::write(Logger::ZONE_ERROR, "Image", "Failed to load image: " + file_ + " or " + altFile_);
        }
    }
    Component::allocateGraphicsMemory();
}


void Image::draw() {
    // Only switch images if both random and slideShow are true
    if (random_ && slideShow_ && !imageList_.empty()) {
        Uint32 currentTime = SDL_GetTicks();
        if (currentTime - lastSwitchTime_ >= static_cast<Uint32>(slideShowTimer_)) {
            switchImage();
            lastSwitchTime_ = currentTime;
        }
    }

    Component::draw(); // Rely on Component::draw() for rendering
}



    

void Image::loadImageList() {
    if (randomSrc_.empty() && altRandomSrc_.empty()) return;
    imageList_.clear();
    std::string targetPath = randomSrc_;
    DIR* dir = opendir(targetPath.c_str());
    if (!dir && !altRandomSrc_.empty()) {
        targetPath = altRandomSrc_;
        dir = opendir(targetPath.c_str());
    }
    if (!dir) {
        Logger::write(Logger::ZONE_ERROR, "Image", "Failed to open directory: " + targetPath);
        return;
    }
    struct dirent* entry;
    while ((entry = readdir(dir))) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        std::string ext = name.substr(name.find_last_of(".") + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp" || ext == "jxl") {
            imageList_.push_back(targetPath + "/" + name);
            Logger::write(Logger::ZONE_DEBUG, "Image", "Added image to list: " + targetPath + "/" + name);
        }
    }
    closedir(dir);
    if (imageList_.empty()) {
        Logger::write(Logger::ZONE_WARNING, "Image", "No valid images found in directory: " + targetPath);
        return;
    }
    if (imageList_.size() > static_cast<size_t>(slideNumber_)) {
        imageList_.resize(slideNumber_);
    }
    if (random_ && imageList_.size() == 1) {
        Logger::write(Logger::ZONE_WARNING, "Image", "Only one image loaded with random=true; no switching will occur");
    }
    if (random_ && !imageList_.empty()) {
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(imageList_.begin(), imageList_.end(), g);
        Logger::write(Logger::ZONE_INFO, "Image", "Shuffled " + std::to_string(imageList_.size()) + " images from " + targetPath);
    }
}

void Image::switchImage() {
    if (imageList_.empty()) return;
    freeGraphicsMemory();
    file_ = imageList_[currentImageIndex_];
    altFile_.clear();
    allocateGraphicsMemory();

    // Store the previous index to avoid repeating it in random mode
    size_t previousIndex = currentImageIndex_;

    if (slideShow_) {
        currentImageIndex_ = (currentImageIndex_ + 1) % imageList_.size();
    }
    else if (random_ && imageList_.size() > 1) { // Only randomize if more than one image
        std::random_device rd;
        std::mt19937 g(rd());
        std::vector<size_t> validIndices;
        for (size_t i = 0; i < imageList_.size(); ++i) {
            if (i != previousIndex) {
                validIndices.push_back(i);
            }
        }
        // Select a random index from validIndices
        std::uniform_int_distribution<size_t> dist(0, validIndices.size() - 1);
        currentImageIndex_ = validIndices[dist(g)];
    }
    else if (random_) {
        // If only one image, keep the same index
        currentImageIndex_ = previousIndex;
    }

    Logger::write(Logger::ZONE_INFO, "Image", "Switched to image: " + file_);
}