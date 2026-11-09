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
#include "Graphics/SearchOverlay.h"
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
#include <vector>
#include <TlHelp32.h>
#include <iostream>
#include <fstream>
#include"Video/LibVLCVideo.h"
#include "OptMenu/OptionsMenu.h"


class CollectionInfo;
class Configuration;
#include "Graphics/Page.h"


class RetroFE
{

public:
    RetroFE( Configuration &c );

   

    virtual ~RetroFE( );
    bool     deInitialize( );
    bool     run( );
    void     freeGraphicsMemory(bool freeAll = false );
    void     allocateGraphicsMemory(bool allocateSecundary = false );
    void     launchEnter( );
    void     launchExit(bool allocateGraphics = true);
    DWORD GetProcId(const wchar_t* procName);
   // void requestReboot();
    void updateSecondaryScreens(float dt);
    void renderSecondaryScreens();

    // MaxQueued configurable
    static const int DefaultMaxQueued = 32;
private:
    std::string getCorrectCollectionName(Item* item, CollectionInfo* currentCollection) const;
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
        RETROFE_CONFIGMENU_EXIT,
        RETROFE_NEXT_SORT_REQUEST,
        RETROFE_NEXT_SORT_EXIT,
  
    };

    void            render( );
    bool            back( bool &exit );
   // void            quit( );
    Page           *loadPage( );
    Page           *loadSplashPage( );
    RETROFE_STATE   processUserInput( Page *page );
    RETROFE_STATE   runSearch( Page *page );
    RETROFE_STATE   runOptionsMenu( Page *page );
    void            buildSearchEntries( Page *page, std::vector<SearchMatcher::Entry> &entries );
    CollectionInfo *getSearchCollection( std::string collectionName );
    void            searchProgress( const std::string &text );
    void            saveSearchCollectionSubs( );
   // void            update( float dt, bool scrollActive );
    CollectionInfo *getCollection( std::string collectionName );
    CollectionInfo *getMenuCollection( std::string collectionName );
	void            saveRetroFEState( );

    // dynamic layout updating
    void            checkAndReloadLayout();
    void            reloadLayout();
    void            renderLoadingScreen();
    void            drawLoadingCircle(SDL_Renderer* renderer, int cx, int cy, int radius, SDL_Color color);
    SDL_Color       parseHexColor(const std::string& hex, SDL_Color defaultColor);
    SDL_Color       hsvToRgb(float h, float s, float v, Uint8 alpha = 255);
    bool EnableShutdownPrivilege();
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
    SearchOverlay      search_;
    OptionsMenu       *activeOptionsMenu_ = nullptr;
    bool               suppressNextBack_ = false;
    std::vector<std::string> searchGroups_;
    std::map<std::string, CollectionInfo *> searchCollections_;
    CollectionInfo    *searchPage_;
    Item              *searchTargetItem_;
    Item              *searchPendingItem_;
    Item              *searchLaunchItem_;
    int                searchFadeAlpha_;
    int                searchJumpIndex_;
    unsigned int       preSearchMenuDepth_;
    CollectionInfo    *searchPageCollection_;
    bool               pendingSearchAfterGameExit_;
    bool               searchIndexBuilt_;
    std::vector<SearchMatcher::Entry> cachedSearchEntries_;
    std::vector<std::string>          cachedSearchGroups_;
    bool               menuMode_;
    bool               attractMode_;
	int                attractModePlaylistCollectionNumber_;
	bool               reboot_;
    std::string        firstPlaylist_;
	bool                pendingBackToMain_;
    bool                jumpNextBack_;
    bool                isJukebox_;

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
    bool               isReload_;
    bool               isReloading_;
    TTF_Font          *loadingFont_;
    int                loadingFontSize_;
    SDL_Texture       *loadingTextTexture_;
    int                loadingTextW_;
    int                loadingTextH_;
    std::string        cachedLoadingText_;
    SDL_Color          cachedLoadingTextColor_;

    struct LoadingImageCache {
        SDL_Surface* surface = nullptr;
        std::map<int, SDL_Texture*> textures;
    };
    struct LoadingFontCache {
        TTF_Font* font = nullptr;
        int fontSize = 0;
    };

    Page::LoadingConfig                      loadingConfig_;
    std::map<std::string, LoadingImageCache> loadingImageCache_;
    std::map<std::string, LoadingFontCache>  loadingFontCache_;
    std::map<std::string, std::map<int, SDL_Texture*>> loadingTextTextureCache_;

    void updateLoadingConfig(const Page::LoadingConfig& config);
    void freeLoadingResources();
    bool               isAutoNavigating_;
    std::vector<bool>  marqueePauseScreen_;
    std::vector<HANDLE> screenDuplicatorHandles;

    bool            isLayoutActive(const std::string& content, const std::string& layoutFile);
    bool            checkGameLayoutExists(const std::string& layoutName, const std::string& collectionName, const std::string& gameName);
    bool            collectionExists(const std::string& name, bool isMenuMode);
    CollectionInfo* createTempCollection(Item* gameItem, CollectionInfo* originalCollection);
    Page* buildGamePage(CollectionInfo* tempCollection, const std::string& layoutName, const std::string& collectionName, const std::string& gameName);

    float              launchStartTime_;
    CollectionInfoBuilder* cib_;
    time_t preSearchMaxModTime_;

    std::vector<std::string> startupEnterQueue_;
    std::map<std::string, std::vector<std::string>> collectionParents_;
    std::string startupSelectItem_;
    void buildMenuTree();
    bool navigateToCollection(const std::string& targetCollection, bool autoEnter, const std::string& savedSelectedItem = "", const std::string& savedPlaylist = "", unsigned int savedScrollOffset = 0);
    void addToRaCollection(const std::string& system, const std::string& game);
    std::set<std::pair<std::string, std::string>> addedToRaCache_;
    bool loggedRaCollectionSkip_ = false;
    std::string reloadSavedItem_;
    std::string reloadSavedPlaylist_;
    unsigned int reloadSavedScroll_;

    Uint32 lastRetroRaCallTime_ = 0;
    std::string lastRetroRaGame_ = "";
    std::string lastRetroRaSystem_ = "";
    Uint32      lastRetroRaSelectionTime_ = 0;
    bool        lastRetroRaTriggered_ = false;
    bool   pendingRetroRaReload_ = false;


    // Queued playlist helpers
    bool isQueuedKey(UserInput::KeyCode_E base, int maxQueued = DefaultMaxQueued);
    int  getQueuedIndex(UserInput::KeyCode_E base, int maxQueued = DefaultMaxQueued);

    bool sortDirectionUp_ = true;

};
