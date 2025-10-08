#pragma once
#include "Component.h"
#include <SDL.h>
#include <SDL_image.h>
#include <string>
#include <unordered_map>

class Page;

class AnimatedImage : public Component {
public:
    AnimatedImage(std::string file, std::string altFile, Page& p, int monitor, bool isAlwaysAnimated = false, std::string frameLoopType = "noPlay", int speed = 7500,
                  bool random = false, bool slideShow = false, int slideShowTimer = 5000, int slideNumber = 5, std::string randomSrc = "", std::string altRandomSrc = "");
    virtual ~AnimatedImage();
    void freeGraphicsMemory() override;
    void allocateGraphicsMemory() override;
    void draw() override;
    void triggerEvent(std::string event, int menuIndex = -1) override;
    void setSelected(bool selected) { isSelected_ = selected; }
    bool isSelected() const { return isSelected_; }

private:
    IMG_Animation* anim_;
    SDL_Texture** texture_;
    std::string file_;
    std::string altFile_;
    bool isPlaying_;
    Uint32 startTime_;
    int currentFrame_;
    int frameCount_;
    int speed_;
    int once_;
    bool isAlwaysAnimated_;
    bool finishedLoopOnce_;
    std::string frameLoopType_;
	int lastMenuIndex_;                // reset to -1 when switching menu
    bool useOriginalDelays_;         // Flag to use original frame delays
    std::vector<int> playbackDelays_; // Delays in playback order
    Uint32 nextFrameTime_;           // Time to switch to next frame when using original delays

    void setPlaying();
    void setSpeed(int frameCount, int speed =7500);
    bool isSelected_ = false;
    static std::unordered_map<std::string, std::pair<IMG_Animation*, int>> animationCache;
    bool isAnimationReady_;

protected:

    bool random_;
    bool slideShow_;
    int slideShowTimer_;
    int slideNumber_;
    std::string randomSrc_;
    std::string altRandomSrc_;
    std::vector<std::string> imageList_;
    size_t currentImageIndex_;
    Uint32 lastSwitchTime_;

    void loadImageList();
    void switchImage();

};