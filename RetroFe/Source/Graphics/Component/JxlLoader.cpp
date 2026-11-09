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
#include "JxlLoader.h"
#include "../../Utility/Log.h"
#include <jxl/decode.h>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cstring>

bool JxlLoader::IsJxlFile(const std::string& file)
{
    auto pos = file.find_last_of('.');
    if (pos == std::string::npos) return false;
    std::string ext = file.substr(pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == "jxl";
}

bool JxlLoader::IsAnimated(const std::string& file)
{
    std::ifstream ifs(file, std::ios::binary);
    if (!ifs) return false;

    uint8_t headerBuf[4096];
    ifs.read(reinterpret_cast<char*>(headerBuf), sizeof(headerBuf));
    std::streamsize bytesRead = ifs.gcount();
    if (bytesRead <= 0) return false;

    JxlDecoder* dec = JxlDecoderCreate(nullptr);
    if (!dec) return false;

    if (JXL_DEC_SUCCESS != JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO))
    {
        JxlDecoderDestroy(dec);
        return false;
    }

    JxlDecoderSetInput(dec, headerBuf, static_cast<size_t>(bytesRead));
    JxlDecoderCloseInput(dec);

    bool isAnim = false;
    JxlDecoderStatus status = JxlDecoderProcessInput(dec);
    if (status == JXL_DEC_BASIC_INFO)
    {
        JxlBasicInfo info;
        if (JXL_DEC_SUCCESS == JxlDecoderGetBasicInfo(dec, &info))
        {
            isAnim = (info.have_animation != 0);
        }
    }

    JxlDecoderDestroy(dec);
    return isAnim;
}

SDL_Surface* JxlLoader::LoadSurface(const std::string& file)
{
    std::ifstream ifs(file, std::ios::binary | std::ios::ate);
    if (!ifs) return nullptr;

    std::streamsize fileSize = ifs.tellg();
    if (fileSize <= 0) return nullptr;
    ifs.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(static_cast<size_t>(fileSize));
    if (!ifs.read(reinterpret_cast<char*>(data.data()), fileSize))
        return nullptr;

    JxlDecoder* dec = JxlDecoderCreate(nullptr);
    if (!dec) return nullptr;

    if (JXL_DEC_SUCCESS != JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE))
    {
        JxlDecoderDestroy(dec);
        return nullptr;
    }

    JxlDecoderSetInput(dec, data.data(), data.size());
    JxlDecoderCloseInput(dec);

    JxlBasicInfo info{};
    JxlPixelFormat format = { 4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0 };
    SDL_Surface* surface = nullptr;
    std::vector<uint8_t> tempPixels;

    for (;;)
    {
        JxlDecoderStatus status = JxlDecoderProcessInput(dec);
        if (status == JXL_DEC_ERROR || status == JXL_DEC_NEED_MORE_INPUT)
        {
            if (surface)
            {
                SDL_FreeSurface(surface);
                surface = nullptr;
            }
            break;
        }
        else if (status == JXL_DEC_BASIC_INFO)
        {
            if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(dec, &info) || info.xsize == 0 || info.ysize == 0)
            {
                break;
            }
        }
        else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER)
        {
            size_t bufferSize = 0;
            if (JXL_DEC_SUCCESS != JxlDecoderImageOutBufferSize(dec, &format, &bufferSize))
                break;

            surface = SDL_CreateRGBSurfaceWithFormat(0, static_cast<int>(info.xsize), static_cast<int>(info.ysize), 32, SDL_PIXELFORMAT_RGBA32);
            if (!surface) break;

            size_t expectedTightSize = static_cast<size_t>(info.xsize) * static_cast<size_t>(info.ysize) * 4;
            if (static_cast<size_t>(surface->pitch) == static_cast<size_t>(info.xsize) * 4 && bufferSize == expectedTightSize)
            {
                if (JXL_DEC_SUCCESS != JxlDecoderSetImageOutBuffer(dec, &format, surface->pixels, bufferSize))
                {
                    SDL_FreeSurface(surface);
                    surface = nullptr;
                    break;
                }
            }
            else
            {
                tempPixels.resize(bufferSize);
                if (JXL_DEC_SUCCESS != JxlDecoderSetImageOutBuffer(dec, &format, tempPixels.data(), bufferSize))
                {
                    SDL_FreeSurface(surface);
                    surface = nullptr;
                    break;
                }
            }
        }
        else if (status == JXL_DEC_FULL_IMAGE)
        {
            if (surface && !tempPixels.empty())
            {
                size_t rowBytes = static_cast<size_t>(info.xsize) * 4;
                for (uint32_t y = 0; y < info.ysize; ++y)
                {
                    std::memcpy(static_cast<uint8_t*>(surface->pixels) + y * surface->pitch,
                        tempPixels.data() + y * rowBytes, rowBytes);
                }
            }
            break; // First frame decoded
        }
        else if (status == JXL_DEC_SUCCESS)
        {
            break;
        }
    }

    JxlDecoderDestroy(dec);
    return surface;
}

IMG_Animation* JxlLoader::LoadAnimation(const std::string& file)
{
    std::ifstream ifs(file, std::ios::binary | std::ios::ate);
    if (!ifs) return nullptr;

    std::streamsize fileSize = ifs.tellg();
    if (fileSize <= 0) return nullptr;
    ifs.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(static_cast<size_t>(fileSize));
    if (!ifs.read(reinterpret_cast<char*>(data.data()), fileSize))
        return nullptr;

    JxlDecoder* dec = JxlDecoderCreate(nullptr);
    if (!dec) return nullptr;

    if (JXL_DEC_SUCCESS != JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO | JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE))
    {
        JxlDecoderDestroy(dec);
        return nullptr;
    }

    JxlDecoderSetInput(dec, data.data(), data.size());
    JxlDecoderCloseInput(dec);

    JxlBasicInfo info{};
    JxlPixelFormat format = { 4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0 };

    std::vector<SDL_Surface*> frames;
    std::vector<int> delays;
    SDL_Surface* currentSurface = nullptr;
    int currentDelayMs = 100;
    std::vector<uint8_t> tempPixels;

    for (;;)
    {
        JxlDecoderStatus status = JxlDecoderProcessInput(dec);
        if (status == JXL_DEC_ERROR || status == JXL_DEC_NEED_MORE_INPUT)
        {
            if (currentSurface)
            {
                SDL_FreeSurface(currentSurface);
                currentSurface = nullptr;
            }
            break;
        }
        else if (status == JXL_DEC_BASIC_INFO)
        {
            if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(dec, &info) || info.xsize == 0 || info.ysize == 0)
            {
                break;
            }
        }
        else if (status == JXL_DEC_FRAME)
        {
            JxlFrameHeader frameHeader{};
            if (JXL_DEC_SUCCESS == JxlDecoderGetFrameHeader(dec, &frameHeader))
            {
                if (info.have_animation && info.animation.tps_numerator > 0)
                {
                    currentDelayMs = static_cast<int>(
                        (static_cast<uint64_t>(frameHeader.duration) * 1000ULL * info.animation.tps_denominator) /
                        info.animation.tps_numerator);
                    if (currentDelayMs <= 0) currentDelayMs = 16;
                }
                else
                {
                    currentDelayMs = 100;
                }
            }
        }
        else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER)
        {
            size_t bufferSize = 0;
            if (JXL_DEC_SUCCESS != JxlDecoderImageOutBufferSize(dec, &format, &bufferSize))
                break;

            currentSurface = SDL_CreateRGBSurfaceWithFormat(0, static_cast<int>(info.xsize), static_cast<int>(info.ysize), 32, SDL_PIXELFORMAT_RGBA32);
            if (!currentSurface) break;

            size_t expectedTightSize = static_cast<size_t>(info.xsize) * static_cast<size_t>(info.ysize) * 4;
            if (static_cast<size_t>(currentSurface->pitch) == static_cast<size_t>(info.xsize) * 4 && bufferSize == expectedTightSize)
            {
                tempPixels.clear();
                if (JXL_DEC_SUCCESS != JxlDecoderSetImageOutBuffer(dec, &format, currentSurface->pixels, bufferSize))
                {
                    SDL_FreeSurface(currentSurface);
                    currentSurface = nullptr;
                    break;
                }
            }
            else
            {
                tempPixels.resize(bufferSize);
                if (JXL_DEC_SUCCESS != JxlDecoderSetImageOutBuffer(dec, &format, tempPixels.data(), bufferSize))
                {
                    SDL_FreeSurface(currentSurface);
                    currentSurface = nullptr;
                    break;
                }
            }
        }
        else if (status == JXL_DEC_FULL_IMAGE)
        {
            if (currentSurface)
            {
                if (!tempPixels.empty())
                {
                    size_t rowBytes = static_cast<size_t>(info.xsize) * 4;
                    for (uint32_t y = 0; y < info.ysize; ++y)
                    {
                        std::memcpy(static_cast<uint8_t*>(currentSurface->pixels) + y * currentSurface->pitch,
                            tempPixels.data() + y * rowBytes, rowBytes);
                    }
                }
                frames.push_back(currentSurface);
                delays.push_back(currentDelayMs);
                currentSurface = nullptr;
            }
        }
        else if (status == JXL_DEC_SUCCESS)
        {
            break;
        }
    }

    JxlDecoderDestroy(dec);

    if (frames.empty())
        return nullptr;

    IMG_Animation* anim = static_cast<IMG_Animation*>(SDL_malloc(sizeof(IMG_Animation)));
    if (!anim)
    {
        for (SDL_Surface* s : frames) SDL_FreeSurface(s);
        return nullptr;
    }

    anim->w = static_cast<int>(info.xsize);
    anim->h = static_cast<int>(info.ysize);
    anim->count = static_cast<int>(frames.size());
    anim->frames = static_cast<SDL_Surface**>(SDL_calloc(anim->count, sizeof(SDL_Surface*)));
    anim->delays = static_cast<int*>(SDL_calloc(anim->count, sizeof(int)));

    if (!anim->frames || !anim->delays)
    {
        if (anim->frames) SDL_free(anim->frames);
        if (anim->delays) SDL_free(anim->delays);
        SDL_free(anim);
        for (SDL_Surface* s : frames) SDL_FreeSurface(s);
        return nullptr;
    }

    for (int i = 0; i < anim->count; ++i)
    {
        anim->frames[i] = frames[i];
        anim->delays[i] = delays[i];
    }

    return anim;
}
