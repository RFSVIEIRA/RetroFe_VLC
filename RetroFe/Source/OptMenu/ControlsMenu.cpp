#include "ControlsMenu.h"
#include "../utility/Log.h"
#include "../Utility/Utils.h"
#include "OptionsMenuBuilder.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <SDL.h>
#include <set>

ControlsMenu::ControlsMenu(const std::string& controlsFile)
    : controlsFile_(controlsFile), joystickName_("No gamepad detected"), selectedOption_(0), editingIndex_(-1), editingText_(false), capturingKey_(false), hasPendingChanges_(false), pendingKeys_(), inputNumber_() {
    initControlsOptions();

    // Log connected joysticks and store the first gamepad name
    int numJoysticks = SDL_NumJoysticks();
    Logger::write(Logger::ZONE_INFO, "ControlsMenu", "Detected " + std::to_string(numJoysticks) + " joysticks");
    for (int i = 0; i < numJoysticks; ++i) {
        SDL_Joystick* joystick = SDL_JoystickOpen(i);
        if (joystick) {
            std::string name = SDL_JoystickName(joystick);
            Logger::write(Logger::ZONE_INFO, "ControlsMenu", "Joystick " + std::to_string(i) + ": " + name);
            if (i == 0) {
                joystickName_ = name;
            }
            SDL_JoystickClose(joystick);
        }
        else {
            Logger::write(Logger::ZONE_ERROR, "ControlsMenu", "Failed to open joystick " + std::to_string(i) + ": " + SDL_GetError());
        }
    }
}

void ControlsMenu::initControlsOptions() {
    std::ifstream inFile(controlsFile_);
    std::map<std::string, Control> controlsMap;

    if (inFile.is_open()) {
        std::string line;
        while (std::getline(inFile, line)) {
            std::string trimmedLine = line;
            trimmedLine.erase(0, trimmedLine.find_first_not_of(" \t"));
            if (trimmedLine.empty() || trimmedLine[0] == '#') continue;

            size_t equalsPos = trimmedLine.find('=');
            if (equalsPos != std::string::npos) {
                std::string action = trimmedLine.substr(0, equalsPos);
                action.erase(action.find_last_not_of(" \t") + 1);
                std::string keysStr = trimmedLine.substr(equalsPos + 1);
                keysStr.erase(0, keysStr.find_first_not_of(" \t"));
                keysStr.erase(keysStr.find_last_not_of(" \t") + 1);

                std::vector<std::string> keys;
                std::stringstream ss(keysStr);
                std::string key;
                while (std::getline(ss, key, ',')) {
                    key.erase(0, key.find_first_not_of(" \t"));
                    key.erase(key.find_last_not_of(" \t") + 1);
                    if (!key.empty()) keys.push_back(key);
                }

                bool isNumeric = (action == "deadZone");
                std::optional<int> numericValue = std::nullopt;
                if (isNumeric && !keys.empty()) {
                    try {
                        numericValue = std::stoi(keys[0]);
                    }
                    catch (const std::exception&) {
                        numericValue = 0;
                    }
                }
                float minValue = isNumeric ? 0.0f : 0.0f;
                float maxValue = isNumeric ? 100.0f : 0.0f;
                std::string displayText = isNumeric ? action + ": " + (keys.empty() ? "0" : keys[0]) : action + ": " + std::accumulate(keys.begin(), keys.end(), std::string(),
                    [](const std::string& a, const std::string& b) { return a.empty() ? b : a + ", " + b; });

                controlsMap[action] = Control{
                    action,                    // action
                    keys,                      // keys
                    false,                     // isCommented
                    displayText,               // displayText
                    numericValue,              // numericValue
                    "",                        // description
                    isNumeric,                 // isNumeric
                    minValue,                  // minValue
                    maxValue                   // maxValue
                };
#ifdef _DEBUG
                Logger::write(Logger::ZONE_INFO, "ControlsMenu", "Loaded control: " + displayText);
#endif
            }
        }
        inFile.close();
    }
    else {
#ifdef _DEBUG
        Logger::write(Logger::ZONE_WARNING, "ControlsMenu", "Could not read " + controlsFile_ + "; no controls loaded (defaults disabled)");
#endif
    }

    controlsOptions_.clear();
    for (const auto& pair : controlsMap) {
        controlsOptions_.push_back(pair.second);
    }
#ifdef _DEBUG
    Logger::write(Logger::ZONE_INFO, "ControlsMenu", "Loaded " + std::to_string(controlsOptions_.size()) + " controls from " + controlsFile_);
#endif
}

MenuAction ControlsMenu::handleInput(SDL_Event& event) {
    static bool rightTriggerHeld = false;
    if (event.type == SDL_JOYAXISMOTION && event.jaxis.axis == 5) {
        rightTriggerHeld = (event.jaxis.value > 16384);
    }
    static bool leftTriggerHeld = false;
    if (event.type == SDL_JOYAXISMOTION && event.jaxis.axis == 4) {
        leftTriggerHeld = (event.jaxis.value > 16384);
    }

    if (capturingKey_) {
        Control& control = controlsOptions_[editingIndex_];
        std::string newKey;
        static Uint32 lastKeyTime = 0; // Track time of last input event for debouncing

        // Debounce: Ignore events within 200ms of the last one
        Uint32 currentTime = SDL_GetTicks();
        if (currentTime - lastKeyTime < 200) {
            return MenuAction::STAY;
        }

        // Whitelist of acceptable keyboard keys
        static const std::set<std::string> allowedKeys = {
            "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
            "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
            "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
            "Up", "Down", "Left", "Right", "Space", "Tab",
            "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
            "Return", "Escape", "Backspace", "Comma"
        };

        if (control.isNumeric) {
            // Handle numeric controls like deadZone
            if (event.type == SDL_TEXTINPUT) {
                std::string newText = event.text.text;
                if (std::all_of(newText.begin(), newText.end(), [](char c) { return ::isdigit(c); })) {
                    inputNumber_ += newText;
                    control.displayText = control.action + ": " + inputNumber_ + "|";
                    lastKeyTime = currentTime;
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
                    if (!inputNumber_.empty()) {
                        try {
                            float value = std::stof(inputNumber_);
                            value = std::max(control.minValue, std::min(control.maxValue, value));
                            inputNumber_ = std::to_string(static_cast<int>(value));
                            control.keys = { inputNumber_ };
                            control.displayText = control.action + ": " + inputNumber_;
                            hasPendingChanges_ = true;
                        }
                        catch (const std::exception&) {
                            inputNumber_ = std::to_string(static_cast<int>(control.minValue));
                            control.keys = { inputNumber_ };
                            control.displayText = control.action + ": " + inputNumber_;
                            hasPendingChanges_ = true;
                        }
                    }
                    capturingKey_ = false;
                    inputNumber_.clear();
                    lastKeyTime = currentTime;
                }
                else if (menuBack) {
                    std::string displayKeys = control.keys.empty() ? "" : std::accumulate(control.keys.begin(), control.keys.end(), std::string(),
                        [](const std::string& a, const std::string& b) { return a.empty() ? b : a + ", " + b; });
                    control.displayText = control.action + ": " + displayKeys;
                    capturingKey_ = false;
                    inputNumber_.clear();
                    lastKeyTime = currentTime;
                }
                else if (deleteChar) {
                    if (!inputNumber_.empty()) {
                        inputNumber_.pop_back();
                        control.displayText = control.action + ": " + inputNumber_ + "|";
                        lastKeyTime = currentTime;
                    }
                }
                else if (increaseSmall || decreaseSmall || increaseLarge || decreaseLarge) {
                    float step = (increaseLarge || decreaseLarge) ? 5.0f : 1.0f;
                    float value = inputNumber_.empty() ? control.minValue : std::stof(inputNumber_);
                    value += (increaseLarge || increaseSmall) ? step : -step;
                    value = std::max(control.minValue, std::min(control.maxValue, value));
                    inputNumber_ = std::to_string(static_cast<int>(value));
                    control.displayText = control.action + ": " + inputNumber_ + "|";
                    lastKeyTime = currentTime;
                }
            }
        }
        else {
            // Handle non-numeric controls
            if (event.type == SDL_KEYDOWN) {
                newKey = SDL_GetKeyName(event.key.keysym.sym);
                if (!newKey.empty() && allowedKeys.find(newKey) != allowedKeys.end() &&
                    newKey != "Return" && newKey != "Escape" && newKey != "Comma") {
                    pendingKeys_.push_back(newKey);
                    std::string displayKeys = std::accumulate(pendingKeys_.begin(), pendingKeys_.end(), std::string(),
                        [](const std::string& a, const std::string& b) { return a.empty() ? b : a + ", " + b; });
                    control.displayText = control.action + ": " + displayKeys + " (Press , for more, ENTER to save, ESC to cancel)";
                    lastKeyTime = currentTime;
                }
            }
            else if (event.type == SDL_JOYBUTTONDOWN) {
                newKey = "joy" + std::to_string(event.jbutton.which) + "Button" + std::to_string(event.jbutton.button);
                pendingKeys_.push_back(newKey);
                std::string displayKeys = std::accumulate(pendingKeys_.begin(), pendingKeys_.end(), std::string(),
                    [](const std::string& a, const std::string& b) { return a.empty() ? b : a + ", " + b; });
                control.displayText = control.action + ": " + displayKeys + " (Press , for more, ENTER to save, ESC to cancel)";
                lastKeyTime = currentTime;
            }
            else if (event.type == SDL_JOYHATMOTION && event.jhat.value != SDL_HAT_CENTERED) {
                std::string direction;
                switch (event.jhat.value) {
                case SDL_HAT_UP: direction = "Up"; break;
                case SDL_HAT_DOWN: direction = "Down"; break;
                case SDL_HAT_LEFT: direction = "Left"; break;
                case SDL_HAT_RIGHT: direction = "Right"; break;
                case SDL_HAT_LEFTUP: direction = "LeftUp"; break;
                case SDL_HAT_LEFTDOWN: direction = "LeftDown"; break;
                case SDL_HAT_RIGHTUP: direction = "RightUp"; break;
                case SDL_HAT_RIGHTDOWN: direction = "RightDown"; break;
                default: return MenuAction::STAY;
                }
                newKey = "joy" + std::to_string(event.jhat.which) + "Hat" + std::to_string(event.jhat.hat) + direction;
                pendingKeys_.push_back(newKey);
                std::string displayKeys = std::accumulate(pendingKeys_.begin(), pendingKeys_.end(), std::string(),
                    [](const std::string& a, const std::string& b) { return a.empty() ? b : a + ", " + b; });
                control.displayText = control.action + ": " + displayKeys + " (Press , for more, ENTER to save, ESC to cancel)";
                lastKeyTime = currentTime;
            }
            else if (event.type == SDL_JOYAXISMOTION) {
                const int AXIS_THRESHOLD = 16384;
                if (std::abs(event.jaxis.value) > AXIS_THRESHOLD) {
                    std::string direction = event.jaxis.value > 0 ? "+" : "-";
                    newKey = "joy" + std::to_string(event.jaxis.which) + "Axis" + std::to_string(event.jaxis.axis) + direction;
                    pendingKeys_.push_back(newKey);
                    std::string displayKeys = std::accumulate(pendingKeys_.begin(), pendingKeys_.end(), std::string(),
                        [](const std::string& a, const std::string& b) { return a.empty() ? b : a + ", " + b; });
                    control.displayText = control.action + ": " + displayKeys + " (Press , for more, ENTER to save, ESC to cancel)";
                    lastKeyTime = currentTime;
                }
            }

            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                case SDLK_COMMA:
                    if (!pendingKeys_.empty()) {
                        std::string displayKeys = std::accumulate(pendingKeys_.begin(), pendingKeys_.end(), std::string(),
                            [](const std::string& a, const std::string& b) { return a.empty() ? b : a + ", " + b; });
                        control.displayText = control.action + ": " + displayKeys + " (Press key or gamepad input)";
                    }
                    lastKeyTime = currentTime;
                    break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    if (!pendingKeys_.empty()) {
                        control.keys = pendingKeys_;
                        std::string displayKeys = std::accumulate(control.keys.begin(), control.keys.end(), std::string(),
                            [](const std::string& a, const std::string& b) { return a.empty() ? b : a + ", " + b; });
                        control.displayText = control.action + ": " + displayKeys;
                        hasPendingChanges_ = true;
                    }
                    capturingKey_ = false;
                    pendingKeys_.clear();
                    lastKeyTime = currentTime;
                    break;
                case SDLK_ESCAPE:
                    std::string displayKeys = control.keys.empty() ? "" : std::accumulate(control.keys.begin(), control.keys.end(), std::string(),
                        [](const std::string& a, const std::string& b) { return a.empty() ? b : a + ", " + b; });
                    control.displayText = control.action + ": " + displayKeys;
                    capturingKey_ = false;
                    pendingKeys_.clear();
                    lastKeyTime = currentTime;
                    break;
                }
            }
            else if (event.type == SDL_JOYBUTTONDOWN && event.jbutton.button == 0) {
                if (!pendingKeys_.empty()) {
                    control.keys = pendingKeys_;
                    std::string displayKeys = std::accumulate(control.keys.begin(), control.keys.end(), std::string(),
                        [](const std::string& a, const std::string& b) { return a.empty() ? b : a + ", " + b; });
                    control.displayText = control.action + ": " + displayKeys;
                    hasPendingChanges_ = true;
                }
                capturingKey_ = false;
                pendingKeys_.clear();
                lastKeyTime = currentTime;
            }
            else if (event.type == SDL_JOYBUTTONDOWN && event.jbutton.button == 1) {
                std::string displayKeys = control.keys.empty() ? "" : std::accumulate(control.keys.begin(), control.keys.end(), std::string(),
                    [](const std::string& a, const std::string& b) { return a.empty() ? b : a + ", " + b; });
                control.displayText = control.action + ": " + displayKeys;
                capturingKey_ = false;
                pendingKeys_.clear();
                lastKeyTime = currentTime;
            }
        }
        return MenuAction::STAY;
    }

    if (event.type == SDL_KEYDOWN || event.type == SDL_JOYHATMOTION || event.type == SDL_JOYBUTTONDOWN) {
        SDL_Keymod mod = SDL_GetModState();
        bool ctrlPressed = (mod & KMOD_CTRL) != 0;

        bool menuUp = (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_UP && !ctrlPressed) ||
            (event.type == SDL_JOYHATMOTION && event.jhat.value == SDL_HAT_UP && !rightTriggerHeld);
        bool menuDown = (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_DOWN && !ctrlPressed) ||
            (event.type == SDL_JOYHATMOTION && event.jhat.value == SDL_HAT_DOWN && !leftTriggerHeld);
        bool menuSelect = (event.type == SDL_KEYDOWN && (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER)) ||
            (event.type == SDL_JOYBUTTONDOWN && event.jbutton.button == 0);
        bool menuBack = (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) ||
            (event.type == SDL_JOYBUTTONDOWN && event.jbutton.button == 1);

        if (menuUp) {
            navigate(true);
            return MenuAction::STAY;
        }
        else if (menuDown) {
            navigate(false);
            return MenuAction::STAY;
        }
        else if (menuSelect) {
            if (selectedOption_ < controlsOptions_.size()) {
                editingIndex_ = selectedOption_;
                capturingKey_ = true;
                SDL_FlushEvents(SDL_KEYDOWN, SDL_JOYAXISMOTION);
                pendingKeys_.clear();
                inputNumber_.clear();
                controlsOptions_[editingIndex_].displayText = controlsOptions_[editingIndex_].action + ": (Press a key or gamepad input)";
            }
            return MenuAction::STAY;
        }
        else if (menuBack) {
            return MenuAction::BACK;
        }
    }
    return MenuAction::STAY;
}

void ControlsMenu::render(SDL_Renderer* renderer, Font* titleFont, Font* optionFont,
    int windowWidth, int windowHeight, const OptionsMenuBuilder& config, float scaleFactor) {
    if (!titleFont || !optionFont || !titleFont->getTexture() || !optionFont->getTexture()) {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_Rect rect = { 50, 50, 200, 100 };
        SDL_RenderFillRect(renderer, &rect);
        return;
    }
    // Render title
    std::string title = config.getControlsTitleText();
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
        Logger::write(Logger::ZONE_WARNING, "ControlsMenu", "Invalid title x alignment, using center");
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
        Logger::write(Logger::ZONE_WARNING, "ControlsMenu", "Invalid title y alignment, using top+50 scaled");
    }
    Logger::write(Logger::ZONE_INFO, "ControlsMenu", "Title alignment: xAlign=" +
        std::string(config.getTitleXAlignment() == OptionsMenuBuilder::XAlignment::Center ? "center" :
            config.getTitleXAlignment() == OptionsMenuBuilder::XAlignment::Left ? "left" :
            config.getTitleXAlignment() == OptionsMenuBuilder::XAlignment::Right ? "right" : "numeric") +
        ", xOffset=" + std::to_string(config.getTitleXOffset()) +
        ", yAlign=" + std::string(config.getTitleYAlignment() == OptionsMenuBuilder::YAlignment::Center ? "center" :
            config.getTitleYAlignment() == OptionsMenuBuilder::YAlignment::Top ? "top" :
            config.getTitleYAlignment() == OptionsMenuBuilder::YAlignment::Bottom ? "bottom" : "numeric") +
        ", yOffset=" + std::to_string(config.getTitleYOffset()));
    Logger::write(Logger::ZONE_INFO, "ControlsMenu", "Title position: x=" + std::to_string(titleX) +
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
    // Render joystick name
    std::string joystickText = "Gamepad: " + joystickName_;
    float joystickTextWidth = 0.0f;
    for (char c : joystickText) {
        if (c < 32 || c > 127) continue;
        Font::GlyphInfo glyph;
        if (optionFont->getRect(static_cast<unsigned char>(c), glyph)) {
            joystickTextWidth += glyph.advance;
        }
    }
    int scaledMenuXOffset = static_cast<int>(config.getMenuXOffset() * scaleFactor);
    int joystickX;
    switch (config.getMenuXAlignment()) {
    case OptionsMenuBuilder::XAlignment::Center:
        joystickX = (windowWidth - static_cast<int>(joystickTextWidth)) / 2 + scaledMenuXOffset;
        break;
    case OptionsMenuBuilder::XAlignment::Left:
        joystickX = scaledMenuXOffset;
        break;
    case OptionsMenuBuilder::XAlignment::Right:
        joystickX = windowWidth - static_cast<int>(joystickTextWidth) + scaledMenuXOffset;
        break;
    case OptionsMenuBuilder::XAlignment::Numeric:
        joystickX = scaledMenuXOffset;
        break;
    default:
        joystickX = (windowWidth - static_cast<int>(joystickTextWidth)) / 2;
    }
    int joystickY = titleY + titleFont->getHeight() + static_cast<int>(10 * scaleFactor);
    renderX = static_cast<float>(joystickX);
    for (char c : joystickText) {
        if (c < 32 || c > 127) continue;
        Font::GlyphInfo glyph;
        if (optionFont->getRect(static_cast<unsigned char>(c), glyph)) {
            SDL_Rect destRect = { static_cast<int>(renderX), joystickY, glyph.rect.w, glyph.rect.h };
            SDL_SetTextureColorMod(optionFont->getTexture(), config.getOptionColor().r,
                config.getOptionColor().g, config.getOptionColor().b);
            SDL_RenderCopy(renderer, optionFont->getTexture(), &glyph.rect, &destRect);
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
        menuY = std::max(menuY, joystickY + optionFont->getHeight() + scaledTitleSpacing);
        break;
    default:
        menuY = joystickY + optionFont->getHeight() + scaledTitleSpacing;
        Logger::write(Logger::ZONE_WARNING, "ControlsMenu", "Invalid menu y alignment, using joystickY+height+20 scaled");
    }
    int scaledSpaceBetween = static_cast<int>(config.getSpaceBetweenText() * scaleFactor);
    int maxVisibleLines = (windowHeight - menuY) / scaledSpaceBetween;
    int scrollOffset = 0;
    if (controlsOptions_.size() > static_cast<size_t>(maxVisibleLines)) {
        if (selectedOption_ >= static_cast<size_t>(maxVisibleLines / 2)) {
            scrollOffset = static_cast<int>(selectedOption_ - maxVisibleLines / 2) * scaledSpaceBetween;
            int maxOffset = static_cast<int>((controlsOptions_.size() - maxVisibleLines) * scaledSpaceBetween);
            scrollOffset = std::min(scrollOffset, maxOffset);
        }
        else {
            scrollOffset = 0;
        }
    }
    int scaledPadding = static_cast<int>(config.getSelectionBarPadding() * scaleFactor);
    int scaledBarHeight = static_cast<int>(config.getSelectionBarHeight() * scaleFactor);
    int scaledBarYOffset = static_cast<int>(config.getSelectionBarYOffset() * scaleFactor);
    for (size_t i = 0; i < controlsOptions_.size(); ++i) {
        std::string text = controlsOptions_[i].displayText;
        bool isSelected = (i == selectedOption_);
        float textWidth = 0.0f;
        for (char c : text) {
            if (c < 32 || c > 127) continue;
            Font::GlyphInfo glyph;
            if (optionFont->getRect(static_cast<unsigned char>(c), glyph)) {
                textWidth += glyph.advance;
            }
        }
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
            Logger::write(Logger::ZONE_WARNING, "ControlsMenu", "Invalid menu x alignment, using center");
        }
        float y = menuY + static_cast<float>(i) * scaledSpaceBetween - scrollOffset;
        if (y < menuY || y + scaledSpaceBetween < 0 || y >= windowHeight) continue;
        if (isSelected) {
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
        renderX = static_cast<float>(optionX);
        SDL_Color lineColor = isSelected ? config.getTitleColor() : config.getOptionColor();
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
    Logger::write(Logger::ZONE_INFO, "ControlsMenu", "Options alignment: xAlign=" +
        std::string(config.getMenuXAlignment() == OptionsMenuBuilder::XAlignment::Center ? "center" :
            config.getMenuXAlignment() == OptionsMenuBuilder::XAlignment::Left ? "left" :
            config.getMenuXAlignment() == OptionsMenuBuilder::XAlignment::Right ? "right" : "numeric") +
        ", xOffset=" + std::to_string(config.getMenuXOffset()) +
        ", yAlign=" + std::string(config.getMenuYAlignment() == OptionsMenuBuilder::YAlignment::Center ? "center" :
            config.getMenuYAlignment() == OptionsMenuBuilder::YAlignment::Top ? "top" :
            config.getMenuYAlignment() == OptionsMenuBuilder::YAlignment::Bottom ? "bottom" : "numeric") +
        ", yOffset=" + std::to_string(config.getMenuYOffset()));
    Logger::write(Logger::ZONE_INFO, "ControlsMenu", "Options position: y=" + std::to_string(menuY) +
        ", scrollOffset=" + std::to_string(scrollOffset));
}

void ControlsMenu::save() {
    std::ofstream outFile(controlsFile_, std::ios::out | std::ios::trunc);
    if (outFile.is_open()) {
        outFile << "# RetroFE Controls Configuration\n";
        for (const auto& control : controlsOptions_) {
            if (control.action.find("info") == 0) continue;
            std::string keysStr;
            for (size_t i = 0; i < control.keys.size(); ++i) {
                keysStr += control.keys[i];
                if (i < control.keys.size() - 1) keysStr += ",";
            }
            outFile << control.action << " = " << keysStr << "\n";
        }
        outFile.close();
        Logger::write(Logger::ZONE_INFO, "ControlsMenu", "Controls saved to " + controlsFile_);
        hasPendingChanges_ = false;
    }
    else {
        Logger::write(Logger::ZONE_ERROR, "ControlsMenu", "Failed to save " + controlsFile_);
    }
}

void ControlsMenu::navigate(bool moveUp) {
    if (controlsOptions_.empty()) return;
    if (moveUp) {
        selectedOption_ = (selectedOption_ > 0) ? selectedOption_ - 1 : controlsOptions_.size() - 1;
    }
    else {
        selectedOption_ = (selectedOption_ < controlsOptions_.size() - 1) ? selectedOption_ + 1 : 0;
    }
}