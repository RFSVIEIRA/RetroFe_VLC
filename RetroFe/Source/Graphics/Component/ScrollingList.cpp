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
#include "../../Utility/MediaAlias.h"
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
#include "ReloadableScrollingText.h"

unsigned int ScrollingList::globalSelectedIndex = 0;
std::vector<ScrollingList*> ScrollingList::allScrollingLists;

ScrollingList::ScrollingList(Configuration& c, Page& p, bool layoutMode, bool commonMode, Font* font,
    std::string layoutKey, std::string imageType, std::string videoType,
    bool random, bool slideShow, Uint32 slideShowTimer, bool textFallbackEnabled)
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
    preloadBehind_(20),
    // Scrolling text defaults
    textType_(""),
    textFormat_(""),
    singlePrefix_(""),
    singlePostfix_(""),
    pluralPrefix_(""),
    pluralPostfix_(""),
    alignment_(""),
    direction_("horizontal"),
    scrollingSpeed_(50.0f),
    startPosition_(0.0f),
    startTime_(1.0f),
    endTime_(1.0f),
    timeFormat_("%H:%M"),
    scrollingTextEnabled_(false),
    textFallbackEnabled_(textFallbackEnabled)
{
    // Initialize preload values from configuration, if available
    config_.getProperty("preloadAhead", preloadAhead_);
    config_.getProperty("preloadBehind", preloadBehind_);

}

// Extended constructor with full reloadableScrollingText support
ScrollingList::ScrollingList(Configuration& c, Page& p, bool layoutMode, bool commonMode, Font* font,
    std::string layoutKey, std::string imageType, std::string videoType,
    bool random, bool slideShow, Uint32 slideShowTimer,
    std::string textType, std::string textFormat,
    std::string singlePrefix, std::string singlePostfix,
    std::string pluralPrefix, std::string pluralPostfix,
    std::string alignment, std::string direction,
    float scrollingSpeed, float startPosition, float startTime, float endTime,
    std::string timeFormat, bool scrollingTextEnabled, bool textFallbackEnabled)
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
    preloadBehind_(20),
    textType_(textType),
    textFormat_(textFormat),
    singlePrefix_(singlePrefix),
    singlePostfix_(singlePostfix),
    pluralPrefix_(pluralPrefix),
    pluralPostfix_(pluralPostfix),
    alignment_(alignment),
    direction_(direction.empty() ? "horizontal" : direction),
    scrollingSpeed_(scrollingSpeed > 0 ? scrollingSpeed : 50.0f),
    startPosition_(startPosition),
    startTime_(startTime),
    endTime_(endTime),
    timeFormat_(timeFormat.empty() ? "%H:%M" : timeFormat),
    textFallbackEnabled_(textFallbackEnabled),
    scrollingTextEnabled_(scrollingTextEnabled || !textType.empty())
{
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
    , textType_(copy.textType_)
    , textFormat_(copy.textFormat_)
    , singlePrefix_(copy.singlePrefix_)
    , singlePostfix_(copy.singlePostfix_)
    , pluralPrefix_(copy.pluralPrefix_)
    , pluralPostfix_(copy.pluralPostfix_)
    , alignment_(copy.alignment_)
    , direction_(copy.direction_)
    , scrollingSpeed_(copy.scrollingSpeed_)
    , startPosition_(copy.startPosition_)
    , startTime_(copy.startTime_)
    , endTime_(copy.endTime_)
    , timeFormat_(copy.timeFormat_)
    , textFallbackEnabled_(copy.textFallbackEnabled_)
    , scrollingTextEnabled_(copy.scrollingTextEnabled_)
{
    scrollPoints_ = NULL;
    tweenPoints_ = NULL;

    setPoints(copy.scrollPoints_, copy.tweenPoints_);
}

ScrollingList::~ScrollingList()
{
    stopLoaderThread();
    destroyItems();


    while (scrollPoints_ && !scrollPoints_->empty()) {
        delete scrollPoints_->back();
        scrollPoints_->pop_back();
    }
}


//void ScrollingList::setItems(std::vector<Item*>* items)
//{
//    isInitializing = true;
//    items_ = items;
//    if (items_) {
//        if (!isGrid) {
//            itemIndex_ = loopDecrement(0, selectedOffsetIndex_, items_->size());
//        }
//        else {
//            itemIndex_ = 0;
//            selectedOffsetIndex_ = 0;
//        }
//#ifdef _DEBUG
//        Logger::write(Logger::ZONE_INFO, "ScrollingList", "setItems: itemIndex_=" + std::to_string(itemIndex_) + ", selectedOffsetIndex_=" + std::to_string(selectedOffsetIndex_));
//#endif
//    }
//    isInitializing = false;
//    if (items_) {
//        allocateSpritePoints();
//        updateCache(); // Update cache when items are set
//    }
//}

void ScrollingList::setItems(std::vector<Item*>* items)
{
    isInitializing = true;
    items_ = items;

    if (items_)
    {
        if (!isGrid)
        {
            // Classic RetroFE behaviour: keep the layout's selectedOffsetIndex_
            itemIndex_ = loopDecrement(0, selectedOffsetIndex_, items_->size());
        }
        else
        {
            // Grid only: always start on first page, first cell
            itemIndex_ = 0;
            selectedOffsetIndex_ = 0;
        }

#ifdef _DEBUG
        Logger::write(Logger::ZONE_INFO, "ScrollingList",
            "setItems: itemIndex_=" + std::to_string(itemIndex_) +
            ", selectedOffsetIndex_=" + std::to_string(selectedOffsetIndex_) +
            (isGrid ? " [GRID]" : " [LIST]"));
#endif
    }
    else
    {
        itemIndex_ = 0;
        if (isGrid)
        {
            selectedOffsetIndex_ = 0;
        }
        globalSelectedIndex = 0;
        deallocateSpritePoints();
        clearCache();
    }

    isInitializing = false;

    if (items_ && items_->size() > 0)
    {
        allocateSpritePoints();
        updateCache();

        // Keep multi-grid sync on the same page
        if (isGrid)
        {
            notifySelectionChange();
        }
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
            if (old) {
                delete old;
            }
            continue;
        }

        Item* item = nullptr;
        try {
            item = items_->at(index);
        } catch (...) {
            continue;
        }
        if (!item) continue;
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
        if (c)
        {
            c->allocateGraphicsMemory();
            ViewInfo* view = scrollPoints_->at(i);
            resetTweens(c, tweenPoints_->at(i), view, view, 0);
        }

        if (old && old != c)
        {
            if (c && !newItemSelected)
                c->baseViewInfo = old->baseViewInfo;
            delete old;
        }
        components_.at(i) = c;
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



void ScrollingList::clearCache()
{
    std::lock_guard<std::mutex> lock(cacheMutex_);
    while (!loadQueue_.empty()) {
        loadQueue_.pop();
    }
    for (auto& pair : componentCache_) {
        if (pair.second) {
            pair.second->freeGraphicsMemory();
            delete pair.second;
        }
    }
    componentCache_.clear();
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
    while (!loadQueue_.empty()) {
        loadQueue_.pop();
    }
    for (auto& pair : componentCache_) {
        if (pair.second) {
            pair.second->freeGraphicsMemory();
            delete pair.second;
        }
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

void ScrollingList::setScrollOffsetIndex(unsigned int index)
{
    if (!items_ || items_->size() == 0) return;

    globalSelectedIndex = index;

    if (isGrid && columns > 0)
    {
        int itemsPerPage = columns * (scrollPoints_ ? (scrollPoints_->size() / columns) : 1);

        itemIndex_ = (index / itemsPerPage) * itemsPerPage;
        selectedOffsetIndex_ = index % itemsPerPage;

        if (selectedOffsetIndex_ >= itemsPerPage || selectedOffsetIndex_ >= static_cast<int>(items_->size()))
        {
            selectedOffsetIndex_ = std::min(static_cast<int>(items_->size()) - 1, itemsPerPage - 1);
        }

        allocateSpritePoints();
        updateGridSelection(-1);
    }
    else
    {
        itemIndex_ = loopDecrement(index, selectedOffsetIndex_, items_->size());
        allocateSpritePoints();
    }

    updateCache();
    if (isGrid) 
    { notifySelectionChange(); }
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

    size_t currentSelectedIndex = (itemIndex_ + selectedOffsetIndex_) % items_->size();
    Item* startItem = items_->at(currentSelectedIndex);
    CollectionInfo* coll = startItem->collectionInfo;

    std::string startValue = coll->getSortValue(startItem, currentSortField);

    // Skip empty values
    if (startValue == "zzzzzzzz")
    {
        for (size_t i = 0; i < items_->size(); ++i)
        {
            size_t idx = increment
                ? (currentSelectedIndex + i) % items_->size()
                : (currentSelectedIndex + items_->size() - i) % items_->size();

            std::string val = coll->getSortValue(items_->at(idx), currentSortField);
            if (val != "zzzzzzzz")
            {
                currentSelectedIndex = idx;
                startValue = val;
                startItem = items_->at(idx);
                break;
            }
        }
    }

    size_t newIndex = currentSelectedIndex;

    // === Find next/previous different value ===
    for (size_t i = 0; i < items_->size(); ++i)
    {
        size_t index = increment
            ? loopIncrement(currentSelectedIndex, i, items_->size())
            : loopDecrement(currentSelectedIndex, i, items_->size());

        std::string endValue = coll->getSortValue(items_->at(index), currentSortField);

        bool isDifferent = false;

        // === Compare full value for these fields ===
        if (currentSortField == "year" ||
            currentSortField == "score" ||
            currentSortField == "numberplayers" ||
            currentSortField == "genre")           //  Added genre here
        {
            if (endValue != startValue && endValue != "zzzzzzzz")
                isDifferent = true;
        }
        else
        {
            // First letter comparison (for title, manufacturer, etc.)
            char startLetter = startValue.empty() ? 0 : startValue[0];
            char endLetter = endValue.empty() ? 0 : endValue[0];

            if ((isalpha(startLetter) ^ isalpha(endLetter)) ||
                (isalpha(startLetter) && isalpha(endLetter) && startLetter != endLetter))
            {
                isDifferent = true;
            }
        }

        if (isDifferent)
        {
            newIndex = index;
            break;
        }
    }

    // === Special backward logic (original behavior) ===
    if (!increment)
    {
        bool prevLetterSubToCurrent = false;
        config_.getProperty("prevLetterSubToCurrent", prevLetterSubToCurrent);

        Item* nextItem = items_->at((newIndex + 1) % items_->size());

        if (!prevLetterSubToCurrent || nextItem == startItem)
        {
            std::string newStartValue = coll->getSortValue(items_->at(newIndex), currentSortField);

            for (size_t i = 0; i < items_->size(); ++i)
            {
                size_t index = loopDecrement(newIndex, i, items_->size());
                std::string endValue = coll->getSortValue(items_->at(index), currentSortField);

                bool isDifferent = false;

                if (currentSortField == "year" ||
                    currentSortField == "score" ||
                    currentSortField == "numberplayers" ||
                    currentSortField == "genre")           //  Also here
                {
                    if (endValue != newStartValue && endValue != "zzzzzzzz")
                        isDifferent = true;
                }
                else
                {
                    char startLetter = newStartValue.empty() ? 0 : newStartValue[0];
                    char endLetter = endValue.empty() ? 0 : endValue[0];

                    if ((isalpha(startLetter) ^ isalpha(endLetter)) ||
                        (isalpha(startLetter) && isalpha(endLetter) && startLetter != endLetter))
                    {
                        isDifferent = true;
                    }
                }

                if (isDifferent)
                {
                    newIndex = loopIncrement(index, 1, items_->size());
                    break;
                }
            }
        }
        else
        {
            newIndex = loopIncrement(newIndex, 1, items_->size());
        }
    }

    setScrollOffsetIndex(newIndex);
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



unsigned int ScrollingList::getSelectedIndex()
{
    if (!items_ || items_->empty()) return 0;
    return loopIncrement(itemIndex_, selectedOffsetIndex_, items_->size());
}

void ScrollingList::setSelectedIndex(unsigned int index)
{
    if (!items_ || items_->empty()) return;

    globalSelectedIndex = index;

    if (isGrid && columns > 0)
    {
        int itemsPerPage = columns * (scrollPoints_ ? (scrollPoints_->size() / columns) : 1);
        if (itemsPerPage <= 0) itemsPerPage = 1;

        itemIndex_ = (index / itemsPerPage) * itemsPerPage;
        selectedOffsetIndex_ = index % itemsPerPage;

        if (selectedOffsetIndex_ >= itemsPerPage || selectedOffsetIndex_ >= static_cast<int>(items_->size()))
            selectedOffsetIndex_ = std::min(static_cast<int>(items_->size()) - 1, itemsPerPage - 1);

        allocateSpritePoints();
        updateGridSelection(-1);
    }
    else
    {
        itemIndex_ = loopDecrement(index, selectedOffsetIndex_, items_->size());
        allocateSpritePoints();
    }

    updateCache();
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
    set->push(new Tween(TWEEN_PROPERTY_ANGLE_Y, LINEAR, currentViewInfo->AngleY, nextViewInfo->AngleY, scrollTime));
	set->push(new Tween(TWEEN_PROPERTY_ANGLE_X, LINEAR, currentViewInfo->AngleX, nextViewInfo->AngleX, scrollTime));
	set->push(new Tween(TWEEN_PROPERTY_SLICE3D_X, LINEAR, currentViewInfo->Slice3DX, nextViewInfo->Slice3DX, scrollTime));
	set->push(new Tween(TWEEN_PROPERTY_SLICE3D_Y, LINEAR, currentViewInfo->Slice3DY, nextViewInfo->Slice3DY, scrollTime));
	set->push(new Tween(TWEEN_PROPERTY_WRAP_LEFT_X, LINEAR, currentViewInfo->WrapLX, nextViewInfo->WrapLX, scrollTime));
	set->push(new Tween(TWEEN_PROPERTY_WRAP_LEFT_Y, LINEAR, currentViewInfo->WrapLY, nextViewInfo->WrapLY, scrollTime));
	set->push(new Tween(TWEEN_PROPERTY_WRAP_RIGHT_X, LINEAR, currentViewInfo->WrapRX, nextViewInfo->WrapRX, scrollTime));
	set->push(new Tween(TWEEN_PROPERTY_WRAP_RIGHT_Y, LINEAR, currentViewInfo->WrapRY, nextViewInfo->WrapRY, scrollTime));
	set->push(new Tween(TWEEN_PROPERTY_WRAP_TOP_X, LINEAR, currentViewInfo->WrapTX, nextViewInfo->WrapTX, scrollTime));
	set->push(new Tween(TWEEN_PROPERTY_WRAP_TOP_Y, LINEAR, currentViewInfo->WrapTY, nextViewInfo->WrapTY, scrollTime));
	set->push(new Tween(TWEEN_PROPERTY_WRAP_BOTTOM_X, LINEAR, currentViewInfo->WrapBX, nextViewInfo->WrapBX, scrollTime));
	set->push(new Tween(TWEEN_PROPERTY_WRAP_BOTTOM_Y, LINEAR, currentViewInfo->WrapBY, nextViewInfo->WrapBY, scrollTime));
    
    scrollTween->Push(set);
}

bool ScrollingList::allocateTexture(unsigned int index, Item* item)
{
    static const size_t MAX_COMPONENTS = 1000; // Arbitrary safe limit
    static const size_t MAX_PATH_LENGTH = 1024; // Limit for file paths

    if (!item) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_ERROR, "ScrollingList", "Null item provided for index: " + std::to_string(index));
#endif
        return false;
    }

    if (index >= components_.size()) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_ERROR, "ScrollingList", "Invalid index: " + std::to_string(index) + ", components_.size(): " + std::to_string(components_.size()));
#endif
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
    std::string collectionName = (item->leaf && item->collectionInfo) ? item->collectionInfo->name : item->name;

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
            std::vector<std::string>{ "png", "jpg", "jpeg", "webp", "jxl" };
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
#ifdef _DEBUG
    Logger::write(Logger::ZONE_INFO, "ScrollingList",
        "DEBUG -> type='" + imageType_ +
        "' | manufacturer='" + item->manufacturer +
        "' | name='" + item->name + "'");
#endif

    std::string basename = "";

    // === Get correct metadata value (same as ReloadableMedia) ===
    if (typeLC == "manufacturer")
        basename = item->manufacturer;
    else if (typeLC == "developer")
        basename = item->developer.empty() ? item->manufacturer : item->developer;
    else if (typeLC == "genre")
        basename = item->genre;
    else if (typeLC == "year")
        basename = item->year;
    else if (typeLC == "title")
        basename = item->title.empty() ? item->fullTitle : item->title;
    else if (typeLC == "name")
        basename = item->name;
    else if (typeLC == "numberplayers")
        basename = item->numberPlayers;
    else if (typeLC == "numberbuttons")
        basename = item->numberButtons;
    else if (typeLC == "rating")
        basename = item->rating;
    else if (typeLC == "score")
        basename = item->score;
    else if (typeLC == "numberplayers")
        basename = item->numberPlayers;
    
    // === NEW JUKEBOX FIELDS ===
    else if (typeLC == "artist")
        basename = item->artist;
    else if (typeLC == "album")
        basename = item->album;
    else if (typeLC == "tracknumber" || typeLC == "track")
        basename = item->trackNumber;
    else if (typeLC == "discnumber" || typeLC == "disc")
        basename = item->discNumber;
    // Strong fallback using getInfo()
    if (basename.empty())
    {
        std::string tmp;
        if (item->getInfo(typeLC, tmp) && !tmp.empty())
            basename = tmp;
    }

    if (!basename.empty())
        names.push_back(basename);
    else
    {
        names.push_back(item->name);
        if (!item->fullTitle.empty())
            names.push_back(item->fullTitle);
    }

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

        if (!t && names[n] != "default")
        {
            std::string targetCol, targetBase;
            std::string mediaType = (videoType_ != "null") ? videoType_ : imageType_;
            if (MediaAlias::resolveAlias(collectionName, mediaType, names[n], targetCol, targetBase) ||
                (item->collectionInfo && MediaAlias::resolveAlias(item->collectionInfo->name, mediaType, names[n], targetCol, targetBase)))
            {
                std::string aliasImgPath, aliasVidPath;
                if (layoutMode_)
                {
                    if (commonMode_)
                        aliasImgPath = Utils::combinePath(Configuration::absolutePath, "layouts", layoutName, "collections", "_common");
                    else
                        aliasImgPath = Utils::combinePath(Configuration::absolutePath, "layouts", layoutName, "collections", targetCol);
                    aliasImgPath = Utils::combinePath(aliasImgPath, "medium_artwork", imageType_);
                    aliasVidPath = Utils::combinePath(aliasImgPath, "medium_artwork", videoType_);
                }
                else
                {
                    config_.getMediaPropertyAbsolutePath(targetCol, imageType_, false, aliasImgPath);
                    config_.getMediaPropertyAbsolutePath(targetCol, videoType_, false, aliasVidPath);
                }
                if (videoType_ != "null")
                {
                    t = videoBuild.createVideo(aliasVidPath, page, targetBase, baseViewInfo.Monitor, false);
                }
                else
                {
                    t = imageBuild.CreateImage(aliasImgPath, page, targetBase, baseViewInfo.Monitor);
                    if (!t)
                    {
                        t = animatedimageBuild.CreateImage(aliasImgPath, page, targetBase, baseViewInfo.Monitor);
                    }
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
        // Get the ViewInfo defined by the user in the layout (from itemDefaults or per-item)
        if (textFallbackEnabled_)
        {
            ViewInfo targetView = baseViewInfo;
            if (!scrollPoints_->empty())
            {
                size_t viewIndex = index % scrollPoints_->size();
                targetView = *(scrollPoints_->at(viewIndex));
            }

            if (scrollingTextEnabled_ && !textType_.empty())
            {
                // === Preferred: Use ReloadableScrollingText ===
                ReloadableScrollingText* scrollingText = new ReloadableScrollingText(
                    config_,
                    false,           // systemMode
                    layoutMode_,     // layoutMode
                    true,            // menuMode
                    textType_,
                    textFormat_,
                    singlePrefix_,
                    singlePostfix_,
                    pluralPrefix_,
                    pluralPostfix_,
                    alignment_,
                    timeFormat_,
                    page,
                    0,
                    fontInst_,
                    direction_,
                    scrollingSpeed_,
                    startPosition_,
                    startTime_,
                    endTime_
                );

                scrollingText->baseViewInfo = targetView;
                scrollingText->allocateGraphicsMemory();
                t = scrollingText;

#ifdef _DEBUG
                Logger::write(Logger::ZONE_INFO, "ScrollingList",
                    "Created ReloadableScrollingText (textType=" + textType_ +
                    ", dir=" + direction_ + ", speed=" + std::to_string(scrollingSpeed_) + ")");
#endif
            }
            else
            {
                // === Normal Text fallback ===
                std::string displayText = basename.empty()
                    ? (item->title.empty() ? item->name : item->title)
                    : basename;

                Text* textComp = new Text(displayText, page, fontInst_, baseViewInfo.Monitor);
                textComp->baseViewInfo = targetView;
                textComp->allocateGraphicsMemory();
                t = textComp;
            }
        }
    }
    components_.at(index) = t;
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
    if (components_.empty() || !items_ || items_->empty()) return;

    // Check if there is at least one component on this layer before doing any work
    bool hasComponentOnLayer = false;
    for (const auto* c : components_) {
        if (c && c->baseViewInfo.Layer == layer) {
            hasComponentOnLayer = true;
            break;
        }
    }
    if (!hasComponentOnLayer) return;

    unsigned int selectedIndex = getSelectedIndex();
    SDL_Renderer* renderer = SDL::getRenderer(baseViewInfo.Monitor);

    // Calculate layout scaling factors once for all items on this layer
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

    // First pass: Draw all non-selected items
    for (unsigned int i = 0; i < components_.size(); ++i)
    {
        Component* c = components_.at(i);
        if (!c || c->baseViewInfo.Layer != layer) continue;

        bool isSelected = isGrid ? (i == static_cast<unsigned int>(selectedOffsetIndex_)) : false;
        if (isSelected) continue; // Skip the selected item for the second pass


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

        bool isSelected = isGrid ? (i == static_cast<unsigned int>(selectedOffsetIndex_)) : false;
        if (!isSelected) continue;


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
        if (components_.size() > 0 && components_.at(0)) {
            components_.at(0)->freeGraphicsMemory();
            delete components_.at(0);
            components_.at(0) = nullptr;
        }
        allocateTexture(0, i);
    }
    else {
        unsigned int newIndex = loopDecrement(itemIndex_, 1, items_->size());
        Item* i = items_->at(newIndex);
        itemIndex_ = loopDecrement(itemIndex_, 1, items_->size());
        unsigned int lastIndex = components_.size() - 1;
        if (components_.size() > lastIndex && components_.at(lastIndex)) {
            components_.at(lastIndex)->freeGraphicsMemory();
            delete components_.at(lastIndex);
            components_.at(lastIndex) = nullptr;
        }
        allocateTexture(lastIndex, i);
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
    if(isGrid){ notifySelectionChange(); }
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
    if (isGrid) { notifySelectionChange(); }
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
    if (isGrid) { notifySelectionChange(); }
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
    if (isGrid) { notifySelectionChange(); }
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
    if (isGrid) { notifySelectionChange(); }
}
//TODO make it beter

void ScrollingList::updateGridSelection(int /*oldSelectedOffsetIndex*/)
{
    if (!scrollPoints_ || !tweenPoints_ || components_.empty()) return;

    for (unsigned int i = 0; i < scrollPoints_->size(); ++i)
    {
        Component* c = components_.at(i);
        if (!c) continue;

        ViewInfo* target = scrollPoints_->at(i);
        c->baseViewInfo = *target;                    // reset base

        if (AnimationEvents* tweens = tweenPoints_->at(i))
        {
            resetTweens(c, tweens, &c->baseViewInfo, target, scrollPeriod_);
        }

        c->triggerEvent("menuScroll");

        bool isSelected = (i == selectedOffsetIndex_);
        if (AnimatedImage* anim = dynamic_cast<AnimatedImage*>(c))
            anim->setSelected(isSelected);
    }
    page.menuScroll();
    updateCache();
}


void ScrollingList::updateCache()
{
    if (!loaderThread_.joinable()) return;
    if (!items_ || items_->empty()) return;
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
    if (!item) return;

    Component* t = nullptr;
    std::string typeLC = Utils::toLower(imageType_);
#ifdef _DEBUG
    Logger::write(Logger::ZONE_INFO, "ScrollingList",
        "loadComponent DEBUG -> type='" + imageType_ +
        "' | manufacturer='" + item->manufacturer +
        "' | name='" + item->name + "'");
#endif
    // === Get metadata basename (same logic as allocateTexture) ===
    std::string basename = "";
    if (typeLC == "manufacturer")
        basename = item->manufacturer;
    else if (typeLC == "developer")
        basename = item->developer.empty() ? item->manufacturer : item->developer;
    else if (typeLC == "genre")
        basename = item->genre;
    else if (typeLC == "year")
        basename = item->year;
    else if (typeLC == "title")
        basename = item->title.empty() ? item->fullTitle : item->title;
    else if (typeLC == "name")
        basename = item->name;
       
    else if (typeLC == "numberplayers")
        basename = item->numberPlayers;
    else if (typeLC == "numberbuttons")
        basename = item->numberButtons;
    else if (typeLC == "rating")
        basename = item->rating;
    else if (typeLC == "score")
        basename = item->score;
    // jukebox types
    else if (typeLC == "artist")
		basename = item->artist;
    else if (typeLC == "album")
		basename = item->album;
	else if (typeLC == "trackNumber")
		basename = item->trackNumber;
	else if (typeLC == "discNumber")
		basename = item->discNumber;
    // Fallback using getInfo() like ReloadableMedia
    if (basename.empty())
    {
        std::string tmp;
        if (item->getInfo(typeLC, tmp) && !tmp.empty())
            basename = tmp;
    }

    // Final fallback
    if (basename.empty())
        basename = item->name;

    // Build image path using metadata name
    std::string imagePath = Utils::combinePath(Configuration::absolutePath,
        "collections", item->collectionInfo->name, "medium_artwork", imageType_);

    ImageBuilder imageBuild;
    AnimatedImageBuilder animatedBuild;

    // Try to load image using metadata basename
    t = imageBuild.CreateImage(imagePath, page, basename, baseViewInfo.Monitor);
    if (!t)
        t = animatedBuild.CreateImage(imagePath, page, basename, baseViewInfo.Monitor);

    if (!t)
    {
        std::string targetCol, targetBase;
        if (MediaAlias::resolveAlias(item->collectionInfo->name, imageType_, basename, targetCol, targetBase))
        {
            std::string aliasPath = Utils::combinePath(Configuration::absolutePath,
                "collections", targetCol, "medium_artwork", imageType_);
            t = imageBuild.CreateImage(aliasPath, page, targetBase, baseViewInfo.Monitor);
            if (!t)
                t = animatedBuild.CreateImage(aliasPath, page, targetBase, baseViewInfo.Monitor);
        }
    }

    // Fallback to text using metadata (this is what you wanted)
    if (!t)
    {
        std::string displayText = basename;
        if (displayText.empty() || displayText == item->name)
            displayText = item->title.empty() ? item->name : item->title;

        ViewInfo targetView = baseViewInfo;
        if (!scrollPoints_->empty())
        {
            size_t viewIndex = itemIndex % scrollPoints_->size();   // approximate position
            targetView = *(scrollPoints_->at(viewIndex));
        }

        Text* textComp = new Text(displayText, page, fontInst_, baseViewInfo.Monitor);
        t = textComp;

        textComp->baseViewInfo = targetView;
        textComp->allocateGraphicsMemory();
    }

    if (t)
    {
        t->allocateGraphicsMemory();
        std::lock_guard<std::mutex> lock(cacheMutex_);
        auto it = componentCache_.find(itemIndex);
        if (it != componentCache_.end() && it->second) {
            it->second->freeGraphicsMemory();
            delete it->second;
        }
        componentCache_[itemIndex] = t;
    }
}
void ScrollingList::enqueueLoad(size_t itemIndex)
{
    if (!loaderThread_.joinable()) return;
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

unsigned int ScrollingList::getAbsoluteSelectedIndex() const
{
    if (!items_ || items_->size() == 0)
        return 0;

    return itemIndex_ + selectedOffsetIndex_;
}

void ScrollingList::notifySelectionChange()
{
    static bool notifying = false;
    if (notifying) return;
    notifying = true;

    unsigned int absIndex = getAbsoluteSelectedIndex();
    globalSelectedIndex = absIndex;

    const std::vector<ScrollingList*>& active = page.getActiveMenus();
    for (auto* list : active)
    {
        if (list && list != this && list->items_ == this->items_)
        {
            list->setSelectedIndex(absIndex);   // this will update sibling grids
        }
    }

    notifying = false;
}
void ScrollingList::setSortField(const std::string& field)
{
    currentSortField = field.empty() ? "title" : field;
}
