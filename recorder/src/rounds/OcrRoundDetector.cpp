#include "rounds/OcrRoundDetector.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "rounds/OcrImagePrep.h"
#include "rounds/OcrRoundText.h"
#include "rounds/OcrTransition.h"
#include "util/Logger.h"

#if defined(_WIN32)
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <winrt/Windows.Storage.Streams.h>

namespace WGI = winrt::Windows::Graphics::Imaging;
namespace WOC = winrt::Windows::Media::Ocr;
#endif

namespace tft {
namespace {

constexpr std::int64_t kTicksPerMs = 10'000;
constexpr std::size_t kMaxQueuedSamples = 3;

int ClampInt(long long value, int low, int high) {
    return static_cast<int>(std::clamp<long long>(value, low, high));
}

struct OcrSample {
    std::vector<std::uint8_t> pixels; // BGRA tightly packed
    int width = 0;
    int height = 0;
    std::int64_t timestamp_100ns = 0;
};

struct OcrMatchRect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

struct RecognizeOutput {
    std::string label;
    std::string raw;
    OcrMatchRect rect;  // coordenadas do bitmap processado (já ampliado)
    bool has_rect = false;
};

/// Recorte do contador dentro da faixa `band`, descoberto a partir da
/// BoundingRect das palavras reconhecidas. Válido somente na worker.
struct CounterLock {
    bool valid = false;
    ImageRect rect{};
    int misses = 0;
};

bool CropBand(const FrameView& frame, const NormalizedRect& band, OcrSample& out) {
    if (!frame.bgra || frame.width <= 0 || frame.height <= 0 || frame.stride <= 0) return false;

    const long long w = frame.width;
    const long long h = frame.height;
    const int x0 = ClampInt(static_cast<long long>(band.x * w), 0, frame.width - 1);
    const int y0 = ClampInt(static_cast<long long>(band.y * h), 0, frame.height - 1);
    const int x1 = ClampInt(static_cast<long long>((band.x + band.w) * w), x0 + 1, frame.width);
    const int y1 = ClampInt(static_cast<long long>((band.y + band.h) * h), y0 + 1, frame.height);

    out.width = x1 - x0;
    out.height = y1 - y0;
    out.pixels.resize(static_cast<std::size_t>(out.width) * out.height * 4);
    for (int y = 0; y < out.height; ++y) {
        const auto* src = frame.bgra
            + static_cast<std::size_t>(y0 + y) * frame.stride
            + static_cast<std::size_t>(x0) * 4;
        auto* dst = out.pixels.data() + static_cast<std::size_t>(y) * out.width * 4;
        std::memcpy(dst, src, static_cast<std::size_t>(out.width) * 4);
    }
    return true;
}

} // namespace

struct OcrRoundDetector::Impl {
    explicit Impl(OcrRoundDetectorConfig config) : config_(std::move(config)) { Reset(); }
    ~Impl() { StopWorker(); }

    void Reset() {
        StopWorker();
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_.clear();
        }
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            queue_.clear();
        }
        last_queued_100ns_ = 0;
        candidate_label_.clear();
        candidate_first_100ns_ = 0;
        candidate_hits_ = 0;
        last_emitted_label_.clear();
        last_emission_100ns_ = 0;
        last_raw_.clear();
        sample_count_ = 0;
        lock_ = CounterLock{};
        history_.clear();
        warned_no_engine_ = false;
        stop_ = false;
        worker_ = std::thread([this]() { WorkerLoop(); });
    }

    void ProcessFrame(const FrameView& frame) {
        if (!frame.bgra || frame.width <= 0 || frame.height <= 0 || frame.stride <= 0) return;
        if (frame.timestamp_100ns < static_cast<std::int64_t>(config_.warmup_ms) * kTicksPerMs) return;
        if (last_queued_100ns_ != 0
            && frame.timestamp_100ns - last_queued_100ns_
               < static_cast<std::int64_t>(config_.sample_interval_ms) * kTicksPerMs) {
            return;
        }

        OcrSample sample;
        if (!CropBand(frame, config_.band, sample)) return;
        sample.timestamp_100ns = frame.timestamp_100ns;
        last_queued_100ns_ = frame.timestamp_100ns;

        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (queue_.size() >= kMaxQueuedSamples) {
                queue_.pop_front(); // descarta a amostra mais antiga: nunca bloqueia o vídeo
                ++dropped_samples_;
            }
            queue_.push_back(std::move(sample));
        }
        queue_cv_.notify_one();
    }

    /// Chamado no Stop(): encerra a worker processando as amostras que ainda
    /// estão na fila, para que a última virada não seja perdida no sidecar.
    void Flush() { StopWorker(); }

    std::vector<DetectedRound> DrainDetections() {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        std::vector<DetectedRound> out;
        out.swap(pending_);
        return out;
    }

private:
    void StopWorker() {
        if (!worker_.joinable()) return;
        stop_ = true;
        queue_cv_.notify_all();
        worker_.join();
        worker_ = std::thread{};
    }

    void Publish(const std::string& label, std::int64_t timestamp_100ns) {
        DetectedRound round;
        round.label = label;
        round.timestamp_100ns = timestamp_100ns;
        round.source = config_.source;
        round.confidence = 1.0; // texto lido e confirmado em duas ou mais leituras
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_.push_back(std::move(round));
        }
        last_emitted_label_ = label;
        last_emission_100ns_ = timestamp_100ns;
        candidate_label_.clear();
        candidate_first_100ns_ = 0;
        candidate_hits_ = 0;
        log::Info("Round detectado por OCR: " + label + " em " +
                  std::to_string(timestamp_100ns / kTicksPerMs) + " ms");
    }

    void OnRecognition(const std::string& label,
                       std::int64_t timestamp_100ns,
                       const std::vector<OcrSignatureSample>& history) {
        // Leitura sem contador é comum: o OCR acerta o número pequeno só de vez
        // em quando. Não pode apagar um candidato ainda dentro da janela.
        if (label.empty()) return;

        if (label == last_emitted_label_) {
            candidate_label_.clear();
            candidate_first_100ns_ = 0;
            candidate_hits_ = 0;
            return;
        }

        if (label != candidate_label_) {
            candidate_label_ = label;
            candidate_first_100ns_ = timestamp_100ns;
            candidate_hits_ = 1;
        } else {
            ++candidate_hits_;
        }

        const std::int64_t window =
            static_cast<std::int64_t>(config_.confirmation_window_ms) * kTicksPerMs;
        if (timestamp_100ns - candidate_first_100ns_ > window) {
            // Candidato velho (ruído isolado): recomeça a contagem agora.
            // Isso acontece quando o OCR lê o contador só uma vez e nunca mais
            // naquele round; sem esse limite, uma leitura errada poderia ser
            // datada minutos depois.
            log::Debug("OCR: candidato " + candidate_label_ + " expirou apos " +
                       std::to_string((timestamp_100ns - candidate_first_100ns_) /
                                      kTicksPerMs) +
                       " ms; reiniciando em " + label);
            candidate_first_100ns_ = timestamp_100ns;
            candidate_hits_ = 1;
        }

        if (candidate_hits_ < config_.confirmation_samples) return;
        if (timestamp_100ns - last_emission_100ns_
            < static_cast<std::int64_t>(config_.cooldown_ms) * kTicksPerMs) {
            return; // mantém o candidato para preservar o timestamp original
        }
        // Data no primeiro instante em que o texto foi lido, não na confirmação.
        // Quando há uma mudança visual nítida logo antes dessa leitura, usa o
        // instante da mudança — o rótulo continua sendo o lido pelo OCR.
        std::int64_t start = candidate_first_100ns_;
        const std::int64_t visual = FindOcrTransition(
            history, candidate_first_100ns_,
            static_cast<std::int64_t>(config_.transition_lookback_ms) * kTicksPerMs,
            config_.transition_min_diff);
        if (visual != 0) start = visual;
        if (start != candidate_first_100ns_) {
            log::Debug("OCR: transição visual em " + std::to_string(start / kTicksPerMs) +
                       " ms (1ª leitura em " +
                       std::to_string(candidate_first_100ns_ / kTicksPerMs) + " ms)");
        }
        Publish(candidate_label_, start);
    }

    void WorkerLoop() {
#if defined(_WIN32)
        try {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
        } catch (...) {
            // Já inicializado nesta thread: ok.
        }

        WOC::OcrEngine engine = nullptr;
        try {
            engine = WOC::OcrEngine::TryCreateFromUserProfileLanguages();
            if (!engine) {
                engine = WOC::OcrEngine::TryCreateFromLanguage(
                    winrt::Windows::Globalization::Language(winrt::hstring(L"en-US")));
            }
            if (!engine) {
                const auto languages = WOC::OcrEngine::AvailableRecognizerLanguages();
                if (languages.Size() > 0) {
                    engine = WOC::OcrEngine::TryCreateFromLanguage(languages.GetAt(0));
                }
            }
        } catch (...) {
            engine = nullptr;
        }
#endif

        // Mesmo após stop_, drena o que já estava na fila (Flush no Stop).
        while (true) {
            OcrSample sample;
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                queue_cv_.wait(lock, [this]() { return stop_ || !queue_.empty(); });
                if (stop_ && queue_.empty()) break;
                sample = std::move(queue_.front());
                queue_.pop_front();
            }

#if defined(_WIN32)
            if (!engine) {
                if (!warned_no_engine_) {
                    log::Warning("Windows OCR indisponível; rounds não serão detectados.");
                    warned_no_engine_ = true;
                }
                continue;
            }

            const auto started = std::chrono::steady_clock::now();
            RecognizeOutput output;
            try {
                Recognize(engine, sample, output);
            } catch (const winrt::hresult_error& ex) {
                log::Warning("OCR falhou: " + winrt::to_string(ex.message())
                             + " (0x" + std::to_string(static_cast<unsigned>(ex.code())) + ")");
                output.label.clear();
                output.raw.clear();
                output.has_rect = false;
            } catch (...) {
                log::Warning("OCR falhou com exceção desconhecida");
                output.label.clear();
                output.raw.clear();
                output.has_rect = false;
            }
            const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started).count();

            if (!output.raw.empty() && output.raw != last_raw_) {
                log::Debug("OCR bruto: " + output.raw + " -> " +
                           (output.label.empty() ? "(sem contador)" : output.label));
                last_raw_ = output.raw;
            }
            if (++sample_count_ % 20 == 0) {
                std::size_t depth = 0;
                {
                    std::lock_guard<std::mutex> lock(queue_mutex_);
                    depth = queue_.size();
                }
                log::Debug("OCR: " + std::to_string(elapsed_ms) + " ms, fila=" +
                           std::to_string(depth) + ", descartadas=" +
                           std::to_string(dropped_samples_.load()) +
                           ", lock=" + (lock_.valid ? "sim" : "nao"));
            }

            UpdateCounterLock(output, sample);
            history_.push_back({sample.timestamp_100ns,
                                MakeBgraSignature(sample.pixels.data(), sample.width,
                                                  sample.height, sample.width * 4, 32, 8)});
            while (!history_.empty()
                   && sample.timestamp_100ns - history_.front().timestamp_100ns
                      > static_cast<std::int64_t>(config_.transition_lookback_ms) * kTicksPerMs
                        + static_cast<std::int64_t>(config_.sample_interval_ms) * kTicksPerMs) {
                history_.erase(history_.begin());
            }
            OnRecognition(output.label, sample.timestamp_100ns, history_);
#else
            (void)sample;
#endif
        }

#if defined(_WIN32)
        try {
            winrt::uninit_apartment();
        } catch (...) {
        }
#endif
    }

#if defined(_WIN32)
    static void CollectFromResult(const WOC::OcrResult& result,
                                  std::string& label,
                                  std::string& joined,
                                  OcrMatchRect& match,
                                  bool& has_match) {
        std::string loose;
        bool exact_found = false;
        for (const auto& line : result.Lines()) {
            const auto text = winrt::to_string(line.Text());
            if (text.empty()) continue;
            if (!joined.empty()) joined += " | ";
            joined += text;

            std::string candidate;
            const bool exact = ParseExactRoundLabel(text, candidate);
            const bool parsed = exact || TryParseRoundLabel(text, candidate);
            if (!parsed) continue;

            if (!has_match) {
                // Usa a BoundingRect das palavras que realmente contêm o
                // contador, não da linha inteira (que pode incluir o timer e
                // outros números do HUD, deixando o recorte largo demais).
                float min_x = 0.0f, min_y = 0.0f, max_x = 0.0f, max_y = 0.0f;
                bool first_word = true;
                for (const auto& word : line.Words()) {
                    const auto word_text = winrt::to_string(word.Text());
                    std::string word_label;
                    if (!TryParseRoundLabel(word_text, word_label)
                        && !ParseExactRoundLabel(word_text, word_label)) {
                        continue;
                    }
                    const auto r = word.BoundingRect();
                    if (first_word) {
                        min_x = r.X;
                        min_y = r.Y;
                        max_x = r.X + r.Width;
                        max_y = r.Y + r.Height;
                        first_word = false;
                    } else {
                        min_x = std::min(min_x, r.X);
                        min_y = std::min(min_y, r.Y);
                        max_x = std::max(max_x, r.X + r.Width);
                        max_y = std::max(max_y, r.Y + r.Height);
                    }
                }
                if (!first_word) {
                    match = {min_x, min_y, max_x - min_x, max_y - min_y};
                    has_match = true;
                }
            }
            if (!exact_found && exact) {
                label = std::move(candidate);
                exact_found = true;
            } else if (loose.empty() && !exact) {
                loose = std::move(candidate);
            }
        }
        if (!exact_found) label = std::move(loose);
    }

    static bool RunOcr(const WOC::OcrEngine& engine,
                       const std::vector<std::uint8_t>& pixels,
                       int width,
                       int height,
                       std::string& label,
                       std::string& raw,
                       OcrMatchRect& match,
                       bool& has_match) {
        winrt::array_view<const std::uint8_t> bytes(pixels.data(), pixels.size());
        const auto buffer = winrt::Windows::Security::Cryptography::CryptographicBuffer::
            CreateFromByteArray(bytes);
        const auto bitmap = WGI::SoftwareBitmap::CreateCopyFromBuffer(
            buffer, WGI::BitmapPixelFormat::Bgra8, width, height,
            WGI::BitmapAlphaMode::Straight);
        const auto result = engine.RecognizeAsync(bitmap).get();
        CollectFromResult(result, label, raw, match, has_match);
        return true;
    }

    void Recognize(const WOC::OcrEngine& engine,
                   const OcrSample& sample,
                   RecognizeOutput& output) {
        // Etapa 1: faixa ampla em BGRA cru, exatamente como antes das
        // melhorias. A faixa estreita (y 4%-9%) capturava só a barra de
        // progresso — o texto do contador fica em y 0%-4% e 9%-18% — e o OCR
        // não retornava nenhuma linha.
        OcrMatchRect wide_match;
        bool has_wide_match = false;
        RunOcr(engine, sample.pixels, sample.width, sample.height,
               output.label, output.raw, wide_match, has_wide_match);

        // Etapa 2: se o contador foi localizado, trava um recorte menor com
        // margem e amplia 3x. É aqui que o 1-5 pequeno/estilizado costuma
        // começar a ser lido.
        if (!lock_.valid && has_wide_match) {
            SetLockFromMatch(wide_match, sample.width, sample.height, output.rect);
            output.has_rect = lock_.valid;
        }
        if (lock_.valid) {
            std::vector<std::uint8_t> tight;
            int tight_w = 0;
            int tight_h = 0;
            if (PrepareOcrBitmap(sample.pixels.data(), sample.width, sample.height,
                                 sample.width * 4, &lock_.rect, config_.tight_scale,
                                 config_.contrast_percent, tight, tight_w, tight_h)) {
                std::string tight_label;
                std::string tight_raw;
                OcrMatchRect tight_match;
                bool has_tight_match = false;
                try {
                    RunOcr(engine, tight, tight_w, tight_h, tight_label, tight_raw,
                           tight_match, has_tight_match);
                } catch (...) {
                    tight_label.clear();
                }
                if (!tight_label.empty()) {
                    output.label = std::move(tight_label);
                    // Raw do recorte travado vai para o log só quando não
                    // houver raw da faixa (evita duplicar linhas).
                    if (output.raw.empty()) output.raw = std::move(tight_raw);
                    if (has_tight_match) {
                        OcrMatchRect adjusted = tight_match;
                        adjusted.x = lock_.rect.x
                            + adjusted.x / static_cast<float>(config_.tight_scale);
                        adjusted.y = lock_.rect.y
                            + adjusted.y / static_cast<float>(config_.tight_scale);
                        adjusted.w /= static_cast<float>(config_.tight_scale);
                        adjusted.h /= static_cast<float>(config_.tight_scale);
                        output.rect = adjusted;
                        output.has_rect = true;
                    }
                }
            }
        }
    }

    void SetLockFromMatch(const OcrMatchRect& match, int bitmap_w, int bitmap_h,
                          OcrMatchRect& out) {
        const float margin = static_cast<float>(config_.lock_margin_percent) / 100.0f;
        const float x = std::max(0.0f, match.x - match.w * margin);
        const float y = std::max(0.0f, match.y - match.h * margin);
        const float w = std::min(static_cast<float>(bitmap_w) - x,
                                 match.w * (1.0f + 2.0f * margin));
        const float h = std::min(static_cast<float>(bitmap_h) - y,
                                 match.h * (1.0f + 2.0f * margin));
        if (w <= 0.0f || h <= 0.0f) return;

        lock_.rect = {static_cast<int>(x), static_cast<int>(y),
                      std::max(1, static_cast<int>(w)), std::max(1, static_cast<int>(h))};
        lock_.valid = true;
        lock_.misses = 0;
        out = {x, y, w, h};
        log::Debug("OCR: contador localizado em (" + std::to_string(lock_.rect.x) + "," +
                   std::to_string(lock_.rect.y) + ") " + std::to_string(lock_.rect.w) + "x" +
                   std::to_string(lock_.rect.h) + " na faixa");
    }

    void UpdateCounterLock(const RecognizeOutput& output, const OcrSample& sample) {
        if (!lock_.valid) return;
        if (output.has_rect) {
            // Recalibra com o retângulo do recorte travado, convertido de volta
            // para coordenadas da faixa.
            OcrMatchRect dummy;
            SetLockFromMatch(output.rect, sample.width, sample.height, dummy);
            return;
        }
        if (++lock_.misses >= config_.lock_miss_samples) {
            lock_ = CounterLock{};
            log::Debug("OCR: lock do contador perdido; voltando para a faixa");
        }
    }
#endif

    OcrRoundDetectorConfig config_;
    std::mutex pending_mutex_;
    std::vector<DetectedRound> pending_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::deque<OcrSample> queue_;
    std::atomic<bool> stop_{false};
    std::atomic<std::uint64_t> dropped_samples_{0};
    std::thread worker_;

    std::int64_t last_queued_100ns_ = 0;
    std::string candidate_label_;
    std::int64_t candidate_first_100ns_ = 0;
    int candidate_hits_ = 0;
    std::string last_emitted_label_;
    std::int64_t last_emission_100ns_ = 0;
    std::string last_raw_;
    int sample_count_ = 0;
    CounterLock lock_;
    std::vector<OcrSignatureSample> history_;
    bool warned_no_engine_ = false;
};

OcrRoundDetector::OcrRoundDetector(OcrRoundDetectorConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

OcrRoundDetector::~OcrRoundDetector() = default;

void OcrRoundDetector::Reset() { impl_->Reset(); }

void OcrRoundDetector::ProcessFrame(const FrameView& frame) { impl_->ProcessFrame(frame); }

void OcrRoundDetector::Flush() { impl_->Flush(); }

std::vector<DetectedRound> OcrRoundDetector::DrainDetections() { return impl_->DrainDetections(); }

} // namespace tft
