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

#include <string>
#include <vector>
#include <map>

class Item;

class CollectionInfo
{
public:
    CollectionInfo(std::string name, std::string listPath, std::string extensions, std::string metadataType, std::string metadataPath);
    virtual ~CollectionInfo();
    std::string settingsPath() const;
    bool Save();
    void sortItems();
    void sortPlaylists();
    void addSubcollection(CollectionInfo *info);
    void extensionList(std::vector<std::string> &extensions);
    std::string name;
    std::string lowercaseName();
    std::string listpath;
    bool saveRequest;
    std::string metadataType;
    std::string launcher;
    std::vector<Item *> items;

    typedef std::map<std::string, std::vector <Item *> *> Playlists_T;
    Playlists_T playlists;

    bool menusort;
    bool ownsItems;
    bool subsSplit;
    bool hasSubs;

    bool lastplayedByLastAccess;    
    bool favoritesByLastAccess;     
    bool favoritesCollectionByAsc;  
    CollectionInfo* originalCollection;
    std::map<std::string, bool> playlistByFileOrder;
    bool subByAscOrder;
    std::vector<CollectionInfo*> subcollections;


    // Dynamic sort support
    std::string sortField = "title";
    std::vector<std::string> sortFieldList = {};
    int currentSortFieldIndex = 0;

    // Persist last used sort index per collection name (survives CollectionInfo recreation during playlist changes)
    static std::map<std::string, int> s_lastSortIndex;

    void cycleSort(bool up);
    std::string getCurrentSortField() const;
    void sortItemsByField(const std::string& field);
    std::string getSortValue(Item* item, const std::string& type) const;
    bool isNumericSortField(const std::string& field) const;
    bool itemIsLessByField(Item* lhs, Item* rhs, const std::string& field);


private:
    std::string metadataPath_;
    std::string extensions_;
    static bool itemIsLess(Item *lhs, Item *rhs);

};
