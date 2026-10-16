// ==WindhawkMod==
// @id              vietnamese-lunar-calendar-1
// @name            Vietnamese Lunar Calendar
// @description     Displays the Vietnamese Lunar Calendar - Public Holidays, and a week number badge in the bottom-right corner of the calendar header. Hides the notification popup.
// @version         1.0
// @author          BrewVN / Mr._VDung
// @homepage        https://tempmailmmo.com
// @github          https://github.com/BrewVN-lab
// @include         ShellExperienceHost.exe
// @include         ShellHost.exe
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -lole32 -loleaut32 -lruntimeobject
// @license         GPL-3.0-only
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Integrate the Vietnamese Lunar Calendar into Windows 11

**Key Features:**
1. Displays a large red clock at the top, the Gregorian date, a week number badge in the bottom-right corner of the calendar header, and the full Vietnamese lunar date.

2. Hides the top notification panel and the bottom Focus/Timer panel in the calendar flyout.

3. Highlights Vietnamese public holidays based on both the Gregorian and lunar calendars, with each holiday's name displayed beneath its corresponding date.

4. Displays a two-line clock on the Windows 11 taskbar and when hovering over the clock.

5. Automatically detects the active Taskbar mod state or allows you to manually select the display mode.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- showLunarDate: true
  $name: Show Vietnamese Lunar Calendar
  $description: Displays the Vietnamese lunar date in the calendar flyout and on the taskbar.

- taskbarMode: auto
  $name: Taskbar Clock Display Mode
  $description: Automatically detects the active taskbar mod state or lets you manually select the display mode. Changes take effect immediately when settings are saved.
  $options:
  - auto: Auto-detect (Recommended)
  - default: Default Taskbar (Separate Time and Date containers - standard auto-sizing)
  - modded: Taskbar Mod Active (Combines both lines into the Time container)

- hideNotificationCenter: true
  $name: Hide Notification Panel Above Calendar
  $description: Completely hides the notification panel above the calendar flyout.

- hideFocusPanel: true
  $name: Hide Bottom Focus/Timer Panel
  $description: Completely hides the Focus and Timer panel at the bottom of the calendar flyout.

- showHolidays: true
  $name: Highlight Vietnamese Holidays in Red
  $description: Highlights Vietnamese public holidays based on both the Gregorian and lunar calendars in red and displays the corresponding holiday names.

- highlightMung1Ram: false
  $name: Also Highlight the 1st and 15th Lunar Days
  $description: Highlights the 1st and 15th days of each lunar month in red.

- lunarFontSize: 8
  $name: Lunar Date Font Size
  $description: Smaller values help prevent text overlap in Windows' compact calendar grid.

- selectedDayCornerRadius: 19
  $name: Current Day Corner Radius
  $description: Set to 19 to create a perfect circular highlight (38x38) for the selected date.

- clockColorHex: "#FF3B30"
  $name: Header Clock Color (Hex)
  $description: Hex color code (red by default) for the HH:MM:SS clock displayed in the header.

- holidayColorHex: "#FF4D4F"
  $name: Holiday Highlight Color (Hex)
  $description: Hex color code (red by default) used to highlight holidays in the calendar grid.
*/
// ==/WindhawkModSettings==

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#undef GetCurrentTime

#include <Unknwn.h>
#include <winstring.h>
#include <tlhelp32.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Shapes.h>
#include <winrt/Windows.UI.Xaml.h>

#include <windhawk_utils.h>

namespace wf = winrt::Windows::Foundation;
namespace wux = winrt::Windows::UI::Xaml;
namespace wuxc = winrt::Windows::UI::Xaml::Controls;
namespace wuxh = winrt::Windows::UI::Xaml::Hosting;
namespace wuxi = winrt::Windows::UI::Xaml::Input;
namespace wuxm = winrt::Windows::UI::Xaml::Media;
namespace wuv = winrt::Windows::UI::ViewManagement;

thread_local bool g_initializedForThread = false;
std::atomic<uint64_t> g_settingsGeneration{1};

enum class TaskbarDisplayMode {
    Auto,
    DefaultTwoElements,
    ModdedSingleElement,
};

struct Settings {
    bool showLunarDate = true;
    TaskbarDisplayMode taskbarMode = TaskbarDisplayMode::Auto;
    bool hideNotificationCenter = true;
    bool hideFocusPanel = true;
    bool showHolidays = true;
    bool highlightMung1Ram = false;
    int lunarFontSize = 8;
    int selectedDayCornerRadius = 19;
    winrt::Windows::UI::Color clockColor{255, 255, 59, 48};    // #FF3B30
    winrt::Windows::UI::Color holidayColor{255, 255, 77, 79};  // #FF4D4F
};

std::mutex g_settingsMutex;
std::shared_ptr<const Settings> g_settings = std::make_shared<Settings>();

std::shared_ptr<const Settings> GetSettings() {
    std::lock_guard<std::mutex> lock(g_settingsMutex);
    return g_settings;
}

std::wstring ReadStringSetting(PCWSTR name) {
    const WindhawkUtils::StringSetting value =
        WindhawkUtils::StringSetting::make(name);
    return std::wstring(value.get());
}

std::optional<winrt::Windows::UI::Color> ParseHexColor(std::wstring const& text) {
    std::wstring value;
    for (wchar_t c : text) {
        if (!iswspace(c)) {
            value += c;
        }
    }
    if (!value.empty() && value[0] == L'#') {
        value = value.substr(1);
    }
    if (value.size() != 6 && value.size() != 8) {
        return std::nullopt;
    }

    auto hexDigit = [](wchar_t c) -> int {
        if (c >= L'0' && c <= L'9') return c - L'0';
        if (c >= L'a' && c <= L'f') return c - L'a' + 10;
        if (c >= L'A' && c <= L'F') return c - L'A' + 10;
        return -1;
    };

    uint8_t bytes[4] = {0xFF, 0x00, 0x00, 0x00};
    size_t offset = value.size() == 8 ? 0 : 1;
    for (size_t i = 0; i + 1 < value.size(); i += 2) {
        int hi = hexDigit(value[i]);
        int lo = hexDigit(value[i + 1]);
        if (hi < 0 || lo < 0) {
            return std::nullopt;
        }
        bytes[offset + i / 2] = static_cast<uint8_t>((hi << 4) | lo);
    }

    return winrt::Windows::UI::Color{bytes[0], bytes[1], bytes[2], bytes[3]};
}

std::wstring TrimWhitespace(std::wstring const& text) {
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && iswspace(text[begin])) {
        begin++;
    }
    while (end > begin && iswspace(text[end - 1])) {
        end--;
    }
    return text.substr(begin, end - begin);
}

void LoadSettings() {
    auto settings = std::make_shared<Settings>();

    settings->showLunarDate = Wh_GetIntSetting(L"showLunarDate") != 0;

    std::wstring tbMode = TrimWhitespace(ReadStringSetting(L"taskbarMode"));
    for (wchar_t& c : tbMode) {
        c = towlower(c);
    }
    if (tbMode == L"modded" || tbMode.find(L"mod") != std::wstring::npos) {
        settings->taskbarMode = TaskbarDisplayMode::ModdedSingleElement;
    } else if (tbMode == L"default" || tbMode.find(L"default") != std::wstring::npos) {
        settings->taskbarMode = TaskbarDisplayMode::DefaultTwoElements;
    } else {
        settings->taskbarMode = TaskbarDisplayMode::Auto;
    }

    settings->hideNotificationCenter = Wh_GetIntSetting(L"hideNotificationCenter") != 0;
    settings->hideFocusPanel = Wh_GetIntSetting(L"hideFocusPanel") != 0;
    settings->showHolidays = Wh_GetIntSetting(L"showHolidays") != 0;
    settings->highlightMung1Ram = Wh_GetIntSetting(L"highlightMung1Ram") != 0;
    settings->lunarFontSize = Wh_GetIntSetting(L"lunarFontSize");
    if (settings->lunarFontSize < 7 || settings->lunarFontSize > 14) {
        settings->lunarFontSize = Settings{}.lunarFontSize;
    }
    settings->selectedDayCornerRadius =
        Wh_GetIntSetting(L"selectedDayCornerRadius");
    if (settings->selectedDayCornerRadius < 0 ||
        settings->selectedDayCornerRadius > 48) {
        settings->selectedDayCornerRadius = Settings{}.selectedDayCornerRadius;
    }

    if (auto c = ParseHexColor(ReadStringSetting(L"clockColorHex"))) {
        settings->clockColor = *c;
    }
    if (auto c = ParseHexColor(ReadStringSetting(L"holidayColorHex"))) {
        settings->holidayColor = *c;
    }

    {
        std::lock_guard<std::mutex> lock(g_settingsMutex);
        g_settings = settings;
    }
    g_settingsGeneration.fetch_add(1);
}

int32_t MakeDateKey(int year, int month, int day) {
    return year * 10000 + month * 100 + day;
}

winrt::Windows::UI::Color GetTodayHighlightColor() {
    try {
        wuv::UISettings uiSettings;
        auto c = uiSettings.GetColorValue(wuv::UIColorType::AccentLight2);
        if (c.A > 0) {
            return c;
        }
    } catch (...) {
    }
    return winrt::Windows::UI::Color{255, 76, 194, 255};
}

constexpr double kPi = 3.14159265358979323846;
constexpr double kVietnamTimeZone = 7.0;

int IntFloor(double value) {
    return static_cast<int>(std::floor(value));
}

int JulianDayFromDate(int day, int month, int year) {
    int a = (14 - month) / 12;
    int y = year + 4800 - a;
    int m = month + 12 * a - 3;
    int jd = day + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;
    if (jd < 2299161) {
        jd = day + (153 * m + 2) / 5 + 365 * y + y / 4 - 32083;
    }
    return jd;
}

int GetIsoWeekOfYear(SYSTEMTIME const& st) {
    int jd = JulianDayFromDate(st.wDay, st.wMonth, st.wYear);
    int d4 = (jd + 31741 - (jd % 7)) % 146097 % 36524 % 1461;
    int L = d4 / 1460;
    int d1 = ((d4 - L) % 365) + L;
    int week = d1 / 7 + 1;
    return (week >= 1 && week <= 53) ? week : 1;
}

int NewMoonDay(int k, double timeZone) {
    double t = k / 1236.85;
    double t2 = t * t;
    double t3 = t2 * t;
    double dr = kPi / 180.0;
    double jd1 = 2415020.75933 + 29.53058868 * k + 0.0001178 * t2 -
                 0.000000155 * t3;
    jd1 += 0.00033 * std::sin((166.56 + 132.87 * t - 0.009173 * t2) * dr);
    double m = 359.2242 + 29.10535608 * k - 0.0000333 * t2 - 0.00000347 * t3;
    double mPrime = 306.0253 + 385.81691806 * k + 0.0107306 * t2 + 0.00001236 * t3;
    double f = 21.2964 + 390.67050646 * k - 0.0016528 * t2 - 0.00000239 * t3;
    double c1 = (0.1734 - 0.000393 * t) * std::sin(m * dr) +
                0.0021 * std::sin(2 * dr * m) - 0.4068 * std::sin(mPrime * dr) +
                0.0161 * std::sin(2 * dr * mPrime) - 0.0004 * std::sin(3 * dr * mPrime) +
                0.0104 * std::sin(2 * dr * f) - 0.0051 * std::sin(dr * (m + mPrime)) -
                0.0074 * std::sin(dr * (m - mPrime)) + 0.0004 * std::sin(dr * (2 * f + m)) -
                0.0004 * std::sin(dr * (2 * f - m)) - 0.0006 * std::sin(dr * (2 * f + mPrime)) +
                0.0010 * std::sin(dr * (2 * f - mPrime)) + 0.0005 * std::sin(dr * (2 * mPrime + m));
    double deltaT = t < -11
        ? 0.001 + 0.000839 * t + 0.0002261 * t2 - 0.00000845 * t3 - 0.000000081 * t * t3
        : -0.000278 + 0.000265 * t + 0.000262 * t2;
    return IntFloor(jd1 + c1 - deltaT + 0.5 + timeZone / 24.0);
}

int SunLongitudeSector(int dayNumber, double timeZone) {
    double t = (dayNumber - 2451545.5 - timeZone / 24.0) / 36525.0;
    double t2 = t * t;
    double dr = kPi / 180.0;
    double m = 357.52910 + 35999.05030 * t - 0.0001559 * t2 - 0.00000048 * t * t2;
    double l0 = 280.46645 + 36000.76983 * t + 0.0003032 * t2;
    double delta = (1.914600 - 0.004817 * t - 0.000014 * t2) * std::sin(dr * m);
    delta += (0.019993 - 0.000101 * t) * std::sin(2 * dr * m) + 0.000290 * std::sin(3 * dr * m);
    double longitude = (l0 + delta) * dr;
    longitude -= kPi * 2 * IntFloor(longitude / (kPi * 2));
    return IntFloor(longitude / kPi * 6);
}

int LunarMonth11(int year, double timeZone) {
    int off = JulianDayFromDate(31, 12, year) - 2415021;
    int k = IntFloor(off / 29.530588853);
    int nm = NewMoonDay(k, timeZone);
    if (SunLongitudeSector(nm, timeZone) >= 9) {
        nm = NewMoonDay(k - 1, timeZone);
    }
    return nm;
}

int LeapMonthOffset(int a11, double timeZone) {
    int k = IntFloor(0.5 + (a11 - 2415021.076998695) / 29.530588853);
    int last = 0;
    int i = 1;
    int arc = SunLongitudeSector(NewMoonDay(k + i, timeZone), timeZone);
    do {
        last = arc;
        ++i;
        arc = SunLongitudeSector(NewMoonDay(k + i, timeZone), timeZone);
    } while (arc != last && i < 14);
    return i - 1;
}

struct VietnameseLunarDateInfo {
    int day = 0;
    int month = 0;
    int year = 0;
    bool leapMonth = false;
};

bool VietnameseLunarDate(int dayNumber, int solarYear,
                         VietnameseLunarDateInfo& lunar) {
    int k = IntFloor((dayNumber - 2415021.076998695) / 29.530588853);
    int monthStart = NewMoonDay(k + 1, kVietnamTimeZone);
    if (monthStart > dayNumber) {
        monthStart = NewMoonDay(k, kVietnamTimeZone);
    }
    int a11 = LunarMonth11(solarYear, kVietnamTimeZone);
    int b11 = a11;
    const bool beforeLunarMonth11 = a11 >= monthStart;
    if (beforeLunarMonth11) {
        a11 = LunarMonth11(solarYear - 1, kVietnamTimeZone);
    } else {
        b11 = LunarMonth11(solarYear + 1, kVietnamTimeZone);
    }
    lunar.day = dayNumber - monthStart + 1;
    int diff = IntFloor((monthStart - a11) / 29.0);
    lunar.month = diff + 11;
    lunar.year = beforeLunarMonth11 ? solarYear : solarYear + 1;
    if (b11 - a11 > 365) {
        int leapOff = LeapMonthOffset(a11, kVietnamTimeZone);
        if (diff >= leapOff) {
            lunar.month = diff + 10;
        }
        if (diff == leapOff) {
            lunar.leapMonth = true;
        }
    }
    if (lunar.month > 12) {
        lunar.month -= 12;
    }
    if (lunar.month >= 11 && diff < 4) {
        lunar.year--;
    }
    return lunar.day >= 1 && lunar.day <= 30 && lunar.month >= 1 &&
           lunar.month <= 12 && lunar.year >= 1;
}

struct VietnameseHoliday {
    PCWSTR shortLabel;
    PCWSTR fullLabel;
};

std::optional<VietnameseHoliday> GetVietnameseHoliday(
    int solarDay, int solarMonth,
    VietnameseLunarDateInfo const& lunar,
    bool includeMung1Ram) {
    if (!lunar.leapMonth) {
        if (lunar.month == 1 && lunar.day == 1) return VietnameseHoliday{L"Tết", L"Mùng 1 Tết Nguyên Đán"};
        if (lunar.month == 1 && lunar.day == 2) return VietnameseHoliday{L"Mùng 2", L"Mùng 2 Tết Nguyên Đán"};
        if (lunar.month == 1 && lunar.day == 3) return VietnameseHoliday{L"Mùng 3", L"Mùng 3 Tết Nguyên Đán"};
        if (lunar.month == 1 && lunar.day == 4) return VietnameseHoliday{L"Mùng 4", L"Mùng 4 Tết Nguyên Đán"};
        if (lunar.month == 1 && lunar.day == 15) return VietnameseHoliday{L"Rằm T1", L"Tết Nguyên Tiêu (Rằm tháng Giêng)"};
        if (lunar.month == 3 && lunar.day == 3) return VietnameseHoliday{L"Hàn thực", L"Tết Hàn Thực (3/3 ÂL)"};
        if (lunar.month == 3 && lunar.day == 10) return VietnameseHoliday{L"Giỗ Tổ", L"Giỗ Tổ Hùng Vương (10/3 ÂL)"};
        if (lunar.month == 4 && lunar.day == 15) return VietnameseHoliday{L"Phật Đản", L"Lễ Phật Đản (15/4 ÂL)"};
        if (lunar.month == 5 && lunar.day == 5) return VietnameseHoliday{L"Đoan Ngọ", L"Tết Đoan Ngọ (5/5 ÂL)"};
        if (lunar.month == 7 && lunar.day == 15) return VietnameseHoliday{L"Vu Lan", L"Lễ Vu Lan (Rằm tháng 7)"};
        if (lunar.month == 8 && lunar.day == 15) return VietnameseHoliday{L"Trung thu", L"Tết Trung Thu (Rằm tháng 8)"};
        if (lunar.month == 12 && lunar.day == 23) return VietnameseHoliday{L"Ông Táo", L"Tiễn Ông Công Ông Táo (23/12 ÂL)"};
        if (lunar.month == 12 && lunar.day == 30) return VietnameseHoliday{L"Tất niên", L"30 Tết (Tất Niên)"};
    }

    if (solarMonth == 1 && solarDay == 1) return VietnameseHoliday{L"Tết DL", L"Tết Dương Lịch"};
    if (solarMonth == 2 && solarDay == 14) return VietnameseHoliday{L"Valentine", L"Lễ Tình Nhân (Valentine)"};
    if (solarMonth == 2 && solarDay == 27) return VietnameseHoliday{L"Thầy thuốc", L"Ngày Thầy thuốc Việt Nam"};
    if (solarMonth == 3 && solarDay == 8) return VietnameseHoliday{L"Q.Tế PN", L"Quốc tế Phụ nữ 8/3"};
    if (solarMonth == 3 && solarDay == 26) return VietnameseHoliday{L"TL Đoàn", L"Ngày thành lập Đoàn TNCS HCM"};
    if (solarMonth == 4 && solarDay == 30) return VietnameseHoliday{L"Thống nhất", L"Ngày Giải phóng miền Nam 30/4"};
    if (solarMonth == 5 && solarDay == 1) return VietnameseHoliday{L"QT Lao động", L"Quốc tế Lao động 1/5"};
    if (solarMonth == 5 && solarDay == 19) return VietnameseHoliday{L"SN Bác", L"Ngày sinh Chủ tịch Hồ Chí Minh"};
    if (solarMonth == 6 && solarDay == 1) return VietnameseHoliday{L"Thiếu nhi", L"Quốc tế Thiếu nhi 1/6"};
    if (solarMonth == 6 && solarDay == 28) return VietnameseHoliday{L"Gia đình", L"Ngày Gia đình Việt Nam"};
    if (solarMonth == 7 && solarDay == 27) return VietnameseHoliday{L"TB Liệt sĩ", L"Ngày Thương binh Liệt sĩ 27/7"};
    if (solarMonth == 8 && solarDay == 19) return VietnameseHoliday{L"CM T8", L"Cách mạng Tháng Tám"};
    if (solarMonth == 9 && solarDay == 2) return VietnameseHoliday{L"Quốc khánh", L"Quốc khánh 2/9"};
    if (solarMonth == 10 && solarDay == 10) return VietnameseHoliday{L"GP Thủ đô", L"Giải phóng Thủ đô 10/10"};
    if (solarMonth == 10 && solarDay == 20) return VietnameseHoliday{L"Phụ nữ VN", L"Ngày Phụ nữ Việt Nam 20/10"};
    if (solarMonth == 11 && solarDay == 20) return VietnameseHoliday{L"Nhà giáo", L"Ngày Nhà giáo Việt Nam 20/11"};
    if (solarMonth == 12 && solarDay == 22) return VietnameseHoliday{L"QĐND VN", L"Ngày thành lập QĐND Việt Nam"};
    if (solarMonth == 12 && solarDay == 24) return VietnameseHoliday{L"Noel", L"Đêm Giáng Sinh"};
    if (solarMonth == 12 && solarDay == 25) return VietnameseHoliday{L"Giáng sinh", L"Lễ Giáng Sinh"};

    if (includeMung1Ram) {
        if (lunar.day == 1) return VietnameseHoliday{nullptr, L"Mùng 1 đầu tháng"};
        if (lunar.day == 15) return VietnameseHoliday{nullptr, L"Ngày Rằm (15 ÂL)"};
    }

    return std::nullopt;
}

struct DayDisplayInfo {
    std::wstring label;
    bool isHoliday = false;
};

DayDisplayInfo GetDayDisplayInfo(int32_t dateKey, Settings const& settings) {
    DayDisplayInfo out;
    if (!settings.showLunarDate) {
        return out;
    }

    int year = dateKey / 10000;
    int month = (dateKey / 100) % 100;
    int day = dateKey % 100;
    int dayNumber = JulianDayFromDate(day, month, year);
    VietnameseLunarDateInfo lunar;
    if (!VietnameseLunarDate(dayNumber, year, lunar)) {
        return out;
    }

    out.label = std::to_wstring(lunar.day) + L"/" + std::to_wstring(lunar.month);

    if (settings.showHolidays) {
        if (auto holiday = GetVietnameseHoliday(day, month, lunar, settings.highlightMung1Ram)) {
            out.isHoliday = true;
            if (holiday->shortLabel && holiday->shortLabel[0] != L'\0') {
                out.label = holiday->shortLabel;
            }
        }
    }

    return out;
}

std::wstring VietnameseLunarFullLabel(int32_t dateKey) {
    int year = dateKey / 10000;
    int month = (dateKey / 100) % 100;
    int day = dateKey % 100;
    VietnameseLunarDateInfo lunar;
    if (!VietnameseLunarDate(JulianDayFromDate(day, month, year), year, lunar)) {
        return L"";
    }

    static constexpr PCWSTR stems[] = {
        L"Giáp", L"Ất", L"Bính", L"Đinh", L"Mậu",
        L"Kỷ", L"Canh", L"Tân", L"Nhâm", L"Quý"};
    static constexpr PCWSTR branches[] = {
        L"Tý", L"Sửu", L"Dần", L"Mão", L"Thìn", L"Tỵ",
        L"Ngọ", L"Mùi", L"Thân", L"Dậu", L"Tuất", L"Hợi"};
    auto positiveMod = [](int value, int divisor) {
        int result = value % divisor;
        return result < 0 ? result + divisor : result;
    };

    std::wstring result = std::to_wstring(lunar.day) + L" Tháng " +
                          std::to_wstring(lunar.month);
    if (lunar.leapMonth) {
        result += L" Nhuận";
    }
    result += L" Năm ";
    result += stems[positiveMod(lunar.year + 6, 10)];
    result += L" ";
    result += branches[positiveMod(lunar.year + 8, 12)];

    if (auto holiday = GetVietnameseHoliday(day, month, lunar, false)) {
        if (holiday->fullLabel) {
            result += L" • ";
            result += holiday->fullLabel;
        }
    }

    return result;
}

std::wstring VietnameseLunarTooltipLabel(int32_t dateKey) {
    int year = dateKey / 10000;
    int month = (dateKey / 100) % 100;
    int day = dateKey % 100;
    VietnameseLunarDateInfo lunar;
    if (!VietnameseLunarDate(JulianDayFromDate(day, month, year), year, lunar)) {
        return L"";
    }

    static constexpr PCWSTR stems[] = {
        L"Giáp", L"Ất", L"Bính", L"Đinh", L"Mậu",
        L"Kỷ", L"Canh", L"Tân", L"Nhâm", L"Quý"};
    static constexpr PCWSTR branches[] = {
        L"Tý", L"Sửu", L"Dần", L"Mão", L"Thìn", L"Tỵ",
        L"Ngọ", L"Mùi", L"Thân", L"Dậu", L"Tuất", L"Hợi"};
    auto positiveMod = [](int value, int divisor) {
        int result = value % divisor;
        return result < 0 ? result + divisor : result;
    };

    std::wstring result;
    if (lunar.day < 10) result += L"0";
    result += std::to_wstring(lunar.day) + L"/";
    if (lunar.month < 10) result += L"0";
    result += std::to_wstring(lunar.month) + L" ";
    result += stems[positiveMod(lunar.year + 6, 10)];
    result += L" ";
    result += branches[positiveMod(lunar.year + 8, 12)];
    return result;
}

std::wstring VietnameseGregorianHeaderDate(SYSTEMTIME const& st) {
    static constexpr PCWSTR kWeekdays[] = {
        L"Chủ Nhật", L"Thứ Hai", L"Thứ Ba", L"Thứ Tư",
        L"Thứ Năm", L"Thứ Sáu", L"Thứ Bảy"};
    wchar_t buf[128];
    swprintf(buf, ARRAYSIZE(buf), L"%s, ngày %d tháng %d",
             kWeekdays[st.wDayOfWeek % 7],
             st.wDay,
             st.wMonth);
    return buf;
}

int32_t DateKeyFromDateTime(wf::DateTime const& dateTime) {
    ULARGE_INTEGER uli;
    uli.QuadPart = static_cast<ULONGLONG>(dateTime.time_since_epoch().count());

    FILETIME fileTime;
    fileTime.dwLowDateTime = uli.LowPart;
    fileTime.dwHighDateTime = uli.HighPart;

    SYSTEMTIME systemTimeUtc{};
    if (!FileTimeToSystemTime(&fileTime, &systemTimeUtc)) {
        return 0;
    }

    SYSTEMTIME systemTimeLocal{};
    if (!SystemTimeToTzSpecificLocalTime(nullptr, &systemTimeUtc,
                                         &systemTimeLocal)) {
        return MakeDateKey(systemTimeUtc.wYear, systemTimeUtc.wMonth, systemTimeUtc.wDay);
    }

    return MakeDateKey(systemTimeLocal.wYear, systemTimeLocal.wMonth, systemTimeLocal.wDay);
}

constexpr PCWSTR kLunarTextBlockName = L"LunarTextBlock";

struct FoundElement {
    wux::DependencyObject element{nullptr};
    int depth = 0;
};

void CollectDescendants(wux::DependencyObject const& root,
                        std::vector<FoundElement>& out) {
    out.push_back({root, 0});

    for (size_t i = 0; i < out.size() && i < 256; i++) {
        wux::DependencyObject current = out[i].element;
        int depth = out[i].depth;
        if (!current || depth > 24) {
            continue;
        }

        int count = 0;
        try {
            count = wuxm::VisualTreeHelper::GetChildrenCount(current);
        } catch (...) {
            continue;
        }

        for (int childIndex = 0; childIndex < count; childIndex++) {
            wux::DependencyObject child = nullptr;
            try {
                child = wuxm::VisualTreeHelper::GetChild(current, childIndex);
            } catch (...) {
                continue;
            }
            if (!child) {
                continue;
            }
            out.push_back({child, depth + 1});
        }
    }
}

std::wstring ElementName(wux::DependencyObject const& element) {
    if (auto frameworkElement = element.try_as<wux::FrameworkElement>()) {
        try {
            return frameworkElement.Name().c_str();
        } catch (...) {
        }
    }
    return {};
}

std::wstring ElementText(wuxc::TextBlock const& textBlock) {
    try {
        return TrimWhitespace(textBlock.Text().c_str());
    } catch (...) {
        return {};
    }
}

bool IsNumberText(std::wstring const& text) {
    if (text.empty()) {
        return false;
    }
    for (wchar_t c : text) {
        if (c < L'0' || c > L'9') {
            return false;
        }
    }
    return true;
}

wuxc::TextBlock FindLabelTextBlock(wuxc::CalendarViewDayItem const& item,
                                   void* preferredId) {
    std::vector<FoundElement> descendants;
    CollectDescendants(item, descendants);

    std::vector<wuxc::TextBlock> candidates;
    for (const auto& found : descendants) {
        auto textBlock = found.element.try_as<wuxc::TextBlock>();
        if (!textBlock || IsNumberText(ElementText(textBlock))) {
            continue;
        }
        candidates.push_back(textBlock);
    }

    if (candidates.empty()) {
        return nullptr;
    }

    if (preferredId) {
        for (const auto& candidate : candidates) {
            if (winrt::get_abi(candidate) == preferredId) {
                return candidate;
            }
        }
    }

    for (const auto& candidate : candidates) {
        if (ElementName(candidate) == kLunarTextBlockName) {
            return candidate;
        }
    }

    wuxc::TextBlock best = nullptr;
    double bestFontSize = 0;
    bool bestHasText = false;
    for (const auto& candidate : candidates) {
        bool hasText = !ElementText(candidate).empty();
        double fontSize = 0;
        try {
            fontSize = candidate.FontSize();
        } catch (...) {
        }

        if (!best || (hasText != bestHasText && hasText) ||
            (hasText == bestHasText && fontSize < bestFontSize)) {
            best = candidate;
            bestFontSize = fontSize;
            bestHasText = hasText;
        }
    }

    return best;
}

struct ColorLocker {
    int64_t token = 0;
    winrt::Windows::UI::Color targetColor{};
    wuxm::Brush originalBrush{nullptr};
    bool hasOriginalBrush = false;
    bool active = false;
};
thread_local std::unordered_map<void*, ColorLocker> t_colorLockers;

bool IsCustomHighlightColor(winrt::Windows::UI::Color const& c) {
    auto settings = GetSettings();
    if (c == settings->holidayColor) {
        return true;
    }
    if (c.R > 220 && c.G < 100 && c.B < 100) {
        return true;
    }
    return false;
}

void LockTextBlockColorStable(wuxc::TextBlock const& tb, winrt::Windows::UI::Color const& color) {
    if (!tb) {
        return;
    }
    try {
        void* id = winrt::get_abi(tb);
        winrt::Windows::UI::Color targetCol{color.A, color.R, color.G, color.B};

        auto& locker = t_colorLockers[id];
        if (!locker.hasOriginalBrush) {
            auto currentBrush = tb.Foreground();
            if (auto scb = currentBrush.try_as<wuxm::SolidColorBrush>()) {
                if (!IsCustomHighlightColor(scb.Color())) {
                    locker.originalBrush = currentBrush;
                    locker.hasOriginalBrush = true;
                }
            } else if (currentBrush) {
                locker.originalBrush = currentBrush;
                locker.hasOriginalBrush = true;
            }
        }

        locker.targetColor = targetCol;

        auto curBrush = tb.Foreground().try_as<wuxm::SolidColorBrush>();
        if (!curBrush || curBrush.Color() != targetCol) {
            tb.Foreground(wuxm::SolidColorBrush(targetCol));
        }

        if (!locker.active) {
            locker.active = true;
            locker.token = tb.RegisterPropertyChangedCallback(
                wuxc::TextBlock::ForegroundProperty(),
                [](wux::DependencyObject const& sender, wux::DependencyProperty const&) {
                    if (auto t = sender.try_as<wuxc::TextBlock>()) {
                        void* sid = winrt::get_abi(t);
                        auto it = t_colorLockers.find(sid);
                        if (it != t_colorLockers.end()) {
                            if (it->second.active) {
                                auto cb = t.Foreground().try_as<wuxm::SolidColorBrush>();
                                if (!cb || cb.Color() != it->second.targetColor) {
                                    t.Foreground(wuxm::SolidColorBrush(it->second.targetColor));
                                }
                            } else {
                                auto cb = t.Foreground().try_as<wuxm::SolidColorBrush>();
                                if (cb && !IsCustomHighlightColor(cb.Color())) {
                                    it->second.originalBrush = cb;
                                    it->second.hasOriginalBrush = true;
                                }
                            }
                        }
                    }
                });
        }
    } catch (...) {
    }
}

void UnlockTextBlockColor(wuxc::TextBlock const& tb) {
    if (!tb) {
        return;
    }
    try {
        void* id = winrt::get_abi(tb);
        auto it = t_colorLockers.find(id);
        if (it != t_colorLockers.end()) {
            if (it->second.active) {
                it->second.active = false;
                if (it->second.token != 0) {
                    try {
                        tb.UnregisterPropertyChangedCallback(
                            wuxc::TextBlock::ForegroundProperty(), it->second.token);
                    } catch (...) {
                    }
                    it->second.token = 0;
                }
            }
        }

        auto curBrush = tb.Foreground().try_as<wuxm::SolidColorBrush>();
        if (curBrush && IsCustomHighlightColor(curBrush.Color())) {
            tb.ClearValue(wuxc::TextBlock::ForegroundProperty());

            auto afterClear = tb.Foreground().try_as<wuxm::SolidColorBrush>();
            if (!afterClear || IsCustomHighlightColor(afterClear.Color())) {
                if (it != t_colorLockers.end() && it->second.hasOriginalBrush && it->second.originalBrush) {
                    tb.Foreground(it->second.originalBrush);
                } else {
                    tb.Foreground(wuxm::SolidColorBrush(winrt::Windows::UI::Color{255, 255, 255, 255}));
                }
            }
        }
    } catch (...) {
    }
}

void ApplyPersistentTodayCircle(wuxc::CalendarViewDayItem const& item,
                                bool isToday,
                                int radiusSetting) {
    if (!item) {
        return;
    }
    try {
        if (auto visual = wuxh::ElementCompositionPreview::GetElementVisual(item)) {
            if (visual.Clip()) {
                visual.Clip(nullptr);
            }
        }

        item.ClearValue(wuxc::Control::BackgroundProperty());

        const double radius = radiusSetting > 0 ? static_cast<double>(radiusSetting) : 19.0;
        const wux::CornerRadius wantedRadius =
            wux::CornerRadiusHelper::FromUniformRadius(radius);

        std::vector<FoundElement> descendants;
        CollectDescendants(item, descendants);
        for (const FoundElement& found : descendants) {
            if (auto border = found.element.try_as<wuxc::Border>()) {
                if (ElementName(border) == L"VnTodayCircle") {
                    border.Visibility(wux::Visibility::Collapsed);
                    continue;
                }

                if (border.CornerRadius().TopLeft != radius) {
                    border.CornerRadius(wantedRadius);
                }

                if (isToday) {
                    winrt::Windows::UI::Color accent = GetTodayHighlightColor();
                    winrt::Windows::UI::Color fillColor{110, accent.R, accent.G, accent.B};
                    winrt::Windows::UI::Color strokeColor{255, accent.R, accent.G, accent.B};

                    border.Width(38.0);
                    border.Height(38.0);
                    border.HorizontalAlignment(wux::HorizontalAlignment::Center);
                    border.VerticalAlignment(wux::VerticalAlignment::Center);

                    border.Background(wuxm::SolidColorBrush(fillColor));
                    border.BorderBrush(wuxm::SolidColorBrush(strokeColor));
                    border.BorderThickness(wux::Thickness{1.5, 1.5, 1.5, 1.5});
                } else {
                    if (!std::isnan(border.Width()) ||
                        border.HorizontalAlignment() == wux::HorizontalAlignment::Center ||
                        border.VerticalAlignment() == wux::VerticalAlignment::Center) {
                        border.ClearValue(wux::FrameworkElement::WidthProperty());
                        border.ClearValue(wux::FrameworkElement::HeightProperty());
                        border.HorizontalAlignment(wux::HorizontalAlignment::Stretch);
                        border.VerticalAlignment(wux::VerticalAlignment::Stretch);
                        border.ClearValue(wuxc::Border::BackgroundProperty());
                        border.ClearValue(wuxc::Border::BorderBrushProperty());
                        border.ClearValue(wuxc::Border::BorderThicknessProperty());
                    }
                }
            }
        }
    } catch (...) {
    }
}

struct DayCell {
    winrt::weak_ref<wuxc::CalendarViewDayItem> item{nullptr};
    winrt::weak_ref<wuxc::TextBlock> dayNumberLabel{nullptr};
    wuxm::Transform savedDayNumberRenderTransform{nullptr};
    bool hasSavedDayNumberRenderTransform = false;

    winrt::weak_ref<wuxc::TextBlock> label{nullptr};
    void* labelId = nullptr;
    int64_t labelChangedToken = 0;
    double savedFontSize = 0;
    bool hasSavedFontSize = false;
    wuxm::Transform savedRenderTransform{nullptr};
    bool hasSavedRenderTransform = false;

    int32_t key = 0;
    int32_t lastStyledKey = 0;
    bool lastIsToday = false;
    uint64_t lastSettingsGen = 0;

    std::wstring savedText;
    bool hasSavedText = false;
    std::wstring appliedText;
    int64_t dateChangedToken = 0;
    bool busy = false;
};

struct TrackedDayItem {
    void* id = nullptr;
    winrt::weak_ref<wuxc::CalendarViewDayItem> item{nullptr};
};

constexpr size_t kMaxTrackedDayItems = 512;

thread_local std::unordered_map<void*, std::unique_ptr<DayCell>> t_cells;
thread_local std::vector<TrackedDayItem> t_dayItems;
thread_local std::unordered_map<void*, winrt::weak_ref<wuxc::CalendarViewDayItem>>
    t_labelOwners;

thread_local winrt::weak_ref<wuxc::CalendarView> t_calendarView{nullptr};
thread_local winrt::weak_ref<wuxc::TextBlock> t_headerOriginalTb{nullptr};
thread_local winrt::weak_ref<wuxc::TextBlock> t_headerClockTb{nullptr};
thread_local winrt::weak_ref<wuxc::TextBlock> t_headerGregTb{nullptr};
thread_local winrt::weak_ref<wuxc::TextBlock> t_headerLunarTb{nullptr};
thread_local winrt::weak_ref<wuxc::TextBlock> t_headerWeekBadgeTb{nullptr};
thread_local winrt::weak_ref<wuxc::Grid> t_weekDayNamesGrid{nullptr};
thread_local winrt::weak_ref<wux::FrameworkElement> t_notificationCenterPanel{nullptr};
thread_local winrt::weak_ref<wux::FrameworkElement> t_focusPanel{nullptr};

void ApplyToDayItem(wuxc::CalendarViewDayItem const& item);

void JumpCalendarToToday() {
    if (auto calView = t_calendarView.get()) {
        try {
            calView.SetDisplayDate(winrt::clock::now());
        } catch (...) {
        }
    }
}

void* CellId(wuxc::CalendarViewDayItem const& item) {
    return winrt::get_abi(item);
}

DayCell& CellFor(wuxc::CalendarViewDayItem const& item) {
    std::unique_ptr<DayCell>& entry = t_cells[CellId(item)];
    if (entry && entry->item.get() != item) {
        entry = nullptr;
    }
    if (!entry) {
        entry = std::make_unique<DayCell>();
        entry->item = winrt::make_weak(item);
    }
    return *entry;
}

void TrackDayItem(wuxc::CalendarViewDayItem const& item) {
    void* id = winrt::get_abi(item);

    for (auto it = t_dayItems.begin(); it != t_dayItems.end(); ++it) {
        if (it->id != id) {
            continue;
        }
        if (!it->item.get()) {
            it->item = winrt::make_weak(item);
        }
        return;
    }

    if (t_dayItems.size() >= kMaxTrackedDayItems) {
        t_dayItems.erase(t_dayItems.begin());
    }
    t_dayItems.push_back({id, winrt::make_weak(item)});
}

void OnLabelTextChanged(wux::DependencyObject const& sender) {
    auto it = t_labelOwners.find(winrt::get_abi(sender));
    if (it == t_labelOwners.end()) {
        return;
    }
    if (auto item = it->second.get()) {
        ApplyToDayItem(item);
    }
}

void ApplyToDayItem(wuxc::CalendarViewDayItem const& item) {
    const std::shared_ptr<const Settings> settings = GetSettings();
    if (!item) {
        return;
    }

    int32_t key = DateKeyFromDateTime(item.Date());
    if (!key) {
        return;
    }

    DayCell& cell = CellFor(item);
    if (cell.busy) {
        return;
    }
    cell.busy = true;
    struct BusyGuard {
        bool& busy;
        ~BusyGuard() { busy = false; }
    } guard{cell.busy};

    if (cell.key != key) {
        if (auto prevDayNum = cell.dayNumberLabel.get()) {
            UnlockTextBlockColor(prevDayNum);
        }
        if (auto prevLabel = cell.label.get()) {
            UnlockTextBlockColor(prevLabel);
        }
        cell.key = key;
        cell.lastStyledKey = 0;
        cell.savedText.clear();
        cell.appliedText.clear();
        cell.hasSavedText = false;
    }

    if (cell.dateChangedToken == 0) {
        try {
            cell.dateChangedToken = item.RegisterPropertyChangedCallback(
                wuxc::CalendarViewDayItem::DateProperty(),
                [](wux::DependencyObject const& sender,
                   wux::DependencyProperty const&) {
                    if (auto dayItem =
                            sender.try_as<wuxc::CalendarViewDayItem>()) {
                        ApplyToDayItem(dayItem);
                    }
                });
        } catch (...) {
        }
    }

    SYSTEMTIME now{};
    GetLocalTime(&now);
    const bool isToday = (key == MakeDateKey(now.wYear, now.wMonth, now.wDay));
    const uint64_t currentGen = g_settingsGeneration.load();

    const DayDisplayInfo displayInfo = GetDayDisplayInfo(key, *settings);
    const bool shouldColorRed = displayInfo.isHoliday && !isToday;

    auto cachedDayNum = cell.dayNumberLabel.get();
    auto cachedLabel = cell.label.get();
    if (cell.lastStyledKey == key &&
        cell.lastIsToday == isToday &&
        cell.lastSettingsGen == currentGen &&
        cachedDayNum && cachedLabel &&
        ElementText(cachedLabel) == displayInfo.label) {
        if (isToday) {
            LockTextBlockColorStable(cachedDayNum, winrt::Windows::UI::Color{255, 255, 255, 255});
            LockTextBlockColorStable(cachedLabel, winrt::Windows::UI::Color{255, 255, 255, 255});
        } else if (shouldColorRed) {
            LockTextBlockColorStable(cachedDayNum, settings->holidayColor);
            LockTextBlockColorStable(cachedLabel, settings->holidayColor);
        } else {
            UnlockTextBlockColor(cachedDayNum);
            UnlockTextBlockColor(cachedLabel);
        }
        return;
    }

    ApplyPersistentTodayCircle(item, isToday, settings->selectedDayCornerRadius);

    try {
        if (!cachedDayNum) {
            std::vector<FoundElement> descendants;
            CollectDescendants(item, descendants);
            for (const FoundElement& found : descendants) {
                if (auto tb = found.element.try_as<wuxc::TextBlock>()) {
                    if (IsNumberText(ElementText(tb))) {
                        cachedDayNum = tb;
                        cell.dayNumberLabel = winrt::make_weak(tb);
                        cell.savedDayNumberRenderTransform = tb.RenderTransform();
                        cell.hasSavedDayNumberRenderTransform = true;
                        break;
                    }
                }
            }
        }
        if (cachedDayNum && settings->showLunarDate) {
            double targetY = -1.5;
            auto currentTransform = cachedDayNum.RenderTransform();
            auto currentOffset = currentTransform.try_as<wuxm::TranslateTransform>();
            if (!currentOffset || std::fabs(currentOffset.Y() - targetY) > 0.1) {
                auto offset = wuxm::TranslateTransform();
                offset.Y(targetY);
                cachedDayNum.RenderTransform(offset);
            }
        }
    } catch (...) {
    }

    if (cachedDayNum) {
        if (isToday) {
            LockTextBlockColorStable(cachedDayNum, winrt::Windows::UI::Color{255, 255, 255, 255});
        } else if (shouldColorRed) {
            LockTextBlockColorStable(cachedDayNum, settings->holidayColor);
        } else {
            UnlockTextBlockColor(cachedDayNum);
        }
    }

    wuxc::TextBlock label = cachedLabel ? cachedLabel : FindLabelTextBlock(item, cell.labelId);
    if (!label) {
        return;
    }

    if (cell.label.get() != label) {
        if (auto previous = cell.label.get()) {
            if (cell.labelChangedToken) {
                try {
                    previous.UnregisterPropertyChangedCallback(
                        wuxc::TextBlock::TextProperty(), cell.labelChangedToken);
                } catch (...) {
                }
            }
            t_labelOwners.erase(winrt::get_abi(previous));
        }
        cell.labelChangedToken = 0;

        cell.label = winrt::make_weak(label);
        cell.labelId = winrt::get_abi(label);
        cell.savedFontSize = label.FontSize();
        cell.hasSavedFontSize = true;
        cell.savedRenderTransform = label.RenderTransform();
        cell.hasSavedRenderTransform = true;
        cell.savedText.clear();
        cell.appliedText.clear();
        cell.hasSavedText = false;
        t_labelOwners[cell.labelId] = winrt::make_weak(item);

        try {
            cell.labelChangedToken = label.RegisterPropertyChangedCallback(
                wuxc::TextBlock::TextProperty(),
                [](wux::DependencyObject const& sender,
                   wux::DependencyProperty const&) {
                    OnLabelTextChanged(sender);
                });
        } catch (...) {
        }
    }

    const std::wstring& desired = displayInfo.label;

    try {
        label.TextTrimming(wux::TextTrimming::None);
        label.TextWrapping(wux::TextWrapping::NoWrap);
        label.HorizontalAlignment(wux::HorizontalAlignment::Center);
        label.TextAlignment(wux::TextAlignment::Center);
        label.Margin(wux::Thickness{-6, 0, -6, 0});

        double wantedFontSize = settings->showLunarDate
                                    ? settings->lunarFontSize
                                    : cell.savedFontSize;
        if (displayInfo.isHoliday && desired.size() >= 7) {
            wantedFontSize = 7.2;
        }
        if (cell.hasSavedFontSize &&
            std::fabs(label.FontSize() - wantedFontSize) > 0.1) {
            label.FontSize(wantedFontSize);
        }
    } catch (...) {
    }

    try {
        if (cell.hasSavedRenderTransform && settings->showLunarDate) {
            double targetLabelY = 0.5;
            auto currentTransform = label.RenderTransform();
            auto currentOffset = currentTransform.try_as<wuxm::TranslateTransform>();
            if (!currentOffset || std::fabs(currentOffset.Y() - targetLabelY) > 0.1) {
                auto offset = wuxm::TranslateTransform();
                offset.Y(targetLabelY);
                label.RenderTransform(offset);
            }
        }
    } catch (...) {
    }

    std::wstring current = ElementText(label);

    if (!desired.empty() && current != desired) {
        if (current != cell.appliedText) {
            cell.savedText = current;
            cell.hasSavedText = true;
        }
        try {
            label.Text(desired);
        } catch (...) {
        }
    } else if (desired.empty() && !cell.appliedText.empty() &&
               current == cell.appliedText) {
        try {
            if (cell.hasSavedText) {
                label.Text(cell.savedText);
            }
        } catch (...) {
        }
        cell.hasSavedText = false;
    }

    cell.appliedText = desired;

    if (isToday) {
        LockTextBlockColorStable(label, winrt::Windows::UI::Color{255, 255, 255, 255});
    } else if (shouldColorRed) {
        LockTextBlockColorStable(label, settings->holidayColor);
    } else {
        UnlockTextBlockColor(label);
    }

    cell.lastStyledKey = key;
    cell.lastIsToday = isToday;
    cell.lastSettingsGen = currentGen;
}

void UpdateFlyoutPanelsVisibility() {
    if (auto notifPanel = t_notificationCenterPanel.get()) {
        try {
            bool wantHide = GetSettings()->hideNotificationCenter;
            auto targetVis = wantHide ? wux::Visibility::Collapsed : wux::Visibility::Visible;
            if (notifPanel.Visibility() != targetVis) {
                notifPanel.Visibility(targetVis);
                notifPanel.Opacity(wantHide ? 0.0 : 1.0);
                notifPanel.MaxHeight(wantHide ? 0.0 : std::numeric_limits<double>::infinity());
                notifPanel.Height(wantHide ? 0.0 : std::numeric_limits<double>::quiet_NaN());
                if (wantHide) {
                    notifPanel.Margin(wux::Thickness{0, 0, 0, 0});
                }
            }
        } catch (...) {
        }
    }

    if (auto focusPanel = t_focusPanel.get()) {
        try {
            bool wantHide = GetSettings()->hideFocusPanel;
            auto targetVis = wantHide ? wux::Visibility::Collapsed : wux::Visibility::Visible;
            if (focusPanel.Visibility() != targetVis) {
                focusPanel.Visibility(targetVis);
                focusPanel.Opacity(wantHide ? 0.0 : 1.0);
                focusPanel.MaxHeight(wantHide ? 0.0 : std::numeric_limits<double>::infinity());
                focusPanel.Height(wantHide ? 0.0 : std::numeric_limits<double>::quiet_NaN());
                if (wantHide) {
                    focusPanel.Margin(wux::Thickness{0, 0, 0, 0});
                }
            }
        } catch (...) {
        }
    }
}

void CompactMonthNavigationHeader(wuxc::CalendarView const& calView) {
    if (!calView) {
        return;
    }
    try {
        std::vector<FoundElement> descendants;
        CollectDescendants(calView, descendants);
        for (const auto& found : descendants) {
            if (auto btn = found.element.try_as<wuxc::Button>()) {
                std::wstring name = ElementName(btn);
                if (name == L"HeaderButton" || name == L"PreviousButton" || name == L"NextButton") {
                    btn.Padding(wux::Thickness{4, 0, 4, 0});
                    btn.MinHeight(28.0);
                    btn.Height(30.0);
                    btn.VerticalAlignment(wux::VerticalAlignment::Center);
                    btn.VerticalContentAlignment(wux::VerticalAlignment::Center);

                    if (auto visual = wuxh::ElementCompositionPreview::GetElementVisual(btn)) {
                        if (visual.Clip()) {
                            visual.Clip(nullptr);
                        }
                    }

                    std::vector<FoundElement> btnChildren;
                    CollectDescendants(btn, btnChildren);
                    for (const auto& childFound : btnChildren) {
                        if (auto fe = childFound.element.try_as<wux::FrameworkElement>()) {
                            if (fe != btn) {
                                fe.Margin(wux::Thickness{0, 0, 0, 0});
                                fe.VerticalAlignment(wux::VerticalAlignment::Center);
                            }
                            if (auto childVis = wuxh::ElementCompositionPreview::GetElementVisual(fe)) {
                                if (childVis.Clip()) {
                                    childVis.Clip(nullptr);
                                }
                            }
                        }
                    }

                    wux::DependencyObject parent = wuxm::VisualTreeHelper::GetParent(btn);
                    if (auto parentGrid = parent.try_as<wuxc::Grid>()) {
                        parentGrid.Margin(wux::Thickness{0, -4, 0, 2});
                        if (auto gridVis = wuxh::ElementCompositionPreview::GetElementVisual(parentGrid)) {
                            if (gridVis.Clip()) {
                                gridVis.Clip(nullptr);
                            }
                        }
                    }
                }
            }
        }
    } catch (...) {
    }
}

void StyleWeekDayHeaderBar(wuxc::Grid const& weekDayGrid) {
    if (!weekDayGrid) {
        return;
    }
    try {
        weekDayGrid.MinHeight(38.0);
        weekDayGrid.Margin(wux::Thickness{0, 8, 0, 4});

        if (auto vis = wuxh::ElementCompositionPreview::GetElementVisual(weekDayGrid)) {
            if (vis.Clip()) {
                vis.Clip(nullptr);
            }
        }
        if (auto parent = wuxm::VisualTreeHelper::GetParent(weekDayGrid).try_as<wux::UIElement>()) {
            if (auto pVis = wuxh::ElementCompositionPreview::GetElementVisual(parent)) {
                if (pVis.Clip()) {
                    pVis.Clip(nullptr);
                }
            }
        }

        wuxc::Border pillBorder = nullptr;
        uint32_t count = weekDayGrid.Children().Size();
        for (uint32_t i = 0; i < count; ++i) {
            auto child = weekDayGrid.Children().GetAt(i);
            if (auto b = child.try_as<wuxc::Border>()) {
                if (ElementName(b) == L"VnWeekDayPillBorder") {
                    pillBorder = b;
                }
            } else if (auto tb = child.try_as<wuxc::TextBlock>()) {
                if (ElementText(tb) == L"C") {
                    tb.Text(L"CN");
                }
                tb.VerticalAlignment(wux::VerticalAlignment::Center);
                tb.Margin(wux::Thickness{0, 0, 0, 0});
            }
        }

        if (!pillBorder) {
            pillBorder = wuxc::Border();
            pillBorder.Name(L"VnWeekDayPillBorder");
            pillBorder.IsHitTestVisible(false);
            wuxc::Grid::SetColumnSpan(pillBorder, 7);
            wuxc::Grid::SetRowSpan(pillBorder, 1);
            weekDayGrid.Children().InsertAt(0, pillBorder);
        }

        pillBorder.Height(28.0);
        pillBorder.VerticalAlignment(wux::VerticalAlignment::Center);
        pillBorder.HorizontalAlignment(wux::HorizontalAlignment::Stretch);
        pillBorder.CornerRadius(wux::CornerRadiusHelper::FromUniformRadius(9.0));
        pillBorder.Background(wuxm::SolidColorBrush(
            winrt::Windows::UI::Color{95, 14, 68, 115}));
        pillBorder.BorderBrush(wuxm::SolidColorBrush(
            winrt::Windows::UI::Color{85, 76, 194, 255}));
        pillBorder.BorderThickness(wux::Thickness{1.5, 1.5, 1.5, 1.5});
        pillBorder.Margin(wux::Thickness{6, 6, 6, 6});
    } catch (...) {
    }
}

bool IsFlyoutTopDateString(std::wstring const& text) {
    if (text.empty()) {
        return false;
    }
    if (text.find(L", ") != std::wstring::npos) {
        return true;
    }
    if (text.find(L"Thứ ") != std::wstring::npos ||
        text.find(L"Chủ Nhật") != std::wstring::npos ||
        text.find(L"Chủ nhật") != std::wstring::npos) {
        return true;
    }
    return false;
}

bool SubtreeContainsCalendarItem(wux::DependencyObject const& root, int depth) {
    if (!root || depth > 20) {
        return false;
    }
    if (root.try_as<wuxc::CalendarView>() || root.try_as<wuxc::CalendarViewDayItem>()) {
        return true;
    }
    int count = 0;
    try {
        count = wuxm::VisualTreeHelper::GetChildrenCount(root);
    } catch (...) {
        return false;
    }
    for (int i = 0; i < count; ++i) {
        wux::DependencyObject child = nullptr;
        try {
            child = wuxm::VisualTreeHelper::GetChild(root, i);
        } catch (...) {
            continue;
        }
        if (SubtreeContainsCalendarItem(child, depth + 1)) {
            return true;
        }
    }
    return false;
}

void FindFlyoutElementsInSubtree(wux::DependencyObject const& root, int depth) {
    if (!root || depth > 32) {
        return;
    }

    if (!t_calendarView.get()) {
        if (auto cv = root.try_as<wuxc::CalendarView>()) {
            t_calendarView = winrt::make_weak(cv);
        }
    }

    if (!t_focusPanel.get()) {
        if (auto tb = root.try_as<wuxc::TextBlock>()) {
            std::wstring text = ElementText(tb);
            if (text.find(L"mins") != std::wstring::npos || text.find(L"phút") != std::wstring::npos || text == L"Focus" || text == L"Tập trung") {
                wux::DependencyObject curr = tb;
                wux::FrameworkElement bestContainer = nullptr;
                for (int k = 0; k < 10 && curr; ++k) {
                    wux::DependencyObject p = wuxm::VisualTreeHelper::GetParent(curr);
                    if (!p || SubtreeContainsCalendarItem(p, 0)) {
                        break;
                    }
                    if (auto fe = p.try_as<wux::FrameworkElement>()) {
                        bestContainer = fe;
                    }
                    curr = p;
                }
                if (bestContainer) {
                    t_focusPanel = winrt::make_weak(bestContainer);
                }
            }
        }
    }

    if (!t_notificationCenterPanel.get()) {
        if (auto fe = root.try_as<wux::FrameworkElement>()) {
            std::wstring name = ElementName(fe);
            if (name == L"NotificationCenterGrid" ||
                name == L"NotificationCenter" ||
                name == L"NotificationsRoot" ||
                name == L"NotificationListView") {
                if (!SubtreeContainsCalendarItem(fe, 0)) {
                    t_notificationCenterPanel = winrt::make_weak(fe);
                }
            }
        }
        if (auto tb = root.try_as<wuxc::TextBlock>()) {
            std::wstring text = ElementText(tb);
            if (text == L"Notifications" || text == L"Thông báo" ||
                text == L"Do not disturb is on" || text == L"No new notifications") {
                wux::DependencyObject curr = tb;
                wux::FrameworkElement bestContainer = nullptr;
                for (int k = 0; k < 10 && curr; ++k) {
                    wux::DependencyObject p = wuxm::VisualTreeHelper::GetParent(curr);
                    if (!p || SubtreeContainsCalendarItem(p, 0)) {
                        break;
                    }
                    if (auto fe = p.try_as<wux::FrameworkElement>()) {
                        bestContainer = fe;
                    }
                    curr = p;
                }
                if (bestContainer) {
                    t_notificationCenterPanel = winrt::make_weak(bestContainer);
                }
            }
        }
    }

    if (!t_headerOriginalTb.get()) {
        if (auto tb = root.try_as<wuxc::TextBlock>()) {
            std::wstring name = ElementName(tb);
            if (name != L"VnHeaderClock" && name != L"VnHeaderGreg" && name != L"VnHeaderLunar" && name != L"VnHeaderWeekBadge") {
                std::wstring text = ElementText(tb);
                if (IsFlyoutTopDateString(text)) {
                    t_headerOriginalTb = winrt::make_weak(tb);
                }
            }
        }
    }

    if (!t_weekDayNamesGrid.get()) {
        if (auto grid = root.try_as<wuxc::Grid>()) {
            std::wstring name = ElementName(grid);
            if (name == L"WeekDayNames") {
                t_weekDayNamesGrid = winrt::make_weak(grid);
            } else if (grid.Children().Size() == 7) {
                bool allShortTextBlocks = true;
                for (uint32_t i = 0; i < 7; ++i) {
                    auto tb = grid.Children().GetAt(i).try_as<wuxc::TextBlock>();
                    if (!tb || ElementText(tb).empty() || ElementText(tb).size() > 3 ||
                        IsNumberText(ElementText(tb))) {
                        allShortTextBlocks = false;
                        break;
                    }
                }
                if (allShortTextBlocks) {
                    t_weekDayNamesGrid = winrt::make_weak(grid);
                }
            }
        }
    }

    int count = 0;
    try {
        count = wuxm::VisualTreeHelper::GetChildrenCount(root);
    } catch (...) {
        return;
    }

    for (int i = 0; i < count; ++i) {
        wux::DependencyObject child = nullptr;
        try {
            child = wuxm::VisualTreeHelper::GetChild(root, i);
        } catch (...) {
            continue;
        }
        FindFlyoutElementsInSubtree(child, depth + 1);
    }
}

void DiscoverElementsFromDayItem(wuxc::CalendarViewDayItem const& item) {
    if ((t_headerOriginalTb.get() && t_weekDayNamesGrid.get() && t_notificationCenterPanel.get() && t_focusPanel.get() && t_calendarView.get()) || !item) {
        return;
    }
    try {
        wux::DependencyObject root = item;
        for (int i = 0; i < 24; ++i) {
            wux::DependencyObject parent = wuxm::VisualTreeHelper::GetParent(root);
            if (!parent) {
                break;
            }
            root = parent;
        }
        FindFlyoutElementsInSubtree(root, 0);
    } catch (...) {
    }
}

void HideOtherHeaderTextBlocksInPanel(wuxc::Panel const& panel) {
    if (!panel) {
        return;
    }
    try {
        for (uint32_t i = 0; i < panel.Children().Size(); ++i) {
            auto child = panel.Children().GetAt(i);
            if (auto g = child.try_as<wuxc::Grid>()) {
                if (g.Name() == L"VnCustomHeaderRoot" || g.Name() == L"VnHeaderBottomGrid") {
                    continue;
                }
            }
            if (auto b = child.try_as<wuxc::Border>()) {
                if (b.Name() == L"VnHeaderWeekBorder") {
                    continue;
                }
            }
            if (auto sp = child.try_as<wuxc::StackPanel>()) {
                if (sp.Name() == L"VnCustomHeaderStack") {
                    continue;
                }
                if (sp.Name() == L"VnLunarHeaderStack") {
                    sp.Visibility(wux::Visibility::Collapsed);
                    sp.Opacity(0.0);
                    continue;
                }
            }
            if (auto tb = child.try_as<wuxc::TextBlock>()) {
                std::wstring name = ElementName(tb);
                if (name != L"VnHeaderClock" && name != L"VnHeaderGreg" && name != L"VnHeaderLunar" && name != L"VnHeaderWeekBadge") {
                    tb.Opacity(0.0);
                    tb.MaxHeight(0.0);
                    tb.Visibility(wux::Visibility::Collapsed);
                }
            }
        }
    } catch (...) {
    }
}

void UpdateFlyoutHeaderClockAndLunar() {
    const std::shared_ptr<const Settings> settings = GetSettings();

    UpdateFlyoutPanelsVisibility();

    if (!settings->showLunarDate) {
        return;
    }

    if (auto weekGrid = t_weekDayNamesGrid.get()) {
        StyleWeekDayHeaderBar(weekGrid);
    }

    auto origTb = t_headerOriginalTb.get();
    if (!origTb) {
        return;
    }

    SYSTEMTIME now{};
    GetLocalTime(&now);
    wchar_t timeStr[32];
    swprintf(timeStr, ARRAYSIZE(timeStr), L"%02d:%02d:%02d",
             now.wHour, now.wMinute, now.wSecond);
    std::wstring gregLine = VietnameseGregorianHeaderDate(now);
    std::wstring lunarLine = VietnameseLunarFullLabel(
        MakeDateKey(now.wYear, now.wMonth, now.wDay));
    
    wchar_t weekStr[32];
    swprintf(weekStr, ARRAYSIZE(weekStr), L"Tuần %d", GetIsoWeekOfYear(now));

    try {
        auto clockTb = t_headerClockTb.get();
        auto gregTb = t_headerGregTb.get();
        auto lunarTb = t_headerLunarTb.get();
        auto weekBadgeTb = t_headerWeekBadgeTb.get();

        if (clockTb && gregTb && lunarTb && weekBadgeTb) {
            if (auto currentBrush = clockTb.Foreground().try_as<wuxm::SolidColorBrush>()) {
                if (currentBrush.Color() != settings->clockColor) {
                    clockTb.Foreground(wuxm::SolidColorBrush(settings->clockColor));
                }
            } else {
                clockTb.Foreground(wuxm::SolidColorBrush(settings->clockColor));
            }

            if (ElementText(clockTb) != timeStr) {
                clockTb.Text(timeStr);
            }
            if (ElementText(gregTb) != gregLine) {
                gregTb.Text(gregLine);
            }
            if (ElementText(lunarTb) != lunarLine) {
                lunarTb.Text(lunarLine);
            }
            if (ElementText(weekBadgeTb) != weekStr) {
                weekBadgeTb.Text(weekStr);
            }
            return;
        }

        origTb.Opacity(0.0);
        origTb.MaxHeight(0.0);
        origTb.Visibility(wux::Visibility::Collapsed);

        wux::DependencyObject curr = origTb;
        wuxc::Panel parentPanel = nullptr;
        wuxc::Grid headerTopGrid = nullptr;
        wux::UIElement childInPanel = origTb;

        for (int i = 0; i < 7 && curr; ++i) {
            wux::DependencyObject p = wuxm::VisualTreeHelper::GetParent(curr);
            if (!p) {
                break;
            }
            if (auto fe = p.try_as<wux::FrameworkElement>()) {
                fe.Height(std::numeric_limits<double>::quiet_NaN());
                fe.MaxHeight(std::numeric_limits<double>::infinity());
                if (fe.MinHeight() < 92.0 && i <= 3) {
                    fe.MinHeight(92.0);
                }
                if (i <= 3) {
                    fe.HorizontalAlignment(wux::HorizontalAlignment::Stretch);
                }
            }
            if (auto g = p.try_as<wuxc::Grid>()) {
                if (!headerTopGrid && !SubtreeContainsCalendarItem(g, 0)) {
                    headerTopGrid = g;
                }
                for (auto rowDef : g.RowDefinitions()) {
                    if (rowDef.Height().GridUnitType == wux::GridUnitType::Pixel &&
                        rowDef.Height().Value > 20.0 && rowDef.Height().Value < 90.0) {
                        rowDef.Height(wux::GridLengthHelper::Auto());
                        rowDef.MinHeight(92.0);
                    }
                }
            }
            if (!parentPanel) {
                if (auto panel = p.try_as<wuxc::Panel>()) {
                    parentPanel = panel;
                    childInPanel = curr.try_as<wux::UIElement>();
                }
            }
            curr = p;
        }

        if (parentPanel) {
            parentPanel.HorizontalAlignment(wux::HorizontalAlignment::Stretch);
            if (childInPanel && childInPanel != origTb) {
                if (auto feCheck = childInPanel.try_as<wux::FrameworkElement>()) {
                    if (feCheck.Name() != L"VnCustomHeaderStack") {
                        childInPanel.Opacity(0.0);
                        childInPanel.Visibility(wux::Visibility::Collapsed);
                    }
                }
            }
            HideOtherHeaderTextBlocksInPanel(parentPanel);
        }

        wuxc::Panel badgeHostPanel = headerTopGrid ? headerTopGrid.try_as<wuxc::Panel>() : parentPanel;

        if (parentPanel && badgeHostPanel) {
            for (uint32_t i = 0; i < parentPanel.Children().Size(); ++i) {
                auto child = parentPanel.Children().GetAt(i);
                if (auto sp = child.try_as<wuxc::StackPanel>()) {
                    if (sp.Name() == L"VnCustomHeaderStack" && sp.Children().Size() >= 3) {
                        clockTb = sp.Children().GetAt(0).try_as<wuxc::TextBlock>();
                        gregTb = sp.Children().GetAt(1).try_as<wuxc::TextBlock>();
                        lunarTb = sp.Children().GetAt(2).try_as<wuxc::TextBlock>();
                    }
                }
            }

            for (uint32_t i = 0; i < badgeHostPanel.Children().Size(); ++i) {
                auto child = badgeHostPanel.Children().GetAt(i);
                if (auto border = child.try_as<wuxc::Border>()) {
                    if (border.Name() == L"VnHeaderWeekBorder") {
                        weekBadgeTb = border.Child().try_as<wuxc::TextBlock>();
                    }
                }
            }

            if (!clockTb) {
                wuxm::SolidColorBrush whiteBrush(winrt::Windows::UI::Color{255, 255, 255, 255});

                wuxc::StackPanel stack;
                stack.Name(L"VnCustomHeaderStack");
                stack.Orientation(wuxc::Orientation::Vertical);
                stack.HorizontalAlignment(wux::HorizontalAlignment::Left);
                stack.VerticalAlignment(wux::VerticalAlignment::Center);
                stack.Margin(wux::Thickness{16, 0, 0, 4});
                stack.Background(wuxm::SolidColorBrush(winrt::Windows::UI::Color{0, 0, 0, 0}));

                stack.Tapped([](wf::IInspectable const&, wuxi::TappedRoutedEventArgs const& args) {
                    args.Handled(true);
                    JumpCalendarToToday();
                });

                clockTb = wuxc::TextBlock();
                clockTb.Name(L"VnHeaderClock");
                clockTb.FontSize(30.0);
                clockTb.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
                clockTb.Foreground(wuxm::SolidColorBrush(settings->clockColor));
                clockTb.Margin(wux::Thickness{0, -2, 0, 1});

                gregTb = wuxc::TextBlock();
                gregTb.Name(L"VnHeaderGreg");
                gregTb.FontSize(13.5);
                gregTb.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
                gregTb.Foreground(whiteBrush);
                gregTb.Margin(wux::Thickness{0, 0, 0, 2});

                lunarTb = wuxc::TextBlock();
                lunarTb.Name(L"VnHeaderLunar");
                lunarTb.FontSize(12.5);
                lunarTb.Foreground(whiteBrush);
                lunarTb.Opacity(0.85);
                lunarTb.Margin(wux::Thickness{0, 0, 0, 0});

                stack.Children().Append(clockTb);
                stack.Children().Append(gregTb);
                stack.Children().Append(lunarTb);

                parentPanel.Children().Append(stack);

                t_headerClockTb = winrt::make_weak(clockTb);
                t_headerGregTb = winrt::make_weak(gregTb);
                t_headerLunarTb = winrt::make_weak(lunarTb);
            }

            if (!weekBadgeTb) {
                wuxm::SolidColorBrush whiteBrush(winrt::Windows::UI::Color{255, 255, 255, 255});

                wuxc::Border weekBorder;
                weekBorder.Name(L"VnHeaderWeekBorder");
                weekBorder.HorizontalAlignment(wux::HorizontalAlignment::Right);
                weekBorder.VerticalAlignment(wux::VerticalAlignment::Bottom);
                weekBorder.Margin(wux::Thickness{0, 0, 14, 8});
                weekBorder.Background(wuxm::SolidColorBrush(winrt::Windows::UI::Color{60, 255, 255, 255}));
                weekBorder.CornerRadius(wux::CornerRadiusHelper::FromUniformRadius(4.0));
                weekBorder.Padding(wux::Thickness{8, 2, 8, 2});
                weekBorder.IsHitTestVisible(true);

                weekBorder.Tapped([](wf::IInspectable const&, wuxi::TappedRoutedEventArgs const& args) {
                    args.Handled(true);
                    JumpCalendarToToday();
                });

                wuxc::Grid::SetColumn(weekBorder, 0);
                wuxc::Grid::SetColumnSpan(weekBorder, 4);
                wuxc::Grid::SetRow(weekBorder, 0);
                wuxc::Grid::SetRowSpan(weekBorder, 4);

                weekBadgeTb = wuxc::TextBlock();
                weekBadgeTb.Name(L"VnHeaderWeekBadge");
                weekBadgeTb.FontSize(11.5);
                weekBadgeTb.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
                weekBadgeTb.Foreground(whiteBrush);

                weekBorder.Child(weekBadgeTb);
                badgeHostPanel.Children().Append(weekBorder);

                t_headerWeekBadgeTb = winrt::make_weak(weekBadgeTb);
            }
        }

        if (clockTb && ElementText(clockTb) != timeStr) {
            clockTb.Text(timeStr);
        }
        if (gregTb && ElementText(gregTb) != gregLine) {
            gregTb.Text(gregLine);
        }
        if (lunarTb && ElementText(lunarTb) != lunarLine) {
            lunarTb.Text(lunarLine);
        }
        if (weekBadgeTb && ElementText(weekBadgeTb) != weekStr) {
            weekBadgeTb.Text(weekStr);
        }
    } catch (...) {
    }
}

thread_local winrt::Windows::System::DispatcherQueueTimer t_sweepTimer{nullptr};
thread_local winrt::Windows::System::DispatcherQueueTimer::Tick_revoker
    t_sweepTimerRevoker;
thread_local bool t_calendarHeaderCompacted = false;

constexpr int kSweepIntervalMs = 500;

void SweepDayItems(bool forceFullTreeScan = false);

void StartSweepTimer() {
    try {
        if (!t_sweepTimer) {
            auto dispatcherQueue =
                winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
            if (!dispatcherQueue) {
                return;
            }

            t_sweepTimer = dispatcherQueue.CreateTimer();
            t_sweepTimer.Interval(std::chrono::milliseconds{kSweepIntervalMs});
            t_sweepTimerRevoker = t_sweepTimer.Tick(
                winrt::auto_revoke,
                [](winrt::Windows::System::DispatcherQueueTimer const&,
                   wf::IInspectable const&) { SweepDayItems(false); });
        }

        if (!t_sweepTimer.IsRunning()) {
            t_sweepTimer.Start();
        }
    } catch (...) {
    }
}

constexpr size_t kMaxScannedElements = 16384;
constexpr int kMaxScanDepth = 96;

void CollectDayItemsFromElement(wux::DependencyObject const& root,
                                size_t& budget,
                                int depth) {
    if (!root || budget == 0 || depth > kMaxScanDepth) {
        return;
    }
    budget--;

    if (auto dayItem = root.try_as<wuxc::CalendarViewDayItem>()) {
        TrackDayItem(dayItem);
        return;
    }

    if (!t_calendarView.get()) {
        if (auto cv = root.try_as<wuxc::CalendarView>()) {
            t_calendarView = winrt::make_weak(cv);
        }
    }

    if (!t_headerOriginalTb.get()) {
        if (auto tb = root.try_as<wuxc::TextBlock>()) {
            std::wstring name = ElementName(tb);
            if (name != L"VnHeaderClock" && name != L"VnHeaderGreg" && name != L"VnHeaderLunar" && name != L"VnHeaderWeekBadge") {
                std::wstring text = ElementText(tb);
                if (IsFlyoutTopDateString(text)) {
                    t_headerOriginalTb = winrt::make_weak(tb);
                }
            }
        }
    }

    if (!t_weekDayNamesGrid.get()) {
        if (auto grid = root.try_as<wuxc::Grid>()) {
            if (ElementName(grid) == L"WeekDayNames") {
                t_weekDayNamesGrid = winrt::make_weak(grid);
            }
        }
    }

    int count = 0;
    try {
        count = wuxm::VisualTreeHelper::GetChildrenCount(root);
    } catch (...) {
        return;
    }

    for (int childIndex = 0; childIndex < count; childIndex++) {
        wux::DependencyObject child = nullptr;
        try {
            child = wuxm::VisualTreeHelper::GetChild(root, childIndex);
        } catch (...) {
            continue;
        }

        CollectDayItemsFromElement(child, budget, depth + 1);
    }
}

void CollectDayItemsOnCurrentThread() {
    wux::Window window = nullptr;
    try {
        window = wux::Window::Current();
    } catch (...) {
        return;
    }

    if (!window) {
        return;
    }

    try {
        size_t budget = kMaxScannedElements;

        if (auto content = window.Content()) {
            CollectDayItemsFromElement(content, budget, 0);
        }

        for (auto popup : wuxm::VisualTreeHelper::GetOpenPopups(window)) {
            if (budget == 0) {
                break;
            }
            if (auto child = popup.Child()) {
                CollectDayItemsFromElement(child, budget, 0);
            }
        }
    } catch (...) {
    }
}

void InitializeForCurrentThread();

void SweepDayItems(bool forceFullTreeScan) {
    InitializeForCurrentThread();

    if (forceFullTreeScan || t_dayItems.size() < 42 || !t_calendarView.get()) {
        CollectDayItemsOnCurrentThread();
    } else if (auto calView = t_calendarView.get()) {
        size_t budget = 1024;
        CollectDayItemsFromElement(calView, budget, 0);
    }

    for (size_t i = 0; i < t_dayItems.size();) {
        auto item = t_dayItems[i].item.get();
        if (!item) {
            t_dayItems.erase(t_dayItems.begin() + i);
            continue;
        }

        if (!t_calendarHeaderCompacted) {
            try {
                wux::DependencyObject curr = item;
                for (int d = 0; d < 16 && curr; ++d) {
                    if (auto calView = curr.try_as<wuxc::CalendarView>()) {
                        t_calendarView = winrt::make_weak(calView);
                        CompactMonthNavigationHeader(calView);

                        auto m = calView.Margin();
                        if (m.Bottom < 12.0) {
                            calView.Margin(wux::Thickness{m.Left, m.Top, m.Right, 12.0});
                        }
                        if (auto parentBorder = wuxm::VisualTreeHelper::GetParent(calView).try_as<wuxc::Border>()) {
                            auto p = parentBorder.Padding();
                            if (p.Bottom < 8.0) {
                                parentBorder.Padding(wux::Thickness{p.Left, p.Top, p.Right, 8.0});
                            }
                        }
                        t_calendarHeaderCompacted = true;
                        break;
                    }
                    curr = wuxm::VisualTreeHelper::GetParent(curr);
                }
            } catch (...) {
            }
        }

        if (!t_headerOriginalTb.get() || !t_weekDayNamesGrid.get() || !t_notificationCenterPanel.get() || !t_focusPanel.get()) {
            DiscoverElementsFromDayItem(item);
        }

        ApplyToDayItem(item);
        i++;
    }

    UpdateFlyoutHeaderClockAndLunar();

    for (auto it = t_cells.begin(); it != t_cells.end();) {
        if (it->second->item.get()) {
            ++it;
        } else {
            it = t_cells.erase(it);
        }
    }

    for (auto it = t_labelOwners.begin(); it != t_labelOwners.end();) {
        if (it->second.get()) {
            ++it;
        } else {
            it = t_labelOwners.erase(it);
        }
    }
}

void OnCalendarOpenedOnCurrentThread() {
    InitializeForCurrentThread();
    t_calendarHeaderCompacted = false;
    SweepDayItems(true);
    StartSweepTimer();
}

void UninitializeDayItemsForCurrentThread() {
    if (auto notifPanel = t_notificationCenterPanel.get()) {
        try {
            notifPanel.Visibility(wux::Visibility::Visible);
            notifPanel.Opacity(1.0);
            notifPanel.MaxHeight(std::numeric_limits<double>::infinity());
            notifPanel.Height(std::numeric_limits<double>::quiet_NaN());
        } catch (...) {
        }
    }

    if (auto focusPanel = t_focusPanel.get()) {
        try {
            focusPanel.Visibility(wux::Visibility::Visible);
            focusPanel.Opacity(1.0);
            focusPanel.MaxHeight(std::numeric_limits<double>::infinity());
            focusPanel.Height(std::numeric_limits<double>::quiet_NaN());
        } catch (...) {
        }
    }

    if (auto origTb = t_headerOriginalTb.get()) {
        try {
            origTb.Opacity(1.0);
            origTb.MaxHeight(std::numeric_limits<double>::infinity());
            origTb.Visibility(wux::Visibility::Visible);
        } catch (...) {
        }
    }

    for (auto& entry : t_cells) {
        DayCell& cell = *entry.second;
        auto item = cell.item.get();
        if (!item) {
            continue;
        }

        ApplyPersistentTodayCircle(item, false, 0);

        if (cell.dateChangedToken != 0) {
            try {
                item.UnregisterPropertyChangedCallback(
                    wuxc::CalendarViewDayItem::DateProperty(),
                    cell.dateChangedToken);
            } catch (...) {
            }
            cell.dateChangedToken = 0;
        }

        if (auto dayNumber = cell.dayNumberLabel.get()) {
            UnlockTextBlockColor(dayNumber);
            if (cell.hasSavedDayNumberRenderTransform) {
                try {
                    dayNumber.RenderTransform(cell.savedDayNumberRenderTransform);
                } catch (...) {
                }
            }
        }

        if (auto label = cell.label.get()) {
            UnlockTextBlockColor(label);
            if (cell.labelChangedToken) {
                try {
                    label.UnregisterPropertyChangedCallback(
                        wuxc::TextBlock::TextProperty(), cell.labelChangedToken);
                } catch (...) {
                }
                cell.labelChangedToken = 0;
            }
            try {
                if (cell.hasSavedText) {
                    label.Text(cell.savedText);
                }
                if (cell.hasSavedFontSize) {
                    label.FontSize(cell.savedFontSize);
                }
                if (cell.hasSavedRenderTransform) {
                    label.RenderTransform(cell.savedRenderTransform);
                }
            } catch (...) {
            }
        }
    }

    decltype(t_cells)().swap(t_cells);
    decltype(t_dayItems)().swap(t_dayItems);
    decltype(t_labelOwners)().swap(t_labelOwners);
    decltype(t_colorLockers)().swap(t_colorLockers);
    t_calendarHeaderCompacted = false;

    try {
        if (t_sweepTimer) {
            t_sweepTimer.Stop();
        }
    } catch (...) {
    }
    t_sweepTimerRevoker.revoke();
    t_sweepTimer = nullptr;
}

enum class Target {
    ShellExperienceHost,
    ShellHost,
};

Target g_target = Target::ShellExperienceHost;
std::atomic<bool> g_shuttingDown{false};
std::atomic<UINT> g_runFromWindowThreadViaPostMessageMsg{0};

UINT RunFromWindowThreadViaPostMessageMsg() {
    UINT message = g_runFromWindowThreadViaPostMessageMsg.load();
    if (!message) {
        message =
            RegisterWindowMessage(L"Windhawk_RunFromWindowThreadViaPostMessage_" WH_MOD_ID);
        g_runFromWindowThreadViaPostMessageMsg.store(message);
    }

    return message;
}

std::mutex g_pendingPostHooksMutex;
std::vector<HHOOK> g_pendingPostHooks;

void RememberPendingPostHook(HHOOK hook) {
    std::lock_guard<std::mutex> lock(g_pendingPostHooksMutex);
    g_pendingPostHooks.push_back(hook);
}

void ForgetPendingPostHook(HHOOK hook) {
    std::lock_guard<std::mutex> lock(g_pendingPostHooksMutex);
    for (auto it = g_pendingPostHooks.begin(); it != g_pendingPostHooks.end(); ++it) {
        if (*it == hook) {
            g_pendingPostHooks.erase(it);
            break;
        }
    }
}

void UnhookPendingPostHooks() {
    std::vector<HHOOK> hooks;
    {
        std::lock_guard<std::mutex> lock(g_pendingPostHooksMutex);
        hooks.swap(g_pendingPostHooks);
    }

    for (HHOOK hook : hooks) {
        UnhookWindowsHookEx(hook);
    }
}

void DrainPendingRunFromWindowThreadPostsOnCurrentThread() {
    const UINT message = g_runFromWindowThreadViaPostMessageMsg.load();
    if (!message) {
        return;
    }

    MSG msg;
    while (PeekMessage(&msg, nullptr, message, message, PM_REMOVE)) {
    }
}

void InitializeForCurrentThread() {
    if (g_initializedForThread) {
        return;
    }
    g_initializedForThread = true;
}

void UninitializeForCurrentThread() {
    DrainPendingRunFromWindowThreadPostsOnCurrentThread();

    if (!g_initializedForThread) {
        return;
    }

    UninitializeDayItemsForCurrentThread();
    g_initializedForThread = false;
}

using RunFromWindowThreadProc_t = void(WINAPI*)(PVOID parameter);

bool RunFromWindowThread(HWND hWnd,
                         RunFromWindowThreadProc_t proc,
                         PVOID procParam) {
    static const UINT runFromWindowThreadRegisteredMsg =
        RegisterWindowMessage(L"Windhawk_RunFromWindowThread_" WH_MOD_ID);

    struct RUN_FROM_WINDOW_THREAD_PARAM {
        RunFromWindowThreadProc_t proc;
        PVOID procParam;
    };

    DWORD dwThreadId = GetWindowThreadProcessId(hWnd, nullptr);
    if (dwThreadId == 0) {
        return false;
    }

    if (dwThreadId == GetCurrentThreadId()) {
        proc(procParam);
        return true;
    }

    HHOOK hook = SetWindowsHookEx(
        WH_CALLWNDPROC,
        [](int nCode, WPARAM wParam, LPARAM lParam) -> LRESULT {
            if (nCode == HC_ACTION) {
                const CWPSTRUCT* cwp = (const CWPSTRUCT*)lParam;
                if (cwp->message == runFromWindowThreadRegisteredMsg) {
                    RUN_FROM_WINDOW_THREAD_PARAM* param =
                        (RUN_FROM_WINDOW_THREAD_PARAM*)cwp->lParam;
                    param->proc(param->procParam);
                }
            }

            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        },
        nullptr, dwThreadId);
    if (!hook) {
        return false;
    }

    RUN_FROM_WINDOW_THREAD_PARAM param;
    param.proc = proc;
    param.procParam = procParam;
    SendMessage(hWnd, runFromWindowThreadRegisteredMsg, 0, (LPARAM)&param);

    UnhookWindowsHookEx(hook);

    return true;
}

bool RunFromWindowThreadViaPostMessage(HWND hWnd,
                                       RunFromWindowThreadProc_t proc,
                                       PVOID procParam) {
    struct RUN_FROM_WINDOW_THREAD_PARAM {
        RunFromWindowThreadProc_t proc;
        PVOID procParam;
        HHOOK hook;
    };

    const UINT message = RunFromWindowThreadViaPostMessageMsg();

    DWORD dwThreadId = GetWindowThreadProcessId(hWnd, nullptr);
    if (dwThreadId == 0) {
        return false;
    }

    HHOOK hook = SetWindowsHookEx(
        WH_GETMESSAGE,
        [](int nCode, WPARAM wParam, LPARAM lParam) -> LRESULT {
            if (nCode == HC_ACTION && wParam == PM_REMOVE) {
                MSG* msg = (MSG*)lParam;
                if (msg->message ==
                    g_runFromWindowThreadViaPostMessageMsg.load()) {
                    auto* param = (RUN_FROM_WINDOW_THREAD_PARAM*)msg->lParam;
                    if (param) {
                        ForgetPendingPostHook(param->hook);
                        param->proc(param->procParam);
                        UnhookWindowsHookEx(param->hook);
                        delete param;
                        msg = nullptr;
                    }
                }
            }

            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        },
        nullptr, dwThreadId);
    if (!hook) {
        return false;
    }

    auto* param = new RUN_FROM_WINDOW_THREAD_PARAM{
        .proc = proc,
        .procParam = procParam,
        .hook = hook,
    };
    RememberPendingPostHook(hook);
    if (!PostMessage(hWnd, message, 0, (LPARAM)param)) {
        ForgetPendingPostHook(hook);
        UnhookWindowsHookEx(hook);
        delete param;
        return false;
    }

    return true;
}

bool IsCalendarWindowClass(PCWSTR className) {
    switch (g_target) {
        case Target::ShellExperienceHost:
            return _wcsicmp(className, L"Windows.UI.Core.CoreWindow") == 0;

        case Target::ShellHost:
            return _wcsicmp(className, L"ControlCenterWindow") == 0;
    }

    return false;
}

bool g_isExplorerProcess = false;

void OnWindowCreated(HWND hWnd, LPCWSTR lpClassName, PCSTR) {
    if (g_shuttingDown || g_isExplorerProcess) {
        return;
    }

    BOOL bTextualClassName = ((ULONG_PTR)lpClassName & ~(ULONG_PTR)0xffff) != 0;
    if (!bTextualClassName || !IsCalendarWindowClass(lpClassName)) {
        return;
    }

    RunFromWindowThreadViaPostMessage(
        hWnd,
        [](PVOID) {
            InitializeForCurrentThread();
            OnCalendarOpenedOnCurrentThread();
        },
        nullptr);
}

using CreateWindowInBand_t = HWND(WINAPI*)(DWORD dwExStyle,
                                           LPCWSTR lpClassName,
                                           LPCWSTR lpWindowName,
                                           DWORD dwStyle,
                                           int X,
                                           int Y,
                                           int nWidth,
                                           int nHeight,
                                           HWND hWndParent,
                                           HMENU hMenu,
                                           HINSTANCE hInstance,
                                           PVOID lpParam,
                                           DWORD dwBand);
CreateWindowInBand_t CreateWindowInBand_Original;
HWND WINAPI CreateWindowInBand_Hook(DWORD dwExStyle,
                                    LPCWSTR lpClassName,
                                    LPCWSTR lpWindowName,
                                    DWORD dwStyle,
                                    int X,
                                    int Y,
                                    int nWidth,
                                    int nHeight,
                                    HWND hWndParent,
                                    HMENU hMenu,
                                    HINSTANCE hInstance,
                                    PVOID lpParam,
                                    DWORD dwBand) {
    HWND hWnd = CreateWindowInBand_Original(
        dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight,
        hWndParent, hMenu, hInstance, lpParam, dwBand);
    if (hWnd) {
        OnWindowCreated(hWnd, lpClassName, __FUNCTION__);
    }
    return hWnd;
}

using CreateWindowInBandEx_t = HWND(WINAPI*)(DWORD dwExStyle,
                                             LPCWSTR lpClassName,
                                             LPCWSTR lpWindowName,
                                             DWORD dwStyle,
                                             int X,
                                             int Y,
                                             int nWidth,
                                             int nHeight,
                                             HWND hWndParent,
                                             HMENU hMenu,
                                             HINSTANCE hInstance,
                                             PVOID lpParam,
                                             DWORD dwBand,
                                             DWORD dwTypeFlags);
CreateWindowInBandEx_t CreateWindowInBandEx_Original;
HWND WINAPI CreateWindowInBandEx_Hook(DWORD dwExStyle,
                                      LPCWSTR lpClassName,
                                      LPCWSTR lpWindowName,
                                      DWORD dwStyle,
                                      int X,
                                      int Y,
                                      int nWidth,
                                      int nHeight,
                                      HWND hWndParent,
                                      HMENU hMenu,
                                      HINSTANCE hInstance,
                                      PVOID lpParam,
                                      DWORD dwBand,
                                      DWORD dwTypeFlags) {
    HWND hWnd = CreateWindowInBandEx_Original(
        dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight,
        hWndParent, hMenu, hInstance, lpParam, dwBand, dwTypeFlags);
    if (hWnd) {
        OnWindowCreated(hWnd, lpClassName, __FUNCTION__);
    }
    return hWnd;
}

void OnWindowShown(HWND hWnd) {
    if (g_shuttingDown || g_isExplorerProcess) {
        return;
    }

    DWORD dwProcessId = 0;
    if (!hWnd || !GetWindowThreadProcessId(hWnd, &dwProcessId) ||
        dwProcessId != GetCurrentProcessId()) {
        return;
    }

    WCHAR szClassName[32];
    if (GetClassName(hWnd, szClassName, ARRAYSIZE(szClassName)) == 0 ||
        !IsCalendarWindowClass(szClassName)) {
        return;
    }

    RunFromWindowThreadViaPostMessage(
        hWnd, [](PVOID) { OnCalendarOpenedOnCurrentThread(); }, nullptr);
}

using ShowWindow_t = decltype(&ShowWindow);
ShowWindow_t ShowWindow_Original;

BOOL WINAPI ShowWindow_Hook(HWND hWnd, int nCmdShow) {
    const BOOL wasVisible = ShowWindow_Original(hWnd, nCmdShow);
    if (nCmdShow != SW_HIDE) {
        OnWindowShown(hWnd);
    }
    return wasVisible;
}

std::vector<HWND> GetCalendarWindows() {
    struct ENUM_WINDOWS_PARAM {
        std::vector<HWND>* hWnds;
    };

    std::vector<HWND> hWnds;
    ENUM_WINDOWS_PARAM param = {&hWnds};
    EnumWindows(
        [](HWND hWnd, LPARAM lParam) -> BOOL {
            ENUM_WINDOWS_PARAM& param = *(ENUM_WINDOWS_PARAM*)lParam;

            DWORD dwProcessId = 0;
            if (!GetWindowThreadProcessId(hWnd, &dwProcessId) ||
                dwProcessId != GetCurrentProcessId()) {
                return TRUE;
            }

            WCHAR szClassName[64];
            if (GetClassName(hWnd, szClassName, ARRAYSIZE(szClassName)) == 0) {
                return TRUE;
            }

            if (IsCalendarWindowClass(szClassName)) {
                param.hWnds->push_back(hWnd);
            }

            return TRUE;
        },
        (LPARAM)&param);

    return hWnds;
}

void RefreshCalendarOnUiThreads() {
    for (HWND hCoreWnd : GetCalendarWindows()) {
        RunFromWindowThread(
            hCoreWnd,
            [](PVOID) {
                t_calendarHeaderCompacted = false;
                SweepDayItems(true);
                StartSweepTimer();
            },
            nullptr);
    }
}

template <typename Prototype>
bool InstallHook(HMODULE module,
                 PCSTR functionName,
                 Prototype* hook,
                 Prototype** original) {
    if (!module) {
        return false;
    }
    Prototype* target =
        reinterpret_cast<Prototype*>(GetProcAddress(module, functionName));
    if (!target) {
        return false;
    }
    return WindhawkUtils::SetFunctionHook(target, hook, original);
}

using ClockStringFunction = LPVOID(WINAPI*)(LPVOID, LPVOID, LPVOID, LPVOID, LPVOID);
ClockStringFunction g_clockTooltipOriginal = nullptr;
ClockStringFunction g_clockTooltip2Original = nullptr;
ClockStringFunction g_clockTimeIconOriginal = nullptr;
ClockStringFunction g_clockTimeIcon2Original = nullptr;
ClockStringFunction g_clockDateIconOriginal = nullptr;
ClockStringFunction g_clockDateIcon2Original = nullptr;

using ClockRefreshFunction = void(WINAPI*)(LPVOID);
ClockRefreshFunction g_clockRefreshOriginal = nullptr;
ClockRefreshFunction g_clockRefresh2Original = nullptr;

std::atomic<LPVOID> g_lastClockModel1{nullptr};
std::atomic<LPVOID> g_lastClockModel2{nullptr};
std::atomic<bool> g_symbolHooksInstalled{false};

bool IsOtherTaskbarModActiveInExplorer() {
    static uint64_t lastGen = 0;
    static ULONGLONG lastCheckTick = 0;
    static bool cachedHasTaskbarMod = false;

    uint64_t curGen = g_settingsGeneration.load();
    ULONGLONG now = GetTickCount64();
    if (lastCheckTick != 0 && (now - lastCheckTick) < 3000 && lastGen == curGen) {
        return cachedHasTaskbarMod;
    }
    lastCheckTick = now;
    lastGen = curGen;

    bool foundTaskbarMod = false;

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (snap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W me{};
        me.dwSize = sizeof(me);
        if (Module32FirstW(snap, &me)) {
            do {
                std::wstring modName = me.szModule;
                for (wchar_t& c : modName) {
                    c = towlower(c);
                }

                std::wstring exePath = me.szExePath;
                for (wchar_t& c : exePath) {
                    c = towlower(c);
                }

                bool isWindhawkModDll =
                    (exePath.find(L"\\mods\\") != std::wstring::npos ||
                     modName.find(L"taskbar-") != std::wstring::npos ||
                     modName.find(L"windows-11-taskbar") != std::wstring::npos);

                if (isWindhawkModDll &&
                    modName.find(L"vietnamese-lunar-calendar") == std::wstring::npos) {
                    // 
                    if (modName.find(L"taskbar") != std::wstring::npos ||
                        modName.find(L"tray") != std::wstring::npos ||
                        modName.find(L"clock") != std::wstring::npos) {
                        foundTaskbarMod = true;
                        break;
                    }
                }
            } while (Module32NextW(snap, &me));
        }
        CloseHandle(snap);
    }

    if (!foundTaskbarMod) {
        HWND hTray = FindWindowW(L"Shell_TrayWnd", nullptr);
        if (hTray) {
            RECT rc{};
            if (GetWindowRect(hTray, &rc)) {
                UINT dpi = GetDpiForWindow(hTray);
                if (dpi == 0) dpi = 96;
                int logicalHeight = MulDiv(rc.bottom - rc.top, 96, dpi);
                if (logicalHeight > 0 && logicalHeight < 42) {
                    foundTaskbarMod = true;
                }
            }
        }
    }

    cachedHasTaskbarMod = foundTaskbarMod;
    return foundTaskbarMod;
}

bool ShouldUseModdedSingleElementMode() {
    auto settings = GetSettings();
    if (settings->taskbarMode == TaskbarDisplayMode::ModdedSingleElement) {
        return true;
    }
    if (settings->taskbarMode == TaskbarDisplayMode::DefaultTwoElements) {
        return false;
    }
    return IsOtherTaskbarModActiveInExplorer();
}

std::wstring ReadHStringSafe(LPVOID hstrPtrPtr) {
    if (!hstrPtrPtr) {
        return L"";
    }
    HSTRING hstr = *reinterpret_cast<HSTRING*>(hstrPtrPtr);
    if (!hstr) {
        return L"";
    }
    UINT32 len = 0;
    PCWSTR raw = WindowsGetStringRawBuffer(hstr, &len);
    if (!raw || len == 0 || len > 4096) {
        return L"";
    }
    return std::wstring(raw, len);
}

void ReplaceHStringSafe(LPVOID hstrPtrPtr, std::wstring const& newText) {
    if (!hstrPtrPtr) {
        return;
    }
    HSTRING* pHstr = reinterpret_cast<HSTRING*>(hstrPtrPtr);
    HSTRING newHstr = nullptr;
    HRESULT hr = WindowsCreateString(newText.c_str(),
                                     static_cast<UINT32>(newText.size()),
                                     &newHstr);
    if (SUCCEEDED(hr)) {
        if (*pHstr) {
            WindowsDeleteString(*pHstr);
        }
        *pHstr = newHstr;
    }
}

std::wstring BuildTaskbarLine1(SYSTEMTIME const& now) {
    static constexpr PCWSTR kDayOfWeek[] = {
        L"CN", L"T2", L"T3", L"T4", L"T5", L"T6", L"T7"};
    wchar_t formatted[64];
    swprintf(formatted, ARRAYSIZE(formatted), L"%02d:%02d - %s, %02d/%02d",
             now.wHour, now.wMinute,
             kDayOfWeek[now.wDayOfWeek % 7],
             now.wDay, now.wMonth);
    return formatted;
}

std::wstring BuildTaskbarLine2(SYSTEMTIME const& now) {
    return VietnameseLunarTooltipLabel(
        MakeDateKey(now.wYear, now.wMonth, now.wDay));
}

void AppendVietnameseLunarTooltip(LPVOID tooltipPtrPtr) {
    if (!tooltipPtrPtr || !GetSettings()->showLunarDate) {
        return;
    }

    SYSTEMTIME localTime{};
    GetLocalTime(&localTime);
    const std::wstring lunarLine = VietnameseLunarFullLabel(
        MakeDateKey(localTime.wYear, localTime.wMonth, localTime.wDay));
    if (lunarLine.empty()) {
        return;
    }

    std::wstring existing = ReadHStringSafe(tooltipPtrPtr);
    if (existing.empty() || existing.find(lunarLine) != std::wstring::npos) {
        return;
    }

    ReplaceHStringSafe(tooltipPtrPtr, existing + L"\r\n" + lunarLine);
}

LPVOID WINAPI ClockTooltipHook(LPVOID self, LPVOID a1, LPVOID a2, LPVOID a3, LPVOID a4) {
    g_lastClockModel1.store(self);
    LPVOID result = g_clockTooltipOriginal(self, a1, a2, a3, a4);
    AppendVietnameseLunarTooltip(result);
    return result;
}

LPVOID WINAPI ClockTooltip2Hook(LPVOID self, LPVOID a1, LPVOID a2, LPVOID a3, LPVOID a4) {
    g_lastClockModel2.store(self);
    LPVOID result = g_clockTooltip2Original(self, a1, a2, a3, a4);
    AppendVietnameseLunarTooltip(result);
    return result;
}

LPVOID WINAPI ClockTimeIconHook(LPVOID self, LPVOID a1, LPVOID a2, LPVOID a3, LPVOID a4) {
    g_lastClockModel1.store(self);
    LPVOID result = g_clockTimeIconOriginal(self, a1, a2, a3, a4);
    if (GetSettings()->showLunarDate) {
        SYSTEMTIME now{};
        GetLocalTime(&now);
        std::wstring line1 = BuildTaskbarLine1(now);
        if (ShouldUseModdedSingleElementMode()) {
            // Taskbar
            ReplaceHStringSafe(result, line1 + L"\r\n" + BuildTaskbarLine2(now));
        } else {
            // Default
            ReplaceHStringSafe(result, line1);
        }
    }
    return result;
}

LPVOID WINAPI ClockTimeIcon2Hook(LPVOID self, LPVOID a1, LPVOID a2, LPVOID a3, LPVOID a4) {
    g_lastClockModel2.store(self);
    LPVOID result = g_clockTimeIcon2Original(self, a1, a2, a3, a4);
    if (GetSettings()->showLunarDate) {
        SYSTEMTIME now{};
        GetLocalTime(&now);
        std::wstring line1 = BuildTaskbarLine1(now);
        if (ShouldUseModdedSingleElementMode()) {
            ReplaceHStringSafe(result, line1 + L"\r\n" + BuildTaskbarLine2(now));
        } else {
            ReplaceHStringSafe(result, line1);
        }
    }
    return result;
}

LPVOID WINAPI ClockDateIconHook(LPVOID self, LPVOID a1, LPVOID a2, LPVOID a3, LPVOID a4) {
    g_lastClockModel1.store(self);
    LPVOID result = g_clockDateIconOriginal(self, a1, a2, a3, a4);
    if (GetSettings()->showLunarDate) {
        if (ShouldUseModdedSingleElementMode()) {
            ReplaceHStringSafe(result, L"");
        } else {
            SYSTEMTIME now{};
            GetLocalTime(&now);
            ReplaceHStringSafe(result, BuildTaskbarLine2(now));
        }
    }
    return result;
}

LPVOID WINAPI ClockDateIcon2Hook(LPVOID self, LPVOID a1, LPVOID a2, LPVOID a3, LPVOID a4) {
    g_lastClockModel2.store(self);
    LPVOID result = g_clockDateIcon2Original(self, a1, a2, a3, a4);
    if (GetSettings()->showLunarDate) {
        if (ShouldUseModdedSingleElementMode()) {
            ReplaceHStringSafe(result, L"");
        } else {
            SYSTEMTIME now{};
            GetLocalTime(&now);
            ReplaceHStringSafe(result, BuildTaskbarLine2(now));
        }
    }
    return result;
}

using GetDateFormatEx_t = int(WINAPI*)(LPCWSTR, DWORD, const SYSTEMTIME*, LPCWSTR, LPWSTR, int, LPCWSTR);
GetDateFormatEx_t GetDateFormatEx_Original = nullptr;

using GetTimeFormatEx_t = int(WINAPI*)(LPCWSTR, DWORD, const SYSTEMTIME*, LPCWSTR, LPWSTR, int);
GetTimeFormatEx_t GetTimeFormatEx_Original = nullptr;

HMODULE g_systemTrayModule = nullptr;

bool IsCallerInSystemTray(void* returnAddress) {
    if (!returnAddress) {
        return false;
    }
    HMODULE callerMod = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(returnAddress), &callerMod)) {
        return false;
    }
    if (callerMod == g_systemTrayModule && g_systemTrayModule != nullptr) {
        return true;
    }
    WCHAR modPath[MAX_PATH];
    if (GetModuleFileNameW(callerMod, modPath, ARRAYSIZE(modPath)) > 0) {
        if (wcsstr(modPath, L"SystemTray.dll") ||
            wcsstr(modPath, L"Taskbar.View.dll") ||
            wcsstr(modPath, L"ExplorerExtensions.dll")) {
            return true;
        }
    }
    return false;
}

int WINAPI GetTimeFormatEx_Hook(LPCWSTR lpLocaleName,
                                DWORD dwFlags,
                                const SYSTEMTIME* lpTime,
                                LPCWSTR lpFormat,
                                LPWSTR lpTimeStr,
                                int cchTime) {

    if (!g_symbolHooksInstalled.load() && lpTime && GetSettings()->showLunarDate &&
        IsCallerInSystemTray(__builtin_return_address(0))) {
        SYSTEMTIME now{};
        GetLocalTime(&now);
        std::wstring text = ShouldUseModdedSingleElementMode()
                                ? (BuildTaskbarLine1(now) + L"\n" + BuildTaskbarLine2(now))
                                : BuildTaskbarLine1(now);
        int len = static_cast<int>(text.size()) + 1;
        if (cchTime == 0) {
            return len;
        }
        if (lpTimeStr && cchTime >= len) {
            std::memcpy(lpTimeStr, text.c_str(), len * sizeof(wchar_t));
            return len;
        }
    }
    return GetTimeFormatEx_Original(lpLocaleName, dwFlags, lpTime, lpFormat,
                                    lpTimeStr, cchTime);
}

int WINAPI GetDateFormatEx_Hook(LPCWSTR lpLocaleName,
                                DWORD dwFlags,
                                const SYSTEMTIME* lpDate,
                                LPCWSTR lpFormat,
                                LPWSTR lpDateStr,
                                int cchDate,
                                LPCWSTR lpCalendar) {
    if (!g_symbolHooksInstalled.load() && lpDate && GetSettings()->showLunarDate &&
        IsCallerInSystemTray(__builtin_return_address(0))) {
        std::wstring text = ShouldUseModdedSingleElementMode()
                                ? L""
                                : BuildTaskbarLine2(*lpDate);
        int len = static_cast<int>(text.size()) + 1;
        if (cchDate == 0) {
            return len;
        }
        if (lpDateStr && cchDate >= len) {
            std::memcpy(lpDateStr, text.c_str(), len * sizeof(wchar_t));
            return len;
        }
    }
    return GetDateFormatEx_Original(lpLocaleName, dwFlags, lpDate, lpFormat,
                                    lpDateStr, cchDate, lpCalendar);
}

bool HookTaskbarClockSymbols() {
    HMODULE module = GetModuleHandleW(L"SystemTray.dll");
    if (!module) {
        module = LoadLibraryW(L"SystemTray.dll");
    }
    if (!module) {
        module = GetModuleHandleW(L"Taskbar.View.dll");
    }
    g_systemTrayModule = module;
    if (!module) {
        return false;
    }

    WindhawkUtils::SYMBOL_HOOK hooks[] = {
        {{LR"(private: struct winrt::hstring __cdecl winrt::SystemTray::implementation::ClockSystemTrayIconDataModel::GetTimeToolTipString2(struct _SYSTEMTIME const &,struct _SYSTEMTIME const &,class SystemTrayTelemetry::ClockUpdate &))"},
         &g_clockTooltipOriginal, ClockTooltipHook, true},
        {{LR"(private: struct winrt::hstring __cdecl winrt::SystemTray::implementation::ClockSystemTrayIconDataModel2::GetTimeToolTipString2(struct _SYSTEMTIME const &,struct _SYSTEMTIME const &,class SystemTrayTelemetry::ClockUpdate &))"},
         &g_clockTooltip2Original, ClockTooltip2Hook, true},
        {{LR"(private: struct winrt::hstring __cdecl winrt::SystemTray::implementation::ClockSystemTrayIconDataModel::GetTimeIconString2(struct _SYSTEMTIME const &,class SystemTrayTelemetry::ClockUpdate &))"},
         &g_clockTimeIconOriginal, ClockTimeIconHook, true},
        {{LR"(private: struct winrt::hstring __cdecl winrt::SystemTray::implementation::ClockSystemTrayIconDataModel2::GetTimeIconString2(struct _SYSTEMTIME const &,class SystemTrayTelemetry::ClockUpdate &))"},
         &g_clockTimeIcon2Original, ClockTimeIcon2Hook, true},
        {{LR"(private: struct winrt::hstring __cdecl winrt::SystemTray::implementation::ClockSystemTrayIconDataModel::GetDateIconString2(struct _SYSTEMTIME const &,class SystemTrayTelemetry::ClockUpdate &))"},
         &g_clockDateIconOriginal, ClockDateIconHook, true},
        {{LR"(private: struct winrt::hstring __cdecl winrt::SystemTray::implementation::ClockSystemTrayIconDataModel2::GetDateIconString2(struct _SYSTEMTIME const &,class SystemTrayTelemetry::ClockUpdate &))"},
         &g_clockDateIcon2Original, ClockDateIcon2Hook, true},
        {{LR"(public: void __cdecl winrt::SystemTray::implementation::ClockSystemTrayIconDataModel::RefreshIconSystemTray(void))"},
         &g_clockRefreshOriginal, nullptr, true},
        {{LR"(public: void __cdecl winrt::SystemTray::implementation::ClockSystemTrayIconDataModel2::RefreshIconSystemTray(void))"},
         &g_clockRefresh2Original, nullptr, true},
    };

    bool ok = WindhawkUtils::HookSymbols(module, hooks, ARRAYSIZE(hooks));
    if (ok && (g_clockTimeIconOriginal || g_clockTimeIcon2Original)) {
        g_symbolHooksInstalled.store(true);
    }
    return ok;
}

void ForceRefreshTaskbarClockNow() {
    auto refreshOnTrayThread = [](HWND hTray) {
        if (!hTray) {
            return;
        }
        RunFromWindowThread(
            hTray,
            [](PVOID) {
                if (g_clockRefreshOriginal) {
                    if (LPVOID model1 = g_lastClockModel1.load()) {
                        try {
                            g_clockRefreshOriginal(model1);
                        } catch (...) {
                        }
                    }
                }
                if (g_clockRefresh2Original) {
                    if (LPVOID model2 = g_lastClockModel2.load()) {
                        try {
                            g_clockRefresh2Original(model2);
                        } catch (...) {
                        }
                    }
                }
            },
            nullptr);

        SendNotifyMessageW(hTray, WM_TIMECHANGE, 0, 0);
        SendNotifyMessageW(hTray, WM_SETTINGCHANGE, 0, reinterpret_cast<LPARAM>(L"intl"));
        EnumChildWindows(
            hTray,
            [](HWND hChild, LPARAM) -> BOOL {
                SendNotifyMessageW(hChild, WM_TIMECHANGE, 0, 0);
                SendNotifyMessageW(hChild, WM_SETTINGCHANGE, 0, reinterpret_cast<LPARAM>(L"intl"));
                return TRUE;
            },
            0);
    };

    HWND hPrimaryTray = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (hPrimaryTray) {
        refreshOnTrayThread(hPrimaryTray);
    }

    HWND hSecondaryTray = nullptr;
    while ((hSecondaryTray = FindWindowExW(nullptr, hSecondaryTray, L"Shell_SecondaryTrayWnd", nullptr)) != nullptr) {
        refreshOnTrayThread(hSecondaryTray);
    }

    SendNotifyMessageW(HWND_BROADCAST, WM_TIMECHANGE, 0, 0);
}

BOOL Wh_ModInit() {
    Wh_Log(L">");

    g_shuttingDown = false;
    g_isExplorerProcess = false;
    g_target = Target::ShellExperienceHost;

    WCHAR moduleFilePath[MAX_PATH];
    switch (GetModuleFileName(nullptr, moduleFilePath, ARRAYSIZE(moduleFilePath))) {
        case 0:
        case ARRAYSIZE(moduleFilePath):
            return FALSE;

        default:
            if (PCWSTR moduleFileName = wcsrchr(moduleFilePath, L'\\')) {
                moduleFileName++;
                if (_wcsicmp(moduleFileName, L"explorer.exe") == 0) {
                    g_isExplorerProcess = true;
                } else if (_wcsicmp(moduleFileName, L"ShellHost.exe") == 0) {
                    g_target = Target::ShellHost;
                }
            } else {
                return FALSE;
            }
            break;
    }

    LoadSettings();
    RunFromWindowThreadViaPostMessageMsg();

    if (g_isExplorerProcess) {
        HookTaskbarClockSymbols();
        HMODULE kernelbase = GetModuleHandleW(L"kernelbase.dll");
        if (kernelbase) {
            InstallHook(kernelbase, "GetTimeFormatEx", GetTimeFormatEx_Hook,
                        &GetTimeFormatEx_Original);
            InstallHook(kernelbase, "GetDateFormatEx", GetDateFormatEx_Hook,
                        &GetDateFormatEx_Original);
        }
        return TRUE;
    }

    HMODULE user32Module =
        LoadLibraryEx(L"user32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    InstallHook(user32Module, "CreateWindowInBand", CreateWindowInBand_Hook,
                &CreateWindowInBand_Original);
    InstallHook(user32Module, "CreateWindowInBandEx",
                CreateWindowInBandEx_Hook, &CreateWindowInBandEx_Original);
    InstallHook(user32Module, "ShowWindow", ShowWindow_Hook, &ShowWindow_Original);
    return TRUE;
}

void Wh_ModAfterInit() {
    Wh_Log(L">");

    if (g_isExplorerProcess) {
        ForceRefreshTaskbarClockNow();
        return;
    }

    for (auto hCoreWnd : GetCalendarWindows()) {
        RunFromWindowThread(
            hCoreWnd,
            [](PVOID) {
                InitializeForCurrentThread();
                OnCalendarOpenedOnCurrentThread();
            },
            nullptr);
    }
}

void Wh_ModUninit() {
    Wh_Log(L">");
    g_shuttingDown = true;

    if (g_isExplorerProcess) {
        ForceRefreshTaskbarClockNow();
        UnhookPendingPostHooks();
        return;
    }

    for (auto hCoreWnd : GetCalendarWindows()) {
        RunFromWindowThread(
            hCoreWnd, [](PVOID) { UninitializeForCurrentThread(); }, nullptr);
    }

    UnhookPendingPostHooks();
}

void Wh_ModSettingsChanged() {
    Wh_Log(L">");
    LoadSettings();

    if (g_isExplorerProcess) {
        ForceRefreshTaskbarClockNow();
        return;
    }

    RefreshCalendarOnUiThreads();
}
