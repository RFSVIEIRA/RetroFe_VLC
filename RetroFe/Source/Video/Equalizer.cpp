#include "Equalizer.h"
#include "LibVLCVideo.h"
#include "../SDL.h"
#include "../Utility/Log.h"
#include "../Graphics/Component/Component.h"
#include <algorithm>
#include <vector>

Equalizer::Equalizer(Page& page, int monitor)
    : Component(page),
    barColor_("#00FF88"),
    backgroundColor_("#00000000")
{
    baseViewInfo.Monitor = monitor;
    bands_.resize(barCount_, 0.0f);
}

Equalizer::~Equalizer() = default;

void Equalizer::setSource(LibVLCVideo* videoSource)
{
    source_ = videoSource;
}

void Equalizer::setBarCount(int count)
{
    barCount_ = std::clamp(count, 4, 64);
    bands_.resize(barCount_, 0.0f);
}

void Equalizer::setGap(int gap)
{
    gap_ = std::max(0, gap);
}

void Equalizer::setBarColor(const std::string& colorStr)
{
    try {
        barColor_ = Color(colorStr);
    }
    catch (...) {
        barColor_ = Color("#00FF88");
    }
}

void Equalizer::setBackgroundColor(const std::string& colorStr)
{
    try {
        backgroundColor_ = Color(colorStr);
    }
    catch (...) {
        backgroundColor_ = Color("#00000000");
    }
}

void Equalizer::update(float dt)
{
    Component::update(dt);

    if (!source_)
    {
        for (auto& b : bands_) b *= 0.65f;   // gentle decay when no source
        return;
    }

    bool playing = source_->isPlaying();
    bool paused = source_->isPaused();

    if (paused || !playing)
    {
        std::fill(bands_.begin(), bands_.end(), 0.0f);
        return;
    }

    // Only call FFT when actually playing
    updateBands();
}

void Equalizer::updateBands()
{
    if (!source_ || source_->isPaused() || !source_->isPlaying())
    {
        std::fill(bands_.begin(), bands_.end(), 0.0f);
        return;
    }

    std::vector<float> raw = source_->getEqualizerBands(barCount_);

    for (int i = 0; i < barCount_; ++i)
    {
        // Stronger multiplier + slight smoothing
        bands_[i] = bands_[i] * 0.75f + raw[i] * 4.2f;
        bands_[i] = std::clamp(bands_[i], 0.0f, 1.0f);
    }
}

void Equalizer::draw()
{
    if (!isVisible()) return;

    float alpha = baseViewInfo.Alpha;
    if (alpha <= 0.01f) return;

    float x = baseViewInfo.XRelativeToOrigin();
    float y = baseViewInfo.YRelativeToOrigin();
    float w = baseViewInfo.ScaledWidth();
    float h = baseViewInfo.ScaledHeight();

    SDL_Rect destRect = {
        static_cast<int>(std::round(x)),
        static_cast<int>(std::round(y)),
        static_cast<int>(std::round(w)),
        static_cast<int>(std::round(h))
    };

    SDL_Renderer* renderer = SDL::getRenderer(baseViewInfo.Monitor);
    if (!renderer) return;

    SDL_Texture* contentTex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, static_cast<int>(w), static_cast<int>(h));
    if (!contentTex) return;

    SDL_SetTextureBlendMode(contentTex, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(renderer, contentTex);

    // Full transparent clear
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    // Background (only if not fully transparent)
    if (backgroundColor_.sdlColor.a > 0)
    {
        backgroundColor_.apply(renderer, 1.0f);
        SDL_RenderFillRect(renderer, nullptr);
    }

    // Bars
    if (barCount_ > 0)
    {
        float totalGap = (barCount_ - 1) * std::max(2, gap_);
        float barWidth = (w - totalGap) / static_cast<float>(barCount_);
        barWidth = std::max(2.0f, barWidth);
        float startX = (w - (barCount_ * barWidth + totalGap)) / 2.0f;

        Uint8 barAlpha = static_cast<Uint8>(barColor_.sdlColor.a * alpha * 0.95f);

        for (int i = 0; i < barCount_; ++i)
        {
            float barHeight = h * bands_[i] * 0.95f;
            barHeight = std::max(1.0f, barHeight);

            SDL_Rect barRect = {
                static_cast<int>(std::round(startX + i * (barWidth + gap_))),
                static_cast<int>(std::round(h - barHeight)),
                static_cast<int>(std::round(barWidth)),
                static_cast<int>(std::round(barHeight))
            };

            SDL_SetRenderDrawColor(renderer,
                barColor_.sdlColor.r,
                barColor_.sdlColor.g,
                barColor_.sdlColor.b,
                barAlpha);
            SDL_RenderFillRect(renderer, &barRect);
        }
    }

    SDL_SetRenderTarget(renderer, nullptr);

    SDL_Rect fullSrc = { 0, 0, static_cast<int>(w), static_cast<int>(h) };
    SDL::renderCopy(contentTex, alpha, &fullSrc, &destRect, baseViewInfo,
        page.getLayoutWidth(baseViewInfo.Monitor),
        page.getLayoutHeight(baseViewInfo.Monitor));

    SDL_DestroyTexture(contentTex);
}