#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "encode/IMp4Muxer.h"

namespace tft {

// Amostra genérica de um encoder (vídeo usa width/height/stride; áudio usa size_bytes).
struct EncoderSample {
    const uint8_t* data = nullptr;
    std::size_t size_bytes = 0;
    int64_t timestamp_100ns = 0;
    int width = 0;
    int height = 0;
    int stride = 0;
};

// Configuração de um encoder é fechada pelo muxer; cada implementação cria seus
// media types e registra o stream. Nenhuma implementação owns o sink writer.
class IEncoder {
public:
    virtual ~IEncoder() = default;
    virtual bool Open(IMp4Muxer& muxer, std::string* error) = 0;
    virtual bool Write(const EncoderSample& sample, std::string* error) = 0;
    virtual void Close() = 0;
    virtual bool IsOpen() const = 0;
    virtual uint32_t StreamIndex() const = 0;
    virtual const std::string& LastError() const = 0;
};

} // namespace tft
