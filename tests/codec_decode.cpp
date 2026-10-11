#include "s2_codec.h"
#include "s2_log.h"
#include "s2_voice.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

static void require(bool ok, const char * message) {
    if (!ok) throw std::runtime_error(message);
}

int main(int argc, char ** argv) {
    if (argc < 3) {
        std::cerr << "Usage: s2-codec-decode-test MODEL.gguf VOICE.s2voice\n";
        return 1;
    }
    try {
        s2::set_log_level(s2::LogLevel::Warning);
        s2::init_backend_logging();
        const auto voice = s2::VoiceProfile::load(argv[2]);
        s2::AudioCodec codec;
        // Only codec computation runs here; the serving GPU is left alone.
        require(codec.load(argv[1]), "codec load failed");
        require(voice.num_codebooks == codec.num_codebooks() &&
                voice.sample_rate == codec.sample_rate(), "incompatible voice profile");
        constexpr int prefix_frames = 64;
        constexpr int full_frames = 96;
        require(voice.T_prompt >= full_frames, "voice needs at least 96 frames");
        auto codes_for = [&](int frames) {
            std::vector<int32_t> codes(static_cast<size_t>(frames) * voice.num_codebooks);
            for (int cb = 0; cb < voice.num_codebooks; ++cb) {
                std::copy_n(voice.codes.data() + static_cast<size_t>(cb) * voice.T_prompt,
                            frames, codes.data() + static_cast<size_t>(cb) * frames);
            }
            return codes;
        };
        const auto prefix_codes = codes_for(prefix_frames);
        const auto full_codes = codes_for(full_frames);
        std::vector<float> prefix, full;
        require(codec.decode(prefix_codes.data(), prefix_frames, 4, prefix), "prefix decode failed");
        require(codec.decode(full_codes.data(), full_frames, 4, full), "full decode failed");
        const size_t samples_per_frame = codec.samples_per_code_frame();
        require(prefix.size() == prefix_frames * samples_per_frame &&
                full.size() == full_frames * samples_per_frame, "unexpected sample count");
        double squared_error = 0.0, squared_signal = 0.0;
        float max_error = 0.0f;
        for (size_t i = 0; i < prefix.size(); ++i) {
            require(std::isfinite(prefix[i]) && std::isfinite(full[i]), "non-finite PCM");
            const double error = prefix[i] - full[i];
            squared_error += error * error;
            squared_signal += static_cast<double>(full[i]) * full[i];
            max_error = std::max(max_error, static_cast<float>(std::abs(error)));
        }
        const double relative_l2 = std::sqrt(squared_error / std::max(1e-12, squared_signal));
        std::cout << "Causal codec prefix: max_abs_error=" << max_error
                  << ", relative_l2=" << relative_l2 << '\n';
        require(max_error < 1e-3f && relative_l2 < 1e-3, "future codes changed prefix PCM");
        std::cout << "PASS\n";
        return 0;
    } catch (const std::exception & e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
