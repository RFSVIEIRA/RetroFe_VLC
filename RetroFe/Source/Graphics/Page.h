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

#include "../Collection/CollectionInfo.h"

#include <map>
#include <string>
#include <list>
#include <vector>
#include "../Database/MetadataDatabase.h"
#include "ViewInfo.h"
#include "SearchConfig.h"
#include <set>

class MetadataDatabase;
class Component;
class Configuration;
class ScrollingList;
class Text;
class Item;
class Sound;

class Page
{
public:
    enum ScrollDirection
    {
        ScrollDirectionForward,
        ScrollDirectionBack,
        ScrollDirectionIdle

    };

    struct LoadingElement
    {
        enum Type { Container, Image, Text };
        Type        type        = Image;
        std::string src         = "";
        std::string text        = "Loading";
        std::string font        = "";
        int         fontSize    = 0;
        SDL_Color   color       = { 255, 255, 255, 255 };
        float       alpha       = 1.0f;
        std::string xStr        = "center";
        std::string yStr        = "center";
        std::string xOffsetStr  = "0";
        std::string yOffsetStr  = "0";
        std::string xOriginStr  = "";
        std::string yOriginStr  = "";
        std::string wStr        = "-1";
        std::string hStr        = "-1";
        std::string scaleMode   = "fit";
        int         layer       = 0;
        int         monitor     = 0;
    };

    struct LoadingConfig
    {
        std::string                 layoutDir;
        std::vector<LoadingElement> elements;
        bool hasCustomElements() const { return !elements.empty(); }
    };

    static SDL_Color     parseColor(const std::string& hex, SDL_Color defaultColor = { 255, 255, 255, 255 });
    void                 setLoadingLayoutDir(const std::string& dir) { loadingConfig_.layoutDir = dir; }
    void                 addLoadingElement(const LoadingElement& el) { loadingConfig_.elements.push_back(el); }
    void                 setLoadingConfig(const LoadingConfig& config) { loadingConfig_ = config; }
    const LoadingConfig& getLoadingConfig() const { return loadingConfig_; }
    void setSearchConfig(const SearchConfig& cfg) { searchConfig_ = cfg; }
    const SearchConfig& getSearchConfig() const { return searchConfig_; }
    SearchConfig& getSearchConfig() { return searchConfig_; }

    void calculateScalingFactors();
    float getScaleX(int monitor) const;
    float getScaleY(int monitor) const;
    float getOffsetX(int monitor) const;
    float getOffsetY(int monitor) const;



    Page(Configuration &c, int layoutWidth, int layoutHeight );
    virtual ~Page();
    void deInitialize();
    virtual void onNewItemSelected();
    virtual void onNewScrollItemSelected();
    void highlightLoadArt();
    void refreshBadges();
    bool pushCollection(CollectionInfo *collection);
    bool pushSearchCollection(CollectionInfo *collection, unsigned int selectedIndex = 0);
    bool popSearchCollection(unsigned int targetDepth = 0);
    bool isSearchActive() const { return searchBackup_.active; }
    std::string getSearchSavedRootItem() const { return searchBackup_.savedRootItem; }
    unsigned int getSearchSavedRootScroll() const { return searchBackup_.savedRootScroll; }
    std::string getSearchSavedCollectionName() const { return searchBackup_.savedCollectionName; }
    bool popCollection();
    void enterMenu(int depth = -1);
    void exitMenu(int depth = -1);
    void enterGame();
    void exitGame();
    std::string getPlaylistName();
    void favPlaylist();
    void nextPlaylist();
    void prevPlaylist();
    void selectPlaylist(std::string playlist);
    void nextCyclePlaylist(std::vector<std::string> list);
    void prevCyclePlaylist(std::vector<std::string> list);
    void pushMenu(ScrollingList *s, int index = -1);
    bool isMenusFull();
    //void setLoadSound(Sound* chunk);
    //void setUnloadSound(Sound* chunk);
    //void setHighlightSound(Sound* chunk);
    //void setSelectSound(Sound* chunk);
    void addSound(const std::string& type, Sound* sound);

    bool addComponent(Component *c);
    void pageScroll(ScrollDirection direction);
    void letterScroll(ScrollDirection direction);
    void subScroll(ScrollDirection direction);
    void cfwLetterSubScroll(ScrollDirection direction);
    size_t getCollectionSize();
    unsigned int getSelectedIndex();
    void selectRandom();
    void start();
    void stop();
    void setScrolling(ScrollDirection direction);
    bool isHorizontalScroll();
    unsigned int getMenuDepth();
    Item *getSelectedItem();
    Item *getSelectedItem(int offset);
    void removeSelectedItem();
    void setScrollOffsetIndex(unsigned int i);
    unsigned int getScrollOffsetIndex();
    bool isIdle();
    bool isAttractIdle();
    bool isGraphicsIdle();
    bool isMenuIdle();
    bool isAutoNavigating() const { return isAutoNavigating_; }
    void setAutoNavigating(bool val) { isAutoNavigating_ = val; }
    void completeAnimations(int maxSteps = 40, float dtStep = 0.1f);
    void setStatusTextComponent(Text *t);
    void update(float dt, bool skipPrimary = false, int monitorFilter = -1);
    void cleanup();
    void draw(int monitor = -1);
    void freeGraphicsMemory(bool freeAll = true);
    void allocateGraphicsMemory(bool allocSecondary = true);
    void deInitializeFonts( );
    void initializeFonts( );
    void playSelect();
    bool isSelectPlaying();
    std::string getCollectionName();
    CollectionInfo *getCollection();
    void  setMinShowTime(float value);
    float getMinShowTime();
    void  menuScroll();
    void  highlightEnter();
    void  highlightExit();
    void  playlistEnter();
    void  playlistExit();
    void  menuJumpEnter();
    void  menuJumpExit();
    void  attractEnter( );
    void  attract( );
    void  attractExit( );
    void  jukeboxJump( );
    void  triggerEvent( std::string action );
    void  setText( std::string text, int id );
    void  addPlaylist();
    void  removePlaylist();
    void  togglePlaylist();
   // void  updateLastPlayedPlaylist( Item *item );

    void  reallocateMenuSpritePoints();
    bool  isMenuScrolling();
    bool  isPlaying();
    void  resetScrollPeriod();
    void  updateScrollPeriod();
    void  scroll(bool forward);
    bool  hasSubs();
    int   getLayoutWidth(int monitor);
    int   getLayoutHeight(int monitor);
    void  setLayoutWidth( int monitor, int width);
    void  setLayoutHeight(int monitor, int height);
    void  setJukebox();
    bool  isJukebox();
    bool  isJukeboxPlaying();
    void  skipForward( );
    void  skipBackward( );
    void  skipForwardp( );
    void  skipBackwardp( );
    // Volume control
    void adjustVolume(float delta);
    void toggleMute();
    bool isMuted() ;
    float getVolume() ;

    void  pause( );
    void  restart( );
    unsigned long long getCurrent( );
    unsigned long long getDuration( );
    bool  isPaused( );
    void setMetadataDatabase(MetadataDatabase* metadb) { metadb_ = metadb; }
    MetadataDatabase* getMetadataDatabase() { return metadb_; }
    
  void setLayoutScaleMode(ViewInfo::ScaleMode mode) {
        layoutScaleMode_ = mode;
    }
    ViewInfo::ScaleMode getLayoutScaleMode() const {
        return layoutScaleMode_;
    }
    void setLayoutScaleFactor(float factor);

    bool isGroupActive(const std::string& group) const { return activeGroups.count(group) > 0; }
    bool isSubgroupActive(const std::string& subgroup) const { return activeSubgroups.count(subgroup) > 0; }
    void setActiveName(const std::string& subgroup, const std::string& name) { activeNames[subgroup] = name; }
    void setGroupActive(const std::string& group, bool active);
    void setSubgroupActive(const std::string& subgroup, bool active);
   
    void initializeActiveStates();
    bool isSharedActive(const std::string& shared) const;
    void setSharedActive(const std::string& shared, bool active);
  
    ScrollingList* getActiveMenu(); // Get the first active menu (typically there's only one)
    const std::vector<ScrollingList*>& getActiveMenus() const { return activeMenu_; }
    void setActiveMenuSelectedIndex(unsigned int index); // Set the selected index for the active menu
    std::string getCollectionNameAtDepth(unsigned int depth) const;
    unsigned int getSelectedIndexAtDepth(unsigned int depth) const;
    ScrollingList* getMenuAtDepth(unsigned int depth);
    std::vector<Item*>* getCurrentPlaylist();
    void selectItem(Item* item);

    bool isMusicEndRandom() const;
  

    void selectNext();
    void selectPrevious();
    // Jukebox queued playlists
    void addToQueued(int queueNum);
    void callQueued(int queueNum);
    void removeFromCurrentQueue();
    void clearCurrentQueue();
    bool isCurrentPlaylistQueued() const;
    std::string getCurrentPlaylistName() const;
    void moveInCurrentPlaylist(bool up);   // true = move up, false = move down
	void RefreshCurrentPlaylist(size_t oldIdx = 999999, bool movedUp = false);
    void ForceFullRefreshCurrentPlaylist();
    // Shuffle playback
    void toggleShuffle();
    bool isShuffleEnabled() const;
    void setShuffleEnabled(bool enabled);
    void selectCurrent();
    void toggleRepeatLoop();
    bool isRepeatLoopEnabled() const;

    bool hasComponentOnLayer(int layer) const;
private:

    bool isAutoNavigating_ = false;
    void playlistChange();
    std::string collectionName_;
    Configuration &config_;

    struct MenuInfo_S
    {
        CollectionInfo *collection;
        CollectionInfo::Playlists_T::iterator playlist; 
        bool queueDelete;
    };

    typedef std::vector< std::vector<ScrollingList *> > MenuVector_T;
    typedef std::list<MenuInfo_S> CollectionVector_T;

    std::vector<ScrollingList *> activeMenu_;
    unsigned int menuDepth_;
    MenuVector_T menus_;
    CollectionVector_T collections_;
    CollectionVector_T deleteCollections_;

    struct SearchBackupState {
        struct SavedMenuState {
            std::vector<Item*>* items = nullptr;
            std::string collectionName;
            unsigned int selectedIndex = 0;
            ViewInfo baseViewInfo;
        };
        bool active = false;
        unsigned int savedMenuDepth = 0;
        CollectionVector_T savedCollections;
        std::string savedCollectionName;
        CollectionInfo::Playlists_T::iterator savedPlaylist;
        std::vector<SavedMenuState> savedGamelistMenus;
        std::vector<ViewInfo> savedComponentViewInfo;
        std::string savedRootItem;
        unsigned int savedRootScroll = static_cast<unsigned int>(-1);
    };
    SearchBackupState searchBackup_;

    static const unsigned int NUM_LAYERS = 21;
    std::vector<Component *> LayerComponents;
    std::list<ScrollingList *> deleteMenuList_;
    std::list<CollectionInfo *> deleteCollectionList_;

    bool scrollActive_;

    Item *selectedItem_;
    Text *textStatusComponent_;

    std::map<std::string, std::vector<Sound*>> soundsByType_;
    void playSound(const std::string& type);


    float minShowTime_;
    float elapsedTime_;
    CollectionInfo::Playlists_T::iterator playlist_;
    std::vector<int> layoutWidth_;
    std::vector<int> layoutHeight_;
    bool jukebox_;
    MetadataDatabase* metadb_;

    ViewInfo::ScaleMode layoutScaleMode_; //  existing ScaleMode enum
    float layoutScaleFactor_;
    std::vector<float> scaleX_;
    std::vector<float> scaleY_;
    std::vector<float> offsetX_;
    std::vector<float> offsetY_;


    std::set<std::string> activeGroups;
    std::set<std::string> activeSubgroups;
    std::map<std::string, std::string> activeNames;
    std::set<std::string> activeShared_;

   /* Sound* loadSoundChunk_;
    Sound* unloadSoundChunk_;
    Sound* highlightSoundChunk_;
    Sound* selectSoundChunk_;*/

    float currentVolume_ = 1.0f;     
    bool isMuted_ = false;
    bool musicEndRandom_ = true;
	bool repeatEnabled_ = false;
    LoadingConfig loadingConfig_;
    SearchConfig searchConfig_;

};
