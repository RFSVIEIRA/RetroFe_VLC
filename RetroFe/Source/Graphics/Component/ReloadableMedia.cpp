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

#include "ReloadableMedia.h"
#include "ImageBuilder.h"
#include "VideoBuilder.h"
#include "ReloadableText.h"
#include "../ViewInfo.h"
#include "../../Video/VideoFactory.h"
#include "../../Database/Configuration.h"
#include "../../Utility/Log.h"
#include "../../Utility/Utils.h"
#include "../../SDL.h"
#include <fstream>
#include <vector>
#include <iostream>
#include<algorithm>
#include <random>
#include <unordered_map>
#include <unordered_set>

Item* ReloadableMedia::currentSelectedItem = nullptr;
std::string ReloadableMedia::currentSelectedSystem = "";
std::string ReloadableMedia::currentSelectedGame = "";
bool ReloadableMedia::randomSyncMedia = false;

std::vector<std::pair<std::string, std::string>> ReloadableMedia::sharedSlideCandidates_;
int ReloadableMedia::sharedCurrentSlideIndex_ = 0;
Uint32 ReloadableMedia::sharedLastSlideChangeTime_ = 0;
bool ReloadableMedia::randomSyncSlide = false;
int lastKnownSlideIndex_ = -1;
bool ReloadableMedia::isSlideshowInitialized = false;


ReloadableMedia::ReloadableMedia(Configuration& config, bool systemMode, bool layoutMode, bool commonMode, bool menuMode, std::string type, std::string imageType, Page& p, int displayOffset, bool isVideo, Font* font, bool jukebox, int jukeboxNumLoops, 
                                                        bool isAlwaysAnimated, std::string frameLoop, int speed, bool random, bool slideShow, Uint32 slideShowTimer)
    : Component(p)
    , config_(config)
    , systemMode_(systemMode)
    , layoutMode_(layoutMode)
    , commonMode_(commonMode)
    , menuMode_(menuMode)
    , loadedComponent_(nullptr)
    , videoInst_(nullptr)
    , isVideo_(isVideo)
    , FfntInst_(font)
    , textFallback_(false)
    , type_(type)
    , displayOffset_(displayOffset)
    , imageType_(imageType)
    , jukebox_(jukebox)
    , jukeboxNumLoops_(jukeboxNumLoops)
    , isAlwaysAnimated_(isAlwaysAnimated)
    , frameLoop_(frameLoop)
    , speed_(speed)
    , random_(random)
    , slideshow_(slideShow)
    , slideShowTimer_(slideShowTimer)
    , currentSlideIndex_(0)
    , elapsedTime_(0.0f)
    , lastUpdateTime_(SDL_GetTicks())
    , lastKnownSlideIndex_(sharedCurrentSlideIndex_)
{
    config_.getProperty("randomSyncMedia", randomSyncMedia);
    config_.getProperty("randomSyncSlide", randomSyncSlide);
    allocateGraphicsMemory();
}

ReloadableMedia::~ReloadableMedia() {
    if (loadedComponent_ != nullptr) {
        delete loadedComponent_;
        loadedComponent_ = nullptr;
    }
    if (loadedCounterpart_ != nullptr) {
        delete loadedCounterpart_;
        loadedCounterpart_ = nullptr;
    }
}

void ReloadableMedia::enableTextFallback_(bool value)
{
    textFallback_ = value;
}

void ReloadableMedia::update(float dt)
{
    Uint32 currentTime = SDL_GetTicks();
    Uint32 timeSinceLastUpdate = currentTime - lastUpdateTime_;
    elapsedTime_ += static_cast<float>(timeSinceLastUpdate);
    lastUpdateTime_ = currentTime;

    if (newItemSelected || (newScrollItemSelected && getMenuScrollReload()) || type_ == "isPaused")
    {
        reloadTexture();
        newItemSelected = false;
        newScrollItemSelected = false;
    }

    if (loadedComponent_)
    {
        loadedComponent_->update(dt);
    }

    // Get the currently selected item to check if it's a game
    Item* selectedItem = page.getSelectedItem(displayOffset_);
    if (selectedItem && random_ && slideshow_ && !selectedItem->leaf) // Only run slideshow for collections
    {
        if (randomSyncSlide && !sharedSlideCandidates_.empty())
        {
            bool shouldReload = false;
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "ReloadableMedia",
                "Sync slideshow: currentTime=" + std::to_string(currentTime) +
                ", lastChange=" + std::to_string(sharedLastSlideChangeTime_) +
                ", diff=" + std::to_string(currentTime - sharedLastSlideChangeTime_) +
                ", timer=" + std::to_string(slideShowTimer_) +
                ", index=" + std::to_string(sharedCurrentSlideIndex_) +
                ", lastKnown=" + std::to_string(lastKnownSlideIndex_) +
                ", isVideo=" + std::to_string(isVideo_));
#endif
            if (currentTime - sharedLastSlideChangeTime_ >= slideShowTimer_)
            {
                sharedCurrentSlideIndex_ = (sharedCurrentSlideIndex_ + 1) % sharedSlideCandidates_.size();
                sharedLastSlideChangeTime_ = currentTime;
                shouldReload = true;
            }
            else if (sharedCurrentSlideIndex_ != lastKnownSlideIndex_)
            {
                shouldReload = true;
            }

            if (shouldReload)
            {
                if (loadedComponent_)
                {
                    delete loadedComponent_;
                    loadedComponent_ = nullptr;
                }
                auto [system, game] = sharedSlideCandidates_[sharedCurrentSlideIndex_];
                loadedComponent_ = findComponent(system, type_, game, "", false, isVideo_);
                if (loadedComponent_)
                {
                    loadedComponent_->allocateGraphicsMemory();
                    baseViewInfo.ImageWidth = loadedComponent_->baseViewInfo.ImageWidth;
                    baseViewInfo.ImageHeight = loadedComponent_->baseViewInfo.ImageHeight;
#ifdef _DEBUG
                    Logger::write(Logger::ZONE_INFO, "ReloadableMedia",
                        "Synchronized slideshow switched to slide " + std::to_string(sharedCurrentSlideIndex_) +
                        ": " + system + "/" + game + " (isVideo=" + std::to_string(isVideo_) + ")");
#endif
                }
                lastKnownSlideIndex_ = sharedCurrentSlideIndex_;
            }
        }
        else if (!slideCandidates_.empty())
        {
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "ReloadableMedia",
                "Unsync slideshow: elapsedTime=" + std::to_string(elapsedTime_) +
                ", timer=" + std::to_string(slideShowTimer_) +
                ", index=" + std::to_string(currentSlideIndex_) +
                ", size=" + std::to_string(slideCandidates_.size()) +
                ", isVideo=" + std::to_string(isVideo_));
#endif
            if (elapsedTime_ >= slideShowTimer_)
            {
                currentSlideIndex_ = (currentSlideIndex_ + 1) % slideCandidates_.size();
                if (loadedComponent_)
                {
                    delete loadedComponent_;
                    loadedComponent_ = nullptr;
                }
                auto [system, game] = slideCandidates_[currentSlideIndex_];
                loadedComponent_ = findComponent(system, type_, game, "", false, isVideo_);
                if (loadedComponent_)
                {
                    loadedComponent_->allocateGraphicsMemory();
                    baseViewInfo.ImageWidth = loadedComponent_->baseViewInfo.ImageWidth;
                    baseViewInfo.ImageHeight = loadedComponent_->baseViewInfo.ImageHeight;
#ifdef _DEBUG
                    Logger::write(Logger::ZONE_INFO, "ReloadableMedia",
                        "Unsynced slideshow switched to slide " + std::to_string(currentSlideIndex_) +
                        ": " + system + "/" + game + " (isVideo=" + std::to_string(isVideo_) + ")");
#endif
                }
                elapsedTime_ = 0.0f;
            }
        }
    }

    Component::update(dt);
}

void ReloadableMedia::allocateGraphicsMemory()
{
    if(loadedComponent_)
    {
        loadedComponent_->allocateGraphicsMemory();
    }

    // NOTICE! needs to be done last to prevent flags from being missed
    Component::allocateGraphicsMemory();
}


void ReloadableMedia::freeGraphicsMemory()
{
    Component::freeGraphicsMemory();

    if(loadedComponent_)
    {
        loadedComponent_->freeGraphicsMemory();
    }
}


void ReloadableMedia::reloadTexture()
{
    if (loadedComponent_)
    {
        delete loadedComponent_;
        loadedComponent_ = nullptr;
    }
    Item* selectedItem = page.getSelectedItem(displayOffset_);
    if (!selectedItem)
    {
        Logger::write(Logger::ZONE_WARNING, "ReloadableMedia", "No selected item");
        return;
    }

    std::string collectionName = selectedItem->collectionInfo ? selectedItem->collectionInfo->name : "";
    if (collectionName.empty())
    {
        config_.getProperty("currentCollection", collectionName);
    }
#ifdef _DEBUG
    Logger::write(Logger::ZONE_DEBUG, "ReloadableMedia", "Selected item collection: " + collectionName);
#endif

    std::vector<std::string> names;
    names.push_back(selectedItem->name);
    names.push_back(selectedItem->fullTitle);
    if (selectedItem->cloneof.length() > 0)
    {
        names.push_back(selectedItem->cloneof);
    }
    std::string typeLC = Utils::toLower(type_);
    if (typeLC == "isfavorite")
    {
        names.push_back(selectedItem->isFavorite ? "yes" : "no");
    }
    if (typeLC == "ispaused")
    {
        names.push_back(page.isPaused() ? "yes" : "no");
    }
    names.push_back("default");

    // Prioritize system mode loading
    if (systemMode_)
    {
        loadedComponent_ = findComponent(collectionName, type_, type_, "", true, isVideo_);
        if (!loadedComponent_ && selectedItem->collectionInfo)
        {
            loadedComponent_ = findComponent(selectedItem->collectionInfo->name, type_, type_, "", true, isVideo_);
        }
        if (!loadedComponent_)
        {
            loadedComponent_ = findComponent(collectionName, type_, type_, "", true, false);
            if (!loadedComponent_ && selectedItem->collectionInfo)
            {
                loadedComponent_ = findComponent(selectedItem->collectionInfo->name, type_, type_, "", true, false);
            }
        }
        if (loadedComponent_)
        {
            loadedComponent_->allocateGraphicsMemory();
            baseViewInfo.ImageWidth = loadedComponent_->baseViewInfo.ImageWidth;
            baseViewInfo.ImageHeight = loadedComponent_->baseViewInfo.ImageHeight;
#ifdef _DEBUG
            Logger::write(Logger::ZONE_INFO, "ReloadableMedia",
                "Loaded system logo for collection: " + collectionName + " (isVideo=" + std::to_string(isVideo_) + ")");
#endif
            return; // Exit after loading system logo
        }
    }

    // Existing logic for non-system mode
    if (random_ && !selectedItem->leaf)
    {
        std::string collectionPath = Utils::combinePath(Configuration::absolutePath, "collections", selectedItem->name);
        int maxSubCollections = 5;
        int maxRandomMedia = 100;
        config_.getProperty("maxsubcollections", maxSubCollections);
        config_.getProperty("maxRandomMedia", maxRandomMedia);
        if (maxRandomMedia < 1) maxRandomMedia = 1;

        if (slideshow_)
        {
            if (randomSyncSlide)
            {
                if (sharedSlideCandidates_.empty() || selectedItem != currentSelectedItem)
                {
                    buildSharedSlideshowCandidates(collectionPath, maxSubCollections, maxRandomMedia, selectedItem);
                    sharedCurrentSlideIndex_ = 0;
                    sharedLastSlideChangeTime_ = SDL_GetTicks();
                    currentSelectedItem = selectedItem;
                }
                if (!sharedSlideCandidates_.empty())
                {
                    auto [system, game] = sharedSlideCandidates_[sharedCurrentSlideIndex_];
                    loadedComponent_ = findComponent(system, type_, game, "", false, isVideo_);
                    if (loadedComponent_)
                    {
                        loadedComponent_->allocateGraphicsMemory();
                        lastKnownSlideIndex_ = sharedCurrentSlideIndex_;
#ifdef _DEBUG
                        Logger::write(Logger::ZONE_INFO, "ReloadableMedia",
                            "Initial sync slideshow: " + system + "/" + game +
                            " (index=" + std::to_string(sharedCurrentSlideIndex_) + ", isVideo=" + std::to_string(isVideo_) + ")");
#endif
                    }
                }
            }
            else
            {
                buildSlideshowCandidates(collectionPath, maxSubCollections, maxRandomMedia, selectedItem);
                currentSlideIndex_ = 0;
                elapsedTime_ = 0.0f;
                lastUpdateTime_ = SDL_GetTicks();
                if (!slideCandidates_.empty())
                {
                    auto [system, game] = slideCandidates_[currentSlideIndex_];
                    loadedComponent_ = findComponent(system, type_, game, "", false, isVideo_);
                    if (loadedComponent_)
                    {
                        loadedComponent_->allocateGraphicsMemory();
#ifdef _DEBUG
                        Logger::write(Logger::ZONE_INFO, "ReloadableMedia",
                            "Initial unsync slideshow: " + system + "/" + game +
                            " (index=" + std::to_string(currentSlideIndex_) + ", isVideo=" + std::to_string(isVideo_) + ")");
#endif
                    }
                }
            }
        }
        else
        {
            if (randomSyncMedia && selectedItem != currentSelectedItem)
            {
                buildSynchronizedCandidates(collectionPath, maxSubCollections, maxRandomMedia, selectedItem);
                if (!currentSelectedGame.empty())
                {
                    loadedComponent_ = findComponent(currentSelectedSystem, type_, currentSelectedGame, "", false, isVideo_);
                    if (loadedComponent_)
                    {
                        loadedComponent_->allocateGraphicsMemory();
#ifdef _DEBUG
                        Logger::write(Logger::ZONE_INFO, "ReloadableMedia",
                            "Sync random: " + currentSelectedSystem + "/" + currentSelectedGame +
                            " (isVideo=" + std::to_string(isVideo_) + ")");
#endif
                    }
                }
            }
            else if (!randomSyncMedia)
            {
                buildUnsyncedRandomCandidate(collectionPath, maxSubCollections, maxRandomMedia, selectedItem);
            }
            else if (randomSyncMedia && !currentSelectedGame.empty())
            {
                loadedComponent_ = findComponent(currentSelectedSystem, type_, currentSelectedGame, "", false, isVideo_);
                if (loadedComponent_)
                {
                    loadedComponent_->allocateGraphicsMemory();
#ifdef _DEBUG
                    Logger::write(Logger::ZONE_INFO, "ReloadableMedia",
                        "Sync random reload: " + currentSelectedSystem + "/" + currentSelectedGame +
                        " (isVideo=" + std::to_string(isVideo_) + ")");
#endif
                }
            }
        }
    }
    else if (selectedItem->leaf)
    {
        for (unsigned int n = 0; n < names.size() && !loadedComponent_; ++n)
        {
            std::string basename = names[n];
            loadedComponent_ = findComponent(collectionName, type_, basename, "", false, isVideo_);
            if (!loadedComponent_ && selectedItem->collectionInfo)
            {
                loadedComponent_ = findComponent(selectedItem->collectionInfo->name, type_, basename, "", false, isVideo_);
            }
            if (!loadedComponent_)
            {
                loadedComponent_ = findComponent(collectionName, type_, "default", "", false, isVideo_);
            }
            if (loadedComponent_)
            {
                loadedComponent_->allocateGraphicsMemory();
                baseViewInfo.ImageWidth = loadedComponent_->baseViewInfo.ImageWidth;
                baseViewInfo.ImageHeight = loadedComponent_->baseViewInfo.ImageHeight;
#ifdef _DEBUG
                Logger::write(Logger::ZONE_INFO, "ReloadableMedia",
                    "Loaded media for game: " + basename + " (isVideo=" + std::to_string(isVideo_) + ")");
#endif
            }
        }
    }

    if (!loadedComponent_)
    {
        if (isVideo_)
        {
            for (unsigned int n = 0; n < names.size() && !loadedComponent_; ++n)
            {
                std::string basename = names[n];
                if (selectedItem->leaf)
                {
                    loadedComponent_ = findComponent(collectionName, type_, basename, "", false, true);
                    if (!loadedComponent_)
                    {
                        loadedComponent_ = findComponent(selectedItem->collectionInfo->name, type_, basename, "", false, true);
                    }
                    if (!loadedComponent_)
                    {
                        loadedComponent_ = findComponent(selectedItem->collectionInfo->name, type_, type_, selectedItem->filepath, false, true);
                    }
                }
                else
                {
                    loadedComponent_ = findComponent(collectionName, type_, basename, "", false, true);
                    if (!loadedComponent_)
                    {
                        loadedComponent_ = findComponent(selectedItem->collectionInfo->name, type_, basename, "", false, true);
                    }
                    if (!loadedComponent_)
                    {
                        loadedComponent_ = findComponent(selectedItem->name, type_, type_, "", true, true);
                    }
                }
                if (loadedComponent_)
                {
                    loadedComponent_->allocateGraphicsMemory();
                    baseViewInfo.ImageWidth = loadedComponent_->baseViewInfo.ImageWidth;
                    baseViewInfo.ImageHeight = loadedComponent_->baseViewInfo.ImageHeight;
                }
            }
        }

        for (unsigned int n = 0; n < names.size() && !loadedComponent_; ++n)
        {
            std::string basename = names[n];
            bool defined = false;
            std::string type = type_;
            if (isVideo_)
            {
                typeLC = Utils::toLower(imageType_);
                type = imageType_;
            }
            if (basename == "default")
            {
                basename = "default";
                defined = true;
            }
            else if (typeLC == "numberbuttons")
            {
                basename = selectedItem->numberButtons;
                defined = true;
            }
            else if (typeLC == "numberplayers")
            {
                basename = selectedItem->numberPlayers;
                defined = true;
            }
            else if (typeLC == "year")
            {
                basename = selectedItem->year;
                defined = true;
            }
            else if (typeLC == "title")
            {
                basename = selectedItem->title;
                defined = true;
            }
            else if (typeLC == "developer")
            {
                basename = selectedItem->developer;
                defined = true;
                if (basename.empty())
                {
                    basename = selectedItem->manufacturer;
                }
            }
            else if (typeLC == "manufacturer")
            {
                basename = selectedItem->manufacturer;
                defined = true;
            }
            else if (typeLC == "genre")
            {
                basename = selectedItem->genre;
                defined = true;
            }
            else if (typeLC == "ctrltype")
            {
                basename = selectedItem->ctrlType;
                defined = true;
            }
            else if (typeLC == "joyways")
            {
                basename = selectedItem->joyWays;
                defined = true;
            }
            else if (typeLC == "rating")
            {
                basename = selectedItem->rating;
                defined = true;
            }
            else if (typeLC == "score")
            {
                basename = selectedItem->score;
                defined = true;
            }
            else if (typeLC.rfind("playlist", 0) == 0)
            {
                basename = page.getPlaylistName();
                defined = true;
            }
            else if (typeLC == "firstletter")
            {
                basename = selectedItem->fullTitle.at(0);
                defined = true;
            }
            if (!selectedItem->leaf)
            {
                (void)config_.getProperty("collections." + selectedItem->name + "." + type, basename);
            }
            bool overwriteXML = false;
            config_.getProperty("overwriteXML", overwriteXML);
            if (!defined || overwriteXML)
            {
                std::string basename_tmp;
                selectedItem->getInfo(type, basename_tmp);
                if (!basename_tmp.empty())
                {
                    basename = basename_tmp;
                }
            }
            Utils::replaceSlashesWithUnderscores(basename);
            if (selectedItem->leaf)
            {
                loadedComponent_ = findComponent(collectionName, type, basename, "", false, false);
                if (!loadedComponent_)
                {
                    loadedComponent_ = findComponent(selectedItem->collectionInfo->name, type, basename, "", false, false);
                }
                if (!loadedComponent_)
                {
                    loadedComponent_ = findComponent(selectedItem->collectionInfo->name, type, type, selectedItem->filepath, false, false);
                }
            }
            else
            {
                loadedComponent_ = findComponent(collectionName, type, basename, "", false, false);
                if (!loadedComponent_)
                {
                    loadedComponent_ = findComponent(selectedItem->collectionInfo->name, type, basename, "", false, false);
                }
                if (!loadedComponent_)
                {
                    loadedComponent_ = findComponent(selectedItem->name, type, type, "", true, false);
                }
            }
            if (loadedComponent_)
            {
                loadedComponent_->allocateGraphicsMemory();
                baseViewInfo.ImageWidth = loadedComponent_->baseViewInfo.ImageWidth;
                baseViewInfo.ImageHeight = loadedComponent_->baseViewInfo.ImageHeight;
            }
        }
    }

    if (!loadedComponent_ && textFallback_)
    {
        loadedComponent_ = new Text(selectedItem->fullTitle, page, FfntInst_, baseViewInfo.Monitor);
        baseViewInfo.ImageWidth = loadedComponent_->baseViewInfo.ImageWidth;
        baseViewInfo.ImageHeight = loadedComponent_->baseViewInfo.ImageHeight;
    }
}


Component* ReloadableMedia::findComponent(std::string collection, std::string type, std::string basename, std::string filepath, bool systemMode, bool isVideo)
{
    std::string imagePath;
    Component* component = nullptr;
    VideoBuilder videoBuild;
    ImageBuilder imageBuild;
    AnimatedImageBuilder animatedImageBuild;

    // Path construction logic
    if (layoutMode_)
    {
        std::string layoutName;
        config_.getProperty("layout", layoutName);
        if (commonMode_)
        {
            imagePath = Utils::combinePath(Configuration::absolutePath, "layouts", layoutName, "collections", "_common");
        }
        else
        {
            imagePath = Utils::combinePath(Configuration::absolutePath, "layouts", layoutName, "collections", collection);
        }
        if (systemMode)
            imagePath = Utils::combinePath(imagePath, "system_artwork");
        else
            imagePath = Utils::combinePath(imagePath, "medium_artwork", type);
    }
    else
    {
        if (commonMode_)
        {
            imagePath = Utils::combinePath(Configuration::absolutePath, "collections", "_common");
            if (systemMode)
                imagePath = Utils::combinePath(imagePath, "system_artwork");
            else
                imagePath = Utils::combinePath(imagePath, "medium_artwork", type);
        }
        else
        {
            config_.getMediaPropertyAbsolutePath(collection, type, systemMode, imagePath);
        }
    }
    if (filepath != "")
        imagePath = filepath;

    if (isVideo)
    {
        if (jukebox_)
            component = videoBuild.createVideo(imagePath, page, basename, baseViewInfo.Monitor, type == "video", jukeboxNumLoops_);
        else
            component = videoBuild.createVideo(imagePath, page, basename, baseViewInfo.Monitor, type == "video");
    }
    else
    {
        component = imageBuild.CreateImage(imagePath, page, basename, baseViewInfo.Monitor);
        if (!component)
        {
            component = animatedImageBuild.CreateImage(imagePath, page, basename, baseViewInfo.Monitor, isAlwaysAnimated_, frameLoop_, speed_);
        }
    }

    if (component)
    {
        // Copy only necessary ViewInfo properties, preserve native dimensions and scaleMode
        component->baseViewInfo.X = baseViewInfo.X;
        component->baseViewInfo.Y = baseViewInfo.Y;
        component->baseViewInfo.Width = baseViewInfo.Width;
        component->baseViewInfo.Height = baseViewInfo.Height;
        component->baseViewInfo.Angle = baseViewInfo.Angle;
        component->baseViewInfo.Monitor = baseViewInfo.Monitor;
#ifdef _DEBUG
        std::string componentType = typeid(*component).name();
        Logger::write(Logger::ZONE_INFO, "ReloadableMedia",
            "Created component: type=" + componentType + ", path=" + imagePath +
            ", isVideo=" + std::to_string(isVideo) +
            ", ImageWidth=" + std::to_string(component->baseViewInfo.ImageWidth) +
            ", ImageHeight=" + std::to_string(component->baseViewInfo.ImageHeight));
#endif
    }
    else
    {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_WARNING, "ReloadableMedia",
            "Failed to create component, isVideo=" + std::to_string(isVideo) + ", path=" + imagePath);
#endif
    }

    return component;
}


void ReloadableMedia::draw() {
    Component::draw();

    if (loadedComponent_) {
        baseViewInfo.ImageHeight = loadedComponent_->baseViewInfo.ImageHeight;
        baseViewInfo.ImageWidth = loadedComponent_->baseViewInfo.ImageWidth;
        loadedComponent_->baseViewInfo = baseViewInfo;
        loadedComponent_->draw();
    }
    if (loadedCounterpart_) {
        loadedCounterpart_->baseViewInfo = baseViewInfo; 
        loadedCounterpart_->draw();
    }
}


bool ReloadableMedia::isJukeboxPlaying()
{
    if ( jukebox_ && loadedComponent_ )
        return loadedComponent_->isPlaying();
    else
        return false;
}


void ReloadableMedia::skipForward( )
{
    if ( jukebox_ && loadedComponent_ )
        loadedComponent_->skipForward( );
}


void ReloadableMedia::skipBackward( )
{
    if ( jukebox_ && loadedComponent_ )
        loadedComponent_->skipBackward( );
}


void ReloadableMedia::skipForwardp( )
{
    if ( jukebox_ && loadedComponent_ )
        loadedComponent_->skipForwardp( );
}


void ReloadableMedia::skipBackwardp( )
{
    if ( jukebox_ && loadedComponent_ )
        loadedComponent_->skipBackwardp( );
}


void ReloadableMedia::pause( )
{
    if ( jukebox_ && loadedComponent_ )
        loadedComponent_->pause( );
}


void ReloadableMedia::restart( )
{
    if ( jukebox_ && loadedComponent_ )
        loadedComponent_->restart( );
}


unsigned long long ReloadableMedia::getCurrent( )
{
    if ( jukebox_ && loadedComponent_ )
        return loadedComponent_->getCurrent( );
    else
        return 0;
}


unsigned long long ReloadableMedia::getDuration( )
{
    if ( jukebox_ && loadedComponent_ )
        return loadedComponent_->getDuration( );
    else
        return 0;
}


bool ReloadableMedia::isPaused( )
{
    if ( jukebox_ && loadedComponent_ )
        return loadedComponent_->isPaused( );
    else
        return false;
}


void ReloadableMedia::buildSlideshowCandidates(const std::string& collectionPath, int maxSubCollections, int maxRandomMedia, Item* selectedItem)
{
    slideCandidates_.clear();
    std::vector<std::string> subFiles = Utils::getFilesInDirectory(collectionPath, { "sub" });
    std::string mediaPath = Utils::combinePath(collectionPath, "medium_artwork", isVideo_ ? "video" : "fanart");
    std::vector<std::string> extensions = isVideo_ ? videoBuild.getSupportedExtensions() : imageBuild.getSupportedExtensions();
    if (!isVideo_)
    {
        auto animatedExtensions = animatedImageBuild.getSupportedExtensions();
        extensions.insert(extensions.end(), animatedExtensions.begin(), animatedExtensions.end());
    }
    std::vector<std::string> directFiles = Utils::getFilesInDirectory(mediaPath, extensions);
    std::vector<std::pair<std::string, std::string>> gameCandidates;
    std::random_device rd;
    std::mt19937 rng(rd());

    // Process sub-collections
    bool hasEmptySubFile = false;
    if (!subFiles.empty())
    {
        int subCollectionsProcessed = 0;
        std::shuffle(subFiles.begin(), subFiles.end(), rng);
        for (const std::string& subFile : subFiles)
        {
            if (subCollectionsProcessed >= maxSubCollections || static_cast<int>(gameCandidates.size()) >= maxRandomMedia) break;
            std::string system = Utils::getFileNameWithoutExtension(subFile);
            std::vector<std::string> games = Utils::readGamesFromSubFile(subFile);
            if (games.empty())
            {
                hasEmptySubFile = true;
                // Load all media from the sub-collection's directory
                std::string systemMediaPath = Utils::combinePath(Configuration::absolutePath, "collections", system, "medium_artwork", isVideo_ ? "video" : "fanart");
                std::vector<std::string> systemFiles = Utils::getFilesInDirectory(systemMediaPath, extensions);
                std::unordered_set<std::string> systemGames;
                for (const std::string& file : systemFiles)
                {
                    systemGames.insert(Utils::getFileNameWithoutExtension(file));
                }
                std::vector<std::string> systemBasenames(systemGames.begin(), systemGames.end());
                std::shuffle(systemBasenames.begin(), systemBasenames.end(), rng);
                for (size_t i = 0; i < systemBasenames.size() && static_cast<int>(gameCandidates.size()) < maxRandomMedia; ++i)
                {
                    gameCandidates.emplace_back(system, systemBasenames[i]);
                }
                subCollectionsProcessed++;
                continue;
            }
            std::string systemMediaPath = Utils::combinePath(Configuration::absolutePath, "collections", system, "medium_artwork", isVideo_ ? "video" : "fanart");
            std::vector<std::string> systemFiles = Utils::getFilesInDirectory(systemMediaPath, extensions);
            std::unordered_set<std::string> gameSet;
            for (const std::string& file : systemFiles)
            {
                gameSet.insert(Utils::getFileNameWithoutExtension(file));
            }
            std::shuffle(games.begin(), games.end(), rng);
            for (const std::string& game : games)
            {
                if (static_cast<int>(gameCandidates.size()) >= maxRandomMedia) break;
                if (gameSet.count(game))
                {
                    gameCandidates.emplace_back(system, game);
                }
            }
            subCollectionsProcessed++;
        }
    }

    // If no sub-files exist, use direct files from the parent collection
    if (subFiles.empty() && static_cast<int>(gameCandidates.size()) < maxRandomMedia && !directFiles.empty())
    {
        int needed = maxRandomMedia - static_cast<int>(gameCandidates.size());
        std::unordered_set<std::string> directGames;
        for (const std::string& file : directFiles)
        {
            directGames.insert(Utils::getFileNameWithoutExtension(file));
        }
        std::vector<std::string> directBasenames(directGames.begin(), directGames.end());
        std::shuffle(directBasenames.begin(), directBasenames.end(), rng);
        for (size_t i = 0; i < static_cast<size_t>(needed) && i < directBasenames.size(); ++i)
        {
            gameCandidates.emplace_back(selectedItem->name, directBasenames[i]);
        }
    }

    // Fallback: try default media in parent collection
    if (gameCandidates.empty())
    {
        std::string defaultMediaPath = Utils::combinePath(mediaPath, "default");
        if (Utils::findMatchingFile(defaultMediaPath, extensions, defaultMediaPath))
        {
            gameCandidates.emplace_back(selectedItem->name, "default");
        }
    }

    // Finalize candidates
    if (!gameCandidates.empty())
    {
        std::shuffle(gameCandidates.begin(), gameCandidates.end(), rng);
        slideCandidates_.assign(gameCandidates.begin(), gameCandidates.begin() + std::min(static_cast<size_t>(maxRandomMedia), gameCandidates.size()));
#ifdef _DEBUG
        Logger::write(Logger::ZONE_INFO, "ReloadableMedia",
            "Collected " + std::to_string(slideCandidates_.size()) + " candidates for slideshow (isVideo=" + std::to_string(isVideo_) + ", hasEmptySubFile=" + std::to_string(hasEmptySubFile) + ")");
#endif
    }
    else
    {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_WARNING, "ReloadableMedia", "No random game candidates found for slideshow (isVideo=" + std::to_string(isVideo_) + ", hasEmptySubFile=" + std::to_string(hasEmptySubFile) + ")");
#endif
    }
}

void ReloadableMedia::buildSynchronizedCandidates(const std::string& collectionPath, int maxSubCollections, int maxRandomMedia, Item* selectedItem)
{
    std::vector<std::string> subFiles = Utils::getFilesInDirectory(collectionPath, { "sub" });
    std::string fanartPath = Utils::combinePath(collectionPath, "medium_artwork", "fanart");
    std::string videoPath = Utils::combinePath(collectionPath, "medium_artwork", "video");
    std::vector<std::string> fanartExtensions = imageBuild.getSupportedExtensions();
    auto animatedExtensions = animatedImageBuild.getSupportedExtensions();
    fanartExtensions.insert(fanartExtensions.end(), animatedExtensions.begin(), animatedExtensions.end());
    std::vector<std::string> videoExtensions = videoBuild.getSupportedExtensions();
    std::vector<std::string> fanartFiles = Utils::getFilesInDirectory(fanartPath, fanartExtensions);
    std::vector<std::string> videoFiles = Utils::getFilesInDirectory(videoPath, videoExtensions);
    std::unordered_set<std::string> fanartGames;
    for (const auto& file : fanartFiles)
    {
        fanartGames.insert(Utils::getFileNameWithoutExtension(file));
    }
    std::unordered_set<std::string> videoGames;
    for (const auto& file : videoFiles)
    {
        videoGames.insert(Utils::getFileNameWithoutExtension(file));
    }
    std::vector<std::pair<std::string, std::string>> gameCandidates;
    std::random_device rd;
    std::mt19937 rng(rd());

    // Direct files with both image and video
    bool hasEmptySubFile = false;
    for (const auto& game : fanartGames)
    {
        if (videoGames.count(game))
        {
            gameCandidates.emplace_back(selectedItem->name, game);
        }
    }

    // Process sub-collections
    if (!subFiles.empty())
    {
        int subCollectionsProcessed = 0;
        std::shuffle(subFiles.begin(), subFiles.end(), rng);
        for (const std::string& subFile : subFiles)
        {
            if (subCollectionsProcessed >= maxSubCollections || static_cast<int>(gameCandidates.size()) >= maxRandomMedia) break;
            std::string system = Utils::getFileNameWithoutExtension(subFile);
            std::vector<std::string> games = Utils::readGamesFromSubFile(subFile);
            if (games.empty())
            {
                hasEmptySubFile = true;
                // Load all media from the sub-collection's directory
                std::string systemFanartPath = Utils::combinePath(Configuration::absolutePath, "collections", system, "medium_artwork", "fanart");
                std::string systemVideoPath = Utils::combinePath(Configuration::absolutePath, "collections", system, "medium_artwork", "video");
                std::vector<std::string> systemFanartFiles = Utils::getFilesInDirectory(systemFanartPath, fanartExtensions);
                std::vector<std::string> systemVideoFiles = Utils::getFilesInDirectory(systemVideoPath, videoExtensions);
                std::unordered_set<std::string> systemFanartGames;
                for (const auto& file : systemFanartFiles)
                {
                    systemFanartGames.insert(Utils::getFileNameWithoutExtension(file));
                }
                std::unordered_set<std::string> systemVideoGames;
                for (const auto& file : systemVideoFiles)
                {
                    systemVideoGames.insert(Utils::getFileNameWithoutExtension(file));
                }
                std::vector<std::string> systemBasenames;
                for (const auto& game : systemFanartGames)
                {
                    if (systemVideoGames.count(game))
                    {
                        systemBasenames.push_back(game);
                    }
                }
                std::shuffle(systemBasenames.begin(), systemBasenames.end(), rng);
                for (size_t i = 0; i < systemBasenames.size() && static_cast<int>(gameCandidates.size()) < maxRandomMedia; ++i)
                {
                    gameCandidates.emplace_back(system, systemBasenames[i]);
                }
                subCollectionsProcessed++;
                continue;
            }
            std::string systemFanartPath = Utils::combinePath(Configuration::absolutePath, "collections", system, "medium_artwork", "fanart");
            std::string systemVideoPath = Utils::combinePath(Configuration::absolutePath, "collections", system, "medium_artwork", "video");
            std::vector<std::string> systemFanartFiles = Utils::getFilesInDirectory(systemFanartPath, fanartExtensions);
            std::vector<std::string> systemVideoFiles = Utils::getFilesInDirectory(systemVideoPath, videoExtensions);
            std::unordered_set<std::string> systemFanartGames;
            for (const auto& file : systemFanartFiles)
            {
                systemFanartGames.insert(Utils::getFileNameWithoutExtension(file));
            }
            std::unordered_set<std::string> systemVideoGames;
            for (const auto& file : systemVideoFiles)
            {
                systemVideoGames.insert(Utils::getFileNameWithoutExtension(file));
            }
            std::shuffle(games.begin(), games.end(), rng);
            for (const std::string& game : games)
            {
                if (static_cast<int>(gameCandidates.size()) >= maxRandomMedia) break;
                if (systemFanartGames.count(game) && systemVideoGames.count(game))
                {
                    gameCandidates.emplace_back(system, game);
                }
            }
            subCollectionsProcessed++;
        }
    }

    // If no sub-files exist, use direct files from the parent collection
    if (subFiles.empty() && static_cast<int>(gameCandidates.size()) < maxRandomMedia)
    {
        int needed = maxRandomMedia - static_cast<int>(gameCandidates.size());
        std::unordered_set<std::string> directGames;
        for (const auto& game : fanartGames)
        {
            if (videoGames.count(game))
            {
                directGames.insert(game);
            }
        }
        std::vector<std::string> directBasenames(directGames.begin(), directGames.end());
        std::shuffle(directBasenames.begin(), directBasenames.end(), rng);
        for (size_t i = 0; i < static_cast<size_t>(needed) && i < directBasenames.size(); ++i)
        {
            gameCandidates.emplace_back(selectedItem->name, directBasenames[i]);
        }
    }

    // Fallback: try default media in parent collection
    if (gameCandidates.empty())
    {
        std::string defaultFanartPath = Utils::combinePath(fanartPath, "default");
        std::string defaultVideoPath = Utils::combinePath(videoPath, "default");
        bool hasDefaultFanart = Utils::findMatchingFile(defaultFanartPath, fanartExtensions, defaultFanartPath);
        bool hasDefaultVideo = Utils::findMatchingFile(defaultVideoPath, videoExtensions, defaultVideoPath);
        if (hasDefaultFanart && hasDefaultVideo)
        {
            gameCandidates.emplace_back(selectedItem->name, "default");
        }
    }

    if (!gameCandidates.empty())
    {
        std::shuffle(gameCandidates.begin(), gameCandidates.end(), rng);
        currentSelectedSystem = gameCandidates[0].first;
        currentSelectedGame = gameCandidates[0].second;
        currentSelectedItem = selectedItem;
#ifdef _DEBUG
        Logger::write(Logger::ZONE_INFO, "ReloadableMedia",
            "randomSyncMedia: Selected " + currentSelectedSystem + "/" + currentSelectedGame + " (hasEmptySubFile=" + std::to_string(hasEmptySubFile) + ")");
#endif
    }
    else
    {
        currentSelectedSystem = "";
        currentSelectedGame = "";
        currentSelectedItem = selectedItem;
#ifdef _DEBUG
        Logger::write(Logger::ZONE_WARNING, "ReloadableMedia", "No synchronized random game candidates found (hasEmptySubFile=" + std::to_string(hasEmptySubFile) + ")");
#endif
    }
}

void ReloadableMedia::buildUnsyncedRandomCandidate(const std::string& collectionPath, int maxSubCollections, int maxRandomMedia, Item* selectedItem)
{
    std::vector<std::string> subFiles = Utils::getFilesInDirectory(collectionPath, { "sub" });
    std::string mediaPath = Utils::combinePath(collectionPath, "medium_artwork", isVideo_ ? "video" : "fanart");
    std::vector<std::string> extensions = isVideo_ ? videoBuild.getSupportedExtensions() : imageBuild.getSupportedExtensions();
    if (!isVideo_)
    {
        auto animatedExtensions = animatedImageBuild.getSupportedExtensions();
        extensions.insert(extensions.end(), animatedExtensions.begin(), animatedExtensions.end());
    }
    std::vector<std::string> directFiles = Utils::getFilesInDirectory(mediaPath, extensions);
    std::vector<std::pair<std::string, std::string>> gameCandidates;
    std::random_device rd;
    std::mt19937 rng(rd());

    // Process sub-collections
    bool hasEmptySubFile = false;
    if (!subFiles.empty())
    {
        int subCollectionsProcessed = 0;
        std::shuffle(subFiles.begin(), subFiles.end(), rng);
        for (const std::string& subFile : subFiles)
        {
            if (subCollectionsProcessed >= maxSubCollections || static_cast<int>(gameCandidates.size()) >= maxRandomMedia) break;
            std::string system = Utils::getFileNameWithoutExtension(subFile);
            std::vector<std::string> games = Utils::readGamesFromSubFile(subFile);
            if (games.empty())
            {
                hasEmptySubFile = true;
                // Load all media from the sub-collection's directory
                std::string systemMediaPath = Utils::combinePath(Configuration::absolutePath, "collections", system, "medium_artwork", isVideo_ ? "video" : "fanart");
                std::vector<std::string> systemFiles = Utils::getFilesInDirectory(systemMediaPath, extensions);
                std::unordered_set<std::string> systemGames;
                for (const std::string& file : systemFiles)
                {
                    systemGames.insert(Utils::getFileNameWithoutExtension(file));
                }
                std::vector<std::string> systemBasenames(systemGames.begin(), systemGames.end());
                std::shuffle(systemBasenames.begin(), systemBasenames.end(), rng);
                for (size_t i = 0; i < systemBasenames.size() && static_cast<int>(gameCandidates.size()) < maxRandomMedia; ++i)
                {
                    gameCandidates.emplace_back(system, systemBasenames[i]);
                }
                subCollectionsProcessed++;
                continue;
            }
            std::string systemMediaPath = Utils::combinePath(Configuration::absolutePath, "collections", system, "medium_artwork", isVideo_ ? "video" : "fanart");
            std::vector<std::string> systemFiles = Utils::getFilesInDirectory(systemMediaPath, extensions);
            std::unordered_set<std::string> gameSet;
            for (const std::string& file : systemFiles)
            {
                gameSet.insert(Utils::getFileNameWithoutExtension(file));
            }
            std::shuffle(games.begin(), games.end(), rng);
            for (const std::string& game : games)
            {
                if (static_cast<int>(gameCandidates.size()) >= maxRandomMedia) break;
                if (gameSet.count(game))
                {
                    gameCandidates.emplace_back(system, game);
                }
            }
            subCollectionsProcessed++;
        }
    }

    // If no sub-files exist, use direct files from the parent collection
    if (subFiles.empty() && static_cast<int>(gameCandidates.size()) < maxRandomMedia && !directFiles.empty())
    {
        int needed = maxRandomMedia - static_cast<int>(gameCandidates.size());
        std::unordered_set<std::string> directGames;
        for (const std::string& file : directFiles)
        {
            directGames.insert(Utils::getFileNameWithoutExtension(file));
        }
        std::vector<std::string> directBasenames(directGames.begin(), directGames.end());
        std::shuffle(directBasenames.begin(), directBasenames.end(), rng);
        for (size_t i = 0; i < static_cast<size_t>(needed) && i < directBasenames.size(); ++i)
        {
            gameCandidates.emplace_back(selectedItem->name, directBasenames[i]);
        }
    }

    // Fallback: try default media in parent collection
    if (gameCandidates.empty())
    {
        std::string defaultMediaPath = Utils::combinePath(mediaPath, "default");
        if (Utils::findMatchingFile(defaultMediaPath, extensions, defaultMediaPath))
        {
            gameCandidates.emplace_back(selectedItem->name, "default");
        }
    }

    if (!gameCandidates.empty())
    {
        std::shuffle(gameCandidates.begin(), gameCandidates.end(), rng);
        auto [system, game] = gameCandidates[0];
        loadedComponent_ = findComponent(system, type_, game, "", false, isVideo_);
        if (loadedComponent_)
        {
            loadedComponent_->allocateGraphicsMemory();
#ifdef _DEBUG
            Logger::write(Logger::ZONE_INFO, "ReloadableMedia",
                "Unsynced random media selected: " + system + "/" + game + " (isVideo=" + std::to_string(isVideo_) + ", hasEmptySubFile=" + std::to_string(hasEmptySubFile) + ")");
#endif
        }
        else
        {
#ifdef _DEBUG
            Logger::write(Logger::ZONE_WARNING, "ReloadableMedia",
                "Failed to load unsynced random media: " + system + "/" + game + " (isVideo=" + std::to_string(isVideo_) + ")");
#endif
        }
    }
    else
    {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_WARNING, "ReloadableMedia", "No random game candidates found for unsynced mode (isVideo=" + std::to_string(isVideo_) + ", hasEmptySubFile=" + std::to_string(hasEmptySubFile) + ")");
#endif
    }
}

void ReloadableMedia::buildSharedSlideshowCandidates(const std::string& collectionPath, int maxSubCollections, int maxRandomMedia, Item* selectedItem)
{
    sharedSlideCandidates_.clear();
    std::vector<std::string> subFiles = Utils::getFilesInDirectory(collectionPath, { "sub" });
    std::string fanartPath = Utils::combinePath(collectionPath, "medium_artwork", "fanart");
    std::string videoPath = Utils::combinePath(collectionPath, "medium_artwork", "video");
    std::vector<std::string> fanartExtensions = imageBuild.getSupportedExtensions();
    auto animatedExtensions = animatedImageBuild.getSupportedExtensions();
    fanartExtensions.insert(fanartExtensions.end(), animatedExtensions.begin(), animatedExtensions.end());
    std::vector<std::string> videoExtensions = videoBuild.getSupportedExtensions();
    std::vector<std::string> fanartFiles = Utils::getFilesInDirectory(fanartPath, fanartExtensions);
    std::vector<std::string> videoFiles = Utils::getFilesInDirectory(videoPath, videoExtensions);
    std::unordered_set<std::string> fanartGames;
    for (const auto& file : fanartFiles)
    {
        fanartGames.insert(Utils::getFileNameWithoutExtension(file));
    }
    std::unordered_set<std::string> videoGames;
    for (const auto& file : videoFiles)
    {
        videoGames.insert(Utils::getFileNameWithoutExtension(file));
    }
    std::vector<std::pair<std::string, std::string>> gameCandidates;
    std::random_device rd;
    std::mt19937 rng(rd());

    // Direct files with both image and video (parent collection)
    bool hasEmptySubFile = false;
    for (const auto& game : fanartGames)
    {
        if (videoGames.count(game))
        {
            gameCandidates.emplace_back(selectedItem->name, game);
        }
    }

    // Process sub-collections
    if (!subFiles.empty())
    {
        int subCollectionsProcessed = 0;
        std::shuffle(subFiles.begin(), subFiles.end(), rng);
        for (const std::string& subFile : subFiles)
        {
            if (subCollectionsProcessed >= maxSubCollections || static_cast<int>(gameCandidates.size()) >= maxRandomMedia) break;
            std::string system = Utils::getFileNameWithoutExtension(subFile);
            std::vector<std::string> games = Utils::readGamesFromSubFile(subFile);
            if (games.empty())
            {
                hasEmptySubFile = true;
                // Load all media from the sub-collection's directory
                std::string systemFanartPath = Utils::combinePath(Configuration::absolutePath, "collections", system, "medium_artwork", "fanart");
                std::string systemVideoPath = Utils::combinePath(Configuration::absolutePath, "collections", system, "medium_artwork", "video");
                std::vector<std::string> systemFanartFiles = Utils::getFilesInDirectory(systemFanartPath, fanartExtensions);
                std::vector<std::string> systemVideoFiles = Utils::getFilesInDirectory(systemVideoPath, videoExtensions);
                std::unordered_set<std::string> systemFanartGames;
                for (const auto& file : systemFanartFiles)
                {
                    systemFanartGames.insert(Utils::getFileNameWithoutExtension(file));
                }
                std::unordered_set<std::string> systemVideoGames;
                for (const auto& file : systemVideoFiles)
                {
                    systemVideoGames.insert(Utils::getFileNameWithoutExtension(file));
                }
                std::vector<std::string> systemBasenames;
                for (const auto& game : systemFanartGames)
                {
                    if (systemVideoGames.count(game))
                    {
                        systemBasenames.push_back(game);
                    }
                }
                std::shuffle(systemBasenames.begin(), systemBasenames.end(), rng);
                for (size_t i = 0; i < systemBasenames.size() && static_cast<int>(gameCandidates.size()) < maxRandomMedia; ++i)
                {
                    gameCandidates.emplace_back(system, systemBasenames[i]);
                }
                // Fallback: try default media in sub-collection
                if (static_cast<int>(gameCandidates.size()) < maxRandomMedia)
                {
                    std::string defaultFanartPath = Utils::combinePath(systemFanartPath, "default");
                    std::string defaultVideoPath = Utils::combinePath(systemVideoPath, "default");
                    bool hasDefaultFanart = Utils::findMatchingFile(defaultFanartPath, fanartExtensions, defaultFanartPath);
                    bool hasDefaultVideo = Utils::findMatchingFile(defaultVideoPath, videoExtensions, defaultVideoPath);
                    if (hasDefaultFanart && hasDefaultVideo)
                    {
                        gameCandidates.emplace_back(system, "default");
                    }
                }
                subCollectionsProcessed++;
                continue;
            }
            std::string systemFanartPath = Utils::combinePath(Configuration::absolutePath, "collections", system, "medium_artwork", "fanart");
            std::string systemVideoPath = Utils::combinePath(Configuration::absolutePath, "collections", system, "medium_artwork", "video");
            std::vector<std::string> systemFanartFiles = Utils::getFilesInDirectory(systemFanartPath, fanartExtensions);
            std::vector<std::string> systemVideoFiles = Utils::getFilesInDirectory(systemVideoPath, videoExtensions);
            std::unordered_set<std::string> systemFanartGames;
            for (const auto& file : systemFanartFiles)
            {
                systemFanartGames.insert(Utils::getFileNameWithoutExtension(file));
            }
            std::unordered_set<std::string> systemVideoGames;
            for (const auto& file : systemVideoFiles)
            {
                systemVideoGames.insert(Utils::getFileNameWithoutExtension(file));
            }
            std::shuffle(games.begin(), games.end(), rng);
            for (const std::string& game : games)
            {
                if (static_cast<int>(gameCandidates.size()) >= maxRandomMedia) break;
                if (systemFanartGames.count(game) && systemVideoGames.count(game))
                {
                    gameCandidates.emplace_back(system, game);
                }
            }
            subCollectionsProcessed++;
        }
    }

    // If no sub-files exist, use direct files from the parent collection
    if (subFiles.empty() && static_cast<int>(gameCandidates.size()) < maxRandomMedia)
    {
        int needed = maxRandomMedia - static_cast<int>(gameCandidates.size());
        std::unordered_set<std::string> directGames;
        for (const auto& game : fanartGames)
        {
            if (videoGames.count(game))
            {
                directGames.insert(game);
            }
        }
        std::vector<std::string> directBasenames(directGames.begin(), directGames.end());
        std::shuffle(directBasenames.begin(), directBasenames.end(), rng);
        for (size_t i = 0; i < static_cast<size_t>(needed) && i < directBasenames.size(); ++i)
        {
            gameCandidates.emplace_back(selectedItem->name, directBasenames[i]);
        }
    }

    // Fallback: try default media in parent collection
    if (gameCandidates.empty())
    {
        std::string defaultFanartPath = Utils::combinePath(fanartPath, "default");
        std::string defaultVideoPath = Utils::combinePath(videoPath, "default");
        bool hasDefaultFanart = Utils::findMatchingFile(defaultFanartPath, fanartExtensions, defaultFanartPath);
        bool hasDefaultVideo = Utils::findMatchingFile(defaultVideoPath, videoExtensions, defaultVideoPath);
        if (hasDefaultFanart && hasDefaultVideo)
        {
            gameCandidates.emplace_back(selectedItem->name, "default");
        }
    }

    if (!gameCandidates.empty())
    {
        std::shuffle(gameCandidates.begin(), gameCandidates.end(), rng);
        sharedSlideCandidates_.assign(gameCandidates.begin(), gameCandidates.begin() + std::min(static_cast<size_t>(maxRandomMedia), gameCandidates.size()));
#ifdef _DEBUG
        Logger::write(Logger::ZONE_INFO, "ReloadableMedia",
            "Collected " + std::to_string(sharedSlideCandidates_.size()) + " synchronized slideshow candidates (hasEmptySubFile=" + std::to_string(hasEmptySubFile) + ")");
#endif
    }
    else
    {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_WARNING, "ReloadableMedia", "No synchronized slideshow candidates found (hasEmptySubFile=" + std::to_string(hasEmptySubFile) + ")");
#endif
    }
}