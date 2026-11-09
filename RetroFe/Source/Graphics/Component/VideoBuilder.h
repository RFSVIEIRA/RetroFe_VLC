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

#include "Image.h"
#include "AnimatedImageBuilder.h"
#include "VideoComponent.h"
#include "../Page.h"
#include "../../Video/VideoFactory.h"


//todo: this is more of a factory than a builder
class VideoBuilder
{
public:
    VideoComponent * createVideo(std::string path, Page &page, std::string name, int monitor, bool isTypeVideo, int numLoops = -1);
    std::vector<std::string> getSupportedExtensions() const;

private:
    VideoFactory factory_;
    std::vector<std::string> extensions = {
        "mp4", "avi", "mkv", "mov", "flv", "webm", "3gp", "wmv",
        "mp3", "wav", "flac", "ogg", "aac", "wma"
    };
};
