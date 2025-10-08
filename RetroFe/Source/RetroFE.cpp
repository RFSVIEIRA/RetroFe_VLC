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


#include "RetroFE.h"
#include "Collection/CollectionInfoBuilder.h"
#include "Collection/CollectionInfo.h"
#include "Database/Configuration.h"
#include "Database/MetadataDatabase.h"
#include "Collection/Item.h"
#include "Execute/Launcher.h"
#include "Menu/Menu.h"
#include "Utility/Log.h"
#include "Utility/Utils.h"
#include "Collection/MenuParser.h"
#include "SDL.h"
#include "Control/UserInput.h"
#include "Graphics/PageBuilder.h"
#include "Graphics/Page.h"
#include "Graphics/Component/ScrollingList.h"
#include "Graphics/Component/Video.h"
#include <vlc/vlc.h>
#include "Video/VideoFactory.h"
#include <algorithm>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>
#include <SDL_ttf.h>

#if defined(__linux) || defined(__APPLE__)
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <cstring>
#endif

#ifdef WIN32
#include <Windows.h>
#include <SDL_syswm.h>
#include <SDL_thread.h>
#include <iostream>
#endif
#include <numeric>


RetroFE::RetroFE(Configuration& c)
    : initialized(false)
    , initializeError(false)
    , initializeThread(NULL)
    , config_(c)
    , db_(NULL)
    , metadb_(NULL)
    , input_(config_)
    , currentPage_(NULL)
    , keyInputDisable_(0)
    , currentTime_(0)
    , lastLaunchReturnTime_(0)
    , keyLastTime_(0)
    , keyDelayTime_(.3f)
    , reboot_(false)
    , layoutFilePath_()  
    , lastLayoutModTime_(0)
    , layoutCheckTimer_(0.0f)
    , layoutCheckInterval_(1.0f) // Check every 1 second
    , enterCollectionAfterReload_(false)
	, enterNumberOfTimes_(0)
    , isInCriticalState_(false)
    , launchStartTime_(0.0f)
    , preSearchMaxModTime_(0)
    , cib_(NULL)
{
    menuMode_ = false;
    attractMode_ = false;
    attractModePlaylistCollectionNumber_ = 0;
    firstPlaylist_ = "all";

	// dynamic layout updating
    std::string layoutName;
    config_.getProperty("layout", layoutName);
    layoutFilePath_ = Utils::combinePath(Configuration::absolutePath, "layouts", layoutName, "layout.xml");
}


RetroFE::~RetroFE()
{
    deInitialize();
}


// Render the current page to the screen
void RetroFE::render()
{

    SDL_LockMutex(SDL::getMutex());
    for (int i = 0; i < SDL::getNumDisplays(); ++i)
    {
        SDL_SetRenderDrawColor(SDL::getRenderer(i), 0x0, 0x0, 0x00, 0xFF);
        SDL_RenderClear(SDL::getRenderer(i));
    }

    if (currentPage_)
    {
        currentPage_->draw();
    }

    for (int i = 0; i < SDL::getNumDisplays(); ++i)
    {
        SDL_RenderPresent(SDL::getRenderer(i));
    }
    SDL_UnlockMutex(SDL::getMutex());

}

void RetroFE::checkAndReloadLayout()
{
    if (isInCriticalState_) return;
    time_t currentModTime = Utils::getFileModificationTime(layoutFilePath_);
    if (currentModTime == 0) {
        Logger::write(Logger::ZONE_ERROR, "RetroFE", "Failed to get modification time for layout file: " + layoutFilePath_);
        return;
    }

    if (currentModTime > lastLayoutModTime_) {
        Logger::write(Logger::ZONE_INFO, "RetroFE", "Layout file changed, reloading: " + layoutFilePath_);
        reloadLayout();
        lastLayoutModTime_ = currentModTime;
    }
}

// Reload the layout by rebuilding the current page
void RetroFE::reloadLayout()
{
    if (!currentPage_) return;

    // Step 1: Save current state before freeing the page
    std::string currentCol = currentPage_->getCollectionName();
    lastMenuOffsets_[currentCol] = currentPage_->getScrollOffsetIndex();
    lastMenuPlaylists_[currentCol] = currentPage_->getPlaylistName();
    Logger::write(Logger::ZONE_INFO, "RetroFE", "Saved state for collection: " + currentCol);

    // Step 2: Free the current page
    Logger::write(Logger::ZONE_INFO, "RetroFE", "Freeing current page...");
    currentPage_->freeGraphicsMemory();
    currentPage_->deInitialize();
    delete currentPage_;
    currentPage_ = nullptr;

    // Step 3: Reload the layout
    Logger::write(Logger::ZONE_INFO, "RetroFE", "Reloading layout...");
    currentPage_ = loadPage();
    if (!currentPage_) {
        Logger::write(Logger::ZONE_ERROR, "RetroFE", "Failed to reload layout!");
        return;
    }

    // Step 4: Navigate back to the previous collection 
    bool rememberMenu = false;
    config_.getProperty("rememberMenu", rememberMenu);
    Logger::write(Logger::ZONE_INFO, "RetroFE", "Navigating back to collection: " + currentCol + " (rememberMenu: " + std::to_string(rememberMenu) + ")");
    navigateToCollection(currentCol, true);  // autoEnter=true to fully enter the collection path

     // Step 5: Refresh input system
    SDL_PumpEvents();
    input_.resetStates();

    Logger::write(Logger::ZONE_INFO, "RetroFE", "Layout reloaded successfully!");
}

// Initialize the configuration and database
int RetroFE::initialize(void* context)
{


    RetroFE* instance = static_cast<RetroFE*>(context);
    Logger::write(Logger::ZONE_INFO, "RetroFE", "Initializing");

    if (!instance->input_.initialize())
    {
        Logger::write(Logger::ZONE_ERROR, "RetroFE", "Could not initialize user controls");
        instance->initializeError = true;
        return -1;
    }

    instance->db_ = new DB(Utils::combinePath(Configuration::absolutePath, "meta.db"));

    if (!instance->db_->initialize())
    {
        Logger::write(Logger::ZONE_ERROR, "RetroFE", "Could not initialize database");
        instance->initializeError = true;
        return -1;
    }

    instance->metadb_ = new MetadataDatabase(*(instance->db_), instance->config_);

    if (!instance->metadb_->initialize())
    {
        Logger::write(Logger::ZONE_ERROR, "RetroFE", "Could not initialize meta database");
        instance->initializeError = true;
        return -1;
    }
    instance->cib_ = new CollectionInfoBuilder(instance->config_, *(instance->metadb_)); // Initialize cib_
    instance->initialized = true;
    return 0;

}


// Launch a game/program
void RetroFE::launchEnter()
{

    // Record launch start time
    launchStartTime_ = static_cast<float>(SDL_GetTicks()) / 1000.0f;
    // Disable window focus
    SDL_SetWindowGrab(SDL::getWindow(0), SDL_FALSE);

    // Free the textures, and optionally take down SDL
    freeGraphicsMemory();

    bool hideMouse = false;
    int  mouseX = 5000;
    int  mouseY = 5000;
    config_.getProperty("hideMouse", hideMouse);
    config_.getProperty("mouseX", mouseX);
    config_.getProperty("mouseY", mouseY);
    if (hideMouse)
        SDL_WarpMouseGlobal(mouseX, mouseY);
}


// Return from the launch of a game/program
void RetroFE::launchExit()
{
    // Calculate play duration
    float launchEndTime = static_cast<float>(SDL_GetTicks()) / 1000.0f;
    float playDurationSeconds = launchEndTime - launchStartTime_;
    int playDurationMinutes = static_cast<int>(playDurationSeconds / 60.0f); // Convert to minutes
    Logger::write(Logger::ZONE_INFO, "RetroFE", "Game session duration: " + std::to_string(playDurationMinutes) + " minutes");

    // Optionally set up SDL, and load the textures
    allocateGraphicsMemory();

    // Restore the SDL settings
    SDL_RestoreWindow(SDL::getWindow(0));
    SDL_RaiseWindow(SDL::getWindow(0));
    SDL_SetWindowGrab(SDL::getWindow(0), SDL_TRUE);


    // Empty event queue, but handle joystick add/remove events
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_JOYDEVICEADDED || e.type == SDL_JOYDEVICEREMOVED)
        {
            input_.update(e);
        }
    }
    input_.resetStates();
    attract_.reset();

    // Restore time settings
    currentTime_ = static_cast<float>(SDL_GetTicks()) / 1000;
    keyLastTime_ = currentTime_;
    lastLaunchReturnTime_ = currentTime_;

    bool hideMouse = false;
    int  mouseX = 5000;
    int  mouseY = 5000;
    config_.getProperty("hideMouse", hideMouse);
    config_.getProperty("mouseX", mouseX);
    config_.getProperty("mouseY", mouseY);
    if (hideMouse)
        SDL_WarpMouseGlobal(mouseX, mouseY);

}


// Free the textures, and optionall take down SDL
void RetroFE::freeGraphicsMemory()
{

    // Free textures
    if (currentPage_)
    {
        currentPage_->freeGraphicsMemory();
    }

    // Close down SDL
    bool unloadSDL = false;
    config_.getProperty("unloadSDL", unloadSDL);
    if (unloadSDL)
    {
        currentPage_->deInitializeFonts();
        SDL::deInitialize();
        input_.clearJoysticks();
    }

}


// Optionally set up SDL, and load the textures
void RetroFE::allocateGraphicsMemory()
{

    // Reopen SDL
    bool unloadSDL = false;
    config_.getProperty("unloadSDL", unloadSDL);
    if (unloadSDL)
    {
        SDL::initialize(config_);
        currentPage_->initializeFonts();
    }

    // Allocate textures
    if (currentPage_)
    {
        currentPage_->allocateGraphicsMemory();
    }

}


// Deinitialize RetroFE
bool RetroFE::deInitialize()
{

    bool retVal = true;

    // Free textures
    Logger::write(Logger::ZONE_INFO, "RetroFE", "Freeing graphics memory");
    freeGraphicsMemory();


    // Delete page
    if (currentPage_)
    {
        Logger::write(Logger::ZONE_INFO, "RetroFE", "Deinitializing current page");
        currentPage_->deInitialize();
        delete currentPage_;
        currentPage_ = NULL;
    }
    
    // Delete databases and CollectionInfoBuilder
    if (cib_)
    {
        delete cib_;
        cib_ = NULL;
    }

    // Delete databases
    if (metadb_)
    {
        delete metadb_;
        metadb_ = NULL;
    }

    if (db_)
    {
        delete db_;
        db_ = NULL;
    }

    initialized = false;

    if (reboot_)
    {
        Logger::write(Logger::ZONE_INFO, "RetroFE", "Rebooting");
    }
    else
    {
        Logger::write(Logger::ZONE_INFO, "RetroFE", "Exiting");
        SDL::deInitialize();
        // Replace gst_deinit()
    }

    return retVal;
}


// Run RetroFE
bool RetroFE::run()
{
    static bool sdlInitialized = false;
    // Initialize SDL
    if (!sdlInitialized)
    {
        if (!SDL::initialize(config_))
        {
            Logger::write(Logger::ZONE_ERROR, "RetroFE", "Failed to initialize SDL");
            return false;
        }
        sdlInitialized = true;
    }
    fontcache_.initialize();

    // Define control configuration
    std::string controlsConfPath = Utils::combinePath(Configuration::absolutePath, "controls.conf");
    if (!config_.import("controls", controlsConfPath))
    {
        Logger::write(Logger::ZONE_ERROR, "RetroFE", "Could not import \"" + controlsConfPath + "\"");
        return false;
    }

    float preloadTime = 0;

    // Initialize video
    bool videoEnable = true;
    int  videoLoop = 0;
    config_.getProperty("videoEnable", videoEnable);
    config_.getProperty("videoLoop", videoLoop);
    VideoFactory::setEnabled(videoEnable);
    VideoFactory::setNumLoops(videoLoop);
    Video::setEnabled(videoEnable);

    initializeThread = SDL_CreateThread(initialize, "RetroFEInit", (void*)this);

    if (!initializeThread)
    {
        Logger::write(Logger::ZONE_INFO, "RetroFE", "Could not initialize RetroFE");
        return false;
    }


    // Preload layouts 
    std::string layoutName;
    config_.getProperty("layout", layoutName);
    PageBuilder pb(layoutName, "layout", config_, &fontcache_);
    pb.preloadLayouts(); // Preload base and first collection layouts

    // Initialize layout monitoring
    lastLayoutModTime_ = Utils::getFileModificationTime(layoutFilePath_);
    if (lastLayoutModTime_ == 0) {
        Logger::write(Logger::ZONE_ERROR, "RetroFE", "Failed to get initial modification time for layout file: " + layoutFilePath_);
        
    }


    int attractModeTime = 0;
    int attractModeNextTime = 0;
    int attractModePlaylistTime = 0;
    int attractModeCollectionTime = 0;
    int attractModeMinTime = 1000;
    int attractModeMaxTime = 5000;
    std::string firstCollection = "Main";
    bool running = true;
    RETROFE_STATE state = RETROFE_NEW;

    config_.getProperty("attractModeTime", attractModeTime);
    config_.getProperty("attractModeNextTime", attractModeNextTime);
    config_.getProperty("attractModePlaylistTime", attractModePlaylistTime);
    config_.getProperty("attractModeCollectionTime", attractModeCollectionTime);
    config_.getProperty("attractModeMinTime", attractModeMinTime);
    config_.getProperty("attractModeMaxTime", attractModeMaxTime);
    config_.getProperty("firstCollection", firstCollection);

    attract_.idleTime = static_cast<float>(attractModeTime);
    attract_.idleNextTime = static_cast<float>(attractModeNextTime);
    attract_.idlePlaylistTime = static_cast<float>(attractModePlaylistTime);
    attract_.idleCollectionTime = static_cast<float>(attractModeCollectionTime);
    attract_.minTime = attractModeMinTime;
    attract_.maxTime = attractModeMaxTime;

    int fps = 60;
    int fpsIdle = 60;
    config_.getProperty("fps", fps);
    config_.getProperty("fpsIdle", fpsIdle);
    double fpsTime = 1000.0 / static_cast<double>(fps);
    double fpsIdleTime = 1000.0 / static_cast<double>(fpsIdle);

    int initializeStatus = 0;
    bool inputClear = false;

    // load the initial splash screen, unload it once it is complete
    currentPage_ = loadSplashPage();
    state = RETROFE_ENTER;
    bool splashMode = true;
    bool exitSplashMode = false;

    Launcher l(config_);
    Menu     m(config_, input_);
    preloadTime = static_cast<float>(SDL_GetTicks()) / 1000;
    l.LEDBlinky(1);

    while (running)
    {

        float lastTime = 0;
        float deltaTime = 0;

        // Exit splash mode when an active key is pressed
        SDL_Event e;
        if (splashMode && SDL_PollEvent(&e))
        {
            if (input_.update(e) && input_.keystate(UserInput::KeyCodeSelect))
            {
                exitSplashMode = true;
                while (SDL_PollEvent(&e))
                {
                    if (e.type == SDL_JOYDEVICEADDED || e.type == SDL_JOYDEVICEREMOVED)
                    {
                        input_.update(e);
                    }
                }
                input_.resetStates();
                attract_.reset();
            }
        }

        if (!currentPage_)
        {
            Logger::write(Logger::ZONE_WARNING, "RetroFE", "Could not load page");
            running = false;
            break;
        }

        switch (state)
        {

            // Idle state; waiting for input
        case RETROFE_IDLE:
            currentPage_->cleanup();

            // Handle end of splash mode first
            if ((initialized || initializeError) && splashMode && (exitSplashMode || (currentPage_->getMinShowTime() <= (currentTime_ - preloadTime) && !(currentPage_->isPlaying()))))
            {
                SDL_WaitThread(initializeThread, &initializeStatus);

                if (initializeError)
                {
                    state = RETROFE_QUIT_REQUEST;
                    break;
                }

                currentPage_->stop();
                state = RETROFE_SPLASH_EXIT;
                break;
            }

            // Process startup queue
            if (!splashMode && currentPage_->isIdle()) {
                // Handle item selection without entering
                if (!startupSelectItem_.empty()) {
                    Item* selectItem = nullptr;
                    std::vector<Item*>& collItems = currentPage_->getCollection()->items;
                    for (auto* item : collItems) {
                        if (item->name == startupSelectItem_ && !item->leaf) {
                            selectItem = item;
                            break;
                        }
                    }
                    if (selectItem) {
                        currentPage_->selectItem(selectItem);
                        currentPage_->onNewItemSelected();
                        currentPage_->allocateGraphicsMemory();
                        currentPage_->start();
                        Logger::write(Logger::ZONE_INFO, "RetroFE", "Selected collection: " + startupSelectItem_);
                    }
                    else {
                        Logger::write(Logger::ZONE_WARNING, "RetroFE", "Could not find item for collection: " + startupSelectItem_ + " in " + currentPage_->getCollectionName());
                    }
                    startupSelectItem_ = "";
                    state = RETROFE_IDLE;
                }
                // Handle collection entry
                else if (!startupEnterQueue_.empty()) {
                    std::string nextCol = startupEnterQueue_.front();
                    startupEnterQueue_.erase(startupEnterQueue_.begin());

                    Item* nextItem = nullptr;
                    std::vector<Item*>& collItems = currentPage_->getCollection()->items;
                    for (auto* item : collItems) {
                        if (item->name == nextCol && !item->leaf) {
                            nextItem = item;
                            break;
                        }
                    }

                    if (nextItem) {
                        currentPage_->selectItem(nextItem);
                        nextPageItem_ = nextItem;
                        enterNumberOfTimes_ = 1;
                        state = RETROFE_NEXT_PAGE_REQUEST;
                        Logger::write(Logger::ZONE_INFO, "RetroFE", "Startup: Entering collection " + nextCol);
                    }
                    else {
                        Logger::write(Logger::ZONE_WARNING, "RetroFE", "Startup: Could not find item for collection " + nextCol + " in " + currentPage_->getCollectionName());
                        state = RETROFE_IDLE;
                    }
                }
                // Handle reload entry
                else if (enterNumberOfTimes_ == 0 && enterCollectionAfterReload_) {
                    nextPageItem_ = currentPage_->getSelectedItem();
                    if (nextPageItem_ && !nextPageItem_->leaf) {
                        Logger::write(Logger::ZONE_INFO, "RetroFE", "Entering collection after reload");
                        state = RETROFE_NEXT_PAGE_REQUEST;
                        enterNumberOfTimes_ = 1;
                    }
                    enterCollectionAfterReload_ = false;
                }
                // Normal idle processing
                else if (lastLaunchReturnTime_ == 0 || (currentTime_ - lastLaunchReturnTime_ > 0.3)) {
                    if (currentPage_->isIdle()) {
                        state = processUserInput(currentPage_);
                    }
                    lastLaunchReturnTime_ = 0;
                }
            }

            break;

            // Load art on entering RetroFE
        case RETROFE_LOAD_ART:
            currentPage_->start();
            state = RETROFE_ENTER;
            break;

            // Wait for onEnter animation to finish
        case RETROFE_ENTER:
            if (currentPage_->isIdle())
            {
                bool startCollectionEnter = false;
                config_.getProperty("startCollectionEnter", startCollectionEnter);
                nextPageItem_ = currentPage_->getSelectedItem();
                if (!splashMode && startCollectionEnter && !nextPageItem_->leaf && startupEnterQueue_.empty() && startupSelectItem_.empty())
                {
                    state = RETROFE_NEXT_PAGE_REQUEST;
                }
                else
                {
                    state = RETROFE_IDLE;
                }
            }
            break;

            // Handle end of splash mode
        case RETROFE_SPLASH_EXIT:
            if (currentPage_->isIdle())
            {
                // Delete the splash screen
                Logger::write(Logger::ZONE_INFO, "RetroFE", "Deinitializing splash page");
                currentPage_->deInitialize();
                delete currentPage_;
                currentPage_ = nullptr;

                // Load new page
                Logger::write(Logger::ZONE_INFO, "RetroFE", "Loading initial page");
                currentPage_ = loadPage();
                splashMode = false;
                if (!currentPage_)
                {
                    Logger::write(Logger::ZONE_ERROR, "RetroFE", "Failed to load initial page");
                    state = RETROFE_QUIT_REQUEST;
                    break;
                }

                // Clear renderer to avoid black screen
                SDL_LockMutex(SDL::getMutex());
                for (int i = 0; i < SDL::getNumDisplays(); ++i)
                {
                    SDL_Renderer* renderer = SDL::getRenderer(i);
                    if (renderer) {
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Black background
                        SDL_RenderClear(renderer);
                        SDL_RenderPresent(renderer);
                        Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Cleared renderer for display " + std::to_string(i));
                    }
                }
                SDL_UnlockMutex(SDL::getMutex());

                std::string firstCollection = "Main";
                config_.getProperty("firstCollection", firstCollection);
                Logger::write(Logger::ZONE_INFO, "RetroFE", "First collection set to: " + firstCollection);

                bool startCollectionEnter = false;
                config_.getProperty("startCollectionEnter", startCollectionEnter);

                // Navigate to firstCollection
                if (!navigateToCollection(firstCollection, startCollectionEnter)) {
                    Logger::write(Logger::ZONE_ERROR, "RetroFE", "Failed to navigate to first collection: " + firstCollection);
                    state = RETROFE_QUIT_REQUEST;
                    break;
                }

                state = RETROFE_LOAD_ART;
            }
            break;

            // Switch playlist; start onHighlightExit animation
        case RETROFE_PLAYLIST_REQUEST:
            inputClear = false;
            config_.getProperty("playlistInputClear", inputClear);
            if (inputClear)
            {
                // Empty event queue
                SDL_Event e;
                while (SDL_PollEvent(&e))
                    input_.update(e);
                input_.resetStates();
            }
            currentPage_->playlistExit();
            currentPage_->setScrolling(Page::ScrollDirectionIdle);
            state = RETROFE_PLAYLIST_EXIT;
            break;

            // Switch playlist; wait for onHighlightExit animation to finish; load art
        case RETROFE_PLAYLIST_EXIT:
            if (currentPage_->isIdle())
            {
                currentPage_->onNewItemSelected();
                state = RETROFE_PLAYLIST_LOAD_ART;
            }
            break;

            // Switch playlist; start onHighlightEnter animation
        case RETROFE_PLAYLIST_LOAD_ART:
            if (currentPage_->isIdle())
            {
                currentPage_->reallocateMenuSpritePoints();
                currentPage_->playlistEnter();
                state = RETROFE_PLAYLIST_ENTER;
            }
            break;

            // Switch playlist; wait for onHighlightEnter animation to finish
        case RETROFE_PLAYLIST_ENTER:
            if (currentPage_->isIdle())
            {
                state = RETROFE_IDLE;
            }
            break;

            // Jump in menu; start onMenuJumpExit animation
        case RETROFE_MENUJUMP_REQUEST:
            inputClear = false;
            config_.getProperty("jumpInputClear", inputClear);
            if (inputClear)
            {
                // Empty event queue
                SDL_Event e;
                while (SDL_PollEvent(&e))
                    input_.update(e);
                input_.resetStates();
            }
            currentPage_->menuJumpExit();
            currentPage_->setScrolling(Page::ScrollDirectionIdle);
            state = RETROFE_MENUJUMP_EXIT;
            break;

            // Jump in menu; wait for onMenuJumpExit animation to finish; load art
        case RETROFE_MENUJUMP_EXIT:
            if (currentPage_->isIdle())
            {
                currentPage_->onNewItemSelected();
                state = RETROFE_MENUJUMP_LOAD_ART;
            }
            break;

            // Jump in menu; start onMenuJumpEnter animation
        case RETROFE_MENUJUMP_LOAD_ART:
            if (currentPage_->isIdle())
            {
                currentPage_->reallocateMenuSpritePoints();
                currentPage_->menuJumpEnter();
                state = RETROFE_MENUJUMP_ENTER;
            }
            break;

            // Jump in menu; wait for onMenuJump animation to finish
        case RETROFE_MENUJUMP_ENTER:
            if (currentPage_->isIdle())
            {
                state = RETROFE_IDLE;
            }
            break;

            // Start onHighlightExit animation
        case RETROFE_HIGHLIGHT_REQUEST:
            currentPage_->setScrolling(Page::ScrollDirectionIdle);
            currentPage_->highlightExit();
            state = RETROFE_HIGHLIGHT_EXIT;
            break;

            // Wait for onHighlightExit animation to finish; load art
        case RETROFE_HIGHLIGHT_EXIT:
            if (currentPage_->isIdle())
            {
                currentPage_->highlightLoadArt();
                state = RETROFE_HIGHLIGHT_LOAD_ART;
            }
            break;

            // Start onHighlightEnter animation
        case RETROFE_HIGHLIGHT_LOAD_ART:
            currentPage_->highlightEnter();
            if (currentPage_->getSelectedItem())
                l.LEDBlinky(9, currentPage_->getSelectedItem()->collectionInfo->name, currentPage_->getSelectedItem());
            state = RETROFE_HIGHLIGHT_ENTER;
            break;

            // Wait for onHighlightEnter animation to finish
        case RETROFE_HIGHLIGHT_ENTER:
            RETROFE_STATE state_tmp;
            if (currentPage_->isMenuIdle() &&
                ((state_tmp = processUserInput(currentPage_)) == RETROFE_HIGHLIGHT_REQUEST ||
                    state_tmp == RETROFE_MENUJUMP_REQUEST ||
                    state_tmp == RETROFE_PLAYLIST_REQUEST))
            {
                state = state_tmp;
            }
            else if (currentPage_->isIdle())
            {
                state = RETROFE_IDLE;
            }
            break;

            // Next page; start onMenuExit animation
        case RETROFE_NEXT_PAGE_REQUEST:
            currentPage_->exitMenu();
            state = RETROFE_NEXT_PAGE_MENU_EXIT;
            break;

            // Wait for onMenuExit animation to finish; load new page if applicable; load art
        case RETROFE_NEXT_PAGE_MENU_EXIT:
            isInCriticalState_ = true;
            if (currentPage_->isIdle())
            {
                if (currentPage_->getSelectedItem())
                    l.LEDBlinky(8, currentPage_->getSelectedItem()->name, currentPage_->getSelectedItem());
                lastMenuOffsets_[currentPage_->getCollectionName()] = currentPage_->getScrollOffsetIndex();
                lastMenuPlaylists_[currentPage_->getCollectionName()] = currentPage_->getPlaylistName();
                std::string nextPageName = nextPageItem_->name;
                if (!menuMode_)
                {
                    // Load new layout if available
                    std::string layoutName;
                    config_.getProperty("layout", layoutName);
                    PageBuilder pb(layoutName, "layout", config_, &fontcache_);
                    Page* page = pb.buildPage(nextPageItem_->name);
                    if (page)
                    {
                        currentPage_->freeGraphicsMemory();
                        pages_.push(currentPage_);
                        currentPage_ = page;
                    }
                }

                config_.setProperty("currentCollection", nextPageName);

                CollectionInfo* info;
                if (menuMode_)
                    info = getMenuCollection(nextPageName);
                else
                    info = getCollection(nextPageName);

                currentPage_->pushCollection(info);

                bool rememberMenu = false;
                config_.getProperty("rememberMenu", rememberMenu);

                std::string autoPlaylist = "all";
                // Check for collection-specific autoPlaylist first
                std::string collectionAutoPlaylistKey = "collections." + currentPage_->getCollectionName() + ".list.autoPlaylist";
                if (!config_.getProperty(collectionAutoPlaylistKey, autoPlaylist))
                {
                    // Fall back to global autoPlaylist if collection-specific one isn’t set
                    config_.getProperty("autoPlaylist", autoPlaylist);
                }

                if (rememberMenu && lastMenuPlaylists_.find(nextPageName) != lastMenuPlaylists_.end())
                {
                    currentPage_->selectPlaylist(lastMenuPlaylists_[nextPageName]); // Switch to last playlist
                }
                else
                {
                    currentPage_->selectPlaylist(autoPlaylist);
                    if (currentPage_->getPlaylistName() != autoPlaylist)
                        currentPage_->selectPlaylist("all");
                }

                if (rememberMenu && lastMenuOffsets_.find(nextPageName) != lastMenuOffsets_.end())
                {
                    currentPage_->setScrollOffsetIndex(lastMenuOffsets_[nextPageName]);
                }

                currentPage_->onNewItemSelected();
                currentPage_->reallocateMenuSpritePoints();

                state = RETROFE_NEXT_PAGE_MENU_LOAD_ART;

                // Check if we've entered an empty collection and need to go back automatically
                if (currentPage_->getCollectionSize() == 0)
                {
                    bool backOnEmpty = false;
                    config_.getProperty("backOnEmpty", backOnEmpty);
                    if (backOnEmpty)
                        state = RETROFE_BACK_MENU_EXIT;
                }

                isInCriticalState_ = false;
            }
            break;

            // Start onMenuEnter animation
        case RETROFE_NEXT_PAGE_MENU_LOAD_ART:
            if (currentPage_->getMenuDepth() != 1)
            {
                currentPage_->enterMenu();
            }
            else
            {
                currentPage_->start();
            }
            if (currentPage_->getSelectedItem())
                l.LEDBlinky(9, currentPage_->getSelectedItem()->collectionInfo->name, currentPage_->getSelectedItem());
            state = RETROFE_NEXT_PAGE_MENU_ENTER;
            break;

            // Wait for onMenuEnter animation to finish
        case RETROFE_NEXT_PAGE_MENU_ENTER:
            if (currentPage_->isIdle())
            {
                inputClear = false;
                config_.getProperty("collectionInputClear", inputClear);
                if (inputClear)
                {
                    // Empty event queue
                    SDL_Event e;
                    while (SDL_PollEvent(&e))
                        input_.update(e);
                    input_.resetStates();
                }
                enterNumberOfTimes_ = 0;
                state = RETROFE_IDLE;
            }
            break;

            // Start exit animation
        case RETROFE_COLLECTION_DOWN_REQUEST:
            if (!pages_.empty() && currentPage_->getMenuDepth() == 1) // Inside a collection with a different layout
            {
                currentPage_->stop();
                m.clearPage();
                menuMode_ = false;
                state = RETROFE_COLLECTION_DOWN_EXIT;
            }
            else if (currentPage_->getMenuDepth() > 1) // Inside a collection with the same layout
            {
                currentPage_->exitMenu();
                state = RETROFE_COLLECTION_DOWN_EXIT;
            }
            else // Not in a collection
            {
                state = RETROFE_COLLECTION_DOWN_ENTER;

                if (attractMode_) // Check playlist change in attract mode
                {
                    attractModePlaylistCollectionNumber_ += 1;
                    int attractModePlaylistCollectionNumber = 0;
                    config_.getProperty("attractModePlaylistCollectionNumber", attractModePlaylistCollectionNumber);
                    // Check if playlist should be changed
                    if (attractModePlaylistCollectionNumber_ > 0 && attractModePlaylistCollectionNumber_ >= attractModePlaylistCollectionNumber)
                    {
                        attractModePlaylistCollectionNumber_ = 0;
                        currentPage_->nextPlaylist();
                        std::string attractModeSkipPlaylist = "";
                        config_.getProperty("attractModeSkipPlaylist", attractModeSkipPlaylist);
                        if (currentPage_->getPlaylistName() == attractModeSkipPlaylist)
                            currentPage_->nextPlaylist();
                        state = RETROFE_PLAYLIST_REQUEST;
                    }
                }
            }
            break;

            // Wait for the menu exit animation to finish
        case RETROFE_COLLECTION_DOWN_EXIT:
            if (currentPage_->isIdle())
            {
                lastMenuOffsets_[currentPage_->getCollectionName()] = currentPage_->getScrollOffsetIndex();
                lastMenuPlaylists_[currentPage_->getCollectionName()] = currentPage_->getPlaylistName();
                if (currentPage_->getMenuDepth() == 1) // Inside a collection with a different layout
                {
                    currentPage_->deInitialize();
                    delete currentPage_;
                    currentPage_ = pages_.top();
                    pages_.pop();
                    currentPage_->allocateGraphicsMemory();
                }
                else // Inside a collection with the same layout
                {
                    currentPage_->popCollection();
                }
                config_.setProperty("currentCollection", currentPage_->getCollectionName());

                bool rememberMenu = false;
                config_.getProperty("rememberMenu", rememberMenu);

                std::string autoPlaylist = "all";
                // Check for collection-specific autoPlaylist first
                std::string collectionAutoPlaylistKey = "collections." + currentPage_->getCollectionName() + ".list.autoPlaylist";
                if (!config_.getProperty(collectionAutoPlaylistKey, autoPlaylist))
                {
                    // Fall back to global autoPlaylist if collection-specific one isn’t set
                    config_.getProperty("autoPlaylist", autoPlaylist);
                }

                if (rememberMenu && lastMenuPlaylists_.find(currentPage_->getCollectionName()) != lastMenuPlaylists_.end())
                {
                    currentPage_->selectPlaylist(lastMenuPlaylists_[currentPage_->getCollectionName()]); // Switch to last playlist
                }
                else
                {
                    currentPage_->selectPlaylist(autoPlaylist);
                    if (currentPage_->getPlaylistName() != autoPlaylist)
                        currentPage_->selectPlaylist("all");
                }

                if (rememberMenu && lastMenuPlaylists_.find(currentPage_->getCollectionName()) != lastMenuPlaylists_.end())
                {
                    currentPage_->setScrollOffsetIndex(lastMenuOffsets_[currentPage_->getCollectionName()]);
                }

                state = RETROFE_COLLECTION_DOWN_MENU_ENTER;
                currentPage_->onNewItemSelected();

                if (attractMode_) // Check playlist change in attract mode
                {
                    attractModePlaylistCollectionNumber_ += 1;
                    int attractModePlaylistCollectionNumber = 0;
                    config_.getProperty("attractModePlaylistCollectionNumber", attractModePlaylistCollectionNumber);
                    // Check if playlist should be changed
                    if (attractModePlaylistCollectionNumber_ > 0 && attractModePlaylistCollectionNumber_ >= attractModePlaylistCollectionNumber)
                    {
                        attractModePlaylistCollectionNumber_ = 0;
                        currentPage_->nextPlaylist();
                        std::string attractModeSkipPlaylist = "";
                        config_.getProperty("attractModeSkipPlaylist", attractModeSkipPlaylist);
                        if (currentPage_->getPlaylistName() == attractModeSkipPlaylist)
                            currentPage_->nextPlaylist();
                        state = RETROFE_PLAYLIST_REQUEST;
                    }
                }

            }
            break;


            // Start menu enter animation
        case RETROFE_COLLECTION_DOWN_MENU_ENTER:
            currentPage_->enterMenu();
            state = RETROFE_COLLECTION_DOWN_ENTER;
            break;


            // Waiting for enter animation to stop
        case RETROFE_COLLECTION_DOWN_ENTER:
            if (currentPage_->isIdle())
            {
                int attractModePlaylistCollectionNumber = 0;
                config_.getProperty("attractModePlaylistCollectionNumber", attractModePlaylistCollectionNumber);
                if (!(attractMode_ && attractModePlaylistCollectionNumber > 0 && attractModePlaylistCollectionNumber_ == 0))
                {
                    currentPage_->setScrolling(Page::ScrollDirectionForward);
                    currentPage_->scroll(true);
                    currentPage_->updateScrollPeriod();
                }
                state = RETROFE_COLLECTION_DOWN_SCROLL;
            }
            break;

            // Waiting for scrolling animation to stop
        case RETROFE_COLLECTION_DOWN_SCROLL:
            if (currentPage_->isMenuIdle())
            {
                std::string attractModeSkipCollection = "";
                config_.getProperty("attractModeSkipCollection", attractModeSkipCollection);
                // Check if we need to skip this collection in attract mode or if we can select it
                if (attractMode_ && currentPage_->getSelectedItem()->name == attractModeSkipCollection)
                {
                    currentPage_->setScrolling(Page::ScrollDirectionForward);
                    currentPage_->scroll(true);
                    currentPage_->updateScrollPeriod();
                }
                else
                {
                    RETROFE_STATE state_tmp = processUserInput(currentPage_);
                    if (state_tmp == RETROFE_COLLECTION_DOWN_REQUEST)
                    {
                        state = RETROFE_COLLECTION_DOWN_REQUEST;
                    }
                    else if (state_tmp == RETROFE_COLLECTION_UP_REQUEST)
                    {
                        state = RETROFE_COLLECTION_UP_REQUEST;
                    }
                    else
                    {
                        currentPage_->setScrolling(Page::ScrollDirectionIdle); // Stop scrolling
                        nextPageItem_ = currentPage_->getSelectedItem();
                        bool enterOnCollection = true;
                        config_.getProperty("enterOnCollection", enterOnCollection);
                        if (currentPage_->getSelectedItem()->leaf || (!attractMode_ && !enterOnCollection)) // Current selection is a game or enterOnCollection is not set
                        {
                            state = RETROFE_HIGHLIGHT_REQUEST;
                        }
                        else // Current selection is a menu
                        {
                            state = RETROFE_COLLECTION_HIGHLIGHT_EXIT;
                        }
                    }
                }
            }
            break;


            // Start onHighlightExit animation
        case RETROFE_COLLECTION_HIGHLIGHT_REQUEST:
            currentPage_->highlightExit();
            state = RETROFE_COLLECTION_HIGHLIGHT_EXIT;
            break;

            // Wait for onHighlightExit animation to finish; load art
        case RETROFE_COLLECTION_HIGHLIGHT_EXIT:
            if (currentPage_->isIdle())
            {
                currentPage_->highlightLoadArt();
                state = RETROFE_COLLECTION_HIGHLIGHT_LOAD_ART;
            }
            break;

            // Start onHighlightEnter animation
        case RETROFE_COLLECTION_HIGHLIGHT_LOAD_ART:
            currentPage_->highlightEnter();
            if (currentPage_->getSelectedItem())
                l.LEDBlinky(9, currentPage_->getSelectedItem()->collectionInfo->name, currentPage_->getSelectedItem());
            state = RETROFE_COLLECTION_HIGHLIGHT_ENTER;
            break;

            // Wait for onHighlightEnter animation to finish
        case RETROFE_COLLECTION_HIGHLIGHT_ENTER:
            if (currentPage_->isIdle())
            {
                RETROFE_STATE state_tmp = processUserInput(currentPage_);
                if (state_tmp == RETROFE_COLLECTION_DOWN_REQUEST)
                {
                    state = RETROFE_COLLECTION_DOWN_REQUEST;
                }
                else if (state_tmp == RETROFE_COLLECTION_UP_REQUEST)
                {
                    state = RETROFE_COLLECTION_UP_REQUEST;
                }
                else
                {
                    state = RETROFE_NEXT_PAGE_REQUEST;
                }
            }
            break;

            // Start exit animation
        case RETROFE_COLLECTION_UP_REQUEST:
            if (!pages_.empty() && currentPage_->getMenuDepth() == 1) // Inside a collection with a different layout
            {
                currentPage_->stop();
                m.clearPage();
                menuMode_ = false;
                state = RETROFE_COLLECTION_UP_EXIT;
            }
            else if (currentPage_->getMenuDepth() > 1) // Inside a collection with the same layout
            {
                currentPage_->exitMenu();
                state = RETROFE_COLLECTION_UP_EXIT;
            }
            else // Not in a collection
            {
                state = RETROFE_COLLECTION_UP_ENTER;
            }
            break;

            // Wait for the menu exit animation to finish
        case RETROFE_COLLECTION_UP_EXIT:
            if (currentPage_->isIdle())
            {
                lastMenuOffsets_[currentPage_->getCollectionName()] = currentPage_->getScrollOffsetIndex();
                lastMenuPlaylists_[currentPage_->getCollectionName()] = currentPage_->getPlaylistName();
                if (currentPage_->getMenuDepth() == 1) // Inside a collection with a different layout
                {
                    currentPage_->deInitialize();
                    delete currentPage_;
                    currentPage_ = pages_.top();
                    pages_.pop();
                    currentPage_->allocateGraphicsMemory();
                }
                else // Inside a collection with the same layout
                {
                    currentPage_->popCollection();
                }
                config_.setProperty("currentCollection", currentPage_->getCollectionName());

                bool rememberMenu = false;
                config_.getProperty("rememberMenu", rememberMenu);

                std::string autoPlaylist = "all";
                // Check for collection-specific autoPlaylist first
                std::string collectionAutoPlaylistKey = "collections." + currentPage_->getCollectionName() + ".list.autoPlaylist";
                if (!config_.getProperty(collectionAutoPlaylistKey, autoPlaylist))
                {
                    // Fall back to global autoPlaylist if collection-specific one isn’t set
                    config_.getProperty("autoPlaylist", autoPlaylist);
                }

                if (rememberMenu && lastMenuPlaylists_.find(currentPage_->getCollectionName()) != lastMenuPlaylists_.end())
                {
                    currentPage_->selectPlaylist(lastMenuPlaylists_[currentPage_->getCollectionName()]); // Switch to last playlist
                }
                else
                {
                    currentPage_->selectPlaylist(autoPlaylist);
                    if (currentPage_->getPlaylistName() != autoPlaylist)
                        currentPage_->selectPlaylist("all");
                }

                if (rememberMenu && lastMenuOffsets_.find(currentPage_->getCollectionName()) != lastMenuOffsets_.end())
                {
                    currentPage_->setScrollOffsetIndex(lastMenuOffsets_[currentPage_->getCollectionName()]);
                }

                currentPage_->onNewItemSelected();
                state = RETROFE_COLLECTION_UP_MENU_ENTER;
            }
            break;


            // Start menu enter animation
        case RETROFE_COLLECTION_UP_MENU_ENTER:
            currentPage_->enterMenu();
            state = RETROFE_COLLECTION_UP_ENTER;
            break;


            // Waiting for enter animation to stop
        case RETROFE_COLLECTION_UP_ENTER:
            if (currentPage_->isIdle())
            {
                currentPage_->setScrolling(Page::ScrollDirectionBack);
                currentPage_->scroll(false);
                currentPage_->updateScrollPeriod();
                state = RETROFE_COLLECTION_UP_SCROLL;
            }
            break;

            // Waiting for scrolling animation to stop
        case RETROFE_COLLECTION_UP_SCROLL:
            if (currentPage_->isMenuIdle())
            {
                RETROFE_STATE state_tmp;
                state_tmp = processUserInput(currentPage_);
                if (state_tmp == RETROFE_COLLECTION_DOWN_REQUEST)
                {
                    state = RETROFE_COLLECTION_DOWN_REQUEST;
                }
                else if (state_tmp == RETROFE_COLLECTION_UP_REQUEST)
                {
                    state = RETROFE_COLLECTION_UP_REQUEST;
                }
                else
                {
                    currentPage_->setScrolling(Page::ScrollDirectionIdle); // Stop scrolling
                    nextPageItem_ = currentPage_->getSelectedItem();
                    bool enterOnCollection = true;
                    config_.getProperty("enterOnCollection", enterOnCollection);
                    if (currentPage_->getSelectedItem()->leaf || !enterOnCollection) // Current selection is a game or enterOnCollection is not set
                    {
                        state = RETROFE_HIGHLIGHT_REQUEST;
                    }
                    else // Current selection is a menu
                    {
                        state = RETROFE_COLLECTION_HIGHLIGHT_EXIT;
                    }
                }
            }
            break;


            // Launching a menu entry
        case RETROFE_HANDLE_MENUENTRY:

            // Empty event queue
            SDL_Event e;
            while (SDL_PollEvent(&e))
                input_.update(e);
            input_.resetStates();

            // Handle menu entry
            m.handleEntry(currentPage_->getSelectedItem());

            // Empty event queue
            while (SDL_PollEvent(&e))
                input_.update(e);
            input_.resetStates();

            state = RETROFE_IDLE;
            break;

            // Launching game; start onGameEnter animation
        case RETROFE_LAUNCH_ENTER:
            currentPage_->enterGame();  // Start onGameEnter animation
            currentPage_->playSelect(); // Play launch sound
            state = RETROFE_LAUNCH_REQUEST;
            break;

            // Wait for onGameEnter animation to finish; launch game; start onGameExit animation
        case RETROFE_LAUNCH_REQUEST:
            if (currentPage_->isIdle() && !currentPage_->isSelectPlaying())
            {
                nextPageItem_ = currentPage_->getSelectedItem();

                // Clear the screen to black before launching the game
                SDL_Renderer* renderer = SDL::getRenderer(0);
                if (renderer) {
                    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Set color to black (RGBA: 0, 0, 0, 255)
                    SDL_RenderClear(renderer);                      // Clear the renderer
                    SDL_RenderPresent(renderer);                    // Present the cleared (black) screen
                }
                else {
                    Logger::write(Logger::ZONE_WARNING, "RetroFE", "No valid renderer available to clear screen before game launch");
                }

                launchEnter();
                CollectionInfoBuilder cib(config_, *metadb_);
                std::string attractModeSkipPlaylist = "";
                std::string lastPlayedSkipCollection = "";
                int size = 0;
                config_.getProperty("attractModeSkipPlaylist", attractModeSkipPlaylist);
                config_.getProperty("lastPlayedSkipCollection", lastPlayedSkipCollection);
                config_.getProperty("lastplayedSize", size);

                CollectionInfo* targetCollection = currentPage_->getCollection();
                if (targetCollection->name == "TempGameCollection" && targetCollection->originalCollection)
                {
                    targetCollection = targetCollection->originalCollection; // Use original collection
                }

                if (currentPage_->getPlaylistName() != attractModeSkipPlaylist &&
                    nextPageItem_->collectionInfo->name != lastPlayedSkipCollection)
                    cib.updateLastPlayedPlaylist(targetCollection, nextPageItem_, size); // Update lastplayed in original collection

                l.LEDBlinky(3, nextPageItem_->collectionInfo->name, nextPageItem_);
                if (l.run(nextPageItem_->collectionInfo->name, nextPageItem_))
                {
                    attract_.reset();
                    reboot_ = true;
                    state = RETROFE_QUIT_REQUEST;
                }
                else
                {
                    launchExit();
                    // Calculate play duration
                    float launchEndTime = static_cast<float>(SDL_GetTicks()) / 1000.0f;
                    float playDurationSeconds = launchEndTime - launchStartTime_;
                    int playDurationMinutes = static_cast<int>(playDurationSeconds / 60.0f);
                    if (cib_)
                    {
                        cib_->saveGameStats(targetCollection, nextPageItem_, playDurationMinutes); // Save to original collection
						cib_->saveTopGameStats(targetCollection); // Save top game stats to collection
                        // Determine the collection to reload stats from
                        std::string collectionName = nextPageItem_->collectionInfo ? nextPageItem_->collectionInfo->name : targetCollection->name;
                        if (targetCollection->originalCollection) {
                            collectionName = targetCollection->name; // Subcollection (e.g., Laser Games)
                            Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Reloading from original collection: " + collectionName);
                        }
                        else if (collectionName == targetCollection->name) {
                            // Fallback: Check for .sub files to confirm original collection
                            std::string collectionDir = Utils::combinePath(Configuration::absolutePath, "collections", targetCollection->name);
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
                                            if (!line.empty() && line == nextPageItem_->name) {
                                                collectionName = file.substr(0, position); // e.g., Laser Games
                                                Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Detected original collection via .sub file: " + collectionName + " for item " + nextPageItem_->name);
                                                break;
                                            }
                                        }
                                        subFile.close();
                                        if (collectionName != targetCollection->name) break;
                                    }
                                }
                                closedir(dp);
                            }
                        }

                        // Reload stats from original collection's gamesinfo
                        std::string statsPath = Utils::combinePath(Configuration::absolutePath, "collections", collectionName, "gamesinfo", nextPageItem_->name + ".txt");
                        Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Reloading stats for " + nextPageItem_->name + " from " + statsPath);
                        try
                        {
                            // Clear existing stats to ensure fresh load
                            nextPageItem_->info_.clear();
                            nextPageItem_->loadInfo(statsPath);
                            // Log reloaded stats
                            std::string value;
                            if (nextPageItem_->getInfo("playCount", value))
                                Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Reloaded playCount=" + value + " for " + nextPageItem_->name);
                            if (nextPageItem_->getInfo("totalPlayTime", value))
                                Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Reloaded totalPlayTime=" + value + " for " + nextPageItem_->name);
                            if (nextPageItem_->getInfo("lastPlayedDate", value))
                                Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Reloaded lastPlayedDate=" + value + " for " + nextPageItem_->name);
                            if (nextPageItem_->getInfo("lastPlayDuration", value))
                                Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Reloaded lastPlayDuration=" + value + " for " + nextPageItem_->name);
                        }
                        catch (const std::exception& e)
                        {
                            Logger::write(Logger::ZONE_ERROR, "RetroFE", "Exception while reloading stats for " + nextPageItem_->name + " from " + statsPath + ": " + e.what());
                        }
                    }
                    l.LEDBlinky(4);
                    currentPage_->exitGame();
                    // Trigger UI refresh
                    currentPage_->onNewItemSelected();
                    // Force reload of all reloadable components
                    currentPage_->restart();
                    state = RETROFE_LAUNCH_EXIT;
                    //launchExit();
                    //// Calculate play duration
                    //float launchEndTime = static_cast<float>(SDL_GetTicks()) / 1000.0f;
                    //float playDurationSeconds = launchEndTime - launchStartTime_;
                    //int playDurationMinutes = static_cast<int>(playDurationSeconds / 60.0f);
                    //if (cib_)
                    //{
                    //    cib_->saveGameStats(targetCollection, nextPageItem_, playDurationMinutes); // Save to original collection
                    //  cib_->saveTopGameStats(targetCollection); // Save top game stats to original collection
                    //   // Determine the collection to reload stats from
                    //    // Determine the collection to reload stats from
                    //    std::string collectionName = nextPageItem_->collectionInfo ? nextPageItem_->collectionInfo->name : targetCollection->name;
                    //    if (targetCollection->originalCollection) {
                    //        collectionName = targetCollection->name; // Subcollection (e.g., Laser Games)
                    //        Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Reloading from original collection: " + collectionName);
                    //    }
                    //    else if (collectionName == targetCollection->name) {
                    //        // Fallback: Check for .sub files to confirm original collection
                    //        std::string collectionDir = Utils::combinePath(Configuration::absolutePath, "collections", targetCollection->name);
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
                    //                        if (!line.empty() && line == nextPageItem_->name) {
                    //                            collectionName = file.substr(0, position); // e.g., Laser Games
                    //                            Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Detected original collection via .sub file: " + collectionName + " for item " + nextPageItem_->name);
                    //                            break;
                    //                        }
                    //                    }
                    //                    subFile.close();
                    //                    if (collectionName != targetCollection->name) break;
                    //                }
                    //            }
                    //            closedir(dp);
                    //        }
                    //    }

                    //    // Reload stats from original collection's gamesinfo
                    //    std::string statsPath = Utils::combinePath(Configuration::absolutePath, "collections", collectionName, "gamesinfo", nextPageItem_->name + ".txt");
                    //    Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Reloading stats for " + nextPageItem_->name + " from " + statsPath);
                    //    try
                    //    {
                    //        nextPageItem_->loadInfo(statsPath);
                    //        // Log reloaded stats
                    //        std::string value;
                    //        if (nextPageItem_->getInfo("playCount", value))
                    //            Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Reloaded playCount=" + value + " for " + nextPageItem_->name);
                    //        if (nextPageItem_->getInfo("totalPlayTime", value))
                    //            Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Reloaded totalPlayTime=" + value + " for " + nextPageItem_->name);
                    //        if (nextPageItem_->getInfo("lastPlayedDate", value))
                    //            Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Reloaded lastPlayedDate=" + value + " for " + nextPageItem_->name);
                    //        if (nextPageItem_->getInfo("lastPlayDuration", value))
                    //            Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Reloaded lastPlayDuration=" + value + " for " + nextPageItem_->name);
                    //    }
                    //    catch (const std::exception& e)
                    //    {
                    //        Logger::write(Logger::ZONE_ERROR, "RetroFE", "Exception while reloading stats for " + nextPageItem_->name + " from " + statsPath + ": " + e.what());
                    //    }
                    //}
                    //l.LEDBlinky(4);
                    //currentPage_->exitGame();
                    //// Trigger UI refresh
                    //currentPage_->onNewItemSelected();
                    //state = RETROFE_LAUNCH_EXIT;
                }
            }
            break;

            // Wait for onGameExit animation to finish
        case RETROFE_LAUNCH_EXIT:
            if (currentPage_->isIdle())
            {
                state = RETROFE_IDLE;
            }
            break;

            // Go back a page; start onMenuExit animation
        case RETROFE_BACK_REQUEST:
            if (currentPage_->getMenuDepth() == 1)
            {
                currentPage_->stop();
                m.clearPage();
                menuMode_ = false;
            }
            else
            {
                currentPage_->exitMenu();
            }
            state = RETROFE_BACK_MENU_EXIT;
            break;

            // Wait for onMenuExit animation to finish; load previous page; load art
        case RETROFE_BACK_MENU_EXIT:
            if (currentPage_->isIdle())
            {
                lastMenuOffsets_[currentPage_->getCollectionName()] = currentPage_->getScrollOffsetIndex();
                lastMenuPlaylists_[currentPage_->getCollectionName()] = currentPage_->getPlaylistName();
                if (currentPage_->getMenuDepth() == 1)
                {
                    currentPage_->deInitialize();
                    delete currentPage_;
                    currentPage_ = pages_.top();
                    pages_.pop();
                    currentPage_->allocateGraphicsMemory();
                }
                else
                {
                    currentPage_->popCollection();
                }
                config_.setProperty("currentCollection", currentPage_->getCollectionName());

                bool rememberMenu = false;
                config_.getProperty("rememberMenu", rememberMenu);

                std::string autoPlaylist = "all";
                // Check for collection-specific autoPlaylist first
                std::string collectionAutoPlaylistKey = "collections." + currentPage_->getCollectionName() + ".list.autoPlaylist";
                if (!config_.getProperty(collectionAutoPlaylistKey, autoPlaylist))
                {
                    // Fall back to global autoPlaylist if collection-specific one isn’t set
                    config_.getProperty("autoPlaylist", autoPlaylist);
                }

                if (rememberMenu && lastMenuPlaylists_.find(currentPage_->getCollectionName()) != lastMenuPlaylists_.end())
                {
                    currentPage_->selectPlaylist(lastMenuPlaylists_[currentPage_->getCollectionName()]); // Switch to last playlist
                }
                else
                {
                    currentPage_->selectPlaylist(autoPlaylist);
                    if (currentPage_->getPlaylistName() != autoPlaylist)
                        currentPage_->selectPlaylist("all");
                }

                if (rememberMenu && lastMenuOffsets_.find(currentPage_->getCollectionName()) != lastMenuOffsets_.end())
                {
                    currentPage_->setScrollOffsetIndex(lastMenuOffsets_[currentPage_->getCollectionName()]);
                }

                currentPage_->onNewItemSelected();
                currentPage_->reallocateMenuSpritePoints();
                state = RETROFE_BACK_MENU_LOAD_ART;
            }
            break;

            // Start onMenuEnter animation
        case RETROFE_BACK_MENU_LOAD_ART:
            currentPage_->enterMenu();
            state = RETROFE_BACK_MENU_ENTER;
            break;

            // Wait for onMenuEnter animation to finish
        case RETROFE_BACK_MENU_ENTER:
            if (currentPage_->isIdle())
            {
                bool collectionInputClear = false;
                config_.getProperty("collectionInputClear", collectionInputClear);
                if (collectionInputClear)
                {
                    // Empty event queue
                    SDL_Event e;
                    while (SDL_PollEvent(&e))
                        input_.update(e);
                    input_.resetStates();
                }
                state = RETROFE_IDLE;
            }
            break;

            // Start menu mode
        case RETROFE_MENUMODE_START_REQUEST:
            if (currentPage_->isIdle())
            {
                lastMenuOffsets_[currentPage_->getCollectionName()] = currentPage_->getScrollOffsetIndex();
                lastMenuPlaylists_[currentPage_->getCollectionName()] = currentPage_->getPlaylistName();
                std::string layoutName;
                config_.getProperty("layout", layoutName);
                PageBuilder pb(layoutName, "layout", config_, &fontcache_, true);
                Page* page = pb.buildPage();
                if (page)
                {
                    currentPage_->freeGraphicsMemory();
                    pages_.push(currentPage_);
                    currentPage_ = page;
                    menuMode_ = true;
                    m.setPage(page);
                }
                config_.setProperty("currentCollection", "menu");
                CollectionInfo* info = getMenuCollection("menu");
                currentPage_->pushCollection(info);
                currentPage_->onNewItemSelected();
                currentPage_->reallocateMenuSpritePoints();
                state = RETROFE_MENUMODE_START_LOAD_ART;
            }
            break;

        case RETROFE_MENUMODE_START_LOAD_ART:
            currentPage_->start();
            state = RETROFE_MENUMODE_START_ENTER;
            break;

        case RETROFE_MENUMODE_START_ENTER:
            if (currentPage_->isIdle())
            {
                SDL_Event e;
                while (SDL_PollEvent(&e))
                    input_.update(e);
                input_.resetStates();
                state = RETROFE_IDLE;
            }
            break;

            // Wait for splash mode animation to finish
        case RETROFE_NEW:
            if (currentPage_->isIdle())
            {
                state = RETROFE_IDLE;
            }
            break;

        //Reload Layout
            case RETROFE_RELOAD_REQUEST:
           
                if (currentPage_->getCollectionName() == "TempGameCollection") 
                {
                    state = RETROFE_GAME_PAGE_EXIT; // Trigger exit from game page
                }
                else {
                    reloadLayout();
                    input_.resetStates();
                    state = RETROFE_IDLE;
                }
			break;

        //Option Menu
        case RETROFE_CONFIGMENU_REQUEST:
            if (currentPage_->isIdle()) {
                if (currentPage_->getCollectionName() == "TempGameCollection") {
                    state = RETROFE_GAME_PAGE_EXIT; // Trigger exit from game page
                }
                else {
                    Logger::write(Logger::ZONE_INFO, "RetroFE", "Not in TempGameCollection, opening options menu");
                    currentPage_->stop();
                    launchEnter();
                    state = RETROFE_CONFIGMENU_ENTER;
                }
            }
            break;

        case RETROFE_CONFIGMENU_ENTER:
        {
            SDL_Renderer* renderer = SDL::getRenderer(0);
            if (!renderer) {
                Logger::write(Logger::ZONE_ERROR, "RetroFE", "No valid renderer available for OptionsMenu");
                state = RETROFE_CONFIGMENU_EXIT;
                break;
            }
            // Clear the screen to black
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Set color to black (RGBA: 0, 0, 0, 255)
            SDL_RenderClear(renderer);                      // Clear the renderer
            SDL_RenderPresent(renderer);                    // Present the cleared (black) screen
            // Proceed with showing the options menu
            OptionsMenu optionsMenu(config_);
            bool settingsChanged = optionsMenu.show();
            if (settingsChanged) {
                reboot_ = true;
                Logger::write(Logger::ZONE_INFO, "RetroFE", "OptionsMenu requested reboot");
                state = RETROFE_QUIT_REQUEST; // Exit immediately and reboot
            }
            else {
                state = RETROFE_CONFIGMENU_EXIT; // Return to menu
            }
        }
        break;

        case RETROFE_CONFIGMENU_EXIT:
            launchExit();
            currentPage_->restart();
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_JOYDEVICEADDED || e.type == SDL_JOYDEVICEREMOVED) {
                    input_.update(e);
                }
            }
            input_.resetStates();
            state = RETROFE_IDLE;
            break;

        case RETROFE_SEARCH_REQUEST:
            if (currentPage_->isIdle()) {
                currentPage_->stop(); // Pause current page rendering
                launchEnter();
                state = RETROFE_SEARCH_ENTER;
            }
            break;

        case RETROFE_SEARCH_ENTER: {
            SDL_Renderer* renderer = SDL::getRenderer(0);
            if (!renderer) {
                Logger::write(Logger::ZONE_ERROR, "RetroFE", "No valid renderer available for search");
                state = RETROFE_SEARCH_EXIT;
                break;
            }
            // Clear the screen to black
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Set color to black (RGBA: 0, 0, 0, 255)
            SDL_RenderClear(renderer);                      // Clear the renderer
            SDL_RenderPresent(renderer);                    // Present the cleared (black) screen

            // Capture the max modification time of Search Collection before search
            std::string searchDir = Utils::combinePath(Configuration::absolutePath, "collections", "Search Collection");
            preSearchMaxModTime_ = Utils::getMaxFileModTime(searchDir);
#ifdef _DEBUG
            Logger::write(Logger::ZONE_INFO, "RetroFE", "Captured pre-search modification time for Search Collection: " + std::to_string(preSearchMaxModTime_));
#endif

            // Proceed with launching search
            Launcher launcheS(config_);
            launcheS.launchSearch();  // Call the new public function
            state = RETROFE_SEARCH_EXIT; // Proceed to exit after search completes
        }
                                 break;

        case RETROFE_SEARCH_EXIT: {
            launchExit();
            currentPage_->restart();
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_JOYDEVICEADDED || e.type == SDL_JOYDEVICEREMOVED) {
                    input_.update(e);
                }
            }
            input_.resetStates();
            // Check if Search Collection was updated
            Logger::write(Logger::ZONE_INFO, "RetroFE", "Checking Search Collection after search...");
            std::string searchDir = Utils::combinePath(Configuration::absolutePath, "collections", "Search Collection");
            time_t postSearchMaxModTime = Utils::getMaxFileModTime(searchDir);
            bool searchUpdated = (postSearchMaxModTime > preSearchMaxModTime_);
            if (!searchUpdated) {
                // No updates, return to current collection
                Logger::write(Logger::ZONE_INFO, "RetroFE", "No updates in Search Collection; returning to current collection: " + currentPage_->getCollectionName());
                currentPage_->allocateGraphicsMemory();
                currentPage_->start();
                state = RETROFE_IDLE;
            }
            else {
                // Store current state
                lastMenuOffsets_[currentPage_->getCollectionName()] = currentPage_->getScrollOffsetIndex();
                lastMenuPlaylists_[currentPage_->getCollectionName()] = currentPage_->getPlaylistName();
                // Free the current page
                Logger::write(Logger::ZONE_INFO, "RetroFE", "Freeing current page after search...");
                currentPage_->freeGraphicsMemory();
                currentPage_->deInitialize();
                delete currentPage_;
                currentPage_ = nullptr;
                // Reload the layout
                Logger::write(Logger::ZONE_INFO, "RetroFE", "Reloading layout after search...");
                currentPage_ = loadPage();
                if (!currentPage_) {
                    Logger::write(Logger::ZONE_ERROR, "RetroFE", "Failed to reload layout after search!");
                    state = RETROFE_QUIT_REQUEST;
                    break;
                }
                // Rebuild menu tree to ensure correct navigation
                buildMenuTree();
                // Navigate to Search Collection
                Logger::write(Logger::ZONE_INFO, "RetroFE", "Navigating to Search Collection");
                bool autoEnter = true; // Automatically enter Search Collection
                if (!navigateToCollection("Search Collection", autoEnter)) {
                    Logger::write(Logger::ZONE_ERROR, "RetroFE", "Failed to navigate to Search Collection; falling back to Main");
                    // Fallback to Main collection
                    std::string mainCollection = "Main";
                    config_.setProperty("currentCollection", mainCollection);
                    CollectionInfo* mainInfo = getCollection(mainCollection);
                    if (!mainInfo) {
                        Logger::write(Logger::ZONE_ERROR, "RetroFE", "Failed to load Main collection");
                        state = RETROFE_QUIT_REQUEST;
                        break;
                    }
                    while (currentPage_->getMenuDepth() > 0) {
                        currentPage_->popCollection();
                    }
                    currentPage_->pushCollection(mainInfo);
                    std::string firstPlaylist = "all";
                    config_.getProperty("firstPlaylist", firstPlaylist);
                    currentPage_->selectPlaylist(firstPlaylist);
                    if (currentPage_->getPlaylistName() != firstPlaylist) {
                        currentPage_->selectPlaylist("all");
                    }
                    currentPage_->onNewItemSelected();
                    currentPage_->reallocateMenuSpritePoints();
                    currentPage_->allocateGraphicsMemory();
                    currentPage_->start();
                    SDL_PumpEvents();
                    input_.resetStates();
                    state = RETROFE_IDLE;
                }
                else {
                    state = RETROFE_LOAD_ART; // Proceed to load art, as in SPLASH_EXIT
                }
            }
        }
        break;

        case RETROFE_GAME_PAGE_ENTER:
            currentPage_->allocateGraphicsMemory();
            currentPage_->start();
            state = RETROFE_IDLE;
            break;

        case RETROFE_GAME_PAGE_EXIT:
            if (currentPage_->isIdle()) 
            {
                currentPage_->stop();
				currentPage_->freeGraphicsMemory(); // Free graphics memory
               
                currentPage_ = pages_.top();  
                pages_.pop();                 
                
				//currentPage_->start(); // Restart the previous page
                currentPage_->allocateGraphicsMemory(); 
                state = RETROFE_IDLE;        
            }
            break;

            // Start the onExit animation
        case RETROFE_QUIT_REQUEST:
            currentPage_->stop();
            state = RETROFE_QUIT;
            break;

            // Wait for onExit animation to finish before quitting RetroFE
        case RETROFE_QUIT:
            bool LEDBlinkyCloseOnExit = true;
            config_.getProperty("LEDBlinkyCloseOnExit", LEDBlinkyCloseOnExit);
            if (currentPage_->isGraphicsIdle())
            {
                if (LEDBlinkyCloseOnExit == true) {
                    l.LEDBlinky(2);
                    running = false;
                }
                else running = false;
            }
            break;
        }
        bool vSync = false;
        config_.getProperty("vSync", vSync);
        // Handle screen updates and attract mode
        if (running)
        {
            lastTime = currentTime_;
            currentTime_ = static_cast<float>(SDL_GetTicks()) / 1000;

            if (currentTime_ < lastTime)
            {
                currentTime_ = lastTime;
            }

            deltaTime = currentTime_ - lastTime;

            // dynamic layout check
            layoutCheckTimer_ += deltaTime;
            if (layoutCheckTimer_ >= layoutCheckInterval_)
            {
                checkAndReloadLayout();
                layoutCheckTimer_ = 0.0f;
            }

            double sleepTime;
            if (state == RETROFE_IDLE)
                sleepTime = fpsIdleTime - deltaTime * 1000;
            else
                sleepTime = fpsTime - deltaTime * 1000;
            if (sleepTime > 0 && sleepTime < 1000 && !vSync)
            {
                SDL_Delay(static_cast<unsigned int>(sleepTime));


            }

            if (currentPage_)
            {
                if (!splashMode)
                {
                    int attractReturn = attract_.update(deltaTime, *currentPage_);
                    if (attractReturn == 1) // Change playlist
                    {
                        attract_.reset(attract_.isSet());

                        bool cyclePlaylist = false;
                        config_.getProperty("attractModeCyclePlaylist", cyclePlaylist);

                        std::string cycleString;
                        config_.getProperty("cyclePlaylist", cycleString);
                        std::vector<std::string> cycleVector;
                        Utils::listToVector(cycleString, cycleVector, ',');

                        if (cyclePlaylist)
                            currentPage_->nextCyclePlaylist(cycleVector);
                        else
                            currentPage_->nextPlaylist();

                        std::string attractModeSkipPlaylist = "";
                        config_.getProperty("attractModeSkipPlaylist", attractModeSkipPlaylist);
                        if (currentPage_->getPlaylistName() == attractModeSkipPlaylist)
                        {
                            if (cyclePlaylist)
                                currentPage_->nextCyclePlaylist(cycleVector);
                            else
                                currentPage_->nextPlaylist();
                        }
                        state = RETROFE_PLAYLIST_REQUEST;
                    }
                    if (attractReturn == 2) // Change collection
                    {
                        attract_.reset(attract_.isSet());
                        state = RETROFE_COLLECTION_DOWN_REQUEST;
                    }
                }
                if (menuMode_)
                {
                    attract_.reset();
                }
                currentPage_->update(deltaTime);
                SDL_PumpEvents();
                input_.updateKeystate();
                if (!splashMode)
                {
                    if (currentPage_->isAttractIdle())
                    {
                        if (!attractMode_ && attract_.isSet())
                        {
                            currentPage_->attractEnter();
                            l.LEDBlinky(5);
                        }
                        else if (attractMode_ && !attract_.isSet())
                        {
                            currentPage_->attractExit();
                            l.LEDBlinky(6);
                        }
                        else if (attract_.isSet())
                        {
                            currentPage_->attract();
                        }
                        attractMode_ = attract_.isSet();
                    }
                }
            }

            render();
        }
    }
    return reboot_;
}



// Check if we can go back a page or quite RetroFE
bool RetroFE::back(bool& exit)
{
    bool canGoBack = false;
    bool exitOnBack = false;
    config_.getProperty("exitOnFirstPageBack", exitOnBack);
    exit = false;

    if (currentPage_->getMenuDepth() <= 1 && pages_.empty())
    {
        exit = exitOnBack;
    }
    else
    {
        canGoBack = true;
    }

    return canGoBack;
}

void RetroFE::resetInput() {
    input_.resetStates();  // Reset all input states (e.g., up, down, left, right)
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        // Clear the event queue, keeping joystick add/remove events
        if (e.type == SDL_JOYDEVICEADDED || e.type == SDL_JOYDEVICEREMOVED) {
            input_.update(e);
        }
    }
}
// Process the user input
RetroFE::RETROFE_STATE RetroFE::processUserInput(Page* page)
{
    bool exit = false;
    RETROFE_STATE state = RETROFE_IDLE;

    // Poll all events until we find an active one
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        input_.update(e); // Update input state for each event
    }
    // Early exit conditions: no action if page isnt idle or key delay isnt met
    if (!page || !page->isIdle() || (currentTime_ - keyLastTime_ <= keyDelayTime_))
    {
        return state;
    }
    // Handle next/previous game inputs
    ScrollingList* activeMenu = page->getActiveMenu();
    if (!activeMenu) {
        Logger::write(Logger::ZONE_DEBUG, "RetroFE", "No active menu");
        return state;
    }
#ifdef _DEBUG
    Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Active menu found, isGrid=" + std::to_string(activeMenu->isGrid) +
        ", rows=" + std::to_string(activeMenu->rows) + ", columns=" + std::to_string(activeMenu->columns));
#endif
    if (activeMenu->isGrid) {
        bool moved = false;
        int previousIndex = activeMenu->getSelectedIndex();

        if (input_.keystate(UserInput::KeyCodeUp)) {
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Grid: Up input, currentIndex=" + std::to_string(previousIndex));
#endif
            activeMenu->moveUp();
            moved = true;
        }
        else if (input_.keystate(UserInput::KeyCodeDown)) {
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Grid: Down input, currentIndex=" + std::to_string(previousIndex));
#endif
            activeMenu->moveDown();
            moved = true;
        }
        else if (input_.keystate(UserInput::KeyCodeLeft)) {
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Grid: Left input, currentIndex=" + std::to_string(previousIndex));
#endif
            activeMenu->moveLeft();
            moved = true;
        }
        else if (input_.keystate(UserInput::KeyCodeRight)) {
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Grid: Right input, currentIndex=" + std::to_string(previousIndex));
#endif
            activeMenu->moveRight();
            moved = true;
        }

        if (moved && activeMenu->getSelectedIndex() != previousIndex) {
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Grid: Moved from index " + std::to_string(previousIndex) +
                " to " + std::to_string(activeMenu->getSelectedIndex()));
#endif
            attract_.reset();
            page->onNewItemSelected();
            // page->playSound("highlight"); // uncoment if sound feedback is desired
            keyLastTime_ = currentTime_;
            resetInput();  // Reset inputs after movement
            return state;
        }
    }
    else if (page->isHorizontalScroll())
    {
        if (input_.keystate(UserInput::KeyCodeRight))
        {
            attract_.reset();
            page->setScrolling(Page::ScrollDirectionForward);
            page->scroll(true);
            page->updateScrollPeriod();
            return state;
        }
        else if (input_.keystate(UserInput::KeyCodeLeft))
        {
            attract_.reset();
            page->setScrolling(Page::ScrollDirectionBack);
            page->scroll(false);
            page->updateScrollPeriod();
            return state;
        }
    }
    else
    {
        if (input_.keystate(UserInput::KeyCodeDown))
        {
            attract_.reset();
            page->setScrolling(Page::ScrollDirectionForward);
            page->scroll(true);
            page->updateScrollPeriod();
            return state;
        }
        else if (input_.keystate(UserInput::KeyCodeUp))
        {
            attract_.reset();
            page->setScrolling(Page::ScrollDirectionBack);
            page->scroll(false);
            page->updateScrollPeriod();
            return state;
        }
    }

    // Ignore other keys while the menu is scrolling
    if (page->isIdle() && currentTime_ - keyLastTime_ > keyDelayTime_)
    {
        if (input_.keystate(UserInput::KeyCodeMenu) && !menuMode_)
        {
            state = RETROFE_MENUMODE_START_REQUEST;
        }
        else if (input_.keystate(UserInput::KeyCodeConfigMenu))
        {
            attract_.reset();
            state = RETROFE_CONFIGMENU_REQUEST;
        }
        else if (input_.keystate(UserInput::KeyCodeSearch))
        {
            attract_.reset();
			state = RETROFE_SEARCH_REQUEST;
        }
        else if (input_.keystate(UserInput::KeyCodeRefresh))
        {
            attract_.reset();
            state = RETROFE_RELOAD_REQUEST;
        }

        // Handle Collection Up/Down keys
        else if ((input_.keystate(UserInput::KeyCodeCollectionUp) && (page->isHorizontalScroll() || !input_.keystate(UserInput::KeyCodeUp))) ||
            (input_.keystate(UserInput::KeyCodeCollectionLeft) && (!page->isHorizontalScroll() || !input_.keystate(UserInput::KeyCodeLeft))))
        {
            attract_.reset();
            bool backOnCollection = false;
            config_.getProperty("backOnCollection", backOnCollection);
            if (page->getMenuDepth() == 1 || !backOnCollection)
                state = RETROFE_COLLECTION_UP_REQUEST;
            else
                state = RETROFE_BACK_REQUEST;
        }

        else if ((input_.keystate(UserInput::KeyCodeCollectionDown) && (page->isHorizontalScroll() || !input_.keystate(UserInput::KeyCodeDown))) ||
            (input_.keystate(UserInput::KeyCodeCollectionRight) && (!page->isHorizontalScroll() || !input_.keystate(UserInput::KeyCodeRight))))
        {
            attract_.reset();
            bool backOnCollection = false;
            config_.getProperty("backOnCollection", backOnCollection);
            if (page->getMenuDepth() == 1 || !backOnCollection)
                state = RETROFE_COLLECTION_DOWN_REQUEST;
            else
                state = RETROFE_BACK_REQUEST;
        }

        else if (input_.keystate(UserInput::KeyCodePageUp))
        {
            attract_.reset();
            page->pageScroll(Page::ScrollDirectionBack);
            state = RETROFE_MENUJUMP_REQUEST;
        }

        else if (input_.keystate(UserInput::KeyCodePageDown))
        {
            attract_.reset();
            page->pageScroll(Page::ScrollDirectionForward);
            state = RETROFE_MENUJUMP_REQUEST;
        }

        else if (input_.keystate(UserInput::KeyCodeLetterUp))
        {
            attract_.reset();
            bool cfwLetterSub;
            config_.getProperty("cfwLetterSub", cfwLetterSub);
            if (cfwLetterSub && page->hasSubs())
                page->cfwLetterSubScroll(Page::ScrollDirectionBack);
            else
                page->letterScroll(Page::ScrollDirectionBack);
            state = RETROFE_MENUJUMP_REQUEST;
        }

        else if (input_.keystate(UserInput::KeyCodeLetterDown))
        {
            attract_.reset();
            bool cfwLetterSub;
            config_.getProperty("cfwLetterSub", cfwLetterSub);
            if (cfwLetterSub && page->hasSubs())
                page->cfwLetterSubScroll(Page::ScrollDirectionForward);
            else
                page->letterScroll(Page::ScrollDirectionForward);
            state = RETROFE_MENUJUMP_REQUEST;
        }

        else if (input_.keystate(UserInput::KeyCodeFavPlaylist))
        {
            attract_.reset();
            page->favPlaylist();
            state = RETROFE_PLAYLIST_REQUEST;
        }

        else if (input_.keystate(UserInput::KeyCodeNextPlaylist) ||
            (input_.keystate(UserInput::KeyCodePlaylistDown) && page->isHorizontalScroll()) ||
            (input_.keystate(UserInput::KeyCodePlaylistRight) && !page->isHorizontalScroll()))
        {
            attract_.reset();
            page->nextPlaylist();
            state = RETROFE_PLAYLIST_REQUEST;
        }

        else if (input_.keystate(UserInput::KeyCodePrevPlaylist) ||
            (input_.keystate(UserInput::KeyCodePlaylistUp) && page->isHorizontalScroll()) ||
            (input_.keystate(UserInput::KeyCodePlaylistLeft) && !page->isHorizontalScroll()))
        {
            attract_.reset();
            page->prevPlaylist();
            state = RETROFE_PLAYLIST_REQUEST;
        }

        else if (input_.keystate(UserInput::KeyCodeCyclePlaylist) ||
            input_.keystate(UserInput::KeyCodeNextCyclePlaylist))
        {
            attract_.reset();
            std::string cycleString;
            config_.getProperty("cyclePlaylist", cycleString);
            std::vector<std::string> cycleVector;
            Utils::listToVector(cycleString, cycleVector, ',');
            page->nextCyclePlaylist(cycleVector);
            state = RETROFE_PLAYLIST_REQUEST;
        }

        else if (input_.keystate(UserInput::KeyCodePrevCyclePlaylist))
        {
            attract_.reset();
            std::string cycleString;
            config_.getProperty("cyclePlaylist", cycleString);
            std::vector<std::string> cycleVector;
            Utils::listToVector(cycleString, cycleVector, ',');
            page->prevCyclePlaylist(cycleVector);
            state = RETROFE_PLAYLIST_REQUEST;
        }

        else if (input_.keystate(UserInput::KeyCodeRemovePlaylist))
        {
            attract_.reset();
            page->removePlaylist();
            state = RETROFE_PLAYLIST_REQUEST;
        }

        else if (input_.keystate(UserInput::KeyCodeAddPlaylist))
        {
            attract_.reset();
            page->addPlaylist();
            state = RETROFE_PLAYLIST_REQUEST;
        }

        else if (input_.keystate(UserInput::KeyCodeTogglePlaylist))
        {
            attract_.reset();
            page->togglePlaylist();
            state = RETROFE_PLAYLIST_REQUEST;
        }

        else if (input_.keystate(UserInput::KeyCodeSkipForward))
        {
            attract_.reset();
            page->skipForward();
            page->jukeboxJump();
            keyLastTime_ = currentTime_;
        }

        else if (input_.keystate(UserInput::KeyCodeSkipBackward))
        {
            attract_.reset();
            page->skipBackward();
            page->jukeboxJump();
            keyLastTime_ = currentTime_;
        }

        else if (input_.keystate(UserInput::KeyCodeSkipForwardp))
        {
            attract_.reset();
            page->skipForwardp();
            page->jukeboxJump();
            keyLastTime_ = currentTime_;
        }

        else if (input_.keystate(UserInput::KeyCodeSkipBackwardp))
        {
            attract_.reset();
            page->skipBackwardp();
            page->jukeboxJump();
            keyLastTime_ = currentTime_;
        }

        else if (input_.keystate(UserInput::KeyCodePause))
        {
            attract_.reset();
            page->pause();
            page->jukeboxJump();
            keyLastTime_ = currentTime_;
        }

        else if (input_.keystate(UserInput::KeyCodeRestart))
        {
            attract_.reset();
            page->restart();
            keyLastTime_ = currentTime_;
        }

        else if (input_.keystate(UserInput::KeyCodeRandom))
        {
            attract_.reset();
            page->selectRandom();
            state = RETROFE_MENUJUMP_REQUEST;
        }

        else if (input_.keystate(UserInput::KeyCodeAdminMode))
        {
            //todo: add admin mode support
        }

         else if (input_.keystate(UserInput::KeyCodeSelect))
{
    attract_.reset();
    nextPageItem_ = page->getSelectedItem();

    if (nextPageItem_)
    {
        if (nextPageItem_->leaf)
        {
            if (menuMode_)
            {
                state = RETROFE_HANDLE_MENUENTRY;
            }
            else
            {
                std::string layoutName;
                config_.getProperty("layout", layoutName);
                std::string collectionName = page->getCollectionName();
                std::string gameName = nextPageItem_->name;

                bool gameLayoutExists = checkGameLayoutExists(layoutName, collectionName, gameName);
                bool defaultLayoutExists = checkGameLayoutExists(layoutName, collectionName, "default");

                if (gameLayoutExists || defaultLayoutExists)
                {
                    CollectionInfo* originalCollection = page->getCollection();
                    CollectionInfo* tempCollection = createTempCollection(nextPageItem_, originalCollection);
                    if (!tempCollection)
                    {
                        Logger::write(Logger::ZONE_ERROR, "RetroFE", "Failed to create temp collection for item: " + gameName);
                        state = RETROFE_LAUNCH_ENTER;
                    }
                    else
                    {
                        Page* gamePage = nullptr;
                        if (gameLayoutExists)
                        {
                            Logger::write(Logger::ZONE_INFO, "RetroFE", "Attempting to load game-specific layout for: " + gameName);
                            gamePage = buildGamePage(tempCollection, layoutName, collectionName, gameName);
                        }
                        else if (defaultLayoutExists)
                        {
                            Logger::write(Logger::ZONE_INFO, "RetroFE", "Game-specific layout not found, falling back to default layout");
                            gamePage = buildGamePage(tempCollection, layoutName, collectionName, "default");
                        }

                        if (gamePage)
                        {
                            currentPage_->stop();
                            currentPage_->freeGraphicsMemory();
                            pages_.push(page);
                            currentPage_ = gamePage;
                            currentPage_->pushCollection(tempCollection);
                            currentPage_->onNewItemSelected();
                            currentPage_->reallocateMenuSpritePoints();
                            state = RETROFE_GAME_PAGE_ENTER;
                        }
                        else
                        {
                            Logger::write(Logger::ZONE_ERROR, "RetroFE", "Failed to build game page for layout: " + (gameLayoutExists ? gameName : "default"));
                            delete tempCollection;
                            state = RETROFE_LAUNCH_ENTER;
                        }
                    }
                }
                else
                {
                    Logger::write(Logger::ZONE_INFO, "RetroFE", "No layout found for game '" + gameName + "' or default. Launching directly.");
                    state = RETROFE_LAUNCH_ENTER;
                }
            }
        }
                else
                {
                    CollectionInfoBuilder cib(config_, *metadb_);
                    std::string attractModeSkipPlaylist = "";
                    std::string lastPlayedSkipCollection = "";
                    int size = 0;
                    config_.getProperty("attractModeSkipPlaylist", attractModeSkipPlaylist);
                    config_.getProperty("lastPlayedSkipCollection", lastPlayedSkipCollection);
                    config_.getProperty("lastplayedCollectionSize", size);

                    if (currentPage_->getPlaylistName() != attractModeSkipPlaylist &&
                        nextPageItem_->collectionInfo->name != lastPlayedSkipCollection)
                    {
                        cib.updateLastPlayedPlaylist(currentPage_->getCollection(), nextPageItem_, size);
                    }
                    state = RETROFE_NEXT_PAGE_REQUEST;
                }
            }
        }

        else if (input_.keystate(UserInput::KeyCodeBack))
        {
            attract_.reset();
            if (back(exit))
            {
                // Check if the current page is a game layout page
                if (currentPage_->getCollectionName() == "TempGameCollection")
                {
                    state = RETROFE_GAME_PAGE_EXIT; // New state to exit game page
                }
                else
                {
                    state = RETROFE_BACK_REQUEST; // Normal back behavior
                }
            }
            else if (exit)
            {
                state = RETROFE_QUIT_REQUEST; // Exit the program
            }
            }
       
        else if (input_.keystate(UserInput::KeyCodeQuit))
        {
            attract_.reset();
            state = RETROFE_QUIT_REQUEST;
        }

        else if (input_.keystate(UserInput::KeyCodeReboot))
        {
            attract_.reset();
            reboot_ = true;
            state = RETROFE_QUIT_REQUEST;
        }

        else if (input_.keystate(UserInput::KeyCodeSaveFirstPlaylist))
        {
            attract_.reset();
            if (page->getMenuDepth() == 1)
            {
                firstPlaylist_ = page->getPlaylistName();
                saveRetroFEState();
            }
        }
    }

    if (state != RETROFE_IDLE)
    {
        keyLastTime_ = currentTime_;
        return state;
    }

    // Check if we're done scrolling
    if (!input_.keystate(UserInput::KeyCodeUp) &&
        !input_.keystate(UserInput::KeyCodeLeft) &&
        !input_.keystate(UserInput::KeyCodeDown) &&
        !input_.keystate(UserInput::KeyCodeRight) &&
        !input_.keystate(UserInput::KeyCodePlaylistUp) &&
        !input_.keystate(UserInput::KeyCodePlaylistLeft) &&
        !input_.keystate(UserInput::KeyCodePlaylistDown) &&
        !input_.keystate(UserInput::KeyCodePlaylistRight) &&
        !input_.keystate(UserInput::KeyCodeCollectionUp) &&
        !input_.keystate(UserInput::KeyCodeCollectionLeft) &&
        !input_.keystate(UserInput::KeyCodeCollectionDown) &&
        !input_.keystate(UserInput::KeyCodeCollectionRight) &&
        !input_.keystate(UserInput::KeyCodePageUp) &&
        !input_.keystate(UserInput::KeyCodePageDown) &&
        !input_.keystate(UserInput::KeyCodeLetterUp) &&
        !input_.keystate(UserInput::KeyCodeLetterDown) &&
        !input_.keystate(UserInput::KeyCodeCollectionUp) &&
        !input_.keystate(UserInput::KeyCodeCollectionDown) &&
        !attract_.isActive())
    {
        page->resetScrollPeriod();
        if (page->isMenuScrolling())
        {
            attract_.reset(attract_.isSet());
            state = RETROFE_HIGHLIGHT_REQUEST;
        }
    }

    return state;
}


// Load a page
Page* RetroFE::loadPage()
{
    std::string layoutName;

    config_.getProperty("layout", layoutName);

    PageBuilder pb(layoutName, "layout", config_, &fontcache_);
    Page* page = pb.buildPage();

   if (page) {
        page->setMetadataDatabase(metadb_); // Set the database here
    } else {
        Logger::write(Logger::ZONE_ERROR, "RetroFE", "Could not create page");
    }

    return page;
}


// Load the splash page
Page* RetroFE::loadSplashPage()
{
    std::string layoutName;
    config_.getProperty("layout", layoutName);

    PageBuilder pb(layoutName, "splash", config_, &fontcache_);
    Page* page = pb.buildPage();
    page->start();

    return page;
}

CollectionInfo* RetroFE::getCollection(std::string collectionName) {
    // Check if subcollections should be merged or split
    bool subsSplit = false;
    config_.getProperty("subsSplit", subsSplit);

    // Build the collection
    CollectionInfoBuilder cib(config_, *metadb_);
    CollectionInfo* collection = cib.buildCollection(collectionName);
    collection->subsSplit = subsSplit;
    cib.injectMetadata(collection);

    // Load subcollections 
    DIR* dp;
    struct dirent* dirp;
    std::string path = Utils::combinePath(Configuration::absolutePath, "collections", collectionName);
    dp = opendir(path.c_str());
    while ((dirp = readdir(dp)) != NULL) {
        std::string file = dirp->d_name;
        size_t position = file.find_last_of(".");
        std::string basename = (std::string::npos == position) ? file : file.substr(0, position);
        std::string comparator = ".sub";
        size_t start = file.length() >= comparator.length() ? file.length() - comparator.length() : 0;
        if (start >= 0 && file.compare(start, comparator.length(), comparator) == 0) {
            Logger::write(Logger::ZONE_INFO, "RetroFE", "Loading subcollection into menu: " + basename);
            CollectionInfo* subcollection = cib.buildCollection(basename, collectionName);
            collection->addSubcollection(subcollection);
            subcollection->subsSplit = subsSplit;
            cib.injectMetadata(subcollection);
            collection->hasSubs = true;
        }
    }
    if (dp) closedir(dp);

    // Load game stats
    Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Loading game stats for collection: " + collectionName);
    std::string statsDir = Utils::combinePath(Configuration::absolutePath, "collections", collectionName, "gamesinfo");
    struct stat dirStat;
    if (stat(statsDir.c_str(), &dirStat) != 0 || !(dirStat.st_mode & S_IFDIR)) {
        Logger::write(Logger::ZONE_WARNING, "RetroFE", "Stats directory not found or not a directory: " + statsDir);
    }
    for (std::vector<Item*>::iterator it = collection->items.begin(); it != collection->items.end(); ++it) {
        std::string infoPath = Utils::combinePath(statsDir, (*it)->name + ".txt");
        Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Attempting to load stats for " + (*it)->name + " from " + infoPath);
        (*it)->loadInfo(infoPath);
        // Log loaded stats
        std::string value;
        if ((*it)->getInfo("playCount", value))
            Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Loaded playCount=" + value + " for " + (*it)->name);
        if ((*it)->getInfo("totalPlayTime", value))
            Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Loaded totalPlayTime=" + value + " for " + (*it)->name);
        if ((*it)->getInfo("lastPlayedDate", value))
            Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Loaded lastPlayedDate=" + value + " for " + (*it)->name);
        if ((*it)->getInfo("lastPlayDuration", value))
            Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Loaded lastPlayDuration=" + value + " for " + (*it)->name);
    }

    // Load sorting settings (menuSort and new options)

    std::string prefix = "collections." + collectionName + ".list.";
    bool menuSort = true; // Global default
    config_.getProperty("menuSort", menuSort); // Global setting
    config_.getProperty(prefix + "menuSort", menuSort); // Per-collection override
    collection->menusort = menuSort;

    // Load new settings
    bool lastplayedByLastAccess = false; // Default
    config_.getProperty("lastplayedByLastAccess", lastplayedByLastAccess);
    config_.getProperty(prefix + "lastplayedByLastAccess", lastplayedByLastAccess);
    collection->lastplayedByLastAccess = lastplayedByLastAccess;

    bool favoritesByLastAccess = false; // Default
    config_.getProperty("favoritesByLastAccess", favoritesByLastAccess);
    config_.getProperty(prefix + "favoritesByLastAccess", favoritesByLastAccess);
    collection->favoritesByLastAccess = favoritesByLastAccess;

    bool favoritesCollectionByAsc = false; // Default
    config_.getProperty("favoritesCollectionByAsc", favoritesCollectionByAsc);
    config_.getProperty(prefix + "favoritesCollectionByAsc", favoritesCollectionByAsc);
    collection->favoritesCollectionByAsc = favoritesCollectionByAsc;

    // Deduplicate items
    std::map<std::string, Item*> uniqueItems;
    std::vector<Item*> newItems;
    for (auto* item : collection->items) {
        std::string key = Utils::toLower(item->name);
        auto it = uniqueItems.find(key);
        if (it == uniqueItems.end()) {
            uniqueItems[key] = item;
            newItems.push_back(item);
        }
        else {
#ifdef _DEBUG
            Logger::write(Logger::ZONE_WARNING, "RetroFE", "Duplicate item found in collection " + collectionName + ": " + item->name + " (keeping first occurrence: " + it->second->name + ")");
#endif
            delete item; // Free duplicate item
        }
    }
    collection->items = newItems;

    // Load playlists and apply sorting
    if (collection->menusort) {
        collection->sortItems();
    }

    // Existing metadata and menu parsing code...
    MenuParser mp;
    mp.buildMenuItems(collection, menuSort);

    cib.addPlaylists(collection);
    collection->sortPlaylists();

    // Add extra info, if available
    for (std::vector<Item*>::iterator it = collection->items.begin(); it != collection->items.end(); it++)
    {
        std::string path = Utils::combinePath(Configuration::absolutePath, "collections", collectionName, "info", (*it)->name + ".conf");
        (*it)->loadInfo(path);
    }

    // Remove parenthesis and brackets, if so configured
    bool showParenthesis = true;
    bool showSquareBrackets = true;

    (void)config_.getProperty("showParenthesis", showParenthesis);
    (void)config_.getProperty("showSquareBrackets", showSquareBrackets);

    typedef std::map<std::string, std::vector <Item*>*> Playlists_T;
    for (Playlists_T::iterator itP = collection->playlists.begin(); itP != collection->playlists.end(); itP++)
    {
        for (std::vector <Item*>::iterator itI = itP->second->begin(); itI != itP->second->end(); itI++)
        {
            if (!showParenthesis)
            {
                std::string::size_type firstPos = (*itI)->title.find_first_of("(");
                std::string::size_type secondPos = (*itI)->title.find_first_of(")", firstPos);

                while (firstPos != std::string::npos && secondPos != std::string::npos)
                {
                    firstPos = (*itI)->title.find_first_of("(");
                    secondPos = (*itI)->title.find_first_of(")", firstPos);

                    if (firstPos != std::string::npos)
                    {
                        (*itI)->title.erase(firstPos, (secondPos - firstPos) + 1);
                    }
                }
            }
            if (!showSquareBrackets)
            {
                std::string::size_type firstPos = (*itI)->title.find_first_of("[");
                std::string::size_type secondPos = (*itI)->title.find_first_of("]", firstPos);

                while (firstPos != std::string::npos && secondPos != std::string::npos)
                {
                    firstPos = (*itI)->title.find_first_of("[");
                    secondPos = (*itI)->title.find_first_of("]", firstPos);

                    if (firstPos != std::string::npos && secondPos != std::string::npos)
                    {
                        (*itI)->title.erase(firstPos, (secondPos - firstPos) + 1);
                    }
                }
            }
        }
    }

    return collection;
}



// Load a menu
CollectionInfo* RetroFE::getMenuCollection(std::string collectionName)
{
    std::string menuPath = Utils::combinePath(Configuration::absolutePath, "menu");
    std::string menuFile = Utils::combinePath(menuPath, collectionName + ".txt");
    std::vector<Item*> menuVector;
    CollectionInfoBuilder cib(config_, *metadb_);
    CollectionInfo* collection = new CollectionInfo(collectionName, menuPath, "", "", "");
    cib.ImportBasicList(collection, menuFile, menuVector);
    for (std::vector<Item*>::iterator it = menuVector.begin(); it != menuVector.end(); ++it)
    {
        (*it)->leaf = false;
        size_t position = (*it)->name.find("=");
        if (position != std::string::npos)
        {
            (*it)->ctrlType = Utils::trimEnds((*it)->name.substr(position + 1, (*it)->name.size() - 1));
            (*it)->name = Utils::trimEnds((*it)->name.substr(0, position));
            (*it)->title = (*it)->name;
            (*it)->fullTitle = (*it)->name;
            (*it)->leaf = true;
        }
        (*it)->collectionInfo = collection;
        collection->items.push_back(*it);
    }
    collection->playlists["all"] = &collection->items;
    return collection;
}


void RetroFE::saveRetroFEState()
{
    std::string file = Utils::combinePath(Configuration::absolutePath, "settings_saved.conf");
    Logger::write(Logger::ZONE_INFO, "RetroFE", "Saving settings_saved.conf");
    std::ofstream filestream;
    try
    {
        filestream.open(file.c_str());
        filestream << "firstPlaylist = " << firstPlaylist_ << std::endl;
        filestream.close();
    }
    catch (std::exception&)
    {
        Logger::write(Logger::ZONE_ERROR, "RetroFE", "Save failed: " + file);
    }
}

DWORD RetroFE::GetProcId(const wchar_t* procName)
{
    PROCESSENTRY32W procEntry;
    procEntry.dwSize = sizeof(procEntry);

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
    if (hSnap == INVALID_HANDLE_VALUE)
        return 0;

    if (Process32FirstW(hSnap, &procEntry))
    {
        do
        {
            if (std::wcscmp(procEntry.szExeFile, procName) == 0)
            {
                CloseHandle(hSnap);
                return procEntry.th32ProcessID;
            }
        } while (Process32NextW(hSnap, &procEntry));
    }

    CloseHandle(hSnap);
    return 0;
}

// Check if a custom game layout file exists
bool RetroFE::isLayoutActive(const std::string& content, const std::string& layoutFile)
{
    std::unique_ptr<rapidxml::xml_document<>> doc = std::make_unique<rapidxml::xml_document<>>();
    try {
        doc->parse<0>(const_cast<char*>(content.c_str()));
    }
    catch (rapidxml::parse_error& e) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_ERROR, "RetroFE", "Failed to parse XML in layout file: " + layoutFile + " - " + e.what());
#endif
        return false;
    }

    rapidxml::xml_node<>* root = doc->first_node("layout");
    if (!root) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Missing <layout> root node in: " + layoutFile);
#endif
        return false;
    }

    rapidxml::xml_attribute<>* activeAttr = root->first_attribute("active");
    if (activeAttr) {
        std::string rawValue = activeAttr->value();
        std::string activeValue = PageBuilder::substituteGlobalVariables(rawValue);
#ifdef _DEBUG
        Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Active attribute raw: " + rawValue + ", substituted: " + activeValue);
#endif
        if (Utils::toLower(activeValue) != "true") {
#ifdef _DEBUG
            Logger::write(Logger::ZONE_INFO, "RetroFE", "Layout is not active: " + activeValue);
#endif
            return false;
        }
    }
    else {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_DEBUG, "RetroFE", "No active attribute found in: " + layoutFile);
#endif
    }
#ifdef _DEBUG
    Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Layout exists and is active: " + layoutFile);

#endif
    return true;
}

bool RetroFE::checkGameLayoutExists(const std::string& layoutName, const std::string& collectionName, const std::string& gameName)
{
    std::string layoutPath = Utils::combinePath(Configuration::absolutePath, "layouts", layoutName, "collections", collectionName, "layout");
    std::string layoutFile = Utils::combinePath(layoutPath, gameName + ".xml");
#ifdef _DEBUG
    Logger::write(Logger::ZONE_INFO, "RetroFE", "Checking game layout file: " + layoutFile);
#endif

    std::ifstream file(layoutFile);
    if (!file.good()) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Layout file does not exist or cannot be opened: " + layoutFile);
#endif
        return false;
    }

    file.seekg(0, std::ios::end);
    if (file.tellg() == 0) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Layout file is empty: " + layoutFile);
#endif
        return false;
    }
    file.seekg(0, std::ios::beg);

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    return isLayoutActive(content, layoutFile);
}


// Create a temporary collection with only the selected game
CollectionInfo* RetroFE::createTempCollection(Item* gameItem, CollectionInfo* originalCollection)
{
    CollectionInfo* tempCollection = new CollectionInfo("TempGameCollection", "", "", "", "");
    tempCollection->items.push_back(gameItem);
    tempCollection->playlists["all"] = &tempCollection->items;
    tempCollection->originalCollection = originalCollection; // Store reference to original collection
    return tempCollection;
}

// Build a page for the game using its custom layout
Page* RetroFE::buildGamePage(CollectionInfo* tempCollection, const std::string& layoutName, const std::string& collectionName, const std::string& gameName)
{
    PageBuilder pb(layoutName, gameName, config_, &fontcache_);
    Page* page = pb.buildPage(collectionName);
    return page;
}
void RetroFE::buildMenuTree() {
    collectionParents_.clear();
    std::string collectionsDir = Utils::combinePath(Configuration::absolutePath, "collections");

    // Helper: read menu file (folder .txt or menu.txt)
    auto processMenuFile = [&](const std::string& menuFilePath, const std::string& parentCol) {
        std::ifstream menuFile(menuFilePath);
        if (!menuFile.is_open()) {
#ifdef _DEBUG
            Logger::write(Logger::ZONE_WARNING, "RetroFE", "Could not open menu file: " + menuFilePath);
#endif
            return;
        }

        std::string line;
        int lineNumber = 0;
        bool isEmpty = true;
        while (std::getline(menuFile, line)) {
            ++lineNumber;
            line = Utils::filterComments(line);
            line = Utils::trimEnds(line);
            if (!line.empty()) {
                isEmpty = false;
                std::string childCol = line;
                size_t pos = line.find("=");
                if (pos != std::string::npos)
                    childCol = Utils::trimEnds(line.substr(0, pos));

                if (childCol.empty()) continue;

                std::string childColPath = Utils::combinePath(collectionsDir, childCol);
                struct stat st;
                if (stat(childColPath.c_str(), &st) == 0 && (st.st_mode & S_IFDIR)) {
                    collectionParents_[childCol] = parentCol;
#ifdef _DEBUG
                    Logger::write(Logger::ZONE_DEBUG, "RetroFE",
                        "Mapped collection '" + childCol + "' to parent '" + parentCol +
                        "' from " + menuFilePath + " (line " + std::to_string(lineNumber) + ")");
#endif
                }
            }
        }
        menuFile.close();

        // If empty, also scan sub menu folder (legacy behavior)
        if (isEmpty) {
            std::string subMenuDir = Utils::combinePath(collectionsDir, parentCol, "menu");
            DIR* subDp = opendir(subMenuDir.c_str());
            if (subDp) {
                struct dirent* subDirp;
                while ((subDirp = readdir(subDp)) != NULL) {
                    std::string subFile = subDirp->d_name;
                    size_t subPos = subFile.find_last_of(".");
                    std::string subExt = (subPos != std::string::npos) ? subFile.substr(subPos) : "";
                    if (subExt == ".txt") {
                        std::string childCol = subFile.substr(0, subPos);
                        std::string childColPath = Utils::combinePath(collectionsDir, childCol);
                        struct stat st;
                        if (stat(childColPath.c_str(), &st) == 0 && (st.st_mode & S_IFDIR)) {
                            collectionParents_[childCol] = parentCol;
#ifdef _DEBUG
                            Logger::write(Logger::ZONE_DEBUG, "RetroFE",
                                "Mapped collection '" + childCol + "' to parent '" + parentCol +
                                "' from submenu file " + subFile);
#endif
                        }
                    }
                }
                closedir(subDp);
            }
        }
        };

    // Main scanning loop
    DIR* dp = opendir(collectionsDir.c_str());
    if (dp) {
        struct dirent* dirp;
        while ((dirp = readdir(dp)) != NULL) {
            std::string colName = dirp->d_name;
            if (colName == "." || colName == ".." || dirp->d_type != DT_DIR)
                continue;

            std::string menuDir = Utils::combinePath(collectionsDir, colName, "menu");
            std::string menuTxt = Utils::combinePath(collectionsDir, colName, "menu.txt"); // NEW

            // Priority: folder first, then single menu.txt file
            DIR* menuDp = opendir(menuDir.c_str());
            if (menuDp) {
                // Case 1: traditional menu/ folder
                std::vector<std::string> menuFiles;
                struct dirent* menuDirp;
                while ((menuDirp = readdir(menuDp)) != NULL) {
                    std::string file = menuDirp->d_name;
                    size_t pos = file.find_last_of(".");
                    std::string ext = (pos != std::string::npos) ? file.substr(pos) : "";
                    if (ext == ".txt")
                        menuFiles.push_back(file);
                }
                std::sort(menuFiles.begin(), menuFiles.end());
                for (const auto& file : menuFiles) {
                    std::string menuFilePath = Utils::combinePath(menuDir, file);
                    processMenuFile(menuFilePath, colName);
                }
                closedir(menuDp);
            }
            else if (Utils::fileExists(menuTxt)) {
                // Case 2: single menu.txt file (NEW)
#ifdef _DEBUG
                Logger::write(Logger::ZONE_DEBUG, "RetroFE",
                    "Found single menu.txt for collection: " + colName);
#endif
                processMenuFile(menuTxt, colName);
            }
            else {
#ifdef _DEBUG
                Logger::write(Logger::ZONE_DEBUG, "RetroFE",
                    "No menu or menu.txt found for collection: " + colName);
#endif
            }
        }
        closedir(dp);
    }
    else {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_ERROR, "RetroFE",
            "Could not open collections directory: " + collectionsDir);
#endif
    }

    // Log result
#ifdef _DEBUG
    Logger::write(Logger::ZONE_INFO, "RetroFE", "Collection parent mappings:");
    if (collectionParents_.empty()) {
        Logger::write(Logger::ZONE_WARNING, "RetroFE",
            "No collection parent mappings created");
    }
    else {
        for (const auto& pair : collectionParents_) {
            Logger::write(Logger::ZONE_INFO, "RetroFE",
                "  " + pair.first + " -> " + pair.second);
        }
    }
#endif
}


bool RetroFE::navigateToCollection(const std::string& targetCollection, bool autoEnter) {
#ifdef _DEBUG
    Logger::write(Logger::ZONE_INFO, "RetroFE", "Navigating to collection: " + targetCollection);
#endif

    // Rebuild menu tree
    buildMenuTree();

    // Build path to target collection using map
    std::vector<std::string> path;
    std::string current = targetCollection;
    int maxDepth = 10;
    bool foundParent = false;
    while (!current.empty() && current != "Main" && maxDepth-- > 0) {
        auto it = collectionParents_.find(current);
        if (it != collectionParents_.end()) {
            path.insert(path.begin(), current);
            current = it->second;
            foundParent = true;
        }
        else {
            // Case-insensitive lookup
            for (const auto& pair : collectionParents_) {
                if (Utils::toLower(pair.first) == Utils::toLower(current)) {
                    path.insert(path.begin(), pair.first); // Use original case
                    current = pair.second;
                    foundParent = true;
                    break;
                }
            }
            if (!foundParent) break;
        }
    }

    // Fallback: Search Main/menu files and their subdirectories for the target collection
    if (!foundParent && targetCollection != "Main") {
        std::string mainMenuDir = Utils::combinePath(Configuration::absolutePath, "collections", "Main", "menu");
        DIR* dp = opendir(mainMenuDir.c_str());
        if (dp) {
            struct dirent* dirp;
            while ((dirp = readdir(dp)) != NULL) {
                std::string file = dirp->d_name;
                size_t pos = file.find_last_of(".");
                std::string ext = (pos != std::string::npos) ? file.substr(pos) : "";
                if (ext == ".txt") {
                    std::string intermediate = file.substr(0, pos); // e.g., "ARCADES"
                    std::string menuFilePath = Utils::combinePath(mainMenuDir, file);
                    std::ifstream menuFile(menuFilePath);
                    bool isEmpty = true;
                    if (menuFile.is_open()) {
                        std::string line;
                        while (std::getline(menuFile, line)) {
#ifdef _DEBUG
                            Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Raw line in " + menuFilePath + ": '" + line + "'");
#endif
                            line = Utils::filterComments(line);
                            line = Utils::trimEnds(line);
                            if (!line.empty()) {
                                isEmpty = false;
                                std::string childCol = line;
                                size_t eqPos = line.find("=");
                                if (eqPos != std::string::npos) {
                                    childCol = Utils::trimEnds(line.substr(0, eqPos));
                                }
                                if (Utils::toLower(childCol) == Utils::toLower(targetCollection)) {
                                    path = { intermediate, targetCollection };
#ifdef _DEBUG
                                    Logger::write(Logger::ZONE_INFO, "RetroFE", "Found '" + targetCollection + "' in menu file '" + file + "'; path: Main -> " + intermediate + " -> " + targetCollection);
#endif
                                    foundParent = true;
                                    break;
                                }
                            }
                        }
                        menuFile.close();
                    }
                    // If the menu file is empty, check the corresponding collection's menu directory
                    if (isEmpty && !foundParent) {
                        std::string subMenuDir = Utils::combinePath(Configuration::absolutePath, "collections", intermediate, "menu");
                        DIR* subDp = opendir(subMenuDir.c_str());
                        if (subDp) {
                            struct dirent* subDirp;
                            while ((subDirp = readdir(subDp)) != NULL) {
                                std::string subFile = subDirp->d_name;
                                size_t subPos = subFile.find_last_of(".");
                                std::string subExt = (subPos != std::string::npos) ? subFile.substr(subPos) : "";
                                if (subExt == ".txt") {
                                    std::string subCol = subFile.substr(0, subPos);
#ifdef _DEBUG
                                    Logger::write(Logger::ZONE_DEBUG, "RetroFE", "Checking submenu file '" + subFile + "' in " + subMenuDir);
#endif
                                    if (Utils::toLower(subCol) == Utils::toLower(targetCollection)) {
                                        path = { intermediate, targetCollection };
#ifdef _DEBUG
                                        Logger::write(Logger::ZONE_INFO, "RetroFE", "Found '" + targetCollection + "' in submenu file '" + subFile + "' under '" + intermediate + "'; path: Main -> " + intermediate + " -> " + targetCollection);
#endif
                                        foundParent = true;
                                        break;
                                    }
                                }
                            }
                            closedir(subDp);
                            if (foundParent) break;
                        }
                        else {
#ifdef _DEBUG
                            Logger::write(Logger::ZONE_DEBUG, "RetroFE", "No menu directory found for collection: " + intermediate);
#endif // _DEBUG
                        }
                    }
                    if (foundParent) break;
                }
            }
            closedir(dp);
        }
        if (!foundParent) {
            // Check if target collection exists as a directory
            std::string colPath = Utils::combinePath(Configuration::absolutePath, "collections", targetCollection);
            struct stat st;
            if (stat(colPath.c_str(), &st) == 0 && (st.st_mode & S_IFDIR)) {
#ifdef _DEBUG
                Logger::write(Logger::ZONE_INFO, "RetroFE", "Target collection '" + targetCollection + "' not in menu; treating as top-level");
#endif // _DEBUG
                path.push_back(targetCollection);
                foundParent = true;
            }
            else {
#ifdef _DEBUG
                Logger::write(Logger::ZONE_ERROR, "RetroFE", "Cannot find '" + targetCollection + "' in any Main menu file or submenu; staying in Main");
#endif // _DEBUG
                return true;
            }
        }
    }

    // Log the path
    std::string pathStr = path.empty() ? "Main" : std::accumulate(path.begin(), path.end(), std::string("Main"),
        [](const std::string& a, const std::string& b) { return a + " -> " + b; });
#ifdef _DEBUG
    Logger::write(Logger::ZONE_INFO, "RetroFE", "Navigation path: " + pathStr);
#endif // _DEBUG

    // Load Main collection
    CollectionInfo* mainInfo = getCollection("Main");
    if (!mainInfo) {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_ERROR, "RetroFE", "Failed to load Main collection");
#endif // _DEBUG
        return false;
    }

    // Clear menu stack and push Main
    while (currentPage_->getMenuDepth() > 0) {
        currentPage_->popCollection();
    }
    currentPage_->pushCollection(mainInfo);
    config_.setProperty("currentCollection", "Main");

    // Select initial playlist
    std::string firstPlaylist = "all";
    config_.getProperty("firstPlaylist", firstPlaylist);
    currentPage_->selectPlaylist(firstPlaylist);
    if (currentPage_->getPlaylistName() != firstPlaylist) {
        currentPage_->selectPlaylist("all");
    }
    currentPage_->onNewItemSelected();
    currentPage_->reallocateMenuSpritePoints();

    // Populate startup queue
    startupEnterQueue_.clear();
    startupEnterQueue_.assign(path.begin(), path.end());
    startupSelectItem_.clear();

    // If not auto-entering, select the last collection without entering
    if (!autoEnter && !startupEnterQueue_.empty()) {
        startupSelectItem_ = startupEnterQueue_.back();
        startupEnterQueue_.pop_back();
#ifdef _DEBUG
        Logger::write(Logger::ZONE_INFO, "RetroFE", "Selecting collection without entering: " + startupSelectItem_);
#endif // _DEBUG
    }

    // Temporarily disable auto-entry if queue is not empty
    enterCollectionAfterReload_ = false;
    enterNumberOfTimes_ = 0;

    // Initialize page
    currentPage_->allocateGraphicsMemory();
    currentPage_->start();

    // Log queue state
    std::string queueStr = std::accumulate(startupEnterQueue_.begin(), startupEnterQueue_.end(), std::string(),
        [](const std::string& a, const std::string& b) { return a.empty() ? b : a + ", " + b; });
    Logger::write(Logger::ZONE_INFO, "RetroFE", "Startup enter queue: [" + (queueStr.empty() ? "empty" : queueStr) + "]");
    Logger::write(Logger::ZONE_INFO, "RetroFE", "Startup select item: " + (startupSelectItem_.empty() ? "none" : startupSelectItem_));

    return true;
}