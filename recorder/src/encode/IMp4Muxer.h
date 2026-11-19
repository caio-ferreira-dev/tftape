#pragma once

#include <cstdint>
#include <string>

namespace tft {

// Dono do IMFSinkWriter: apenas muxing/escrita de streams. Os tipos de mídia são
// ponteiros opacos (IMFMediaType*/IMFSample*) para manter este header portátil.
class IMp4Muxer {
public:
    virtual ~IMp4Muxer() = default;
    virtual bool Open(const std::string& path, std::string* error) = 0;
    virtual bool AddStream(void* output_media_type, uint32_t& stream_index, std::string* error) = 0;
    virtual bool SetInputMediaType(uint32_t stream_index, void* input_media_type, std::string* error) = 0;
    virtual bool WriteSample(uint32_t stream_index, void* sample, std::string* error) = 0;
    virtual bool BeginWriting(std::string* error) = 0;
    virtual bool Finalize(std::string* error) = 0;
    virtual void Abort() = 0;
    virtual void Close() = 0;
    virtual bool IsOpen() const = 0;
    virtual const std::string& LastError() const = 0;
};

} // namespace tft
