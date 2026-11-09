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
#ifndef MENU_SECTION_H
#define MENU_SECTION_H

#include "../database/Configuration.h"
#include "../graphics/FontCache.h"
#include <SDL.h>
#include <vector>
#include <string>
#include <optional>
#include <map>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include "OptionsMenuBuilder.h"
#include "../Graphics/Font.h"

enum class SettingType {
    Boolean,
    Numeric,
    String,
    Path,
    Title,
    Subtitle,
    Subgroup
};

struct Setting {
    std::string key;
    std::string value;
    bool isCommented;
    SettingType type;
    std::string displayText;
    std::string originalName;
    std::vector<std::string> validValues;
    float minValue;
    float maxValue;
    std::string description;
    std::string group;
    std::string subgroup;
    std::string sourceFile;
    std::string menuSrc;
    std::string optMenuIndex = "";
};

struct Group {
    std::string name;
    std::string displayName;
    std::vector<Setting> items;
    std::string optMenuIndex = "";
};

struct Control {
    std::string action;
    std::vector<std::string> keys;
    bool isCommented;
    std::string displayText;
    std::optional<int> numericValue;
    std::string description;
    bool isNumeric;
    float minValue; 
    float maxValue;
};

enum class MenuAction { STAY, BACK };

class UserInput;

class MenuSection {
public:
    virtual ~MenuSection() {}
    virtual MenuAction handleInput(SDL_Event& event, UserInput* input = nullptr) = 0;
    virtual void render(SDL_Renderer* renderer, Font* titleFont, Font* optionFont,
        int windowWidth, int windowHeight, const OptionsMenuBuilder& config, float scaleFactor) = 0;
    virtual bool hasChanges() const = 0;
    virtual void save() = 0;
    virtual size_t getItemCount() const { return 0; }
};

#endif // MENU_SECTION_H