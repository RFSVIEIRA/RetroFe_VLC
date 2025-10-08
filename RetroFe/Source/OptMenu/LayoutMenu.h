#ifndef LAYOUT_MENU_H
#define LAYOUT_MENU_H

#include "MenuSection.h"
#include <memory>
#include <set>

class LayoutMenu : public MenuSection {
public:
    explicit LayoutMenu(const std::string& layoutDir);
    ~LayoutMenu();
    MenuAction handleInput(SDL_Event& event) override;
    void render(SDL_Renderer* renderer, Font* titleFont, Font* optionFont,
        int windowWidth, int windowHeight, const OptionsMenuBuilder& config, float scaleFactor) override;
    void save() override;
    bool hasChanges() const override;
    std::string getSubgroupForOption(size_t index);

private:
    void initLayoutOptions();
    void parseFile(const std::string& filePath, std::set<std::string>& parsedFiles); 
    void navigate(bool moveUp);
    std::vector<std::string> getLayoutFiles(const std::string& layoutDir);
    class Impl;
    std::unique_ptr<Impl> impl_;
};

#endif // LAYOUT_MENU_H