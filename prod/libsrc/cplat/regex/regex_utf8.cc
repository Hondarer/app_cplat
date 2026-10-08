/**
 *******************************************************************************
 *  @file           regex_utf8.cc
 *  @brief          regex モジュール内部の UTF-8 変換ヘルパーを実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/08/04
 *  @version        1.0.0
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

/*
 * 本ファイルの関数は regex モジュール内部の実装であり、共有ライブラリの公開 ABI
 * ではない。C++ 標準ライブラリのテンプレート実体化とともに動的シンボル表へ
 * 現れないよう、-fvisibility=hidden でコンパイルする (同ディレクトリの makepart.mk)。
 */

#include <cplat/regex/regex_utf8.h>

#include <algorithm>

namespace cplat
{
namespace regex_detail
{

namespace
{

/** サロゲート ペアの上位側を表すコード単位の下限。 */
const unsigned int SURROGATE_HIGH_MIN = 0xD800U;

/** サロゲート ペアの上位側を表すコード単位の上限。 */
const unsigned int SURROGATE_HIGH_MAX = 0xDBFFU;

/** サロゲート ペアの下位側を表すコード単位の下限。 */
const unsigned int SURROGATE_LOW_MIN = 0xDC00U;

/** サロゲート ペアの下位側を表すコード単位の上限。 */
const unsigned int SURROGATE_LOW_MAX = 0xDFFFU;

/** BMP 外のコード ポイントの下限。 */
const unsigned long SUPPLEMENTARY_MIN = 0x10000UL;

/** Unicode のコード ポイントの上限。 */
const unsigned long CODE_POINT_MAX = 0x10FFFFUL;

/* コード単位がサロゲート ペアの下位側かどうかを判定する。 */
bool is_low_surrogate(wchar_t unit)
{
    const unsigned int value = static_cast<unsigned int>(unit) & 0xFFFFU;
    return (value >= SURROGATE_LOW_MIN) && (value <= SURROGATE_LOW_MAX);
}

/* コード単位がサロゲート ペアの上位側かどうかを判定する。 */
bool is_high_surrogate(wchar_t unit)
{
    const unsigned int value = static_cast<unsigned int>(unit) & 0xFFFFU;
    return (value >= SURROGATE_HIGH_MIN) && (value <= SURROGATE_HIGH_MAX);
}

/* 索引がサロゲート ペアの内側 (上位と下位の間) を指すかどうかを判定する。 */
bool is_inside_pair(const std::wstring &units, std::size_t index)
{
    if ((index == 0) || (index >= units.size()))
    {
        return false;
    }
    if (!is_low_surrogate(units[index]))
    {
        return false;
    }
    return is_high_surrogate(units[index - 1]);
}

} /* namespace */

/* Doxygen コメントは、ヘッダーに記載 */

bool cplat_internal_regex_utf8_decode(const char *text, std::size_t text_len, std::wstring &units_out,
                                      std::vector<std::size_t> &offsets_out)
{
    units_out.clear();
    offsets_out.clear();

    if ((text == nullptr) && (text_len != 0))
    {
        return false;
    }

    std::size_t pos = 0;
    while (pos < text_len)
    {
        const unsigned int lead = static_cast<unsigned char>(text[pos]);
        std::size_t sequence_len = 0;
        unsigned long code_point = 0;

        if (lead < 0x80U)
        {
            sequence_len = 1;
            code_point = lead;
        }
        else if ((lead & 0xE0U) == 0xC0U)
        {
            sequence_len = 2;
            code_point = lead & 0x1FU;
        }
        else if ((lead & 0xF0U) == 0xE0U)
        {
            sequence_len = 3;
            code_point = lead & 0x0FU;
        }
        else if ((lead & 0xF8U) == 0xF0U)
        {
            sequence_len = 4;
            code_point = lead & 0x07U;
        }
        else
        {
            return false;
        }

        if ((text_len - pos) < sequence_len)
        {
            return false;
        }

        for (std::size_t k = 1; k < sequence_len; k++)
        {
            const unsigned int trail = static_cast<unsigned char>(text[pos + k]);
            if ((trail & 0xC0U) != 0x80U)
            {
                return false;
            }
            code_point = (code_point << 6) | (trail & 0x3FU);
        }

        /* オーバー ロング表現を拒否する。 */
        if ((sequence_len == 2) && (code_point < 0x80UL))
        {
            return false;
        }
        if ((sequence_len == 3) && (code_point < 0x800UL))
        {
            return false;
        }
        if ((sequence_len == 4) && (code_point < SUPPLEMENTARY_MIN))
        {
            return false;
        }
        /* 単独サロゲートと範囲外のコード ポイントを拒否する。 */
        if ((code_point >= SURROGATE_HIGH_MIN) && (code_point <= SURROGATE_LOW_MAX))
        {
            return false;
        }
        if (code_point > CODE_POINT_MAX)
        {
            return false;
        }

        if (code_point < SUPPLEMENTARY_MIN)
        {
            offsets_out.push_back(pos);
            units_out.push_back(static_cast<wchar_t>(code_point));
        }
        else
        {
            const unsigned long value = code_point - SUPPLEMENTARY_MIN;
            offsets_out.push_back(pos);
            units_out.push_back(static_cast<wchar_t>(SURROGATE_HIGH_MIN + (value >> 10)));
            offsets_out.push_back(pos);
            units_out.push_back(static_cast<wchar_t>(SURROGATE_LOW_MIN + (value & 0x3FFUL)));
        }

        pos += sequence_len;
    }

    offsets_out.push_back(text_len);
    return true;
}

/* Doxygen コメントは、ヘッダーに記載 */

bool cplat_internal_regex_utf8_encode(const std::wstring &units, std::string &text_out)
{
    text_out.clear();

    std::size_t index = 0;
    while (index < units.size())
    {
        unsigned long code_point = static_cast<unsigned int>(units[index]) & 0xFFFFU;

        if (is_high_surrogate(units[index]))
        {
            if ((index + 1) >= units.size())
            {
                return false;
            }
            if (!is_low_surrogate(units[index + 1]))
            {
                return false;
            }
            const unsigned long high = code_point - SURROGATE_HIGH_MIN;
            const unsigned long low = (static_cast<unsigned int>(units[index + 1]) & 0xFFFFU) - SURROGATE_LOW_MIN;
            code_point = SUPPLEMENTARY_MIN + (high << 10) + low;
            index += 2;
        }
        else if (is_low_surrogate(units[index]))
        {
            return false;
        }
        else
        {
            index++;
        }

        if (code_point < 0x80UL)
        {
            text_out.push_back(static_cast<char>(code_point));
        }
        else if (code_point < 0x800UL)
        {
            text_out.push_back(static_cast<char>(0xC0UL | (code_point >> 6)));
            text_out.push_back(static_cast<char>(0x80UL | (code_point & 0x3FUL)));
        }
        else if (code_point < SUPPLEMENTARY_MIN)
        {
            text_out.push_back(static_cast<char>(0xE0UL | (code_point >> 12)));
            text_out.push_back(static_cast<char>(0x80UL | ((code_point >> 6) & 0x3FUL)));
            text_out.push_back(static_cast<char>(0x80UL | (code_point & 0x3FUL)));
        }
        else
        {
            text_out.push_back(static_cast<char>(0xF0UL | (code_point >> 18)));
            text_out.push_back(static_cast<char>(0x80UL | ((code_point >> 12) & 0x3FUL)));
            text_out.push_back(static_cast<char>(0x80UL | ((code_point >> 6) & 0x3FUL)));
            text_out.push_back(static_cast<char>(0x80UL | (code_point & 0x3FUL)));
        }
    }

    return true;
}

/* Doxygen コメントは、ヘッダーに記載 */

std::size_t cplat_internal_regex_offset_of_begin(const std::wstring &units, const std::vector<std::size_t> &offsets,
                                                 std::size_t index)
{
    if (offsets.empty())
    {
        return 0;
    }
    if (index >= offsets.size())
    {
        return offsets.back();
    }
    /* 上位サロゲートの位置へ丸めるため、写像表の値をそのまま使用する。 */
    static_cast<void>(units);
    return offsets[index];
}

/* Doxygen コメントは、ヘッダーに記載 */

std::size_t cplat_internal_regex_offset_of_end(const std::wstring &units, const std::vector<std::size_t> &offsets,
                                               std::size_t index)
{
    if (offsets.empty())
    {
        return 0;
    }
    if (index >= offsets.size())
    {
        return offsets.back();
    }
    if (is_inside_pair(units, index))
    {
        /* サロゲート ペアの内側で終わる場合は、そのコード ポイントの末尾まで含める。 */
        return offsets[index + 1];
    }
    return offsets[index];
}

/* Doxygen コメントは、ヘッダーに記載 */

bool cplat_internal_regex_index_of_offset(const std::vector<std::size_t> &offsets, std::size_t offset,
                                          std::size_t &index_out)
{
    index_out = 0;

    const std::vector<std::size_t>::const_iterator found = std::lower_bound(offsets.begin(), offsets.end(), offset);
    if (found == offsets.end())
    {
        return false;
    }
    if (*found != offset)
    {
        return false;
    }

    index_out = static_cast<std::size_t>(found - offsets.begin());
    return true;
}

} /* namespace regex_detail */
} /* namespace cplat */
