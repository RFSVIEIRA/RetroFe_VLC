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
#include "OptionsMenu.h"
#include "../Control/UserInput.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include "../utility/Log.h"
#include "../Utility/Utils.h"
#include <numeric>
#include <filesystem>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include "../Graphics/Page.h"
#include "../SDL.h"
#ifdef WIN32
#include <Windows.h>
#include <TlHelp32.h>
#endif
#include <codecvt>

inline bool utf8Decode(const std::string& s, size_t& i, uint32_t& out)
{
    unsigned char c = s[i];
    if (c < 0x80) {                     // 1-byte ASCII
        out = c;
        i += 1;
        return true;
    }
    else if ((c >> 5) == 0x6) {         // 2-byte sequence
        if (i + 1 >= s.size()) return false;
        out = ((c & 0x1F) << 6) |
            (s[i + 1] & 0x3F);
        i += 2;
        return true;
    }
    else if ((c >> 4) == 0xE) {         // 3-byte sequence
        if (i + 2 >= s.size()) return false;
        out = ((c & 0x0F) << 12) |
            ((s[i + 1] & 0x3F) << 6) |
            (s[i + 2] & 0x3F);
        i += 3;
        return true;
    }
    else if ((c >> 3) == 0x1E) {        // 4-byte sequence
        if (i + 3 >= s.size()) return false;
        out = ((c & 0x07) << 18) |
            ((s[i + 1] & 0x3F) << 12) |
            ((s[i + 2] & 0x3F) << 6) |
            (s[i + 3] & 0x3F);
        i += 4;
        return true;
    }
    return false;
}

std::string OptionsMenu::findConfigPath(const std::string& layoutDir) {
    // layout.xml is ALWAYS the primary authority for the layout (master configuration)
    std::string l = Utils::combinePath(layoutDir, "layout.xml");
    if (std::filesystem::exists(l)) {
        return l;
    }

    // Fallbacks only if layout.xml does not exist
    std::string opt2 = Utils::combinePath(layoutDir, "options2.xml");
    if (std::filesystem::exists(opt2)) return opt2;
    std::string opt = Utils::combinePath(layoutDir, "options.xml");
    if (std::filesystem::exists(opt)) return opt;
    std::string optMenu = Utils::combinePath(layoutDir, "optionsMenu.xml");
    if (std::filesystem::exists(optMenu)) return optMenu;
    return "";
}

OptionsMenu::OptionsMenu(Configuration& config, Page* page)
    : config_(config),
      page_(page),
      menuConfig_(findConfigPath([this]() {
          std::string layoutName;
          config_.getProperty("layout", layoutName);
          if (layoutName.empty()) layoutName = "default";
          return Utils::combinePath(Configuration::absolutePath, "layouts", layoutName);
      }())),
      layoutMenu_([this]() {
          std::string layoutName;
          config_.getProperty("layout", layoutName);
          if (layoutName.empty()) layoutName = "default";
          return Utils::combinePath(Configuration::absolutePath, "layouts", layoutName);
      }()),
      titleFont_(nullptr),
      optionFont_(nullptr),
      selectedOption_(0),
      running_(false),
      settingsChanged_(false),
      savedChanges_(false),
      quitSelected_(false),
      kioskModeSelected_(false),
      signOutSelected_(false),
      restartSelected_(false),
      shutDownSelected_(false),
      currentSection_(nullptr),
      windowWidth_(1920),
      windowHeight_(1080),
      originalWindowWidth_(1920),
      originalWindowHeight_(1080),
      scaleFactor_(1.0f)
{
    initMainOptions();
}

OptionsMenu::~OptionsMenu() {
    close();
}

void OptionsMenu::close() {
    running_ = false;
    for (auto& img : images_) {
        if (img.isAnimated && img.frames) {
            for (int i = 0; i < img.frameCount; ++i) {
                if (img.frames[i]) SDL_DestroyTexture(img.frames[i]);
            }
            SDL_free(img.frames);
            img.frames = nullptr;
        }
        else if (img.texture) {
            SDL_DestroyTexture(img.texture);
            img.texture = nullptr;
        }
    }
    images_.clear();
    for (auto& pair : discs_) {
        if (pair.second) {
            SDL_DestroyTexture(pair.second);
        }
    }
    discs_.clear();
    for (auto& pair : iconCache_) {
        if (pair.second) {
            SDL_DestroyTexture(pair.second);
        }
    }
    iconCache_.clear();
    for (auto& pair : buttonTexCache_) {
        if (pair.second) {
            SDL_DestroyTexture(pair.second);
        }
    }
    buttonTexCache_.clear();
    buttonTexSize_.clear();
    if (titleFont_) {
        delete titleFont_;
        titleFont_ = nullptr;
    }
    if (optionFont_) {
        delete optionFont_;
        optionFont_ = nullptr;
    }
}

bool OptionsMenu::open(SDL_Renderer* renderer) {
    if (!renderer) {
        renderer = SDL::getRenderer(0);
    }
    if (!renderer) {
        Logger::write(Logger::ZONE_ERROR, "OptionsMenu", "No valid renderer available");
        return false;
    }

    // Get display dimensions
    windowWidth_ = SDL::getWindowWidth(0);
    windowHeight_ = SDL::getWindowHeight(0);
    if (windowWidth_ <= 0 || windowHeight_ <= 0) {
        SDL_GetRendererOutputSize(renderer, &windowWidth_, &windowHeight_);
    }
    if (windowWidth_ <= 0) windowWidth_ = 1920;
    if (windowHeight_ <= 0) windowHeight_ = 1080;

    originalWindowWidth_ = menuConfig_.getWindowWidth() > 0 ? menuConfig_.getWindowWidth() : 1920;
    originalWindowHeight_ = menuConfig_.getWindowHeight() > 0 ? menuConfig_.getWindowHeight() : 1080;
    float scaleX = (originalWindowWidth_ > 0) ? static_cast<float>(windowWidth_) / originalWindowWidth_ : 1.0f;
    float scaleY = (originalWindowHeight_ > 0) ? static_cast<float>(windowHeight_) / originalWindowHeight_ : 1.0f;
    std::string winScale = menuConfig_.getWindowScale();
    if (winScale == "stretch") {
        scaleFactor_ = scaleX;
    } else {
        // "fit" or default: keep aspect ratio to prevent clipping vertical content
        scaleFactor_ = (menuConfig_.getWindowWidth() > 0 && menuConfig_.getWindowHeight() > 0)
            ? (std::min)(scaleX, scaleY)
            : scaleX;
    }

    // Inherit layout font if not explicitly set
    if (page_ && !page_->getSearchConfig().font.empty() &&
        (menuConfig_.getFontPath().empty() || menuConfig_.getFontPath() == "C:/Windows/Fonts/Arial.ttf")) {
        menuConfig_.setFontPath(page_->getSearchConfig().font);
    }

    std::string resolvedFontPath = menuConfig_.getFontPath();
    if (!resolvedFontPath.empty()) {
        bool isAbs = (resolvedFontPath.size() > 1 && resolvedFontPath[1] == ':') || resolvedFontPath[0] == '/' || resolvedFontPath[0] == '\\';
        if (!isAbs) {
            std::string layoutName;
            config_.getProperty("layout", layoutName);
            if (layoutName.empty()) layoutName = "default";
            std::string candidate = Utils::combinePath(Configuration::absolutePath, "layouts", layoutName, resolvedFontPath);
            if (std::filesystem::exists(candidate)) {
                resolvedFontPath = candidate;
            }
        }
    }

    // Initialize fonts
    int scaledTitleFontSize = static_cast<int>(menuConfig_.getTitleFontSize() * scaleFactor_);
    if (scaledTitleFontSize < 10) scaledTitleFontSize = 10;
    titleFont_ = new Font(resolvedFontPath, scaledTitleFontSize, menuConfig_.getTitleColor(), 0);
    if (!titleFont_ || !titleFont_->initialize(renderer)) {
        delete titleFont_;
        titleFont_ = new Font("C:/Windows/Fonts/Arial.ttf", scaledTitleFontSize, menuConfig_.getTitleColor(), 0);
        if (!titleFont_ || !titleFont_->initialize(renderer)) {
            delete titleFont_;
            titleFont_ = nullptr;
        }
    }

    int scaledOptionFontSize = static_cast<int>(menuConfig_.getOptionFontSize() * scaleFactor_);
    if (scaledOptionFontSize < 10) scaledOptionFontSize = 10;
    optionFont_ = new Font(resolvedFontPath, scaledOptionFontSize, menuConfig_.getOptionColor(), 0);
    if (!optionFont_ || !optionFont_->initialize(renderer)) {
        delete optionFont_;
        optionFont_ = new Font("C:/Windows/Fonts/Arial.ttf", scaledOptionFontSize, menuConfig_.getOptionColor(), 0);
        if (!optionFont_ || !optionFont_->initialize(renderer)) {
            delete optionFont_;
            optionFont_ = nullptr;
        }
    }

    // Re-initialize main options to ensure icons and items are up to date
    initMainOptions();

    // Load item icons
    for (auto& item : mainOptionItems_) {
        if (!item.iconPath.empty()) {
            std::string resolvedPath = item.iconPath;
            if (!std::filesystem::exists(resolvedPath)) {
                std::string layoutName;
                config_.getProperty("layout", layoutName);
                if (layoutName.empty()) layoutName = "default";
                std::string cand = Utils::combinePath(Configuration::absolutePath, "layouts", layoutName, item.iconPath);
                if (std::filesystem::exists(cand)) resolvedPath = cand;
            }
            if (std::filesystem::exists(resolvedPath)) {
                auto it = iconCache_.find(resolvedPath);
                if (it != iconCache_.end()) {
                    item.iconTexture = it->second;
                } else {
                    SDL_Texture* tex = IMG_LoadTexture(renderer, resolvedPath.c_str());
                    if (tex) {
                        iconCache_[resolvedPath] = tex;
                        item.iconTexture = tex;
                    }
                }
                if (item.iconTexture) {
                    SDL_QueryTexture(item.iconTexture, nullptr, nullptr, &item.iconW, &item.iconH);
                }
            }
        }
    }

    // Load images
    images_.clear();
    for (auto& imgConfig : menuConfig_.getImages()) {
        ImageInstance instance;
        instance.isAnimated = imgConfig.isAnimated;
        instance.isMenu = imgConfig.isMenu;
        instance.scale = imgConfig.scale;
        instance.xAlign = imgConfig.xAlign;
        instance.yAlign = imgConfig.yAlign;
        instance.xOffset = imgConfig.xOffset;
        instance.yOffset = imgConfig.yOffset;
        instance.dest.x = imgConfig.x;
        instance.dest.y = imgConfig.y;
        instance.dest.w = imgConfig.width;
        instance.dest.h = imgConfig.height;
        instance.xOrigin = imgConfig.xOrigin;
        instance.yOrigin = imgConfig.yOrigin;
        instance.hasExplicitXOrigin = imgConfig.hasExplicitXOrigin;
        instance.hasExplicitYOrigin = imgConfig.hasExplicitYOrigin;
        instance.alpha = imgConfig.alpha;
        instance.layer = imgConfig.layer;

        std::string imgPath = imgConfig.src;
        if (!imgPath.empty()) {
            bool isAbs = (imgPath.size() > 1 && imgPath[1] == ':') || imgPath[0] == '/' || imgPath[0] == '\\';
            if (!isAbs) {
                std::string layoutName;
                config_.getProperty("layout", layoutName);
                if (layoutName.empty()) layoutName = "default";
                imgPath = Utils::combinePath(Configuration::absolutePath, "layouts", layoutName, imgPath);
            }
        }

        if (instance.isAnimated) {
            IMG_Animation* anim = IMG_LoadAnimation(imgPath.c_str());
            if (anim) {
                instance.frameCount = anim->count;
                instance.frames = (SDL_Texture**)SDL_calloc(anim->count, sizeof(SDL_Texture*));
                instance.frameDelays.resize(anim->count);
                for (int i = 0; i < anim->count; ++i) {
                    instance.frames[i] = SDL_CreateTextureFromSurface(renderer, anim->frames[i]);
                    instance.frameDelays[i] = anim->delays[i] > 0 ? anim->delays[i] : 100;
                    if (instance.frames[i]) SDL_SetTextureBlendMode(instance.frames[i], SDL_BLENDMODE_BLEND);
                }
                instance.currentFrame = 0;
                instance.lastFrameTime = SDL_GetTicks();
                IMG_FreeAnimation(anim);
                images_.push_back(instance);
            }
        }
        else {
            SDL_Surface* surf = IMG_Load(imgPath.c_str());
            if (surf) {
                instance.texture = SDL_CreateTextureFromSurface(renderer, surf);
                if (instance.texture) SDL_SetTextureBlendMode(instance.texture, SDL_BLENDMODE_BLEND);
                SDL_FreeSurface(surf);
                images_.push_back(instance);
            }
        }
    }

    selectedOption_ = 0;
    currentSection_ = nullptr;
    openTicks_ = SDL_GetTicks();
    running_ = true;
    return true;
}

bool OptionsMenu::show(SDL_Renderer* renderer, std::function<void()> renderBase, UserInput* input) {
    if (!renderer) {
        renderer = SDL::getRenderer(0);
    }
    if (!open(renderer)) {
        return false;
    }

    running_ = true;
    while (running_) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (input) {
                input->update(event);
            }
            handleEvent(event, input);
        }

        if (renderBase) {
            renderBase();
        }
        else {
            draw(renderer);
            SDL_RenderPresent(renderer);
        }
        SDL_Delay(16);
    }

    close();
    if (input) {
        input->resetStates();
    }
    return settingsChanged_;
}

SDL_Texture* OptionsMenu::discTexture(SDL_Renderer* renderer, int radius) {
    auto it = discs_.find(radius);
    if (it != discs_.end()) return it->second;

    int d = radius * 2;
    std::vector<Uint8> pixels(d * d * 4, 0);
    float r = static_cast<float>(radius);
    int ss = 4;
    float ssf = static_cast<float>(ss);

    for (int py = 0; py < d; ++py) {
        for (int px = 0; px < d; ++px) {
            int inside = 0;
            for (int sy = 0; sy < ss; ++sy) {
                float syf = py + (sy + 0.5f) / ssf;
                float dy = syf - r;
                for (int sx = 0; sx < ss; ++sx) {
                    float sxf = px + (sx + 0.5f) / ssf;
                    float dx = sxf - r;
                    if (dx * dx + dy * dy <= r * r) ++inside;
                }
            }
            if (inside == 0) continue;
            size_t o = static_cast<size_t>((py * d + px) * 4);
            pixels[o + 0] = 255;
            pixels[o + 1] = 255;
            pixels[o + 2] = 255;
            pixels[o + 3] = static_cast<Uint8>(255 * inside / (ss * ss));
        }
    }

    SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, d, d);
    if (tex) {
        SDL_UpdateTexture(tex, NULL, &pixels[0], d * 4);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    }
    discs_[radius] = tex;
    return tex;
}

void OptionsMenu::rrect(SDL_Renderer* renderer, int x, int y, int w, int h, int radius, SDL_Color c, int alpha) {
    if (w <= 0 || h <= 0 || alpha <= 0) return;
    radius = (std::min)(radius, (std::min)(w / 2, h / 2));
    SDL_Texture* disc = radius >= 2 ? discTexture(renderer, radius) : NULL;
    if (!disc) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, static_cast<Uint8>(alpha));
        SDL_Rect r = { x, y, w, h };
        SDL_RenderFillRect(renderer, &r);
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, static_cast<Uint8>(alpha));
    SDL_Rect r1 = { x + radius, y, w - 2 * radius, h };
    SDL_Rect r2 = { x, y + radius, radius, h - 2 * radius };
    SDL_Rect r3 = { x + w - radius, y + radius, radius, h - 2 * radius };
    SDL_RenderFillRect(renderer, &r1);
    SDL_RenderFillRect(renderer, &r2);
    SDL_RenderFillRect(renderer, &r3);

    SDL_SetTextureColorMod(disc, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(disc, static_cast<Uint8>(alpha));

    int r = radius;
    SDL_Rect src;
    SDL_Rect dst;
    src.w = src.h = dst.w = dst.h = r;

    src.x = 0; src.y = 0; dst.x = x;         dst.y = y;
    SDL_RenderCopy(renderer, disc, &src, &dst);
    src.x = r; src.y = 0; dst.x = x + w - r; dst.y = y;
    SDL_RenderCopy(renderer, disc, &src, &dst);
    src.x = 0; src.y = r; dst.x = x;         dst.y = y + h - r;
    SDL_RenderCopy(renderer, disc, &src, &dst);
    src.x = r; src.y = r; dst.x = x + w - r; dst.y = y + h - r;
    SDL_RenderCopy(renderer, disc, &src, &dst);
}

void OptionsMenu::glow(SDL_Renderer* renderer, int x, int y, int w, int h, int radius, SDL_Color c, int strength) {
    const int layers = 5;
    int step = (std::max)(2, radius / 3);
    for (int i = layers; i >= 1; --i) {
        int g = i * step;
        rrect(renderer, x - g, y - g, w + 2 * g, h + 2 * g, radius + g, c, strength / (i + 1));
    }
}

void OptionsMenu::draw(SDL_Renderer* renderer) {
    if (!renderer || !running_) return;

    // 1. Draw Backdrop
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_Color bg = menuConfig_.getBackgroundColor();
    SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, bg.a);
    SDL_Rect fullRect = { 0, 0, windowWidth_, windowHeight_ };
    SDL_RenderFillRect(renderer, &fullRect);

    // 1.5 Draw Modal Card with Rounded Corners (if configured)
    SDL_Color panelCol = menuConfig_.getPanelColor();
    SDL_Color borderCol = menuConfig_.getBorderColor();
    int radius = static_cast<int>(menuConfig_.getCornerRadius() * scaleFactor_);

    if (panelCol.a > 0 || borderCol.a > 0 || radius > 0) {
        size_t currentItemCount = (currentSection_ != nullptr) ? currentSection_->getItemCount() : mainOptions_.size();
        int modalX = 0, modalY = 0, modalW = 0, modalH = 0;
        menuConfig_.calculateModalBounds(currentItemCount, windowWidth_, windowHeight_, scaleFactor_, modalX, modalY, modalW, modalH);

        // Soft outer glow
        SDL_Color glowCol = menuConfig_.getGlowColor();
        int glowStr = menuConfig_.getGlowStrength();
        if (glowCol.a > 0 && glowStr > 0) {
            glow(renderer, modalX, modalY, modalW, modalH, radius, glowCol, glowStr);
        }

        // Border and Panel
        int bw = static_cast<int>(menuConfig_.getBorderWidth() * scaleFactor_);
        if (bw < 1 && borderCol.a > 0) bw = 1;

        if (borderCol.a > 0 && bw > 0) {
            rrect(renderer, modalX, modalY, modalW, modalH, radius, borderCol, borderCol.a);
            if (panelCol.a > 0) {
                int innerRad = (std::max)(0, radius - bw);
                rrect(renderer, modalX + bw, modalY + bw, modalW - 2 * bw, modalH - 2 * bw, innerRad, panelCol, panelCol.a);
            }
        }
        else if (panelCol.a > 0) {
            rrect(renderer, modalX, modalY, modalW, modalH, radius, panelCol, panelCol.a);
        }
    }

    // 1.8 Draw Custom Containers / Lines
    for (const auto& cont : menuConfig_.getContainers()) {
        int cW = static_cast<int>(cont.width * scaleFactor_);
        int cH = static_cast<int>(cont.height * scaleFactor_);
        int cX = 0;
        switch (cont.xAlign) {
        case OptionsMenuBuilder::XAlignment::Center:
            cX = (windowWidth_ - cW) / 2 + static_cast<int>(cont.xOffset * scaleFactor_);
            break;
        case OptionsMenuBuilder::XAlignment::Left:
            cX = static_cast<int>(cont.xOffset * scaleFactor_);
            break;
        case OptionsMenuBuilder::XAlignment::Right:
            cX = windowWidth_ - cW + static_cast<int>(cont.xOffset * scaleFactor_);
            break;
        default:
            cX = static_cast<int>(cont.x * scaleFactor_);
            break;
        }

        int cY = 0;
        switch (cont.yAlign) {
        case OptionsMenuBuilder::YAlignment::Center:
            cY = (windowHeight_ - cH) / 2 + static_cast<int>(cont.yOffset * scaleFactor_);
            break;
        case OptionsMenuBuilder::YAlignment::Top:
            cY = static_cast<int>(cont.yOffset * scaleFactor_);
            break;
        case OptionsMenuBuilder::YAlignment::Bottom:
            cY = windowHeight_ - cH + static_cast<int>(cont.yOffset * scaleFactor_);
            break;
        default:
            cY = static_cast<int>(cont.y * scaleFactor_);
            break;
        }

        int cRad = static_cast<int>(cont.cornerRadius * scaleFactor_);
        Uint8 cAlpha = static_cast<Uint8>(cont.color.a * cont.alpha);
        if (cRad > 0) {
            rrect(renderer, cX, cY, cW, cH, cRad, cont.color, cAlpha);
        } else {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, cont.color.r, cont.color.g, cont.color.b, cAlpha);
            SDL_Rect cRect = { cX, cY, cW, cH };
            SDL_RenderFillRect(renderer, &cRect);
        }
    }

    // 2. Draw Images (sorted by layer)
    std::stable_sort(images_.begin(), images_.end(), [](const ImageInstance& a, const ImageInstance& b) {
        return a.layer < b.layer;
    });

    for (auto& img : images_) {
        if (img.isMenu) continue;
        SDL_Texture* tex = img.isAnimated ? (img.frames ? img.frames[img.currentFrame] : nullptr) : img.texture;
        if (!tex) continue;

        if (img.isAnimated && img.frameCount > 1) {
            Uint32 now = SDL_GetTicks();
            if (now - img.lastFrameTime >= img.frameDelays[img.currentFrame]) {
                img.currentFrame = (img.currentFrame + 1) % img.frameCount;
                img.lastFrameTime = now;
            }
        }

        int texW, texH;
        SDL_QueryTexture(tex, nullptr, nullptr, &texW, &texH);
        SDL_Rect srcRect = { 0, 0, texW, texH };
        int scaledW = static_cast<int>(img.dest.w * scaleFactor_);
        int scaledH = static_cast<int>(img.dest.h * scaleFactor_);
        SDL_Rect destRect = { 0, 0, scaledW, scaledH };
        int scaledXOff = static_cast<int>(img.xOffset * scaleFactor_);
        int scaledYOff = static_cast<int>(img.yOffset * scaleFactor_);

        int baseX = 0;
        switch (img.xAlign) {
        case OptionsMenuBuilder::XAlignment::Center:
            baseX = windowWidth_ / 2;
            break;
        case OptionsMenuBuilder::XAlignment::Left:
            baseX = 0;
            break;
        case OptionsMenuBuilder::XAlignment::Right:
            baseX = windowWidth_;
            break;
        default:
            baseX = static_cast<int>(img.dest.x * scaleFactor_);
            break;
        }

        if (img.hasExplicitXOrigin) {
            destRect.x = static_cast<int>(baseX + scaledXOff - (img.xOrigin * scaledW));
        } else {
            switch (img.xAlign) {
            case OptionsMenuBuilder::XAlignment::Center:
                destRect.x = (windowWidth_ - scaledW) / 2 + scaledXOff;
                break;
            case OptionsMenuBuilder::XAlignment::Left:
                destRect.x = scaledXOff;
                break;
            case OptionsMenuBuilder::XAlignment::Right:
                destRect.x = windowWidth_ - scaledW + scaledXOff;
                break;
            default:
                destRect.x = static_cast<int>(img.dest.x * scaleFactor_);
                break;
            }
        }

        int baseY = 0;
        switch (img.yAlign) {
        case OptionsMenuBuilder::YAlignment::Center:
            baseY = windowHeight_ / 2;
            break;
        case OptionsMenuBuilder::YAlignment::Top:
            baseY = 0;
            break;
        case OptionsMenuBuilder::YAlignment::Bottom:
            baseY = windowHeight_;
            break;
        default:
            baseY = static_cast<int>(img.dest.y * scaleFactor_);
            break;
        }

        if (img.hasExplicitYOrigin) {
            destRect.y = static_cast<int>(baseY + scaledYOff - (img.yOrigin * scaledH));
        } else {
            switch (img.yAlign) {
            case OptionsMenuBuilder::YAlignment::Center:
                destRect.y = (windowHeight_ - scaledH) / 2 + scaledYOff;
                break;
            case OptionsMenuBuilder::YAlignment::Top:
                destRect.y = scaledYOff;
                break;
            case OptionsMenuBuilder::YAlignment::Bottom:
                destRect.y = windowHeight_ - scaledH + scaledYOff;
                break;
            default:
                destRect.y = static_cast<int>(img.dest.y * scaleFactor_);
                break;
            }
        }

        if (img.alpha < 1.0f) {
            SDL_SetTextureAlphaMod(tex, static_cast<Uint8>(img.alpha * 255.0f));
        } else {
            SDL_SetTextureAlphaMod(tex, 255);
        }

        if (img.scale == "fit") {
            float aspectTex = static_cast<float>(texW) / texH;
            float aspectDest = static_cast<float>(destRect.w) / destRect.h;
            if (aspectTex > aspectDest) {
                destRect.h = static_cast<int>(destRect.w / aspectTex);
                destRect.y += (scaledH - destRect.h) / 2;
            }
            else {
                destRect.w = static_cast<int>(destRect.h * aspectTex);
                destRect.x += (scaledW - destRect.w) / 2;
            }
        }
        SDL_RenderCopy(renderer, tex, (img.scale == "fill") ? &srcRect : nullptr, &destRect);
    }

    // 3. Render Section or Main Menu
    if (currentSection_ != nullptr) {
        currentSection_->render(renderer, titleFont_, optionFont_, windowWidth_, windowHeight_, menuConfig_, scaleFactor_);
        return;
    }

    if (!titleFont_ || !optionFont_) return;

    // Check if modal card layout is enabled
    bool isModal = menuConfig_.isModal();
    int modalX = 0, modalY = 0, modalW = 0, modalH = 0;
    if (isModal) {
        menuConfig_.calculateModalBounds(mainOptions_.size(), windowWidth_, windowHeight_, scaleFactor_, modalX, modalY, modalW, modalH);
    } else {
        modalW = windowWidth_;
        modalH = windowHeight_;
        modalX = 0;
        modalY = 0;
    }

    // 4. Render Main Title
    std::string title = menuConfig_.getMainTitleText();
    float titleWidth = 0.0f;
    for (size_t i = 0; i < title.size(); ) {
        uint32_t codepoint;
        if (!utf8Decode(title, i, codepoint)) continue;
        Font::GlyphInfo glyph;
        if (titleFont_->getRect(codepoint, glyph)) {
            titleWidth += glyph.advance;
        }
    }

    int scaledTitleXOffset = static_cast<int>(menuConfig_.getTitleXOffset() * scaleFactor_);
    float baseTitleX = 0.0f;
    if (menuConfig_.getTitleXAlignment() == OptionsMenuBuilder::XAlignment::Center) {
        baseTitleX = windowWidth_ / 2.0f;
    } else if (menuConfig_.getTitleXAlignment() == OptionsMenuBuilder::XAlignment::Right) {
        baseTitleX = static_cast<float>(windowWidth_);
    } else {
        baseTitleX = 0.0f;
    }
    int titleX = static_cast<int>(baseTitleX + scaledTitleXOffset - menuConfig_.getTitleXOrigin() * titleWidth);

    int scaledTitleYOffset = static_cast<int>(menuConfig_.getTitleYOffset() * scaleFactor_);
    float baseTitleY = 0.0f;
    if (menuConfig_.getTitleYAlignment() == OptionsMenuBuilder::YAlignment::Center) {
        baseTitleY = windowHeight_ / 2.0f;
    } else if (menuConfig_.getTitleYAlignment() == OptionsMenuBuilder::YAlignment::Bottom) {
        baseTitleY = static_cast<float>(windowHeight_);
    } else {
        baseTitleY = 0.0f;
    }
    int titleY = 0;
    if (isModal && menuConfig_.isModalAutoScale()) {
        int topPadding = (menuConfig_.getTitleYOffset() > 0 && menuConfig_.getTitleYOffset() < 100) ?
            static_cast<int>(menuConfig_.getTitleYOffset() * scaleFactor_) :
            static_cast<int>(35 * scaleFactor_);
        titleY = modalY + topPadding;
    } else {
        titleY = static_cast<int>(baseTitleY + scaledTitleYOffset - menuConfig_.getTitleYOrigin() * titleFont_->getHeight());
    }

    float renderX = static_cast<float>(titleX);
    for (size_t i = 0; i < title.size(); ) {
        uint32_t codepoint;
        if (!utf8Decode(title, i, codepoint)) continue;
        Font::GlyphInfo glyph;
        if (titleFont_->getRect(codepoint, glyph)) {
            SDL_Rect destRect = { static_cast<int>(renderX), titleY, glyph.rect.w, glyph.rect.h };
            SDL_SetTextureColorMod(titleFont_->getTexture(), menuConfig_.getTitleColor().r, menuConfig_.getTitleColor().g, menuConfig_.getTitleColor().b);
            SDL_RenderCopy(renderer, titleFont_->getTexture(), &glyph.rect, &destRect);
            renderX += glyph.advance;
        }
    }

    // Title Underline / Header Divider
    if (menuConfig_.getSubtitleUnderlineColor().a > 0) {
        int underlineY = titleY + titleFont_->getHeight() + static_cast<int>(8 * scaleFactor_);
        int lineW = (menuConfig_.getSeparatorWidth() > 0 && isModal) ?
            static_cast<int>(menuConfig_.getSeparatorWidth() * scaleFactor_) :
            static_cast<int>(titleWidth);
        int lineX = isModal ?
            (modalX + (modalW - lineW) / 2) :
            ((menuConfig_.getTitleXAlignment() == OptionsMenuBuilder::XAlignment::Center) ?
                ((windowWidth_ - lineW) / 2) :
                titleX);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, menuConfig_.getSubtitleUnderlineColor().r, menuConfig_.getSubtitleUnderlineColor().g, menuConfig_.getSubtitleUnderlineColor().b, menuConfig_.getSubtitleUnderlineColor().a);
        SDL_Rect uRect = { lineX, underlineY, lineW, static_cast<int>(2 * scaleFactor_) };
        SDL_RenderFillRect(renderer, &uRect);
    }

    // 5. Render Main Menu Options
    int scaledSpaceBetween = static_cast<int>(menuConfig_.getSpaceBetweenText() * scaleFactor_);
    if (scaledSpaceBetween < 20) scaledSpaceBetween = static_cast<int>(40 * scaleFactor_);

    int scaledMenuXOffset = static_cast<int>(menuConfig_.getMenuXOffset() * scaleFactor_);
    int scaledMenuYOffset = static_cast<int>(menuConfig_.getMenuYOffset() * scaleFactor_);
    int scaledTitleSpacing = static_cast<int>(menuConfig_.getTitleSpacing() * scaleFactor_);

    float baseMenuY = 0.0f;
    if (menuConfig_.getMenuYAlignment() == OptionsMenuBuilder::YAlignment::Center) {
        baseMenuY = windowHeight_ / 2.0f;
    } else if (menuConfig_.getMenuYAlignment() == OptionsMenuBuilder::YAlignment::Bottom) {
        baseMenuY = static_cast<float>(windowHeight_);
    } else {
        baseMenuY = 0.0f;
    }
    float totalMenuHeight = static_cast<float>(mainOptions_.size() * scaledSpaceBetween);
    int menuY = 0;
    if (isModal && menuConfig_.isModalAutoScale()) {
        int menuGap = (menuConfig_.getMenuYOffset() > 0 && menuConfig_.getMenuYOffset() < 100) ?
            static_cast<int>(menuConfig_.getMenuYOffset() * scaleFactor_) :
            static_cast<int>(30 * scaleFactor_);
        menuY = titleY + titleFont_->getHeight() + menuGap;
    } else {
        menuY = static_cast<int>(baseMenuY + scaledMenuYOffset - menuConfig_.getMenuYOrigin() * totalMenuHeight);
    }

    float baseOptionX = 0.0f;
    if (menuConfig_.getMenuXAlignment() == OptionsMenuBuilder::XAlignment::Center) {
        baseOptionX = windowWidth_ / 2.0f;
    } else if (menuConfig_.getMenuXAlignment() == OptionsMenuBuilder::XAlignment::Right) {
        baseOptionX = static_cast<float>(windowWidth_);
    } else {
        baseOptionX = 0.0f;
    }
    float optionAnchorX = baseOptionX + scaledMenuXOffset;

    int scaledPadding = static_cast<int>(menuConfig_.getSelectionBarPadding() * scaleFactor_);
    int scaledBarHeight = menuConfig_.getSelectionBarHeight() > 0 ?
        static_cast<int>(menuConfig_.getSelectionBarHeight() * scaleFactor_) :
        static_cast<int>(optionFont_->getHeight() + 8 * scaleFactor_);
    int scaledBarYOffset = static_cast<int>(menuConfig_.getSelectionBarYOffset() * scaleFactor_);

    for (size_t i = 0; i < mainOptions_.size(); ++i) {
        std::string text = mainOptions_[i];
        bool isSelected = (i == selectedOption_);

        float textWidth = 0.0f;
        size_t s = 0;
        while (s < text.size()) {
            uint32_t cp;
            if (!utf8Decode(text, s, cp)) break;
            if (cp < 32) continue;
            Font::GlyphInfo glyph;
            if (optionFont_->getRect(cp, glyph)) {
                textWidth += glyph.advance;
            }
        }

        int iconDrawW = 0;
        int iconGap = 0;
        int targetIconH = static_cast<int>(optionFont_->getHeight());
        if (i < mainOptionItems_.size() && mainOptionItems_[i].iconTexture && mainOptionItems_[i].iconH > 0) {
            iconDrawW = mainOptionItems_[i].iconW * targetIconH / mainOptionItems_[i].iconH;
            iconGap = static_cast<int>(12 * scaleFactor_);
        }
        float totalItemWidth = textWidth + iconDrawW + iconGap;

        int optionX = static_cast<int>(optionAnchorX - menuConfig_.getMenuXOrigin() * totalItemWidth);
        float y = menuY + static_cast<float>(i) * scaledSpaceBetween;

        // Selection bar
        if (isSelected) {
            SDL_Color barColor = menuConfig_.getSelectionBarColor();
            int barX;
            int barW;
            if (menuConfig_.getSeparatorWidth() > 0 && isModal) {
                barW = static_cast<int>(menuConfig_.getSeparatorWidth() * scaleFactor_);
                int scaledSepXOff = static_cast<int>(menuConfig_.getSeparatorXOffset() * scaleFactor_);
                if (menuConfig_.getSeparatorXAlignment() == OptionsMenuBuilder::XAlignment::Left) {
                    barX = (menuConfig_.isSeparatorAlignText() ? optionX : (optionX - scaledPadding)) + scaledSepXOff;
                } else if (menuConfig_.getSeparatorXAlignment() == OptionsMenuBuilder::XAlignment::Right) {
                    barX = modalX + modalW - barW - scaledPadding + scaledSepXOff;
                } else if (menuConfig_.getSeparatorXAlignment() == OptionsMenuBuilder::XAlignment::Numeric) {
                    int numVal = static_cast<int>((menuConfig_.getSeparatorX() > 0 ? menuConfig_.getSeparatorX() : menuConfig_.getSeparatorXOffset()) * scaleFactor_);
                    barX = (isModal && numVal < modalX) ? (modalX + numVal) : numVal;
                } else {
                    barX = modalX + (modalW - barW) / 2 + scaledSepXOff;
                }
            } else {
                barX = optionX - scaledPadding;
                barW = static_cast<int>(totalItemWidth) + 2 * scaledPadding;
            }
            int barY = static_cast<int>(y + scaledBarYOffset);

            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, barColor.r, barColor.g, barColor.b, barColor.a);
            SDL_Rect barRect = { barX, barY, barW, scaledBarHeight };
            SDL_RenderFillRect(renderer, &barRect);
        }

        // Separator line
        if (menuConfig_.getSeparatorColor().a > 0) {
            int sepW = menuConfig_.getSeparatorWidth() > 0 ?
                static_cast<int>(menuConfig_.getSeparatorWidth() * scaleFactor_) :
                static_cast<int>(textWidth + 2 * scaledPadding);
            int scaledSepXOff = static_cast<int>(menuConfig_.getSeparatorXOffset() * scaleFactor_);
            int sepX = 0;
            if (menuConfig_.getSeparatorXAlignment() == OptionsMenuBuilder::XAlignment::Left) {
                sepX = (menuConfig_.isSeparatorAlignText() ? optionX : (optionX - scaledPadding)) + scaledSepXOff;
            } else if (menuConfig_.getSeparatorXAlignment() == OptionsMenuBuilder::XAlignment::Right) {
                sepX = isModal ? (modalX + modalW - sepW - scaledPadding) : (windowWidth_ - sepW);
                sepX += scaledSepXOff;
            } else if (menuConfig_.getSeparatorXAlignment() == OptionsMenuBuilder::XAlignment::Numeric) {
                int numVal = static_cast<int>((menuConfig_.getSeparatorX() > 0 ? menuConfig_.getSeparatorX() : menuConfig_.getSeparatorXOffset()) * scaleFactor_);
                sepX = (isModal && numVal < modalX) ? (modalX + numVal) : numVal;
            } else {
                sepX = isModal ?
                    (modalX + (modalW - sepW) / 2) :
                    ((menuConfig_.getMenuXAlignment() == OptionsMenuBuilder::XAlignment::Center) ?
                        ((windowWidth_ - sepW) / 2 + scaledMenuXOffset) :
                        (optionX - scaledPadding));
                sepX += scaledSepXOff;
            }
            int sepY = static_cast<int>(y + optionFont_->getHeight() + menuConfig_.getSeparatorYOffset() * scaleFactor_);
            int sepH = static_cast<int>(menuConfig_.getSeparatorHeight() * scaleFactor_);
            if (sepH < 1) sepH = 1;

            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, menuConfig_.getSeparatorColor().r, menuConfig_.getSeparatorColor().g, menuConfig_.getSeparatorColor().b, menuConfig_.getSeparatorColor().a);
            SDL_Rect sepRect = { sepX, sepY, sepW, sepH };
            SDL_RenderFillRect(renderer, &sepRect);
        }

        // Draw icon if present
        float renderX = static_cast<float>(optionX);
        if (iconDrawW > 0 && i < mainOptionItems_.size() && mainOptionItems_[i].iconTexture) {
            SDL_Rect iconDst = { static_cast<int>(renderX), static_cast<int>(y), iconDrawW, targetIconH };
            SDL_RenderCopy(renderer, mainOptionItems_[i].iconTexture, nullptr, &iconDst);
            renderX += iconDrawW + iconGap;
        }

        // Option text glyphs
        SDL_Color lineColor = isSelected ? menuConfig_.getSelectedColor() : menuConfig_.getOptionColor();
        int renderY = static_cast<int>(y);
        s = 0;
        while (s < text.size()) {
            uint32_t cp;
            if (!utf8Decode(text, s, cp)) break;
            if (cp < 32) continue;
            Font::GlyphInfo glyph;
            if (optionFont_->getRect(cp, glyph)) {
                SDL_Rect destRect = { static_cast<int>(renderX), renderY, glyph.rect.w, glyph.rect.h };
                SDL_SetTextureColorMod(optionFont_->getTexture(), lineColor.r, lineColor.g, lineColor.b);
                SDL_RenderCopy(renderer, optionFont_->getTexture(), &glyph.rect, &destRect);
                renderX += glyph.advance;
            }
        }
    }

    drawFooterHints(renderer, modalX, modalY, modalW, modalH);
}

void OptionsMenu::handleEvent(SDL_Event& event, UserInput* input) {
    handleInput(event, input);
}

void OptionsMenu::handleInput(SDL_Event& event, UserInput* input) {
    if (event.type == SDL_QUIT) {
        running_ = false;
        quitSelected_ = true;
        return;
    }

    if (SDL_GetTicks() - openTicks_ < 150) {
        return;
    }

    if (currentSection_ != nullptr) {
        MenuAction action = currentSection_->handleInput(event, input);
        if (action == MenuAction::BACK) {
            currentSection_ = nullptr;
        }
        return;
    }

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

        if (menuUp) {
            selectedOption_ = (selectedOption_ == 0) ? mainOptions_.size() - 1 : selectedOption_ - 1;
        }
        else if (menuDown) {
            selectedOption_ = (selectedOption_ == mainOptions_.size() - 1) ? 0 : selectedOption_ + 1;
        }
        else if (menuSelect) {
            if (selectedOption_ >= mainOptionItems_.size()) return;

            const auto& item = mainOptionItems_[selectedOption_];

            switch (item.type) {
            case MainOptionItem::Type::MenuSection:
                layoutMenu_.setActiveMenuId(item.menuId, item.displayText);
                currentSection_ = &layoutMenu_;
                break;
            case MainOptionItem::Type::SaveExit:
                if (layoutMenu_.hasChanges()) {
                    layoutMenu_.save();
                    savedChanges_ = true;
                }
                running_ = false;
                break;
            case MainOptionItem::Type::SignOut:
                signOutSelected_ = true;
                running_ = false;
                break;
            case MainOptionItem::Type::Restart:
                restartSelected_ = true;
                running_ = false;
                break;
            case MainOptionItem::Type::ShutDown:
                shutDownSelected_ = true;
                running_ = false;
                break;
            case MainOptionItem::Type::Quit:
                quitSelected_ = true;
                running_ = false;
                break;
            }
        }
        else if (menuBack) {
            running_ = false;
        }
    }
}

void OptionsMenu::initMainOptions() {
    mainOptionItems_.clear();
    mainOptions_.clear();

    const auto& menuTitles = menuConfig_.getMenuTitles();

    std::string customSaveExitTitle;
    std::string customQuitTitle;
    std::string customSignOutTitle;
    std::string customRestartTitle;
    std::string customShutDownTitle;

    if (!menuTitles.empty()) {
        bool layoutAdded = false;

        for (const auto& entry : menuTitles) {
            std::string idLower = Utils::toLower(entry.id);
            std::string titleLower = Utils::toLower(entry.title);

            // Intercept system actions - these configure icons/titles, NOT separate menu sections
            if (idLower == "save_exit" || idLower == "save" || idLower == "saveexit" || idLower == "save_and_exit") {
                if (!entry.title.empty()) customSaveExitTitle = entry.title;
                continue;
            }
            if (idLower == "quit" || idLower == "exit") {
                if (!entry.title.empty()) customQuitTitle = entry.title;
                continue;
            }
            if (idLower == "signout" || idLower == "sign_out") {
                if (!entry.title.empty()) customSignOutTitle = entry.title;
                continue;
            }
            if (idLower == "restart") {
                if (!entry.title.empty()) customRestartTitle = entry.title;
                continue;
            }
            if (idLower == "shutdown" || idLower == "shut_down") {
                if (!entry.title.empty()) customShutDownTitle = entry.title;
                continue;
            }

            // Skip any entry that has no title (unless it's the layout section)
            if (entry.title.empty() && idLower != "layout") {
                continue;
            }

            if (idLower == "layout" || titleLower == "layout" || titleLower == Utils::toLower(menuConfig_.getLayoutTitleText())) {
                MainOptionItem item;
                item.displayText = entry.title.empty() ? menuConfig_.getLayoutTitleText() : entry.title;
                item.menuId = "";
                item.type = MainOptionItem::Type::MenuSection;
                item.iconPath = !entry.icon.empty() ? entry.icon : menuConfig_.getMenuIcon(entry.id);
                if (item.iconPath.empty()) item.iconPath = menuConfig_.getMenuIcon("layout");
                if (item.iconPath.empty()) item.iconPath = menuConfig_.getMenuIcon(item.displayText);
                if (!item.displayText.empty()) {
                    mainOptionItems_.push_back(item);
                    layoutAdded = true;
                }
            } else {
                MainOptionItem item;
                item.displayText = entry.title;
                item.menuId = entry.id;
                item.type = MainOptionItem::Type::MenuSection;
                item.iconPath = !entry.icon.empty() ? entry.icon : menuConfig_.getMenuIcon(entry.id);
                if (item.iconPath.empty()) item.iconPath = menuConfig_.getMenuIcon(entry.title);
                if (!item.displayText.empty()) {
                    mainOptionItems_.push_back(item);
                }
            }
        }

        if (!layoutAdded) {
            MainOptionItem item;
            item.displayText = menuConfig_.getLayoutTitleText();
            item.menuId = "";
            item.type = MainOptionItem::Type::MenuSection;
            item.iconPath = menuConfig_.getMenuIcon("layout");
            if (item.iconPath.empty()) item.iconPath = menuConfig_.getMenuIcon(item.displayText);
            if (!item.displayText.empty()) {
                mainOptionItems_.push_back(item);
            }
        }
    } else {
        // Default when no <menuTitles> is defined:
        // 1. Layout
        MainOptionItem layoutItem;
        layoutItem.displayText = menuConfig_.getLayoutTitleText();
        layoutItem.menuId = "";
        layoutItem.type = MainOptionItem::Type::MenuSection;
        layoutItem.iconPath = menuConfig_.getMenuIcon("layout");
        if (layoutItem.iconPath.empty()) layoutItem.iconPath = menuConfig_.getMenuIcon(layoutItem.displayText);
        if (!layoutItem.displayText.empty()) {
            mainOptionItems_.push_back(layoutItem);
        }

        // 2. Custom menus detected from LayoutMenu
        for (const auto& cm : layoutMenu_.getCustomMenus()) {
            MainOptionItem item;
            item.displayText = cm.second;
            item.menuId = cm.first;
            item.type = MainOptionItem::Type::MenuSection;
            item.iconPath = menuConfig_.getMenuIcon(cm.first);
            if (item.iconPath.empty()) item.iconPath = menuConfig_.getMenuIcon(cm.second);
            if (!item.displayText.empty()) {
                mainOptionItems_.push_back(item);
            }
        }
    }

    // Save & Exit
    MainOptionItem saveItem;
    saveItem.displayText = !customSaveExitTitle.empty() ? customSaveExitTitle : menuConfig_.getSaveExitText();
    saveItem.type = MainOptionItem::Type::SaveExit;
    saveItem.iconPath = menuConfig_.getMenuIcon("save_exit");
    if (saveItem.iconPath.empty()) saveItem.iconPath = menuConfig_.getMenuIcon("save");
    if (saveItem.iconPath.empty()) saveItem.iconPath = menuConfig_.getMenuIcon(saveItem.displayText);
    if (!saveItem.displayText.empty()) {
        mainOptionItems_.push_back(saveItem);
    }

    // Kiosk Mode options (only when in Kiosk Mode)
    if (isKioskModeActive()) {
        MainOptionItem signOutItem;
        signOutItem.displayText = !customSignOutTitle.empty() ? customSignOutTitle : menuConfig_.getSignOutText();
        signOutItem.type = MainOptionItem::Type::SignOut;
        signOutItem.iconPath = menuConfig_.getMenuIcon("signout");
        if (signOutItem.iconPath.empty()) signOutItem.iconPath = menuConfig_.getMenuIcon(signOutItem.displayText);
        if (!signOutItem.displayText.empty()) {
            mainOptionItems_.push_back(signOutItem);
        }

        MainOptionItem restartItem;
        restartItem.displayText = !customRestartTitle.empty() ? customRestartTitle : menuConfig_.getRestartText();
        restartItem.type = MainOptionItem::Type::Restart;
        restartItem.iconPath = menuConfig_.getMenuIcon("restart");
        if (restartItem.iconPath.empty()) restartItem.iconPath = menuConfig_.getMenuIcon(restartItem.displayText);
        if (!restartItem.displayText.empty()) {
            mainOptionItems_.push_back(restartItem);
        }

        MainOptionItem shutDownItem;
        shutDownItem.displayText = !customShutDownTitle.empty() ? customShutDownTitle : menuConfig_.getShutDownText();
        shutDownItem.type = MainOptionItem::Type::ShutDown;
        shutDownItem.iconPath = menuConfig_.getMenuIcon("shutdown");
        if (shutDownItem.iconPath.empty()) shutDownItem.iconPath = menuConfig_.getMenuIcon(shutDownItem.displayText);
        if (!shutDownItem.displayText.empty()) {
            mainOptionItems_.push_back(shutDownItem);
        }
    }

    // Quit
    MainOptionItem quitItem;
    quitItem.displayText = !customQuitTitle.empty() ? customQuitTitle : menuConfig_.getQuitText();
    quitItem.type = MainOptionItem::Type::Quit;
    quitItem.iconPath = menuConfig_.getMenuIcon("quit");
    if (quitItem.iconPath.empty()) quitItem.iconPath = menuConfig_.getMenuIcon("exit");
    if (quitItem.iconPath.empty()) quitItem.iconPath = menuConfig_.getMenuIcon(quitItem.displayText);
    if (!quitItem.displayText.empty()) {
        mainOptionItems_.push_back(quitItem);
    }

    for (const auto& item : mainOptionItems_) {
        if (!item.displayText.empty()) {
            mainOptions_.push_back(item.displayText);
        }
    }
}

bool OptionsMenu::isKioskModeActive() const {
#ifdef WIN32
    const std::vector<std::wstring> kioskProcesses = {
        L"SimpleKioskMode.exe",
        L"ShellInfrastructureHost.exe",
        L"ApplicationFrameHost.exe",
        L"dotnet.exe"
    };

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);
    bool kioskProcessFound = false;

    if (Process32FirstW(snapshot, &pe32)) {
        do {
            for (const auto& name : kioskProcesses) {
                if (_wcsicmp(pe32.szExeFile, name.c_str()) == 0) {
                    kioskProcessFound = true;
                    break;
                }
            }
            if (kioskProcessFound) break;
        } while (Process32NextW(snapshot, &pe32));
    }
    CloseHandle(snapshot);

    if (!kioskProcessFound) {
        return false;
    }

    HKEY hKey;
    const wchar_t* regPath = L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon";
    const wchar_t* valueName = L"Shell";
    wchar_t shellValue[512];
    DWORD valueSize = sizeof(shellValue);
    DWORD type;

    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, regPath, 0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS) {
        return false;
    }

    result = RegQueryValueExW(hKey, valueName, nullptr, &type, (LPBYTE)shellValue, &valueSize);
    RegCloseKey(hKey);
    if (result != ERROR_SUCCESS || type != REG_SZ) {
        return false;
    }

    std::wstring shellStr(shellValue);
    bool isShellMode = (
        shellStr.find(L"kiosk_shell.vbs") != std::wstring::npos ||
        shellStr.find(L"SimpleKioskMode.exe /shell") != std::wstring::npos ||
        shellStr.find(L"explorer.exe") == std::wstring::npos
        );

    return kioskProcessFound && isShellMode;
#else
    return false;
#endif
}

SDL_Texture* OptionsMenu::getButtonTexture(SDL_Renderer* renderer, const std::string& rawKey, int& texW, int& texH) {
    std::string key = Utils::toLower(rawKey);
    auto it = buttonTexCache_.find(key);
    if (it != buttonTexCache_.end()) {
        if (it->second) {
            auto sit = buttonTexSize_.find(key);
            if (sit != buttonTexSize_.end()) {
                texW = sit->second.first;
                texH = sit->second.second;
            }
        }
        return it->second;
    }

    std::string imgPath;
    const auto& bmap = menuConfig_.getButtonImages();
    auto bit = bmap.find(key);
    if (bit != bmap.end()) {
        imgPath = bit->second;
    }

    if (imgPath.empty()) {
        std::vector<std::string> aliases;
        if (key == "select" || key == "hintselect" || key == "a" || key == "btna") {
            static const char* a[] = { "a", "btna", "select", "hintselect" };
            aliases.assign(a, a + 4);
        } else if (key == "back" || key == "hintback" || key == "b" || key == "btnb") {
            static const char* a[] = { "b", "btnb", "back", "hintback" };
            aliases.assign(a, a + 4);
        } else if (key == "x" || key == "btnx" || key == "hintx") {
            static const char* a[] = { "x", "btnx", "hintx" };
            aliases.assign(a, a + 3);
        } else if (key == "y" || key == "btny" || key == "hinty") {
            static const char* a[] = { "y", "btny", "hinty" };
            aliases.assign(a, a + 3);
        } else if (key == "dpad" || key == "d-pad" || key == "hintdirections" || key == "directions" || key == "all") {
            static const char* a[] = { "dpad", "d-pad", "all", "directions", "hintdirections" };
            aliases.assign(a, a + 5);
        } else if (key == "down" || key == "hintdown" || key == "updown") {
            static const char* a[] = { "down", "updown", "hintdown" };
            aliases.assign(a, a + 3);
        } else if (key == "left" || key == "hintleft" || key == "leftright") {
            static const char* a[] = { "left", "leftright", "hintleft" };
            aliases.assign(a, a + 3);
        } else if (key == "up" || key == "hintup") {
            static const char* a[] = { "up", "updown", "hintup" };
            aliases.assign(a, a + 3);
        } else if (key == "lb" || key == "btnlb" || key == "l" || key == "l1" || key == "hintlb") {
            static const char* a[] = { "lb", "l", "btnlb", "l1", "hintlb" };
            aliases.assign(a, a + 5);
        } else if (key == "rb" || key == "btnrb" || key == "r" || key == "r1" || key == "hintrb") {
            static const char* a[] = { "rb", "r", "btnrb", "r1", "hintrb" };
            aliases.assign(a, a + 5);
        } else if (key == "start" || key == "btnstart" || key == "hintstart") {
            static const char* a[] = { "start", "btnstart", "hintstart" };
            aliases.assign(a, a + 3);
        }

        for (const auto& al : aliases) {
            for (const auto& pair : bmap) {
                if (Utils::toLower(pair.first) == al) {
                    imgPath = pair.second;
                    break;
                }
            }
            if (!imgPath.empty()) break;
        }
    }

    if (imgPath.empty() && !menuConfig_.getButtonFolder().empty()) {
        std::string cand = Utils::combinePath(menuConfig_.getButtonFolder(), key + ".png");
        if (std::filesystem::exists(cand)) imgPath = cand;
        else {
            cand = Utils::combinePath(menuConfig_.getButtonFolder(), key + ".jpg");
            if (std::filesystem::exists(cand)) imgPath = cand;
            else {
                cand = Utils::combinePath(menuConfig_.getButtonFolder(), key + ".webp");
                if (std::filesystem::exists(cand)) imgPath = cand;
            }
        }
    }

    if (imgPath.empty() || !std::filesystem::exists(imgPath)) {
        buttonTexCache_[key] = nullptr;
        return nullptr;
    }

    SDL_Texture* tex = IMG_LoadTexture(renderer, imgPath.c_str());
    if (tex) {
        SDL_QueryTexture(tex, nullptr, nullptr, &texW, &texH);
        buttonTexCache_[key] = tex;
        buttonTexSize_[key] = std::make_pair(texW, texH);
        return tex;
    }

    buttonTexCache_[key] = nullptr;
    return nullptr;
}

void OptionsMenu::drawFooterHints(SDL_Renderer* renderer, int modalX, int modalY, int modalW, int modalH) {
    if (!menuConfig_.hasButtonImages() || !optionFont_) return;

    struct Hint {
        std::string key;
        std::string label;
        std::string btnId;
    };
    std::vector<Hint> hints;
    hints.push_back({ "A", "Select", "btna" });
    hints.push_back({ "B", "Back", "btnb" });

    int footerH = static_cast<int>(32 * scaleFactor_);
    int footerY = (modalH > 0 && modalY > 0) ? (modalY + modalH - footerH - static_cast<int>(12 * scaleFactor_)) : (windowHeight_ - footerH - static_cast<int>(16 * scaleFactor_));
    int startX = (modalW > 0 && modalX > 0) ? (modalX + static_cast<int>(24 * scaleFactor_)) : static_cast<int>(32 * scaleFactor_);

    int curX = startX;
    int targetH = static_cast<int>(20 * scaleFactor_);
    if (targetH < 14) targetH = 14;

    for (const auto& h : hints) {
        int bw = 0, bh = 0;
        SDL_Texture* bTex = getButtonTexture(renderer, h.btnId, bw, bh);
        if (!bTex) bTex = getButtonTexture(renderer, h.key, bw, bh);

        if (bTex && bw > 0 && bh > 0) {
            int drawW = bw * targetH / bh;
            SDL_Rect dst = { curX, footerY + (footerH - targetH) / 2, drawW, targetH };
            SDL_RenderCopy(renderer, bTex, nullptr, &dst);
            curX += drawW + static_cast<int>(6 * scaleFactor_);
        }

        // Draw hint text
        std::string txt = h.label;
        size_t s = 0;
        SDL_Color c = menuConfig_.getOptionColor();
        c.a = 200;
        while (s < txt.size()) {
            uint32_t cp;
            if (!utf8Decode(txt, s, cp)) break;
            if (cp < 32) continue;
            Font::GlyphInfo glyph;
            if (optionFont_->getRect(cp, glyph)) {
                float gScale = (float)targetH / (float)optionFont_->getHeight();
                int gw = static_cast<int>(glyph.rect.w * gScale);
                int gh = static_cast<int>(glyph.rect.h * gScale);
                int adv = static_cast<int>(glyph.advance * gScale);
                SDL_Rect dst = { curX, footerY + (footerH - gh) / 2, gw, gh };
                SDL_SetTextureColorMod(optionFont_->getTexture(), c.r, c.g, c.b);
                SDL_RenderCopy(renderer, optionFont_->getTexture(), &glyph.rect, &dst);
                curX += adv;
            }
        }
        curX += static_cast<int>(18 * scaleFactor_);
    }
}

