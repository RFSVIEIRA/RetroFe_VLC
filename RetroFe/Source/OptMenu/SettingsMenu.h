#ifndef SETTINGS_MENU_H
#define SETTINGS_MENU_H

#include "MenuSection.h"




class SettingsMenu : public MenuSection {
public:
    SettingsMenu(const std::string& settingsFile, Configuration& config);
    MenuAction handleInput(SDL_Event& event) override;
    void render(SDL_Renderer* renderer, Font* titleFont, Font* optionFont, int windowWidth, int windowHeight, const OptionsMenuBuilder& config, float scaleFactor) override;
    bool hasChanges() const override { return hasChanges_; }
    void save() override;


private:
    void initSettingsOptionsFromFile();
    void navigate(bool moveUp);


    std::map<std::string, std::string> pendingChanges_;
    std::string settingsFile_;
    Configuration& config_;
    std::vector<Setting> settingsOptions_;
    std::vector<std::string> originalSettingsLines_;
    size_t selectedOption_ = 0;
    size_t editingIndex_ = static_cast<size_t>(-1);
    bool editingText_ = false;
    std::string inputText_;
    bool hasPendingChanges_ = false;
    bool hasChanges_ = false; 

    float textScrollOffset_ = 0.0f;
    Uint32 lastScrollTime_ = 0;
    bool isScrolling_ = false;
    float scrollWidth_ = 0.0f;
};

#endif // SETTINGS_MENU_H