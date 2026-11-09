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
#pragma once

#include <string>
#include <unordered_map>
#include <utility>
#include <mutex>

class MediaAlias
{
public:
    // Resolves media alias for a game in a collection and media type.
    // Priority:
    // 1. Local artwork folder: collections/<collection>/medium_artwork/<mediaType>/aliases.txt
    // 2. Global collection root: collections/<collection>/aliases.txt
    // Returns true and sets targetCollection and targetBasename if an alias was found.
    static bool resolveAlias(const std::string& collection, const std::string& mediaType, const std::string& basename,
                             std::string& targetCollection, std::string& targetBasename);

    // Clears the in-memory cache
    static void clearCache();

private:
    static const std::unordered_map<std::string, std::pair<std::string, std::string>>* getAliasMap(const std::string& filePath);

    // filePath -> map of (gameName -> {targetCollection, targetGameName})
    static std::unordered_map<std::string, std::unordered_map<std::string, std::pair<std::string, std::string>>> cache_;
    static std::unordered_map<std::string, bool> fileExistsCache_;
    static std::mutex mutex_;
};
