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
#include "Component.h"
#include "Container.h"
#include "../Animate/Tween.h"
#include "../../Graphics/ViewInfo.h"
#include "../../Utility/Log.h"
#include "../../SDL.h"
#include "../PageBuilder.h"
#include <map>
#include <algorithm>
#include "../Animate/TweenTypes.h"


Component::Component(Page &p)
: page(p)
{
    tweens_                   = NULL;
    backgroundTexture_        = NULL;
    menuScrollReload_         = false;
    freeGraphicsMemory();
    id_                       = -1;
}

Component::Component(const Component &copy)
    : page(copy.page)
{
    tweens_ = NULL;
    backgroundTexture_ = NULL;
    freeGraphicsMemory();

    if ( copy.tweens_ )
    {
        AnimationEvents *tweens = new AnimationEvents(*copy.tweens_);
        setTweens(tweens);
    }


}

Component::~Component()
{
    freeGraphicsMemory();
}

void Component::freeGraphicsMemory()
{
    animationRequestedType_ = "";
    animationType_          = "";
    animationRequested_     = false;
    newItemSelected         = false;
    newScrollItemSelected   = false;
    menuIndex_              = -1;

    currentTweens_        = NULL;
    currentTweenIndex_    = 0;
    currentTweenComplete_ = true;
    elapsedTweenTime_     = 0;

    //################### vlc Player manages it 
    if ( backgroundTexture_ )
    {
       
              
       
        SDL_LockMutex(SDL::getMutex());
        SDL_DestroyTexture(backgroundTexture_);
        SDL_UnlockMutex(SDL::getMutex());

        backgroundTexture_ = NULL;
    } 
}

void Component::allocateGraphicsMemory()
{
    //################### vlc Player manages it 
    if (!backgroundTexture_)
    {

        // make a 4x4 pixel wide surface to be stretched during rendering, make it a white background so we can use
        // color  later
        SDL_Surface* surface = SDL_CreateRGBSurface(0, 4, 4, 32, 0, 0, 0, 0);
        SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 255, 255, 255));

        SDL_LockMutex(SDL::getMutex());
        backgroundTexture_ = SDL_CreateTextureFromSurface(SDL::getRenderer(baseViewInfo.Monitor), surface);
        SDL_UnlockMutex(SDL::getMutex());

        SDL_FreeSurface(surface);
        SDL_SetTextureBlendMode(backgroundTexture_, SDL_BLENDMODE_BLEND);
        
    }

   
}


void Component::deInitializeFonts()
{
}


void Component::initializeFonts()
{
}


void Component::triggerEvent(std::string event, int menuIndex)  
{
    animationRequestedType_ = event;
    animationRequested_     = true;
    menuIndex_              = (menuIndex > 0 ? menuIndex : 0);
}

void Component::setPlaylist(std::string name)
{
    this->playlistName = name;
}

void Component::setNewItemSelected()
{
    newItemSelected = true;
}

void Component::setNewScrollItemSelected()
{
    newScrollItemSelected = true;
}

void Component::setId( int id )
{
    id_ = id;
}

bool Component::isIdle()
{
    return (currentTweenComplete_ || animationType_ == "idle" || animationType_ == "menuIdle" || animationType_ == "attract");
}

bool Component::isAttractIdle()
{
    return (currentTweenComplete_ || animationType_ == "idle" || animationType_ == "menuIdle");
}

bool Component::isMenuScrolling()
{
    return (!currentTweenComplete_ && animationType_ == "menuScroll");
}

void Component::setTweens(AnimationEvents *set)
{
    tweens_ = set;
}

void Component::update(float dt)
{
    elapsedTweenTime_ += dt;

    if ( animationRequested_ && animationRequestedType_ != "" )
    {
      Animation *newTweens;
      // Check if this component is part of an active scrolling list
      if ( menuIndex_ >= MENU_INDEX_HIGH )
      {
          // Check for animation at index i
          newTweens = tweens_->getAnimation( animationRequestedType_, MENU_INDEX_HIGH );
          if ( !(newTweens && newTweens->size() > 0) )
          {
              // Check for animation at the current menuIndex
              newTweens = tweens_->getAnimation( animationRequestedType_, menuIndex_ - MENU_INDEX_HIGH);
          }
      }
      else
      {
          // Check for animation at the current menuIndex
          newTweens = tweens_->getAnimation( animationRequestedType_, menuIndex_ );
      }
      if (newTweens && newTweens->size() > 0)
      {
        animationType_        = animationRequestedType_;
        currentTweens_        = newTweens;
        currentTweenIndex_    = 0;
        elapsedTweenTime_     = 0;
        storeViewInfo_        = baseViewInfo;
        currentTweenComplete_ = false;
      }
      animationRequested_   = false;
    }

    if (tweens_ && currentTweenComplete_)
    {
        animationType_        = "idle";
        currentTweens_        = tweens_->getAnimation( "idle", menuIndex_ );
        if ( currentTweens_ && currentTweens_->size( ) == 0 && !page.isMenuScrolling( ) )
        {
            currentTweens_    = tweens_->getAnimation( "menuIdle", menuIndex_ );
            if ( currentTweens_ && currentTweens_->size( ) > 0 )
            {
                currentTweens_ = currentTweens_;
            }
        }
        currentTweenIndex_    = 0;
        elapsedTweenTime_     = 0;
        storeViewInfo_        = baseViewInfo;
        currentTweenComplete_ = false;
        animationRequested_   = false;
    }

    currentTweenComplete_ = animate();
    if ( currentTweenComplete_ )
    {
      currentTweens_     = NULL;
      currentTweenIndex_ = 0;
    }
}


void Component::draw() {
    if (!isVisible()) {
        baseViewInfo.Alpha = 0.0f;
        return;
    }

    SDL_Texture* texture = getTexture();
    if (!texture) {
        // background-only components (container, etc.)
        if (backgroundTexture_ && baseViewInfo.BackgroundAlpha > 0.001f && dynamic_cast<Container*>(this) != nullptr) {
            if (!baseViewInfo.Shape.empty() && baseViewInfo.Shape != "rect") {
                SDL::renderShapeGeometry(baseViewInfo,
                    page.getLayoutWidth(baseViewInfo.Monitor), page.getLayoutHeight(baseViewInfo.Monitor));
                return;
            }
            SDL_Rect rect = {
                static_cast<int>(baseViewInfo.XRelativeToOrigin()),
                static_cast<int>(baseViewInfo.YRelativeToOrigin()),
                static_cast<int>(baseViewInfo.ScaledWidth()),
                static_cast<int>(baseViewInfo.ScaledHeight())
            };
            SDL_SetTextureColorMod(backgroundTexture_,
                static_cast<Uint8>(baseViewInfo.BackgroundRed * 255),
                static_cast<Uint8>(baseViewInfo.BackgroundGreen * 255),
                static_cast<Uint8>(baseViewInfo.BackgroundBlue * 255));
            // BackgroundAlpha is modulated with Alpha inside SDL::renderCopy
            SDL::renderCopy(backgroundTexture_, baseViewInfo.BackgroundAlpha, nullptr, &rect, baseViewInfo,
                page.getLayoutWidth(baseViewInfo.Monitor), page.getLayoutHeight(baseViewInfo.Monitor));
        }
        return;
    }

    int origW, origH;
    if (SDL_QueryTexture(texture, nullptr, nullptr, &origW, &origH) != 0) {
        Logger::write(Logger::ZONE_ERROR, "Component", "Failed to query texture size: " + std::string(SDL_GetError()));
        return;
    }

    float imgW = static_cast<float>(origW);
    float imgH = static_cast<float>(origH);

    if (baseViewInfo.ImageWidth == 0 || baseViewInfo.ImageHeight == 0) {
        baseViewInfo.ImageWidth = imgW;
        baseViewInfo.ImageHeight = imgH;
    }

    float contW = baseViewInfo.ScaledWidth();
    float contH = baseViewInfo.ScaledHeight();

    // 1. Compute final render size + srcRect (ScaleMode)
    float renderW, renderH;
    SDL_Rect srcRect = { 0, 0, origW, origH };
    SDL_Rect* pSrcRect = nullptr;

    switch (baseViewInfo.scaleMode) {
    case ViewInfo::ScaleMode::Stretch:
        renderW = contW;
        renderH = contH;
        break;

    case ViewInfo::ScaleMode::Fit: {
        float imgAspect = imgW / imgH;
        float contAspect = contW / contH;
        if (imgAspect > contAspect) {
            renderW = contW;
            renderH = contW / imgAspect;
        }
        else {
            renderH = contH;
            renderW = contH * imgAspect;
        }
        break;
    }

    case ViewInfo::ScaleMode::Fill: {
        float imgAspect = imgW / imgH;
        float contAspect = contW / contH;
        float scale;
        if (imgAspect < contAspect) {
            renderW = contW;
            renderH = contW / imgAspect;
            scale = renderW / imgW;
        }
        else {
            renderH = contH;
            renderW = contH * imgAspect;
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
        break;
    }

    case ViewInfo::ScaleMode::None:
        renderW = imgW;
        renderH = imgH;
        break;
    }

    // Prepare common params for SDL rendering
    float x = baseViewInfo.X + baseViewInfo.XOffset - baseViewInfo.XOrigin * renderW;
    float y = baseViewInfo.Y + baseViewInfo.YOffset - baseViewInfo.YOrigin * renderH;
    SDL_Rect destRect = {
        static_cast<int>(std::round(x)),
        static_cast<int>(std::round(y)),
        static_cast<int>(std::round(renderW)),
        static_cast<int>(std::round(renderH))
    };


        SDL::renderCopy(texture, baseViewInfo.Alpha, pSrcRect, &destRect, baseViewInfo,
            page.getLayoutWidth(baseViewInfo.Monitor),
            page.getLayoutHeight(baseViewInfo.Monitor));
   
}



 // Define member pointer types
    using FloatMember = float ViewInfo::*;
using UIntMember = unsigned int ViewInfo::*;

// Map for float properties in ViewInfo
static const std::map<TweenProperty, FloatMember> floatProperties = {
    {TWEEN_PROPERTY_X, &ViewInfo::X},
    {TWEEN_PROPERTY_Y, &ViewInfo::Y},
    {TWEEN_PROPERTY_HEIGHT, &ViewInfo::Height},
    {TWEEN_PROPERTY_WIDTH, &ViewInfo::Width},
    {TWEEN_PROPERTY_ANGLE, &ViewInfo::Angle},
    {TWEEN_PROPERTY_ALPHA, &ViewInfo::Alpha},
    {TWEEN_PROPERTY_X_ORIGIN, &ViewInfo::XOrigin},
    {TWEEN_PROPERTY_Y_ORIGIN, &ViewInfo::YOrigin},
    {TWEEN_PROPERTY_X_OFFSET, &ViewInfo::XOffset},
    {TWEEN_PROPERTY_Y_OFFSET, &ViewInfo::YOffset},
    {TWEEN_PROPERTY_FONT_SIZE, &ViewInfo::FontSize},
    {TWEEN_PROPERTY_BACKGROUND_ALPHA, &ViewInfo::BackgroundAlpha},
    {TWEEN_PROPERTY_MAX_WIDTH, &ViewInfo::MaxWidth},
    {TWEEN_PROPERTY_MAX_HEIGHT, &ViewInfo::MaxHeight},
    {TWEEN_PROPERTY_CONTAINER_X, &ViewInfo::ContainerX},
    {TWEEN_PROPERTY_CONTAINER_Y, &ViewInfo::ContainerY},
    {TWEEN_PROPERTY_CONTAINER_WIDTH, &ViewInfo::ContainerWidth},
    {TWEEN_PROPERTY_CONTAINER_HEIGHT, &ViewInfo::ContainerHeight},
    {TWEEN_PROPERTY_ANGLE_Y, &ViewInfo::AngleY},
	{TWEEN_PROPERTY_ANGLE_X, &ViewInfo::AngleX},
	{TWEEN_PROPERTY_SLICE3D_X, &ViewInfo::Slice3DX},
	{TWEEN_PROPERTY_SLICE3D_Y, &ViewInfo::Slice3DY},
	{TWEEN_PROPERTY_WRAP_LEFT_X, &ViewInfo::WrapLX},
	{TWEEN_PROPERTY_WRAP_LEFT_Y, &ViewInfo::WrapLY},
	{TWEEN_PROPERTY_WRAP_RIGHT_X, &ViewInfo::WrapRX},
	{TWEEN_PROPERTY_WRAP_RIGHT_Y, &ViewInfo::WrapRY},
	{TWEEN_PROPERTY_WRAP_TOP_Y, &ViewInfo::WrapTY},
	{TWEEN_PROPERTY_WRAP_TOP_X, &ViewInfo::WrapTX},
	{TWEEN_PROPERTY_WRAP_BOTTOM_Y, &ViewInfo::WrapBY},
	{TWEEN_PROPERTY_WRAP_BOTTOM_X, &ViewInfo::WrapBX},
    {TWEEN_PROPERTY_VOLUME, &ViewInfo::Volume}
};

// Update the declaration of uintProperties to ensure compatibility with the map initialization.  
static const std::map<TweenProperty, UIntMember> uintProperties = {  
    {TWEEN_PROPERTY_LAYER, reinterpret_cast<UIntMember>(&ViewInfo::Layer)},  
    {TWEEN_PROPERTY_MONITOR, reinterpret_cast<UIntMember>(&ViewInfo::Monitor)}  
};

// Refactored animate method
bool Component::animate()
{
    // Early return if no tweens or all sets processed
    if (!currentTweens_ || currentTweenIndex_ >= currentTweens_->size()) {
        return true;
    }

    // Get the current tween set
    TweenSet* tweens = currentTweens_->tweenSet(currentTweenIndex_);
    bool currentDone = true;

    // Process each tween in the current set
    for (unsigned int i = 0; i < tweens->size(); i++) {
        Tween* tween = tweens->tweens()->at(i);

        // Clamp elapsed time to the tween's duration
        double effectiveElapsedTime = std::fmin(elapsedTweenTime_, static_cast<double>(tween->duration));

        // If time remains, the current set isn’t done
        if (tween->duration <= 0.0001f) {
            effectiveElapsedTime = tween->duration;
        }
        else if (elapsedTweenTime_ < tween->duration) {
            currentDone = false;
        }

        // Apply animation based on property type
        if (auto it = floatProperties.find(tween->property); it != floatProperties.end()) {
            FloatMember member = it->second;
            if (tween->startDefined) {
                // Use defined start value
                baseViewInfo.*member = tween->animate(effectiveElapsedTime);
            }
            else {
                // Use stored value as start
                baseViewInfo.*member = tween->animate(effectiveElapsedTime, storeViewInfo_.*member);
            }
        }
        else if (auto it = uintProperties.find(tween->property); it != uintProperties.end()) {
            UIntMember member = it->second;
            if (tween->startDefined) {
                // Use defined start value, cast to unsigned int
                baseViewInfo.*member = static_cast<unsigned int>(tween->animate(effectiveElapsedTime));
            }
            else {
                // Use stored value as start, cast to unsigned int
                baseViewInfo.*member = static_cast<unsigned int>(tween->animate(effectiveElapsedTime, storeViewInfo_.*member));
            }
        }
        // Unmapped properties (e.g., TWEEN_PROPERTY_GIFFRAMES, TWEEN_PROPERTY_NOP) are ignored
    }

    // If the current set is complete, advance to the next set
    if (currentDone) {
        currentTweenIndex_++;
        elapsedTweenTime_ = 0;
        storeViewInfo_ = baseViewInfo; // Update stored view info
    }

    // Return true if all tween sets are processed
    return currentTweenIndex_ >= currentTweens_->size();
}



bool Component::isPlaying()
{
    return false;
}




bool Component::isJukeboxPlaying()
{
    return false;
}


void Component::setMenuScrollReload(bool menuScrollReload)
{
    menuScrollReload_ = menuScrollReload;
}


bool Component::getMenuScrollReload()
{
    return menuScrollReload_;
}

int Component::getId( )
{
    return id_;
}

bool Component::isVisible() const
{
    if (!active) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_DEBUG, "Component", "Not visible: active=false for " + name + "/" + group + "/" + subgroup + " shared=" + (shared.empty() ? "none" : shared[0]));
#endif
        return false;
    }
    if (!group.empty() && !page.isGroupActive(group)) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_DEBUG, "Component", "Not visible: group inactive for " + name + "/" + group + "/" + subgroup + " shared=" + (shared.empty() ? "none" : shared[0]));
#endif
        return false;
    }
    if (!subgroup.empty() && !page.isSubgroupActive(subgroup)) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_DEBUG, "Component", "Not visible: subgroup inactive for " + name + "/" + group + "/" + subgroup + " shared=" + (shared.empty() ? "none" : shared[0]));
#endif
        return false;
    }
    if (!shared.empty()) {
        bool sharedActive = false;
        for (const std::string& s : shared) {
            std::string effectiveShared = s;
            if (!effectiveShared.empty() && (!name.empty() || !group.empty() || !subgroup.empty())) effectiveShared += "/";
            if (!name.empty()) {
                effectiveShared += name;
                if (!group.empty() || !subgroup.empty()) effectiveShared += "/";
            }
            if (!group.empty()) {
                effectiveShared += group;
                if (!subgroup.empty()) effectiveShared += "/";
            }
            if (!subgroup.empty()) effectiveShared += subgroup;
            if (page.isSharedActive(effectiveShared)) {
                sharedActive = true;
                break;
            }
        }
        if (!sharedActive) {
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "Component", "Not visible: no active shared group for " + name + "/" + group + "/" + subgroup + " shared=" + (shared.empty() ? "none" : shared[0]));
#endif
            return false;
        }
    }
#ifdef _DEBUG
    Logger::write(Logger::ZONE_DEBUG, "Component", "Visible: " + name + "/" + group + "/" + subgroup + " shared=" + (shared.empty() ? "none" : shared[0]));
#endif
    return true;
}