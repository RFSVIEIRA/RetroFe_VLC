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

#include "ReloadableText.h"
#include "../ViewInfo.h"
#include "../../Database/Configuration.h"
#include "../../Utility/Log.h"
#include "../../SDL.h"
#include <fstream>
#include <vector>
#include <iostream>
#include <time.h>
#include <algorithm>
#include "../../Collection/CollectionInfoBuilder.h"
#include "../../Utility/Utils.h"
#include <ctime>

ReloadableText::ReloadableText(std::string type, Page &page, Configuration &config, bool systemMode, Font *font, std::string layoutKey, std::string timeFormat, std::string textFormat, std::string singlePrefix, std::string singlePostfix, std::string pluralPrefix, std::string pluralPostfix)
    : Component(page)
    , config_(config)
    , systemMode_(systemMode)
    , imageInst_(NULL)
    , type_(type)
    , layoutKey_(layoutKey)
    , fontInst_(font)
    , timeFormat_(timeFormat)
    , textFormat_(textFormat)
    , singlePrefix_(singlePrefix)
    , singlePostfix_(singlePostfix)
    , pluralPrefix_(pluralPrefix)
    , pluralPostfix_(pluralPostfix)
{
    allocateGraphicsMemory();
}



ReloadableText::~ReloadableText()
{
    if (imageInst_ != NULL)
    {
        delete imageInst_;
    }
}

void ReloadableText::update(float dt)
{
    if (newItemSelected ||
       (newScrollItemSelected && getMenuScrollReload()) ||
        type_ == "time" || type_ == "current" || type_ == "duration" || type_ == "isPaused")
    {
        ReloadTexture();
        newItemSelected = false;
    }

    // needs to be ran at the end to prevent the NewItemSelected flag from being detected
    Component::update(dt);

}

void ReloadableText::allocateGraphicsMemory()
{
    ReloadTexture();

    // NOTICE! needs to be done last to prevent flags from being missed
    Component::allocateGraphicsMemory();
}


void ReloadableText::freeGraphicsMemory()
{
    Component::freeGraphicsMemory();

    if (imageInst_ != NULL)
    {
        delete imageInst_;
        imageInst_ = NULL;
    }
}


void ReloadableText::initializeFonts()
{
    fontInst_->initialize();
}


void ReloadableText::deInitializeFonts()
{
    fontInst_->deInitialize();
}


void ReloadableText::ReloadTexture()
{
    if (imageInst_ != NULL)
    {
        delete imageInst_;
        imageInst_ = NULL;
    }

    Item *selectedItem = page.getSelectedItem();

    if (selectedItem != NULL)
    {
        std::stringstream ss;
        std::string text = "";
        if (type_ == "time")
        {
          time_t    now = time(0);
          struct tm tstruct;
          char      buf[80];
          tstruct = *localtime(&now);
          strftime(buf, sizeof(buf), timeFormat_.c_str(), &tstruct);
          ss << buf;
        }
        if (type_ == "numberButtons")
        {
            text = selectedItem->numberButtons;
        }
        else if (type_ == "numberPlayers")
        {
            text = selectedItem->numberPlayers;
        }
        else if (type_ == "ctrlType")
        {
            text = selectedItem->ctrlType;
        }
        else if (type_ == "numberJoyWays")
        {
            text = selectedItem->joyWays;
        }
        else if (type_ == "rating")
        {
            text = selectedItem->rating;
        }
        else if (type_ == "score")
        {
            text = selectedItem->score;
        }
        else if (type_ == "year")
        {
              text = selectedItem->year;
        }
        else if (type_ == "title")
        {
            text = selectedItem->title;
        }
        else if(type_ == "developer")
        {
            text = selectedItem->developer;
            // Overwrite in case developer has not been specified
            if (text == "")
            {
                text = selectedItem->manufacturer;
            }
        }
        else if (type_ == "manufacturer")
        {
            text = selectedItem->manufacturer;
        }
        else if (type_ == "genre")
        {
            text = selectedItem->genre;
        }
        else if (type_.rfind( "playlist", 0 ) == 0)
        {
            text = playlistName;
        }
        else if (type_ == "firstLetter")
        {
          text = selectedItem->fullTitle.at(0);
        }
        else if (type_ == "collectionName")
        {
            text = page.getCollectionName();
        }
        else if (type_ == "availablecollectionize") 
        {
            MetadataDatabase* metadb = page.getMetadataDatabase();
            if (metadb && !selectedItem->leaf) { // Check if it’s a collection
                std::string collectionName = selectedItem->name;
                int gameCount = metadb->getGameCount(collectionName);
                if (gameCount == 1) {
                    ss << singlePrefix_ << gameCount << singlePostfix_;
                }
                else {
                    ss << pluralPrefix_ << gameCount << pluralPostfix_;
                }
            }
            else {
                ss << ""; // Fallback if no database or not a collection
            }
        }
        /*else if (type_ == "actualCollectionSize" && selectedItem != NULL && !selectedItem->leaf) {
            MetadataDatabase* metaDB = page.getMetadataDatabase();
            if (metaDB) {
                int gameCount = CollectionInfoBuilder::getActualGameCount(config_, selectedItem->name, *metaDB);
                if (gameCount == 1) {
                    ss << singlePrefix_ << gameCount << singlePostfix_;
                }
                else if (gameCount > 1) {
                    ss << pluralPrefix_ << gameCount << pluralPostfix_;
                }
                else {
                    ss << "";
                }
            }
            else {
                ss << "";
            }
        }*/
        else if (type_ == "collectionSize")
        {
            if (page.getCollectionSize() == 0)
            {
                ss << singlePrefix_ << page.getCollectionSize() << pluralPostfix_;
            }
            else if (page.getCollectionSize() == 1)
            {
                ss << singlePrefix_ << page.getCollectionSize() << singlePostfix_;
            }
            else
            {
                ss << pluralPrefix_ << page.getCollectionSize() << pluralPostfix_;
            }
        }
        else if (type_ == "collectionIndex")
        {
            if (page.getSelectedIndex() == 0)
            {
                ss << singlePrefix_ << (page.getSelectedIndex()+1) << pluralPostfix_;
            }
            else if (page.getSelectedIndex() == 1)
            {
                ss << singlePrefix_ << (page.getSelectedIndex()+1) << singlePostfix_;
            }
            else
            {
                ss << pluralPrefix_ << (page.getSelectedIndex()+1) << pluralPostfix_;
            }
        }
        else if (type_ == "collectionIndexSize")
        {
            size_t size = page.getCollectionSize();
            if (page.getSelectedIndex() == 0)
            {
                ss << singlePrefix_ << (page.getSelectedIndex()+1) << "/" << page.getCollectionSize() << pluralPostfix_;
            }
            else if (page.getSelectedIndex() == 1)
            {
                ss << singlePrefix_ << (page.getSelectedIndex()+1) << "/" << page.getCollectionSize() << singlePostfix_;
            }
            else
            {
                ss << pluralPrefix_ << (page.getSelectedIndex()+1) << "/" << page.getCollectionSize() << pluralPostfix_;
            }
         saveCollectionSizeToFile(size);
        }
        else if (type_ == "isFavorite")
        {
            if (selectedItem->isFavorite)
                text = "yes";
            else
                text = "no";
        }
        else if (type_ == "isPaused")
        {
            if (page.isPaused( ))
                text = "Paused";
            else
                text = "";
        }
        else if (type_ == "current")
        {
            unsigned long long current = 0;
            current     = page.getCurrent( );
            current    /= 1000000000;
            int seconds = current%60;
            int minutes = (current/60)%60;
            int hours   = int( current/3600 );
            text        = std::to_string( hours ) + ":";
            if ( minutes < 10 )
                text   += "0" + std::to_string( minutes ) + ":";
            else
                text   += std::to_string( minutes ) + ":";
            if ( seconds < 10 )
                text   +=  "0" + std::to_string( seconds );
            else
                text   += std::to_string( seconds );
			if ( page.getDuration( ) == 0 )
				text    = "--:--:--";
        }
        else if (type_ == "duration")
        {
            unsigned long long duration = 0;
            duration    = page.getDuration( );
            duration   /= 1000000000;
            int seconds = duration%60;
            int minutes = (duration/60)%60;
            int hours   = int( duration/3600 );
            text        = std::to_string( hours ) + ":";
            if ( minutes < 10 )
                text   += "0" + std::to_string( minutes ) + ":";
            else
                text   += std::to_string( minutes ) + ":";
            if ( seconds < 10 )
                text   +=  "0" + std::to_string( seconds );
            else
                text   += std::to_string( seconds );
			if ( page.getDuration( ) == 0 )
				text    = "--:--:--";
        }
        else if (type_ == "playCount")
        {
            text = selectedItem->playCount;
            }
        else if (type_ == "totalPlayTime")
        {
            std::string minutesStr;
            if (selectedItem->getInfo("totalPlayTime", minutesStr) && !minutesStr.empty()) {
                try {
                    int totalMinutes = std::stoi(minutesStr);
                    struct tm tstruct = {};
                    tstruct.tm_hour = totalMinutes / 60;
                    tstruct.tm_min = totalMinutes % 60;
                    char buf[80];
                    strftime(buf, sizeof(buf), timeFormat_.c_str(), &tstruct);
                    text = buf;
                    Logger::write(Logger::ZONE_DEBUG, "ReloadableText", "totalPlayTime=" + std::string(buf) + " for " + selectedItem->name);
                }
                catch (const std::exception& e) {
                    Logger::write(Logger::ZONE_WARNING, "ReloadableText", "Invalid totalPlayTime value: " + minutesStr + " for " + selectedItem->name + ", error: " + e.what());
                    text = "";
                }
            }
            else {
                Logger::write(Logger::ZONE_DEBUG, "ReloadableText", "totalPlayTime not found or empty for " + selectedItem->name);
                text = "";
            }
            }
        else if (type_ == "lastPlayedDate")
        {
            std::string dateStr;
            if (selectedItem->getInfo("lastPlayedDate", dateStr) && !dateStr.empty()) {
                try {
                    std::stringstream ss(dateStr);
                    int day, year;
                    std::string monthStr;
                    char slash;
                    ss >> day >> slash >> monthStr >> slash >> year;
                    struct tm tstruct = {};
                    tstruct.tm_mday = day;
                    tstruct.tm_year = year - 1900;
                    static const std::string months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
                    bool validMonth = false;
                    for (int i = 0; i < 12; ++i) {
                        if (monthStr == months[i]) {
                            tstruct.tm_mon = i;
                            validMonth = true;
                            break;
                        }
                    }
                    if (!validMonth) {
                        Logger::write(Logger::ZONE_WARNING, "ReloadableText", "Invalid month in lastPlayedDate: " + dateStr + " for " + selectedItem->name);
                        text = "";
                    }
                    else {
                        char buf[80];
                        strftime(buf, sizeof(buf), timeFormat_.c_str(), &tstruct);
                        text = buf;
                        Logger::write(Logger::ZONE_DEBUG, "ReloadableText", "lastPlayedDate=" + std::string(buf) + " for " + selectedItem->name);
                    }
                }
                catch (const std::exception& e) {
                    Logger::write(Logger::ZONE_WARNING, "ReloadableText", "Invalid lastPlayedDate format: " + dateStr + " for " + selectedItem->name + ", error: " + e.what());
                    text = "";
                }
            }
            else {
                Logger::write(Logger::ZONE_DEBUG, "ReloadableText", "lastPlayedDate not found or empty for " + selectedItem->name);
                text = "";
            }
            }
        else if (type_ == "lastPlayDuration")
        {
            std::string minutesStr;
            if (selectedItem->getInfo("lastPlayDuration", minutesStr) && !minutesStr.empty()) {
                try {
                    int totalMinutes = std::stoi(minutesStr);
                    struct tm tstruct = {};
                    tstruct.tm_hour = totalMinutes / 60;
                    tstruct.tm_min = totalMinutes % 60;
                    char buf[80];
                    strftime(buf, sizeof(buf), timeFormat_.c_str(), &tstruct);
                    text = buf;
                    Logger::write(Logger::ZONE_DEBUG, "ReloadableText", "lastPlayDuration=" + std::string(buf) + " for " + selectedItem->name);
                }
                catch (const std::exception& e) {
                    Logger::write(Logger::ZONE_WARNING, "ReloadableText", "Invalid lastPlayDuration value: " + minutesStr + " for " + selectedItem->name + ", error: " + e.what());
                    text = "";
                }
            }
            else {
                Logger::write(Logger::ZONE_DEBUG, "ReloadableText", "lastPlayDuration not found or empty for " + selectedItem->name);
                text = "";
            }
            }
         
        if (!selectedItem->leaf || systemMode_) // item is not a leaf
        {
            (void)config_.getProperty("collections." + selectedItem->name + "." + type_, text );
        }

        if (systemMode_) // Get the system information in stead
        {
            text = "";
            (void)config_.getProperty("collections." + page.getCollectionName() + "." + type_, text );
        }

        bool overwriteXML = false;
        config_.getProperty( "overwriteXML", overwriteXML );
        if ( text == "" || overwriteXML ) // No text was found yet; check the info in stead
        {
            std::string text_tmp;
            selectedItem->getInfo( type_, text_tmp );
            if ( text_tmp != "" )
            {
                text = text_tmp;
            }
        }

        if (text == "0")
        {
            text = singlePrefix_ + text + pluralPostfix_;
        }
        else if (text == "1")
        {
            text = singlePrefix_ + text + singlePostfix_;
        }
        else if (text != "")
        {
            text = pluralPrefix_ + text + pluralPostfix_;
        }

        if (text != "")
        {
            if (textFormat_ == "uppercase")
            {
                std::transform(text.begin(), text.end(), text.begin(), ::toupper);
            }
            if (textFormat_ == "lowercase")
            {
                std::transform(text.begin(), text.end(), text.begin(), ::tolower);
            }
            ss << text;
        }

        imageInst_ = new Text(ss.str(), page, fontInst_, baseViewInfo.Monitor);
    }
}


void ReloadableText::draw()
{
    if(imageInst_)
    {
        imageInst_->baseViewInfo = baseViewInfo;
        imageInst_->draw();
    }
}




void ReloadableText::saveCollectionSizeToFile(size_t size)
{

    std::string layoutName;
    config_.getProperty("layout", layoutName);
    if (layoutName.empty())
    {

        Logger::write(Logger::ZONE_WARNING, "ReloadableText", "No layout specified, using 'default'");
        return;
    }

    // Get filename from config (e.g., "sizeFileName")
    std::string fileName;
    config_.getProperty("sizeFileName", fileName);
    if (fileName.empty())
    {
        Logger::write(Logger::ZONE_INFO, "ReloadableText", "No 'sizeFileName' specified in config, skipping file save");
        return; // Do nothing if filename isn’t specified
    }

    CollectionInfo* collection = page.getCollection();
    if (!collection) {
        Logger::write(Logger::ZONE_ERROR, "ReloadableText", "No collection found");
        return;
    }
    std::string collectionName = collection->name;


#if defined(WIN32) || defined(_WIN64)
    const char separator = '\\';
#else
    const char separator = '/';
#endif
    // Hardcode here can be bettered
    // Construct the absolute file path directly using Configuration::absolutePath
    std::string filePath = Configuration::absolutePath;
    if (!filePath.empty() && filePath.back() != separator)
        filePath += separator;
    filePath += "layouts";
    filePath += separator;
    filePath += layoutName;
    filePath += separator;
    filePath += "collections";
    filePath += separator;
    filePath += collectionName;
    filePath += separator;
    filePath += "system_artwork";
    filePath += separator;
    filePath += fileName;

    std::string directory = filePath.substr(0, filePath.rfind(separator));
    Utils::createDirectories(directory);

    // Write the size to the file
    std::ofstream outFile(filePath.c_str());
    if (outFile.is_open())
    {
        outFile << size;
        outFile.close();
    }
    else
    {
        Logger::write(Logger::ZONE_ERROR, "ReloadableText", "Failed to open " + filePath + " for writing");
    }
}

