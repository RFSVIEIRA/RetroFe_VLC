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

#include "../Collection/SearchMatcher.h"
#include "SearchConfig.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <map>
#include <string>
#include <vector>

class Page;
class UserInput;

// A pop-up search overlay with an on-screen keyboard and a live result list.
//
// It is drawn on top of the current page. All drawing uses plain SDL
// primitives and a built-in bitmap font, so it does not depend on the layout's
// fonts or artwork. Navigation uses the normal RetroFE controls (up / down /
// left / right / select / back), so it works with a joystick, and a physical
// keyboard can be used to type directly.
class SearchOverlay
{
public:
    SearchOverlay( );
    virtual ~SearchOverlay( );

    // Reads all items of the page's current list and shows the overlay.
    void open( Page *page );

    // Shows the overlay for an arbitrary set of entries (e.g. games from
    // several collections).
    void open( const std::vector<SearchMatcher::Entry> &entries );

    // Hides the overlay without choosing anything.
    void cancel( );

    // List Search only: retain the selected results screen during fade-out.
    bool isClosing( ) const;
    void completeClose( );

    void setConfig( const SearchConfig &cfg );
    const SearchConfig &getConfig( ) const { return config_; }

    // Optional TrueType font file for the overlay (absolute path). When empty,
    // a suitable system font is looked for; if none is found a built-in
    // bitmap font is used.
    void setFontPath( const std::string &path );
    void setListMode( bool enabled );
    bool isListMode( ) const { return listMode_; }
    void toggleMode( );

    void setCurrentCollection( const std::string &col );
    const std::string &getCurrentCollection( ) const { return currentCollection_; }
    void setScopeAll( bool all );
    bool isScopeAll( ) const { return scopeAll_; }
    void toggleScope( );
    bool hasCollectionScope( ) const;
    void toggleGenre( int dir = 1 );
    void updateAvailableGenres( );
    std::string getActiveGenreFilter( ) const;
    void pageResults( int delta );

    bool isActive( ) const;

    // A short message (e.g. "LOADING ...") drawn over the page while the overlay
    // is not open yet. An empty string removes it.
    void setStatus( const std::string &text );
    bool hasStatus( ) const;
    unsigned int itemCount( ) const;

    // True once the user picked a result; chosenIndex() is then the absolute
    // index of the item in the page's current list.
    bool wasChosen( ) const;
    unsigned int chosenIndex( ) const;

    // Group (collection) of the chosen entry, -1 for the page's own list.
    int chosenGroup( ) const;

    // Feeds one SDL event to the overlay. 'input' is RetroFE's UserInput so
    // that the configured controls are honoured.
    void handleEvent( SDL_Event &e, UserInput &input );

    // Draws the overlay on the given renderer.
    void draw( SDL_Renderer *renderer );

    // The current query (as typed), e.g. for building the results collection.
    const std::string &query( ) const { return query_; }
    // Number of entries matching the current query.
    size_t resultCount( ) const { return results_.size( ); }
    // The entry at the given position in the current (sorted) result list.
    const SearchMatcher::Entry &resultEntry( size_t i ) const { return entries_[results_[i]]; }

private:
    enum Zone
    {
        ZoneKeys,
        ZoneResults,
        ZoneRecent,
        ZoneModeToggle,
        ZoneScopeToggle,
        ZoneGenreToggle
    };

    enum Outcome
    {
        OutcomeNone,
        OutcomeChosen,
        OutcomeCancelled
    };

    struct Layout;

    // input handling
    bool fired( UserInput &input, int code, bool repeat ) const;
    bool dispatchMapped( UserInput &input, bool repeat );
    bool handleRawKey( SDL_Keycode sym, bool repeat );
    void finish( Outcome outcome );
    void move( int dx, int dy );
    void pressSelect( );
    void pressKey( );
    void commit( );
    void appendChar( char c );
    void backspace( );
    void clearQuery( );
    void refresh( );
    void ensureCursorVisible( );

    // Drawing
    struct TextTex
    {
        SDL_Texture *tex;
        int          w;
        int          h;
    };

    void         releaseResources( );
    void         resolveFonts( );
    TTF_Font    *getFont( int px, bool bold );
    bool         useTtf( ) const;
    int          textW( const std::string &s, int px, bool bold );
    int          textH( int px, bool bold );
    std::string  fitTo( const std::string &s, int px, bool bold, int maxW );
    void         text( int x, int y, int px, bool bold, const std::string &s, SDL_Color c, int alpha = 255 );
    void         textRuns( int x, int y, int px, const std::string &s, const std::vector<bool> &mask,
                           SDL_Color normal, SDL_Color hit, int alpha = 255 );
    SDL_Texture *discTexture( int radius );
    void         rect( int x, int y, int w, int h, SDL_Color c, int alpha = 255 );
    void         rrect( int x, int y, int w, int h, int radius, SDL_Color c, int alpha = 255 );
    void         glow( int x, int y, int w, int h, int radius, SDL_Color c, int strength );
    void         shadow( int x, int y, int w, int h, int radius, int spread );
    void         drawStatusBox( int w, int h );
    SDL_Texture *getButtonTexture( const std::string &buttonKey, int &texW, int &texH );
    void         computeLayout( int w, int h, Layout &l ) const;
    void         drawBitmapText( int x, int y, int scale, const std::string &s, SDL_Color c, int alpha );
    void         loadRecentSearches( );
    void         saveRecentSearch( const std::string &term );
    void         clearRecentSearches( );
    void         removeRecentSearch( size_t index );
    int          bitmapScale( int px ) const;

    static const int kCols        = 10;
    static const int kNumCharRows = 4;
    static const int kSpecialRow  = 4;
    static const int kSpecialKeys = 5;
    static const size_t kMaxQuery = 40;

    std::string  status_;
    bool         active_;
    bool         closing_;
    Outcome      outcome_;
    unsigned int chosenIndex_;
    int          chosenGroup_;

    std::vector<SearchMatcher::Entry> entries_;
    std::vector<unsigned int>         results_; // indices into entries_

    std::string query_;
    Zone        zone_;
    int         keyRow_;
    int         keyCol_;
    int         resultCursor_;
    int         recentCursor_;
    int         resultTop_;
    int         visibleRows_;
    bool        listMode_;
    bool        typedMode_;    // last edit came from a physical keyboard
    bool        suppressText_; // drop the SDL_TEXTINPUT that follows a mapped key
    bool        textWasActive_;
    Uint32      openTicks_;

    // drawing state / resources (created lazily while drawing)
    SDL_Renderer *rend_;
    int           fade_;                       // 0..255, opening animation
    std::string   userFontPath_;
    std::string   fontRegular_;
    std::string   fontBold_;
    bool          fontsResolved_;
    bool          fontsFailed_;
    int           groupCount_;
    std::map<int, TTF_Font *>           fonts_;
    std::map<std::string, TextTex>      textCache_;
    std::map<std::string, std::string>  fitCache_;
    std::map<int, SDL_Texture *>        discs_;
    SearchConfig                        config_;
    std::string                         currentCollection_;
    bool                                scopeAll_;
    std::vector<std::string>            recentSearches_;
    std::string                         currentPreviewPath_;
    SDL_Texture                        *currentPreviewTex_;
    int                                 currentPreviewW_;
    int                                 currentPreviewH_;
    std::vector<std::string>            availableGenres_;
    int                                 genreIndex_;
    std::map<std::string, SDL_Texture *>        buttonTexCache_;
    std::map<std::string, std::pair<int, int>>  buttonTexSize_;
};
