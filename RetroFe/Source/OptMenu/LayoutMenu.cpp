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
#include "LayoutMenu.h"
#include "../utility/Log.h"
#include "../Utility/Utils.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <filesystem>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include "OptionsMenu.h"
#include "../Control/UserInput.h"
#include <SDL_image.h>
#include"../Graphics/PageBuilder.h"

class LayoutMenu::Impl {
public:
    ~Impl() {
        if (image_.isAnimated && image_.frames) {
            for (int i = 0; i < image_.frameCount; ++i) {
                if (image_.frames[i]) SDL_DestroyTexture(image_.frames[i]);
            }
            SDL_free(image_.frames);
        }
        else if (image_.texture) {
            SDL_DestroyTexture(image_.texture);
        }
    }
    std::vector<Group> groups_;
    bool inSubMenu_ = false;
    size_t currentGroupIndex_ = 0;
    size_t selectedOption_ = 0;
    bool layoutChanged_ = false;
    std::string layoutDir_;
    std::vector<std::string> parsedFiles_;
    OptionsMenu::ImageInstance image_;  // Stores the current image for the selected setting
    SDL_Renderer* renderer_ = nullptr; // Store renderer for dynamic texture creation
    std::string currentImagePath_;
    int scrollIndex_ = 0;
    std::string activeMenuId_;
    std::string customMenuTitle_;
    std::vector<size_t> filteredGroupIndices_;
};
// Returns Unicode codepoint and advances pointer
static uint32_t utf8_decode(const char*& p)
{
    const unsigned char c = (unsigned char)*p;

    // 1-byte ASCII
    if (c < 0x80) {
        p++;
        return c;
    }

    // 2-byte: 110xxxxx 10xxxxxx
    if ((c >> 5) == 0x6) {
        uint32_t cp = ((c & 0x1F) << 6) |
            ((p[1] & 0x3F));
        p += 2;
        return cp;
    }

    // 3-byte: 1110xxxx 10xxxxxx 10xxxxxx
    if ((c >> 4) == 0xE) {
        uint32_t cp = ((c & 0x0F) << 12) |
            ((p[1] & 0x3F) << 6) |
            (p[2] & 0x3F);
        p += 3;
        return cp;
    }

    // 4-byte: 11110xxx  
    if ((c >> 3) == 0x1E) {
        uint32_t cp = ((c & 0x07) << 18) |
            ((p[1] & 0x3F) << 12) |
            ((p[2] & 0x3F) << 6) |
            (p[3] & 0x3F);
        p += 4;
        return cp;
    }

    // invalid ? skip
    p++;
    return '?';
}

LayoutMenu::LayoutMenu(const std::string& layoutDir)
    : impl_(std::make_unique<Impl>()) {
    impl_->layoutDir_ = layoutDir;
    initLayoutOptions();
}

LayoutMenu::~LayoutMenu() = default;

std::vector<std::string> LayoutMenu::getLayoutFiles(const std::string& layoutDir) {
    std::vector<std::string> files;
    if (!std::filesystem::exists(layoutDir)) {
        Logger::write(Logger::ZONE_ERROR, "LayoutMenu", "Layout directory not found: " + layoutDir);
        return files;
    }
    for (const auto& entry : std::filesystem::directory_iterator(layoutDir)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            if ((filename.rfind("layout", 0) == 0 || filename.rfind("splash", 0) == 0) &&
                filename.ends_with(".xml") &&
                filename.find("Menu.xml") == std::string::npos) {
                files.push_back(entry.path().string());
            }
        }
    }
    Logger::write(Logger::ZONE_INFO, "LayoutMenu", "Found " + std::to_string(files.size()) +
        " layout files in " + layoutDir);
    return files;
}

void LayoutMenu::parseFile(const std::string& filePath, std::set<std::string>& parsedFiles) {
    if (parsedFiles.count(filePath)) {
        Logger::write(Logger::ZONE_DEBUG, "LayoutMenu", "Skipping already parsed file: " + filePath);
        return;
    }
    parsedFiles.insert(filePath);
    impl_->parsedFiles_.push_back(filePath);
    Logger::write(Logger::ZONE_INFO, "LayoutMenu", "Parsing file: " + filePath);

    if (!std::filesystem::exists(filePath)) {
        Logger::write(Logger::ZONE_WARNING, "LayoutMenu", "File not found: " + filePath);
        return;
    }

    xmlDocPtr doc = xmlReadFile(filePath.c_str(), nullptr, XML_PARSE_RECOVER | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (!doc) {
        Logger::write(Logger::ZONE_ERROR, "LayoutMenu", "Failed to load " + filePath);
        return;
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root) {
        Logger::write(Logger::ZONE_ERROR, "LayoutMenu", "No root element in " + filePath);
        xmlFreeDoc(doc);
        return;
    }

    Logger::write(Logger::ZONE_INFO, "LayoutMenu", "Root element: " + std::string((const char*)root->name));

    int elementCount = 0;
    int skippedCount = 0;

    auto trim = [](const xmlChar* str) -> std::string {
        if (!str) return "";
        std::string s = reinterpret_cast<const char*>(str);
        s.erase(0, s.find_first_not_of(" \t"));
        s.erase(s.find_last_not_of(" \t") + 1);
        return s;
        };

    auto trimAndLower = [](const xmlChar* str) -> std::string {
        if (!str) return "";
        std::string s = reinterpret_cast<const char*>(str);
        s.erase(0, s.find_first_not_of(" \t"));
        s.erase(s.find_last_not_of(" \t") + 1);
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s;
        };

    for (xmlNodePtr elem = root->children; elem; elem = elem->next) {
        if (elem->type != XML_ELEMENT_NODE) continue;

        std::string tagName = trimAndLower(elem->name);
        xmlChar* groupAttr = xmlGetProp(elem, (const xmlChar*)"group");
        xmlChar* subgroupAttr = xmlGetProp(elem, (const xmlChar*)"subgroup");
        xmlChar* nameAttr = xmlGetProp(elem, (const xmlChar*)"name");
        xmlChar* activeAttr = xmlGetProp(elem, (const xmlChar*)"active");
        xmlChar* menuSrcAttr = xmlGetProp(elem, (const xmlChar*)"menuSrc");
        xmlChar* srcAttr = xmlGetProp(elem, (const xmlChar*)"src");
        xmlChar* optMenuIdxAttr = xmlGetProp(elem, (const xmlChar*)"optMenuIndex");
        if (!optMenuIdxAttr) optMenuIdxAttr = xmlGetProp(elem, (const xmlChar*)"optmenuindex");
        if (!optMenuIdxAttr) optMenuIdxAttr = xmlGetProp(elem, (const xmlChar*)"mainMenu");
        if (!optMenuIdxAttr) optMenuIdxAttr = xmlGetProp(elem, (const xmlChar*)"mainmenu");

        // Apply substitution to attribute values before trimming ---
        std::string rawGroup = groupAttr ? substituteVariables(reinterpret_cast<const char*>(groupAttr)) : "";
        std::string rawSubgroup = subgroupAttr ? substituteVariables(reinterpret_cast<const char*>(subgroupAttr)) : "";
        std::string rawName = nameAttr ? substituteVariables(reinterpret_cast<const char*>(nameAttr)) : "";
        std::string rawActive = activeAttr ? substituteVariables(reinterpret_cast<const char*>(activeAttr)) : "";
        std::string rawMenuSrc = menuSrcAttr ? substituteVariables(reinterpret_cast<const char*>(menuSrcAttr)) : "";
        std::string rawSrc = srcAttr ? substituteVariables(reinterpret_cast<const char*>(srcAttr)) : "";
        std::string rawOptMenuIdx = optMenuIdxAttr ? substituteVariables(reinterpret_cast<const char*>(optMenuIdxAttr)) : "";
        std::string optMenuIdxStr = trimAndLower((const xmlChar*)rawOptMenuIdx.c_str());

        // Replace calls to trim/trimAndLower with the new raw variables
        std::string originalGroup = rawGroup;
        std::string groupStr = trimAndLower((const xmlChar*)rawGroup.c_str());
        std::string originalSubgroup = rawSubgroup;
        std::string subgroupStr = trimAndLower((const xmlChar*)rawSubgroup.c_str());
        std::string originalName = rawName;
        std::string nameStr = trimAndLower((const xmlChar*)rawName.c_str());
        std::string activeStr = trimAndLower((const xmlChar*)rawActive.c_str());
        std::string menuSrcStr = rawMenuSrc; // menuSrc should likely NOT be lowercased
        std::string srcStr = rawSrc;

        xmlFree(groupAttr); xmlFree(subgroupAttr); xmlFree(nameAttr);
        xmlFree(activeAttr); xmlFree(menuSrcAttr); xmlFree(srcAttr);
        if (optMenuIdxAttr) xmlFree(optMenuIdxAttr);

        Logger::write(Logger::ZONE_DEBUG, "LayoutMenu", "Processing tag <" + tagName + "> with name: " + nameStr + ", group: " + groupStr + ", subgroup: " + subgroupStr + ", active: " + activeStr + ", src: " + srcStr);

        if (!nameStr.empty()) {
            if (groupStr.empty()) {
                groupStr = "default";
                originalGroup = "Default";
            }
            if (subgroupStr.empty()) {
                subgroupStr = "default";
                originalSubgroup = "Default";
            }

            bool isActive = (activeStr == "true");
            if (activeStr != "true" && activeStr != "false") {
                isActive = false;
            }

            auto groupIt = std::find_if(impl_->groups_.begin(), impl_->groups_.end(),
                [&groupStr](const Group& g) { return g.name == groupStr; });
            Group* group = nullptr;
            if (groupIt == impl_->groups_.end()) {
                impl_->groups_.push_back(Group{ groupStr, originalGroup, {}, optMenuIdxStr });
                group = &impl_->groups_.back();
                Logger::write(Logger::ZONE_INFO, "LayoutMenu", "Created new group: " + groupStr + " (optMenuIndex: " + optMenuIdxStr + ")");
            }
            else {
                group = &(*groupIt);
                if (group->optMenuIndex.empty() && !optMenuIdxStr.empty()) {
                    group->optMenuIndex = optMenuIdxStr;
                }
            }

            auto subgroupIt = std::find_if(group->items.begin(), group->items.end(),
                [&subgroupStr](const Setting& s) { return s.key == "subgroup_" + subgroupStr; });
            if (subgroupIt == group->items.end()) {
                group->items.push_back(Setting{
                    "subgroup_" + subgroupStr, "", true, SettingType::Subgroup,
                    originalSubgroup, originalSubgroup,
                    {}, 0, 0, "", groupStr, subgroupStr, filePath, "", optMenuIdxStr
                    });
                Logger::write(Logger::ZONE_INFO, "LayoutMenu", "Created subgroup: " + subgroupStr + " in group: " + groupStr);
            }

            auto settingIt = std::find_if(group->items.begin(), group->items.end(),
                [&nameStr, &subgroupStr](const Setting& s) {
                    return s.key == nameStr && s.type == SettingType::Boolean && s.subgroup == subgroupStr;
                });
            if (settingIt != group->items.end()) {
                settingIt->value = isActive ? "yes" : "no";
                settingIt->displayText = originalName + ": " + (isActive ? "yes" : "no");
                settingIt->originalName = originalName;
                settingIt->sourceFile = filePath;
                settingIt->menuSrc = menuSrcStr;
                Logger::write(Logger::ZONE_WARNING, "LayoutMenu", "Overwriting setting: " + nameStr + " in group: " + groupStr + ", subgroup: " + subgroupStr);
            }
            else {
                group->items.push_back(Setting{
                    nameStr, // key
                    isActive ? "yes" : "no",
                    false,
                    SettingType::Boolean,
                    originalName + ": " + (isActive ? "yes" : "no"),
                    originalName,
                    {"yes", "no"},
                    0,
                    0,
                    "Toggle " + originalName + " layout",
                    groupStr,
                    subgroupStr, // Ensure subgroup is stored
                    filePath,
                    menuSrcStr,
                    optMenuIdxStr
                    });
                Logger::write(Logger::ZONE_INFO, "LayoutMenu", "Added setting: " + nameStr + " in group: " + groupStr + ", subgroup: " + subgroupStr);
            }
            elementCount++;
        }
        else {
            skippedCount++;
            Logger::write(Logger::ZONE_DEBUG, "LayoutMenu", "Skipping tag <" + tagName + "> with no name attribute");
        }

        if (tagName == "include" && !srcStr.empty()) {
            std::string includeActiveStr = activeStr;
            Logger::write(Logger::ZONE_DEBUG, "LayoutMenu", "Processing include tag with src: " + srcStr + ", active: " + includeActiveStr);
            if (includeActiveStr.empty() || includeActiveStr == "true") {
                // Use theme root (impl_->layoutDir_) instead of current file's directory
                std::string includePath = Utils::combinePath(impl_->layoutDir_, srcStr);
                if (!std::filesystem::exists(includePath)) {
                    Logger::write(Logger::ZONE_WARNING, "LayoutMenu", "Included file not found: " + includePath);
                }
                else {
                    Logger::write(Logger::ZONE_INFO, "LayoutMenu", "Parsing included file: " + includePath);
                    parseFile(includePath, parsedFiles);
                }
            }
        }
    }

    Logger::write(Logger::ZONE_INFO, "LayoutMenu", "Parsed " + filePath + ": " + std::to_string(elementCount) +
        " named elements, skipped " + std::to_string(skippedCount));
    xmlFreeDoc(doc);
}

void LayoutMenu::initLayoutOptions() {
    impl_->groups_.clear();
    impl_->parsedFiles_.clear();

    std::set<std::string> parsedFiles;
    std::vector<std::string> layoutFiles = getLayoutFiles(impl_->layoutDir_);
    if (layoutFiles.empty()) {
        Logger::write(Logger::ZONE_WARNING, "LayoutMenu", "No layout files found in " + impl_->layoutDir_);
        Group placeholderGroup{
            "no_options", "NO OPTIONS", { Setting{
                "no_options",           // key
                "",                     // value
                true,                   // isCommented
                SettingType::String,    // type
                "No layout options available", // displayText
                "No Options",           // originalName (new field)
                {},                     // validValues
                0,                      // minValue
                0,                      // maxValue
                "",                     // description
                "no_options",           // group
                "",                     // subgroup
                ""                      // sourceFile
            }}
        };
        impl_->groups_.push_back(placeholderGroup);
        return;
    }

    // Optional: Determine active file based on screen ratio
    std::string activeFile;
    int displayIndex = 0; // Could be passed from OptionsMenu
    SDL_Rect displayBounds;
    if (SDL_GetDisplayBounds(displayIndex, &displayBounds) == 0) {
        float aspect = static_cast<float>(displayBounds.w) / displayBounds.h;
        if (std::abs(aspect - 16.0f / 9.0f) < 0.1f) {
            activeFile = Utils::combinePath(impl_->layoutDir_, "layout 16x9.xml");
        }
        else if (std::abs(aspect - 4.0f / 3.0f) < 0.1f) {
            activeFile = Utils::combinePath(impl_->layoutDir_, "layout 4x3.xml");
        }
    }
    if (activeFile.empty() || !std::filesystem::exists(activeFile)) {
        activeFile = Utils::combinePath(impl_->layoutDir_, "layout.xml");
    }

    // Parse active file first (optional, for prioritization)
    if (!activeFile.empty() && std::find(layoutFiles.begin(), layoutFiles.end(), activeFile) != layoutFiles.end()) {
        parseFile(activeFile, parsedFiles);
        layoutFiles.erase(std::remove(layoutFiles.begin(), layoutFiles.end(), activeFile), layoutFiles.end());
    }

    // Parse remaining files
    for (const auto& file : layoutFiles) {
        parseFile(file, parsedFiles);
    }
    setActiveMenuId("");
}

MenuAction LayoutMenu::handleInput(SDL_Event& event, UserInput* input) {
    if (event.type == SDL_KEYDOWN || event.type == SDL_JOYHATMOTION || event.type == SDL_JOYBUTTONDOWN) {
        bool menuUp = (input && input->newKeyPressed(UserInput::KeyCodeUp)) ||
            (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_UP) ||
            (event.type == SDL_JOYHATMOTION && event.jhat.value == SDL_HAT_UP);
        bool menuDown = (input && input->newKeyPressed(UserInput::KeyCodeDown)) ||
            (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_DOWN) ||
            (event.type == SDL_JOYHATMOTION && event.jhat.value == SDL_HAT_DOWN);
        bool menuSelect = (input && input->newKeyPressed(UserInput::KeyCodeSelect)) ||
            (event.type == SDL_KEYDOWN && (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER)) ||
            (event.type == SDL_JOYBUTTONDOWN && event.jbutton.button == 0);
        bool menuBack = (input && (input->newKeyPressed(UserInput::KeyCodeBack) || input->newKeyPressed(UserInput::KeyCodeMenu))) ||
            (event.type == SDL_KEYDOWN && (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_BACKSPACE)) ||
            (event.type == SDL_JOYBUTTONDOWN && event.jbutton.button == 1);
        bool toggleEnable = (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_SPACE) ||
            (event.type == SDL_JOYBUTTONDOWN && event.jbutton.button == 2);
        bool disableAll = (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_d) ||
            (event.type == SDL_JOYBUTTONDOWN && event.jbutton.button == 4);

        if (menuUp) {
            navigate(true);
            return MenuAction::STAY;
        }
        else if (menuDown) {
            navigate(false);
            return MenuAction::STAY;
        }
        else if (menuSelect && impl_->selectedOption_ < (impl_->inSubMenu_ ? impl_->groups_[impl_->currentGroupIndex_].items.size() : impl_->filteredGroupIndices_.size())) {
            if (!impl_->inSubMenu_) {
                // Enter sub-menu
                if (impl_->selectedOption_ < impl_->filteredGroupIndices_.size()) {
                    impl_->inSubMenu_ = true;
                    impl_->currentGroupIndex_ = impl_->filteredGroupIndices_[impl_->selectedOption_];
                    impl_->selectedOption_ = 0;
                    impl_->scrollIndex_ = 0;
                    navigate(true); // Move to first selectable setting
                    Logger::write(Logger::ZONE_INFO, "LayoutMenu", "Entered sub-menu for group: " + impl_->groups_[impl_->currentGroupIndex_].displayName);
                }
            }
            else {
                Setting& setting = impl_->groups_[impl_->currentGroupIndex_].items[impl_->selectedOption_];
                if (setting.type == SettingType::Boolean && !setting.isCommented) {
                    std::string subgroup = getSubgroupForOption(impl_->selectedOption_);
                    if (subgroup.empty()) {
                        Logger::write(Logger::ZONE_ERROR, "LayoutMenu", "Option " + setting.originalName + " has no subgroup assigned");
                        return MenuAction::STAY;
                    }
                    if (setting.value == "no") {
                        // Enable this option, disable others in the same subgroup
                        for (auto& opt : impl_->groups_[impl_->currentGroupIndex_].items) {
                            if (&opt != &setting && opt.type == SettingType::Boolean && opt.subgroup == subgroup) {
                                opt.value = "no";
                                opt.displayText = opt.originalName + ": no";
                            }
                        }
                        setting.value = "yes";
                        setting.displayText = setting.originalName + ": yes";
                        impl_->layoutChanged_ = true;
                        Logger::write(Logger::ZONE_DEBUG, "LayoutMenu", "Enabled " + setting.originalName + " and disabled others in subgroup " + subgroup);
                    }
                    else {
                        setting.value = "no";
                        setting.displayText = setting.originalName + ": no";
                        impl_->layoutChanged_ = true;
                        Logger::write(Logger::ZONE_DEBUG, "LayoutMenu", "Disabled " + setting.originalName);
                    }
                }
            }
            return MenuAction::STAY;
        }
        else if (menuBack) {
            if (impl_->inSubMenu_) {
                std::string menuIdLower = Utils::toLower(impl_->activeMenuId_);
                bool isDefaultLayout = (menuIdLower.empty() || menuIdLower == "layout" || menuIdLower == "0");
                if (impl_->filteredGroupIndices_.size() <= 1 && !isDefaultLayout) {
                    impl_->inSubMenu_ = false;
                    return MenuAction::BACK;
                }
                impl_->inSubMenu_ = false;
                impl_->selectedOption_ = 0;
                impl_->scrollIndex_ = 0;
                Logger::write(Logger::ZONE_INFO, "LayoutMenu", "Returned to top-level menu");
                return MenuAction::STAY;
            }
            return MenuAction::BACK;
        }
        else if (toggleEnable && impl_->inSubMenu_ && impl_->selectedOption_ < impl_->groups_[impl_->currentGroupIndex_].items.size()) {
            Setting& setting = impl_->groups_[impl_->currentGroupIndex_].items[impl_->selectedOption_];
            if (setting.type == SettingType::Boolean) {
                setting.isCommented = !setting.isCommented;
                setting.displayText = setting.originalName + ": " + setting.value + (setting.isCommented ? " (disabled)" : "");
                impl_->layoutChanged_ = true;
                Logger::write(Logger::ZONE_DEBUG, "LayoutMenu", "Toggled comment for " + setting.originalName + " to " + (setting.isCommented ? "disabled" : "enabled"));
            }
            return MenuAction::STAY;
        }
        else if (disableAll && impl_->inSubMenu_ && impl_->selectedOption_ < impl_->groups_[impl_->currentGroupIndex_].items.size()) {
            Setting& setting = impl_->groups_[impl_->currentGroupIndex_].items[impl_->selectedOption_];
            if (setting.type == SettingType::Subgroup) {
                std::string targetSubgroup = setting.subgroup;
                for (auto& opt : impl_->groups_[impl_->currentGroupIndex_].items) {
                    if (opt.type == SettingType::Boolean && !opt.isCommented && opt.subgroup == targetSubgroup) {
                        opt.value = "no";
                        opt.displayText = opt.originalName + ": no";
                    }
                }
                impl_->layoutChanged_ = true;
                Logger::write(Logger::ZONE_INFO, "LayoutMenu", "Disabled all options in subgroup " + targetSubgroup);
            }
            return MenuAction::STAY;
        }
    }
    return MenuAction::STAY;
}

void LayoutMenu::render(SDL_Renderer* renderer, Font* titleFont, Font* optionFont,
    int windowWidth, int windowHeight, const OptionsMenuBuilder& config, float scaleFactor) {
    if (!titleFont || !optionFont || !titleFont->getTexture() || !optionFont->getTexture()) {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_Rect rect = { 50, 50, 200, 100 };
        SDL_RenderFillRect(renderer, &rect);
        return;
    }
    impl_->renderer_ = renderer;
    // Find isMenu image configuration
    const OptionsMenuBuilder::ImageConfig* menuImageConfig = nullptr;
    for (const auto& img : config.getImages()) {
        if (img.isMenu) {
            menuImageConfig = &img;
            break;
        }
    }
    // Determine the image path for the selected setting
    std::string selectedName;
    std::string newImagePath;
    if (impl_->inSubMenu_ && impl_->selectedOption_ < impl_->groups_[impl_->currentGroupIndex_].items.size()) {
        const Setting& setting = impl_->groups_[impl_->currentGroupIndex_].items[impl_->selectedOption_];
        if (setting.type == SettingType::Boolean && !setting.isCommented && !setting.menuSrc.empty() && menuImageConfig) {
            selectedName = setting.originalName;
            std::vector<std::string> extensions = { ".png", ".jpg", ".jpeg", ".gif", ".webp" };
            for (const auto& ext : extensions) {
                newImagePath = Utils::combinePath(impl_->layoutDir_, setting.menuSrc, setting.originalName + ext);
                if (std::filesystem::exists(newImagePath)) {
                    break;
                }
                newImagePath = "";
            }
        }
    }
    // Load image only if the path has changed
    if (newImagePath != impl_->currentImagePath_) {
        // Clean up existing image
        if (impl_->image_.isAnimated && impl_->image_.frames) {
            for (int i = 0; i < impl_->image_.frameCount; ++i) {
                if (impl_->image_.frames[i]) SDL_DestroyTexture(impl_->image_.frames[i]);
            }
            SDL_free(impl_->image_.frames);
            impl_->image_.frames = nullptr;
        }
        else if (impl_->image_.texture) {
            SDL_DestroyTexture(impl_->image_.texture);
            impl_->image_.texture = nullptr;
        }
        impl_->image_ = OptionsMenu::ImageInstance(); // Reset image instance
        impl_->currentImagePath_ = newImagePath;
        if (!newImagePath.empty()) {
            if (menuImageConfig->isAnimated) {
                // Load animated GIF
                IMG_Animation* anim = IMG_LoadAnimation(newImagePath.c_str());
                if (anim) {
                    impl_->image_.isAnimated = true;
                    impl_->image_.frameCount = anim->count;
                    impl_->image_.frames = (SDL_Texture**)SDL_calloc(impl_->image_.frameCount, sizeof(SDL_Texture*));
                    impl_->image_.frameDelays.resize(impl_->image_.frameCount);
                    for (int i = 0; i < anim->count; ++i) {
                        impl_->image_.frames[i] = SDL_CreateTextureFromSurface(renderer, anim->frames[i]);
                        impl_->image_.frameDelays[i] = anim->delays[i] > 0 ? anim->delays[i] : 10; // Minimum delay of 10ms
                        SDL_SetTextureBlendMode(impl_->image_.frames[i], SDL_BLENDMODE_BLEND);
                    }
                    IMG_FreeAnimation(anim);
                    Logger::write(Logger::ZONE_INFO, "LayoutMenu", "Loaded animated GIF with " + std::to_string(impl_->image_.frameCount) + " frames: " + newImagePath);
                }
                else {
                    Logger::write(Logger::ZONE_WARNING, "LayoutMenu", "Failed to load animated GIF: " + newImagePath + " - " + IMG_GetError());
                }
            }
            else {
                // Load static image
                SDL_Surface* surface = IMG_Load(newImagePath.c_str());
                if (surface) {
                    impl_->image_.texture = SDL_CreateTextureFromSurface(renderer, surface);
                    SDL_SetTextureBlendMode(impl_->image_.texture, SDL_BLENDMODE_BLEND);
                    SDL_FreeSurface(surface);
                    Logger::write(Logger::ZONE_INFO, "LayoutMenu", "Loaded static image: " + newImagePath);
                }
                else {
                    Logger::write(Logger::ZONE_WARNING, "LayoutMenu", "Failed to load static image: " + newImagePath + " - " + IMG_GetError());
                }
            }
            if (impl_->image_.isAnimated || impl_->image_.texture) {
                impl_->image_.isMenu = true;
                impl_->image_.dest = { menuImageConfig->x, menuImageConfig->y, menuImageConfig->width, menuImageConfig->height };
                impl_->image_.scale = menuImageConfig->scale;
                impl_->image_.xAlign = menuImageConfig->xAlign;
                impl_->image_.yAlign = menuImageConfig->yAlign;
                impl_->image_.xOffset = menuImageConfig->xOffset;
                impl_->image_.yOffset = menuImageConfig->yOffset;
                impl_->image_.xOrigin = menuImageConfig->xOrigin;
                impl_->image_.yOrigin = menuImageConfig->yOrigin;
                impl_->image_.hasExplicitXOrigin = menuImageConfig->hasExplicitXOrigin;
                impl_->image_.hasExplicitYOrigin = menuImageConfig->hasExplicitYOrigin;
                impl_->image_.alpha = menuImageConfig->alpha;
                impl_->image_.layer = menuImageConfig->layer;
                impl_->image_.currentFrame = 0;
                impl_->image_.lastFrameTime = SDL_GetTicks();
            }
        }
    }
    // Render image
    SDL_Texture* texture = nullptr;
    if (impl_->image_.isAnimated && impl_->image_.frames && impl_->image_.frameCount > 0) {
        Uint32 currentTime = SDL_GetTicks();
        if (currentTime - impl_->image_.lastFrameTime >= impl_->image_.frameDelays[impl_->image_.currentFrame]) {
            impl_->image_.currentFrame = (impl_->image_.currentFrame + 1) % impl_->image_.frameCount;
            impl_->image_.lastFrameTime = currentTime;
        }
        texture = impl_->image_.frames[impl_->image_.currentFrame];
    }
    else if (impl_->image_.texture) {
        texture = impl_->image_.texture;
    }
    if (texture && menuImageConfig) {
        int texW, texH;
        SDL_QueryTexture(texture, nullptr, 0, &texW, &texH);
        SDL_Rect srcRect = { 0, 0, texW, texH };
        int scaledWidth = static_cast<int>(impl_->image_.dest.w * scaleFactor);
        int scaledHeight = static_cast<int>(impl_->image_.dest.h * scaleFactor);
        SDL_Rect destRect = { 0, 0, scaledWidth, scaledHeight };
        int scaledXOffset = static_cast<int>(impl_->image_.xOffset * scaleFactor);
        int scaledYOffset = static_cast<int>(impl_->image_.yOffset * scaleFactor);
        int lmBaseX = 0;
        switch (impl_->image_.xAlign) {
        case OptionsMenuBuilder::XAlignment::Center:
            lmBaseX = windowWidth / 2;
            break;
        case OptionsMenuBuilder::XAlignment::Left:
            lmBaseX = 0;
            break;
        case OptionsMenuBuilder::XAlignment::Right:
            lmBaseX = windowWidth;
            break;
        default:
            lmBaseX = static_cast<int>(impl_->image_.dest.x * scaleFactor);
            break;
        }

        if (impl_->image_.hasExplicitXOrigin) {
            destRect.x = static_cast<int>(lmBaseX + scaledXOffset - (impl_->image_.xOrigin * scaledWidth));
        } else {
            switch (impl_->image_.xAlign) {
            case OptionsMenuBuilder::XAlignment::Center:
                destRect.x = (windowWidth - scaledWidth) / 2 + scaledXOffset;
                break;
            case OptionsMenuBuilder::XAlignment::Left:
                destRect.x = scaledXOffset;
                break;
            case OptionsMenuBuilder::XAlignment::Right:
                destRect.x = windowWidth - scaledWidth + scaledXOffset;
                break;
            case OptionsMenuBuilder::XAlignment::Numeric:
                destRect.x = static_cast<int>(impl_->image_.dest.x * scaleFactor);
                break;
            default:
                destRect.x = (windowWidth - scaledWidth) / 2;
                break;
            }
        }

        int lmBaseY = 0;
        switch (impl_->image_.yAlign) {
        case OptionsMenuBuilder::YAlignment::Center:
            lmBaseY = windowHeight / 2;
            break;
        case OptionsMenuBuilder::YAlignment::Top:
            lmBaseY = 0;
            break;
        case OptionsMenuBuilder::YAlignment::Bottom:
            lmBaseY = windowHeight;
            break;
        default:
            lmBaseY = static_cast<int>(impl_->image_.dest.y * scaleFactor);
            break;
        }

        if (impl_->image_.hasExplicitYOrigin) {
            destRect.y = static_cast<int>(lmBaseY + scaledYOffset - (impl_->image_.yOrigin * scaledHeight));
        } else {
            switch (impl_->image_.yAlign) {
            case OptionsMenuBuilder::YAlignment::Center:
                destRect.y = (windowHeight - scaledHeight) / 2 + scaledYOffset;
                break;
            case OptionsMenuBuilder::YAlignment::Top:
                destRect.y = scaledYOffset;
                break;
            case OptionsMenuBuilder::YAlignment::Bottom:
                destRect.y = windowHeight - scaledHeight + scaledYOffset;
                break;
            case OptionsMenuBuilder::YAlignment::Numeric:
                destRect.y = static_cast<int>(impl_->image_.dest.y * scaleFactor);
                break;
            default:
                destRect.y = scaledYOffset;
                break;
            }
        }

        if (impl_->image_.alpha < 1.0f) {
            SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(impl_->image_.alpha * 255.0f));
        } else {
            SDL_SetTextureAlphaMod(texture, 255);
        }
        if (impl_->image_.scale == "fit") {
            float aspectTex = static_cast<float>(texW) / texH;
            float aspectDest = static_cast<float>(destRect.w) / destRect.h;
            if (aspectTex > aspectDest) {
                destRect.h = static_cast<int>(destRect.w / aspectTex);
                destRect.y += (scaledHeight - destRect.h) / 2;
            }
            else {
                destRect.w = static_cast<int>(destRect.h * aspectTex);
                destRect.x += (scaledWidth - destRect.w) / 2;
            }
        }
        else if (impl_->image_.scale == "fill") {
            float aspectTex = static_cast<float>(texW) / texH;
            float aspectDest = static_cast<float>(destRect.w) / destRect.h;
            if (aspectTex < aspectDest) {
                int srcH = static_cast<int>(texW / aspectDest);
                srcRect.y = (texH - srcH) / 2;
                srcRect.h = srcH;
            }
            else {
                int srcW = static_cast<int>(texH * aspectDest);
                srcRect.x = (texW - srcW) / 2;
                srcRect.w = srcW;
            }
        }
#ifdef _DEBUG
        Logger::write(Logger::ZONE_INFO, "LayoutMenu", "Rendering image: x=" + std::to_string(destRect.x) +
            ", y=" + std::to_string(destRect.y) + ", w=" + std::to_string(destRect.w) +
            ", h=" + std::to_string(destRect.h));
#endif
        SDL_RenderCopy(renderer, texture, impl_->image_.scale == "fill" ? &srcRect : nullptr, &destRect);
        // Render selected name as text below the image
        if (!selectedName.empty()) {
            // Measure
            float textWidth = 0.0f;
            const char* sp = selectedName.c_str();
            while (*sp) {
                uint32_t cp = utf8_decode(sp);
                Font::GlyphInfo glyph;
                if (optionFont->getRect(cp, glyph)) {
                    textWidth += glyph.advance;
                }
            }

            int textX = destRect.x + (destRect.w - static_cast<int>(textWidth)) / 2;
            int textY = destRect.y + destRect.h + static_cast<int>(10 * scaleFactor);
            float textRenderX = static_cast<float>(textX);

            SDL_SetTextureColorMod(optionFont->getTexture(),
                config.getOptionColor().r,
                config.getOptionColor().g,
                config.getOptionColor().b);

            // Render
            const char* sp2 = selectedName.c_str();
            while (*sp2) {
                uint32_t cp = utf8_decode(sp2);
                Font::GlyphInfo glyph;
                if (optionFont->getRect(cp, glyph)) {
                    SDL_Rect textDestRect = {
                        static_cast<int>(textRenderX),
                        textY,
                        glyph.rect.w,
                        glyph.rect.h
                    };
                    SDL_RenderCopy(renderer, optionFont->getTexture(), &glyph.rect, &textDestRect);
                    textRenderX += glyph.advance;
                }
            }
        }
    }

    // Check if modal card layout is enabled
    bool isModal = config.isModal();
    const auto& options = impl_->inSubMenu_ ? impl_->groups_[impl_->currentGroupIndex_].items : std::vector<Setting>{};
    size_t itemCount = impl_->inSubMenu_ ? options.size() : impl_->filteredGroupIndices_.size();

    int modalX = 0, modalY = 0, modalW = 0, modalH = 0;
    if (isModal) {
        config.calculateModalBounds(itemCount, windowWidth, windowHeight, scaleFactor, modalX, modalY, modalW, modalH);
    } else {
        modalW = windowWidth;
        modalH = windowHeight;
        modalX = 0;
        modalY = 0;
    }

    // Render main title (top-level) or group title (sub-menu)
    std::string title;
    if (!impl_->customMenuTitle_.empty()) {
        title = impl_->customMenuTitle_;
    } else if (impl_->inSubMenu_ && impl_->currentGroupIndex_ < impl_->groups_.size()) {
        title = impl_->groups_[impl_->currentGroupIndex_].displayName;
    } else {
        title = config.getLayoutTitleText();
    }
    float titleWidth = 0.0f;
    const char* tp = title.c_str();
    while (*tp) {
        uint32_t cp = utf8_decode(tp);
        Font::GlyphInfo glyph;
        if (titleFont->getRect(cp, glyph)) {
            titleWidth += glyph.advance;
        }
    }

    int scaledTitleXOffset = static_cast<int>(config.getTitleXOffset() * scaleFactor);
    float baseTitleX = 0.0f;
    if (config.getTitleXAlignment() == OptionsMenuBuilder::XAlignment::Center) {
        baseTitleX = windowWidth / 2.0f;
    } else if (config.getTitleXAlignment() == OptionsMenuBuilder::XAlignment::Right) {
        baseTitleX = static_cast<float>(windowWidth);
    } else {
        baseTitleX = 0.0f;
    }
    int titleX = static_cast<int>(baseTitleX + scaledTitleXOffset - config.getTitleXOrigin() * titleWidth);

    int scaledTitleYOffset = static_cast<int>(config.getTitleYOffset() * scaleFactor);
    float baseTitleY = 0.0f;
    if (config.getTitleYAlignment() == OptionsMenuBuilder::YAlignment::Center) {
        baseTitleY = windowHeight / 2.0f;
    } else if (config.getTitleYAlignment() == OptionsMenuBuilder::YAlignment::Bottom) {
        baseTitleY = static_cast<float>(windowHeight);
    } else {
        baseTitleY = 0.0f;
    }
    int titleY = 0;
    if (isModal && config.isModalAutoScale()) {
        int topPadding = (config.getTitleYOffset() > 0 && config.getTitleYOffset() < 100) ?
            static_cast<int>(config.getTitleYOffset() * scaleFactor) :
            static_cast<int>(35 * scaleFactor);
        titleY = modalY + topPadding;
    } else {
        titleY = static_cast<int>(baseTitleY + scaledTitleYOffset - config.getTitleYOrigin() * titleFont->getHeight());
    }

    // Render title text glyphs
    float renderX = static_cast<float>(titleX);
    const char* tp2 = title.c_str();
    while (*tp2) {
        uint32_t cp = utf8_decode(tp2);
        Font::GlyphInfo glyph;
        if (titleFont->getRect(cp, glyph)) {
            SDL_Rect destRect = { static_cast<int>(renderX), titleY, glyph.rect.w, glyph.rect.h };
            SDL_SetTextureColorMod(titleFont->getTexture(), config.getTitleColor().r, config.getTitleColor().g, config.getTitleColor().b);
            SDL_RenderCopy(renderer, titleFont->getTexture(), &glyph.rect, &destRect);
            renderX += glyph.advance;
        }
    }

    // Render title underline / header divider
    if (config.getSubtitleUnderlineColor().a > 0) {
        int underlineY = titleY + titleFont->getHeight() + static_cast<int>(8 * scaleFactor);
        int lineW = (config.getSeparatorWidth() > 0 && isModal) ?
            static_cast<int>(config.getSeparatorWidth() * scaleFactor) :
            static_cast<int>(titleWidth);
        int lineX = isModal ?
            (modalX + (modalW - lineW) / 2) :
            ((config.getTitleXAlignment() == OptionsMenuBuilder::XAlignment::Center) ?
                ((windowWidth - lineW) / 2) :
                titleX);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, config.getSubtitleUnderlineColor().r, config.getSubtitleUnderlineColor().g, config.getSubtitleUnderlineColor().b, config.getSubtitleUnderlineColor().a);
        SDL_Rect uRect = { lineX, underlineY, lineW, static_cast<int>(2 * scaleFactor) };
        SDL_RenderFillRect(renderer, &uRect);
    }

    // Menu options vertical bounds
    int scaledSpaceBetween = static_cast<int>(config.getSpaceBetweenText() * scaleFactor);
    if (scaledSpaceBetween < 20) scaledSpaceBetween = static_cast<int>(40 * scaleFactor);

    int scaledMenuXOffset = static_cast<int>(config.getMenuXOffset() * scaleFactor);
    int scaledMenuYOffset = static_cast<int>(config.getMenuYOffset() * scaleFactor);
    int scaledTitleSpacing = static_cast<int>(config.getTitleSpacing() * scaleFactor);

    int bottomLimit = isModal ?
        (modalY + modalH - static_cast<int>(30 * scaleFactor)) :
        (windowHeight - static_cast<int>(40 * scaleFactor));

    float baseMenuY = 0.0f;
    if (config.getMenuYAlignment() == OptionsMenuBuilder::YAlignment::Center) {
        baseMenuY = windowHeight / 2.0f;
    } else if (config.getMenuYAlignment() == OptionsMenuBuilder::YAlignment::Bottom) {
        baseMenuY = static_cast<float>(windowHeight);
    } else {
        baseMenuY = 0.0f;
    }

    int menuY = 0;
    if (isModal && config.isModalAutoScale()) {
        int menuGap = (config.getMenuYOffset() > 0 && config.getMenuYOffset() < 100) ?
            static_cast<int>(config.getMenuYOffset() * scaleFactor) :
            static_cast<int>(30 * scaleFactor);
        menuY = titleY + titleFont->getHeight() + menuGap;
    } else {
        float totalMenuHeight = static_cast<float>(itemCount * scaledSpaceBetween);
        menuY = static_cast<int>(baseMenuY + scaledMenuYOffset - config.getMenuYOrigin() * totalMenuHeight);
    }

    int availHeight = bottomLimit - menuY;
    int maxVisibleLines = availHeight / scaledSpaceBetween;
    if (maxVisibleLines < 3) maxVisibleLines = 3;

    // Viewport window scrolling logic
    if (impl_->selectedOption_ < static_cast<size_t>(impl_->scrollIndex_)) {
        impl_->scrollIndex_ = static_cast<int>(impl_->selectedOption_);
    } else if (impl_->selectedOption_ >= static_cast<size_t>(impl_->scrollIndex_ + maxVisibleLines)) {
        impl_->scrollIndex_ = static_cast<int>(impl_->selectedOption_) - maxVisibleLines + 1;
    }
    int maxScroll = static_cast<int>(itemCount) > maxVisibleLines ? static_cast<int>(itemCount) - maxVisibleLines : 0;
    if (impl_->scrollIndex_ > maxScroll) impl_->scrollIndex_ = maxScroll;
    if (impl_->scrollIndex_ < 0) impl_->scrollIndex_ = 0;

    size_t startIdx = static_cast<size_t>(impl_->scrollIndex_);
    size_t endIdx = (std::min)(itemCount, startIdx + static_cast<size_t>(maxVisibleLines));

    int scaledPadding = static_cast<int>(config.getSelectionBarPadding() * scaleFactor);
    int scaledBarHeight = config.getSelectionBarHeight() > 0 ?
        static_cast<int>(config.getSelectionBarHeight() * scaleFactor) :
        static_cast<int>(optionFont->getHeight() + 8 * scaleFactor);
    int scaledBarYOffset = static_cast<int>(config.getSelectionBarYOffset() * scaleFactor);

    for (size_t i = startIdx; i < endIdx; ++i) {
        float y = static_cast<float>(menuY + static_cast<int>(i - startIdx) * scaledSpaceBetween);

        std::string text;
        bool isGroup = false;
        bool isSubgroup = false;
        bool isSelectable = false;
        if (impl_->inSubMenu_) {
            const Setting& setting = options[i];
            text = setting.displayText;
            isSubgroup = setting.type == SettingType::Subgroup;
            isSelectable = setting.type == SettingType::Boolean && !setting.isCommented;
        } else {
            size_t gIdx = (i < impl_->filteredGroupIndices_.size()) ? impl_->filteredGroupIndices_[i] : 0;
            text = (gIdx < impl_->groups_.size()) ? impl_->groups_[gIdx].displayName : "";
            isGroup = true;
            isSelectable = true;
        }

        float textWidth = 0.0f;
        const char* p = text.c_str();
        while (*p) {
            uint32_t cp = utf8_decode(p);
            Font::GlyphInfo glyph;
            if (optionFont->getRect(cp, glyph)) {
                textWidth += glyph.advance;
            }
        }

        float baseOptionX = 0.0f;
        if (config.getMenuXAlignment() == OptionsMenuBuilder::XAlignment::Center) {
            baseOptionX = windowWidth / 2.0f;
        } else if (config.getMenuXAlignment() == OptionsMenuBuilder::XAlignment::Right) {
            baseOptionX = static_cast<float>(windowWidth);
        } else {
            baseOptionX = 0.0f;
        }
        int optionX = static_cast<int>(baseOptionX + scaledMenuXOffset - config.getMenuXOrigin() * textWidth);

        bool isSelected = (i == impl_->selectedOption_);

        // 1. Draw Selection Bar
        if (isSelected && isSelectable) {
            SDL_Color barColor = config.getSelectionBarColor();
            int barX = optionX - scaledPadding;
            int barW = static_cast<int>(textWidth) + 2 * scaledPadding;
            if (config.getSeparatorWidth() > 0 && isModal) {
                barW = static_cast<int>(config.getSeparatorWidth() * scaleFactor);
                int scaledSepXOff = static_cast<int>(config.getSeparatorXOffset() * scaleFactor);
                if (config.getSeparatorXAlignment() == OptionsMenuBuilder::XAlignment::Left) {
                    barX = (config.isSeparatorAlignText() ? optionX : (optionX - scaledPadding)) + scaledSepXOff;
                } else if (config.getSeparatorXAlignment() == OptionsMenuBuilder::XAlignment::Right) {
                    barX = modalX + modalW - barW - scaledPadding + scaledSepXOff;
                } else if (config.getSeparatorXAlignment() == OptionsMenuBuilder::XAlignment::Numeric) {
                    int numVal = static_cast<int>((config.getSeparatorX() > 0 ? config.getSeparatorX() : config.getSeparatorXOffset()) * scaleFactor);
                    barX = (isModal && numVal < modalX) ? (modalX + numVal) : numVal;
                } else {
                    barX = modalX + (modalW - barW) / 2 + scaledSepXOff;
                }
            }
            int barY = static_cast<int>(y + scaledBarYOffset);

            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, barColor.r, barColor.g, barColor.b, barColor.a);
            SDL_Rect barRect = { barX, barY, barW, scaledBarHeight };
            SDL_RenderFillRect(renderer, &barRect);
        }

        // 2. Draw Separator Line between EACH option
        if (config.getSeparatorColor().a > 0 && !isSubgroup) {
            int sepW = config.getSeparatorWidth() > 0 ?
                static_cast<int>(config.getSeparatorWidth() * scaleFactor) :
                static_cast<int>(textWidth + 2 * scaledPadding);
            int scaledSepXOff = static_cast<int>(config.getSeparatorXOffset() * scaleFactor);
            int sepX = 0;
            if (config.getSeparatorXAlignment() == OptionsMenuBuilder::XAlignment::Left) {
                sepX = (config.isSeparatorAlignText() ? optionX : (optionX - scaledPadding)) + scaledSepXOff;
            } else if (config.getSeparatorXAlignment() == OptionsMenuBuilder::XAlignment::Right) {
                sepX = isModal ? (modalX + modalW - sepW - scaledPadding) : (windowWidth - sepW);
                sepX += scaledSepXOff;
            } else if (config.getSeparatorXAlignment() == OptionsMenuBuilder::XAlignment::Numeric) {
                int numVal = static_cast<int>((config.getSeparatorX() > 0 ? config.getSeparatorX() : config.getSeparatorXOffset()) * scaleFactor);
                sepX = (isModal && numVal < modalX) ? (modalX + numVal) : numVal;
            } else {
                sepX = isModal ?
                    (modalX + (modalW - sepW) / 2) :
                    ((config.getMenuXAlignment() == OptionsMenuBuilder::XAlignment::Center) ?
                        ((windowWidth - sepW) / 2 + scaledMenuXOffset) :
                        (optionX - scaledPadding));
                sepX += scaledSepXOff;
            }
            int sepY = static_cast<int>(y + optionFont->getHeight() + config.getSeparatorYOffset() * scaleFactor);
            int sepH = static_cast<int>(config.getSeparatorHeight() * scaleFactor);
            if (sepH < 1) sepH = 1;

            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, config.getSeparatorColor().r, config.getSeparatorColor().g, config.getSeparatorColor().b, config.getSeparatorColor().a);
            SDL_Rect sepRect = { sepX, sepY, sepW, sepH };
            SDL_RenderFillRect(renderer, &sepRect);
        }

        // 3. Draw Option Text
        SDL_Color textColor = isSelected && isSelectable ? config.getSelectedColor() : (isSubgroup ? config.getTitleColor() : config.getOptionColor());
        float renderTextX = static_cast<float>(optionX);
        int renderY = static_cast<int>(y);
        const char* p2 = text.c_str();
        while (*p2) {
            uint32_t cp = utf8_decode(p2);
            Font::GlyphInfo glyph;
            if (optionFont->getRect(cp, glyph)) {
                SDL_Rect destRect = { static_cast<int>(renderTextX), renderY, glyph.rect.w, glyph.rect.h };
                SDL_SetTextureColorMod(optionFont->getTexture(), textColor.r, textColor.g, textColor.b);
                SDL_RenderCopy(renderer, optionFont->getTexture(), &glyph.rect, &destRect);
                renderTextX += glyph.advance;
            }
        }
    }

    // 4. Scrollbar indicator when items exceed maxVisibleLines
    if (itemCount > static_cast<size_t>(maxVisibleLines) && isModal) {
        int trackX = modalX + modalW - static_cast<int>(12 * scaleFactor);
        int trackY = menuY;
        int trackW = static_cast<int>(4 * scaleFactor);
        if (trackW < 2) trackW = 2;
        int trackH = maxVisibleLines * scaledSpaceBetween;

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 35);
        SDL_Rect trackRect = { trackX, trackY, trackW, trackH };
        SDL_RenderFillRect(renderer, &trackRect);

        int thumbH = (maxVisibleLines * trackH) / static_cast<int>(itemCount);
        if (thumbH < 24) thumbH = 24;
        int maxTravel = trackH - thumbH;
        int thumbY = trackY + (impl_->scrollIndex_ * maxTravel) / maxScroll;

        SDL_Color thumbCol = config.getBorderColor();
        if (thumbCol.a == 0) thumbCol = { 255, 165, 0, 255 };
        SDL_SetRenderDrawColor(renderer, thumbCol.r, thumbCol.g, thumbCol.b, 200);
        SDL_Rect thumbRect = { trackX, thumbY, trackW, thumbH };
        SDL_RenderFillRect(renderer, &thumbRect);
    }
}

void LayoutMenu::save() {
    if (!impl_->layoutChanged_) {
        Logger::write(Logger::ZONE_INFO, "LayoutMenu", "No layout changes to save");
        return;
    }

    Logger::write(Logger::ZONE_DEBUG, "LayoutMenu", "Starting save operation");

    // Build lookup tables
    std::map<std::string, std::map<std::tuple<std::string, std::string, std::string>, std::string>> fileSettings;
    std::map<std::pair<std::string, std::string>, std::map<std::string, bool>> sharedSettings;

    for (const auto& group : impl_->groups_) {
        for (const auto& item : group.items) {
            if (item.type != SettingType::Boolean || item.isCommented) continue;

            std::string subgroup = item.subgroup.empty() ? "" : item.subgroup;

            fileSettings[item.sourceFile][std::make_tuple(group.name, subgroup, item.key)] = item.value;

            std::string keyLower = item.key;
            std::transform(keyLower.begin(), keyLower.end(), keyLower.begin(), ::tolower);

            sharedSettings[{group.name, item.subgroup}][keyLower] = (item.value == "yes");
        }
    }

    // Helper: trim + lowercase
    auto trimAndLower = [](std::string s) -> std::string {
        s.erase(0, s.find_first_not_of(" \t\r\n"));
        s.erase(s.find_last_not_of(" \t\r\n") + 1);
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s;
        };

    for (const auto& filePath : impl_->parsedFiles_) {
        Logger::write(Logger::ZONE_INFO, "LayoutMenu", "Saving: " + filePath);

        xmlDocPtr doc = xmlReadFile(filePath.c_str(), nullptr, XML_PARSE_RECOVER | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
        if (!doc) {
            doc = xmlNewDoc((const xmlChar*)"1.0");
            xmlNodePtr root = xmlNewNode(nullptr, (const xmlChar*)"layout");
            xmlDocSetRootElement(doc, root);
        }

        xmlNodePtr root = xmlDocGetRootElement(doc);
        if (!root || xmlStrcmp(root->name, (const xmlChar*)"layout") != 0) {
            if (root) xmlFreeNode(root);
            root = xmlNewNode(nullptr, (const xmlChar*)"layout");
            xmlDocSetRootElement(doc, root);
        }

        int updatedCount = 0;

        for (xmlNodePtr elem = root->children; elem; elem = elem->next) {
            if (elem->type != XML_ELEMENT_NODE) continue;

            xmlChar* g = xmlGetProp(elem, (const xmlChar*)"group");
            xmlChar* sg = xmlGetProp(elem, (const xmlChar*)"subgroup");
            xmlChar* n = xmlGetProp(elem, (const xmlChar*)"name");
            xmlChar* sh = xmlGetProp(elem, (const xmlChar*)"shared");
            xmlChar* a = xmlGetProp(elem, (const xmlChar*)"active");

            std::string rawGroup = g ? substituteVariables((const char*)g) : "";
            std::string rawSubgroup = sg ? substituteVariables((const char*)sg) : "";
            std::string rawName = n ? substituteVariables((const char*)n) : "";
            std::string rawShared = sh ? substituteVariables((const char*)sh) : "";
            std::string rawActive = a ? substituteVariables((const char*)a) : "";

            std::string groupStr = trimAndLower(rawGroup);
            std::string subgroupStr = trimAndLower(rawSubgroup);
            std::string nameStr = trimAndLower(rawName);
            std::string sharedStr = trimAndLower(rawShared);

            xmlFree(g); xmlFree(sg); xmlFree(n); xmlFree(sh); xmlFree(a);

            if (groupStr.empty()) continue;

            bool elementUpdated = false;
            const char* finalActive = "false";

            // 1. Direct named setting
            if (!nameStr.empty()) {
                auto key = std::make_tuple(groupStr, subgroupStr, nameStr);
                auto fileIt = fileSettings.find(filePath);
                if (fileIt != fileSettings.end()) {
                    auto settingIt = fileIt->second.find(key);
                    if (settingIt != fileIt->second.end()) {
                        finalActive = (settingIt->second == "yes") ? "true" : "false";
                        elementUpdated = true;
                    }
                }
            }

            // 2. Shared logic
            if (!elementUpdated && !sharedStr.empty()) {
                std::vector<std::string> sharedNames;
                std::istringstream iss(sharedStr);
                std::string token;
                while (std::getline(iss, token, ',')) {
                    token = trimAndLower(token);
                    if (!token.empty()) sharedNames.push_back(token);
                }

                bool anyActive = false;
                auto sharedKey = std::make_pair(groupStr, subgroupStr);
                auto groupIt = sharedSettings.find(sharedKey);
                if (groupIt != sharedSettings.end()) {
                    for (const auto& name : sharedNames) {
                        auto it = groupIt->second.find(name);
                        if (it != groupIt->second.end() && it->second) {
                            anyActive = true;
                            break;
                        }
                    }
                }

                if (!sharedNames.empty()) {
                    finalActive = anyActive ? "true" : "false";
                    elementUpdated = true;
                }
            }

            // 3. Preserve valid active from file (with variable support)
            if (!elementUpdated && !nameStr.empty()) {
                std::string cur = trimAndLower(rawActive);
                if (cur == "true" || cur == "false") {
                    finalActive = cur.c_str();
                }
            }

            xmlSetProp(elem, (const xmlChar*)"active", (const xmlChar*)finalActive);
            if (elementUpdated) updatedCount++;
        }

        xmlSaveFormatFileEnc(filePath.c_str(), doc, "UTF-8", 1);
        Logger::write(Logger::ZONE_INFO, "LayoutMenu", "Saved " + filePath + " (" + std::to_string(updatedCount) + " updated)");
        xmlFreeDoc(doc);
    }

    impl_->layoutChanged_ = false;
    xmlCleanupParser();
    Logger::write(Logger::ZONE_INFO, "LayoutMenu", "Save completed successfully");
}

void LayoutMenu::navigate(bool moveUp) {
    if (impl_->groups_.empty()) {
        Logger::write(Logger::ZONE_WARNING, "LayoutMenu", "No groups to navigate");
        return;
    }

    size_t maxOption = impl_->inSubMenu_ ?
        (impl_->currentGroupIndex_ < impl_->groups_.size() ? impl_->groups_[impl_->currentGroupIndex_].items.size() : 0) :
        impl_->filteredGroupIndices_.size();

    if (maxOption == 0) return;
    int current = static_cast<int>(impl_->selectedOption_);
    int step = moveUp ? -1 : 1;
    int newOption = current;

    do {
        newOption += step;
        if (newOption < 0) {
            newOption = static_cast<int>(maxOption) - 1;
        }
        else if (newOption >= static_cast<int>(maxOption)) {
            newOption = 0;
        }
        if (newOption == current) break;

        if (impl_->inSubMenu_) {
            const Setting& setting = impl_->groups_[impl_->currentGroupIndex_].items[newOption];
            if (setting.type == SettingType::Boolean && !setting.isCommented) {
                break;
            }
        }
        else {
            break; // All groups are selectable in top-level
        }
    } while (newOption != current);

    if (newOption != current) {
        impl_->selectedOption_ = static_cast<size_t>(newOption);
        std::string itemText;
        if (impl_->inSubMenu_ && impl_->currentGroupIndex_ < impl_->groups_.size()) {
            itemText = impl_->groups_[impl_->currentGroupIndex_].items[newOption].displayText;
        } else if (newOption < static_cast<int>(impl_->filteredGroupIndices_.size())) {
            itemText = impl_->groups_[impl_->filteredGroupIndices_[newOption]].displayName;
        }
        Logger::write(Logger::ZONE_DEBUG, "LayoutMenu", "Navigated to option " + std::to_string(newOption) + ": " + itemText);
    }
}

std::string LayoutMenu::getSubgroupForOption(size_t index) {
    if (!impl_->inSubMenu_ || index >= impl_->groups_[impl_->currentGroupIndex_].items.size()) {
        return "";
    }
    for (int i = static_cast<int>(index); i >= 0; --i) {
        if (impl_->groups_[impl_->currentGroupIndex_].items[i].type == SettingType::Subgroup) {
            return impl_->groups_[impl_->currentGroupIndex_].items[i].subgroup;
        }
    }
    return "";
}

bool LayoutMenu::hasChanges() const {
    return impl_->layoutChanged_;
}

size_t LayoutMenu::getItemCount() const {
    if (!impl_) return 0;
    if (impl_->inSubMenu_) {
        if (impl_->currentGroupIndex_ < impl_->groups_.size()) {
            return impl_->groups_[impl_->currentGroupIndex_].items.size();
        }
        return 0;
    }
    return impl_->filteredGroupIndices_.size();
}

void LayoutMenu::setActiveMenuId(const std::string& menuId, const std::string& customTitle) {
    if (!impl_) return;
    impl_->activeMenuId_ = menuId;
    impl_->customMenuTitle_ = customTitle;
    impl_->filteredGroupIndices_.clear();

    std::string menuIdLower = Utils::toLower(menuId);
    bool isDefaultLayout = (menuIdLower.empty() || menuIdLower == "layout" || menuIdLower == "0");

    for (size_t i = 0; i < impl_->groups_.size(); ++i) {
        const auto& g = impl_->groups_[i];
        std::string gIdxLower = Utils::toLower(g.optMenuIndex);
        if (isDefaultLayout) {
            if (gIdxLower.empty() || gIdxLower == "layout" || gIdxLower == "0") {
                impl_->filteredGroupIndices_.push_back(i);
            }
        } else {
            if (gIdxLower == menuIdLower) {
                impl_->filteredGroupIndices_.push_back(i);
            }
        }
    }

    impl_->scrollIndex_ = 0;
    impl_->selectedOption_ = 0;

    // If there is exactly 1 group in this filtered menu and it's a custom menu, enter it directly!
    if (impl_->filteredGroupIndices_.size() == 1 && !isDefaultLayout) {
        impl_->inSubMenu_ = true;
        impl_->currentGroupIndex_ = impl_->filteredGroupIndices_[0];
        impl_->selectedOption_ = 0;
        navigate(true);
    } else {
        impl_->inSubMenu_ = false;
        impl_->currentGroupIndex_ = impl_->filteredGroupIndices_.empty() ? 0 : impl_->filteredGroupIndices_[0];
    }
}

std::vector<std::pair<std::string, std::string>> LayoutMenu::getCustomMenus() const {
    std::vector<std::pair<std::string, std::string>> result;
    if (!impl_) return result;
    std::set<std::string> seenIds;
    for (const auto& group : impl_->groups_) {
        std::string gIdxLower = Utils::toLower(group.optMenuIndex);
        if (!gIdxLower.empty() && gIdxLower != "layout" && gIdxLower != "0") {
            if (seenIds.find(gIdxLower) == seenIds.end()) {
                seenIds.insert(gIdxLower);
                result.push_back({ group.optMenuIndex, group.displayName });
            }
        }
    }
    return result;
}


std::string LayoutMenu::substituteVariables(std::string str) {
    if (str.empty() || str.find('$') == std::string::npos) {
        return str;
    }

    const auto& globalVariables = PageBuilder::getGlobalVariables();

    // Critical: sort keys by length (longest first)
    // This prevents shorter keys like "list" from destroying longer ones like "listbottom"
    std::vector<std::pair<std::string, std::string>> sorted;
    sorted.reserve(globalVariables.size());
    for (const auto& pair : globalVariables) {
        sorted.emplace_back(pair.first, pair.second);
    }

    std::sort(sorted.begin(), sorted.end(),
        [](const auto& a, const auto& b) {
            return a.first.length() > b.first.length();
        });

    for (const auto& pair : sorted) {
        // Support both ${var} and $var forms
        str = Utils::replace(str, "${" + pair.first + "}", pair.second);
        str = Utils::replace(str, "$" + pair.first, pair.second);
    }

    return str;
}