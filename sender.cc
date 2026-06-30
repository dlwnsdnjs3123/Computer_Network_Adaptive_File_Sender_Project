#include "netsim.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <vector>

namespace {

constexpr int kMinPayload = 1;
constexpr int kMaxPayload = 65535;
constexpr int kHeaderBytes = 2;
constexpr int kCrcBytes = 4;
constexpr int kFrameOverheadBytes = kHeaderBytes + kCrcBytes;
constexpr int kRoundTripCostBytes = 250;
constexpr uint32_t kCrcPolynomial = 0x04C11DB7u;

uint32_t compute_crc32(const uint8_t *data, int length) {
    uint32_t crc = 0u;

    for (int i = 0; i < length; ++i) {
        const uint8_t byte = data[i];
        for (int bit = 7; bit >= 0; --bit) {
            const uint32_t next_bit = static_cast<uint32_t>((byte >> bit) & 1u);
            const uint32_t high_bit = crc >> 31;
            crc = (crc << 1) | next_bit;
            if (high_bit != 0u) {
                crc ^= kCrcPolynomial;
            }
        }
    }

    for (int i = 0; i < 32; ++i) {
        const uint32_t high_bit = crc >> 31;
        crc <<= 1;
        if (high_bit != 0u) {
            crc ^= kCrcPolynomial;
        }
    }

    return crc;
}

bool open_input_file(const char *path, std::ifstream *input, size_t *total_size) {
    input->open(path, std::ios::binary);
    if (!*input) {
        std::fprintf(stderr, "sender: failed to open input file: %s\n", path);
        return false;
    }

    input->seekg(0, std::ios::end);
    const std::streamoff file_size = input->tellg();
    if (file_size < 0) {
        std::fprintf(stderr, "sender: failed to determine file size: %s\n", path);
        return false;
    }

    input->seekg(0, std::ios::beg);
    *total_size = static_cast<size_t>(file_size);
    return true;
}

bool fill_pending_bytes(std::ifstream *input, std::vector<uint8_t> *pending_bytes,
                        size_t required_size) {
    while (pending_bytes->size() < required_size) {
        const size_t old_size = pending_bytes->size();
        const size_t bytes_needed = required_size - old_size;

        pending_bytes->resize(required_size);
        input->read(reinterpret_cast<char *>(pending_bytes->data() + old_size),
                    static_cast<std::streamsize>(bytes_needed));

        if (input->gcount() != static_cast<std::streamsize>(bytes_needed)) {
            std::fprintf(stderr, "sender: failed to stage input bytes for transmission\n");
            return false;
        }
    }

    return true;
}

long double log1mexp(long double x) {
    const long double log_half = -0.693147180559945309417232121458176568L;
    if (x >= 0.0L) {
        return -std::numeric_limits<long double>::infinity();
    }
    if (x < log_half) {
        return std::log1p(-std::exp(x));
    }
    return std::log(-std::expm1(x));
}

std::vector<int> build_payload_candidates() {
    std::vector<int> sizes;
    sizes.push_back(92);
    sizes.push_back(103);
    sizes.push_back(115);
    sizes.push_back(128);
    sizes.push_back(143);
    sizes.push_back(158);
    sizes.push_back(176);
    sizes.push_back(194);
    sizes.push_back(215);
    sizes.push_back(224);
    sizes.push_back(237);
    sizes.push_back(261);
    sizes.push_back(287);
    sizes.push_back(315);
    sizes.push_back(345);
    sizes.push_back(349);
    sizes.push_back(378);
    sizes.push_back(414);
    sizes.push_back(452);
    sizes.push_back(493);
    sizes.push_back(538);
    sizes.push_back(586);
    sizes.push_back(637);
    sizes.push_back(682);
    sizes.push_back(693);
    sizes.push_back(752);
    sizes.push_back(817);
    sizes.push_back(886);
    sizes.push_back(961);
    sizes.push_back(1041);
    sizes.push_back(1066);
    sizes.push_back(1127);
    sizes.push_back(1220);
    sizes.push_back(1319);
    sizes.push_back(1426);
    sizes.push_back(1542);
    sizes.push_back(1665);
    sizes.push_back(1799);
    sizes.push_back(1942);
    sizes.push_back(2096);
    sizes.push_back(2261);
    sizes.push_back(2439);
    sizes.push_back(2602);
    sizes.push_back(2630);
    sizes.push_back(2835);
    sizes.push_back(3056);
    sizes.push_back(3293);
    sizes.push_back(3548);
    sizes.push_back(3822);
    sizes.push_back(4066);
    sizes.push_back(4116);
    sizes.push_back(4432);
    sizes.push_back(4772);
    sizes.push_back(5138);
    sizes.push_back(5530);
    sizes.push_back(5952);
    sizes.push_back(6406);
    sizes.push_back(6893);
    sizes.push_back(7417);
    sizes.push_back(7941);
    sizes.push_back(7979);
    sizes.push_back(8584);
    sizes.push_back(9234);
    sizes.push_back(9932);
    sizes.push_back(10683);
    sizes.push_back(11489);
    sizes.push_back(12356);
    sizes.push_back(13287);
    sizes.push_back(14288);
    sizes.push_back(15363);
    sizes.push_back(16519);
    sizes.push_back(17761);
    return sizes;
}

std::vector<int> build_bootstrap_sizes() {
    std::vector<int> sizes;
    sizes.push_back(452);
    sizes.push_back(1665);
    sizes.push_back(3056);
    sizes.push_back(4772);
    return sizes;
}

class AdaptivePayloadChooser {
  public:
    AdaptivePayloadChooser()
        : payload_ladder_(build_payload_candidates()),
          bootstrap_sizes_(build_bootstrap_sizes()),
          current_index_(nearest_index(452)),
          bootstrap_index_(0),
          in_bootstrap_(true) {
        const int grid_points = 65;
        posterior_log_weights_.assign(grid_points, 0.0L);
        log_success_per_bit_.reserve(grid_points);

        for (int i = 0; i < grid_points; ++i) {
            const long double exponent =
                -7.0L + (4.0L * static_cast<long double>(i)) /
                            static_cast<long double>(grid_points - 1);
            const long double ber = std::pow(10.0L, exponent);
            log_success_per_bit_.push_back(std::log1p(-ber));
        }

        normalize_weights();
    }

    int choose_payload(size_t remaining_bytes) const {
        if (in_bootstrap_) {
            const size_t target =
                static_cast<size_t>(bootstrap_sizes_[bootstrap_index_]);
            return static_cast<int>(std::min(target, remaining_bytes));
        }
        const size_t target = static_cast<size_t>(payload_ladder_[current_index_]);
        return static_cast<int>(std::min(target, remaining_bytes));
    }

    void observe_result(int payload_size, int result_code) {
        const bool ack = (result_code == NETSIM_ACK);
        update_posterior(payload_size, ack);

        const int desired_index = best_index_from_posterior();
        const int sent_index = floor_index_for_payload(payload_size);

        if (in_bootstrap_) {
            if (ack) {
                if (bootstrap_index_ + 1 < static_cast<int>(bootstrap_sizes_.size())) {
                    ++bootstrap_index_;
                    current_index_ =
                        floor_index_for_payload(bootstrap_sizes_[bootstrap_index_]);
                    return;
                }

                in_bootstrap_ = false;
                current_index_ = std::max(desired_index, sent_index);
                return;
            }

            in_bootstrap_ = false;
            current_index_ = std::min(desired_index, sent_index);
            return;
        }

        if (!ack) {
            current_index_ = std::min(desired_index, sent_index);
            return;
        }

        current_index_ = desired_index;
    }

  private:
    int nearest_index(int payload_size) const {
        int best_index = 0;
        int best_distance = std::abs(payload_ladder_[0] - payload_size);
        for (size_t i = 1; i < payload_ladder_.size(); ++i) {
            const int distance = std::abs(payload_ladder_[i] - payload_size);
            if (distance < best_distance) {
                best_distance = distance;
                best_index = static_cast<int>(i);
            }
        }
        return best_index;
    }

    int floor_index_for_payload(int payload_size) const {
        const auto it =
            std::upper_bound(payload_ladder_.begin(), payload_ladder_.end(), payload_size);
        if (it == payload_ladder_.begin()) {
            return 0;
        }
        return static_cast<int>((it - payload_ladder_.begin()) - 1);
    }

    void normalize_weights() {
        const long double max_log_weight = *std::max_element(
            posterior_log_weights_.begin(), posterior_log_weights_.end());

        posterior_weights_.assign(posterior_log_weights_.size(), 0.0L);
        long double total = 0.0L;
        for (size_t i = 0; i < posterior_log_weights_.size(); ++i) {
            const long double weight = std::exp(posterior_log_weights_[i] - max_log_weight);
            posterior_weights_[i] = weight;
            total += weight;
        }

        if (total == 0.0L) {
            const long double uniform =
                1.0L / static_cast<long double>(posterior_weights_.size());
            for (size_t i = 0; i < posterior_weights_.size(); ++i) {
                posterior_weights_[i] = uniform;
                posterior_log_weights_[i] = 0.0L;
            }
            return;
        }

        for (size_t i = 0; i < posterior_weights_.size(); ++i) {
            posterior_weights_[i] /= total;
            posterior_log_weights_[i] -= max_log_weight;
        }
    }

    void update_posterior(int payload_size, bool ack) {
        const long double protected_bits =
            8.0L * static_cast<long double>(payload_size + kCrcBytes);

        for (size_t i = 0; i < posterior_log_weights_.size(); ++i) {
            const long double log_success = protected_bits * log_success_per_bit_[i];

            if (ack) {
                posterior_log_weights_[i] += log_success;
            } else {
                posterior_log_weights_[i] += log1mexp(log_success);
            }
        }

        normalize_weights();
    }

    long double expected_cost_per_byte_with_weights(
        int payload_size, const std::vector<long double> &weights) const {
        const long double payload = static_cast<long double>(payload_size);
        const long double base_cost =
            (payload + static_cast<long double>(kFrameOverheadBytes) +
             static_cast<long double>(kRoundTripCostBytes)) /
            payload;

        const long double protected_bits =
            8.0L * static_cast<long double>(payload_size + kCrcBytes);

        long double expected = 0.0L;
        const long double huge = 1.0e100L;

        for (size_t i = 0; i < weights.size(); ++i) {
            const long double weight = weights[i];
            if (weight < 1.0e-18L) {
                continue;
            }

            const long double log_success = protected_bits * log_success_per_bit_[i];
            if (log_success < -80.0L) {
                return huge;
            }

            expected += weight * base_cost * std::exp(-log_success);
            if (expected > huge) {
                return huge;
            }
        }

        return expected;
    }

    long double expected_cost_per_byte(int payload_size) const {
        return expected_cost_per_byte_with_weights(payload_size, posterior_weights_);
    }

    int best_index_from_posterior() const {
        int best_index = 0;
        long double best_cost = std::numeric_limits<long double>::infinity();

        for (size_t i = 0; i < payload_ladder_.size(); ++i) {
            const long double cost = expected_cost_per_byte(payload_ladder_[i]);
            if (cost < best_cost) {
                best_cost = cost;
                best_index = static_cast<int>(i);
            }
        }

        return best_index;
    }

    std::vector<int> payload_ladder_;
    std::vector<int> bootstrap_sizes_;
    std::vector<long double> log_success_per_bit_;
    std::vector<long double> posterior_log_weights_;
    std::vector<long double> posterior_weights_;
    int current_index_;
    int bootstrap_index_;
    bool in_bootstrap_;
};

std::vector<uint8_t> build_frame(const std::vector<uint8_t> &pending_bytes,
                                 int payload_size) {
    std::vector<uint8_t> frame(static_cast<size_t>(kHeaderBytes + payload_size + kCrcBytes));

    frame[0] = static_cast<uint8_t>((payload_size >> 8) & 0xFF);
    frame[1] = static_cast<uint8_t>(payload_size & 0xFF);

    std::memcpy(frame.data() + kHeaderBytes, pending_bytes.data(),
                static_cast<size_t>(payload_size));

    const uint32_t crc = compute_crc32(frame.data(), kHeaderBytes + payload_size);

    const size_t crc_offset = static_cast<size_t>(kHeaderBytes + payload_size);
    frame[crc_offset + 0] = static_cast<uint8_t>((crc >> 24) & 0xFF);
    frame[crc_offset + 1] = static_cast<uint8_t>((crc >> 16) & 0xFF);
    frame[crc_offset + 2] = static_cast<uint8_t>((crc >> 8) & 0xFF);
    frame[crc_offset + 3] = static_cast<uint8_t>(crc & 0xFF);

    return frame;
}

void consume_acked_prefix(std::vector<uint8_t> *pending_bytes, size_t bytes_to_drop) {
    pending_bytes->erase(pending_bytes->begin(),
                         pending_bytes->begin() + static_cast<std::ptrdiff_t>(bytes_to_drop));
}

}  // namespace

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s input_file\n", argv[0]);
        return 1;
    }

    std::ifstream input;
    size_t total_bytes = 0;
    if (!open_input_file(argv[1], &input, &total_bytes)) {
        return 1;
    }

    if (total_bytes == 0) {
        return 0;
    }

    AdaptivePayloadChooser chooser;
    std::vector<uint8_t> pending_bytes;
    size_t acked_bytes = 0;

    while (acked_bytes < total_bytes) {
        const size_t remaining = total_bytes - acked_bytes;
        const int payload_size = chooser.choose_payload(remaining);
        if (payload_size < kMinPayload || payload_size > kMaxPayload) {
            std::fprintf(stderr, "sender: invalid payload size chosen: %d\n", payload_size);
            return 1;
        }

        if (!fill_pending_bytes(&input, &pending_bytes, static_cast<size_t>(payload_size))) {
            return 1;
        }

        const std::vector<uint8_t> frame = build_frame(pending_bytes, payload_size);
        const int result = send_frame(frame.data(), static_cast<int>(frame.size()));

        if (result == NETSIM_ERROR) {
            std::fprintf(stderr, "sender: send_frame() failed\n");
            return 1;
        }

        chooser.observe_result(payload_size, result);
        if (result == NETSIM_ACK) {
            acked_bytes += static_cast<size_t>(payload_size);
            consume_acked_prefix(&pending_bytes, static_cast<size_t>(payload_size));
        }
    }

    return 0;
}
