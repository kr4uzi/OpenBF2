// [TEMP] [CHOSEN, not recovered] S2.10 port checks against the production engine objects.
// [RETIRE-WHEN: s2-10-port-checks] Retire when equivalent UTF-16 file, unaligned packet,
// Interlocked contention and Python pending-call checks run in the native platform suite.
#include "../../BF2/IO/File/File.h"
#include "../../BF2/IO/File/FileManager.h"
#include "../../BF2/IO/File/UnixFileSystem.h"
#include "../../BF2/IO/Localization/ExcelLexicon.h"
#include "../../BF2/IO/BitStream.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <codecvt>
#include <locale>
#include <memory>
#include <thread>
#include <vector>
#include <netinet/in.h>

extern "C" {
int Py_AddPendingCall(int (*)(void*), void*);
int Py_MakePendingCalls();
unsigned int gsiInterlockedIncrement(unsigned int*);
unsigned int gsiInterlockedDecrement(unsigned int*);
void NNProcessData(char*, int, sockaddr_in*);
}
using namespace dice::hfe;

static void check(bool value, const char* message)
{
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::abort();
    }
}

static void sample(const char* path, const std::u16string& units, bool oddTail = false)
{
    std::FILE* out = std::fopen(path, "wb");
    check(out != nullptr, "create generated UTF-16 sample");
    for (char16_t unit : units) {
        const unsigned char bytes[] = {static_cast<unsigned char>(unit), static_cast<unsigned char>(unit >> 8)};
        check(std::fwrite(bytes, 1, 2, out) == 2, "write UTF-16LE unit");
    }
    if (oddTail) std::fputc(0x41, out);
    check(std::fclose(out) == 0, "close sample");
}

static void utf16Files()
{
    io::FileManager manager;
    io::UnixFileSystem* disk = new io::UnixFileSystem;
    manager.setBaseSystem(disk);
    disk->release();
    check(io::fileManager == nullptr, "fixture requires no initialized engine filesystem");
    io::fileManager = &manager;
    sample("lexicon.utxt", u"\ufeffKEY\x1b\tEspa\u00f1ol \u6f22 \U0001f642\x1b\t\r\n"
                           u"key\x1b\tduplicate\x1b\t\nEMPTY\x1b\t\x1b\t\n"
                           u"TRIM annotation\x1b\ttrimmed\x1b\t\n");
    io::ExcelLexicon lexicon;
    check(lexicon.load("lexicon.utxt"), "load production UTF-16 lexicon");
    check(lexicon.getString("key") == L"Espa\u00f1ol \u6f22 \U0001f642", "BMP/surrogate conversion; first duplicate wins");
    check(lexicon.getString("empty") == L"empty", "empty value skipped");
    check(lexicon.getString("trim") == L"trimmed", "key annotation removed and next record rewound by two bytes");
    sample("delimited.utxt", std::u16string(u"A\0B\x1bZ", 5), true);
    {
        io::File file;
        check(file.open("delimited.utxt", io::FILEMODE_READ, static_cast<io::FileAccess>(0), 0), "open sample");
        std::wstring text;
        check(file.read(text, wchar_t(27)) && text == L"A", "embedded NUL truncation");
        check(file.getPosition() == 8, "delimiter consumed as one UTF-16 unit");
        check(file.read(text, wchar_t(27)) && text == L"Z", "EOF/odd-byte tail has Windows-equivalent result");
        check(file.getPosition() == 11, "odd tail consumed");
    }
    sample("full-buffer.utxt", std::u16string(4095, u'\u6f22') + u"\x1b");
    {
        io::File file;
        check(file.open("full-buffer.utxt", io::FILEMODE_READ, static_cast<io::FileAccess>(0), 0), "open buffer-edge sample");
        std::wstring text;
        check(file.read(text, wchar_t(27)) && text == std::wstring(4095, L'\u6f22'), "4095-unit result and terminator fit");
        check(file.getPosition() == 8192, "two-byte delimiter at buffer edge");
    }
    // Windows-equivalent reference: the explicit UTF-16 units above decode to the same
    // Unicode scalar sequence on POSIX; no Windows executable or retail file is used.
    io::fileManager = nullptr;
    std::puts("PASS UTF-16LE File/ExcelLexicon generated files");
}

// Optional cloned retail files and independently decoded UTF-8 key/value oracles.
// This is a filesystem boundary check, not a client startup or server test.
static void retailLexicons(int argc, char** argv)
{
    check((argc - 1) % 3 == 0, "retail arguments are file/key/value triples");
    io::FileManager manager;
    io::UnixFileSystem* disk = new io::UnixFileSystem;
    manager.setBaseSystem(disk);
    disk->release();
    io::fileManager = &manager;
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conversion;
    for (int i = 1; i < argc; i += 3) {
        io::ExcelLexicon lexicon;
        check(lexicon.load(argv[i]), argv[i]);
        check(lexicon.getString(argv[i + 1]) == conversion.from_bytes(argv[i + 2]), "retail lexicon matches independent UTF-16 decode");
        std::printf("PASS retail lexicon %s\n", argv[i]);
    }
    io::fileManager = nullptr;
}

static void bitsAndPackets()
{
    for (unsigned int offset = 0; offset < 8; ++offset) {
        for (unsigned int width = 1; width <= 32; ++width) {
            const unsigned int bytes = (offset + width + 7) / 8;
            // Odd base and exact-sized heap allocations expose accidental aligned word access
            // and speculative reads with ASan/UBSan when those instrumented builds are used.
            std::unique_ptr<unsigned char[]> storage(new unsigned char[bytes + 1]());
            unsigned char* data = storage.get() + 1;
            const unsigned int sourceBytes = (width + 7) / 8;
            std::unique_ptr<unsigned char[]> source(new unsigned char[sourceBytes]);
            for (unsigned int i = 0; i < sourceBytes; ++i) source[i] = static_cast<unsigned char>(0xa5 + i);
            io::BitStream stream(data, bytes);
            if (offset) check(stream.skipBits(offset), "write offset");
            check(stream.writeBits(source.get(), width), "write bits");
            for (unsigned int i = 0; i < width; ++i)
                check(((data[(i + offset) / 8] >> ((i + offset) % 8)) & 1)
                    == ((source[i / 8] >> (i % 8)) & 1), "golden LSB-first bytes");
            stream.setReadMode();
            if (offset) check(stream.skipBits(offset), "read offset");
            unsigned int result = 0;
            check(stream.readBits(&result, width), "read bits");
            unsigned int expected = 0;
            std::memcpy(&expected, source.get(), sourceBytes);
            if (width < 32) expected &= (1u << width) - 1u;
            check(result == expected, "unaligned round trip");
            check(!stream.readBits(&result, bytes * 8 + 1), "reject overrun");
        }
    }
    unsigned char data[4]{};
    io::BitStream stream(data, sizeof(data));
    unsigned int value = 0x55;
    check(stream.writeBits(&value, 8), "saved-position payload");
    io::BSPosition position{};
    check(stream.setWritePosition(position), "seek to patch");
    stream.resetWritePosition();
    check(stream.getBitPosition() == 8, "restore saved position without overlay");
    sockaddr_in peer{};
    for (int length = 0; length < 12; ++length) {
        std::unique_ptr<char[]> packet(new char[length]);
        NNProcessData(packet.get(), length, &peer);
    }
    char unknown[21] = {char(0xfd), char(0xfc), 0x1e, 0x66, 0x6a, char(0xb2), 2, 0x7f};
    NNProcessData(unknown, sizeof(unknown), &peer); // no matching negotiator: ignored without network activity
    std::puts("PASS BitStream golden bytes, odd base, exact buffers, saved position, short/unknown NAT packets");
}

static void atomics()
{
    std::int32_t word = 0;
    check(InterlockedIncrement(&word) == 1, "increment returns new");
    check(InterlockedDecrement(&word) == 0, "decrement returns new");
    check(InterlockedExchange(&word, 7) == 0, "exchange returns old");
    check(InterlockedExchangeAdd(&word, 3) == 7, "exchange-add returns old");
    check(InterlockedAdd(&word, -2) == 8, "add returns new");
    check(InterlockedCompareExchange(&word, 5, 9) == 8 && word == 8, "CAS failure returns actual");
    check(InterlockedCompareExchange(&word, 5, 8) == 8 && word == 5, "CAS success returns old");
    check(InterlockedExchange(&word, INT32_MAX) == 5 && InterlockedIncrement(&word) == INT32_MIN, "32-bit wrap");
    unsigned int sdkWord = 0;
    check(gsiInterlockedIncrement(&sdkWord) == 1 && gsiInterlockedDecrement(&sdkWord) == 0, "GameSpy new-value semantics");
    word = 0;
    std::vector<std::thread> workers;
    for (int i = 0; i < 4; ++i) workers.emplace_back([&] {
        for (int j = 0; j < 10000; ++j) {
            InterlockedIncrement(&word);
            gsiInterlockedIncrement(&sdkWord);
        }
    });
    for (auto& worker : workers) worker.join();
    check(word == 40000 && sdkWord == 40000, "contended increments");
    std::puts("PASS POSIX Interlocked semantics and four-thread contention");
}

static int pending(void* data) { ++*static_cast<int*>(data); return 0; }

static void pendingCalls()
{
    std::atomic<int> done{0};
    int count = 0; // callbacks run only on this thread
    std::vector<std::thread> workers;
    for (int i = 0; i < 4; ++i) workers.emplace_back([&] {
        for (int j = 0; j < 10000; ++j) {
            while (Py_AddPendingCall(pending, &count) != 0) std::this_thread::yield();
        }
        ++done;
    });
    while (done != 4) check(Py_MakePendingCalls() == 0, "pending callback drain");
    for (auto& worker : workers) worker.join();
    check(Py_MakePendingCalls() == 0 && count == 40000, "pending calls neither lost nor duplicated");
    std::puts("PASS Python pending-call publication: four producers, 40000 callbacks");
}

int main(int argc, char** argv)
{
    utf16Files();
    if (argc > 1) retailLexicons(argc, argv);
    bitsAndPackets();
    atomics();
    pendingCalls();
}
