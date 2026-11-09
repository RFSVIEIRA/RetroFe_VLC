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

static uint32_t decodeUTF8(const std::string& str, size_t& pos)
{
    if (pos >= str.size()) return 0xFFFD;
    unsigned char c = static_cast<unsigned char>(str[pos]);
    if (c <= 0x7F) {
        ++pos;
        return c;
    }
    else if ((c & 0xE0) == 0xC0) { // 2 bytes
        if (pos + 1 >= str.size()) { ++pos; return 0xFFFD; }
        uint32_t cp = ((c & 0x1F) << 6) | (static_cast<unsigned char>(str[pos + 1]) & 0x3F);
        pos += 2;
        return cp;
    }
    else if ((c & 0xF0) == 0xE0) { // 3 bytes
        if (pos + 2 >= str.size()) { pos += str.size() - pos; return 0xFFFD; }
        uint32_t cp = ((c & 0x0F) << 12) |
            ((static_cast<unsigned char>(str[pos + 1]) & 0x3F) << 6) |
            (static_cast<unsigned char>(str[pos + 2]) & 0x3F);
        pos += 3;
        return cp;
    }
    else if ((c & 0xF8) == 0xF0) { // 4 bytes
        if (pos + 3 >= str.size()) { pos += str.size() - pos; return 0xFFFD; }
        uint32_t cp = ((c & 0x07) << 18) |
            ((static_cast<unsigned char>(str[pos + 1]) & 0x3F) << 12) |
            ((static_cast<unsigned char>(str[pos + 2]) & 0x3F) << 6) |
            (static_cast<unsigned char>(str[pos + 3]) & 0x3F);
        pos += 4;
        return cp;
    }
    ++pos;
    return 0xFFFD;
}

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
        type_ == "time" ||
        type_ == "current" ||
        type_ == "duration" ||
        type_ == "isPaused" ||
        type_ == "isMuted" ||     
        type_ == "volumePercent" ||       
        type_ == "volumeBar" ||    
        type_ == "progressBar" ||  
        type_ == "currentBar" ||
        type_ == "currentPercent" ||
        type_ == "currentDuration" ||
        type_ == "isShuffle" ||
        type_ == "isRepeat" )

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

    Item* selectedItem = page.getSelectedItem();
    if (selectedItem == NULL)
        return;

    std::stringstream ss;
    std::string text = "";

        if (type_ == "time")
        {
            time_t now = time(0);
            struct tm tstruct = *localtime(&now);
            char buf[80];
            strftime(buf, sizeof(buf), timeFormat_.c_str(), &tstruct);
            text = buf;
        }
        else if (type_ == "numberButtons")
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
        else if (type_ == "name")
        {
            text = selectedItem->name;
            
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
        // === JUKEBOX FIELDS ===
        else if (type_ == "artist")
        {
            text = selectedItem->artist;
        }
        else if (type_ == "album")
        {
            text = selectedItem->album;
        }
        else if (type_ == "trackNumber" || type_ == "tracknumber" || type_ == "track")
        {
            text = selectedItem->trackNumber;
        }
        else if (type_ == "discNumber" || type_ == "discnumber" || type_ == "disc")
        {
            text = selectedItem->discNumber;
        }
        else if (type_.rfind( "playlist", 0 ) == 0)
        {
            text = playlistName;
        }
        else if (type_ == "firstLetter")
        {
            {
                size_t pos = 0;
                uint32_t cp = decodeUTF8(selectedItem->fullTitle, pos);
                if (cp != 0xFFFD && cp != 0)
                {
                    // Convert codepoint back to UTF-8 string
                    if (cp <= 0x7F)
                        text = std::string(1, static_cast<char>(cp));
                    else if (cp <= 0x7FF)
                        text = { char(0xC0 | (cp >> 6)), char(0x80 | (cp & 0x3F)) };
                    else if (cp <= 0xFFFF)
                        text = { char(0xE0 | (cp >> 12)), char(0x80 | ((cp >> 6) & 0x3F)),
                                char(0x80 | (cp & 0x3F)) };
                    else
                        text = { char(0xF0 | (cp >> 18)), char(0x80 | ((cp >> 12) & 0x3F)),
                                char(0x80 | ((cp >> 6) & 0x3F)), char(0x80 | (cp & 0x3F)) };
                }
                else
                {
                    text = selectedItem->fullTitle.empty() ? "" : selectedItem->fullTitle.substr(0, 1);
                }
            }
        }
        else if (type_ == "collectionName")
        {
            if (page.getCollectionName() == "SearchResults" && selectedItem && selectedItem->collectionInfo)
            {
                text = selectedItem->collectionInfo->name;
            }
            else
            {
                text = page.getCollectionName();
            }
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
            std::string currentPlaylist = page.getPlaylistName();

            // Skip favorite badge when inside the favorites playlist
            if (currentPlaylist == "favorites")
            {
                text = "";   // or "no" if you prefer
            }
            else
            {
                text = selectedItem->isFavorite ? "yes" : "no";
            }
            }
        else if (type_.rfind("isQueued", 0) == 0 || type_.rfind("isqueued", 0) == 0)
        {
            std::string numStr = type_.substr(8);
            int queueNum = 0;
            try {
                queueNum = std::stoi(numStr);
            }
            catch (...) {
                queueNum = 0;
            }

            if (queueNum < 1 || queueNum > 32)
            {
                text = "no";
            }
            else
            {
                std::string padded = (queueNum < 10 ? "0" : "");
                std::string qname = "queued" + padded + std::to_string(queueNum);

                std::string currentPlaylist = page.getPlaylistName();

                // === SKIP if we are inside ANY queued playlist ===
                if (currentPlaylist.rfind("queued", 0) == 0)
                {
                    text = "";   // Hide all isQueuedN badges while viewing any queue
                }
                else
                {
                    // Check if the song is in this specific queue
                    bool isInQueue = false;
                    CollectionInfo* coll = page.getCollection();
                    if (coll)
                    {
                        auto it = coll->playlists.find(qname);
                        if (it != coll->playlists.end() && it->second)
                        {
                            if (std::find(it->second->begin(), it->second->end(), selectedItem)
                                != it->second->end())
                            {
                                isInQueue = true;
                            }
                        }
                    }
                    text = isInQueue ? "yes" : "no";
                }
            }

#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "ReloadableText",
                "isQueued" + std::to_string(queueNum) + " = " + text);
#endif
                }
        else if (type_ == "isPaused")
        {
            if (page.isPaused( ))
                text = "Paused";
            else
                text = "Playing";
        }
        else if (type_ == "isMuted")
        {
            if (page.isMuted())
                text = "Muted";
            else
                text = "Unmuted";
        }
        else if (type_ == "isShuffle" || type_ == "isShuffled")
        {
            if (page.isShuffleEnabled())
                text = "Shuffle On";
            else
                text = "Shuffle Off";
		}
        else if (type_ == "isRepeat" || type_ == "isRepeated")
        {
            if (page.isRepeatLoopEnabled())
                text = "Repeat On";
            else
                text = "Repeat Off";
#ifdef _DEBUG
            Logger::write(Logger::ZONE_INFO, "ReloadableText",
                "Repeat " + text);
#endif
		}
        else if (type_ == "volumePercent" || type_ == "volumepercent")
        {
            float vol = page.getVolume();
            vol = std::clamp(vol, 0.0f, 1.0f);   // segurança

            int percent = static_cast<int>(vol * 100.0f + 0.5f);
            percent = (percent / 5) * 5;

            text = std::to_string(percent);
            }
        else if (type_ == "volumeBar")
        {
            float vol = page.getVolume();
            int barLength = 12;   // Change if you want longer/shorter bar
            int filled = static_cast<int>(vol * barLength + 0.5f);

            std::string bar = "[";
            for (int i = 0; i < barLength; ++i)
            {
                bar += (i < filled) ? "=" : "-";
            }
            bar += "] ";
            text = bar;
            }
        else if (type_ == "currentBar")
        {
            unsigned long long current = page.getCurrent();
            unsigned long long duration = page.getDuration();
            std::string textTemp;

            if (duration == 0)
            {
                textTemp = "[--------------------] --:--/--:--";
            }
            else
            {
                const int barLength = 20;
                int filled = static_cast<int>((current * barLength) / duration);
                if (filled > barLength)
                    filled = barLength;

                std::string bar = "[";
                for (int i = 0; i < barLength; ++i)
                {
                    bar += (i < filled) ? '=' : '-';
                }
                bar += "] ";

                // Convert from nanoseconds to seconds
                current /= 1000000000ULL;
                duration /= 1000000000ULL;

                // Calculate components
                int curHours = current / 3600;
                int curMin = (current / 60) % 60;
                int curSec = current % 60;

                int durHours = duration / 3600;
                int durMin = (duration / 60) % 60;
                int durSec = duration % 60;

                char timeStr[32];

                if (durHours > 0)
                {
                    // Auto add hours when duration is 1 hour or more
                    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d/%02d:%02d:%02d",
                        curHours, curMin, curSec,
                        durHours, durMin, durSec);
                }
                else
                {
                    // Normal MM:SS format
                    snprintf(timeStr, sizeof(timeStr), "%02d:%02d/%02d:%02d",
                        curMin, curSec,
                        durMin, durSec);
                }

                textTemp = bar + timeStr;
            }

            text = textTemp;
            }
        else if (type_ == "currentPercent")
        {
            unsigned long long current = page.getCurrent();
            unsigned long long duration = page.getDuration();

            if (duration == 0)
            {
                text = "0";
            }
            else
            {
                float progress = static_cast<float>(current) / static_cast<float>(duration);

                int percent = static_cast<int>(progress * 100.0f + 0.5f);

                // round to nearest 5%
                percent = (percent / 5) * 5;

                if (percent > 100) percent = 100;

                text = std::to_string(percent);
            }
        }
        else if (type_ == "currentDuration")
        {
            unsigned long long current = page.getCurrent();
            unsigned long long duration = page.getDuration();
            std::string textTemp;
            if (duration == 0)
            {
                textTemp = "--:--/--:--";
            }
            else
            {
                current /= 1000000000ULL;
                duration /= 1000000000ULL;

                int curHours = current / 3600;
                int curMin = (current / 60) % 60;
                int curSec = current % 60;
                int durHours = duration / 3600;
                int durMin = (duration / 60) % 60;
                int durSec = duration % 60;

                char timeStr[32];
                if (durHours > 0)
                {
                    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d/%02d:%02d:%02d",
                        curHours, curMin, curSec,
                        durHours, durMin, durSec);
                }
                else
                {
                    snprintf(timeStr, sizeof(timeStr), "%02d:%02d/%02d:%02d",
                        curMin, curSec,
                        durMin, durSec);
                }
                textTemp = timeStr;
            }
            text = textTemp;          
        }
        else if (type_ == "current")
        {
            unsigned long long current = page.getCurrent();
            unsigned long long duration = page.getDuration();

            if (duration == 0)
            {
                text = "--:--";
            }
            else
            {
                current /= 1000000000ULL;

                int hours = current / 3600;
                int minutes = (current / 60) % 60;
                int seconds = current % 60;

                char timeStr[16];

                if (hours > 0)
                {
                    // Show hours if >= 1 hour
                    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", hours, minutes, seconds);
                }
                else
                {
                    // Show only MM:SS if under 1 hour
                    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", minutes, seconds);
                }

                text = timeStr;
            }
            }

        else if (type_ == "duration")
        {
            unsigned long long duration = page.getDuration();

            if (duration == 0)
            {
                text = "--:--";
            }
            else
            {
                duration /= 1000000000ULL;

                int hours = duration / 3600;
                int minutes = (duration / 60) % 60;
                int seconds = duration % 60;

                char timeStr[16];

                if (hours > 0)
                {
                    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", hours, minutes, seconds);
                }
                else
                {
                    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", minutes, seconds);
                }

                text = timeStr;
            }
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

                    int days = totalMinutes / 1440;      // 1440 minutes = 1 day
                    int hours = (totalMinutes % 1440) / 60;
                    int minutes = totalMinutes % 60;

                    char buf[128];

                    if (days > 0) {
                        // Use singular/plural correctly
                        if (days == 1)
                            snprintf(buf, sizeof(buf), "%d day %02d:%02d", days, hours, minutes);
                        else
                            snprintf(buf, sizeof(buf), "%d days %02d:%02d", days, hours, minutes);
                    }
                    else {
                        // Less than 1 day, just hours:minutes
                        snprintf(buf, sizeof(buf), "%02d:%02d", hours, minutes);
                    }

                    text = buf;
#ifdef _DEBUG
                    Logger::write(Logger::ZONE_DEBUG, "ReloadableText", "totalPlayTime=" + text + " for " + selectedItem->name);
#endif
                }
                catch (const std::exception& e) {
#ifdef _DEBUG
                    Logger::write(Logger::ZONE_WARNING, "ReloadableText", "Invalid totalPlayTime value: " + minutesStr + " for " + selectedItem->name + ", error: " + e.what());
#endif
                    text = "";
                }
            }
            else {
#ifdef _DEBUG
                Logger::write(Logger::ZONE_DEBUG, "ReloadableText", "totalPlayTime not found or empty for " + selectedItem->name);
#endif
                text = "";
            }
            }

        else if (type_ == "lastPlayedDate")
        {
            std::string dateStr;
            if (selectedItem->getInfo("lastPlayedDate", dateStr) && !dateStr.empty()) {
                try {
                    // Expect format: DD/Mon/YYYY
                    int day, year;
                    std::string monthStr;

                    size_t firstSlash = dateStr.find('/');
                    size_t secondSlash = dateStr.find('/', firstSlash + 1);
                    if (firstSlash == std::string::npos || secondSlash == std::string::npos) {
                        text = dateStr; // fallback if format is wrong
                    }
                    else {
                        day = std::stoi(dateStr.substr(0, firstSlash));
                        monthStr = dateStr.substr(firstSlash + 1, secondSlash - firstSlash - 1);
                        year = std::stoi(dateStr.substr(secondSlash + 1));

                        static const std::string months[] = { "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec" };
                        int month = -1;
                        for (int i = 0; i < 12; ++i)
                            if (monthStr == months[i]) { month = i; break; }

                        if (month < 0) {
                            text = dateStr; // fallback if month not recognized
                        }
                        else {
                            struct tm tstruct = {};
                            tstruct.tm_mday = day;
                            tstruct.tm_mon = month;
                            tstruct.tm_year = year - 1900;
                            tstruct.tm_hour = 0; tstruct.tm_min = 0; tstruct.tm_sec = 0;

                            time_t playedTime = mktime(&tstruct);
                            time_t now = time(nullptr);

                            double diffDays = difftime(now, playedTime) / (60 * 60 * 24);

                            if (diffDays < 1) {
                                text = "Today";
                            }
                            else if (diffDays < 2) {
                                text = "Yesterday";
                            }
                            else if (diffDays < 7) {
                                char buf[32];
                                strftime(buf, sizeof(buf), "%A", &tstruct); // weekday
                                text = buf;
                            }
                            else {
                                char buf[80];
                                // Use XML-provided timeFormat_ if available, otherwise fallback to "%d %b %Y"
                                const char* format = !timeFormat_.empty() ? timeFormat_.c_str() : "%d %b %Y";
                                strftime(buf, sizeof(buf), format, &tstruct);
                                text = buf;
                            }
                        }
                    }

                    Logger::write(Logger::ZONE_DEBUG, "ReloadableText", "lastPlayedDate=" + text + " for " + selectedItem->name);
                }
                catch (const std::exception& e) {
                    Logger::write(Logger::ZONE_WARNING, "ReloadableText", "Failed to parse lastPlayedDate: " + dateStr + ", error: " + e.what());
                    text = dateStr; // fallback
                }
            }
            else text = "";
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
         
        if (text.empty() && ss.str().empty())
        {
            if (!selectedItem->leaf || systemMode_)
                config_.getProperty("collections." + selectedItem->name + "." + type_, text);
            if (systemMode_)
            {
                std::string colName = (page.getCollectionName() == "SearchResults" && selectedItem && selectedItem->collectionInfo)
                                      ? selectedItem->collectionInfo->name
                                      : page.getCollectionName();
                config_.getProperty("collections." + colName + "." + type_, text);
            }
            // Fallback for overwriteXML or missing data
            bool overwriteXML = false;
            config_.getProperty("overwriteXML", overwriteXML);
            if (text.empty() || overwriteXML)
            {
                std::string text_tmp;
                if (selectedItem->getInfo(type_, text_tmp) && !text_tmp.empty())
                {
                    text = text_tmp;
#ifdef _DEBUG
                    Logger::write(Logger::ZONE_DEBUG, "ReloadableText",
                        "Using getInfo fallback for " + type_ + " = " + text +
                        " for " + selectedItem->name);
#endif
                }
            }
        }

            if (!text.empty())
            {
                if (text == "0")
                    text = singlePrefix_ + text + pluralPostfix_;
                else if (text == "1")
                    text = singlePrefix_ + text + singlePostfix_;
                else
                    text = pluralPrefix_ + text + pluralPostfix_;

                if (textFormat_ == "uppercase")
                    std::transform(text.begin(), text.end(), text.begin(), ::toupper);
                else if (textFormat_ == "lowercase")
                    std::transform(text.begin(), text.end(), text.begin(), ::tolower);

                ss << text;
            }

            imageInst_ = new Text(ss.str(), page, fontInst_, baseViewInfo.Monitor);
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
    if (collectionName == "SearchResults") return;


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
    std::ofstream outFile(filePath.c_str(), std::ios::binary | std::ios::trunc);
    if (outFile.is_open())
    {
        outFile << size;          // still no newline
        // or, if you ever want a newline on purpose:
        // outFile << size << '\n';
        outFile.close();
    }
    else
    {
        Logger::write(Logger::ZONE_ERROR, "ReloadableText", "Failed to open " + filePath + " for writing");
    }
}

void ReloadableText::refreshBadges()
{
    std::string tmp;
    Item* item = page.getSelectedItem();
    if (type_ == "0" || type_ == "isRA" || type_ == "isra" || (item && item->getInfo(type_, tmp)))
    {
        ReloadTexture();
    }
}

