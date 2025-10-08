#ifndef CONTROLS_MENU_H
#define CONTROLS_MENU_H

#include "MenuSection.h"
#include <SDL.h>
#include <string>
#include <vector>
#include <map>
#include <tuple>

class OptionsMenuBuilder; 

class ControlsMenu : public MenuSection {
public:
    ControlsMenu(const std::string& controlsFile);
    MenuAction handleInput(SDL_Event& event) override;
    void render(SDL_Renderer* renderer, Font* titleFont, Font* optionFont, int windowWidth, int windowHeight, const OptionsMenuBuilder& config, float scaleFactor) override;
    bool hasChanges() const override { return hasPendingChanges_; }
    void save() override;

private:
    void initControlsOptions();
    void navigate(bool moveUp);

    std::map<std::string, std::tuple<std::vector<std::string>, std::string>> defaults_;
    std::string controlsFile_;
    std::string joystickName_;
    std::vector<Control> controlsOptions_;
    std::vector<std::string> pendingKeys_;
     std::string inputNumber_;
    size_t selectedOption_ = 0;
    size_t editingIndex_ = static_cast<size_t>(-1);
    bool editingText_ = false;
    bool capturingKey_ = false;
    std::string inputText_;
    bool hasPendingChanges_ = false;
};

#endif 