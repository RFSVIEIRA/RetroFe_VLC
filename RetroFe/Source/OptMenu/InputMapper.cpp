#include "InputMapper.h"
#include "../utility/Log.h"
#include <fstream>
#include <sstream>
#include <algorithm>

const std::set<std::string> InputMapper::allowedKeys_ = {
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
    "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "Up", "Down", "Left", "Right", "Space", "Tab",
    "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
    "Return", "Escape", "Backspace", "Comma"
};

InputMapper::InputMapper(const std::string& actionsFile) : actionsFile_(actionsFile) {
    loadMappings();
}

void InputMapper::loadMappings() {
    actionMappings_.clear();
    std::ifstream inFile(actionsFile_);
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
                actionMappings_[action] = keys;
#ifdef _DEBUG
                Logger::write(Logger::ZONE_INFO, "InputMapper", "Loaded action: " + action + " = " + keysStr);
#endif
            }
        }
        inFile.close();
    }
    else {
        Logger::write(Logger::ZONE_WARNING, "InputMapper", "Could not read " + actionsFile_ + "; using default mappings");
        // Default mappings
        actionMappings_["navigateUp"] = { "Up", "joyHat0Up" };
        actionMappings_["navigateDown"] = { "Down", "joyHat0Down" };
        actionMappings_["select"] = { "Return", "KP_Enter", "joyButton0" };
        actionMappings_["back"] = { "Escape", "joyButton1" };
        actionMappings_["delete"] = { "Backspace", "joyButton3" };
        actionMappings_["toggle"] = { "Space", "joyButton2" };
        actionMappings_["increaseSmall"] = { "Right", "joyHat0Right" };
        actionMappings_["decreaseSmall"] = { "Left", "joyHat0Left" };
        actionMappings_["increaseLarge"] = { "Right", "joyHat0Right" };
        actionMappings_["decreaseLarge"] = { "Left", "joyHat0Left" };
    }
}

bool InputMapper::isActionTriggered(const std::string& action, const SDL_Event& event) const {
    auto it = actionMappings_.find(action);
    if (it == actionMappings_.end()) return false;

    const auto& keys = it->second;
    if (event.type == SDL_KEYDOWN) {
        std::string keyName = SDL_GetKeyName(event.key.keysym.sym);
        return std::find(keys.begin(), keys.end(), keyName) != keys.end();
    }
    else if (event.type == SDL_JOYBUTTONDOWN) {
        std::string joyKey = "joy" + std::to_string(event.jbutton.which) + "Button" + std::to_string(event.jbutton.button);
        return std::find(keys.begin(), keys.end(), joyKey) != keys.end();
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
        default: return false;
        }
        std::string joyKey = "joy" + std::to_string(event.jhat.which) + "Hat" + std::to_string(event.jhat.hat) + direction;
        return std::find(keys.begin(), keys.end(), joyKey) != keys.end();
    }
    return false;
}

const std::vector<std::string>& InputMapper::getKeysForAction(const std::string& action) const {
    static const std::vector<std::string> empty;
    auto it = actionMappings_.find(action);
    return it != actionMappings_.end() ? it->second : empty;
}

void InputMapper::updateAction(const std::string& action, const std::vector<std::string>& keys) {
    actionMappings_[action] = keys;
}

void InputMapper::save() {
    std::ofstream outFile(actionsFile_, std::ios::out | std::ios::trunc);
    if (outFile.is_open()) {
        outFile << "# RetroFE Actions Configuration\n";
        for (const auto& pair : actionMappings_) {
            std::string keysStr;
            for (size_t i = 0; i < pair.second.size(); ++i) {
                keysStr += pair.second[i];
                if (i < pair.second.size() - 1) keysStr += ",";
            }
            outFile << pair.first << " = " << keysStr << "\n";
        }
        outFile.close();
        Logger::write(Logger::ZONE_INFO, "InputMapper", "Saved actions to " + actionsFile_);
    }
    else {
        Logger::write(Logger::ZONE_ERROR, "InputMapper", "Failed to save " + actionsFile_);
    }
}

const std::map<std::string, std::vector<std::string>>& InputMapper::getActionMappings() const {
    return actionMappings_;
}