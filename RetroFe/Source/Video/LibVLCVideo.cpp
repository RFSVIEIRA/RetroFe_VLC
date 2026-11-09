/* This file is part of RetroFeVLC.
 *
 * RetroFeVLC is free software: Fork of RetroFE you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * RetroFE / RetroFeVLC is distributed in the hope that it will be useful,
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
#include <algorithm>


const char* videoFormat = "RV32";
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
    volume_(0.0f),
    currentVolume_(0.0f),
    lastSetVolume_(-1),
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
        // "--avcodec-hw=dxva2",   // In 2.1.63 dxva2 was disabled to avoid hardware device negotiation latency
        "--file-caching=1000",
        "--no-keyboard-events",
        "--no-xlib",
        "--aout=directsound",
        "--no-stats",
        "--no-osd",
        "--no-snapshot-preview",
        "--no-spu",
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
#ifdef _DEBUG
        Logger::write(Logger::ZONE_WARNING, "LibVLCVideo", "Failed to stop playback during deinitialization");
#endif
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
#ifdef _DEBUG
            Logger::write(Logger::ZONE_INFO, "LibVLCVideo", "Releasing VLC instance from vector");
#endif
            libvlc_release(instance);
        }
    }
    vlcInstances.clear();
    currentInstanceIndex = 0;

    // Release the single vlcInstance (if still used)
    if (vlcInstance) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_INFO, "LibVLCVideo", "Releasing single VLC instance");
#endif
        libvlc_release(vlcInstance);
        vlcInstance = nullptr;
    }

    if (ctx.texture) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_DEBUG, "Video", "Destroying texture in stop()");
#endif
        SDL_DestroyTexture(ctx.texture);
        ctx.texture = nullptr;
    }

    initialized_ = false;
    paused_ = false;
    lastSetVolume_ = -1;
    return true;
}


bool LibVLCVideo::stop()
{
    paused_ = false;
    if (!initialized_) return false;

    if (mediaPlayer) {
        libvlc_audio_set_mute(mediaPlayer, 1);
        // Instant non-blocking stop: release media and player immediately

        freeElements();  // Release resources immediately
        mediaPlayer = nullptr;  // Ensure pointer is cleared
    }

    isPlaying_ = false;
    height_ = 0;
    width_ = 0;
    frameReady_ = false;
    lastSetVolume_ = -1;
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
}


bool LibVLCVideo::play(std::string file)
{
    if (isPlaying_) {
        stop();  // Ensure previous playback is fully stopped before starting new one
    }

    playCount_ = 0;
    lastSetVolume_ = -1;

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
    tracks = NULL;
    numTracks = 0;
#ifdef _DEBUG
    // Log media type
    if (hasAudio && !hasVideo) {
        Logger::write(Logger::ZONE_DEBUG, "Video", "This is an audio file.");
    }
    else if (!hasAudio && hasVideo) {
        Logger::write(Logger::ZONE_DEBUG, "Video", "This is a video file.");
    }
    else if (hasAudio && hasVideo) {
        Logger::write(Logger::ZONE_DEBUG, "Video", "This is a multimedia file (audio + video).");
    }
    else {
        Logger::write(Logger::ZONE_DEBUG, "Video", "This file has no audio or video tracks.");
    }
#endif

    if (hasVideo && width_ > 0 && height_ > 0) {
        if (ctx.texture) {
            int w, h;
            SDL_QueryTexture(ctx.texture, NULL, NULL, &w, &h);
            Logger::write(Logger::ZONE_DEBUG, "Video", "Existing texture size: " + std::to_string(w) + "x" + std::to_string(h));
            if (w != static_cast<int>(width_) || h != static_cast<int>(height_)) {
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
    int result = libvlc_media_player_play(mediaPlayer);
    if (result == -1)
    {
        Logger::write(Logger::ZONE_ERROR, "Video", "Failed to start playback");
        freeElements();
        return false;
    }

    // === WINDOWS AUDIO FIX - Respect layout volume ===
    int vlcVolume = static_cast<int>(volume_ * 100.0f);

    libvlc_audio_set_volume(mediaPlayer, vlcVolume);
    libvlc_audio_set_mute(mediaPlayer, (vlcVolume < 5) ? 1 : 0);
    lastSetVolume_ = vlcVolume;
#ifdef _DEBUG
    Logger::write(Logger::ZONE_INFO, "Video",
        "Video started: " + file + " | Volume: " + std::to_string(vlcVolume));
#endif
    if (!hasVideo && hasAudio && volume_ == 0) {
        Logger::write(Logger::ZONE_DEBUG, "Audio file no sound", "Could not get video size");
        libvlc_media_player_set_pause(mediaPlayer, 1);
    }
    else {
        libvlc_media_player_set_pause(mediaPlayer, 0);
    }

    currentInstanceIndex = (currentInstanceIndex + 1) % vlcInstances.size();

    libvlc_state_t playerState = libvlc_media_player_get_state(mediaPlayer);
    if (playerState == libvlc_Error) {
        isPlaying_ = false;
#ifdef _DEBUG
        Logger::write(Logger::ZONE_ERROR, "Video", "Error: Unable to set the pipeline to the playing state.");
#endif
        freeElements();
        return false;
    }

    return true;
}


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

        // Apply volume only if it changed (per-instance, no longer shared static)
        if (vlcVolume != lastSetVolume_) {
            libvlc_audio_set_volume(mediaPlayer, vlcVolume);
            lastSetVolume_ = vlcVolume;
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
        else if (shouldMute && !hasVideo && hasAudio) {
            libvlc_media_player_set_pause(mediaPlayer, 1);
        }

        // Check if playback ended
        if (libvlc_media_player_get_state(mediaPlayer) == libvlc_Ended) {
            isPlaying_ = false;
        }
    }

    SDL_UnlockMutex(SDL::getMutex());
}



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



void LibVLCVideo::unlock(void* data, void* /*id*/, void* const* /*p_pixels*/) {
    LibVLCVideo* self = static_cast<LibVLCVideo*>(data);
    if (!self->ctx.texture) {
        return; // Avoid unlocking a null texture
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
    volume_ = std::clamp(volume, 0.0f, 1.0f);

    if (mediaPlayer)
    {
        int vlcVol = static_cast<int>(volume_ * 100.0f);
        if (vlcVol != lastSetVolume_)
        {
            libvlc_audio_set_volume(mediaPlayer, vlcVol);
            libvlc_audio_set_mute(mediaPlayer, (vlcVol < 5) ? 1 : 0);
            lastSetVolume_ = vlcVol;
        }
    }
}


void LibVLCVideo::skipForward()
{
    if (!isPlaying_ || !mediaPlayer) return;
    libvlc_time_t current = libvlc_media_player_get_time(mediaPlayer);
    libvlc_time_t duration = libvlc_media_get_duration(media);
    if (current == -1 || duration == -1) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_DEBUG, "Video", "skipForward: Failed to get time/duration");
#endif
        return;
    }
    current += 10000; // +10 seconds
    if (current >= duration)
        current = duration - 1000;
    libvlc_media_player_set_time(mediaPlayer, current);
}
void LibVLCVideo::skipBackward()
{
    if (!isPlaying_ || !mediaPlayer) return;
    libvlc_time_t current = libvlc_media_player_get_time(mediaPlayer);
    if (current == -1) return;
    current -= 10000; // -10 seconds
    if (current < 0)
        current = 0;
    libvlc_media_player_set_time(mediaPlayer, current);
}
void LibVLCVideo::skipForwardp()
{
    if (!isPlaying_ || !mediaPlayer) return;
    libvlc_time_t current = libvlc_media_player_get_time(mediaPlayer);
    libvlc_time_t duration = libvlc_media_get_duration(media);
    if (current == -1 || duration == -1) return;
    current += duration / 20; // +5%
    if (current >= duration)
        current = duration - 1000;
    libvlc_media_player_set_time(mediaPlayer, current);
}
void LibVLCVideo::skipBackwardp()
{
    if (!isPlaying_ || !mediaPlayer) return;
    libvlc_time_t current = libvlc_media_player_get_time(mediaPlayer);
    libvlc_time_t duration = libvlc_media_get_duration(media);
    if (current == -1 || duration == -1) return;
    libvlc_time_t skipAmount = duration / 20; // 5%
    if (current > skipAmount)
        current -= skipAmount;
    else
        current = 0;
    libvlc_media_player_set_time(mediaPlayer, current);
}
void LibVLCVideo::pause()
{
    if (!mediaPlayer) return;
    paused_ = !paused_;
    libvlc_media_player_set_pause(mediaPlayer, paused_ ? 1 : 0);
}
void LibVLCVideo::restart()
{
    if (!mediaPlayer) return;
    libvlc_media_player_stop(mediaPlayer);
    libvlc_media_player_set_time(mediaPlayer, 0);
    libvlc_media_player_play(mediaPlayer);
}


unsigned long long LibVLCVideo::getCurrent()
{
    if (!mediaPlayer)
        return 0;

    libvlc_time_t t = libvlc_media_player_get_time(mediaPlayer);
    if (t < 0)
        return 0;

    return static_cast<unsigned long long>(t) * 1000000ULL;
}


unsigned long long LibVLCVideo::getDuration()
{
    if (!mediaPlayer)
        return 0;

    libvlc_time_t d = libvlc_media_player_get_length(mediaPlayer);
    if (d <= 0 && media)
        d = libvlc_media_get_duration(media);
    if (d < 0)
        return 0;

    return static_cast<unsigned long long>(d) * 1000000ULL;
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
    // Refresh renderer pointer in case SDL reinitialized (reboot)
    ctx.renderer = SDL::getRenderer(monitor_);

    frameReady_ = false;
    isPlaying_ = false;
    paused_ = false;
    playCount_ = 0;
    lastSetVolume_ = -1;
#ifdef _DEBUG
    Logger::write(Logger::ZONE_INFO, "LibVLCVideo", "Reset video state for monitor " + std::to_string(monitor_));
#endif
}

void LibVLCVideo::adjustVolume(float delta)
{
    if (!mediaPlayer) return;
    volume_ = std::clamp(volume_ + delta, 0.0f, 1.0f);

    int vlcVol = static_cast<int>(volume_ * 100);
    libvlc_audio_set_volume(mediaPlayer, vlcVol);
    libvlc_audio_set_mute(mediaPlayer, (vlcVol < 5) ? 1 : 0);
}

void LibVLCVideo::toggleMute()
{
    if (!mediaPlayer) return;
    bool currentlyMuted = (volume_ < 0.01f);
    volume_ = currentlyMuted ? 0.7f : 0.0f;  // toggle between 70% and mute

    int vlcVol = static_cast<int>(volume_ * 100);
    libvlc_audio_set_volume(mediaPlayer, vlcVol);
    libvlc_audio_set_mute(mediaPlayer, (vlcVol < 5) ? 1 : 0);
}
bool LibVLCVideo::isMuted()
{
    if (!mediaPlayer)
        return (volume_ < 0.01f);

    return libvlc_audio_get_mute(mediaPlayer) != 0 || (volume_ < 0.01f);
  
}

float LibVLCVideo::getVolume()
{
    if (!mediaPlayer)
        return volume_;

    int vlcVol = libvlc_audio_get_volume(mediaPlayer);

    if (vlcVol < 0)
        return volume_;

    volume_ = vlcVol / 100.0f;

    return volume_;
}
