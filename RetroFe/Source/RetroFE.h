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


#include "Collection/Item.h"
#include "Collection/CollectionInfoBuilder.h"
#include "Control/UserInput.h"
#include "Database/DB.h"
#include "Database/MetadataDatabase.h"
#include "Execute/AttractMode.h"
#include "Graphics/FontCache.h"
#include "Video/IVideo.h"
#include "Video/VideoFactory.h" 
#include <SDL.h>
#include <SDL_ttf.h>
#include <list>
#include <stack>
#include <map>
#include <string>
#include <iostream>
#include <cwchar>
#include <windows.h>
#include <TlHelp32.h>
#include <iostream>
#include <fstream>
#include"Video/LibVLCVideo.h"
#include "OptMenu/OptionsMenu.h"


class CollectionInfo;
class Configuration;
class Page;


class RetroFE
{

public:
    RetroFE( Configuration &c );
    virtual ~RetroFE( );
    bool     deInitialize( );
    bool     run( );
    void     freeGraphicsMemory( );
    void     allocateGraphicsMemory( );
    void     launchEnter( );
    void     launchExit( );
    DWORD GetProcId(const wchar_t* procName);
   // void requestReboot();

private:
    volatile bool initialized;
    volatile bool initializeError;
    SDL_Thread   *initializeThread;
    static int    initialize( void *context );
    DWORD procId;
    enum RETROFE_STATE
    {
        RETROFE_IDLE,
        RETROFE_LOAD_ART,
        RETROFE_ENTER,
        RETROFE_SPLASH_EXIT,
        RETROFE_PLAYLIST_REQUEST,
        RETROFE_PLAYLIST_EXIT,
        RETROFE_PLAYLIST_LOAD_ART,
        RETROFE_PLAYLIST_ENTER,
        RETROFE_MENUJUMP_REQUEST,
        RETROFE_MENUJUMP_EXIT,
        RETROFE_MENUJUMP_LOAD_ART,
        RETROFE_MENUJUMP_ENTER,
        RETROFE_HIGHLIGHT_REQUEST,
        RETROFE_HIGHLIGHT_EXIT,
        RETROFE_HIGHLIGHT_LOAD_ART,
        RETROFE_HIGHLIGHT_ENTER,
        RETROFE_NEXT_PAGE_REQUEST,
        RETROFE_NEXT_PAGE_MENU_EXIT,
        RETROFE_NEXT_PAGE_MENU_LOAD_ART,
        RETROFE_NEXT_PAGE_MENU_ENTER,
        RETROFE_COLLECTION_UP_REQUEST,
        RETROFE_COLLECTION_UP_EXIT,
        RETROFE_COLLECTION_UP_MENU_ENTER,
        RETROFE_COLLECTION_UP_ENTER,
        RETROFE_COLLECTION_UP_SCROLL,
        RETROFE_COLLECTION_HIGHLIGHT_REQUEST,
        RETROFE_COLLECTION_HIGHLIGHT_EXIT,
        RETROFE_COLLECTION_HIGHLIGHT_LOAD_ART,
        RETROFE_COLLECTION_HIGHLIGHT_ENTER,
        RETROFE_COLLECTION_DOWN_REQUEST,
        RETROFE_COLLECTION_DOWN_EXIT,
        RETROFE_COLLECTION_DOWN_MENU_ENTER,
        RETROFE_COLLECTION_DOWN_ENTER,
        RETROFE_COLLECTION_DOWN_SCROLL,
        RETROFE_HANDLE_MENUENTRY,
        RETROFE_LAUNCH_ENTER,
        RETROFE_LAUNCH_REQUEST,
        RETROFE_LAUNCH_EXIT,
        RETROFE_BACK_REQUEST,
        RETROFE_BACK_MENU_EXIT,
        RETROFE_BACK_MENU_LOAD_ART,
        RETROFE_BACK_MENU_ENTER,
        RETROFE_MENUMODE_START_REQUEST,
        RETROFE_MENUMODE_START_LOAD_ART,
        RETROFE_MENUMODE_START_ENTER,
        RETROFE_NEW,
        RETROFE_QUIT_REQUEST,
        RETROFE_QUIT,
        RETROFE_GAME_PAGE_ENTER,
        RETROFE_GAME_PAGE_EXIT,
        RETROFE_SEARCH_REQUEST,
        RETROFE_SEARCH_ENTER,
        RETROFE_SEARCH_EXIT,
		RETROFE_RELOAD_REQUEST,
        RETROFE_CONFIGMENU_REQUEST,  // New states for options menu
        RETROFE_CONFIGMENU_ENTER,
        RETROFE_CONFIGMENU_EXIT
  
    };

    void            render( );
    bool            back( bool &exit );
   // void            quit( );
    Page           *loadPage( );
    Page           *loadSplashPage( );
    RETROFE_STATE   processUserInput( Page *page );
   // void            update( float dt, bool scrollActive );
    CollectionInfo *getCollection( std::string collectionName );
    CollectionInfo *getMenuCollection( std::string collectionName );
	void            saveRetroFEState( );

    // dynamic layout updating
    void            checkAndReloadLayout();
    void            reloadLayout();
    // for the Grid
    void            resetInput();

    Configuration     &config_;
    DB                *db_;
    MetadataDatabase  *metadb_;
    UserInput          input_;
    Page              *currentPage_;
    std::stack<Page *> pages_;
    float              keyInputDisable_;
    float              currentTime_;
    float              lastLaunchReturnTime_;
    float              keyLastTime_;
    float              keyDelayTime_;
    Item              *nextPageItem_;
    FontCache          fontcache_;
    AttractMode        attract_;
    bool               menuMode_;
    bool               attractMode_;
	int                attractModePlaylistCollectionNumber_;
	bool               reboot_;
    std::string        firstPlaylist_;

    std::map<std::string, unsigned int> lastMenuOffsets_;
    std::map<std::string, std::string>  lastMenuPlaylists_;

    //  for dynamic layout updating
    std::string        layoutFilePath_;       // Path to the layout XML file
    time_t             lastLayoutModTime_;    // Last modification time of the layout file
    float              layoutCheckTimer_;     // Timer for periodic checks
    float              layoutCheckInterval_;  // Interval between checks (e.g., 1 second)
    bool               enterCollectionAfterReload_;
    int                enterNumberOfTimes_;
    bool               isInCriticalState_;

    bool            isLayoutActive(const std::string& content, const std::string& layoutFile);
    bool            checkGameLayoutExists(const std::string& layoutName, const std::string& collectionName, const std::string& gameName);
    CollectionInfo* createTempCollection(Item* gameItem, CollectionInfo* originalCollection);
    Page* buildGamePage(CollectionInfo* tempCollection, const std::string& layoutName, const std::string& collectionName, const std::string& gameName);

    float              launchStartTime_;
    CollectionInfoBuilder* cib_;
    time_t preSearchMaxModTime_;

    std::vector<std::string> startupEnterQueue_;
    std::map<std::string, std::string> collectionParents_;
    std::string startupSelectItem_;
    void buildMenuTree();
    bool navigateToCollection(const std::string& targetCollection, bool autoEnter);

};

