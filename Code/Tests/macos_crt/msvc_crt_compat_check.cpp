// [TEMP] [CHOSEN, not recovered] Checks Core/MsvcCrtCompat.h against the documented MSVC 7.1
// semantics: _vsnwprintf return values and truncation, the wide %s/%S/%c/%C conversions,
// _swprintf, _snprintf_s/_TRUNCATE, sprintf_s, _time32 and _splitpath with both separators. [RETIRE-WHEN: native-crt-compat-tests]
// Fold into the tree's unit-test runner once the macOS client has one.
// Build: c++ -std=gnu++14 -o <out>/msvc_crt_compat_check msvc_crt_compat_check.cpp && run it.
#include "../../BF2/Core/MsvcCrtCompat.h"
#include <clocale>
#include <cstdio>
#include <cstring>
#include <cwchar>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

static int vsn(wchar_t* b, size_t n, const wchar_t* f, ...)
{
    va_list a; va_start(a, f); const int r = _vsnwprintf(b, n, f, a); va_end(a); return r;
}

int main()
{
    std::setlocale(LC_ALL, "en_US.UTF-8");
    wchar_t b[16];
    // Fits: terminated, length returned.
    std::wmemset(b, L'#', 16);
    CHECK(vsn(b, 8, L"%s-%d", L"ab", 7) == 4 && std::wcscmp(b, L"ab-7") == 0);
    // Exactly count: no terminator, count returned.
    std::wmemset(b, L'#', 16);
    CHECK(vsn(b, 4, L"%s", L"abcd") == 4 && std::wmemcmp(b, L"abcd#", 5) == 0);
    // Longer: count characters, no terminator, -1.
    std::wmemset(b, L'#', 16);
    CHECK(vsn(b, 3, L"%s", L"abcdef") == -1 && std::wmemcmp(b, L"abc#", 4) == 0);
    // MSVC wide-family conversions: %s/%c wide, %S/%C/%hs narrow, %ls/%ws wide, I64.
    std::wmemset(b, 0, 16);
    CHECK(vsn(b, 15, L"%S|%hs|%ws", "n1", "n2", L"w") == 7 && std::wcscmp(b, L"n1|n2|w") == 0);
    CHECK(vsn(b, 15, L"%c%C%lc", L'x', 'y', L'z') == 3 && std::wcscmp(b, L"xyz") == 0);
    CHECK(vsn(b, 15, L"%I64d %%", (long long) -5) == 4 && std::wcscmp(b, L"-5 %") == 0);
    CHECK(vsn(b, 15, L"%-4s|%5.1f", L"a", 2.25) == 10 && std::wcscmp(b, L"a   |  2.2") == 0);
    // _swprintf: unbounded, terminated.
    wchar_t big[80];
    CHECK(_swprintf(big, L"%s", L"WARNING: x") == 10 && std::wcscmp(big, L"WARNING: x") == 0);
    // _time32 stores what it returns.
    __time32_t t = 0;
    CHECK(_time32(&t) == t && t > 0);
    // _splitpath: drive, both separators, last dot after the last separator.
    char drive[3], dir[256], fname[256], ext[256];
    _splitpath("C:\\a/b\\name.tar.gz", drive, dir, fname, ext);
    CHECK(!std::strcmp(drive, "C:") && !std::strcmp(dir, "\\a/b\\") && !std::strcmp(fname, "name.tar") && !std::strcmp(ext, ".gz"));
    _splitpath("dir.d/file", drive, dir, fname, ext);
    CHECK(!std::strcmp(drive, "") && !std::strcmp(dir, "dir.d/") && !std::strcmp(fname, "file") && !std::strcmp(ext, ""));
    _splitpath("Fonts/arial.dif", 0, 0, fname, ext);
    CHECK(!std::strcmp(fname, "arial") && !std::strcmp(ext, ".dif"));
    _splitpath(".hidden", drive, dir, fname, ext);
    CHECK(!std::strcmp(fname, "") && !std::strcmp(ext, ".hidden"));
    // _snprintf_s with _TRUNCATE: terminated, -1 on truncation; sprintf_s when it fits.
    char n8[8];
    CHECK(_snprintf_s(n8, sizeof n8, _TRUNCATE, "%s", "abc") == 3 && !std::strcmp(n8, "abc"));
    CHECK(_snprintf_s(n8, sizeof n8, _TRUNCATE, "%s", "abcdefghij") == -1 && !std::strcmp(n8, "abcdefg"));
    CHECK(_snprintf_s(n8, sizeof n8, 2, "%s", "abc") == -1 && !std::strcmp(n8, "ab"));
    char num[32];
    CHECK(sprintf_s(num, "%d", -42) == 3 && !std::strcmp(num, "-42"));
    std::printf(failures ? "msvc_crt_compat_check: %d failure(s)\n" : "msvc_crt_compat_check: all passed\n", failures);
    return failures ? 1 : 0;
}
