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

#include "MediaAlias.h"
#include "Utils.h"
#include "../Database/Configuration.h"
#include "Log.h"
#include <fstream>
#include <algorithm>

std::unordered_map<std::string, std::unordered_map<std::string, std::pair<std::string, std::string>>> MediaAlias::cache_;
std::unordered_map<std::string, bool> MediaAlias::fileExistsCache_;
std::mutex MediaAlias::mutex_;

void MediaAlias::clearCache()
{
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
    fileExistsCache_.clear();
}

const std::unordered_map<std::string, std::pair<std::string, std::string>>* MediaAlias::getAliasMap(const std::string& filePath)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_.find(filePath);
    if (it != cache_.end())
    {
        return &(it->second);
    }

    auto fileIt = fileExistsCache_.find(filePath);
    if (fileIt != fileExistsCache_.end() && !fileIt->second)
    {
        return nullptr;
    }

    if (!Utils::fileExists(filePath))
    {
        fileExistsCache_[filePath] = false;
        return nullptr;
    }

    fileExistsCache_[filePath] = true;
    auto& map = cache_[filePath];

    std::ifstream stream(filePath);
    if (!stream.is_open())
    {
        return nullptr;
    }

    std::string line;
    while (std::getline(stream, line))
    {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ' || line.back() == '\t'))
        {
            line.pop_back();
        }

        line = Utils::filterComments(line);
        line = Utils::trimEnds(line);
        if (line.empty()) continue;

        // Supported formats:
        // 1. Contra (USA) = Arcade:contra
        // 2. 1942 = Arcade:1942
        // 3. 1942 = Arcade
        // 4. _Arcade:1942
        size_t eqPos = line.find('=');
        if (eqPos != std::string::npos)
        {
            std::string sourceName = Utils::trimEnds(line.substr(0, eqPos));
            std::string target = Utils::trimEnds(line.substr(eqPos + 1));
            if (!sourceName.empty() && !target.empty())
            {
                size_t colonPos = target.find(':');
                std::string targetCol, targetGame;
                if (colonPos != std::string::npos)
                {
                    targetCol = Utils::trimEnds(target.substr(0, colonPos));
                    targetGame = Utils::trimEnds(target.substr(colonPos + 1));
                    if (targetGame.empty()) targetGame = sourceName;
                }
                else
                {
                    targetCol = target;
                    targetGame = sourceName;
                }

                if (!targetCol.empty() && !targetGame.empty())
                {
                    map[sourceName] = { targetCol, targetGame };
                    map[Utils::toLower(sourceName)] = { targetCol, targetGame };
                }
            }
        }
        else if (line[0] == '_')
        {
            // _Arcade:1942
            std::string rem = line.substr(1);
            size_t colonPos = rem.find(':');
            if (colonPos != std::string::npos)
            {
                std::string targetCol = Utils::trimEnds(rem.substr(0, colonPos));
                std::string targetGame = Utils::trimEnds(rem.substr(colonPos + 1));
                if (!targetCol.empty() && !targetGame.empty())
                {
                    map[targetGame] = { targetCol, targetGame };
                    map[Utils::toLower(targetGame)] = { targetCol, targetGame };
                }
            }
        }
    }

#ifdef _DEBUG
    Logger::write(Logger::ZONE_INFO, "MediaAlias", "Loaded " + std::to_string(map.size()) + " aliases from " + filePath);
#endif

    return &map;
}

bool MediaAlias::resolveAlias(const std::string& collection, const std::string& mediaType, const std::string& basename,
                             std::string& targetCollection, std::string& targetBasename)
{
    if (collection.empty() || mediaType.empty() || basename.empty())
    {
        return false;
    }

    // 1. Try local folder aliases: collections/<collection>/medium_artwork/<mediaType>/aliases.txt
    std::string localFile = Utils::combinePath(Configuration::absolutePath, "collections", collection, "medium_artwork", mediaType, "aliases.txt");
    const auto* localMap = getAliasMap(localFile);
    if (localMap && !localMap->empty())
    {
        auto it = localMap->find(basename);
        if (it != localMap->end())
        {
            targetCollection = it->second.first;
            targetBasename = it->second.second;
            return true;
        }
        std::string lowerName = Utils::toLower(basename);
        it = localMap->find(lowerName);
        if (it != localMap->end())
        {
            targetCollection = it->second.first;
            targetBasename = it->second.second;
            return true;
        }
    }

    // 2. Try global collection aliases: collections/<collection>/aliases.txt
    std::string globalFile = Utils::combinePath(Configuration::absolutePath, "collections", collection, "aliases.txt");
    const auto* globalMap = getAliasMap(globalFile);
    if (globalMap && !globalMap->empty())
    {
        auto it = globalMap->find(basename);
        if (it != globalMap->end())
        {
            targetCollection = it->second.first;
            targetBasename = it->second.second;
            return true;
        }
        std::string lowerName = Utils::toLower(basename);
        it = globalMap->find(lowerName);
        if (it != globalMap->end())
        {
            targetCollection = it->second.first;
            targetBasename = it->second.second;
            return true;
        }
    }

    return false;
}
