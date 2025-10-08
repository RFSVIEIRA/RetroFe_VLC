#pragma once
#include "Component.h"
#include "ReloadableText.h"
#include "../../Video/IVideo.h"
#include "../../Collection/Item.h"
#include <SDL.h>
#include <string>
#include "AnimatedImageBuilder.h"
#include "ImageBuilder.h"
#include "../../Graphics/Component/VideoBuilder.h"

class Image;

class ReloadableMedia : public Component
{
public:
    ReloadableMedia(Configuration& config, bool systemMode, bool layoutMode, bool commonMode, bool menuMode, std::string type, std::string imageType, Page& page, int displayOffset, bool isVideo, Font* font, bool jukebox, int jukeboxNumLoops,
        bool isAlwaysAnimated = true, std::string frameLoop = "noPlay", int speed = 7500, bool random = false, bool slideShow = false, Uint32 slideShowTimer = 2000);
    virtual ~ReloadableMedia();
    void update(float dt);
    void draw();
    void freeGraphicsMemory();
    void allocateGraphicsMemory();
    Component* findComponent(std::string collection, std::string type, std::string basename, std::string filepath, bool systemMode, bool isVideo);

    void enableTextFallback_(bool value);
    virtual bool isJukeboxPlaying();
    virtual void skipForward();
    virtual void skipBackward();
    virtual void skipForwardp();
    virtual void skipBackwardp();
    virtual void pause();
    virtual void restart();
    virtual unsigned long long getCurrent();
    virtual unsigned long long getDuration();
    virtual bool isPaused();

    // Support for random same game if reloadable media is true for image and video
    static Item* currentSelectedItem;
    static std::string currentSelectedSystem;
    static std::string currentSelectedGame;
    static bool randomSyncMedia;

    // Instance-specific slideshow variables
    std::vector<std::pair<std::string, std::string>> slideCandidates_; 
    int currentSlideIndex_;                                
    float elapsedTime_;                                       
    Uint32 lastUpdateTime_;                                  
    
    // Shared slideshow variables for synchronization
    static std::vector<std::pair<std::string, std::string>> sharedSlideCandidates_;
    static int sharedCurrentSlideIndex_;
    static Uint32 sharedLastSlideChangeTime_;
    static bool randomSyncSlide;
    static bool isSlideshowInitialized;

private:
    void reloadTexture();
    void buildSlideshowCandidates(const std::string& collectionPath, int maxSubCollections, int maxRandomMedia, Item* selectedItem);
    void buildSynchronizedCandidates(const std::string& collectionPath, int maxSubCollections, int maxRandomMedia, Item* selectedItem);
    void buildUnsyncedRandomCandidate(const std::string& collectionPath, int maxSubCollections, int maxRandomMedia, Item* selectedItem);
    void buildSharedSlideshowCandidates(const std::string& collectionPath, int maxSubCollections, int maxRandomMedia, Item* selectedItem); 
    Configuration& config_;
    bool systemMode_;
    bool layoutMode_;
    bool commonMode_;
    bool menuMode_;
    Component* loadedComponent_;
    Component* loadedCounterpart_ = nullptr;
    IVideo* videoInst_;
    bool isVideo_;
    Font* FfntInst_;
    bool textFallback_;
    std::string type_;
    std::string currentCollection_;
    Page* page_;
    int displayOffset_;
    std::string imageType_;
    bool jukebox_;
    int  jukeboxNumLoops_;
    bool isAlwaysAnimated_;
    std::string frameLoop_;
    int speed_;
    bool random_;
    bool slideshow_;
    Uint32 slideShowTimer_;
    int lastKnownSlideIndex_;

    VideoBuilder videoBuild;
    ImageBuilder imageBuild;
    AnimatedImageBuilder animatedImageBuild;


};