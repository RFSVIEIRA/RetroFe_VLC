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

#include "LibVLCVideo.h"
#include "../Graphics/ViewInfo.h"
#include "../Graphics/Component/Image.h"
#include "../Graphics/Component/AnimatedImage.h"
#include "../Database/Configuration.h"
#include "../Utility/Log.h"
#include "../Utility/Utils.h"
#include "../SDL.h"
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <SDL.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <vlc/vlc.h>


const char* videoFormat = "RV32";  // Allocate memory if needed
bool LibVLCVideo::initialized_ = false;

LibVLCVideo::LibVLCVideo(int monitor)

    :vlcInstance(NULL),
    mediaPlayer(NULL),
    media(NULL),
	tracks(NULL),
    monitor_(monitor),
	numTracks(0),
    isPlaying_(false),
    frameReady_(false),
    playCount_(0),
    numLoops_(0),
    height_(0),
    width_(0),
    volume_(0.0),
    currentVolume_(0.0),
    currentInstanceIndex(0),
    hasVideo(0),
    hasAudio(0)

{
    ctx.renderer = SDL::getRenderer(monitor_);
    ctx.texture = nullptr;
    ctx.n = 0;
    ctx.pixels = nullptr;

    const char* vlc_args[] = {

        "--no-video-title-show", // Prevent title overlay
        "--vout=vmem",          // Use video memory output (custom rendering)
        /*"--avcodec-fast",*/
       // "--avcodec-hw=dxva2",   // Use DirectX VAAPI (Windows, NVIDIA/AMD)
        "--file-caching=1000",
      //  "--avcodec-hw=any",      // Hardware acceleration
        "--quiet"
    };
    for (int i = 0; i < 5; ++i) {
        libvlc_instance_t* instance = libvlc_new(sizeof(vlc_args) / sizeof(vlc_args[0]), vlc_args);
        if (instance) {
            libvlc_log_set(instance, logCallback, this);
            vlcInstances.push_back(instance);
        }
        else {
            Logger::write(Logger::ZONE_DEBUG, "Video", "LibVLC initialization failure.");
        }
    }
    paused_ = false;


}
LibVLCVideo::~LibVLCVideo()
{
    stop(); // release mediaPlayer, media, tracks, and texture
    for (libvlc_instance_t* instance : vlcInstances) {
        if (instance) {
            libvlc_release(instance);
        }
    }
    vlcInstances.clear();


}
#ifdef _DEBUG

void  LibVLCVideo::logCallback(void* data, int level, const libvlc_log_t* ctx, const char* fmt, va_list args) {
 
    if (level > 0) {
        char message[1024];
        vsnprintf(message, sizeof(message), fmt, args);
        printf("LibVLC Log (Level %d): %s\n", level, message);
    }
}
#else
void LibVLCVideo::logCallback(void* /*data*/, int /*level*/, const libvlc_log_t* /*ctx*/, const char* /*fmt*/, va_list /*args*/) {
	// nothing here no logging in release mode
}
#endif // DEBUG
void LibVLCVideo::setNumLoops(int n)
{
    if (n > 0)
        numLoops_ = n;
}

SDL_Texture* LibVLCVideo::getTexture() const
{
    return ctx.texture;
}


bool LibVLCVideo::initialize()
{
    if (initialized_)
    {
        return true;
    }

#if (WIN32)
    std::string path = Utils::combinePath(Configuration::absolutePath, "Core");
#elif (_WIN64)
    std::string path = Utils::combinePath(Configuration::absolutePath, "Corex64");
#endif // (WIN32)

    // Initialise libVLC.
    libvlc_instance_t* currentInstance = vlcInstances[currentInstanceIndex];


    // Update the currentInstanceIndex for the next call
    currentInstanceIndex = (currentInstanceIndex + 1) % vlcInstances.size();

    initialized_ = true;
    paused_ = false;

    return true;
}

bool LibVLCVideo::deInitialize()
{
    Logger::write(Logger::ZONE_INFO, "LibVLCVideo", "Deinitializing LibVLCVideo");

    // Stop playback and release media-related resources
    if (!stop()) {
        Logger::write(Logger::ZONE_WARNING, "LibVLCVideo", "Failed to stop playback during deinitialization");
    }

    // Release media tracks if they’re still allocated
    if (tracks != NULL) {
        libvlc_media_tracks_release(tracks, numTracks);
        tracks = NULL;
        numTracks = 0;
    }

    // Release all VLC instances in the vector
    for (auto* instance : vlcInstances) {
        if (instance) {
            Logger::write(Logger::ZONE_INFO, "LibVLCVideo", "Releasing VLC instance from vector");
            libvlc_release(instance);
        }
    }
    vlcInstances.clear();
    currentInstanceIndex = 0;

    // Release the single vlcInstance (if still used)
    if (vlcInstance) {
        Logger::write(Logger::ZONE_INFO, "LibVLCVideo", "Releasing single VLC instance");
        libvlc_release(vlcInstance);
        vlcInstance = nullptr;
    }

    if (ctx.texture) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_DEBUG, "Video", "Destroying texture in stop()");
#endif
        SDL_DestroyTexture(ctx.texture);
        /* ctx.texture = SDL_CreateTexture(ctx.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width_, height_);
         SDL_SetTextureBlendMode(ctx.texture, SDL_BLENDMODE_BLEND);*/
        ctx.texture = nullptr;
    }

    initialized_ = false;
    paused_ = false; // Added to match your version
    return true;
}


bool LibVLCVideo::stop()
{
    paused_ = false;
    if (!initialized_) return false;

    if (mediaPlayer) {
        libvlc_audio_set_mute(mediaPlayer, 1);
        libvlc_media_player_stop(mediaPlayer);
        libvlc_state_t state;
        do {
            state = libvlc_media_player_get_state(mediaPlayer);
            if (state != libvlc_Stopped && state != libvlc_Ended && state != libvlc_Error) {
                Logger::write(Logger::ZONE_DEBUG, "Video", "Waiting for media player to stop, current state: " + std::to_string(state));
               // SDL_Delay(10);
            }
        } while (state != libvlc_Stopped && state != libvlc_Ended && state != libvlc_Error);
        freeElements();  // Release resources immediately
        mediaPlayer = nullptr;  // Ensure pointer is cleared
    }

//    if (ctx.texture) {
//#ifdef _DEBUG
//        Logger::write(Logger::ZONE_DEBUG, "Video", "Destroying texture in stop()");
//#endif
//        SDL_DestroyTexture(ctx.texture);
//       /* ctx.texture = SDL_CreateTexture(ctx.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width_, height_);
//        SDL_SetTextureBlendMode(ctx.texture, SDL_BLENDMODE_BLEND);*/
//        ctx.texture = nullptr;
//    }

  //  freeElements();
    isPlaying_ = false;
    height_ = 0;
    width_ = 0;
    frameReady_ = false;
    return true;
}

void LibVLCVideo::freeElements()
{
    // Release media player and media resources
    if (mediaPlayer) {
        libvlc_media_player_stop(mediaPlayer);
        libvlc_media_player_release(mediaPlayer);
    }
    if (media) {
        libvlc_media_release(media);
    }
    media = NULL;
    mediaPlayer = NULL;
   // paused_ = false;
}


bool LibVLCVideo::play(std::string file)
{
    if (isPlaying_) {
        stop();  // Ensure previous playback is fully stopped before starting new one
       SDL_Delay(10);  //delay to ensure audio stops
    }

    playCount_ = 0;

    if (!initialized_)
    {
        return false;
    }

    if (vlcInstances.empty() || currentInstanceIndex >= vlcInstances.size()) {
        Logger::write(Logger::ZONE_ERROR, "Video", "Invalid VLC instance index");
        return false;
    }

    // Get the current libvlc instance
    libvlc_instance_t* currentInstance = vlcInstances[currentInstanceIndex];
    currentFile_ = file;
  //  stop();  // Ensure previous playback is fully stopped

    const char* uriFile = file.c_str();
    if (!uriFile || std::strlen(uriFile) == 0) {
        Logger::write(Logger::ZONE_DEBUG, "Video", "Could not convert video/audio string to char.");
        return false;
    }

    // Create new media
    media = libvlc_media_new_path(currentInstance, uriFile);
    if (!media) {
        Logger::write(Logger::ZONE_DEBUG, "Video", "Could not create Media");
        freeElements();
        return false;
    }

    mediaPlayer = libvlc_media_player_new_from_media(media);
    if (!mediaPlayer) {
        Logger::write(Logger::ZONE_DEBUG, "Video", "Could not create Media Player");
        freeElements();
        return false;
    }
    currentVolume_ = volume_; // Sync immediately
    libvlc_audio_set_volume(mediaPlayer, static_cast<int>(volume_ * 100));
    libvlc_audio_set_mute(mediaPlayer, (volume_ < 0.01f) ? 1 : 0);

    // Parse media to get size and track info
    libvlc_media_parse(media);
    libvlc_video_get_size(mediaPlayer, 0, &width_, &height_);

    // Check track information
    numTracks = libvlc_media_tracks_get(media, &tracks);
    if (!tracks) {
        Logger::write(Logger::ZONE_ERROR, "Video", "Failed to retrieve track information.");
        freeElements();
        return false;
    }

    hasAudio = hasVideo = 0;
    for (unsigned int i = 0; i < numTracks; i++) {
        if (tracks[i]->i_type == libvlc_track_audio) hasAudio = 1;
        if (tracks[i]->i_type == libvlc_track_video) hasVideo = 1;
    }
    libvlc_media_tracks_release(tracks, numTracks);
#ifdef _DEBUG
    // Log media type
    if (hasAudio && !hasVideo) {
        Logger::write(Logger::ZONE_DEBUG, "Video", "This is an audio file.");
    } else if (!hasAudio && hasVideo) {
        Logger::write(Logger::ZONE_DEBUG, "Video", "This is a video file.");
    } else if (hasAudio && hasVideo) {
        Logger::write(Logger::ZONE_DEBUG, "Video", "This is a multimedia file (audio + video).");
    } else {
        Logger::write(Logger::ZONE_DEBUG, "Video", "This file has no audio or video tracks.");
    }
#endif

    if (hasVideo && width_ > 0 && height_ > 0) {
        if (ctx.texture) {
            int w, h;
            SDL_QueryTexture(ctx.texture, NULL, NULL, &w, &h);
            Logger::write(Logger::ZONE_DEBUG, "Video", "Existing texture size: " + std::to_string(w) + "x" + std::to_string(h));
            if (w != width_ || h != height_) {
                SDL_DestroyTexture(ctx.texture);
                ctx.texture = SDL_CreateTexture(ctx.renderer, SDL_PIXELFORMAT_ARGB8888,
                    SDL_TEXTUREACCESS_STREAMING, width_, height_);
                Logger::write(Logger::ZONE_DEBUG, "Video", "Recreated texture with size: " + std::to_string(width_) + "x" + std::to_string(height_));
            }
        }
        else {
            ctx.texture = SDL_CreateTexture(ctx.renderer, SDL_PIXELFORMAT_ARGB8888,
                SDL_TEXTUREACCESS_STREAMING, width_, height_);
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "Video", "Created new texture with size: " + std::to_string(width_) + "x" + std::to_string(height_));
#endif
        }
        if (!ctx.texture) {
            Logger::write(Logger::ZONE_ERROR, "Video", "Failed to create SDL_Texture: " + std::string(SDL_GetError()));
            freeElements();
            return false;
        }
        SDL_SetTextureBlendMode(ctx.texture, SDL_BLENDMODE_BLEND);
    }
    // Set looping
    std::string loopOption = (numLoops_ == 0) ? ":input-repeat=65535" : ":input-repeat=" + std::to_string(numLoops_ - 1);
    libvlc_media_add_option(media, loopOption.c_str());

    ctx.pixels = nullptr;
    isPlaying_ = true;

    // Set media and callbacks
    libvlc_media_player_set_media(mediaPlayer, media);
    if (hasVideo) {
        libvlc_video_set_format(mediaPlayer, videoFormat, width_, height_, width_ * 4);
        libvlc_video_set_callbacks(mediaPlayer, lock, unlock, nullptr, this);
    }

    // Start playback
    libvlc_media_player_play(mediaPlayer);

    if (!hasVideo && hasAudio && volume_ == 0) {
        Logger::write(Logger::ZONE_DEBUG, "Audio file no sound", "Could not get video size");
        libvlc_media_player_set_pause(mediaPlayer, 1);
    } else {
        libvlc_media_player_set_pause(mediaPlayer, 0);
    }

    currentInstanceIndex = (currentInstanceIndex + 1) % vlcInstances.size();

    libvlc_state_t playerState = libvlc_media_player_get_state(mediaPlayer);
    if (playerState == libvlc_Error) {
        isPlaying_ = false;
        Logger::write(Logger::ZONE_ERROR, "Video", "Error: Unable to set the pipeline to the playing state.");
        freeElements();
        return false;
    }

    return true;
}
 
//void LibVLCVideo::update(float dt) {
//    SDL_LockMutex(SDL::getMutex());
//
//    if (mediaPlayer) {
//        // Clamp target volume to [0.0, 1.0]
//        volume_ = std::max(0.0f, std::min(1.0f, volume_));
//
//        // If target is 0, immediately mute and set volume to 0
//        if (volume_ == 0.0f) {
//            if (currentVolume_ != 0.0f) {
//                currentVolume_ = 0.0f;
//                libvlc_audio_set_volume(mediaPlayer, 0);
//                libvlc_audio_set_mute(mediaPlayer, 1);
//                libvlc_media_player_set_pause(mediaPlayer, 1);
//#ifdef _DEBUG
//                Logger::write(Logger::ZONE_INFO, "Audio", "Immediate mute: volume_ = 0, currentVolume_ = 0");
//#endif
//            }
//        }
//        else {
//            // Smooth transition for non-zero target volume
//            if (std::abs(volume_ - currentVolume_) >= 0.005f) {
//                const float step = 0.1f * dt; // Adjust speed with dt (0.1 per second)
//                if (volume_ > currentVolume_) {
//                    currentVolume_ = std::min(currentVolume_ + step, volume_);
//                }
//                else {
//                    currentVolume_ = std::max(currentVolume_ - step, volume_);
//                }
//#ifdef _DEBUG
//                Logger::write(Logger::ZONE_INFO, "Audio", "Smoothing: volume_ = " + std::to_string(volume_) +
//                    ", currentVolume_ = " + std::to_string(currentVolume_));
//#endif
//            }
//        }
//
//        // Convert to VLC volume (0-100 scale)
//        int vlcVolume = static_cast<int>(currentVolume_ * 100);
//        vlcVolume = std::max(0, std::min(100, vlcVolume));
//
//        // Apply volume only if it changed
//        static int lastSetVolume = -1;
//        if (vlcVolume != lastSetVolume) {
//            libvlc_audio_set_volume(mediaPlayer, vlcVolume);
//            lastSetVolume = vlcVolume;
//#ifdef _DEBUG
//            Logger::write(Logger::ZONE_INFO, "Audio", "Volume Updated: vlcVolume = " + std::to_string(vlcVolume));
//#endif
//        }
//
//        // Apply mute state for very low volume (except when volume_ is 0)
//        bool shouldMute = (volume_ > 0.0f && currentVolume_ < 0.005f);
//        libvlc_audio_set_mute(mediaPlayer, shouldMute ? 1 : 0);
//
//        // Ensure playback resumes or pauses
//        if (shouldMute && libvlc_media_player_get_state(mediaPlayer) != libvlc_Paused) {
//            libvlc_media_player_set_pause(mediaPlayer, 1);
//        }
//        else if (!shouldMute && volume_ > 0.0f && libvlc_media_player_get_state(mediaPlayer) == libvlc_Paused) {
//            libvlc_media_player_set_pause(mediaPlayer, 0);
//        }
//
//        if (libvlc_media_player_get_state(mediaPlayer) == libvlc_Ended) {
//            isPlaying_ = false;
//        }
//    }
//
//    SDL_UnlockMutex(SDL::getMutex());
//}


void LibVLCVideo::update(float /*dt*/) {
    SDL_LockMutex(SDL::getMutex());

    if (mediaPlayer) {
        // Clamp target volume to [0.0, 1.0]
        volume_ = std::max(0.0f, std::min(1.0f, volume_));

        // If no significant change, skip updates
        if (std::abs(volume_ - currentVolume_) <= 0.01f) {
            SDL_UnlockMutex(SDL::getMutex());
            // Check if playback ended
            if (libvlc_media_player_get_state(mediaPlayer) == libvlc_Ended) {
                isPlaying_ = false;
            }
            return;
        }

        // Smooth transition: Gradually adjust volume toward target
        const float step = 0.03f;  // Adjust for smoother transitions
        currentVolume_ += (volume_ > currentVolume_) ? step : -step;

        // Convert to VLC volume (0-100 scale)
        int vlcVolume = static_cast<int>(currentVolume_ * 100);
        vlcVolume = std::max(3, vlcVolume); // Ensure minimum volume

        // Apply volume only if it changed
        static int lastSetVolume = -1;
        if (vlcVolume != lastSetVolume) {
            libvlc_audio_set_volume(mediaPlayer, vlcVolume);
            lastSetVolume = vlcVolume;
#ifdef _DEBUG
            Logger::write(Logger::ZONE_INFO, "Audio", "Volume Updated: " + std::to_string(vlcVolume));
#endif
        }

        // Handle mute state
        bool shouldMute = (currentVolume_ < 0.01f);
        libvlc_audio_set_mute(mediaPlayer, shouldMute ? 1 : 0);

        // Ensure playback resumes when unmuted
        if (!shouldMute && libvlc_media_player_get_state(mediaPlayer) == libvlc_Paused) {
            libvlc_media_player_set_pause(mediaPlayer, 0);
        }
        else if (shouldMute) {
            libvlc_media_player_set_pause(mediaPlayer, 1);
        }

        // Check if playback ended
        if (libvlc_media_player_get_state(mediaPlayer) == libvlc_Ended) {
            isPlaying_ = false;
        }
    }

    SDL_UnlockMutex(SDL::getMutex());
}

//
//void LibVLCVideo::update(float /*dt*/)
//{
//    SDL_LockMutex(SDL::getMutex());
//
//    if (mediaPlayer) {
//        // Clamp target volume to [0.0, 1.0]
//        volume_ = std::max(0.0f, std::min(1.0f, volume_));
//
//        // Smooth volume transition using a gradually smaller step as volume approaches target
//        const float maxStep = 0.5f;  // Much smaller step size to prevent noise
//        const float minStep = 0.5f; // Very small step size to avoid distortion
//        float distance = std::abs(volume_ - currentVolume_);
//        float step = std::max(minStep, std::min(maxStep, distance));  // Smaller steps as we get closer to the target
//        
//
//        // Apply the step size to smoothly adjust the volume
//        if (distance > 0.09f) {
//            currentVolume_ += (volume_ > currentVolume_) ? step : -step;
//        }
//        else {
//            currentVolume_ = volume_;
//        }
//
//        // Ensure minimum volume
//        int vlcVolume = static_cast<int>(currentVolume_ * 100);
//        vlcVolume = std::max(10, vlcVolume);
//#ifdef _DEBUG
//        // Log the calculated volume
//        Logger::write(Logger::ZONE_INFO, "Audio", "Target Volume: " + std::to_string(volume_));
//        Logger::write(Logger::ZONE_INFO, "Audio", "Current Volume: " + std::to_string(currentVolume_));
//        Logger::write(Logger::ZONE_INFO, "Audio", "VLC Volume Set To: " + std::to_string(vlcVolume));
//#endif
//        // Apply volume
//        libvlc_audio_set_mute(mediaPlayer, 0);
//        libvlc_audio_set_volume(mediaPlayer, vlcVolume);
//
//        // Check the actual VLC volume after setting
//        int actualVolume = libvlc_audio_get_volume(mediaPlayer);
//#ifdef _DEBUG
//        Logger::write(Logger::ZONE_INFO, "Audio", "VLC Actual Volume: " + std::to_string(actualVolume));
// #endif
//
//        // Handle muting and pausing independently
//        bool shouldMute = (currentVolume_ < 0.005f);
//        libvlc_audio_set_mute(mediaPlayer, shouldMute ? 1 : 0);
//
//        // Ensure playback resumes if volume increases from mute state
//        if (!shouldMute && libvlc_media_player_get_state(mediaPlayer) == libvlc_Paused) {
//            libvlc_media_player_set_pause(mediaPlayer, 0);
//            libvlc_media_player_set_time(mediaPlayer, 0);
//        }
//        else if (shouldMute) {
//            libvlc_media_player_set_pause(mediaPlayer, 1);
//        }
//
//        if (libvlc_media_player_get_state(mediaPlayer) == libvlc_Ended) {
//            isPlaying_ = false;
//        }
//    }
//
//    SDL_UnlockMutex(SDL::getMutex());
//}


bool textureLocked = false; // Track if lock() succeeded

void* LibVLCVideo::lock(void* data, void** p_pixels) {
    LibVLCVideo* self = static_cast<LibVLCVideo*>(data);
    SDL_LockMutex(SDL::getMutex());
    if (!self->ctx.texture) {
        Logger::write(Logger::ZONE_ERROR, "Video", "Texture is NULL in lock(). File: " + self->currentFile_);
        SDL_UnlockMutex(SDL::getMutex());
        return nullptr;
    }
    int pitch;
    if (SDL_LockTexture(self->ctx.texture, NULL, (void**)&self->ctx.pixels, &pitch) != 0) {
        Logger::write(Logger::ZONE_ERROR, "Video", "Error locking texture: " + std::string(SDL_GetError()));
        SDL_UnlockMutex(SDL::getMutex());
        return nullptr;
    }
    *p_pixels = self->ctx.pixels;
    self->frameReady_ = true; // Mark frame as ready
    SDL_UnlockMutex(SDL::getMutex());
    return nullptr;
}



void LibVLCVideo::unlock(void* data, void* id, void* const* p_pixels) {
    LibVLCVideo* self = static_cast<LibVLCVideo*>(data);
    if (!self->ctx.texture) {
        return; // Avoid unlocking a null texture
        Logger::write(Logger::ZONE_ERROR, "Video", "Texture is NULL in unlock().");
    }

    SDL_LockMutex(SDL::getMutex()); // Lock before unlocking texture
    SDL_UnlockTexture(self->ctx.texture);
    SDL_UnlockMutex(SDL::getMutex()); // Unlock mutex
}






int LibVLCVideo::getHeight()
{
    return static_cast<int>(height_);
}

int LibVLCVideo::getWidth()
{
    return static_cast<int>(width_);
}


void LibVLCVideo::draw()
{
    frameReady_ = false;
}

bool LibVLCVideo::isPlaying()
{
    return isPlaying_;
}


void LibVLCVideo::setVolume(float volume)
{
    volume_ = volume;
}


void LibVLCVideo::skipForward()
{
    if (!isPlaying_ || !mediaPlayer)
        return;

    libvlc_time_t current = libvlc_media_player_get_time(mediaPlayer);
    libvlc_time_t duration = libvlc_media_get_duration(media);

    if (current == -1 || duration == -1) {
        Logger::write(Logger::ZONE_DEBUG, "Video", "Failed to get current time or duration for skipForward");
        return;
    }

    // Skip forward by 10 seconds (10000 milliseconds)
    current += 10000;
    if (current >= duration)
        current = duration - 1; // Stay within bounds, leave 1ms before end

    libvlc_media_player_set_time(mediaPlayer, current);
}

void LibVLCVideo::skipBackward()
{
    if (!isPlaying_ || !mediaPlayer)
        return;

    libvlc_time_t current = libvlc_media_player_get_time(mediaPlayer);

    if (current == -1) {
        Logger::write(Logger::ZONE_DEBUG, "Video", "Failed to get current time for skipBackward");
        return;
    }

    // Skip backward by 10 seconds (10000 milliseconds)
    if (current > 10000)
        current -= 10000;
    else
        current = 0;

    libvlc_media_player_set_time(mediaPlayer, current);
}


void LibVLCVideo::skipForwardp()
{
    if (!isPlaying_ || !mediaPlayer)
        return;

    libvlc_time_t current = libvlc_media_player_get_time(mediaPlayer);
    libvlc_time_t duration = libvlc_media_get_duration(media);

    if (current == -1 || duration == -1) {
        Logger::write(Logger::ZONE_DEBUG, "Video", "Failed to get current time or duration for skipForwardp");
        return;
    }

    // Skip forward by 5% of the duration (in milliseconds)
    current += duration / 20; // 1/20th = 5%
    if (current >= duration)
        current = duration - 1; // Stay within bounds

    libvlc_media_player_set_time(mediaPlayer, current);
}

void LibVLCVideo::skipBackwardp()
{
    if (!isPlaying_) {
        return;
    }

    libvlc_time_t current = libvlc_media_player_get_time(mediaPlayer) * 1000000;
    libvlc_time_t duration = libvlc_media_get_duration(media) * 1000000;

    if (current > duration / 20)
        current -= duration / 20;
    else
        current = 0;
    libvlc_media_player_set_time(mediaPlayer, current);


}


void LibVLCVideo::pause()
{
    paused_ = !paused_;
    if (paused_)
        // Pause media playback
        libvlc_media_player_set_pause(mediaPlayer, 1);
    else
        // Pause media playback
        libvlc_media_player_set_pause(mediaPlayer, 0);
}


void LibVLCVideo::restart()
{

    if (!isPlaying_)
        return;

    // Stop media player
    libvlc_media_player_stop(mediaPlayer);

    // Set the time back to the beginning (or desired starting position)
    libvlc_media_player_set_time(mediaPlayer, 0); // Set to 0 milliseconds for the beginning
	
    // Play the media again to restart
    libvlc_media_player_play(mediaPlayer);

}


unsigned long long LibVLCVideo::getCurrent()
{
    libvlc_time_t ret = 0;

    if (!libvlc_media_player_is_playing(mediaPlayer)) {
        ret = 0;
    }
    else {
        ret = libvlc_media_player_get_time(mediaPlayer) * static_cast<long long> (1000000);  // Convert milliseconds to nanoseconds
    }

    return static_cast<unsigned long long>(ret);

}


unsigned long long LibVLCVideo::getDuration()
{
    libvlc_time_t ret = 0;

    if (!libvlc_media_player_is_playing(mediaPlayer)) {
        ret = 0;
    }
    else {
        libvlc_media_t* media = libvlc_media_player_get_media(mediaPlayer);
        if (media != nullptr) {
            ret = libvlc_media_get_duration(media) * static_cast<long long> (1000000);  // Convert milliseconds to nanoseconds

        }
    }

    return static_cast<unsigned long long>(ret);
}


bool LibVLCVideo::isPaused()
{
    return paused_;
}
//Wen reboot this will manage the libvlc Window to not duplicate creating a bug of no image and more one window
void LibVLCVideo::reset()
{
    stop(); // Stop playback and release media resources
    if (ctx.texture)
    {
        SDL_DestroyTexture(ctx.texture);
        ctx.texture = nullptr;
    }
    frameReady_ = false;
    isPlaying_ = false;
    paused_ = false;
    playCount_ = 0;
   
    Logger::write(Logger::ZONE_INFO, "LibVLCVideo", "Reset video state for monitor " + std::to_string(monitor_));
}