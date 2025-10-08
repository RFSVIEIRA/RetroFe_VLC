/* This file is part of RetroFE.
 *
 * RetroFE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * RetroFE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with RetroFE.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include "Component/Image.h"
#include "Component/AnimatedImageBuilder.h"
#include "FontCache.h"
#include <SDL.h>
#include <SDL_mixer.h>
#include <rapidxml/rapidxml.hpp>
#include <vector>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <unordered_map>

static const int MENU_INDEX_HIGH = 16;

class ScrollingList;
class Page;
class ViewInfo;
class Configuration;
class Font;

class PageBuilder
{
public:
    PageBuilder(std::string layoutKey, std::string layoutPage, Configuration& c, FontCache* fc, bool isMenu = false);
    virtual ~PageBuilder();
    Page* buildPage(std::string_view collectionName = "");
    void preloadLayouts(); 
    static std::string substituteGlobalVariables(const std::string& value);

private:
    std::string layoutKey;
    std::string layoutPage;
    std::string layoutPath;
    std::string baseLayoutPath;
    Configuration& config_;
    int screenHeight_;
    int screenWidth_;
    int layoutHeight_;
    int layoutWidth_;
    int monitor_;
    SDL_Color fontColor_;
    std::string fontName_;
    int fontSize_;
    FontCache* fontCache_;
    bool isMenu_;
    std::vector<std::string> monitors_;
    bool monitorsInitialized_ = false;

    // Cache for preloaded layout documents
    struct LayoutCache {
        std::string buffer;
        std::unique_ptr<rapidxml::xml_document<>> doc;
        std::string sourceFile;
    };
    std::map<std::string, LayoutCache> layoutCache_;

    // Helper function to parse common attributes
    void parseCommonAttributes(rapidxml::xml_node<>* node, Component* component);

    Font* addFont(rapidxml::xml_node<>* component, rapidxml::xml_node<>* defaults);
    void loadReloadableImages(rapidxml::xml_node<>* layout, std::string tagName, Page* page);
    float getVerticalAlignment(rapidxml::xml_attribute<>* attribute, float valueIfNull);
    float getHorizontalAlignment(rapidxml::xml_attribute<>* attribute, float valueIfNull);
    void buildViewInfo(rapidxml::xml_node<>* componentXml, ViewInfo& info, rapidxml::xml_node<>* defaultXml = nullptr, const ViewInfo* parentViewInfo = nullptr);
    bool buildComponents(rapidxml::xml_node<>* layout, Page* page);
    void loadTweens(Component* c, rapidxml::xml_node<>* componentXml);
    AnimationEvents* createTweenInstance(rapidxml::xml_node<>* componentXml);
    void buildTweenSet(AnimationEvents* tweens, rapidxml::xml_node<>* componentXml, std::string tagName, std::string tweenName);
    ScrollingList* buildMenu(rapidxml::xml_node<>* menuXml, Page& p);
    void buildCustomMenu(ScrollingList* menu, rapidxml::xml_node<>* menuXml, rapidxml::xml_node<>* itemDefaults);
    void buildVerticalMenu(ScrollingList* menu, rapidxml::xml_node<>* menuXml, rapidxml::xml_node<>* itemDefaults);
    int parseMenuPosition(std::string strIndex);
    rapidxml::xml_attribute<>* findAttribute(rapidxml::xml_node<>* componentXml, std::string attribute, rapidxml::xml_node<>* defaultXml);
    void getTweenSet(rapidxml::xml_node<>* node, Animation* animation);
    void getAnimationEvents(rapidxml::xml_node<>* node, TweenSet& tweens);
    ViewInfo* createMenuItemInfo(rapidxml::xml_node<>* component, rapidxml::xml_node<>* defaults, float y);

    void initializeMonitors();
    std::string_view constructLayoutPath(std::string_view collectionName);
    std::pair<std::string, std::string> constructLayoutFiles(std::string_view layoutPath, std::string_view monitor);
    bool processLayoutFile(std::string_view layoutFile, std::string_view layoutFileAspect, std::unique_ptr<Page>& page);
    bool parseLayoutAttributes(rapidxml::xml_node<>* root, std::unique_ptr<Page>& page);
    void loadSounds(rapidxml::xml_node<>* root, std::unique_ptr<Page>& page);
    
    // Variables storage
    std::map<std::string, std::string> variables_;
    static std::map<std::string, std::string> globalVariables_; 
    bool isDefaultLayout_; 
    void parseVariables(rapidxml::xml_node<>* node);
    std::string substituteVariables(const std::string& value) const;

    //includes
    bool processInclude(rapidxml::xml_node<>* includeNode, rapidxml::xml_document<>& mainDoc, rapidxml::xml_node<>* parentNode);
    std::set<std::string> includedFiles_;
    static constexpr size_t MAX_INCLUDE_DEPTH = 8;

};