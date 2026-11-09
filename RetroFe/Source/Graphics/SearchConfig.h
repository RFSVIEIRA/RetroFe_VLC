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
#pragma once

#include <SDL.h>
#include <string>

#include <map>

struct SearchColors
{
    // All 30 configurable keys | Default and CORE palettes (from PDF)
    // Default palette values (PDF column 1):
    SDL_Color accent         = { 255, 191,  64, 255 }; // #FFBF40 Primary highlight
    SDL_Color accentDk       = {  28,  22,   8, 255 }; // #1C1608 Accent contrast
    SDL_Color panel          = {  22,  26,  40, 255 }; // #161A28 Main overlay surface
    SDL_Color border         = {  58,  66,  98, 255 }; // #3A4262 Panel outlines
    SDL_Color card           = {  29,  34,  52, 255 }; // #1D2234 Cards / list surfaces
    SDL_Color field          = {  13,  16,  26, 255 }; // #0D101A Search entry background
    SDL_Color backdrop       = {   2,   3,   8, 255 }; // #020308 Full-screen dimmer
    SDL_Color textHi         = { 236, 239, 248, 255 }; // #ECEFF8 Primary text
    SDL_Color textMid        = { 152, 162, 192, 255 }; // #98A2C0 Secondary text
    SDL_Color textLow        = { 100, 110, 142, 255 }; // #646E8E Muted text
    SDL_Color noMatches      = { 255, 120, 120, 255 }; // #FF7878 No matches warning
    SDL_Color keyFace        = {  54,  62,  90, 255 }; // #363E5A Normal key fill
    SDL_Color keyEdge        = {  34,  39,  60, 255 }; // #22273C Normal key edge
    SDL_Color specialFace    = {  66,  75, 106, 255 }; // #424B6A Special key fill
    SDL_Color specialEdge    = {  40,  46,  70, 255 }; // #282E46 Special key edge
    SDL_Color okFace         = {  48, 128,  98, 255 }; // #308062 OK key fill
    SDL_Color okEdge         = {  30,  84,  64, 255 }; // #1E5440 OK key edge
    SDL_Color cancelFace     = { 132,  64,  72, 255 }; // #844048 Cancel key fill
    SDL_Color cancelEdge     = {  90,  40,  47, 255 }; // #5A282F Cancel key edge
    SDL_Color focusEdge      = { 255, 191,  64, 255 }; // #FFBF40 Focused key outline
    SDL_Color resultFocused  = {  70,  64,  48, 255 }; // #464030 Selected result fill
    SDL_Color resultInactive = {  43,  48,  65, 255 }; // #2B3041 Unselected result fill
    SDL_Color resultStripe   = { 255, 191,  64, 255 }; // #FFBF40 Selected result stripe
    SDL_Color footerEdge     = {  34,  39,  60, 255 }; // #22273C Footer button edge
    SDL_Color footerFace     = {  60,  69, 100, 255 }; // #3C4564 Footer button fill
    SDL_Color statusBackdrop = {   0,   0,   0, 255 }; // #000000 Loading overlay dimmer
    SDL_Color statusBorder   = {  58,  66,  96, 255 }; // #3A4260 Loading box outline
    SDL_Color statusPanel    = {  24,  28,  42, 255 }; // #181C2A Loading box surface
    SDL_Color statusSpinner  = { 255, 191,  64, 255 }; // #FFBF40 Loading spinner
    SDL_Color statusText     = { 236, 239, 248, 255 }; // #ECEFF8 Loading message text

};

struct SearchTexts
{
    // Header & Modes
    std::string title          = "Search";
    std::string modeView       = "MODE: VIEW";   // Replaces "MODE: PAGE" per user request
    std::string modeList       = "MODE: LIST";
    std::string scopeAll       = "SCOPE: ALL";
    std::string scopeCurrent   = "SCOPE: CURRENT";
    std::string recentTitle    = "RECENT SEARCHES";
    std::string genreAll       = "GENRE: ALL";
    std::string genreLabel     = "GENRE";
    std::string genreBack      = "<";
    std::string game           = "game";
    std::string games          = "games";
    std::string collection     = "collection";
    std::string collections    = "collections";
    std::string separator      = "";
    std::string infoFormat     = "";

    // Query field & chips
    std::string placeholder    = "Type a game name, or pick letters below";
    std::string minChars       = "Min 3 chars";
    std::string noMatches      = "No matches";
    std::string resultSingle   = "result";
    std::string resultPlural   = "results";

    // Virtual keyboard special keys
    std::string keySpace       = "SPACE";
    std::string keyDel         = "DEL";
    std::string keyClear       = "CLEAR";
    std::string keyOk          = "OK";
    std::string keyCancel      = "CANCEL";

    // List mode card
    std::string matchingTitle  = "MATCHING GAMES";
    std::string listPrompt     = "Enter a game name to see matches";
    std::string listMinChars   = "Type at least 3 characters to search";
    std::string listNoMatches  = "No matching games";

    // Footer hints (keys/buttons) - Default to Xbox controller conventions (A, B, X, Y, D-PAD)
    std::string hintSelect     = "A";
    std::string hintBack       = "B";
    std::string hintDirections = "D-PAD";
    std::string hintDown       = "DOWN";
    std::string hintLeft       = "LEFT";
    std::string hintUp         = "UP";
    std::string hintX          = "X";
    std::string hintY          = "Y";
    std::string hintLB         = "LB";
    std::string hintRB         = "RB";
    std::string hintStart      = "START";

    // Footer actions (fully configurable via search theme XML)
    std::string actionType        = "type";
    std::string actionClose       = "close";
    std::string actionMove        = "move";
    std::string actionToggle      = "toggle mode";
    std::string actionScope       = "toggle scope";
    std::string actionKeyboard    = "keyboard";
    std::string actionBrowse      = "browse";
    std::string actionLaunch      = "launch";
    std::string actionDelete      = "delete";
    std::string actionSpace       = "space";
    std::string actionNextGenre   = "next";
    std::string actionPrevGenre   = "prev";
    std::string actionResetGenre  = "reset all";
    std::string actionPage        = "page +/-";
    std::string actionBackspace   = "backspace";
    std::string genreNext         = "next genre";
    std::string genrePrevNext     = "prev/next genre";
    std::string genreReset        = "reset (all)";
};

struct SearchConfig

{

    SearchColors colors;

    SearchTexts  texts;

    std::string  font;

    std::string  previewType = "logo";
    std::string  previewHeight = "";

    std::string  buttonFolder = "configs/ButtomBar/artwork/xbox";

    std::map<std::string, std::string> buttonImages;

    bool         configured = false;

};
