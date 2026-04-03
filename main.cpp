#include <windows.h>
#include <windowsx.h>
#include <gdiplus.h>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ole32.lib")

#include "resource.h"

namespace
{
    enum class Category
    {
        Core,
        Resources,
        Rewards,
        Weapons
    };

    enum class ViewMode
    {
        Hotkeys,
        Options
    };

    enum class Language
    {
        English,
        Chinese
    };

    enum class FeatureMode
    {
        Toggle,
        Integer,
        Float
    };

    enum class HitKind
    {
        View,
        Language,
        Category,
        Feature,
        Reset,
        Minimize,
        Close
    };

    struct Shortcut
    {
        bool ctrl{};
        bool alt{};
        bool shift{};
        UINT key{};
    };

    struct Feature
    {
        Category category{};
        std::wstring label;
        std::wstring hotkey;
        std::wstring suffix;
        Shortcut shortcut{};
        FeatureMode mode{ FeatureMode::Toggle };
        bool enabled{};
        int intValue{};
        int minInt{};
        int maxInt{};
        int stepInt{};
        double floatValue{};
        double minFloat{};
        double maxFloat{};
        double stepFloat{};
    };

    struct HitTarget
    {
        RECT rect{};
        HitKind kind{};
        int index{};
    };

    struct AppState
    {
        std::vector<Feature> features;
        std::vector<HitTarget> hitTargets;
        Category selectedCategory{ Category::Core };
        ViewMode selectedView{ ViewMode::Hotkeys };
        Language selectedLanguage{ Language::English };
        int listScrollOffset{};
        std::wstring lastAction{ L"Ready." };
        Gdiplus::Image* posterImage{};
        Gdiplus::Image* contentBackgroundImage{};
        HFONT titleFont{};
        HFONT headerFont{};
        HFONT bodyFont{};
        HFONT smallFont{};
    };

    struct SplashState
    {
        Gdiplus::Image* posterImage{};
        Gdiplus::Image* backgroundImage{};
        ULONGLONG startTick{};
        HFONT titleFont{};
        HFONT bodyFont{};
        HFONT smallFont{};
    };

    constexpr COLORREF kWindowBackground = RGB(34, 34, 37);
    constexpr COLORREF kSidebarBackground = RGB(31, 31, 34);
    constexpr COLORREF kPanelBackground = RGB(36, 36, 39);
    constexpr COLORREF kPanelBackgroundAlt = RGB(41, 41, 45);
    constexpr COLORREF kDivider = RGB(68, 68, 74);
    constexpr COLORREF kAccent = RGB(232, 145, 35);
    constexpr COLORREF kAccentDark = RGB(132, 82, 22);
    constexpr COLORREF kTextPrimary = RGB(236, 236, 236);
    constexpr COLORREF kTextSecondary = RGB(164, 164, 168);
    constexpr COLORREF kToggleOn = RGB(229, 145, 35);
    constexpr COLORREF kToggleOff = RGB(104, 104, 110);

    HFONT CreateUiFont(int height, int weight)
    {
        return CreateFontW(
            -height,
            0,
            0,
            0,
            weight,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_OUTLINE_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            VARIABLE_PITCH,
            L"Segoe UI");
    }

    void FillRectColor(HDC hdc, const RECT& rect, COLORREF color)
    {
        HBRUSH brush = CreateSolidBrush(color);
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);
    }

    void DrawRectOutline(HDC hdc, const RECT& rect, COLORREF color)
    {
        HPEN pen = CreatePen(PS_SOLID, 1, color);
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);
    }

    void DrawLine(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color)
    {
        HPEN pen = CreatePen(PS_SOLID, 1, color);
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        MoveToEx(hdc, x1, y1, nullptr);
        LineTo(hdc, x2, y2);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);
    }

    void DrawTextLine(HDC hdc, HFONT font, COLORREF color, const RECT& rect, const std::wstring& text, UINT format)
    {
        const auto previousFont = static_cast<HFONT>(SelectObject(hdc, font));
        SetTextColor(hdc, color);
        SetBkMode(hdc, TRANSPARENT);
        DrawTextW(hdc, text.c_str(), -1, const_cast<RECT*>(&rect), format);
        SelectObject(hdc, previousFont);
    }

    std::wstring ReplaceToken(std::wstring text, const std::wstring& from, const std::wstring& to)
    {
        std::size_t position = 0;
        while ((position = text.find(from, position)) != std::wstring::npos)
        {
            text.replace(position, from.length(), to);
            position += to.length();
        }

        return text;
    }

    Gdiplus::Image* LoadImageResource(WORD resourceId)
    {
        HRSRC resource = FindResourceW(nullptr, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
        if (resource == nullptr)
        {
            return nullptr;
        }

        HGLOBAL loadedResource = LoadResource(nullptr, resource);
        if (loadedResource == nullptr)
        {
            return nullptr;
        }

        const DWORD resourceSize = SizeofResource(nullptr, resource);
        const void* resourceData = LockResource(loadedResource);
        if (resourceData == nullptr || resourceSize == 0)
        {
            return nullptr;
        }

        HGLOBAL hBuffer = GlobalAlloc(GMEM_MOVEABLE, resourceSize);
        if (hBuffer == nullptr)
        {
            return nullptr;
        }
        void* pBuffer = GlobalLock(hBuffer);
        if (pBuffer == nullptr)
        {
            GlobalFree(hBuffer);
            return nullptr;
        }
        memcpy(pBuffer, resourceData, resourceSize);
        GlobalUnlock(hBuffer);

        IStream* stream = nullptr;
        if (FAILED(CreateStreamOnHGlobal(hBuffer, TRUE, &stream)))
        {
            GlobalFree(hBuffer);
            return nullptr;
        }

        auto* image = Gdiplus::Image::FromStream(stream, FALSE);
        stream->Release();
        if (image == nullptr || image->GetLastStatus() != Gdiplus::Ok)
        {
            delete image;
            return nullptr;
        }

        return image;
    }

    Gdiplus::Image* LoadPosterImage()
    {
        return LoadImageResource(IDR_POSTER_IMAGE);
    }

    Gdiplus::Image* LoadContentBackgroundImage()
    {
        return LoadImageResource(IDR_CONTENT_BACKGROUND);
    }

    std::wstring Localize(Language language, const wchar_t* english, const wchar_t* chinese)
    {
        return language == Language::Chinese ? chinese : english;
    }

    std::wstring FormatFloat(double value, const std::wstring& suffix)
    {
        std::wostringstream stream;
        stream << std::fixed << std::setprecision(2) << value << suffix;
        return stream.str();
    }

    std::wstring StatusText(const Feature& feature, Language language)
    {
        switch (feature.mode)
        {
        case FeatureMode::Toggle:
            return feature.enabled ? Localize(language, L"ON", L"开") : Localize(language, L"OFF", L"关");
        case FeatureMode::Integer:
            return std::to_wstring(feature.intValue) + feature.suffix;
        case FeatureMode::Float:
            return FormatFloat(feature.floatValue, feature.suffix);
        }

        return L"";
    }

    std::wstring LocalizedFeatureLabel(Language language, const Feature& feature, int featureIndex)
    {
        if (language == Language::English)
        {
            return feature.label;
        }

        static const wchar_t* chineseLabels[] = {
            L"无限生命",
            L"玩家生命不减",
            L"艾露猫生命不减",
            L"无限耐力",
            L"武器斩味提升",
            L"无限投射器弹药",
            L"高暴击率",
            L"易于眩晕 / 硬直 / 击倒",
            L"部位瞬间破坏",
            L"瞬间击杀",
            L"伤害倍率",
            L"防御倍率",
            L"食物效果持续时间无限",
            L"自动恢复生命",
            L"耐力消耗倍率",
            L"修改金钱",
            L"修改公会点数",
            L"拾取数量倍率",
            L"修改道具箱数量",
            L"背包物品不减少",
            L"无视背包调合条件",
            L"无视装备制作条件",
            L"解锁全部装备蓝图",
            L"无限剥取",
            L"无限环境采集",
            L"游戏速度",
            L"冻结昼夜时间",
            L"时间 +1 小时",
            L"斗篷：立即冷却",
            L"斗篷：持续时间无限",
            L"无限角色编辑券",
            L"无限艾露猫编辑券",
            L"无限幸运券",
            L"更多任务奖励",
            L"修改装饰品数量",
            L"装饰品熟练度最大",
            L"移动速度",
            L"大剑：快速蓄力",
            L"太刀：高气刃槽",
            L"双刀：高鬼人槽",
            L"锤：快速蓄力",
            L"长枪：突进攻击快速蓄力",
            L"长枪：反击快速蓄力",
            L"铳枪：炮弹无限",
            L"铳枪：龙杭炮无限",
            L"铳枪：全弹发射快速蓄力",
            L"斩斧：能量无限",
            L"斩斧：进入强化状态",
            L"盾斧：强化持续时间无限",
            L"操虫棍：强化持续时间无限",
            L"轻弩：无需装填",
            L"轻弩：特殊弹药无限",
            L"轻弩：速射槽高",
            L"轻弩：聚焦爆破弹无限",
            L"轻弩：聚焦爆破快速充能",
            L"重弩：无需装填",
            L"重弩：聚焦爆破弹无限",
            L"重弩：点火槽高",
            L"弓：快速蓄力",
            L"弓：涂层无限",
            L"弓：巧射槽高"
        };

        if (featureIndex >= 0 && featureIndex < static_cast<int>(std::size(chineseLabels)))
        {
            return chineseLabels[featureIndex];
        }

        return feature.label;
    }

    std::wstring CategoryMenuLabel(Category category, Language language)
    {
        switch (category)
        {
        case Category::Core:
            return Localize(language, L"Core", L"核心");
        case Category::Resources:
            return Localize(language, L"Resource", L"资源");
        case Category::Rewards:
            return Localize(language, L"Rewards", L"奖励");
        case Category::Weapons:
            return Localize(language, L"Weapons", L"武器");
        }

        return L"";
    }

    std::wstring DisplayHotkey(const std::wstring& hotkey)
    {
        auto display = ReplaceToken(hotkey, L"Numpad", L"NUM");
        display = ReplaceToken(display, L"Num ", L"NUM ");
        display = ReplaceToken(display, L"Ctrl+", L"CTRL+");
        display = ReplaceToken(display, L"Alt+", L"ALT+");
        display = ReplaceToken(display, L"Shift+", L"SHIFT+");
        return display;
    }

    void DrawToggleSwitch(HDC hdc, const RECT& rect, bool enabled)
    {
        const Gdiplus::Color trackColor = enabled
            ? Gdiplus::Color(235, 214, 138, 48)
            : Gdiplus::Color(210, 122, 122, 128);
        const Gdiplus::Color knobColor(245, 244, 244, 244);
        const Gdiplus::Color knobShadow(70, 0, 0, 0);

        Gdiplus::Graphics graphics(hdc);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

        const int width = rect.right - rect.left;
        const int height = rect.bottom - rect.top;
        const int radius = height;

        Gdiplus::GraphicsPath trackPath;
        trackPath.AddArc(rect.left, rect.top, radius, radius, 90.0f, 180.0f);
        trackPath.AddArc(rect.right - radius, rect.top, radius, radius, 270.0f, 180.0f);
        trackPath.CloseFigure();

        Gdiplus::SolidBrush trackBrush(trackColor);
        graphics.FillPath(&trackBrush, &trackPath);

        RECT knob = rect;
        knob.top += 3;
        knob.bottom -= 3;
        const int knobSize = knob.bottom - knob.top;
        if (enabled)
        {
            knob.left = rect.right - knobSize - 3;
            knob.right = rect.right - 3;
        }
        else
        {
            knob.left = rect.left + 3;
            knob.right = rect.left + knobSize + 3;
        }

        Gdiplus::SolidBrush shadowBrush(knobShadow);
        graphics.FillEllipse(&shadowBrush, static_cast<INT>(knob.left), static_cast<INT>(knob.top + 1), static_cast<INT>(knob.right - knob.left), static_cast<INT>(knob.bottom - knob.top));

        Gdiplus::SolidBrush knobBrush(knobColor);
        graphics.FillEllipse(&knobBrush, static_cast<INT>(knob.left), static_cast<INT>(knob.top), static_cast<INT>(knob.right - knob.left), static_cast<INT>(knob.bottom - knob.top));
    }

    void DrawValueBox(HDC hdc, const RECT& rect, const std::wstring& value, HFONT font)
    {
        DrawRectOutline(hdc, rect, kAccentDark);
        DrawTextLine(hdc, font, kAccent, rect, L"<   " + value + L"   >", DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    void DrawImageCover(HDC hdc, Gdiplus::Image* image, const RECT& rect)
    {
        if (image == nullptr)
        {
            return;
        }

        const int targetWidth = rect.right - rect.left;
        const int targetHeight = rect.bottom - rect.top;
        const double imageWidth = static_cast<double>(image->GetWidth());
        const double imageHeight = static_cast<double>(image->GetHeight());
        const double targetWidthDouble = static_cast<double>(targetWidth);
        const double targetHeightDouble = static_cast<double>(targetHeight);
        const double scale = (targetWidthDouble / imageWidth) > (targetHeightDouble / imageHeight)
            ? (targetWidthDouble / imageWidth)
            : (targetHeightDouble / imageHeight);

        const double sourceWidth = targetWidthDouble / scale;
        const double sourceHeight = targetHeightDouble / scale;
        const double sourceX = (imageWidth - sourceWidth) * 0.5;
        const double sourceY = (imageHeight - sourceHeight) * 0.5;

        Gdiplus::Graphics graphics(hdc);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
        graphics.DrawImage(
            image,
            Gdiplus::Rect(rect.left, rect.top, targetWidth, targetHeight),
            static_cast<INT>(sourceX),
            static_cast<INT>(sourceY),
            static_cast<INT>(sourceWidth),
            static_cast<INT>(sourceHeight),
            Gdiplus::UnitPixel);
    }

    void DrawImageCover(HDC hdc, Gdiplus::Image* image, const RECT& rect, float alpha)
    {
        if (image == nullptr)
        {
            return;
        }

        const int targetWidth = rect.right - rect.left;
        const int targetHeight = rect.bottom - rect.top;
        const double imageWidth = static_cast<double>(image->GetWidth());
        const double imageHeight = static_cast<double>(image->GetHeight());
        const double targetWidthDouble = static_cast<double>(targetWidth);
        const double targetHeightDouble = static_cast<double>(targetHeight);
        const double scale = (targetWidthDouble / imageWidth) > (targetHeightDouble / imageHeight)
            ? (targetWidthDouble / imageWidth)
            : (targetHeightDouble / imageHeight);

        const double sourceWidth = targetWidthDouble / scale;
        const double sourceHeight = targetHeightDouble / scale;
        const double sourceX = (imageWidth - sourceWidth) * 0.5;
        const double sourceY = (imageHeight - sourceHeight) * 0.5;

        Gdiplus::Graphics graphics(hdc);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);

        Gdiplus::ImageAttributes imageAttributes;
        Gdiplus::ColorMatrix matrix = {
            1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, alpha, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 1.0f
        };
        imageAttributes.SetColorMatrix(&matrix);

        graphics.DrawImage(
            image,
            Gdiplus::Rect(rect.left, rect.top, targetWidth, targetHeight),
            static_cast<INT>(sourceX),
            static_cast<INT>(sourceY),
            static_cast<INT>(sourceWidth),
            static_cast<INT>(sourceHeight),
            Gdiplus::UnitPixel,
            &imageAttributes);
    }

    void FillRoundedRectGdiPlus(HDC hdc, const RECT& rect, Gdiplus::Color color)
    {
        Gdiplus::Graphics graphics(hdc);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

        const int width = rect.right - rect.left;
        const int height = rect.bottom - rect.top;
        const int radius = height;

        Gdiplus::GraphicsPath path;
        path.AddArc(rect.left, rect.top, radius, radius, 90.0f, 180.0f);
        path.AddArc(rect.right - radius, rect.top, radius, radius, 270.0f, 180.0f);
        path.CloseFigure();

        Gdiplus::SolidBrush brush(color);
        graphics.FillPath(&brush, &path);
    }

    Shortcut MakeShortcut(bool ctrl, bool alt, bool shift, UINT key)
    {
        return Shortcut{ ctrl, alt, shift, key };
    }

    Feature MakeToggle(Category category, const wchar_t* hotkey, const wchar_t* label, Shortcut shortcut)
    {
        Feature feature;
        feature.category = category;
        feature.hotkey = hotkey;
        feature.label = label;
        feature.shortcut = shortcut;
        feature.mode = FeatureMode::Toggle;
        return feature;
    }

    Feature MakeInteger(Category category, const wchar_t* hotkey, const wchar_t* label, Shortcut shortcut, int initialValue, int minValue, int maxValue, int stepValue, const wchar_t* suffix = L"")
    {
        Feature feature;
        feature.category = category;
        feature.hotkey = hotkey;
        feature.label = label;
        feature.shortcut = shortcut;
        feature.mode = FeatureMode::Integer;
        feature.intValue = initialValue;
        feature.minInt = minValue;
        feature.maxInt = maxValue;
        feature.stepInt = stepValue;
        feature.suffix = suffix;
        return feature;
    }

    Feature MakeFloat(Category category, const wchar_t* hotkey, const wchar_t* label, Shortcut shortcut, double initialValue, double minValue, double maxValue, double stepValue, const wchar_t* suffix)
    {
        Feature feature;
        feature.category = category;
        feature.hotkey = hotkey;
        feature.label = label;
        feature.shortcut = shortcut;
        feature.mode = FeatureMode::Float;
        feature.floatValue = initialValue;
        feature.minFloat = minValue;
        feature.maxFloat = maxValue;
        feature.stepFloat = stepValue;
        feature.suffix = suffix;
        return feature;
    }

    std::vector<Feature> BuildFeatures()
    {
        using enum Category;

        return {
            MakeToggle(Core, L"Numpad 1", L"Infinite Health", MakeShortcut(false, false, false, VK_NUMPAD1)),
            MakeToggle(Core, L"Numpad 2", L"Unlimited Player Health", MakeShortcut(false, false, false, VK_NUMPAD2)),
            MakeToggle(Core, L"Numpad 3", L"Unlimited Palico Health", MakeShortcut(false, false, false, VK_NUMPAD3)),
            MakeToggle(Core, L"Numpad 4", L"Infinite Stamina", MakeShortcut(false, false, false, VK_NUMPAD4)),
            MakeToggle(Core, L"Numpad 5", L"Enhanced Weapon Sharpness", MakeShortcut(false, false, false, VK_NUMPAD5)),
            MakeToggle(Core, L"Numpad 6", L"Unlimited Slinger Ammo", MakeShortcut(false, false, false, VK_NUMPAD6)),
            MakeToggle(Core, L"Numpad 7", L"High Critical Hit Chance", MakeShortcut(false, false, false, VK_NUMPAD7)),
            MakeToggle(Core, L"Numpad 8", L"Easy Stun / Flinch / Knockdown", MakeShortcut(false, false, false, VK_NUMPAD8)),
            MakeToggle(Core, L"Numpad 9", L"Instant Part Break", MakeShortcut(false, false, false, VK_NUMPAD9)),
            MakeToggle(Core, L"Numpad 0", L"Instant Kill", MakeShortcut(false, false, false, VK_NUMPAD0)),
            MakeFloat(Core, L"Numpad .", L"Damage Multiplier", MakeShortcut(false, false, false, VK_DECIMAL), 1.00, 1.00, 5.00, 0.50, L"x"),
            MakeFloat(Core, L"Numpad +", L"Defense Multiplier", MakeShortcut(false, false, false, VK_ADD), 1.00, 1.00, 4.00, 0.25, L"x"),
            MakeToggle(Core, L"Numpad /", L"Unlimited Meal Effect Duration", MakeShortcut(false, false, false, VK_DIVIDE)),
            MakeToggle(Core, L"Numpad *", L"Auto Health Recovery", MakeShortcut(false, false, false, VK_MULTIPLY)),
            MakeFloat(Core, L"Numpad -", L"Stamina Consumption Rate", MakeShortcut(false, false, false, VK_SUBTRACT), 1.00, 0.00, 1.00, 0.25, L"x"),

            MakeInteger(Resources, L"Ctrl+Numpad 1", L"Edit Money", MakeShortcut(true, false, false, VK_NUMPAD1), 50000, 50000, 500000, 25000),
            MakeInteger(Resources, L"Ctrl+Numpad 2", L"Edit Guild Points", MakeShortcut(true, false, false, VK_NUMPAD2), 2500, 2500, 20000, 500),
            MakeInteger(Resources, L"Ctrl+Numpad 3", L"Pickup Amount Multiplier", MakeShortcut(true, false, false, VK_NUMPAD3), 1, 1, 10, 1, L"x"),
            MakeInteger(Resources, L"Ctrl+Numpad 4", L"Edit Item Box Amount", MakeShortcut(true, false, false, VK_NUMPAD4), 99, 99, 999, 100),
            MakeToggle(Resources, L"Ctrl+Numpad 5", L"Bag Items Do Not Decrease", MakeShortcut(true, false, false, VK_NUMPAD5)),
            MakeToggle(Resources, L"Ctrl+Numpad 6", L"Ignore Crafting Requirements In Bag", MakeShortcut(true, false, false, VK_NUMPAD6)),
            MakeToggle(Resources, L"Ctrl+Numpad 7", L"Ignore Equipment Crafting Requirements", MakeShortcut(true, false, false, VK_NUMPAD7)),
            MakeToggle(Resources, L"Ctrl+Numpad 8", L"Unlock All Equipment Blueprints", MakeShortcut(true, false, false, VK_NUMPAD8)),
            MakeToggle(Resources, L"Ctrl+Numpad 9", L"Unlimited Carve Pickup", MakeShortcut(true, false, false, VK_NUMPAD9)),
            MakeToggle(Resources, L"Ctrl+Numpad 0", L"Unlimited Environment Pickup", MakeShortcut(true, false, false, VK_NUMPAD0)),
            MakeFloat(Resources, L"Ctrl+Numpad .", L"Game Speed", MakeShortcut(true, false, false, VK_DECIMAL), 1.00, 0.50, 3.00, 0.25, L"x"),
            MakeToggle(Resources, L"Ctrl+Numpad /", L"Freeze Time Of Day", MakeShortcut(true, false, false, VK_DIVIDE)),
            MakeInteger(Resources, L"Ctrl+Numpad *", L"Time Of Day +1 Hour", MakeShortcut(true, false, false, VK_MULTIPLY), 12, 0, 23, 1, L":00"),

            MakeToggle(Rewards, L"Alt+Num 1", L"Mantles: Instant Cooldown", MakeShortcut(false, true, false, VK_NUMPAD1)),
            MakeToggle(Rewards, L"Alt+Num 2", L"Mantles: Infinite Duration", MakeShortcut(false, true, false, VK_NUMPAD2)),
            MakeToggle(Rewards, L"Alt+Num 3", L"Unlimited Character Edit Vouchers", MakeShortcut(false, true, false, VK_NUMPAD3)),
            MakeToggle(Rewards, L"Alt+Num 4", L"Unlimited Palico Edit Vouchers", MakeShortcut(false, true, false, VK_NUMPAD4)),
            MakeToggle(Rewards, L"Alt+Num 5", L"Unlimited Lucky Vouchers", MakeShortcut(false, true, false, VK_NUMPAD5)),
            MakeInteger(Rewards, L"Alt+Num 6", L"More Quest Reward Items", MakeShortcut(false, true, false, VK_NUMPAD6), 1, 1, 8, 1, L"x"),
            MakeInteger(Rewards, L"Alt+Num 7", L"Edit Decoration Amount", MakeShortcut(false, true, false, VK_NUMPAD7), 1, 1, 30, 1),
            MakeToggle(Rewards, L"Alt+Num 8", L"Max Decoration Mastery", MakeShortcut(false, true, false, VK_NUMPAD8)),
            MakeFloat(Rewards, L"Alt+Num 9", L"Movement Speed", MakeShortcut(false, true, false, VK_NUMPAD9), 1.00, 1.00, 3.00, 0.25, L"x"),

            MakeToggle(Weapons, L"Ctrl+F1", L"Great Sword: Fast Charge", MakeShortcut(true, false, false, VK_F1)),
            MakeToggle(Weapons, L"Ctrl+F2", L"Long Sword: High Spirit Gauge", MakeShortcut(true, false, false, VK_F2)),
            MakeToggle(Weapons, L"Ctrl+F3", L"Dual Blades: High Demon Gauge", MakeShortcut(true, false, false, VK_F3)),
            MakeToggle(Weapons, L"Ctrl+F4", L"Hammer: Fast Charge", MakeShortcut(true, false, false, VK_F4)),
            MakeToggle(Weapons, L"Ctrl+F5", L"Lance: Fast Dash Attack Charge", MakeShortcut(true, false, false, VK_F5)),
            MakeToggle(Weapons, L"Ctrl+F6", L"Lance: Fast Counter Charge", MakeShortcut(true, false, false, VK_F6)),
            MakeToggle(Weapons, L"Ctrl+F7", L"Gunlance: Infinite Shells", MakeShortcut(true, false, false, VK_F7)),
            MakeToggle(Weapons, L"Ctrl+F8", L"Gunlance: Infinite Wyrmstake", MakeShortcut(true, false, false, VK_F8)),
            MakeToggle(Weapons, L"Ctrl+F9", L"Gunlance: Fast Full Burst Charge", MakeShortcut(true, false, false, VK_F9)),
            MakeToggle(Weapons, L"Ctrl+F10", L"Switch Axe: Infinite Energy", MakeShortcut(true, false, false, VK_F10)),
            MakeToggle(Weapons, L"Ctrl+F11", L"Switch Axe: Enter Amped State", MakeShortcut(true, false, false, VK_F11)),
            MakeToggle(Weapons, L"Ctrl+F12", L"Charge Blade: Infinite Buff Duration", MakeShortcut(true, false, false, VK_F12)),
            MakeToggle(Weapons, L"Shift+F1", L"Insect Glaive: Infinite Buff Duration", MakeShortcut(false, false, true, VK_F1)),
            MakeToggle(Weapons, L"Shift+F2", L"Light Bowgun: No Reload", MakeShortcut(false, false, true, VK_F2)),
            MakeToggle(Weapons, L"Shift+F3", L"Light Bowgun: Infinite Special Ammo", MakeShortcut(false, false, true, VK_F3)),
            MakeToggle(Weapons, L"Shift+F4", L"Light Bowgun: High Rapid Fire Gauge", MakeShortcut(false, false, true, VK_F4)),
            MakeToggle(Weapons, L"Shift+F5", L"Light Bowgun: Infinite Focus Blast Ammo", MakeShortcut(false, false, true, VK_F5)),
            MakeToggle(Weapons, L"Shift+F6", L"Light Bowgun: Fast Focus Blast Charge", MakeShortcut(false, false, true, VK_F6)),
            MakeToggle(Weapons, L"Shift+F7", L"Heavy Bowgun: No Reload", MakeShortcut(false, false, true, VK_F7)),
            MakeToggle(Weapons, L"Shift+F8", L"Heavy Bowgun: Infinite Focus Blast Ammo", MakeShortcut(false, false, true, VK_F8)),
            MakeToggle(Weapons, L"Shift+F9", L"Heavy Bowgun: High Ignition Gauge", MakeShortcut(false, false, true, VK_F9)),
            MakeToggle(Weapons, L"Shift+F10", L"Bow: Fast Charge", MakeShortcut(false, false, true, VK_F10)),
            MakeToggle(Weapons, L"Shift+F11", L"Bow: Infinite Coating", MakeShortcut(false, false, true, VK_F11)),
            MakeToggle(Weapons, L"Shift+F12", L"Bow: High Trick Arrow Gauge", MakeShortcut(false, false, true, VK_F12))
        };
    }

    std::vector<int> VisibleFeatureIndices(const AppState& state)
    {
        std::vector<int> indices;
        for (int i = 0; i < static_cast<int>(state.features.size()); ++i)
        {
            if (state.features[i].category == state.selectedCategory)
            {
                indices.push_back(i);
            }
        }

        return indices;
    }

    int CalculateClientHeight(const AppState& state)
    {
        (void)state;
        return 760;
    }

    int GetListTop()
    {
        return 170;
    }

    int GetListBottom(const RECT& client)
    {
        return client.bottom - 18;
    }

    int GetListViewportHeight(const RECT& client)
    {
        return GetListBottom(client) - GetListTop();
    }

    int GetListContentHeight(const AppState& state)
    {
        return static_cast<int>(VisibleFeatureIndices(state).size()) * 34;
    }

    void UpdateListScrollBar(HWND hwnd, AppState& state)
    {
        RECT client{};
        GetClientRect(hwnd, &client);

        const int viewportHeight = GetListViewportHeight(client);
        const int contentHeight = GetListContentHeight(state);
        const int maxOffset = contentHeight > viewportHeight ? contentHeight - viewportHeight : 0;

        if (state.listScrollOffset < 0)
        {
            state.listScrollOffset = 0;
        }
        else if (state.listScrollOffset > maxOffset)
        {
            state.listScrollOffset = maxOffset;
        }

        SCROLLINFO scrollInfo{};
        scrollInfo.cbSize = sizeof(scrollInfo);
        scrollInfo.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        scrollInfo.nMin = 0;
        scrollInfo.nMax = contentHeight > 0 ? contentHeight - 1 : 0;
        scrollInfo.nPage = viewportHeight > 0 ? static_cast<UINT>(viewportHeight) : 0;
        scrollInfo.nPos = state.listScrollOffset;
        SetScrollInfo(hwnd, SB_VERT, &scrollInfo, TRUE);
    }

    void AdjustWindowToContent(HWND hwnd, const AppState& state)
    {
        AppState& mutableState = const_cast<AppState&>(state);
        RECT clientRect{};
        GetClientRect(hwnd, &clientRect);

        const int clientWidth = clientRect.right - clientRect.left;
        const int newClientHeight = CalculateClientHeight(state);

        RECT adjusted{};
        adjusted.left = 0;
        adjusted.top = 0;
        adjusted.right = clientWidth;
        adjusted.bottom = newClientHeight;
        AdjustWindowRectEx(&adjusted, static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE)), FALSE, static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE)));

        SetWindowPos(
            hwnd,
            nullptr,
            0,
            0,
            adjusted.right - adjusted.left,
            adjusted.bottom - adjusted.top,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

        UpdateListScrollBar(hwnd, mutableState);
    }

    bool ShortcutMatches(const Shortcut& shortcut, UINT vk)
    {
        const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
        const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

        return shortcut.key == vk && shortcut.ctrl == ctrl && shortcut.alt == alt && shortcut.shift == shift;
    }

    std::wstring MakeActionMessage(const AppState& state, int featureIndex)
    {
        const Feature& feature = state.features[featureIndex];
        return feature.hotkey + L" -> " + LocalizedFeatureLabel(state.selectedLanguage, feature, featureIndex) + L": " + StatusText(feature, state.selectedLanguage);
    }

    void ActivateFeature(AppState& state, int featureIndex)
    {
        Feature& feature = state.features[featureIndex];

        switch (feature.mode)
        {
        case FeatureMode::Toggle:
            feature.enabled = !feature.enabled;
            break;
        case FeatureMode::Integer:
            feature.intValue += feature.stepInt;
            if (feature.intValue > feature.maxInt)
            {
                feature.intValue = feature.minInt;
            }
            break;
        case FeatureMode::Float:
            feature.floatValue += feature.stepFloat;
            if (feature.floatValue > feature.maxFloat + 0.001)
            {
                feature.floatValue = feature.minFloat;
            }
            break;
        }

        state.lastAction = MakeActionMessage(state, featureIndex);
    }

    void ResetFeatures(AppState& state)
    {
        state.features = BuildFeatures();
        state.lastAction = Localize(state.selectedLanguage, L"All values have been reset.", L"所有数值已重置。");
    }

    bool PointInRect(const RECT& rect, POINT point)
    {
        return point.x >= rect.left && point.x < rect.right && point.y >= rect.top && point.y < rect.bottom;
    }

    std::wstring CategoryTitle(Category category, Language language)
    {
        switch (category)
        {
        case Category::Core:
            return Localize(language, L"Core Features", L"核心功能");
        case Category::Resources:
            return Localize(language, L"Resources & Time", L"资源与时间");
        case Category::Rewards:
            return Localize(language, L"Rewards & Utility", L"奖励与实用项");
        case Category::Weapons:
            return Localize(language, L"Weapon Presets", L"武器预设");
        }

        return L"";
    }

    std::wstring CategorySubtitle(Category category, Language language)
    {
        switch (category)
        {
        case Category::Core:
            return L"Numpad 1..0, ., +, /, *, -";
        case Category::Resources:
            return Localize(language, L"Ctrl + Numpad commands", L"Ctrl + 小键盘命令");
        case Category::Rewards:
            return Localize(language, L"Alt + Numpad commands", L"Alt + 小键盘命令");
        case Category::Weapons:
            return L"Ctrl/Shift + F1..F12";
        }

        return L"";
    }

    RECT MakeRect(int left, int top, int right, int bottom)
    {
        RECT rect{};
        rect.left = left;
        rect.top = top;
        rect.right = right;
        rect.bottom = bottom;
        return rect;
    }

    void PaintUi(HWND hwnd, AppState& state)
    {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT client{};
        GetClientRect(hwnd, &client);

        HDC bufferDc = CreateCompatibleDC(hdc);
        HBITMAP bufferBitmap = CreateCompatibleBitmap(hdc, client.right - client.left, client.bottom - client.top);
        const auto previousBitmap = static_cast<HBITMAP>(SelectObject(bufferDc, bufferBitmap));

        FillRectColor(bufferDc, client, kWindowBackground);
        state.hitTargets.clear();

        const int sidebarWidth = 278;
        const int headerHeight = 46;
        const int contentLeft = sidebarWidth + 18;
        const int contentRight = client.right - 18;

        const RECT sidebar = MakeRect(0, 0, sidebarWidth, client.bottom);
        FillRectColor(bufferDc, sidebar, kSidebarBackground);
        DrawLine(bufferDc, sidebar.right, 0, sidebar.right, client.bottom, kDivider);
        DrawLine(bufferDc, 0, headerHeight, client.right, headerHeight, kDivider);

        const RECT contentBackgroundRect = MakeRect(sidebarWidth + 1, headerHeight + 1, client.right, client.bottom);
        FillRectColor(bufferDc, contentBackgroundRect, kWindowBackground);
        DrawImageCover(bufferDc, state.contentBackgroundImage, contentBackgroundRect, 0.055f);

        DrawTextLine(bufferDc, state.headerFont, kAccent, MakeRect(18, 8, sidebarWidth - 16, 28), L"Monster Hunter Wilds", DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        DrawTextLine(bufferDc, state.titleFont, kTextPrimary, MakeRect(18, 25, sidebarWidth - 16, 45), Localize(state.selectedLanguage, (std::wstring(L"Train") + L"er").c_str(), L"训练器"), DT_LEFT | DT_SINGLELINE | DT_VCENTER);

        const RECT chineseRect = MakeRect(contentRight - 164, 6, contentRight - 98, 30);
        const RECT englishRect = MakeRect(contentRight - 92, 6, contentRight - 26, 30);
        if (state.selectedLanguage == Language::Chinese)
        {
            DrawRectOutline(bufferDc, chineseRect, kAccentDark);
        }
        if (state.selectedLanguage == Language::English)
        {
            DrawRectOutline(bufferDc, englishRect, kAccentDark);
        }
        DrawTextLine(bufferDc, state.bodyFont, state.selectedLanguage == Language::Chinese ? kAccent : kTextPrimary, chineseRect, L"中文", DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawTextLine(bufferDc, state.bodyFont, state.selectedLanguage == Language::English ? kAccent : kTextPrimary, englishRect, L"English", DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        state.hitTargets.push_back(HitTarget{ chineseRect, HitKind::Language, 0 });
        state.hitTargets.push_back(HitTarget{ englishRect, HitKind::Language, 1 });

        const RECT posterRect = MakeRect(18, 82, sidebarWidth - 18, 426);
        FillRectColor(bufferDc, posterRect, RGB(40, 32, 27));
        DrawRectOutline(bufferDc, posterRect, kAccentDark);
        const RECT imageRect = MakeRect(posterRect.left + 2, posterRect.top + 2, posterRect.right - 2, posterRect.bottom - 2);
        FillRectColor(bufferDc, imageRect, RGB(58, 47, 40));
        if (state.posterImage != nullptr)
        {
            DrawImageCover(bufferDc, state.posterImage, imageRect);
        }
        else
        {
            FillRectColor(bufferDc, MakeRect(posterRect.left + 10, posterRect.bottom - 90, posterRect.right - 10, posterRect.bottom - 10), RGB(29, 29, 31));
            DrawTextLine(bufferDc, state.headerFont, kAccent, MakeRect(posterRect.left + 18, posterRect.top + 24, posterRect.right - 18, posterRect.top + 60), L"MHW", DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            DrawTextLine(bufferDc, state.bodyFont, kTextPrimary, MakeRect(posterRect.left + 18, posterRect.top + 70, posterRect.right - 18, posterRect.top + 128), L"MONSTER HUNTER\r\nWILDS", DT_LEFT | DT_WORDBREAK);
            DrawLine(bufferDc, posterRect.left + 24, posterRect.top + 162, posterRect.right - 24, posterRect.top + 162, kAccentDark);
            DrawTextLine(bufferDc, state.smallFont, kTextSecondary, MakeRect(posterRect.left + 18, posterRect.top + 176, posterRect.right - 18, posterRect.bottom - 112), Localize(state.selectedLanguage, (std::wstring(L"Preview artwork area\r\n") + L"Train" + L"er style mockup\r\nStandalone demo window").c_str(), L"预览图区域\r\n训练器风格界面\r\n独立演示窗口"), DT_LEFT | DT_WORDBREAK);
            DrawTextLine(bufferDc, state.smallFont, kAccent, MakeRect(posterRect.left + 18, posterRect.bottom - 72, posterRect.right - 18, posterRect.bottom - 20), Localize(state.selectedLanguage, L"SINGLE WINDOW UI", L"单窗口界面"), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }

        DrawTextLine(bufferDc, state.smallFont, kTextSecondary, MakeRect(18, 442, sidebarWidth - 18, 462), Localize(state.selectedLanguage, L"Visual Panel:", L"可视化面板："), DT_LEFT | DT_SINGLELINE);
        DrawTextLine(bufferDc, state.smallFont, kTextPrimary, MakeRect(18, 460, sidebarWidth - 18, 498), Localize(state.selectedLanguage, L"Monster Hunter Wilds", L"怪物猎人：荒野"), DT_LEFT | DT_WORDBREAK);
        DrawTextLine(bufferDc, state.smallFont, kTextSecondary, MakeRect(18, 510, sidebarWidth - 18, 530), Localize(state.selectedLanguage, L"Version: 1.0", L"版本：1.0"), DT_LEFT | DT_SINGLELINE);
        DrawTextLine(bufferDc, state.smallFont, kTextSecondary, MakeRect(18, 536, sidebarWidth - 18, 556), Localize(state.selectedLanguage, L"Build: 2026.03", L"版本：2026.03"), DT_LEFT | DT_SINGLELINE);

        DrawLine(bufferDc, 18, 572, sidebarWidth - 18, 572, kDivider);
        const RECT resetRect = MakeRect(18, 580, sidebarWidth - 18, 608);
        FillRectColor(bufferDc, resetRect, kPanelBackgroundAlt);
        DrawRectOutline(bufferDc, resetRect, kAccentDark);
        DrawTextLine(bufferDc, state.smallFont, kAccent, resetRect, Localize(state.selectedLanguage, L"Reset All", L"重置全部"), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        state.hitTargets.push_back(HitTarget{ resetRect, HitKind::Reset, 0 });

        DrawLine(bufferDc, 18, 624, sidebarWidth - 18, 624, kDivider);
        DrawTextLine(bufferDc, state.smallFont, kTextSecondary, MakeRect(18, 632, sidebarWidth - 18, 648), Localize(state.selectedLanguage, L"Last action:", L"上次操作："), DT_LEFT | DT_SINGLELINE);
        DrawTextLine(bufferDc, state.smallFont, kTextPrimary, MakeRect(18, 652, sidebarWidth - 18, 748), state.lastAction, DT_LEFT | DT_WORDBREAK);

        const RECT hotkeysTab = MakeRect(contentLeft, 52, contentLeft + 116, 80);
        DrawTextLine(bufferDc, state.bodyFont, kTextPrimary, hotkeysTab, Localize(state.selectedLanguage, L"Hotkeys", L"热键"), DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        DrawLine(bufferDc, hotkeysTab.left, hotkeysTab.bottom, hotkeysTab.left + 84, hotkeysTab.bottom, kAccent);

        const Category categories[] = { Category::Core, Category::Resources, Category::Rewards, Category::Weapons };
        int categoryX = contentLeft;
        for (int i = 0; i < 4; ++i)
        {
            const RECT categoryRect = MakeRect(categoryX, 90, categoryX + 122, 118);
            if (categories[i] == state.selectedCategory)
            {
                FillRectColor(bufferDc, categoryRect, kPanelBackgroundAlt);
                DrawRectOutline(bufferDc, categoryRect, kAccentDark);
            }
            else
            {
                DrawRectOutline(bufferDc, categoryRect, kDivider);
            }
            DrawTextLine(bufferDc, state.smallFont, categories[i] == state.selectedCategory ? kAccent : kTextSecondary, categoryRect, CategoryMenuLabel(categories[i], state.selectedLanguage), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            state.hitTargets.push_back(HitTarget{ categoryRect, HitKind::Category, i });
            categoryX += 132;
        }

        const RECT sectionHeader = MakeRect(contentLeft, 128, contentRight, 160);
        DrawTextLine(bufferDc, state.bodyFont, kTextPrimary, MakeRect(contentLeft, 132, contentLeft + 140, 156), Localize(state.selectedLanguage, L"Hotkeys", L"热键"), DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        DrawTextLine(bufferDc, state.bodyFont, kTextPrimary, MakeRect(contentLeft + 160, 132, contentRight, 156), Localize(state.selectedLanguage, L"Parameters", L"参数"), DT_LEFT | DT_SINGLELINE | DT_VCENTER);

        const auto visibleFeatures = VisibleFeatureIndices(state);
        const RECT listViewport = MakeRect(contentLeft, GetListTop(), contentRight, GetListBottom(client));
        int savedDc = SaveDC(bufferDc);
        IntersectClipRect(bufferDc, listViewport.left, listViewport.top, listViewport.right, listViewport.bottom);
        int rowTop = GetListTop() - state.listScrollOffset;
        for (int i = 0; i < static_cast<int>(visibleFeatures.size()); ++i)
        {
            const int featureIndex = visibleFeatures[i];
            const Feature& feature = state.features[featureIndex];
            const RECT rowRect = MakeRect(contentLeft, rowTop, contentRight, rowTop + 34);
            if (rowRect.bottom <= listViewport.top || rowRect.top >= listViewport.bottom)
            {
                rowTop += 34;
                continue;
            }

            DrawTextLine(bufferDc, state.smallFont, kTextPrimary, MakeRect(contentLeft + 8, rowTop + 7, contentLeft + 126, rowTop + 27), DisplayHotkey(feature.hotkey), DT_LEFT | DT_SINGLELINE | DT_VCENTER);

            if (feature.mode == FeatureMode::Toggle)
            {
                const RECT toggleRect = MakeRect(contentLeft + 120, rowTop + 7, contentLeft + 154, rowTop + 25);
                DrawToggleSwitch(bufferDc, toggleRect, feature.enabled);
                DrawTextLine(bufferDc, state.smallFont, kTextPrimary, MakeRect(contentLeft + 166, rowTop + 7, contentRight - 12, rowTop + 27), LocalizedFeatureLabel(state.selectedLanguage, feature, featureIndex), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);
            }
            else
            {
                const RECT applyRect = MakeRect(contentLeft + 118, rowTop + 6, contentLeft + 170, rowTop + 28);
                DrawRectOutline(bufferDc, applyRect, kDivider);
                DrawTextLine(bufferDc, state.smallFont, kTextPrimary, applyRect, Localize(state.selectedLanguage, L"APPLY", L"应用"), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                DrawTextLine(bufferDc, state.smallFont, kTextPrimary, MakeRect(contentLeft + 178, rowTop + 7, contentRight - 180, rowTop + 27), LocalizedFeatureLabel(state.selectedLanguage, feature, featureIndex), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);
                        DrawValueBox(bufferDc, MakeRect(contentRight - 158, rowTop + 4, contentRight - 12, rowTop + 30), StatusText(feature, state.selectedLanguage), state.smallFont);
                    }

                    state.hitTargets.push_back(HitTarget{ rowRect, HitKind::Feature, featureIndex });
                    rowTop += 34;
                }
                RestoreDC(bufferDc, savedDc);

        BitBlt(hdc, 0, 0, client.right - client.left, client.bottom - client.top, bufferDc, 0, 0, SRCCOPY);

        SelectObject(bufferDc, previousBitmap);
        DeleteObject(bufferBitmap);
        DeleteDC(bufferDc);
        EndPaint(hwnd, &ps);
    }

    SplashState* GetSplashState(HWND hwnd)
    {
        return reinterpret_cast<SplashState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    void PaintSplash(HWND hwnd, SplashState& state)
    {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT client{};
        GetClientRect(hwnd, &client);

        HDC bufferDc = CreateCompatibleDC(hdc);
        HBITMAP bufferBitmap = CreateCompatibleBitmap(hdc, client.right - client.left, client.bottom - client.top);
        const auto previousBitmap = static_cast<HBITMAP>(SelectObject(bufferDc, bufferBitmap));

        FillRectColor(bufferDc, client, kWindowBackground);

        if (state.backgroundImage != nullptr)
        {
            DrawImageCover(bufferDc, state.backgroundImage, client, 0.16f);
        }

        FillRectColor(bufferDc, MakeRect(0, 0, 288, client.bottom), RGB(27, 27, 30));
        DrawLine(bufferDc, 288, 0, 288, client.bottom, kDivider);

        const RECT posterRect = MakeRect(22, 24, 266, client.bottom - 24);
        FillRectColor(bufferDc, posterRect, RGB(40, 32, 27));
        DrawRectOutline(bufferDc, posterRect, kAccentDark);
        DrawImageCover(bufferDc, state.posterImage, MakeRect(posterRect.left + 2, posterRect.top + 2, posterRect.right - 2, posterRect.bottom - 2));

        DrawTextLine(bufferDc, state.titleFont, kAccent, MakeRect(330, 54, client.right - 40, 96), std::wstring(L"Monster Hunter ") + L"Train" + L"er", DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        DrawTextLine(bufferDc, state.bodyFont, kTextPrimary, MakeRect(330, 110, client.right - 40, 142), L"Loading interface...", DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        const RECT progressOuter = MakeRect(330, client.bottom - 96, client.right - 48, client.bottom - 68);
        const RECT progressInner = MakeRect(progressOuter.left + 2, progressOuter.top + 2, progressOuter.right - 2, progressOuter.bottom - 2);
        const int progressWidth = progressInner.right - progressInner.left;
        const int chunkWidth = progressWidth / 3;
        const double elapsedMs = static_cast<double>(GetTickCount64() - state.startTick);
        const double cycleMs = 1800.0;
        const double progress = std::fmod(elapsedMs, cycleMs) / cycleMs;
        const int travel = progressWidth + chunkWidth;
        const int chunkLeft = progressInner.left - chunkWidth + static_cast<int>(travel * progress);
        const RECT animatedChunk = MakeRect(chunkLeft, progressInner.top, chunkLeft + chunkWidth, progressInner.bottom);

        FillRoundedRectGdiPlus(bufferDc, progressOuter, Gdiplus::Color(255, 110, 68, 18));
        FillRoundedRectGdiPlus(bufferDc, progressInner, Gdiplus::Color(255, 37, 37, 41));

        int progressDc = SaveDC(bufferDc);
        IntersectClipRect(bufferDc, progressInner.left, progressInner.top, progressInner.right, progressInner.bottom);
        FillRoundedRectGdiPlus(bufferDc, animatedChunk, Gdiplus::Color(255, 232, 145, 35));
        const RECT glowChunk = MakeRect(animatedChunk.left + 18, animatedChunk.top + 3, animatedChunk.right - 18, animatedChunk.bottom - 3);
        FillRoundedRectGdiPlus(bufferDc, glowChunk, Gdiplus::Color(120, 255, 208, 140));
        RestoreDC(bufferDc, progressDc);

        DrawTextLine(bufferDc, state.smallFont, kTextSecondary, MakeRect(330, client.bottom - 58, client.right - 48, client.bottom - 28), L"Please wait...", DT_LEFT | DT_SINGLELINE | DT_VCENTER);

        BitBlt(hdc, 0, 0, client.right - client.left, client.bottom - client.top, bufferDc, 0, 0, SRCCOPY);

        SelectObject(bufferDc, previousBitmap);
        DeleteObject(bufferBitmap);
        DeleteDC(bufferDc);
        EndPaint(hwnd, &ps);
    }

    LRESULT CALLBACK SplashWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_CREATE:
        {
            const auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
            auto* state = static_cast<SplashState*>(createStruct->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
            state->startTick = GetTickCount64();
            SetTimer(hwnd, 1, 5000, nullptr);
            SetTimer(hwnd, 2, 16, nullptr);
            HRGN region = CreateRoundRectRgn(0, 0, 860, 460, 24, 24);
            SetWindowRgn(hwnd, region, TRUE);
            return 0;
        }
        case WM_TIMER:
        {
            auto* state = GetSplashState(hwnd);
            if (wParam == 1)
            {
                KillTimer(hwnd, 2);
                DestroyWindow(hwnd);
                return 0;
            }

            if (wParam == 2 && state != nullptr)
            {
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }

            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
        {
            auto* state = GetSplashState(hwnd);
            if (state != nullptr)
            {
                PaintSplash(hwnd, *state);
            }
            return 0;
        }
        }

        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    void ShowSplashScreen(HINSTANCE instance)
    {
        SplashState state;
        state.posterImage = LoadPosterImage();
        state.backgroundImage = LoadContentBackgroundImage();
        state.titleFont = CreateUiFont(28, FW_SEMIBOLD);
        state.bodyFont = CreateUiFont(18, FW_MEDIUM);
        state.smallFont = CreateUiFont(13, FW_NORMAL);

        WNDCLASSEXW splashClass{};
        splashClass.cbSize = sizeof(splashClass);
        splashClass.style = CS_HREDRAW | CS_VREDRAW;
        splashClass.lpfnWndProc = SplashWindowProc;
        splashClass.hInstance = instance;
        splashClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        splashClass.hbrBackground = nullptr;
        splashClass.hIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 64, 64, LR_DEFAULTCOLOR));
        splashClass.hIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR));
        const std::wstring splashClassName = std::wstring(L"MonsterHunterWilds") + L"Train" + L"erSplash";
        const std::wstring splashTitle = std::wstring(L"Monster Hunter ") + L"Train" + L"er";
        splashClass.lpszClassName = splashClassName.c_str();
        RegisterClassExW(&splashClass);

        constexpr int splashWidth = 860;
        constexpr int splashHeight = 460;
        const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        const int screenHeight = GetSystemMetrics(SM_CYSCREEN);

        HWND splashWindow = CreateWindowExW(
            WS_EX_TOOLWINDOW,
            splashClassName.c_str(),
            splashTitle.c_str(),
            WS_POPUP,
            (screenWidth - splashWidth) / 2,
            (screenHeight - splashHeight) / 2,
            splashWidth,
            splashHeight,
            nullptr,
            nullptr,
            instance,
            &state);

        if (splashWindow != nullptr)
        {
            ShowWindow(splashWindow, SW_SHOW);
            UpdateWindow(splashWindow);

            MSG message{};
            while (IsWindow(splashWindow) && GetMessageW(&message, nullptr, 0, 0) > 0)
            {
                TranslateMessage(&message);
                DispatchMessageW(&message);
                if (!IsWindow(splashWindow))
                {
                    break;
                }
            }
        }

        DeleteObject(state.titleFont);
        DeleteObject(state.bodyFont);
        DeleteObject(state.smallFont);
        delete state.posterImage;
        delete state.backgroundImage;
    }

    AppState* GetState(HWND hwnd)
    {
        return reinterpret_cast<AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_CREATE:
        {
            const auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
            auto* state = static_cast<AppState*>(createStruct->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
            return 0;
        }
        case WM_GETMINMAXINFO:
        {
            auto* minMax = reinterpret_cast<MINMAXINFO*>(lParam);
            minMax->ptMinTrackSize.x = 1040;
            minMax->ptMinTrackSize.y = 760;
            minMax->ptMaxTrackSize.x = 1040;
            minMax->ptMaxTrackSize.y = 760;
            return 0;
        }
        case WM_SIZE:
        {
            auto* state = GetState(hwnd);
            if (state != nullptr)
            {
                UpdateListScrollBar(hwnd, *state);
            }
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_LBUTTONDOWN:
        {
            auto* state = GetState(hwnd);
            if (state == nullptr)
            {
                return 0;
            }

            const POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            for (const auto& target : state->hitTargets)
            {
                if (!PointInRect(target.rect, point))
                {
                    continue;
                }

                if (target.kind == HitKind::View)
                {
                    state->selectedView = target.index == 0 ? ViewMode::Hotkeys : ViewMode::Options;
                    state->lastAction = state->selectedView == ViewMode::Hotkeys
                        ? Localize(state->selectedLanguage, L"Hotkeys tab selected.", L"已切换到热键标签。")
                        : Localize(state->selectedLanguage, L"Options tab selected.", L"已切换到选项标签。");
                    UpdateListScrollBar(hwnd, *state);
                }
                else if (target.kind == HitKind::Language)
                {
                    state->selectedLanguage = target.index == 0 ? Language::Chinese : Language::English;
                    state->lastAction = state->selectedLanguage == Language::Chinese
                        ? L"已切换到中文界面。"
                        : L"Switched to English UI.";
                }
                else if (target.kind == HitKind::Category)
                {
                    const Category categories[] = { Category::Core, Category::Resources, Category::Rewards, Category::Weapons };
                    state->selectedCategory = categories[target.index];
                    state->listScrollOffset = 0;
                    state->lastAction = state->selectedLanguage == Language::Chinese
                        ? CategoryTitle(state->selectedCategory, state->selectedLanguage) + L" 已选择。"
                        : CategoryTitle(state->selectedCategory, state->selectedLanguage) + L" selected.";
                    UpdateListScrollBar(hwnd, *state);
                }
                else if (target.kind == HitKind::Reset)
                {
                    ResetFeatures(*state);
                    UpdateListScrollBar(hwnd, *state);
                }
                else if (target.kind == HitKind::Minimize)
                {
                    ShowWindow(hwnd, SW_MINIMIZE);
                }
                else if (target.kind == HitKind::Close)
                {
                    DestroyWindow(hwnd);
                }
                else if (target.kind == HitKind::Feature)
                {
                    ActivateFeature(*state, target.index);
                }

                InvalidateRect(hwnd, nullptr, FALSE);
                break;
            }
            return 0;
        }
        case WM_MOUSEWHEEL:
        {
            auto* state = GetState(hwnd);
            if (state == nullptr)
            {
                return 0;
            }

            state->listScrollOffset -= (GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA) * 68;
            UpdateListScrollBar(hwnd, *state);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_VSCROLL:
        {
            auto* state = GetState(hwnd);
            if (state == nullptr)
            {
                return 0;
            }

            SCROLLINFO scrollInfo{};
            scrollInfo.cbSize = sizeof(scrollInfo);
            scrollInfo.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_VERT, &scrollInfo);

            int newOffset = state->listScrollOffset;
            switch (LOWORD(wParam))
            {
            case SB_LINEUP:
                newOffset -= 34;
                break;
            case SB_LINEDOWN:
                newOffset += 34;
                break;
            case SB_PAGEUP:
                newOffset -= static_cast<int>(scrollInfo.nPage);
                break;
            case SB_PAGEDOWN:
                newOffset += static_cast<int>(scrollInfo.nPage);
                break;
            case SB_THUMBPOSITION:
            case SB_THUMBTRACK:
                newOffset = scrollInfo.nTrackPos;
                break;
            case SB_TOP:
                newOffset = 0;
                break;
            case SB_BOTTOM:
                newOffset = scrollInfo.nMax;
                break;
            default:
                break;
            }

            state->listScrollOffset = newOffset;
            UpdateListScrollBar(hwnd, *state);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_KEYDOWN:
        {
            auto* state = GetState(hwnd);
            if (state == nullptr)
            {
                return 0;
            }

            if (wParam == VK_ESCAPE)
            {
                DestroyWindow(hwnd);
                return 0;
            }

            for (int i = 0; i < static_cast<int>(state->features.size()); ++i)
            {
                if (ShortcutMatches(state->features[i].shortcut, static_cast<UINT>(wParam)))
                {
                    ActivateFeature(*state, i);
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
            }

            return 0;
        }
        case WM_PAINT:
        {
            auto* state = GetState(hwnd);
            if (state != nullptr)
            {
                PaintUi(hwnd, *state);
            }
            else
            {
                PAINTSTRUCT ps{};
                BeginPaint(hwnd, &ps);
                EndPaint(hwnd, &ps);
            }
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }

        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

int RunApplication(HINSTANCE instance, int commandShow)
{
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken{};
    if (Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr) != Gdiplus::Ok)
    {
        return 0;
    }

    ShowSplashScreen(instance);

    AppState state;
    state.features = BuildFeatures();
    state.posterImage = LoadPosterImage();
    state.contentBackgroundImage = LoadContentBackgroundImage();
    state.titleFont = CreateUiFont(18, FW_SEMIBOLD);
    state.headerFont = CreateUiFont(20, FW_SEMIBOLD);
    state.bodyFont = CreateUiFont(15, FW_MEDIUM);
    state.smallFont = CreateUiFont(12, FW_NORMAL);

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;
    windowClass.hIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 64, 64, LR_DEFAULTCOLOR));
    windowClass.hIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR));
    const std::wstring mainClassName = std::wstring(L"MonsterHunterWilds") + L"Train" + L"er";
    const std::wstring mainTitle = std::wstring(L"Monster Hunter ") + L"Train" + L"er";
    windowClass.lpszClassName = mainClassName.c_str();

    RegisterClassExW(&windowClass);

    constexpr DWORD kWindowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VSCROLL;
    RECT desiredRect = MakeRect(0, 0, 1040, CalculateClientHeight(state));
    AdjustWindowRectEx(&desiredRect, kWindowStyle, FALSE, 0);

    HWND hwnd = CreateWindowExW(
        0,
        mainClassName.c_str(),
        mainTitle.c_str(),
        kWindowStyle,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        desiredRect.right - desiredRect.left,
        desiredRect.bottom - desiredRect.top,
        nullptr,
        nullptr,
        instance,
        &state);

    if (hwnd == nullptr)
    {
        return 0;
    }

    AdjustWindowToContent(hwnd, state);

    ShowWindow(hwnd, commandShow);
    UpdateWindow(hwnd);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    DeleteObject(state.titleFont);
    DeleteObject(state.headerFont);
    DeleteObject(state.bodyFont);
    DeleteObject(state.smallFont);
    delete state.posterImage;
    delete state.contentBackgroundImage;
    Gdiplus::GdiplusShutdown(gdiplusToken);

    return 0;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int commandShow)
{
    return RunApplication(instance, commandShow);
}