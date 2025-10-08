#include "OptionsMenuBuilder.h"
#include "../utility/Log.h"
#include "../Utility/Utils.h"
#include <filesystem>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <regex>

OptionsMenuBuilder::OptionsMenuBuilder(const std::string& xmlPath) {
    // Initialize default colors
    backgroundColor_ = { 0, 0, 0, 255 }; // Black, opaque
    titleColor_ = { 255, 255, 255, 255 }; // White, opaque
    optionColor_ = { 255, 255, 255, 255 }; // White, opaque
    selectionBarColor_ = { 100, 100, 100, 255 }; // Gray, opaque (default)
    titleBarColor_ = { 0, 0, 0, 255 }; // Black, opaque
    subtitleUnderlineColor_ = { 255, 255, 255, 255 }; // White, opaque
    fontColor_ = { 255, 255, 255, 255 }; // White, opaque
    selectionBarColorParsed_ = false;
    titleSpacing_ = 20; // Matches previous hardcoded +20
    selectionBarYOffset_ = -5; // Matches previous hardcoded -5
    loadConfig(xmlPath);
}

void OptionsMenuBuilder::parseAlignment(const std::string& value, XAlignment& xAlign, int& xOffset, YAlignment& yAlign, int& yOffset) {
    xAlign = XAlignment::Numeric;
    yAlign = YAlignment::Numeric;
    xOffset = 0;
    yOffset = 0;
#ifdef _DEBUG
    Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsing alignment value: " + value);
#endif

    try {
        size_t pos;
        int num = std::stoi(value, &pos);
        if (pos == value.length()) {
            xAlign = XAlignment::Numeric;
            yAlign = YAlignment::Numeric;
            xOffset = num;
            yOffset = num;
#ifdef _DEBUG
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed as numeric: " + std::to_string(num));
#endif
            return;
        }
    }
    catch (const std::exception&) {}

    std::regex alignRegex(R"(^(center|left|right|top|bottom)([+-]\d+)?$)", std::regex::icase);
    std::smatch match;
    if (std::regex_match(value, match, alignRegex)) {
        std::string align = match[1].str();
        std::string offsetStr = match[2].str();
        std::transform(align.begin(), align.end(), align.begin(), ::tolower);

        if (align == "center") {
            xAlign = XAlignment::Center;
            yAlign = YAlignment::Center;
        }
        else if (align == "left") {
            xAlign = XAlignment::Left;
        }
        else if (align == "right") {
            xAlign = XAlignment::Right;
        }
        else if (align == "top") {
            yAlign = YAlignment::Top;
        }
        else if (align == "bottom") {
            yAlign = YAlignment::Bottom;
        }

        if (!offsetStr.empty()) {
            try {
                int offset = std::stoi(offsetStr);
                xOffset = offset;
                yOffset = offset;
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed offset: " + std::to_string(offset));
            }
            catch (const std::exception&) { 
                Logger::write(Logger::ZONE_WARNING, "OptionsMenuBuilder", "Invalid offset: " + offsetStr);
            }
        }
    }
    else {
        Logger::write(Logger::ZONE_WARNING, "OptionsMenuBuilder", "Invalid alignment value: " + value);
    }
#ifdef _DEBUG
    Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed alignment: xAlign=" +
        (std::string(xAlign == XAlignment::Center ? "center" :
            xAlign == XAlignment::Left ? "left" :
            xAlign == XAlignment::Right ? "right" : "numeric") +
            ", xOffset=" + std::to_string(xOffset) +
            ", yAlign=" + (yAlign == YAlignment::Center ? "center" :
                yAlign == YAlignment::Top ? "top" :
                yAlign == YAlignment::Bottom ? "bottom" : "numeric") +
            ", yOffset=" + std::to_string(yOffset)));
#endif
}

bool OptionsMenuBuilder::loadConfig(const std::string& xmlPath)
{
    Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Attempting to load layoutMenu.xml from: " + xmlPath);
    if (!std::filesystem::exists(xmlPath)) {
        Logger::write(Logger::ZONE_WARNING, "OptionsMenuBuilder", "layoutMenu.xml not found at " + xmlPath + "; using defaults");
        return false;
    }
    xmlDocPtr doc = xmlReadFile(xmlPath.c_str(), nullptr, 0);
    if (!doc) {
        Logger::write(Logger::ZONE_WARNING, "OptionsMenuBuilder", "Failed to parse layoutMenu.xml: " + xmlPath);
        return false;
    }
    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root || xmlStrcmp(root->name, (const xmlChar*)"layoutmenu") != 0) {
        Logger::write(Logger::ZONE_WARNING, "OptionsMenuBuilder", "Invalid root tag; expected <layoutmenu>, found " +
            std::string(root ? reinterpret_cast<const char*>(root->name) : "none"));
        xmlFreeDoc(doc);
        return false;
    }
    Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsing layoutMenu.xml with root <layoutmenu>");
    images_.clear();
    for (xmlNodePtr node = root->children; node; node = node->next) {
        if (node->type != XML_ELEMENT_NODE) continue;
        std::string nodeName = reinterpret_cast<const char*>(node->name);
        xmlChar* content = xmlNodeGetContent(node);
        std::string value = content ? reinterpret_cast<const char*>(content) : "";
        xmlFree(content);
        if (nodeName == "image") {
            ImageConfig img;
            xmlChar* src = xmlGetProp(node, (const xmlChar*)"src");
            img.src = src ? reinterpret_cast<const char*>(src) : "";
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Raw src value: " + img.src);
            xmlFree(src);
            xmlChar* animated = xmlGetProp(node, (const xmlChar*)"animated");
            img.isAnimated = animated && std::string(reinterpret_cast<const char*>(animated)) == "true";
            xmlFree(animated);
            xmlChar* isMenu = xmlGetProp(node, (const xmlChar*)"isMenu");
            img.isMenu = isMenu && std::string(reinterpret_cast<const char*>(isMenu)) == "true";
            xmlFree(isMenu);
            xmlChar* x = xmlGetProp(node, (const xmlChar*)"x");
            if (x) {
                parseAlignment(reinterpret_cast<const char*>(x), img.xAlign, img.xOffset, img.yAlign, img.yOffset);
                img.x = img.xOffset;
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed image x: " + std::string(reinterpret_cast<const char*>(x)));
            }
            xmlFree(x);
            xmlChar* y = xmlGetProp(node, (const xmlChar*)"y");
            if (y) {
                XAlignment tempXAlign;
                YAlignment tempYAlign;
                int tempXOffset, tempYOffset;
                parseAlignment(reinterpret_cast<const char*>(y), tempXAlign, tempXOffset, tempYAlign, tempYOffset);
                img.yAlign = tempYAlign;
                img.yOffset = tempYOffset;
                img.y = img.yOffset;
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed image y: " + std::string(reinterpret_cast<const char*>(y)));
            }
            xmlFree(y);
            xmlChar* width = xmlGetProp(node, (const xmlChar*)"width");
            img.width = width ? std::stoi(reinterpret_cast<const char*>(width)) : 100;
            xmlFree(width);
            xmlChar* height = xmlGetProp(node, (const xmlChar*)"height");
            img.height = height ? std::stoi(reinterpret_cast<const char*>(height)) : 100;
            xmlFree(height);
            xmlChar* scale = xmlGetProp(node, (const xmlChar*)"scale");
            img.scale = scale ? reinterpret_cast<const char*>(scale) : "stretch";
            xmlFree(scale);
            if (!img.src.empty() || img.isMenu) { // Allow isMenu images even if src is empty
                images_.push_back(img);
#ifdef _DEBUG
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder",
                    "Parsed image: src=" + img.src + ", animated=" + (img.isAnimated ? "true" : "false") +
                    ", isMenu=" + (img.isMenu ? "true" : "false") +
                    ", xAlign=" + (img.xAlign == XAlignment::Center ? "center" :
                        img.xAlign == XAlignment::Left ? "left" :
                        img.xAlign == XAlignment::Right ? "right" : "numeric") +
                    ", xOffset=" + std::to_string(img.xOffset) +
                    ", yAlign=" + (img.yAlign == YAlignment::Center ? "center" :
                        img.yAlign == YAlignment::Top ? "top" :
                        img.yAlign == YAlignment::Bottom ? "bottom" : "numeric") +
                    ", yOffset=" + std::to_string(img.yOffset) +
                    ", width=" + std::to_string(img.width) + ", height=" + std::to_string(img.height) +
                    ", scale=" + img.scale);
#endif
            }
            else {
                Logger::write(Logger::ZONE_WARNING, "OptionsMenuBuilder", "Skipping image with empty src");
            }
        }
        else if (nodeName == "backgroundColor") {
            backgroundColor_ = parseColor(value);
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed backgroundColor: " + value);
        }
        else if (nodeName == "fontPath") {
            fontPath_ = value;
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed fontPath: " + value);
        }
        else if (nodeName == "title") {
            xmlChar* size = xmlGetProp(node, (const xmlChar*)"textSize");
            xmlChar* color = xmlGetProp(node, (const xmlChar*)"color");
            xmlChar* x = xmlGetProp(node, (const xmlChar*)"x");
            xmlChar* y = xmlGetProp(node, (const xmlChar*)"y");
            if (size) {
                titleFontSize_ = std::stoi(reinterpret_cast<const char*>(size));
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed title textSize: " + std::string(reinterpret_cast<const char*>(size)));
            }
            if (color) {
                titleColor_ = parseColor(reinterpret_cast<const char*>(color));
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed title color: " + std::string(reinterpret_cast<const char*>(color)));
            }
            if (x) {
                XAlignment tempXAlign;
                YAlignment tempYAlign;
                int tempXOffset, tempYOffset;
                parseAlignment(reinterpret_cast<const char*>(x), tempXAlign, tempXOffset, tempYAlign, tempYOffset);
                titleXAlignment_ = tempXAlign;
                titleXOffset_ = tempXOffset;
                titleX_ = tempXOffset;
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed title x: " + std::string(reinterpret_cast<const char*>(x)));
            }
            if (y) {
                XAlignment tempXAlign;
                YAlignment tempYAlign;
                int tempXOffset, tempYOffset;
                parseAlignment(reinterpret_cast<const char*>(y), tempXAlign, tempXOffset, tempYAlign, tempYOffset);
                titleYAlignment_ = tempYAlign;
                titleYOffset_ = tempYOffset;
                titleY_ = tempYOffset;
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed title y: " + std::string(reinterpret_cast<const char*>(y)));
            }
            xmlFree(size);
            xmlFree(color);
            xmlFree(x);
            xmlFree(y);
#ifdef _DEBUG
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed title: size=" + std::to_string(titleFontSize_) +
                ", xAlign=" + (titleXAlignment_ == XAlignment::Center ? "center" :
                    titleXAlignment_ == XAlignment::Left ? "left" :
                    titleXAlignment_ == XAlignment::Right ? "right" : "numeric") +
                ", xOffset=" + std::to_string(titleXOffset_) +
                ", yAlign=" + (titleYAlignment_ == YAlignment::Center ? "center" :
                    titleYAlignment_ == YAlignment::Top ? "top" :
                    titleYAlignment_ == YAlignment::Bottom ? "bottom" : "numeric") +
                ", yOffset=" + std::to_string(titleYOffset_));
#endif
        }
        else if (nodeName == "Text") {
            xmlChar* size = xmlGetProp(node, (const xmlChar*)"textSize");
            xmlChar* color = xmlGetProp(node, (const xmlChar*)"color");
            xmlChar* selectionBarColor = xmlGetProp(node, (const xmlChar*)"selectionBarColor");
            xmlChar* titleBarColor = xmlGetProp(node, (const xmlChar*)"titleBarColor");
            xmlChar* subtitleUnderlineColor = xmlGetProp(node, (const xmlChar*)"subtitleUnderlineColor");
            xmlChar* space = xmlGetProp(node, (const xmlChar*)"spaceBetween");
            xmlChar* x = xmlGetProp(node, (const xmlChar*)"x");
            xmlChar* y = xmlGetProp(node, (const xmlChar*)"y");
            xmlChar* selectionBarPadding = xmlGetProp(node, (const xmlChar*)"selectionBarPadding");
            xmlChar* selectionBarHeight = xmlGetProp(node, (const xmlChar*)"selectionBarHeight");
            xmlChar* titleSpacing = xmlGetProp(node, (const xmlChar*)"titleSpacing");
            xmlChar* selectionBarYOffset = xmlGetProp(node, (const xmlChar*)"selectionBarYOffset");
            if (size) {
                optionFontSize_ = std::stoi(reinterpret_cast<const char*>(size));
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed Text textSize: " + std::string(reinterpret_cast<const char*>(size)));
            }
            if (color) {
                optionColor_ = parseColor(reinterpret_cast<const char*>(color));
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed Text color: " + std::string(reinterpret_cast<const char*>(color)));
            }
            if (selectionBarColor && !selectionBarColorParsed_) {
                selectionBarColor_ = parseColor(reinterpret_cast<const char*>(selectionBarColor));
                selectionBarColorParsed_ = true; // Mark as parsed
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed Text selectionBarColor: " + std::string(reinterpret_cast<const char*>(selectionBarColor)));
            }
            if (titleBarColor) {
                titleBarColor_ = parseColor(reinterpret_cast<const char*>(titleBarColor));
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed Text titleBarColor: " + std::string(reinterpret_cast<const char*>(titleBarColor)));
            }
            if (subtitleUnderlineColor) {
                subtitleUnderlineColor_ = parseColor(reinterpret_cast<const char*>(subtitleUnderlineColor));
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed Text subtitleUnderlineColor: " + std::string(reinterpret_cast<const char*>(subtitleUnderlineColor)));
            }
            if (space) {
                spaceBetweenText_ = std::stoi(reinterpret_cast<const char*>(space));
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed Text spaceBetween: " + std::string(reinterpret_cast<const char*>(space)));
            }
            if (x) {
                XAlignment tempXAlign;
                YAlignment tempYAlign;
                int tempXOffset, tempYOffset;
                parseAlignment(reinterpret_cast<const char*>(x), tempXAlign, tempXOffset, tempYAlign, tempYOffset);
                menuXAlignment_ = tempXAlign;
                menuXOffset_ = tempXOffset;
                menuX_ = tempXOffset;
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed Text x: " + std::string(reinterpret_cast<const char*>(x)));
            }
            if (y) {
                XAlignment tempXAlign;
                YAlignment tempYAlign;
                int tempXOffset, tempYOffset;
                parseAlignment(reinterpret_cast<const char*>(y), tempXAlign, tempXOffset, tempYAlign, tempYOffset);
                menuYAlignment_ = tempYAlign;
                menuYOffset_ = tempYOffset;
                menuY_ = tempYOffset;
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed Text y: " + std::string(reinterpret_cast<const char*>(y)));
            }
            if (selectionBarPadding) {
                try {
                    selectionBarPadding_ = std::stoi(reinterpret_cast<const char*>(selectionBarPadding));
                    Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed Text selectionBarPadding: " + std::string(reinterpret_cast<const char*>(selectionBarPadding)));
                }
                catch (const std::exception&) {
                    Logger::write(Logger::ZONE_WARNING, "OptionsMenuBuilder", "Invalid selectionBarPadding: " + std::string(reinterpret_cast<const char*>(selectionBarPadding)));
                }
            }
            if (selectionBarHeight) {
                try {
                    selectionBarHeight_ = std::stoi(reinterpret_cast<const char*>(selectionBarHeight));
                    Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed Text selectionBarHeight: " + std::string(reinterpret_cast<const char*>(selectionBarHeight)));
                }
                catch (const std::exception&) {
                    Logger::write(Logger::ZONE_WARNING, "OptionsMenuBuilder", "Invalid selectionBarHeight: " + std::string(reinterpret_cast<const char*>(selectionBarHeight)));
                }
            }
            if (titleSpacing) {
                try {
                    titleSpacing_ = std::stoi(reinterpret_cast<const char*>(titleSpacing));
                    Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed Text titleSpacing: " + std::string(reinterpret_cast<const char*>(titleSpacing)));
                }
                catch (const std::exception&) {
                    Logger::write(Logger::ZONE_WARNING, "OptionsMenuBuilder", "Invalid titleSpacing: " + std::string(reinterpret_cast<const char*>(titleSpacing)));
                }
            }
            if (selectionBarYOffset) {
                try {
                    selectionBarYOffset_ = std::stoi(reinterpret_cast<const char*>(selectionBarYOffset));
                    Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed Text selectionBarYOffset: " + std::string(reinterpret_cast<const char*>(selectionBarYOffset)));
                }
                catch (const std::exception&) {
                    Logger::write(Logger::ZONE_WARNING, "OptionsMenuBuilder", "Invalid selectionBarYOffset: " + std::string(reinterpret_cast<const char*>(selectionBarYOffset)));
                }
            }
            xmlFree(size);
            xmlFree(color);
            xmlFree(selectionBarColor);
            xmlFree(titleBarColor);
            xmlFree(subtitleUnderlineColor);
            xmlFree(space);
            xmlFree(x);
            xmlFree(y);
            xmlFree(selectionBarPadding);
            xmlFree(selectionBarHeight);
            xmlFree(titleSpacing);
            xmlFree(selectionBarYOffset);
#ifdef _DEBUG
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed Text: size=" + std::to_string(optionFontSize_) +
                ", xAlign=" + (menuXAlignment_ == XAlignment::Center ? "center" :
                    menuXAlignment_ == XAlignment::Left ? "left" :
                    menuXAlignment_ == XAlignment::Right ? "right" : "numeric") +
                ", xOffset=" + std::to_string(menuXOffset_) +
                ", yAlign=" + (menuYAlignment_ == YAlignment::Center ? "center" :
                    menuYAlignment_ == YAlignment::Top ? "top" :
                    menuYAlignment_ == YAlignment::Bottom ? "bottom" : "numeric") +
                ", yOffset=" + std::to_string(menuYOffset_));
#endif
        }
        else if (nodeName == "selectionBarColor") {
            selectionBarColor_ = parseColor(value);
            selectionBarColorParsed_ = true;
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed selectionBarColor: " + value);
        }
        else if (nodeName == "window") {
            xmlChar* width = xmlGetProp(node, (const xmlChar*)"width");
            xmlChar* height = xmlGetProp(node, (const xmlChar*)"height");
            xmlChar* scale = xmlGetProp(node, (const xmlChar*)"scale");
            if (width) {
                try {
                    windowWidth_ = std::stoi(reinterpret_cast<const char*>(width));
                    Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed window width: " + std::string(reinterpret_cast<const char*>(width)));
                }
                catch (const std::exception&) {
                    Logger::write(Logger::ZONE_WARNING, "OptionsMenuBuilder", "Invalid window width: " + std::string(reinterpret_cast<const char*>(width)));
                }
                xmlFree(width);
            }
            if (height) {
                try {
                    windowHeight_ = std::stoi(reinterpret_cast<const char*>(height));
                    Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed window height: " + std::string(reinterpret_cast<const char*>(height)));
                }
                catch (const std::exception&) {
                    Logger::write(Logger::ZONE_WARNING, "OptionsMenuBuilder", "Invalid window height: " + std::string(reinterpret_cast<const char*>(height)));
                }
                xmlFree(height);
            }
            if (scale) {
                scale_ = reinterpret_cast<const char*>(scale);
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed window scale: " + scale_);
                xmlFree(scale);
            }
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed window: width=" + std::to_string(windowWidth_) +
                ", height=" + std::to_string(windowHeight_) + ", scale=" + (scale_.empty() ? "none" : scale_));
        }
        else if (nodeName == "menuLang") {
            for (xmlNodePtr entryNode = node->children; entryNode; entryNode = entryNode->next) {
                if (entryNode->type != XML_ELEMENT_NODE || std::string(reinterpret_cast<const char*>(entryNode->name)) != "entry") continue;
                xmlChar* key = xmlGetProp(entryNode, (const xmlChar*)"key");
                xmlChar* text = xmlGetProp(entryNode, (const xmlChar*)"text");
                if (key && text) {
                    std::string keyStr = reinterpret_cast<const char*>(key);
                    std::string textStr = reinterpret_cast<const char*>(text);
                    if (keyStr == "main") mainTitleText_ = textStr;
                    else if (keyStr == "settings") settingsTitleText_ = textStr;
                    else if (keyStr == "controls") controlsTitleText_ = textStr;
                    else if (keyStr == "actions") actionsTitleText_ = textStr;
                    else if (keyStr == "layout") layoutTitleText_ = textStr;
                    else if (keyStr == "save_exit") saveExitText_ = textStr;
                    Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed menuLang entry: key=" + keyStr + ", text=" + textStr);
                }
                xmlFree(key);
                xmlFree(text);
            }
        }
        // Parsing new standalone elements
        else if (nodeName == "fontColor") {
            fontColor_ = parseColor(value);
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed fontColor: " + value);
        }
        else if (nodeName == "selectionBarColor") {
            selectionBarColor_ = parseColor(value);
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed selectionBarColor: " + value);
        }
        else if (nodeName == "titleBarColor") {
            titleBarColor_ = parseColor(value);
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed titleBarColor: " + value);
        }
        else if (nodeName == "subtitleUnderlineColor") {
            subtitleUnderlineColor_ = parseColor(value);
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed subtitleUnderlineColor: " + value);
        }
        else if (nodeName == "spaceBetweenText") {
            try {
                spaceBetweenText_ = std::stoi(value);
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed spaceBetweenText: " + value);
            }
            catch (const std::exception&) {
                Logger::write(Logger::ZONE_WARNING, "OptionsMenuBuilder", "Invalid spaceBetweenText: " + value);
            }
        }
        else if (nodeName == "titleSpacing") {
            try {
                titleSpacing_ = std::stoi(value);
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed titleSpacing: " + value);
            }
            catch (const std::exception&) {
                Logger::write(Logger::ZONE_WARNING, "OptionsMenuBuilder", "Invalid titleSpacing: " + value);
            }
        }
        else if (nodeName == "selectionBarYOffset") {
            try {
                selectionBarYOffset_ = std::stoi(value);
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed selectionBarYOffset: " + value);
            }
            catch (const std::exception&) {
                Logger::write(Logger::ZONE_WARNING, "OptionsMenuBuilder", "Invalid selectionBarYOffset: " + value);
            }
        }
        else if (nodeName == "selectionBarPadding") {
            try {
                selectionBarPadding_ = std::stoi(value);
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed selectionBarPadding: " + value);
            }
            catch (const std::exception&) {
                Logger::write(Logger::ZONE_WARNING, "OptionsMenuBuilder", "Invalid selectionBarPadding: " + value);
            }
        }
        else if (nodeName == "selectionBarHeight") {
            try {
                selectionBarHeight_ = std::stoi(value);
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed selectionBarHeight: " + value);
            }
            catch (const std::exception&) {
                Logger::write(Logger::ZONE_WARNING, "OptionsMenuBuilder", "Invalid selectionBarHeight: " + value);
            }
        }
        else if (nodeName == "menuXAlignment") {
            if (value == "center") menuXAlignment_ = XAlignment::Center;
            else if (value == "left") menuXAlignment_ = XAlignment::Left;
            else if (value == "right") menuXAlignment_ = XAlignment::Right;
            else {
                Logger::write(Logger::ZONE_WARNING, "OptionsMenuBuilder", "Invalid menuXAlignment: " + value);
            }
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed menuXAlignment: " + value);
        }
        else if (nodeName == "menuYAlignment") {
            if (value == "center") menuYAlignment_ = YAlignment::Center;
            else if (value == "top") menuYAlignment_ = YAlignment::Top;
            else if (value == "bottom") menuYAlignment_ = YAlignment::Bottom;
            else {
                Logger::write(Logger::ZONE_WARNING, "OptionsMenuBuilder", "Invalid menuYAlignment: " + value);
            }
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed menuYAlignment: " + value);
        }
        else if (nodeName == "menuXOffset") {
            try {
                menuXOffset_ = std::stoi(value);
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed menuXOffset: " + value);
            }
            catch (const std::exception&) {
                Logger::write(Logger::ZONE_WARNING, "OptionsMenuBuilder", "Invalid menuXOffset: " + value);
            }
        }
        else if (nodeName == "menuYOffset") {
            try {
                menuYOffset_ = std::stoi(value);
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed menuYOffset: " + value);
            }
            catch (const std::exception&) {
                Logger::write(Logger::ZONE_WARNING, "OptionsMenuBuilder", "Invalid menuYOffset: " + value);
            }
        }
    }
    Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed " + std::to_string(images_.size()) + " images");
    xmlFreeDoc(doc);
    return true;
}


SDL_Color OptionsMenuBuilder::parseColor(const std::string& hex) {
    SDL_Color color = { 255, 255, 255, 255 };
    if (hex.length() >= 6) {
        try {
            color.r = static_cast<Uint8>(std::stoi(hex.substr(0, 2), nullptr, 16));
            color.g = static_cast<Uint8>(std::stoi(hex.substr(2, 2), nullptr, 16));
            color.b = static_cast<Uint8>(std::stoi(hex.substr(4, 2), nullptr, 16));
            if (hex.length() >= 8) {
                color.a = static_cast<Uint8>(std::stoi(hex.substr(6, 2), nullptr, 16)); // Parse alpha
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed RGBA color: " + hex +
                    ", alpha=" + std::to_string(color.a));
            }
            else {
                color.a = 255; // Default to opaque for 6-char codes
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed RGB color: " + hex + ", default alpha=255");
            }
        }
        catch (const std::exception& e) {
            Logger::write(Logger::ZONE_WARNING, "OptionsMenuBuilder", "Invalid color format: " + hex + ", error: " + e.what());
        }
    }
    else {
        Logger::write(Logger::ZONE_WARNING, "OptionsMenuBuilder", "Color code too short: " + hex);
    }
    return color;
}