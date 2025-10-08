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
#include "CollectionInfoBuilder.h"
#include "CollectionInfo.h"
#include "Item.h"
#include "../Database/Configuration.h"
#include "../Database/MetadataDatabase.h"
#include "../Database/DB.h"
#include "../Utility/Log.h"
#include "../Utility/Utils.h"
#include <dirent.h>
#include <filesystem>

#if defined(__linux) || defined(__APPLE__)
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <cstring>
#endif

#include <sstream>
#include <vector>
#include <fstream>
#include <algorithm>
#include <set>
#include <libxml/parser.h>
#include <libxml/xmlreader.h> // For XML parsing, if needed
#include <string>

namespace fs = std::filesystem;

CollectionInfoBuilder::CollectionInfoBuilder(Configuration &c, MetadataDatabase &mdb)
    : conf_(c)
    , metaDB_(mdb)
{
}

CollectionInfoBuilder::~CollectionInfoBuilder()
{
}

bool CollectionInfoBuilder::createCollectionDirectory(std::string name)
{
    std::string collectionPath = Utils::combinePath(Configuration::absolutePath, "collections", name);

    std::vector<std::string> paths;
    paths.push_back(collectionPath);
    paths.push_back(Utils::combinePath(collectionPath, "medium_artwork"));
    paths.push_back(Utils::combinePath(collectionPath, "medium_artwork", "artwork_back"));
    paths.push_back(Utils::combinePath(collectionPath, "medium_artwork", "artwork_front"));
    paths.push_back(Utils::combinePath(collectionPath, "medium_artwork", "bezel"));
    paths.push_back(Utils::combinePath(collectionPath, "medium_artwork", "logo"));
    paths.push_back(Utils::combinePath(collectionPath, "medium_artwork", "medium_back"));
    paths.push_back(Utils::combinePath(collectionPath, "medium_artwork", "medium_front"));
    paths.push_back(Utils::combinePath(collectionPath, "medium_artwork", "screenshot"));
    paths.push_back(Utils::combinePath(collectionPath, "medium_artwork", "screentitle"));
    paths.push_back(Utils::combinePath(collectionPath, "medium_artwork", "video"));
    paths.push_back(Utils::combinePath(collectionPath, "roms"));
    paths.push_back(Utils::combinePath(collectionPath, "system_artwork"));
    paths.push_back(Utils::combinePath(collectionPath, "gamesinfo"));

    for(std::vector<std::string>::iterator it = paths.begin(); it != paths.end(); it++)
    {
        std::cout << "Creating folder \"" << *it << "\"" << std::endl;

#if defined(_WIN32) && !defined(__GNUC__)
        if (!CreateDirectory(it->c_str(), NULL))
        {
            if (ERROR_ALREADY_EXISTS != GetLastError())
            {
                std::cout << "Could not create folder \"" << *it << "\"" << std::endl;
                return false;
            }
        }
#else 
#if defined(__MINGW32__)
        if (mkdir(it->c_str()) == -1)
#else
        if (mkdir(it->c_str(), 0755) == -1)
#endif        
        {
           std::cout << "Could not create folder \"" << *it << "\":" << errno << std::endl;
        }
    #endif
    }

    std::string filename = Utils::combinePath(collectionPath, "include.txt");
    std::cout << "Creating file \"" << filename << "\"" << std::endl;

    std::ofstream includeFile;
    includeFile.open(filename.c_str());
    includeFile << "# Add a list of files to show on the menu (one filename per line, without the extension)." << std::endl;
    includeFile << "# If no items are in this list then all files in the folder specified" << std::endl;
    includeFile << "# by settings.conf will be used" << std::endl;
    includeFile.close();

    filename = Utils::combinePath(collectionPath, "exclude.txt");
    std::cout << "Creating file \"" << filename << "\"" << std::endl;
    std::ofstream excludeFile;
    excludeFile.open(filename.c_str());

    includeFile << "# Add a list of files to hide on the menu (one filename per line, without the extension)." << std::endl;
    excludeFile.close();

    filename = Utils::combinePath(collectionPath, "settings.conf");
    std::cout << "Creating file \"" << filename << "\"" << std::endl;
    std::ofstream settingsFile;
    settingsFile.open(filename.c_str());

    settingsFile << "# Uncomment and edit the following line to use a different ROM path." << std::endl;
    settingsFile << "#list.path = " << Utils::combinePath("%BASE_ITEM_PATH%", "%ITEM_COLLECTION_NAME%", "roms") << std::endl;
    settingsFile << "list.includeMissingItems = false" << std::endl;
    settingsFile << "list.extensions = zip" << std::endl;
    settingsFile << "list.menuSort = yes" << std::endl;
    settingsFile << std::endl;
    settingsFile << "launcher = mame" << std::endl;
    settingsFile << "#metadata.type = MAME" << std::endl;
    settingsFile << std::endl;
    settingsFile << std::endl;
    settingsFile << "#media.screenshot      = " << Utils::combinePath("%BASE_MEDIA_PATH%", "%ITEM_COLLECTION_NAME%", "medium_artwork", "screenshot") << std::endl;
    settingsFile << "#media.screentitle     = " << Utils::combinePath("%BASE_MEDIA_PATH%", "%ITEM_COLLECTION_NAME%", "medium_artwork", "screentitle") << std::endl;
    settingsFile << "#media.artwork_back    = " << Utils::combinePath("%BASE_MEDIA_PATH%", "%ITEM_COLLECTION_NAME%", "medium_artwork", "artwork_back") << std::endl;
    settingsFile << "#media.artwork_front   = " << Utils::combinePath("%BASE_MEDIA_PATH%", "%ITEM_COLLECTION_NAME%", "medium_artwork", "artwork_front") << std::endl;
    settingsFile << "#media.logo            = " << Utils::combinePath("%BASE_MEDIA_PATH%", "%ITEM_COLLECTION_NAME%", "medium_artwork", "logo") << std::endl;
    settingsFile << "#media.medium_back     = " << Utils::combinePath("%BASE_MEDIA_PATH%", "%ITEM_COLLECTION_NAME%", "medium_artwork", "medium_back") << std::endl;
    settingsFile << "#media.medium_front    = " << Utils::combinePath("%BASE_MEDIA_PATH%", "%ITEM_COLLECTION_NAME%", "medium_artwork", "medium_front") << std::endl;
    settingsFile << "#media.screenshot      = " << Utils::combinePath("%BASE_MEDIA_PATH%", "%ITEM_COLLECTION_NAME%", "medium_artwork", "screenshot") << std::endl;
    settingsFile << "#media.screentitle     = " << Utils::combinePath("%BASE_MEDIA_PATH%", "%ITEM_COLLECTION_NAME%", "medium_artwork", "screentitle") << std::endl;
    settingsFile << "#media.video           = " << Utils::combinePath("%BASE_MEDIA_PATH%", "%ITEM_COLLECTION_NAME%", "medium_artwork", "video") << std::endl;
    settingsFile << "#media.system_artwork  = " << Utils::combinePath("%BASE_MEDIA_PATH%", "%ITEM_COLLECTION_NAME%", "system_artwork") << std::endl;
    settingsFile.close();

    filename = Utils::combinePath(collectionPath, "menu.txt");
    std::cout << "Creating file \"" << filename << "\"" << std::endl;
    std::ofstream menuFile;
    menuFile.open(filename.c_str());
    menuFile.close();

    return true;
}
CollectionInfo *CollectionInfoBuilder::buildCollection(std::string name)
{
   return buildCollection(name, "");
}

CollectionInfo *CollectionInfoBuilder::buildCollection(std::string name, std::string mergedCollectionName)
{
    std::string listItemsPathKey = "collections." + name + ".list.path";
    std::string listFilterKey = "collections." + name + ".list.filter";
    std::string extensionsKey = "collections." + name + ".list.extensions";
    std::string launcherKey = "collections." + name + ".launcher";

    //todo: metadata is not fully not implemented
    std::string metadataTypeKey = "collections." + name + ".metadata.type";
    std::string metadataPathKey = "collections." + name + ".metadata.path";

    std::string listItemsPath;
    std::string launcherName;
    std::string extensions;
    std::string metadataType = name;
    std::string metadataPath;
    
    conf_.getCollectionAbsolutePath(name, listItemsPath);

    (void)conf_.getProperty(extensionsKey, extensions);
    (void)conf_.getProperty(metadataTypeKey, metadataType);
    (void)conf_.getProperty(metadataPathKey, metadataPath);

    if (!conf_.getProperty(launcherKey, launcherName))
    {
        std::stringstream ss;
        ss        << "\""
                  << launcherKey
                  << "\" points to a launcher that is not configured (launchers."
                  << launcherName
                  << "). Your collection will be viewable, however you will not be able to "
                  << "launch any of the items in your collection.";

        Logger::write(Logger::ZONE_NOTICE, "Collections", ss.str());
    }

    CollectionInfo *collection = new CollectionInfo(name, listItemsPath, extensions, metadataType, metadataPath);

    (void)conf_.getProperty("collections." + collection->name + ".launcher", collection->launcher);

    ImportDirectory(collection, mergedCollectionName);

    return collection;
}


bool CollectionInfoBuilder::ImportBasicList(CollectionInfo *info, std::string file, std::map<std::string, Item *> &list)
{
    std::ifstream includeStream(file.c_str());

    if (!includeStream.good())
    {
        return false;
    }

    std::string line; 

    while(std::getline(includeStream, line))
    {
        line = Utils::filterComments(line);
        
        if (!line.empty() && list.find(line) == list.end())
        {
            Item *i = new Item();

            line.erase( std::remove(line.begin(), line.end(), '\r'), line.end() );

            i->fullTitle = line;
            i->name = line;
            i->title = line;
            i->collectionInfo = info;

            list[line] = i;
        }
    }

    return true;
}

bool CollectionInfoBuilder::ImportBasicList(CollectionInfo *info, std::string file, std::vector<Item *> &list)
{
    std::ifstream includeStream(file.c_str());

    if (!includeStream.good())
    {
        return false;
    }

    std::string line; 

    while(std::getline(includeStream, line))
    {
        line = Utils::filterComments(line);
        
        if (!line.empty())
        {

            bool found = false;
            for (std::vector<Item *>::iterator it = list.begin(); it != list.end(); ++it)
            {
                if (line == (*it)->name)
                {
                    found = true;
                }
            }

            if (!found)
            {
                Item *i = new Item();

                line.erase( std::remove(line.begin(), line.end(), '\r'), line.end() );

                i->fullTitle = line;
                i->name = line;
                i->title = line;
                i->collectionInfo = info;

                list.push_back(i);
            }

        }
    }

    return true;
}
bool CollectionInfoBuilder::ImportBasicList(CollectionInfo* info, std::string file, std::vector<std::string>& order, std::map<std::string, Item*>& list) {
    std::ifstream includeStream(file.c_str());
    if (!includeStream.good()) {
        return false; // File couldn't be opened
    }

    std::string line;
    while (std::getline(includeStream, line)) {
        line = Utils::filterComments(line); // Remove comments
        if (!line.empty() && list.find(line) == list.end()) { // Check for duplicates
            Item* i = new Item();
            line.erase(std::remove(line.begin(), line.end(), '\r'), line.end()); // Remove carriage returns
            i->fullTitle = line;
            i->name = line;
            i->title = line;
            i->collectionInfo = info;
            list[line] = i;        // Store in map for lookup
            order.push_back(line); // Store in vector to preserve order
        }
    }
    includeStream.close();
    return true;
}
bool CollectionInfoBuilder::ImportDirectory(CollectionInfo *info, std::string mergedCollectionName)
{
    std::string path = info->listpath;
    std::vector<Item *> includeFilterUnsorted;
    std::map<std::string, Item *> includeFilter;
    std::map<std::string, Item *> excludeFilter;
    std::string includeFile    = Utils::combinePath(Configuration::absolutePath, "collections", info->name, "include.txt");
    std::string excludeFile    = Utils::combinePath(Configuration::absolutePath, "collections", info->name, "exclude.txt");

    std::string launcher;
    bool showMissing  = false; 
    bool romHierarchy = false;
    bool emuarc       = false;
 
    if (mergedCollectionName != "")
    {
        
        std::string mergedFile = Utils::combinePath(Configuration::absolutePath, "collections", mergedCollectionName, info->name + ".sub");
        Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Checking for \"" + mergedFile + "\"");
        (void)conf_.getProperty("collections." + mergedCollectionName + ".list.includeMissingItems", showMissing);
        ImportBasicList(info, mergedFile, includeFilterUnsorted);
        ImportBasicList(info, mergedFile, includeFilter);

    }
    (void)conf_.getProperty("collections." + info->name + ".list.includeMissingItems", showMissing);
    (void)conf_.getProperty("collections." + info->name + ".list.romHierarchy", romHierarchy);
    (void)conf_.getProperty("collections." + info->name + ".list.emuarc", emuarc);
    if (emuarc)
        romHierarchy = true;

    Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Checking for \"" + includeFile + "\"");
    ImportBasicList(info, includeFile, includeFilterUnsorted);
    ImportBasicList(info, includeFile, includeFilter);
    ImportBasicList(info, excludeFile, excludeFilter);

    for(std::vector<Item *>::iterator it = includeFilterUnsorted.begin(); it != includeFilterUnsorted.end(); ++it)
    {
        if (showMissing && excludeFilter.find((*it)->name) == excludeFilter.end())
        {
            info->items.push_back(*it);
        }
        else
        {
            delete *it;
        }
    }
    includeFilterUnsorted.clear( );

    // Read ROM directory if showMissing is false
    if (!showMissing || includeFilter.size() == 0)
    {
        do
        {
             std::string rompath;
             size_t position = path.find( ";" );
             if(position != std::string::npos)
             {
                 rompath = path.substr(0, position);
                 path    = path.substr(position+1);
             }
             else
             {
                 rompath = path;
                 path    = "";
             }
             ImportRomDirectory(rompath, info, includeFilter, excludeFilter, romHierarchy, emuarc);
        } while (path != "");
    }

    while(includeFilter.size() > 0)
    {
        std::map<std::string, Item *>::iterator it = includeFilter.begin();
        // delete the unused items if they were never pushed to the main collection
        if (!showMissing)
        {
            delete it->second;
        }
        includeFilter.erase(it);
    }
    while(excludeFilter.size() > 0)
    {
        std::map<std::string, Item *>::iterator it = excludeFilter.begin();
        delete it->second;
        excludeFilter.erase(it);
    }

    return true;
}


void CollectionInfoBuilder::addPlaylists(CollectionInfo *info)
{
    std::map<std::string, Item *> excludeAllFilter;
    std::string excludeAllFile = Utils::combinePath(Configuration::absolutePath, "collections", info->name, "exclude_all.txt");

    ImportBasicList(info, excludeAllFile, excludeAllFilter);

    if ( excludeAllFilter.size() > 0)
    {
        info->playlists["all"] = new std::vector<Item *>();
        for(std::vector<Item *>::iterator it = info->items.begin(); it != info->items.end(); it++)
        {
            bool found = false;
            for(std::map<std::string, Item *>::iterator itex = excludeAllFilter.begin(); itex != excludeAllFilter.end(); itex++)
            {
                std::string collectionName = info->name;
                std::string itemName       = itex->first;
                if (itemName.at(0) == '_') // name consists of _<collectionName>:<itemName>
                {
                     itemName.erase(0, 1); // Remove _
                     size_t position = itemName.find(":");
                     if (position != std::string::npos )
                     {
                         collectionName = itemName.substr(0, position);
                         itemName       = itemName.erase(0, position+1);
                     }
                }
                if ( ((*it)->name == itemName || itemName == "*") && (*it)->collectionInfo->name == collectionName )
                {
                    found = true;
                }
            }
            if ( !found )
            {
                info->playlists["all"]->push_back((*it));
            }
        }
        while(excludeAllFilter.size() > 0)
        {
            std::map<std::string, Item *>::iterator it = excludeAllFilter.begin();
            excludeAllFilter.erase(it);
        }
    }
    else
    {
        info->playlists["all"] = &info->items;
    }

    DIR *dp;
    struct dirent *dirp;
    std::string path = Utils::combinePath(Configuration::absolutePath, "collections", info->name, "playlists");
    dp = opendir(path.c_str());

    if (dp == NULL)
    {
        info->playlists["favorites"] = new std::vector<Item *>();
        return;
    }

    while ((dirp = readdir(dp)) != NULL)
    {
        std::string file = dirp->d_name;

        size_t position = file.find_last_of(".");
        std::string basename = (std::string::npos == position) ? file : file.substr(0, position);

        std::string comparator = ".txt";
        size_t start = file.length() >= comparator.length() ? file.length() - comparator.length() : 0;

        if (start >= 0)
        {
            if (file.compare(start, comparator.length(), comparator) == 0)
            {
                Logger::write(Logger::ZONE_INFO, "RetroFE", "Loading playlist: " + basename);

                std::vector<std::string> order; // To preserve file order
                std::map<std::string, Item *> playlistFilter;
                std::string playlistFile = Utils::combinePath(Configuration::absolutePath, "collections", info->name, "playlists", file);
                ImportBasicList(info, playlistFile, order, playlistFilter);

                info->playlists[basename] = new std::vector<Item *>();

                // Add items in file order
                for (std::vector<std::string>::iterator itOrder = order.begin(); itOrder != order.end(); ++itOrder)
                {
                    std::string itemName = *itOrder;
                    std::string collectionName = info->name;

                    if (!itemName.empty() && itemName.at(0) == '_') // Handle _<collectionName>:<itemName>
                    {
                        itemName.erase(0, 1); // Remove _
                        size_t pos = itemName.find(":");
                        if (pos != std::string::npos)
                        {
                            collectionName = itemName.substr(0, pos);
                            itemName = itemName.substr(pos + 1);
                        }
                    }

                    for (std::vector<Item *>::iterator it = info->items.begin(); it != info->items.end(); ++it)
                    {
                        if (((*it)->name == itemName || itemName == "*") && (*it)->collectionInfo->name == collectionName)
                        {
                            info->playlists[basename]->push_back(*it);
                            if (basename == "favorites")
                                (*it)->isFavorite = true;
                            break; // Each item should appear once
                        }
                    }
                }

                // Clean up playlistFilter
                while (playlistFilter.size() > 0)
                {
                    std::map<std::string, Item *>::iterator it = playlistFilter.begin();
                    delete it->second;
                    playlistFilter.erase(it->first);
                }
            }
        }
    }

    if (dp) closedir(dp);

    if (info->playlists["favorites"] == NULL)
    {
        info->playlists["favorites"] = new std::vector<Item *>();
    }

    if (info->playlists["lastplayed"] == NULL)
    {
        info->playlists["lastplayed"] = new std::vector<Item *>();
    }

    return;
}


void CollectionInfoBuilder::updateLastPlayedPlaylist(CollectionInfo *info, Item *item, int size)
{
    std::string path = Utils::combinePath(Configuration::absolutePath, "collections", info->name, "playlists");
    Logger::write(Logger::ZONE_INFO, "RetroFE", "Updating lastplayed playlist");

    std::vector<Item *> lastplayedList;
    std::string playlistFile = Utils::combinePath(Configuration::absolutePath, "collections", info->name, "playlists", "lastplayed.txt");
    ImportBasicList(info, playlistFile, lastplayedList);

    if (info->playlists["lastplayed"] == NULL)
        info->playlists["lastplayed"] = new std::vector<Item *>();
    else
        info->playlists["lastplayed"]->clear();

    if (size == 0)
        return;

    // Put the new item at the front of the list.
    info->playlists["lastplayed"]->push_back(item);

    // Add the items already in the playlist up to the lastplayedSize.
    for(std::vector<Item *>::iterator it = lastplayedList.begin(); it != lastplayedList.end(); it++)
    {
        if (info->playlists["lastplayed"]->size() >= static_cast<unsigned int>( size ))
            break;

        std::string collectionName = info->name;
        std::string itemName       = (*it)->name;
        if (itemName.at(0) == '_') // name consists of _<collectionName>:<itemName>
        {
             itemName.erase(0, 1); // Remove _
             size_t position = itemName.find(":");
             if (position != std::string::npos )
             {
                 collectionName = itemName.substr(0, position);
                 itemName       = itemName.erase(0, position+1);
             }
        }

        for(std::vector<Item *>::iterator it = info->items.begin(); it != info->items.end(); it++)
        {
            if ( (*it)->name == itemName && (*it)->collectionInfo->name == collectionName && (*it) != item)
            {
                info->playlists["lastplayed"]->push_back((*it));
            }
        }
    }

    // Write new lastplayed playlist
    std::string dir  = Utils::combinePath(Configuration::absolutePath, "collections", info->name, "playlists");
    std::string file = Utils::combinePath(Configuration::absolutePath, "collections", info->name, "playlists/lastplayed.txt");
    Logger::write(Logger::ZONE_INFO, "Collection", "Saving " + file);

    std::ofstream filestream;
    try
    {
        // Create playlists directory if it does not exist yet.
        struct stat infostat;
        if ( stat( dir.c_str(), &infostat ) != 0 )
        {
#if defined(_WIN32) && !defined(__GNUC__)
            if(!CreateDirectory(dir.c_str(), NULL))
            {
                if(ERROR_ALREADY_EXISTS != GetLastError())
                {
                    Logger::write(Logger::ZONE_WARNING, "Collection", "Could not create directory " + dir);
                    return;
                }
            }
#else 
#if defined(__MINGW32__)
            if(mkdir(dir.c_str()) == -1)
#else
            if(mkdir(dir.c_str(), 0755) == -1)
#endif        
            {
                Logger::write(Logger::ZONE_WARNING, "Collection", "Could not create directory " + dir);
                return;
            }
#endif
        }
        else if ( !(infostat.st_mode & S_IFDIR) )
        {
            Logger::write(Logger::ZONE_WARNING, "Collection", dir + " exists, but is not a directory.");
            return;
        }

        filestream.open(file.c_str());
        std::vector<Item *> *saveitems = info->playlists["lastplayed"];
        for(std::vector<Item *>::iterator it = saveitems->begin(); it != saveitems->end(); it++)
        {
            if ((*it)->collectionInfo->name == info->name)
            {
                filestream << (*it)->name << std::endl;
            }
            else
            {
                filestream << "_" << (*it)->collectionInfo->name << ":" << (*it)->name << std::endl;
            }
        }

        filestream.close();
    }
    catch(std::exception &)
    {
        Logger::write(Logger::ZONE_ERROR, "Collection", "Save failed: " + file);
    }

    // Sort the playlist(s)
    info->sortPlaylists( );
    return;

}


void CollectionInfoBuilder::ImportRomDirectory(std::string path, CollectionInfo *info, std::map<std::string, Item *> includeFilter, std::map<std::string, Item *> excludeFilter, bool romHierarchy, bool emuarc)
{

    DIR                               *dp;
    struct dirent                     *dirp;
    std::vector<std::string>           extensions;
    std::vector<std::string>::iterator extensionsIt;

    info->extensionList(extensions);

    dp = opendir(path.c_str());

    Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Scanning directory \"" + path + "\"");
    if (dp == NULL)
    {
        Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Could not read directory \"" + path + "\". Ignore if this is a menu.");
        return;
    }

    while(dp != NULL && (dirp = readdir(dp)) != NULL)
    {
        std::string file = dirp->d_name;

        // Check if the file is a directory or a file
        struct stat sb;
        if (romHierarchy && file != "." && file != ".." && stat( Utils::combinePath( path, file ).c_str(), &sb ) == 0 && S_ISDIR( sb.st_mode ))
        {
            ImportRomDirectory( Utils::combinePath( path, file ), info, includeFilter, excludeFilter, romHierarchy, emuarc );
        }
        else if (file != "." && file != "..")
        {
            size_t position = file.find_last_of(".");
            std::string basename = (std::string::npos == position)? file : file.substr(0, position);
        
            // if there is an include list, only include roms that are found and are in the include list
            // if there is an exclude list, exclude those roms
            if ((includeFilter.size() == 0 || (includeFilter.find(basename) != includeFilter.end())) &&
                    (excludeFilter.size() == 0 || excludeFilter.find(basename) == excludeFilter.end()))
            {
                // iterate through all known file extensions
                for(extensionsIt = extensions.begin(); extensionsIt != extensions.end(); ++extensionsIt)
                {
                    std::string comparator = "." + *extensionsIt;
                    size_t start = file.length() >= comparator.length() + 1 ? file.length() - comparator.length() + 1 : 0; //start is never greater than the length of the file string and is always a valid starting index for substring comparison.

                    if (start >= 0)
                    {
                        if (file.compare(start, comparator.length(), *extensionsIt) == 0)
                        {
                            Item *i = new Item();

                            i->name           = basename;
                            i->fullTitle      = basename;
                            i->title          = basename;
                            i->collectionInfo = info;
                            i->filepath       = path + Utils::pathSeparator;

                            if ( emuarc )
                            {
                                i->file      = basename;
                                i->name      = Utils::getFileName( path );
                                i->fullTitle = i->name;
                                i->title     = i->name;
                            }

                            // Load game statistics from gamesinfo folder
                            std::string statsPath = Utils::combinePath(Configuration::absolutePath, "collections", info->name, "gamesinfo", basename + ".txt");
#ifdef _DEBUG
                            Logger::write(Logger::ZONE_DEBUG, "CollectionInfoBuilder", "Loading stats for " + basename + " from " + statsPath);
#endif
                            i->loadInfo(statsPath);
                            // Log loaded stats
#ifdef _DEBUG
                            std::string value;
                            if (i->getInfo("playCount", value))
                                Logger::write(Logger::ZONE_DEBUG, "CollectionInfoBuilder", "Loaded playCount=" + value + " for " + basename);
                            if (i->getInfo("totalPlayTime", value))
                                Logger::write(Logger::ZONE_DEBUG, "CollectionInfoBuilder", "Loaded totalPlayTime=" + value + " for " + basename);
                            if (i->getInfo("lastPlayedDate", value))
                                Logger::write(Logger::ZONE_DEBUG, "CollectionInfoBuilder", "Loaded lastPlayedDate=" + value + " for " + basename);
                            if (i->getInfo("lastPlayDuration", value))
                                Logger::write(Logger::ZONE_DEBUG, "CollectionInfoBuilder", "Loaded lastPlayDuration=" + value + " for " + basename);
#endif

                            // Add item if it doesn't already exist
                            bool found = false;
                            for ( std::vector<Item*>::iterator it = info->items.begin(); it != info->items.end( ); it++ )
                               if ( (*it)->name == basename )
                                   found = true;
                            if ( !found )
                                info->items.push_back(i);
                        }
                    }
                }
            }
        }
    }

    if (dp) closedir(dp);

    return;

}


void CollectionInfoBuilder::injectMetadata(CollectionInfo *info)
{
    metaDB_.injectMetadata(info);
    return;
}

void CollectionInfoBuilder::saveGameStats(CollectionInfo* info, Item* item, int playDuration) {
    // Skip saving stats if play duration is less than 1 minute
    if (playDuration < 1) {
        Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Skipping stats save for " + item->name + ": play duration " + std::to_string(playDuration) + " minutes is less than 1 minute");
        return;
    }

    std::string collectionName = item->collectionInfo ? item->collectionInfo->name : info->name; // Prefer item->collectionInfo

    // If launched from a subcollection, use originalCollection to confirm
    if (info->originalCollection) {
        collectionName = info->originalCollection->name; // Subcollection (e.g., Laser Games)
#ifdef _DEBUG
        Logger::write(Logger::ZONE_DEBUG, "CollectionInfoBuilder", "Using originalCollection: " + collectionName + " for item " + item->name);
#endif
    }
    else if (collectionName == info->name) {
        // Fallback: Check for .sub files to confirm original collection
        std::string collectionDir = Utils::combinePath(Configuration::absolutePath, "collections", info->name);
        DIR* dp = opendir(collectionDir.c_str());
        if (dp) {
            struct dirent* dirp;
            while ((dirp = readdir(dp)) != NULL) {
                std::string file = dirp->d_name;
                size_t position = file.find_last_of(".");
                std::string extension = (position != std::string::npos) ? file.substr(position) : "";
                if (extension == ".sub") {
                    std::string subFilePath = Utils::combinePath(collectionDir, file);
                    std::ifstream subFile(subFilePath);
                    std::string line;
                    while (std::getline(subFile, line)) {
                        line = Utils::filterComments(line);
                        if (!line.empty() && line == item->name) {
                            collectionName = file.substr(0, position); // e.g., Laser Games
#ifdef _DEBUG
                            Logger::write(Logger::ZONE_DEBUG, "CollectionInfoBuilder", "Detected original collection via .sub file: " + collectionName + " for item " + item->name);
#endif
                            break;
                        }
                    }
                    subFile.close();
                    if (collectionName != info->name) break;
                }
            }
            closedir(dp);
        }
    }
#ifdef _DEBUG
    Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Saving stats for " + item->name + " to collection: " + collectionName);
#endif

    // Save stats to the collection's gamesinfo directory
    std::string statsDir = Utils::combinePath(Configuration::absolutePath, "collections", collectionName, "gamesinfo");
    std::string statsFile = Utils::combinePath(statsDir, item->name + ".txt");
#ifdef _DEBUG
    Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Saving game stats to " + statsFile);
#endif

    // Create gamesinfo directory if it doesn't exist
    struct stat infostat;
    if (stat(statsDir.c_str(), &infostat) != 0) {
#if defined(_WIN32) && !defined(__GNUC__)
        if (!CreateDirectory(statsDir.c_str(), NULL)) {
            if (ERROR_ALREADY_EXISTS != GetLastError()) {
#ifdef _DEBUG
                Logger::write(Logger::ZONE_WARNING, "CollectionInfoBuilder", "Could not create directory " + statsDir + ": Error " + std::to_string(GetLastError()));
#endif
                return;
            }
        }
#else
#if defined(__MINGW32__)
        if (mkdir(statsDir.c_str()) == -1)
#else
        if (mkdir(statsDir.c_str(), 0755) == -1)
#endif
        {
            if (errno != EEXIST) {
                Logger::write(Logger::ZONE_WARNING, "CollectionInfoBuilder", "Could not create directory " + statsDir + ": " + std::string(strerror(errno)));
                return;
            }
        }
#endif
    }
    else if (!(infostat.st_mode & S_IFDIR)) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_WARNING, "CollectionInfoBuilder", statsDir + " exists, but is not a directory.");
#endif
        return;
    }

    // Get current stats
    int playCount = 0;
    int totalPlayTime = 0;
    std::string lastPlayedDate;
    std::string lastPlayDuration = std::to_string(playDuration);

    std::string temp;
    try {
        if (item->getInfo("playCount", temp) && !temp.empty()) {
            playCount = std::stoi(temp);
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "CollectionInfoBuilder", "Current playCount=" + temp + " for " + item->name);
#endif
        }
        if (item->getInfo("totalPlayTime", temp) && !temp.empty()) {
            totalPlayTime = std::stoi(temp);
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "CollectionInfoBuilder", "Current totalPlayTime=" + temp + " for " + item->name);
#endif
        }
    }
    catch (const std::exception& e) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_WARNING, "CollectionInfoBuilder", "Invalid stats value for " + item->name + ": " + e.what());
#endif
        playCount = 0;
        totalPlayTime = 0;
    }

    // Update stats
    playCount++;
    totalPlayTime += playDuration;

    // Get current date
    time_t now = time(0);
    struct tm tstruct = *localtime(&now);
    char buf[80];
    strftime(buf, sizeof(buf), "%d/%b/%Y", &tstruct); // Format: DD/MMM/YYYY
    lastPlayedDate = buf;

    // Write to collection's stats file
    try {
        std::ofstream filestream(statsFile.c_str());
        if (filestream.is_open()) {
            filestream << "playCount=" << playCount << std::endl;
            filestream << "totalPlayTime=" << totalPlayTime << std::endl;
            filestream << "lastPlayedDate=" << lastPlayedDate << std::endl;
            filestream << "lastPlayDuration=" << lastPlayDuration << std::endl;
            filestream.close();
#ifdef _DEBUG
            Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Saved game stats for " + item->name + " to " + statsFile);
#endif

            // Update Item attributes
            item->setInfo("playCount", std::to_string(playCount));
            item->setInfo("totalPlayTime", std::to_string(totalPlayTime));
            item->setInfo("lastPlayedDate", lastPlayedDate);
            item->setInfo("lastPlayDuration", lastPlayDuration);
        }
        else {
#ifdef _DEBUG
            Logger::write(Logger::ZONE_ERROR, "CollectionInfoBuilder", "Failed to open stats file for writing: " + statsFile + ": Error " + std::string(strerror(errno)));
#endif
            return;
        }
    }
    catch (const std::exception& e) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_ERROR, "CollectionInfoBuilder", "Exception while saving stats to " + statsFile + ": " + e.what());
#endif
        return;
    }
}


void CollectionInfoBuilder::saveTopGameStats(CollectionInfo* info) {
    // Collect all relevant collections (main and subcollections)
    std::string mainCollectionName = info->originalCollection ? info->originalCollection->name : info->name;
    std::map<std::string, std::string> collectionMetadataTypes; // Map collectionName to metadataType
    std::set<std::string> subCollections; // Subcollection names from .sub files

    // Add main collection
    std::string metadataType = mainCollectionName;
    conf_.getProperty("collections." + mainCollectionName + ".metadata.type", metadataType);
    collectionMetadataTypes[mainCollectionName] = metadataType;

    // Check for subcollections via .sub files
    std::string collectionDir = Utils::combinePath(Configuration::absolutePath, "collections", info->name);
    DIR* dp = opendir(collectionDir.c_str());
    if (dp) {
        struct dirent* dirp;
        while ((dirp = readdir(dp)) != NULL) {
            std::string file = dirp->d_name;
            size_t position = file.find_last_of(".");
            std::string extension = (position != std::string::npos) ? file.substr(position) : "";
            if (extension == ".sub") {
                std::string subCollectionName = file.substr(0, position);
                subCollections.insert(subCollectionName);
                metadataType = subCollectionName;
                conf_.getProperty("collections." + subCollectionName + ".metadata.type", metadataType);
                collectionMetadataTypes[subCollectionName] = metadataType;
#ifdef _DEBUG
                Logger::write(Logger::ZONE_DEBUG, "CollectionInfoBuilder", "Detected subcollection: " + subCollectionName + " with metadataType: " + metadataType);
#endif
            }
        }
        closedir(dp);
    }

    // Variables to track the top game
    std::string topGameName; // Display name from XML
    std::string topGameFileName; // Filename (e.g., xmvsf)
    int topPlayCount = -1;
    int topTotalPlayTime = -1;
    std::string topLastPlayedDate;
    std::string topLastPlayDuration;
    time_t topLastPlayedTimeT = 0;
    std::string topCollectionName; // Collection where the top game resides

    // Process main collection's gamesinfo directory (if it exists)
    std::string mainGamesinfoDir = Utils::combinePath(Configuration::absolutePath, "collections", mainCollectionName, "gamesinfo");
    DIR* dir = opendir(mainGamesinfoDir.c_str());
    if (dir) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Processing main collection gamesinfo directory: " + mainGamesinfoDir);
#endif
        struct dirent* dirp;
        while ((dirp = readdir(dir)) != NULL) {
            std::string file = dirp->d_name;
            if (file.ends_with(".txt")) {
                std::string gameFileName = file.substr(0, file.find_last_of(".")); // e.g., xmvsf
                std::string statsPath = Utils::combinePath(mainGamesinfoDir, file);
                std::ifstream statsFile(statsPath);
                if (!statsFile.good()) {
#ifdef _DEBUG
                    Logger::write(Logger::ZONE_WARNING, "CollectionInfoBuilder", "Could not open stats file: " + statsPath);
#endif
                    continue;
                }

                int playCount = 0;
                int totalPlayTime = 0;
                std::string lastPlayedDate;
                std::string lastPlayDuration;
                time_t lastPlayedTimeT = 0;

                // Parse stats from the gamesinfo file
                std::string line;
                while (std::getline(statsFile, line)) {
                    line = Utils::filterComments(line);
                    if (line.empty()) continue;
                    size_t pos = line.find("=");
                    if (pos == std::string::npos) continue;
                    std::string key = line.substr(0, pos);
                    std::string value = line.substr(pos + 1);

                    try {
                        if (key == "playCount") {
                            playCount = std::stoi(value);
                        }
                        else if (key == "totalPlayTime") {
                            totalPlayTime = std::stoi(value);
                        }
                        else if (key == "lastPlayedDate") {
                            lastPlayedDate = value;
                            struct tm tstruct = {};
                            std::stringstream ss(value);
                            int day, year;
                            std::string monthStr;
                            char slash;
                            ss >> day >> slash >> monthStr >> slash >> year;
                            static const std::string months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
                            bool validMonth = false;
                            for (int i = 0; i < 12; ++i) {
                                if (monthStr == months[i]) {
                                    tstruct.tm_mon = i;
                                    validMonth = true;
                                    break;
                                }
                            }
                            if (validMonth) {
                                tstruct.tm_mday = day;
                                tstruct.tm_year = year - 1900;
                                lastPlayedTimeT = mktime(&tstruct);
                            }
                        }
                        else if (key == "lastPlayDuration") {
                            lastPlayDuration = value;
                        }
                    }
                    catch (const std::exception& e) {
                        Logger::write(Logger::ZONE_WARNING, "CollectionInfoBuilder", "Invalid value in " + statsPath + " for " + key + ": " + e.what());
                        if (key == "playCount" || key == "totalPlayTime") {
                            playCount = 0;
                            totalPlayTime = 0;
                        }
                    }
                }
                statsFile.close();

                // Get display name from HyperList XML
                std::string gameDisplayName;
                if (getGameDescriptionFromXml(collectionMetadataTypes[mainCollectionName], gameFileName, gameDisplayName) && !gameDisplayName.empty()) {
                    Logger::write(Logger::ZONE_DEBUG, "CollectionInfoBuilder", "Retrieved description for " + gameFileName + " in " + collectionMetadataTypes[mainCollectionName] + ": " + gameDisplayName);
                }
                else {
                    gameDisplayName = gameFileName; // Fallback to filename
                    Logger::write(Logger::ZONE_WARNING, "CollectionInfoBuilder", "No description found in XML for " + gameFileName + " in metadataType " + collectionMetadataTypes[mainCollectionName] + ", using filename");
                }

                // Update top game
                bool update = false;
                if (playCount > topPlayCount) {
                    update = true;
                }
                else if (playCount == topPlayCount && playCount >= 0) {
                    if (totalPlayTime > topTotalPlayTime) {
                        update = true;
                    }
                    else if (totalPlayTime == topTotalPlayTime) {
                        if (lastPlayedTimeT > topLastPlayedTimeT) {
                            update = true;
                        }
                        else if (lastPlayedTimeT == topLastPlayedTimeT) {
                            if (gameFileName < topGameFileName || topGameFileName.empty()) {
                                update = true;
                            }
                        }
                    }
                }

                if (update) {
                    topGameName = gameDisplayName;
                    topGameFileName = gameFileName;
                    topPlayCount = playCount;
                    topTotalPlayTime = totalPlayTime;
                    topLastPlayedDate = lastPlayedDate;
                    topLastPlayDuration = lastPlayDuration;
                    topLastPlayedTimeT = lastPlayedTimeT;
                    topCollectionName = info->name; // Save to the collection's system_artwork
                }
            }
        }
        closedir(dir);
    }

    // Process subcollections (.sub files)
    for (const auto& subCollectionName : subCollections) {
        std::string subFilePath = Utils::combinePath(collectionDir, subCollectionName + ".sub");
        std::ifstream subFile(subFilePath);
        if (!subFile.good()) {
#ifdef _DEBUG
            Logger::write(Logger::ZONE_WARNING, "CollectionInfoBuilder", "Could not open subcollection file: " + subFilePath);
#endif
            continue;
        }
#ifdef _DEBUG
        Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Processing subcollection file: " + subFilePath);
#endif
        std::string gameFileName;
        while (std::getline(subFile, gameFileName)) {
            gameFileName = Utils::filterComments(gameFileName);
            if (gameFileName.empty()) continue;

            // Check stats in the original collection's gamesinfo directory
            std::string statsPath = Utils::combinePath(Configuration::absolutePath, "collections", subCollectionName, "gamesinfo", gameFileName + ".txt");
            std::ifstream statsFile(statsPath);
            if (!statsFile.good()) {
#ifdef _DEBUG
                Logger::write(Logger::ZONE_DEBUG, "CollectionInfoBuilder", "No stats file found for " + gameFileName + " in " + subCollectionName + ": " + statsPath);
#endif
                continue;
            }

            int playCount = 0;
            int totalPlayTime = 0;
            std::string lastPlayedDate;
            std::string lastPlayDuration;
            time_t lastPlayedTimeT = 0;

            // Parse stats
            std::string line;
            while (std::getline(statsFile, line)) {
                line = Utils::filterComments(line);
                if (line.empty()) continue;
                size_t pos = line.find("=");
                if (pos == std::string::npos) continue;
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);

                try {
                    if (key == "playCount") {
                        playCount = std::stoi(value);
                    }
                    else if (key == "totalPlayTime") {
                        totalPlayTime = std::stoi(value);
                    }
                    else if (key == "lastPlayedDate") {
                        lastPlayedDate = value;
                        struct tm tstruct = {};
                        std::stringstream ss(value);
                        int day, year;
                        std::string monthStr;
                        char slash;
                        ss >> day >> slash >> monthStr >> slash >> year;
                        static const std::string months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
                        bool validMonth = false;
                        for (int i = 0; i < 12; ++i) {
                            if (monthStr == months[i]) {
                                tstruct.tm_mon = i;
                                validMonth = true;
                                break;
                            }
                        }
                        if (validMonth) {
                            tstruct.tm_mday = day;
                            tstruct.tm_year = year - 1900;
                            lastPlayedTimeT = mktime(&tstruct);
                        }
                    }
                    else if (key == "lastPlayDuration") {
                        lastPlayDuration = value;
                    }
                }
                catch (const std::exception& e) {
#ifdef _DEBUG
                    Logger::write(Logger::ZONE_WARNING, "CollectionInfoBuilder", "Invalid value in " + statsPath + " for " + key + ": " + e.what());
#endif
                    if (key == "playCount" || key == "totalPlayTime") {
                        playCount = 0;
                        totalPlayTime = 0;
                    }
                }
            }
            statsFile.close();

            // Get display name from HyperList XML
            std::string gameDisplayName;
            if (getGameDescriptionFromXml(collectionMetadataTypes[subCollectionName], gameFileName, gameDisplayName) && !gameDisplayName.empty()) {
                Logger::write(Logger::ZONE_DEBUG, "CollectionInfoBuilder", "Retrieved description for " + gameFileName + " in " + collectionMetadataTypes[subCollectionName] + ": " + gameDisplayName);
            }
            else {
                gameDisplayName = gameFileName; // Fallback to filename
#ifdef _DEBUG
                Logger::write(Logger::ZONE_WARNING, "CollectionInfoBuilder", "No description found in XML for " + gameFileName + " in metadataType " + collectionMetadataTypes[subCollectionName] + ", using filename");
#endif
            }

            // Update top game
            bool update = false;
            if (playCount > topPlayCount) {
                update = true;
            }
            else if (playCount == topPlayCount && playCount >= 0) {
                if (totalPlayTime > topTotalPlayTime) {
                    update = true;
                }
                else if (totalPlayTime == topTotalPlayTime) {
                    if (lastPlayedTimeT > topLastPlayedTimeT) {
                        update = true;
                    }
                    else if (lastPlayedTimeT == topLastPlayedTimeT) {
                        if (gameFileName < topGameFileName || topGameFileName.empty()) {
                            update = true;
                        }
                    }
                }
            }

            if (update) {
                topGameName = gameDisplayName;
                topGameFileName = gameFileName;
                topPlayCount = playCount;
                topTotalPlayTime = totalPlayTime;
                topLastPlayedDate = lastPlayedDate;
                topLastPlayDuration = lastPlayDuration;
                topLastPlayedTimeT = lastPlayedTimeT;
                topCollectionName = info->name; // Save to the collection's system_artwork
            }
        }
        subFile.close();
    }

    // If no valid games found, do not create files
    if (topGameName.empty()) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "No valid game stats found in any collection for " + info->name);
#endif
        return;
    }

    // Get layout name
    std::string layoutName;
    conf_.getProperty("layout", layoutName);
    if (layoutName.empty()) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_WARNING, "CollectionInfoBuilder", "No layout specified, using 'default'");
#endif
        return;
    }

    // Construct file path for the collection with the top game
#if defined(_WIN32) || defined(_WIN64)
    const char separator = '\\';
#else
    const char separator = '/';
#endif
    std::string filePath = Utils::combinePath(Configuration::absolutePath, "layouts", layoutName, "collections", topCollectionName, "system_artwork");

    // Check if top game has changed
    std::string currentTopGame;
    std::string topGameNamePath = Utils::combinePath(filePath, "topgamename.txt");
    std::ifstream topGameFile(topGameNamePath);
    if (topGameFile.good()) {
        std::getline(topGameFile, currentTopGame);
        topGameFile.close();
    }

    bool updateFiles = currentTopGame != topGameName;
    if (!updateFiles) {
        // Check if stats have changed
        std::string currentPlayCount, currentTotalPlayTime, currentLastPlayedDate, currentLastPlayDuration;
        std::ifstream file;
        file.open(Utils::combinePath(filePath, "topplaycount.txt"));
        if (file.good()) std::getline(file, currentPlayCount);
        file.close();
        file.open(Utils::combinePath(filePath, "topplaytime.txt"));
        if (file.good()) std::getline(file, currentTotalPlayTime);
        file.close();
        file.open(Utils::combinePath(filePath, "toplastplaydate.txt"));
        if (file.good()) std::getline(file, currentLastPlayedDate);
        file.close();
        file.open(Utils::combinePath(filePath, "toplastplaytime.txt"));
        if (file.good()) std::getline(file, currentLastPlayDuration);
        file.close();

        updateFiles = (currentPlayCount != std::to_string(topPlayCount) ||
            currentTotalPlayTime != std::to_string(topTotalPlayTime) ||
            currentLastPlayedDate != topLastPlayedDate ||
            currentLastPlayDuration != topLastPlayDuration);
    }

    if (updateFiles) {
        // Create directory if it doesn't exist
        Utils::createDirectories(filePath);

        // Write top game stats to files
        std::ofstream outFile;
        outFile.open(topGameNamePath);
        if (outFile.is_open()) {
            outFile << topGameName; // Write display name
            outFile.close();
#ifdef _DEBUG
            Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Wrote top game name to " + topGameNamePath);
#endif
        }
        else {
            Logger::write(Logger::ZONE_ERROR, "CollectionInfoBuilder", "Failed to write to " + topGameNamePath);
        }

        outFile.open(Utils::combinePath(filePath, "topplaycount.txt"));
        if (outFile.is_open()) {
            outFile << topPlayCount;
            outFile.close();
#ifdef _DEBUG
            Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Wrote top play count to " + filePath + "/topplaycount.txt");
#endif
        }
        else {
            Logger::write(Logger::ZONE_ERROR, "CollectionInfoBuilder", "Failed to write to " + filePath + "/topplaycount.txt");
        }

        outFile.open(Utils::combinePath(filePath, "topplaytime.txt"));
        if (outFile.is_open()) {
            outFile << topTotalPlayTime;
            outFile.close();
#ifdef _DEBUG
            Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Wrote top play time to " + filePath + "/topplaytime.txt");
#endif
        }
        else {
            Logger::write(Logger::ZONE_ERROR, "CollectionInfoBuilder", "Failed to write to " + filePath + "/topplaytime.txt");
        }

        outFile.open(Utils::combinePath(filePath, "toplastplaydate.txt"));
        if (outFile.is_open()) {
            outFile << topLastPlayedDate;
            outFile.close();
#ifdef _DEBUG
            Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Wrote top last played date to " + filePath + "/toplastplaydate.txt");
#endif
        }
        else {
            Logger::write(Logger::ZONE_ERROR, "CollectionInfoBuilder", "Failed to write to " + filePath + "/toplastplaydate.txt");
        }

        outFile.open(Utils::combinePath(filePath, "toplastplaytime.txt"));
        if (outFile.is_open()) {
            outFile << topLastPlayDuration;
            outFile.close();
#ifdef _DEBUG
            Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Wrote top last play duration to " + filePath + "/toplastplaytime.txt");
#endif
        }
        else {
            Logger::write(Logger::ZONE_ERROR, "CollectionInfoBuilder", "Failed to write to " + filePath + "/toplastplaytime.txt");
        }
    }
    else {
        Logger::write(Logger::ZONE_DEBUG, "CollectionInfoBuilder", "No update needed for top game files in " + topCollectionName);
    }
}
bool CollectionInfoBuilder::getGameDescriptionFromXml(const std::string& metadataType, const std::string& gameName, std::string& description) {
    std::string xmlPath = Utils::combinePath(Configuration::absolutePath, "meta", "hyperlist", metadataType + ".xml");
    xmlTextReaderPtr reader = xmlReaderForFile(xmlPath.c_str(), NULL, 0);
    if (!reader) {
        Logger::write(Logger::ZONE_WARNING, "CollectionInfoBuilder", "Could not open XML file: " + xmlPath);
        description = "";
        return false;
    }

    bool found = false;
    description = "";
    bool inGame = false;

    while (xmlTextReaderRead(reader) == 1) {
        int nodeType = xmlTextReaderNodeType(reader);
        if (nodeType != XML_READER_TYPE_ELEMENT) continue;
        const xmlChar* name = xmlTextReaderConstName(reader);
        if (!name) continue;

        if (std::string(reinterpret_cast<const char*>(name)) == "game") {
            inGame = true;
            const xmlChar* gameNameAttr = xmlTextReaderGetAttribute(reader, BAD_CAST "name");
            if (gameNameAttr && std::string(reinterpret_cast<const char*>(gameNameAttr)) == gameName) {
                while (xmlTextReaderRead(reader) == 1) {
                    nodeType = xmlTextReaderNodeType(reader);
                    name = xmlTextReaderConstName(reader);
                    if (nodeType == XML_READER_TYPE_ELEMENT && name && std::string(reinterpret_cast<const char*>(name)) == "description") {
                        xmlTextReaderRead(reader); // Move to text node
                        const xmlChar* value = xmlTextReaderConstValue(reader);
                        if (value) {
                            description = reinterpret_cast<const char*>(value);
#ifdef _DEBUG
                            Logger::write(Logger::ZONE_DEBUG, "CollectionInfoBuilder", "Found description for " + gameName + " in " + xmlPath + ": " + description);
#endif
                            found = true;
                        }
                        else {
                            Logger::write(Logger::ZONE_WARNING, "CollectionInfoBuilder", "Empty description for " + gameName + " in " + xmlPath);
                        }
                        break; // Stop after finding description
                    }
                    else if (nodeType == XML_READER_TYPE_END_ELEMENT && name && std::string(reinterpret_cast<const char*>(name)) == "game") {
                        break; // Exit game node
                    }
                }
                xmlFree((void*)gameNameAttr);
                if (found) break; // Stop parsing file
            }
            xmlFree((void*)gameNameAttr);
        }
        else if (inGame && nodeType == XML_READER_TYPE_END_ELEMENT && name && std::string(reinterpret_cast<const char*>(name)) == "game") {
            inGame = false;
        }
        if (found) break; // Exit outer loop
    }

    xmlTextReaderClose(reader);
    xmlFreeTextReader(reader);

    if (!found) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_WARNING, "CollectionInfoBuilder", "No game found with name " + gameName + " or no description in " + xmlPath);
#endif
        description = "";
        return false;
    }

    return true;
}


//void CollectionInfoBuilder::saveGameStats(CollectionInfo* info, Item* item, int playDuration) {
//    std::string collectionName = item->collectionInfo ? item->collectionInfo->name : info->name; // Prefer item->collectionInfo
//
//    // If launched from a subcollection, use originalCollection to confirm
//    if (info->originalCollection) {
//        collectionName = info->name; // Subcollection (e.g., Laser Games)
//        Logger::write(Logger::ZONE_DEBUG, "CollectionInfoBuilder", "Using originalCollection: " + collectionName + " for item " + item->name);
//    }
//    else if (collectionName == info->name) {
//        // Fallback: Check for .sub files to confirm original collection
//        std::string collectionDir = Utils::combinePath(Configuration::absolutePath, "collections", info->name);
//        DIR* dp = opendir(collectionDir.c_str());
//        if (dp) {
//            struct dirent* dirp;
//            while ((dirp = readdir(dp)) != NULL) {
//                std::string file = dirp->d_name;
//                size_t position = file.find_last_of(".");
//                std::string extension = (position != std::string::npos) ? file.substr(position) : "";
//                if (extension == ".sub") {
//                    std::string subFilePath = Utils::combinePath(collectionDir, file);
//                    std::ifstream subFile(subFilePath);
//                    std::string line;
//                    while (std::getline(subFile, line)) {
//                        line = Utils::filterComments(line);
//                        if (!line.empty() && line == item->name) {
//                            collectionName = file.substr(0, position); // e.g., Laser Games
//                            Logger::write(Logger::ZONE_DEBUG, "CollectionInfoBuilder", "Detected original collection via .sub file: " + collectionName + " for item " + item->name);
//                            break;
//                        }
//                    }
//                    subFile.close();
//                    if (collectionName != info->name) break;
//                }
//            }
//            closedir(dp);
//        }
//    }
//
//    Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Saving stats for " + item->name + " to collection: " + collectionName);
//
//    // Save stats to the collection's gamesinfo directory
//    std::string statsDir = Utils::combinePath(Configuration::absolutePath, "collections", collectionName, "gamesinfo");
//    std::string statsFile = Utils::combinePath(statsDir, item->name + ".txt");
//    Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Saving game stats to " + statsFile);
//
//    // Create gamesinfo directory if it doesn't exist
//    struct stat infostat;
//    if (stat(statsDir.c_str(), &infostat) != 0) {
//#if defined(_WIN32) && !defined(__GNUC__)
//        if (!CreateDirectory(statsDir.c_str(), NULL)) {
//            if (ERROR_ALREADY_EXISTS != GetLastError()) {
//                Logger::write(Logger::ZONE_WARNING, "CollectionInfoBuilder", "Could not create directory " + statsDir + ": Error " + std::to_string(GetLastError()));
//                return;
//            }
//        }
//#else
//#if defined(__MINGW32__)
//        if (mkdir(statsDir.c_str()) == -1)
//#else
//        if (mkdir(statsDir.c_str(), 0755) == -1)
//#endif
//        {
//            if (errno != EEXIST) {
//                Logger::write(Logger::ZONE_WARNING, "CollectionInfoBuilder", "Could not create directory " + statsDir + ": " + std::string(strerror(errno)));
//                return;
//            }
//        }
//#endif
//    }
//    else if (!(infostat.st_mode & S_IFDIR)) {
//        Logger::write(Logger::ZONE_WARNING, "CollectionInfoBuilder", statsDir + " exists, but is not a directory.");
//        return;
//    }
//
//    // Get current stats
//    int playCount = 0;
//    int totalPlayTime = 0;
//    std::string lastPlayedDate;
//    std::string lastPlayDuration = std::to_string(playDuration);
//
//    std::string temp;
//    try {
//        if (item->getInfo("playCount", temp) && !temp.empty()) {
//            playCount = std::stoi(temp);
//            Logger::write(Logger::ZONE_DEBUG, "CollectionInfoBuilder", "Current playCount=" + temp + " for " + item->name);
//        }
//        if (item->getInfo("totalPlayTime", temp) && !temp.empty()) {
//            totalPlayTime = std::stoi(temp);
//            Logger::write(Logger::ZONE_DEBUG, "CollectionInfoBuilder", "Current totalPlayTime=" + temp + " for " + item->name);
//        }
//    }
//    catch (const std::exception& e) {
//        Logger::write(Logger::ZONE_WARNING, "CollectionInfoBuilder", "Invalid stats value for " + item->name + ": " + e.what());
//        playCount = 0;
//        totalPlayTime = 0;
//    }
//
//    // Update stats
//    playCount++;
//    totalPlayTime += playDuration;
//
//    // Get current date
//    time_t now = time(0);
//    struct tm tstruct = *localtime(&now);
//    char buf[80];
//    strftime(buf, sizeof(buf), "%d/%b/%Y", &tstruct); // Format: DD/MMM/YYYY
//    lastPlayedDate = buf;
//
//    // Write to collection's stats file
//    try {
//        std::ofstream filestream(statsFile.c_str());
//        if (filestream.is_open()) {
//            filestream << "playCount=" << playCount << std::endl;
//            filestream << "totalPlayTime=" << totalPlayTime << std::endl;
//            filestream << "lastPlayedDate=" << lastPlayedDate << std::endl;
//            filestream << "lastPlayDuration=" << lastPlayDuration << std::endl;
//            filestream.close();
//            Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Saved game stats for " + item->name + " to " + statsFile);
//
//            // Update Item attributes
//            item->setInfo("playCount", std::to_string(playCount));
//            item->setInfo("totalPlayTime", std::to_string(totalPlayTime));
//            item->setInfo("lastPlayedDate", lastPlayedDate);
//            item->setInfo("lastPlayDuration", lastPlayDuration);
//        }
//        else {
//            Logger::write(Logger::ZONE_ERROR, "CollectionInfoBuilder", "Failed to open stats file for writing: " + statsFile + ": Error " + std::string(strerror(errno)));
//            return;
//        }
//    }
//    catch (const std::exception& e) {
//        Logger::write(Logger::ZONE_ERROR, "CollectionInfoBuilder", "Exception while saving stats to " + statsFile + ": " + e.what());
//        return;
//    }
//}

//int CollectionInfoBuilder::getActualGameCount(Configuration& config, const std::string& collectionName,
//    MetadataDatabase& metaDB) {
//    std::string basePath = Utils::combinePath(Configuration::absolutePath, "collections", collectionName);
//    std::string includeFile = Utils::combinePath(basePath, "include.txt");
//    std::string excludeFile = Utils::combinePath(basePath, "exclude.txt");
//    std::string cacheFile = Utils::combinePath(basePath, "gamecount.cache");
//    std::string romsPath;
//    config.getCollectionAbsolutePath(collectionName, romsPath);
//    bool hasRomsPath = !romsPath.empty() && fs::exists(romsPath);
//    bool includeMissingItems = false;
//    config.getProperty("collections." + collectionName + ".list.includeMissingItems", includeMissingItems);
//    Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Processing collection: " + collectionName);
//    Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Main ROM path: " + (hasRomsPath ? romsPath : "none"));
//    Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "includeMissingItems: " + std::string(includeMissingItems ? "true" : "false"));
//
//    // Step 1: Check cache
//    int cachedCount = -1;
//    fs::file_time_type cacheTime{};
//    if (fs::exists(cacheFile)) {
//        std::ifstream cacheIn(cacheFile);
//        if (cacheIn.good()) {
//            std::string line;
//            if (std::getline(cacheIn, line)) {
//                try {
//                    cachedCount = std::stoi(line);
//                    Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Read cached count: " + std::to_string(cachedCount));
//                }
//                catch (const std::exception& e) {
//                    Logger::write(Logger::ZONE_ERROR, "CollectionInfoBuilder", "Invalid cache count in " + cacheFile + ": " + e.what());
//                }
//            }
//            cacheIn.close();
//            try {
//                cacheTime = fs::last_write_time(cacheFile);
//            }
//            catch (const std::filesystem::filesystem_error& e) {
//                Logger::write(Logger::ZONE_ERROR, "CollectionInfoBuilder", std::string("Failed to get cache file time: ") + e.what());
//            }
//        }
//    }
//
//    // Step 2: Check if cache is up-to-date
//    bool needsRecalc = cachedCount == -1;
//    if (!needsRecalc) {
//        if (hasRomsPath && fs::last_write_time(romsPath) > cacheTime) needsRecalc = true;
//        if (fs::exists(includeFile) && fs::last_write_time(includeFile) > cacheTime) needsRecalc = true;
//        if (fs::exists(excludeFile) && fs::last_write_time(excludeFile) > cacheTime) needsRecalc = true;
//        DIR* dp = opendir(basePath.c_str());
//        if (dp) {
//            struct dirent* dirp;
//            while (!needsRecalc && (dirp = readdir(dp)) != NULL) {
//                std::string file = dirp->d_name;
//                if (file.ends_with(".sub")) {
//                    std::string subFile = Utils::combinePath(basePath, file);
//                    if (fs::exists(subFile) && fs::last_write_time(subFile) > cacheTime) {
//                        needsRecalc = true;
//                        break;
//                    }
//                }
//            }
//            closedir(dp);
//        }
//    }
//
//    if (!needsRecalc) {
//        Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Using cached count for " + collectionName + ": " + std::to_string(cachedCount));
//        return cachedCount;
//    }
//
//    // Step 3: Recalculate count
//    CollectionInfoBuilder builder(config, metaDB);
//    std::string extensionsStr;
//    if (!config.getProperty("collections." + collectionName + ".list.extensions", extensionsStr)) {
//        extensionsStr = "zip";
//        Logger::write(Logger::ZONE_WARNING, "CollectionInfoBuilder", "No extensions defined for " + collectionName + "; using default: " + extensionsStr);
//    }
//    Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Main extensions: " + extensionsStr);
//    CollectionInfo tempInfo(collectionName, romsPath, extensionsStr, "", "");
//
//    std::vector<Item*> listedItems;
//    std::map<std::string, Item*> globalIncludeFilter; // Unified filter for all collections
//    std::map<std::string, Item*> globalExcludeFilter;
//
//    if (fs::exists(includeFile)) {
//        Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Loading main include.txt: " + includeFile);
//        builder.ImportBasicList(&tempInfo, includeFile, listedItems);
//        for (auto* item : listedItems) {
//            globalIncludeFilter[item->name] = item;
//        }
//    }
//
//    if (fs::exists(excludeFile)) {
//        Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Loading main exclude.txt: " + excludeFile);
//        builder.ImportBasicList(&tempInfo, excludeFile, globalExcludeFilter);
//    }
//
//    int actualCount = 0;
//    if (hasRomsPath) {
//        actualCount = countActualFiles(config, collectionName, romsPath, globalIncludeFilter, globalExcludeFilter);
//        Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Main collection ROM count: " + std::to_string(actualCount));
//    }
//
//    DIR* dp = opendir(basePath.c_str());
//    if (dp) {
//        struct dirent* dirp;
//        while ((dirp = readdir(dp)) != NULL) {
//            std::string file = dirp->d_name;
//            if (file.ends_with(".sub")) {
//                std::string subCollectionName = file.substr(0, file.find_last_of("."));
//                std::string subFile = Utils::combinePath(basePath, file);
//                Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Processing sub-collection file: " + subFile);
//
//                std::vector<Item*> subListedItems;
//                builder.ImportBasicList(&tempInfo, subFile, subListedItems);
//                for (auto* item : subListedItems) {
//                    if (globalIncludeFilter.find(item->name) == globalIncludeFilter.end()) {
//                        globalIncludeFilter[item->name] = item; // Add to global filter
//                    }
//                }
//                listedItems.insert(listedItems.end(), subListedItems.begin(), subListedItems.end());
//
//                std::string subRomsPath;
//                config.getCollectionAbsolutePath(subCollectionName, subRomsPath);
//                if (!subRomsPath.empty() && fs::exists(subRomsPath)) {
//                    std::string subExtensionsStr;
//                    if (!config.getProperty("collections." + subCollectionName + ".list.extensions", subExtensionsStr)) {
//                        subExtensionsStr = extensionsStr; // Fall back to main collection’s extensions
//                        Logger::write(Logger::ZONE_WARNING, "CollectionInfoBuilder", "No extensions for " + subCollectionName + "; using main: " + subExtensionsStr);
//                    }
//                    Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Sub-collection " + subCollectionName + " ROM path: " + subRomsPath);
//                    Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Sub-collection extensions: " + subExtensionsStr);
//
//                    // Use subcollection-specific extensions with global filters
//                    int subCount = countActualFiles(config, subCollectionName, subRomsPath, globalIncludeFilter, globalExcludeFilter);
//                    actualCount += subCount;
//                    Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Sub-collection " + subCollectionName + " count: " + std::to_string(subCount));
//                }
//                else {
//                    Logger::write(Logger::ZONE_WARNING, "CollectionInfoBuilder", "No valid ROM path for sub-collection: " + subCollectionName);
//                }
//            }
//        }
//        closedir(dp);
//    }
//
//    std::ofstream cacheOut(cacheFile);
//    if (cacheOut.good()) {
//        cacheOut << actualCount << std::endl;
//        cacheOut.close();
//        Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Wrote count to cache: " + std::to_string(actualCount));
//    }
//    else {
//        Logger::write(Logger::ZONE_ERROR, "CollectionInfoBuilder", "Failed to write cache file: " + cacheFile);
//    }
//
//    for (auto* item : listedItems) {
//        delete item;
//    }
//
//    Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Total count for " + collectionName + ": " + std::to_string(actualCount));
//    return actualCount;
//}
//
//int CollectionInfoBuilder::countActualFiles(Configuration& config, const std::string& collectionName,
//    const std::string& dirPath,
//    const std::map<std::string, Item*>& includeFilter,
//    const std::map<std::string, Item*>& excludeFilter) {
//    if (!fs::exists(dirPath)) {
//        Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Directory does not exist: " + dirPath);
//        return 0;
//    }
//
//    std::string extensionsStr;
//    if (!config.getProperty("collections." + collectionName + ".list.extensions", extensionsStr)) {
//        extensionsStr = "zip"; 
//        Logger::write(Logger::ZONE_WARNING, "CollectionInfoBuilder", "No extensions defined for " + collectionName + "; using default: " + extensionsStr);
//    }
//    std::vector<std::string> extensions;
//    Utils::listToVector(extensionsStr, extensions, ',');
//    Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Counting files in " + dirPath + " with extensions: " + extensionsStr);
//
//    int count = 0;
//    std::set<std::string> topLevelBasenames; 
//
//    try {
//        // Step 1: Count top-level files and build basename set
//        for (const auto& entry : fs::directory_iterator(dirPath)) {
//            if (entry.is_regular_file()) {
//                std::string fileName = entry.path().filename().string();
//                std::string basename = fileName.substr(0, fileName.find_last_of("."));
//                std::string ext = entry.path().extension().string();
//                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
//
//                bool extensionMatch = false;
//                for (const auto& e : extensions) {
//                    std::string lowercaseExt = e;
//                    std::transform(lowercaseExt.begin(), lowercaseExt.end(), lowercaseExt.begin(), ::tolower);
//                    if (ext == "." + lowercaseExt || ext == lowercaseExt) {
//                        extensionMatch = true;
//                        break;
//                    }
//                }
//
//                if (extensionMatch &&
//                    (includeFilter.empty() || includeFilter.find(basename) != includeFilter.end()) &&
//                    excludeFilter.find(basename) == excludeFilter.end()) {
//                    Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Counted top-level file: " + entry.path().string());
//                    count++;
//                    topLevelBasenames.insert(basename); 
//                }
//            }
//        }
//
//        // Step 2: Process subfolders, skipping those matching top-level basenames
//        for (const auto& entry : fs::directory_iterator(dirPath)) {
//            if (entry.is_directory() && entry.path().filename().string() != "." && entry.path().filename().string() != "..") {
//                std::string subfolderName = entry.path().filename().string();
//                if (topLevelBasenames.find(subfolderName) != topLevelBasenames.end()) {
//                    Logger::write(Logger::ZONE_DEBUG, "CollectionInfoBuilder", "Skipped subfolder (matches top-level basename): " + entry.path().string());
//                    continue; // Skip entire subfolder if its name matches a top-level basename
//                }
//
//                for (const auto& subEntry : fs::directory_iterator(entry.path())) {
//                    if (subEntry.is_regular_file()) {
//                        std::string fileName = subEntry.path().filename().string();
//                        std::string basename = fileName.substr(0, fileName.find_last_of("."));
//                        std::string ext = subEntry.path().extension().string();
//                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
//
//                        bool extensionMatch = false;
//                        for (const auto& e : extensions) {
//                            std::string lowercaseExt = e;
//                            std::transform(lowercaseExt.begin(), lowercaseExt.end(), lowercaseExt.begin(), ::tolower);
//                            if (ext == "." + lowercaseExt || ext == lowercaseExt) {
//                                extensionMatch = true;
//                                break;
//                            }
//                        }
//
//                        if (extensionMatch &&
//                            (includeFilter.empty() || includeFilter.find(basename) != includeFilter.end()) &&
//                            excludeFilter.find(basename) == excludeFilter.end()) {
//                            Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Counted subfolder file: " + subEntry.path().string());
//                            count++;
//                        }
//                    }
//                }
//            }
//        }
//    }
//    catch (const std::filesystem::filesystem_error& e) {
//        Logger::write(Logger::ZONE_ERROR, "CollectionInfoBuilder", "Filesystem error: " + std::string(e.what()));
//        return count; 
//    }
//
//    Logger::write(Logger::ZONE_INFO, "CollectionInfoBuilder", "Total files counted in " + dirPath + ": " + std::to_string(count));
//    return count;
//}