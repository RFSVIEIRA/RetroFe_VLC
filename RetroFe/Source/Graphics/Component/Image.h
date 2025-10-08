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

#include "Component.h"
#include <SDL.h>
#include <string>
#include "../../Utility/Utils.h"




 

class Image : public Component
{
public:
    Image(std::string file, std::string altFile, Page &p, int monitor, 
          bool random = false, bool slideShow = false, int slideShowTimer = 5000, int slideNumber = 5, std::string randomSrc = "", std::string altRandomSrc = "");
    virtual ~Image();
    void freeGraphicsMemory();
    void allocateGraphicsMemory();
    void draw();
    static Configuration* config_;
    SDL_Texture* getTexture() const override { return texture_; }

    protected:
    SDL_Texture *texture_;
    std::string  file_;
    std::string  altFile_;

    bool random_;
    bool slideShow_;
    int slideShowTimer_;
    int slideNumber_;
    std::string randomSrc_;
    std::string altRandomSrc_;
    std::vector<std::string> imageList_; 
    size_t currentImageIndex_;
    Uint32 lastSwitchTime_; 

    void loadImageList(); 
    void switchImage(); 
    
};


