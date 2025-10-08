#include "SettingsMenu.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include "../utility/Log.h"
#include "../Utility/Utils.h"
#include "OptionsMenuBuilder.h"
#include "../Graphics/Font.h"

SettingsMenu::SettingsMenu(const std::string& settingsFile, Configuration& config)
    : settingsFile_(settingsFile), config_(config), selectedOption_(0), editingIndex_(-1), editingText_(false), hasChanges_(false) {
    initSettingsOptionsFromFile();
}

void SettingsMenu::initSettingsOptionsFromFile() {
    std::ifstream inFile(settingsFile_);
    std::vector<Setting> settingsOptions;
    originalSettingsLines_.clear();

    if (inFile.is_open()) {
        std::string line;
        while (std::getline(inFile, line)) {
            originalSettingsLines_.push_back(line);
            std::string trimmedLine = line;
            trimmedLine.erase(0, trimmedLine.find_first_not_of(" \t"));
            if (trimmedLine.empty()) continue;

            size_t hashCount = 0;
            while (hashCount < trimmedLine.length() && trimmedLine[hashCount] == '#') {
                hashCount++;
            }

            if (hashCount >= 4) {
                continue;
            }

            if (hashCount == 2 || hashCount == 3) {
                std::string text = trimmedLine.substr(hashCount);
                text.erase(0, text.find_first_not_of(" \t"));
                text.erase(text.find_last_not_of(" \t") + 1);
                if (text.empty()) continue;

                Setting setting{
                      text,                         // key
                      "",                           // value
                      false,                        // isCommented
                      hashCount == 2 ? SettingType::Title : SettingType::Subtitle, // type
                      text,                         // displayText
                      text,                         // originalName
                      {},                           // validValues
                      0.0f,                         // minValue
                      0.0f,                         // maxValue
                      "",                           // description
                      "",                           // group
                      "",                           // subgroup
                      settingsFile_                 // sourceFile
                };
                settingsOptions.push_back(setting);
#ifdef _DEBUG
                Logger::write(Logger::ZONE_DEBUG, "SettingsMenu", "Loaded " + std::string(hashCount == 2 ? "title" : "subtitle") + ": " + text);
#endif
                continue;
            }

            bool isCommented = (hashCount == 1);
            std::string keyValue = isCommented ? trimmedLine.substr(1) : trimmedLine;
            keyValue.erase(0, keyValue.find_first_not_of(" \t"));

            size_t equalsPos = keyValue.find('=');
            if (equalsPos == std::string::npos) continue;

            std::string key = keyValue.substr(0, equalsPos);
            key.erase(key.find_last_not_of(" \t") + 1);

            std::string rest = keyValue.substr(equalsPos + 1);
            rest.erase(0, rest.find_first_not_of(" \t"));
            size_t commentPos = rest.find('#');
            std::string value = (commentPos != std::string::npos) ? rest.substr(0, commentPos) : rest;
            value.erase(value.find_last_not_of(" \t") + 1);

            if (value == "true") value = "yes";
            else if (value == "false") value = "no";

            SettingType type;
            std::vector<std::string> validValues;
            float minValue = 0.0f;  
            float maxValue = 9999.0f;  
            if (value == "yes" || value == "no") {
                type = SettingType::Boolean;
                validValues = { "yes", "no" };
            }
            else if (std::all_of(value.begin(), value.end(), ::isdigit)) {
                type = SettingType::Numeric;
                minValue = 0.0f;  
                maxValue = 9999.0f;  
            }
            else {
                type = SettingType::String;
            }

            if (key.find("Path") != std::string::npos || key.find("path") != std::string::npos) {
                type = SettingType::Path;
            }

            std::string displayText = key + ": " + value + (isCommented ? " (disabled)" : "");
            Setting setting{
                   key,                          // key
                   value,                        // value
                   isCommented,                  // isCommented
                   type,                         // type
                   displayText,                  // displayText
                   key,                          // originalName
                   validValues,                  // validValues
                   minValue,                     // minValue
                   maxValue,                     // maxValue
                   "",                           // description
                   "",                           // group
                   "",                           // subgroup
                   settingsFile_                 // sourceFile
            };
            settingsOptions.push_back(setting);
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "SettingsMenu", "Loaded setting: " + displayText);
#endif
        }
        inFile.close();
    }
    else {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_WARNING, "SettingsMenu", "Could not read " + settingsFile_ + "; no settings loaded");
#endif
    }

    settingsOptions_ = settingsOptions;
#ifdef _DEBUG
    Logger::write(Logger::ZONE_INFO, "SettingsMenu", "Loaded " + std::to_string(settingsOptions_.size()) + " items from " + settingsFile_);
#endif
}

MenuAction SettingsMenu::handleInput(SDL_Event& event) {
     static bool rightTriggerHeld = false;
    if (event.type == SDL_JOYAXISMOTION && event.jaxis.axis == 5) {
        rightTriggerHeld = (event.jaxis.value > 16384);
    }
    static bool leftTriggerHeld = false;
    if (event.type == SDL_JOYAXISMOTION && event.jaxis.axis == 4) {
        leftTriggerHeld = (event.jaxis.value > 16384);
    }
    if (editingText_) {
        Setting& setting = settingsOptions_[editingIndex_];
        if (setting.type == SettingType::Title || setting.type == SettingType::Subtitle) {
            editingText_ = false;
            inputText_.clear();
            SDL_StopTextInput();
            return MenuAction::STAY;
        }

        if (event.type == SDL_TEXTINPUT) {
            std::string newText = event.text.text;
            if (setting.type == SettingType::Numeric) {
                // Allow digits and one decimal point
                if (std::all_of(newText.begin(), newText.end(), [](char c) { return ::isdigit(c) || c == '.'; }) &&
                    std::count(inputText_.begin(), inputText_.end(), '.') <= 1) {
                    inputText_ += newText;
                    setting.displayText = setting.key + ": " + inputText_ + (setting.isCommented ? " (disabled)" : "") + "|";
                }
            }
            else if (setting.type == SettingType::String || setting.type == SettingType::Path) {
                inputText_ += newText;
                setting.displayText = setting.key + ": " + inputText_ + (setting.isCommented ? " (disabled)" : "") + "|";
            }
        }
        else if (event.type == SDL_KEYDOWN || event.type == SDL_JOYHATMOTION || event.type == SDL_JOYBUTTONDOWN) {
            SDL_Keymod mod = SDL_GetModState();
            bool ctrlPressed = (mod & KMOD_CTRL) != 0;

            bool menuSelect = (event.type == SDL_KEYDOWN && (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER)) ||
                (event.type == SDL_JOYBUTTONDOWN && event.jbutton.button == 0);
            bool menuBack = (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) ||
                (event.type == SDL_JOYBUTTONDOWN && event.jbutton.button == 1);
            bool deleteChar = (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_BACKSPACE) ||
                (event.type == SDL_JOYBUTTONDOWN && event.jbutton.button == 3);
            bool increaseSmall = (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RIGHT && !ctrlPressed) ||
                (event.type == SDL_JOYHATMOTION && event.jhat.value == SDL_HAT_RIGHT && !rightTriggerHeld);
            bool decreaseSmall = (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_LEFT && !ctrlPressed) ||
                (event.type == SDL_JOYHATMOTION && event.jhat.value == SDL_HAT_LEFT && !leftTriggerHeld);
            bool increaseLarge = (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RIGHT && ctrlPressed) ||
                (event.type == SDL_JOYHATMOTION && event.jhat.value == SDL_HAT_RIGHT && rightTriggerHeld);
            bool decreaseLarge = (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_LEFT && ctrlPressed) ||
                (event.type == SDL_JOYHATMOTION && event.jhat.value == SDL_HAT_LEFT && leftTriggerHeld);

            if (menuSelect) {
                if (setting.type == SettingType::Numeric && !inputText_.empty()) {
                    if (!setting.validValues.empty()) {
                        if (std::find(setting.validValues.begin(), setting.validValues.end(), inputText_) == setting.validValues.end()) {
                            inputText_ = setting.validValues[0];
                        }
                    }
                    else {
                        try {
                            float value = std::stof(inputText_);
                            if (value < setting.minValue) value = setting.minValue;
                            if (value > setting.maxValue) value = setting.maxValue;
                            inputText_ = std::to_string(value);
                        }
                        catch (const std::exception&) {
                            inputText_ = std::to_string(setting.minValue);
                        }
                    }
                }
                setting.value = inputText_;
                setting.displayText = setting.key + ": " + inputText_ + (setting.isCommented ? " (disabled)" : "");
                pendingChanges_[setting.key] = inputText_;
                hasChanges_ = true;
                editingText_ = false;
                inputText_.clear();
                SDL_StopTextInput();
            }
            else if (menuBack) {
                editingText_ = false;
                inputText_.clear();
                setting.displayText = setting.key + ": " + setting.value + (setting.isCommented ? " (disabled)" : "");
                SDL_StopTextInput();
            }
            else if (deleteChar) {
                if (!inputText_.empty()) {
                    inputText_.pop_back();
                    setting.displayText = setting.key + ": " + inputText_ + (setting.isCommented ? " (disabled)" : "") + "|";
                }
            }
            else if (increaseSmall || decreaseSmall || increaseLarge || decreaseLarge) {
                if (!setting.validValues.empty()) {
                    size_t currentIdx = 0;
                    for (size_t i = 0; i < setting.validValues.size(); ++i) {
                        if (setting.validValues[i] == inputText_) currentIdx = i;
                    }
                    if (increaseSmall || increaseLarge) {
                        currentIdx = (currentIdx + 1) % setting.validValues.size();
                    }
                    else if (decreaseSmall || decreaseLarge) {
                        currentIdx = (currentIdx == 0) ? setting.validValues.size() - 1 : currentIdx - 1;
                    }
                    inputText_ = setting.validValues[currentIdx];
                    setting.displayText = setting.key + ": " + inputText_ + (setting.isCommented ? " (disabled)" : "") + "|";
                }
                else if (setting.type == SettingType::Numeric) {
                    float step = (increaseLarge || decreaseLarge) ? 5.0f : 0.5f;
                    float value = inputText_.empty() ? setting.minValue : std::stof(inputText_);
                    value += (increaseLarge || increaseSmall) ? step : -step;
                    value = std::max(static_cast<float>(setting.minValue), std::min(static_cast<float>(setting.maxValue), value));
                    inputText_ = std::to_string(value);
                    setting.displayText = setting.key + ": " + inputText_ + (setting.isCommented ? " (disabled)" : "") + "|";
                }
            }
        }
        return MenuAction::STAY;
    }

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

        if (menuUp) {
            navigate(true);
        }
        else if (menuDown) {
            navigate(false);
        }
        else if (menuSelect && selectedOption_ < settingsOptions_.size()) {
            Setting& setting = settingsOptions_[selectedOption_];
            if (setting.type != SettingType::Title && setting.type != SettingType::Subtitle && setting.key != "layout") {
                editingIndex_ = selectedOption_;
                editingText_ = true;
                inputText_ = setting.value;
                SDL_StartTextInput();
                setting.displayText = setting.key + ": " + inputText_ + (setting.isCommented ? " (disabled)" : "") + "|";
            }
        }
        else if (menuBack) {
            return MenuAction::BACK;
        }
        else if (toggleEnable && selectedOption_ < settingsOptions_.size()) {
            Setting& setting = settingsOptions_[selectedOption_];
            if (setting.type != SettingType::Title && setting.type != SettingType::Subtitle) {
                setting.isCommented = !setting.isCommented;
                setting.displayText = setting.key + ": " + setting.value + (setting.isCommented ? " (disabled)" : "");
                hasChanges_ = true;
            }
        }
    }
    return MenuAction::STAY;
}

void SettingsMenu::render(SDL_Renderer* renderer, Font* titleFont, Font* optionFont,
    int windowWidth, int windowHeight, const OptionsMenuBuilder& config, float scaleFactor) {
    if (!titleFont || !optionFont || !titleFont->getTexture() || !optionFont->getTexture()) {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_Rect rect = { 50, 50, 200, 100 };
        SDL_RenderFillRect(renderer, &rect);
        return;
    }
    static const SDL_Color greenColor = OptionsMenuBuilder::parseColor("00FF00");
    static const SDL_Color redColor = OptionsMenuBuilder::parseColor("FF0000");
    // Render title
    std::string title = config.getSettingsTitleText();
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
        Logger::write(Logger::ZONE_WARNING, "SettingsMenu", "Invalid title x alignment, using center");
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
        Logger::write(Logger::ZONE_WARNING, "SettingsMenu", "Invalid title y alignment, using top+50 scaled");
    }
    Logger::write(Logger::ZONE_INFO, "SettingsMenu", "Title alignment: xAlign=" +
        std::string(config.getTitleXAlignment() == OptionsMenuBuilder::XAlignment::Center ? "center" :
            config.getTitleXAlignment() == OptionsMenuBuilder::XAlignment::Left ? "left" :
            config.getTitleXAlignment() == OptionsMenuBuilder::XAlignment::Right ? "right" : "numeric") +
        ", xOffset=" + std::to_string(config.getTitleXOffset()) +
        ", yAlign=" + std::string(config.getTitleYAlignment() == OptionsMenuBuilder::YAlignment::Center ? "center" :
            config.getTitleYAlignment() == OptionsMenuBuilder::YAlignment::Top ? "top" :
            config.getTitleYAlignment() == OptionsMenuBuilder::YAlignment::Bottom ? "bottom" : "numeric") +
        ", yOffset=" + std::to_string(config.getTitleYOffset()));
    Logger::write(Logger::ZONE_INFO, "SettingsMenu", "Title position: x=" + std::to_string(titleX) +
        ", y=" + std::to_string(titleY) + ", titleWidth=" + std::to_string(titleWidth));
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
    // Render options
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
        Logger::write(Logger::ZONE_WARNING, "SettingsMenu", "Invalid menu y alignment, using titleY+height+20 scaled");
    }
    int scaledSpaceBetween = static_cast<int>(config.getSpaceBetweenText() * scaleFactor);
    int maxVisibleLines = (windowHeight - menuY) / scaledSpaceBetween;
    int scrollOffset = 0;
    if (settingsOptions_.size() > static_cast<size_t>(maxVisibleLines)) {
        if (selectedOption_ >= static_cast<size_t>(maxVisibleLines / 2)) {
            scrollOffset = static_cast<int>(selectedOption_ - maxVisibleLines / 2) * scaledSpaceBetween;
            int maxOffset = static_cast<int>((settingsOptions_.size() - maxVisibleLines) * scaledSpaceBetween);
            scrollOffset = std::min(scrollOffset, maxOffset);
        }
        else {
            scrollOffset = 0;
        }
    }
    const float scrollSpeed = 50.0f * scaleFactor;  // Scale scroll speed if needed
    const Uint32 pauseDuration = 1000;
    if (selectedOption_ < settingsOptions_.size()) {
        std::string selectedText = settingsOptions_[selectedOption_].displayText;
        float selectedTextWidth = 0.0f;
        for (char c : selectedText) {
            if (c < 32 || c > 127) continue;
            Font::GlyphInfo glyph;
            if (optionFont->getRect(static_cast<unsigned char>(c), glyph)) {
                selectedTextWidth += glyph.advance;
            }
        }
        float availableWidth = static_cast<float>(windowWidth) - 100 * scaleFactor;  // Scale available width
        isScrolling_ = selectedTextWidth > availableWidth;
        scrollWidth_ = isScrolling_ ? selectedTextWidth - availableWidth : 0.0f;
        if (isScrolling_) {
            Uint32 currentTime = SDL_GetTicks();
            if (lastScrollTime_ == 0) lastScrollTime_ = currentTime;
            Uint32 elapsed = currentTime - lastScrollTime_;
            float scrollCycleDuration = (scrollWidth_ / scrollSpeed) * 1000 + 2 * pauseDuration;
            float cycleProgress = std::fmod(static_cast<float>(elapsed), scrollCycleDuration) / 1000.0f;
            if (cycleProgress < pauseDuration / 1000.0f) {
                textScrollOffset_ = 0.0f;
            }
            else if (cycleProgress < (scrollWidth_ / scrollSpeed + pauseDuration / 1000.0f)) {
                float scrollTime = cycleProgress - pauseDuration / 1000.0f;
                textScrollOffset_ = scrollTime * scrollSpeed;
            }
            else if (cycleProgress < (scrollWidth_ / scrollSpeed + 2 * pauseDuration / 1000.0f)) {
                textScrollOffset_ = scrollWidth_;
            }
            else {
                textScrollOffset_ = 0.0f;
                lastScrollTime_ = currentTime;
            }
        }
        else {
            textScrollOffset_ = 0.0f;
            lastScrollTime_ = 0;
        }
    }
    else {
        isScrolling_ = false;
        textScrollOffset_ = 0.0f;
        lastScrollTime_ = 0;
    }
    int scaledPadding = static_cast<int>(config.getSelectionBarPadding() * scaleFactor);
    int scaledBarHeight = static_cast<int>(config.getSelectionBarHeight() * scaleFactor);
    int scaledBarYOffset = static_cast<int>(config.getSelectionBarYOffset() * scaleFactor);
    for (size_t i = 0; i < settingsOptions_.size(); ++i) {
        const Setting& setting = settingsOptions_[i];
        std::string text = setting.displayText;
        bool isSelected = (i == selectedOption_);
        bool isEditing = editingText_ && i == editingIndex_;
        float textWidth = 0.0f;
        int textHeight = optionFont->getHeight();
        for (char c : text) {
            if (c < 32 || c > 127) continue;
            Font::GlyphInfo glyph;
            if (optionFont->getRect(static_cast<unsigned char>(c), glyph)) {
                textWidth += glyph.advance;
            }
        }
        float y = menuY + static_cast<float>(i) * scaledSpaceBetween - scrollOffset;
        if (y < menuY || y + scaledSpaceBetween < 0 || y >= windowHeight) continue;
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
            Logger::write(Logger::ZONE_WARNING, "SettingsMenu", "Invalid menu x alignment, using center");
        }
        float x = optionX - (isSelected && isScrolling_ ? textScrollOffset_ : 0.0f);
        // Render titles with title bar color
        if (setting.type == SettingType::Title) {
            SDL_Rect barRect = {
         static_cast<int>(optionX - scaledPadding),
         static_cast<int>(y + scaledBarYOffset),
         static_cast<int>(textWidth + 2 * scaledPadding),
         scaledBarHeight
            };
            SDL_SetRenderDrawColor(renderer, config.getTitleBarColor().r, config.getTitleBarColor().g,
                config.getTitleBarColor().b, config.getTitleBarColor().a);
            SDL_RenderFillRect(renderer, &barRect);
        }
        // Render subtitles with subtitle underline color
        else if (setting.type == SettingType::Subtitle) {
            float underlineY = y + optionFont->getHeight() + 2 * scaleFactor;  // Scale underline offset if needed
            SDL_SetRenderDrawColor(renderer, config.getSubtitleUnderlineColor().r, config.getSubtitleUnderlineColor().g,
                config.getSubtitleUnderlineColor().b, config.getSubtitleUnderlineColor().a);
            SDL_RenderDrawLine(renderer, static_cast<int>(optionX), static_cast<int>(underlineY),
                static_cast<int>(optionX + textWidth), static_cast<int>(underlineY));
        }
        // Render settings with selection bar color
        else if (isSelected && !isEditing) {
            SDL_Rect barRect = {
      static_cast<int>(optionX - scaledPadding),
      static_cast<int>(y + scaledBarYOffset),
      static_cast<int>(textWidth + 2 * scaledPadding),
      scaledBarHeight
            };
            SDL_SetRenderDrawColor(renderer, config.getSelectionBarColor().r, config.getSelectionBarColor().g,
                config.getSelectionBarColor().b, config.getSelectionBarColor().a);
            SDL_RenderFillRect(renderer, &barRect);
        }
        renderX = x;
        SDL_Color lineColor = isSelected ? config.getTitleColor() : config.getOptionColor();
        if (setting.type == SettingType::Boolean) {
            lineColor = (setting.value == "yes") ? greenColor :
                (setting.value == "no") ? redColor : lineColor;
        }
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
    Logger::write(Logger::ZONE_INFO, "SettingsMenu", "Options alignment: xAlign=" +
        std::string(config.getMenuXAlignment() == OptionsMenuBuilder::XAlignment::Center ? "center" :
            config.getMenuXAlignment() == OptionsMenuBuilder::XAlignment::Left ? "left" :
            config.getMenuXAlignment() == OptionsMenuBuilder::XAlignment::Right ? "right" : "numeric") +
        ", xOffset=" + std::to_string(config.getMenuXOffset()) +
        ", yAlign=" + std::string(config.getMenuYAlignment() == OptionsMenuBuilder::YAlignment::Center ? "center" :
            config.getMenuYAlignment() == OptionsMenuBuilder::YAlignment::Top ? "top" :
            config.getMenuYAlignment() == OptionsMenuBuilder::YAlignment::Bottom ? "bottom" : "numeric") +
        ", yOffset=" + std::to_string(config.getMenuYOffset()));
    Logger::write(Logger::ZONE_INFO, "SettingsMenu", "Options position: y=" + std::to_string(menuY) +
        ", scrollOffset=" + std::to_string(scrollOffset));
}

void SettingsMenu::save() {
    std::ofstream outFile(settingsFile_);
    if (!outFile.is_open()) {
        Logger::write(Logger::ZONE_ERROR, "SettingsMenu", "Failed to save " + settingsFile_);
        return;
    }

    std::map<std::string, Setting> currentSettings;
    for (const auto& setting : settingsOptions_) {
        if (setting.type != SettingType::Title && setting.type != SettingType::Subtitle) {
            currentSettings[setting.key] = setting;
        }
    }

    size_t optionIndex = 0;
    for (const auto& line : originalSettingsLines_) {
        std::string trimmedLine = line;
        trimmedLine.erase(0, trimmedLine.find_first_not_of(" \t"));

        if (trimmedLine.empty()) {
            outFile << line << "\n";
            continue;
        }

        size_t hashCount = 0;
        while (hashCount < trimmedLine.length() && trimmedLine[hashCount] == '#') {
            hashCount++;
        }

        if (hashCount >= 4) {
            outFile << line << "\n";
            continue;
        }

        if (hashCount == 2 || hashCount == 3) {
            if (optionIndex < settingsOptions_.size() &&
                (settingsOptions_[optionIndex].type == SettingType::Title || settingsOptions_[optionIndex].type == SettingType::Subtitle)) {
                outFile << line << "\n";
                optionIndex++;
            }
            else {
                outFile << line << "\n";
            }
            continue;
        }

        bool isCommented = (hashCount == 1);
        std::string keyValue = isCommented ? trimmedLine.substr(1) : trimmedLine;
        keyValue.erase(0, keyValue.find_first_not_of(" \t"));
        size_t equalsPos = keyValue.find('=');
        if (equalsPos == std::string::npos) {
            outFile << line << "\n";
            continue;
        }

        std::string key = keyValue.substr(0, equalsPos);
        key.erase(key.find_last_not_of(" \t") + 1);

        auto it = currentSettings.find(key);
        if (it != currentSettings.end()) {
            std::string prefix = it->second.isCommented ? "#" : "";
            outFile << prefix << it->second.key << " = " << it->second.value << "\n";
            currentSettings.erase(it);
            optionIndex++;
        }
        else {
            outFile << line << "\n";
        }
    }

    if (!currentSettings.empty()) {
        outFile << "\n# New settings added by SettingsMenu\n";
        for (const auto& setting : currentSettings) {
            outFile << (setting.second.isCommented ? "#" : "") << setting.second.key << " = " << setting.second.value << "\n";
            Logger::write(Logger::ZONE_INFO, "SettingsMenu", "Added new setting: " + setting.second.key + " = " + setting.second.value);
        }
    }

    outFile.close();
    Logger::write(Logger::ZONE_INFO, "SettingsMenu", "Settings saved to " + settingsFile_);
    hasChanges_ = false;
}

void SettingsMenu::navigate(bool moveUp) {
    if (settingsOptions_.empty()) return;

    size_t newOption = selectedOption_;
    do {
        if (moveUp) {
            newOption = (newOption > 0) ? newOption - 1 : settingsOptions_.size() - 1;
        }
        else {
            newOption = (newOption < settingsOptions_.size() - 1) ? newOption + 1 : 0;
        }
        if (newOption == selectedOption_ || (settingsOptions_[newOption].type != SettingType::Title &&
            settingsOptions_[newOption].type != SettingType::Subtitle)) {
            break;
        }
    } while (newOption != selectedOption_);

    selectedOption_ = newOption;
}