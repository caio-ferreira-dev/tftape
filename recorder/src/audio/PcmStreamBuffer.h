#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tft {

/// Buffer linear (thread-confined) usado para guardar sobras de pacotes PCM entre
/// leituras. Mantém no máximo `Capacity()` bytes: ao exceder, descarta os bytes mais
/// antigos para limitar latência/memória em capturas ao vivo.
class PcmStreamBuffer {
public:
    explicit PcmStreamBuffer(size_t capacity_bytes = 1u << 20);

    void SetCapacity(size_t capacity_bytes);
    size_t Capacity() const { return capacity_; }
    size_t Size() const { return size_; }
    bool Empty() const { return size_ == 0; }

    void Clear();

    /// Anexa bytes ao fim do buffer. Se ultrapassar a capacidade, descarta o início.
    void Append(const uint8_t* data, size_t bytes);

    /// Copia até `max_bytes` dos dados mais antigos e os remove do buffer.
    /// Retorna a quantidade de bytes copiados.
    size_t Read(uint8_t* dst, size_t max_bytes);

private:
    void Compact();

    std::vector<uint8_t> buf_;
    size_t read_pos_ = 0; // início dos dados válidos em buf_
    size_t size_ = 0;     // bytes válidos a partir de read_pos_
    size_t capacity_ = 0;
};

} // namespace tft
