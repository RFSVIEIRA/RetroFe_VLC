#ifndef OPTIONS_MENU_BUILDER_H
#define OPTIONS_MENU_BUILDER_H

#include <string>
#include <vector>
#include <SDL.h>

class OptionsMenuBuilder {
public:
    enum class XAlignment { Center, Left, Right, Numeric };
    enum class YAlignment { Center, Top, Bottom, Numeric };

    struct ImageConfig {
        std::string src;
        bool isAnimated = false;
        bool isMenu = false;
        int x = 0, y = 0, width = 100, height = 100;
        XAlignment xAlign = XAlignment::Numeric;
        YAlignment yAlign = YAlignment::Numeric;
        int xOffset = 0, yOffset = 0;
        std::string scale = "stretch";
    };

    OptionsMenuBuilder(const std::string& xmlPath);

    bool loadConfig(const std::string& xmlPath);
    static SDL_Color parseColor(const std::string& hex);
    void parseAlignment(const std::string& value, XAlignment& xAlign, int& xOffset, YAlignment& yAlign, int& yOffset);

    // Getters
    const std::vector<ImageConfig>& getImages() const { return images_; }
    SDL_Color getBackgroundColor() const { return backgroundColor_; }
    std::string getFontPath() const { return fontPath_; }
    int getTitleFontSize() const { return titleFontSize_; }
    SDL_Color getTitleColor() const { return titleColor_; }
    XAlignment getTitleXAlignment() const { return titleXAlignment_; }
    YAlignment getTitleYAlignment() const { return titleYAlignment_; }
    int getTitleXOffset() const { return titleXOffset_; }
    int getTitleYOffset() const { return titleYOffset_; }
    int getTitleX() const { return titleX_; }
    int getTitleY() const { return titleY_; }
    int getOptionFontSize() const { return optionFontSize_; }
    SDL_Color getOptionColor() const { return optionColor_; }
    SDL_Color getSelectionBarColor() const { return selectionBarColor_; }
    SDL_Color getTitleBarColor() const { return titleBarColor_; }
    SDL_Color getSubtitleUnderlineColor() const { return subtitleUnderlineColor_; }
    int getSpaceBetweenText() const { return spaceBetweenText_; }
    XAlignment getMenuXAlignment() const { return menuXAlignment_; }
    YAlignment getMenuYAlignment() const { return menuYAlignment_; }
    int getMenuXOffset() const { return menuXOffset_; }
    int getMenuYOffset() const { return menuYOffset_; }
    int getMenuX() const { return menuX_; }
    int getMenuY() const { return menuY_; }
    int getWindowWidth() const { return windowWidth_; }
    int getWindowHeight() const { return windowHeight_; }
    std::string getWindowScale() const { return scale_; }
    std::string getMainTitleText() const { return mainTitleText_; }
    std::string getSettingsTitleText() const { return settingsTitleText_; }
    std::string getControlsTitleText() const { return controlsTitleText_; }
    std::string getActionsTitleText() const { return actionsTitleText_; }
    std::string getLayoutTitleText() const { return layoutTitleText_; }
    std::string getSaveExitText() const { return saveExitText_; }
    SDL_Color getFontColor() const { return fontColor_; } 
    const std::string& getDefaultMenuSrc() const { return defaultMenuSrc_; }
    int getSelectionBarPadding() const { return selectionBarPadding_; } 
    int getSelectionBarHeight() const { return selectionBarHeight_; }   
    int getTitleSpacing() const { return titleSpacing_; }
    int getSelectionBarYOffset() const { return selectionBarYOffset_; }

private:
    std::vector<ImageConfig> images_;
    SDL_Color backgroundColor_ = { 0, 0, 0, 255 };
    std::string fontPath_ = "C:/Windows/Fonts/Arial.ttf";
    int titleFontSize_ = 34;
    SDL_Color titleColor_ = { 255, 255, 255, 255 };
    XAlignment titleXAlignment_ = XAlignment::Center;
    YAlignment titleYAlignment_ = YAlignment::Top;
    int titleXOffset_ = 0;
    int titleYOffset_ = 50;
    int titleX_ = 0;
    int titleY_ = 50;
    int optionFontSize_ = 24;
    SDL_Color optionColor_ = { 255, 255, 255, 255 };
    SDL_Color selectionBarColor_ = { 50, 50, 50, 255 };
    SDL_Color titleBarColor_ = { 30, 144, 255, 255 };
    SDL_Color subtitleUnderlineColor_ = { 255, 215, 0, 255 };
    int spaceBetweenText_ = 40;
    XAlignment menuXAlignment_ = XAlignment::Center;
    YAlignment menuYAlignment_ = YAlignment::Top;
    int menuXOffset_ = 0;
    int menuYOffset_ = 150;
    int menuX_ = 0;
    int menuY_ = 150;
    int windowWidth_ = 3440;
    int windowHeight_ = 1440;
    std::string scale_; 
    std::string mainTitleText_ = "Options Setup";
    std::string settingsTitleText_ = "Settings";
    std::string controlsTitleText_ = "Control";
    std::string layoutTitleText_ = "Layout";
    std::string actionsTitleText_ = "Info Keys";
    std::string saveExitText_ = "Save & Exit";
    SDL_Color fontColor_ = { 255, 255, 255, 255 }; 
    std::string defaultMenuSrc_;
    bool selectionBarColorParsed_ = false;
    int selectionBarPadding_ = 10; 
    int selectionBarHeight_ = 40;
    int titleSpacing_ = 20; 
    int selectionBarYOffset_ = -5; 
};

#endif 