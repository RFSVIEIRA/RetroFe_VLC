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

#include "PageBuilder.h"
#include "DefaultLayout.h"
#include "Page.h"
#include "ViewInfo.h"
#include "Component/Container.h"
#include "Component/Image.h"
#include "Component/AnimatedImage.h"
#include "Component/Text.h"
#include "Component/ReloadableText.h"
#include "Component/ReloadableMedia.h"
#include "Component/ReloadableScrollingText.h"
#include "Component/ScrollingList.h"
#include "Component/Video.h"
#include "Animate/AnimationEvents.h"
#include "Animate/TweenTypes.h"
#include "../Sound/Sound.h"
#include "../Collection/Item.h"
#include "../SDL.h"
#include "../Utility/Log.h"
#include "../Utility/Utils.h"
#include <algorithm>
#include <cfloat>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <map>
#include <string>
#include <memory>

using namespace rapidxml;

static const int MENU_FIRST = 0;   // first visible item in the list
static const int MENU_LAST = -3;   // last visible item in the list
static const int MENU_START = -1;  // first item transitions here after it scrolls "off the menu/screen"
static const int MENU_END = -2;    // last item transitions here after it scrolls "off the menu/screen"
static const int MENU_CENTER = -4;

std::map<std::string, std::string> PageBuilder::globalVariables_;
SearchConfig PageBuilder::defaultSearchConfig_;

PageBuilder::PageBuilder(std::string layoutKey, std::string layoutPage, Configuration& c, FontCache* fc, bool isMenu)
    : layoutKey(std::move(layoutKey))
    , layoutPage(std::move(layoutPage))
    , config_(c)
    , screenHeight_(SDL::getWindowHeight(0))
    , screenWidth_(SDL::getWindowWidth(0))
    , layoutHeight_(0)
    , layoutWidth_(0)
    , monitor_(0)
    , fontSize_(24)
    , fontCache_(fc)
    , isMenu_(isMenu)
    , baseLayoutPath(Utils::combinePath(c.absolutePath, "layouts", this->layoutKey))
    , isDefaultLayout_(layoutPage.empty() || layoutPage == "layout") // Default layout check
{
    fontColor_ = {0, 0, 0, 255};
}

PageBuilder::~PageBuilder() = default;

void PageBuilder::preloadLayouts()
{
    if (!monitorsInitialized_) {
        initializeMonitors();
    }

    includedFiles_.clear(); // Reset include tracking
    std::vector<std::string> layoutsToPreload = { "" }; // Base layout
    std::string firstCollection = "Main";
    config_.getProperty("firstCollection", firstCollection);
    if (!firstCollection.empty()) {
        layoutsToPreload.push_back(firstCollection);
    }

    for (const auto& collection : layoutsToPreload) {
        isDefaultLayout_ = collection.empty();
        std::string_view layoutPath = constructLayoutPath(collection);
        for (const auto& monitor : monitors_) {
            auto [layoutFile, layoutFileAspect] = constructLayoutFiles(layoutPath, monitor);
            std::string key = layoutFileAspect;
            auto file = std::make_unique<std::ifstream>(layoutFileAspect, std::ios::binary);
            if (!file->good()) {
                file = std::make_unique<std::ifstream>(layoutFile, std::ios::binary);
                key = layoutFile;
                if (!file->good()) continue;
            }

            file->seekg(0, std::ios::end);
            std::string buffer;
            buffer.resize(file->tellg());
            file->seekg(0, std::ios::beg);
            file->read(buffer.data(), buffer.size());
            buffer.push_back('\0');

            LayoutCache& cache = layoutCache_[key];
            cache.buffer = std::move(buffer);
            cache.doc = std::make_unique<rapidxml::xml_document<>>();
            cache.sourceFile = key;
            cache.doc->parse<0>(cache.buffer.data());
            rapidxml::xml_node<>* root = cache.doc->first_node("layout");
            if (root) {
                parseVariables(root);
                // Scan for <include> tags
                for (auto* includeNode = root->first_node("include"); includeNode; includeNode = includeNode->next_sibling("include")) {
                    xml_attribute<>* srcAttr = includeNode->first_attribute("src");
                    if (srcAttr) {
                        std::string src = substituteVariables(srcAttr->value());
                        std::string includePath = Utils::combinePath(baseLayoutPath, src);
                        if (includedFiles_.find(includePath) == includedFiles_.end() && includedFiles_.size() < MAX_INCLUDE_DEPTH) {
                            includedFiles_.insert(includePath);
                            auto includeFile = std::make_unique<std::ifstream>(includePath, std::ios::binary);
                            if (includeFile->good()) {
                                includeFile->seekg(0, std::ios::end);
                                std::string includeBuffer;
                                includeBuffer.resize(includeFile->tellg());
                                includeFile->seekg(0, std::ios::beg);
                                includeFile->read(includeBuffer.data(), includeBuffer.size());
                                includeBuffer.push_back('\0');

                                LayoutCache& includeCache = layoutCache_[includePath];
                                includeCache.buffer = std::move(includeBuffer);
                                includeCache.doc = std::make_unique<rapidxml::xml_document<>>();
                                includeCache.sourceFile = includePath;
                                includeCache.doc->parse<0>(includeCache.buffer.data());
                                Logger::write(Logger::ZONE_INFO, "Layout", "Preloaded include: " + includePath);
                            }
                            includedFiles_.erase(includePath);
                        }
                    }
                }
            }
            Logger::write(Logger::ZONE_INFO, "Layout", "Preloaded " + key);
        }
    }
}

Page* PageBuilder::buildPage(std::string_view collectionName)
{
    std::unique_ptr<Page> page;
    if (!monitorsInitialized_) {
        initializeMonitors();
    }
    isDefaultLayout_ = collectionName.empty();
    std::string_view layoutPath = constructLayoutPath(collectionName);
    for (size_t monitor = 0; monitor < monitors_.size(); ++monitor) {
        monitor_ = (monitor > 0) ? static_cast<int>(monitor - 1) : 0;
        auto [layoutFile, layoutFileAspect] = constructLayoutFiles(layoutPath, monitors_[monitor]);

        // Check cache first
        auto it = layoutCache_.find(layoutFileAspect);
        if (it == layoutCache_.end()) it = layoutCache_.find(layoutFile);
        if (it != layoutCache_.end()) {
            rapidxml::xml_node<>* root = it->second.doc->first_node("layout");
            if (root && parseLayoutAttributes(root, page)) {
                parseVariables(root);
                loadSounds(root, page);
                if (buildComponents(root, page.get())) {
                    Logger::write(Logger::ZONE_INFO, "Layout", "Initialized from cache: " + it->first);
                    if (page) {
                        page->setLoadingLayoutDir(Utils::combinePath(Configuration::absolutePath, "layouts", layoutKey));
                    }
                    return page.release();
                }
            }
        }

        // Fallback to live processing
        if (processLayoutFile(layoutFile, layoutFileAspect, page)) {
            break;
        }
    }

    // Fail-Safe: If no layout or splash file was found on disk, use embedded defaults
    if (!page && collectionName.empty()) {
        if (layoutPage == "layout") {
            processEmbeddedDefaultLayout(page, false);
        } else if (layoutPage == "splash") {
            processEmbeddedDefaultLayout(page, true);
        }
    }

    if (page) {
        page->setLoadingLayoutDir(Utils::combinePath(Configuration::absolutePath, "layouts", layoutKey));
    }
    return page.release();
}
bool PageBuilder::processEmbeddedDefaultLayout(std::unique_ptr<Page>& page, bool isSplash)
{
    monitor_ = 0;
    if (isSplash) {
        Logger::write(Logger::ZONE_INFO, "Layout", "No splash file found on disk. Falling back to embedded splash...");
    } else {
        Logger::write(Logger::ZONE_WARNING, "Layout", "No layout file found on disk. Falling back to embedded default layout (BaseCenter)...");
    }

    std::string buffer = isSplash ? DefaultLayout::getSplashXml() : DefaultLayout::getLayoutXml();
    buffer.push_back('\0');

    auto doc = std::make_unique<rapidxml::xml_document<>>();
    try {
        doc->parse<0>(buffer.data());
        rapidxml::xml_node<>* root = doc->first_node("layout");
        if (!root) {
            Logger::write(Logger::ZONE_ERROR, "Layout", "Embedded layout missing <layout> tag");
            return false;
        }

        parseVariables(root);
        if (!parseLayoutAttributes(root, page)) {
            return false;
        }

        loadSounds(root, page);
        if (!buildComponents(root, page.get())) {
            return false;
        }

        Logger::write(Logger::ZONE_INFO, "Layout", isSplash ? "Embedded splash loaded successfully!" : "Embedded default layout (BaseCenter) loaded successfully!");
        return true;
    } catch (const rapidxml::parse_error& e) {
        Logger::write(Logger::ZONE_ERROR, "Layout", std::string("Embedded layout XML parse error: ") + e.what());
        return false;
    }
}

void PageBuilder::initializeMonitors()
{
    monitors_.reserve(SDL::getNumScreens() + 1);
    monitors_.emplace_back("");
    for (int i = 0; i < SDL::getNumScreens(); ++i) {
        monitors_.emplace_back(" - " + std::to_string(i));
    }
    monitorsInitialized_ = true;
}

std::string_view PageBuilder::constructLayoutPath(std::string_view collectionName)
{
    static std::string menuPath = Utils::combinePath(Configuration::absolutePath, "menu");
    static std::string layoutBasePath = Utils::combinePath(Configuration::absolutePath, "layouts", layoutKey);

    if (isMenu_) return menuPath;
    if (collectionName.empty()) return layoutBasePath;
    
    if (layoutPath.empty() || layoutPath.find(collectionName) == std::string::npos) {
        layoutPath = Utils::combinePath(
            Utils::combinePath(Configuration::absolutePath, "layouts", layoutKey, "collections", std::string(collectionName)),
            "layout"
        );
    }
    return layoutPath;
}

std::pair<std::string, std::string> PageBuilder::constructLayoutFiles(std::string_view layoutPath, std::string_view monitor)
{
    std::string layoutFile = Utils::combinePath(std::string(layoutPath), layoutPage + std::string(monitor) + ".xml");
    std::string aspectRatio = std::to_string(screenWidth_ / Utils::gcd(screenWidth_, screenHeight_)) + "x" +
                              std::to_string(screenHeight_ / Utils::gcd(screenWidth_, screenHeight_));
    std::string layoutFileAspect = Utils::combinePath(std::string(layoutPath), layoutPage + " " + aspectRatio + std::string(monitor) + ".xml");
    return {std::move(layoutFile), std::move(layoutFileAspect)};
}

bool PageBuilder::processLayoutFile(std::string_view layoutFile, std::string_view layoutFileAspect, std::unique_ptr<Page>& page)
{
    auto file = std::make_unique<std::ifstream>(std::string(layoutFileAspect).c_str(), std::ios::binary);
    if (!file->good()) {
        Logger::write(Logger::ZONE_INFO, "Layout", "Could not find layout file: " + std::string(layoutFileAspect));
        Logger::write(Logger::ZONE_INFO, "Layout", "Initializing " + std::string(layoutFile));
        file = std::make_unique<std::ifstream>(std::string(layoutFile).c_str(), std::ios::binary);
        if (!file->good()) {
            Logger::write(Logger::ZONE_INFO, "Layout", "Could not find layout file: " + std::string(layoutFile));
            return false;
        }
    }

    file->seekg(0, std::ios::end);
    std::string buffer;
    buffer.resize(file->tellg());
    file->seekg(0, std::ios::beg);
    file->read(buffer.data(), buffer.size());
    buffer.push_back('\0');

    auto doc = std::make_unique<rapidxml::xml_document<>>();
    try {
        doc->parse<0>(buffer.data());
        rapidxml::xml_node<>* root = doc->first_node("layout");
        if (!root) {
            Logger::write(Logger::ZONE_ERROR, "Layout", "Missing <layout> tag");
            return false;
        }

        parseVariables(root);
        if (!parseLayoutAttributes(root, page)) {
            return false;
        }

        loadSounds(root, page);
        if (!buildComponents(root, page.get())) {
            return false;
        }

        // Cache the layout
        LayoutCache& cache = layoutCache_[std::string(layoutFileAspect.empty() ? layoutFile : layoutFileAspect)];
        cache.buffer = std::move(buffer);
        cache.doc = std::make_unique<rapidxml::xml_document<>>();
        cache.sourceFile = std::string(layoutFileAspect.empty() ? layoutFile : layoutFileAspect);
        cache.doc->parse<0>(cache.buffer.data());

        Logger::write(Logger::ZONE_INFO, "Layout", "Initialized");
        return true;
    }
    catch (rapidxml::parse_error& e) {
        std::string reason = (e.what() != nullptr) ? e.what() : "unknown error";
        std::string errorMsg = "Could not parse layout file. [Line: " +
            std::to_string(std::count(buffer.begin(),
                std::find(buffer.begin(), buffer.end(), *e.where<char>()), '\n') + 1) +
            "] Reason: " + reason;

        if (reason.find("expected >") != std::string::npos) {
            // Harmless case for PHOENIX theme - silent in Release
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "Layout", errorMsg + " (harmless in layout)");
#else
            // completely silent for normal users
#endif
        }
        else {
            // Real error - always visible
            Logger::write(Logger::ZONE_ERROR, "Layout", errorMsg);
        }
        return false;
    }
    catch (std::exception& e) {
        Logger::write(Logger::ZONE_ERROR, "Layout", "Could not parse layout file. Reason: " + std::string(e.what()));
        return false;
    }
}
bool PageBuilder::parseLayoutAttributes(rapidxml::xml_node<>* root, std::unique_ptr<Page>& page)
{
    rapidxml::xml_attribute<>* layoutWidthXml = root->first_attribute("width");
    rapidxml::xml_attribute<>* layoutHeightXml = root->first_attribute("height");
    rapidxml::xml_attribute<>* fontXml = root->first_attribute("font");
    rapidxml::xml_attribute<>* fontColorXml = root->first_attribute("fontColor");
    rapidxml::xml_attribute<>* fontSizeXml = root->first_attribute("loadFontSize");
    rapidxml::xml_attribute<>* minShowTimeXml = root->first_attribute("minShowTime");

    xml_attribute<>* activeAttr = root->first_attribute("active");
    if (activeAttr && Utils::toLower(substituteVariables(activeAttr->value())) != "true") {
        Logger::write(Logger::ZONE_DEBUG, "Layout", "Skipping inactive layout: active=" + std::string(activeAttr->value()));
        return false;  // Skip this entire layout file
    }

    if (!layoutWidthXml || !layoutHeightXml) {
        Logger::write(Logger::ZONE_ERROR, "Layout", "<layout> tag must specify a width and height");
        return false;
    }

    if (fontXml) {
        fontName_ = config_.convertToAbsolutePath(
            Utils::combinePath(config_.absolutePath, "layouts", layoutKey, ""),
            substituteVariables(fontXml->value()).c_str()); 
    }

    if (fontColorXml) {
        int intColor = 0;
        std::istringstream ss(substituteVariables(fontColorXml->value())); 
        ss >> std::hex >> intColor;
        fontColor_.b = intColor & 0xFF;
        fontColor_.g = (intColor >> 8) & 0xFF;
        fontColor_.r = (intColor >> 16) & 0xFF;
    }

    if (fontSizeXml) {
        fontSize_ = Utils::convertInt(substituteVariables(fontSizeXml->value()).c_str()); 
    }

    layoutWidth_ = Utils::convertInt(substituteVariables(layoutWidthXml->value()).c_str()); 
    layoutHeight_ = Utils::convertInt(substituteVariables(layoutHeightXml->value()).c_str()); 

    if (layoutWidth_ == 0 || layoutHeight_ == 0) {
        Logger::write(Logger::ZONE_ERROR, "Layout", "Layout width and height cannot be set to 0");
        return false;
    }

    // Log resolution only once or in debug mode to reduce string construction
#ifdef DEBUG
    std::ostringstream ss;
    ss << layoutWidth_ << "x" << layoutHeight_ << " (scale "
        << static_cast<float>(screenWidth_) / layoutWidth_ << "x"
        << static_cast<float>(screenHeight_) / layoutHeight_ << ")";
    Logger::write(Logger::ZONE_INFO, "Layout", "Layout resolution " + ss.str());
#endif

    if (!page) {
        page = std::make_unique<Page>(config_, layoutWidth_, layoutHeight_);
        page->setSearchConfig(defaultSearchConfig_);
        page->setLoadingLayoutDir(Utils::combinePath(Configuration::absolutePath, "layouts", layoutKey));
    }
    else {
        page->setLayoutWidth(monitor_, layoutWidth_);
        page->setLayoutHeight(monitor_, layoutHeight_);
    }

    if (minShowTimeXml) {
        page->setMinShowTime(Utils::convertFloat(substituteVariables(minShowTimeXml->value()).c_str())); 
    }

    return true;
}


void PageBuilder::loadSounds(rapidxml::xml_node<>* root, std::unique_ptr<Page>& page) {
    for (rapidxml::xml_node<>* soundNode = root->first_node("sound"); soundNode; soundNode = soundNode->next_sibling("sound")) {
        // Check if the sound node is active
        xml_attribute<>* activeAttr = soundNode->first_attribute("active");
        if (activeAttr) { // Check if the active attribute exists
            std::string activeValue = Utils::toLower(substituteVariables(activeAttr->value())); 
            if (activeValue != "true") { // Skip unless value is exactly "true"
#ifdef _DEBUG
                Logger::write(Logger::ZONE_DEBUG, "Layout", "Skipping inactive or invalid 'active' value for sound component: " + activeValue);
#endif
                continue; // Skip creation of inactive or invalid components
            }
        }
        rapidxml::xml_attribute<>* src = soundNode->first_attribute("src");
        rapidxml::xml_attribute<>* type = soundNode->first_attribute("type");
        if (!type || !src) {
            Logger::write(Logger::ZONE_ERROR, "Layout", "Sound tag missing required attributes");
            continue;
        }

        // File paths
        std::string file = Configuration::convertToAbsolutePath(layoutPath, substituteVariables(src->value()).c_str()); 
        std::string altfile;
        config_.getProperty("layout", altfile);
        altfile = Utils::combinePath(Configuration::absolutePath, "layouts", altfile, substituteVariables(src->value()).c_str()); 

        // Create Sound object
        auto soundPtr = std::make_unique<Sound>(file, altfile);

        // Parse new attributes
        soundPtr->group = soundNode->first_attribute("group") ? substituteVariables(soundNode->first_attribute("group")->value()) : ""; 
        soundPtr->subgroup = soundNode->first_attribute("subgroup") ? substituteVariables(soundNode->first_attribute("subgroup")->value()) : ""; 
        soundPtr->name = soundNode->first_attribute("name") ? substituteVariables(soundNode->first_attribute("name")->value()) : ""; 
        soundPtr->active = true; // Default to active
        if (activeAttr) {
            std::string activeValue = substituteVariables(activeAttr->value());
            soundPtr->active = (activeValue == "true"); // Set to false only if explicitly "false"
        }

        // Parse 'shared' attribute (e.g., comma-separated list)
        if (auto sharedAttr = soundNode->first_attribute("shared")) {
            std::string sharedStr = substituteVariables(sharedAttr->value()); 
            Utils::listToVector(sharedStr, soundPtr->shared, ',');
        }

        // Add to page
        page->addSound(substituteVariables(type->value()), soundPtr.release()); 
    }
}


float PageBuilder::getHorizontalAlignment(xml_attribute<>* attribute, float valueIfNull)
{
    float value;
    std::string str;

    if (!attribute)
    {
        value = valueIfNull;
    }
    else
    {
        str = substituteVariables(attribute->value()); 

        if (!str.compare("left"))
        {
            value = 0;
        }
        else if (!str.compare("center"))
        {
            value = static_cast<float>(layoutWidth_) / 2;
        }
        else if (!str.compare("right") || !str.compare("stretch"))
        {
            value = static_cast<float>(layoutWidth_);
        }
        else
        {
            value = Utils::convertFloat(str.c_str()); // Modified: Use .c_str() for compatibility
        }
    }

    return value;
}

float PageBuilder::getVerticalAlignment(xml_attribute<>* attribute, float valueIfNull)
{
    float value;
    std::string str;
    if (!attribute)
    {
        value = valueIfNull;
    }
    else
    {
        str = substituteVariables(attribute->value()); 

        if (!str.compare("top"))
        {
            value = 0;
        }
        else if (!str.compare("center"))
        {
            value = static_cast<float>(layoutHeight_ / 2);
        }
        else if (!str.compare("bottom") || !str.compare("stretch"))
        {
            value = static_cast<float>(layoutHeight_);
        }
        else
        {
            value = Utils::convertFloat(str.c_str()); // Modified: Use .c_str() for compatibility
        }
    }
    return value;
}




bool PageBuilder::buildComponents(xml_node<>* layout, Page* page)
{
    // Process <include> tags first
    for (rapidxml::xml_node<>* includeNode = layout->first_node("include"); includeNode; includeNode = includeNode->next_sibling("include")) {
        if (!processInclude(includeNode, *layout->document(), layout)) {
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "Layout", "Failed to process <include> tag (usually harmless in PHOENIX theme)");
#endif
        }
    }

    // Process <search> configuration
    for (rapidxml::xml_node<>* searchXml = layout->first_node("search"); searchXml; searchXml = searchXml->next_sibling("search"))
    {
        parseSearchConfig(searchXml, page);
    }

    for (xml_node<>* componentXml = layout->first_node("menu"); componentXml; componentXml = componentXml->next_sibling("menu"))
    {
        xml_attribute<>* activeAttr = componentXml->first_attribute("active");
        if (activeAttr) {
            std::string activeValue = Utils::toLower(substituteVariables(activeAttr->value())); 
            if (activeValue != "true") {
#ifdef _DEBUG
                Logger::write(Logger::ZONE_DEBUG, "Layout", "Skipping inactive or invalid 'active' value for menu component: " + activeValue);
#endif
                continue;
            }
        }
        ScrollingList* scrollingList = buildMenu(componentXml, *page);
        xml_attribute<>* indexXml = componentXml->first_attribute("menuIndex");
        int index = indexXml ? Utils::convertInt(substituteVariables(indexXml->value()).c_str()) : -1; 
        page->pushMenu(scrollingList, index);
    }

    for (xml_node<>* componentXml = layout->first_node("container"); componentXml; componentXml = componentXml->next_sibling("container"))
    {
        xml_attribute<>* isOptMenu = componentXml->first_attribute("isOptionsMenu");
        if (isOptMenu) {
            std::string optVal = Utils::toLower(substituteVariables(isOptMenu->value()));
            if (optVal == "true" || optVal == "yes" || optVal == "1") {
                continue;
            }
        }
        xml_attribute<>* activeAttr = componentXml->first_attribute("active");
        if (activeAttr) {
            std::string activeValue = Utils::toLower(substituteVariables(activeAttr->value())); 
            if (activeValue != "true") {
#ifdef _DEBUG
                Logger::write(Logger::ZONE_DEBUG, "Layout", "Skipping inactive or invalid 'active' value for container component: " + activeValue);
#endif
                continue;
            }
        }
        xml_attribute<>* loadingAttr = componentXml->first_attribute("loading");
        if (loadingAttr && (Utils::toLower(substituteVariables(loadingAttr->value())) == "true" ||
                            Utils::toLower(substituteVariables(loadingAttr->value())) == "yes"))
        {
            Page::LoadingElement el;
            el.type = Page::LoadingElement::Container;
            xml_attribute<>* bg = findAttribute(componentXml, "backgroundColor", nullptr);
            if (!bg) bg = findAttribute(componentXml, "color", nullptr);
            if (bg) el.color = Page::parseColor(substituteVariables(bg->value()), { 0, 0, 0, 255 });
            xml_attribute<>* x = findAttribute(componentXml, "x", nullptr);
            if (x) el.xStr = substituteVariables(x->value());
            xml_attribute<>* y = findAttribute(componentXml, "y", nullptr);
            if (y) el.yStr = substituteVariables(y->value());
            xml_attribute<>* xOffset = findAttribute(componentXml, "xOffset", nullptr);
            if (xOffset) el.xOffsetStr = substituteVariables(xOffset->value());
            xml_attribute<>* yOffset = findAttribute(componentXml, "yOffset", nullptr);
            if (yOffset) el.yOffsetStr = substituteVariables(yOffset->value());
            xml_attribute<>* xOrigin = findAttribute(componentXml, "xOrigin", nullptr);
            if (xOrigin) el.xOriginStr = substituteVariables(xOrigin->value());
            xml_attribute<>* yOrigin = findAttribute(componentXml, "yOrigin", nullptr);
            if (yOrigin) el.yOriginStr = substituteVariables(yOrigin->value());
            xml_attribute<>* w = findAttribute(componentXml, "width", nullptr);
            if (w) el.wStr = substituteVariables(w->value());
            xml_attribute<>* h = findAttribute(componentXml, "height", nullptr);
            if (h) el.hStr = substituteVariables(h->value());
            xml_attribute<>* alpha = findAttribute(componentXml, "backgroundAlpha", nullptr);
            if (!alpha) alpha = findAttribute(componentXml, "alpha", nullptr);
            if (alpha) el.alpha = Utils::convertFloat(substituteVariables(alpha->value()).c_str());
            el.layer = 0;
            xml_attribute<>* layerAttr = findAttribute(componentXml, "layer", nullptr);
            if (layerAttr) el.layer = Utils::convertInt(substituteVariables(layerAttr->value()).c_str());
            xml_attribute<>* mon = findAttribute(componentXml, "monitor", nullptr);
            if (mon) el.monitor = Utils::convertInt(substituteVariables(mon->value()).c_str());
            page->addLoadingElement(el);
            continue;
        }

        Container* c = new Container(*page);
        xml_attribute<>* menuScrollReload = componentXml->first_attribute("menuScrollReload");
        if (menuScrollReload &&
            (Utils::toLower(substituteVariables(menuScrollReload->value())) == "true" || 
                Utils::toLower(substituteVariables(menuScrollReload->value())) == "yes"))
        {
            c->setMenuScrollReload(true);
        }
        parseCommonAttributes(componentXml, c);
        buildViewInfo(componentXml, c->baseViewInfo);
        loadTweens(c, componentXml);
        page->addComponent(c);
    }

    for (xml_node<>* componentXml = layout->first_node("image"); componentXml; componentXml = componentXml->next_sibling("image")) {
        xml_attribute<>* isOptMenu = componentXml->first_attribute("isOptionsMenu");
        if (isOptMenu) {
            std::string optVal = Utils::toLower(substituteVariables(isOptMenu->value()));
            if (optVal == "true" || optVal == "yes" || optVal == "1") {
                continue;
            }
        }
        xml_attribute<>* activeAttr = componentXml->first_attribute("active");
        if (activeAttr) {
            std::string activeValue = Utils::toLower(substituteVariables(activeAttr->value())); 
            if (activeValue != "true") {
#ifdef _DEBUG
                Logger::write(Logger::ZONE_DEBUG, "Layout", "Skipping inactive or invalid 'active' value for image component: " + activeValue);
#endif
                continue;
            }
        }
        xml_attribute<>* loadingAttr = componentXml->first_attribute("loading");
        if (loadingAttr && (Utils::toLower(substituteVariables(loadingAttr->value())) == "true" ||
                            Utils::toLower(substituteVariables(loadingAttr->value())) == "yes"))
        {
            xml_attribute<>* src = componentXml->first_attribute("src");
            xml_attribute<>* randomSrcXml = componentXml->first_attribute("randomsrc");
            Page::LoadingElement el;
            el.type = Page::LoadingElement::Image;
            if (src) el.src = substituteVariables(src->value());
            else if (randomSrcXml) el.src = substituteVariables(randomSrcXml->value());
            xml_attribute<>* x = findAttribute(componentXml, "x", nullptr);
            if (x) el.xStr = substituteVariables(x->value());
            xml_attribute<>* y = findAttribute(componentXml, "y", nullptr);
            if (y) el.yStr = substituteVariables(y->value());
            xml_attribute<>* xOffset = findAttribute(componentXml, "xOffset", nullptr);
            if (xOffset) el.xOffsetStr = substituteVariables(xOffset->value());
            xml_attribute<>* yOffset = findAttribute(componentXml, "yOffset", nullptr);
            if (yOffset) el.yOffsetStr = substituteVariables(yOffset->value());
            xml_attribute<>* xOrigin = findAttribute(componentXml, "xOrigin", nullptr);
            if (xOrigin) el.xOriginStr = substituteVariables(xOrigin->value());
            xml_attribute<>* yOrigin = findAttribute(componentXml, "yOrigin", nullptr);
            if (yOrigin) el.yOriginStr = substituteVariables(yOrigin->value());
            xml_attribute<>* w = findAttribute(componentXml, "width", nullptr);
            if (w) el.wStr = substituteVariables(w->value());
            xml_attribute<>* h = findAttribute(componentXml, "height", nullptr);
            if (h) el.hStr = substituteVariables(h->value());
            xml_attribute<>* sm = findAttribute(componentXml, "scaleMode", nullptr);
            if (sm) el.scaleMode = Utils::toLower(substituteVariables(sm->value()));
            xml_attribute<>* alpha = findAttribute(componentXml, "alpha", nullptr);
            if (alpha) el.alpha = Utils::convertFloat(substituteVariables(alpha->value()).c_str());
            el.layer = 1;
            xml_attribute<>* layerAttr = findAttribute(componentXml, "layer", nullptr);
            if (layerAttr) el.layer = Utils::convertInt(substituteVariables(layerAttr->value()).c_str());
            xml_attribute<>* mon = findAttribute(componentXml, "monitor", nullptr);
            if (mon) el.monitor = Utils::convertInt(substituteVariables(mon->value()).c_str());
            page->addLoadingElement(el);
            continue;
        }

        xml_attribute<>* src = componentXml->first_attribute("src");
        xml_attribute<>* idXml = componentXml->first_attribute("id");
        xml_attribute<>* monitorXml = componentXml->first_attribute("monitor");
        xml_attribute<>* frameLoop = componentXml->first_attribute("frameLoop");
        xml_attribute<>* alwaysAnimated = componentXml->first_attribute("isAlwaysAnimated");
        xml_attribute<>* speedXml = componentXml->first_attribute("speed");
        xml_attribute<>* randomXml = componentXml->first_attribute("random");
        xml_attribute<>* slideShowXml = componentXml->first_attribute("slideShow");
        xml_attribute<>* slideShowTimerXml = componentXml->first_attribute("slideShowTimer");
        xml_attribute<>* slideNumberXml = componentXml->first_attribute("slideNumber");
        xml_attribute<>* randomSrcXml = componentXml->first_attribute("randomsrc");

        int id = idXml ? Utils::convertInt(substituteVariables(idXml->value()).c_str()) : -1; 
        if (!src && !randomSrcXml) {
            Logger::write(Logger::ZONE_ERROR, "Layout", "Image component in layout does not specify a source file");
            continue;
        }

        std::string imagePath = Utils::combinePath(Configuration::convertToAbsolutePath(layoutPath, ""), src ? substituteVariables(src->value()) : ""); 
        std::string layoutName;
        config_.getProperty("layout", layoutName);
        std::string altImagePath = Utils::combinePath(Configuration::absolutePath, "layouts", layoutName, src ? substituteVariables(src->value()) : ""); 
        int monitor = monitorXml ? Utils::convertInt(substituteVariables(monitorXml->value()).c_str()) : monitor_; 
        std::string frameLoopValue = frameLoop ? substituteVariables(frameLoop->value()) : "noPlay"; 
        bool isAlwaysAnimated = alwaysAnimated ?
            (Utils::toLower(substituteVariables(alwaysAnimated->value())) == "true" || 
                Utils::toLower(substituteVariables(alwaysAnimated->value())) == "yes") : true;
        int speed = speedXml ? Utils::convertInt(substituteVariables(speedXml->value()).c_str()) : 7500; 
        std::string randomSrcPath = randomSrcXml ? Utils::combinePath(Configuration::convertToAbsolutePath(layoutPath, ""), substituteVariables(randomSrcXml->value())) : ""; 
        std::string altRandomSrcPath = randomSrcXml ? Utils::combinePath(Configuration::absolutePath, "layouts", layoutName, substituteVariables(randomSrcXml->value())) : ""; 
        bool random = randomXml && (Utils::toLower(substituteVariables(randomXml->value())) == "true" || 
            Utils::toLower(substituteVariables(randomXml->value())) == "yes");
        bool slideShow = slideShowXml && (Utils::toLower(substituteVariables(slideShowXml->value())) == "true" || 
            Utils::toLower(substituteVariables(slideShowXml->value())) == "yes");
        int slideShowTimer = slideShowTimerXml ? Utils::convertInt(substituteVariables(slideShowTimerXml->value()).c_str()) : 5000; 
        int slideNumber = slideNumberXml ? Utils::convertInt(substituteVariables(slideNumberXml->value()).c_str()) : 5; 
        config_.getProperty("slideShowTimer", slideShowTimer);
        config_.getProperty("maxRandomMedia", slideNumber);

        Component* c = nullptr;
        if (frameLoop) {
            c = new AnimatedImage(imagePath, altImagePath, *page, monitor, isAlwaysAnimated, frameLoopValue, speed, random, slideShow, slideShowTimer, slideNumber, randomSrcPath, altRandomSrcPath);
            Logger::write(Logger::ZONE_INFO, "PageBuilder",
                "Created AnimatedImage: " + imagePath + " with frameLoop=" + frameLoopValue + ", speed=" + std::to_string(speed));
        }
        else {
            c = new Image(imagePath, altImagePath, *page, monitor, random, slideShow, slideShowTimer, slideNumber, randomSrcPath, altRandomSrcPath);
#ifdef _DEBUG
            Logger::write(Logger::ZONE_INFO, "PageBuilder", "Created static Image: " + imagePath);
#endif
        }

        c->setId(id);
        xml_attribute<>* menuScrollReload = componentXml->first_attribute("menuScrollReload");
        if (menuScrollReload && (Utils::toLower(substituteVariables(menuScrollReload->value())) == "true" || 
            Utils::toLower(substituteVariables(menuScrollReload->value())) == "yes")) {
            c->setMenuScrollReload(true);
        }
        parseCommonAttributes(componentXml, c);
        buildViewInfo(componentXml, c->baseViewInfo);
        loadTweens(c, componentXml);
        page->addComponent(c);
    }

    for (xml_node<>* componentXml = layout->first_node("video"); componentXml; componentXml = componentXml->next_sibling("video"))
    {
        xml_attribute<>* activeAttr = componentXml->first_attribute("active");
        if (activeAttr) {
            std::string activeValue = Utils::toLower(substituteVariables(activeAttr->value())); 
            if (activeValue != "true") {
#ifdef _DEBUG
                Logger::write(Logger::ZONE_DEBUG, "Layout", "Skipping inactive or invalid 'active' value for video component: " + activeValue);
#endif
                continue;
            }
        }
        xml_attribute<>* srcXml = componentXml->first_attribute("src");
        xml_attribute<>* numLoopsXml = componentXml->first_attribute("numLoops");
        xml_attribute<>* idXml = componentXml->first_attribute("id");
        xml_attribute<>* monitorXml = componentXml->first_attribute("monitor");

        int id = -1;
        if (idXml)
        {
            id = Utils::convertInt(substituteVariables(idXml->value()).c_str()); 
        }

        if (!srcXml)
        {
            Logger::write(Logger::ZONE_ERROR, "Layout", "Video component in layout does not specify a source video file");
        }
        else
        {
            std::string videoPath;
            videoPath = Utils::combinePath(Configuration::convertToAbsolutePath(layoutPath, videoPath), substituteVariables(srcXml->value())); 
            std::string layoutName;
            config_.getProperty("layout", layoutName);
            std::string altVideoPath;
            altVideoPath = Utils::combinePath(Configuration::absolutePath, "layouts", layoutName, substituteVariables(srcXml->value())); 
            int numLoops = numLoopsXml ? Utils::convertInt(substituteVariables(numLoopsXml->value()).c_str()) : 1; 
            int monitor = monitorXml ? Utils::convertInt(substituteVariables(monitorXml->value()).c_str()) : monitor_; 
            Video* c = new Video(videoPath, altVideoPath, numLoops, *page, monitor);
            c->setId(id);
            xml_attribute<>* menuScrollReload = componentXml->first_attribute("menuScrollReload");
            if (menuScrollReload &&
                (Utils::toLower(substituteVariables(menuScrollReload->value())) == "true" || 
                    Utils::toLower(substituteVariables(menuScrollReload->value())) == "yes"))
            {
                c->setMenuScrollReload(true);
            }
            parseCommonAttributes(componentXml, c);
            buildViewInfo(componentXml, c->baseViewInfo);
            loadTweens(c, componentXml);
            page->addComponent(c);
        }
    }

    for (xml_node<>* componentXml = layout->first_node("text"); componentXml; componentXml = componentXml->next_sibling("text"))
    {
        xml_attribute<>* isOptMenu = componentXml->first_attribute("isOptionsMenu");
        if (isOptMenu) {
            std::string optVal = Utils::toLower(substituteVariables(isOptMenu->value()));
            if (optVal == "true" || optVal == "yes" || optVal == "1") {
                continue;
            }
        }
        xml_attribute<>* activeAttr = componentXml->first_attribute("active");
        if (activeAttr) {
            std::string activeValue = Utils::toLower(substituteVariables(activeAttr->value())); 
            if (activeValue != "true") {
#ifdef _DEBUG
                Logger::write(Logger::ZONE_DEBUG, "Layout", "Skipping inactive or invalid 'active' value for text component: " + activeValue);
#endif
                continue;
            }
        }
        xml_attribute<>* loadingAttr = componentXml->first_attribute("loading");
        if (loadingAttr && (Utils::toLower(substituteVariables(loadingAttr->value())) == "true" ||
                            Utils::toLower(substituteVariables(loadingAttr->value())) == "yes"))
        {
            xml_attribute<>* value = componentXml->first_attribute("value");
            Page::LoadingElement el;
            el.type = Page::LoadingElement::Text;
            if (value) el.text = substituteVariables(value->value());
            xml_attribute<>* fontXml = findAttribute(componentXml, "font", nullptr);
            if (fontXml) el.font = substituteVariables(fontXml->value());
            xml_attribute<>* fontColorXml = findAttribute(componentXml, "fontColor", nullptr);
            if (!fontColorXml) fontColorXml = findAttribute(componentXml, "color", nullptr);
            if (fontColorXml) el.color = Page::parseColor(substituteVariables(fontColorXml->value()), { 255, 255, 255, 255 });
            xml_attribute<>* fontSizeXml = findAttribute(componentXml, "fontSize", nullptr);
            if (fontSizeXml) el.fontSize = Utils::convertInt(substituteVariables(fontSizeXml->value()).c_str());
            xml_attribute<>* x = findAttribute(componentXml, "x", nullptr);
            if (x) el.xStr = substituteVariables(x->value());
            xml_attribute<>* y = findAttribute(componentXml, "y", nullptr);
            if (y) el.yStr = substituteVariables(y->value());
            xml_attribute<>* xOffset = findAttribute(componentXml, "xOffset", nullptr);
            if (xOffset) el.xOffsetStr = substituteVariables(xOffset->value());
            xml_attribute<>* yOffset = findAttribute(componentXml, "yOffset", nullptr);
            if (yOffset) el.yOffsetStr = substituteVariables(yOffset->value());
            xml_attribute<>* xOrigin = findAttribute(componentXml, "xOrigin", nullptr);
            if (xOrigin) el.xOriginStr = substituteVariables(xOrigin->value());
            xml_attribute<>* yOrigin = findAttribute(componentXml, "yOrigin", nullptr);
            if (yOrigin) el.yOriginStr = substituteVariables(yOrigin->value());
            xml_attribute<>* alpha = findAttribute(componentXml, "alpha", nullptr);
            if (alpha) el.alpha = Utils::convertFloat(substituteVariables(alpha->value()).c_str());
            el.layer = 2;
            xml_attribute<>* layerAttr = findAttribute(componentXml, "layer", nullptr);
            if (layerAttr) el.layer = Utils::convertInt(substituteVariables(layerAttr->value()).c_str());
            xml_attribute<>* mon = findAttribute(componentXml, "monitor", nullptr);
            if (mon) el.monitor = Utils::convertInt(substituteVariables(mon->value()).c_str());
            page->addLoadingElement(el);
            continue;
        }

        xml_attribute<>* value = componentXml->first_attribute("value");
        xml_attribute<>* idXml = componentXml->first_attribute("id");
        xml_attribute<>* monitorXml = componentXml->first_attribute("monitor");

        int id = -1;
        if (idXml)
        {
            id = Utils::convertInt(substituteVariables(idXml->value()).c_str()); 
        }

        if (!value)
        {
            Logger::write(Logger::ZONE_WARNING, "Layout", "Text component in layout does not specify a value");
        }
        else
        {
            Font* font = addFont(componentXml, NULL);
            int monitor = monitorXml ? Utils::convertInt(substituteVariables(monitorXml->value()).c_str()) : monitor_; 
            Text* c = new Text(substituteVariables(value->value()), *page, font, monitor); 
            c->setId(id);
            xml_attribute<>* menuScrollReload = componentXml->first_attribute("menuScrollReload");
            if (menuScrollReload &&
                (Utils::toLower(substituteVariables(menuScrollReload->value())) == "true" || 
                    Utils::toLower(substituteVariables(menuScrollReload->value())) == "yes"))
            {
                c->setMenuScrollReload(true);
            }
            parseCommonAttributes(componentXml, c);
            buildViewInfo(componentXml, c->baseViewInfo);
            loadTweens(c, componentXml);
            page->addComponent(c);
        }
    }

    for (xml_node<>* componentXml = layout->first_node("statusText"); componentXml; componentXml = componentXml->next_sibling("statusText"))
    {
        xml_attribute<>* activeAttr = componentXml->first_attribute("active");
        if (activeAttr) {
            std::string activeValue = Utils::toLower(substituteVariables(activeAttr->value())); 
            if (activeValue != "true") {
#ifdef _DEBUG
                Logger::write(Logger::ZONE_DEBUG, "Layout", "Skipping inactive or invalid 'active' value for statusText component: " + activeValue);
#endif
                continue;
            }
        }
        xml_attribute<>* monitorXml = componentXml->first_attribute("monitor");
        Font* font = addFont(componentXml, NULL);
        int monitor = monitorXml ? Utils::convertInt(substituteVariables(monitorXml->value()).c_str()) : monitor_; 
        Text* c = new Text("", *page, font, monitor);
        xml_attribute<>* menuScrollReload = componentXml->first_attribute("menuScrollReload");
        if (menuScrollReload &&
            (Utils::toLower(substituteVariables(menuScrollReload->value())) == "true" || 
                Utils::toLower(substituteVariables(menuScrollReload->value())) == "yes"))
        {
            c->setMenuScrollReload(true);
        }
        parseCommonAttributes(componentXml, c);
        buildViewInfo(componentXml, c->baseViewInfo);
        loadTweens(c, componentXml);
        page->addComponent(c);
        page->setStatusTextComponent(c);
    }

    loadReloadableImages(layout, "reloadableImage", page);
    loadReloadableImages(layout, "reloadableAudio", page);
    loadReloadableImages(layout, "reloadableVideo", page);
    loadReloadableImages(layout, "reloadableText", page);
    loadReloadableImages(layout, "reloadableScrollingText", page);

    return true;
}

void PageBuilder::loadReloadableImages(xml_node<>* layout, std::string tagName, Page* page)
{
    for (xml_node<>* componentXml = layout->first_node(tagName.c_str()); componentXml; componentXml = componentXml->next_sibling(tagName.c_str()))
    {
        xml_attribute<>* activeAttr = componentXml->first_attribute("active");
        if (activeAttr) {
            std::string activeValue = Utils::toLower(substituteVariables(activeAttr->value())); 
            if (activeValue != "true") {
#ifdef _DEBUG
                Logger::write(Logger::ZONE_DEBUG, "Layout", "Skipping inactive or invalid 'active' value for reloadableImages component: " + activeValue);
#endif
                continue;
            }
        }
        std::string reloadableImagePath;
        std::string reloadableVideoPath;
        xml_attribute<>* type = componentXml->first_attribute("type");
        xml_attribute<>* imageType = componentXml->first_attribute("imageType");
        xml_attribute<>* mode = componentXml->first_attribute("mode");
        xml_attribute<>* timeFormatXml = componentXml->first_attribute("timeFormat");
        xml_attribute<>* textFormatXml = componentXml->first_attribute("textFormat");
        xml_attribute<>* singlePrefixXml = componentXml->first_attribute("singlePrefix");
        xml_attribute<>* singlePostfixXml = componentXml->first_attribute("singlePostfix");
        xml_attribute<>* pluralPrefixXml = componentXml->first_attribute("pluralPrefix");
        xml_attribute<>* pluralPostfixXml = componentXml->first_attribute("pluralPostfix");
        xml_attribute<>* selectedOffsetXml = componentXml->first_attribute("selectedOffset");
        xml_attribute<>* directionXml = componentXml->first_attribute("direction");
        xml_attribute<>* scrollingSpeedXml = componentXml->first_attribute("scrollingSpeed");
        xml_attribute<>* startPositionXml = componentXml->first_attribute("startPosition");
        xml_attribute<>* startTimeXml = componentXml->first_attribute("startTime");
        xml_attribute<>* endTimeXml = componentXml->first_attribute("endTime");
        xml_attribute<>* alignmentXml = componentXml->first_attribute("alignment");
        xml_attribute<>* idXml = componentXml->first_attribute("id");
        xml_attribute<>* frameLoopXml = componentXml->first_attribute("frameLoop");
        xml_attribute<>* alwaysAnimatedXml = componentXml->first_attribute("isAlwaysAnimated");
        xml_attribute<>* speedXml = componentXml->first_attribute("speed");
        xml_attribute<>* randomXml = componentXml->first_attribute("random");
        bool random = randomXml && (Utils::toLower(substituteVariables(randomXml->value())) == "true" || 
            Utils::toLower(substituteVariables(randomXml->value())) == "yes");
        xml_attribute<>* slideshowXml = componentXml->first_attribute("slideShow");
        bool slideshow = slideshowXml && (Utils::toLower(substituteVariables(slideshowXml->value())) == "true" || 
            Utils::toLower(substituteVariables(slideshowXml->value())) == "yes");
        xml_attribute<>* slideShowTimerXml = componentXml->first_attribute("slideShowTimer");
        int slideShowTimer = slideShowTimerXml ? Utils::convertInt(substituteVariables(slideShowTimerXml->value()).c_str()) : 5000; 
        bool isAlwaysAnimated = alwaysAnimatedXml ? (Utils::toLower(substituteVariables(alwaysAnimatedXml->value())) == "true" || 
            Utils::toLower(substituteVariables(alwaysAnimatedXml->value())) == "yes") : true;
        std::string frameLoop = frameLoopXml ? substituteVariables(frameLoopXml->value()) : "noPlay"; 
        int speed = speedXml ? Utils::convertInt(substituteVariables(speedXml->value()).c_str()) : 7500; 
        bool systemMode = false;
        bool layoutMode = false;
        bool commonMode = false;
        bool menuMode = false;
        int selectedOffset = 0;

        if (!slideShowTimerXml)
        {
            config_.getProperty("slideShowTimer", slideShowTimer);
        }

        int id = -1;
        if (idXml)
        {
            id = Utils::convertInt(substituteVariables(idXml->value()).c_str()); 
        }

        if (!imageType && (tagName == "reloadableVideo" || tagName == "reloadableAudio"))
        {
            Logger::write(Logger::ZONE_WARNING, "Layout", "<reloadableImage> component in layout does not specify an imageType for when the video does not exist");
        }
        if (!type && (tagName == "reloadableImage" || tagName == "reloadableText"))
        {
            Logger::write(Logger::ZONE_ERROR, "Layout", "Image component in layout does not specify a source image file");
        }
        if (!type && tagName == "reloadableScrollingText")
        {
            Logger::write(Logger::ZONE_ERROR, "Layout", "Reloadable scrolling text component in layout does not specify a type");
        }

        if (mode)
        {
            std::string sysMode = substituteVariables(mode->value()); 
            if (sysMode == "system")
            {
                systemMode = true;
            }
            if (sysMode == "layout")
            {
                layoutMode = true;
            }
            if (sysMode == "common")
            {
                commonMode = true;
            }
            if (sysMode == "commonlayout")
            {
                layoutMode = true;
                commonMode = true;
            }
            if (sysMode == "systemlayout")
            {
                systemMode = true;
                layoutMode = true;
            }
            if (sysMode == "menu")
            {
                menuMode = true;
            }
        }

        if (selectedOffsetXml)
        {
            std::stringstream ss;
            ss << substituteVariables(selectedOffsetXml->value()); 
            ss >> selectedOffset;
        }

        Component* c = NULL;

        if (tagName == "reloadableText")
        {
            if (type)
            {
                Font* font = addFont(componentXml, NULL);
                std::string timeFormat;
                if (timeFormatXml && std::string(timeFormatXml->value()).length() > 0)
                {
                    timeFormat = substituteVariables(timeFormatXml->value());
                }
                else
                {
                    // Fallback depending on type
                    std::string t = type ? substituteVariables(type->value()) : "";
                    if (t == "time" || t == "current" || t == "duration")
                        timeFormat = "%H:%M";
                    else if (t == "lastPlayedDate")
                        timeFormat = "%d %b %Y";
                    else
                        timeFormat = ""; // default empty for other fields
                }

                std::string textFormat = "";
                if (textFormatXml)
                {
                    textFormat = substituteVariables(textFormatXml->value()); 
                }
                std::string singlePrefix = "";
                if (singlePrefixXml)
                {
                    singlePrefix = substituteVariables(singlePrefixXml->value()); 
                }
                std::string singlePostfix = "";
                if (singlePostfixXml)
                {
                    singlePostfix = substituteVariables(singlePostfixXml->value()); 
                }
                std::string pluralPrefix = "";
                if (pluralPrefixXml)
                {
                    pluralPrefix = substituteVariables(pluralPrefixXml->value()); 
                }
                std::string pluralPostfix = "";
                if (pluralPostfixXml)
                {
                    pluralPostfix = substituteVariables(pluralPostfixXml->value()); 
                }
                c = new ReloadableText(substituteVariables(type->value()), *page, config_, systemMode, font, layoutKey, timeFormat, textFormat, singlePrefix, singlePostfix, pluralPrefix, pluralPostfix); 
                c->setId(id);
                xml_attribute<>* menuScrollReload = componentXml->first_attribute("menuScrollReload");
                if (menuScrollReload &&
                    (Utils::toLower(substituteVariables(menuScrollReload->value())) == "true" || 
                        Utils::toLower(substituteVariables(menuScrollReload->value())) == "yes"))
                {
                    c->setMenuScrollReload(true);
                }
            }
        }
        else if (tagName == "reloadableScrollingText")
        {
            if (type)
            {
                Font* font = addFont(componentXml, NULL);
                std::string direction = "horizontal";
                std::string textFormat = "";
                std::string timeFormat;
                if (timeFormatXml && std::string(timeFormatXml->value()).length() > 0)
                {
                    timeFormat = substituteVariables(timeFormatXml->value());
                }
                else
                {
                    // Fallback depending on type
                    std::string t = type ? substituteVariables(type->value()) : "";
                    if (t == "time" || t == "current" || t == "duration")
                        timeFormat = "%H:%M";
                    else if (t == "lastPlayedDate")
                        timeFormat = "%d %b %Y";
                    else
                        timeFormat = ""; // default empty for other fields
                }
                if (textFormatXml)
                {
                    textFormat = substituteVariables(textFormatXml->value());
                }
                if (directionXml)
                {
                    direction = substituteVariables(directionXml->value());
                }
                float scrollingSpeed = 1.0f;
                if (scrollingSpeedXml)
                {
                    scrollingSpeed = Utils::convertFloat(substituteVariables(scrollingSpeedXml->value()).c_str());
                }
                float startPosition = 0.0f;
                if (startPositionXml)
                {
                    startPosition = Utils::convertFloat(substituteVariables(startPositionXml->value()).c_str());
                }
                float startTime = 0.0f;
                if (startTimeXml)
                {
                    startTime = Utils::convertFloat(substituteVariables(startTimeXml->value()).c_str());
                }
                float endTime = 0.0f;
                if (endTimeXml)
                {
                    endTime = Utils::convertFloat(substituteVariables(endTimeXml->value()).c_str());
                }
                std::string alignment = "";
                if (alignmentXml)
                {
                    alignment = substituteVariables(alignmentXml->value());
                }
                std::string singlePrefix = "";
                if (singlePrefixXml)
                {
                    singlePrefix = substituteVariables(singlePrefixXml->value());
                }
                std::string singlePostfix = "";
                if (singlePostfixXml)
                {
                    singlePostfix = substituteVariables(singlePostfixXml->value());
                }
                std::string pluralPrefix = "";
                if (pluralPrefixXml)
                {
                    pluralPrefix = substituteVariables(pluralPrefixXml->value());
                }
                std::string pluralPostfix = "";
                if (pluralPostfixXml)
                {
                    pluralPostfix = substituteVariables(pluralPostfixXml->value());
                }
                c = new ReloadableScrollingText(config_, systemMode, layoutMode, menuMode, substituteVariables(type->value()), textFormat, singlePrefix, singlePostfix, pluralPrefix, pluralPostfix, alignment, timeFormat, *page, selectedOffset, font, direction, scrollingSpeed, startPosition, startTime, endTime);
                c->setId(id);
                xml_attribute<>* menuScrollReload = componentXml->first_attribute("menuScrollReload");
                if (menuScrollReload &&
                    (Utils::toLower(substituteVariables(menuScrollReload->value())) == "true" ||
                        Utils::toLower(substituteVariables(menuScrollReload->value())) == "yes"))
                {
                    c->setMenuScrollReload(true);
                }
            }
        }
        else
        {
            xml_attribute<>* jukeboxXml = componentXml->first_attribute("jukebox");
            bool jukebox = false;
            int jukeboxNumLoops = 0;
            if (jukeboxXml &&
                (Utils::toLower(substituteVariables(jukeboxXml->value())) == "true" || 
                    Utils::toLower(substituteVariables(jukeboxXml->value())) == "yes"))
            {
                jukebox = true;
                page->setJukebox();

                xml_attribute<>* numLoopsXml = componentXml->first_attribute("jukeboxNumLoops");
                jukeboxNumLoops = numLoopsXml ? Utils::convertInt(substituteVariables(numLoopsXml->value()).c_str()) : 1; 

            }
            Font* font = addFont(componentXml, NULL);
            std::string typeString = "video";
            std::string imageTypeString = "";
            if (type)
                typeString = substituteVariables(type->value()); 
            if (imageType)
                imageTypeString = substituteVariables(imageType->value()); 
            c = new ReloadableMedia(config_, systemMode, layoutMode, commonMode, menuMode, typeString, imageTypeString, *page, selectedOffset, (tagName == "reloadableVideo") || (tagName == "reloadableAudio"), font, jukebox, jukeboxNumLoops, isAlwaysAnimated, frameLoop, speed, random, slideshow, slideShowTimer);
            c->setId(id);
            xml_attribute<>* menuScrollReload = componentXml->first_attribute("menuScrollReload");
            if (menuScrollReload &&
                (Utils::toLower(substituteVariables(menuScrollReload->value())) == "true" || 
                    Utils::toLower(substituteVariables(menuScrollReload->value())) == "yes"))
            {
                c->setMenuScrollReload(true);
            }
            xml_attribute<>* textFallback = componentXml->first_attribute("textFallback");
            if (textFallback && Utils::toLower(substituteVariables(textFallback->value())) == "true") 
            {
                static_cast<ReloadableMedia*>(c)->enableTextFallback_(true);
            }
            else
            {
                static_cast<ReloadableMedia*>(c)->enableTextFallback_(false);
            }
        }

        if (c)
        {
            parseCommonAttributes(componentXml, c);
            buildViewInfo(componentXml, c->baseViewInfo);
            loadTweens(c, componentXml);
            page->addComponent(c);
        }
    }
}

Font* PageBuilder::addFont(xml_node<>* component, xml_node<>* defaults)
{
    xml_attribute<>* fontXml = component->first_attribute("font");
    xml_attribute<>* fontColorXml = component->first_attribute("fontColor");
    xml_attribute<>* fontSizeXml = component->first_attribute("loadFontSize");
    xml_attribute<>* monitorXml = component->first_attribute("monitor");

    if (defaults)
    {
        if (!fontXml && defaults->first_attribute("font"))
        {
            fontXml = defaults->first_attribute("font");
        }

        if (!fontColorXml && defaults->first_attribute("fontColor"))
        {
            fontColorXml = defaults->first_attribute("fontColor");
        }

        if (!fontSizeXml && defaults->first_attribute("loadFontSize"))
        {
            fontSizeXml = defaults->first_attribute("loadFontSize");
        }
    }

    // use layout defaults unless overridden
    std::string fontName = fontName_;
    SDL_Color fontColor = fontColor_;
    int fontSize = fontSize_;

    if (fontXml)
    {
        fontName = config_.convertToAbsolutePath(
            Utils::combinePath(config_.absolutePath, "layouts", layoutKey, ""),
            substituteVariables(fontXml->value()).c_str()); 

        Logger::write(Logger::ZONE_DEBUG, "Layout", "loading font " + fontName);
    }
    if (fontColorXml)
    {
        int intColor = 0;
        std::stringstream ss;
        ss << std::hex << substituteVariables(fontColorXml->value()); 
        ss >> intColor;

        fontColor.b = intColor & 0xFF;
        intColor >>= 8;
        fontColor.g = intColor & 0xFF;
        intColor >>= 8;
        fontColor.r = intColor & 0xFF;
    }

    if (fontSizeXml)
    {
        fontSize = Utils::convertInt(substituteVariables(fontSizeXml->value()).c_str()); 
    }

    int monitor = monitorXml ? Utils::convertInt(substituteVariables(monitorXml->value()).c_str()) : monitor_; 
    fontCache_->loadFont(fontName, fontSize, fontColor, monitor);

    return fontCache_->getFont(fontName, fontSize, fontColor);
}

void PageBuilder::loadTweens(Component* c, xml_node<>* componentXml)
{
    buildViewInfo(componentXml, c->baseViewInfo);

    c->setTweens(createTweenInstance(componentXml));
}

AnimationEvents* PageBuilder::createTweenInstance(xml_node<>* componentXml)
{
    AnimationEvents* tweens = new AnimationEvents();

    buildTweenSet(tweens, componentXml, "onEnter", "enter");
    buildTweenSet(tweens, componentXml, "onExit", "exit");
    buildTweenSet(tweens, componentXml, "onIdle", "idle");
    buildTweenSet(tweens, componentXml, "onMenuIdle", "menuIdle");
    buildTweenSet(tweens, componentXml, "onMenuScroll", "menuScroll");
    buildTweenSet(tweens, componentXml, "onHighlightEnter", "highlightEnter");
    buildTweenSet(tweens, componentXml, "onHighlightExit", "highlightExit");
    buildTweenSet(tweens, componentXml, "onMenuEnter", "menuEnter");
    buildTweenSet(tweens, componentXml, "onMenuExit", "menuExit");
    buildTweenSet(tweens, componentXml, "onGameEnter", "gameEnter");
    buildTweenSet(tweens, componentXml, "onGameExit", "gameExit");
    buildTweenSet(tweens, componentXml, "onPlaylistEnter", "playlistEnter");
    buildTweenSet(tweens, componentXml, "onPlaylistExit", "playlistExit");
    buildTweenSet(tweens, componentXml, "onMenuJumpEnter", "menuJumpEnter");
    buildTweenSet(tweens, componentXml, "onMenuJumpExit", "menuJumpExit");
    buildTweenSet(tweens, componentXml, "onAttractEnter", "attractEnter");
    buildTweenSet(tweens, componentXml, "onAttract", "attract");
    buildTweenSet(tweens, componentXml, "onAttractExit", "attractExit");
    buildTweenSet(tweens, componentXml, "onJukeboxJump", "jukeboxJump");

    buildTweenSet(tweens, componentXml, "onMenuActionInputEnter", "menuActionInputEnter");
    buildTweenSet(tweens, componentXml, "onMenuActionInputExit", "menuActionInputExit");
    buildTweenSet(tweens, componentXml, "onMenuActionSelectEnter", "menuActionSelectEnter");
    buildTweenSet(tweens, componentXml, "onMenuActionSelectExit", "menuActionSelectExit");

    return tweens;
}

void PageBuilder::buildTweenSet(AnimationEvents* tweens, xml_node<>* componentXml, std::string tagName, std::string tweenName)
{
    for (componentXml = componentXml->first_node(tagName.c_str()); componentXml; componentXml = componentXml->next_sibling(tagName.c_str()))
    {
        xml_attribute<>* indexXml = componentXml->first_attribute("menuIndex");

        if (indexXml)
        {
            std::string indexs = substituteVariables(indexXml->value()); 
            if (indexs[0] == '!')
            {
                indexs.erase(0, 1);
                int index = Utils::convertInt(indexs.c_str()); // Modified: Use .c_str() for compatibility
                for (int i = 0; i < MENU_INDEX_HIGH - 1; i++)
                {
                    if (i != index)
                    {
                        Animation* animation = new Animation();
                        getTweenSet(componentXml, animation);
                        tweens->setAnimation(tweenName, i, animation);
                    }
                }
            }
            else if (indexs[0] == '<')
            {
                indexs.erase(0, 1);
                int index = Utils::convertInt(indexs.c_str()); // Modified: Use .c_str() for compatibility
                for (int i = 0; i < MENU_INDEX_HIGH - 1; i++)
                {
                    if (i < index)
                    {
                        Animation* animation = new Animation();
                        getTweenSet(componentXml, animation);
                        tweens->setAnimation(tweenName, i, animation);
                    }
                }
            }
            else if (indexs[0] == '>')
            {
                indexs.erase(0, 1);
                int index = Utils::convertInt(indexs.c_str()); // Modified: Use .c_str() for compatibility
                for (int i = 0; i < MENU_INDEX_HIGH - 1; i++)
                {
                    if (i > index)
                    {
                        Animation* animation = new Animation();
                        getTweenSet(componentXml, animation);
                        tweens->setAnimation(tweenName, i, animation);
                    }
                }
            }
            else if (indexs[0] == 'i')
            {
                Animation* animation = new Animation();
                getTweenSet(componentXml, animation);
                tweens->setAnimation(tweenName, MENU_INDEX_HIGH, animation);
            }
            else
            {
                int index = Utils::convertInt(substituteVariables(indexXml->value()).c_str());  
                Animation* animation = new Animation();
                getTweenSet(componentXml, animation);
                tweens->setAnimation(tweenName, index, animation);
            }
        }
        else
        {
            Animation* animation = new Animation();
            getTweenSet(componentXml, animation);
            tweens->setAnimation(tweenName, -1, animation);
#ifdef _DEBUG
            Logger::write(Logger::ZONE_INFO, "PageBuilder", "Loaded " + tagName + " tween with no menuIndex");
#endif
        }
    }
}


ScrollingList* PageBuilder::buildMenu(xml_node<>* menuXml, Page& page)
{
    ScrollingList* menu = NULL;
    std::string menuType = "vertical";
    std::string imageType = "null";
    std::string videoType = "null";
    xml_node<>* itemDefaults = menuXml->first_node("itemDefaults");
    xml_attribute<>* modeXml = menuXml->first_attribute("mode");
    xml_attribute<>* imageTypeXml = menuXml->first_attribute("imageType");
    xml_attribute<>* videoTypeXml = menuXml->first_attribute("videoType");
    xml_attribute<>* menuTypeXml = menuXml->first_attribute("type");
    xml_attribute<>* scrollTimeXml = menuXml->first_attribute("scrollTime");
    xml_attribute<>* scrollAccelerationXml = menuXml->first_attribute("scrollAcceleration");
    xml_attribute<>* minScrollTimeXml = menuXml->first_attribute("minScrollTime");
    xml_attribute<>* scrollOrientationXml = menuXml->first_attribute("orientation");
    xml_attribute<>* textFallbackXml = menuXml->first_attribute("textFallback");
    xml_attribute<>* randomXml = menuXml->first_attribute("random");
    xml_attribute<>* slideShowXml = menuXml->first_attribute("slideShow");
    xml_attribute<>* slideShowTimerXml = menuXml->first_attribute("slideShowTimer");
    xml_attribute<>* selectedItemXml = menuXml->first_attribute("selectedItem"); 

    bool textFallback = true;                       
    if (textFallbackXml)
    {
        std::string val = Utils::toLower(substituteVariables(textFallbackXml->value()));
        textFallback = (val == "true" || val == "yes");
    }

    bool random = randomXml && (Utils::toLower(substituteVariables(randomXml->value())) == "true" ||
        Utils::toLower(substituteVariables(randomXml->value())) == "yes");
    bool slideShow = slideShowXml && (Utils::toLower(substituteVariables(slideShowXml->value())) == "true" ||
        Utils::toLower(substituteVariables(slideShowXml->value())) == "yes");
    int slideShowTimer = slideShowTimerXml ? Utils::convertInt(substituteVariables(slideShowTimerXml->value()).c_str()) : 5000;

    if (!slideShowTimerXml)
    {
        config_.getProperty("slideShowTimer", slideShowTimer);
    }

    if (menuTypeXml)
    {
        menuType = substituteVariables(menuTypeXml->value());
    }

    if (!itemDefaults)
    {
        Logger::write(Logger::ZONE_WARNING, "Layout", "Menu tag is missing <itemDefaults> tag.");
    }

    if (imageTypeXml)
    {
        imageType = substituteVariables(imageTypeXml->value());
    }

    if (videoTypeXml)
    {
        videoType = substituteVariables(videoTypeXml->value());
    }

    bool layoutMode = false;
    bool commonMode = false;
    if (modeXml)
    {
        std::string sysMode = substituteVariables(modeXml->value());
        if (sysMode == "layout") layoutMode = true;
        if (sysMode == "common") commonMode = true;
        if (sysMode == "commonlayout") { layoutMode = true; commonMode = true; }
    }

    Font* font = addFont(itemDefaults, NULL);

    // === Parse scrolling text options (priority: item > itemDefaults > menu) ===
    xml_attribute<>* textTypeXml = findAttribute(menuXml, "textType", itemDefaults);
    xml_attribute<>* textFormatXml = findAttribute(menuXml, "textFormat", itemDefaults);
    xml_attribute<>* singlePrefixXml = findAttribute(menuXml, "singlePrefix", itemDefaults);
    xml_attribute<>* singlePostfixXml = findAttribute(menuXml, "singlePostfix", itemDefaults);
    xml_attribute<>* pluralPrefixXml = findAttribute(menuXml, "pluralPrefix", itemDefaults);
    xml_attribute<>* pluralPostfixXml = findAttribute(menuXml, "pluralPostfix", itemDefaults);
    xml_attribute<>* alignmentXml = findAttribute(menuXml, "alignment", itemDefaults);
    xml_attribute<>* directionXml = findAttribute(menuXml, "direction", itemDefaults);
    xml_attribute<>* scrollingSpeedXml = findAttribute(menuXml, "scrollingSpeed", itemDefaults);
    xml_attribute<>* startPositionXml = findAttribute(menuXml, "startPosition", itemDefaults);
    xml_attribute<>* startTimeXml = findAttribute(menuXml, "startTime", itemDefaults);
    xml_attribute<>* endTimeXml = findAttribute(menuXml, "endTime", itemDefaults);
    xml_attribute<>* timeFormatXml = findAttribute(menuXml, "timeFormat", itemDefaults);

    std::string textType = textTypeXml ? substituteVariables(textTypeXml->value()) : "";
    std::string textFormat = textFormatXml ? substituteVariables(textFormatXml->value()) : "";
    std::string singlePrefix = singlePrefixXml ? substituteVariables(singlePrefixXml->value()) : "";
    std::string singlePostfix = singlePostfixXml ? substituteVariables(singlePostfixXml->value()) : "";
    std::string pluralPrefix = pluralPrefixXml ? substituteVariables(pluralPrefixXml->value()) : "";
    std::string pluralPostfix = pluralPostfixXml ? substituteVariables(pluralPostfixXml->value()) : "";
    std::string alignment = alignmentXml ? substituteVariables(alignmentXml->value()) : "";
    std::string direction = directionXml ? substituteVariables(directionXml->value()) : "horizontal";
    float scrollingSpeed = scrollingSpeedXml ? Utils::convertFloat(substituteVariables(scrollingSpeedXml->value()).c_str()) : 50.0f;
    float startPosition = startPositionXml ? Utils::convertFloat(substituteVariables(startPositionXml->value()).c_str()) : 0.0f;
    float startTime = startTimeXml ? Utils::convertFloat(substituteVariables(startTimeXml->value()).c_str()) : 1.0f;
    float endTime = endTimeXml ? Utils::convertFloat(substituteVariables(endTimeXml->value()).c_str()) : 1.0f;
    std::string timeFormat = timeFormatXml ? substituteVariables(timeFormatXml->value()) : "%H:%M";

    bool scrollingTextEnabled = !textType.empty() || directionXml || scrollingSpeedXml;

    menu = new ScrollingList(config_, page, layoutMode, commonMode, font, layoutKey, imageType, videoType, random, slideShow, slideShowTimer,
        textType, textFormat, singlePrefix, singlePostfix, pluralPrefix, pluralPostfix,
        alignment, direction, scrollingSpeed, startPosition, startTime, endTime, timeFormat, scrollingTextEnabled, textFallback);

    parseCommonAttributes(menuXml, menu);
    buildViewInfo(menuXml, menu->baseViewInfo);

    if (scrollTimeXml)
    {
        menu->setStartScrollTime(Utils::convertFloat(substituteVariables(scrollTimeXml->value()).c_str()));
    }

    if (scrollAccelerationXml)
    {
        float accel = Utils::convertFloat(substituteVariables(scrollAccelerationXml->value()).c_str());
        menu->setScrollAcceleration(accel);
        menu->setMinScrollTime(accel); // Default minScrollTime to acceleration if not specified
    }

    if (minScrollTimeXml)
    {
        menu->setMinScrollTime(Utils::convertFloat(substituteVariables(minScrollTimeXml->value()).c_str()));
    }

    // Handle orientation, including new grid type
    std::string orientation = "vertical";
    if (scrollOrientationXml)
    {
        orientation = substituteVariables(scrollOrientationXml->value());
        if (orientation == "horizontal")
        {
            menu->horizontalScroll = true;
        }
        else if (orientation == "Grid")
        {
            menu->isGrid = true;
            xml_attribute<>* rowsXml = menuXml->first_attribute("rows");
            xml_attribute<>* columnsXml = menuXml->first_attribute("columns");
            xml_attribute<>* cellWidthXml = menuXml->first_attribute("cellWidth");
            xml_attribute<>* cellHeightXml = menuXml->first_attribute("cellHeight");
            xml_attribute<>* spacingXXml = menuXml->first_attribute("spacingX");
            xml_attribute<>* spacingYXml = menuXml->first_attribute("spacingY");
            xml_attribute<>* wrapXml = menuXml->first_attribute("wrap");
            xml_attribute<>* selectedZoomXml = menuXml->first_attribute("selectedZoom");
            xml_attribute<>* alphaUnselectedXml = menuXml->first_attribute("alphaUnselected");

            int rows = rowsXml ? Utils::convertInt(substituteVariables(rowsXml->value()).c_str()) : 1;
            int columns = columnsXml ? Utils::convertInt(substituteVariables(columnsXml->value()).c_str()) : 1;
            float cellWidth = cellWidthXml ? Utils::convertFloat(substituteVariables(cellWidthXml->value()).c_str()) : 100;
            float cellHeight = cellHeightXml ? Utils::convertFloat(substituteVariables(cellHeightXml->value()).c_str()) : 100;
            float spacingX = spacingXXml ? Utils::convertFloat(substituteVariables(spacingXXml->value()).c_str()) : 0;
            float spacingY = spacingYXml ? Utils::convertFloat(substituteVariables(spacingYXml->value()).c_str()) : 0;
            bool wrap = wrapXml && (Utils::toLower(substituteVariables(wrapXml->value())) == "true");

            // Validate grid attributes
            if (rows <= 0 || columns <= 0)
            {
                Logger::write(Logger::ZONE_ERROR, "Layout", "Grid menu requires positive 'rows' and 'columns' values.");
                menu->isGrid = false;
            }
            else
            {
                menu->rows = rows;
                menu->columns = columns;
                menu->cellWidth = cellWidth;
                menu->cellHeight = cellHeight;
                menu->spacingX = spacingX;
                menu->spacingY = spacingY;
                menu->wrap = wrap;

                // Generate scrollPoints for the grid only if there are no explicit <item> tags
                // (if <item> tags exist, buildCustomMenu will build them from the XML)
                if (!menuXml->first_node("item"))
                {
                    std::vector<ViewInfo*>* scrollPoints = new std::vector<ViewInfo*>();
                    std::vector<AnimationEvents*>* tweenPoints = new std::vector<AnimationEvents*>();
                    float startX = menu->baseViewInfo.X;
                    float startY = menu->baseViewInfo.Y;

                    for (int r = 0; r < rows; r++)
                    {
                        for (int c = 0; c < columns; c++)
                        {
                            ViewInfo* point = new ViewInfo();
                            point->X = startX + (c * (cellWidth + spacingX));
                            point->Y = startY + (r * (cellHeight + spacingY));
                            point->Width = cellWidth;
                            point->Height = cellHeight;
                            buildViewInfo(itemDefaults, *point, itemDefaults);
                            point->selectedZoom = selectedZoomXml ? Utils::convertFloat(substituteVariables(selectedZoomXml->value()).c_str()) : 1.3f;
                            point->alphaUnselected = alphaUnselectedXml ? Utils::convertFloat(substituteVariables(alphaUnselectedXml->value()).c_str()) : 0.5f;
                            scrollPoints->push_back(point);
                            tweenPoints->push_back(nullptr); // No animations for now
                        }
                    }

                    menu->setPoints(scrollPoints, tweenPoints);
                }

                // Parse selectedItem (e.g., "2x3")
                if (selectedItemXml)
                {
                    std::string selectedItem = substituteVariables(selectedItemXml->value());
                    int row = 1, col = 1;
                    size_t xPos = selectedItem.find('x');
                    if (xPos != std::string::npos)
                    {
                        row = Utils::convertInt(selectedItem.substr(0, xPos).c_str());
                        col = Utils::convertInt(selectedItem.substr(xPos + 1).c_str());
                    }
                    if (row > 0 && col > 0 && row <= rows && col <= columns)
                    {
                        int index = (row - 1) * columns + (col - 1);
                        menu->setSelectedIndex(index);
                    }
                    else
                    {
                        Logger::write(Logger::ZONE_WARNING, "Layout", "Invalid selectedItem format or out of bounds: " + selectedItem);
                    }
                }
            }
        }
    }

    buildViewInfo(menuXml, menu->baseViewInfo);

    if (menuType == "custom" || menuXml->first_node("item"))
    {
        buildCustomMenu(menu, menuXml, itemDefaults);
    }
    else if (!menu->isGrid)
    {
        buildVerticalMenu(menu, menuXml, itemDefaults);
    }

    loadTweens(menu, menuXml);

    return menu;
}


void PageBuilder::buildCustomMenu(ScrollingList* menu, xml_node<>* menuXml, xml_node<>* itemDefaults)
{
    std::vector<ViewInfo*>* points = new std::vector<ViewInfo*>();
    std::vector<AnimationEvents*>* tweenPoints = new std::vector<AnimationEvents*>();

    int i = 0;
    for (xml_node<>* componentXml = menuXml->first_node("item"); componentXml; componentXml = componentXml->next_sibling("item"))
    {
        xml_attribute<>* activeAttr = componentXml->first_attribute("active");
        if (activeAttr)
        {
            std::string activeValue = Utils::toLower(substituteVariables(activeAttr->value()));
            if (activeValue != "true")
            {
#ifdef _DEBUG
                Logger::write(Logger::ZONE_DEBUG, "Layout", "Skipping inactive or invalid 'active' value for item component: " + activeValue);
#endif
                continue;
            }
        }

        ViewInfo* viewInfo = new ViewInfo();
        buildViewInfo(componentXml, *viewInfo, itemDefaults, &menu->baseViewInfo);
        viewInfo->Monitor = menu->baseViewInfo.Monitor;

        // Override position for grid menus
        if (menu->isGrid)
        {
            viewInfo->selectedZoom = menu->baseViewInfo.selectedZoom;
            // Optionally, apply alphaUnselected too if needed
            viewInfo->alphaUnselected = menu->baseViewInfo.alphaUnselected;
        }
        if (menu->isGrid)
        {
            int row = i / menu->columns;
            int col = i % menu->columns;
            viewInfo->XOffset = col * (menu->cellWidth + menu->spacingX);
            viewInfo->YOffset = row * (menu->cellHeight + menu->spacingY);

            // Ensure grid size matches item count (optional strictness)
            int expectedItems = menu->rows * menu->columns;
            if (i >= expectedItems)
            {
                Logger::write(Logger::ZONE_WARNING, "Layout", "Number of <item> tags exceeds grid size (" +
                    std::to_string(menu->rows) + "x" + std::to_string(menu->columns) + ").");
                break; // Stop adding items beyond grid capacity
            }
        }

        points->push_back(viewInfo);
        tweenPoints->push_back(createTweenInstance(componentXml));

        // Handle selected attribute
        xml_attribute<>* selected = componentXml->first_attribute("selected");
        if (selected)
        {
            std::string selectedValue = substituteVariables(selected->value());
            if (menu->isGrid && selectedValue.find('x') != std::string::npos)
            {
                // Parse "rowxcol" format, e.g., "1x2"
                size_t xPos = selectedValue.find('x');
                int row = Utils::convertInt(selectedValue.substr(0, xPos).c_str());
                int col = Utils::convertInt(selectedValue.substr(xPos + 1).c_str());
                int selectedIndex = row * menu->columns + col;
                if (selectedIndex >= 0 && selectedIndex < menu->rows * menu->columns)
                {
                    menu->setSelectedIndex(selectedIndex);
                }
                else
                {
                    Logger::write(Logger::ZONE_WARNING, "Layout", "Selected index " + selectedValue + " out of grid bounds.");
                }
            }
            else if (Utils::toLower(selectedValue) == "true")
            {
                menu->setSelectedIndex(i);
            }
        }

        i++;
    }

    // For grid menus, ensure we have enough points
    if (menu->isGrid)
    {
        int expectedItems = menu->rows * menu->columns;
        if (i < expectedItems)
        {
            Logger::write(Logger::ZONE_WARNING, "Layout", "Fewer <item> tags (" + std::to_string(i) +
                ") than grid size (" + std::to_string(expectedItems) + "); remaining cells will be empty.");
            // Optionally, pad with empty ViewInfo objects if desired
        }
    }

    menu->setPoints(points, tweenPoints);
}

void PageBuilder::buildVerticalMenu(ScrollingList* menu, xml_node<>* menuXml, xml_node<>* itemDefaults)
{
    std::vector<ViewInfo*>* points = new std::vector<ViewInfo*>();
    std::vector<AnimationEvents*>* tweenPoints = new std::vector<AnimationEvents*>();

    int selectedIndex = MENU_FIRST;
    std::map<int, xml_node<>*> overrideItems;

    // By default the menu will automatically determine the offsets for list items.
   
    for (xml_node<>* componentXml = menuXml->first_node("item"); componentXml; componentXml = componentXml->next_sibling("item"))
    {
        xml_attribute<>* activeAttr = componentXml->first_attribute("active");
        if (activeAttr) {
            std::string activeValue = Utils::toLower(substituteVariables(activeAttr->value())); 
            if (activeValue != "true") {
#ifdef _DEBUG
                Logger::write(Logger::ZONE_DEBUG, "Layout", "Skipping inactive or invalid 'active' value for verticalMenu component: " + activeValue);
#endif
                continue;
            }
        }
        xml_attribute<>* xmlIndex = componentXml->first_attribute("index");

        if (xmlIndex)
        {
            int itemIndex = parseMenuPosition(substituteVariables(xmlIndex->value())); 
            overrideItems[itemIndex] = componentXml;

            // check to see if the item specified is the selected index
            xml_attribute<>* xmlSelectedIndex = componentXml->first_attribute("selected");

            if (xmlSelectedIndex && Utils::toLower(substituteVariables(xmlSelectedIndex->value())) == "true")  
            {
                selectedIndex = itemIndex;
            }
        }
    }

    bool end = false;

    
    float height = 0;
    int index = 0;

    if (overrideItems.find(MENU_START) != overrideItems.end())
    {
        xml_node<>* component = overrideItems[MENU_START];

        ViewInfo* viewInfo = createMenuItemInfo(component, itemDefaults, menu->baseViewInfo.Y + height);
        points->push_back(viewInfo);
        tweenPoints->push_back(createTweenInstance(component));
        height += viewInfo->Height;

        // increment the selected index to account for the new "invisible" menu item
        selectedIndex++;
    }
    while (!end)
    {
        ViewInfo* viewInfo = new ViewInfo();
        xml_node<>* component = itemDefaults;

        // use overridden item setting if specified by layout for the given index
        if (overrideItems.find(index) != overrideItems.end())
        {
            component = overrideItems[index];
        }

        // calculate the total height of menu items if can load any additional items
        buildViewInfo(component, *viewInfo, itemDefaults, &menu->baseViewInfo); 
        xml_attribute<>* itemSpacingXml = component->first_attribute("spacing");
        int itemSpacing = itemSpacingXml ? Utils::convertInt(substituteVariables(itemSpacingXml->value()).c_str()) : 0; 
        float nextHeight = height + viewInfo->Height + itemSpacing;

        if (nextHeight >= menu->baseViewInfo.Height)
        {
            end = true;
        }

        // reached the last menuitem
        if (end && overrideItems.find(MENU_LAST) != overrideItems.end())
        {
            component = overrideItems[MENU_LAST];

            buildViewInfo(component, *viewInfo, itemDefaults);
            xml_attribute<>* itemSpacingXml = component->first_attribute("spacing");
            int itemSpacing = itemSpacingXml ? Utils::convertInt(substituteVariables(itemSpacingXml->value()).c_str()) : 0; 
            nextHeight = height + viewInfo->Height + itemSpacing;
        }

        viewInfo->Y = menu->baseViewInfo.Y + (float)height;
        points->push_back(viewInfo);
        tweenPoints->push_back(createTweenInstance(component)); 
        index++;
        height = nextHeight;
    }

    //menu end
    if (overrideItems.find(MENU_END) != overrideItems.end())
    {
        xml_node<>* component = overrideItems[MENU_END];
        ViewInfo* viewInfo = createMenuItemInfo(component, itemDefaults, menu->baseViewInfo.Y + height);
        points->push_back(viewInfo);
        tweenPoints->push_back(createTweenInstance(component));
    }

    if (selectedIndex >= ((int)points->size()))
    {
        std::stringstream ss;

        ss << "Design error! Selected menu item was set to " << selectedIndex
            << " although there are only " << points->size()
            << " menu points that can be displayed";

        Logger::write(Logger::ZONE_ERROR, "Layout", "Design error! \"duration\" attribute");

        selectedIndex = 0;
    }

    menu->setSelectedIndex(selectedIndex);
    menu->setPoints(points, tweenPoints);
}

ViewInfo* PageBuilder::createMenuItemInfo(xml_node<>* component, xml_node<>* defaults, float y)
{
    ViewInfo* viewInfo = new ViewInfo();
    buildViewInfo(component, *viewInfo, defaults);
    viewInfo->Y = y;
    return viewInfo;
}

int PageBuilder::parseMenuPosition(std::string strIndex)
{
    int index = MENU_FIRST;

    strIndex = substituteVariables(strIndex); 

    if (strIndex == "end")
    {
        index = MENU_END;
    }
    else if (strIndex == "last")
    {
        index = MENU_LAST;
    }
    else if (strIndex == "start")
    {
        index = MENU_START;
    }
    else if (strIndex == "first")
    {
        index = MENU_FIRST;
    }
    else
    {
        index = Utils::convertInt(strIndex.c_str()); // Modified: Use .c_str() for compatibility
    }
    return index;
}

xml_attribute<>* PageBuilder::findAttribute(xml_node<>* componentXml, std::string attribute, xml_node<>* defaultXml = NULL)
{
    xml_attribute<>* attributeXml = componentXml->first_attribute(attribute.c_str());

    if (!attributeXml && defaultXml)
    {
        attributeXml = defaultXml->first_attribute(attribute.c_str());
    }

    return attributeXml;
}

void PageBuilder::buildViewInfo(rapidxml::xml_node<>* componentXml, ViewInfo& info, rapidxml::xml_node<>* defaultXml, const ViewInfo* parentViewInfo)
{
    xml_attribute<>* x = findAttribute(componentXml, "x", defaultXml);
    xml_attribute<>* y = findAttribute(componentXml, "y", defaultXml);
    xml_attribute<>* xOffset = findAttribute(componentXml, "xOffset", defaultXml);
    xml_attribute<>* yOffset = findAttribute(componentXml, "yOffset", defaultXml);
    xml_attribute<>* xOrigin = findAttribute(componentXml, "xOrigin", defaultXml);
    xml_attribute<>* yOrigin = findAttribute(componentXml, "yOrigin", defaultXml);
    xml_attribute<>* height = findAttribute(componentXml, "height", defaultXml);
    xml_attribute<>* width = findAttribute(componentXml, "width", defaultXml);
    xml_attribute<>* fontSize = findAttribute(componentXml, "fontSize", defaultXml);
    xml_attribute<>* fontColor = findAttribute(componentXml, "fontColor", defaultXml);
    xml_attribute<>* minHeight = findAttribute(componentXml, "minHeight", defaultXml);
    xml_attribute<>* minWidth = findAttribute(componentXml, "minWidth", defaultXml);
    xml_attribute<>* maxHeight = findAttribute(componentXml, "maxHeight", defaultXml);
    xml_attribute<>* maxWidth = findAttribute(componentXml, "maxWidth", defaultXml);
    xml_attribute<>* alpha = findAttribute(componentXml, "alpha", defaultXml);
    xml_attribute<>* angle = findAttribute(componentXml, "angle", defaultXml);
    xml_attribute<>* layer = findAttribute(componentXml, "layer", defaultXml);
    xml_attribute<>* backgroundColor = findAttribute(componentXml, "backgroundColor", defaultXml);
    xml_attribute<>* backgroundAlpha = findAttribute(componentXml, "backgroundAlpha", defaultXml);
    xml_attribute<>* reflection = findAttribute(componentXml, "reflection", defaultXml);
    xml_attribute<>* reflectionDistance = findAttribute(componentXml, "reflectionDistance", defaultXml);
    xml_attribute<>* reflectionScale = findAttribute(componentXml, "reflectionScale", defaultXml);
    xml_attribute<>* reflectionAlpha = findAttribute(componentXml, "reflectionAlpha", defaultXml);
    xml_attribute<>* containerX = findAttribute(componentXml, "containerX", defaultXml);
    xml_attribute<>* containerY = findAttribute(componentXml, "containerY", defaultXml);
    xml_attribute<>* containerWidth = findAttribute(componentXml, "containerWidth", defaultXml);
    xml_attribute<>* containerHeight = findAttribute(componentXml, "containerHeight", defaultXml);
    xml_attribute<>* monitor = findAttribute(componentXml, "monitor", defaultXml);
    xml_attribute<>* volume = findAttribute(componentXml, "volume", defaultXml);
    xml_attribute<>* scaleModeAttr = findAttribute(componentXml, "scaleMode", defaultXml);
    xml_attribute<>* borderColorAttr = findAttribute(componentXml, "borderColor", defaultXml);
    xml_attribute<>* selectedBorderColorAttr = findAttribute(componentXml, "selectedBorderColor", defaultXml); 
    xml_attribute<>* borderSpaceAttr = findAttribute(componentXml, "borderSpace", defaultXml);
    xml_attribute<>* borderThicknessAttr = findAttribute(componentXml, "borderThickness", defaultXml);
    xml_attribute<>* frameLoopAttr = findAttribute(componentXml, "frameLoop", defaultXml);
    xml_attribute<>* speedAttr = findAttribute(componentXml, "speed", defaultXml);
    xml_attribute<>* selectedOnlyAnimationAttr = findAttribute(componentXml, "selectedOnlyAnimation", defaultXml);
    xml_attribute<>* selectedZoomAttr = findAttribute(componentXml, "selectedZoom", defaultXml);
    xml_attribute<>* alphaUnselectedAttr = findAttribute(componentXml, "alphaUnselected", defaultXml);
    xml_attribute<>* angleYAttr = findAttribute(componentXml, "flip3DY", defaultXml);
    xml_attribute<>* angleXAttr = findAttribute(componentXml, "flip3DX", defaultXml);
	xml_attribute<>* slice3DXAttr = findAttribute(componentXml, "roller3DX", defaultXml);
	xml_attribute<>* slice3DYAttr = findAttribute(componentXml, "roller3DY", defaultXml);
	xml_attribute<>* wrapLXAttr = findAttribute(componentXml, "wrapLX", defaultXml);
	xml_attribute<>* wrapLYAttr = findAttribute(componentXml, "wrapLY", defaultXml);
	xml_attribute<>* wrapRXAttr = findAttribute(componentXml, "wrapRX", defaultXml);
	xml_attribute<>* wrapRYAttr = findAttribute(componentXml, "wrapRY", defaultXml);
	xml_attribute<>* wrapTXAttr = findAttribute(componentXml, "wrapTX", defaultXml);
	xml_attribute<>* wrapTYAttr = findAttribute(componentXml, "wrapTY", defaultXml);
	xml_attribute<>* wrapBXAttr = findAttribute(componentXml, "wrapBX", defaultXml);
	xml_attribute<>* wrapBYAttr = findAttribute(componentXml, "wrapBY", defaultXml);

    xml_attribute<>* shapeAttr = findAttribute(componentXml, "shape", defaultXml);
    xml_attribute<>* curvePosAttr = findAttribute(componentXml, "curvePosition", defaultXml);
    if (!curvePosAttr) curvePosAttr = findAttribute(componentXml, "curveSide", defaultXml);
    xml_attribute<>* curveModeAttr = findAttribute(componentXml, "curveMode", defaultXml);
    xml_attribute<>* curveDepthAttr = findAttribute(componentXml, "curveDepth", defaultXml);
    xml_attribute<>* radiusAttr = findAttribute(componentXml, "radius", defaultXml);
    xml_attribute<>* thicknessAttr = findAttribute(componentXml, "thickness", defaultXml);
    xml_attribute<>* startAngleAttr = findAttribute(componentXml, "startAngle", defaultXml);
    xml_attribute<>* endAngleAttr = findAttribute(componentXml, "endAngle", defaultXml);

    xml_attribute<>* autoFitAttr = findAttribute(componentXml, "autoFit", defaultXml);
    xml_attribute<>* fitWidthAttr = findAttribute(componentXml, "fitToWidth", defaultXml);
    xml_attribute<>* fitHeightAttr = findAttribute(componentXml, "fitToHeight", defaultXml);
    xml_attribute<>* minFontSizeAttr = findAttribute(componentXml, "minFontSize", defaultXml);
    xml_attribute<>* maxFontSizeAttr = findAttribute(componentXml, "maxFontSize", defaultXml);

    if (selectedOnlyAnimationAttr) {
        std::string value = Utils::toLower(substituteVariables(selectedOnlyAnimationAttr->value()));
        info.selectedOnlyAnimation = (value == "true" || value == "yes");
#ifdef _DEBUG
        Logger::write(Logger::ZONE_DEBUG, "PageBuilder", "Set selectedOnlyAnimation: " + std::to_string(info.selectedOnlyAnimation));
#endif
    }
  

    if (scaleModeAttr) {
        std::string modeStr = Utils::toLower(substituteVariables(scaleModeAttr->value()));
        if (modeStr == "fit") {
            info.scaleMode = ViewInfo::ScaleMode::Fit;
        }
        else if (modeStr == "fill") {
            info.scaleMode = ViewInfo::ScaleMode::Fill;
        }
        else if (modeStr == "none") {
            info.scaleMode = ViewInfo::ScaleMode::None;
        }
        else if (modeStr == "stretch") {
            info.scaleMode = ViewInfo::ScaleMode::Stretch;
        }
        else {
            Logger::write(Logger::ZONE_WARNING, "PageBuilder", "Unknown scaleMode '" + modeStr + "', defaulting to Stretch");
            info.scaleMode = ViewInfo::ScaleMode::Stretch;
        }
    }
    else if (parentViewInfo) {
        info.scaleMode = parentViewInfo->scaleMode;
    }
    else {
        info.scaleMode = ViewInfo::ScaleMode::Stretch;
    }

    info.X = getHorizontalAlignment(x, 0);
    info.Y = getVerticalAlignment(y, 0);
    info.XOffset = getHorizontalAlignment(xOffset, 0);
    info.YOffset = getVerticalAlignment(yOffset, 0);
    if (xOrigin)
    {
        float xOriginRelative = getHorizontalAlignment(xOrigin, 0);
        info.XOrigin = xOriginRelative / layoutWidth_;
    }
    else if (parentViewInfo)
    {
        info.XOrigin = parentViewInfo->XOrigin;
    }
    else
    {
        info.XOrigin = 0.0f;
    }

    if (yOrigin)
    {
        float yOriginRelative = getVerticalAlignment(yOrigin, 0);
        info.YOrigin = yOriginRelative / layoutHeight_;
    }
    else if (parentViewInfo)
    {
        info.YOrigin = parentViewInfo->YOrigin;
    }
    else
    {
        info.YOrigin = 0.0f;
    }

    if (!height && !width)
    {
        info.Height = -1;
        info.Width = -1;
    }
    else
    {
        info.Height = getVerticalAlignment(height, -1);
        info.Width = getHorizontalAlignment(width, -1);
    }
    info.FontSize = getVerticalAlignment(fontSize, -1);
    info.MinHeight = getVerticalAlignment(minHeight, 0);
    info.MinWidth = getHorizontalAlignment(minWidth, 0);
    info.MaxHeight = getVerticalAlignment(maxHeight, FLT_MAX);
    info.MaxWidth = getVerticalAlignment(maxWidth, FLT_MAX);
    info.Alpha = alpha ? Utils::convertFloat(substituteVariables(alpha->value()).c_str()) : 1.f;
    info.Angle = angle ? Utils::convertFloat(substituteVariables(angle->value()).c_str()) : 0.f;
    info.Layer = layer ? Utils::convertInt(substituteVariables(layer->value()).c_str()) : 0;
    info.Reflection = reflection ? substituteVariables(reflection->value()) : "";
    info.ReflectionDistance = reflectionDistance ? Utils::convertInt(substituteVariables(reflectionDistance->value()).c_str()) : 0;
    info.ReflectionScale = reflectionScale ? Utils::convertFloat(substituteVariables(reflectionScale->value()).c_str()) : 0.25f;
    info.ReflectionAlpha = reflectionAlpha ? Utils::convertFloat(substituteVariables(reflectionAlpha->value()).c_str()) : 1.f;
    info.ContainerX = containerX ? Utils::convertFloat(substituteVariables(containerX->value()).c_str()) : 0.f;
    info.ContainerY = containerY ? Utils::convertFloat(substituteVariables(containerY->value()).c_str()) : 0.f;
    info.ContainerWidth = containerWidth ? Utils::convertFloat(substituteVariables(containerWidth->value()).c_str()) : -1.f;
    info.ContainerHeight = containerHeight ? Utils::convertFloat(substituteVariables(containerHeight->value()).c_str()) : -1.f;
    info.Monitor = monitor ? Utils::convertInt(substituteVariables(monitor->value()).c_str()) : 0;
    info.Volume = volume ? Utils::convertFloat(substituteVariables(volume->value()).c_str()) : 1.f;
    info.selectedZoom = selectedZoomAttr ? Utils::convertFloat(substituteVariables(selectedZoomAttr->value()).c_str()) : 1.3f;
    info.alphaUnselected = alphaUnselectedAttr ? Utils::convertFloat(substituteVariables(alphaUnselectedAttr->value()).c_str()) : 0.5f;
    info.AngleY = angleYAttr ? Utils::convertFloat(substituteVariables(angleYAttr->value()).c_str()) : 0.f;
    info.AngleX = angleXAttr ? Utils::convertFloat(substituteVariables(angleXAttr->value()).c_str()) : 0.f;
	info.Slice3DX = slice3DXAttr ? Utils::convertInt(substituteVariables(slice3DXAttr->value()).c_str()) : 0.0f;
	info.Slice3DY = slice3DYAttr ? Utils::convertInt(substituteVariables(slice3DYAttr->value()).c_str()) : 0.0f;
	info.WrapLX = wrapLXAttr ? Utils::convertInt(substituteVariables(wrapLXAttr->value()).c_str()) : 0.0f;
	info.WrapLY = wrapLYAttr ? Utils::convertInt(substituteVariables(wrapLYAttr->value()).c_str()) : 0.0f;
	info.WrapRX = wrapRXAttr ? Utils::convertInt(substituteVariables(wrapRXAttr->value()).c_str()) : 0.0f;
	info.WrapRY = wrapRYAttr ? Utils::convertInt(substituteVariables(wrapRYAttr->value()).c_str()) : 0.0f;
	info.WrapTX = wrapTXAttr ? Utils::convertInt(substituteVariables(wrapTXAttr->value()).c_str()) : 0.0f;
	info.WrapTY = wrapTYAttr ? Utils::convertInt(substituteVariables(wrapTYAttr->value()).c_str()) : 0.0f;
	info.WrapBX = wrapBXAttr ? Utils::convertInt(substituteVariables(wrapBXAttr->value()).c_str()) : 0.0f;
	info.WrapBY = wrapBYAttr ? Utils::convertInt(substituteVariables(wrapBYAttr->value()).c_str()) : 0.0f;
    info.autoFit = autoFitAttr && Utils::toLower(substituteVariables(autoFitAttr->value())) == "true";
    info.fitToWidth = fitWidthAttr && Utils::toLower(substituteVariables(fitWidthAttr->value())) == "true";
    info.fitToHeight = fitHeightAttr && Utils::toLower(substituteVariables(fitHeightAttr->value())) == "true";

    if (minFontSizeAttr)
        info.minFontSize = Utils::convertFloat(substituteVariables(minFontSizeAttr->value()).c_str());
    if (maxFontSizeAttr)
        info.maxFontSize = Utils::convertFloat(substituteVariables(maxFontSizeAttr->value()).c_str());
    if (borderColorAttr) {
        std::string colorStr = substituteVariables(borderColorAttr->value());
        try {
            info.borderColor = new Color(colorStr);
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "PageBuilder", "Set borderColor: " + colorStr);
#endif
        }
        catch (const std::exception& e) {
            Logger::write(Logger::ZONE_ERROR, "PageBuilder", "Failed to set borderColor: " + colorStr + ", Error: " + e.what());
            info.borderColor = nullptr;
        }
    }

    if (selectedBorderColorAttr) {
        try {
            std::string colorStr = substituteVariables(selectedBorderColorAttr->value());
            info.selectedBorderColor = new Color(colorStr);
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "PageBuilder", "Set selectedBorderColor: " + colorStr);
#endif
        }
        catch (const std::exception& e) {
            Logger::write(Logger::ZONE_ERROR, "PageBuilder", "Failed to set selectedBorderColor: " + std::string(e.what()));
            info.selectedBorderColor = nullptr;
        }
    }
    else {
        info.selectedBorderColor = nullptr; // Explicitly set to nullptr if not in item XML
    }

    if (borderSpaceAttr) {
        info.borderSpace = Utils::convertFloat(substituteVariables(borderSpaceAttr->value()).c_str());
#ifdef _DEBUG
        Logger::write(Logger::ZONE_DEBUG, "PageBuilder", "Set borderSpace: " + std::to_string(info.borderSpace));
#endif
    }
    if (borderThicknessAttr) {
        info.borderThickness = Utils::convertFloat(substituteVariables(borderThicknessAttr->value()).c_str());
#ifdef _DEBUG
        Logger::write(Logger::ZONE_DEBUG, "PageBuilder", "Set borderThickness: " + std::to_string(info.borderThickness));
#endif
    }

    if (fontColor)
    {
        Font* font = addFont(componentXml, defaultXml);
        info.font = font;
#ifdef _DEBUG
        Logger::write(Logger::ZONE_DEBUG, "PageBuilder", "Set font from fontColor attribute");
#endif
    }

    if (backgroundColor)
    {
        std::stringstream ss(substituteVariables(backgroundColor->value()));
        int num;
        ss >> std::hex >> num;
        int red = num / 0x10000;
        int green = (num / 0x100) % 0x100;
        int blue = num % 0x100;

        info.BackgroundRed = static_cast<float>(red) / 255.0f;
        info.BackgroundGreen = static_cast<float>(green) / 255.0f;
        info.BackgroundBlue = static_cast<float>(blue) / 255.0f;
#ifdef _DEBUG
        Logger::write(Logger::ZONE_DEBUG, "PageBuilder", "Set backgroundColor: R=" + std::to_string(red) + ", G=" + std::to_string(green) + ", B=" + std::to_string(blue));
#endif
    }

    if (backgroundAlpha)
    {
        info.BackgroundAlpha = Utils::convertFloat(substituteVariables(backgroundAlpha->value()).c_str());
#ifdef _DEBUG
        Logger::write(Logger::ZONE_DEBUG, "PageBuilder", "Set backgroundAlpha: " + std::to_string(info.BackgroundAlpha));
#endif
    }

    if (shapeAttr) {
        info.Shape = Utils::toLower(substituteVariables(shapeAttr->value()));
    }
    if (curvePosAttr) {
        info.CurvePosition = Utils::toLower(substituteVariables(curvePosAttr->value()));
    }
    if (curveModeAttr) {
        info.CurveMode = Utils::toLower(substituteVariables(curveModeAttr->value()));
    }
    if (curveDepthAttr) {
        info.CurveDepth = Utils::convertFloat(substituteVariables(curveDepthAttr->value()).c_str());
    }
    if (radiusAttr) {
        info.Radius = Utils::convertFloat(substituteVariables(radiusAttr->value()).c_str());
    }
    if (thicknessAttr) {
        info.Thickness = Utils::convertFloat(substituteVariables(thicknessAttr->value()).c_str());
    }
    if (startAngleAttr) {
        info.StartAngle = Utils::convertFloat(substituteVariables(startAngleAttr->value()).c_str());
    }
    if (endAngleAttr) {
        info.EndAngle = Utils::convertFloat(substituteVariables(endAngleAttr->value()).c_str());
    }

    if (info.Shape.empty() || info.Shape == "rect") {
        if (curvePosAttr || curveDepthAttr) {
            info.Shape = "curve";
        } else if (radiusAttr) {
            info.Shape = "circle";
        }
    }

    if (frameLoopAttr) {
        info.frameLoop = substituteVariables(frameLoopAttr->value());
        Logger::write(Logger::ZONE_DEBUG, "PageBuilder", "Set frameLoop: " + info.frameLoop);
    }
    else if (parentViewInfo) {
        info.frameLoop = parentViewInfo->frameLoop;
    }
    else {
        info.frameLoop = "noPlay";  
    }

    if (speedAttr) {
        info.speed = Utils::convertInt(substituteVariables(speedAttr->value()).c_str());
        Logger::write(Logger::ZONE_DEBUG, "PageBuilder", "Set speed: " + std::to_string(info.speed));
    }
    else if (parentViewInfo) {
        info.speed = parentViewInfo->speed;
    }
    else {
        info.speed = 7500;  
    }
}

void PageBuilder::getTweenSet(xml_node<>* node, Animation* animation)
{
    if (node)
    {
        for (xml_node<>* set = node->first_node("set"); set; set = set->next_sibling("set"))
        {
            TweenSet* ts = new TweenSet();
            getAnimationEvents(set, *ts);
            animation->Push(ts);
        }
    }
}

void PageBuilder::getAnimationEvents(xml_node<>* node, TweenSet& tweens)
{
    xml_attribute<>* durationXml = node->first_attribute("duration");

    if (!durationXml)
    {
        Logger::write(Logger::ZONE_ERROR, "Layout", "Animation set tag missing \"duration\" attribute");
    }
    else
    {
        for (xml_node<>* animate = node->first_node("animate"); animate; animate = animate->next_sibling("animate"))
        {
            xml_attribute<>* type = animate->first_attribute("type");
            xml_attribute<>* from = animate->first_attribute("from");
            xml_attribute<>* to = animate->first_attribute("to");
            xml_attribute<>* algorithmXml = animate->first_attribute("algorithm");

            std::string animateType;
            if (type)
            {
                animateType = substituteVariables(type->value()); 
            }

            if (!type)
            {
                Logger::write(Logger::ZONE_ERROR, "Layout", "Animate tag missing \"type\" attribute");
            }
            else if (!to && animateType != "nop")
            {
                Logger::write(Logger::ZONE_ERROR, "Layout", "Animate tag missing \"to\" attribute");
            }
            else
            {
                float fromValue = 0.0f;
                bool fromDefined = true;
                if (from)
                {
                    fromValue = Utils::convertFloat(substituteVariables(from->value()).c_str()); 
                }
                else
                {
                    fromDefined = false;
                }
                float toValue = 0.0f;
                if (to)
                {
                    toValue = Utils::convertFloat(substituteVariables(to->value()).c_str()); 
                }
                float durationValue = Utils::convertFloat(substituteVariables(durationXml->value()).c_str()); 

                TweenAlgorithm algorithm = LINEAR;
                TweenProperty property;

                if (algorithmXml)
                {
                    algorithm = Tween::getTweenType(substituteVariables(algorithmXml->value()).c_str()); 
                }

                if (Tween::getTweenProperty(animateType.c_str(), property)) // Modified: Use substituted animateType
                {
                    switch (property)
                    {
                    case TWEEN_PROPERTY_WIDTH:
                    case TWEEN_PROPERTY_X:
                    case TWEEN_PROPERTY_X_OFFSET:
                    case TWEEN_PROPERTY_CONTAINER_X:
                    case TWEEN_PROPERTY_CONTAINER_WIDTH:
                        fromValue = getHorizontalAlignment(from, 0); 
                        toValue = getHorizontalAlignment(to, 0);
                        break;

                    case TWEEN_PROPERTY_X_ORIGIN:
                        fromValue = getHorizontalAlignment(from, 0) / layoutWidth_;
                        toValue = getHorizontalAlignment(to, 0) / layoutWidth_;
                        break;

                    case TWEEN_PROPERTY_HEIGHT:
                    case TWEEN_PROPERTY_Y:
                    case TWEEN_PROPERTY_Y_OFFSET:
                    case TWEEN_PROPERTY_FONT_SIZE:
                    case TWEEN_PROPERTY_CONTAINER_Y:
                    case TWEEN_PROPERTY_CONTAINER_HEIGHT:
                        fromValue = getVerticalAlignment(from, 0); 
                        toValue = getVerticalAlignment(to, 0);
                        break;

                    case TWEEN_PROPERTY_Y_ORIGIN:
                        fromValue = getVerticalAlignment(from, 0) / layoutHeight_;
                        toValue = getVerticalAlignment(to, 0) / layoutHeight_;
                        break;

                    case TWEEN_PROPERTY_MAX_WIDTH:
                    case TWEEN_PROPERTY_MAX_HEIGHT:
                        fromValue = getVerticalAlignment(from, FLT_MAX);
                        toValue = getVerticalAlignment(to, FLT_MAX);
                        break;

                    default:
                        break;
                    }

                    Tween* t = new Tween(property, algorithm, fromValue, toValue, durationValue);
                    if (!fromDefined)
                        t->startDefined = false;
                    tweens.push(t);
                }
                else
                {
                    std::stringstream ss;
                    ss << "Unsupported tween type attribute \"" << animateType << "\""; 
                    Logger::write(Logger::ZONE_ERROR, "Layout", ss.str());
                }
            }
        }
    }
}

void PageBuilder::parseCommonAttributes(rapidxml::xml_node<>* node, Component* component)
{
    component->group = node->first_attribute("group") ? substituteVariables(node->first_attribute("group")->value()) : "";
    component->subgroup = node->first_attribute("subgroup") ? substituteVariables(node->first_attribute("subgroup")->value()) : "";
    component->name = node->first_attribute("name") ? substituteVariables(node->first_attribute("name")->value()) : "";
    component->active = node->first_attribute("active") ? (Utils::toLower(substituteVariables(node->first_attribute("active")->value())) == "true") : true;
    component->shared.clear();
    if (auto sharedAttr = node->first_attribute("shared")) {
        std::string sharedStr = substituteVariables(sharedAttr->value());
        Utils::listToVector(sharedStr, component->shared, ',');
    }
}

void PageBuilder::parseVariables(rapidxml::xml_node<>* node)
{
    rapidxml::xml_node<>* varsNode = node->first_node("variables");
    if (varsNode) {
        for (rapidxml::xml_node<>* var = varsNode->first_node(); var; var = var->next_sibling()) {
            std::string name = var->name();
            std::string value = var->value();
            if (!name.empty()) {
                variables_[name] = value;
                if (isDefaultLayout_) {
                    globalVariables_[name] = value;
                    Logger::write(Logger::ZONE_DEBUG, "PageBuilder", "Stored global variable '" + name + "' = '" + value + "'");
                }
            }
        }
    }
    if (!isDefaultLayout_) {
        for (const auto& gv : globalVariables_) {
            if (variables_.find(gv.first) == variables_.end()) {
                variables_[gv.first] = gv.second;
                Logger::write(Logger::ZONE_DEBUG, "PageBuilder", "Included global variable '" + gv.first + "' = '" + gv.second + "' into local variables");
            }
        }
    }
}

std::string PageBuilder::substituteVariables(const std::string& value) const
{
    std::string result = value;
    size_t pos = 0;
    while (pos < result.length()) {
        if (result[pos] == '$') {
            if (pos + 1 < result.length() && result[pos + 1] == '{') { // Braced variable, e.g., ${randomImageSrc}
                size_t start = pos + 2; // Skip '${'
                size_t end = start;
                while (end < result.length() && (std::isalnum(result[end]) || result[end] == '_')) {
                    ++end;
                }
                if (end < result.length() && result[end] == '}') { // Found closing '}'
                    std::string varName = result.substr(start, end - start);
                    auto it = variables_.find(varName);
                    if (it != variables_.end()) {
                        // Replace ${varName} with its value, keep suffix for further processing
                        result.replace(pos, end + 1 - pos, it->second);
                        Logger::write(Logger::ZONE_DEBUG, "PageBuilder", "Substituted ${" + varName + "} to " + it->second);
                        // Move pos to start of replaced value to check for nested variables
                        pos = pos + it->second.length() - it->second.length(); // Reset to pos to reprocess for nested variables
                    }
                    else {
#ifdef _DEBUG
                        Logger::write(Logger::ZONE_WARNING, "PageBuilder", "Undefined variable: ${" + varName + "}");
#endif
                        pos = end + 1; // Skip past }
                    }
                }
                else {
                    pos = end; // Malformed, skip to end of variable name
                }
            }
            else { // Non-braced variable, e.g., $randomImageSrc
                size_t start = pos + 1;
                size_t end = start;
                while (end < result.length() && (std::isalnum(result[end]) || result[end] == '_')) {
                    ++end; 
                }
                if (end > start) {
                    std::string varName = result.substr(start, end - start);
                    auto it = variables_.find(varName);
                    if (it != variables_.end()) {
                        // Replace $varName with its value
                        result.replace(pos, end - pos, it->second);
                        Logger::write(Logger::ZONE_DEBUG, "PageBuilder", "Substituted $" + varName + " to " + it->second);
                        pos = pos + it->second.length() - it->second.length(); // Reset to pos to reprocess
                    }
                    else {
#ifdef _DEBUG
                        Logger::write(Logger::ZONE_WARNING, "PageBuilder", "Undefined variable: $" + varName);
#endif
                        pos = end;
                    }
                }
                else {
                    ++pos; // Skip lone '$'
                }
            }
        }
        else {
            ++pos; // Move to next character
        }
    }
    return result;
}

bool PageBuilder::processInclude(rapidxml::xml_node<>* includeNode, rapidxml::xml_document<>& mainDoc, rapidxml::xml_node<>* parentNode)
{
    xml_attribute<>* srcAttr = includeNode->first_attribute("src");
    if (!srcAttr) {
        Logger::write(Logger::ZONE_ERROR, "Layout", "<include> tag missing 'src' attribute");
        return false;
    }

    // Parse attributes similar to loadSounds
    std::string name = includeNode->first_attribute("name") ? substituteVariables(includeNode->first_attribute("name")->value()) : "";
    std::string group = includeNode->first_attribute("group") ? substituteVariables(includeNode->first_attribute("group")->value()) : "";
    std::string subgroup = includeNode->first_attribute("subgroup") ? substituteVariables(includeNode->first_attribute("subgroup")->value()) : "";
    bool active = includeNode->first_attribute("active") ? (Utils::toLower(substituteVariables(includeNode->first_attribute("active")->value())) == "true") : true;
    std::vector<std::string> shared;
    if (auto sharedAttr = includeNode->first_attribute("shared")) {
        std::string sharedStr = substituteVariables(sharedAttr->value());
        Utils::listToVector(sharedStr, shared, ',');
    }

    // Apply variable substitution to src
    std::string src = substituteVariables(srcAttr->value());
    std::string includePath = Utils::combinePath(baseLayoutPath, src);

    // Log attributes for debugging
    std::string sharedLog = shared.empty() ? "none" : "";
    for (size_t i = 0; i < shared.size(); ++i) {
        sharedLog += shared[i];
        if (i < shared.size() - 1) sharedLog += ",";
    }
    Logger::write(Logger::ZONE_DEBUG, "Layout", "Processing <include> src=" + src +
        ", name=" + name +
        ", group=" + group +
        ", subgroup=" + subgroup +
        ", active=" + (active ? "true" : "false") +
        ", shared=" + sharedLog);

    // Check active attribute
    if (!active) {
        Logger::write(Logger::ZONE_DEBUG, "Layout", "Skipping inactive <include> tag: " + src);
        return true; // Skip but don't fail
    }

    // Prevent circular includes
    if (includedFiles_.find(includePath) != includedFiles_.end()) {
        Logger::write(Logger::ZONE_WARNING, "Layout", "Circular include detected: " + includePath);
        return false;
    }
    if (includedFiles_.size() >= MAX_INCLUDE_DEPTH) {
        Logger::write(Logger::ZONE_ERROR, "Layout", "Maximum include depth exceeded: " + includePath);
        return false;
    }
    includedFiles_.insert(includePath);

    // Save current variables to restore later (optional, depending on desired scope)
    auto originalVariables = variables_;

    // Check cache first
    auto it = layoutCache_.find(includePath);
    if (it != layoutCache_.end()) {
        rapidxml::xml_node<>* includeRoot = it->second.doc->first_node("layout");
        if (!includeRoot) {
            Logger::write(Logger::ZONE_ERROR, "Layout", "Included file missing <layout> tag: " + includePath);
            includedFiles_.erase(includePath);
            return false;
        }
        // Check if included layout is active
        xml_attribute<>* layoutActiveAttr = includeRoot->first_attribute("active");
        if (layoutActiveAttr && Utils::toLower(substituteVariables(layoutActiveAttr->value())) != "true") {
            Logger::write(Logger::ZONE_DEBUG, "Layout", "Skipping inactive included layout: " + includePath);
            includedFiles_.erase(includePath);
            return true;
        }
        // Parse variables from included file
        parseVariables(includeRoot);
        // Clone nodes with immediate variable substitution
        for (auto* child = includeRoot->first_node(); child; child = child->next_sibling()) {
            auto* clonedNode = mainDoc.clone_node(child);
            // Substitute variables in all attributes of the cloned node
            for (auto* attr = clonedNode->first_attribute(); attr; attr = attr->next_attribute()) {
                std::string substitutedValue = substituteVariables(attr->value());
                attr->value(mainDoc.allocate_string(substitutedValue.c_str()));
            }
            parentNode->append_node(clonedNode);
        }
        // Restore original variables (optional)
        variables_ = originalVariables;
        includedFiles_.erase(includePath);
        return true;
    }

    // Load file
    auto file = std::make_unique<std::ifstream>(includePath, std::ios::binary);
    if (!file->good()) {
        Logger::write(Logger::ZONE_ERROR, "Layout", "Could not find included file: " + includePath);
        includedFiles_.erase(includePath);
        return false;
    }

    file->seekg(0, std::ios::end);
    std::string buffer;
    buffer.resize(file->tellg());
    file->seekg(0, std::ios::beg);
    file->read(buffer.data(), buffer.size());
    buffer.push_back('\0');

    auto doc = std::make_unique<rapidxml::xml_document<>>();
    try {
        doc->parse<0>(buffer.data());
        rapidxml::xml_node<>* includeRoot = doc->first_node("layout");
        if (!includeRoot) {
            Logger::write(Logger::ZONE_ERROR, "Layout", "Included file missing <layout> tag: " + includePath);
            includedFiles_.erase(includePath);
            return false;
        }
        // Check if included layout is active
        xml_attribute<>* layoutActiveAttr = includeRoot->first_attribute("active");
        if (layoutActiveAttr && Utils::toLower(substituteVariables(layoutActiveAttr->value())) != "true") {
            Logger::write(Logger::ZONE_DEBUG, "Layout", "Skipping inactive included layout: " + includePath);
            includedFiles_.erase(includePath);
            return true;
        }
        // Parse variables from included file
        parseVariables(includeRoot);
        // Clone nodes with immediate variable substitution
        for (auto* child = includeRoot->first_node(); child; child = child->next_sibling()) {
            auto* clonedNode = mainDoc.clone_node(child);
            // Substitute variables in all attributes of the cloned node
            for (auto* attr = clonedNode->first_attribute(); attr; attr = attr->next_attribute()) {
                std::string substitutedValue = substituteVariables(attr->value());
                attr->value(mainDoc.allocate_string(substitutedValue.c_str()));
            }
            parentNode->append_node(clonedNode);
        }
        // Cache the included file
        LayoutCache& cache = layoutCache_[includePath];
        cache.buffer = std::move(buffer);
        cache.doc = std::make_unique<rapidxml::xml_document<>>();
        cache.sourceFile = includePath;
        cache.doc->parse<0>(cache.buffer.data());
        Logger::write(Logger::ZONE_INFO, "Layout", "Cached included file: " + includePath);
        // Restore original variables (optional)
        variables_ = originalVariables;
        includedFiles_.erase(includePath);
        return true;
    }
    catch (rapidxml::parse_error& e) {
        std::string reason = (e.what() != nullptr) ? e.what() : "unknown error";
        std::string errorMsg = "Could not parse included file: " + includePath +
            " [Line: " + std::to_string(std::count(buffer.begin(),
                std::find(buffer.begin(), buffer.end(), *e.where<char>()), '\n') + 1) +
            "] Reason: " + reason;

        // Completely hide the common "expected >" false alarm (your PHOENIX theme)
        if (reason.find("expected >") != std::string::npos) {
#ifdef _DEBUG
            Logger::write(Logger::ZONE_DEBUG, "Layout", errorMsg + " (harmless - long variable substitution)");
#else
            // Silent in Release builds
#endif
        }
        else {
            // Any real XML error will still show as ERROR
            Logger::write(Logger::ZONE_ERROR, "Layout", errorMsg);
        }

        includedFiles_.erase(includePath);
        return false;
    }
}

std::string PageBuilder::substituteGlobalVariables(const std::string& value)
{
    std::string result = value;
    size_t pos = 0;
    while (pos < result.length()) {
        if (result[pos] == '$') {
            if (pos + 1 < result.length() && result[pos + 1] == '{') { // Braced variable, e.g., ${dLayout}
                size_t start = pos + 2;
                size_t end = start;
                while (end < result.length() && (std::isalnum(result[end]) || result[end] == '_')) {
                    ++end;
                }
                if (end < result.length() && result[end] == '}') {
                    std::string varName = result.substr(start, end - start);
                    auto it = globalVariables_.find(varName);
                    if (it != globalVariables_.end()) {
                        Logger::write(Logger::ZONE_DEBUG, "PageBuilder", "Substituted ${" + varName + "} with global value '" + it->second + "'");
                        result.replace(pos, end + 1 - pos, it->second);
                        pos = 0; // Restart to handle nested variables
                    }
                    else {
                        Logger::write(Logger::ZONE_WARNING, "PageBuilder", "Undefined braced variable: ${" + varName + "}");
                        pos = end + 1;
                    }
                }
                else {
                    pos = end;
                }
            }
            else { // Non-braced variable, e.g., $dLayout
                size_t start = pos + 1;
                size_t end = start;
                while (end < result.length() && (std::isalnum(result[end]) || result[end] == '_')) {
                    ++end;
                }
                if (end > start) {
                    std::string varName = result.substr(start, end - start);
                    auto it = globalVariables_.find(varName);
                    if (it != globalVariables_.end()) {
                        Logger::write(Logger::ZONE_DEBUG, "PageBuilder", "Substituted $" + varName + " with global value '" + it->second + "'");
                        result.replace(pos, end - pos, it->second);
                        pos = 0; // Restart to handle subsequent variables
                    }
                    else {
#ifdef _DEBUG
                        Logger::write(Logger::ZONE_WARNING, "PageBuilder", "Undefined variable: $" + varName);
#endif
                        pos = end;
                    }
                }
                else {
                    ++pos;
                }
            }
        }
        else {
            ++pos;
        }
    }
    return result;
}

void PageBuilder::parseSearchConfig(rapidxml::xml_node<>* searchNode, Page* page)
{
    if (!searchNode || !page) return;

    SearchConfig cfg = page->getSearchConfig();
    cfg.configured = true;

    auto applyKeyValue = [&](const std::string& rawKey, const std::string& rawVal)
    {
        std::string key = Utils::toLower(rawKey);
        std::string val = substituteVariables(rawVal);
        if (val.empty()) return;

        // TrueType font
        if (key == "font")
        {
            bool isAbs = (val.size() > 1 && val[1] == ':') || val[0] == '/' || val[0] == '\\';
            if (!isAbs)
            {
                std::string testPath = config_.convertToAbsolutePath(
                    Utils::combinePath(config_.absolutePath, "layouts", layoutKey, ""),
                    val.c_str());
                std::ifstream f(testPath);
                if (f.good())
                {
                    cfg.font = testPath;
                }
                else
                {
                    cfg.font = Utils::combinePath(config_.absolutePath, val);
                }
            }
            else
            {
                cfg.font = val;
            }
            return;
        }

        // All 30 Color keys from PDF:
        if (key == "accent") { cfg.colors.accent = Page::parseColor(val, cfg.colors.accent); return; }
        if (key == "accentdk" || key == "accent_dk" || key == "accentcontrast") { cfg.colors.accentDk = Page::parseColor(val, cfg.colors.accentDk); return; }
        if (key == "panel" || key == "panelcol" || key == "panelsurface" || key == "mainpanel") { cfg.colors.panel = Page::parseColor(val, cfg.colors.panel); return; }
        if (key == "border" || key == "panelborder" || key == "paneloutline") { cfg.colors.border = Page::parseColor(val, cfg.colors.border); return; }
        if (key == "card" || key == "cardcol" || key == "cards") { cfg.colors.card = Page::parseColor(val, cfg.colors.card); return; }
        if (key == "field" || key == "fieldcol" || key == "searchfield" || key == "entryfield") { cfg.colors.field = Page::parseColor(val, cfg.colors.field); return; }
        if (key == "backdrop" || key == "dimmer") { cfg.colors.backdrop = Page::parseColor(val, cfg.colors.backdrop); return; }
        if (key == "texthi" || key == "text_hi" || key == "texthigh" || key == "primarytext") { cfg.colors.textHi = Page::parseColor(val, cfg.colors.textHi); return; }
        if (key == "textmid" || key == "text_mid" || key == "secondarytext") { cfg.colors.textMid = Page::parseColor(val, cfg.colors.textMid); return; }
        if (key == "textlow" || key == "text_low" || key == "mutedtext") { cfg.colors.textLow = Page::parseColor(val, cfg.colors.textLow); return; }
        if (key == "nomatchescolor" || key == "nomatches_color" || key == "no_matches_color") { cfg.colors.noMatches = Page::parseColor(val, cfg.colors.noMatches); return; }
        if (key == "keyface" || key == "key_face" || key == "normalkeyfill") { cfg.colors.keyFace = Page::parseColor(val, cfg.colors.keyFace); return; }
        if (key == "keyedge" || key == "key_edge" || key == "normalkeyedge") { cfg.colors.keyEdge = Page::parseColor(val, cfg.colors.keyEdge); return; }
        if (key == "specialface" || key == "special_face" || key == "specialkeyfill") { cfg.colors.specialFace = Page::parseColor(val, cfg.colors.specialFace); return; }
        if (key == "specialedge" || key == "special_edge" || key == "specialkeyedge") { cfg.colors.specialEdge = Page::parseColor(val, cfg.colors.specialEdge); return; }
        if (key == "okface" || key == "ok_face" || key == "okfill") { cfg.colors.okFace = Page::parseColor(val, cfg.colors.okFace); return; }
        if (key == "okedge" || key == "ok_edge") { cfg.colors.okEdge = Page::parseColor(val, cfg.colors.okEdge); return; }
        if (key == "cancelface" || key == "cancel_face" || key == "cancelfill") { cfg.colors.cancelFace = Page::parseColor(val, cfg.colors.cancelFace); return; }
        if (key == "canceledge" || key == "cancel_edge") { cfg.colors.cancelEdge = Page::parseColor(val, cfg.colors.cancelEdge); return; }
        if (key == "focusedge" || key == "focus_edge" || key == "focusedkeyoutline") { cfg.colors.focusEdge = Page::parseColor(val, cfg.colors.focusEdge); return; }
        if (key == "resultfocused" || key == "result_focused" || key == "selectedresult") { cfg.colors.resultFocused = Page::parseColor(val, cfg.colors.resultFocused); return; }
        if (key == "resultinactive" || key == "result_inactive" || key == "unselectedresult") { cfg.colors.resultInactive = Page::parseColor(val, cfg.colors.resultInactive); return; }
        if (key == "resultstripe" || key == "result_stripe" || key == "selectedresultstripe" || key == "resultstrip") { cfg.colors.resultStripe = Page::parseColor(val, cfg.colors.resultStripe); return; }
        if (key == "footeredge" || key == "footer_edge" || key == "footerbuttonedge") { cfg.colors.footerEdge = Page::parseColor(val, cfg.colors.footerEdge); return; }
        if (key == "footerface" || key == "footer_face" || key == "footerbuttonfill") { cfg.colors.footerFace = Page::parseColor(val, cfg.colors.footerFace); return; }
        if (key == "statusbackdrop" || key == "status_backdrop" || key == "loadingbackdrop") { cfg.colors.statusBackdrop = Page::parseColor(val, cfg.colors.statusBackdrop); return; }
        if (key == "statusborder" || key == "status_border" || key == "loadingborder") { cfg.colors.statusBorder = Page::parseColor(val, cfg.colors.statusBorder); return; }
        if (key == "statuspanel" || key == "status_panel" || key == "loadingpanel") { cfg.colors.statusPanel = Page::parseColor(val, cfg.colors.statusPanel); return; }
        if (key == "statusspinner" || key == "status_spinner" || key == "loadingspinner" || key == "spinner") { cfg.colors.statusSpinner = Page::parseColor(val, cfg.colors.statusSpinner); return; }
        if (key == "statustext" || key == "status_text" || key == "loadingtext") { cfg.colors.statusText = Page::parseColor(val, cfg.colors.statusText); return; }

        // Ambiguous "nomatches": check if value is likely a color (hex/RGB) or text
        if (key == "nomatches" || key == "no_matches")
        {
            std::string s = val;
            s.erase(0, s.find_first_not_of(" \t\r\n"));
            s.erase(s.find_last_not_of(" \t\r\n") + 1);
            bool isColor = (s.find(',') != std::string::npos);
            if (!isColor)
            {
                std::string h = s;
                if (!h.empty() && h[0] == '#') h = h.substr(1);
                else if (h.rfind("0x", 0) == 0 || h.rfind("0X", 0) == 0) h = h.substr(2);
                if (h.length() == 6 || h.length() == 8)
                {
                    isColor = true;
                    for (char c : h) if (!isxdigit(static_cast<unsigned char>(c))) { isColor = false; break; }
                }
            }
            if (isColor)
            {
                cfg.colors.noMatches = Page::parseColor(val, cfg.colors.noMatches);
            }
            else
            {
                cfg.texts.noMatches = val;
            }
            return;
        }

        // Text translations:
        // Header & Modes
        if (key == "title" || key == "searchtitle" || key == "header") { cfg.texts.title = val; return; }
        if (key == "modeview" || key == "mode_view" || key == "viewmode" || key == "modepage" || key == "mode_page" || key == "pagemode") { cfg.texts.modeView = val; return; }
        if (key == "modelist" || key == "mode_list" || key == "listmode") { cfg.texts.modeList = val; return; }
        if (key == "scopeall" || key == "scope_all") { cfg.texts.scopeAll = val; return; }
        if (key == "scopecurrent" || key == "scope_current" || key == "scopesystem" || key == "scope_system") { cfg.texts.scopeCurrent = val; return; }
        if (key == "game" || key == "single_game") { cfg.texts.game = val; return; }
        if (key == "games") { cfg.texts.games = val; return; }
        if (key == "collection" || key == "single_collection") { cfg.texts.collection = val; return; }
        if (key == "collections") { cfg.texts.collections = val; return; }
        if (key == "separator" || key == "infoseparator" || key == "dot") { cfg.texts.separator = val; return; }
        if (key == "info" || key == "infoformat" || key == "infopattern") { cfg.texts.infoFormat = val; return; }

        // Query field & chips
        if (key == "placeholder" || key == "queryplaceholder" || key == "prompt") { cfg.texts.placeholder = val; return; }
        if (key == "minchars" || key == "min_chars" || key == "minimumchars") { cfg.texts.minChars = val; return; }
        if (key == "nomatchestext" || key == "no_matches_text") { cfg.texts.noMatches = val; return; }
        if (key == "resultsingle" || key == "result_single" || key == "result") { cfg.texts.resultSingle = val; return; }
        if (key == "resultplural" || key == "result_plural" || key == "results") { cfg.texts.resultPlural = val; return; }

        // Virtual keyboard special keys
        if (key == "keyspace" || key == "key_space" || key == "space") { cfg.texts.keySpace = val; return; }
        if (key == "keydel" || key == "key_del" || key == "del" || key == "delete") { cfg.texts.keyDel = val; return; }
        if (key == "keyclear" || key == "key_clear" || key == "clear") { cfg.texts.keyClear = val; return; }
        if (key == "keyok" || key == "key_ok" || key == "ok") { cfg.texts.keyOk = val; return; }
        if (key == "keycancel" || key == "key_cancel" || key == "cancel") { cfg.texts.keyCancel = val; return; }

        // List mode card
        if (key == "matchingtitle" || key == "matching_title" || key == "matchinggames" || key == "matching_games") { cfg.texts.matchingTitle = val; return; }
        if (key == "listprompt" || key == "list_prompt" || key == "enteragame" || key == "promptlist") { cfg.texts.listPrompt = val; return; }
        if (key == "listminchars" || key == "list_min_chars") { cfg.texts.listMinChars = val; return; }
        if (key == "listnomatches" || key == "list_no_matches" || key == "nomatchinggames") { cfg.texts.listNoMatches = val; return; }

        // Preview type (e.g. "logo", "artwork_3d", "screenshot")
        if (key == "previewtype" || key == "preview_type" || key == "previewimage" || key == "preview") { cfg.previewType = val; return; }

        // Preview card height configuration (pixels like "160" or percentage like "28%")
        if (key == "previewheight" || key == "preview_height" || key == "previewh" || key == "preview_h" || key == "previewcardheight") { cfg.previewHeight = val; return; }

        // Footer hints (keys/buttons) - Supports Xbox, PlayStation, Arcade or custom names
        if (key == "hintselect" || key == "hint_select" || key == "select" || key == "btna" || key == "btn_a") { cfg.texts.hintSelect = val; return; }
        if (key == "hintback" || key == "hint_back" || key == "back" || key == "btnb" || key == "btn_b") { cfg.texts.hintBack = val; return; }
        if (key == "hintx" || key == "hint_x" || key == "btnx" || key == "btn_x") { cfg.texts.hintX = val; return; }
        if (key == "hinty" || key == "hint_y" || key == "btny" || key == "btn_y") { cfg.texts.hintY = val; return; }
        if (key == "hintdirections" || key == "hint_directions" || key == "directions" || key == "dpad" || key == "d_pad") { cfg.texts.hintDirections = val; return; }
        if (key == "hintdown" || key == "hint_down" || key == "down") { cfg.texts.hintDown = val; return; }
        if (key == "hintleft" || key == "hint_left" || key == "left") { cfg.texts.hintLeft = val; return; }
        if (key == "hintup" || key == "hint_up" || key == "up") { cfg.texts.hintUp = val; return; }
        if (key == "hintlb" || key == "hint_lb" || key == "btnlb" || key == "l1") { cfg.texts.hintLB = val; return; }
        if (key == "hintrb" || key == "hint_rb" || key == "btnrb" || key == "r1") { cfg.texts.hintRB = val; return; }
        if (key == "hintstart" || key == "hint_start" || key == "btnstart" || key == "start") { cfg.texts.hintStart = val; return; }

        // Footer actions (fully configurable via search theme XML)
        if (key == "actiontype" || key == "action_type" || key == "type") { cfg.texts.actionType = val; return; }
        if (key == "actionclose" || key == "action_close" || key == "close") { cfg.texts.actionClose = val; return; }
        if (key == "actionmove" || key == "action_move" || key == "move") { cfg.texts.actionMove = val; return; }
        if (key == "actiontoggle" || key == "action_toggle" || key == "toggle" || key == "togglemode" || key == "toggle_mode" || key == "actiontogglemode") { cfg.texts.actionToggle = val; return; }
        if (key == "actionscope" || key == "action_scope" || key == "togglescope" || key == "toggle_scope" || key == "actiontogglescope") { cfg.texts.actionScope = val; return; }
        if (key == "actionkeyboard" || key == "action_keyboard" || key == "keyboard") { cfg.texts.actionKeyboard = val; return; }
        if (key == "actionbrowse" || key == "action_browse" || key == "browse") { cfg.texts.actionBrowse = val; return; }
        if (key == "actionlaunch" || key == "action_launch" || key == "launch") { cfg.texts.actionLaunch = val; return; }
        if (key == "actiondelete" || key == "action_delete" || key == "delaction" || key == "deleteaction") { cfg.texts.actionDelete = val; return; }
        if (key == "actionspace" || key == "action_space" || key == "spaceaction") { cfg.texts.actionSpace = val; return; }
        if (key == "actionnextgenre" || key == "action_next_genre" || key == "nextgenre" || key == "actioncyclegenre" || key == "cyclegenre") { cfg.texts.actionNextGenre = val; return; }
        if (key == "actionprevgenre" || key == "action_prev_genre" || key == "prevgenre") { cfg.texts.actionPrevGenre = val; return; }
        if (key == "actionresetgenre" || key == "action_reset_genre" || key == "resetgenre" || key == "resetall") { cfg.texts.actionResetGenre = val; cfg.texts.genreReset = val; return; }
        if (key == "actionpage" || key == "action_page" || key == "pageaction") { cfg.texts.actionPage = val; return; }
        if (key == "actionbackspace" || key == "action_backspace") { cfg.texts.actionBackspace = val; return; }
        if (key == "genrenext" || key == "genre_next") { cfg.texts.genreNext = val; cfg.texts.actionNextGenre = val; return; }
        if (key == "genreprevnext" || key == "genre_prev_next" || key == "genreprev") { cfg.texts.genrePrevNext = val; cfg.texts.actionPrevGenre = val; return; }
        if (key == "genrereset" || key == "genre_reset") { cfg.texts.genreReset = val; cfg.texts.actionResetGenre = val; return; }

        // Optional folder for button artwork
        if (key == "buttonfolder" || key == "button_folder" || key == "buttonsrc" || key == "button_src" || key == "buttonpath" || key == "button_path")
        {
            bool isAbs = (val.size() > 1 && val[1] == ':') || val[0] == '/' || val[0] == '\\';
            if (isAbs) { cfg.buttonFolder = val; }
            else {
                std::string p1 = Utils::combinePath(baseLayoutPath, val);
                std::string p2 = Utils::combinePath(config_.absolutePath, val);
                cfg.buttonFolder = Utils::fileExists(p1) ? p1 : (Utils::fileExists(p2) ? p2 : p1);
            }
            return;
        }
    };

    auto resolvePath = [&](const std::string& raw) -> std::string
    {
        std::string p = substituteVariables(raw);
        if (p.empty()) return "";
        bool isAbs = (p.size() > 1 && p[1] == ':') || p[0] == '/' || p[0] == '\\';
        if (isAbs) return p;

        std::string p1 = Utils::combinePath(baseLayoutPath, p);
        if (Utils::fileExists(p1)) return p1;
        std::string p2 = Utils::combinePath(baseLayoutPath, "search", p);
        if (Utils::fileExists(p2)) return p2;
        std::string p3 = Utils::combinePath(config_.absolutePath, p);
        if (Utils::fileExists(p3)) return p3;
        return p1;
    };

    auto isImageFile = [](const std::string& s) -> bool
    {
        std::string lower = Utils::toLower(s);
        return (lower.find(".png") != std::string::npos ||
                lower.find(".jpg") != std::string::npos ||
                lower.find(".jpeg") != std::string::npos ||
                lower.find(".webp") != std::string::npos ||
                lower.find(".tga") != std::string::npos ||
                lower.find(".bmp") != std::string::npos);
    };

    // 1. Process all attributes on <search>
    for (rapidxml::xml_attribute<>* attr = searchNode->first_attribute(); attr; attr = attr->next_attribute())
    {
        applyKeyValue(attr->name(), attr->value());
    }

    // 2. Process all child nodes of <search>
    for (rapidxml::xml_node<>* ch = searchNode->first_node(); ch; ch = ch->next_sibling())
    {
        std::string nodeName = Utils::toLower(ch->name());
        std::string nodeVal = ch->value() ? ch->value() : "";

        // Check for src, image, file, path attribute
        std::string srcAttr = "";
        if (ch->first_attribute("src")) srcAttr = ch->first_attribute("src")->value();
        else if (ch->first_attribute("image")) srcAttr = ch->first_attribute("image")->value();
        else if (ch->first_attribute("file")) srcAttr = ch->first_attribute("file")->value();
        else if (ch->first_attribute("path")) srcAttr = ch->first_attribute("path")->value();

        // Check if node value itself is an image path
        if (srcAttr.empty() && isImageFile(nodeVal))
        {
            srcAttr = nodeVal;
            nodeVal.clear();
        }

        if (!srcAttr.empty())
        {
            std::string resolvedImg = resolvePath(srcAttr);
            std::string targetBtn = nodeName;
            if (nodeName == "image" || nodeName == "buttonimage" || nodeName == "button")
            {
                if (ch->first_attribute("name")) targetBtn = Utils::toLower(ch->first_attribute("name")->value());
                else if (ch->first_attribute("button")) targetBtn = Utils::toLower(ch->first_attribute("button")->value());
                else if (ch->first_attribute("key")) targetBtn = Utils::toLower(ch->first_attribute("key")->value());
            }

            cfg.buttonImages[targetBtn] = resolvedImg;
            if (targetBtn == "btna" || targetBtn == "btn_a" || targetBtn == "a" || targetBtn == "hintselect" || targetBtn == "select")
            {
                cfg.buttonImages["a"] = resolvedImg;
                cfg.buttonImages["btna"] = resolvedImg;
                cfg.buttonImages["hintselect"] = resolvedImg;
                cfg.buttonImages["select"] = resolvedImg;
            }
            else if (targetBtn == "btnb" || targetBtn == "btn_b" || targetBtn == "b" || targetBtn == "hintback" || targetBtn == "back")
            {
                cfg.buttonImages["b"] = resolvedImg;
                cfg.buttonImages["btnb"] = resolvedImg;
                cfg.buttonImages["hintback"] = resolvedImg;
                cfg.buttonImages["back"] = resolvedImg;
            }
            else if (targetBtn == "btnx" || targetBtn == "btn_x" || targetBtn == "x" || targetBtn == "hintx")
            {
                cfg.buttonImages["x"] = resolvedImg;
                cfg.buttonImages["btnx"] = resolvedImg;
                cfg.buttonImages["hintx"] = resolvedImg;
            }
            else if (targetBtn == "btny" || targetBtn == "btn_y" || targetBtn == "y" || targetBtn == "hinty")
            {
                cfg.buttonImages["y"] = resolvedImg;
                cfg.buttonImages["btny"] = resolvedImg;
                cfg.buttonImages["hinty"] = resolvedImg;
            }
            else if (targetBtn == "dpad" || targetBtn == "d_pad" || targetBtn == "hintdirections" || targetBtn == "directions")
            {
                cfg.buttonImages["dpad"] = resolvedImg;
                cfg.buttonImages["d-pad"] = resolvedImg;
                cfg.buttonImages["hintdirections"] = resolvedImg;
                cfg.buttonImages["all"] = resolvedImg;
            }
            else if (targetBtn == "down" || targetBtn == "hintdown")
            {
                cfg.buttonImages["down"] = resolvedImg;
                cfg.buttonImages["hintdown"] = resolvedImg;
                cfg.buttonImages["updown"] = resolvedImg;
            }
            else if (targetBtn == "left" || targetBtn == "hintleft")
            {
                cfg.buttonImages["left"] = resolvedImg;
                cfg.buttonImages["hintleft"] = resolvedImg;
                cfg.buttonImages["leftright"] = resolvedImg;
            }
            else if (targetBtn == "up" || targetBtn == "hintup")
            {
                cfg.buttonImages["up"] = resolvedImg;
                cfg.buttonImages["hintup"] = resolvedImg;
                cfg.buttonImages["updown"] = resolvedImg;
            }
            else if (targetBtn == "lb" || targetBtn == "btnlb" || targetBtn == "l1" || targetBtn == "hintlb")
            {
                cfg.buttonImages["lb"] = resolvedImg;
                cfg.buttonImages["l"] = resolvedImg;
                cfg.buttonImages["hintlb"] = resolvedImg;
            }
            else if (targetBtn == "rb" || targetBtn == "btnrb" || targetBtn == "r1" || targetBtn == "hintrb")
            {
                cfg.buttonImages["rb"] = resolvedImg;
                cfg.buttonImages["r"] = resolvedImg;
                cfg.buttonImages["hintrb"] = resolvedImg;
            }
            else if (targetBtn == "start" || targetBtn == "btnstart" || targetBtn == "hintstart")
            {
                cfg.buttonImages["start"] = resolvedImg;
                cfg.buttonImages["hintstart"] = resolvedImg;
            }

            if (!nodeVal.empty())
            {
                cfg.buttonImages[Utils::toLower(substituteVariables(nodeVal))] = resolvedImg;
            }
        }

        if (!nodeVal.empty())
        {
            applyKeyValue(ch->name(), nodeVal);
        }
    }

    // Apply to current page
    page->setSearchConfig(cfg);

    // Save as default if default layout or first configured
    if (isDefaultLayout_ || !defaultSearchConfig_.configured)
    {
        defaultSearchConfig_ = cfg;
    }
}
