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

#include "VideoBuilder.h"
#include "../../Utility/Utils.h"
#include "../../Utility/Log.h"
#include "../../Video/VideoFactory.h"
#include <fstream>
#include <algorithm>

VideoComponent* VideoBuilder::createVideo(std::string path, Page& page, std::string name, int monitor, bool isTypeVideo, int numLoops)
{
    VideoComponent* component = nullptr;
    std::string prefix = Utils::combinePath(path, name);
    std::string file;
    if (Utils::findMatchingFile(prefix, extensions, file))
    {
        // Convert the file extension to lowercase for case-insensitive comparison
        std::transform(file.begin(), file.end(), file.begin(), ::tolower);
        IVideo* video = factory_.createVideo(monitor, isTypeVideo, numLoops);
        if (video)
        {
            component = new VideoComponent(video, page, file);
        }
    }
    return component;
}

std::vector<std::string> VideoBuilder::getSupportedExtensions() const
{
    return extensions;
}


