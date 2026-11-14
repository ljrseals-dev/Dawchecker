#include "core/ForensicHash.h"

#include <memory>
#include <stdexcept>

#include <openssl/evp.h>

namespace dawchecker::forensic {

std::vector<std::uint8_t> sha256(const void* data, std::size_t len) {
    if (data == nullptr && len != 0) {
        throw std::invalid_argument("SHA-256 input pointer is null");
    }

    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>
        ctx(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
    if (!ctx) {
        throw std::runtime_error("EVP_MD_CTX_new failed");
    }

    if (EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1) {
        throw std::runtime_error("EVP_DigestInit_ex failed");
    }
    if (len != 0 && EVP_DigestUpdate(ctx.get(), data, len) != 1) {
        throw std::runtime_error("EVP_DigestUpdate failed");
    }

    std::vector<std::uint8_t> digest(EVP_MAX_MD_SIZE);
    unsigned int out_len = 0;
    if (EVP_DigestFinal_ex(ctx.get(), digest.data(), &out_len) != 1) {
        throw std::runtime_error("EVP_DigestFinal_ex failed");
    }
    if (out_len != 32) {
        throw std::runtime_error("SHA-256 digest length is not 32 bytes");
    }

    digest.resize(32);
    return digest;
}

std::vector<std::uint8_t> sha256(const std::vector<std::uint8_t>& input) {
    return sha256(input.data(), input.size());
}

} // namespace dawchecker::forensic
