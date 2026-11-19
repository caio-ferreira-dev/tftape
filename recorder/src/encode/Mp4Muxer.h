#pragma once

#include <string>

#include "encode/IMp4Muxer.h"

namespace tft {

// Implementação Media Foundation do muxer: dono do IMFSinkWriter e do ciclo de vida
// do arquivo MP4. Encoders registram streams e escrevem amostras através dele.
class Mp4Muxer final : public IMp4Muxer {
public:
    Mp4Muxer() = default;
    ~Mp4Muxer() override;

    bool Open(const std::string& path, std::string* error) override;
    bool AddStream(void* output_media_type, uint32_t& stream_index, std::string* error) override;
    bool SetInputMediaType(uint32_t stream_index, void* input_media_type, std::string* error) override;
    bool WriteSample(uint32_t stream_index, void* sample, std::string* error) override;
    bool BeginWriting(std::string* error) override;
    bool Finalize(std::string* error) override;
    void Abort() override;
    void Close() override;
    bool IsOpen() const override { return opened_; }
    const std::string& LastError() const override { return last_error_; }

private:
    bool opened_ = false;
    bool finalized_ = false;
    std::string last_error_;
#if defined(_WIN32)
    void* sink_writer_ = nullptr; // IMFSinkWriter*
#endif
};

} // namespace tft
