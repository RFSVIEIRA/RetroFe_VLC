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
#ifndef LAYOUT_MENU_H
#define LAYOUT_MENU_H

#include "MenuSection.h"
#include <memory>
#include <set>

class LayoutMenu : public MenuSection {
public:
    explicit LayoutMenu(const std::string& layoutDir);
    ~LayoutMenu();
    MenuAction handleInput(SDL_Event& event, UserInput* input = nullptr) override;
    void render(SDL_Renderer* renderer, Font* titleFont, Font* optionFont,
        int windowWidth, int windowHeight, const OptionsMenuBuilder& config, float scaleFactor) override;
    void save() override;
    bool hasChanges() const override;
    size_t getItemCount() const override;
    std::string getSubgroupForOption(size_t index);
    void setActiveMenuId(const std::string& menuId, const std::string& customTitle = "");
    std::vector<std::pair<std::string, std::string>> getCustomMenus() const;

private:
    void initLayoutOptions();
    void parseFile(const std::string& filePath, std::set<std::string>& parsedFiles); 
    void navigate(bool moveUp);
    std::vector<std::string> getLayoutFiles(const std::string& layoutDir);
    class Impl;
    std::unique_ptr<Impl> impl_;
    std::string substituteVariables(std::string str);
};

#endif // LAYOUT_MENU_H