#include "s2_model.h"
#include "s2_log.h"
#include "s2_prompt.h"
#include "s2_tokenizer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

static void require(bool ok, const char * message) {
    if (!ok) throw std::runtime_error(message);
}

static float compare(const std::vector<float> & a, const std::vector<float> & b,
                     float tolerance, const char * message) {
    require(a.size() == b.size(), "output size mismatch");
    float max_error = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        require(std::isfinite(a[i]) && std::isfinite(b[i]), "non-finite output");
        max_error = std::max(max_error, std::abs(a[i] - b[i]));
    }
    std::cout << message << ": max_abs_error=" << max_error << '\n';
    require(max_error <= tolerance, message);
    return max_error;
}

static void compare_distribution(const std::vector<float> & reference, const std::vector<float> & cached,
                                 const char * label) {
    require(reference.size() == cached.size(), "output size mismatch");
    const float ref_max = *std::max_element(reference.begin(), reference.end());
    const float cached_max = *std::max_element(cached.begin(), cached.end());
    double ref_sum = 0.0, cached_sum = 0.0, squared_error = 0.0, squared_signal = 0.0;
    float max_error = 0.0f;
    for (size_t i = 0; i < reference.size(); ++i) {
        require(std::isfinite(reference[i]) && std::isfinite(cached[i]), "non-finite output");
        ref_sum += std::exp(static_cast<double>(reference[i] - ref_max));
        cached_sum += std::exp(static_cast<double>(cached[i] - cached_max));
        const double error = cached[i] - reference[i];
        squared_error += error * error;
        squared_signal += static_cast<double>(reference[i]) * reference[i];
        max_error = std::max(max_error, static_cast<float>(std::abs(error)));
    }
    double kl = 0.0;
    for (size_t i = 0; i < reference.size(); ++i) {
        const double log_p = reference[i] - ref_max - std::log(ref_sum);
        const double log_q = cached[i] - cached_max - std::log(cached_sum);
        kl += std::exp(log_p) * (log_p - log_q);
    }
    const double relative_error = std::sqrt(squared_error / std::max(1e-12, squared_signal));
    std::cout << label << ": max_abs_error=" << max_error
              << ", relative_l2=" << relative_error << ", KL=" << kl << '\n';
    // Quantized GEMV and GEMM use different accumulation paths. Check both
    // logit error and the sampling distribution, rather than bitwise equality.
    require(relative_error < 0.01 && kl < 0.002, "sampling distribution mismatch");
}

int main(int argc, char ** argv) {
    if (argc < 3) {
        std::cerr << "Usage: s2-model-decode-test MODEL.gguf tokenizer.json [cuda|cpu] [frames]\n";
        return 1;
    }
    try {
        const bool cuda = argc > 3 && std::string(argv[3]) == "cuda";
        const int frames = argc > 4 ? std::max(1, std::stoi(argv[4])) : 10;
        s2::set_log_level(s2::LogLevel::Warning);
        s2::init_backend_logging();
        s2::Tokenizer tokenizer;
        require(tokenizer.load(argv[2]), "tokenizer load failed");
        s2::SlowARModel model;
        require(model.load(argv[1], cuda ? 0 : -1,
            cuda ? s2::BackendType::CUDA : s2::BackendType::CPU), "model load failed");
        model.set_flash_attention(false);
        const auto & hp = model.hparams();
        const int32_t eos = tokenizer.config().im_end_id;
        auto prompt = s2::build_prompt(tokenizer, "Hello world.", "", nullptr, hp.num_codebooks, 0);
        std::vector<int32_t> tokens(prompt.data.size());
        for (int32_t t = 0; t < prompt.cols; ++t) {
            for (int32_t r = 0; r < prompt.rows; ++r) {
                tokens[static_cast<size_t>(t) * prompt.rows + r] = prompt.data[static_cast<size_t>(r) * prompt.cols + t];
            }
        }
        require(model.init_kv_cache(prompt.cols + 8), "KV allocation failed");
        s2::StepResult full, compact;
        require(model.prefill_fast(tokens, prompt.cols, 4, full), "full prefill failed");
        model.reset();
        require(model.prefill_fast(tokens, prompt.cols, 4, compact, eos), "compact prefill failed");
        compare(full.hidden, compact.hidden, 1e-5f, "prefill hidden");
        auto compare_head = [&]() {
            std::vector<float> a, b;
            for (int32_t i = 0; i < hp.vocab_size; ++i) {
                if ((i >= hp.semantic_begin_id && i <= hp.semantic_end_id) || i == eos) {
                    a.push_back(full.logits[i]);
                    b.push_back(compact.logits[i]);
                } else {
                    require(std::isinf(compact.logits[i]) && compact.logits[i] < 0, "unmasked text logit");
                }
            }
            compare(a, b, 1e-4f, "semantic/EOS head");
        };
        compare_head();

        // Replay identical semantic steps through full and restricted output heads.
        std::vector<s2::StepResult> reference_steps(3);
        std::vector<int32_t> step(hp.num_codebooks + 1, 0);
        model.reset();
        require(model.prefill_fast(tokens, prompt.cols, 4, full), "full replay prefill failed");
        for (int i = 0; i < 3; ++i) {
            step[0] = hp.semantic_begin_id + i;
            require(model.step(step, 4, reference_steps[i]), "full step failed");
        }
        model.reset();
        require(model.prefill_fast(tokens, prompt.cols, 4, compact, eos), "compact replay prefill failed");
        for (int i = 0; i < 3; ++i) {
            step[0] = hp.semantic_begin_id + i;
            require(model.step(step, 4, compact, eos), "compact step failed");
            full = reference_steps[i];
            compare(full.hidden, compact.hidden, 1e-5f, "step hidden");
            compare_head();
        }

        // Compare the GPU single-query fused attention path with the unfused
        // path, using the same prefill and input codes at each position.
        model.set_flash_attention(true);
        model.reset();
        require(model.prefill_fast(tokens, prompt.cols, 4, compact, eos), "flash prefill failed");
        for (int i = 0; i < 3; ++i) {
            step[0] = hp.semantic_begin_id + i;
            require(model.step(step, 4, compact, eos), "flash step failed");
            std::vector<float> reference_logits, flash_logits;
            for (int32_t id = hp.semantic_begin_id; id <= hp.semantic_end_id; ++id) {
                reference_logits.push_back(reference_steps[i].logits[id]);
                flash_logits.push_back(compact.logits[id]);
            }
            reference_logits.push_back(reference_steps[i].logits[eos]);
            flash_logits.push_back(compact.logits[eos]);
            compare_distribution(reference_logits, flash_logits, "Slow-AR flash logits");
        }

        const auto hidden = compact.hidden;
        std::vector<float> reference, cached;
        // All codebook positions, frame changes, and graph replay after warmup.
        for (int frame = 0; frame < 3; ++frame) {
            auto frame_hidden = hidden;
            frame_hidden[0] += frame * 0.01f;
            std::vector<int32_t> prefix = { frame };
            for (int cb = 1; cb < hp.num_codebooks; ++cb) {
                require(model.fast_decode(frame_hidden, prefix, 4, reference), "reference Fast-AR failed");
                require(model.fast_decode_cached(frame_hidden, prefix, 4, cached), "cached Fast-AR failed");
                compare_distribution(reference, cached, "Fast-AR logits");
                prefix.push_back(static_cast<int32_t>(std::max_element(reference.begin(), reference.end()) - reference.begin()));
            }
        }
        // Nonsequential prefixes and changed hidden state must use the reference path.
        for (auto prefix : { std::vector<int32_t>{}, std::vector<int32_t>{1, 2, 3}, std::vector<int32_t>{4, 5} }) {
            require(model.fast_decode(hidden, prefix, 4, reference), "fallback reference failed");
            require(model.fast_decode_cached(hidden, prefix, 4, cached), "fallback cached failed");
            compare(reference, cached, 1e-5f, "arbitrary prefix fallback");
        }
        require(!model.fast_decode_cached({}, {1}, 4, cached), "invalid hidden accepted");
        require(!model.fast_decode_cached(hidden, std::vector<int32_t>(hp.num_codebooks, 0), 4, cached), "invalid prefix accepted");

        auto benchmark = [&](bool use_cache) {
            auto start = std::chrono::steady_clock::now();
            for (int frame = 0; frame < frames; ++frame) {
                std::vector<int32_t> prefix = { frame % hp.codebook_size };
                for (int cb = 1; cb < hp.num_codebooks; ++cb) {
                    require(use_cache ? model.fast_decode_cached(hidden, prefix, 4, cached)
                                      : model.fast_decode(hidden, prefix, 4, cached), "benchmark failed");
                    prefix.push_back((cb * 17 + frame) % hp.codebook_size);
                }
            }
            return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / frames;
        };
        const double full_ms = benchmark(false);
        const double cached_ms = benchmark(true);
        std::cout << "Fast-AR per frame: full=" << full_ms << " ms, cached=" << cached_ms
                  << " ms, speedup=" << full_ms / cached_ms << "x\nPASS\n";
        model.clear_kv_cache();
        return 0;
    } catch (const std::exception & e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
