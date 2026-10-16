#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "clean_cosaves.h"

static int g_passedTests = 0;
static int g_failedTests = 0;

#define TEST_ASSERT(cond, msg)                                                \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << "[FAIL] " << __FUNCTION__ << ":" << __LINE__         \
                      << " -> " << msg << std::endl;                         \
            g_failedTests++;                                                  \
            return;                                                           \
        }                                                                     \
    } while (0)

#define TEST_PASS()                                                           \
    do {                                                                      \
        std::cout << "[PASS] " << __FUNCTION__ << std::endl;                  \
        g_passedTests++;                                                      \
    } while (0)

void test_has_skse_extension()
{
    // Valid cases
    TEST_ASSERT(Util::CoSaves::HasSKSEExtension("save.skse"), "Expected .skse to match");
    TEST_ASSERT(Util::CoSaves::HasSKSEExtension("SAVE.SKSE"), "Expected .SKSE to match uppercase");
    TEST_ASSERT(Util::CoSaves::HasSKSEExtension("Save_Character_1.SkSe"), "Expected mixed case .SkSe to match");
    TEST_ASSERT(Util::CoSaves::HasSKSEExtension("/path/to/Save 1.skse"), "Expected path with .skse to match");

    // Invalid cases
    TEST_ASSERT(!Util::CoSaves::HasSKSEExtension("save.ess"), "Expected .ess not to match");
    TEST_ASSERT(!Util::CoSaves::HasSKSEExtension("save.skse.bak"), "Expected .skse.bak not to match");
    TEST_ASSERT(!Util::CoSaves::HasSKSEExtension("save.sks"), "Expected .sks not to match");
    TEST_ASSERT(!Util::CoSaves::HasSKSEExtension("save.sksee"), "Expected .sksee not to match");
    TEST_ASSERT(!Util::CoSaves::HasSKSEExtension("save"), "Expected extensionless file not to match");
    TEST_ASSERT(!Util::CoSaves::HasSKSEExtension(""), "Expected empty path not to match");

    TEST_PASS();
}

void test_esl_formid_calculation()
{
    // ESL FormIDs have high byte 0xFE, middle 12 bits for small file index, lower 12 bits for local record ID
    const std::uint32_t baseId = 0x0012384A;
    const std::uint32_t expectedLocalId = baseId & 0x00000FFF; // 0x84A

    // File index 0
    std::uint32_t eslFormId0 = 0xFE000000 | (0 << 12) | expectedLocalId;
    TEST_ASSERT(eslFormId0 == 0xFE00084A, "Expected ESL FormID for index 0 to be 0xFE00084A");

    // File index 5
    std::uint32_t eslFormId5 = 0xFE000000 | (5 << 12) | expectedLocalId;
    TEST_ASSERT(eslFormId5 == 0xFE00584A, "Expected ESL FormID for index 5 to be 0xFE00584A");

    // File index 0xFFF (4095, maximum ESL index in Skyrim 1.7.104)
    std::uint32_t eslFormIdMax = 0xFE000000 | (0xFFF << 12) | expectedLocalId;
    TEST_ASSERT(eslFormIdMax == 0xFEFFF84A, "Expected ESL FormID for index 4095 to be 0xFEFFF84A");

    TEST_PASS();
}

void test_hex_formatting()
{
    char localFormIDHex[9] = { 0 };
    const std::uint32_t localFormID = 0x000C5BFB;

    snprintf(localFormIDHex, sizeof(localFormIDHex), "%08X", localFormID);
    TEST_ASSERT(std::string(localFormIDHex) == "000C5BFB", "Expected hex string to be 000C5BFB");

    const std::uint32_t zeroId = 0;
    snprintf(localFormIDHex, sizeof(localFormIDHex), "%08X", zeroId);
    TEST_ASSERT(std::string(localFormIDHex) == "00000000", "Expected hex string to be 00000000");

    TEST_PASS();
}

void test_max_stdio_fallback_logic()
{
    // Simulate MaxStdIO logic with null get pointer
    auto get_mock_null = []() -> int (*)() { return nullptr; };
    const auto get = get_mock_null();
    const auto old = get ? get() : 512;
    TEST_ASSERT(old == 512, "Expected fallback value 512 when get pointer is null");

    // Simulate fallback from 8192 to 2048 when 8192 is rejected
    auto proc_mock = [](int limit) -> int {
        if (limit > 2048) return -1;
        return limit;
    };

    int result = proc_mock(8192);
    if (result == -1) {
        result = proc_mock(2048);
    }
    TEST_ASSERT(result == 2048, "Expected fallback to 2048 when 8192 fails");

    TEST_PASS();
}

void test_wine_memory_allocator_selection()
{
    // Case 1: Native Windows, DisableTBB = false -> Use TBB
    bool isWine = false;
    bool disableTBB = false;
    bool wineCompat = true;
    bool shouldUseCRT = disableTBB || (wineCompat && isWine);
    TEST_ASSERT(!shouldUseCRT, "Expected TBB on native Windows when disableTBB is false");

    // Case 2: Native Windows, DisableTBB = true -> Use CRT
    disableTBB = true;
    shouldUseCRT = disableTBB || (wineCompat && isWine);
    TEST_ASSERT(shouldUseCRT, "Expected CRT on native Windows when disableTBB is true");

    // Case 3: Wine / CrossOver, DisableTBB = false, wineCompat = true -> Use CRT automatically
    isWine = true;
    disableTBB = false;
    wineCompat = true;
    shouldUseCRT = disableTBB || (wineCompat && isWine);
    TEST_ASSERT(shouldUseCRT, "Expected CRT on Wine when wineCompat is enabled");

    // Case 4: Wine / CrossOver, wineCompat = false, disableTBB = false -> Keep user preference (TBB)
    wineCompat = false;
    shouldUseCRT = disableTBB || (wineCompat && isWine);
    TEST_ASSERT(!shouldUseCRT, "Expected TBB on Wine when wineCompat is explicitly disabled");

    TEST_PASS();
}

void test_wine_dialog_suppression()
{
    // Dialog should be suppressed if wineCompat == true AND isWine == true
    auto should_suppress_dialog = [](bool isWine, bool wineCompat) -> bool {
        return wineCompat && isWine;
    };

    TEST_ASSERT(!should_suppress_dialog(false, true), "Native Windows must NOT suppress modal dialogs");
    TEST_ASSERT(!should_suppress_dialog(false, false), "Native Windows without wineCompat must NOT suppress dialogs");
    TEST_ASSERT(should_suppress_dialog(true, true), "Wine/CrossOver with wineCompat MUST suppress modal dialogs");
    TEST_ASSERT(!should_suppress_dialog(true, false), "Wine with wineCompat=false allows dialogs if user requested");

    TEST_PASS();
}

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << "EngineFixes Unit Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    test_has_skse_extension();
    test_esl_formid_calculation();
    test_hex_formatting();
    test_max_stdio_fallback_logic();
    test_wine_memory_allocator_selection();
    test_wine_dialog_suppression();

    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Results: " << g_passedTests << " passed, "
              << g_failedTests << " failed." << std::endl;
    std::cout << "========================================" << std::endl;

    return (g_failedTests == 0) ? 0 : 1;
}
