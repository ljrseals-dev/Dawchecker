#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dawchecker::forensic {

// Returns a 32-byte SHA-256 digest. Throws std::invalid_argument for a null
// data pointer with non-zero length and std::runtime_error on EVP failures.
std::vector<std::uint8_t> sha256(const void* data, std::size_t len);
std::vector<std::uint8_t> sha256(const std::vector<std::uint8_t>& input);

} // namespace dawchecker::forensic
