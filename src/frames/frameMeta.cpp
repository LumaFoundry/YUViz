#include "frameMeta.h"
#include <cassert>


FrameMeta::FrameMeta() {}

FrameMeta::~FrameMeta() = default;

int FrameMeta::yWidth() const {
    return m_yWidth;
}

int FrameMeta::yHeight() const {
    return m_yHeight;
}

int FrameMeta::uvWidth() const {
    return m_uvWidth;
}

int FrameMeta::uvHeight() const {
    return m_uvHeight;
}

int FrameMeta::ySize() const {
    return yWidth() * yHeight();
}

int FrameMeta::uvSize() const {
    return uvWidth() * uvHeight();
}

AVPixelFormat FrameMeta::format() const {
    return m_fmt;
}

AVRational FrameMeta::timeBase() const {
    return m_timeBase;
}

AVRational FrameMeta::sampleAspectRatio() const {
    return m_sampleAspectRatio;
}

AVColorRange FrameMeta::colorRange() const {
    return m_colorRange;
}

AVColorSpace FrameMeta::colorSpace() const {
    return m_colorSpace;
}

std::string FrameMeta::filename() const {
    return m_filename;
}

void FrameMeta::setYWidth(int width) {
    m_yWidth = width;
}

void FrameMeta::setYHeight(int height) {
    m_yHeight = height;
}

void FrameMeta::setUVWidth(int width) {
    m_uvWidth = width;
}

void FrameMeta::setUVHeight(int height) {
    m_uvHeight = height;
}

void FrameMeta::setPixelFormat(AVPixelFormat fmt) {
    m_fmt = fmt;
}

void FrameMeta::setTimeBase(AVRational timeBase) {
    m_timeBase = timeBase;
}

void FrameMeta::setSampleAspectRatio(AVRational sampleAspectRatio) {
    m_sampleAspectRatio = sampleAspectRatio;
}

void FrameMeta::setColorRange(AVColorRange range) {
    m_colorRange = range;
}

void FrameMeta::setColorSpace(AVColorSpace space) {
    m_colorSpace = space;
}

void FrameMeta::setFilename(const std::string& filename) {
    m_filename = filename;
}