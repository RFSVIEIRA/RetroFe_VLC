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
#include "../Video/LibVLCVideo.h"
#include "../Database/MetadataDatabase.h"
#include "ViewInfo.h"
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
    bool pushCollection(CollectionInfo *collection);
    bool popCollection();
    void enterMenu();
    void exitMenu();
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
    void setStatusTextComponent(Text *t);
    void update(float dt);
    void cleanup();
    void draw();
    void freeGraphicsMemory();
    void allocateGraphicsMemory();
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
    void setActiveMenuSelectedIndex(unsigned int index); // Set the selected index for the active menu
    std::string getCollectionNameAtDepth(unsigned int depth) const;
    unsigned int getSelectedIndexAtDepth(unsigned int depth) const;
    ScrollingList* getMenuAtDepth(unsigned int depth);
    std::vector<Item*>* getCurrentPlaylist();
    void selectItem(Item* item);
private:

    void playlistChange();
    std::string collectionName_;
    Configuration &config_;
    LibVLCVideo libVLCInstance;

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

    static const unsigned int NUM_LAYERS = 20;
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

};
