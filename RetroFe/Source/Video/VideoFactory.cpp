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

#include "VideoFactory.h"
#include "IVideo.h"
#include "../Utility/Log.h"
#include "LibVLCVideo.h"
#include <map>

bool VideoFactory::enabled_ = true;
int VideoFactory::numLoops_ = 0;
std::map<int, IVideo*> VideoFactory::instances_; //One video per display 

IVideo* VideoFactory::createVideo(int monitor, bool isTypeVideo, int numLoops)
{
    IVideo* instance = NULL;
    if (enabled_)
    {
        if (isTypeVideo)
        {
            auto it = instances_.find(monitor);
            if (it == instances_.end())
            {
                instance = new LibVLCVideo(monitor);
                instance->initialize();
                instances_[monitor] = instance;
            }
            else
            {
                instance = it->second;
            }
        }
        else
        {
            instance = new LibVLCVideo(monitor);
            instance->initialize();
        }
    }
    if (instance)
    {
        if (numLoops > 0)
            ((LibVLCVideo*)(instance))->setNumLoops(numLoops);
        else
            ((LibVLCVideo*)(instance))->setNumLoops(numLoops_);
    }
    return instance;
}
bool VideoFactory::canDelete(IVideo* instance)
{
    // Search all monitors for this pointer
    for (const auto& pair : instances_)
    {
        if (pair.second == instance)
            return false; //  shared instance for that monitor
    }
    return true; // Not found safe to delete
}
void VideoFactory::setEnabled(bool enabled)
{
    enabled_ = enabled;
}
void VideoFactory::setNumLoops(int numLoops)
{
    numLoops_ = numLoops;
}

void VideoFactory::resetAll()
{
    for (const auto& pair : instances_)
    {
        if (pair.second)
            pair.second->reset();
    }
}



