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

#include "rounds/OcrRoundText.h"
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

    void OnRecognition(const std::string& label, std::int64_t timestamp_100ns) {
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
        Publish(candidate_label_, candidate_first_100ns_);
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
            std::string label;
            std::string raw;
            try {
                Recognize(engine, sample, label, raw);
            } catch (...) {
                label.clear();
            }
            const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started).count();

            if (!raw.empty() && raw != last_raw_) {
                log::Debug("OCR bruto: " + raw + " -> " + (label.empty() ? "(sem contador)" : label));
                last_raw_ = raw;
            }
            if (++sample_count_ % 20 == 0) {
                std::size_t depth = 0;
                {
                    std::lock_guard<std::mutex> lock(queue_mutex_);
                    depth = queue_.size();
                }
                log::Debug("OCR: " + std::to_string(elapsed_ms) + " ms, fila=" +
                           std::to_string(depth) + ", descartadas=" +
                           std::to_string(dropped_samples_.load()));
            }
            OnRecognition(label, sample.timestamp_100ns);
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
    static void Recognize(const WOC::OcrEngine& engine,
                          const OcrSample& sample,
                          std::string& label,
                          std::string& raw) {
        winrt::array_view<const std::uint8_t> bytes(sample.pixels.data(), sample.pixels.size());
        const auto buffer = winrt::Windows::Security::Cryptography::CryptographicBuffer::
            CreateFromByteArray(bytes);
        const auto bitmap = WGI::SoftwareBitmap::CreateCopyFromBuffer(
            buffer, WGI::BitmapPixelFormat::Bgra8, sample.width, sample.height,
            WGI::BitmapAlphaMode::Straight);

        const auto result = engine.RecognizeAsync(bitmap).get();
        std::string joined;
        std::string loose;
        bool exact_found = false;
        for (const auto& line : result.Lines()) {
            const auto text = winrt::to_string(line.Text());
            if (text.empty()) continue;
            if (!joined.empty()) joined += " | ";
            joined += text;

            std::string candidate;
            if (!exact_found && ParseExactRoundLabel(text, candidate)) {
                label = std::move(candidate);
                exact_found = true;
            } else if (loose.empty() && TryParseRoundLabel(text, candidate)) {
                loose = std::move(candidate);
            }
        }
        if (!exact_found) label = std::move(loose);
        raw = std::move(joined);
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
