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
#include "CollectionInfo.h"
#include "Item.h"
#include "../Database/Configuration.h"
#include "../Utility/Utils.h"
#include "../Utility/Log.h"
#include <sstream>
#include <fstream>
#include <algorithm>
#include <exception>
#include <unordered_set>
#include <sys/stat.h>
#include <sys/types.h>


#ifdef _WIN32
#include <Windows.h>
#endif

#if defined(__linux) || defined(__APPLE__)
#include <errno.h>
#include <cstring>
#endif

CollectionInfo::CollectionInfo(std::string name,
                               std::string listPath,
                               std::string extensions,
                               std::string metadataType,
                               std::string metadataPath)
    : name(name)
    , listpath(listPath)
    , saveRequest(false)
    , metadataType(metadataType)
    , menusort(true)
    , ownsItems(true)
    , subsSplit(false)
    , hasSubs(false)
    , metadataPath_(metadataPath)
	, extensions_(extensions)
	, lastplayedByLastAccess(false)
	, favoritesByLastAccess(false)
	, favoritesCollectionByAsc(false)
    , originalCollection(nullptr)
    , subByAscOrder(false)
{
}

CollectionInfo::~CollectionInfo()
{
    Playlists_T::iterator pit = playlists.begin();

    while(pit != playlists.end())
    {
        if(pit->second != &items)
        {
            delete pit->second;
        }
        playlists.erase(pit);
        pit = playlists.begin();
    }

	std::vector<Item *>::iterator it = items.begin();
    while(it != items.end())
    {
        if (ownsItems) delete *it;
        items.erase(it);
        it = items.begin();
    }

    for (CollectionInfo* sub : subcollections) {
        if (sub) {
            sub->items.clear();
            delete sub;
        }
    }
    subcollections.clear();
}

bool CollectionInfo::Save()
{
    if (!saveRequest)
        return true;

    bool retval = true;
    std::string collectionName = (originalCollection && name == "TempGameCollection")
        ? originalCollection->name : name;

    if (collectionName == "SearchResults")
    {
        collectionName = "Search Collection";
    }

    std::string dir = Utils::combinePath(Configuration::absolutePath, "collections", collectionName, "playlists");

    // Create folder if needed
    struct stat info;
    if (stat(dir.c_str(), &info) != 0)
    {
        // create directory code...
#if defined(_WIN32) && !defined(__GNUC__)
        CreateDirectory(dir.c_str(), NULL);
#else
        mkdir(dir.c_str(), 0755);
#endif
    }

    // Save ALL playlists
    for (auto& pair : playlists)
    {
        std::string playlistName = pair.first;

        // Optional: skip internal playlists
        if (playlistName == "all" || playlistName == "lastplayed")
            continue;

        std::string file = Utils::combinePath(dir, playlistName + ".txt");
        Logger::write(Logger::ZONE_INFO, "Collection", "Saving " + file);

        std::ofstream filestream(file);
        if (!filestream.is_open())
        {
            Logger::write(Logger::ZONE_ERROR, "Collection", "Failed to save " + file);
            retval = false;
            continue;
        }

        std::vector<Item*>* items = pair.second;
        if (!items) continue;
        for (Item* item : *items)
        {
            if (!item || !item->collectionInfo) continue;
            if (item->collectionInfo->name == collectionName)
                filestream << item->name << std::endl;
            else
                filestream << "_" << item->collectionInfo->name << ":" << item->name << std::endl;
        }
        filestream.close();
    }

    saveRequest = false;
    return retval;
}


std::string CollectionInfo::settingsPath() const
{
    return Utils::combinePath(Configuration::absolutePath, "collections", name);
}


void CollectionInfo::extensionList(std::vector<std::string> &extensionlist)
{
    std::istringstream ss(extensions_);
    std::string token;

    while(std::getline(ss, token, ','))
    {
        token = Utils::trimEnds(token);
    	extensionlist.push_back(token);
    }
}

std::string CollectionInfo::lowercaseName()
{
    std::string lcstr = name;
    std::transform(lcstr.begin(), lcstr.end(), lcstr.begin(), ::tolower);
    return lcstr;
}

void CollectionInfo::addSubcollection(CollectionInfo *newinfo)
{
    items.insert(items.begin(), newinfo->items.begin(), newinfo->items.end());
    subcollections.push_back(newinfo);
}

bool CollectionInfo::itemIsLess(Item *lhs, Item *rhs)
{
    if(lhs->leaf && !rhs->leaf) return true;
    if(!lhs->leaf && rhs->leaf) return false;
    if(lhs->collectionInfo->subsSplit && lhs->collectionInfo != rhs->collectionInfo)
        return lhs->collectionInfo->lowercaseName() < rhs->collectionInfo->lowercaseName();
    // Respect subByAscOrder - never sort if enabled
    if (lhs->collectionInfo->subByAscOrder || rhs->collectionInfo->subByAscOrder)
        return false;

    if(!lhs->collectionInfo->menusort && !lhs->leaf && !rhs->leaf)
        return false;

    return lhs->lowercaseFullTitle() < rhs->lowercaseFullTitle();
}


void CollectionInfo::sortItems()
{
    if (subByAscOrder) {
        Logger::write(Logger::ZONE_INFO, "CollectionInfo",
            "subByAscOrder = true skipping sort for collection: " + name);
        return;
    }

    // Apply dynamic attribute comparator if requested
    if (!sortField.empty() && sortField != "title") {
        sortItemsByField(sortField);
    } else {
        // Default title sort
        std::sort(items.begin(), items.end(), CollectionInfo::itemIsLess);
    }

    sortPlaylists();
}

void CollectionInfo::sortPlaylists() {
    std::vector<Item*>* allItems = &items;

    for (Playlists_T::iterator itP = playlists.begin(); itP != playlists.end(); ++itP) {
        std::string playlistName = itP->first;
        bool shouldRebuild = true;

        // Check settings to decide whether to rebuild each playlist
        if (playlistName == "lastplayed" && lastplayedByLastAccess) {
            shouldRebuild = false; // Keep file order
        }
        else if (playlistName == "favorites") {
            if (name == "FAVORITES") {
                if (!favoritesCollectionByAsc) {
                    shouldRebuild = false; // Keep file order
                }
            }
            else {
                if (favoritesByLastAccess) {
                    shouldRebuild = false; // Keep file order
                }
            }
        }
        else {
            auto customIt = playlistByFileOrder.find(playlistName);
            if (customIt != playlistByFileOrder.end() && customIt->second) {
                shouldRebuild = false; // FileOrder=true: Keep file order
            }
        }
        // Rebuild playlist to match items order only if shouldRebuild is true
        if (shouldRebuild && itP->second != allItems) {
            std::unordered_set<Item*> toSortSet(itP->second->begin(), itP->second->end());
            itP->second->clear();
            for (std::vector<Item*>::iterator itAll = allItems->begin(); itAll != allItems->end(); ++itAll) {
                if (toSortSet.find(*itAll) != toSortSet.end()) {
                    itP->second->push_back(*itAll);
                }
            }
        }
    }
}

// Dynamic item sorting

// Definition of the static member (put this once in CollectionInfo.cpp)
std::map<std::string, int> CollectionInfo::s_lastSortIndex;

void CollectionInfo::cycleSort(bool up) {
    int listSize = (int)sortFieldList.size();
    int titleIndex = listSize; // virtual index for "title"

    // If no custom fields, only title
    if (listSize == 0) {
        sortField = "title";
        currentSortFieldIndex = -1;
        sortItemsByField("title");
#ifdef _DEBUG
        Logger::write(Logger::ZONE_INFO, "CollectionInfo",
            "Only title sorting available (empty sortList) for " + name);
#endif
        return;
    }

    // Determine current position
    if (sortField == "title" || sortField.empty()) {
        currentSortFieldIndex = titleIndex;
    }
    else {
        int found = -1;
        for (size_t i = 0; i < sortFieldList.size(); ++i) {
            if (Utils::toLower(sortFieldList[i]) == Utils::toLower(sortField)) {
                found = (int)i;
                break;
            }
        }
        currentSortFieldIndex = (found != -1) ? found : 0;
    }

    // Cycle to next/prev
    if (up) {
        currentSortFieldIndex = (currentSortFieldIndex + 1) % (listSize + 1);
    }
    else {
        currentSortFieldIndex = (currentSortFieldIndex - 1 + (listSize + 1)) % (listSize + 1);
    }

    // Resolve field name
    if (currentSortFieldIndex == titleIndex) {
        sortField = "title";
    }
    else {
        sortField = sortFieldList[currentSortFieldIndex];
    }

    // Apply sort
    int nonEmpty = 0;
    for (Item* it : items) {
        if (getSortValue(it, sortField) != "zzzzzzzz") nonEmpty++;
    }
#ifdef _DEBUG
    Logger::write(Logger::ZONE_INFO, "CollectionInfo",
        "Sort cycled " + std::string(up ? "NEXT" : "PREV") +
        " to '" + sortField + "' (non-empty: " + std::to_string(nonEmpty) + ")");
#endif
    sortItemsByField(sortField);
    s_lastSortIndex[name] = currentSortFieldIndex;
}

std::string CollectionInfo::getCurrentSortField() const {
    if (sortField.empty()) {
        return "title";
    }
    return sortField;
}

std::string CollectionInfo::getSortValue(Item* item, const std::string& type) const {
    if (!item) return "zzzzzzzz";

    std::string typeLC = Utils::toLower(type);
    std::string val;

    if (typeLC == "manufacturer")
        val = item->manufacturer;
    else if (typeLC == "developer")
        val = item->developer.empty() ? item->manufacturer : item->developer;
    else if (typeLC == "genre")
        val = item->genre;
    else if (typeLC == "year")
        val = item->year;
    else if (typeLC == "title")
        val = item->fullTitle.empty()
        ? (item->name.empty() ? item->title : item->name)
        : item->fullTitle;
    else if (typeLC == "name")
        val = item->name;
    else if (typeLC == "numberplayers")
        val = item->numberPlayers;
    else if (typeLC == "numberbuttons")
        val = item->numberButtons;
    else if (typeLC == "rating")
        val = item->rating;
    else if (typeLC == "score")
        val = item->score;
    else if (typeLC == "artist")
        val = item->artist;
    else if (typeLC == "album")
        val = item->album;
    else
        val = item->fullTitle.empty() ? item->name : item->fullTitle;

    val = Utils::trimEnds(val);
    std::string valLC = Utils::toLower(val);

    if (val.empty() || valLC == "na" || valLC == "unknown" || valLC == "n/a" || valLC == "?" || valLC == "0")
        return "zzzzzzzz";

    return valLC;
}

bool CollectionInfo::itemIsLessByField(Item* lhs, Item* rhs, const std::string& field)
{
    if (!lhs || !rhs) return false;

    std::string lval = getSortValue(lhs, field);
    std::string rval = getSortValue(rhs, field);

    const std::string EMPTY = "zzzzzzzz";

    if (lval == EMPTY && rval != EMPTY) return false;
    if (lval != EMPTY && rval == EMPTY) return true;
    if (lval == EMPTY && rval == EMPTY) {
        return lhs->lowercaseFullTitle() < rhs->lowercaseFullTitle();
    }

    // Float attributes (score, rating)
    if (field == "score" || field == "rating")
    {
        auto toDouble = [EMPTY](const std::string& s) -> double {
            if (s.empty() || s == EMPTY) return -999999.0;
            try {
                return std::stod(s);
            }
            catch (...) {
                return -999999.0;
            }
            };

        double ld = toDouble(lval);
        double rd = toDouble(rval);

        if (ld != rd) return ld < rd;
    }
    // Release year
    else if (field == "year")
    {
        auto extractYear = [EMPTY](const std::string& s) -> long {
            std::string num;
            for (char c : s) {
                if (isdigit(static_cast<unsigned char>(c))) num += c;
                else if (!num.empty()) break;
            }
            if (num.empty()) return 999999L;
            try { return std::stol(num); }
            catch (...) { return 999999L; }
            };
        long ly = extractYear(lval);
        long ry = extractYear(rval);
        if (ly != ry) return ly < ry;
    }
    // Player count
    else if (field == "numberplayers")
    {
        auto getMinPlayers = [EMPTY](const std::string& s) -> int {
            if (s.empty() || s == EMPTY) return 99;
            std::string num;
            for (char c : s) {
                if (isdigit(static_cast<unsigned char>(c))) num += c;
                else if (!num.empty()) break;
            }
            if (num.empty()) return 99;
            try { return std::stoi(num); }
            catch (...) { return 99; }
            };
        int lp = getMinPlayers(lval);
        int rp = getMinPlayers(rval);
        if (lp != rp) return lp < rp;
    }
    // Numeric attributes
    else if (isNumericSortField(field))
    {
        long ln = 0, rn = 0;
        try {
            ln = std::stol(lval);
            rn = std::stol(rval);
        }
        catch (...) {}
        if (ln != rn) return ln < rn;
    }
    // String attributes
    else
    {
        std::string llower = Utils::toLower(lval);
        std::string rlower = Utils::toLower(rval);
        if (llower != rlower) return llower < rlower;
    }

    return lhs->lowercaseFullTitle() < rhs->lowercaseFullTitle();
}

bool CollectionInfo::isNumericSortField(const std::string& field) const {
    std::string f = Utils::toLower(field);
    return (f == "year" || f == "numberplayers" || f == "numberbuttons" ||
        f == "score" || f == "rating");
}


void CollectionInfo::sortItemsByField(const std::string& field) {
    if (subByAscOrder) {
        Logger::write(Logger::ZONE_INFO, "CollectionInfo",
            "subByAscOrder = true skipping sort for collection: " + name);
        return;
    }

    std::string target = Utils::toLower(field.empty() ? "title" : field);

    if (sortFieldList.empty() || target == "title") {
        sortField = "title";
        currentSortFieldIndex = (int)sortFieldList.size(); // virtual title index
    }
    else {
        currentSortFieldIndex = 0;
        bool found = false;
        for (size_t i = 0; i < sortFieldList.size(); ++i) {
            if (Utils::toLower(sortFieldList[i]) == target) {
                currentSortFieldIndex = static_cast<int>(i);
                found = true;
                break;
            }
        }
        if (!found) {
            sortField = "title";
            currentSortFieldIndex = (int)sortFieldList.size();
        }
        else {
            sortField = sortFieldList[currentSortFieldIndex];
        }
    }

    // Debug: count non-empty
    int nonEmpty = 0;
    for (Item* it : items) {
        std::string v = getSortValue(it, sortField);
        if (v != "zzzzzzzz") nonEmpty++;
    }
#ifdef _DEBUG
    Logger::write(Logger::ZONE_INFO, "CollectionInfo",
        "sortItemsByField('" + sortField + "') called for " + name +
        " - non-empty values: " + std::to_string(nonEmpty) + "/" + std::to_string(items.size()));
#endif
    // Sort using improved comparator
    std::sort(items.begin(), items.end(), [this](Item* a, Item* b) {
        return this->itemIsLessByField(a, b, this->sortField);
        });

    sortPlaylists();

    // Debug first 3 items
    if (!items.empty()) {
        Logger::write(Logger::ZONE_DEBUG, "CollectionInfo",
            "First 3 items after sort by '" + sortField + "': " +
            items[0]->name + " | " + items[1]->name + " | " + items[2]->name);
    }
}