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

#include "Page.h"
#include "ComponentItemBinding.h"
#include "Component/Component.h"
#include "../Collection/CollectionInfo.h"
#include "Component/Text.h"
#include "../Utility/Log.h"
#include "Component/ScrollingList.h"
#include "../Sound/Sound.h"
#include "ComponentItemBindingBuilder.h"
#include "PageBuilder.h"
#include <algorithm>
#include <sstream>



Page::Page(Configuration& config, int layoutWidth, int layoutHeight)
    : config_(config)
    , menuDepth_(0)
    , scrollActive_(false)
    , selectedItem_(NULL)
    , textStatusComponent_(NULL)
    , minShowTime_(0)
    , jukebox_(false)
    , libVLCInstance(NULL)
    , layoutScaleMode_(ViewInfo::ScaleMode::Stretch) // Default to Stretch
    , layoutScaleFactor_(1.0f)
{
    int numScreens = SDL::getNumScreens();
    layoutWidth_.resize(numScreens, layoutWidth);
    layoutHeight_.resize(numScreens, layoutHeight);
    scaleX_.resize(numScreens);
    scaleY_.resize(numScreens);
    offsetX_.resize(numScreens);
    offsetY_.resize(numScreens);
    calculateScalingFactors(); // Initial calculation
}


Page::~Page()
{
}


void Page::deInitialize()
{

    Logger::write(Logger::ZONE_INFO, "Page", "Deinitializing Page, libVLCInstance: ");

    //if (VideoFactory::isEnabled()) {
    //    Logger::write(Logger::ZONE_INFO, "Page", "Stopping LibVLCVideo");
    //    libVLCInstance.stop(); // Test with just this first
    //}

    cleanup();

    // Delete elements in menus_
    for (auto it = menus_.begin(); it != menus_.end(); ++it) {
        for (auto it2 = it->begin(); it2 != it->end(); ++it2) {
            delete* it2;
        }
    }
    menus_.clear();

    // Delete elements in LayerComponents
    for (auto it = LayerComponents.begin(); it != LayerComponents.end(); ++it) {
        delete* it;
    }
    LayerComponents.clear();

    // Delete elements in collections_
    for (auto it = collections_.begin(); it != collections_.end(); ++it) {
        if (it->collection) {
            it->collection->Save();
            delete it->collection;
        }
    }
    collections_.clear();

    for (auto& pair : soundsByType_) {
        for (Sound* sound : pair.second) {
            delete sound;
        }
    }
    soundsByType_.clear();

}


bool Page::isMenusFull()
{
    return (menuDepth_ > menus_.size());
}

void Page::addSound(const std::string& type, Sound* sound) {
    soundsByType_[type].push_back(sound);
}




void Page::onNewItemSelected()
{
    if (!(activeMenu_.size() > 0 && activeMenu_[0])) return;
    selectedItem_ = activeMenu_[0]->getSelectedItem();

    for (MenuVector_T::iterator it = menus_.begin(); it != menus_.end(); it++)
    {
        for (std::vector<ScrollingList*>::iterator it2 = menus_[std::distance(menus_.begin(), it)].begin(); it2 != menus_[std::distance(menus_.begin(), it)].end(); it2++)
        {
            ScrollingList* menu = *it2;
            if (menu) menu->setNewItemSelected();
        }
    }

    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->setNewItemSelected();
    }

}


void Page::onNewScrollItemSelected()
{
    if (!(activeMenu_.size() > 0 && activeMenu_[0])) return;
    selectedItem_ = activeMenu_[0]->getSelectedItem();

    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->setNewScrollItemSelected();
    }

}


void Page::highlightLoadArt()
{
    if (!(activeMenu_.size() > 0 && activeMenu_[0])) return;
    selectedItem_ = activeMenu_[0]->getSelectedItem();

    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->setNewItemSelected();
    }

}


void Page::pushMenu(ScrollingList* s, int index)
{
    if (index < 0 || index >= static_cast<int>(menus_.size()))
    {
        index = static_cast<int>(menus_.size());
        menus_.resize(index + 1);
    }

    menus_[index].push_back(s);

    if (s->active)
    {
        activeGroups.insert(s->group);
        activeSubgroups.insert(s->subgroup);
        for (const std::string& shared : s->shared)
        {
            std::string effectiveShared = shared;
            if (!effectiveShared.empty() && (!s->name.empty() || !s->group.empty() || !s->subgroup.empty())) effectiveShared += "/";
            if (!s->name.empty()) {
                effectiveShared += s->name;
                if (!s->group.empty() || !s->subgroup.empty()) effectiveShared += "/";
            }
            if (!s->group.empty()) {
                effectiveShared += s->group;
                if (!s->subgroup.empty()) effectiveShared += "/";
            }
            if (!s->subgroup.empty()) effectiveShared += s->subgroup;
            activeShared_.insert(effectiveShared);
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "Page", "Activated menu shared key: " + effectiveShared + " for menu: " + s->name + "/" + s->group + "/" + s->subgroup);
#endif
        }
#ifdef _DEBUG
        std::string sharedLog = s->shared.empty() ? "none" : "";
        for (size_t i = 0; i < s->shared.size(); ++i)
        {
            sharedLog += s->shared[i];
            if (i < s->shared.size() - 1) sharedLog += ",";
        }
        Logger::write(Logger::ZONE_DEBUG, "Page", "Added active menu: " + s->name + "/" + s->group + "/" + s->subgroup + " shared=" + sharedLog);
#endif
    }
    else
    {
        // Ensure invisible menus start with alpha=0
        s->baseViewInfo.Alpha = 0.0f;
#ifdef _DEBUG
        std::string sharedLog = s->shared.empty() ? "none" : "";
        for (size_t i = 0; i < s->shared.size(); ++i)
        {
            sharedLog += s->shared[i];
            if (i < s->shared.size() - 1) sharedLog += ",";
        }
        Logger::write(Logger::ZONE_DEBUG, "Page", "Added inactive menu with alpha=0: " + s->name + "/" + s->group + "/" + s->subgroup + " shared=" + sharedLog);
#endif
    }
}


unsigned int Page::getMenuDepth()
{
    return menuDepth_;
}


void Page::setStatusTextComponent(Text* t)
{
    textStatusComponent_ = t;
}


bool Page::addComponent(Component* c)
{
    bool retVal = false;
    if (c->baseViewInfo.Layer < NUM_LAYERS)
    {
        LayerComponents.push_back(c);
        if (c->active) {
            activeGroups.insert(c->group);
            activeSubgroups.insert(c->subgroup);
            for (const std::string& shared : c->shared) {
                std::string effectiveShared = shared;
                if (!effectiveShared.empty() && (!c->name.empty() || !c->group.empty() || !c->subgroup.empty())) effectiveShared += "/";
                if (!c->name.empty()) {
                    effectiveShared += c->name;
                    if (!c->group.empty() || !c->subgroup.empty()) effectiveShared += "/";
                }
                if (!c->group.empty()) {
                    effectiveShared += c->group;
                    if (!c->subgroup.empty()) effectiveShared += "/";
                }
                if (!c->subgroup.empty()) effectiveShared += c->subgroup;
                activeShared_.insert(effectiveShared);
#ifdef _DEBUG
                Logger::write(Logger::ZONE_DEBUG, "Component", "Added active shared key: " + effectiveShared + " for component: " + c->name + "/" + c->group + "/" + c->subgroup);
#endif
            }
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "Component", "Added active component: " + c->name + "/" + c->group + "/" + c->subgroup + " shared=" + (c->shared.empty() ? "none" : c->shared[0]));
#endif
        }
        else {
            // Ensure invisible components start with alpha=0
            c->baseViewInfo.Alpha = 0.0f;
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "Component", "Added inactive component with alpha=0: " + c->name + "/" + c->group + "/" + c->subgroup + " shared=" + (c->shared.empty() ? "none" : c->shared[0]));
#endif
        }
        retVal = true;
    }
    else
    {
        std::stringstream ss;
        ss << "Component layer too large Layer: " << c->baseViewInfo.Layer;
        Logger::write(Logger::ZONE_ERROR, "Page", ss.str());
    }
    return retVal;
}


bool Page::isMenuIdle()
{
    bool idle = true;

    for (MenuVector_T::iterator it = menus_.begin(); it != menus_.end(); it++)
    {
        for (std::vector<ScrollingList*>::iterator it2 = menus_[std::distance(menus_.begin(), it)].begin(); it2 != menus_[std::distance(menus_.begin(), it)].end(); it2++)
        {
            ScrollingList* menu = *it2;

            if (!menu->isIdle())
            {
                idle = false;
                break;
            }
        }
    }
    return idle;
}


bool Page::isIdle()
{
    bool idle = isMenuIdle();

    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end() && idle; ++it)
    {
        idle = (*it)->isIdle();
    }

    return idle;
}


bool Page::isAttractIdle()
{
    bool idle = true;

    for (MenuVector_T::iterator it = menus_.begin(); it != menus_.end(); it++)
    {
        for (std::vector<ScrollingList*>::iterator it2 = menus_[std::distance(menus_.begin(), it)].begin(); it2 != menus_[std::distance(menus_.begin(), it)].end(); it2++)
        {
            ScrollingList* menu = *it2;

            if (!menu->isAttractIdle())
            {
                idle = false;
                break;
            }
        }
    }

    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end() && idle; ++it)
    {
        idle = (*it)->isAttractIdle();
    }

    return idle;
}


bool Page::isGraphicsIdle()
{
    bool idle = true;

    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end() && idle; ++it)
    {
        idle = (*it)->isIdle();
    }

    return idle;
}


void Page::start()
{
    for (auto it = menus_.begin(); it != menus_.end(); it++) {
        for (auto it2 = it->begin(); it2 != it->end(); ++it2) {
            (*it2)->triggerEvent("enter");
            (*it2)->triggerEnterEvent();
        }
    }
    playSound("load"); // Updated to use playSound

    for (auto it = LayerComponents.begin(); it != LayerComponents.end(); ++it) {
        (*it)->triggerEvent("enter");
    }
}


void Page::stop()
{
    for (auto it = menus_.begin(); it != menus_.end(); it++) {
        for (auto it2 = it->begin(); it2 != it->end(); ++it2) {
            (*it2)->triggerEvent("exit");
            (*it2)->triggerExitEvent();
        }
    }
    playSound("unload"); // Updated to use playSound

    for (auto it = LayerComponents.begin(); it != LayerComponents.end(); ++it) {
        (*it)->triggerEvent("exit");
    }
}


Item* Page::getSelectedItem()
{
    return selectedItem_;
}


Item* Page::getSelectedItem(int offset)
{
    if (!(activeMenu_.size() > 0 && activeMenu_[0])) return NULL;
    return activeMenu_[0]->getItemByOffset(offset);
}


void Page::removeSelectedItem()
{
    /*
    //todo: change method to RemoveItem() and pass in SelectedItem
    if(Menu)
    {
        Menu->RemoveSelectedItem();
    }
    */
    selectedItem_ = NULL;

}


void Page::setScrollOffsetIndex(unsigned int i)
{
    if (!(activeMenu_.size() > 0 && activeMenu_[0])) return;
    for (std::vector<ScrollingList*>::iterator it = activeMenu_.begin(); it != activeMenu_.end(); it++)
    {
        ScrollingList* menu = *it;
        menu->setScrollOffsetIndex(i);
    }
}


unsigned int Page::getScrollOffsetIndex()
{
    if (!(activeMenu_.size() > 0 && activeMenu_[0])) return -1;
    return activeMenu_[0]->getScrollOffsetIndex();
}


void Page::setMinShowTime(float value)
{
    minShowTime_ = value;
}


float Page::getMinShowTime()
{
    return minShowTime_;
}


void Page::playlistChange()
{
    for (std::vector<ScrollingList*>::iterator it = activeMenu_.begin(); it != activeMenu_.end(); it++)
    {
        ScrollingList* menu = *it;
        if (menu) menu->setPlaylist(playlist_->first);
    }

    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->setPlaylist(playlist_->first);
    }
}


void Page::menuScroll()
{
    Item* item = selectedItem_;

    if (!item) return;

    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->triggerEvent("menuScroll", menuDepth_ - 1);
    }
}


void Page::highlightEnter()
{
    Item* item = selectedItem_;

    if (!item) return;
    for (MenuVector_T::iterator it = menus_.begin(); it != menus_.end(); it++)
    {
        for (std::vector<ScrollingList*>::iterator it2 = menus_[std::distance(menus_.begin(), it)].begin(); it2 != menus_[std::distance(menus_.begin(), it)].end(); it2++)
        {
            ScrollingList* menu = *it2;
            if (menuDepth_ - 1 == static_cast<unsigned int>(distance(menus_.begin(), it)))
            {
                // Also trigger animations for index i for active menu
                menu->triggerEvent("highlightEnter", MENU_INDEX_HIGH + menuDepth_ - 1);
                menu->triggerHighlightEnterEvent(MENU_INDEX_HIGH + menuDepth_ - 1);
            }
            else
            {
                menu->triggerEvent("highlightEnter", menuDepth_ - 1);
                menu->triggerHighlightEnterEvent(menuDepth_ - 1);
            }
        }
    }

    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->triggerEvent("highlightEnter", menuDepth_ - 1);
    }
}


void Page::highlightExit()
{
    Item* item = selectedItem_;

    if (!item) return;
    for (MenuVector_T::iterator it = menus_.begin(); it != menus_.end(); it++)
    {
        for (std::vector<ScrollingList*>::iterator it2 = menus_[std::distance(menus_.begin(), it)].begin(); it2 != menus_[std::distance(menus_.begin(), it)].end(); it2++)
        {
            ScrollingList* menu = *it2;
            if (menuDepth_ - 1 == static_cast<unsigned int>(distance(menus_.begin(), it)))
            {
                // Also trigger animations for index i for active menu
                menu->triggerEvent("highlightExit", MENU_INDEX_HIGH + menuDepth_ - 1);
                menu->triggerHighlightExitEvent(MENU_INDEX_HIGH + menuDepth_ - 1);
            }
            else
            {
                menu->triggerEvent("highlightExit", menuDepth_ - 1);
                menu->triggerHighlightExitEvent(menuDepth_ - 1);
            }
        }
    }

    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->triggerEvent("highlightExit", menuDepth_ - 1);
    }
}


void Page::playlistEnter()
{
    Item* item = selectedItem_;

    if (!item) return;
    for (MenuVector_T::iterator it = menus_.begin(); it != menus_.end(); it++)
    {
        for (std::vector<ScrollingList*>::iterator it2 = menus_[std::distance(menus_.begin(), it)].begin(); it2 != menus_[std::distance(menus_.begin(), it)].end(); it2++)
        {
            ScrollingList* menu = *it2;
            if (menuDepth_ - 1 == static_cast<unsigned int>(distance(menus_.begin(), it)))
            {
                // Also trigger animations for index i for active menu
                menu->triggerEvent("playlistEnter", MENU_INDEX_HIGH + menuDepth_ - 1);
                menu->triggerPlaylistEnterEvent(MENU_INDEX_HIGH + menuDepth_ - 1);
            }
            else
            {
                menu->triggerEvent("playlistEnter", menuDepth_ - 1);
                menu->triggerPlaylistEnterEvent(menuDepth_ - 1);
            }
        }
    }

    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->triggerEvent("playlistEnter", menuDepth_ - 1);
    }
}


void Page::playlistExit()
{
    Item* item = selectedItem_;

    if (!item) return;
    for (MenuVector_T::iterator it = menus_.begin(); it != menus_.end(); it++)
    {
        for (std::vector<ScrollingList*>::iterator it2 = menus_[std::distance(menus_.begin(), it)].begin(); it2 != menus_[std::distance(menus_.begin(), it)].end(); it2++)
        {
            ScrollingList* menu = *it2;
            if (menuDepth_ - 1 == static_cast<unsigned int>(distance(menus_.begin(), it)))
            {
                // Also trigger animations for index i for active menu
                menu->triggerEvent("playlistExit", MENU_INDEX_HIGH + menuDepth_ - 1);
                menu->triggerPlaylistExitEvent(MENU_INDEX_HIGH + menuDepth_ - 1);
            }
            else
            {
                menu->triggerEvent("playlistExit", menuDepth_ - 1);
                menu->triggerPlaylistExitEvent(menuDepth_ - 1);
            }
        }
    }

    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->triggerEvent("playlistExit", menuDepth_ - 1);
    }
}


void Page::menuJumpEnter()
{
    Item* item = selectedItem_;

    if (!item) return;
    for (MenuVector_T::iterator it = menus_.begin(); it != menus_.end(); it++)
    {
        for (std::vector<ScrollingList*>::iterator it2 = menus_[std::distance(menus_.begin(), it)].begin(); it2 != menus_[std::distance(menus_.begin(), it)].end(); it2++)
        {
            ScrollingList* menu = *it2;
            if (menuDepth_ - 1 == static_cast<unsigned int>(distance(menus_.begin(), it)))
            {
                // Also trigger animations for index i for active menu
                menu->triggerEvent("menuJumpEnter", MENU_INDEX_HIGH + menuDepth_ - 1);
                menu->triggerMenuJumpEnterEvent(MENU_INDEX_HIGH + menuDepth_ - 1);
            }
            else
            {
                menu->triggerEvent("menuJumpEnter", menuDepth_ - 1);
                menu->triggerMenuJumpEnterEvent(menuDepth_ - 1);
            }
        }
    }

    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->triggerEvent("menuJumpEnter", menuDepth_ - 1);
    }
}


void Page::menuJumpExit()
{
    Item* item = selectedItem_;

    if (!item) return;
    for (MenuVector_T::iterator it = menus_.begin(); it != menus_.end(); it++)
    {
        for (std::vector<ScrollingList*>::iterator it2 = menus_[std::distance(menus_.begin(), it)].begin(); it2 != menus_[std::distance(menus_.begin(), it)].end(); it2++)
        {
            ScrollingList* menu = *it2;
            if (menuDepth_ - 1 == static_cast<unsigned int>(distance(menus_.begin(), it)))
            {
                // Also trigger animations for index i for active menu
                menu->triggerEvent("menuJumpExit", MENU_INDEX_HIGH + menuDepth_ - 1);
                menu->triggerMenuJumpExitEvent(MENU_INDEX_HIGH + menuDepth_ - 1);
            }
            else
            {
                menu->triggerEvent("menuJumpExit", menuDepth_ - 1);
                menu->triggerMenuJumpExitEvent(menuDepth_ - 1);
            }
        }
    }

    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->triggerEvent("menuJumpExit", menuDepth_ - 1);
    }
}


void Page::attractEnter()
{
    Item* item = selectedItem_;

    if (!item) return;
    for (MenuVector_T::iterator it = menus_.begin(); it != menus_.end(); it++)
    {
        for (std::vector<ScrollingList*>::iterator it2 = menus_[std::distance(menus_.begin(), it)].begin(); it2 != menus_[std::distance(menus_.begin(), it)].end(); it2++)
        {
            ScrollingList* menu = *it2;
            if (menuDepth_ - 1 == static_cast<unsigned int>(distance(menus_.begin(), it)))
            {
                // Also trigger animations for index i for active menu
                menu->triggerEvent("attractEnter", MENU_INDEX_HIGH + menuDepth_ - 1);
                menu->triggerAttractEnterEvent(MENU_INDEX_HIGH + menuDepth_ - 1);
            }
            else
            {
                menu->triggerEvent("attractEnter", menuDepth_ - 1);
                menu->triggerAttractEnterEvent(menuDepth_ - 1);
            }
        }
    }

    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->triggerEvent("attractEnter", menuDepth_ - 1);
    }
}


void Page::attract()
{
    Item* item = selectedItem_;

    if (!item) return;
    for (MenuVector_T::iterator it = menus_.begin(); it != menus_.end(); it++)
    {
        for (std::vector<ScrollingList*>::iterator it2 = menus_[std::distance(menus_.begin(), it)].begin(); it2 != menus_[std::distance(menus_.begin(), it)].end(); it2++)
        {
            ScrollingList* menu = *it2;
            if (menuDepth_ - 1 == static_cast<unsigned int>(distance(menus_.begin(), it)))
            {
                // Also trigger animations for index i for active menu
                menu->triggerEvent("attract", MENU_INDEX_HIGH + menuDepth_ - 1);
                menu->triggerAttractEvent(MENU_INDEX_HIGH + menuDepth_ - 1);
            }
            else
            {
                menu->triggerEvent("attract", menuDepth_ - 1);
                menu->triggerAttractEvent(menuDepth_ - 1);
            }
        }
    }

    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->triggerEvent("attract", menuDepth_ - 1);
    }
}


void Page::attractExit()
{
    Item* item = selectedItem_;

    if (!item) return;
    for (MenuVector_T::iterator it = menus_.begin(); it != menus_.end(); it++)
    {
        for (std::vector<ScrollingList*>::iterator it2 = menus_[std::distance(menus_.begin(), it)].begin(); it2 != menus_[std::distance(menus_.begin(), it)].end(); it2++)
        {
            ScrollingList* menu = *it2;
            if (menuDepth_ - 1 == static_cast<unsigned int>(distance(menus_.begin(), it)))
            {
                // Also trigger animations for index i for active menu
                menu->triggerEvent("attractExit", MENU_INDEX_HIGH + menuDepth_ - 1);
                menu->triggerAttractExitEvent(MENU_INDEX_HIGH + menuDepth_ - 1);
            }
            else
            {
                menu->triggerEvent("attractExit", menuDepth_ - 1);
                menu->triggerAttractExitEvent(menuDepth_ - 1);
            }
        }
    }

    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->triggerEvent("attractExit", menuDepth_ - 1);
    }
}


void Page::jukeboxJump()
{
    Item* item = selectedItem_;

    if (!item) return;
    for (MenuVector_T::iterator it = menus_.begin(); it != menus_.end(); it++)
    {
        for (std::vector<ScrollingList*>::iterator it2 = menus_[std::distance(menus_.begin(), it)].begin(); it2 != menus_[std::distance(menus_.begin(), it)].end(); it2++)
        {
            ScrollingList* menu = *it2;
            if (menuDepth_ - 1 == static_cast<unsigned int>(distance(menus_.begin(), it)))
            {
                // Also trigger animations for index i for active menu
                menu->triggerEvent("jukeboxJump", MENU_INDEX_HIGH + menuDepth_ - 1);
                menu->triggerJukeboxJumpEvent(MENU_INDEX_HIGH + menuDepth_ - 1);
            }
            else
            {
                menu->triggerEvent("jukeboxJump", menuDepth_ - 1);
                menu->triggerJukeboxJumpEvent(menuDepth_ - 1);
            }
        }
    }

    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->triggerEvent("jukeboxJump", menuDepth_ - 1);
    }
}


void Page::triggerEvent(std::string action)
{
    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->triggerEvent(action);
    }
}


void Page::setText(std::string text, int id)
{
    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->setText(text, id);
    }
}


void Page::setScrolling(ScrollDirection direction)
{
    switch (direction)
    {
    case ScrollDirectionForward:
        if (!scrollActive_)
        {
            menuScroll();
        }
        scrollActive_ = true;
        break;
    case ScrollDirectionBack:
        if (!scrollActive_)
        {
            menuScroll();
        }
        scrollActive_ = true;
        break;
    case ScrollDirectionIdle:
    default:
        scrollActive_ = false;
        break;
    }

}


bool Page::isHorizontalScroll()
{
    if (!(activeMenu_.size() > 0 && activeMenu_[0])) return false;
    return activeMenu_[0]->horizontalScroll;
}


void Page::pageScroll(ScrollDirection direction)
{
    if (activeMenu_.size() > 0 && activeMenu_[0])
    {
        if (direction == ScrollDirectionForward)
        {
            activeMenu_[0]->pageDown();
        }
        if (direction == ScrollDirectionBack)
        {
            activeMenu_[0]->pageUp();
        }

        unsigned int index = activeMenu_[0]->getScrollOffsetIndex();
        for (std::vector<ScrollingList*>::iterator it = activeMenu_.begin(); it != activeMenu_.end(); it++)
        {
            ScrollingList* menu = *it;
            if (menu)
                menu->setScrollOffsetIndex(index);
        }
    }
}


void Page::selectRandom()
{
    if (activeMenu_.size() > 0 && activeMenu_[0])
    {
        activeMenu_[0]->random();
        unsigned int index = activeMenu_[0]->getScrollOffsetIndex();
        for (std::vector<ScrollingList*>::iterator it = activeMenu_.begin(); it != activeMenu_.end(); it++)
        {
            ScrollingList* menu = *it;
            menu->setScrollOffsetIndex(index);
        }
    }
}


void Page::letterScroll(ScrollDirection direction)
{
    for (std::vector<ScrollingList*>::iterator it = activeMenu_.begin(); it != activeMenu_.end(); it++)
    {
        ScrollingList* menu = *it;
        if (menu)
        {
            if (direction == ScrollDirectionForward)
            {
                menu->letterDown();
            }
            if (direction == ScrollDirectionBack)
            {
                menu->letterUp();
            }
        }
    }
}


void Page::subScroll(ScrollDirection direction)
{
    for (std::vector<ScrollingList*>::iterator it = activeMenu_.begin(); it != activeMenu_.end(); it++)
    {
        ScrollingList* menu = *it;
        if (menu)
        {
            if (direction == ScrollDirectionForward)
            {
                menu->subDown();
            }
            if (direction == ScrollDirectionBack)
            {
                menu->subUp();
            }
        }
    }
}


void Page::cfwLetterSubScroll(ScrollDirection direction)
{
    for (std::vector<ScrollingList*>::iterator it = activeMenu_.begin(); it != activeMenu_.end(); it++)
    {
        ScrollingList* menu = *it;
        if (menu)
        {
            if (direction == ScrollDirectionForward)
            {
                menu->cfwLetterSubDown();
            }
            if (direction == ScrollDirectionBack)
            {
                menu->cfwLetterSubUp();
            }
        }
    }
}


size_t Page::getCollectionSize()
{
    if (!(activeMenu_.size() > 0 && activeMenu_[0])) return 0;
    return activeMenu_[0]->getSize();
}


unsigned int Page::getSelectedIndex()
{
    if (!(activeMenu_.size() > 0 && activeMenu_[0])) return 0;
    return activeMenu_[0]->getSelectedIndex();
}


bool Page::pushCollection(CollectionInfo* collection)
{

    // grow the menu as needed
    if (menus_.size() <= menuDepth_ && activeMenu_.size() > 0 && activeMenu_[0])
    {
        for (std::vector<ScrollingList*>::iterator it = activeMenu_.begin(); it != activeMenu_.end(); it++)
        {
            ScrollingList* menu = *it;
            ScrollingList* newMenu = new ScrollingList(*menu);
            pushMenu(newMenu, menuDepth_);
        }
    }

    activeMenu_ = menus_[menuDepth_];
    for (std::vector<ScrollingList*>::iterator it = activeMenu_.begin(); it != activeMenu_.end(); it++)
    {
        ScrollingList* menu = *it;
        menu->collectionName = collection->name;
        menu->setItems(&collection->items);
    }

    // build the collection info instance
    MenuInfo_S info;
    info.collection = collection;
    info.playlist = collection->playlists.begin();
    info.queueDelete = false;
    collections_.push_back(info);

    playlist_ = info.playlist;
    playlistChange();

    if (menuDepth_ < menus_.size())
    {
        menuDepth_++;
    }

    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->collectionName = collection->name;
    }

    return true;
}


bool Page::popCollection()
{

    if (!(activeMenu_.size() > 0 && activeMenu_[0])) return false;
    if (menuDepth_ <= 1) return false;
    if (collections_.size() <= 1) return false;

    // queue the collection for deletion
    MenuInfo_S* info = &collections_.back();
    info->queueDelete = true;
    deleteCollections_.push_back(*info);

    // get the next collection off of the stack
    collections_.pop_back();
    info = &collections_.back();
    playlist_ = info->playlist;
    playlistChange();

    menuDepth_--;
    activeMenu_ = menus_[menuDepth_ - 1];

    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->collectionName = info->collection->name;
    }

    return true;
}


void Page::enterMenu()
{

    for (MenuVector_T::iterator it = menus_.begin(); it != menus_.end(); it++)
    {
        for (std::vector<ScrollingList*>::iterator it2 = menus_[std::distance(menus_.begin(), it)].begin(); it2 != menus_[std::distance(menus_.begin(), it)].end(); it2++)
        {
            ScrollingList* menu = *it2;
            if (menuDepth_ - 1 == static_cast<unsigned int>(distance(menus_.begin(), it)))
            {
                // Also trigger animations for index i for active menu
                menu->triggerEvent("menuEnter", MENU_INDEX_HIGH + menuDepth_ - 1);
                menu->triggerMenuEnterEvent(MENU_INDEX_HIGH + menuDepth_ - 1);
            }
            else
            {
                menu->triggerEvent("menuEnter", menuDepth_ - 1);
                menu->triggerMenuEnterEvent(menuDepth_ - 1);
            }
        }
    }

    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->triggerEvent("menuEnter", menuDepth_ - 1);
    }

    return;
}


void Page::exitMenu()
{

    for (MenuVector_T::iterator it = menus_.begin(); it != menus_.end(); it++)
    {
        for (std::vector<ScrollingList*>::iterator it2 = menus_[std::distance(menus_.begin(), it)].begin(); it2 != menus_[std::distance(menus_.begin(), it)].end(); it2++)
        {
            ScrollingList* menu = *it2;
            if (menuDepth_ - 1 == static_cast<unsigned int>(distance(menus_.begin(), it)))
            {
                // Also trigger animations for index i for active menu
                menu->triggerEvent("menuExit", MENU_INDEX_HIGH + menuDepth_ - 1);
                menu->triggerMenuExitEvent(MENU_INDEX_HIGH + menuDepth_ - 1);
            }
            else
            {
                menu->triggerEvent("menuExit", menuDepth_ - 1);
                menu->triggerMenuExitEvent(menuDepth_ - 1);
            }
        }
    }

    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->triggerEvent("menuExit", menuDepth_ - 1);
    }

    return;
}


void Page::enterGame()
{

    for (MenuVector_T::iterator it = menus_.begin(); it != menus_.end(); it++)
    {
        for (std::vector<ScrollingList*>::iterator it2 = menus_[std::distance(menus_.begin(), it)].begin(); it2 != menus_[std::distance(menus_.begin(), it)].end(); it2++)
        {
            ScrollingList* menu = *it2;
            if (menuDepth_ - 1 == static_cast<unsigned int>(distance(menus_.begin(), it)))
            {
                // Also trigger animations for index i for active menu
                menu->triggerEvent("gameEnter", MENU_INDEX_HIGH + menuDepth_ - 1);
                menu->triggerGameEnterEvent(MENU_INDEX_HIGH + menuDepth_ - 1);
            }
            else
            {
                menu->triggerEvent("gameEnter", menuDepth_ - 1);
                menu->triggerGameEnterEvent(menuDepth_ - 1);
            }
        }
    }

    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->triggerEvent("gameEnter", menuDepth_ - 1);
    }

    return;
}


void Page::exitGame()
{

    for (MenuVector_T::iterator it = menus_.begin(); it != menus_.end(); it++)
    {
        for (std::vector<ScrollingList*>::iterator it2 = menus_[std::distance(menus_.begin(), it)].begin(); it2 != menus_[std::distance(menus_.begin(), it)].end(); it2++)
        {
            ScrollingList* menu = *it2;
            if (menuDepth_ - 1 == static_cast<unsigned int>(distance(menus_.begin(), it)))
            {
                // Also trigger animations for index i for active menu
                menu->triggerEvent("gameExit", MENU_INDEX_HIGH + menuDepth_ - 1);
                menu->triggerGameExitEvent(MENU_INDEX_HIGH + menuDepth_ - 1);
            }
            else
            {
                menu->triggerEvent("gameExit", menuDepth_ - 1);
                menu->triggerGameExitEvent(menuDepth_ - 1);
            }
        }
    }

    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->triggerEvent("gameExit", menuDepth_ - 1);
    }

    return;
}


std::string Page::getPlaylistName()
{
    return playlist_->first;
}


void Page::favPlaylist()
{
    if (playlist_->first == "favorites")
    {
        selectPlaylist("all");
    }
    else
    {
        selectPlaylist("favorites");
    }
    return;
}


void Page::nextPlaylist()
{
    MenuInfo_S& info = collections_.back();
    size_t numlists = info.collection->playlists.size();

    for (size_t i = 0; i <= numlists; ++i)
    {
        playlist_++;
        // wrap
        if (playlist_ == info.collection->playlists.end()) playlist_ = info.collection->playlists.begin();

        // find the first playlist
        if (playlist_->second->size() != 0) break;
    }

    for (std::vector<ScrollingList*>::iterator it = activeMenu_.begin(); it != activeMenu_.end(); it++)
    {
        ScrollingList* menu = *it;
        menu->setItems(playlist_->second);
    }
    playlistChange();
}


void Page::prevPlaylist()
{
    MenuInfo_S& info = collections_.back();
    size_t numlists = info.collection->playlists.size();

    for (size_t i = 0; i <= numlists; ++i)
    {
        // wrap
        if (playlist_ == info.collection->playlists.begin())
        {
            playlist_ = info.collection->playlists.end();
        }
        playlist_--;

        // find the first playlist
        if (playlist_->second->size() != 0) break;
    }

    for (std::vector<ScrollingList*>::iterator it = activeMenu_.begin(); it != activeMenu_.end(); it++)
    {
        ScrollingList* menu = *it;
        menu->setItems(playlist_->second);
    }
    playlistChange();
}


void Page::selectPlaylist(std::string playlist)
{
    MenuInfo_S& info = collections_.back();
    info.collection->Save();
    size_t numlists = info.collection->playlists.size();

    // Store current playlist
    CollectionInfo::Playlists_T::iterator playlist_store = playlist_;

    for (size_t i = 0; i <= numlists; ++i)
    {
        playlist_++;
        // wrap
        if (playlist_ == info.collection->playlists.end()) playlist_ = info.collection->playlists.begin();

        // find the first playlist
        if (playlist_->second->size() != 0 && playlist_->first == playlist) break;
    }

    // Do not change playlist if it does not exist or if it's empty
    if (playlist_->second->size() == 0 || playlist_->first != playlist)
        playlist_ = playlist_store;

    for (std::vector<ScrollingList*>::iterator it = activeMenu_.begin(); it != activeMenu_.end(); it++)
    {
        ScrollingList* menu = *it;
        menu->setItems(playlist_->second);
    }
    playlistChange();
}


void Page::nextCyclePlaylist(std::vector<std::string> list)
{

    // Empty list
    if (list.size() == 0)
        return;

    // Find the current playlist in the list
    std::vector<std::string>::iterator it = list.begin();
    while (*it != getPlaylistName() && it != list.end())
        ++it;

    // If current playlist not found, switch to the first found cycle playlist in the playlist list
    if (it == list.end())
    {
        for (std::vector<std::string>::iterator it2 = list.begin(); it2 != list.end(); ++it2)
        {
            selectPlaylist(*it2);
            if (*it2 == getPlaylistName())
                break;
        }
    }
    // Current playlist found; switch to the next found playlist in the list
    else
    {
        for (;;)
        {
            ++it;
            if (it == list.end()) it = list.begin(); // wrap
            selectPlaylist(*it);
            if (*it == getPlaylistName())
                break;
        }
    }

}


void Page::prevCyclePlaylist(std::vector<std::string> list)
{

    // Empty list
    if (list.size() == 0)
        return;

    // Find the current playlist in the list
    std::vector<std::string>::iterator it = list.begin();
    while (*it != getPlaylistName() && it != list.end())
        ++it;

    // If current playlist not found, switch to the first found cycle playlist in the playlist list
    if (it == list.end())
    {
        for (std::vector<std::string>::iterator it2 = list.begin(); it2 != list.end(); ++it2)
        {
            selectPlaylist(*it2);
            if (*it2 == getPlaylistName())
                break;
        }
    }
    // Current playlist found; switch to the previous found playlist in the list
    else
    {
        for (;;)
        {
            if (it == list.begin()) it = list.end(); // wrap
            --it;
            selectPlaylist(*it);
            if (*it == getPlaylistName())
                break;
        }
    }

}


void Page::update(float dt)
{
    for (MenuVector_T::iterator it = menus_.begin(); it != menus_.end(); it++)
    {
        for (std::vector<ScrollingList*>::iterator it2 = menus_[std::distance(menus_.begin(), it)].begin(); it2 != menus_[std::distance(menus_.begin(), it)].end(); it2++)
        {
            ScrollingList* menu = *it2;
            menu->update(dt);
        }
    }

    if (textStatusComponent_)
    {
        std::string status;
        config_.setProperty("status", status);
        textStatusComponent_->setText(status);
    }

    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        if (*it) (*it)->update(dt);
    }

}


void Page::cleanup()
{
    std::list<MenuInfo_S>::iterator del = deleteCollections_.begin();

    while (del != deleteCollections_.end())
    {
        MenuInfo_S& info = *del;
        if (info.queueDelete)
        {
            std::list<MenuInfo_S>::iterator next = del;
            ++next;

            if (info.collection)
            {
                info.collection->Save();
                delete info.collection;
            }
            deleteCollections_.erase(del);
            del = next;
        }
        else
        {
            ++del;
        }
    }
}


void Page::draw()
{
    for (unsigned int i = 0; i < NUM_LAYERS; ++i)
    {
        // Draw all components in the current layer
        for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
        {
            Component* component = *it;
            if (component && component->baseViewInfo.Layer == i)
            {
                if (!component->isVisible())
                {
                    Logger::write(Logger::ZONE_DEBUG, "Page", "Skipping render for component: " + component->group + "/" + component->subgroup +
                        " shared=" + (component->shared.empty() ? "none" : component->shared[0]));
                    continue;
                }
                component->draw();
            }
        }

        // Draw all menus from all depths
        for (MenuVector_T::iterator it = menus_.begin(); it != menus_.end(); ++it)
        {
            for (std::vector<ScrollingList*>::iterator it2 = it->begin(); it2 != it->end(); ++it2)
            {
                ScrollingList* menu = *it2;
                if (!menu->isVisible())
                {
                    Logger::write(Logger::ZONE_DEBUG, "Page", "Skipping render for menu: " + menu->group + "/" + menu->subgroup +
                        " shared=" + (menu->shared.empty() ? "none" : menu->shared[0]));
                    continue;
                }
                menu->draw(i);
            }
        }
    }
}

void Page::removePlaylist()
{
    if (!selectedItem_) return;

    MenuInfo_S& info = collections_.back();
    CollectionInfo* collection = info.collection;

    // Use original collection if on a game layout page
    if (collection->name == "TempGameCollection" && collection->originalCollection)
    {
        collection = collection->originalCollection;
    }

    std::vector<Item*>* items = collection->playlists["favorites"];
    if (items)
    {
        std::vector<Item*>::iterator it = std::find(items->begin(), items->end(), selectedItem_);
        if (it != items->end())
        {
            items->erase(it);
            selectedItem_->isFavorite = false;
            collection->sortPlaylists();
            collection->saveRequest = true;

            std::string order;
            for (auto* item : *items) order += item->name + ", ";
            Logger::write(Logger::ZONE_INFO, "Debug", "After remove: " + order);
        }
        collection->Save();
    }
}


void Page::addPlaylist()
{
    if (!selectedItem_) return;

    MenuInfo_S& info = collections_.back();
    CollectionInfo* collection = info.collection;

    // Use original collection if on a game layout page
    if (collection->name == "TempGameCollection" && collection->originalCollection)
    {
        collection = collection->originalCollection;
    }

    std::vector<Item*>* items = collection->playlists["favorites"];
    if (!items)
    {
        items = new std::vector<Item*>();
        collection->playlists["favorites"] = items;
    }

    if (playlist_->first != "favorites")
    {
        // Remove existing item with same name
        for (auto it = items->begin(); it != items->end(); )
        {
            if ((*it)->name == selectedItem_->name)
            {
                it = items->erase(it);
            }
            else
            {
                ++it;
            }
        }
        // Add to top
        items->insert(items->begin(), selectedItem_);
        selectedItem_->isFavorite = true;
        collection->sortPlaylists();
        collection->saveRequest = true;

        std::string order;
        for (auto* item : *items) order += item->name + ", ";
        Logger::write(Logger::ZONE_INFO, "Debug", "After add: " + order);
    }
    collection->Save();
}


void Page::togglePlaylist()
{
    if (!selectedItem_) return;

    MenuInfo_S& info = collections_.back();
    CollectionInfo* collection = info.collection;

    // Use original collection if on a game layout page
    if (collection->name == "TempGameCollection" && collection->originalCollection)
    {
        collection = collection->originalCollection;
    }

    std::vector<Item*>* items = collection->playlists["favorites"];
    if (!items)
    {
        items = new std::vector<Item*>();
        collection->playlists["favorites"] = items;
    }

    bool isInFavorites = false;
    for (auto it = items->begin(); it != items->end(); ++it)
    {
        if (*it == selectedItem_)
        {
            isInFavorites = true;
            items->erase(it);
            selectedItem_->isFavorite = false;
            break;
        }
    }

    if (!isInFavorites)
    {
        // Remove existing item with same name
        for (auto it = items->begin(); it != items->end(); )
        {
            if ((*it)->name == selectedItem_->name)
            {
                it = items->erase(it);
            }
            else
            {
                ++it;
            }
        }
        items->insert(items->begin(), selectedItem_);
        selectedItem_->isFavorite = true;
    }

    collection->sortPlaylists();
    collection->saveRequest = true;

    std::string order;
    for (auto* item : *items) order += item->name + ", ";
    Logger::write(Logger::ZONE_INFO, "Debug", "After toggle: " + order);

    collection->Save();
}


std::string Page::getCollectionName()
{
    if (collections_.size() == 0) return "";

    MenuInfo_S& info = collections_.back();
    return info.collection->name;

}


CollectionInfo* Page::getCollection()
{
    return collections_.back().collection;
}


void Page::freeGraphicsMemory()
{
    for (auto it = menus_.begin(); it != menus_.end(); it++) {
        for (auto it2 = it->begin(); it2 != it->end(); ++it2) {
            (*it2)->freeGraphicsMemory();
        }
    }

    for (auto& pair : soundsByType_) {
        for (Sound* sound : pair.second) {
            sound->free();
        }
    }
  
    for (auto it = LayerComponents.begin(); it != LayerComponents.end(); ++it) {
        (*it)->freeGraphicsMemory();
    }
}


void Page::allocateGraphicsMemory()
{
    Logger::write(Logger::ZONE_DEBUG, "Page", "Allocating graphics memory");

    // Reset and allocate memory for all components in LayerComponents (single pass) :P
    for (auto it = LayerComponents.begin(); it != LayerComponents.end(); ++it) {
        if (LibVLCVideo* video = dynamic_cast<LibVLCVideo*>(*it)) {
            video->reset();
        }
        (*it)->allocateGraphicsMemory();
    }

    // Allocate memory for active menu layers up to menuDepth_
    for (size_t i = 0; i < menuDepth_ && i < menus_.size(); ++i) {
        for (auto it = menus_[i].begin(); it != menus_[i].end(); ++it) {
            (*it)->allocateGraphicsMemory();
        }
    }

    // Allocate sound resources
    for (auto& pair : soundsByType_) {
        for (Sound* sound : pair.second) {
            sound->allocate();
        }
    }

    // Handle jukebox video if applicable
    if (jukebox_)
    {
        // If jukebox uses libVLCInstance directly (not in LayerComponents)
        libVLCInstance.reset();
        Logger::write(Logger::ZONE_DEBUG, "Page", "Reset jukebox LibVLCVideo instance");
    }

    Logger::write(Logger::ZONE_DEBUG, "Page", "Allocate graphics memory complete");
}

void Page::deInitializeFonts()
{
    for (MenuVector_T::iterator it = menus_.begin(); it != menus_.end(); it++)
    {
        for (std::vector<ScrollingList*>::iterator it2 = menus_[std::distance(menus_.begin(), it)].begin(); it2 != menus_[std::distance(menus_.begin(), it)].end(); it2++)
        {
            ScrollingList* menu = *it2;
            menu->deInitializeFonts();
        }
    }

    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->deInitializeFonts();
    }
}


void Page::initializeFonts()
{
    for (MenuVector_T::iterator it = menus_.begin(); it != menus_.end(); it++)
    {
        for (std::vector<ScrollingList*>::iterator it2 = menus_[std::distance(menus_.begin(), it)].begin(); it2 != menus_[std::distance(menus_.begin(), it)].end(); it2++)
        {
            ScrollingList* menu = *it2;
            menu->initializeFonts();
        }
    }

    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->initializeFonts();
    }
}


void Page::playSelect() {
    playSound("select");
}

void Page::playSound(const std::string& type) {
    auto it = soundsByType_.find(type);
    if (it != soundsByType_.end()) {
        for (Sound* sound : it->second) {
            if (sound->active) {
                sound->play();
                break; 
            }
        }
    }
}

bool Page::isSelectPlaying()
{
    auto it = soundsByType_.find("select");
    if (it != soundsByType_.end()) {
        
        for (Sound* sound : it->second) {
            
            if (sound->active && sound->isPlaying()) {
                return true;
            }
        }
    }
    return false;
}


void Page::reallocateMenuSpritePoints()
{
    for (std::vector<ScrollingList*>::iterator it = activeMenu_.begin(); it != activeMenu_.end(); it++)
    {
        ScrollingList* menu = *it;
        if (menu)
        {
            menu->deallocateSpritePoints();
            menu->allocateSpritePoints();
        }
    }
}


bool Page::isMenuScrolling()
{
    return scrollActive_;
}


bool Page::isPlaying()
{

    bool retVal = false;

    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        retVal |= (*it)->isPlaying();
    }

    return retVal;

}


void Page::resetScrollPeriod()
{
    for (std::vector<ScrollingList*>::iterator it = activeMenu_.begin(); it != activeMenu_.end(); it++)
    {
        ScrollingList* menu = *it;
        if (menu) menu->resetScrollPeriod();
    }
    return;
}


void Page::updateScrollPeriod()
{
    for (std::vector<ScrollingList*>::iterator it = activeMenu_.begin(); it != activeMenu_.end(); it++)
    {
        ScrollingList* menu = *it;
        if (menu) menu->updateScrollPeriod();
    }
    return;
}


void Page::scroll(bool forward)
{
    for (auto it = activeMenu_.begin(); it != activeMenu_.end(); it++) {
        if (*it) (*it)->scroll(forward);
    }
    onNewScrollItemSelected();
    playSound("highlight");
}


bool Page::hasSubs()
{
    return collections_.back().collection->hasSubs;
}


int Page::getLayoutWidth(int monitor)
{
    if (monitor < SDL::getNumScreens())
        return layoutWidth_[monitor];
    else
        return 0;
}


int Page::getLayoutHeight(int monitor)
{
    if (monitor < SDL::getNumScreens())
        return layoutHeight_[monitor];
    else
        return 0;
}

//void Page::setLoadSound(Sound* chunk)
//{
//    loadSoundChunk_ = chunk;
//}
//
//
//void Page::setUnloadSound(Sound* chunk)
//{
//    unloadSoundChunk_ = chunk;
//}
//
//
//void Page::setHighlightSound(Sound* chunk)
//{
//    highlightSoundChunk_ = chunk;
//}
//
//
//void Page::setSelectSound(Sound* chunk)
//{
//    selectSoundChunk_ = chunk;
//}


void Page::setLayoutWidth( int monitor, int width)
{
    if (monitor < SDL::getNumScreens())
        layoutWidth_[monitor] = width;
}


void Page::setLayoutHeight( int monitor, int height)
{
    if (monitor < SDL::getNumScreens())
        layoutHeight_[monitor] = height;
}


void Page::setJukebox()
{
    jukebox_ = true;
    return;
}


bool Page::isJukebox()
{
    return jukebox_;
}


bool Page::isJukeboxPlaying()
{

    bool retVal = false;

    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        retVal |= (*it)->isJukeboxPlaying();
    }

    return retVal;

}


void Page::skipForward()
{
    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->skipForward();
    }
}


void Page::skipBackward()
{
    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->skipBackward();
    }
}


void Page::skipForwardp()
{
    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->skipForwardp();
    }
}


void Page::skipBackwardp()
{
    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->skipBackwardp();
    }
}


void Page::pause()
{
    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->pause();
    }
}


void Page::restart()
{
    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        (*it)->restart();
    }
}


unsigned long long Page::getCurrent()
{
    unsigned long long ret = 0;
    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        ret += (*it)->getCurrent();
    }
    return ret;
}


unsigned long long Page::getDuration()
{
    unsigned long long ret = 0;
    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        ret += (*it)->getDuration();
    }
    return ret;
}


bool Page::isPaused()
{
    bool ret = false;
    for (std::vector<Component*>::iterator it = LayerComponents.begin(); it != LayerComponents.end(); ++it)
    {
        ret |= (*it)->isPaused();
    }
    return ret;
}

void Page::calculateScalingFactors() {
    int numScreens = SDL::getNumScreens();
    scaleX_.resize(numScreens);
    scaleY_.resize(numScreens);
    offsetX_.resize(numScreens);
    offsetY_.resize(numScreens);

    for (int i = 0; i < numScreens; ++i) {
        float windowWidth = static_cast<float>(SDL::getWindowWidth(i));
        float windowHeight = static_cast<float>(SDL::getWindowHeight(i));
        float layoutWidth = static_cast<float>(layoutWidth_[i]);
        float layoutHeight = static_cast<float>(layoutHeight_[i]);

        switch (layoutScaleMode_) {
        case ViewInfo::ScaleMode::Stretch:
            scaleX_[i] = (windowWidth / layoutWidth) * layoutScaleFactor_;
            scaleY_[i] = (windowHeight / layoutHeight) * layoutScaleFactor_;
            offsetX_[i] = 0;
            offsetY_[i] = 0;
            break;

        case ViewInfo::ScaleMode::Fit: {
            float aspectLayout = layoutWidth / layoutHeight;
            float aspectWindow = windowWidth / windowHeight;
            float scale = (aspectLayout > aspectWindow) ?
                (windowWidth / layoutWidth) :
                (windowHeight / layoutHeight);
            scale *= layoutScaleFactor_;
            scaleX_[i] = scale;
            scaleY_[i] = scale;
            offsetX_[i] = (windowWidth - layoutWidth * scale) / 2;
            offsetY_[i] = (windowHeight - layoutHeight * scale) / 2;
            break;
        }

        case ViewInfo::ScaleMode::Fill: {
            float aspectLayout = layoutWidth / layoutHeight;
            float aspectWindow = windowWidth / windowHeight;
            float scale = (aspectLayout < aspectWindow) ?
                (windowWidth / layoutWidth) :
                (windowHeight / layoutHeight);
            scale *= layoutScaleFactor_;
            scaleX_[i] = scale;
            scaleY_[i] = scale;
            offsetX_[i] = (windowWidth - layoutWidth * scale) / 2;
            offsetY_[i] = (windowHeight - layoutHeight * scale) / 2;
            break;
        }

        case ViewInfo::ScaleMode::None:
            scaleX_[i] = layoutScaleFactor_;
            scaleY_[i] = layoutScaleFactor_;
            offsetX_[i] = (windowWidth - layoutWidth * layoutScaleFactor_) / 2;
            offsetY_[i] = (windowHeight - layoutHeight * layoutScaleFactor_) / 2;
            break;
        }
    }
}

float Page::getScaleX(int monitor) const {
    if (monitor < 0 || monitor >= static_cast<int>(scaleX_.size())) {
        return scaleX_[0];
    }
    return scaleX_[monitor];
}

float Page::getScaleY(int monitor) const {
    if (monitor < 0 || monitor >= static_cast<int>(scaleY_.size())) {
        return scaleY_[0];
    }
    return scaleY_[monitor];
}

float Page::getOffsetX(int monitor) const {
    if (monitor < 0 || monitor >= static_cast<int>(offsetX_.size())) {
        return offsetX_[0];
    }
    return offsetX_[monitor];
}

float Page::getOffsetY(int monitor) const {
    if (monitor < 0 || monitor >= static_cast<int>(offsetY_.size())) {
        return offsetY_[0];
    }
    return offsetY_[monitor];
}
void Page::setLayoutScaleFactor(float factor) {
    layoutScaleFactor_ = factor;
}



void Page::setGroupActive(const std::string& group, bool active) {
    if (active) {
        activeGroups.insert(group);
    }
    else {
        activeGroups.erase(group);
    }
}

void Page::setSubgroupActive(const std::string& subgroup, bool active) {
    if (active) {
        activeSubgroups.insert(subgroup);
    }
    else {
        activeSubgroups.erase(subgroup);
    }
}

void Page::initializeActiveStates() {
    for (const auto& component : LayerComponents) {
        if (component->active) {
            activeGroups.insert(component->group);
            activeSubgroups.insert(component->subgroup);
        }
    }
}
bool Page::isSharedActive(const std::string& shared) const {
    return activeShared_.count(shared) > 0;
}

void Page::setSharedActive(const std::string& shared, bool active) {
    if (active) {
        activeShared_.insert(shared);
    }
    else {
        activeShared_.erase(shared);
    }
}

// Get the first active menu(typically there's only one)
    ScrollingList * Page::getActiveMenu()
{
    if (activeMenu_.empty()) {
        return nullptr;
    }
    return activeMenu_[0];
}

// Set the selected index for the active menu
void Page::setActiveMenuSelectedIndex(unsigned int index)
{
    if (!activeMenu_.empty() && activeMenu_[0]) {
        activeMenu_[0]->setSelectedIndex(index);
    }
}

std::string Page::getCollectionNameAtDepth(unsigned int depth) const
{
    if (depth >= collections_.size()) {
        return "";  // Return empty string if depth is out of bounds
    }
    auto it = collections_.begin();  // Start at the beginning of the list
    std::advance(it, depth);        // Move iterator to the desired position
    return it->collection->name;    // Access the name field
}
unsigned int Page::getSelectedIndexAtDepth(unsigned int depth) const
{
    if (depth < menus_.size() && !menus_[depth].empty()) {
        return menus_[depth][0]->getSelectedIndex();
    }
    return 0;
}
ScrollingList* Page::getMenuAtDepth(unsigned int depth)
{
    if (depth < menus_.size() && !menus_[depth].empty()) {
        return menus_[depth][0];
    }
    return nullptr;
}

std::vector<Item*>* Page::getCurrentPlaylist() {
    if (collections_.empty()) return nullptr;
    CollectionInfo* coll = collections_.back().collection;
    auto it = coll->playlists.find(getPlaylistName());
    if (it == coll->playlists.end()) return nullptr;
    return it->second;
}
void Page::selectItem(Item* item) {
    std::vector<Item *>* playlist = getCurrentPlaylist();
    if (playlist) {
        auto it = std::find(playlist->begin(), playlist->end(), item);
        if (it != playlist->end()) {
            unsigned int idx = static_cast<unsigned int>(std::distance(playlist->begin(), it));
            ScrollingList *menu = getActiveMenu();
            if (menu) {
                menu->setSelectedIndex(idx);
                onNewItemSelected();
            }
        }
    }
}