/* This file is part of RetroFeVLC.
 *
 * RetroFeVLC is free software: Fork of RetroFE you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#pragma once

#include <string>
#include <SDL.h>
#include <SDL_image.h>

class JxlLoader
{
public:
    static bool IsJxlFile(const std::string& file);
    static bool IsAnimated(const std::string& file);
    static SDL_Surface* LoadSurface(const std::string& file);
    static IMG_Animation* LoadAnimation(const std::string& file);
};
