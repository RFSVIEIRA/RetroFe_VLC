#ifndef OPTIONS_MENU_H
#define OPTIONS_MENU_H

#include "MenuSection.h"
#include "SettingsMenu.h"
#include "ControlsMenu.h"
#include "ActionsMenu.h"
#include "LayoutMenu.h"
#include "OptionsMenuBuilder.h"
#include "../database/Configuration.h"
#include "../graphics/FontCache.h"
#include <SDL.h>
#include <vector>
#include <string>

class OptionsMenu {
public:
    OptionsMenu(Configuration& config);
    ~OptionsMenu();
    bool run();
    bool show();
    bool settingsChanged() const { return settingsChanged_; }
    struct ImageInstance {
        bool isAnimated;
        bool isMenu = false;
        SDL_Texture* texture;
        SDL_Texture** frames;
        int frameCount;
        int currentFrame;
        Uint32 lastFrameTime;
        std::vector<Uint32> frameDelays;
        SDL_Rect dest;
        std::string scale;
        OptionsMenuBuilder::XAlignment xAlign = OptionsMenuBuilder::XAlignment::Numeric;
        OptionsMenuBuilder::YAlignment yAlign = OptionsMenuBuilder::YAlignment::Numeric;
        int xOffset = 0;
        int yOffset = 0;
    };

private:
    enum class MenuState { MAIN_MENU, SETTINGS_MENU, CONTROLS_MENU, ACTIONS_MENU, LAYOUT_MENU };

   

    void initMainOptions();
    void handleInput(SDL_Event& event);
    void render();

    bool initializedSDL_ = false;
    Configuration& config_;
    OptionsMenuBuilder menuConfig_;
    SDL_Window* menuWindow_;
    SDL_Renderer* menuRenderer_;
    Font* titleFont_;
    Font* optionFont_;
    std::vector<ImageInstance> images_;
    MenuState state_ = MenuState::MAIN_MENU;
    size_t selectedOption_ = 0;
    int windowWidth_ = 800;
    int windowHeight_ = 600;
    bool running_ = false;
    bool settingsChanged_ = false;
    SDL_Color backgroundColor_ = { 0, 0, 0, 255 };
    std::vector<std::string> mainOptions_;
    std::vector<SDL_Rect> optionRects_;
    SettingsMenu settingsMenu_;
    ControlsMenu controlsMenu_;
    ActionsMenu actionsMenu_;
    LayoutMenu layoutMenu_;
    MenuSection* currentSection_;
    int originalWindowWidth_; 
    int originalWindowHeight_; 
    float scaleFactor_;
};

#endif 