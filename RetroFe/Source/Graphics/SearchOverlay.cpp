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

#include "SearchOverlay.h"
#include "Page.h"
#include "../Collection/Item.h"
#include "../Control/UserInput.h"
#include "../Database/Configuration.h"
#include "../Utility/Utils.h"
#include <SDL_image.h>
#include <fstream>
#include <algorithm>
#include <set>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace
{

    // ------------------------------------------------------------------
    // Built-in 5x7 bitmap font. Only upper case letters are provided; text
    // is upper-cased before drawing. Using our own font keeps the overlay
    // independent of the layout's fonts.
    // ------------------------------------------------------------------
    struct GlyphDef
    {
        char        ch;
        const char *rows[7];
    };

    const GlyphDef kGlyphs[] =
    {
        { ' ', { "00000", "00000", "00000", "00000", "00000", "00000", "00000" } },
        { 'A', { "01110", "10001", "10001", "11111", "10001", "10001", "10001" } },
        { 'B', { "11110", "10001", "10001", "11110", "10001", "10001", "11110" } },
        { 'C', { "01110", "10001", "10000", "10000", "10000", "10001", "01110" } },
        { 'D', { "11110", "10001", "10001", "10001", "10001", "10001", "11110" } },
        { 'E', { "11111", "10000", "10000", "11110", "10000", "10000", "11111" } },
        { 'F', { "11111", "10000", "10000", "11110", "10000", "10000", "10000" } },
        { 'G', { "01110", "10001", "10000", "10111", "10001", "10001", "01111" } },
        { 'H', { "10001", "10001", "10001", "11111", "10001", "10001", "10001" } },
        { 'I', { "01110", "00100", "00100", "00100", "00100", "00100", "01110" } },
        { 'J', { "00111", "00010", "00010", "00010", "00010", "10010", "01100" } },
        { 'K', { "10001", "10010", "10100", "11000", "10100", "10010", "10001" } },
        { 'L', { "10000", "10000", "10000", "10000", "10000", "10000", "11111" } },
        { 'M', { "10001", "11011", "10101", "10101", "10001", "10001", "10001" } },
        { 'N', { "10001", "11001", "10101", "10011", "10001", "10001", "10001" } },
        { 'O', { "01110", "10001", "10001", "10001", "10001", "10001", "01110" } },
        { 'P', { "11110", "10001", "10001", "11110", "10000", "10000", "10000" } },
        { 'Q', { "01110", "10001", "10001", "10001", "10101", "10010", "01101" } },
        { 'R', { "11110", "10001", "10001", "11110", "10100", "10010", "10001" } },
        { 'S', { "01111", "10000", "10000", "01110", "00001", "00001", "11110" } },
        { 'T', { "11111", "00100", "00100", "00100", "00100", "00100", "00100" } },
        { 'U', { "10001", "10001", "10001", "10001", "10001", "10001", "01110" } },
        { 'V', { "10001", "10001", "10001", "10001", "10001", "01010", "00100" } },
        { 'W', { "10001", "10001", "10001", "10101", "10101", "10101", "01010" } },
        { 'X', { "10001", "10001", "01010", "00100", "01010", "10001", "10001" } },
        { 'Y', { "10001", "10001", "01010", "00100", "00100", "00100", "00100" } },
        { 'Z', { "11111", "00001", "00010", "00100", "01000", "10000", "11111" } },
        { '0', { "01110", "10001", "10011", "10101", "11001", "10001", "01110" } },
        { '1', { "00100", "01100", "00100", "00100", "00100", "00100", "01110" } },
        { '2', { "01110", "10001", "00001", "00010", "00100", "01000", "11111" } },
        { '3', { "11110", "00001", "00001", "01110", "00001", "00001", "11110" } },
        { '4', { "00010", "00110", "01010", "10010", "11111", "00010", "00010" } },
        { '5', { "11111", "10000", "11110", "00001", "00001", "10001", "01110" } },
        { '6', { "00110", "01000", "10000", "11110", "10001", "10001", "01110" } },
        { '7', { "11111", "00001", "00010", "00100", "01000", "01000", "01000" } },
        { '8', { "01110", "10001", "10001", "01110", "10001", "10001", "01110" } },
        { '9', { "01110", "10001", "10001", "01111", "00001", "00010", "01100" } },
        { '-', { "00000", "00000", "00000", "11111", "00000", "00000", "00000" } },
        { '\'', { "00100", "00100", "01000", "00000", "00000", "00000", "00000" } },
        { '.', { "00000", "00000", "00000", "00000", "00000", "01100", "01100" } },
        { ',', { "00000", "00000", "00000", "00000", "01100", "00100", "01000" } },
        { ':', { "00000", "01100", "01100", "00000", "01100", "01100", "00000" } },
        { '!', { "00100", "00100", "00100", "00100", "00100", "00000", "00100" } },
        { '?', { "01110", "10001", "00001", "00010", "00100", "00000", "00100" } },
        { '/', { "00001", "00010", "00010", "00100", "01000", "01000", "10000" } },
        { '&', { "01100", "10010", "10100", "01000", "10101", "10010", "01101" } },
        { '(', { "00010", "00100", "01000", "01000", "01000", "00100", "00010" } },
        { ')', { "01000", "00100", "00010", "00010", "00010", "00100", "01000" } },
        { '+', { "00000", "00100", "00100", "11111", "00100", "00100", "00000" } },
        { '_', { "00000", "00000", "00000", "00000", "00000", "00000", "11111" } },
        { '"', { "01010", "01010", "01010", "00000", "00000", "00000", "00000" } },
        { '*', { "00000", "10101", "01110", "11111", "01110", "10101", "00000" } },
        { '#', { "01010", "01010", "11111", "01010", "11111", "01010", "01010" } },
        { '=', { "00000", "00000", "11111", "00000", "11111", "00000", "00000" } },
        { '<', { "00010", "00100", "01000", "10000", "01000", "00100", "00010" } },
        { '>', { "01000", "00100", "00010", "00001", "00010", "00100", "01000" } }
    };

    const int kGlyphCount = static_cast<int>( sizeof( kGlyphs ) / sizeof( kGlyphs[0] ) );

    const GlyphDef *findGlyph( char c )
    {
        for ( int i = 0; i < kGlyphCount; ++i )
        {
            if ( kGlyphs[i].ch == c ) return &kGlyphs[i];
        }
        return NULL;
    }

    // Reduces a UTF-8 string to something the bitmap font can draw: upper
    // case ASCII, with every non-ASCII character shown as '?'.
    std::string decodeText( const std::string &s )
    {
        std::string out;
        out.reserve( s.size( ) );
        for ( size_t i = 0; i < s.size( ); ++i )
        {
            unsigned char c = static_cast<unsigned char>( s[i] );
            if ( c >= 0x80 )
            {
                if ( ( c & 0xC0 ) != 0x80 ) out += '?'; // lead byte; skip continuation bytes
            }
            else if ( c >= 32 && c < 127 )
            {
                out += static_cast<char>( std::toupper( c ) );
            }
        }
        return out;
    }

    const char *kCharRows[4] =
    {
        "1234567890",
        "QWERTYUIOP",
        "ASDFGHJKL-",
        "ZXCVBNM,.&"
    };

    const char *kSpecialLabels[5] =
    {
        "SPACE", "DEL", "CLEAR", "OK", "CANCEL"
    };

    inline int imax( int a, int b ) { return a > b ? a : b; }
    inline int imin( int a, int b ) { return a < b ? a : b; }

} // namespace


SearchOverlay::SearchOverlay( )
    : active_( false )
    , closing_( false )
    , outcome_( OutcomeNone )
    , chosenIndex_( 0 )
    , chosenGroup_( -1 )
    , zone_( ZoneKeys )
    , keyRow_( 0 )
    , keyCol_( 0 )
    , resultCursor_( 0 )
    , resultTop_( 0 )
    , recentCursor_( 0 )
    , visibleRows_( 8 )
    , listMode_( false )
    , typedMode_( false )
    , suppressText_( false )
    , textWasActive_( false )
    , openTicks_( 0 )
    , rend_( NULL )
    , fade_( 255 )
    , fontsResolved_( false )
    , fontsFailed_( false )
    , groupCount_( 0 )
    , currentCollection_( "" )
    , scopeAll_( true )
    , currentPreviewTex_( NULL )
    , currentPreviewW_( 0 )
    , currentPreviewH_( 0 )
    , genreIndex_( 0 )
{
}


SearchOverlay::~SearchOverlay( )
{
    releaseResources( );
}


void SearchOverlay::toggleMode( )
{
    setListMode( !listMode_ );
    if ( !listMode_ && zone_ == ZoneResults )
    {
        zone_ = ZoneKeys;
    }
}


void SearchOverlay::setCurrentCollection( const std::string &col )
{
    currentCollection_ = col;
    updateAvailableGenres( );
    refresh( );
}


void SearchOverlay::setScopeAll( bool all )
{
    scopeAll_ = all;
    updateAvailableGenres( );
    refresh( );
}


void SearchOverlay::toggleScope( )
{
    if ( !hasCollectionScope( ) ) return;
    scopeAll_ = !scopeAll_;
    updateAvailableGenres( );
    refresh( );
}


void SearchOverlay::updateAvailableGenres( )
{
    std::string currentSelected = getActiveGenreFilter( );

    std::set<std::string> uniqueGenres;
    for ( size_t i = 0; i < entries_.size( ); ++i )
    {
        const SearchMatcher::Entry &e = entries_[i];
        if ( e.genre.empty( ) ) continue;
        if ( !scopeAll_ && hasCollectionScope( ) && e.groupName != currentCollection_ )
        {
            continue;
        }
        uniqueGenres.insert( e.genre );
    }

    availableGenres_.clear( );
    availableGenres_.push_back( "ALL" );
    for ( std::set<std::string>::const_iterator it = uniqueGenres.begin( ); it != uniqueGenres.end( ); ++it )
    {
        availableGenres_.push_back( *it );
    }

    genreIndex_ = 0;
    if ( !currentSelected.empty( ) )
    {
        for ( size_t i = 0; i < availableGenres_.size( ); ++i )
        {
            if ( availableGenres_[i] == currentSelected )
            {
                genreIndex_ = static_cast<int>( i );
                break;
            }
        }
    }
}


std::string SearchOverlay::getActiveGenreFilter( ) const
{
    if ( genreIndex_ > 0 && genreIndex_ < static_cast<int>( availableGenres_.size( ) ) )
    {
        return availableGenres_[genreIndex_];
    }
    return "";
}


void SearchOverlay::toggleGenre( int dir )
{
    if ( availableGenres_.size( ) <= 1 ) return;
    genreIndex_ += dir;
    if ( genreIndex_ < 0 )
    {
        genreIndex_ = static_cast<int>( availableGenres_.size( ) ) - 1;
    }
    else if ( genreIndex_ >= static_cast<int>( availableGenres_.size( ) ) )
    {
        genreIndex_ = 0;
    }
    refresh( );
}


bool SearchOverlay::hasCollectionScope( ) const
{
    if ( currentCollection_.empty( ) ) return false;
    for ( size_t i = 0; i < entries_.size( ); ++i )
    {
        if ( SearchMatcher::sameCollection( entries_[i].groupName, currentCollection_ ) )
        {
            return true;
        }
    }
    return false;
}


void SearchOverlay::pageResults( int delta )
{
    if ( !listMode_ || results_.empty( ) ) return;
    int step = visibleRows_ > 1 ? visibleRows_ - 1 : 1;
    resultCursor_ += delta * step;
    if ( resultCursor_ < 0 ) resultCursor_ = 0;
    if ( resultCursor_ >= static_cast<int>( results_.size( ) ) )
    {
        resultCursor_ = static_cast<int>( results_.size( ) ) - 1;
    }
    ensureCursorVisible( );
}


void SearchOverlay::loadRecentSearches( )
{
    recentSearches_.clear( );
    std::string path = Utils::combinePath( Configuration::absolutePath, "collections", "Search Collection", "search_history.txt" );
    std::ifstream f( path.c_str( ) );
    if ( !f.is_open( ) ) return;
    std::string line;
    while ( std::getline( f, line ) )
    {
        std::string trimmed = SearchMatcher::trim( line );
        if ( !trimmed.empty( ) && trimmed.size( ) >= 3 )
        {
            if ( std::find( recentSearches_.begin( ), recentSearches_.end( ), trimmed ) == recentSearches_.end( ) )
            {
                recentSearches_.push_back( trimmed );
                if ( recentSearches_.size( ) >= 6 ) break;
            }
        }
    }
}


void SearchOverlay::saveRecentSearch( const std::string &term )
{
    std::string t = SearchMatcher::trim( term );
    if ( t.size( ) < 3 ) return;
    std::vector<std::string>::iterator it = std::find( recentSearches_.begin( ), recentSearches_.end( ), t );
    if ( it != recentSearches_.end( ) ) recentSearches_.erase( it );
    recentSearches_.insert( recentSearches_.begin( ), t );
    while ( recentSearches_.size( ) > 6 ) recentSearches_.pop_back( );

    std::string dir = Utils::combinePath( Configuration::absolutePath, "collections", "Search Collection" );
    Utils::createDirectories( dir );
    std::string path = Utils::combinePath( dir, "search_history.txt" );
    std::ofstream f( path.c_str( ), std::ios::trunc );
    if ( f.is_open( ) )
    {
        for ( size_t i = 0; i < recentSearches_.size( ); ++i )
        {
            f << recentSearches_[i] << "\n";
        }
    }
}


void SearchOverlay::clearRecentSearches( )
{
    recentSearches_.clear( );
    std::string dir = Utils::combinePath( Configuration::absolutePath, "collections", "Search Collection" );
    std::string path = Utils::combinePath( dir, "search_history.txt" );
    std::ofstream f( path.c_str( ), std::ios::trunc );
}


void SearchOverlay::removeRecentSearch( size_t index )
{
    if ( index < recentSearches_.size( ) )
    {
        recentSearches_.erase( recentSearches_.begin( ) + index );
        std::string dir = Utils::combinePath( Configuration::absolutePath, "collections", "Search Collection" );
        std::string path = Utils::combinePath( dir, "search_history.txt" );
        std::ofstream f( path.c_str( ), std::ios::trunc );
        if ( f.is_open( ) )
        {
            for ( size_t i = 0; i < recentSearches_.size( ); ++i )
            {
                f << recentSearches_[i] << "\n";
            }
        }
    }
}


void SearchOverlay::setListMode( bool enabled )
{
    listMode_ = enabled;
}


bool SearchOverlay::isActive( ) const
{
    return active_;
}


void SearchOverlay::setStatus( const std::string &text )
{
    status_ = text;
    if ( text.empty( ) && !active_ )
    {
        releaseResources( );
    }
}


void SearchOverlay::setFontPath( const std::string &path )
{
    if ( path != userFontPath_ )
    {
        userFontPath_  = path;
        fontsResolved_ = false;
        fontsFailed_   = false;
        releaseResources( );
    }
}


SDL_Texture *SearchOverlay::getButtonTexture( const std::string &rawKey, int &texW, int &texH )
{
    texW = 0;
    texH = 0;
    if ( !rend_ || rawKey.empty( ) ) return NULL;

    std::string key = Utils::toLower( rawKey );

    std::map<std::string, SDL_Texture *>::iterator it = buttonTexCache_.find( key );
    if ( it != buttonTexCache_.end( ) )
    {
        if ( it->second )
        {
            texW = buttonTexSize_[key].first;
            texH = buttonTexSize_[key].second;
            return it->second;
        }
        return NULL;
    }

    std::string imgPath;

    // Check directly in config_.buttonImages (case-insensitive search)
    for ( std::map<std::string, std::string>::const_iterator imgIt = config_.buttonImages.begin( );
          imgIt != config_.buttonImages.end( ); ++imgIt )
    {
        if ( Utils::toLower( imgIt->first ) == key )
        {
            imgPath = imgIt->second;
            break;
        }
    }

    // Check common aliases if not found yet
    if ( imgPath.empty( ) )
    {
        std::vector<std::string> aliases;
        if ( key == "select" || key == "hintselect" || key == "a" || key == "btna" )
        {
            static const char *a[] = { "a", "btna", "select", "hintselect" };
            aliases.assign( a, a + 4 );
        }
        else if ( key == "back" || key == "hintback" || key == "b" || key == "btnb" )
        {
            static const char *a[] = { "b", "btnb", "back", "hintback" };
            aliases.assign( a, a + 4 );
        }
        else if ( key == "x" || key == "btnx" || key == "hintx" )
        {
            static const char *a[] = { "x", "btnx", "hintx" };
            aliases.assign( a, a + 3 );
        }
        else if ( key == "y" || key == "btny" || key == "hinty" )
        {
            static const char *a[] = { "y", "btny", "hinty" };
            aliases.assign( a, a + 3 );
        }
        else if ( key == "dpad" || key == "d-pad" || key == "hintdirections" || key == "directions" || key == "all" )
        {
            static const char *a[] = { "dpad", "d-pad", "all", "directions", "hintdirections" };
            aliases.assign( a, a + 5 );
        }
        else if ( key == "down" || key == "hintdown" || key == "updown" )
        {
            static const char *a[] = { "down", "updown", "hintdown" };
            aliases.assign( a, a + 3 );
        }
        else if ( key == "left" || key == "hintleft" || key == "leftright" )
        {
            static const char *a[] = { "left", "leftright", "hintleft" };
            aliases.assign( a, a + 3 );
        }
        else if ( key == "up" || key == "hintup" )
        {
            static const char *a[] = { "up", "updown", "hintup" };
            aliases.assign( a, a + 3 );
        }
        else if ( key == "lb" || key == "btnlb" || key == "l" || key == "l1" || key == "hintlb" || key == "l1/r1" || key == "lb/rb" )
        {
            static const char *a[] = { "lb", "l", "btnlb", "l1", "hintlb" };
            aliases.assign( a, a + 5 );
        }
        else if ( key == "rb" || key == "btnrb" || key == "r" || key == "r1" || key == "hintrb" )
        {
            static const char *a[] = { "rb", "r", "btnrb", "r1", "hintrb" };
            aliases.assign( a, a + 5 );
        }
        else if ( key == "start" || key == "btnstart" || key == "hintstart" )
        {
            static const char *a[] = { "start", "btnstart", "hintstart" };
            aliases.assign( a, a + 3 );
        }

        for ( size_t ai = 0; ai < aliases.size( ); ++ai )
        {
            for ( std::map<std::string, std::string>::const_iterator imgIt = config_.buttonImages.begin( );
                  imgIt != config_.buttonImages.end( ); ++imgIt )
            {
                if ( Utils::toLower( imgIt->first ) == aliases[ai] )
                {
                    imgPath = imgIt->second;
                    break;
                }
            }
            if ( !imgPath.empty( ) ) break;
        }
    }

    // Check buttonFolder if configured
    if ( imgPath.empty( ) && !config_.buttonFolder.empty( ) )
    {
        std::string cand = Utils::combinePath( config_.buttonFolder, key + ".png" );
        if ( Utils::fileExists( cand ) ) imgPath = cand;
        else
        {
            cand = Utils::combinePath( config_.buttonFolder, key + ".jpg" );
            if ( Utils::fileExists( cand ) ) imgPath = cand;
        }
    }

    if ( imgPath.empty( ) || !Utils::fileExists( imgPath ) )
    {
        buttonTexCache_[key] = NULL;
        return NULL;
    }

    SDL_Texture *tex = IMG_LoadTexture( rend_, imgPath.c_str( ) );
    if ( tex )
    {
        SDL_QueryTexture( tex, NULL, NULL, &texW, &texH );
        buttonTexCache_[key] = tex;
        buttonTexSize_[key] = std::make_pair( texW, texH );
        return tex;
    }
    else
    {
        buttonTexCache_[key] = NULL;
        return NULL;
    }
}


void SearchOverlay::setConfig( const SearchConfig &cfg )
{
    config_ = cfg;
    if ( !cfg.font.empty( ) )
    {
        setFontPath( cfg.font );
    }
    releaseResources( );
}


bool SearchOverlay::hasStatus( ) const
{
    return !status_.empty( );
}


unsigned int SearchOverlay::itemCount( ) const
{
    return static_cast<unsigned int>( entries_.size( ) );
}


bool SearchOverlay::wasChosen( ) const
{
    return outcome_ == OutcomeChosen;
}


unsigned int SearchOverlay::chosenIndex( ) const
{
    return chosenIndex_;
}


int SearchOverlay::chosenGroup( ) const
{
    return chosenGroup_;
}


// Lifecycle

void SearchOverlay::open( Page *page )
{
    std::vector<SearchMatcher::Entry> entries;

    unsigned int size     = page ? page->getCollectionSize( ) : 0;
    int          selected = page ? static_cast<int>( page->getSelectedIndex( ) ) : 0;

    entries.reserve( size );
    for ( unsigned int i = 0; i < size; ++i )
    {
        // Offsets are relative to the selected item; this walks the list in
        // its natural order and wraps around as needed.
        Item *item = page->getSelectedItem( static_cast<int>( i ) - selected );
        if ( !item ) continue;

        entries.push_back( SearchMatcher::makeEntry( i,
                                                     item->title,
                                                     item->fullTitle,
                                                     item->name,
                                                     item->manufacturer,
                                                     item->year,
                                                     item->genre ) );
    }

    open( entries );
}


void SearchOverlay::open( const std::vector<SearchMatcher::Entry> &entries )
{
    entries_ = entries;
    groupCount_ = 0;
    for ( size_t i = 0; i < entries_.size( ); ++i )
    {
        if ( entries_[i].group + 1 > groupCount_ ) groupCount_ = entries_[i].group + 1;
    }
    results_.clear( );
    query_.clear( );
    zone_          = ZoneKeys;
    keyRow_        = 0;
    keyCol_        = 0;
    resultCursor_  = 0;
    recentCursor_  = 0;
    resultTop_     = 0;
    visibleRows_   = 8;
    typedMode_     = false;
    suppressText_  = false;
    closing_ = false;
    outcome_ = OutcomeNone;
    chosenIndex_   = 0;
    chosenGroup_   = -1;
    scopeAll_      = true;
    updateAvailableGenres( );
    loadRecentSearches( );

    if ( !active_ )
    {
        textWasActive_ = ( SDL_IsTextInputActive( ) == SDL_TRUE );
    }
    SDL_StartTextInput( );

    // Ignore characters produced by the key that opened the overlay.
    SDL_FlushEvent( SDL_TEXTINPUT );
    SDL_FlushEvent( SDL_TEXTEDITING );
    openTicks_ = SDL_GetTicks( );

    active_ = true;
}


bool SearchOverlay::isClosing( ) const
{
    return closing_;
}


void SearchOverlay::completeClose( )
{
    if ( closing_ )
    {
        finish( OutcomeChosen );
    }
}


void SearchOverlay::cancel( )
{
    finish( OutcomeCancelled );
}


void SearchOverlay::finish( Outcome outcome )
{
    if ( active_ && !textWasActive_ )
    {
        SDL_StopTextInput( );
    }
    active_ = false;
    closing_ = false;
    outcome_ = outcome;
    releaseResources( );
}


// Input handling

bool SearchOverlay::fired( UserInput &input, int code, bool repeat ) const
{
    UserInput::KeyCode_E k = static_cast<UserInput::KeyCode_E>( code );
    return repeat ? input.keystate( k ) : input.newKeyPressed( k );
}


// Handles the controls from controls.conf. Returns true if one fired.
bool SearchOverlay::dispatchMapped( UserInput &input, bool repeat )
{
    // Select and back never auto-repeat.
    if ( !repeat && fired( input, UserInput::KeyCodeBack, false ) )
    {
        cancel( );
        return true;
    }
    if ( !repeat && fired( input, UserInput::KeyCodeSelect, false ) )
    {
        pressSelect( );
        return true;
    }

    // Gamepad & Arcade mapped shortcuts according to user configuration:
    // nextPlaylist (joyButton2 / F3 - Button X): fast Backspace
    if ( fired( input, UserInput::KeyCodeNextPlaylist, repeat ) )
    {
        backspace( );
        return true;
    }

    // prevPlaylist (joyButton3 / F2 - Button Y): fast Space (or Reset Genre if on Genre button)
    if ( !repeat && fired( input, UserInput::KeyCodePrevPlaylist, false ) )
    {
        if ( zone_ == ZoneGenreToggle )
        {
            genreIndex_ = 0;
            refresh( );
            return true;
        }
        appendChar( ' ' );
        return true;
    }

    // removePlaylist (joyButton8 / O): Clear typed query or Remove from Recent History
    if ( !repeat && fired( input, UserInput::KeyCodeRemovePlaylist, false ) )
    {
        if ( zone_ == ZoneRecent )
        {
            if ( recentCursor_ >= 0 && recentCursor_ < static_cast<int>( recentSearches_.size( ) ) )
            {
                removeRecentSearch( static_cast<size_t>( recentCursor_ ) );
                if ( recentSearches_.empty( ) ) zone_ = ZoneKeys;
                else if ( recentCursor_ >= static_cast<int>( recentSearches_.size( ) ) )
                    recentCursor_ = static_cast<int>( recentSearches_.size( ) ) - 1;
            }
            else
            {
                clearRecentSearches( );
                zone_ = ZoneKeys;
            }
            return true;
        }
        else if ( !query_.empty( ) )
        {
            clearQuery( );
            return true;
        }
    }

    // letterDown (joyButton4 / N - LB) and letterUp (joyButton5 / M - RB)
    if ( zone_ == ZoneGenreToggle )
    {
        if ( !repeat && ( fired( input, UserInput::KeyCodePageUp, false ) || fired( input, UserInput::KeyCodeLetterDown, false ) ) )
        {
            toggleGenre( -1 );
            return true;
        }
        if ( !repeat && ( fired( input, UserInput::KeyCodePageDown, false ) || fired( input, UserInput::KeyCodeLetterUp, false ) ) )
        {
            toggleGenre( 1 );
            return true;
        }
    }
    else if ( listMode_ && zone_ == ZoneResults )
    {
        if ( fired( input, UserInput::KeyCodePageUp, repeat ) || fired( input, UserInput::KeyCodeLetterDown, repeat ) )
        {
            pageResults( -1 );
            return true;
        }
        if ( fired( input, UserInput::KeyCodePageDown, repeat ) || fired( input, UserInput::KeyCodeLetterUp, repeat ) )
        {
            pageResults( 1 );
            return true;
        }
    }
    else if ( zone_ == ZoneKeys )
    {
        if ( !repeat && fired( input, UserInput::KeyCodeLetterDown, false ) )
        {
            toggleScope( );
            return true;
        }
        if ( !repeat && fired( input, UserInput::KeyCodeLetterUp, false ) )
        {
            toggleMode( );
            return true;
        }
    }

    if ( fired( input, UserInput::KeyCodeUp, repeat ) )
    {
        move( 0, -1 );
        return true;
    }
    if ( fired( input, UserInput::KeyCodeDown, repeat ) )
    {
        move( 0, 1 );
        return true;
    }
    if ( fired( input, UserInput::KeyCodeLeft, repeat ) )
    {
        move( -1, 0 );
        return true;
    }
    if ( fired( input, UserInput::KeyCodeRight, repeat ) )
    {
        move( 1, 0 );
        return true;
    }
    return false;
}


// Fallback for keys that are not part of controls.conf, so that a plain
// keyboard always works for typing and basic navigation.
bool SearchOverlay::handleRawKey( SDL_Keycode sym, bool repeat )
{
    switch ( sym )
    {
        case SDLK_ESCAPE:
            if ( !repeat ) cancel( );
            return true;

        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            // Physical Enter commits the typed query (the results are then
            // shown as a collection); with an empty query it presses the
            // highlighted on-screen key instead.
            if ( !repeat ) commit( );
            return true;

        case SDLK_DELETE:
            if ( zone_ == ZoneGenreToggle )
            {
                if ( !repeat )
                {
                    genreIndex_ = 0;
                    refresh( );
                }
                return true;
            }
            if ( zone_ == ZoneRecent )
            {
                if ( !repeat )
                {
                    if ( recentCursor_ >= 0 && recentCursor_ < static_cast<int>( recentSearches_.size( ) ) )
                    {
                        removeRecentSearch( static_cast<size_t>( recentCursor_ ) );
                        if ( recentSearches_.empty( ) ) zone_ = ZoneKeys;
                        else if ( recentCursor_ >= static_cast<int>( recentSearches_.size( ) ) )
                            recentCursor_ = static_cast<int>( recentSearches_.size( ) ) - 1;
                    }
                    else
                    {
                        clearRecentSearches( );
                        zone_ = ZoneKeys;
                    }
                }
                return true;
            }
            clearQuery( );
            return true;

        case SDLK_BACKSPACE:
            if ( zone_ == ZoneGenreToggle )
            {
                if ( !repeat ) toggleGenre( -1 );
                return true;
            }
            backspace( );
            return true;

        case SDLK_TAB:
            if ( !repeat )
            {
                if ( zone_ == ZoneKeys )
                {
                    if ( listMode_ && !results_.empty( ) ) zone_ = ZoneResults;
                    else if ( hasCollectionScope( ) ) zone_ = ZoneScopeToggle;
                    else if ( availableGenres_.size( ) > 1 ) zone_ = ZoneGenreToggle;
                    else zone_ = ZoneModeToggle;
                }
                else if ( zone_ == ZoneResults )
                {
                    zone_ = ZoneModeToggle;
                }
                else if ( zone_ == ZoneModeToggle )
                {
                    if ( hasCollectionScope( ) ) zone_ = ZoneScopeToggle;
                    else if ( availableGenres_.size( ) > 1 ) zone_ = ZoneGenreToggle;
                    else zone_ = ZoneKeys;
                }
                else if ( zone_ == ZoneScopeToggle )
                {
                    if ( availableGenres_.size( ) > 1 ) zone_ = ZoneGenreToggle;
                    else zone_ = ZoneKeys;
                }
                else if ( zone_ == ZoneGenreToggle )
                {
                    zone_ = ZoneKeys;
                }
                else
                {
                    zone_ = ZoneKeys;
                }
                ensureCursorVisible( );
            }
            return true;

        case SDLK_UP:
            move( 0, -1 );
            return true;

        case SDLK_DOWN:
            move( 0, 1 );
            return true;

        case SDLK_LEFT:
            move( -1, 0 );
            return true;

        case SDLK_RIGHT:
            move( 1, 0 );
            return true;

        case SDLK_PAGEUP:
            if ( zone_ == ZoneGenreToggle )
            {
                if ( !repeat ) toggleGenre( -1 );
            }
            else if ( listMode_ && zone_ == ZoneResults )
            {
                pageResults( -1 );
            }
            return true;

        case SDLK_PAGEDOWN:
            if ( zone_ == ZoneGenreToggle )
            {
                if ( !repeat ) toggleGenre( 1 );
            }
            else if ( listMode_ && zone_ == ZoneResults )
            {
                pageResults( 1 );
            }
            return true;

        default:
            break;
    }
    return false;
}


void SearchOverlay::handleEvent( SDL_Event &e, UserInput &input )
{
    if ( !active_ || closing_ ) return;

    switch ( e.type )
    {
        case SDL_QUIT:
            cancel( );
            break;

        // Events that only change the input state
        case SDL_JOYDEVICEADDED:
        case SDL_JOYDEVICEREMOVED:
        case SDL_JOYBUTTONUP:
        case SDL_MOUSEBUTTONUP:
            input.update( e );
            break;

        case SDL_KEYUP:
            // SDL_TEXTINPUT always arrives right after its SDL_KEYDOWN, i.e.
            // before the key is released.
            suppressText_ = false;
            input.update( e );
            break;

        // Joystick / mouse events that may trigger a configured control
        case SDL_JOYAXISMOTION:
        case SDL_JOYBUTTONDOWN:
        case SDL_JOYHATMOTION:
        case SDL_MOUSEBUTTONDOWN:
            input.update( e );
            dispatchMapped( input, false );
            break;

        case SDL_KEYDOWN:
        {
            bool repeat = ( e.key.repeat != 0 );
            input.update( e );

            // Physical Enter is bound to "select" in controls.conf, so the
            // mapped handler would fire first and do nothing (with the OK key
            // highlighted). Handle Enter explicitly BEFORE mapped dispatch so
            // a plain keyboard Enter always commits the query.
            SDL_Keycode sym = e.key.keysym.sym;
            bool acted = false;
            if ( ( sym == SDLK_RETURN || sym == SDLK_KP_ENTER ) && !repeat )
            {
                commit( );
                acted = true;
            }
            else
            {
                acted = dispatchMapped( input, repeat );
                if ( !acted ) acted = handleRawKey( sym, repeat );
            }
            // A key that did something must not also type its character.
            // Only keys that produce a character are followed by an
            // SDL_TEXTINPUT event (not Backspace, Enter, arrows, ...), so
            // only those need the next text event dropped.
            suppressText_   = acted && sym >= 32 && sym < 127;
            break;
        }

        case SDL_TEXTINPUT:
        {
            if ( suppressText_ )
            {
                suppressText_ = false;
                break;
            }
            if ( SDL_GetTicks( ) - openTicks_ < 150 ) break;

            bool typed = false;
            for ( const char *p = e.text.text; *p != '\0'; ++p )
            {
                unsigned char c = static_cast<unsigned char>( *p );
                if ( c >= 32 && c < 127 )
                {
                    appendChar( static_cast<char>( c ) );
                    typed = true;
                }
            }
            if ( typed ) typedMode_ = true;
            break;
        }

        default:
            break;
    }
}


void SearchOverlay::move( int dx, int dy )
{
    // List Mode can navigate the live results; Page Mode retains keyboard navigation.
    typedMode_ = false;

    if ( zone_ == ZoneRecent )
    {
        if ( recentSearches_.empty( ) )
        {
            zone_ = ZoneKeys;
            return;
        }

        const int maxIdx = static_cast<int>( recentSearches_.size( ) ); // index == size is [Limpar Histórico]

        if ( dy > 0 )
        {
            if ( recentCursor_ < maxIdx ) ++recentCursor_;
        }
        else if ( dy < 0 )
        {
            if ( recentCursor_ > 0 )
            {
                --recentCursor_;
            }
            else
            {
                // Up from top recent item moves to top toggle buttons
                if ( availableGenres_.size( ) > 1 ) zone_ = ZoneGenreToggle;
                else if ( hasCollectionScope( ) )   zone_ = ZoneScopeToggle;
                else                                zone_ = ZoneModeToggle;
            }
        }
        else if ( dx < 0 )
        {
            // Left moves back to keyboard
            zone_ = ZoneKeys;
        }
        return;
    }

    if ( zone_ == ZoneModeToggle )
    {
        if ( dy > 0 )
        {
            zone_ = ZoneKeys;
            keyRow_ = 0;
        }
        else if ( dx > 0 )
        {
            if ( hasCollectionScope( ) )
            {
                zone_ = ZoneScopeToggle;
            }
            else if ( availableGenres_.size( ) > 1 )
            {
                zone_ = ZoneGenreToggle;
            }
            else if ( listMode_ && !results_.empty( ) )
            {
                zone_ = ZoneResults;
                ensureCursorVisible( );
            }
            else if ( listMode_ && query_.empty( ) && !recentSearches_.empty( ) )
            {
                zone_ = ZoneRecent;
                recentCursor_ = 0;
            }
        }
        return;
    }

    if ( zone_ == ZoneScopeToggle )
    {
        if ( dy > 0 )
        {
            zone_ = ZoneKeys;
            keyRow_ = 0;
        }
        else if ( dx < 0 )
        {
            zone_ = ZoneModeToggle;
        }
        else if ( dx > 0 )
        {
            if ( availableGenres_.size( ) > 1 )
            {
                zone_ = ZoneGenreToggle;
            }
            else if ( listMode_ && !results_.empty( ) )
            {
                zone_ = ZoneResults;
                ensureCursorVisible( );
            }
            else if ( listMode_ && query_.empty( ) && !recentSearches_.empty( ) )
            {
                zone_ = ZoneRecent;
                recentCursor_ = 0;
            }
        }
        return;
    }

    if ( zone_ == ZoneGenreToggle )
    {
        if ( dy > 0 )
        {
            zone_ = ZoneKeys;
            keyRow_ = 0;
        }
        else if ( dy < 0 )
        {
            toggleGenre( -1 );
        }
        else if ( dx < 0 )
        {
            if ( hasCollectionScope( ) )
            {
                zone_ = ZoneScopeToggle;
            }
            else
            {
                zone_ = ZoneModeToggle;
            }
        }
        else if ( dx > 0 )
        {
            if ( listMode_ && !results_.empty( ) )
            {
                zone_ = ZoneResults;
                ensureCursorVisible( );
            }
            else if ( listMode_ && query_.empty( ) && !recentSearches_.empty( ) )
            {
                zone_ = ZoneRecent;
                recentCursor_ = 0;
            }
        }
        return;
    }

    if ( listMode_ && zone_ == ZoneResults )
    {
        if ( results_.empty( ) )
        {
            zone_ = ZoneKeys;
            return;
        }

        const int count = static_cast<int>( results_.size( ) );

        if ( dy > 0 && resultCursor_ < count - 1 )
            ++resultCursor_;
        else if ( dy < 0 && resultCursor_ > 0 )
            --resultCursor_;
        else if ( dy < 0 && resultCursor_ == 0 )
        {
            if ( availableGenres_.size( ) > 1 )
                zone_ = ZoneGenreToggle;
            else if ( hasCollectionScope( ) )
                zone_ = ZoneScopeToggle;
            else
                zone_ = ZoneModeToggle;
        }
        else if ( dx < 0 )
            zone_ = ZoneKeys;

        ensureCursorVisible( );
        return;
    }

    if ( listMode_ && dx > 0 &&
         ( ( keyRow_ == kSpecialRow && keyCol_ / 2 == kSpecialKeys - 1 ) ||
           ( keyRow_ != kSpecialRow && keyCol_ == kCols - 1 ) ) )
    {
        if ( !results_.empty( ) )
        {
            zone_ = ZoneResults;
            ensureCursorVisible( );
            return;
        }
        else if ( query_.empty( ) && !recentSearches_.empty( ) )
        {
            zone_ = ZoneRecent;
            recentCursor_ = 0;
            return;
        }
    }

    if ( dy > 0 )
    {
        if ( keyRow_ < kSpecialRow ) ++keyRow_;
        else zone_ = ZoneModeToggle;
    }
    else if ( dy < 0 )
    {
        if ( keyRow_ > 0 ) --keyRow_;
        else
        {
            // Up from row 0 navigates to the toggle buttons
            if ( keyCol_ < 3 )
            {
                zone_ = ZoneModeToggle;
            }
            else if ( keyCol_ < 6 )
            {
                zone_ = hasCollectionScope( ) ? ZoneScopeToggle : ZoneModeToggle;
            }
            else
            {
                if ( availableGenres_.size( ) > 1 )
                {
                    zone_ = ZoneGenreToggle;
                }
                else
                {
                    zone_ = hasCollectionScope( ) ? ZoneScopeToggle : ZoneModeToggle;
                }
            }
        }
    }
    else if ( dx != 0 )
    {
        if ( keyRow_ == kSpecialRow )
        {
            int b = keyCol_ / 2 + dx;
            if ( b >= kSpecialKeys ) b = 0;
            if ( b < 0 ) b = kSpecialKeys - 1;
            keyCol_ = b * 2;
        }
        else
        {
            int c = keyCol_ + dx;
            if ( c >= kCols ) c = 0;
            if ( c < 0 ) c = kCols - 1;
            keyCol_ = c;
        }
    }
}


void SearchOverlay::pressSelect( )
{
    if ( zone_ == ZoneRecent )
    {
        if ( recentCursor_ >= 0 && recentCursor_ < static_cast<int>( recentSearches_.size( ) ) )
        {
            query_ = recentSearches_[recentCursor_];
            refresh( );
            zone_ = ZoneResults;
            resultCursor_ = 0;
            ensureCursorVisible( );
        }
        else
        {
            clearRecentSearches( );
            zone_ = ZoneKeys;
        }
        return;
    }

    if ( zone_ == ZoneModeToggle )
    {
        toggleMode( );
        return;
    }

    if ( zone_ == ZoneScopeToggle )
    {
        toggleScope( );
        return;
    }

    if ( zone_ == ZoneGenreToggle )
    {
        toggleGenre( 1 );
        return;
    }

    // In List Mode, Select chooses the highlighted result when focused.
    if ( listMode_ && zone_ == ZoneResults )
    {
        commit( );
        return;
    }

    // Page Mode and the List Mode keyboard retain their existing controls.
    pressKey( );
}


void SearchOverlay::pressKey( )
{
    if ( keyRow_ < kNumCharRows )
    {
        appendChar( kCharRows[keyRow_][keyCol_] );
        return;
    }

    switch ( keyCol_ / 2 )
    {
        case 0: appendChar( ' ' ); break;
        case 1: backspace( );      break;
        case 2: clearQuery( );     break;
        case 3: commit( );         break;
        case 4: cancel( );         break;
        default: break;
    }
}


// Accept the typed query; the caller shows every match as a collection.
void SearchOverlay::commit( )
{
    if ( SearchMatcher::trim( query_ ).size( ) < 3 ) return;
    if ( results_.empty( ) ) return;   // nothing matches: stay open and keep typing

    // Use highlighted result cursor if valid; otherwise default to first result.
    size_t selected = 0;
    if ( resultCursor_ >= 0 && static_cast<size_t>( resultCursor_ ) < results_.size( ) )
    {
        selected = static_cast<size_t>( resultCursor_ );
    }

    saveRecentSearch( query_ );
    chosenIndex_ = entries_[results_[selected]].index;
    chosenGroup_ = entries_[results_[selected]].group;
    if ( listMode_ )
    {
        outcome_ = OutcomeChosen;
        closing_ = true;
    }
    else
    {
        finish( OutcomeChosen );
    }
}


void SearchOverlay::appendChar( char c )
{
    if ( query_.size( ) >= kMaxQuery ) return;

    // No leading or doubled spaces
    if ( c == ' ' && ( query_.empty( ) || query_[query_.size( ) - 1] == ' ' ) ) return;

    query_ += c;
    refresh( );
}


void SearchOverlay::backspace( )
{
    if ( query_.empty( ) ) return;
    query_.erase( query_.size( ) - 1 );
    refresh( );
}


void SearchOverlay::clearQuery( )
{
    query_.clear( );
    refresh( );
}


void SearchOverlay::refresh( )
{
    std::string genreFilter = getActiveGenreFilter( );
    if ( SearchMatcher::trim( query_ ).empty( ) && genreFilter.empty( ) )
    {
        results_.clear( );
        resultCursor_ = 0;
        resultTop_    = 0;
        if ( zone_ == ZoneResults )
        {
            zone_ = ZoneKeys;
        }
        return;
    }

    std::string colFilter = ( !scopeAll_ && hasCollectionScope( ) ) ? currentCollection_ : "";
    SearchMatcher::filter( entries_, query_, results_, colFilter, genreFilter );
    if ( resultCursor_ >= static_cast<int>( results_.size( ) ) )
    {
        resultCursor_ = results_.empty( ) ? 0 : static_cast<int>( results_.size( ) ) - 1;
    }
    ensureCursorVisible( );
    if ( results_.empty( ) && zone_ == ZoneResults )
    {
        zone_ = ZoneKeys;
    }
}


void SearchOverlay::ensureCursorVisible( )
{
    if ( visibleRows_ < 1 ) visibleRows_ = 1;

    if ( resultCursor_ < resultTop_ )
    {
        resultTop_ = resultCursor_;
    }
    else if ( resultCursor_ >= resultTop_ + visibleRows_ )
    {
        resultTop_ = resultCursor_ - visibleRows_ + 1;
    }
}


// Rendering

namespace
{

    SDL_Color rgb( int r, int g, int b )
    {
        SDL_Color c;
        c.r = static_cast<Uint8>( r );
        c.g = static_cast<Uint8>( g );
        c.b = static_cast<Uint8>( b );
        c.a = 255;
        return c;
    }

    SDL_Color mix( SDL_Color a, SDL_Color b, int t256 )
    {
        return rgb( a.r + ( b.r - a.r ) * t256 / 256,
                    a.g + ( b.g - a.g ) * t256 / 256,
                    a.b + ( b.b - a.b ) * t256 / 256 );
    }

    // A stable, pleasant colour for a collection name (used for its tag).
    SDL_Color hueColor( const std::string &name )
    {
        unsigned int hash = 2166136261u;
        for ( size_t i = 0; i < name.size( ); ++i )
        {
            hash ^= static_cast<unsigned char>( name[i] );
            hash *= 16777619u;
        }
        float h = static_cast<float>( hash % 360 ) / 60.0f;
        float s = 0.50f;
        float v = 0.98f;
        int   i = static_cast<int>( h );
        float f = h - static_cast<float>( i );
        float p = v * ( 1.0f - s );
        float q = v * ( 1.0f - s * f );
        float t = v * ( 1.0f - s * ( 1.0f - f ) );
        float r = v, g = t, b = p;
        switch ( i % 6 )
        {
            case 0: r = v; g = t; b = p; break;
            case 1: r = q; g = v; b = p; break;
            case 2: r = p; g = v; b = t; break;
            case 3: r = p; g = q; b = v; break;
            case 4: r = t; g = p; b = v; break;
            default: r = v; g = p; b = q; break;
        }
        return rgb( static_cast<int>( r * 255 ), static_cast<int>( g * 255 ), static_cast<int>( b * 255 ) );
    }

    bool fileExists( const std::string &path )
    {
        if ( path.empty( ) ) return false;
        FILE *f = fopen( path.c_str( ), "rb" );
        if ( !f ) return false;
        fclose( f );
        return true;
    }

    // Cuts a UTF-8 string to at most n bytes without splitting a character.
    size_t utf8Boundary( const std::string &s, size_t n )
    {
        if ( n >= s.size( ) ) return s.size( );
        while ( n > 0 && ( static_cast<unsigned char>( s[n] ) & 0xC0 ) == 0x80 ) --n;
        return n;
    }

    int scaleAlpha( int a, int fade )
    {
        return a * fade / 255;
    }

} // namespace


struct SearchOverlay::Layout
{
    int      w, h, u, pad, gap;
    SDL_Rect panel;
    SDL_Rect titleRow;
    SDL_Rect modeBtn;
    SDL_Rect scopeBtn;
    SDL_Rect genreBtn;
    SDL_Rect field;
    SDL_Rect kbCard;
    SDL_Rect resultsCard;
    SDL_Rect previewBox;
    SDL_Rect footer;
    int      cell, keyGap, keysX, keysY;
    int      cardPad;
};


// Font cache

void SearchOverlay::resolveFonts( )
{
    if ( fontsResolved_ ) return;
    fontsResolved_ = true;
    fontRegular_.clear( );
    fontBold_.clear( );

    if ( !TTF_WasInit( ) && TTF_Init( ) != 0 )
    {
        fontsFailed_ = true;
        return;
    }

    if ( fileExists( userFontPath_ ) )
    {
        fontRegular_ = userFontPath_;
        fontBold_    = userFontPath_;
        return;
    }

    if ( !userFontPath_.empty( ) )
    {
        try {
            std::filesystem::path ufp( userFontPath_ );
            std::string fname = ufp.filename( ).string( );
            const char *win = getenv( "WINDIR" );
            if ( !win ) win = getenv( "SystemRoot" );
            std::string fontsDir = std::string( win ? win : "C:\\Windows" ) + "\\Fonts\\";
            if ( fileExists( fontsDir + fname ) )
            {
                fontRegular_ = fontsDir + fname;
                fontBold_    = fontsDir + fname;
                return;
            }
        } catch ( ... ) { }
    }

    std::vector<std::string> regular;
    std::vector<std::string> bold;

    const char *win = getenv( "WINDIR" );
    if ( !win ) win = getenv( "SystemRoot" );
    std::string fonts = std::string( win ? win : "C:\\Windows" ) + "\\Fonts\\";

    const char *winNames[][2] =
    {
        { "segoeui.ttf",  "segoeuib.ttf"  },
        { "calibri.ttf",  "calibrib.ttf"  },
        { "tahoma.ttf",   "tahomabd.ttf"  },
        { "arial.ttf",    "arialbd.ttf"   },
        { "verdana.ttf",  "verdanab.ttf"  }
    };
    for ( size_t i = 0; i < sizeof( winNames ) / sizeof( winNames[0] ); ++i )
    {
        regular.push_back( fonts + winNames[i][0] );
        bold.push_back( fonts + winNames[i][1] );
    }

    regular.push_back( "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf" );
    bold.push_back( "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf" );
    regular.push_back( "/usr/share/fonts/TTF/DejaVuSans.ttf" );
    bold.push_back( "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf" );
    regular.push_back( "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf" );
    bold.push_back( "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf" );
    regular.push_back( "/System/Library/Fonts/Supplemental/Arial.ttf" );
    bold.push_back( "/System/Library/Fonts/Supplemental/Arial Bold.ttf" );

    for ( size_t i = 0; i < regular.size( ); ++i )
    {
        if ( fileExists( regular[i] ) )
        {
            fontRegular_ = regular[i];
            fontBold_    = fileExists( bold[i] ) ? bold[i] : regular[i];
            return;
        }
    }
}


bool SearchOverlay::useTtf( ) const
{
    return fontsResolved_ && !fontsFailed_ && !fontRegular_.empty( );
}


TTF_Font *SearchOverlay::getFont( int px, bool bold )
{
    if ( !useTtf( ) ) return NULL;
    if ( px < 6 ) px = 6;

    int key = px * 2 + ( bold ? 1 : 0 );
    std::map<int, TTF_Font *>::iterator it = fonts_.find( key );
    if ( it != fonts_.end( ) ) return it->second;

    TTF_Font *font = TTF_OpenFont( ( bold ? fontBold_ : fontRegular_ ).c_str( ), px );
    if ( !font && bold ) font = TTF_OpenFont( fontRegular_.c_str( ), px );
    if ( !font )
    {
        fontsFailed_ = true; // fall back to the bitmap font
        return NULL;
    }
    fonts_[key] = font;
    return font;
}


void SearchOverlay::releaseResources( )
{
    for ( std::map<std::string, TextTex>::iterator it = textCache_.begin( ); it != textCache_.end( ); ++it )
    {
        if ( it->second.tex ) SDL_DestroyTexture( it->second.tex );
    }
    textCache_.clear( );
    fitCache_.clear( );

    for ( std::map<int, SDL_Texture *>::iterator it = discs_.begin( ); it != discs_.end( ); ++it )
    {
        if ( it->second ) SDL_DestroyTexture( it->second );
    }
    discs_.clear( );

    for ( std::map<std::string, SDL_Texture *>::iterator it = buttonTexCache_.begin( ); it != buttonTexCache_.end( ); ++it )
    {
        if ( it->second ) SDL_DestroyTexture( it->second );
    }
    buttonTexCache_.clear( );
    buttonTexSize_.clear( );

    if ( currentPreviewTex_ )
    {
        SDL_DestroyTexture( currentPreviewTex_ );
        currentPreviewTex_ = NULL;
    }
    currentPreviewPath_.clear( );

    if ( TTF_WasInit( ) )
    {
        for ( std::map<int, TTF_Font *>::iterator it = fonts_.begin( ); it != fonts_.end( ); ++it )
        {
            if ( it->second ) TTF_CloseFont( it->second );
        }
    }
    fonts_.clear( );
}


// Text rendering

int SearchOverlay::bitmapScale( int px ) const
{
    return imax( 1, px / 9 );
}


int SearchOverlay::textW( const std::string &s, int px, bool bold )
{
    if ( s.empty( ) ) return 0;

    TTF_Font *font = getFont( px, bold );
    if ( font )
    {
        int w = 0;
        int h = 0;
        if ( TTF_SizeUTF8( font, s.c_str( ), &w, &h ) == 0 ) return w;
        return 0;
    }

    int scale = bitmapScale( px );
    std::string d = decodeText( s );
    if ( d.empty( ) ) return 0;
    return static_cast<int>( d.size( ) ) * 6 * scale - scale;
}


int SearchOverlay::textH( int px, bool bold )
{
    TTF_Font *font = getFont( px, bold );
    if ( font ) return TTF_FontHeight( font );
    return 7 * bitmapScale( px );
}


// Shortens text with "..." so that it is at most maxW pixels wide.
std::string SearchOverlay::fitTo( const std::string &s, int px, bool bold, int maxW )
{
    if ( maxW <= 0 || s.empty( ) ) return "";

    std::string key = std::to_string( px ) + ( bold ? "b" : "r" ) + ":" + std::to_string( maxW ) + ":" + s;
    std::map<std::string, std::string>::iterator it = fitCache_.find( key );
    if ( it != fitCache_.end( ) ) return it->second;

    std::string result;
    if ( textW( s, px, bold ) <= maxW )
    {
        result = s;
    }
    else
    {
        const std::string ell = "...";
        int ew = textW( ell, px, bold );
        if ( ew <= maxW )
        {
            size_t lo = 0;
            size_t hi = s.size( );
            while ( lo < hi )
            {
                size_t mid = ( lo + hi + 1 ) / 2;
                size_t cut = utf8Boundary( s, mid );
                if ( textW( s.substr( 0, cut ), px, bold ) + ew <= maxW ) lo = mid;
                else hi = mid - 1;
            }
            size_t cut = utf8Boundary( s, lo );
            std::string head = s.substr( 0, cut );
            while ( !head.empty( ) && head[head.size( ) - 1] == ' ' ) head.erase( head.size( ) - 1 );
            result = head + ell;
        }
    }

    if ( fitCache_.size( ) > 4000 ) fitCache_.clear( );
    fitCache_[key] = result;
    return result;
}


void SearchOverlay::drawBitmapText( int x, int y, int scale, const std::string &s, SDL_Color c, int alpha )
{
    std::string d = decodeText( s );
    if ( d.empty( ) ) return;

    std::vector<SDL_Rect> rects;
    rects.reserve( d.size( ) * 16 );
    for ( size_t i = 0; i < d.size( ); ++i )
    {
        const GlyphDef *g = findGlyph( d[i] );
        if ( !g ) g = findGlyph( '?' );
        if ( !g ) continue;

        int gx = x + static_cast<int>( i ) * 6 * scale;
        for ( int row = 0; row < 7; ++row )
        {
            for ( int col = 0; col < 5; ++col )
            {
                if ( g->rows[row][col] == '1' )
                {
                    SDL_Rect px;
                    px.x = gx + col * scale;
                    px.y = y + row * scale;
                    px.w = scale;
                    px.h = scale;
                    rects.push_back( px );
                }
            }
        }
    }
    if ( rects.empty( ) ) return;
    SDL_SetRenderDrawColor( rend_, c.r, c.g, c.b, static_cast<Uint8>( alpha ) );
    SDL_RenderFillRects( rend_, &rects[0], static_cast<int>( rects.size( ) ) );
}


// Draws text with its top-left corner at (x, y).
void SearchOverlay::text( int x, int y, int px, bool bold, const std::string &s, SDL_Color c, int alpha )
{
    if ( s.empty( ) || !rend_ ) return;
    alpha = scaleAlpha( alpha, fade_ );
    if ( alpha <= 0 ) return;

    TTF_Font *font = getFont( px, bold );
    if ( !font )
    {
        drawBitmapText( x, y, bitmapScale( px ), s, c, alpha );
        return;
    }

    std::string key = std::to_string( px ) + ( bold ? "b" : "r" ) + ":" + s;
    std::map<std::string, TextTex>::iterator it = textCache_.find( key );
    if ( it == textCache_.end( ) )
    {
        if ( textCache_.size( ) > 800 )
        {
            for ( it = textCache_.begin( ); it != textCache_.end( ); ++it )
            {
                if ( it->second.tex ) SDL_DestroyTexture( it->second.tex );
            }
            textCache_.clear( );
        }

        TextTex tt;
        tt.tex = NULL;
        tt.w   = 0;
        tt.h   = 0;

        SDL_Color white = rgb( 255, 255, 255 );
        SDL_Surface *surface = TTF_RenderUTF8_Blended( font, s.c_str( ), white );
        if ( surface )
        {
            tt.tex = SDL_CreateTextureFromSurface( rend_, surface );
            tt.w   = surface->w;
            tt.h   = surface->h;
            SDL_FreeSurface( surface );
            if ( tt.tex ) SDL_SetTextureBlendMode( tt.tex, SDL_BLENDMODE_BLEND );
        }
        it = textCache_.insert( std::make_pair( key, tt ) ).first;
    }

    if ( !it->second.tex ) return;

    SDL_Rect dst;
    dst.x = x;
    dst.y = y;
    dst.w = it->second.w;
    dst.h = it->second.h;
    SDL_SetTextureColorMod( it->second.tex, c.r, c.g, c.b );
    SDL_SetTextureAlphaMod( it->second.tex, static_cast<Uint8>( alpha ) );
    SDL_RenderCopy( rend_, it->second.tex, NULL, &dst );
}


// Text where the characters flagged in 'mask' are drawn highlighted (bold, other colour).
void SearchOverlay::textRuns( int x, int y, int px, const std::string &s, const std::vector<bool> &mask,
                              SDL_Color normal, SDL_Color hit, int alpha )
{
    size_t i = 0;
    while ( i < s.size( ) )
    {
        bool   h = i < mask.size( ) && mask[i];
        size_t j = i;
        while ( j < s.size( ) && ( ( j < mask.size( ) && mask[j] ) == h ) ) ++j;

        std::string run = s.substr( i, j - i );
        text( x, y, px, h, run, h ? hit : normal, alpha );
        x += textW( run, px, h );
        i = j;
    }
}


// Vector shapes

void SearchOverlay::rect( int x, int y, int w, int h, SDL_Color c, int alpha )
{
    alpha = scaleAlpha( alpha, fade_ );
    if ( w <= 0 || h <= 0 || alpha <= 0 ) return;
    SDL_Rect r;
    r.x = x;
    r.y = y;
    r.w = w;
    r.h = h;
    SDL_SetRenderDrawColor( rend_, c.r, c.g, c.b, static_cast<Uint8>( alpha ) );
    SDL_RenderFillRect( rend_, &r );
}


// A white, anti-aliased disc; quarters of it make the corners of rounded rectangles.
SDL_Texture *SearchOverlay::discTexture( int radius )
{
    std::map<int, SDL_Texture *>::iterator it = discs_.find( radius );
    if ( it != discs_.end( ) ) return it->second;

    int d = radius * 2;
    std::vector<Uint8> pixels( static_cast<size_t>( d ) * d * 4 );
    const int ss = 4;
    for ( int y = 0; y < d; ++y )
    {
        for ( int x = 0; x < d; ++x )
        {
            int inside = 0;
            for ( int sy = 0; sy < ss; ++sy )
            {
                for ( int sx = 0; sx < ss; ++sx )
                {
                    float fx = static_cast<float>( x ) + ( static_cast<float>( sx ) + 0.5f ) / ss - static_cast<float>( radius );
                    float fy = static_cast<float>( y ) + ( static_cast<float>( sy ) + 0.5f ) / ss - static_cast<float>( radius );
                    if ( fx * fx + fy * fy <= static_cast<float>( radius * radius ) ) ++inside;
                }
            }
            size_t o = ( static_cast<size_t>( y ) * d + x ) * 4;
            pixels[o + 0] = 255;
            pixels[o + 1] = 255;
            pixels[o + 2] = 255;
            pixels[o + 3] = static_cast<Uint8>( 255 * inside / ( ss * ss ) );
        }
    }

    SDL_Texture *tex = SDL_CreateTexture( rend_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, d, d );
    if ( tex )
    {
        SDL_UpdateTexture( tex, NULL, &pixels[0], d * 4 );
        SDL_SetTextureBlendMode( tex, SDL_BLENDMODE_BLEND );
    }
    discs_[radius] = tex;
    return tex;
}


void SearchOverlay::rrect( int x, int y, int w, int h, int radius, SDL_Color c, int alpha )
{
    alpha = scaleAlpha( alpha, fade_ );
    if ( w <= 0 || h <= 0 || alpha <= 0 ) return;

    radius = imin( radius, imin( w / 2, h / 2 ) );
    SDL_Texture *disc = radius >= 2 ? discTexture( radius ) : NULL;
    if ( !disc )
    {
        int saved = fade_;
        fade_ = 255;
        rect( x, y, w, h, c, alpha );
        fade_ = saved;
        return;
    }

    int saved = fade_;
    fade_ = 255; // alpha is already scaled
    rect( x + radius, y, w - 2 * radius, h, c, alpha );
    rect( x, y + radius, radius, h - 2 * radius, c, alpha );
    rect( x + w - radius, y + radius, radius, h - 2 * radius, c, alpha );
    fade_ = saved;

    SDL_SetTextureColorMod( disc, c.r, c.g, c.b );
    SDL_SetTextureAlphaMod( disc, static_cast<Uint8>( alpha ) );

    int r = radius;
    SDL_Rect src;
    SDL_Rect dst;
    src.w = src.h = dst.w = dst.h = r;

    src.x = 0; src.y = 0; dst.x = x;         dst.y = y;
    SDL_RenderCopy( rend_, disc, &src, &dst );
    src.x = r; src.y = 0; dst.x = x + w - r; dst.y = y;
    SDL_RenderCopy( rend_, disc, &src, &dst );
    src.x = 0; src.y = r; dst.x = x;         dst.y = y + h - r;
    SDL_RenderCopy( rend_, disc, &src, &dst );
    src.x = r; src.y = r; dst.x = x + w - r; dst.y = y + h - r;
    SDL_RenderCopy( rend_, disc, &src, &dst );
}


// A soft coloured halo around a rounded rectangle.
void SearchOverlay::glow( int x, int y, int w, int h, int radius, SDL_Color c, int strength )
{
    const int layers = 5;
    int step = imax( 2, radius / 3 );
    for ( int i = layers; i >= 1; --i )
    {
        int g = i * step;
        rrect( x - g, y - g, w + 2 * g, h + 2 * g, radius + g, c, strength / ( i + 1 ) );
    }
}


// A soft drop shadow below a rounded rectangle.
void SearchOverlay::shadow( int x, int y, int w, int h, int radius, int spread )
{
    SDL_Color black = rgb( 0, 0, 0 );
    for ( int i = spread; i >= 1; --i )
    {
        rrect( x - i, y - i + spread / 2, w + 2 * i, h + 2 * i, radius + i, black, 9 );
    }
}


// Layout calculations

void SearchOverlay::computeLayout( int w, int h, Layout &l ) const
{
    l.w = w;
    l.h = h;
    l.u = imax( 8, h / 54 );
    int u = l.u;

    l.pad = u * 3 / 2;
    l.gap = u;

    int mx = imax( u, w * 5 / 100 );
    int my = imax( u, h * 6 / 100 );
    l.panel.x = mx;
    l.panel.y = my;
    l.panel.w = w - 2 * mx;
    l.panel.h = h - 2 * my;

    int innerX = l.panel.x + l.pad;
    int innerW = l.panel.w - 2 * l.pad;

    l.titleRow.x = innerX;
    l.titleRow.y = l.panel.y + l.pad;
    l.titleRow.w = innerW;
    l.titleRow.h = u * 3;

    int curBtnX = innerX + u * 13;
    int btnH = u * 2 + u / 4;
    int btnY = l.titleRow.y + ( l.titleRow.h - btnH ) / 2;

    int modeW = u * 13;
    l.modeBtn = { curBtnX, btnY, modeW, btnH };
    curBtnX += modeW + u;

    if ( hasCollectionScope( ) )
    {
        int scopeW = u * 15;
        l.scopeBtn = { curBtnX, btnY, scopeW, btnH };
        curBtnX += scopeW + u;
    }
    else
    {
        l.scopeBtn = { 0, 0, 0, 0 };
    }

    if ( availableGenres_.size( ) > 1 )
    {
        int genreW = u * 15;
        l.genreBtn = { curBtnX, btnY, genreW, btnH };
    }
    else
    {
        l.genreBtn = { 0, 0, 0, 0 };
    }

    l.field.x = innerX;
    l.field.y = l.titleRow.y + l.titleRow.h + u / 2;
    l.field.w = innerW;
    l.field.h = u * 4;

    l.footer.x = innerX;
    l.footer.w = innerW;
    l.footer.h = u * 3;
    l.footer.y = l.panel.y + l.panel.h - l.pad - l.footer.h;

    int contentY = l.field.y + l.field.h + l.gap;
    int contentH = l.footer.y - l.gap - contentY;
    if ( contentH < 1 ) contentH = 1;

    l.kbCard.x = innerX;
    l.kbCard.y = contentY;
    l.kbCard.w = innerW;
    l.kbCard.h = contentH;
    l.resultsCard = { 0, 0, 0, 0 };

    if ( listMode_ )
    {
        const int splitGap = l.gap;
        const int keyboardW = ( innerW - splitGap ) * 52 / 100;

        l.kbCard.w = keyboardW;
        l.resultsCard.x = innerX + keyboardW + splitGap;
        l.resultsCard.y = contentY;
        l.resultsCard.w = innerW - keyboardW - splitGap;
        l.resultsCard.h = contentH;

        int previewH = 0;
        if ( !config_.previewHeight.empty( ) )
        {
            std::string phStr = config_.previewHeight;
            if ( phStr.back( ) == '%' )
            {
                int pct = atoi( phStr.substr( 0, phStr.size( ) - 1 ).c_str( ) );
                if ( pct > 5 && pct < 80 )
                {
                    previewH = l.resultsCard.h * pct / 100;
                }
            }
            else
            {
                int pxVal = atoi( phStr.c_str( ) );
                if ( pxVal > 20 )
                {
                    previewH = pxVal * h / 1080;
                }
            }
        }
        if ( previewH <= 0 )
        {
            // Default comfortable height: 32% of card or u * 8
            // Gives logo and 3 lines of text (Title, System, Year/Company/Genre) full breathing room!
            previewH = imax( u * 8, l.resultsCard.h * 32 / 100 );
        }
        if ( previewH > l.resultsCard.h * 48 / 100 ) previewH = l.resultsCard.h * 48 / 100;

        l.previewBox = { l.resultsCard.x + l.pad,
                         l.resultsCard.y + l.resultsCard.h - l.pad - previewH,
                         l.resultsCard.w - 2 * l.pad,
                         previewH };
    }
    else
    {
        l.previewBox = { 0, 0, 0, 0 };
    }

    l.cardPad = u;
    int availW = l.kbCard.w - 2 * l.cardPad;
    int availH = l.kbCard.h - 2 * l.cardPad;
    l.cell   = imax( 6, imin( availW / kCols, availH / ( kSpecialRow + 1 ) ) );
    l.keyGap = imax( 3, l.cell / 9 );
    l.keysX  = l.kbCard.x + ( l.kbCard.w - l.cell * kCols ) / 2;
    l.keysY  = l.kbCard.y + ( l.kbCard.h - l.cell * ( kSpecialRow + 1 ) ) / 2;
}


void SearchOverlay::drawStatusBox( int w, int h )
{
    int u   = imax( 8, h / 54 );
    int px  = u * 3 / 2;
    int pad = u * 2;

    std::string msg = fitTo( status_, px, false, w * 7 / 10 - 4 * pad );
    int tw   = textW( msg, px, false );
    int th   = textH( px, false );
    int spin = u * 3;
    int bw   = tw + spin + 3 * pad;
    int bh   = imax( th, spin ) + 2 * pad;
    int bx   = ( w - bw ) / 2;
    int by   = ( h - bh ) / 2;

    rect( 0, 0, w, h, config_.colors.statusBackdrop, 150 );
    shadow( bx, by, bw, bh, u, u );
    rrect( bx, by, bw, bh, u, config_.colors.statusBorder, 255 );
    rrect( bx + 1, by + 1, bw - 2, bh - 2, u, config_.colors.statusPanel, 255 );

    // little spinner
    int   cx    = bx + pad + spin / 2;
    int   cy    = by + bh / 2;
    int   ring  = spin / 2 - u / 3;
    int   dot   = imax( 2, u / 3 );
    Uint32 tick = SDL_GetTicks( ) / 90;
    for ( int i = 0; i < 8; ++i )
    {
        float a  = 6.2831853f * static_cast<float>( i ) / 8.0f;
        int   dx = static_cast<int>( static_cast<float>( ring ) * sinf( a ) );
        int   dy = static_cast<int>( -static_cast<float>( ring ) * cosf( a ) );
        int   age = static_cast<int>( ( tick + 8 - static_cast<Uint32>( i ) ) % 8 );
        rrect( cx + dx - dot, cy + dy - dot, dot * 2, dot * 2, dot, config_.colors.statusSpinner, 255 - age * 28 );
    }

    text( bx + pad + spin + pad / 2, cy - th / 2, px, false, msg, config_.colors.statusText );
}


// Main overlay render

void SearchOverlay::draw( SDL_Renderer *r )
{
    if ( !r ) return;
    if ( !active_ && status_.empty( ) ) return;

    int w = 0;
    int h = 0;
    SDL_GetRendererOutputSize( r, &w, &h );
    if ( w <= 0 || h <= 0 ) return;

    rend_ = r;
    resolveFonts( );

    SDL_BlendMode prevBlend = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode( r, &prevBlend );
    SDL_SetRenderDrawBlendMode( r, SDL_BLENDMODE_BLEND );

    // fade in
    fade_ = 255;
    if ( active_ )
    {
        Uint32 elapsed = SDL_GetTicks( ) - openTicks_;
        if ( elapsed < 180 )
        {
            float t = static_cast<float>( elapsed ) / 180.0f;
            fade_   = static_cast<int>( 255.0f * ( 1.0f - ( 1.0f - t ) * ( 1.0f - t ) ) );
        }
    }

    if ( !active_ )
    {
        drawStatusBox( w, h );
        SDL_SetRenderDrawBlendMode( r, prevBlend );
        return;
    }

    Layout l;
    computeLayout( w, h, l );
    const int u = l.u;

    visibleRows_ = imax( 1, ( 8 * u ) / ( 4 * u ) );
    ensureCursorVisible( );

    // palette
    const SDL_Color accent    = config_.colors.accent;
    const SDL_Color accentDk  = config_.colors.accentDk;
    const SDL_Color panelCol  = config_.colors.panel;
    const SDL_Color border    = config_.colors.border;
    const SDL_Color cardCol   = config_.colors.card;
    const SDL_Color fieldCol  = config_.colors.field;
    const SDL_Color textHi    = config_.colors.textHi;
    const SDL_Color textMid   = config_.colors.textMid;
    const SDL_Color textLow   = config_.colors.textLow;
    const bool      ttf       = useTtf( );
    const std::string dot     = ttf ? "  \xC2\xB7  " : "  -  ";

    // Backdrop and panel
    rect( 0, 0, w, h, config_.colors.backdrop, 185 );
    shadow( l.panel.x, l.panel.y, l.panel.w, l.panel.h, u * 2, u * 3 / 2 );
    rrect( l.panel.x, l.panel.y, l.panel.w, l.panel.h, u * 2, border, 255 );
    rrect( l.panel.x + 1, l.panel.y + 1, l.panel.w - 2, l.panel.h - 2, u * 2 - 1, panelCol, 255 );

    // Title bar
    {
        int px  = u * 17 / 10;
        int th  = textH( px, true );
        text( l.titleRow.x + u / 2, l.titleRow.y + ( l.titleRow.h - th ) / 2, px, true, config_.texts.title, textHi );

        // mode toggle button
        {
            bool modeFocused = ( zone_ == ZoneModeToggle );
            SDL_Color btnFace = modeFocused ? rgb( 48, 56, 82 ) : cardCol;
            SDL_Color btnEdge = modeFocused ? accent : border;
            SDL_Color btnText = modeFocused ? accent : textHi;

            if ( modeFocused )
            {
                glow( l.modeBtn.x, l.modeBtn.y, l.modeBtn.w, l.modeBtn.h, l.modeBtn.h / 2, accent, 35 );
            }
            rrect( l.modeBtn.x, l.modeBtn.y, l.modeBtn.w, l.modeBtn.h, l.modeBtn.h / 2, btnEdge, 255 );
            rrect( l.modeBtn.x + 1, l.modeBtn.y + 1, l.modeBtn.w - 2, l.modeBtn.h - 2, l.modeBtn.h / 2 - 1, btnFace, 255 );

            std::string modeStr = listMode_ ? config_.texts.modeList : config_.texts.modeView;
            int mpx = u * 11 / 10;
            int mw  = textW( modeStr, mpx, true );
            int mh  = textH( mpx, true );
            text( l.modeBtn.x + ( l.modeBtn.w - mw ) / 2,
                  l.modeBtn.y + ( l.modeBtn.h - mh ) / 2,
                  mpx, true, modeStr, btnText );
        }

        // scope toggle button
        if ( hasCollectionScope( ) )
        {
            bool scopeFocused = ( zone_ == ZoneScopeToggle );
            SDL_Color btnFace = scopeFocused ? rgb( 48, 56, 82 ) : cardCol;
            SDL_Color btnEdge = scopeFocused ? accent : border;
            SDL_Color btnText = scopeFocused ? accent : textHi;

            if ( scopeFocused )
            {
                glow( l.scopeBtn.x, l.scopeBtn.y, l.scopeBtn.w, l.scopeBtn.h, l.scopeBtn.h / 2, accent, 35 );
            }
            rrect( l.scopeBtn.x, l.scopeBtn.y, l.scopeBtn.w, l.scopeBtn.h, l.scopeBtn.h / 2, btnEdge, 255 );
            rrect( l.scopeBtn.x + 1, l.scopeBtn.y + 1, l.scopeBtn.w - 2, l.scopeBtn.h - 2, l.scopeBtn.h / 2 - 1, btnFace, 255 );

            std::string scopeStr;
            if ( scopeAll_ )
            {
                scopeStr = !config_.texts.scopeAll.empty( ) ? config_.texts.scopeAll : "SCOPE: ALL";
            }
            else
            {
                if ( !currentCollection_.empty( ) )
                {
                    scopeStr = "SCOPE: " + currentCollection_;
                }
                else
                {
                    scopeStr = !config_.texts.scopeCurrent.empty( ) ? config_.texts.scopeCurrent : "SCOPE: CURRENT";
                }
            }

            int spx = u * 11 / 10;
            std::string fittedScope = fitTo( scopeStr, spx, true, l.scopeBtn.w - u * 2 );
            int sw  = textW( fittedScope, spx, true );
            int sh  = textH( spx, true );
            text( l.scopeBtn.x + ( l.scopeBtn.w - sw ) / 2,
                  l.scopeBtn.y + ( l.scopeBtn.h - sh ) / 2,
                  spx, true, fittedScope, btnText );
        }

        if ( l.genreBtn.w > 0 )
        {
            bool genreFocused = ( zone_ == ZoneGenreToggle );
            SDL_Color btnFace = genreFocused ? rgb( 48, 56, 82 ) : cardCol;
            SDL_Color btnEdge = genreFocused ? accent : border;
            SDL_Color btnText = genreFocused ? accent : ( genreIndex_ > 0 ? accent : textHi );

            if ( genreFocused )
            {
                glow( l.genreBtn.x, l.genreBtn.y, l.genreBtn.w, l.genreBtn.h, l.genreBtn.h / 2, accent, 35 );
            }
            rrect( l.genreBtn.x, l.genreBtn.y, l.genreBtn.w, l.genreBtn.h, l.genreBtn.h / 2, btnEdge, 255 );
            rrect( l.genreBtn.x + 1, l.genreBtn.y + 1, l.genreBtn.w - 2, l.genreBtn.h - 2, l.genreBtn.h / 2 - 1, btnFace, 255 );

            std::string genreStr;
            if ( genreIndex_ == 0 )
            {
                genreStr = !config_.texts.genreAll.empty( ) ? config_.texts.genreAll : "GENRE: ALL";
            }
            else
            {
                std::string gLabel = !config_.texts.genreLabel.empty( ) ? config_.texts.genreLabel : "GENRE";
                genreStr = gLabel + ": " + availableGenres_[genreIndex_];
            }

            int gpx = u * 11 / 10;
            std::string fittedGenre = fitTo( genreStr, gpx, true, l.genreBtn.w - u * 2 );
            int gw  = textW( fittedGenre, gpx, true );
            int gh  = textH( gpx, true );
            text( l.genreBtn.x + ( l.genreBtn.w - gw ) / 2,
                  l.genreBtn.y + ( l.genreBtn.h - gh ) / 2,
                  gpx, true, fittedGenre, btnText );
        }

        std::string sep = !config_.texts.separator.empty( ) ? config_.texts.separator : dot;
        std::string gameWord = ( entries_.size( ) == 1 ) ? config_.texts.game : config_.texts.games;
        std::string info;
        if ( !config_.texts.infoFormat.empty( ) )
        {
            info = config_.texts.infoFormat;
            size_t p = info.find( "{collections}" );
            if ( p != std::string::npos ) info.replace( p, 13, std::to_string( groupCount_ ) );
            p = info.find( "{games}" );
            if ( p != std::string::npos ) info.replace( p, 7, std::to_string( entries_.size( ) ) );
            p = info.find( "{collectionLabel}" );
            if ( p != std::string::npos ) info.replace( p, 17, ( groupCount_ == 1 ? config_.texts.collection : config_.texts.collections ) );
            p = info.find( "{gameLabel}" );
            if ( p != std::string::npos ) info.replace( p, 11, gameWord );
        }
        else
        {
            info = std::to_string( entries_.size( ) ) + " " + gameWord;
            if ( groupCount_ > 0 )
            {
                std::string collWord = ( groupCount_ == 1 ) ? config_.texts.collection : config_.texts.collections;
                info = std::to_string( groupCount_ ) + " " + collWord + sep + info;
            }
        }
        int ipx = u;
        int iw  = textW( info, ipx, false );
        text( l.titleRow.x + l.titleRow.w - iw - u / 2,
              l.titleRow.y + ( l.titleRow.h - textH( ipx, false ) ) / 2, ipx, false, info, textLow );
    }

    // Query input field
    {
        const SDL_Rect &f = l.field;
        int rad = f.h / 2;
        int bw  = imax( 2, u / 8 );
        glow( f.x, f.y, f.w, f.h, rad, accent, 26 );
        rrect( f.x, f.y, f.w, f.h, rad, accent, 150 );
        rrect( f.x + bw, f.y + bw, f.w - 2 * bw, f.h - 2 * bw, rad - bw, fieldCol, 255 );

        // magnifier icon
        int cx = f.x + u * 5 / 2;
        int cy = f.y + f.h / 2 - u / 5;
        int ro = u * 8 / 10;
        int ri = ro - imax( 2, u / 7 );
        rrect( cx - ro, cy - ro, ro * 2, ro * 2, ro, textMid, 255 );
        rrect( cx - ri, cy - ri, ri * 2, ri * 2, ri, fieldCol, 255 );
        int hr = imax( 2, u / 7 );
        for ( int i = 0; i <= 6; ++i )
        {
            int hx = cx + ro * 7 / 10 + i * u / 9;
            int hy = cy + ro * 7 / 10 + i * u / 9;
            rrect( hx - hr, hy - hr, hr * 2, hr * 2, hr, textMid, 255 );
        }

        // result count chip (right)
        std::string chip;
        SDL_Color   chipCol = accent;
        std::string trimmedQuery = SearchMatcher::trim( query_ );
        std::string activeGenre = getActiveGenreFilter( );
        if ( query_.empty( ) && activeGenre.empty( ) ) chip = "";
        else if ( results_.empty( ) )                 { chip = config_.texts.noMatches; chipCol = config_.colors.noMatches; }
        else if ( trimmedQuery.size( ) < 3 && activeGenre.empty( ) )
        {
            chip = std::to_string( results_.size( ) ) + " " + ( results_.size( ) == 1 ? config_.texts.game : config_.texts.games ) + " [min 3]";
            chipCol = textMid;
        }
        else chip = std::to_string( results_.size( ) ) + " " + ( results_.size( ) == 1 ? config_.texts.resultSingle : config_.texts.resultPlural );

        int chipLeft = f.x + f.w - u;
        if ( !chip.empty( ) )
        {
            int cpx = u * 11 / 10;
            int cw  = textW( chip, cpx, true ) + u * 2;
            int ch  = u * 2 + u / 2;
            int chx = f.x + f.w - u * 3 / 2 - cw;
            rrect( chx, f.y + ( f.h - ch ) / 2, cw, ch, ch / 2, chipCol, 40 );
            text( chx + u, f.y + ( f.h - textH( cpx, true ) ) / 2, cpx, true, chip, chipCol );
            chipLeft = chx - u;
        }

        // query text
        int qpx = u * 18 / 10;
        int qx  = f.x + u * 5;
        int qth = textH( qpx, false );
        int qy  = f.y + ( f.h - qth ) / 2;
        int avail = chipLeft - qx - u;

        std::string q = query_;
        while ( q.size( ) > 1 && textW( q, qpx, false ) > avail ) q.erase( 0, 1 );

        if ( query_.empty( ) )
        {
            text( qx, qy, qpx, false, fitTo( config_.texts.placeholder, qpx, false, avail ), textLow );
        }
        else
        {
            text( qx, qy, qpx, false, q, textHi );
        }

        if ( ( SDL_GetTicks( ) / 500 ) % 2 == 0 )
        {
            int caretW = imax( 2, u / 6 );
            int cx2    = qx + ( query_.empty( ) ? 0 : textW( q, qpx, false ) ) + u / 5;
            rrect( cx2, f.y + ( f.h - qth ) / 2 + qth / 8, caretW, qth * 3 / 4, caretW / 2, accent, 255 );
        }
    }

    // Content panels
    shadow( l.kbCard.x, l.kbCard.y, l.kbCard.w, l.kbCard.h, u, u / 2 );
    rrect( l.kbCard.x, l.kbCard.y, l.kbCard.w, l.kbCard.h, u, cardCol, 255 );

    // Virtual keyboard
    for ( int row = 0; row <= kSpecialRow; ++row )
    {
        int buttons = ( row == kSpecialRow ) ? kSpecialKeys : kCols;
        for ( int b = 0; b < buttons; ++b )
        {
            int cols  = ( row == kSpecialRow ) ? 2 : 1;
            int x     = l.keysX + b * cols * l.cell + l.keyGap / 2;
            int y     = l.keysY + row * l.cell + l.keyGap / 2;
            int bw    = cols * l.cell - l.keyGap;
            int bh    = l.cell - l.keyGap;
            int depth = imax( 2, l.cell / 14 );
            int rad   = imax( 3, l.cell / 5 );
            int faceH = bh - depth;

            bool focused = false;
            if ( zone_ == ZoneKeys && row == keyRow_ )
            {
                int fb  = ( row == kSpecialRow ) ? keyCol_ / 2 : keyCol_;
                focused = ( fb == b );
            }

            SDL_Color face = config_.colors.keyFace;
            SDL_Color edge = config_.colors.keyEdge;
            if ( row == kSpecialRow )
            {
                face = config_.colors.specialFace;
                edge = config_.colors.specialEdge;
                if ( b == 3 )
                {
                    bool canCommit = ( SearchMatcher::trim( query_ ).size( ) >= 3 && !results_.empty( ) );
                    if ( canCommit )
                    {
                        face = config_.colors.okFace; edge = config_.colors.okEdge;
                    }
                    else
                    {
                        face = rgb( 48, 56, 78 ); edge = rgb( 32, 36, 52 );
                    }
                }  // OK
                if ( b == 4 ) { face = config_.colors.cancelFace; edge = config_.colors.cancelEdge; }  // CANCEL
            }
            SDL_Color label = textHi;
            if ( row == kSpecialRow && b == 3 && SearchMatcher::trim( query_ ).size( ) < 3 && !focused )
            {
                label = textLow;
            }

            if ( focused )
            {
                glow( x, y, bw, bh, rad, accent, 70 );
                face  = accent;
                edge  = config_.colors.focusEdge;
                label = accentDk;
            }

            rrect( x, y + depth, bw, faceH, rad, edge, 255 );
            rrect( x, y, bw, faceH, rad, face, 255 );
            // light top edge
            rrect( x + 2, y + 1, bw - 4, imax( 2, faceH / 3 ), rad, rgb( 255, 255, 255 ), focused ? 38 : 16 );

            std::string text_;
            int         px;
            if ( row == kSpecialRow )
            {
                switch ( b )
                {
                case 0: text_ = config_.texts.keySpace;  break;
                case 1: text_ = config_.texts.keyDel;    break;
                case 2: text_ = config_.texts.keyClear;  break;
                case 3: text_ = config_.texts.keyOk;     break;
                case 4: text_ = config_.texts.keyCancel; break;
                default: text_ = ""; break;
                }
                px    = imax( 7, l.cell * 27 / 100 );
                while ( px > 7 && textW( text_, px, true ) > bw - l.keyGap * 2 ) --px;
            }
            else
            {
                text_ = std::string( 1, kCharRows[row][b] );
                px    = imax( 7, l.cell * 42 / 100 );
            }

            int tw = textW( text_, px, true );
            int th = textH( px, true );
            text( x + ( bw - tw ) / 2, y + ( faceH - th ) / 2, px, true, text_, label );
        }
    }

    // Live search results
    if ( listMode_ && l.resultsCard.w > 0 )
    {
        const SDL_Rect &rc = l.resultsCard;

        shadow( rc.x, rc.y, rc.w, rc.h, u, u / 2 );
        rrect( rc.x, rc.y, rc.w, rc.h, u, cardCol, 255 );

        const int inset = u;
        const int titlePx = imax( 10, u * 13 / 10 );
        const int itemPx = imax( 10, u * 12 / 10 );
        const int detailPx = imax( 9, u * 9 / 10 );

        const int headingY = rc.y + inset;
        text( rc.x + inset, headingY, titlePx, true, config_.texts.matchingTitle, textHi );

        const int listY = headingY + textH( titlePx, true ) + u;
        const int listH = l.previewBox.h > 0
            ? imax( 0, l.previewBox.y - u - listY )
            : imax( 0, rc.y + rc.h - inset - listY );

        const int minRowH = imax( u * 2 + u / 4, textH( itemPx, true ) + ( scopeAll_ ? textH( detailPx, false ) + u / 4 : u / 3 ) );
        const int targetRows = 10;
        int rowH = listH / targetRows;
        if ( rowH < minRowH ) rowH = minRowH;
        const int rows = imax( 1, listH / rowH );
        visibleRows_ = rows;
        ensureCursorVisible( );

        std::string trimmedListQ = SearchMatcher::trim( query_ );
        bool hasGenreFilter = ( genreIndex_ > 0 && genreIndex_ < static_cast<int>( availableGenres_.size( ) ) );

        if ( query_.empty( ) && !hasGenreFilter )
        {
            text( rc.x + inset, listY, detailPx, false,
                  fitTo( config_.texts.listPrompt,
                         detailPx, false, rc.w - 2 * inset ), textMid );

            if ( !recentSearches_.empty( ) )
            {
                int rY = listY + textH( detailPx, false ) + u * 2;
                text( rc.x + inset, rY, detailPx, true, config_.texts.recentTitle, accent );
                rY += textH( detailPx, true ) + u;

                int itemH = textH( itemPx, false ) + u;
                for ( size_t ri = 0; ri < recentSearches_.size( ); ++ri )
                {
                    int itemY = rY + static_cast<int>( ri ) * itemH;
                    bool isSelected = ( zone_ == ZoneRecent && recentCursor_ == static_cast<int>( ri ) );
                    if ( isSelected )
                    {
                        rrect( rc.x + inset - u / 2, itemY - u / 4, rc.w - 2 * inset + u, itemH, u / 2, config_.colors.resultFocused, 255 );
                        glow( rc.x + inset - u / 2, itemY - u / 4, rc.w - 2 * inset + u, itemH, u / 2, accent, 1 );
                        text( rc.x + inset, itemY, itemPx, true, "> " + recentSearches_[ri], textHi );
                    }
                    else
                    {
                        text( rc.x + inset, itemY, itemPx, false, "  " + recentSearches_[ri], textHi );
                    }
                }

                int clearY = rY + static_cast<int>( recentSearches_.size( ) ) * itemH + u / 2;
                bool isClearSelected = ( zone_ == ZoneRecent && recentCursor_ == static_cast<int>( recentSearches_.size( ) ) );
                std::string clearLabel = isClearSelected ? "> [ LIMPAR HISTORICO ]" : "  [ Limpar Historico ]";
                if ( isClearSelected )
                {
                    rrect( rc.x + inset - u / 2, clearY - u / 4, rc.w - 2 * inset + u, itemH, u / 2, config_.colors.resultFocused, 255 );
                    text( rc.x + inset, clearY, itemPx, true, clearLabel, accent );
                }
                else
                {
                    text( rc.x + inset, clearY, detailPx, false, clearLabel, textMid );
                }
            }
        }
        else if ( results_.empty( ) )
        {
            text( rc.x + inset, listY, detailPx, false,
                  config_.texts.listNoMatches, textMid );
        }
        else
        {
            for ( int i = 0; i < rows; ++i )
            {
                const size_t resultPos = static_cast<size_t>( resultTop_ + i );
                if ( resultPos >= results_.size( ) ) break;

                const SearchMatcher::Entry &entry = entries_[results_[resultPos]];
                const int y = listY + i * rowH;
                const int maxTextW = imax( 1, rc.w - 2 * inset );

                // Highlight the selected result only in List Mode.
                if ( static_cast<int>( resultPos ) == resultCursor_ )
                {
                    const SDL_Color highlight = zone_ == ZoneResults
                        ? config_.colors.resultFocused
                        : config_.colors.resultInactive;

                    rrect( rc.x + inset / 2,
                           y - u / 4,
                           rc.w - inset,
                           rowH - u / 4,
                           u / 2,
                           highlight,
                           255 );

                    if ( zone_ == ZoneResults )
                    {
                        rect( rc.x + inset / 2,
                              y,
                              imax( 2, u / 4 ),
                              rowH - u,
                              config_.colors.resultStripe,
                              255 );
                    }
                }

                std::string titleStr = entry.isFavorite ? ( "★ " + entry.display ) : entry.display;
                SDL_Color titleColor = entry.isFavorite ? accent : textHi;
                text( rc.x + inset, y, itemPx, true,
                      fitTo( titleStr, itemPx, true, maxTextW ), titleColor );

                std::string secondary;
                if ( scopeAll_ && !entry.groupName.empty( ) )
                {
                    secondary = entry.groupName;
                }

                if ( !secondary.empty( ) )
                {
                    text( rc.x + inset,
                          y + textH( itemPx, true ) + u / 4,
                          detailPx, false,
                          fitTo( secondary, detailPx, false, maxTextW ), textMid );
                }
            }
        }

        // Preview panel
        if ( l.previewBox.h > 0 )
        {
            const SDL_Rect &pb = l.previewBox;
            rrect( pb.x, pb.y, pb.w, pb.h, u / 2, border, 255 );
            rrect( pb.x + 1, pb.y + 1, pb.w - 2, pb.h - 2, u / 2 - 1, fieldCol, 255 );

            if ( !results_.empty( ) && resultCursor_ >= 0 &&
                 static_cast<size_t>( resultCursor_ ) < results_.size( ) )
            {
                const SearchMatcher::Entry &cur = entries_[results_[resultCursor_]];

                // Find preview image (default to "logo")
                std::string targetImgPath;
                std::string pType = !config_.previewType.empty( ) ? config_.previewType : "logo";
                std::string colName = !cur.groupName.empty( ) ? cur.groupName : currentCollection_;

                static const char *exts[] = { ".png", ".jpg", ".jpeg", ".webp" };

                if ( !colName.empty( ) && !cur.name.empty( ) )
                {
                    std::string baseDir = Utils::combinePath( Configuration::absolutePath, "collections", colName, "medium_artwork", pType );
                    for ( size_t ei = 0; ei < 4; ++ei )
                    {
                        std::string p = Utils::combinePath( baseDir, cur.name + exts[ei] );
                        if ( Utils::fileExists( p ) )
                        {
                            targetImgPath = p;
                            break;
                        }
                    }
                    if ( targetImgPath.empty( ) )
                    {
                        for ( size_t ei = 0; ei < 4; ++ei )
                        {
                            std::string p = Utils::combinePath( baseDir, cur.display + exts[ei] );
                            if ( Utils::fileExists( p ) )
                            {
                                targetImgPath = p;
                                break;
                            }
                        }
                    }
                    if ( targetImgPath.empty( ) && pType == "logo" )
                    {
                        std::string wheelDir = Utils::combinePath( Configuration::absolutePath, "collections", colName, "medium_artwork", "wheel" );
                        for ( size_t ei = 0; ei < 4; ++ei )
                        {
                            std::string p = Utils::combinePath( wheelDir, cur.name + exts[ei] );
                            if ( Utils::fileExists( p ) )
                            {
                                targetImgPath = p;
                                break;
                            }
                        }
                    }
                }

                if ( targetImgPath.empty( ) && !cur.name.empty( ) )
                {
                    std::string searchColDir = Utils::combinePath( Configuration::absolutePath, "collections", "Search Collection", "medium_artwork", pType );
                    for ( size_t ei = 0; ei < 4; ++ei )
                    {
                        std::string p = Utils::combinePath( searchColDir, cur.name + exts[ei] );
                        if ( Utils::fileExists( p ) )
                        {
                            targetImgPath = p;
                            break;
                        }
                    }
                }

                // Update preview texture cache
                if ( targetImgPath != currentPreviewPath_ )
                {
                    if ( currentPreviewTex_ )
                    {
                        SDL_DestroyTexture( currentPreviewTex_ );
                        currentPreviewTex_ = NULL;
                    }
                    currentPreviewPath_ = targetImgPath;
                    currentPreviewW_ = 0;
                    currentPreviewH_ = 0;
                    if ( !targetImgPath.empty( ) )
                    {
                        currentPreviewTex_ = IMG_LoadTexture( rend_, targetImgPath.c_str( ) );
                        if ( currentPreviewTex_ )
                        {
                            SDL_QueryTexture( currentPreviewTex_, NULL, NULL, &currentPreviewW_, &currentPreviewH_ );
                        }
                    }
                }

                int textStartX = pb.x + u;
                int textMaxW   = pb.w - 2 * u;

                if ( currentPreviewTex_ && currentPreviewW_ > 0 && currentPreviewH_ > 0 )
                {
                    // Render Image with Aspect Ratio on the left
                    int maxImgW = pb.w * 35 / 100;
                    int maxImgH = pb.h - u;
                    float scale = std::min( static_cast<float>( maxImgW ) / currentPreviewW_,
                                            static_cast<float>( maxImgH ) / currentPreviewH_ );
                    int drawW = static_cast<int>( currentPreviewW_ * scale );
                    int drawH = static_cast<int>( currentPreviewH_ * scale );
                    SDL_Rect dst = { pb.x + u + ( maxImgW - drawW ) / 2,
                                     pb.y + ( pb.h - drawH ) / 2,
                                     drawW, drawH };
                    SDL_RenderCopy( rend_, currentPreviewTex_, NULL, &dst );

                    textStartX = pb.x + u + maxImgW + u;
                    textMaxW   = pb.w - ( maxImgW + 3 * u );
                }

                // Render Metadata Text (Text Fallback when no image, or alongside image)
                int titlePx = imax( 11, u * 13 / 10 );
                int metaPx  = imax( 10, u * 11 / 10 );
                int subPx   = imax( 9, u );

                std::string headerTitle = cur.isFavorite ? ( "\u2605 " + cur.display ) : cur.display;
                std::string metaLine;
                if ( !cur.year.empty( ) && cur.year != "0" ) metaLine += cur.year;
                if ( !cur.manufacturer.empty( ) ) metaLine += ( metaLine.empty( ) ? "" : "  ·  " ) + cur.manufacturer;
                if ( !cur.genre.empty( ) ) metaLine += ( metaLine.empty( ) ? "" : "  ·  " ) + cur.genre;

                int hTitle = textH( titlePx, true );
                int hGroup = !cur.groupName.empty( ) ? textH( metaPx, true ) : 0;
                int hSub   = !metaLine.empty( ) ? textH( subPx, false ) : 0;

                int totalTextH = hTitle + ( hGroup > 0 ? hGroup + u / 4 : 0 ) + ( hSub > 0 ? hSub + u / 4 : 0 );
                int ty = pb.y + ( pb.h - totalTextH ) / 2;
                if ( ty < pb.y + u / 3 ) ty = pb.y + u / 3;

                text( textStartX, ty, titlePx, true, fitTo( headerTitle, titlePx, true, textMaxW ), cur.isFavorite ? accent : textHi );
                ty += hTitle + u / 4;

                if ( hGroup > 0 )
                {
                    text( textStartX, ty, metaPx, true, fitTo( cur.groupName, metaPx, true, textMaxW ), accent );
                    ty += hGroup + u / 4;
                }

                if ( hSub > 0 )
                {
                    text( textStartX, ty, subPx, false, fitTo( metaLine, subPx, false, textMaxW ), textMid );
                }
            }
        }
    }

    // Footer action hints
    {
        int  px = u * 9 / 10;
        int  x  = l.footer.x + u / 2;
        int  cy = l.footer.y + l.footer.h / 2;
        struct Hint { std::string key; std::string what; std::string btnId; };
        std::vector<Hint> hints;

        if ( zone_ == ZoneModeToggle )
        {
            hints.push_back( { config_.texts.hintSelect, config_.texts.actionToggle, "select" } );
            hints.push_back( { config_.texts.hintDown,   config_.texts.actionKeyboard, "down" } );
            hints.push_back( { config_.texts.hintBack,   config_.texts.actionClose, "back" } );
        }
        else if ( zone_ == ZoneScopeToggle )
        {
            hints.push_back( { config_.texts.hintSelect, config_.texts.actionToggle, "select" } );
            hints.push_back( { config_.texts.hintDown,   config_.texts.actionKeyboard, "down" } );
            hints.push_back( { config_.texts.hintBack,   config_.texts.actionClose, "back" } );
        }
        else if ( zone_ == ZoneGenreToggle )
        {
            hints.push_back( { config_.texts.hintSelect, !config_.texts.genreNext.empty( ) ? config_.texts.genreNext : "next genre", "select" } );
            hints.push_back( { "L1/R1",                  !config_.texts.genrePrevNext.empty( ) ? config_.texts.genrePrevNext : "prev/next genre", "lb" } );
            hints.push_back( { "Y",                      !config_.texts.genreReset.empty( ) ? config_.texts.genreReset : "reset (all)", "btnY" } );
            hints.push_back( { config_.texts.hintDown,   config_.texts.actionKeyboard, "down" } );
            hints.push_back( { config_.texts.hintBack,   config_.texts.actionClose, "back" } );
        }
        else if ( zone_ == ZoneResults )
        {
            hints.push_back( { config_.texts.hintDirections, config_.texts.actionBrowse, "dpad" } );
            hints.push_back( { config_.texts.hintSelect,     config_.texts.actionLaunch, "select" } );
            hints.push_back( { "L1/R1",                      !config_.texts.actionPage.empty( ) ? config_.texts.actionPage : "page +/-", "lb" } );
            hints.push_back( { "O",                          "limpar busca", "" } );
            hints.push_back( { config_.texts.hintLeft,       config_.texts.actionKeyboard, "left" } );
            hints.push_back( { config_.texts.hintBack,       config_.texts.actionClose, "back" } );
        }
        else if ( zone_ == ZoneRecent )
        {
            hints.push_back( { config_.texts.hintDirections, config_.texts.actionMove, "dpad" } );
            hints.push_back( { config_.texts.hintSelect,     "selecionar", "select" } );
            hints.push_back( { "O",                          "apagar recente", "" } );
            hints.push_back( { config_.texts.hintLeft,       config_.texts.actionKeyboard, "left" } );
            hints.push_back( { config_.texts.hintBack,       config_.texts.actionClose, "back" } );
        }
        else
        {
            hints.push_back( { config_.texts.hintDirections, config_.texts.actionMove, "dpad" } );
            hints.push_back( { config_.texts.hintSelect,     config_.texts.actionType, "select" } );
            hints.push_back( { "X",                          !config_.texts.actionBackspace.empty( ) ? config_.texts.actionBackspace : "backspace", "btnX" } );
            hints.push_back( { "Y",                          !config_.texts.actionSpace.empty( ) ? config_.texts.actionSpace : "space", "btnY" } );
            hints.push_back( { config_.texts.hintBack,       config_.texts.actionClose, "back" } );
        }

        for ( size_t i = 0; i < hints.size( ); ++i )
        {
            int texW = 0, texH = 0;
            SDL_Texture *btnTex = getButtonTexture( hints[i].btnId, texW, texH );
            if ( !btnTex )
            {
                btnTex = getButtonTexture( hints[i].key, texW, texH );
            }

            int kh = u * 2 + u / 4;
            if ( btnTex && texW > 0 && texH > 0 )
            {
                int drawH = kh;
                int drawW = texW * drawH / texH;
                SDL_Rect dst = { x, cy - drawH / 2, drawW, drawH };
                SDL_RenderCopy( rend_, btnTex, NULL, &dst );
                x += drawW + u / 2;
            }
            else
            {
                int kw = textW( hints[i].key, px, true ) + u * 3 / 2;
                rrect( x, cy - kh / 2 + 2, kw, kh, u / 2, config_.colors.footerEdge, 255 );
                rrect( x, cy - kh / 2, kw, kh, u / 2, config_.colors.footerFace, 255 );
                text( x + u * 3 / 4, cy - kh / 2 + ( kh - textH( px, true ) ) / 2, px, true, hints[i].key, textHi );
                x += kw + u / 2;
            }

            text( x, cy - textH( px, false ) / 2, px, false, hints[i].what, textMid );
            x += textW( hints[i].what, px, false ) + u * 2;
        }
    }

    SDL_SetRenderDrawBlendMode( r, prevBlend );
}
