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
#include "../ViewInfo.h"
#include "../../SDL.h"
#include "../../Utility/Log.h"
#include <SDL_image.h>
#include <SDL2_rotozoom.h>
#include "../../Database/Configuration.h"
#include <random>
#include <filesystem>
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


void Image::allocateGraphicsMemory() {
    int width, height;
    if (!texture_) {
        SDL_Surface* surface = IMG_Load(file_.c_str());
        if (!surface && !altFile_.empty()) surface = IMG_Load(altFile_.c_str());

        if (surface) {
            int targetWidth = static_cast<int>(baseViewInfo.Width * SDL::getWindowWidth(baseViewInfo.Monitor) / page.getLayoutWidth(baseViewInfo.Monitor));
            int targetHeight = static_cast<int>(baseViewInfo.Height * SDL::getWindowHeight(baseViewInfo.Monitor) / page.getLayoutHeight(baseViewInfo.Monitor));

            // Check if the image is 4K and needs scaling
            if ((surface->w >= 3840 || surface->h >= 2160) && (surface->w > targetWidth || surface->h > targetHeight)) {
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
                    scaled = SDL_CreateRGBSurface(0, targetWidth, targetHeight, 32, 0, 0, 0, 0);
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
                Logger::write(Logger::ZONE_ERROR, "Image", "Texture creation failed: " + std::string(SDL_GetError()));
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
        if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp") {
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