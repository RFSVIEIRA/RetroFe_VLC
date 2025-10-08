#include "OptionsMenu.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include "../utility/Log.h"
#include "../Utility/Utils.h"
#include <numeric>
#include <filesystem>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

OptionsMenu::OptionsMenu(Configuration& config)
    : config_(config),
    menuConfig_(Utils::combinePath(Configuration::absolutePath, "layouts",
        [this]() {
            std::string layoutName;
            config_.getProperty("layout", layoutName);
            return layoutName.empty() ? "default" : layoutName;
        }(), "layoutMenu.xml")),
    settingsMenu_(Utils::combinePath(Configuration::absolutePath, "settings.conf"), config_),
    controlsMenu_(Utils::combinePath(Configuration::absolutePath, "controls.conf")),
    actionsMenu_(Utils::combinePath(Configuration::absolutePath, "omcontrols.conf")),
    layoutMenu_([this]() {
    std::string layoutName;
    config_.getProperty("layout", layoutName);
    if (layoutName.empty()) layoutName = "default";
    return Utils::combinePath(Configuration::absolutePath, "layouts", layoutName);
        }()),
    menuWindow_(nullptr),
    menuRenderer_(nullptr),
    titleFont_(nullptr),
    optionFont_(nullptr),
    selectedOption_(0),
    running_(false),
    settingsChanged_(false),
    currentSection_(nullptr),
    initializedSDL_(false)
{
    initMainOptions();
}

OptionsMenu::~OptionsMenu() {
    for (auto& img : images_) {
        if (img.isAnimated && img.frames) {
            for (int i = 0; i < img.frameCount; ++i) {
                if (img.frames[i]) SDL_DestroyTexture(img.frames[i]);
            }
            SDL_free(img.frames);
        }
        else if (img.texture) {
            SDL_DestroyTexture(img.texture);
        }
    }
    if (titleFont_) delete titleFont_;
    if (optionFont_) delete optionFont_;
    if (menuRenderer_) SDL_DestroyRenderer(menuRenderer_);
    if (menuWindow_) SDL_DestroyWindow(menuWindow_);
    if (initializedSDL_) {
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        Logger::write(Logger::ZONE_INFO, "OptionsMenu", "Shut down SDL, SDL_ttf, and SDL_image");
    }
}

bool OptionsMenu::run() {
    running_ = true;
    while (running_) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running_ = false;
            }
            else {
                handleInput(event);
            }
        }
        render();
        SDL_Delay(16); // ~60 FPS
    }
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    Logger::write(Logger::ZONE_INFO, "OptionsMenu", "Event queue flushed before exiting run()");
    return settingsChanged_;
}

bool OptionsMenu::show() {
    settingsChanged_ = false;
    // Ensure SDL video subsystem is initialized
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            Logger::write(Logger::ZONE_ERROR, "OptionsMenu", "Failed to initialize SDL video: " + std::string(SDL_GetError()));
            return false;
        }
        initializedSDL_ = true;
        Logger::write(Logger::ZONE_INFO, "OptionsMenu", "Initialized SDL with SDL_INIT_VIDEO");
    }
    // Initialize SDL_ttf
    if (TTF_Init() < 0) {
        Logger::write(Logger::ZONE_ERROR, "OptionsMenu", "Failed to initialize SDL_ttf: " + std::string(TTF_GetError()));
        return false;
    }
    Logger::write(Logger::ZONE_INFO, "OptionsMenu", "Initialized SDL_ttf");
    // Initialize SDL_image
    int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP;
    if (!(IMG_Init(imgFlags) & imgFlags)) {
        Logger::write(Logger::ZONE_ERROR, "OptionsMenu", "Failed to initialize SDL_image: " + std::string(IMG_GetError()));
        return false;
    }
    Logger::write(Logger::ZONE_INFO, "OptionsMenu", "Initialized SDL_image");
    // Initialize joystick subsystem
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0) {
        Logger::write(Logger::ZONE_ERROR, "OptionsMenu", "Failed to init SDL joystick: " + std::string(SDL_GetError()));
    }
    else {
        int numJoysticks = SDL_NumJoysticks();
        Logger::write(Logger::ZONE_INFO, "OptionsMenu", "Detected " + std::to_string(numJoysticks) + " joysticks");
        for (int i = 0; i < numJoysticks; ++i) {
            SDL_Joystick* joystick = SDL_JoystickOpen(i);
            if (joystick) {
                Logger::write(Logger::ZONE_INFO, "OptionsMenu", "Opened joystick " + std::to_string(i) + ": " + SDL_JoystickName(joystick));
            }
            else {
                Logger::write(Logger::ZONE_ERROR, "OptionsMenu", "Failed to open joystick " + std::to_string(i) + ": " + SDL_GetError());
            }
        }
    }
    // Set window size from menuConfig and store original dimensions
    originalWindowWidth_ = menuConfig_.getWindowWidth();
    originalWindowHeight_ = menuConfig_.getWindowHeight();
    windowWidth_ = originalWindowWidth_;
    windowHeight_ = originalWindowHeight_;
    Logger::write(Logger::ZONE_INFO, "OptionsMenu", "Initial window size from layoutMenu.xml: " +
        std::to_string(windowWidth_) + "x" + std::to_string(windowHeight_));
    // Get screenNum0 from settings.conf
    int displayIndex = 0; // Default to primary display
    std::string screenNumStr;
    if (config_.getProperty("screenNum0", screenNumStr)) {
        try {
            int screenNum = std::stoi(screenNumStr);
            int numDisplays = SDL_GetNumVideoDisplays();
            if (screenNum >= 0 && screenNum < numDisplays) {
                displayIndex = screenNum;
                Logger::write(Logger::ZONE_INFO, "OptionsMenu", "Using display index " + std::to_string(displayIndex) + " from screenNum0");
            }
            else {
                Logger::write(Logger::ZONE_WARNING, "OptionsMenu", "Invalid screenNum0 value (" + screenNumStr +
                    "); valid range is 0 to " + std::to_string(numDisplays - 1) + ", defaulting to 0");
            }
        }
        catch (const std::exception&) {
            Logger::write(Logger::ZONE_WARNING, "OptionsMenu", "Invalid screenNum0 value (" + screenNumStr +
                "); must be numeric, defaulting to 0");
        }
    }
    else {
        Logger::write(Logger::ZONE_INFO, "OptionsMenu", "screenNum0 not found in settings.conf, defaulting to display 0");
    }
    // Override with config_ only if specific numeric values are provided
    std::string horizontalStr, verticalStr;
    bool overrideConfig = false;
    if (config_.getProperty("horizontal0", horizontalStr) && config_.getProperty("vertical0", verticalStr)) {
        if (horizontalStr != "stretch" && verticalStr != "stretch") {
            try {
                int newWidth = std::stoi(horizontalStr);
                int newHeight = std::stoi(verticalStr);
                windowWidth_ = newWidth;
                windowHeight_ = newHeight;
                overrideConfig = true;
                Logger::write(Logger::ZONE_INFO, "OptionsMenu", "Overridden from config_: " +
                    std::to_string(windowWidth_) + "x" + std::to_string(windowHeight_));
            }
            catch (const std::exception&) {
                Logger::write(Logger::ZONE_WARNING, "OptionsMenu", "Invalid config_ resolution values (" +
                    horizontalStr + ", " + verticalStr + "); using layoutMenu.xml size");
            }
        }
    }
    if (!overrideConfig) {
        Logger::write(Logger::ZONE_INFO, "OptionsMenu", "Using window size from layoutMenu.xml: " +
            std::to_string(windowWidth_) + "x" + std::to_string(windowHeight_));
        // Apply scaling if scale="fit"
        std::string scale = menuConfig_.getWindowScale();
        if (scale == "fit") {
            SDL_Rect displayBounds;
            if (SDL_GetDisplayBounds(displayIndex, &displayBounds) == 0) {
                float configAspect = static_cast<float>(windowWidth_) / windowHeight_;
                float displayAspect = static_cast<float>(displayBounds.w) / displayBounds.h;
                if (configAspect > displayAspect) {
                    // Fit to display width, scale height
                    windowWidth_ = displayBounds.w;
                    windowHeight_ = static_cast<int>(displayBounds.w / configAspect);
                }
                else {
                    // Fit to display height, scale width
                    windowHeight_ = displayBounds.h;
                    windowWidth_ = static_cast<int>(displayBounds.h * configAspect);
                }
                Logger::write(Logger::ZONE_INFO, "OptionsMenu", "Scaled window to fit display " +
                    std::to_string(displayIndex) + ": " + std::to_string(windowWidth_) + "x" +
                    std::to_string(windowHeight_) + ", aspect ratio preserved");
            }
            else {
                Logger::write(Logger::ZONE_WARNING, "OptionsMenu", "Failed to get display bounds for index " +
                    std::to_string(displayIndex) + ": " + SDL_GetError() + ", using specified size");
            }
        }
        else {
            Logger::write(Logger::ZONE_INFO, "OptionsMenu", "No scaling applied (scale=" + (scale.empty() ? "none" : scale) + ")");
        }
    }
    // Compute scale factor after final window size is set
    scaleFactor_ = (originalWindowWidth_ > 0) ? static_cast<float>(windowWidth_) / originalWindowWidth_ : 1.0f;
    Logger::write(Logger::ZONE_INFO, "OptionsMenu", "Computed scale factor: " + std::to_string(scaleFactor_));
    // Create window on the specified display
    Uint32 windowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS;
    menuWindow_ = SDL_CreateWindow("RetroFE Options",
        SDL_WINDOWPOS_CENTERED_DISPLAY(displayIndex),
        SDL_WINDOWPOS_CENTERED_DISPLAY(displayIndex),
        windowWidth_, windowHeight_, windowFlags);
    if (!menuWindow_) {
        Logger::write(Logger::ZONE_ERROR, "OptionsMenu", "Window creation failed on display " +
            std::to_string(displayIndex) + ": " + std::string(SDL_GetError()));
        return false;
    }
    Logger::write(Logger::ZONE_INFO, "OptionsMenu", "Window created on display " + std::to_string(displayIndex) +
        " with size " + std::to_string(windowWidth_) + "x" + std::to_string(windowHeight_));
    // Set the window to always stay on top
    SDL_SetWindowAlwaysOnTop(menuWindow_, SDL_TRUE);
    Logger::write(Logger::ZONE_INFO, "OptionsMenu", "Set options menu window to always on top");
    menuRenderer_ = SDL_CreateRenderer(menuWindow_, -1, SDL_RENDERER_ACCELERATED);
    if (!menuRenderer_) {
        Logger::write(Logger::ZONE_ERROR, "OptionsMenu", "Renderer creation failed: " + std::string(SDL_GetError()));
        SDL_DestroyWindow(menuWindow_);
        menuWindow_ = nullptr;
        return false;
    }
    SDL_SetRenderDrawBlendMode(menuRenderer_, SDL_BLENDMODE_BLEND); // Enable blending
    Logger::write(Logger::ZONE_INFO, "OptionsMenu", "Renderer created with blending enabled");
    // Initialize fonts with scaled sizes
    int scaledTitleFontSize = static_cast<int>(menuConfig_.getTitleFontSize() * scaleFactor_);
    titleFont_ = new Font(menuConfig_.getFontPath(), scaledTitleFontSize, menuConfig_.getTitleColor(), 0);
    if (!titleFont_ || !titleFont_->initialize(menuRenderer_)) {
        delete titleFont_;
        titleFont_ = new Font("C:/Windows/Fonts/Arial.ttf", scaledTitleFontSize, menuConfig_.getTitleColor(), 0);
        if (!titleFont_ || !titleFont_->initialize(menuRenderer_)) {
            delete titleFont_;
            titleFont_ = nullptr;
        }
    }
    int scaledOptionFontSize = static_cast<int>(menuConfig_.getOptionFontSize() * scaleFactor_);
    optionFont_ = new Font(menuConfig_.getFontPath(), scaledOptionFontSize, menuConfig_.getOptionColor(), 0);
    if (!optionFont_ || !optionFont_->initialize(menuRenderer_)) {
        delete optionFont_;
        optionFont_ = new Font("C:/Windows/Fonts/Arial.ttf", scaledOptionFontSize, menuConfig_.getOptionColor(), 0);
        if (!optionFont_ || !optionFont_->initialize(menuRenderer_)) {
            delete optionFont_;
            optionFont_ = nullptr;
        }
    }
    // Load images
    images_.clear();
    std::string layoutName;
    config_.getProperty("layout", layoutName);
    if (layoutName.empty()) {
        Logger::write(Logger::ZONE_ERROR, "OptionsMenu", "No layout specified in configuration");
        return false;
    }
    std::string layoutBasePath = Utils::combinePath(Configuration::absolutePath, "layouts", layoutName);
    Logger::write(Logger::ZONE_DEBUG, "OptionsMenu", "Layout base path: " + layoutBasePath);
    for (const auto& config : menuConfig_.getImages()) {
        ImageInstance img;
        img.isAnimated = config.isAnimated;
        img.dest = { config.x, config.y, config.width, config.height }; // Backward compatibility
        img.scale = config.scale;
        img.xAlign = config.xAlign; // Copy alignment fields
        img.yAlign = config.yAlign;
        img.xOffset = config.xOffset;
        img.yOffset = config.yOffset;
        img.frameCount = 0;
        img.currentFrame = 0;
        img.lastFrameTime = SDL_GetTicks();
        img.frames = nullptr;
        img.texture = nullptr;
        std::string fullPath = Utils::combinePath(layoutBasePath, config.src);
        Logger::write(Logger::ZONE_DEBUG, "OptionsMenu", "Resolved image path: " + fullPath);
        if (img.isAnimated) {
            IMG_Animation* anim = IMG_LoadAnimation(fullPath.c_str());
            if (anim) {
                img.frameCount = anim->count;
                img.frames = static_cast<SDL_Texture**>(SDL_calloc(img.frameCount, sizeof(SDL_Texture*)));
                img.frameDelays.resize(img.frameCount); // Initialize frameDelays vector
                if (img.frames) {
                    for (int i = 0; i < img.frameCount; ++i) {
                        img.frames[i] = SDL_CreateTextureFromSurface(menuRenderer_, anim->frames[i]);
                        if (!img.frames[i]) {
                            Logger::write(Logger::ZONE_ERROR, "OptionsMenu",
                                "Failed to create texture for frame " + std::to_string(i) + ": " + SDL_GetError());
                        }
                        else {
                            SDL_SetTextureBlendMode(img.frames[i], SDL_BLENDMODE_BLEND);
                        }
                        // Store frame delay, ensure minimum 10ms if zero or invalid
                        img.frameDelays[i] = (anim->delays[i] > 0) ? anim->delays[i] : 10;
                    }
                    IMG_FreeAnimation(anim);
                    images_.push_back(img);
                    Logger::write(Logger::ZONE_INFO, "OptionsMenu",
                        "Loaded animated image: " + fullPath + " with " + std::to_string(img.frameCount) + " frames, xAlign=" +
                        (img.xAlign == OptionsMenuBuilder::XAlignment::Center ? "center" :
                            img.xAlign == OptionsMenuBuilder::XAlignment::Left ? "left" :
                            img.xAlign == OptionsMenuBuilder::XAlignment::Right ? "right" : "numeric") +
                        ", yAlign=" + (img.yAlign == OptionsMenuBuilder::YAlignment::Center ? "center" :
                            img.yAlign == OptionsMenuBuilder::YAlignment::Top ? "top" :
                            img.yAlign == OptionsMenuBuilder::YAlignment::Bottom ? "bottom" : "numeric"));
                }
                else {
                    Logger::write(Logger::ZONE_ERROR, "OptionsMenu", "Failed to allocate memory for frames: " + fullPath);
                    IMG_FreeAnimation(anim);
                }
            }
            else {
                Logger::write(Logger::ZONE_WARNING, "OptionsMenu", "Failed to load animated image: " + fullPath + " - " + IMG_GetError());
            }
        }
        else {
            SDL_Surface* surface = IMG_Load(fullPath.c_str());
            if (surface) {
                img.frameCount = 1;
                img.texture = SDL_CreateTextureFromSurface(menuRenderer_, surface);
                SDL_FreeSurface(surface);
                if (img.texture) {
                    SDL_SetTextureBlendMode(img.texture, SDL_BLENDMODE_BLEND);
                    images_.push_back(img);
                    Logger::write(Logger::ZONE_INFO, "OptionsMenu",
                        "Loaded static image: " + fullPath + ", xAlign=" +
                        (img.xAlign == OptionsMenuBuilder::XAlignment::Center ? "center" :
                            img.xAlign == OptionsMenuBuilder::XAlignment::Left ? "left" :
                            img.xAlign == OptionsMenuBuilder::XAlignment::Right ? "right" : "numeric") +
                        ", yAlign=" + (img.yAlign == OptionsMenuBuilder::YAlignment::Center ? "center" :
                            img.yAlign == OptionsMenuBuilder::YAlignment::Top ? "top" :
                            img.yAlign == OptionsMenuBuilder::YAlignment::Bottom ? "bottom" : "numeric"));
                }
                else {
                    Logger::write(Logger::ZONE_ERROR, "OptionsMenu", "Failed to create texture from surface: " + std::string(SDL_GetError()));
                }
            }
            else {
                Logger::write(Logger::ZONE_WARNING, "OptionsMenu", "Failed to load static image: " + fullPath + " - " + IMG_GetError());
            }
        }
    }
    SDL_ShowCursor(SDL_ENABLE);
    bool result = run();
    SDL_ShowCursor(SDL_DISABLE);
    // Clean up joystick
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        SDL_Joystick* joystick = SDL_JoystickFromInstanceID(i);
        if (joystick && SDL_JoystickGetAttached(joystick)) {
            SDL_JoystickClose(joystick);
            Logger::write(Logger::ZONE_INFO, "OptionsMenu", "Closed joystick " + std::to_string(i));
        }
    }
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    // Clean up resources
    for (auto& img : images_) {
        if (img.isAnimated && img.frames) {
            for (int i = 0; i < img.frameCount; ++i) {
                if (img.frames[i]) SDL_DestroyTexture(img.frames[i]);
            }
            SDL_free(img.frames);
        }
        else if (img.texture) {
            SDL_DestroyTexture(img.texture);
        }
    }
    images_.clear();
    if (titleFont_) delete titleFont_;
    if (optionFont_) delete optionFont_;
    if (menuRenderer_) SDL_DestroyRenderer(menuRenderer_);
    if (menuWindow_) SDL_DestroyWindow(menuWindow_);
    titleFont_ = nullptr;
    optionFont_ = nullptr;
    menuRenderer_ = nullptr;
    menuWindow_ = nullptr;
    if (initializedSDL_) {
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        Logger::write(Logger::ZONE_INFO, "OptionsMenu", "Shut down SDL, SDL_ttf, and SDL_image");
    }
    return result;
}

void OptionsMenu::initMainOptions() {
    mainOptions_ = {
        menuConfig_.getSettingsTitleText(),
        menuConfig_.getControlsTitleText(),
        menuConfig_.getLayoutTitleText(),
        menuConfig_.getActionsTitleText(),
        menuConfig_.getSaveExitText()
    };
}

void OptionsMenu::handleInput(SDL_Event& event) {
    // NEW: Handle window events to detect and restore focus if lost (e.g., due to clicking outside)
    if (event.type == SDL_WINDOWEVENT && event.window.windowID == SDL_GetWindowID(menuWindow_)) {
        if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
            // Force focus back to the options menu window
            SDL_RaiseWindow(menuWindow_);
            SDL_SetWindowInputFocus(menuWindow_);
            Logger::write(Logger::ZONE_INFO, "OptionsMenu", "Focus lost detected; restoring focus to options menu window");
        }
        // You can handle other window events if needed, but focus lost is the key one
    }
    
    if (currentSection_ != nullptr) {
        MenuAction action = currentSection_->handleInput(event);
        if (action == MenuAction::BACK) {
            currentSection_ = nullptr;
        }
    }
    else {
        if (event.type == SDL_KEYDOWN || event.type == SDL_JOYHATMOTION || event.type == SDL_JOYBUTTONDOWN) {
            bool menuUp = (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_UP) ||
                (event.type == SDL_JOYHATMOTION && event.jhat.value == SDL_HAT_UP);
            bool menuDown = (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_DOWN) ||
                (event.type == SDL_JOYHATMOTION && event.jhat.value == SDL_HAT_DOWN);
            bool menuSelect = (event.type == SDL_KEYDOWN && (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER)) ||
                (event.type == SDL_JOYBUTTONDOWN && event.jbutton.button == 0);
            bool menuBack = (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) ||
                (event.type == SDL_JOYBUTTONDOWN && event.jbutton.button == 1);

            if (menuUp) {
                // Wrap around: if at first option, go to last
                selectedOption_ = (selectedOption_ == 0) ? mainOptions_.size() - 1 : selectedOption_ - 1;
            }
            else if (menuDown) {
                // Wrap around: if at last option, go to first
                selectedOption_ = (selectedOption_ == mainOptions_.size() - 1) ? 0 : selectedOption_ + 1;
            }
            else if (menuSelect) {
                if (selectedOption_ == 0) currentSection_ = &settingsMenu_;
                else if (selectedOption_ == 1) currentSection_ = &controlsMenu_;
                else if (selectedOption_ == 2) currentSection_ = &layoutMenu_;
                else if (selectedOption_ == 3) currentSection_ = &actionsMenu_;
                else if (selectedOption_ == 4) {
                    if (settingsMenu_.hasChanges() || controlsMenu_.hasChanges() || actionsMenu_.hasChanges()) {
                        settingsChanged_ = true;
                        if (settingsMenu_.hasChanges()) settingsMenu_.save();
                        if (controlsMenu_.hasChanges()) controlsMenu_.save();
                        if (actionsMenu_.hasChanges()) actionsMenu_.save();
                    }
                    if (layoutMenu_.hasChanges()) layoutMenu_.save();
                    running_ = false;
                }
                selectedOption_ = 0;
            }
            else if (menuBack) {
                running_ = false;
            }
        }
    }
}

void OptionsMenu::render() {
    SDL_SetRenderDrawColor(menuRenderer_, menuConfig_.getBackgroundColor().r,
        menuConfig_.getBackgroundColor().g, menuConfig_.getBackgroundColor().b,
        menuConfig_.getBackgroundColor().a);
    SDL_RenderClear(menuRenderer_);

    if (!titleFont_ || !optionFont_ || !menuRenderer_) {
        Logger::write(Logger::ZONE_ERROR, "OptionsMenu", "Missing fonts or renderer");
        SDL_RenderPresent(menuRenderer_);
        return;
    }

    int actualWindowWidth, actualWindowHeight;
    SDL_GetWindowSize(menuWindow_, &actualWindowWidth, &actualWindowHeight);
    if (actualWindowWidth != windowWidth_ || actualWindowHeight != windowHeight_) {
        Logger::write(Logger::ZONE_WARNING, "OptionsMenu", "Window size mismatch: Config=" +
            std::to_string(windowWidth_) + "x" + std::to_string(windowHeight_) +
            ", Actual=" + std::to_string(actualWindowWidth) + "x" + std::to_string(actualWindowHeight));
        windowWidth_ = actualWindowWidth;
        windowHeight_ = actualWindowHeight;
        // Update scale factor on resize
        scaleFactor_ = (originalWindowWidth_ > 0) ? static_cast<float>(windowWidth_) / originalWindowWidth_ : 1.0f;
        Logger::write(Logger::ZONE_INFO, "OptionsMenu", "Updated scale factor on resize: " + std::to_string(scaleFactor_));
        // Recreate fonts with new scale factor
        if (titleFont_) delete titleFont_;
        if (optionFont_) delete optionFont_;
        int scaledTitleFontSize = static_cast<int>(menuConfig_.getTitleFontSize() * scaleFactor_);
        titleFont_ = new Font(menuConfig_.getFontPath(), scaledTitleFontSize, menuConfig_.getTitleColor(), 0);
        if (!titleFont_ || !titleFont_->initialize(menuRenderer_)) {
            delete titleFont_;
            titleFont_ = new Font("C:/Windows/Fonts/Arial.ttf", scaledTitleFontSize, menuConfig_.getTitleColor(), 0);
            if (!titleFont_ || !titleFont_->initialize(menuRenderer_)) {
                delete titleFont_;
                titleFont_ = nullptr;
            }
        }
        int scaledOptionFontSize = static_cast<int>(menuConfig_.getOptionFontSize() * scaleFactor_);
        optionFont_ = new Font(menuConfig_.getFontPath(), scaledOptionFontSize, menuConfig_.getOptionColor(), 0);
        if (!optionFont_ || !optionFont_->initialize(menuRenderer_)) {
            delete optionFont_;
            optionFont_ = new Font("C:/Windows/Fonts/Arial.ttf", scaledOptionFontSize, menuConfig_.getOptionColor(), 0);
            if (!optionFont_ || !optionFont_->initialize(menuRenderer_)) {
                delete optionFont_;
                optionFont_ = nullptr;
            }
        }
        Logger::write(Logger::ZONE_INFO, "OptionsMenu", "Recreated fonts on window resize with title size: " + std::to_string(scaledTitleFontSize) + ", option size: " + std::to_string(scaledOptionFontSize));
    }

    // Calculate scaling factors based on original window dimensions
    float scaleX = (originalWindowWidth_ > 0) ? static_cast<float>(windowWidth_) / originalWindowWidth_ : 1.0f;
    float scaleY = (originalWindowHeight_ > 0) ? static_cast<float>(windowHeight_) / originalWindowHeight_ : 1.0f;
    // Assuming aspect-preserving scale, use scaleFactor for consistency
    float scaleFactor = scaleFactor_;  // Use member

    int winX, winY;
    SDL_GetWindowPosition(menuWindow_, &winX, &winY);
    Logger::write(Logger::ZONE_INFO, "OptionsMenu", "Window position: x=" + std::to_string(winX) +
        ", y=" + std::to_string(winY) + ", size=" + std::to_string(windowWidth_) + "x" + std::to_string(windowHeight_));

    for (auto& img : images_) {
        if (img.isMenu) {
            Logger::write(Logger::ZONE_DEBUG, "OptionsMenu", "Skipping image with isMenu=true");
            continue;
        }
        SDL_Texture* texture = img.isAnimated ? img.frames[img.currentFrame] : img.texture;
        if (!texture) continue;

        if (img.isAnimated && img.frameCount > 1) {
            Uint32 currentTime = SDL_GetTicks();
            if (currentTime - img.lastFrameTime >= img.frameDelays[img.currentFrame]) {
                img.currentFrame = (img.currentFrame + 1) % img.frameCount;
                img.lastFrameTime = currentTime;
            }
        }

        int texW, texH;
        SDL_QueryTexture(texture, nullptr, nullptr, &texW, &texH);

        SDL_Rect srcRect = { 0, 0, texW, texH };
        int scaledW = static_cast<int>(img.dest.w * scaleFactor);
        int scaledH = static_cast<int>(img.dest.h * scaleFactor);
        SDL_Rect destRect = { 0, 0, scaledW, scaledH };

        int scaledXOffset = static_cast<int>(img.xOffset * scaleFactor);
        int scaledYOffset = static_cast<int>(img.yOffset * scaleFactor);

        switch (img.xAlign) {
        case OptionsMenuBuilder::XAlignment::Center:
            destRect.x = (windowWidth_ - scaledW) / 2 + scaledXOffset;
            break;
        case OptionsMenuBuilder::XAlignment::Left:
            destRect.x = scaledXOffset;
            break;
        case OptionsMenuBuilder::XAlignment::Right:
            destRect.x = windowWidth_ - scaledW + scaledXOffset;
            break;
        case OptionsMenuBuilder::XAlignment::Numeric:
            destRect.x = static_cast<int>(img.dest.x * scaleFactor);
            break;
        default:
            destRect.x = (windowWidth_ - scaledW) / 2;
            Logger::write(Logger::ZONE_WARNING, "OptionsMenu", "Invalid image x alignment, using center");
        }

        switch (img.yAlign) {
        case OptionsMenuBuilder::YAlignment::Center:
            destRect.y = (windowHeight_ - scaledH) / 2 + scaledYOffset;
            break;
        case OptionsMenuBuilder::YAlignment::Top:
            destRect.y = scaledYOffset;
            break;
        case OptionsMenuBuilder::YAlignment::Bottom:
            destRect.y = windowHeight_ - scaledH + scaledYOffset;
            break;
        case OptionsMenuBuilder::YAlignment::Numeric:
            destRect.y = static_cast<int>(img.dest.y * scaleFactor);
            break;
        default:
            destRect.y = scaledYOffset;
            Logger::write(Logger::ZONE_WARNING, "OptionsMenu", "Invalid image y alignment, using numeric");
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
        else if (img.scale == "fill") {
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
        Logger::write(Logger::ZONE_INFO, "OptionsMenu", "Rendering image: x=" + std::to_string(destRect.x) +
            ", y=" + std::to_string(destRect.y) + ", w=" + std::to_string(destRect.w) +
            ", h=" + std::to_string(destRect.h) + ", xAlign=" +
            (img.xAlign == OptionsMenuBuilder::XAlignment::Center ? "center" :
                img.xAlign == OptionsMenuBuilder::XAlignment::Left ? "left" :
                img.xAlign == OptionsMenuBuilder::XAlignment::Right ? "right" : "numeric") +
            ", yAlign=" + (img.yAlign == OptionsMenuBuilder::YAlignment::Center ? "center" :
                img.yAlign == OptionsMenuBuilder::YAlignment::Top ? "top" :
                img.yAlign == OptionsMenuBuilder::YAlignment::Bottom ? "bottom" : "numeric"));
#endif
        SDL_RenderCopy(menuRenderer_, texture, img.scale == "fill" ? &srcRect : nullptr, &destRect);
    }

    if (currentSection_ != nullptr) {
        currentSection_->render(menuRenderer_, titleFont_, optionFont_, windowWidth_, windowHeight_, menuConfig_, scaleFactor_);
    }
    else {
        std::string title = menuConfig_.getMainTitleText();
        float titleWidth = 0.0f;
        for (char c : title) {
            if (c < 32 || c > 127) continue;
            Font::GlyphInfo glyph;
            if (titleFont_->getRect(static_cast<unsigned char>(c), glyph)) {
                titleWidth += glyph.advance;
            }
        }

        int scaledTitleXOffset = static_cast<int>(menuConfig_.getTitleXOffset() * scaleFactor);
        int titleX;
        switch (menuConfig_.getTitleXAlignment()) {
        case OptionsMenuBuilder::XAlignment::Center:
            titleX = (windowWidth_ - static_cast<int>(titleWidth)) / 2 + scaledTitleXOffset;
            break;
        case OptionsMenuBuilder::XAlignment::Left:
            titleX = scaledTitleXOffset;
            break;
        case OptionsMenuBuilder::XAlignment::Right:
            titleX = windowWidth_ - static_cast<int>(titleWidth) + scaledTitleXOffset;
            break;
        case OptionsMenuBuilder::XAlignment::Numeric:
            titleX = scaledTitleXOffset;
            break;
        default:
            titleX = (windowWidth_ - static_cast<int>(titleWidth)) / 2;
            Logger::write(Logger::ZONE_WARNING, "OptionsMenu", "Invalid title x alignment, using center");
        }

        int scaledTitleYOffset = static_cast<int>(menuConfig_.getTitleYOffset() * scaleFactor);
        int titleY;
        switch (menuConfig_.getTitleYAlignment()) {
        case OptionsMenuBuilder::YAlignment::Center:
            titleY = (windowHeight_ - titleFont_->getHeight()) / 2 + scaledTitleYOffset;
            break;
        case OptionsMenuBuilder::YAlignment::Top:
            titleY = scaledTitleYOffset;
            break;
        case OptionsMenuBuilder::YAlignment::Bottom:
            titleY = windowHeight_ - titleFont_->getHeight() + scaledTitleYOffset;
            break;
        case OptionsMenuBuilder::YAlignment::Numeric:
            titleY = scaledTitleYOffset;
            break;
        default:
            titleY = scaledTitleYOffset;
            Logger::write(Logger::ZONE_WARNING, "OptionsMenu", "Invalid title y alignment, using numeric offset");
        }

        Logger::write(Logger::ZONE_INFO, "OptionsMenu", "Title alignment: xAlign=" +
            std::string(menuConfig_.getTitleXAlignment() == OptionsMenuBuilder::XAlignment::Center ? "center" :
                menuConfig_.getTitleXAlignment() == OptionsMenuBuilder::XAlignment::Left ? "left" :
                menuConfig_.getTitleXAlignment() == OptionsMenuBuilder::XAlignment::Right ? "right" : "numeric") +
            ", xOffset=" + std::to_string(menuConfig_.getTitleXOffset()) +
            ", yAlign=" + std::string(menuConfig_.getTitleYAlignment() == OptionsMenuBuilder::YAlignment::Center ? "center" :
                menuConfig_.getTitleYAlignment() == OptionsMenuBuilder::YAlignment::Top ? "top" :
                menuConfig_.getTitleYAlignment() == OptionsMenuBuilder::YAlignment::Bottom ? "bottom" : "numeric") +
            ", yOffset=" + std::to_string(menuConfig_.getTitleYOffset()));
        Logger::write(Logger::ZONE_INFO, "OptionsMenu", "Title position: x=" + std::to_string(titleX) +
            ", y=" + std::to_string(titleY) + ", titleWidth=" + std::to_string(titleWidth));

        float renderX = static_cast<float>(titleX);
        for (char c : title) {
            if (c < 32 || c > 127) continue;
            Font::GlyphInfo glyph;
            if (titleFont_->getRect(static_cast<unsigned char>(c), glyph)) {
                SDL_Rect destRect = { static_cast<int>(renderX), titleY, glyph.rect.w, glyph.rect.h };
                SDL_SetTextureColorMod(titleFont_->getTexture(), menuConfig_.getTitleColor().r,
                    menuConfig_.getTitleColor().g, menuConfig_.getTitleColor().b);
                SDL_RenderCopy(menuRenderer_, titleFont_->getTexture(), &glyph.rect, &destRect);
                renderX += glyph.advance;
            }
        }

        int scaledMenuXOffset = static_cast<int>(menuConfig_.getMenuXOffset() * scaleFactor);
        int scaledMenuYOffset = static_cast<int>(menuConfig_.getMenuYOffset() * scaleFactor);
        int scaledTitleSpacing = static_cast<int>(menuConfig_.getTitleSpacing() * scaleFactor);
        int menuY;
        switch (menuConfig_.getMenuYAlignment()) {
        case OptionsMenuBuilder::YAlignment::Center:
            menuY = windowHeight_ / 2 + scaledMenuYOffset;
            break;
        case OptionsMenuBuilder::YAlignment::Top:
            menuY = scaledMenuYOffset;
            break;
        case OptionsMenuBuilder::YAlignment::Bottom:
            menuY = windowHeight_ - (mainOptions_.size() * menuConfig_.getSpaceBetweenText()) + scaledMenuYOffset;
            break;
        case OptionsMenuBuilder::YAlignment::Numeric:
            menuY = scaledMenuYOffset;
            menuY = std::max(menuY, titleY + titleFont_->getHeight() + scaledTitleSpacing);
            break;
        default:
            menuY = titleY + titleFont_->getHeight() + scaledTitleSpacing;
            Logger::write(Logger::ZONE_WARNING, "OptionsMenu", "Invalid menu y alignment, using titleY + fontHeight + titleSpacing");
        }

        int scaledSpaceBetween = static_cast<int>(menuConfig_.getSpaceBetweenText() * scaleFactor);
        int scaledPadding = static_cast<int>(menuConfig_.getSelectionBarPadding() * scaleFactor);
        int scaledBarHeight = static_cast<int>(menuConfig_.getSelectionBarHeight() * scaleFactor);
        int scaledBarYOffset = static_cast<int>(menuConfig_.getSelectionBarYOffset() * scaleFactor);

        optionRects_.clear();
        for (size_t i = 0; i < mainOptions_.size(); ++i) {
            std::string text = mainOptions_[i];
            bool isSelected = (i == selectedOption_);
            SDL_Color lineColor = isSelected ? menuConfig_.getTitleColor() : menuConfig_.getOptionColor();

            float textWidth = 0.0f;
            for (char c : text) {
                if (c < 32 || c > 127) continue;
                Font::GlyphInfo glyph;
                if (optionFont_->getRect(static_cast<unsigned char>(c), glyph)) {
                    textWidth += glyph.advance;
                }
            }

            int optionX;
            switch (menuConfig_.getMenuXAlignment()) {
            case OptionsMenuBuilder::XAlignment::Center:
                optionX = (windowWidth_ - static_cast<int>(textWidth)) / 2 + scaledMenuXOffset;
                break;
            case OptionsMenuBuilder::XAlignment::Left:
                optionX = scaledMenuXOffset;
                break;
            case OptionsMenuBuilder::XAlignment::Right:
                optionX = windowWidth_ - static_cast<int>(textWidth) + scaledMenuXOffset;
                break;
            case OptionsMenuBuilder::XAlignment::Numeric:
                optionX = scaledMenuXOffset;
                break;
            default:
                optionX = (windowWidth_ - static_cast<int>(textWidth)) / 2;
                Logger::write(Logger::ZONE_WARNING, "OptionsMenu", "Invalid menu x alignment, using center");
            }

            float y = menuY + static_cast<float>(i) * scaledSpaceBetween;

            if (isSelected) {
                SDL_Rect barRect = {
                    static_cast<int>(optionX - scaledPadding),
                    static_cast<int>(y + scaledBarYOffset),
                    static_cast<int>(textWidth + 2 * scaledPadding),
                    scaledBarHeight
                };
                SDL_Color barColor = menuConfig_.getSelectionBarColor();
                SDL_SetRenderDrawColor(menuRenderer_, barColor.r, barColor.g, barColor.b, barColor.a);
                Logger::write(Logger::ZONE_DEBUG, "OptionsMenu", "Rendering selection bar: R=" +
                    std::to_string(barColor.r) + ", G=" + std::to_string(barColor.g) + ", B=" +
                    std::to_string(barColor.b) + ", A=" + std::to_string(barColor.a));
                SDL_RenderFillRect(menuRenderer_, &barRect);
            }

            SDL_Rect optionRect = {
                static_cast<int>(optionX),
                static_cast<int>(y),
                static_cast<int>(textWidth),
                optionFont_->getHeight()
            };
            optionRects_.push_back(optionRect);

            renderX = static_cast<float>(optionX);
            for (char c : text) {
                if (c < 32 || c > 127) continue;
                Font::GlyphInfo glyph;
                if (optionFont_->getRect(static_cast<unsigned char>(c), glyph)) {
                    SDL_Rect destRect = { static_cast<int>(renderX), static_cast<int>(y), glyph.rect.w, glyph.rect.h };
                    SDL_SetTextureColorMod(optionFont_->getTexture(), lineColor.r, lineColor.g, lineColor.b);
                    SDL_RenderCopy(menuRenderer_, optionFont_->getTexture(), &glyph.rect, &destRect);
                    renderX += glyph.advance;
                }
            }
        }

        Logger::write(Logger::ZONE_INFO, "OptionsMenu", "Options alignment: xAlign=" +
            std::string(menuConfig_.getMenuXAlignment() == OptionsMenuBuilder::XAlignment::Center ? "center" :
                menuConfig_.getMenuXAlignment() == OptionsMenuBuilder::XAlignment::Left ? "left" :
                menuConfig_.getMenuXAlignment() == OptionsMenuBuilder::XAlignment::Right ? "right" : "numeric") +
            ", xOffset=" + std::to_string(menuConfig_.getMenuXOffset()) +
            ", yAlign=" + std::string(menuConfig_.getMenuYAlignment() == OptionsMenuBuilder::YAlignment::Center ? "center" :
                menuConfig_.getMenuYAlignment() == OptionsMenuBuilder::YAlignment::Top ? "top" :
                menuConfig_.getMenuYAlignment() == OptionsMenuBuilder::YAlignment::Bottom ? "bottom" : "numeric") +
            ", yOffset=" + std::to_string(menuConfig_.getMenuYOffset()));
        Logger::write(Logger::ZONE_INFO, "OptionsMenu", "Options position: y=" + std::to_string(menuY));
    }
    SDL_RenderPresent(menuRenderer_);
}