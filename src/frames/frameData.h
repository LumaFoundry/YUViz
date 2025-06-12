#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <memory>
#include <vector>

class FrameData {
public:
    FrameData(int ySize, int uvSize, int64_t pts, 
              std::shared_ptr<std::vector<uint8_t>> poolPtr,
              size_t poolOffset);
    ~FrameData();

    uint8_t* yPtr() const;
    uint8_t* uPtr() const;
    uint8_t* vPtr() const;
    int64_t pts() const;
    void setPts(int64_t pts);

    // TODO: deal with inconsistent frame size

private:
    int64_t m_pts;
    std::shared_ptr<std::vector<uint8_t>> m_poolPtr;
    size_t m_poolOffset;
    std::array<size_t, 3> m_planeOffset;
};