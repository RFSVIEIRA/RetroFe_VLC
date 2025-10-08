#ifndef ACTIONS_MENU_H
#define ACTIONS_MENU_H

#include "MenuSection.h"
#include <SDL.h>
#include <string>
#include <vector>
#include <map>
#include <tuple>

class OptionsMenuBuilder;

class ActionsMenu : public MenuSection {
public:
    ActionsMenu(const std::string& actionsFile);
    MenuAction handleInput(SDL_Event& event) override;
    void render(SDL_Renderer* renderer, Font* titleFont, Font* optionFont,
        int windowWidth, int windowHeight, const OptionsMenuBuilder& config, float scaleFactor) override;
    virtual bool hasChanges() const;
    void save() override;

private:
    void initActionsOptions();
    void navigate(bool moveUp);

    std::map<std::string, std::tuple<std::vector<std::string>, std::string>> defaults_;
    std::string actionsFile_;
    std::vector<Control> actionsOptions_;
    std::vector<std::string> pendingKeys_;
    size_t selectedOption_ = 0;
	size_t editingIndex_ = static_cast<size_t>(-1);
    bool editingText_ = false;
    bool capturingKey_ = false;
    std::string inputText_;
    bool hasPendingChanges_ = false;
};

#endif 