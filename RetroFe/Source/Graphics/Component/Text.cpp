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


#include "Text.h"
#include "../../Utility/Log.h"
#include "../../SDL.h"
#include "../Font.h"
#include <sstream>


Text::Text( std::string text, Page &p, Font *font, int monitor )
    : Component(p)
    , textData_(text)
    , fontInst_(font)
{
    allocateGraphicsMemory( );
    baseViewInfo.Monitor = monitor;
}

Text::~Text( )
{
    freeGraphicsMemory( );
}

void Text::freeGraphicsMemory( )
{
    Component::freeGraphicsMemory( );
}

void Text::allocateGraphicsMemory( )
{
    Component::allocateGraphicsMemory( );
}

void Text::deInitializeFonts( )
{
    fontInst_->deInitialize( );
}

void Text::initializeFonts( )
{
    fontInst_->initialize( );
}

void Text::setText( std::string text, int id )
{
    if ( getId( ) == id )
        textData_ = text;
}

void Text::draw()
{
    Component::draw();

    if (textData_.empty()) return;

    Font* font = baseViewInfo.font ? baseViewInfo.font : fontInst_;
    if (!font || !font->getTexture()) return;

    SDL_Texture* t = font->getTexture();

    float originalFontSize = baseViewInfo.FontSize > 0 ? baseViewInfo.FontSize : 48.0f;
    float baseHeight = (float)font->getHeight();
    float baseScale = originalFontSize / baseHeight;

    // === Measure full text at original size ===
    float fullTextWidth = 0.0f;
    for (char c : textData_)
    {
        Font::GlyphInfo glyph;
        if (font->getRect(c, glyph))
            fullTextWidth += glyph.advance;
    }
    fullTextWidth *= baseScale;
    float fullTextHeight = baseHeight * baseScale;

    // === Determine constraints ===
    float targetWidth = baseViewInfo.MaxWidth > 0 ? baseViewInfo.MaxWidth : baseViewInfo.Width;
    float targetHeight = baseViewInfo.MaxHeight > 0 ? baseViewInfo.MaxHeight : baseViewInfo.Height;

    bool doFit = false;
    float fitScale = 1.0f;

    if (baseViewInfo.autoFit && (targetWidth > 0 || targetHeight > 0))
        doFit = true;
    else if (baseViewInfo.fitToWidth && targetWidth > 0 && fullTextWidth > targetWidth)
        doFit = true;
    else if (baseViewInfo.fitToHeight && targetHeight > 0 && fullTextHeight > targetHeight)
        doFit = true;

    if (doFit)
    {
        float scaleW = targetWidth > 0 ? targetWidth / fullTextWidth : 99999.0f;
        float scaleH = targetHeight > 0 ? targetHeight / fullTextHeight : 99999.0f;

        float candidate = 99999.0f;
        if (baseViewInfo.autoFit)           candidate = std::min(scaleW, scaleH);
        else if (baseViewInfo.fitToWidth)   candidate = scaleW;
        else if (baseViewInfo.fitToHeight)  candidate = scaleH;

        float newFontSize = originalFontSize * candidate;
        newFontSize = std::max(newFontSize, baseViewInfo.minFontSize);
        newFontSize = std::min(newFontSize, baseViewInfo.maxFontSize);

        fitScale = newFontSize / originalFontSize;
    }

    float finalScale = baseScale * fitScale;
    float finalWidth = fullTextWidth * fitScale;
    float finalHeight = fullTextHeight * fitScale;

    // === TEMPORARILY modify ViewInfo so X/YRelativeToOrigin works correctly ===
    float oldWidth = baseViewInfo.Width;
    float oldHeight = baseViewInfo.Height;
    float oldImageW = baseViewInfo.ImageWidth;
    float oldImageH = baseViewInfo.ImageHeight;

    baseViewInfo.Width = finalWidth;
    baseViewInfo.Height = finalHeight;
    baseViewInfo.ImageWidth = finalWidth / fitScale;  // raw pixel width
    baseViewInfo.ImageHeight = finalHeight / fitScale;

    float xOrigin = baseViewInfo.XRelativeToOrigin();
    float yOrigin = baseViewInfo.YRelativeToOrigin();

    // Restore original values
    baseViewInfo.Width = oldWidth;
    baseViewInfo.Height = oldHeight;
    baseViewInfo.ImageWidth = oldImageW;
    baseViewInfo.ImageHeight = oldImageH;

    // === Render with correct scale and position ===
    SDL_Rect dst;
    dst.x = static_cast<int>(xOrigin);
    dst.y = static_cast<int>(yOrigin);

    for (char c : textData_)
    {
        Font::GlyphInfo glyph;
        if (!font->getRect(c, glyph) || glyph.rect.h <= 0) continue;

        SDL_Rect src = glyph.rect;

        dst.w = static_cast<int>(src.w * finalScale);
        dst.h = static_cast<int>(src.h * finalScale);

        if (glyph.minX < 0)
            dst.x += static_cast<int>(glyph.minX * finalScale);

        if (font->getAscent() < glyph.maxY)
            dst.y += static_cast<int>((font->getAscent() - glyph.maxY) * finalScale);

        // Optional: hard clip (only if you want to be extra safe)
        if (targetWidth > 0 && dst.x + dst.w > xOrigin + targetWidth)
        {
            int overflow = (dst.x + dst.w) - (xOrigin + targetWidth);
            dst.w -= overflow;
            if (dst.w <= 0) break;
        }

        SDL::renderCopy(t, baseViewInfo.Alpha, &src, &dst, baseViewInfo,
            page.getLayoutWidth(baseViewInfo.Monitor),
            page.getLayoutHeight(baseViewInfo.Monitor));

        dst.x += static_cast<int>(glyph.advance * finalScale);
    }
}
