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

#include "VideoComponent.h"
#include "../ViewInfo.h"
#include "../../Database/Configuration.h"
#include "../../Utility/Log.h"
#include "../../Video/LibVLCVideo.h"
#include "../../Video/VideoFactory.h"
#include "../../SDL.h"
#include <cmath>

VideoComponent::VideoComponent(IVideo *videoInst, Page &p, std::string videoFile)
    : Component(p)
    , videoFile_(videoFile)
    , videoInst_(videoInst)
    , isPlaying_(false)
{
//   AllocateGraphicsMemory();
}

VideoComponent::~VideoComponent()
{
#ifdef _DEBUG
    Logger::write(Logger::ZONE_DEBUG, "VideoComponent", "Destructor called");
#endif
    freeGraphicsMemory();
    if (videoInst_) {
        videoInst_->stop();
        if (VideoFactory::canDelete(videoInst_))
            delete videoInst_;
        videoInst_ = NULL;
    }
}

void VideoComponent::update(float dt)
{
    if (videoInst_)
    {
        isPlaying_ = ((LibVLCVideo *)(videoInst_))->isPlaying();
    }
    if(isPlaying_)
    {
        videoInst_->setVolume(baseViewInfo.Volume);
        videoInst_->update(dt);

        // video needs to run a frame to start getting size info
        if(baseViewInfo.ImageHeight == 0 && baseViewInfo.ImageWidth == 0)
        {
            baseViewInfo.ImageHeight = static_cast<float>(videoInst_->getHeight());
            baseViewInfo.ImageWidth = static_cast<float>(videoInst_->getWidth());
        }
    }

    Component::update(dt);

}

void VideoComponent::allocateGraphicsMemory()
{
#ifdef _DEBUG
    Logger::write(Logger::ZONE_DEBUG, "VideoComponent", "Allocating graphics memory for video: " + videoFile_ + ", current isPlaying_: " + std::to_string(isPlaying_));
#endif
    Component::allocateGraphicsMemory();
    if (!isPlaying_) {
        isPlaying_ = videoInst_->play(videoFile_);
#ifdef _DEBUG
        Logger::write(Logger::ZONE_DEBUG, "VideoComponent", "Play called, isPlaying_: " + std::to_string(isPlaying_));
#endif
    }
    else {
        Logger::write(Logger::ZONE_WARNING, "VideoComponent", "allocateGraphicsMemory() called while already playing: " + videoFile_);
    }
}

void VideoComponent::freeGraphicsMemory()
{
    if (videoInst_) {
        if (isPlaying_) { // VideoComponent's own isPlaying_ flag
#ifdef _DEBUG
            Logger::write(Logger::ZONE_WARNING, "VideoComponent", "Freeing graphics memory while video is playing: " + videoFile_);
#endif
            videoInst_->stop();
            // Wait for playback to fully stop
            while (videoInst_->isPlaying()) { // Now calls IVideo::isPlaying()
                Logger::write(Logger::ZONE_DEBUG, "VideoComponent", "Waiting for video to stop...");
                SDL_Delay(10);
            }
            isPlaying_ = false;
        }
        else {
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "VideoComponent", "Freeing graphics memory (video not playing): " + videoFile_);
#endif
            videoInst_->stop();
        }
    }
    Component::freeGraphicsMemory();
}


//void VideoComponent::draw()
//{
//    SDL_Rect rect;
//
//    rect.x = static_cast<int>(baseViewInfo.XRelativeToOrigin());
//    rect.y = static_cast<int>(baseViewInfo.YRelativeToOrigin());
//    rect.h = static_cast<int>(baseViewInfo.ScaledHeight());
//    rect.w = static_cast<int>(baseViewInfo.ScaledWidth());
//
//    videoInst_->draw();
//    SDL_Texture *texture = videoInst_->getTexture();
//
//    if(texture)
//    {
//        SDL::renderCopy(texture, baseViewInfo.Alpha, NULL, &rect, baseViewInfo, page.getLayoutWidth(baseViewInfo.Monitor), page.getLayoutHeight(baseViewInfo.Monitor));
//    }
//}
void VideoComponent::draw() {
    SDL_Texture* texture = videoInst_->getTexture();
    if (!texture) return;

    // Base destination rectangle using original positioning and sizing
    SDL_Rect destRect = {
        static_cast<int>(baseViewInfo.XRelativeToOrigin()),
        static_cast<int>(baseViewInfo.YRelativeToOrigin()),
        static_cast<int>(baseViewInfo.ScaledWidth()),
        static_cast<int>(baseViewInfo.ScaledHeight())
    };
    SDL_Rect srcRect = { 0, 0, static_cast<int>(baseViewInfo.ImageWidth), static_cast<int>(baseViewInfo.ImageHeight) };

    // Adjust rendering based on scaleMode
    switch (baseViewInfo.scaleMode) {
    case ViewInfo::ScaleMode::Fit: {
        float aspect = static_cast<float>(baseViewInfo.ImageWidth) / baseViewInfo.ImageHeight;
        float destAspect = static_cast<float>(destRect.w) / destRect.h;
        if (aspect > destAspect) {
            // Fit to width, adjust height
            int newHeight = static_cast<int>(destRect.w / aspect);
            destRect.y += (destRect.h - newHeight) / 2;  // Center vertically
            destRect.h = newHeight;
        }
        else {
            // Fit to height, adjust width
            int newWidth = static_cast<int>(destRect.h * aspect);
            destRect.x += (destRect.w - newWidth) / 2;  // Center horizontally
            destRect.w = newWidth;
        }
        break;
    }
    case ViewInfo::ScaleMode::Fill: {
        float aspect = static_cast<float>(baseViewInfo.ImageWidth) / baseViewInfo.ImageHeight;
        float destAspect = static_cast<float>(destRect.w) / destRect.h;
        if (aspect < destAspect) {
            // Crop top and bottom
            int newSrcHeight = static_cast<int>(destRect.w / aspect);
            srcRect.y = static_cast<int>(std::round((baseViewInfo.ImageHeight - newSrcHeight) / 2.0f));
            srcRect.h = newSrcHeight;
        }
        else {
            // Crop left and right
            int newSrcWidth = static_cast<int>(destRect.h * aspect);
            srcRect.x = static_cast<int>(std::round((baseViewInfo.ImageWidth - newSrcWidth) / 2.0f));
            srcRect.w = newSrcWidth;
        }
        break;
    }
    case ViewInfo::ScaleMode::None: {
        // Use original video size, centered
        destRect.w = static_cast<int>(baseViewInfo.ImageWidth);
        destRect.h = static_cast<int>(baseViewInfo.ImageHeight);
        destRect.x += static_cast<int>(std::round((baseViewInfo.ScaledWidth() - destRect.w) / 2.0f));  // Center horizontally
        destRect.y += static_cast<int>(std::round((baseViewInfo.ScaledHeight() - destRect.h) / 2.0f)); // Center vertically
        break;
    }
    case ViewInfo::ScaleMode::Stretch:
    default:
        // Stretch to fill destRect (matches original behavior)
        break;
    }

    // Render using SDL with layout auto-resizing
    SDL::renderCopy(texture, baseViewInfo.Alpha, &srcRect, &destRect, baseViewInfo,
        page.getLayoutWidth(baseViewInfo.Monitor), page.getLayoutHeight(baseViewInfo.Monitor));
}
bool VideoComponent::isPlaying()
{
    return isPlaying_;
}


void VideoComponent::skipForward( )
{
    if ( videoInst_ )
        videoInst_->skipForward( );
}


void VideoComponent::skipBackward( )
{
    if ( videoInst_ )
        videoInst_->skipBackward( );
}


void VideoComponent::skipForwardp( )
{
    if ( videoInst_ )
        videoInst_->skipForwardp( );
}


void VideoComponent::skipBackwardp( )
{
    if ( videoInst_ )
        videoInst_->skipBackwardp( );
}


void VideoComponent::pause( )
{
    if ( videoInst_ )
        videoInst_->pause( );
}


void VideoComponent::restart( )
{
    if ( videoInst_ )
        videoInst_->restart( );
}


unsigned long long VideoComponent::getCurrent( )
{
    if ( videoInst_ )
        return videoInst_->getCurrent( );
    else
        return 0;
}


unsigned long long VideoComponent::getDuration( )
{
    if ( videoInst_ )
        return videoInst_->getDuration( );
    else
        return 0;
}


bool VideoComponent::isPaused( )
{
    if ( videoInst_ )
        return videoInst_->isPaused( );
    else
        return false;
}
