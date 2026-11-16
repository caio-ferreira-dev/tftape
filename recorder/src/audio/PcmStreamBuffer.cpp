#include "audio/PcmStreamBuffer.h"

#include <algorithm>
#include <cstring>

namespace tft {

PcmStreamBuffer::PcmStreamBuffer(size_t capacity_bytes) {
    SetCapacity(capacity_bytes);
}

void PcmStreamBuffer::SetCapacity(size_t capacity_bytes) {
    capacity_ = capacity_bytes > 0 ? capacity_bytes : 1;
    if (size_ > capacity_) {
        // mantém os dados mais recentes
        read_pos_ += size_ - capacity_;
        size_ = capacity_;
    }
}

void PcmStreamBuffer::Compact() {
    if (read_pos_ == 0) return;
    if (size_ > 0) {
        std::memmove(buf_.data(), buf_.data() + read_pos_, size_);
    }
    buf_.resize(size_);
    read_pos_ = 0;
}

void PcmStreamBuffer::Clear() {
    buf_.clear();
    read_pos_ = 0;
    size_ = 0;
}

void PcmStreamBuffer::Append(const uint8_t* data, size_t bytes) {
    if (!data || bytes == 0) return;
    Compact();
    buf_.insert(buf_.end(), data, data + bytes);
    size_ += bytes;
    if (size_ > capacity_) {
        const size_t drop = size_ - capacity_;
        buf_.erase(buf_.begin(), buf_.begin() + static_cast<std::ptrdiff_t>(drop));
        size_ = capacity_;
    }
}

size_t PcmStreamBuffer::Read(uint8_t* dst, size_t max_bytes) {
    if (!dst || max_bytes == 0 || size_ == 0) return 0;
    const size_t n = std::min(max_bytes, size_);
    std::memcpy(dst, buf_.data() + read_pos_, n);
    read_pos_ += n;
    size_ -= n;
    if (size_ == 0) {
        buf_.clear();
        read_pos_ = 0;
    }
    return n;
}

} // namespace tft
