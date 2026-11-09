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
#ifndef OPTIONS_MENU_BUILDER_H
#define OPTIONS_MENU_BUILDER_H

#include <string>
#include <vector>
#include <SDL.h>
#include <set>
#include <map>
#include <libxml/tree.h>

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
        float xOrigin = 0.0f, yOrigin = 0.0f;
        bool hasExplicitXOrigin = false;
        bool hasExplicitYOrigin = false;
        float alpha = 1.0f;
        int layer = 0;
    };

    OptionsMenuBuilder(const std::string& xmlPath = "");

    void applyDefaultConfig();
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
    float getTitleXOrigin() const {
        if (hasExplicitTitleXOrigin_) return titleXOrigin_;
        if (titleXAlignment_ == XAlignment::Center) return 0.5f;
        if (titleXAlignment_ == XAlignment::Right) return 1.0f;
        return 0.0f;
    }
    float getTitleYOrigin() const {
        if (hasExplicitTitleYOrigin_) return titleYOrigin_;
        if (titleYAlignment_ == YAlignment::Center) return 0.5f;
        if (titleYAlignment_ == YAlignment::Bottom) return 1.0f;
        return 0.0f;
    }
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
    float getMenuXOrigin() const {
        if (hasExplicitMenuXOrigin_) return menuXOrigin_;
        if (menuXAlignment_ == XAlignment::Center) return 0.5f;
        if (menuXAlignment_ == XAlignment::Right) return 1.0f;
        return 0.0f;
    }
    float getMenuYOrigin() const {
        if (hasExplicitMenuYOrigin_) return menuYOrigin_;
        if (menuYAlignment_ == YAlignment::Center) return 0.5f;
        if (menuYAlignment_ == YAlignment::Bottom) return 1.0f;
        return 0.0f;
    }
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
    std::string getQuitText() const { return quitText_; }
  //  std::string getKioskModeText() const { return kioskModeText_; }
    std::string getSignOutText() const { return signOutText_; }
    std::string getRestartText() const { return restartText_; }
    std::string getShutDownText() const { return shutDownText_; }
    SDL_Color getFontColor() const { return fontColor_; }
    SDL_Color getSelectedColor() const { return selectedColor_; }
    SDL_Color getSeparatorColor() const { return separatorColor_; }
    int getSeparatorHeight() const { return separatorHeight_; }
    int getSeparatorWidth() const { return separatorWidth_; }
    int getSeparatorYOffset() const { return separatorYOffset_; }
    XAlignment getSeparatorXAlignment() const { return separatorXAlignment_; }
    int getSeparatorX() const { return separatorX_; }
    int getSeparatorXOffset() const { return separatorXOffset_; }
    bool isSeparatorAlignText() const { return separatorAlignText_; }
    void setFontPath(const std::string& path) { if (!path.empty()) fontPath_ = path; }
    const std::string& getDefaultMenuSrc() const { return defaultMenuSrc_; }
    int getSelectionBarPadding() const { return selectionBarPadding_; }
    int getSelectionBarHeight() const { return selectionBarHeight_; }
    int getTitleSpacing() const { return titleSpacing_; }
    int getSelectionBarYOffset() const { return selectionBarYOffset_; }

    struct ContainerConfig {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        XAlignment xAlign = XAlignment::Numeric;
        YAlignment yAlign = YAlignment::Numeric;
        int xOffset = 0;
        int yOffset = 0;
        float alpha = 1.0f;
        int layer = 0;
        SDL_Color color = { 255, 255, 255, 255 };
        SDL_Color borderColor = { 0, 0, 0, 0 };
        int borderWidth = 0;
        int cornerRadius = 0;
    };

    const std::vector<ContainerConfig>& getContainers() const { return containers_; }
    SDL_Color getPanelColor() const { return panelColor_; }
    SDL_Color getBorderColor() const { return borderColor_; }
    int getBorderWidth() const { return borderWidth_; }
    int getCornerRadius() const { return cornerRadius_; }
    SDL_Color getGlowColor() const { return glowColor_; }
    int getGlowStrength() const { return glowStrength_; }
    int getModalWidth() const { return modalWidth_; }
    int getModalHeight() const { return modalHeight_; }
    bool isModalAutoScale() const { return modalAutoScale_; }
    int getModalMaxHeight() const { return modalMaxHeight_; }
    int getModalMinHeight() const { return modalMinHeight_; }
    int getModalMaxWidth() const { return modalMaxWidth_; }
    int getModalMinWidth() const { return modalMinWidth_; }
    struct MenuTitleEntry {
        std::string id;
        std::string title;
        std::string icon;
    };
    const std::vector<MenuTitleEntry>& getMenuTitles() const { return menuTitles_; }

    bool isModal() const {
        return (modalWidth_ > 0 || modalMaxWidth_ > 0 || modalAutoScale_ || panelColor_.a > 0 || borderColor_.a > 0);
    }
    void calculateModalBounds(size_t itemCount, int windowWidth, int windowHeight, float scaleFactor,
                              int& outModalX, int& outModalY, int& outModalW, int& outModalH,
                              int maxContentWidth = 0) const;
    int getModalXOffset() const { return modalXOffset_; }
    int getModalYOffset() const { return modalYOffset_; }

    const std::map<std::string, std::string>& getMenuIcons() const { return menuIcons_; }
    std::string getMenuIcon(const std::string& idOrKey) const;
    const std::map<std::string, std::string>& getButtonImages() const { return buttonImages_; }
    const std::string& getButtonFolder() const { return buttonFolder_; }
    bool hasButtonImages() const { return !buttonImages_.empty() || !buttonFolder_.empty(); }
    const std::string& getIconFolder() const { return iconFolder_; }

private:
    std::vector<ImageConfig> images_;
    std::vector<ContainerConfig> containers_;
        SDL_Color panelColor_ = { 22, 26, 40, 240 };       // 161A28F0 dark chrome-slate modal card
    SDL_Color borderColor_ = { 255, 191, 64, 255 };     // #FFBF40 golden amber from logo
    int borderWidth_ = 3;
    int cornerRadius_ = 24;
    SDL_Color glowColor_ = { 255, 191, 64, 255 };       // #FFBF40 golden amber glow
    int glowStrength_ = 60;
    int modalWidth_ = 500;
    int modalHeight_ = 0;
    bool modalAutoScale_ = true;
    int modalMaxHeight_ = 820;
    int modalMinHeight_ = 260;
    int modalMaxWidth_ = 0;
    int modalMinWidth_ = 0;
    int modalXOffset_ = 0;
    int modalYOffset_ = 0;
    SDL_Color backgroundColor_ = { 0, 0, 0, 176 };     // 000000B0 translucent backdrop dimmer
    std::string fontPath_ = "fonts/verdana.ttf";
    int titleFontSize_ = 26;
    SDL_Color titleColor_ = { 255, 255, 255, 255 };
    XAlignment titleXAlignment_ = XAlignment::Center;
    YAlignment titleYAlignment_ = YAlignment::Top;
    int titleXOffset_ = 0;
    int titleYOffset_ = 165;
    int titleX_ = 0;
    int titleY_ = 165;
    float titleXOrigin_ = 0.5f;
    bool hasExplicitTitleXOrigin_ = true;
    float titleYOrigin_ = 0.0f;
    bool hasExplicitTitleYOrigin_ = true;
    float menuXOrigin_ = 0.0f;
    bool hasExplicitMenuXOrigin_ = true;
    float menuYOrigin_ = 0.0f;
    bool hasExplicitMenuYOrigin_ = true;
    int optionFontSize_ = 18;
    SDL_Color optionColor_ = { 255, 255, 255, 255 };
    SDL_Color selectionBarColor_ = { 255, 191, 64, 80 }; // #FFBF4050
    SDL_Color titleBarColor_ = { 0, 0, 0, 255 };
    SDL_Color subtitleUnderlineColor_ = { 255, 191, 64, 128 }; // #FFBF4080
    int spaceBetweenText_ = 44;
    XAlignment menuXAlignment_ = XAlignment::Center;
    YAlignment menuYAlignment_ = YAlignment::Top;
    int menuXOffset_ = -140;
    int menuYOffset_ = 230;
    int menuX_ = -140;
    int menuY_ = 230;
    int windowWidth_ = 1920;
    int windowHeight_ = 1080;
    std::string scale_ = "fit";
    std::string mainTitleText_ = "Options Setup";
    std::string settingsTitleText_ = "Settings";
    std::string controlsTitleText_ = "Control";
    std::string layoutTitleText_ = "Layout";
    std::string actionsTitleText_ = "Info Keys";
    std::string saveExitText_ = "Save & Exit";
    std::string quitText_ = "Quit";
   // std::string kioskModeText_ = "Kiosk Mode";
    std::string signOutText_ = "Sign Out";
    std::string restartText_ = "Restart";
    std::string shutDownText_ = "Shut Down";
    SDL_Color fontColor_ = { 255, 255, 255, 255 };
    SDL_Color selectedColor_ = { 255, 255, 255, 255 };
    SDL_Color separatorColor_ = { 0, 0, 0, 0 };
    int separatorHeight_ = 1;
    int separatorWidth_ = 0;
    int separatorYOffset_ = 10;
    XAlignment separatorXAlignment_ = XAlignment::Center;
    int separatorX_ = 0;
    int separatorXOffset_ = 0;
    bool separatorAlignText_ = false;
    bool hasExplicitSeparatorXAlignment_ = false;
    std::string defaultMenuSrc_;
    std::vector<MenuTitleEntry> menuTitles_;
    std::map<std::string, std::string> menuIcons_;
    std::map<std::string, std::string> buttonImages_;
    std::string buttonFolder_;
    std::string iconFolder_;
    bool selectionBarColorParsed_ = false;
    int selectionBarPadding_ = 10;
    int selectionBarHeight_ = 40;
    int titleSpacing_ = 20;
    int selectionBarYOffset_ = -5;

    // variable / include system
    std::map<std::string, std::string> variables_;
    std::set<std::string> includedFiles_;
    std::string basePath_;
    static constexpr int MAX_INCLUDE_DEPTH = 15;

    // helper methods (kept private)
    std::string substituteVariables(const std::string& input);
    void parseVariables(xmlNodePtr rootNode);
    bool processInclude(xmlNodePtr includeNode, xmlDocPtr doc, xmlNodePtr parent);
    bool isNodeActive(xmlNodePtr node) const;

};

#endif