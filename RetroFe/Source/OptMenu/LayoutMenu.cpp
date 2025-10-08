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
#include <SDL_image.h>

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
};

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

        std::string originalGroup = trim(groupAttr);
        std::string groupStr = trimAndLower(groupAttr);
        std::string originalSubgroup = trim(subgroupAttr);
        std::string subgroupStr = trimAndLower(subgroupAttr);
        std::string originalName = trim(nameAttr);
        std::string nameStr = trimAndLower(nameAttr);
        std::string activeStr = trimAndLower(activeAttr);
        std::string menuSrcStr = trim(menuSrcAttr);
        std::string srcStr = srcAttr ? trim(srcAttr) : "";

        xmlFree(groupAttr); xmlFree(subgroupAttr); xmlFree(nameAttr);
        xmlFree(activeAttr); xmlFree(menuSrcAttr); xmlFree(srcAttr);

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
                impl_->groups_.push_back(Group{ groupStr, originalGroup, {} });
                group = &impl_->groups_.back();
                Logger::write(Logger::ZONE_INFO, "LayoutMenu", "Created new group: " + groupStr);
            }
            else {
                group = &(*groupIt);
            }

            auto subgroupIt = std::find_if(group->items.begin(), group->items.end(),
                [&subgroupStr](const Setting& s) { return s.key == "subgroup_" + subgroupStr; });
            if (subgroupIt == group->items.end()) {
                group->items.push_back(Setting{
                    "subgroup_" + subgroupStr, "", true, SettingType::Subgroup,
                    originalSubgroup, originalSubgroup,
                    {}, 0, 0, "", groupStr, subgroupStr, filePath, ""
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
                    menuSrcStr
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
}

MenuAction LayoutMenu::handleInput(SDL_Event& event) {
    if (event.type == SDL_KEYDOWN || event.type == SDL_JOYHATMOTION || event.type == SDL_JOYBUTTONDOWN) {
        bool menuUp = (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_UP) ||
            (event.type == SDL_JOYHATMOTION && event.jhat.value == SDL_HAT_UP);
        bool menuDown = (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_DOWN) ||
            (event.type == SDL_JOYHATMOTION && event.jhat.value == SDL_HAT_DOWN);
        bool menuSelect = (event.type == SDL_KEYDOWN && (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER)) ||
            (event.type == SDL_JOYBUTTONDOWN && event.jbutton.button == 0);
        bool menuBack = (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) ||
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
        else if (menuSelect && impl_->selectedOption_ < (impl_->inSubMenu_ ? impl_->groups_[impl_->currentGroupIndex_].items.size() : impl_->groups_.size())) {
            if (!impl_->inSubMenu_) {
                // Enter sub-menu
                impl_->inSubMenu_ = true;
                impl_->currentGroupIndex_ = impl_->selectedOption_;
                impl_->selectedOption_ = 0;
                navigate(true); // Move to first selectable setting
                Logger::write(Logger::ZONE_INFO, "LayoutMenu", "Entered sub-menu for group: " + impl_->groups_[impl_->currentGroupIndex_].displayName);
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
                // Return to top-level
                impl_->inSubMenu_ = false;
                impl_->selectedOption_ = impl_->currentGroupIndex_;
                Logger::write(Logger::ZONE_INFO, "LayoutMenu", "Returned to top-level menu from group: " + impl_->groups_[impl_->currentGroupIndex_].displayName);
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
            Logger::write(Logger::ZONE_WARNING, "LayoutMenu", "Invalid image x alignment, using center");
        }
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
            Logger::write(Logger::ZONE_WARNING, "LayoutMenu", "Invalid image y alignment, using numeric");
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
            float textWidth = 0.0f;
            for (char c : selectedName) {
                if (c < 32 || c > 127) continue;
                Font::GlyphInfo glyph;
                if (optionFont->getRect(static_cast<unsigned char>(c), glyph)) {
                    textWidth += glyph.advance;
                }
            }
            int textX = destRect.x + (destRect.w - static_cast<int>(textWidth)) / 2;
            int textY = destRect.y + destRect.h + static_cast<int>(10 * scaleFactor);
            float textRenderX = static_cast<float>(textX);
            SDL_SetTextureColorMod(optionFont->getTexture(), config.getOptionColor().r, config.getOptionColor().g, config.getOptionColor().b);
            for (char c : selectedName) {
                if (c < 32 || c > 127) continue;
                Font::GlyphInfo glyph;
                if (optionFont->getRect(static_cast<unsigned char>(c), glyph)) {
                    SDL_Rect textDestRect = { static_cast<int>(textRenderX), textY, glyph.rect.w, glyph.rect.h };
                    SDL_RenderCopy(renderer, optionFont->getTexture(), &glyph.rect, &textDestRect);
                    textRenderX += glyph.advance;
                }
            }
            Logger::write(Logger::ZONE_INFO, "LayoutMenu", "Rendered selected name: " + selectedName +
                " at x=" + std::to_string(textX) + ", y=" + std::to_string(textY));
        }
    }

    // Render main title (top-level) or group title (sub-menu)
    std::string title = impl_->inSubMenu_ ? impl_->groups_[impl_->currentGroupIndex_].displayName : config.getLayoutTitleText();
    float titleWidth = 0.0f;
    for (char c : title) {
        if (c < 32 || c > 127) continue;
        Font::GlyphInfo glyph;
        if (titleFont->getRect(static_cast<unsigned char>(c), glyph)) {
            titleWidth += glyph.advance;
        }
    }
    int scaledTitleXOffset = static_cast<int>(config.getTitleXOffset() * scaleFactor);
    int titleX;
    switch (config.getTitleXAlignment()) {
    case OptionsMenuBuilder::XAlignment::Center:
        titleX = (windowWidth - static_cast<int>(titleWidth)) / 2 + scaledTitleXOffset;
        break;
    case OptionsMenuBuilder::XAlignment::Left:
        titleX = scaledTitleXOffset;
        break;
    case OptionsMenuBuilder::XAlignment::Right:
        titleX = windowWidth - static_cast<int>(titleWidth) + scaledTitleXOffset;
        break;
    case OptionsMenuBuilder::XAlignment::Numeric:
        titleX = scaledTitleXOffset;
        break;
    default:
        titleX = (windowWidth - static_cast<int>(titleWidth)) / 2;
        Logger::write(Logger::ZONE_WARNING, "LayoutMenu", "Invalid title x alignment, using center");
    }
    int scaledTitleYOffset = static_cast<int>(config.getTitleYOffset() * scaleFactor);
    int titleY;
    switch (config.getTitleYAlignment()) {
    case OptionsMenuBuilder::YAlignment::Center:
        titleY = (windowHeight - titleFont->getHeight()) / 2 + scaledTitleYOffset;
        break;
    case OptionsMenuBuilder::YAlignment::Top:
        titleY = scaledTitleYOffset;
        break;
    case OptionsMenuBuilder::YAlignment::Bottom:
        titleY = windowHeight - titleFont->getHeight() + scaledTitleYOffset;
        break;
    case OptionsMenuBuilder::YAlignment::Numeric:
        titleY = scaledTitleYOffset;
        break;
    default:
        titleY = static_cast<int>(50 * scaleFactor);
        Logger::write(Logger::ZONE_WARNING, "LayoutMenu", "Invalid title y alignment, using top+50 scaled");
    }
    Logger::write(Logger::ZONE_INFO, "LayoutMenu", "Title alignment: xAlign=" +
        std::string(config.getTitleXAlignment() == OptionsMenuBuilder::XAlignment::Center ? "center" :
            config.getTitleXAlignment() == OptionsMenuBuilder::XAlignment::Left ? "left" :
            config.getTitleXAlignment() == OptionsMenuBuilder::XAlignment::Right ? "right" : "numeric") +
        ", xOffset=" + std::to_string(config.getTitleXOffset()) +
        ", yAlign=" + std::string(config.getTitleYAlignment() == OptionsMenuBuilder::YAlignment::Center ? "center" :
            config.getTitleYAlignment() == OptionsMenuBuilder::YAlignment::Top ? "top" :
            config.getTitleYAlignment() == OptionsMenuBuilder::YAlignment::Bottom ? "bottom" : "numeric") +
        ", yOffset=" + std::to_string(config.getTitleYOffset()));
    Logger::write(Logger::ZONE_INFO, "LayoutMenu", "Title position: x=" + std::to_string(titleX) +
        ", y=" + std::to_string(titleY) + ", titleWidth=" + std::to_string(titleWidth));
    if (impl_->inSubMenu_) {
        int scaledPadding = static_cast<int>(config.getSelectionBarPadding() * scaleFactor);
        int scaledBarYOffset = static_cast<int>(config.getSelectionBarYOffset() * scaleFactor);
        int scaledBarHeight = static_cast<int>(config.getSelectionBarHeight() * scaleFactor);
        SDL_Rect titleBarRect = {
            static_cast<int>(titleX - scaledPadding),
            static_cast<int>(titleY + scaledBarYOffset),
            static_cast<int>(titleWidth + 2 * scaledPadding),
            scaledBarHeight
        };
        SDL_SetRenderDrawColor(renderer, config.getTitleBarColor().r, config.getTitleBarColor().g,
            config.getTitleBarColor().b, config.getTitleBarColor().a);
        SDL_RenderFillRect(renderer, &titleBarRect);
    }
    float renderX = static_cast<float>(titleX);
    for (char c : title) {
        if (c < 32 || c > 127) continue;
        Font::GlyphInfo glyph;
        if (titleFont->getRect(static_cast<unsigned char>(c), glyph)) {
            SDL_Rect destRect = { static_cast<int>(renderX), titleY, glyph.rect.w, glyph.rect.h };
            SDL_SetTextureColorMod(titleFont->getTexture(), config.getTitleColor().r,
                config.getTitleColor().g, config.getTitleColor().b);
            SDL_RenderCopy(renderer, titleFont->getTexture(), &glyph.rect, &destRect);
            renderX += glyph.advance;
        }
    }
    int scaledMenuYOffset = static_cast<int>(config.getMenuYOffset() * scaleFactor);
    int scaledTitleSpacing = static_cast<int>(config.getTitleSpacing() * scaleFactor);
    int menuY;
    switch (config.getMenuYAlignment()) {
    case OptionsMenuBuilder::YAlignment::Center:
        menuY = windowHeight / 2 + scaledMenuYOffset;
        break;
    case OptionsMenuBuilder::YAlignment::Top:
        menuY = scaledMenuYOffset;
        break;
    case OptionsMenuBuilder::YAlignment::Bottom:
        menuY = windowHeight + scaledMenuYOffset;
        break;
    case OptionsMenuBuilder::YAlignment::Numeric:
        menuY = scaledMenuYOffset;
        menuY = std::max(menuY, titleY + titleFont->getHeight() + scaledTitleSpacing);
        break;
    default:
        menuY = titleY + titleFont->getHeight() + scaledTitleSpacing;
        Logger::write(Logger::ZONE_WARNING, "LayoutMenu", "Invalid menu y alignment, using titleY+height+20 scaled");
    }
    int scaledSpaceBetween = static_cast<int>(config.getSpaceBetweenText() * scaleFactor);
    int maxVisibleLines = (windowHeight - menuY) / scaledSpaceBetween;
    int scrollOffset = 0;
    const auto& options = impl_->inSubMenu_ ? impl_->groups_[impl_->currentGroupIndex_].items : std::vector<Setting>{};
    size_t itemCount = impl_->inSubMenu_ ? options.size() : impl_->groups_.size();
    if (itemCount > static_cast<size_t>(maxVisibleLines)) {
        if (impl_->selectedOption_ >= static_cast<size_t>(maxVisibleLines / 2)) {
            scrollOffset = static_cast<int>(impl_->selectedOption_ - maxVisibleLines / 2) * scaledSpaceBetween;
            int maxOffset = static_cast<int>((itemCount - maxVisibleLines) * scaledSpaceBetween);
            scrollOffset = std::min(scrollOffset, maxOffset);
        }
        else {
            scrollOffset = 0;
        }
    }
    int scaledPadding = static_cast<int>(config.getSelectionBarPadding() * scaleFactor);
    int scaledBarHeight = static_cast<int>(config.getSelectionBarHeight() * scaleFactor);
    int scaledBarYOffset = static_cast<int>(config.getSelectionBarYOffset() * scaleFactor);
    for (size_t i = 0; i < itemCount; ++i) {
        std::string text;
        bool isGroup = false;
        bool isSubgroup = false;
        bool isSelectable = false;
        if (impl_->inSubMenu_) {
            const Setting& setting = options[i];
            text = setting.displayText;
            isSubgroup = setting.type == SettingType::Subgroup;
            isSelectable = setting.type == SettingType::Boolean && !setting.isCommented;
        }
        else {
            text = impl_->groups_[i].displayName;
            isGroup = true;
            isSelectable = true;
        }
        float textWidth = 0.0f;
        for (char c : text) {
            if (c < 32 || c > 127) continue;
            Font::GlyphInfo glyph;
            if (optionFont->getRect(static_cast<unsigned char>(c), glyph)) {
                textWidth += glyph.advance;
            }
        }
        int scaledMenuXOffset = static_cast<int>(config.getMenuXOffset() * scaleFactor);
        int optionX;
        switch (config.getMenuXAlignment()) {
        case OptionsMenuBuilder::XAlignment::Center:
            optionX = (windowWidth - static_cast<int>(textWidth)) / 2 + scaledMenuXOffset;
            break;
        case OptionsMenuBuilder::XAlignment::Left:
            optionX = scaledMenuXOffset;
            break;
        case OptionsMenuBuilder::XAlignment::Right:
            optionX = windowWidth - static_cast<int>(textWidth) + scaledMenuXOffset;
            break;
        case OptionsMenuBuilder::XAlignment::Numeric:
            optionX = scaledMenuXOffset;
            break;
        default:
            optionX = (windowWidth - static_cast<int>(textWidth)) / 2;
            Logger::write(Logger::ZONE_WARNING, "LayoutMenu", "Invalid menu x alignment, using center");
        }
        float x = static_cast<float>(optionX);
        float y = menuY + static_cast<float>(i) * scaledSpaceBetween - scrollOffset;
        if (y < menuY || y + scaledSpaceBetween < 0 || y >= windowHeight) continue;
        bool isSelected = (i == impl_->selectedOption_);
        if (isSelected && isSelectable) {
            SDL_Rect barRect = {
         static_cast<int>(optionX - scaledPadding),
         static_cast<int>(y + scaledBarYOffset),
         static_cast<int>(textWidth + 2 * scaledPadding),
         scaledBarHeight
            };
            SDL_Color barColor = config.getSelectionBarColor();
            SDL_SetRenderDrawColor(renderer, barColor.r, barColor.g, barColor.b, barColor.a);
            Logger::write(Logger::ZONE_DEBUG, "LayoutMenu", "Rendering selection bar: R=" +
                std::to_string(barColor.r) + ", G=" + std::to_string(barColor.g) + ", B=" +
                std::to_string(barColor.b) + ", A=" + std::to_string(barColor.a));
            SDL_RenderFillRect(renderer, &barRect);
        }
        else if (isSubgroup) {
            float underlineY = y + optionFont->getHeight() + static_cast<int>(2 * scaleFactor);
            SDL_SetRenderDrawColor(renderer, config.getSubtitleUnderlineColor().r, config.getSubtitleUnderlineColor().g,
                config.getSubtitleUnderlineColor().b, config.getSubtitleUnderlineColor().a);
            SDL_RenderDrawLine(renderer, static_cast<int>(optionX), static_cast<int>(underlineY),
                static_cast<int>(optionX + textWidth), static_cast<int>(underlineY));
        }
        SDL_Color lineColor = isSelected && isSelectable ? config.getTitleColor() : config.getOptionColor();
        renderX = x;
        for (char c : text) {
            if (c < 32 || c > 127) continue;
            Font::GlyphInfo glyph;
            if (optionFont->getRect(static_cast<unsigned char>(c), glyph)) {
                SDL_Rect destRect = { static_cast<int>(renderX), static_cast<int>(y), glyph.rect.w, glyph.rect.h };
                SDL_SetTextureColorMod(optionFont->getTexture(), lineColor.r, lineColor.g, lineColor.b);
                SDL_RenderCopy(renderer, optionFont->getTexture(), &glyph.rect, &destRect);
                renderX += glyph.advance;
            }
        }
    }
    Logger::write(Logger::ZONE_INFO, "LayoutMenu", "Options alignment: xAlign=" +
        std::string(config.getMenuXAlignment() == OptionsMenuBuilder::XAlignment::Center ? "center" :
            config.getMenuXAlignment() == OptionsMenuBuilder::XAlignment::Left ? "left" :
            config.getMenuXAlignment() == OptionsMenuBuilder::XAlignment::Right ? "right" : "numeric") +
        ", xOffset=" + std::to_string(config.getMenuXOffset()) +
        ", yAlign=" + std::string(config.getMenuYAlignment() == OptionsMenuBuilder::YAlignment::Center ? "center" :
            config.getMenuYAlignment() == OptionsMenuBuilder::YAlignment::Top ? "top" :
            config.getMenuYAlignment() == OptionsMenuBuilder::YAlignment::Bottom ? "bottom" : "numeric") +
        ", yOffset=" + std::to_string(config.getMenuYOffset()));
    Logger::write(Logger::ZONE_INFO, "LayoutMenu", "Options position: y=" + std::to_string(menuY) +
        ", scrollOffset=" + std::to_string(scrollOffset));
}


void LayoutMenu::save() {
    if (!impl_->layoutChanged_) {
        Logger::write(Logger::ZONE_INFO, "LayoutMenu", "No layout changes to save");
        return;
    }
    Logger::write(Logger::ZONE_DEBUG, "LayoutMenu", "Starting save operation for " + std::to_string(impl_->parsedFiles_.size()) + " parsed files");

    // Map settings by source file, group, subgroup, and name for named elements
    std::map<std::string, std::map<std::tuple<std::string, std::string, std::string>, std::string>> fileSettings;

    // UPDATED: Change sharedSettings to use std::pair<group, subgroup> as the key
    // This ensures shared attributes are scoped to subgroup
    std::map<std::pair<std::string, std::string>, std::map<std::string, bool>> sharedSettings;

    for (const auto& group : impl_->groups_) {
        for (const auto& item : group.items) {
            if (item.type == SettingType::Boolean && !item.isCommented) {
                std::string subgroup = item.subgroup.empty() ? "" : item.subgroup;
                fileSettings[item.sourceFile][std::make_tuple(group.name, subgroup, item.key)] = item.value;

                std::string keyLower = item.key;
                std::transform(keyLower.begin(), keyLower.end(), keyLower.begin(), ::tolower);

                // UPDATED: Use pair of group and subgroup as key
                sharedSettings[std::make_pair(group.name, item.subgroup)][keyLower] = (item.value == "yes");

                Logger::write(Logger::ZONE_DEBUG, "LayoutMenu", "Mapped setting: key=" + item.key + ", group=" + group.name + ", subgroup=" + subgroup + ", value=" + item.value + ", sourceFile=" + item.sourceFile + ", sharedKey=" + keyLower);
            }
        }
    }

    for (const auto& filePath : impl_->parsedFiles_) {
        Logger::write(Logger::ZONE_INFO, "LayoutMenu", "Processing file for save: " + filePath);
        xmlDocPtr doc = xmlReadFile(filePath.c_str(), nullptr, XML_PARSE_RECOVER | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
        xmlNodePtr root = nullptr;
        if (!doc) {
            Logger::write(Logger::ZONE_WARNING, "LayoutMenu", "Creating new XML doc for " + filePath);
            doc = xmlNewDoc((const xmlChar*)"1.0");
            root = xmlNewNode(nullptr, (const xmlChar*)"layout");
            xmlDocSetRootElement(doc, root);
        }
        else {
            root = xmlDocGetRootElement(doc);
            if (!root || xmlStrcmp(root->name, (const xmlChar*)"layout") != 0) {
                Logger::write(Logger::ZONE_WARNING, "LayoutMenu", "Invalid root in " + filePath + "; creating new root");
                if (root) xmlFreeNode(root);
                root = xmlNewNode(nullptr, (const xmlChar*)"layout");
                xmlDocSetRootElement(doc, root);
            }
        }

        auto trimAndLower = [](const xmlChar* str) -> std::string {
            if (!str) return "";
            std::string s = reinterpret_cast<const char*>(str);
            s.erase(0, s.find_first_not_of(" \t"));
            s.erase(s.find_last_not_of(" \t") + 1);
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            return s;
            };

        int updatedCount = 0;
        int skippedCount = 0;
        Logger::write(Logger::ZONE_DEBUG, "LayoutMenu", "Processing elements in " + filePath);
        for (xmlNodePtr elem = root->children; elem; elem = elem->next) {
            if (elem->type != XML_ELEMENT_NODE) {
                skippedCount++;
                Logger::write(Logger::ZONE_DEBUG, "LayoutMenu", "Skipped non-element node in " + filePath);
                continue;
            }
            std::string tagName = trimAndLower(elem->name);
            xmlChar* group = xmlGetProp(elem, (const xmlChar*)"group");
            xmlChar* subgroup = xmlGetProp(elem, (const xmlChar*)"subgroup");
            xmlChar* name = xmlGetProp(elem, (const xmlChar*)"name");
            xmlChar* shared = xmlGetProp(elem, (const xmlChar*)"shared");
            std::string groupStr = trimAndLower(group);
            std::string subgroupStr = trimAndLower(subgroup);
            std::string nameStr = trimAndLower(name);
            std::string sharedStr = trimAndLower(shared);
            Logger::write(Logger::ZONE_DEBUG, "LayoutMenu", "Element <" + tagName + "> in " + filePath + ": group=" + groupStr + ", subgroup=" + subgroupStr + ", name=" + nameStr + ", shared=" + sharedStr);

            // Skip elements with empty group
            if (groupStr.empty()) {
                skippedCount++;
                Logger::write(Logger::ZONE_DEBUG, "LayoutMenu", "Skipped element in " + filePath + ": empty group");
                xmlFree(group);
                xmlFree(subgroup);
                xmlFree(name);
                xmlFree(shared);
                continue;
            }

            bool updated = false;

            // Update elements with name (e.g., or with name)
            if (!nameStr.empty()) {
                auto& settingsMap = fileSettings[filePath];
                auto key = std::make_tuple(groupStr, subgroupStr, nameStr);
                auto it = settingsMap.find(key);
                if (it != settingsMap.end()) {
                    const char* newActive = (it->second == "yes") ? "true" : "false";
                    xmlSetProp(elem, (const xmlChar*)"active", (const xmlChar*)newActive);
                    updated = true;
                    updatedCount++;
                    Logger::write(Logger::ZONE_DEBUG, "LayoutMenu", "Updated named element in " + filePath + ": name=" + nameStr + ", group=" + groupStr + ", subgroup=" + subgroupStr + ", active=" + newActive);
                }
                else {
                    Logger::write(Logger::ZONE_DEBUG, "LayoutMenu", "No matching setting for named element in " + filePath + ": name=" + nameStr + ", group=" + groupStr + ", subgroup=" + subgroupStr);
                }
            }

            // Update elements with shared attributes (including those with name)
            if (!sharedStr.empty()) {
                std::vector<std::string> sharedList;
                std::string sharedLower = sharedStr;
                size_t pos = 0;
                while ((pos = sharedLower.find(',')) != std::string::npos) {
                    sharedList.push_back(trimAndLower((const xmlChar*)sharedLower.substr(0, pos).c_str()));
                    sharedLower.erase(0, pos + 1);
                }
                sharedList.push_back(trimAndLower((const xmlChar*)sharedLower.c_str()));

                bool shouldBeActive = false;
                // UPDATED: Use pair of groupStr and subgroupStr for lookup
                auto groupIt = sharedSettings.find(std::make_pair(groupStr, subgroupStr));
                if (groupIt != sharedSettings.end()) {
                    for (const auto& sharedName : sharedList) {
                        auto sharedIt = groupIt->second.find(sharedName);
                        if (sharedIt != groupIt->second.end() && sharedIt->second) {
                            shouldBeActive = true;
                            break;
                        }
                    }
                }
                const char* newActive = shouldBeActive ? "true" : "false";
                xmlSetProp(elem, (const xmlChar*)"active", (const xmlChar*)newActive);
                updated = true;
                updatedCount++;
                Logger::write(Logger::ZONE_DEBUG, "LayoutMenu", "Updated shared element in " + filePath + ": shared=" + sharedStr + ", name=" + nameStr + ", group=" + groupStr + ", subgroup=" + subgroupStr + ", active=" + newActive);
            }

            // Handle unmatched named elements to preserve or default active state
            if (!updated && !nameStr.empty()) {
                xmlChar* active = xmlGetProp(elem, (const xmlChar*)"active");
                std::string currentActive = trimAndLower(active);
                if (active && (currentActive == "true" || currentActive == "false")) {
                    xmlSetProp(elem, (const xmlChar*)"active", (const xmlChar*)currentActive.c_str());
                    Logger::write(Logger::ZONE_DEBUG, "LayoutMenu", "Preserved active state for element in " + filePath + ": name=" + nameStr + ", group=" + groupStr + ", active=" + currentActive);
                }
                else {
                    xmlSetProp(elem, (const xmlChar*)"active", (const xmlChar*)"false");
                    Logger::write(Logger::ZONE_DEBUG, "LayoutMenu", "Set default active=false for element in " + filePath + ": name=" + nameStr + ", group=" + groupStr);
                }
                xmlFree(active);
            }
            else if (!updated) {
                Logger::write(Logger::ZONE_DEBUG, "LayoutMenu", "No update for element in " + filePath + ": name=" + nameStr + ", group=" + groupStr + ", shared=" + sharedStr);
            }

            xmlFree(group);
            xmlFree(subgroup);
            xmlFree(name);
            xmlFree(shared);
        }

        Logger::write(Logger::ZONE_INFO, "LayoutMenu", "Saving file " + filePath + ": " + std::to_string(updatedCount) + " elements updated, " + std::to_string(skippedCount) + " skipped");
        xmlSaveFormatFileEnc(filePath.c_str(), doc, "UTF-8", 1);
        if (std::filesystem::exists(filePath) && std::filesystem::file_size(filePath) > 0) {
            Logger::write(Logger::ZONE_INFO, "LayoutMenu", "Successfully saved " + filePath);
        }
        else {
            Logger::write(Logger::ZONE_ERROR, "LayoutMenu", "Failed to save " + filePath + ": file not created or empty");
        }
        xmlFreeDoc(doc);
    }

    impl_->layoutChanged_ = false;
    xmlCleanupParser();
    Logger::write(Logger::ZONE_INFO, "LayoutMenu", "Save operation completed");
}

void LayoutMenu::navigate(bool moveUp) {
    if (impl_->groups_.empty()) {
        Logger::write(Logger::ZONE_WARNING, "LayoutMenu", "No groups to navigate");
        return;
    }

    size_t maxOption = impl_->inSubMenu_ ? impl_->groups_[impl_->currentGroupIndex_].items.size() : impl_->groups_.size();
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
        std::string itemText = impl_->inSubMenu_ ? impl_->groups_[impl_->currentGroupIndex_].items[newOption].displayText : impl_->groups_[newOption].displayName;
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