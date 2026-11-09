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

// Header-only, dependency-free matching logic used by the search overlay.
// Kept separate from SDL/Page so that it can be tested in isolation.

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace SearchMatcher
{

    // One searchable item. Lower-case / normalised strings are precomputed
    // once when the overlay opens so that every key press is cheap.
    struct Entry
    {
        unsigned int index;      // index in its list (the page's list, or the collection's list)
        int          group;      // -1 = item of the page's current list, otherwise id of the
                                 // collection (menu item) the game lives in
        std::string  name;       // ROM / file name (e.g. "sf2")
        std::string  display;    // text shown in the result list
        std::string  groupName;  // collection the game belongs to (empty for the page's own list)
        std::string  year;
        std::string  manufacturer;
        std::string  genre;
        std::string  detail;     // secondary text (year / manufacturer)
        std::string  titleLower; // lower-case title
        std::string  titleNorm;  // title with only a-z0-9 left
        std::string  primary;    // lower-case title + full title + name
        std::string  primaryNorm;
        std::string  secondary;  // lower-case manufacturer + year + genre
        std::string  secondaryNorm;
        bool         isFavorite = false;
        bool         isRecent   = false;
    };

    struct Query
    {
        std::vector<std::string> raw;  // lower-case tokens
        std::vector<std::string> norm; // same tokens, only a-z0-9 left
        std::vector<std::string> alt;  // roman/arabic equivalent if any
    };

    inline std::string romanEquivalent( const std::string &token )
    {
        static const struct {
            const char *arabic;
            const char *roman;
        } table[] = {
            { "1",  "i" },
            { "2",  "ii" },
            { "3",  "iii" },
            { "4",  "iv" },
            { "5",  "v" },
            { "6",  "vi" },
            { "7",  "vii" },
            { "8",  "viii" },
            { "9",  "ix" },
            { "10", "x" },
            { "11", "xi" },
            { "12", "xii" },
            { "13", "xiii" },
            { "14", "xiv" },
            { "15", "xv" },
            { "16", "xvi" },
            { "17", "xvii" },
            { "18", "xviii" },
            { "19", "xix" },
            { "20", "xx" }
        };
        for ( size_t i = 0; i < sizeof( table ) / sizeof( table[0] ); ++i )
        {
            if ( token == table[i].arabic ) return table[i].roman;
            if ( token == table[i].roman ) return table[i].arabic;
        }
        return std::string( );
    }

    inline bool containsWord( const std::string &hay, const std::string &word )
    {
        if ( word.empty( ) || hay.empty( ) ) return false;
        size_t pos = hay.find( word, 0 );
        while ( pos != std::string::npos )
        {
            bool leftOk = ( pos == 0 || !std::isalnum( static_cast<unsigned char>( hay[pos - 1] ) ) );
            bool rightOk = ( ( pos + word.size( ) ) >= hay.size( ) || !std::isalnum( static_cast<unsigned char>( hay[pos + word.size( )] ) ) );
            if ( leftOk && rightOk ) return true;
            pos = hay.find( word, pos + 1 );
        }
        return false;
    }

    inline bool sameCollection( const std::string &a, const std::string &b )
    {
        if ( a.empty( ) || b.empty( ) ) return false;
        if ( a.size( ) != b.size( ) ) return false;
        for ( size_t i = 0; i < a.size( ); ++i )
        {
            if ( std::tolower( static_cast<unsigned char>( a[i] ) ) !=
                 std::tolower( static_cast<unsigned char>( b[i] ) ) )
            {
                return false;
            }
        }
        return true;
    }

    inline std::string lower( const std::string &s )
    {
        std::string r = s;
        for ( size_t i = 0; i < r.size( ); ++i )
        {
            r[i] = static_cast<char>( std::tolower( static_cast<unsigned char>( r[i] ) ) );
        }
        return r;
    }

    // Lower-case and drop everything that is not a-z / 0-9, so that
    // "pacman" also finds "Ms. Pac-Man".
    inline std::string normalize( const std::string &s )
    {
        std::string r;
        for ( size_t i = 0; i < s.size( ); ++i )
        {
            unsigned char c = static_cast<unsigned char>( s[i] );
            if ( std::isalnum( c ) && c < 128 )
            {
                r += static_cast<char>( std::tolower( c ) );
            }
        }
        return r;
    }

    inline bool contains( const std::string &hay, const std::string &needle )
    {
        return !needle.empty( ) && hay.find( needle ) != std::string::npos;
    }

    inline bool startsWith( const std::string &hay, const std::string &needle )
    {
        return !needle.empty( ) && hay.compare( 0, needle.size( ), needle ) == 0;
    }

    inline std::string trim( const std::string &s )
    {
        size_t b = 0;
        size_t e = s.size( );
        while ( b < e && std::isspace( static_cast<unsigned char>( s[b] ) ) ) ++b;
        while ( e > b && std::isspace( static_cast<unsigned char>( s[e - 1] ) ) ) --e;
        return s.substr( b, e - b );
    }

    inline Entry makeEntry( unsigned int index,
                            const std::string &title,
                            const std::string &fullTitle,
                            const std::string &name,
                            const std::string &manufacturer,
                            const std::string &year,
                            const std::string &genre,
                            int group = -1,
                            const std::string &groupName = std::string( ),
                            bool isFavorite = false,
                            bool isRecent = false )
    {
        Entry e;
        e.index         = index;
        e.group         = group;
        e.name          = name;
        e.display       = title.empty( ) ? name : title;
        e.groupName     = groupName;
        e.year          = year;
        e.manufacturer  = manufacturer;
        e.genre         = genre;
        e.isFavorite    = isFavorite;
        e.isRecent      = isRecent;
        std::string extra;
        if ( !year.empty( ) && year != "0" ) extra += ( extra.empty( ) ? "" : "  ·  " ) + year;
        if ( !manufacturer.empty( ) ) extra += ( extra.empty( ) ? "" : "  ·  " ) + manufacturer;
        e.detail        = groupName.empty( ) ? extra : ( extra.empty( ) ? groupName : groupName + "  ·  " + extra );
        e.titleLower    = lower( e.display );
        e.titleNorm     = normalize( e.display );
        e.primary       = lower( title + "\n" + fullTitle + "\n" + name );
        e.primaryNorm   = normalize( title + fullTitle + name );
        e.secondary     = lower( manufacturer + "\n" + year + "\n" + genre + "\n" + groupName );
        e.secondaryNorm = normalize( manufacturer + year + genre + groupName );
        return e;
    }

    inline Query parseQuery( const std::string &text )
    {
        Query q;
        std::string token;
        for ( size_t i = 0; i <= text.size( ); ++i )
        {
            if ( i == text.size( ) || std::isspace( static_cast<unsigned char>( text[i] ) ) )
            {
                if ( !token.empty( ) )
                {
                    q.raw.push_back( lower( token ) );
                    q.norm.push_back( normalize( token ) );
                    q.alt.push_back( romanEquivalent( q.norm.back( ) ) );
                    token.clear( );
                }
            }
            else
            {
                token += text[i];
            }
        }
        return q;
    }

    // Returns -1 when the entry does not match, otherwise a rank where
    // lower is better:
    //   0 = title starts with the first search word
    //   1 = all words found in title / full title / name
    //   2 = some words only found in manufacturer / year / genre
    // Every search word has to match somewhere (AND semantics).
    inline int score( const Entry &e, const Query &q )
    {
        if ( q.raw.empty( ) ) return -1;

        bool allInPrimary = true;
        for ( size_t i = 0; i < q.raw.size( ); ++i )
        {
            bool inPrimary = contains( e.primary, q.raw[i] ) ||
                             contains( e.primaryNorm, q.norm[i] ) ||
                             ( !q.alt[i].empty( ) && ( containsWord( e.primary, q.alt[i] ) || containsWord( e.primaryNorm, q.alt[i] ) ) );
            if ( inPrimary ) continue;

            bool inSecondary = contains( e.secondary, q.raw[i] ) ||
                               contains( e.secondaryNorm, q.norm[i] );
            if ( !inSecondary ) return -1;
            allInPrimary = false;
        }

        if ( !allInPrimary ) return 2;

        if ( startsWith( e.titleLower, q.raw[0] ) || startsWith( e.titleNorm, q.norm[0] ) ||
             ( !q.alt[0].empty( ) && containsWord( e.titleLower, q.alt[0] ) ) )
        {
            return 0;
        }
        return 1;
    }

    // Fills 'out' with indices into 'entries', best matches first. Matches
    // of equal rank keep the order of the list.
    struct MatchHit
    {
        int          score;    // 0, 1, 2
        int          priority; // 0 = favorite, 1 = recent, 2 = normal
        unsigned int index;

        bool operator<( const MatchHit &o ) const
        {
            if ( score != o.score ) return score < o.score;
            if ( priority != o.priority ) return priority < o.priority;
            return index < o.index;
        }
    };

    inline void filter( const std::vector<Entry> &entries,
                        const std::string &text,
                        std::vector<unsigned int> &out,
                        const std::string &collectionFilter = std::string( ),
                        const std::string &genreFilter = std::string( ) )
    {
        out.clear( );
        Query q = parseQuery( text );
        if ( q.raw.empty( ) && genreFilter.empty( ) ) return;

        std::vector<MatchHit> hits;
        for ( size_t i = 0; i < entries.size( ); ++i )
        {
            if ( !collectionFilter.empty( ) )
            {
                if ( !entries[i].groupName.empty( ) && !sameCollection( entries[i].groupName, collectionFilter ) )
                {
                    continue;
                }
            }

            if ( !genreFilter.empty( ) )
            {
                if ( !sameCollection( entries[i].genre, genreFilter ) )
                {
                    continue;
                }
            }

            int s = 0;
            if ( !q.raw.empty( ) )
            {
                s = score( entries[i], q );
                if ( s < 0 ) continue;
            }

            int prio = entries[i].isFavorite ? 0 : ( entries[i].isRecent ? 1 : 2 );
            hits.push_back( MatchHit{ s, prio, static_cast<unsigned int>( i ) } );
        }

        std::sort( hits.begin( ), hits.end( ) );

        out.reserve( hits.size( ) );
        for ( size_t i = 0; i < hits.size( ); ++i )
        {
            out.push_back( hits[i].index );
        }
    }

} // namespace SearchMatcher
