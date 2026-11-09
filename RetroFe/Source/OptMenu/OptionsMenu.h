/* This file is part of RetroFeVLC.
 *
 * RetroFeVLC is free software: Fork of RetroFE you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * RetroFE / RetroFeVLC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with RetroFE.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef OPTIONS_MENU_H
#define OPTIONS_MENU_H

#include "MenuSection.h"
#include "LayoutMenu.h"
#include "OptionsMenuBuilder.h"
#include "../database/Configuration.h"
#include "../graphics/FontCache.h"
#include <SDL.h>
#include <vector>
#include <string>
#include <functional>

class Page;
class UserInput;

class OptionsMenu {
public:
    OptionsMenu(Configuration& config, Page* page = nullptr);
    ~OptionsMenu();

    bool open(SDL_Renderer* renderer = nullptr);
    void close();
    bool isActive() const { return running_; }
    void handleEvent(SDL_Event& event, UserInput* input = nullptr);
    void draw(SDL_Renderer* renderer);

    bool show(SDL_Renderer* renderer = nullptr, std::function<void()> renderBase = nullptr, UserInput* input = nullptr);

    bool settingsChanged() const { return settingsChanged_; }
    bool hasSavedChanges() const { return savedChanges_; }
    bool isQuitSelected() const { return quitSelected_; }
    bool isKioskModeSelected() const { return kioskModeSelected_; }
    bool isSignOutSelected() const { return signOutSelected_; }
    bool isRestartSelected() const { return restartSelected_; }
    bool isShutDownSelected() const { return shutDownSelected_; }

    struct ImageInstance {
        bool isAnimated = false;
        bool isMenu = false;
        SDL_Texture* texture = nullptr;
        SDL_Texture** frames = nullptr;
        int frameCount = 0;
        int currentFrame = 0;
        Uint32 lastFrameTime = 0;
        std::vector<Uint32> frameDelays;
        SDL_Rect dest = { 0, 0, 0, 0 };
        std::string scale = "stretch";
        OptionsMenuBuilder::XAlignment xAlign = OptionsMenuBuilder::XAlignment::Numeric;
        OptionsMenuBuilder::YAlignment yAlign = OptionsMenuBuilder::YAlignment::Numeric;
        int xOffset = 0;
        int yOffset = 0;
        float xOrigin = 0.0f;
        float yOrigin = 0.0f;
        bool hasExplicitXOrigin = false;
        bool hasExplicitYOrigin = false;
        float alpha = 1.0f;
        int layer = 0;
    };

private:
    void initMainOptions();
    void handleInput(SDL_Event& event, UserInput* input = nullptr);
    bool isKioskModeActive() const;
    static std::string findConfigPath(const std::string& layoutDir);

    Configuration& config_;
    Page* page_;
    OptionsMenuBuilder menuConfig_;
    Font* titleFont_;
    Font* optionFont_;
    std::vector<ImageInstance> images_;
    size_t selectedOption_ = 0;
    int windowWidth_ = 1920;
    int windowHeight_ = 1080;
    bool running_ = false;
    bool settingsChanged_ = false;
    bool savedChanges_ = false;
    bool quitSelected_ = false;
    bool kioskModeSelected_ = false;
    bool signOutSelected_ = false;
    bool restartSelected_ = false;
    bool shutDownSelected_ = false;
    struct MainOptionItem {
        std::string displayText;
        std::string menuId;
        std::string iconPath;
        SDL_Texture* iconTexture = nullptr;
        int iconW = 0;
        int iconH = 0;
        enum class Type {
            MenuSection,
            SaveExit,
            SignOut,
            Restart,
            ShutDown,
            Quit
        } type = Type::MenuSection;
    };
    std::vector<MainOptionItem> mainOptionItems_;
    std::vector<std::string> mainOptions_;
    std::vector<SDL_Rect> optionRects_;
    LayoutMenu layoutMenu_;
    MenuSection* currentSection_ = nullptr;
    int originalWindowWidth_ = 1920;
    int originalWindowHeight_ = 1080;
    float scaleFactor_ = 1.0f;
    Uint32 openTicks_ = 0;

    std::map<int, SDL_Texture*> discs_;
    SDL_Texture* discTexture(SDL_Renderer* renderer, int radius);
    void rrect(SDL_Renderer* renderer, int x, int y, int w, int h, int radius, SDL_Color c, int alpha = 255);
    void glow(SDL_Renderer* renderer, int x, int y, int w, int h, int radius, SDL_Color c, int strength);

    std::map<std::string, SDL_Texture*> iconCache_;
    std::map<std::string, SDL_Texture*> buttonTexCache_;
    std::map<std::string, std::pair<int, int>> buttonTexSize_;
    SDL_Texture* getButtonTexture(SDL_Renderer* renderer, const std::string& key, int& outW, int& outH);
    void drawFooterHints(SDL_Renderer* renderer, int modalX, int modalY, int modalW, int modalH);
};

#endif
