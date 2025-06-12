#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <memory>
#include <vector>

class FrameData {
public:
    FrameData(int ySize, int uvSize, 
              std::shared_ptr<std::vector<uint8_t>> bufferPtr,
              size_t bufferOffset);
    ~FrameData();

    uint8_t* yPtr() const;
    uint8_t* uPtr() const;
    uint8_t* vPtr() const;
    int64_t pts() const;
    void setPts(int64_t pts);

    // TODO: deal with inconsistent frame size

private:
    int64_t m_pts = -1;
    std::shared_ptr<std::vector<uint8_t>> m_bufferPtr;
    size_t m_bufferOffset;
    std::array<size_t, 3> m_planeOffset;
};