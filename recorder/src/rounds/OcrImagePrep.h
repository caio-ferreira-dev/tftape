#pragma once

#include <cstdint>
#include <vector>

namespace tft {

struct ImageRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

/// Converte BGRA -> cinza, aplica contraste e opcionalmente amplia a imagem
/// antes de mandá-la ao Windows OCR. Texto pequeno do HUD é o principal
/// motivo de falha do reconhecedor; ampliar 3x e aumentar o contraste melhora
/// a segmentação dos glifos sem custo perceptível (a amostra é pequena).
/// `rect == nullptr` processa a imagem inteira. `scale` vai de 1 a 4.
bool PrepareOcrBitmap(const std::uint8_t* src,
                      int src_width,
                      int src_height,
                      int src_stride,
                      const ImageRect* rect,
                      int scale,
                      int contrast_percent,
                      std::vector<std::uint8_t>& out,
                      int& out_width,
                      int& out_height);

/// Reduz um frame BGRA a uma "assinatura" em cinza (out_width x out_height).
/// A assinatura serve para localizar o instante exato em que o contador mudou:
/// o OCR confirma o rótulo, a diferença entre assinaturas data a virada.
std::vector<std::uint8_t> MakeBgraSignature(const std::uint8_t* src,
                                            int src_width,
                                            int src_height,
                                            int src_stride,
                                            int out_width = 32,
                                            int out_height = 8);

} // namespace tft
