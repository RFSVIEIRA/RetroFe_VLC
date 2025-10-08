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


#include "ScrollingList.h"
#include "../Animate/Tween.h"
#include "../Animate/TweenSet.h"
#include "../Animate/Animation.h"
#include "../Animate/AnimationEvents.h"
#include "../Animate/TweenTypes.h"
#include "../Font.h"
#include "ImageBuilder.h"
#include "VideoBuilder.h"
#include "VideoComponent.h"
#include "ReloadableMedia.h"
#include "Text.h"
#include "../../Database/Configuration.h"
#include "../../Collection/Item.h"
#include "../../Utility/Utils.h"
#include "../../Utility/Log.h"
#include "../../SDL.h"
#include "../ViewInfo.h"
#include <math.h>
#include <SDL_image.h>
#include <sstream>
#include <cctype>
#include <iomanip>
#include "AnimatedImageBuilder.h"
#include <random>
#include <unordered_map>
#include <unordered_set>

ScrollingList::ScrollingList(Configuration& c, Page& p, bool layoutMode, bool commonMode, Font* font,
    std::string layoutKey, std::string imageType, std::string videoType,
    bool random, bool slideShow, Uint32 slideShowTimer)
    : Component(p), 
    horizontalScroll(false), 
    layoutMode_(layoutMode), 
    commonMode_(commonMode),
    spriteList_(NULL), 
    scrollPoints_(NULL), 
    tweenPoints_(NULL), 
    itemIndex_(0), 
    selectedOffsetIndex_(0),
    scrollAcceleration_(0), 
    startScrollTime_(0.500), 
    minScrollTime_(0.500), 
    scrollPeriod_(0),
    config_(c), 
    fontInst_(font), 
    layoutKey_(layoutKey), 
    imageType_(imageType), 
    videoType_(videoType),
    random_(random), 
    slideShow_(slideShow), 
    slideShowTimer_(slideShowTimer), 
    items_(NULL),
    preloadAhead_(20), 
    preloadBehind_(20) // Default preload values
{
    // Initialize preload values from configuration, if available
    config_.getProperty("preloadAhead", preloadAhead_);
    config_.getProperty("preloadBehind", preloadBehind_);
}


ScrollingList::ScrollingList(const ScrollingList& copy)
    : Component(copy)
    , horizontalScroll(copy.horizontalScroll)
    , layoutMode_(copy.layoutMode_)
    , commonMode_(copy.commonMode_)
    , spriteList_(NULL)
    , itemIndex_(0)
    , selectedOffsetIndex_(copy.selectedOffsetIndex_)
    , scrollAcceleration_(copy.scrollAcceleration_)
    , startScrollTime_(copy.startScrollTime_)
    , minScrollTime_(copy.minScrollTime_)
    , scrollPeriod_(copy.startScrollTime_)
    , config_(copy.config_)
    , fontInst_(copy.fontInst_)
    , layoutKey_(copy.layoutKey_)
    , imageType_(copy.imageType_)
    , items_(NULL)
{
    scrollPoints_ = NULL;
    tweenPoints_ = NULL;

    setPoints(copy.scrollPoints_, copy.tweenPoints_);

}


ScrollingList::~ScrollingList()
{
    stopLoaderThread();
    destroyItems();
    while (scrollPoints_ && scrollPoints_->size() > 0) {
        ViewInfo* scrollPoint = scrollPoints_->back();
        delete scrollPoint;
        scrollPoints_->pop_back();
    }
}


void ScrollingList::setItems(std::vector<Item*>* items)
{
    isInitializing = true;
    items_ = items;
    if (items_) {
        if (!isGrid) {
            itemIndex_ = loopDecrement(0, selectedOffsetIndex_, items_->size());
        }
        else {
            itemIndex_ = 0;
            selectedOffsetIndex_ = 0;
        }
#ifdef _DEBUG
        Logger::write(Logger::ZONE_INFO, "ScrollingList", "setItems: itemIndex_=" + std::to_string(itemIndex_) + ", selectedOffsetIndex_=" + std::to_string(selectedOffsetIndex_));
#endif
    }
    isInitializing = false;
    if (items_) {
        allocateSpritePoints();
        updateCache(); // Update cache when items are set
    }
}
unsigned int ScrollingList::loopIncrement(size_t offset, size_t i, size_t size)
{
    if (size == 0) return 0;
    return static_cast<int>((offset + i) % size); 
}


unsigned int ScrollingList::loopDecrement(size_t offset, size_t i, size_t size)
{
    if (size == 0) return 0;
    return static_cast<int>(((offset % size) - (i % size) + size) % size);  
}


void ScrollingList::setScrollAcceleration(float value)
{
    scrollAcceleration_ = value;
}


void ScrollingList::setStartScrollTime(float value)
{
    startScrollTime_ = value;
}


void ScrollingList::setMinScrollTime(float value)
{
    minScrollTime_ = value;
}


void ScrollingList::deallocateSpritePoints()
{
    for (unsigned int i = 0; i < components_.size(); ++i)
    {
        deallocateTexture(i);
    }
}

#include <chrono>

void ScrollingList::allocateSpritePoints() {
    auto start = std::chrono::high_resolution_clock::now();
    if (!items_ || items_->size() == 0 || !scrollPoints_ || components_.size() == 0) return;

    for (unsigned int i = 0; i < scrollPoints_->size(); ++i) {
        auto loopStart = std::chrono::high_resolution_clock::now();
        unsigned int index = isGrid ? (itemIndex_ + i) : loopIncrement(itemIndex_, i, items_->size());
        Component* old = components_.at(i);

        if (isGrid && index >= items_->size()) {
            deallocateTexture(i);
            components_.at(i) = nullptr;
            if (old && !newItemSelected) {
                delete old;
            }
            continue;
        }

        Item* item = items_->at(index);
        std::lock_guard<std::mutex> lock(cacheMutex_);
        auto it = componentCache_.find(index);
        if (it != componentCache_.end() && it->second) {
            components_.at(i) = it->second;
            componentCache_.erase(it);
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "ScrollingList", "Used cached component for index=" + std::to_string(index));
#endif
        }
        else {
            auto allocStart = std::chrono::high_resolution_clock::now();
            allocateTexture(i, item);
            auto allocEnd = std::chrono::high_resolution_clock::now();
            auto allocDuration = std::chrono::duration_cast<std::chrono::milliseconds>(allocEnd - allocStart).count();
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "ScrollingList", "allocateTexture for index=" + std::to_string(index) + " took " + std::to_string(allocDuration) + " ms");
#endif
        }

        Component* c = components_.at(i);
        if (c) {
            auto graphicsStart = std::chrono::high_resolution_clock::now();
            c->allocateGraphicsMemory();
            auto graphicsEnd = std::chrono::high_resolution_clock::now();
            auto graphicsDuration = std::chrono::duration_cast<std::chrono::milliseconds>(graphicsEnd - graphicsStart).count();
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "ScrollingList", "allocateGraphicsMemory for index=" + std::to_string(index) + " took " + std::to_string(graphicsDuration) + " ms");
#endif

            ViewInfo* view = scrollPoints_->at(i);
            resetTweens(c, tweenPoints_->at(i), view, view, 0);
        }
        if (old && !newItemSelected) {
            if (c) c->baseViewInfo = old->baseViewInfo;
            delete old;
        }
#ifdef _DEBUG
        auto loopEnd = std::chrono::high_resolution_clock::now();
        auto loopDuration = std::chrono::duration_cast<std::chrono::milliseconds>(loopEnd - loopStart).count();
        if (loopDuration > 10) {
            Logger::write(Logger::ZONE_WARNING, "ScrollingList", "Loop iteration " + std::to_string(i) + " took " + std::to_string(loopDuration) + " ms");
        }
#endif
    }
#ifdef _DEBUG
    auto end = std::chrono::high_resolution_clock::now();
    auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    Logger::write(Logger::ZONE_INFO, "ScrollingList", "allocateSpritePoints took " + std::to_string(totalDuration) + " ms");
#endif
}



void ScrollingList::destroyItems()
{
    for (unsigned int i = 0; i < components_.size(); ++i) {
        if (components_.at(i)) {
            components_.at(i)->freeGraphicsMemory();
            delete components_.at(i);
            components_.at(i) = nullptr;
        }
    }
    std::lock_guard<std::mutex> lock(cacheMutex_);
    for (auto& pair : componentCache_) {
        pair.second->freeGraphicsMemory();
        delete pair.second;
    }
    componentCache_.clear();
    Logger::write(Logger::ZONE_DEBUG, "ScrollingList", "destroyItems: Cleared component cache");
}

void ScrollingList::setPoints(std::vector<ViewInfo*>* scrollPoints, std::vector<AnimationEvents*>* tweenPoints)
{
    scrollPoints_ = scrollPoints;
    tweenPoints_ = tweenPoints;

    // empty out the list as we will resize it
    components_.clear();

    size_t size = 0;

    if (scrollPoints)
    {
        size = scrollPoints_->size();
    }
    components_.resize(size);

    if (items_)
    {
        itemIndex_ = loopDecrement(0, selectedOffsetIndex_, items_->size());
    }
}


unsigned int ScrollingList::getScrollOffsetIndex()
{
    return loopIncrement(itemIndex_, selectedOffsetIndex_, items_->size());
}


void ScrollingList::setScrollOffsetIndex(unsigned int index) {
    if (!items_ || items_->size() == 0) return;
    if (isGrid) {
        // Calculate items per page based on grid dimensions
        int itemsPerPage = columns * (scrollPoints_ ? scrollPoints_->size() / columns : 1);
        // Set the starting page index
        itemIndex_ = (index / itemsPerPage) * itemsPerPage;
        // Set the offset within the page
        selectedOffsetIndex_ = index - itemIndex_;
        // Ensure the offset is within bounds
        if (selectedOffsetIndex_ >= itemsPerPage || selectedOffsetIndex_ >= static_cast<int>(items_->size())) {
            selectedOffsetIndex_ = std::min(static_cast<int>(items_->size()) - 1, itemsPerPage - 1);
        }
    }
    else {
        // For non-grid menus
        itemIndex_ = loopDecrement(index, selectedOffsetIndex_, items_->size());
    }
    allocateSpritePoints(); // Update the grid layout
#ifdef _DEBUG
    Logger::write(Logger::ZONE_DEBUG, "ScrollingList", "setScrollOffsetIndex: index=" + std::to_string(index) + ", itemIndex_=" + std::to_string(itemIndex_) + ", selectedOffsetIndex_=" + std::to_string(selectedOffsetIndex_));
#endif
}


void ScrollingList::setSelectedIndex(int selectedIndex)
{
    selectedOffsetIndex_ = selectedIndex;

}


Item* ScrollingList::getItemByOffset(int offset)
{

    if (!items_ || items_->size() == 0) return NULL;

    unsigned int index = getSelectedIndex();
    if (offset >= 0)
    {
        index = loopIncrement(index, offset, items_->size());
    }
    else
    {
        index = loopDecrement(index, offset * -1, items_->size());
    }

    return items_->at(index);

}


Item* ScrollingList::getSelectedItem()
{
    if (!items_ || items_->size() == 0) return NULL;
    return items_->at(loopIncrement(itemIndex_, selectedOffsetIndex_, items_->size()));
}


void ScrollingList::pageUp()
{
    if (components_.size() == 0) return;
    itemIndex_ = loopDecrement(itemIndex_, components_.size(), items_->size());

}


void ScrollingList::pageDown()
{
    if (components_.size() == 0) return;
    itemIndex_ = loopIncrement(itemIndex_, components_.size(), items_->size());

}


void ScrollingList::random()
{
    if (!items_ || items_->size() == 0) return;
    itemIndex_ = rand() % items_->size();

}


void ScrollingList::letterUp()
{
    letterChange(true);
}


void ScrollingList::letterDown()
{
    letterChange(false);
}


void ScrollingList::letterChange(bool increment)
{
    if (!items_ || items_->size() == 0) return;

    // Calculate the current selected index
    size_t currentSelectedIndex = (itemIndex_ + selectedOffsetIndex_) % items_->size();
    std::string startname = items_->at(currentSelectedIndex)->lowercaseFullTitle();
    char startLetter = startname.empty() ? 0 : startname[0];

    // Initialize the new selected index
    size_t newSelectedIndex = currentSelectedIndex;

    // Find the next or previous item where the letter changes
    if (increment) {
        for (size_t i = 1; i < items_->size(); ++i) {
            size_t checkIndex = (currentSelectedIndex + i) % items_->size();
            std::string checkname = items_->at(checkIndex)->lowercaseFullTitle();
            char checkLetter = checkname.empty() ? 0 : checkname[0];
            if ((isalpha(startLetter) != isalpha(checkLetter)) ||
                (isalpha(startLetter) && isalpha(checkLetter) && startLetter != checkLetter)) {
                newSelectedIndex = checkIndex;
                break;
            }
        }
    }
    else {
        for (size_t i = 1; i < items_->size(); ++i) {
            size_t checkIndex = (currentSelectedIndex + items_->size() - i) % items_->size();
            std::string checkname = items_->at(checkIndex)->lowercaseFullTitle();
            char checkLetter = checkname.empty() ? 0 : checkname[0];
            if ((isalpha(startLetter) != isalpha(checkLetter)) ||
                (isalpha(startLetter) && isalpha(checkLetter) && startLetter != checkLetter)) {
                newSelectedIndex = checkIndex;
                break;
            }
        }
        // Adjust to the first item of the new letter group (for decrementing)
        char newLetter = items_->at(newSelectedIndex)->lowercaseFullTitle()[0];
        while (newSelectedIndex > 0) {
            size_t prevIndex = (newSelectedIndex - 1) % items_->size();
            char prevLetter = items_->at(prevIndex)->lowercaseFullTitle()[0];
            if (prevLetter != newLetter) {
                break;
            }
            newSelectedIndex = prevIndex;
        }
    }

    // Update itemIndex_ and selectedOffsetIndex_ using setScrollOffsetIndex
    setScrollOffsetIndex(newSelectedIndex);
}


void ScrollingList::subUp()
{
    subChange(true);
}


void ScrollingList::subDown()
{
    subChange(false);
}


void ScrollingList::subChange(bool increment)
{

    if (!items_ || items_->size() == 0) return;

    Item* startItem = items_->at((itemIndex_ + selectedOffsetIndex_) % items_->size());
    std::string startname = items_->at((itemIndex_ + selectedOffsetIndex_) % items_->size())->collectionInfo->lowercaseName();

    for (unsigned int i = 0; i < items_->size(); ++i)
    {
        unsigned int index = 0;
        if (increment)
        {
            index = loopIncrement(itemIndex_, i, items_->size());
        }
        else
        {
            index = loopDecrement(itemIndex_, i, items_->size());
        }

        std::string endname = items_->at((index + selectedOffsetIndex_) % items_->size())->collectionInfo->lowercaseName();

        if (startname != endname)
        {
            itemIndex_ = index;
            break;
        }
    }

    if (!increment) // For decrement, find the first game of the new sub
    {
        bool prevLetterSubToCurrent = false;
        config_.getProperty("prevLetterSubToCurrent", prevLetterSubToCurrent);
        if (!prevLetterSubToCurrent || items_->at((itemIndex_ + 1 + selectedOffsetIndex_) % items_->size()) == startItem)
        {
            startname = items_->at((itemIndex_ + selectedOffsetIndex_) % items_->size())->collectionInfo->lowercaseName();

            for (unsigned int i = 0; i < items_->size(); ++i)
            {
                unsigned int index = loopDecrement(itemIndex_, i, items_->size());

                std::string endname = items_->at((index + selectedOffsetIndex_) % items_->size())->collectionInfo->lowercaseName();

                if (startname != endname)
                {
                    itemIndex_ = loopIncrement(index, 1, items_->size());
                    break;
                }
            }
        }
        else
        {
            itemIndex_ = loopIncrement(itemIndex_, 1, items_->size());
        }
    }
}


void ScrollingList::cfwLetterSubUp()
{
    if (Utils::toLower(collectionName) != items_->at((itemIndex_ + selectedOffsetIndex_) % items_->size())->collectionInfo->lowercaseName())
        subChange(true);
    else
        letterChange(true);
}


void ScrollingList::cfwLetterSubDown()
{
    if (Utils::toLower(collectionName) != items_->at((itemIndex_ + selectedOffsetIndex_) % items_->size())->collectionInfo->lowercaseName())
    {
        subChange(false);
        if (Utils::toLower(collectionName) == items_->at((itemIndex_ + selectedOffsetIndex_) % items_->size())->collectionInfo->lowercaseName())
        {
            subChange(true);
            letterChange(false);
        }
    }
    else
    {
        letterChange(false);
        if (Utils::toLower(collectionName) != items_->at((itemIndex_ + selectedOffsetIndex_) % items_->size())->collectionInfo->lowercaseName())
        {
            letterChange(true);
            subChange(false);
        }
    }
}


void ScrollingList::allocateGraphicsMemory()
{
    Component::allocateGraphicsMemory();
    scrollPeriod_ = startScrollTime_;

    allocateSpritePoints();
}


void ScrollingList::freeGraphicsMemory()
{
    Component::freeGraphicsMemory();
    scrollPeriod_ = 0;

    deallocateSpritePoints();
}

void ScrollingList::triggerEnterEvent()
{
    for (unsigned int i = 0; i < components_.size(); ++i)
    {
        Component* c = components_.at(i);
        if (c) c->triggerEvent("enter");
    }
}

void ScrollingList::triggerExitEvent()
{
    for (unsigned int i = 0; i < components_.size(); ++i)
    {
        Component* c = components_.at(i);
        if (c) c->triggerEvent("exit");
    }
}

void ScrollingList::triggerMenuEnterEvent(int menuIndex)
{
    for (unsigned int i = 0; i < components_.size(); ++i)
    {
        Component* c = components_.at(i);
        if (c) c->triggerEvent("menuEnter", menuIndex);
    }
}

void ScrollingList::triggerMenuExitEvent(int menuIndex)
{
    for (unsigned int i = 0; i < components_.size(); ++i)
    {
        Component* c = components_.at(i);
        if (c) c->triggerEvent("menuExit", menuIndex);
    }
}

void ScrollingList::triggerGameEnterEvent(int menuIndex)
{
    for (unsigned int i = 0; i < components_.size(); ++i)
    {
        Component* c = components_.at(i);
        if (c) c->triggerEvent("gameEnter", menuIndex);
    }
}

void ScrollingList::triggerGameExitEvent(int menuIndex)
{
    for (unsigned int i = 0; i < components_.size(); ++i)
    {
        Component* c = components_.at(i);
        if (c) c->triggerEvent("gameExit", menuIndex);
    }
}

void ScrollingList::triggerHighlightEnterEvent(int menuIndex)
{
    for (unsigned int i = 0; i < components_.size(); ++i)
    {
        Component* c = components_.at(i);
        if (c) c->triggerEvent("highlightEnter", menuIndex);
    }
}

void ScrollingList::triggerHighlightExitEvent(int menuIndex)
{
    for (unsigned int i = 0; i < components_.size(); ++i)
    {
        Component* c = components_.at(i);
        if (c) c->triggerEvent("highlightExit", menuIndex);
    }
}

void ScrollingList::triggerPlaylistEnterEvent(int menuIndex)
{
    for (unsigned int i = 0; i < components_.size(); ++i)
    {
        Component* c = components_.at(i);
        if (c) c->triggerEvent("playlistEnter", menuIndex);
    }
}

void ScrollingList::triggerPlaylistExitEvent(int menuIndex)
{
    for (unsigned int i = 0; i < components_.size(); ++i)
    {
        Component* c = components_.at(i);
        if (c) c->triggerEvent("playlistExit", menuIndex);
    }
}

void ScrollingList::triggerMenuJumpEnterEvent(int menuIndex)
{
    for (unsigned int i = 0; i < components_.size(); ++i)
    {
        Component* c = components_.at(i);
        if (c) c->triggerEvent("menuJumpEnter", menuIndex);
    }
}

void ScrollingList::triggerMenuJumpExitEvent(int menuIndex)
{
    for (unsigned int i = 0; i < components_.size(); ++i)
    {
        Component* c = components_.at(i);
        if (c) c->triggerEvent("menuJumpExit", menuIndex);
    }
}

void ScrollingList::triggerAttractEnterEvent(int menuIndex)
{
    for (unsigned int i = 0; i < components_.size(); ++i)
    {
        Component* c = components_.at(i);
        if (c) c->triggerEvent("attractEnter", menuIndex);
    }
}

void ScrollingList::triggerAttractEvent(int menuIndex)
{
    for (unsigned int i = 0; i < components_.size(); ++i)
    {
        Component* c = components_.at(i);
        if (c) c->triggerEvent("attract", menuIndex);
    }
}

void ScrollingList::triggerAttractExitEvent(int menuIndex)
{
    for (unsigned int i = 0; i < components_.size(); ++i)
    {
        Component* c = components_.at(i);
        if (c) c->triggerEvent("attractExit", menuIndex);
    }
}

void ScrollingList::triggerJukeboxJumpEvent(int menuIndex)
{
    for (unsigned int i = 0; i < components_.size(); ++i)
    {
        Component* c = components_.at(i);
        if (c) c->triggerEvent("jukeboxJump", menuIndex);
    }
}

void ScrollingList::update(float dt)
{

    Component::update(dt);

    if (components_.size() == 0) return;
    if (!items_) return;

    for (unsigned int i = 0; i < scrollPoints_->size(); i++)
    {
        Component* c = components_.at(i);
        if (c) c->update(dt);
    }
}

//TODO slideShow is crashing program

//void ScrollingList::update(float dt)
//{
//    Component::update(dt);
//
//    if (components_.size() == 0 || !items_) return;
//
//    for (unsigned int i = 0; i < scrollPoints_->size(); i++)
//    {
//        Component *c = components_.at(i);
//        if (c) c->update(dt);
//
//        // Slideshow logic
//        if (random_ && slideShow_ && !slideStates_[i].slideCandidates.empty())
//        {
//            slideStates_[i].elapsedTime += dt;
//            if (slideStates_[i].elapsedTime >= slideShowTimer_ / 1000.0f) // Convert ms to seconds
//            {
//                slideStates_[i].elapsedTime = 0.0f;
//                slideStates_[i].currentSlideIndex = (slideStates_[i].currentSlideIndex + 1) % slideStates_[i].slideCandidates.size();
//
//                // Load the next candidate
//                auto [system, game] = slideStates_[i].slideCandidates[slideStates_[i].currentSlideIndex];
//                std::string candidatePath = Utils::combinePath(Configuration::absolutePath, "collections", system, "medium_artwork", videoType_ != "null" ? videoType_ : imageType_);
//
//                Component *newComponent = nullptr;
//                ImageBuilder imageBuild;
//                VideoBuilder videoBuild;
//                AnimatedImageBuilder animatedimageBuild;
//
//                if (videoType_ != "null")
//                {
//                    newComponent = videoBuild.createVideo(candidatePath, page, game, baseViewInfo.Monitor, false);
//                }
//                else
//                {
//                    newComponent = imageBuild.CreateImage(candidatePath, page, game, baseViewInfo.Monitor);
//                    if (!newComponent)
//                    {
//                        newComponent = animatedimageBuild.CreateImage(candidatePath, page, game, baseViewInfo.Monitor);
//                    }
//                }
//
//                if (newComponent)
//                {
//                    newComponent->allocateGraphicsMemory();
//                    newComponent->baseViewInfo = components_[i]->baseViewInfo; // Preserve position, scale, etc.
//                    components_[i]->freeGraphicsMemory();
//                    delete components_[i];
//                    components_[i] = newComponent;
//                }
//            }
//        }
//    }
//}

unsigned int ScrollingList::getSelectedIndex()
{
    if (!items_) return 0;
    return loopIncrement(itemIndex_, selectedOffsetIndex_, items_->size());
}


void ScrollingList::setSelectedIndex(unsigned int index)
{
    if (!items_) return;
    itemIndex_ = loopDecrement(index, selectedOffsetIndex_, items_->size());
}


size_t ScrollingList::getSize()
{
    if (!items_) return 0;
    return items_->size();
}


void ScrollingList::resetTweens(Component* c, AnimationEvents* sets, ViewInfo* currentViewInfo, ViewInfo* nextViewInfo, double scrollTime)
{
    if (!c) return;
    if (!sets) return;
    if (!currentViewInfo) return;
    if (!nextViewInfo) return;

    currentViewInfo->ImageHeight = c->baseViewInfo.ImageHeight;
    currentViewInfo->ImageWidth = c->baseViewInfo.ImageWidth;
    nextViewInfo->ImageHeight = c->baseViewInfo.ImageHeight;
    nextViewInfo->ImageWidth = c->baseViewInfo.ImageWidth;
    nextViewInfo->BackgroundAlpha = c->baseViewInfo.BackgroundAlpha;

    c->setTweens(sets);

    Animation* scrollTween = sets->getAnimation("menuScroll");
    scrollTween->Clear();
    c->baseViewInfo = *currentViewInfo;
    c->baseViewInfo.frameLoop = nextViewInfo->frameLoop;
    c->baseViewInfo.speed = nextViewInfo->speed;

    TweenSet* set = new TweenSet();
    set->push(new Tween(TWEEN_PROPERTY_HEIGHT, LINEAR, currentViewInfo->Height, nextViewInfo->Height, scrollTime));
    set->push(new Tween(TWEEN_PROPERTY_WIDTH, LINEAR, currentViewInfo->Width, nextViewInfo->Width, scrollTime));
    set->push(new Tween(TWEEN_PROPERTY_ANGLE, LINEAR, currentViewInfo->Angle, nextViewInfo->Angle, scrollTime));
    set->push(new Tween(TWEEN_PROPERTY_ALPHA, LINEAR, currentViewInfo->Alpha, nextViewInfo->Alpha, scrollTime));
    set->push(new Tween(TWEEN_PROPERTY_X, LINEAR, currentViewInfo->X, nextViewInfo->X, scrollTime));
    set->push(new Tween(TWEEN_PROPERTY_Y, LINEAR, currentViewInfo->Y, nextViewInfo->Y, scrollTime));
    set->push(new Tween(TWEEN_PROPERTY_X_ORIGIN, LINEAR, currentViewInfo->XOrigin, nextViewInfo->XOrigin, scrollTime));
    set->push(new Tween(TWEEN_PROPERTY_Y_ORIGIN, LINEAR, currentViewInfo->YOrigin, nextViewInfo->YOrigin, scrollTime));
    set->push(new Tween(TWEEN_PROPERTY_X_OFFSET, LINEAR, currentViewInfo->XOffset, nextViewInfo->XOffset, scrollTime));
    set->push(new Tween(TWEEN_PROPERTY_Y_OFFSET, LINEAR, currentViewInfo->YOffset, nextViewInfo->YOffset, scrollTime));
    set->push(new Tween(TWEEN_PROPERTY_FONT_SIZE, LINEAR, currentViewInfo->FontSize, nextViewInfo->FontSize, scrollTime));
    set->push(new Tween(TWEEN_PROPERTY_BACKGROUND_ALPHA, LINEAR, currentViewInfo->BackgroundAlpha, nextViewInfo->BackgroundAlpha, scrollTime));
    set->push(new Tween(TWEEN_PROPERTY_MAX_WIDTH, LINEAR, currentViewInfo->MaxWidth, nextViewInfo->MaxWidth, scrollTime));
    set->push(new Tween(TWEEN_PROPERTY_MAX_HEIGHT, LINEAR, currentViewInfo->MaxHeight, nextViewInfo->MaxHeight, scrollTime));
    set->push(new Tween(TWEEN_PROPERTY_LAYER, LINEAR, currentViewInfo->Layer, nextViewInfo->Layer, scrollTime));
    set->push(new Tween(TWEEN_PROPERTY_VOLUME, LINEAR, currentViewInfo->Volume, nextViewInfo->Volume, scrollTime));
    set->push(new Tween(TWEEN_PROPERTY_MONITOR, LINEAR, currentViewInfo->Monitor, nextViewInfo->Monitor, scrollTime));
    scrollTween->Push(set);
}

// TODO slideShow is crashing Program

//bool ScrollingList::allocateTexture(unsigned int index, Item* item)
//{
//
//    if (index >= components_.size()) return false;
//
//    std::string imagePath;
//    std::string videoPath;
//
//    Component* t = NULL;
//
//    ImageBuilder imageBuild;
//    VideoBuilder videoBuild;
//    AnimatedImageBuilder animatedimageBuild;
//
//    std::string layoutName;
//    config_.getProperty("layout", layoutName);
//
//    std::string typeLC = Utils::toLower(imageType_);
//
//    // Ensure slideStates_ is large enough
//    if (slideStates_.size() <= index) {
//        slideStates_.resize(components_.size(), { {}, 0, 0.0f });
//    }
//
//    // Free any existing component at this index to prevent memory leaks
//    if (components_[index]) {
//        components_[index]->freeGraphicsMemory();
//        delete components_[index];
//        components_[index] = nullptr;
//    }
//
//    // Random and slideshow logic
//    if (random_)
//    {
//        std::string collectionName = item->name;
//        if (!collectionName.empty())
//        {
//            std::string collectionPath = Utils::combinePath(Configuration::absolutePath, "collections", collectionName);
//            std::vector<std::string> subFiles = Utils::getFilesInDirectory(collectionPath, { "sub" });
//            std::string mediaPath = Utils::combinePath(collectionPath, "medium_artwork", videoType_ != "null" ? videoType_ : imageType_);
//            std::vector<std::string> extensions = videoType_ != "null" ?
//                std::vector<std::string>{ "mp4", "avi", "mkv" } :
//                std::vector<std::string>{ "png", "jpg", "jpeg", "webp" };
//            std::vector<std::string> directFiles = Utils::getFilesInDirectory(mediaPath, extensions);
//
//            std::vector<std::pair<std::string, std::string>> candidates; // Pair of (system, game)
//            std::random_device rd;
//            std::mt19937 rng(rd());
//            const int maxSubCollections = 5;  // Configurable limit
//            const int maxRandomMedia = 10;    // Number of slideshow items, configurable
//
//            // Process subdirectories (.sub files)
//            if (!subFiles.empty())
//            {
//                int subCollectionsProcessed = 0;
//                std::shuffle(subFiles.begin(), subFiles.end(), rng);
//                for (const std::string& subFile : subFiles)
//                {
//                    if (subCollectionsProcessed >= maxSubCollections || static_cast<int>(candidates.size()) >= maxRandomMedia) break;
//
//                    std::string system = Utils::getFileNameWithoutExtension(subFile);
//                    std::vector<std::string> games = Utils::readGamesFromSubFile(subFile);
//                    if (games.empty()) { subCollectionsProcessed++; continue; }
//
//                    std::string systemMediaPath = Utils::combinePath(Configuration::absolutePath, "collections", system, "medium_artwork", videoType_ != "null" ? videoType_ : imageType_);
//                    std::vector<std::string> systemFiles = Utils::getFilesInDirectory(systemMediaPath, extensions);
//
//                    std::unordered_set<std::string> gameSet;
//                    for (const std::string& file : systemFiles)
//                    {
//                        gameSet.insert(Utils::getFileNameWithoutExtension(file));
//                    }
//
//                    std::shuffle(games.begin(), games.end(), rng);
//                    for (const std::string& game : games)
//                    {
//                        if (static_cast<int>(candidates.size()) >= maxRandomMedia) break;
//                        if (gameSet.count(game))
//                        {
//                            candidates.emplace_back(system, game);
//                        }
//                    }
//                    subCollectionsProcessed++;
//                }
//            }
//
//            // Process direct files in the collection's medium_artwork directory
//            if (!directFiles.empty() && static_cast<int>(candidates.size()) < maxRandomMedia)
//            {
//                int needed = maxRandomMedia - static_cast<int>(candidates.size());
//                std::unordered_set<std::string> directGames;
//                for (const std::string& file : directFiles)
//                {
//                    directGames.insert(Utils::getFileNameWithoutExtension(file));
//                }
//
//                std::vector<std::string> directBasenames(directGames.begin(), directGames.end());
//                std::shuffle(directBasenames.begin(), directBasenames.end(), rng);
//                for (size_t i = 0; i < static_cast<size_t>(needed) && i < directBasenames.size(); ++i)
//                {
//                    candidates.emplace_back(collectionName, directBasenames[i]);
//                }
//            }
//
//            // Handle candidates based on slideshow or single random selection
//            if (!candidates.empty())
//            {
//                std::shuffle(candidates.begin(), candidates.end(), rng);
//                if (slideShow_)
//                {
//                    // Store candidates for slideshow
//                    slideStates_[index].slideCandidates = candidates;
//                    slideStates_[index].currentSlideIndex = 0;
//                    slideStates_[index].elapsedTime = 0.0f;
//
//                    // Load the first candidate
//                    auto [system, game] = candidates[0];
//                    std::string candidatePath = Utils::combinePath(Configuration::absolutePath, "collections", system, "medium_artwork", videoType_ != "null" ? videoType_ : imageType_);
//                    if (videoType_ != "null")
//                    {
//                        t = videoBuild.createVideo(candidatePath, page, game, baseViewInfo.Monitor, false);
//                    }
//                    else
//                    {
//                        t = imageBuild.CreateImage(candidatePath, page, game, baseViewInfo.Monitor);
//                        if (!t)
//                        {
//                            t = animatedimageBuild.CreateImage(candidatePath, page, game, baseViewInfo.Monitor);
//                        }
//                    }
//                }
//                else
//                {
//                    // Load a single random candidate
//                    auto [system, game] = candidates[0];
//                    std::string candidatePath = Utils::combinePath(Configuration::absolutePath, "collections", system, "medium_artwork", videoType_ != "null" ? videoType_ : imageType_);
//                    if (videoType_ != "null")
//                    {
//                        t = videoBuild.createVideo(candidatePath, page, game, baseViewInfo.Monitor, false);
//                    }
//                    else
//                    {
//                        t = imageBuild.CreateImage(candidatePath, page, game, baseViewInfo.Monitor);
//                        if (!t)
//                        {
//                            t = animatedimageBuild.CreateImage(candidatePath, page, game, baseViewInfo.Monitor);
//                        }
//                    }
//                    slideStates_[index].slideCandidates.clear(); // No slideshow
//                }
//            }
//            else
//            {
//                slideStates_[index].slideCandidates.clear(); // No candidates
//            }
//        }
//    }

bool ScrollingList::allocateTexture(unsigned int index, Item* item)
{
    static const size_t MAX_COMPONENTS = 1000; // Arbitrary safe limit
    static const size_t MAX_PATH_LENGTH = 1024; // Limit for file paths

    if (!item) {
        Logger::write(Logger::ZONE_ERROR, "ScrollingList", "Null item provided for index: " + std::to_string(index));
        return false;
    }

    if (index >= components_.size()) {
        Logger::write(Logger::ZONE_ERROR, "ScrollingList", "Invalid index: " + std::to_string(index) + ", components_.size(): " + std::to_string(components_.size()));
        return false;
    }

    // Ensure slideStates_ is large enough, but bound the size
    if (slideStates_.size() <= index) {
        size_t newSize = std::min(components_.size(), MAX_COMPONENTS);
        if (newSize > slideStates_.size()) {
            slideStates_.resize(newSize, { {}, 0, 0.0f });
            Logger::write(Logger::ZONE_INFO, "ScrollingList", "Resized slideStates_ to: " + std::to_string(newSize));
        }
    }

    std::string imagePath;
    std::string videoPath;
    Component* t = nullptr;

    ImageBuilder imageBuild;
    VideoBuilder videoBuild;
    AnimatedImageBuilder animatedimageBuild;

    std::string layoutName;
    config_.getProperty("layout", layoutName);

    std::string typeLC = Utils::toLower(imageType_);
    std::string collectionName = item->name;

    // Random selection logic
    if (random_ && !collectionName.empty()) {
        int maxSubCollections = 5;
        int maxRandomMedia = 100;
        config_.getProperty("maxsubcollections", maxSubCollections);
        config_.getProperty("maxRandomMedia", maxRandomMedia);

        std::string collectionPath = Utils::combinePath(Configuration::absolutePath, "collections", collectionName);
        if (collectionPath.size() > MAX_PATH_LENGTH) {
            Logger::write(Logger::ZONE_WARNING, "ScrollingList", "Collection path too long, truncating: " + collectionPath.substr(0, 100));
            collectionPath = collectionPath.substr(0, MAX_PATH_LENGTH);
        }

        std::vector<std::string> subFiles = Utils::getFilesInDirectory(collectionPath, { "sub" });
        std::string mediaPath = Utils::combinePath(collectionPath, "medium_artwork", videoType_ != "null" ? videoType_ : imageType_);
        if (mediaPath.size() > MAX_PATH_LENGTH) {
            Logger::write(Logger::ZONE_WARNING, "ScrollingList", "Media path too long, truncating: " + mediaPath.substr(0, 100));
            mediaPath = mediaPath.substr(0, MAX_PATH_LENGTH);
        }

        std::vector<std::string> extensions = videoType_ != "null" ?
            std::vector<std::string>{"mp4", "avi", "mkv"} :
            std::vector<std::string>{ "png", "jpg", "jpeg" };
        std::vector<std::string> directFiles = Utils::getFilesInDirectory(mediaPath, extensions);

        std::vector<std::pair<std::string, std::string>> candidates;
        std::random_device rd;
        std::mt19937 rng(rd());

        // Process subdirectories (.sub files)
        int subCollectionsProcessed = 0;
        if (!subFiles.empty()) {
            std::shuffle(subFiles.begin(), subFiles.end(), rng);
            for (const std::string& subFile : subFiles) {
                if (subCollectionsProcessed >= maxSubCollections || static_cast<int>(candidates.size()) >= maxRandomMedia) break;

                std::string system = Utils::getFileNameWithoutExtension(subFile);
                std::vector<std::string> games = Utils::readGamesFromSubFile(subFile);
                if (games.empty()) {
                    subCollectionsProcessed++;
                    continue;
                }

                std::string systemMediaPath = Utils::combinePath(Configuration::absolutePath, "collections", system, "medium_artwork", videoType_ != "null" ? videoType_ : imageType_);
                if (systemMediaPath.size() > MAX_PATH_LENGTH) {
                    Logger::write(Logger::ZONE_WARNING, "ScrollingList", "System media path too long, truncating: " + systemMediaPath.substr(0, 100));
                    systemMediaPath = systemMediaPath.substr(0, MAX_PATH_LENGTH);
                }

                std::vector<std::string> systemFiles = Utils::getFilesInDirectory(systemMediaPath, extensions);

                std::unordered_set<std::string> gameSet;
                for (const std::string& file : systemFiles) {
                    gameSet.insert(Utils::getFileNameWithoutExtension(file));
                }

                std::shuffle(games.begin(), games.end(), rng);
                for (const std::string& game : games) {
                    if (static_cast<int>(candidates.size()) >= maxRandomMedia) break;
                    if (gameSet.count(game)) {
                        candidates.emplace_back(system, game);
                    }
                }
                subCollectionsProcessed++;
            }
        }

        // Fallback to direct files
        if (!directFiles.empty() && static_cast<int>(candidates.size()) < maxRandomMedia) {
            int needed = maxRandomMedia - static_cast<int>(candidates.size());
            std::unordered_set<std::string> directGames;
            for (const std::string& file : directFiles) {
                directGames.insert(Utils::getFileNameWithoutExtension(file));
            }

            std::vector<std::string> directBasenames(directGames.begin(), directGames.end());
            std::shuffle(directBasenames.begin(), directBasenames.end(), rng);
            for (size_t i = 0; i < static_cast<size_t>(needed) && i < directBasenames.size(); ++i) {
                candidates.emplace_back(collectionName, directBasenames[i]);
            }
        }

        // Select a random candidate
        if (!candidates.empty()) {
            std::shuffle(candidates.begin(), candidates.end(), rng);
            auto [system, game] = candidates[0];
            std::string candidatePath = Utils::combinePath(Configuration::absolutePath, "collections", system, "medium_artwork", videoType_ != "null" ? videoType_ : imageType_);
            if (candidatePath.size() > MAX_PATH_LENGTH) {
                Logger::write(Logger::ZONE_WARNING, "ScrollingList", "Candidate path too long, truncating: " + candidatePath.substr(0, 100));
                candidatePath = candidatePath.substr(0, MAX_PATH_LENGTH);
            }

            if (videoType_ != "null") {
                t = videoBuild.createVideo(candidatePath, page, game, baseViewInfo.Monitor, false);
            }
            else {
                t = imageBuild.CreateImage(candidatePath, page, game, baseViewInfo.Monitor);
                if (!t) {
                    t = animatedimageBuild.CreateImage(candidatePath, page, game, baseViewInfo.Monitor);
                }
            }
            slideStates_[index].slideCandidates.clear(); // Clear to avoid slideshow issues
        }
    }

    std::vector<std::string> names;
    names.push_back(item->name);
    names.push_back(item->fullTitle);
    if (item->cloneof != "")
        names.push_back(item->cloneof);
    if (typeLC == "numberbuttons")
        names.push_back(item->numberButtons);
    if (typeLC == "numberplayers")
        names.push_back(item->numberPlayers);
    if (typeLC == "year")
        names.push_back(item->year);
    if (typeLC == "title")
        names.push_back(item->title);
    if (typeLC == "developer")
    {
        if (item->developer == "")
        {
            names.push_back(item->manufacturer);
        }
        else
        {
            names.push_back(item->developer);
        }
    }
    if (typeLC == "manufacturer")
        names.push_back(item->manufacturer);
    if (typeLC == "genre")
        names.push_back(item->genre);
    if (typeLC == "ctrltype")
        names.push_back(item->ctrlType);
    if (typeLC == "joyways")
        names.push_back(item->joyWays);
    if (typeLC == "rating")
        names.push_back(item->rating);
    if (typeLC == "score")
        names.push_back(item->score);
    names.push_back("default");

    for (unsigned int n = 0; n < names.size() && !t; ++n)
    {
        // check collection path for art
        if (layoutMode_)
        {
            if (commonMode_)
                imagePath = Utils::combinePath(Configuration::absolutePath, "layouts", layoutName, "collections", "_common");
            else
                imagePath = Utils::combinePath(Configuration::absolutePath, "layouts", layoutName, "collections", collectionName);
            imagePath = Utils::combinePath(imagePath, "medium_artwork", imageType_);
            videoPath = Utils::combinePath(imagePath, "medium_artwork", videoType_);
        }
        else
        {
            if (commonMode_)
            {
                imagePath = Utils::combinePath(Configuration::absolutePath, "collections", "_common");
                imagePath = Utils::combinePath(imagePath, "medium_artwork", imageType_);
                videoPath = Utils::combinePath(imagePath, "medium_artwork", videoType_);
            }
            else
            {
                config_.getMediaPropertyAbsolutePath(collectionName, imageType_, false, imagePath);
                config_.getMediaPropertyAbsolutePath(collectionName, videoType_, false, videoPath);
            }
        }
        if (!t)
        {
            if (videoType_ != "null")
            {
                t = videoBuild.createVideo(videoPath, page, names[n], baseViewInfo.Monitor, false);
            }
            else
            {
                t = imageBuild.CreateImage(imagePath, page, names[n], baseViewInfo.Monitor);

                if (!t) //if there is no static image (returns null) and we try to find if it is a gif
                {
                    t = animatedimageBuild.CreateImage(imagePath, page, names[n], baseViewInfo.Monitor);
                }
            }

        }

        // check sub-collection path for art
        if (!t && !commonMode_)
        {
            if (layoutMode_)
            {
                imagePath = Utils::combinePath(Configuration::absolutePath, "layouts", layoutName, "collections", item->collectionInfo->name);
                imagePath = Utils::combinePath(imagePath, "medium_artwork", imageType_);
                videoPath = Utils::combinePath(imagePath, "medium_artwork", videoType_);
            }
            else
            {
                config_.getMediaPropertyAbsolutePath(item->collectionInfo->name, imageType_, false, imagePath);
                config_.getMediaPropertyAbsolutePath(item->collectionInfo->name, videoType_, false, videoPath);
            }
            if (videoType_ != "null")
            {
                t = videoBuild.createVideo(videoPath, page, names[n], baseViewInfo.Monitor, false);
            }
            else
            {
                t = imageBuild.CreateImage(imagePath, page, names[n], baseViewInfo.Monitor);

                if (!t)
                {
                    t = animatedimageBuild.CreateImage(imagePath, page, names[n], baseViewInfo.Monitor);
                }
            }
        }
    }

    // check collection path for art based on system name
    if (!t)
    {
        if (layoutMode_)
        {
            if (commonMode_)
                imagePath = Utils::combinePath(Configuration::absolutePath, "layouts", layoutName, "collections", "_common");
            else
                imagePath = Utils::combinePath(Configuration::absolutePath, "layouts", layoutName, "collections", item->name);
            imagePath = Utils::combinePath(imagePath, "system_artwork");
            videoPath = imagePath;
        }
        else
        {
            if (commonMode_)
            {
                imagePath = Utils::combinePath(Configuration::absolutePath, "collections", "_common");
                imagePath = Utils::combinePath(imagePath, "system_artwork");
                videoPath = imagePath;
            }
            else
            {
                config_.getMediaPropertyAbsolutePath(item->name, imageType_, true, imagePath);
                config_.getMediaPropertyAbsolutePath(item->name, videoType_, true, videoPath);
            }
        }
        if (videoType_ != "null")
        {
            t = videoBuild.createVideo(videoPath, page, videoType_, baseViewInfo.Monitor, false);
        }
        else
        {
            t = imageBuild.CreateImage(imagePath, page, imageType_, baseViewInfo.Monitor);
            if (!t)
            {
                t = animatedimageBuild.CreateImage(imagePath, page, imageType_, baseViewInfo.Monitor);
            }
        }
    }

    // check rom directory path for art
    if (!t)
    {
        if (videoType_ != "null")
        {
            t = videoBuild.createVideo(item->filepath, page, videoType_, baseViewInfo.Monitor, false);
        }
        else
        {
            t = imageBuild.CreateImage(item->filepath, page, imageType_, baseViewInfo.Monitor);
            if (!t)
            {
                t = animatedimageBuild.CreateImage(item->filepath, page, imageType_, baseViewInfo.Monitor);
            }
        }
    }

    // Check for fallback art in case no video could be found
    if (videoType_ != "null" && !t)
    {
        for (unsigned int n = 0; n < names.size() && !t; ++n)
        {
            // check collection path for art
            if (layoutMode_)
            {
                if (commonMode_)
                    imagePath = Utils::combinePath(Configuration::absolutePath, "layouts", layoutName, "collections", "_common");
                else
                    imagePath = Utils::combinePath(Configuration::absolutePath, "layouts", layoutName, "collections", collectionName);
                imagePath = Utils::combinePath(imagePath, "medium_artwork", imageType_);
            }
            else
            {
                if (commonMode_)
                {
                    imagePath = Utils::combinePath(Configuration::absolutePath, "collections", "_common");
                    imagePath = Utils::combinePath(imagePath, "medium_artwork", imageType_);
                }
                else
                {
                    config_.getMediaPropertyAbsolutePath(collectionName, imageType_, false, imagePath);
                }
            }

            t = imageBuild.CreateImage(imagePath, page, names[n], baseViewInfo.Monitor);
            if (!t)
            {
                t = animatedimageBuild.CreateImage(imagePath, page, names[n], baseViewInfo.Monitor);
            }

            // check sub-collection path for art
            if (!t && !commonMode_)
            {
                if (layoutMode_)
                {
                    imagePath = Utils::combinePath(Configuration::absolutePath, "layouts", layoutName, "collections", item->collectionInfo->name);
                    imagePath = Utils::combinePath(imagePath, "medium_artwork", imageType_);
                }
                else
                {
                    config_.getMediaPropertyAbsolutePath(item->collectionInfo->name, imageType_, false, imagePath);
                }

                t = imageBuild.CreateImage(imagePath, page, names[n], baseViewInfo.Monitor);
                if (!t)
                {
                    t = animatedimageBuild.CreateImage(imagePath, page, names[n], baseViewInfo.Monitor);
                }
            }
        }

        // check collection path for art based on system name
        if (!t)
        {
            if (layoutMode_)
            {
                if (commonMode_)
                    imagePath = Utils::combinePath(Configuration::absolutePath, "layouts", layoutName, "collections", "_common");
                else
                    imagePath = Utils::combinePath(Configuration::absolutePath, "layouts", layoutName, "collections", item->name);
                imagePath = Utils::combinePath(imagePath, "system_artwork");
            }
            else
            {
                if (commonMode_)
                {
                    imagePath = Utils::combinePath(Configuration::absolutePath, "collections", "_common");
                    imagePath = Utils::combinePath(imagePath, "system_artwork");
                }
                else
                {
                    config_.getMediaPropertyAbsolutePath(item->name, imageType_, true, imagePath);
                }
            }
            if (!t)
            {
                t = imageBuild.CreateImage(imagePath, page, imageType_, baseViewInfo.Monitor);
                if (!t)
                {
                    t = animatedimageBuild.CreateImage(imagePath, page, imageType_, baseViewInfo.Monitor);
                }
            }
        }
        // check rom directory path for art
        if (!t)
        {
            t = imageBuild.CreateImage(item->filepath, page, imageType_, baseViewInfo.Monitor);
            if (!t)
            {
                t = animatedimageBuild.CreateImage(item->filepath, page, imageType_, baseViewInfo.Monitor);
            }
        }

    }

    if (!t)
    {
        t = new Text(item->title, page, fontInst_, baseViewInfo.Monitor);
    }

    if (t)
    {
        components_.at(index) = t;
    }

    return true;
}


void ScrollingList::deallocateTexture(unsigned int index)
{
    if (components_.size() <= index) return;

    Component* s = components_.at(index);

    if (s)
        s->freeGraphicsMemory();
}

void ScrollingList::draw()
{
    //todo: Poor design implementation.
    // caller should instead call ScrollingList::Draw( unsigned int layer )
}


void ScrollingList::draw(unsigned int layer)
{
    if (components_.size() == 0) return;

    unsigned int selectedIndex = getSelectedIndex();
    SDL_Renderer* renderer = SDL::getRenderer(baseViewInfo.Monitor);

    // First pass: Draw all non-selected items
    for (unsigned int i = 0; i < components_.size(); ++i)
    {
        Component* c = components_.at(i);
        if (!c || c->baseViewInfo.Layer != layer) continue;

        bool isSelected = (isGrid && loopIncrement(itemIndex_, i, items_->size()) == selectedIndex);
        if (isSelected) continue; // Skip the selected item for the second pass

        // Calculate layout scaling factors
        float layoutWidth = static_cast<float>(page.getLayoutWidth(baseViewInfo.Monitor));
        float layoutHeight = static_cast<float>(page.getLayoutHeight(baseViewInfo.Monitor));
        float screenWidth = static_cast<float>(SDL::getWindowWidth(baseViewInfo.Monitor));
        float screenHeight = static_cast<float>(SDL::getWindowHeight(baseViewInfo.Monitor));

        std::string layoutMode = "stretch";
        float layoutScale = 1.0f;
        config_.getProperty("layoutScaleMode", layoutMode);
        config_.getProperty("layoutZoomMode", layoutScale);
        std::string layoutModeKey = "layoutScaleMode" + std::to_string(baseViewInfo.Monitor);
        std::string layoutScaleKey = "layoutZoomMode" + std::to_string(baseViewInfo.Monitor);
        config_.getProperty(layoutModeKey, layoutMode);
        config_.getProperty(layoutScaleKey, layoutScale);

        float scaleX, scaleY, offsetX, offsetY;
        if (layoutMode == "fit") {
            float aspectLayout = layoutWidth / layoutHeight;
            float aspectWindow = screenWidth / screenHeight;
            float scale = (aspectLayout > aspectWindow) ? (screenWidth / layoutWidth) : (screenHeight / layoutHeight);
            scale *= layoutScale;
            scaleX = scale;
            scaleY = scale;
            offsetX = (screenWidth - layoutWidth * scale) / 2;
            offsetY = (screenHeight - layoutHeight * scale) / 2;
        }
        else if (layoutMode == "fill") {
            float aspectLayout = layoutWidth / layoutHeight;
            float aspectWindow = screenWidth / screenHeight;
            float scale = (aspectLayout < aspectWindow) ? (screenWidth / layoutWidth) : (screenHeight / layoutHeight);
            scale *= layoutScale;
            scaleX = scale;
            scaleY = scale;
            offsetX = (screenWidth - layoutWidth * scale) / 2;
            offsetY = (screenHeight - layoutHeight * scale) / 2;
        }
        else if (layoutMode == "none") {
            scaleX = layoutScale;
            scaleY = layoutScale;
            offsetX = (screenWidth - layoutWidth * layoutScale) / 2;
            offsetY = (screenHeight - layoutHeight * layoutScale) / 2;
        }
        else { // "stretch" or default
            scaleX = (screenWidth / layoutWidth) * layoutScale;
            scaleY = (screenHeight / layoutHeight) * layoutScale;
            offsetX = 0;
            offsetY = 0;
        }

        // Zoom and alpha for non-selected items
        float zoomFactor = isGrid ? 1.0f : 1.0f; // No zoom for non-selected items
        float alphaValue = isGrid ? (c->baseViewInfo.Alpha * baseViewInfo.alphaUnselected) : c->baseViewInfo.Alpha;

        // Calculate zoomed dimensions
        float scaledWidth = c->baseViewInfo.ScaledWidth() * zoomFactor;
        float scaledHeight = c->baseViewInfo.ScaledHeight() * zoomFactor;

        // Adjust position based on origin and zoom
        float originX = c->baseViewInfo.XOrigin * scaledWidth;
        float originY = c->baseViewInfo.YOrigin * scaledHeight;
        float x = c->baseViewInfo.X + c->baseViewInfo.XOffset - originX;
        float y = c->baseViewInfo.Y + c->baseViewInfo.YOffset - originY;

        // Define the destination rectangle
        SDL_Rect destRect = {
            static_cast<int>(x * scaleX + offsetX),
            static_cast<int>(y * scaleY + offsetY),
            static_cast<int>(scaledWidth * scaleX),
            static_cast<int>(scaledHeight * scaleY)
        };

        // Draw border if specified
        if (c->baseViewInfo.borderColor && c->baseViewInfo.Alpha > 0 && c->baseViewInfo.ScaledWidth() > 0 && c->baseViewInfo.ScaledHeight() > 0) {
            float space = c->baseViewInfo.borderSpace * zoomFactor;
            float thickness = c->baseViewInfo.borderThickness > 0 ? c->baseViewInfo.borderThickness : 1.0f;
            int pixelThickness = static_cast<int>(std::round(thickness * scaleX * zoomFactor));

            c->baseViewInfo.borderColor->apply(renderer, alphaValue);

            for (int t = 0; t < pixelThickness; ++t) {
                SDL_Rect borderRect = {
                    destRect.x - static_cast<int>(space * scaleX) - t,
                    destRect.y - static_cast<int>(space * scaleY) - t,
                    destRect.w + static_cast<int>(2 * space * scaleX) + 2 * t,
                    destRect.h + static_cast<int>(2 * space * scaleY) + 2 * t
                };
                SDL_RenderDrawRect(renderer, &borderRect);
            }
        }

        // Set selection state for AnimatedImage
        if (AnimatedImage* animatedImage = dynamic_cast<AnimatedImage*>(c)) {
            animatedImage->setSelected(isSelected);
        }

        // Draw non-selected component
        if (isGrid) {
            float originalAlpha = c->baseViewInfo.Alpha;
            c->baseViewInfo.Alpha = alphaValue;
            c->baseViewInfo.Width *= zoomFactor;
            c->baseViewInfo.Height *= zoomFactor;
            c->draw();
            c->baseViewInfo.Alpha = originalAlpha;
            c->baseViewInfo.Width /= zoomFactor;
            c->baseViewInfo.Height /= zoomFactor;
        }
        else {
            c->draw();
        }
    }

    // Second pass: Draw the selected item
    for (unsigned int i = 0; i < components_.size(); ++i)
    {
        Component* c = components_.at(i);
        if (!c || c->baseViewInfo.Layer != layer) continue;

        bool isSelected = (isGrid && loopIncrement(itemIndex_, i, items_->size()) == selectedIndex);
        if (!isSelected) continue;

        // Recalculate scaling factors (for consistency)
        float layoutWidth = static_cast<float>(page.getLayoutWidth(baseViewInfo.Monitor));
        float layoutHeight = static_cast<float>(page.getLayoutHeight(baseViewInfo.Monitor));
        float screenWidth = static_cast<float>(SDL::getWindowWidth(baseViewInfo.Monitor));
        float screenHeight = static_cast<float>(SDL::getWindowHeight(baseViewInfo.Monitor));

        std::string layoutMode = "stretch";
        float layoutScale = 1.0f;
        config_.getProperty("layoutScaleMode", layoutMode);
        config_.getProperty("layoutZoomMode", layoutScale);
        std::string layoutModeKey = "layoutScaleMode" + std::to_string(baseViewInfo.Monitor);
        std::string layoutScaleKey = "layoutZoomMode" + std::to_string(baseViewInfo.Monitor);
        config_.getProperty(layoutModeKey, layoutMode);
        config_.getProperty(layoutScaleKey, layoutScale);

        float scaleX, scaleY, offsetX, offsetY;
        if (layoutMode == "fit") {
            float aspectLayout = layoutWidth / layoutHeight;
            float aspectWindow = screenWidth / screenHeight;
            float scale = (aspectLayout > aspectWindow) ? (screenWidth / layoutWidth) : (screenHeight / layoutHeight);
            scale *= layoutScale;
            scaleX = scale;
            scaleY = scale;
            offsetX = (screenWidth - layoutWidth * scale) / 2;
            offsetY = (screenHeight - layoutHeight * scale) / 2;
        }
        else if (layoutMode == "fill") {
            float aspectLayout = layoutWidth / layoutHeight;
            float aspectWindow = screenWidth / screenHeight;
            float scale = (aspectLayout < aspectWindow) ? (screenWidth / layoutWidth) : (screenHeight / layoutHeight);
            scale *= layoutScale;
            scaleX = scale;
            scaleY = scale;
            offsetX = (screenWidth - layoutWidth * scale) / 2;
            offsetY = (screenHeight - layoutHeight * scale) / 2;
        }
        else if (layoutMode == "none") {
            scaleX = layoutScale;
            scaleY = layoutScale;
            offsetX = (screenWidth - layoutWidth * layoutScale) / 2;
            offsetY = (screenHeight - layoutHeight * layoutScale) / 2;
        }
        else { // "stretch" or default
            scaleX = (screenWidth / layoutWidth) * layoutScale;
            scaleY = (screenHeight / layoutHeight) * layoutScale;
            offsetX = 0;
            offsetY = 0;
        }

        // Zoom and alpha for selected item
        float zoomFactor = isGrid ? (baseViewInfo.selectedZoom) : 1.2f; // 20% zoom for selected item
        float alphaValue = c->baseViewInfo.Alpha; // No dimming for selected item

        // Calculate zoomed dimensions
        float scaledWidth = c->baseViewInfo.ScaledWidth() * zoomFactor;
        float scaledHeight = c->baseViewInfo.ScaledHeight() * zoomFactor;

        // Adjust position based on origin and zoom
        float originX = c->baseViewInfo.XOrigin * scaledWidth;
        float originY = c->baseViewInfo.YOrigin * scaledHeight;
        float x = c->baseViewInfo.X + c->baseViewInfo.XOffset - originX;
        float y = c->baseViewInfo.Y + c->baseViewInfo.YOffset - originY;

        // Define the destination rectangle
        SDL_Rect destRect = {
            static_cast<int>(x * scaleX + offsetX),
            static_cast<int>(y * scaleY + offsetY),
            static_cast<int>(scaledWidth * scaleX),
            static_cast<int>(scaledHeight * scaleY)
        };

        // Draw border if specified
        Color* borderColor = c->baseViewInfo.selectedBorderColor ? c->baseViewInfo.selectedBorderColor : c->baseViewInfo.borderColor;
        if (borderColor && c->baseViewInfo.Alpha > 0 && c->baseViewInfo.ScaledWidth() > 0 && c->baseViewInfo.ScaledHeight() > 0) {
            float space = c->baseViewInfo.borderSpace * zoomFactor;
            float thickness = c->baseViewInfo.borderThickness > 0 ? c->baseViewInfo.borderThickness : 1.0f;
            int pixelThickness = static_cast<int>(std::round(thickness * scaleX * zoomFactor));

            borderColor->apply(renderer, alphaValue);

            for (int t = 0; t < pixelThickness; ++t) {
                SDL_Rect borderRect = {
                    destRect.x - static_cast<int>(space * scaleX) - t,
                    destRect.y - static_cast<int>(space * scaleY) - t,
                    destRect.w + static_cast<int>(2 * space * scaleX) + 2 * t,
                    destRect.h + static_cast<int>(2 * space * scaleY) + 2 * t
                };
                SDL_RenderDrawRect(renderer, &borderRect);
            }
        }

        // Set selection state for AnimatedImage
        if (AnimatedImage* animatedImage = dynamic_cast<AnimatedImage*>(c)) {
            animatedImage->setSelected(isSelected);
        }

        // Draw selected component
        if (isGrid) {
            float originalAlpha = c->baseViewInfo.Alpha;
            float originalWidth = c->baseViewInfo.Width;
            float originalHeight = c->baseViewInfo.Height;

            c->baseViewInfo.Alpha = alphaValue;
            c->baseViewInfo.Width *= zoomFactor;
            c->baseViewInfo.Height *= zoomFactor;
            c->draw();

            c->baseViewInfo.Alpha = originalAlpha;
            c->baseViewInfo.Width = originalWidth;
            c->baseViewInfo.Height = originalHeight;
        }
        else {
            c->draw();
        }
        break; // Only one selected item
    }
}
bool ScrollingList::isIdle()
{
    if (!Component::isIdle()) return false;

    for (unsigned int i = 0; i < components_.size(); ++i)
    {
        Component* c = components_.at(i);
        if (c && !c->isIdle()) return false;
    }

    return true;
}


bool ScrollingList::isAttractIdle()
{
    if (!Component::isAttractIdle()) return false;

    for (unsigned int i = 0; i < components_.size(); ++i)
    {
        Component* c = components_.at(i);
        if (c && !c->isAttractIdle()) return false;
    }

    return true;
}


void ScrollingList::resetScrollPeriod()
{
    scrollPeriod_ = startScrollTime_;
    return;
}


void ScrollingList::updateScrollPeriod()
{
    scrollPeriod_ -= scrollAcceleration_;
    if (scrollPeriod_ < minScrollTime_)
    {
        scrollPeriod_ = minScrollTime_;
    }
}


void ScrollingList::scroll(bool forward)
{
    if (!items_ || items_->size() == 0 || !scrollPoints_ || scrollPoints_->size() == 0) return;
    if (scrollPeriod_ < minScrollTime_) scrollPeriod_ = minScrollTime_;

    // Replace the item that's scrolled out
    if (forward) {
        unsigned int newIndex = loopIncrement(itemIndex_, scrollPoints_->size(), items_->size());
        Item* i = items_->at(newIndex);
        itemIndex_ = loopIncrement(itemIndex_, 1, items_->size());
        deallocateTexture(0);
        allocateTexture(0, i);
    }
    else {
        unsigned int newIndex = loopDecrement(itemIndex_, 1, items_->size());
        Item* i = items_->at(newIndex);
        itemIndex_ = loopDecrement(itemIndex_, 1, items_->size());
        deallocateTexture(components_.size() - 1);
        allocateTexture(components_.size() - 1, i);
    }

    // Set the animations
    for (unsigned int i = 0; i < scrollPoints_->size(); i++) {
        unsigned int nextI = forward ? loopDecrement(i, 1, scrollPoints_->size()) : loopIncrement(i, 1, scrollPoints_->size());
        Component* c = components_.at(i);
        if (c) {
            c->allocateGraphicsMemory();
            resetTweens(c, tweenPoints_->at(nextI), scrollPoints_->at(i), scrollPoints_->at(nextI), scrollPeriod_);
            c->baseViewInfo.font = scrollPoints_->at(nextI)->font;
            c->triggerEvent("menuScroll");
        }
    }

    // Reorder the components
    if (forward) {
        Component* first = components_.front();
        components_.erase(components_.begin());
        components_.push_back(first);
    }
    else {
        Component* last = components_.back();
        components_.pop_back();
        components_.insert(components_.begin(), last);
    }

    updateCache(); // Update cache after scrolling
}

void ScrollingList::moveDown()
{
    if (!items_ || items_->size() == 0 || !isGrid) return;

    int visibleRows = static_cast<int>(scrollPoints_->size()) / columns;
    int totalItems = static_cast<int>(items_->size());
    int absoluteIndex = itemIndex_ + selectedOffsetIndex_;
    int currentRow = selectedOffsetIndex_ / columns;
    int currentColumn = selectedOffsetIndex_ % columns;
    int oldSelectedOffsetIndex = selectedOffsetIndex_;

    if (absoluteIndex + columns < totalItems) {
        // Move down to the next row, same column
        if (currentRow < visibleRows - 1 && selectedOffsetIndex_ + columns < static_cast<int>(scrollPoints_->size())) {
            selectedOffsetIndex_ += columns;
            updateGridSelection(oldSelectedOffsetIndex);

        }
        else {
            // Scroll so that the new item is at the top-left
            int newAbsoluteIndex = absoluteIndex + columns;
            itemIndex_ = newAbsoluteIndex - (newAbsoluteIndex % columns);
            selectedOffsetIndex_ = currentColumn;
            allocateSpritePoints();

        }
    }
    else if (wrap) {
        // Wrap to the first item
        itemIndex_ = 0;
        selectedOffsetIndex_ = 0;
        allocateSpritePoints();

    }
    updateCache();
}

void ScrollingList::moveUp()
{
    if (!items_ || items_->size() == 0 || !isGrid) return;

    int visibleRows = static_cast<int>(scrollPoints_->size()) / columns;
    int itemsPerPage = visibleRows * columns;
    int totalItems = static_cast<int>(items_->size());
    int absoluteIndex = itemIndex_ + selectedOffsetIndex_;
    int currentRow = selectedOffsetIndex_ / columns;
    int currentColumn = selectedOffsetIndex_ % columns;
    int oldSelectedOffsetIndex = selectedOffsetIndex_;

    if (absoluteIndex - columns >= 0) {
        // Move up to the previous row, same column
        if (currentRow > 0) {
            selectedOffsetIndex_ -= columns;
            updateGridSelection(oldSelectedOffsetIndex);
        }
        else if (itemIndex_ >= static_cast<unsigned int>(itemsPerPage)) {
            // Scroll to the previous page
            itemIndex_ -= itemsPerPage;
            selectedOffsetIndex_ = (visibleRows - 1) * columns + currentColumn; // Last visible row, same column
            allocateSpritePoints();
        }
    }
    else if (wrap) {
        // Wrap to the last item
        int totalPages = (totalItems + itemsPerPage - 1) / itemsPerPage;
        itemIndex_ = (totalPages - 1) * itemsPerPage;
        selectedOffsetIndex_ = (totalItems - 1) % itemsPerPage;
        if (selectedOffsetIndex_ >= static_cast<int>(scrollPoints_->size())) {
            selectedOffsetIndex_ = scrollPoints_->size() - 1;
        }
        allocateSpritePoints();
    }
    updateCache();
}

void ScrollingList::moveLeft()
{
    if (!items_ || items_->size() == 0 || !isGrid) return;

    int visibleRows = static_cast<int>(scrollPoints_->size()) / columns;
    int itemsPerPage = visibleRows * columns;
    int totalItems = static_cast<int>(items_->size());
    int absoluteIndex = itemIndex_ + selectedOffsetIndex_;
    int currentColumn = selectedOffsetIndex_ % columns;
    int currentRow = selectedOffsetIndex_ / columns;
    int oldSelectedOffsetIndex = selectedOffsetIndex_;

    if (absoluteIndex > 0) {
        int newAbsoluteIndex = absoluteIndex - 1;
        if (currentColumn > 0 && selectedOffsetIndex_ - 1 >= 0) {
            // Move left within the same row
            selectedOffsetIndex_ -= 1;
            updateGridSelection(oldSelectedOffsetIndex);
        }
        else {
            // Move to the last column of the previous row or scroll
            if (newAbsoluteIndex < itemIndex_) {
                // Scroll to the previous page
                int prevPageStart = std::fmax(0, itemIndex_ - itemsPerPage);
                itemIndex_ = prevPageStart;
                selectedOffsetIndex_ = newAbsoluteIndex - prevPageStart;
                allocateSpritePoints();
            }
            else {
                selectedOffsetIndex_ = (currentRow - 1) * columns + (columns - 1);
            }
        }
    }
    else if (wrap) {
        // Wrap to the last item
        int lastPageStart = (totalItems - 1) / itemsPerPage * itemsPerPage;
        itemIndex_ = lastPageStart;
        selectedOffsetIndex_ = totalItems - 1 - lastPageStart;
        // Ensure selectedOffsetIndex_ is within valid range
        if (selectedOffsetIndex_ >= static_cast<int>(scrollPoints_->size()) || selectedOffsetIndex_ >= totalItems) {
            selectedOffsetIndex_ = std::min(totalItems - 1, static_cast<int>(scrollPoints_->size()) - 1);
        }
        allocateSpritePoints();
    }
    updateCache();
}

void ScrollingList::moveRight()
{
    if (!items_ || items_->size() == 0 || !isGrid) return;

    int visibleRows = static_cast<int>(scrollPoints_->size()) / columns;
    int totalItems = static_cast<int>(items_->size());
    int absoluteIndex = itemIndex_ + selectedOffsetIndex_;
    int currentColumn = selectedOffsetIndex_ % columns;
    int currentRow = selectedOffsetIndex_ / columns;
    int oldSelectedOffsetIndex = selectedOffsetIndex_;

    if (absoluteIndex + 1 < totalItems) {
        // Calculate the new absolute index
        int newAbsoluteIndex = absoluteIndex + 1;
        int newRow = newAbsoluteIndex / columns;
        int newCol = newAbsoluteIndex % columns;

        // Check if the new position is within the current visible grid
        int currentStartRow = itemIndex_ / columns;
        int currentEndRow = currentStartRow + visibleRows - 1;
        updateGridSelection(oldSelectedOffsetIndex);

        if (newCol > 0 && currentColumn < columns - 1 && selectedOffsetIndex_ + 1 < static_cast<int>(scrollPoints_->size())) {
            // Move right within the same row
            selectedOffsetIndex_ += 1;
        }
        else if (newRow <= currentEndRow) {
            // Move to the first column of the next row, still visible
            selectedOffsetIndex_ = (currentRow + 1) * columns;
        }
        else {
            // Scroll so that the new item is at the top-left (row 1, column 1)
            itemIndex_ = newAbsoluteIndex; 
            selectedOffsetIndex_ = 0;     
            allocateSpritePoints();
        }
    }
    else if (wrap) {
        // Wrap to the first item
        itemIndex_ = 0;
        selectedOffsetIndex_ = 0;
        allocateSpritePoints();
    }
    updateCache();
}
//TODO make it beter
void ScrollingList::updateGridSelection(int oldSelectedOffsetIndex)
{
    if (!scrollPoints_ || !tweenPoints_ || components_.empty()) return;

    for (unsigned int i = 0; i < scrollPoints_->size(); i++) {
        Component* c = components_.at(i);
        if (!c) continue;

        // Get the current and target ViewInfo
        ViewInfo* currentViewInfo = &c->baseViewInfo;
        ViewInfo* targetViewInfo = scrollPoints_->at(i);

        // Update ViewInfo based on selection state
        // Note: For grid menus, scrollPoints_ already defines the base positions (set in buildCustomMenu)
        // We rely on <onMenuScroll> in the XML to define animation changes (e.g., alpha, scale)
        c->baseViewInfo = *targetViewInfo; // Reset to base position

        // Reset tweens to apply <onMenuScroll> animations
        AnimationEvents* tweens = tweenPoints_->at(i);
        resetTweens(c, tweens, currentViewInfo, targetViewInfo, scrollPeriod_);

        // Trigger menuScroll event to apply XML-defined animations
        c->triggerEvent("menuScroll");

#ifdef _DEBUG
        Logger::write(Logger::ZONE_DEBUG, "ScrollingList", "Triggered menuScroll for component at index=" +
            std::to_string(i) + ", selected=" + (i == static_cast<unsigned int>(selectedOffsetIndex_) ? "true" : "false"));
#endif
    }
    updateCache();
}


void ScrollingList::updateCache()
{
    size_t currentIndex = getSelectedIndex();
    size_t start = (currentIndex > static_cast<size_t>(preloadBehind_)) ? currentIndex - preloadBehind_ : 0;
    size_t end = std::min(currentIndex + preloadAhead_, items_->size() - 1);
    for (size_t i = start; i <= end; ++i) {
        enqueueLoad(i);
    }
    std::lock_guard<std::mutex> lock(cacheMutex_);
    for (auto it = componentCache_.begin(); it != componentCache_.end(); ) {
        if (it->first < start || it->first > end) {
            it->second->freeGraphicsMemory();
            delete it->second;
            it = componentCache_.erase(it);
        }
        else {
            ++it;
        }
    }
}

void ScrollingList::loadComponent(size_t itemIndex)
{
    if (itemIndex >= items_->size()) return;
    Item* item = items_->at(itemIndex);
    Component* t = nullptr;
    std::string imagePath = Utils::combinePath(Configuration::absolutePath, "collections", item->collectionInfo->name, "medium_artwork", imageType_);
    ImageBuilder imageBuild;
    AnimatedImageBuilder animatedBuild;
    t = imageBuild.CreateImage(imagePath, page, item->name, baseViewInfo.Monitor);
    if (!t) t = animatedBuild.CreateImage(imagePath, page, item->name, baseViewInfo.Monitor);
    if (!t) t = new Text(item->title, page, fontInst_, baseViewInfo.Monitor);
    if (t) {
        t->allocateGraphicsMemory();
        std::lock_guard<std::mutex> lock(cacheMutex_);
        componentCache_[itemIndex] = t;
    }
}
void ScrollingList::enqueueLoad(size_t itemIndex)
{
    if (!items_ || itemIndex >= items_->size()) return;
    std::lock_guard<std::mutex> lock(cacheMutex_);
    if (componentCache_.find(itemIndex) == componentCache_.end()) {
        loadQueue_.push(itemIndex);
        loadCondition_.notify_one();
    }
}

void ScrollingList::stopLoaderThread()
{
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        stopLoader_ = true;
        loadCondition_.notify_all();
    }
    if (loaderThread_.joinable()) {
        loaderThread_.join();
    }
}
void ScrollingList::startLoaderThread()
{
    stopLoader_ = false;
    loaderThread_ = std::thread([this]() {
        while (!stopLoader_) {
            std::unique_lock<std::mutex> lock(cacheMutex_);
            loadCondition_.wait(lock, [this]() { return !loadQueue_.empty() || stopLoader_; });
            if (stopLoader_) break;
            size_t itemIndex = loadQueue_.front();
            loadQueue_.pop();
            lock.unlock();
            loadComponent(itemIndex);
        }
        });
}