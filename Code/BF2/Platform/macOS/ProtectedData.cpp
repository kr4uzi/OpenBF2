// [CHOSEN, not recovered] macOS port: see ProtectedData.h.
#include "ProtectedData.h"

#if defined(__APPLE__) && !(defined(DEDICATED) && DEDICATED)

#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonHMAC.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace dice {
namespace hfe {
namespace macos {

namespace {

const unsigned char kMagic[4] = { 'O', 'B', 'F', '2' };
const unsigned char kVersion = 1;
const std::size_t kKeyBytes = 32;               // AES-256 key, then the HMAC-SHA256 key
const std::size_t kIvBytes = kCCBlockSizeAES128;
const std::size_t kMacBytes = CC_SHA256_DIGEST_LENGTH;
const std::size_t kHeaderBytes = sizeof(kMagic) + 1 + kIvBytes;

std::string homeDirectory()
{
    if (const char* home = getenv("HOME"))
        if (*home)
            return home;
    if (const passwd* pw = getpwuid(getuid()))
        if (pw->pw_dir)
            return pw->pw_dir;
    return std::string();
}

// Creates every missing directory of path with mode 0700; existing ones are left as they are.
void makeDirectories(const std::string& path)
{
    for (std::string::size_type at = 1; at <= path.size(); ++at)
        if (at == path.size() || path[at] == '/')
            mkdir(path.substr(0, at).c_str(), 0700);
}

bool readAll(int fd, unsigned char* out, std::size_t size)
{
    std::size_t done = 0;
    while (done < size) {
        const ssize_t n = read(fd, out + done, size - done);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        done += static_cast<std::size_t>(n);
    }
    return true;
}

bool writeAll(int fd, const unsigned char* data, std::size_t size)
{
    std::size_t done = 0;
    while (done < size) {
        const ssize_t n = write(fd, data + done, size - done);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        done += static_cast<std::size_t>(n);
    }
    return true;
}

// Reads the 64 key bytes; creates the file (and its directory) first when create is set and it
// does not exist. Only a regular file of exactly 64 bytes is accepted.
bool loadKeys(const std::string& path, bool create, unsigned char keys[2 * kKeyBytes])
{
    if (path.empty())
        return false;
    int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW);
    if (fd < 0 && errno == ENOENT && create) {
        const std::string::size_type slash = path.rfind('/');
        if (slash != std::string::npos && slash > 0)
            makeDirectories(path.substr(0, slash));
        unsigned char fresh[2 * kKeyBytes];
        arc4random_buf(fresh, sizeof(fresh));
        const int wfd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
        if (wfd >= 0) {
            const bool ok = writeAll(wfd, fresh, sizeof(fresh)) && fsync(wfd) == 0;
            close(wfd);
            memset(fresh, 0, sizeof(fresh));
            if (!ok) {
                unlink(path.c_str());
                return false;
            }
        }
        // Either this call created it or another process won the race; read what is there.
        fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW);
    }
    if (fd < 0)
        return false;
    struct stat st;
    const bool ok = fstat(fd, &st) == 0 && S_ISREG(st.st_mode)
                    && st.st_size == static_cast<off_t>(2 * kKeyBytes)
                    && readAll(fd, keys, 2 * kKeyBytes);
    close(fd);
    return ok;
}

void mac(const unsigned char* macKey, const unsigned char* data, std::size_t size,
         unsigned char out[kMacBytes])
{
    CCHmac(kCCHmacAlgSHA256, macKey, kKeyBytes, data, size, out);
}

} // namespace

std::string defaultProtectedDataKeyPath()
{
    const std::string home = homeDirectory();
    if (home.empty())
        return std::string();
    return home + "/Library/Application Support/OpenBF2/ProtectedData.key";
}

bool protectDataWithKeyFile(const std::string& keyPath, const unsigned char* data, std::size_t size,
                            DATA_BLOB* out)
{
    unsigned char keys[2 * kKeyBytes];
    if (!out || !loadKeys(keyPath, true, keys))
        return false;
    std::vector<unsigned char> blob(kHeaderBytes + size + kCCBlockSizeAES128 + kMacBytes);
    memcpy(&blob[0], kMagic, sizeof(kMagic));
    blob[sizeof(kMagic)] = kVersion;
    unsigned char* iv = &blob[sizeof(kMagic) + 1];
    arc4random_buf(iv, kIvBytes);
    std::size_t cipherBytes = 0;
    const CCCryptorStatus status = CCCrypt(kCCEncrypt, kCCAlgorithmAES, kCCOptionPKCS7Padding,
                                           keys, kKeyBytes, iv, data, size,
                                           &blob[kHeaderBytes], size + kCCBlockSizeAES128,
                                           &cipherBytes);
    if (status != kCCSuccess) {
        memset(keys, 0, sizeof(keys));
        return false;
    }
    const std::size_t macAt = kHeaderBytes + cipherBytes;
    mac(keys + kKeyBytes, &blob[0], macAt, &blob[macAt]);
    memset(keys, 0, sizeof(keys));
    const std::size_t total = macAt + kMacBytes;
    unsigned char* result = static_cast<unsigned char*>(malloc(total));
    if (!result)
        return false;
    memcpy(result, &blob[0], total);
    out->cbData = static_cast<std::uint32_t>(total);
    out->pbData = result;
    return true;
}

bool unprotectDataWithKeyFile(const std::string& keyPath, const unsigned char* data,
                              std::size_t size, DATA_BLOB* out)
{
    if (!out || !data || size < kHeaderBytes + kCCBlockSizeAES128 + kMacBytes
        || memcmp(data, kMagic, sizeof(kMagic)) != 0 || data[sizeof(kMagic)] != kVersion
        || (size - kHeaderBytes - kMacBytes) % kCCBlockSizeAES128 != 0)
        return false;
    unsigned char keys[2 * kKeyBytes];
    if (!loadKeys(keyPath, false, keys))
        return false;
    const std::size_t macAt = size - kMacBytes;
    unsigned char expected[kMacBytes];
    mac(keys + kKeyBytes, data, macAt, expected);
    unsigned char diff = 0;
    for (std::size_t i = 0; i < kMacBytes; ++i)
        diff |= static_cast<unsigned char>(expected[i] ^ data[macAt + i]);
    if (diff) {
        memset(keys, 0, sizeof(keys));
        return false;
    }
    const std::size_t cipherBytes = macAt - kHeaderBytes;
    unsigned char* plain = static_cast<unsigned char*>(malloc(cipherBytes ? cipherBytes : 1));
    if (!plain) {
        memset(keys, 0, sizeof(keys));
        return false;
    }
    std::size_t plainBytes = 0;
    const CCCryptorStatus status = CCCrypt(kCCDecrypt, kCCAlgorithmAES, kCCOptionPKCS7Padding,
                                           keys, kKeyBytes, data + sizeof(kMagic) + 1,
                                           data + kHeaderBytes, cipherBytes, plain, cipherBytes,
                                           &plainBytes);
    memset(keys, 0, sizeof(keys));
    if (status != kCCSuccess) {
        free(plain);
        return false;
    }
    out->cbData = static_cast<std::uint32_t>(plainBytes);
    out->pbData = plain;
    return true;
}

bool protectData(const unsigned char* data, std::size_t size, DATA_BLOB* out)
{
    return protectDataWithKeyFile(defaultProtectedDataKeyPath(), data, size, out);
}

bool unprotectData(const unsigned char* data, std::size_t size, DATA_BLOB* out)
{
    return unprotectDataWithKeyFile(defaultProtectedDataKeyPath(), data, size, out);
}

void freeProtectedData(void* data)
{
    free(data);
}

} // namespace macos
} // namespace hfe
} // namespace dice

#endif // __APPLE__ && !DEDICATED
