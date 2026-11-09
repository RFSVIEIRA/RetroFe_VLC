#pragma once
#include "../Graphics/Component/Component.h"
#include "../Graphics/ViewInfo.h"
#include "../Graphics/Component/Color.h"
#include <vector>

class LibVLCVideo;

class Equalizer : public Component
{
public:
    Equalizer(Page& page, int monitor = 0);
    ~Equalizer() override;

    void setSource(LibVLCVideo* videoSource);
    void update(float dt) override;
    void draw() override;

    void setBarCount(int count);
    void setGap(int gap);
    void setBarColor(const std::string& colorStr);
    void setBackgroundColor(const std::string& colorStr);

private:
    LibVLCVideo* source_ = nullptr;
    std::vector<float> bands_;
    int barCount_ = 16;
    int gap_ = 4;

    Color barColor_;
    Color backgroundColor_;

    void updateBands();
};