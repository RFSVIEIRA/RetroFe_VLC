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
#pragma once

#ifndef LIBVLCVIDEO_H
#define LIBVLCVIDEO_H

#include "IVideo.h"
#include <vector>
#include <string>
#include <SDL.h>
#include <vlc/vlc.h>

class LibVLCVideo : public IVideo
{
public:
    LibVLCVideo(int monitor);
    ~LibVLCVideo();
    bool initialize();
    bool play(std::string file);
    bool stop();
    bool deInitialize();
    SDL_Texture* getTexture() const;
    void draw();
    void update(float);
    void setNumLoops(int n);
    void freeElements();
    int getHeight();
    int getWidth();
    bool isPlaying();
    void setVolume(float volume);
    void skipForward();
    void skipBackward();
    void skipForwardp();
    void skipBackwardp();
    void pause();
    void restart();
    unsigned long long getCurrent();
    unsigned long long getDuration();
    bool isPaused();
    void reset();

private:
    static void unlock(void* data, void* id, void* const* p_pixels);
    static void* lock(void* data, void** p_pixels);
    static void logCallback(void* data, int level, const libvlc_log_t* ctx, const char* fmt, va_list args);

    struct Context {
        SDL_Renderer* renderer;
        SDL_Texture* texture;
        int n;
        uint32_t* pixels;
    } ctx;

    libvlc_instance_t* vlcInstance;
    libvlc_media_player_t* mediaPlayer;
    libvlc_media_t* media;
    libvlc_media_track_t** tracks;
    std::vector<libvlc_instance_t*> vlcInstances;
    size_t currentInstanceIndex;
    unsigned int height_;
    unsigned int width_;
    bool frameReady_;
    bool isPlaying_;
    static bool initialized_;
    int playCount_;
    std::string currentFile_;
    int numLoops_;
    float volume_;
    float currentVolume_;
    int monitor_;
    bool paused_;
    unsigned int numTracks;
    int hasVideo;
    int hasAudio;
    bool reboot_;
};

#endif // LIBVLCVIDEO_H
