#pragma once
#include <SDL.h>
#include <string>
#include <vector>
#include "../../Utility/Utils.h"
#include <stdexcept>

// Options Menu Color Component

class Color {
public:
    SDL_Color sdlColor;

    // Constructor: Parses both decimal ("255,255,255,255") and hex ("#RRGGBBAA") strings
    Color(const std::string& colorStr = "255,255,255,255") {
        if (colorStr.empty()) {
            sdlColor = { 255, 255, 255, 255 };  // Default to white if empty
            return;
        }

        if (colorStr[0] == '#') {
            parseHexColor(colorStr);
        }
        else {
            parseDecimalColor(colorStr);
        }
    }

    // Apply the color to the renderer, optionally modulated by an alpha value
    void apply(SDL_Renderer* renderer, float alpha = 1.0f) const {
        Uint8 finalAlpha = static_cast<Uint8>(sdlColor.a * alpha);
        SDL_SetRenderDrawColor(renderer, sdlColor.r, sdlColor.g, sdlColor.b, finalAlpha);
    }

private:
    // Parse hex color strings like "#RRGGBB" or "#RRGGBBAA"
    void parseHexColor(const std::string& hexStr) {
        std::string hex = hexStr.substr(1);  // Remove '#'
        if (hex.length() == 6) {
            sdlColor.r = static_cast<Uint8>(std::stoi(hex.substr(0, 2), nullptr, 16));
            sdlColor.g = static_cast<Uint8>(std::stoi(hex.substr(2, 2), nullptr, 16));
            sdlColor.b = static_cast<Uint8>(std::stoi(hex.substr(4, 2), nullptr, 16));
            sdlColor.a = 255;  // Default to fully opaque
        }
        else if (hex.length() == 8) {
            sdlColor.r = static_cast<Uint8>(std::stoi(hex.substr(0, 2), nullptr, 16));
            sdlColor.g = static_cast<Uint8>(std::stoi(hex.substr(2, 2), nullptr, 16));
            sdlColor.b = static_cast<Uint8>(std::stoi(hex.substr(4, 2), nullptr, 16));
            sdlColor.a = static_cast<Uint8>(std::stoi(hex.substr(6, 2), nullptr, 16));
        }
        else {
            throw std::invalid_argument("Invalid hex color length");
        }
    }

    // Parse decimal color strings like "255,0,0,255"
    void parseDecimalColor(const std::string& decimalStr) {
        std::vector<std::string> rgba = Utils::split(decimalStr, ',');
        sdlColor.r = rgba.size() > 0 ? static_cast<Uint8>(std::stoi(rgba[0])) : 255;
        sdlColor.g = rgba.size() > 1 ? static_cast<Uint8>(std::stoi(rgba[1])) : 255;
        sdlColor.b = rgba.size() > 2 ? static_cast<Uint8>(std::stoi(rgba[2])) : 255;
        sdlColor.a = rgba.size() > 3 ? static_cast<Uint8>(std::stoi(rgba[3])) : 255;
    }
};