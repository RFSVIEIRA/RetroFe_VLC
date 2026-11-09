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
#include "OptionsMenuBuilder.h"
#include "../utility/Log.h"
#include "../Utility/Utils.h"
#include <filesystem>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <regex>
#include <algorithm>
#include <functional>

void OptionsMenuBuilder::applyDefaultConfig() {
    images_.clear();
    containers_.clear();
    variables_.clear();
    includedFiles_.clear();

    windowWidth_ = 1920;
    windowHeight_ = 1080;
    scale_ = "fit";
    backgroundColor_ = { 0, 0, 0, 176 };          // 000000B0 translucent backdrop dimmer
    panelColor_ = { 22, 26, 40, 240 };             // 161A28F0 dark chrome-slate modal card
    borderColor_ = { 255, 191, 64, 255 };          // #FFBF40 golden amber from logo "FE" / cone
    borderWidth_ = 3;
    cornerRadius_ = 24;
    glowColor_ = { 255, 191, 64, 255 };            // #FFBF40 golden amber glow
    glowStrength_ = 60;
    modalWidth_ = 500;
    modalHeight_ = 0;
    modalAutoScale_ = true;
    modalMaxHeight_ = 820;
    modalMinHeight_ = 260;
    modalMaxWidth_ = 0;
    modalMinWidth_ = 0;
    modalXOffset_ = 0;
    modalYOffset_ = 0;

    fontPath_ = "fonts/verdana.ttf";
    titleFontSize_ = 26;
    titleColor_ = { 255, 255, 255, 255 };
    titleXAlignment_ = XAlignment::Center;
    titleYAlignment_ = YAlignment::Top;
    titleXOffset_ = 0;
    titleYOffset_ = 165;
    titleX_ = 0;
    titleY_ = 165;
    titleXOrigin_ = 0.5f;
    hasExplicitTitleXOrigin_ = true;
    titleYOrigin_ = 0.0f;
    hasExplicitTitleYOrigin_ = true;
    titleSpacing_ = 20;
    subtitleUnderlineColor_ = { 255, 191, 64, 128 }; // #FFBF4080

    optionFontSize_ = 18;
    optionColor_ = { 255, 255, 255, 255 };
    fontColor_ = { 255, 255, 255, 255 };
    selectedColor_ = { 255, 255, 255, 255 };
    selectionBarColor_ = { 255, 191, 64, 80 };       // #FFBF4050
    selectionBarColorParsed_ = true;
    selectionBarPadding_ = 14;
    selectionBarHeight_ = 44;
    selectionBarYOffset_ = -18;
    spaceBetweenText_ = 44;

    separatorColor_ = { 255, 191, 64, 64 };          // #FFBF4040
    separatorHeight_ = 1;
    separatorWidth_ = 300;
    separatorYOffset_ = 4;
    separatorXAlignment_ = XAlignment::Center;

    menuXAlignment_ = XAlignment::Center;
    menuXOffset_ = -140;
    menuXOrigin_ = 0.0f;                             // left
    hasExplicitMenuXOrigin_ = true;
    menuYAlignment_ = YAlignment::Top;
    menuYOffset_ = 230;
    menuYOrigin_ = 0.0f;                             // top
    hasExplicitMenuYOrigin_ = true;

    iconFolder_ = "optMenuImages/icons";
    buttonFolder_ = "configs/ButtomBar/artwork/xbox";
    buttonImages_["a"] = "configs/ButtomBar/artwork/xbox/a.png";
    buttonImages_["select"] = "configs/ButtomBar/artwork/xbox/a.png";
    buttonImages_["b"] = "configs/ButtomBar/artwork/xbox/b.png";
    buttonImages_["back"] = "configs/ButtomBar/artwork/xbox/b.png";
    buttonImages_["all"] = "configs/ButtomBar/artwork/xbox/all.png";
    buttonImages_["directions"] = "configs/ButtomBar/artwork/xbox/all.png";
}

OptionsMenuBuilder::OptionsMenuBuilder(const std::string& xmlPath) {
    applyDefaultConfig();
    if (!xmlPath.empty()) {
        if (!loadConfig(xmlPath)) {
            applyDefaultConfig();
        }
    }
}

static float parseOriginValue(const std::string& str, float defaultVal = 0.0f) {
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    size_t first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return defaultVal;
    size_t last = s.find_last_not_of(" \t\r\n");
    s = s.substr(first, (last - first + 1));

    if (s == "left" || s == "top" || s == "esquerda" || s == "topo" || s == "cima") return 0.0f;
    if (s == "center" || s == "centro" || s == "middle" || s == "meio") return 0.5f;
    if (s == "right" || s == "bottom" || s == "direita" || s == "baixo") return 1.0f;
    try {
        return std::stof(s);
    } catch (...) {
        return defaultVal;
    }
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

    std::regex alignRegex(R"(^(center|centro|left|esquerda|right|direita|top|topo|cima|bottom|baixo)([+-]\d+)?$)", std::regex::icase);
    std::smatch match;
    if (std::regex_match(value, match, alignRegex)) {
        std::string align = match[1].str();
        std::string offsetStr = match[2].str();
        std::transform(align.begin(), align.end(), align.begin(), ::tolower);

        if (align == "center" || align == "centro") {
            xAlign = XAlignment::Center;
            yAlign = YAlignment::Center;
        }
        else if (align == "left" || align == "esquerda") {
            xAlign = XAlignment::Left;
        }
        else if (align == "right" || align == "direita") {
            xAlign = XAlignment::Right;
        }
        else if (align == "top" || align == "topo" || align == "cima") {
            yAlign = YAlignment::Top;
        }
        else if (align == "bottom" || align == "baixo") {
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
    Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Loading layout file: " + xmlPath);

    // Reset previous state
    images_.clear();
    containers_.clear();
    variables_.clear();
    includedFiles_.clear();
    selectionBarColorParsed_ = false;

    // Determine base path (folder containing the XML file)
    std::filesystem::path p(xmlPath);
    basePath_ = p.has_parent_path() ? p.parent_path().string() : "";

    // Load XML document
    xmlDocPtr doc = xmlReadFile(xmlPath.c_str(), nullptr, XML_PARSE_NOBLANKS);
    if (!doc) {
        Logger::write(Logger::ZONE_ERROR, "OptionsMenuBuilder", "Failed to load XML: " + xmlPath);
        return false;
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root) {
        Logger::write(Logger::ZONE_ERROR, "OptionsMenuBuilder", "XML has no root element");
        xmlFreeDoc(doc);
        return false;
    }

    std::string rootName = (const char*)root->name;
    std::transform(rootName.begin(), rootName.end(), rootName.begin(), ::tolower);

    xmlNodePtr menuRoot = root;
    if (rootName == "layout") {
        for (xmlNodePtr ch = root->children; ch; ch = ch->next) {
            if (ch->type != XML_ELEMENT_NODE) continue;
            std::string cName = (const char*)ch->name;
            std::transform(cName.begin(), cName.end(), cName.begin(), ::tolower);
            if (cName == "optionsmenu" || cName == "layoutmenu" || cName == "options") {
                menuRoot = ch;
                break;
            }
        }
        if (menuRoot == root) {
            // Process layout includes for optionsMenu/layoutMenu
            for (xmlNodePtr node = root->children; node; ) {
                xmlNodePtr nextNode = node->next;
                if (node->type == XML_ELEMENT_NODE && strcmp((const char*)node->name, "include") == 0) {
                    processInclude(node, doc, root);
                    xmlUnlinkNode(node);
                    xmlFreeNode(node);
                }
                node = nextNode;
            }
            // Check again among children
            for (xmlNodePtr ch = root->children; ch; ch = ch->next) {
                if (ch->type != XML_ELEMENT_NODE) continue;
                std::string cName = (const char*)ch->name;
                std::transform(cName.begin(), cName.end(), cName.begin(), ::tolower);
                if (cName == "optionsmenu" || cName == "layoutmenu" || cName == "options") {
                    menuRoot = ch;
                    break;
                }
            }
        }
        if (menuRoot == root) {
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "No <optionsMenu> or <layoutMenu> found in layout.xml");
            xmlFreeDoc(doc);
            return false;
        }
    }
    else if (rootName != "layoutmenu" && rootName != "optionsmenu" && rootName != "options") {
        Logger::write(Logger::ZONE_ERROR, "OptionsMenuBuilder", "Root element must be <optionsMenu> or <layoutmenu>, got <" + rootName + ">");
        xmlFreeDoc(doc);
        return false;
    }

    // Confirmed menuRoot: reset modal properties so full-screen XMLs (e.g. FullScreen.xml) don't inherit modal bounds
    panelColor_ = { 0, 0, 0, 0 };
    borderColor_ = { 0, 0, 0, 0 };
    borderWidth_ = 2;
    cornerRadius_ = 0;
    glowColor_ = { 0, 0, 0, 0 };
    glowStrength_ = 0;
    modalWidth_ = 0;
    modalHeight_ = 0;
    modalAutoScale_ = false;
    modalMaxHeight_ = 0;
    modalMinHeight_ = 0;
    modalMaxWidth_ = 0;
    modalMinWidth_ = 0;
    modalXOffset_ = 0;
    modalYOffset_ = 0;
    backgroundColor_ = { 0, 0, 0, 255 };
    selectionBarColorParsed_ = false;

    // Parse attributes on menuRoot (<optionsMenu width="..." height="..." scale="..." backgroundColor="...">)
    xmlChar* rWidth = xmlGetProp(menuRoot, (const xmlChar*)"width");
    if (rWidth) {
        try { windowWidth_ = std::stoi(reinterpret_cast<const char*>(rWidth)); } catch (...) {}
        xmlFree(rWidth);
    }
    xmlChar* rHeight = xmlGetProp(menuRoot, (const xmlChar*)"height");
    if (rHeight) {
        try { windowHeight_ = std::stoi(reinterpret_cast<const char*>(rHeight)); } catch (...) {}
        xmlFree(rHeight);
    }
    if (windowWidth_ <= 0) {
        xmlChar* lWidth = xmlGetProp(root, (const xmlChar*)"width");
        if (lWidth) { try { windowWidth_ = std::stoi(reinterpret_cast<const char*>(lWidth)); } catch (...) {} xmlFree(lWidth); }
    }
    if (windowHeight_ <= 0) {
        xmlChar* lHeight = xmlGetProp(root, (const xmlChar*)"height");
        if (lHeight) { try { windowHeight_ = std::stoi(reinterpret_cast<const char*>(lHeight)); } catch (...) {} xmlFree(lHeight); }
    }
    xmlChar* rScale = xmlGetProp(menuRoot, (const xmlChar*)"scale");
    if (!rScale) rScale = xmlGetProp(menuRoot, (const xmlChar*)"scaleMode");
    if (!rScale) rScale = xmlGetProp(menuRoot, (const xmlChar*)"scalemode");
    if (rScale) {
        scale_ = reinterpret_cast<const char*>(rScale);
        xmlFree(rScale);
    }
    xmlChar* rBg = xmlGetProp(menuRoot, (const xmlChar*)"backgroundColor");
    if (!rBg) rBg = xmlGetProp(menuRoot, (const xmlChar*)"backgroundcolor");
    if (rBg) {
        backgroundColor_ = parseColor(reinterpret_cast<const char*>(rBg));
        xmlFree(rBg);
    }
    xmlChar* rFont = xmlGetProp(menuRoot, (const xmlChar*)"font");
    if (!rFont) rFont = xmlGetProp(menuRoot, (const xmlChar*)"fontPath");
    if (!rFont) rFont = xmlGetProp(root, (const xmlChar*)"font");
    if (rFont) {
        fontPath_ = reinterpret_cast<const char*>(rFont);
        xmlFree(rFont);
    }

    xmlChar* rPanel = xmlGetProp(menuRoot, (const xmlChar*)"panelColor");
    if (!rPanel) rPanel = xmlGetProp(menuRoot, (const xmlChar*)"panel");
    if (rPanel) { panelColor_ = parseColor(reinterpret_cast<const char*>(rPanel)); xmlFree(rPanel); }

    xmlChar* rBorder = xmlGetProp(menuRoot, (const xmlChar*)"borderColor");
    if (!rBorder) rBorder = xmlGetProp(menuRoot, (const xmlChar*)"border");
    if (rBorder) { borderColor_ = parseColor(reinterpret_cast<const char*>(rBorder)); xmlFree(rBorder); }

    xmlChar* rBorderW = xmlGetProp(menuRoot, (const xmlChar*)"borderWidth");
    if (!rBorderW) rBorderW = xmlGetProp(menuRoot, (const xmlChar*)"borderThickness");
    if (rBorderW) { try { borderWidth_ = std::stoi(reinterpret_cast<const char*>(rBorderW)); } catch (...) {} xmlFree(rBorderW); }

    xmlChar* rRadius = xmlGetProp(menuRoot, (const xmlChar*)"cornerRadius");
    if (!rRadius) rRadius = xmlGetProp(menuRoot, (const xmlChar*)"radius");
    if (rRadius) { try { cornerRadius_ = std::stoi(reinterpret_cast<const char*>(rRadius)); } catch (...) {} xmlFree(rRadius); }

    xmlChar* rGlow = xmlGetProp(menuRoot, (const xmlChar*)"glowColor");
    if (rGlow) { glowColor_ = parseColor(reinterpret_cast<const char*>(rGlow)); xmlFree(rGlow); }

    xmlChar* rGlowStr = xmlGetProp(menuRoot, (const xmlChar*)"glowStrength");
    if (!rGlowStr) rGlowStr = xmlGetProp(menuRoot, (const xmlChar*)"glow");
    if (rGlowStr) { try { glowStrength_ = std::stoi(reinterpret_cast<const char*>(rGlowStr)); } catch (...) {} xmlFree(rGlowStr); }

    xmlChar* rModalW = xmlGetProp(menuRoot, (const xmlChar*)"modalWidth");
    if (!rModalW) rModalW = xmlGetProp(menuRoot, (const xmlChar*)"cardWidth");
    if (rModalW) {
        std::string sW = reinterpret_cast<const char*>(rModalW);
        std::transform(sW.begin(), sW.end(), sW.begin(), ::tolower);
        if (sW == "auto") { modalAutoScale_ = true; modalWidth_ = 0; }
        else { try { modalWidth_ = std::stoi(sW); } catch (...) {} }
        xmlFree(rModalW);
    }

    xmlChar* rModalH = xmlGetProp(menuRoot, (const xmlChar*)"modalHeight");
    if (!rModalH) rModalH = xmlGetProp(menuRoot, (const xmlChar*)"cardHeight");
    if (rModalH) {
        std::string sH = reinterpret_cast<const char*>(rModalH);
        std::transform(sH.begin(), sH.end(), sH.begin(), ::tolower);
        if (sH == "auto") { modalAutoScale_ = true; modalHeight_ = 0; }
        else { try { modalHeight_ = std::stoi(sH); } catch (...) {} }
        xmlFree(rModalH);
    }

    xmlChar* rAuto = xmlGetProp(menuRoot, (const xmlChar*)"modalAutoScale");
    if (!rAuto) rAuto = xmlGetProp(menuRoot, (const xmlChar*)"modalautoscale");
    if (!rAuto) rAuto = xmlGetProp(menuRoot, (const xmlChar*)"autoScale");
    if (!rAuto) rAuto = xmlGetProp(menuRoot, (const xmlChar*)"autoscale");
    if (rAuto) {
        std::string sA = reinterpret_cast<const char*>(rAuto);
        std::transform(sA.begin(), sA.end(), sA.begin(), ::tolower);
        if (sA == "true" || sA == "yes" || sA == "1") modalAutoScale_ = true;
        xmlFree(rAuto);
    }

    xmlChar* rMaxH = xmlGetProp(menuRoot, (const xmlChar*)"modalMaxHeight");
    if (!rMaxH) rMaxH = xmlGetProp(menuRoot, (const xmlChar*)"modalmaxheight");
    if (!rMaxH) rMaxH = xmlGetProp(menuRoot, (const xmlChar*)"modalMaxH");
    if (!rMaxH) rMaxH = xmlGetProp(menuRoot, (const xmlChar*)"modalmaxh");
    if (rMaxH) {
        try { modalMaxHeight_ = std::stoi(reinterpret_cast<const char*>(rMaxH)); modalAutoScale_ = true; } catch (...) {}
        xmlFree(rMaxH);
    }

    xmlChar* rMinH = xmlGetProp(menuRoot, (const xmlChar*)"modalMinHeight");
    if (!rMinH) rMinH = xmlGetProp(menuRoot, (const xmlChar*)"modalminheight");
    if (!rMinH) rMinH = xmlGetProp(menuRoot, (const xmlChar*)"modalMinH");
    if (!rMinH) rMinH = xmlGetProp(menuRoot, (const xmlChar*)"modalminh");
    if (rMinH) {
        try { modalMinHeight_ = std::stoi(reinterpret_cast<const char*>(rMinH)); modalAutoScale_ = true; } catch (...) {}
        xmlFree(rMinH);
    }

    xmlChar* rMaxW = xmlGetProp(menuRoot, (const xmlChar*)"modalMaxWidth");
    if (!rMaxW) rMaxW = xmlGetProp(menuRoot, (const xmlChar*)"modalmaxwidth");
    if (!rMaxW) rMaxW = xmlGetProp(menuRoot, (const xmlChar*)"modalMaxW");
    if (!rMaxW) rMaxW = xmlGetProp(menuRoot, (const xmlChar*)"modalmaxw");
    if (rMaxW) {
        try { modalMaxWidth_ = std::stoi(reinterpret_cast<const char*>(rMaxW)); } catch (...) {}
        xmlFree(rMaxW);
    }

    xmlChar* rMinW = xmlGetProp(menuRoot, (const xmlChar*)"modalMinWidth");
    if (!rMinW) rMinW = xmlGetProp(menuRoot, (const xmlChar*)"modalminwidth");
    if (!rMinW) rMinW = xmlGetProp(menuRoot, (const xmlChar*)"modalMinW");
    if (!rMinW) rMinW = xmlGetProp(menuRoot, (const xmlChar*)"modalminw");
    if (rMinW) {
        try { modalMinWidth_ = std::stoi(reinterpret_cast<const char*>(rMinW)); } catch (...) {}
        xmlFree(rMinW);
    }

    xmlChar* rModalXOff = xmlGetProp(menuRoot, (const xmlChar*)"modalXOffset");
    if (rModalXOff) { try { modalXOffset_ = std::stoi(reinterpret_cast<const char*>(rModalXOff)); } catch (...) {} xmlFree(rModalXOff); }

    xmlChar* rModalYOff = xmlGetProp(menuRoot, (const xmlChar*)"modalYOffset");
    if (rModalYOff) { try { modalYOffset_ = std::stoi(reinterpret_cast<const char*>(rModalYOff)); } catch (...) {} xmlFree(rModalYOff); }

    // 1. Process includes
    for (xmlNodePtr node = menuRoot->children; node; ) {
        xmlNodePtr nextNode = node->next;
        if (node->type == XML_ELEMENT_NODE && strcmp((const char*)node->name, "include") == 0) {
            if (!processInclude(node, doc, menuRoot)) {
                Logger::write(Logger::ZONE_WARNING, "OptionsMenuBuilder", "Skipping invalid include.");
            }

            // Remove <include> node after processing
            xmlUnlinkNode(node);
            xmlFreeNode(node);
        }
        node = nextNode;
    }

    // 2. Parse variable blocks
    parseVariables(root);
    if (menuRoot != root) {
        parseVariables(menuRoot);
    }

    // 3. Substitute variables in document
    std::function<void(xmlNodePtr)> substituteTree = [&](xmlNodePtr n)
        {
            for (; n; n = n->next) {
                if (n->type == XML_ELEMENT_NODE) {
                    // substitute attributes
                    for (xmlAttrPtr attr = n->properties; attr; attr = attr->next) {
                        xmlChar* prop = xmlGetProp(n, attr->name);
                        if (prop) {
                            std::string before = (const char*)prop;
                            std::string after = substituteVariables(before);
                            if (after != before) {
                                xmlSetProp(n, attr->name, (xmlChar*)after.c_str());
                            }
                            xmlFree(prop);
                        }
                    }

                    // substitute content only for leaf element nodes (without child element nodes)
                    bool hasChildElements = false;
                    for (xmlNodePtr ch = n->children; ch; ch = ch->next) {
                        if (ch->type == XML_ELEMENT_NODE) {
                            hasChildElements = true;
                            break;
                        }
                    }

                    if (!hasChildElements) {
                        xmlChar* content = xmlNodeGetContent(n);
                        if (content) {
                            std::string before = (const char*)content;
                            std::string after = substituteVariables(before);
                            if (after != before)
                                xmlNodeSetContent(n, (xmlChar*)after.c_str());
                            xmlFree(content);
                        }
                    }
                }

                substituteTree(n->children);
            }
        };
    substituteTree(root->children);

    // 4. Parse elements and configuration
    Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsing menu config with root <" + rootName + ">");
    images_.clear();
    containers_.clear();

    std::vector<xmlNodePtr> nodesToParse;
    if (menuRoot != root) {
        for (xmlNodePtr ch = root->children; ch; ch = ch->next) {
            if (ch->type != XML_ELEMENT_NODE || ch == menuRoot) continue;
            xmlChar* isOpt = xmlGetProp(ch, (const xmlChar*)"isOptionsMenu");
            if (!isOpt) isOpt = xmlGetProp(ch, (const xmlChar*)"isoptionsmenu");
            if (isOpt) {
                std::string sOpt = reinterpret_cast<const char*>(isOpt);
                std::transform(sOpt.begin(), sOpt.end(), sOpt.begin(), ::tolower);
                if (sOpt == "true" || sOpt == "yes" || sOpt == "1") {
                    nodesToParse.push_back(ch);
                }
                xmlFree(isOpt);
            }
        }
    }
    for (xmlNodePtr ch = menuRoot->children; ch; ch = ch->next) {
        if (ch->type == XML_ELEMENT_NODE) {
            nodesToParse.push_back(ch);
        }
    }

    Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsing " + std::to_string(nodesToParse.size()) + " nodes from menu config");
    for (xmlNodePtr node : nodesToParse) {
        if (node->type != XML_ELEMENT_NODE) continue;
        std::string nodeName = reinterpret_cast<const char*>(node->name);
        Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsing node: <" + nodeName + ">");
        std::string nodeLower = nodeName;
        std::transform(nodeLower.begin(), nodeLower.end(), nodeLower.begin(), ::tolower);
        xmlChar* content = xmlNodeGetContent(node);
        std::string value = content ? reinterpret_cast<const char*>(content) : "";
        xmlFree(content);

        // Image elements
        if (nodeLower == "image") {
            ImageConfig img;
            xmlChar* src = xmlGetProp(node, (const xmlChar*)"src");
            img.src = src ? reinterpret_cast<const char*>(src) : "";
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Raw src value: " + img.src);
            xmlFree(src);

            xmlChar* animated = xmlGetProp(node, (const xmlChar*)"animated");
            img.isAnimated = animated && (std::string(reinterpret_cast<const char*>(animated)) == "true" ||
                                         std::string(reinterpret_cast<const char*>(animated)) == "yes" ||
                                         std::string(reinterpret_cast<const char*>(animated)) == "1");
            xmlFree(animated);

            xmlChar* isMenu = xmlGetProp(node, (const xmlChar*)"isMenu");
            if (!isMenu) isMenu = xmlGetProp(node, (const xmlChar*)"ismenu");
            img.isMenu = isMenu && (std::string(reinterpret_cast<const char*>(isMenu)) == "true" ||
                                    std::string(reinterpret_cast<const char*>(isMenu)) == "yes" ||
                                    std::string(reinterpret_cast<const char*>(isMenu)) == "1");
            xmlFree(isMenu);

            int explicitXOff = 0;
            bool hasExplicitXOff = false;
            xmlChar* xOff = xmlGetProp(node, (const xmlChar*)"xOffset");
            if (!xOff) xOff = xmlGetProp(node, (const xmlChar*)"xoffset");
            if (xOff) {
                try { explicitXOff = std::stoi(reinterpret_cast<const char*>(xOff)); hasExplicitXOff = true; } catch (...) {}
                xmlFree(xOff);
            }

            int explicitYOff = 0;
            bool hasExplicitYOff = false;
            xmlChar* yOff = xmlGetProp(node, (const xmlChar*)"yOffset");
            if (!yOff) yOff = xmlGetProp(node, (const xmlChar*)"yoffset");
            if (yOff) {
                try { explicitYOff = std::stoi(reinterpret_cast<const char*>(yOff)); hasExplicitYOff = true; } catch (...) {}
                xmlFree(yOff);
            }

            xmlChar* x = xmlGetProp(node, (const xmlChar*)"x");
            if (x) {
                XAlignment tempXAlign;
                YAlignment tempYAlign;
                int tempXOffset, tempYOffset;
                parseAlignment(reinterpret_cast<const char*>(x), tempXAlign, tempXOffset, tempYAlign, tempYOffset);
                img.xAlign = tempXAlign;
                img.xOffset = hasExplicitXOff ? explicitXOff : tempXOffset;
                img.x = img.xOffset;
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed image x: " + std::string(reinterpret_cast<const char*>(x)));
                xmlFree(x);
            } else if (hasExplicitXOff) {
                img.xOffset = explicitXOff;
                img.x = img.xOffset;
            }

            xmlChar* y = xmlGetProp(node, (const xmlChar*)"y");
            if (y) {
                XAlignment tempXAlign;
                YAlignment tempYAlign;
                int tempXOffset, tempYOffset;
                parseAlignment(reinterpret_cast<const char*>(y), tempXAlign, tempXOffset, tempYAlign, tempYOffset);
                img.yAlign = tempYAlign;
                img.yOffset = hasExplicitYOff ? explicitYOff : tempYOffset;
                img.y = img.yOffset;
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed image y: " + std::string(reinterpret_cast<const char*>(y)));
                xmlFree(y);
            } else if (hasExplicitYOff) {
                img.yOffset = explicitYOff;
                img.y = img.yOffset;
            }

            xmlChar* width = xmlGetProp(node, (const xmlChar*)"width");
            img.width = width ? std::stoi(reinterpret_cast<const char*>(width)) : 100;
            xmlFree(width);

            xmlChar* height = xmlGetProp(node, (const xmlChar*)"height");
            img.height = height ? std::stoi(reinterpret_cast<const char*>(height)) : 100;
            xmlFree(height);

            xmlChar* scale = xmlGetProp(node, (const xmlChar*)"scaleMode");
            if (!scale) scale = xmlGetProp(node, (const xmlChar*)"scale");
            if (!scale) scale = xmlGetProp(node, (const xmlChar*)"scalemode");
            img.scale = scale ? reinterpret_cast<const char*>(scale) : "stretch";
            xmlFree(scale);

            xmlChar* xOrigin = xmlGetProp(node, (const xmlChar*)"xOrigin");
            if (!xOrigin) xOrigin = xmlGetProp(node, (const xmlChar*)"xorigin");
            if (xOrigin) {
                std::string sOrigin = reinterpret_cast<const char*>(xOrigin);
                std::transform(sOrigin.begin(), sOrigin.end(), sOrigin.begin(), ::tolower);
                if (sOrigin == "left") img.xOrigin = 0.0f;
                else if (sOrigin == "center") img.xOrigin = 0.5f;
                else if (sOrigin == "right") img.xOrigin = 1.0f;
                else {
                    try { img.xOrigin = std::stof(sOrigin); } catch (...) {}
                }
                img.hasExplicitXOrigin = true;
                xmlFree(xOrigin);
            }

            xmlChar* yOrigin = xmlGetProp(node, (const xmlChar*)"yOrigin");
            if (!yOrigin) yOrigin = xmlGetProp(node, (const xmlChar*)"yorigin");
            if (yOrigin) {
                std::string sOrigin = reinterpret_cast<const char*>(yOrigin);
                std::transform(sOrigin.begin(), sOrigin.end(), sOrigin.begin(), ::tolower);
                if (sOrigin == "top") img.yOrigin = 0.0f;
                else if (sOrigin == "center") img.yOrigin = 0.5f;
                else if (sOrigin == "bottom") img.yOrigin = 1.0f;
                else {
                    try { img.yOrigin = std::stof(sOrigin); } catch (...) {}
                }
                img.hasExplicitYOrigin = true;
                xmlFree(yOrigin);
            }

            xmlChar* alpha = xmlGetProp(node, (const xmlChar*)"alpha");
            if (alpha) {
                try { img.alpha = std::stof(reinterpret_cast<const char*>(alpha)); } catch (...) {}
                xmlFree(alpha);
            }

            xmlChar* layer = xmlGetProp(node, (const xmlChar*)"layer");
            if (layer) {
                try { img.layer = std::stoi(reinterpret_cast<const char*>(layer)); } catch (...) {}
                xmlFree(layer);
            }

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

        // Background color
        else if (nodeLower == "backgroundcolor") {
            backgroundColor_ = parseColor(value);
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed backgroundColor: " + value);
        }

        // Font configuration
        else if (nodeLower == "fontpath" || nodeLower == "font") {
            fontPath_ = value;
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed fontPath: " + value);
        }

        // Menu title
        else if (nodeLower == "title") {
            xmlChar* textProp = xmlGetProp(node, (const xmlChar*)"text");
            if (!textProp) textProp = xmlGetProp(node, (const xmlChar*)"value");
            if (textProp) {
                mainTitleText_ = reinterpret_cast<const char*>(textProp);
                xmlFree(textProp);
            }
            xmlChar* size = xmlGetProp(node, (const xmlChar*)"fontSize");
            if (!size) size = xmlGetProp(node, (const xmlChar*)"textSize");
            if (!size) size = xmlGetProp(node, (const xmlChar*)"size");
            xmlChar* color = xmlGetProp(node, (const xmlChar*)"fontColor");
            if (!color) color = xmlGetProp(node, (const xmlChar*)"color");
            xmlChar* underline = xmlGetProp(node, (const xmlChar*)"underlineColor");
            if (!underline) underline = xmlGetProp(node, (const xmlChar*)"subtitleUnderlineColor");
            if (underline) {
                subtitleUnderlineColor_ = parseColor(reinterpret_cast<const char*>(underline));
                xmlFree(underline);
            }
            xmlChar* align = xmlGetProp(node, (const xmlChar*)"alignment");
            if (align) {
                int dummy;
                parseAlignment(reinterpret_cast<const char*>(align), titleXAlignment_, titleXOffset_, titleYAlignment_, dummy);
                xmlFree(align);
            }
            int explicitXOff = 0;
            bool hasExplicitXOff = false;
            xmlChar* xOff = xmlGetProp(node, (const xmlChar*)"xOffset");
            if (!xOff) xOff = xmlGetProp(node, (const xmlChar*)"xoffset");
            if (xOff) {
                try { explicitXOff = std::stoi(reinterpret_cast<const char*>(xOff)); hasExplicitXOff = true; } catch (...) {}
                xmlFree(xOff);
            }
            int explicitYOff = 0;
            bool hasExplicitYOff = false;
            xmlChar* yOff = xmlGetProp(node, (const xmlChar*)"yOffset");
            if (!yOff) yOff = xmlGetProp(node, (const xmlChar*)"yoffset");
            if (yOff) {
                try { explicitYOff = std::stoi(reinterpret_cast<const char*>(yOff)); hasExplicitYOff = true; } catch (...) {}
                xmlFree(yOff);
            }
            xmlChar* x = xmlGetProp(node, (const xmlChar*)"x");
            xmlChar* y = xmlGetProp(node, (const xmlChar*)"y");

            if (size) {
                try { titleFontSize_ = std::stoi(reinterpret_cast<const char*>(size)); } catch (...) {}
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed title textSize: " + std::string(reinterpret_cast<const char*>(size)));
                xmlFree(size);
            }

            if (color) {
                titleColor_ = parseColor(reinterpret_cast<const char*>(color));
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed title color: " + std::string(reinterpret_cast<const char*>(color)));
                xmlFree(color);
            }

            if (x) {
                XAlignment tempXAlign;
                YAlignment tempYAlign;
                int tempXOffset, tempYOffset;
                parseAlignment(reinterpret_cast<const char*>(x), tempXAlign, tempXOffset, tempYAlign, tempYOffset);
                titleXAlignment_ = tempXAlign;
                titleXOffset_ = hasExplicitXOff ? explicitXOff : tempXOffset;
                titleX_ = titleXOffset_;
                if (!hasExplicitTitleXOrigin_) {
                    if (tempXAlign == XAlignment::Left) titleXOrigin_ = 0.0f;
                    else if (tempXAlign == XAlignment::Right) titleXOrigin_ = 1.0f;
                    else if (tempXAlign == XAlignment::Center) titleXOrigin_ = 0.5f;
                }
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed title x: " + std::string(reinterpret_cast<const char*>(x)));
                xmlFree(x);
            } else if (hasExplicitXOff) {
                titleXOffset_ = explicitXOff;
                titleX_ = titleXOffset_;
            }

            if (y) {
                XAlignment tempXAlign;
                YAlignment tempYAlign;
                int tempXOffset, tempYOffset;
                parseAlignment(reinterpret_cast<const char*>(y), tempXAlign, tempXOffset, tempYAlign, tempYOffset);
                titleYAlignment_ = tempYAlign;
                titleYOffset_ = hasExplicitYOff ? explicitYOff : tempYOffset;
                titleY_ = titleYOffset_;
                if (!hasExplicitTitleYOrigin_) {
                    if (tempYAlign == YAlignment::Top) titleYOrigin_ = 0.0f;
                    else if (tempYAlign == YAlignment::Bottom) titleYOrigin_ = 1.0f;
                    else if (tempYAlign == YAlignment::Center) titleYOrigin_ = 0.5f;
                }
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed title y: " + std::string(reinterpret_cast<const char*>(y)));
                xmlFree(y);
            } else if (hasExplicitYOff) {
                titleYOffset_ = explicitYOff;
                titleY_ = titleYOffset_;
            }

            xmlChar* titleXOrig = xmlGetProp(node, (const xmlChar*)"xOrigin");
            if (!titleXOrig) titleXOrig = xmlGetProp(node, (const xmlChar*)"xorigin");
            if (!titleXOrig) titleXOrig = xmlGetProp(node, (const xmlChar*)"xOrigen");
            if (!titleXOrig) titleXOrig = xmlGetProp(node, (const xmlChar*)"xorigen");
            if (titleXOrig) {
                titleXOrigin_ = parseOriginValue(reinterpret_cast<const char*>(titleXOrig), 0.0f);
                hasExplicitTitleXOrigin_ = true;
                xmlFree(titleXOrig);
            }
            xmlChar* titleYOrig = xmlGetProp(node, (const xmlChar*)"yOrigin");
            if (!titleYOrig) titleYOrig = xmlGetProp(node, (const xmlChar*)"yorigin");
            if (!titleYOrig) titleYOrig = xmlGetProp(node, (const xmlChar*)"yOrigen");
            if (!titleYOrig) titleYOrig = xmlGetProp(node, (const xmlChar*)"yorigen");
            if (titleYOrig) {
                titleYOrigin_ = parseOriginValue(reinterpret_cast<const char*>(titleYOrig), 0.0f);
                hasExplicitTitleYOrigin_ = true;
                xmlFree(titleYOrig);
            }

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

        // Text options
        else if (nodeLower == "text") {
            xmlChar* size = xmlGetProp(node, (const xmlChar*)"fontSize");
            if (!size) size = xmlGetProp(node, (const xmlChar*)"textSize");
            if (!size) size = xmlGetProp(node, (const xmlChar*)"size");
            xmlChar* color = xmlGetProp(node, (const xmlChar*)"fontColor");
            if (!color) color = xmlGetProp(node, (const xmlChar*)"color");
            xmlChar* fontProp = xmlGetProp(node, (const xmlChar*)"font");
            if (!fontProp) fontProp = xmlGetProp(node, (const xmlChar*)"fontPath");
            if (fontProp) {
                fontPath_ = reinterpret_cast<const char*>(fontProp);
                xmlFree(fontProp);
            }
            xmlChar* selColor = xmlGetProp(node, (const xmlChar*)"selectedColor");
            if (selColor) {
                selectedColor_ = parseColor(reinterpret_cast<const char*>(selColor));
                xmlFree(selColor);
            }
            xmlChar* selectionBarColor = xmlGetProp(node, (const xmlChar*)"selectionBarColor");
            if (!selectionBarColor) selectionBarColor = xmlGetProp(node, (const xmlChar*)"barColor");
            xmlChar* titleBarColor = xmlGetProp(node, (const xmlChar*)"titleBarColor");
            xmlChar* subtitleUnderlineColor = xmlGetProp(node, (const xmlChar*)"subtitleUnderlineColor");
            xmlChar* space = xmlGetProp(node, (const xmlChar*)"spaceBetween");
            if (!space) space = xmlGetProp(node, (const xmlChar*)"spaceBetweenText");
            xmlChar* align = xmlGetProp(node, (const xmlChar*)"alignment");
            if (align) {
                int dummy;
                parseAlignment(reinterpret_cast<const char*>(align), menuXAlignment_, menuXOffset_, menuYAlignment_, dummy);
                xmlFree(align);
            }
            int explicitXOff = 0;
            bool hasExplicitXOff = false;
            xmlChar* xOff = xmlGetProp(node, (const xmlChar*)"xOffset");
            if (!xOff) xOff = xmlGetProp(node, (const xmlChar*)"xoffset");
            if (xOff) {
                try { explicitXOff = std::stoi(reinterpret_cast<const char*>(xOff)); hasExplicitXOff = true; } catch (...) {}
                xmlFree(xOff);
            }
            int explicitYOff = 0;
            bool hasExplicitYOff = false;
            xmlChar* yOff = xmlGetProp(node, (const xmlChar*)"yOffset");
            if (!yOff) yOff = xmlGetProp(node, (const xmlChar*)"yoffset");
            if (yOff) {
                try { explicitYOff = std::stoi(reinterpret_cast<const char*>(yOff)); hasExplicitYOff = true; } catch (...) {}
                xmlFree(yOff);
            }
            xmlChar* sepColor = xmlGetProp(node, (const xmlChar*)"separatorColor");
            if (sepColor) {
                separatorColor_ = parseColor(reinterpret_cast<const char*>(sepColor));
                xmlFree(sepColor);
            }
            xmlChar* sepH = xmlGetProp(node, (const xmlChar*)"separatorHeight");
            if (sepH) {
                try { separatorHeight_ = std::stoi(reinterpret_cast<const char*>(sepH)); } catch (...) {}
                xmlFree(sepH);
            }
            xmlChar* sepW = xmlGetProp(node, (const xmlChar*)"separatorWidth");
            if (sepW) {
                try { separatorWidth_ = std::stoi(reinterpret_cast<const char*>(sepW)); } catch (...) {}
                xmlFree(sepW);
            }
            xmlChar* sepY = xmlGetProp(node, (const xmlChar*)"separatorYOffset");
            if (sepY) {
                try { separatorYOffset_ = std::stoi(reinterpret_cast<const char*>(sepY)); } catch (...) {}
                xmlFree(sepY);
            }
            xmlChar* sepX = xmlGetProp(node, (const xmlChar*)"separatorX");
            if (!sepX) sepX = xmlGetProp(node, (const xmlChar*)"separatorx");
            if (!sepX) sepX = xmlGetProp(node, (const xmlChar*)"separatorAlign");
            if (!sepX) sepX = xmlGetProp(node, (const xmlChar*)"separatoralign");
            if (!sepX) sepX = xmlGetProp(node, (const xmlChar*)"separatorAlignment");
            if (!sepX) sepX = xmlGetProp(node, (const xmlChar*)"separatoralignment");
            if (sepX) {
                std::string sAlign = reinterpret_cast<const char*>(sepX);
                std::transform(sAlign.begin(), sAlign.end(), sAlign.begin(), ::tolower);
                if (sAlign == "text" || sAlign == "option" || sAlign == "texto") {
                    separatorXAlignment_ = XAlignment::Left;
                    separatorAlignText_ = true;
                } else {
                    int dummyYOff = 0;
                    YAlignment dummyY;
                    parseAlignment(reinterpret_cast<const char*>(sepX), separatorXAlignment_, separatorXOffset_, dummyY, dummyYOff);
                }
                hasExplicitSeparatorXAlignment_ = true;
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed Text separatorX: " + std::string(reinterpret_cast<const char*>(sepX)));
                xmlFree(sepX);
            }
            xmlChar* sepXOff = xmlGetProp(node, (const xmlChar*)"separatorXOffset");
            if (!sepXOff) sepXOff = xmlGetProp(node, (const xmlChar*)"separatorxoffset");
            if (!sepXOff) sepXOff = xmlGetProp(node, (const xmlChar*)"separatorOffset");
            if (!sepXOff) sepXOff = xmlGetProp(node, (const xmlChar*)"separatoroffset");
            if (sepXOff) {
                try { separatorXOffset_ = std::stoi(reinterpret_cast<const char*>(sepXOff)); } catch (...) {}
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed Text separatorXOffset: " + std::string(reinterpret_cast<const char*>(sepXOff)));
                xmlFree(sepXOff);
            }
            xmlChar* x = xmlGetProp(node, (const xmlChar*)"x");
            xmlChar* y = xmlGetProp(node, (const xmlChar*)"y");
            xmlChar* selectionBarPadding = xmlGetProp(node, (const xmlChar*)"selectionBarPadding");
            xmlChar* selectionBarHeight = xmlGetProp(node, (const xmlChar*)"selectionBarHeight");
            xmlChar* titleSpacing = xmlGetProp(node, (const xmlChar*)"titleSpacing");
            xmlChar* selectionBarYOffset = xmlGetProp(node, (const xmlChar*)"selectionBarYOffset");

            if (size) {
                try { optionFontSize_ = std::stoi(reinterpret_cast<const char*>(size)); } catch (...) {}
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed Text textSize: " + std::string(reinterpret_cast<const char*>(size)));
                xmlFree(size);
            }
            if (color) {
                optionColor_ = parseColor(reinterpret_cast<const char*>(color));
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed Text color: " + std::string(reinterpret_cast<const char*>(color)));
                xmlFree(color);
            }
            if (selectionBarColor && !selectionBarColorParsed_) {
                selectionBarColor_ = parseColor(reinterpret_cast<const char*>(selectionBarColor));
                selectionBarColorParsed_ = true;
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed Text selectionBarColor: " + std::string(reinterpret_cast<const char*>(selectionBarColor)));
                xmlFree(selectionBarColor);
            }
            if (titleBarColor) {
                titleBarColor_ = parseColor(reinterpret_cast<const char*>(titleBarColor));
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed Text titleBarColor: " + std::string(reinterpret_cast<const char*>(titleBarColor)));
                xmlFree(titleBarColor);
            }
            if (subtitleUnderlineColor) {
                subtitleUnderlineColor_ = parseColor(reinterpret_cast<const char*>(subtitleUnderlineColor));
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed Text subtitleUnderlineColor: " + std::string(reinterpret_cast<const char*>(subtitleUnderlineColor)));
                xmlFree(subtitleUnderlineColor);
            }
            if (space) {
                try { spaceBetweenText_ = std::stoi(reinterpret_cast<const char*>(space)); } catch (...) {}
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed Text spaceBetween: " + std::string(reinterpret_cast<const char*>(space)));
                xmlFree(space);
            }
            if (x) {
                XAlignment tempXAlign;
                YAlignment tempYAlign;
                int tempXOffset, tempYOffset;
                parseAlignment(reinterpret_cast<const char*>(x), tempXAlign, tempXOffset, tempYAlign, tempYOffset);
                menuXAlignment_ = tempXAlign;
                menuXOffset_ = hasExplicitXOff ? explicitXOff : tempXOffset;
                menuX_ = menuXOffset_;
                if (!hasExplicitMenuXOrigin_) {
                    if (tempXAlign == XAlignment::Left) menuXOrigin_ = 0.0f;
                    else if (tempXAlign == XAlignment::Right) menuXOrigin_ = 1.0f;
                    else if (tempXAlign == XAlignment::Center) menuXOrigin_ = 0.5f;
                }
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed Text x: " + std::string(reinterpret_cast<const char*>(x)));
                xmlFree(x);
            } else if (hasExplicitXOff) {
                menuXOffset_ = explicitXOff;
                menuX_ = menuXOffset_;
            }
            if (y) {
                XAlignment tempXAlign;
                YAlignment tempYAlign;
                int tempXOffset, tempYOffset;
                parseAlignment(reinterpret_cast<const char*>(y), tempXAlign, tempXOffset, tempYAlign, tempYOffset);
                menuYAlignment_ = tempYAlign;
                menuYOffset_ = hasExplicitYOff ? explicitYOff : tempYOffset;
                menuY_ = menuYOffset_;
                if (!hasExplicitMenuYOrigin_) {
                    if (tempYAlign == YAlignment::Top) menuYOrigin_ = 0.0f;
                    else if (tempYAlign == YAlignment::Bottom) menuYOrigin_ = 1.0f;
                    else if (tempYAlign == YAlignment::Center) menuYOrigin_ = 0.5f;
                }
                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed Text y: " + std::string(reinterpret_cast<const char*>(y)));
                xmlFree(y);
            } else if (hasExplicitYOff) {
                menuYOffset_ = explicitYOff;
                menuY_ = menuYOffset_;
            }

            xmlChar* textXOrig = xmlGetProp(node, (const xmlChar*)"xOrigin");
            if (!textXOrig) textXOrig = xmlGetProp(node, (const xmlChar*)"xorigin");
            if (!textXOrig) textXOrig = xmlGetProp(node, (const xmlChar*)"xOrigen");
            if (!textXOrig) textXOrig = xmlGetProp(node, (const xmlChar*)"xorigen");
            if (textXOrig) {
                menuXOrigin_ = parseOriginValue(reinterpret_cast<const char*>(textXOrig), 0.0f);
                hasExplicitMenuXOrigin_ = true;
                xmlFree(textXOrig);
            }
            xmlChar* textYOrig = xmlGetProp(node, (const xmlChar*)"yOrigin");
            if (!textYOrig) textYOrig = xmlGetProp(node, (const xmlChar*)"yorigin");
            if (!textYOrig) textYOrig = xmlGetProp(node, (const xmlChar*)"yOrigen");
            if (!textYOrig) textYOrig = xmlGetProp(node, (const xmlChar*)"yorigen");
            if (textYOrig) {
                menuYOrigin_ = parseOriginValue(reinterpret_cast<const char*>(textYOrig), 0.0f);
                hasExplicitMenuYOrigin_ = true;
                xmlFree(textYOrig);
            }

            auto parseIntProp = [&](xmlChar* prop, int& outValue, const std::string& name) {
                if (prop) {
                    try {
                        outValue = std::stoi(reinterpret_cast<const char*>(prop));
                        Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed Text " + name + ": " + std::string(reinterpret_cast<const char*>(prop)));
                    }
                    catch (...) {
                        Logger::write(Logger::ZONE_WARNING, "OptionsMenuBuilder", "Invalid " + name + ": " + std::string(reinterpret_cast<const char*>(prop)));
                    }
                }
            };

            parseIntProp(selectionBarPadding, selectionBarPadding_, "selectionBarPadding");
            parseIntProp(selectionBarHeight, selectionBarHeight_, "selectionBarHeight");
            parseIntProp(titleSpacing, titleSpacing_, "titleSpacing");
            parseIntProp(selectionBarYOffset, selectionBarYOffset_, "selectionBarYOffset");

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

        // Standalone styling attributes
        else if (nodeLower == "selectionbarcolor") {
            selectionBarColor_ = parseColor(value);
            selectionBarColorParsed_ = true;
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed standalone selectionBarColor: " + value);
        }
        else if (nodeLower == "selectionbarpadding") {
            try { selectionBarPadding_ = std::stoi(value); } catch (...) {}
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed standalone selectionBarPadding: " + value);
        }
        else if (nodeLower == "selectionbarheight") {
            try { selectionBarHeight_ = std::stoi(value); } catch (...) {}
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed standalone selectionBarHeight: " + value);
        }
        else if (nodeLower == "selectionbaryoffset") {
            try { selectionBarYOffset_ = std::stoi(value); } catch (...) {}
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed standalone selectionBarYOffset: " + value);
        }
        else if (nodeLower == "subtitleunderlinecolor") {
            subtitleUnderlineColor_ = parseColor(value);
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed standalone subtitleUnderlineColor: " + value);
        }
        else if (nodeLower == "titlebarcolor") {
            titleBarColor_ = parseColor(value);
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed standalone titleBarColor: " + value);
        }
        else if (nodeLower == "fontcolor") {
            fontColor_ = parseColor(value);
            optionColor_ = fontColor_;
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed standalone fontColor: " + value);
        }
        else if (nodeLower == "spacebetweentext" || nodeLower == "spacebetween") {
            try { spaceBetweenText_ = std::stoi(value); } catch (...) {}
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed standalone spaceBetweenText: " + value);
        }
        else if (nodeLower == "menuxalignment") {
            XAlignment tempXAlign;
            YAlignment dummyY;
            int tempXOffset, dummyYOffset;
            parseAlignment(value, tempXAlign, tempXOffset, dummyY, dummyYOffset);
            menuXAlignment_ = tempXAlign;
            if (tempXOffset != 0) menuXOffset_ = tempXOffset;
            if (!hasExplicitMenuXOrigin_) {
                if (tempXAlign == XAlignment::Left) menuXOrigin_ = 0.0f;
                else if (tempXAlign == XAlignment::Right) menuXOrigin_ = 1.0f;
                else if (tempXAlign == XAlignment::Center) menuXOrigin_ = 0.5f;
            }
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed standalone menuXAlignment: " + value);
        }
        else if (nodeLower == "menuyalignment") {
            XAlignment dummyX;
            YAlignment tempYAlign;
            int dummyXOffset, tempYOffset;
            parseAlignment(value, dummyX, dummyXOffset, tempYAlign, tempYOffset);
            menuYAlignment_ = tempYAlign;
            if (tempYOffset != 0) menuYOffset_ = tempYOffset;
            if (!hasExplicitMenuYOrigin_) {
                if (tempYAlign == YAlignment::Top) menuYOrigin_ = 0.0f;
                else if (tempYAlign == YAlignment::Bottom) menuYOrigin_ = 1.0f;
                else if (tempYAlign == YAlignment::Center) menuYOrigin_ = 0.5f;
            }
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed standalone menuYAlignment: " + value);
        }
        else if (nodeLower == "menuxoffset") {
            try { menuXOffset_ = std::stoi(value); } catch (...) {}
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed standalone menuXOffset: " + value);
        }
        else if (nodeLower == "menuyoffset") {
            try { menuYOffset_ = std::stoi(value); } catch (...) {}
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed standalone menuYOffset: " + value);
        }
        else if (nodeLower == "menuxorigin" || nodeLower == "menuxorigen") {
            menuXOrigin_ = parseOriginValue(value, 0.0f);
            hasExplicitMenuXOrigin_ = true;
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed standalone menuXOrigin: " + value);
        }
        else if (nodeLower == "menuyorigin" || nodeLower == "menuyorigen") {
            menuYOrigin_ = parseOriginValue(value, 0.0f);
            hasExplicitMenuYOrigin_ = true;
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed standalone menuYOrigin: " + value);
        }
        else if (nodeLower == "titlexorigin" || nodeLower == "titlexorigen") {
            titleXOrigin_ = parseOriginValue(value, 0.0f);
            hasExplicitTitleXOrigin_ = true;
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed standalone titleXOrigin: " + value);
        }
        else if (nodeLower == "titleyorigin" || nodeLower == "titleyorigen") {
            titleYOrigin_ = parseOriginValue(value, 0.0f);
            hasExplicitTitleYOrigin_ = true;
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed standalone titleYOrigin: " + value);
        }
        else if (nodeLower == "separatorcolor") {
            separatorColor_ = parseColor(value);
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed standalone separatorColor: " + value);
        }
        else if (nodeLower == "separatorheight") {
            try { separatorHeight_ = std::stoi(value); } catch (...) {}
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed standalone separatorHeight: " + value);
        }
        else if (nodeLower == "separatorwidth") {
            try { separatorWidth_ = std::stoi(value); } catch (...) {}
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed standalone separatorWidth: " + value);
        }
        else if (nodeLower == "separatoryoffset") {
            try { separatorYOffset_ = std::stoi(value); } catch (...) {}
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed standalone separatorYOffset: " + value);
        }
        else if (nodeLower == "separatorx" || nodeLower == "separatoralign" || nodeLower == "separatoralignment") {
            std::string sAlign = value;
            std::transform(sAlign.begin(), sAlign.end(), sAlign.begin(), ::tolower);
            if (sAlign == "text" || sAlign == "option" || sAlign == "texto") {
                separatorXAlignment_ = XAlignment::Left;
                separatorAlignText_ = true;
            } else {
                int dummyYOff = 0;
                YAlignment dummyY;
                parseAlignment(value, separatorXAlignment_, separatorXOffset_, dummyY, dummyYOff);
            }
            hasExplicitSeparatorXAlignment_ = true;
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed standalone separatorX: " + value);
        }
        else if (nodeLower == "separatorxoffset" || nodeLower == "separatoroffset") {
            try { separatorXOffset_ = std::stoi(value); } catch (...) {}
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed standalone separatorXOffset: " + value);
        }
        else if (nodeLower == "selectedcolor") {
            selectedColor_ = parseColor(value);
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed standalone selectedColor: " + value);
        }

        // Layout nodes
        else if (nodeLower == "panelcolor" || nodeLower == "panel") {
            panelColor_ = parseColor(value);
        }
        else if (nodeLower == "bordercolor" || nodeLower == "border") {
            borderColor_ = parseColor(value);
        }
        else if (nodeLower == "borderwidth" || nodeLower == "borderthickness") {
            try { borderWidth_ = std::stoi(value); } catch (...) {}
        }
        else if (nodeLower == "cornerradius" || nodeLower == "radius" || nodeLower == "roundradius") {
            try { cornerRadius_ = std::stoi(value); } catch (...) {}
        }
        else if (nodeLower == "glowcolor") {
            glowColor_ = parseColor(value);
        }
        else if (nodeLower == "glowstrength" || nodeLower == "glow") {
            try { glowStrength_ = std::stoi(value); } catch (...) {}
        }
        else if (nodeLower == "modalwidth" || nodeLower == "cardwidth") {
            try { modalWidth_ = std::stoi(value); } catch (...) {}
        }
        else if (nodeLower == "modalheight" || nodeLower == "cardheight") {
            std::string sVal = value;
            std::transform(sVal.begin(), sVal.end(), sVal.begin(), ::tolower);
            if (sVal == "auto") { modalAutoScale_ = true; modalHeight_ = 0; }
            else { try { modalHeight_ = std::stoi(sVal); } catch (...) {} }
        }
        else if (nodeLower == "modalautoscale" || nodeLower == "autoscale") {
            std::string sA = value;
            std::transform(sA.begin(), sA.end(), sA.begin(), ::tolower);
            if (sA == "true" || sA == "yes" || sA == "1") modalAutoScale_ = true;
        }
        else if (nodeLower == "modalmaxheight" || nodeLower == "modalmaxh") {
            try { modalMaxHeight_ = std::stoi(value); modalAutoScale_ = true; } catch (...) {}
        }
        else if (nodeLower == "modalminheight" || nodeLower == "modalminh") {
            try { modalMinHeight_ = std::stoi(value); modalAutoScale_ = true; } catch (...) {}
        }
        else if (nodeLower == "modalmaxwidth" || nodeLower == "modalmaxw") {
            try { modalMaxWidth_ = std::stoi(value); } catch (...) {}
        }
        else if (nodeLower == "modalminwidth" || nodeLower == "modalminw") {
            try { modalMinWidth_ = std::stoi(value); } catch (...) {}
        }
        else if (nodeLower == "modalxoffset") {
            try { modalXOffset_ = std::stoi(value); } catch (...) {}
        }
        else if (nodeLower == "modalyoffset") {
            try { modalYOffset_ = std::stoi(value); } catch (...) {}
        }
        else if (nodeLower == "container" || nodeLower == "box" || nodeLower == "line") {
            ContainerConfig c;
            xmlChar* width = xmlGetProp(node, (const xmlChar*)"width");
            xmlChar* height = xmlGetProp(node, (const xmlChar*)"height");
            xmlChar* x = xmlGetProp(node, (const xmlChar*)"x");
            xmlChar* y = xmlGetProp(node, (const xmlChar*)"y");
            xmlChar* xOff = xmlGetProp(node, (const xmlChar*)"xOffset");
            if (!xOff) xOff = xmlGetProp(node, (const xmlChar*)"xoffset");
            xmlChar* yOff = xmlGetProp(node, (const xmlChar*)"yOffset");
            if (!yOff) yOff = xmlGetProp(node, (const xmlChar*)"yoffset");
            xmlChar* col = xmlGetProp(node, (const xmlChar*)"color");
            if (!col) col = xmlGetProp(node, (const xmlChar*)"backgroundColor");
            if (!col) col = xmlGetProp(node, (const xmlChar*)"backgroundcolor");
            xmlChar* bCol = xmlGetProp(node, (const xmlChar*)"borderColor");
            xmlChar* bW = xmlGetProp(node, (const xmlChar*)"borderWidth");
            xmlChar* rad = xmlGetProp(node, (const xmlChar*)"cornerRadius");
            if (!rad) rad = xmlGetProp(node, (const xmlChar*)"radius");
            xmlChar* layer = xmlGetProp(node, (const xmlChar*)"layer");
            xmlChar* alpha = xmlGetProp(node, (const xmlChar*)"alpha");

            if (width) { try { c.width = std::stoi(reinterpret_cast<const char*>(width)); } catch (...) {} xmlFree(width); }
            if (height) { try { c.height = std::stoi(reinterpret_cast<const char*>(height)); } catch (...) {} xmlFree(height); }
            if (xOff) { try { c.xOffset = std::stoi(reinterpret_cast<const char*>(xOff)); } catch (...) {} xmlFree(xOff); }
            if (yOff) { try { c.yOffset = std::stoi(reinterpret_cast<const char*>(yOff)); } catch (...) {} xmlFree(yOff); }
            if (layer) { try { c.layer = std::stoi(reinterpret_cast<const char*>(layer)); } catch (...) {} xmlFree(layer); }
            if (alpha) { try { c.alpha = std::stof(reinterpret_cast<const char*>(alpha)); } catch (...) {} xmlFree(alpha); }
            if (col) { c.color = parseColor(reinterpret_cast<const char*>(col)); xmlFree(col); }
            if (bCol) { c.borderColor = parseColor(reinterpret_cast<const char*>(bCol)); xmlFree(bCol); }
            if (bW) { try { c.borderWidth = std::stoi(reinterpret_cast<const char*>(bW)); } catch (...) {} xmlFree(bW); }
            if (rad) { try { c.cornerRadius = std::stoi(reinterpret_cast<const char*>(rad)); } catch (...) {} xmlFree(rad); }

            if (x) {
                std::string sX = reinterpret_cast<const char*>(x);
                std::transform(sX.begin(), sX.end(), sX.begin(), ::tolower);
                if (sX == "center") c.xAlign = XAlignment::Center;
                else if (sX == "left") c.xAlign = XAlignment::Left;
                else if (sX == "right") c.xAlign = XAlignment::Right;
                else {
                    try { c.x = std::stoi(sX); c.xAlign = XAlignment::Numeric; } catch (...) {}
                }
                xmlFree(x);
            }
            if (y) {
                std::string sY = reinterpret_cast<const char*>(y);
                std::transform(sY.begin(), sY.end(), sY.begin(), ::tolower);
                if (sY == "center") c.yAlign = YAlignment::Center;
                else if (sY == "top") c.yAlign = YAlignment::Top;
                else if (sY == "bottom") c.yAlign = YAlignment::Bottom;
                else {
                    try { c.y = std::stoi(sY); c.yAlign = YAlignment::Numeric; } catch (...) {}
                }
                xmlFree(y);
            }
            containers_.push_back(c);
        }
        else if (nodeLower == "window") {
            xmlChar* width = xmlGetProp(node, (const xmlChar*)"width");
            xmlChar* height = xmlGetProp(node, (const xmlChar*)"height");
            xmlChar* scale = xmlGetProp(node, (const xmlChar*)"scale");
            if (width) {
                try { windowWidth_ = std::stoi(reinterpret_cast<const char*>(width)); }
                catch (...) {}
                xmlFree(width);
            }
            if (height) {
                try { windowHeight_ = std::stoi(reinterpret_cast<const char*>(height)); }
                catch (...) {}
                xmlFree(height);
            }
            if (scale) { scale_ = reinterpret_cast<const char*>(scale); xmlFree(scale); }
        }

        // Section titles
        else if (nodeLower == "menutitles") {
            // 1. Child elements: <entry id="1" title="Menu Settings"/> or <entry id="2" title="Layout"/>
            for (xmlNodePtr entryNode = node->children; entryNode; entryNode = entryNode->next) {
                if (entryNode->type != XML_ELEMENT_NODE) continue;
                std::string entryName = (const char*)entryNode->name;
                std::transform(entryName.begin(), entryName.end(), entryName.begin(), ::tolower);
                if (entryName != "entry" && entryName != "menu" && entryName != "item") continue;

                xmlChar* idAttr = xmlGetProp(entryNode, (const xmlChar*)"id");
                if (!idAttr) idAttr = xmlGetProp(entryNode, (const xmlChar*)"key");
                if (!idAttr) idAttr = xmlGetProp(entryNode, (const xmlChar*)"index");

                xmlChar* titleAttr = xmlGetProp(entryNode, (const xmlChar*)"title");
                if (!titleAttr) titleAttr = xmlGetProp(entryNode, (const xmlChar*)"text");
                if (!titleAttr) titleAttr = xmlGetProp(entryNode, (const xmlChar*)"name");

                xmlChar* iconAttr = xmlGetProp(entryNode, (const xmlChar*)"icon");
                if (!iconAttr) iconAttr = xmlGetProp(entryNode, (const xmlChar*)"src");
                if (!iconAttr) iconAttr = xmlGetProp(entryNode, (const xmlChar*)"image");

                std::string iconStr = "";
                if (iconAttr) {
                    iconStr = substituteVariables(reinterpret_cast<const char*>(iconAttr));
                    if (!iconStr.empty()) {
                        bool isAbs = (iconStr.size() > 1 && iconStr[1] == ':') || iconStr[0] == '/' || iconStr[0] == '\\';
                        if (!isAbs) iconStr = (std::filesystem::path(basePath_) / iconStr).string();
                    }
                    xmlFree(iconAttr);
                }

                if (titleAttr || !iconStr.empty()) {
                    std::string idStr = idAttr ? reinterpret_cast<const char*>(idAttr) : std::to_string(menuTitles_.size() + 1);
                    std::string titleStr = titleAttr ? substituteVariables(reinterpret_cast<const char*>(titleAttr)) : "";

                    menuTitles_.push_back({ idStr, titleStr, iconStr });
                    if (!iconStr.empty()) {
                        menuIcons_[Utils::toLower(idStr)] = iconStr;
                        if (!titleStr.empty()) menuIcons_[Utils::toLower(titleStr)] = iconStr;
                    }
                    Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed menuTitles entry: id=" + idStr + ", title=" + titleStr + ", icon=" + iconStr);
                }
                if (idAttr) xmlFree(idAttr);
                if (titleAttr) xmlFree(titleAttr);
            }

            // 2. Direct attributes: <menuTitles m1="Settings" m2="Layout"/>
            for (xmlAttrPtr attr = node->properties; attr; attr = attr->next) {
                std::string attrName = (const char*)attr->name;
                std::string attrLower = attrName;
                std::transform(attrLower.begin(), attrLower.end(), attrLower.begin(), ::tolower);

                std::string idStr;
                if (attrLower.rfind("menu", 0) == 0 && attrLower.size() > 4) {
                    idStr = attrLower.substr(4);
                } else if (attrLower.rfind("m", 0) == 0 && attrLower.size() > 1 && isdigit(attrLower[1])) {
                    idStr = attrLower.substr(1);
                } else if (attrLower.rfind("_", 0) == 0 && attrLower.size() > 1 && isdigit(attrLower[1])) {
                    idStr = attrLower.substr(1);
                }

                if (!idStr.empty()) {
                    xmlChar* val = xmlGetProp(node, attr->name);
                    if (val) {
                        std::string titleStr = substituteVariables(reinterpret_cast<const char*>(val));
                        menuTitles_.push_back({ idStr, titleStr });
                        Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed menuTitles attribute: id=" + idStr + ", title=" + titleStr);
                        xmlFree(val);
                    }
                }
            }
        }

        // Modal theme properties
        else if (nodeLower == "accent" || nodeLower == "accentcolor") {
            selectionBarColor_ = parseColor(value);
            selectionBarColorParsed_ = true;
            subtitleUnderlineColor_ = selectionBarColor_;
        }
        else if (nodeLower == "panel" || nodeLower == "panelcolor" || nodeLower == "card" || nodeLower == "cardcolor") {
            panelColor_ = parseColor(value);
        }
        else if (nodeLower == "texthi" || nodeLower == "texthighlight" || nodeLower == "selectedcolor") {
            selectedColor_ = parseColor(value);
            titleColor_ = selectedColor_;
        }
        else if (nodeLower == "textmid" || nodeLower == "textmedium" || nodeLower == "optioncolor" || nodeLower == "fontcolor") {
            optionColor_ = parseColor(value);
        }
        else if (nodeLower == "font" || nodeLower == "fontpath") {
            std::string fp = substituteVariables(value);
            bool isAbs = (fp.size() > 1 && fp[1] == ':') || fp[0] == '/' || fp[0] == '\\';
            if (!isAbs) fp = (std::filesystem::path(basePath_) / fp).string();
            fontPath_ = fp;
        }
        else if (nodeLower == "cornerradius" || nodeLower == "radius") {
            try { cornerRadius_ = std::stoi(value); } catch (...) {}
        }
        else if (nodeLower == "borderwidth" || nodeLower == "borderthickness") {
            try { borderWidth_ = std::stoi(value); } catch (...) {}
        }
        else if (nodeLower == "bordercolor" || nodeLower == "border") {
            borderColor_ = parseColor(value);
        }
        else if (nodeLower == "glowcolor") {
            glowColor_ = parseColor(value);
        }
        else if (nodeLower == "glowstrength" || nodeLower == "glow") {
            try { glowStrength_ = std::stoi(value); } catch (...) {}
        }
        else if (nodeLower == "buttonfolder" || nodeLower == "buttonpath") {
            std::string bf = substituteVariables(value);
            bool isAbs = (bf.size() > 1 && bf[1] == ':') || bf[0] == '/' || bf[0] == '\\';
            if (!isAbs) bf = (std::filesystem::path(basePath_) / bf).string();
            buttonFolder_ = bf;
        }
        else if (nodeLower == "iconfolder" || nodeLower == "iconspath" || nodeLower == "iconpath") {
            std::string inf = substituteVariables(value);
            bool isAbs = (inf.size() > 1 && inf[1] == ':') || inf[0] == '/' || inf[0] == '\\';
            if (!isAbs) inf = (std::filesystem::path(basePath_) / inf).string();
            iconFolder_ = inf;
        }
        else if (nodeLower == "icon") {
            xmlChar* forAttr = xmlGetProp(node, (const xmlChar*)"for");
            if (!forAttr) forAttr = xmlGetProp(node, (const xmlChar*)"key");
            if (!forAttr) forAttr = xmlGetProp(node, (const xmlChar*)"id");
            if (!forAttr) forAttr = xmlGetProp(node, (const xmlChar*)"name");
            xmlChar* srcAttr = xmlGetProp(node, (const xmlChar*)"src");
            if (!srcAttr) srcAttr = xmlGetProp(node, (const xmlChar*)"image");
            if (!srcAttr) srcAttr = xmlGetProp(node, (const xmlChar*)"file");
            if (forAttr && srcAttr) {
                std::string forStr = Utils::toLower(reinterpret_cast<const char*>(forAttr));
                std::string srcStr = substituteVariables(reinterpret_cast<const char*>(srcAttr));
                bool isAbs = (srcStr.size() > 1 && srcStr[1] == ':') || srcStr[0] == '/' || srcStr[0] == '\\';
                if (!isAbs) srcStr = (std::filesystem::path(basePath_) / srcStr).string();
                menuIcons_[forStr] = srcStr;
            }
            if (forAttr) xmlFree(forAttr);
            if (srcAttr) xmlFree(srcAttr);
        }
        else if (nodeLower == "btna" || nodeLower == "btnb" || nodeLower == "btnx" || nodeLower == "btny" ||
                 nodeLower == "dpad" || nodeLower == "down" || nodeLower == "left" || nodeLower == "up" ||
                 nodeLower == "lb" || nodeLower == "rb" || nodeLower == "start" || nodeLower == "select" || nodeLower == "back") {
            xmlChar* srcAttr = xmlGetProp(node, (const xmlChar*)"src");
            if (!srcAttr) srcAttr = xmlGetProp(node, (const xmlChar*)"image");
            if (!srcAttr) srcAttr = xmlGetProp(node, (const xmlChar*)"file");
            std::string src = srcAttr ? reinterpret_cast<const char*>(srcAttr) : value;
            if (srcAttr) xmlFree(srcAttr);
            src = substituteVariables(src);
            if (!src.empty() && (src.find(".png") != std::string::npos || src.find(".jpg") != std::string::npos || src.find(".webp") != std::string::npos)) {
                bool isAbs = (src.size() > 1 && src[1] == ':') || src[0] == '/' || src[0] == '\\';
                if (!isAbs) src = (std::filesystem::path(basePath_) / src).string();
                buttonImages_[nodeLower] = src;
                if (nodeLower == "btna") { buttonImages_["a"] = src; buttonImages_["select"] = src; }
                if (nodeLower == "btnb") { buttonImages_["b"] = src; buttonImages_["back"] = src; }
                if (nodeLower == "btnx") { buttonImages_["x"] = src; }
                if (nodeLower == "btny") { buttonImages_["y"] = src; }
                if (nodeLower == "dpad") { buttonImages_["all"] = src; buttonImages_["directions"] = src; }
            }
        }

        // Localization mappings
        else if (nodeLower == "menulang") {
    for (xmlNodePtr entryNode = node->children; entryNode; entryNode = entryNode->next) {
        if (entryNode->type != XML_ELEMENT_NODE || std::string(reinterpret_cast<const char*>(entryNode->name)) != "entry") continue;
        xmlChar* key = xmlGetProp(entryNode, (const xmlChar*)"key");
        xmlChar* text = xmlGetProp(entryNode, (const xmlChar*)"text");
        if (key && text) {
            std::string keyStr = reinterpret_cast<const char*>(key);
            std::string textStr = reinterpret_cast<const char*>(text);

                        textStr = substituteVariables(textStr);

            if (keyStr == "main") mainTitleText_ = textStr;
            else if (keyStr == "settings") settingsTitleText_ = textStr;
            else if (keyStr == "controls") controlsTitleText_ = textStr;
            else if (keyStr == "actions") actionsTitleText_ = textStr;
            else if (keyStr == "layout") layoutTitleText_ = textStr;
            else if (keyStr == "save_exit") saveExitText_ = textStr;
            else if (keyStr == "quit") quitText_ = textStr;
            else if (keyStr == "signOut") signOutText_ = textStr;
            else if (keyStr == "restart") restartText_ = textStr;
            else if (keyStr == "shutDown") shutDownText_ = textStr;

            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed menuLang entry: key=" + keyStr + ", text=" + textStr);
        }
        xmlFree(key);
        xmlFree(text);
    }
}
}

    Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed " + std::to_string(images_.size()) + " images");
    Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Config summary: menuXAlignment=" +
        std::string(menuXAlignment_ == XAlignment::Left ? "left" : menuXAlignment_ == XAlignment::Center ? "center" : "right") +
        ", menuXOffset=" + std::to_string(menuXOffset_) +
        ", menuXOrigin=" + std::to_string(menuXOrigin_) +
        ", menuYAlignment=" + std::string(menuYAlignment_ == YAlignment::Top ? "top" : menuYAlignment_ == YAlignment::Center ? "center" : "bottom") +
        ", menuYOffset=" + std::to_string(menuYOffset_) +
        ", menuYOrigin=" + std::to_string(menuYOrigin_) +
        ", titleXOrigin=" + std::to_string(titleXOrigin_) +
        ", titleYOrigin=" + std::to_string(titleYOrigin_) +
        ", modalWidth=" + std::to_string(modalWidth_) +
        ", modalHeight=" + std::to_string(modalHeight_));
    // Fallback: check root for <menuTitles> if menuRoot was a child/included node
    if (menuTitles_.empty() && root != menuRoot) {
        for (xmlNodePtr n = root->children; n; n = n->next) {
            if (n->type != XML_ELEMENT_NODE) continue;
            std::string nLower = (const char*)n->name;
            std::transform(nLower.begin(), nLower.end(), nLower.begin(), ::tolower);
            if (nLower == "menutitles") {
                for (xmlNodePtr entryNode = n->children; entryNode; entryNode = entryNode->next) {
                    if (entryNode->type != XML_ELEMENT_NODE) continue;
                    std::string entryName = (const char*)entryNode->name;
                    std::transform(entryName.begin(), entryName.end(), entryName.begin(), ::tolower);
                    if (entryName != "entry" && entryName != "menu" && entryName != "item") continue;

                    xmlChar* idAttr = xmlGetProp(entryNode, (const xmlChar*)"id");
                    if (!idAttr) idAttr = xmlGetProp(entryNode, (const xmlChar*)"key");
                    if (!idAttr) idAttr = xmlGetProp(entryNode, (const xmlChar*)"index");

                    xmlChar* titleAttr = xmlGetProp(entryNode, (const xmlChar*)"title");
                    if (!titleAttr) titleAttr = xmlGetProp(entryNode, (const xmlChar*)"text");
                    if (!titleAttr) titleAttr = xmlGetProp(entryNode, (const xmlChar*)"name");

                    xmlChar* iconAttr = xmlGetProp(entryNode, (const xmlChar*)"icon");
                    if (!iconAttr) iconAttr = xmlGetProp(entryNode, (const xmlChar*)"src");
                    if (!iconAttr) iconAttr = xmlGetProp(entryNode, (const xmlChar*)"image");

                    std::string iconStr = "";
                    if (iconAttr) {
                        iconStr = substituteVariables(reinterpret_cast<const char*>(iconAttr));
                        if (!iconStr.empty()) {
                            bool isAbs = (iconStr.size() > 1 && iconStr[1] == ':') || iconStr[0] == '/' || iconStr[0] == '\\';
                            if (!isAbs) iconStr = (std::filesystem::path(basePath_) / iconStr).string();
                        }
                        xmlFree(iconAttr);
                    }

                    if (titleAttr || !iconStr.empty()) {
                        std::string idStr = idAttr ? reinterpret_cast<const char*>(idAttr) : std::to_string(menuTitles_.size() + 1);
                        std::string titleStr = titleAttr ? substituteVariables(reinterpret_cast<const char*>(titleAttr)) : "";
                        menuTitles_.push_back({ idStr, titleStr, iconStr });
                        if (!iconStr.empty()) {
                            menuIcons_[Utils::toLower(idStr)] = iconStr;
                            if (!titleStr.empty()) menuIcons_[Utils::toLower(titleStr)] = iconStr;
                        }
                        Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Parsed menuTitles from root: id=" + idStr + ", title=" + titleStr + ", icon=" + iconStr);
                    }
                    if (idAttr) xmlFree(idAttr);
                    if (titleAttr) xmlFree(titleAttr);
                }
            }
        }
    }

    xmlFreeDoc(doc);
    return true;
}



std::string OptionsMenuBuilder::getMenuIcon(const std::string& idOrKey) const {
    if (idOrKey.empty()) return "";
    std::string lowerKey = Utils::toLower(idOrKey);
    auto it = menuIcons_.find(lowerKey);
    if (it != menuIcons_.end()) return it->second;

    for (const auto& pair : menuIcons_) {
        if (Utils::toLower(pair.first) == lowerKey) return pair.second;
    }

    if (!iconFolder_.empty()) {
        std::string cand = Utils::combinePath(iconFolder_, lowerKey + ".png");
        if (std::filesystem::exists(cand)) return cand;
        cand = Utils::combinePath(iconFolder_, lowerKey + ".jpg");
        if (std::filesystem::exists(cand)) return cand;
        cand = Utils::combinePath(iconFolder_, lowerKey + ".webp");
        if (std::filesystem::exists(cand)) return cand;
    }
    return "";
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

bool OptionsMenuBuilder::processInclude(xmlNodePtr includeNode, xmlDocPtr doc, xmlNodePtr root)
{
    xmlChar* srcAttr = xmlGetProp(includeNode, (const xmlChar*)"src");
    if (!srcAttr) return false;

    std::filesystem::path includePath = std::filesystem::path(basePath_) / reinterpret_cast<const char*>(srcAttr);
    xmlFree(srcAttr);

    // Check if include is already processed
    if (includedFiles_.count(includePath.string())) {
        Logger::write(Logger::ZONE_WARNING, "OptionsMenuBuilder", "Skipping circular include: " + includePath.string());
        return false;
    }

    // Check <include active="false"> on this node
    xmlChar* activeAttr = xmlGetProp(includeNode, (const xmlChar*)"active");
    if (activeAttr) {
        std::string activeValue = Utils::toLower(substituteVariables(reinterpret_cast<const char*>(activeAttr)));
        xmlFree(activeAttr);
        if (activeValue != "true") {
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Skipping inactive include: " + includePath.string());
            return true; // Skip but don't fail
        }
    }

    xmlDocPtr includeDoc = xmlReadFile(includePath.string().c_str(), nullptr, XML_PARSE_NOBLANKS);
    if (!includeDoc) {
        Logger::write(Logger::ZONE_WARNING, "OptionsMenuBuilder", "Failed to load include: " + includePath.string());
        return false;
    }

    xmlNodePtr includeRoot = xmlDocGetRootElement(includeDoc);
    if (!includeRoot) {
        Logger::write(Logger::ZONE_WARNING, "OptionsMenuBuilder", "Include XML has no root: " + includePath.string());
        xmlFreeDoc(includeDoc);
        return false;
    }

    // Check <layout active="false"> inside included file
    xmlChar* layoutActiveAttr = xmlGetProp(includeRoot, (const xmlChar*)"active");
    if (layoutActiveAttr) {
        std::string layoutActive = Utils::toLower(substituteVariables(reinterpret_cast<const char*>(layoutActiveAttr)));
        xmlFree(layoutActiveAttr);
        if (layoutActive != "true") {
            Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder", "Skipping inactive included layout: " + includePath.string());
            xmlFreeDoc(includeDoc);
            return true;
        }
    }

    includedFiles_.insert(includePath.string());

    // Parse variables from included file
    parseVariables(includeRoot);

    // Merge children into main document
    for (xmlNodePtr child = includeRoot->children; child; child = child->next) {
        if (child->type != XML_ELEMENT_NODE) continue;

        // Skip child if it has active="false"
        xmlChar* childActive = xmlGetProp(child, (const xmlChar*)"active");
        if (childActive) {
            std::string childActiveValue = Utils::toLower(substituteVariables(reinterpret_cast<const char*>(childActive)));
            xmlFree(childActive);
            if (childActiveValue != "true") continue;
        }

        xmlNodePtr copy = xmlCopyNode(child, 1);

        // Substitute all attributes
        for (xmlAttrPtr attr = copy->properties; attr; attr = attr->next) {
            xmlChar* val = xmlGetProp(copy, attr->name);
            if (val) {
                std::string substituted = substituteVariables(reinterpret_cast<const char*>(val));
                xmlSetProp(copy, attr->name, reinterpret_cast<const xmlChar*>(substituted.c_str()));
                xmlFree(val);
            }
        }

        // Substitute node content only for leaf element nodes (without child element nodes)
        bool hasChildElements = false;
        for (xmlNodePtr c = copy->children; c; c = c->next) {
            if (c->type == XML_ELEMENT_NODE) {
                hasChildElements = true;
                break;
            }
        }
        if (!hasChildElements) {
            xmlChar* content = xmlNodeGetContent(copy);
            if (content) {
                std::string substituted = substituteVariables(reinterpret_cast<const char*>(content));
                if (substituted != reinterpret_cast<const char*>(content)) {
                    xmlNodeSetContent(copy, reinterpret_cast<const xmlChar*>(substituted.c_str()));
                }
                xmlFree(content);
            }
        }

        xmlAddChild(root, copy);
    }

    xmlFreeDoc(includeDoc);
    return true;
}




void OptionsMenuBuilder::parseVariables(xmlNodePtr node)
{
    if (!node) return;

    for (xmlNodePtr cur = node; cur; cur = cur->next)
    {
        if (cur->type != XML_ELEMENT_NODE) {
            // recurse children even for non-element nodes just in case
            parseVariables(cur->children);
            continue;
        }

        const char* nodeName = reinterpret_cast<const char*>(cur->name);
        // Look for <variables> blocks anywhere in the tree
        if (nodeName && strcmp(nodeName, "variables") == 0)
        {
            for (xmlNodePtr varNode = cur->children; varNode; varNode = varNode->next)
            {
                if (varNode->type != XML_ELEMENT_NODE) continue;

                const char* varNameC = reinterpret_cast<const char*>(varNode->name);
                if (!varNameC) continue;

                // Read variable content
                xmlChar* content = xmlNodeGetContent(varNode);
                std::string varValue;
                if (content) {
                    varValue = reinterpret_cast<const char*>(content);
                    xmlFree(content);
                }

                std::string varName(varNameC);

                // Store/overwrite variable
                variables_[varName] = varValue;

                Logger::write(Logger::ZONE_INFO, "OptionsMenuBuilder",
                    "Loaded variable: " + varName + " = " + varValue);
            }
        }

        // Recurse into children for everything
        parseVariables(cur->children);
    }
}



std::string OptionsMenuBuilder::substituteVariables(const std::string& input)
{
    std::string result = input;
    size_t pos = 0;

    while (pos < result.length()) {
        if (result[pos] == '$') {
            if (pos + 1 < result.length() && result[pos + 1] == '{') { // ${varName}
                size_t start = pos + 2;
                size_t end = start;
                while (end < result.length() && (isalnum(result[end]) || result[end] == '_')) ++end;
                if (end < result.length() && result[end] == '}') {
                    std::string varName = result.substr(start, end - start);
                    auto it = variables_.find(varName);
                    if (it != variables_.end()) {
                        result.replace(pos, end + 1 - pos, it->second);
                        pos += it->second.length();
                    }
                    else {
                        pos = end + 1;
                    }
                }
                else {
                    ++pos;
                }
            }
            else { // $varName
                size_t start = pos + 1;
                size_t end = start;
                while (end < result.length() && (isalnum(result[end]) || result[end] == '_')) ++end;
                std::string varName = result.substr(start, end - start);
                auto it = variables_.find(varName);
                if (it != variables_.end()) {
                    result.replace(pos, end - pos, it->second);
                    pos += it->second.length();
                }
                else {
                    pos = end;
                }
            }
        }
        else {
            ++pos;
        }
    }
    return result;
}



void OptionsMenuBuilder::calculateModalBounds(size_t itemCount, int windowWidth, int windowHeight, float scaleFactor,
                                             int& outModalX, int& outModalY, int& outModalW, int& outModalH,
                                             int maxContentWidth) const {
    if (!isModal()) {
        outModalW = windowWidth;
        outModalH = windowHeight;
        outModalX = 0;
        outModalY = 0;
        return;
    }

    // Modal Width
    if (modalWidth_ > 0) {
        outModalW = static_cast<int>(modalWidth_ * scaleFactor);
    } else if (maxContentWidth > 0) {
        outModalW = maxContentWidth + static_cast<int>(100 * scaleFactor);
    } else if (modalMaxWidth_ > 0) {
        outModalW = static_cast<int>(modalMaxWidth_ * scaleFactor);
    } else if (windowWidth_ > 0 && windowWidth_ < 1920) {
        outModalW = static_cast<int>(windowWidth_ * scaleFactor);
    } else {
        outModalW = static_cast<int>(720 * scaleFactor);
    }

    if (modalMaxWidth_ > 0 && outModalW > static_cast<int>(modalMaxWidth_ * scaleFactor)) {
        outModalW = static_cast<int>(modalMaxWidth_ * scaleFactor);
    }
    if (modalMinWidth_ > 0 && outModalW < static_cast<int>(modalMinWidth_ * scaleFactor)) {
        outModalW = static_cast<int>(modalMinWidth_ * scaleFactor);
    }

    // Modal Height
    if (modalAutoScale_) {
        int topPadding = static_cast<int>(35 * scaleFactor);
        int titleH = static_cast<int>(34 * scaleFactor);
        int titleGap = static_cast<int>(30 * scaleFactor);
        int itemSpacing = static_cast<int>(spaceBetweenText_ * scaleFactor);
        if (itemSpacing < 20) itemSpacing = static_cast<int>(40 * scaleFactor);
        int bottomPadding = static_cast<int>(35 * scaleFactor);

        int contentH = topPadding + titleH + titleGap + static_cast<int>(itemCount * itemSpacing) + bottomPadding;
        outModalH = contentH;

        int maxH = modalMaxHeight_ > 0 ?
            static_cast<int>(modalMaxHeight_ * scaleFactor) :
            (modalHeight_ > 0 ? static_cast<int>(modalHeight_ * scaleFactor) : static_cast<int>(windowHeight - 80 * scaleFactor));
        int minH = modalMinHeight_ > 0 ?
            static_cast<int>(modalMinHeight_ * scaleFactor) :
            static_cast<int>(200 * scaleFactor);

        if (maxH > 0 && outModalH > maxH) outModalH = maxH;
        if (minH > 0 && outModalH < minH) outModalH = minH;
    } else {
        outModalH = modalHeight_ > 0 ?
            static_cast<int>(modalHeight_ * scaleFactor) :
            (windowHeight_ > 0 && windowHeight_ < 1080 ?
                static_cast<int>(windowHeight_ * scaleFactor) : static_cast<int>(780 * scaleFactor));

        if (modalMaxHeight_ > 0 && outModalH > static_cast<int>(modalMaxHeight_ * scaleFactor)) {
            outModalH = static_cast<int>(modalMaxHeight_ * scaleFactor);
        }
        if (modalMinHeight_ > 0 && outModalH < static_cast<int>(modalMinHeight_ * scaleFactor)) {
            outModalH = static_cast<int>(modalMinHeight_ * scaleFactor);
        }
    }

    outModalX = (windowWidth - outModalW) / 2 + static_cast<int>(modalXOffset_ * scaleFactor);
    outModalY = (windowHeight - outModalH) / 2 + static_cast<int>(modalYOffset_ * scaleFactor);
}