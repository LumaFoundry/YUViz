#include "frames/frameQueue.h"
#include "frames/frameMeta.h"

int main(int argc, char *argv[]) {
    FrameMeta meta = FrameMeta();
    meta.setYWidth(900);
    meta.setYHeight(600);
    meta.setUVWidth(450);
    meta.setUVHeight(300);
    meta.setPixelFormat(AV_PIX_FMT_YUV420P);
    meta.setColorRange(AVColorRange::AVCOL_RANGE_MPEG);
    meta.setColorSpace(AVColorSpace::AVCOL_SPC_BT709);
    
    FrameQueue queue = FrameQueue(meta);
    FrameData* data = queue.getTailFrame();

    int yWidth = meta.yWidth();
    int yHeight = meta.yHeight();
    int uvWidth = meta.uvWidth();
    int uvHeight = meta.uvHeight();

    uint8_t* yPlane = data->yPtr();
    uint8_t* uPlane = data->uPtr();
    uint8_t* vPlane = data->vPtr();

    struct YUV { uint8_t y, u, v; };
    YUV grid[3][3] = {
        {{16, 128, 128},  {128, 128, 128},  {235, 128, 128}},
        {{63, 102, 240}, {32, 240, 118},  {173, 42, 26}},
        {{74, 202, 230}, {212, 16, 138}, {191, 196, 16}}
    };

    int tileY = yHeight / 3;
    int tileX = yWidth / 3;
    for (int gy = 0; gy < 3; ++gy) {
        for (int gx = 0; gx < 3; ++gx) {
            YUV color = grid[gy][gx];
            for (int y = gy * tileY; y < (gy + 1) * tileY; ++y) {
                for (int x = gx * tileX; x < (gx + 1) * tileX; ++x) {
                    yPlane[y * yWidth + x] = color.y;
                }
            }
            for (int y = (gy * tileY) / 2; y < ((gy + 1) * tileY) / 2; ++y) {
                for (int x = (gx * tileX) / 2; x < ((gx + 1) * tileX) / 2; ++x) {
                    uPlane[y * uvWidth + x] = color.u;
                    vPlane[y * uvWidth + x] = color.v;
                }
            }
        }
    }

    return 0;
}