#include "AnimatedImage.h"
#include "SDL.h"
#include "../ViewInfo.h"
#include "../../Utility/Log.h"
#include "../../Collection/Item.h"
#include <random>
#include <dirent.h>
#include <algorithm>

std::unordered_map<std::string, std::pair<IMG_Animation*, int>> AnimatedImage::animationCache;

AnimatedImage::AnimatedImage(std::string file, std::string altFile, Page& p, int monitor, bool isAlwaysAnimated, std::string frameLoopType, int speed,
    bool random, bool slideShow, int slideShowTimer, int slideNumber, std::string randomSrc, std::string altRandomSrc)
    : Component(p),
    anim_(nullptr),
    texture_(nullptr),
    file_(file),
    altFile_(altFile),
    isPlaying_(false),
    startTime_(0),
    currentFrame_(0),
    frameCount_(0),
    speed_(speed),
    once_(30),
    isAlwaysAnimated_(isAlwaysAnimated),
    finishedLoopOnce_(false),
    frameLoopType_(frameLoopType),
    random_(random),
    slideShow_(slideShow),
    slideShowTimer_(slideShowTimer),
    slideNumber_(slideNumber),
    randomSrc_(randomSrc),
    altRandomSrc_(altRandomSrc),
    currentImageIndex_(0),
    lastSwitchTime_(0),
    lastMenuIndex_(-1),
    useOriginalDelays_(speed <= 0),
    nextFrameTime_(0),
    isAnimationReady_(false) {
    Uint32 startTime = SDL_GetTicks();
    baseViewInfo.Monitor = monitor;

    if (random_ && !randomSrc_.empty()) {
        loadImageList();
        if (!imageList_.empty()) {
            file_ = imageList_[currentImageIndex_];
#ifdef _DEBUG
            Logger::write(Logger::ZONE_INFO, "AnimatedImage", "Initial random animated image set to: " + file_);
#endif
        }
        else {
#ifdef _DEBUG
            Logger::write(Logger::ZONE_WARNING, "AnimatedImage", "No animated images found in randomSrc: " + randomSrc_);
#endif
        }
    }

    allocateGraphicsMemory();
    Uint32 endTime = SDL_GetTicks();
#ifdef _DEBUG
    Logger::write(Logger::ZONE_INFO, "Profiling", "AnimatedImage constructor took " + std::to_string(endTime - startTime) + " ms");
#endif
}

AnimatedImage::~AnimatedImage() {
    Uint32 startTime = SDL_GetTicks();
    freeGraphicsMemory();
    Uint32 endTime = SDL_GetTicks();
#ifdef _DEBUG
    Logger::write(Logger::ZONE_INFO, "Profiling", "AnimatedImage destructor took " + std::to_string(endTime - startTime) + " ms");
#endif
}

void AnimatedImage::freeGraphicsMemory() {
    Uint32 startTime = SDL_GetTicks();
    if (texture_) {
        SDL_LockMutex(SDL::getMutex());
        for (int i = 0; i < frameCount_; ++i) {
            if (texture_[i]) {
                SDL_DestroyTexture(texture_[i]);
                texture_[i] = nullptr;
            }
        }
        SDL_UnlockMutex(SDL::getMutex());
        SDL_free(texture_);
        texture_ = nullptr;
    }

    if (anim_) {
        std::string key = (file_ == altFile_ || altFile_.empty()) ? file_ : altFile_;
        auto it = animationCache.find(key);
        if (it != animationCache.end()) {
            if (--it->second.second == 0) {
                IMG_FreeAnimation(it->second.first);
                animationCache.erase(it);
            }
        }
        anim_ = nullptr;
    }
    Component::freeGraphicsMemory();
    Uint32 endTime = SDL_GetTicks();
#ifdef _DEBUG
    Logger::write(Logger::ZONE_INFO, "Profiling", "freeGraphicsMemory took " + std::to_string(endTime - startTime) + " ms");
#endif
}

void AnimatedImage::allocateGraphicsMemory() {
    Uint32 startTime = SDL_GetTicks();
    if (texture_) return;

    auto it = animationCache.find(file_);
    if (it != animationCache.end()) {
        anim_ = it->second.first;
        it->second.second++;
        Logger::write(Logger::ZONE_INFO, "Profiling", "Cache hit for " + file_);
    }
    else {
        Logger::write(Logger::ZONE_INFO, "Profiling", "Cache miss for " + file_);
        anim_ = IMG_LoadAnimation(file_.c_str());
        if (!anim_) {
#ifdef _DEBUG
            Logger::write(Logger::ZONE_ERROR, "AnimatedImage", "Failed to load animation from " + file_ + ": " + IMG_GetError());
#endif
            if (!altFile_.empty()) {
                it = animationCache.find(altFile_);
                if (it != animationCache.end()) {
                    anim_ = it->second.first;
                    it->second.second++;
                    Logger::write(Logger::ZONE_INFO, "Profiling", "Cache hit for alternate " + altFile_);
                }
                else {
                    Logger::write(Logger::ZONE_INFO, "Profiling", "Cache miss for alternate " + altFile_);
                    anim_ = IMG_LoadAnimation(altFile_.c_str());
                    if (!anim_) {
#ifdef _DEBUG
                        Logger::write(Logger::ZONE_ERROR, "AnimatedImage", "Failed to load alternate animation from " + altFile_ + ": " + IMG_GetError());
#endif
                        return;
                    }
                    else {
                        animationCache[altFile_] = { anim_, 1 };
                    }
                }
            }
            else {
                return;
            }
        }
        else {
            animationCache[file_] = { anim_, 1 };
        }
    }

    frameCount_ = anim_->count;
    texture_ = static_cast<SDL_Texture**>(SDL_calloc(frameCount_, sizeof(SDL_Texture*)));
    if (!texture_) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_ERROR, "AnimatedImage", "Failed to allocate memory for textures: " + std::string(SDL_GetError()));
#endif
        IMG_FreeAnimation(anim_);
        anim_ = nullptr;
        return;
    }

    SDL_LockMutex(SDL::getMutex());
    SDL_Renderer* renderer = SDL::getRenderer(baseViewInfo.Monitor);
    // Create only the first texture initially
    texture_[0] = SDL_CreateTextureFromSurface(renderer, anim_->frames[0]);
    if (!texture_[0]) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_ERROR, "AnimatedImage", "Failed to create texture for frame 0: " + std::string(SDL_GetError()));
#endif
    }
    else {
        SDL_SetTextureBlendMode(texture_[0], SDL_BLENDMODE_BLEND);
    }
    // Initialize remaining textures as nullptr
    for (int j = 1; j < frameCount_; ++j) {
        texture_[j] = nullptr;
    }
    SDL_UnlockMutex(SDL::getMutex());

    if (texture_[0]) {
        int w, h;
        SDL_QueryTexture(texture_[0], nullptr, nullptr, &w, &h);
        baseViewInfo.ImageWidth = static_cast<float>(w);
        baseViewInfo.ImageHeight = static_cast<float>(h);
    }

    if (!useOriginalDelays_) {
        setSpeed(frameCount_, speed_);
    }
    setPlaying();
    Component::allocateGraphicsMemory();
    isAnimationReady_ = false; // Animation not fully ready yet
    Uint32 endTime = SDL_GetTicks();
#ifdef _DEBUG
    Logger::write(Logger::ZONE_INFO, "Profiling", "allocateGraphicsMemory took " + std::to_string(endTime - startTime) + " ms");
#endif
}

void AnimatedImage::draw() {
    Uint32 startTime = SDL_GetTicks();
    Component::draw();
    if (!texture_ || !anim_) return;

    if (random_ && slideShow_ && !imageList_.empty()) {
        Uint32 currentTime = SDL_GetTicks();
        if (currentTime - lastSwitchTime_ >= static_cast<Uint32>(slideShowTimer_)) {
#ifdef _DEBUG
            Logger::write(Logger::ZONE_INFO, "AnimatedImage", "Switching to image: " + imageList_[currentImageIndex_]);
#endif
            switchImage();
            lastSwitchTime_ = currentTime;
        }
    }

    int origW, origH;
    SDL_QueryTexture(texture_[0], nullptr, nullptr, &origW, &origH);
    float imgW = static_cast<float>(origW);
    float imgH = static_cast<float>(origH);
#ifdef _DEBUG
    Logger::write(Logger::ZONE_DEBUG, "AnimatedImage", "Original size: " + std::to_string(imgW) + "x" + std::to_string(imgH));
#endif

    float contW = baseViewInfo.ScaledWidth();
    float contH = baseViewInfo.ScaledHeight();
#ifdef _DEBUG
    Logger::write(Logger::ZONE_DEBUG, "AnimatedImage", "Container size: " + std::to_string(contW) + "x" + std::to_string(contH));
#endif

    float renderW, renderH;
    SDL_Rect srcRect = { 0, 0, origW, origH };
    SDL_Rect* pSrcRect = nullptr;

    switch (baseViewInfo.scaleMode) {
    case ViewInfo::ScaleMode::Stretch:
        renderW = contW;
        renderH = contH;
        break;
    case ViewInfo::ScaleMode::Fit: {
        float imageAspect = imgW / imgH;
        float containerAspect = contW / contH;
        if (imageAspect > containerAspect) {
            renderW = contW;
            renderH = contW / imageAspect;
        }
        else {
            renderH = contH;
            renderW = contH * imageAspect;
        }
#ifdef _DEBUG
        Logger::write(Logger::ZONE_DEBUG, "AnimatedImage", "Fit mode render size: " + std::to_string(renderW) + "x" + std::to_string(renderH));
#endif
        break;
    }
    case ViewInfo::ScaleMode::Fill: {
        float imageAspect = imgW / imgH;
        float containerAspect = contW / contH;
        float scale;
        if (imageAspect < containerAspect) {
            renderW = contW;
            renderH = contW / imageAspect;
            scale = renderW / imgW;
        }
        else {
            renderH = contH;
            renderW = contH * imageAspect;
            scale = renderH / imgH;
        }
        float srcW = contW / scale;
        float srcH = contH / scale;
        srcRect.x = static_cast<int>((imgW - srcW) / 2);
        srcRect.y = static_cast<int>((imgH - srcH) / 2);
        srcRect.w = static_cast<int>(srcW);
        srcRect.h = static_cast<int>(srcH);
        renderW = contW;
        renderH = contH;
        pSrcRect = &srcRect;
#ifdef _DEBUG
        Logger::write(Logger::ZONE_DEBUG, "AnimatedImage", "Fill mode render size: " + std::to_string(renderW) + "x" + std::to_string(renderH));
#endif
        break;
    }
    case ViewInfo::ScaleMode::None:
        renderW = imgW;
        renderH = imgH;
        break;
    }

    float x = baseViewInfo.X + baseViewInfo.XOffset - baseViewInfo.XOrigin * renderW;
    float y = baseViewInfo.Y + baseViewInfo.YOffset - baseViewInfo.YOrigin * renderH;

    SDL_Rect destRect = { static_cast<int>(x), static_cast<int>(y), static_cast<int>(renderW), static_cast<int>(renderH) };

#ifdef _DEBUG
    Logger::write(Logger::ZONE_DEBUG, "AnimatedImage", "Final render size: " + std::to_string(destRect.w) + "x" + std::to_string(destRect.h));
#endif

    // Progressively create textures if not all are ready
    if (!isAnimationReady_) {
        SDL_LockMutex(SDL::getMutex());
        SDL_Renderer* renderer = SDL::getRenderer(baseViewInfo.Monitor);
        for (int j = 1; j < frameCount_; ++j) {
            if (texture_[j] == nullptr) {
                texture_[j] = SDL_CreateTextureFromSurface(renderer, anim_->frames[j]);
                if (texture_[j]) {
                    SDL_SetTextureBlendMode(texture_[j], SDL_BLENDMODE_BLEND);
                }
                break; // Create one texture per draw call
            }
        }
        SDL_UnlockMutex(SDL::getMutex());

        // Check if all textures are created
        bool allCreated = true;
        for (int j = 0; j < frameCount_; ++j) {
            if (texture_[j] == nullptr) {
                allCreated = false;
                break;
            }
        }
        if (allCreated) {
            isAnimationReady_ = true;
        }
    }

    // Update animation frame
    if (isAnimationReady_ && isPlaying_ && (!baseViewInfo.selectedOnlyAnimation || isSelected_)) {
        Uint32 currentTime = SDL_GetTicks();
        if (startTime_ == 0) startTime_ = currentTime;

        if (baseViewInfo.frameLoop == "noPlay") {
            currentFrame_ = 0;
        }
        else if (baseViewInfo.frameLoop == "playOnce" && finishedLoopOnce_) {
            currentFrame_ = frameCount_ - 1;
        }
        else if (baseViewInfo.frameLoop == "playInvertedOnce" && finishedLoopOnce_) {
            currentFrame_ = 0; // Stop at first frame for inverted playback
        }
        else {
            int frame = ((currentTime - startTime_) * once_ / baseViewInfo.speed);
            if (baseViewInfo.frameLoop == "playOnce") {
                currentFrame_ = static_cast<int>(std::fmin(frame, frameCount_ - 1));
                finishedLoopOnce_ = (currentFrame_ == frameCount_ - 1);
            }
            else if (baseViewInfo.frameLoop == "playInvertedOnce") {
                // Calculate frame in reverse
                currentFrame_ = frameCount_ - 1 - static_cast<int>(std::fmin(frame, frameCount_ - 1));
                finishedLoopOnce_ = (currentFrame_ == 0);
            }
            else if (baseViewInfo.frameLoop == "playLoop") {
                // Forward looping playback
                currentFrame_ = frame % frameCount_;
            }
            else if (baseViewInfo.frameLoop == "playInverted") {
                // Reverse looping playback
                currentFrame_ = frameCount_ - 1 - (frame % frameCount_);
            }
        }
    }
    else {
        currentFrame_ = 0; // Display first frame until animation is ready
    }

    // Render the current frame
    SDL::renderCopy(texture_[currentFrame_], baseViewInfo.Alpha, pSrcRect, &destRect, baseViewInfo, page.getLayoutWidth(baseViewInfo.Monitor), page.getLayoutHeight(baseViewInfo.Monitor));

    Uint32 endTime = SDL_GetTicks();
    Uint32 duration = endTime - startTime;
    if (duration > 16) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_WARNING, "Profiling", "Slow draw: " + std::to_string(duration) + " ms");
#endif
    }
    else {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_INFO, "Profiling", "draw took " + std::to_string(duration) + " ms");
#endif
    }
}

void AnimatedImage::triggerEvent(std::string event, int menuIndex) {
    Uint32 startTime = SDL_GetTicks();
    Component::triggerEvent(event, menuIndex);

    if (menuIndex != lastMenuIndex_) {
        if (frameLoopType_ == "playOnce" || frameLoopType_ == "playInvertedOnce") {
            currentFrame_ = 0;
            startTime_ = 0;
            finishedLoopOnce_ = false;
            isPlaying_ = true;
#ifdef _DEBUG
            Logger::write(Logger::ZONE_INFO, "AnimatedImage", "Reset animation due to menuIndex change from " + std::to_string(lastMenuIndex_) + " to " + std::to_string(menuIndex));
#endif
        }
        lastMenuIndex_ = menuIndex;
    }
    Uint32 endTime = SDL_GetTicks();
#ifdef _DEBUG
    Logger::write(Logger::ZONE_INFO, "Profiling", "triggerEvent took " + std::to_string(endTime - startTime) + " ms");
#endif
}

void AnimatedImage::setPlaying() {
    Uint32 startTime = SDL_GetTicks();
    isPlaying_ = true;
    startTime_ = 0;
    currentFrame_ = 0;
    finishedLoopOnce_ = false;
    if (useOriginalDelays_ && !playbackDelays_.empty()) {
        nextFrameTime_ = SDL_GetTicks() + playbackDelays_[0];
    }
    Uint32 endTime = SDL_GetTicks();
#ifdef _DEBUG
    Logger::write(Logger::ZONE_INFO, "Profiling", "setPlaying took " + std::to_string(endTime - startTime) + " ms");
#endif
}

void AnimatedImage::setSpeed(int frameCount, int speed) {
    Uint32 startTime = SDL_GetTicks();
    frameCount_ = frameCount;
    speed_ = (speed > 0) ? speed : 7500;
    once_ = 60;
    Uint32 endTime = SDL_GetTicks();
#ifdef _DEBUG
    Logger::write(Logger::ZONE_INFO, "Profiling", "setSpeed took " + std::to_string(endTime - startTime) + " ms");
#endif
}

void AnimatedImage::loadImageList() {
    if (randomSrc_.empty() && altRandomSrc_.empty()) return;
    imageList_.clear();
    std::string targetPath = randomSrc_;
    DIR* dir = opendir(targetPath.c_str());
    if (!dir && !altRandomSrc_.empty()) {
        targetPath = altRandomSrc_;
        dir = opendir(targetPath.c_str());
    }
    if (!dir) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_ERROR, "AnimatedImage", "Failed to open directory: " + targetPath);
#endif
        return;
    }
    struct dirent* entry;
    while ((entry = readdir(dir))) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        std::string ext = name.substr(name.find_last_of(".") + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == "gif" || ext == "webp") {
            imageList_.push_back(targetPath + "/" + name);
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "AnimatedImage", "Added image to list: " + targetPath + "/" + name);
#endif
        }
    }
    closedir(dir);
    if (imageList_.empty()) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_WARNING, "AnimatedImage", "No valid images found in directory: " + targetPath);
#endif
        return;
    }
    if (imageList_.size() > static_cast<size_t>(slideNumber_)) {
        imageList_.resize(slideNumber_);
    }
    if (random_ && imageList_.size() == 1) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_WARNING, "AnimatedImage", "Only one image loaded with random=true; no switching will occur");
#endif
    }
    if (random_ && !imageList_.empty()) {
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(imageList_.begin(), imageList_.end(), g);
#ifdef _DEBUG
        Logger::write(Logger::ZONE_INFO, "AnimatedImage", "Shuffled " + std::to_string(imageList_.size()) + " images from " + targetPath);
#endif
    }
}

void AnimatedImage::switchImage() {
    if (imageList_.empty()) return;
    freeGraphicsMemory();
    file_ = imageList_[currentImageIndex_];
    altFile_.clear();
    allocateGraphicsMemory();

    size_t previousIndex = currentImageIndex_;
    if (slideShow_) {
        currentImageIndex_ = (currentImageIndex_ + 1) % imageList_.size();
    }
    else if (random_ && imageList_.size() > 1) {
        std::random_device rd;
        std::mt19937 g(rd());
        std::vector<size_t> validIndices;
        for (size_t i = 0; i < imageList_.size(); ++i) {
            if (i != previousIndex) {
                validIndices.push_back(i);
            }
        }
        std::uniform_int_distribution<size_t> dist(0, validIndices.size() - 1);
        currentImageIndex_ = validIndices[dist(g)];
    }
    else if (random_) {
        currentImageIndex_ = previousIndex;
    }
#ifdef _DEBUG
    Logger::write(Logger::ZONE_INFO, "AnimatedImage", "Switched to image: " + file_);
#endif
}