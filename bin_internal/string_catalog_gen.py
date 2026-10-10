#!/usr/bin/env python3
"""カタログ定義 (JSONC) から cplat 文字列カタログの生成物 2 ファイルを書き出す。"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

# 引数種別から、型付きラッパーの仮引数の型への対応。
# 正本は prod/include/cplat/string_catalog/argument.h の対応表。
ARGUMENT_TYPES = {
    "STRING": "const char *",
    "CHAR": "char",
    "INT8": "int8_t",
    "UINT8": "uint8_t",
    "INT16": "int16_t",
    "UINT16": "uint16_t",
    "INT32": "int32_t",
    "UINT32": "uint32_t",
    "INT64": "int64_t",
    "UINT64": "uint64_t",
    "HEX8": "uint8_t",
    "HEX16": "uint16_t",
    "HEX32": "uint32_t",
    "HEX64": "uint64_t",
    "SIZE": "size_t",
    "SSIZE": "int64_t",
    "POINTER": "const void *",
    "DOUBLE": "double",
    "ERROR_CODE": "int",
}

# ライブラリ側の接頭辞。カタログ定義には書かず、生成器が補う。
# 文字列カタログの型名と API 名を決めるのはライブラリであり、利用者の選択肢ではないため。
LIBRARY_PREFIX = "cplat_string_catalog"

# 生成物が include するライブラリの公開ヘッダー。接頭辞から機械的に導けないため、別に定数化する。
LIBRARY_HEADER = "cplat/string_catalog/string_catalog.h"

# texts と notes のキーに書ける言語。cplat_string_catalog_language の並びと揃える。
# 言語はライブラリが定める仕様であり、カタログ定義が増減できる項目ではない。
LANGUAGES = ("neutral", "japanese", "english")

# 位置指定のインデックスに書ける最大の桁数。
# prod/libsrc/cplat/string_catalog/string_catalog_render.c の INDEX_DIGITS_MAX と揃える。
INDEX_DIGITS_MAX = 2

# 1 つの文字列が取れる引数の最大個数。CPLAT_STRING_CATALOG_ARGUMENT_MAX と揃える。
ARGUMENT_MAX = 50

# 公開ヘッダーとして出力した場合に、利用側の include パスを置く場所。
PUBLIC_INCLUDE_KEY = "public_include"

# カタログ定義が app 単位の設定ファイルを指す項目と、読み込んだ内容を置く場所。
SETTINGS_REFERENCE = "settings"
SETTINGS_KEY = "settings_document"

# カタログ定義の export に書ける公開範囲。省略した場合は公開しない。
# api は戻り値が cplat の構造体を指さない関数だけを公開し、full はすべてを公開する。
EXPORT_SCOPE_API = "api"
EXPORT_SCOPE_FULL = "full"
EXPORT_SCOPES = (EXPORT_SCOPE_API, EXPORT_SCOPE_FULL)

# カタログ定義の kind に書ける値。省略した場合は message として扱う。
CATALOG_KIND_MESSAGE = "message"
CATALOG_KIND_TRACE = "trace"
CATALOG_KINDS = (CATALOG_KIND_MESSAGE, CATALOG_KIND_TRACE)

# トレース種別の level に書ける値。cplat_trace_level の並びと揃える。
# 生成器は名前を分類値の整数へ変換し、生成物は cplat_trace_level へ戻して使用する。
TRACE_LEVELS = ("CRITICAL", "ERROR", "WARNING", "INFO", "VERBOSE", "DEBUG", "NONE")

# コンテキスト引数を配置し始める位置指定。利用者が記載できる引数は、この番号の手前までとなる。
CONTEXT_ARGUMENT_BASE = 40

# トレース種別で、生成器が引数配列へ付け加えるコンテキスト引数。
# macro_value を持つものは呼び出し位置で確定するため、マクロが型付きラッパーへ渡す。
# inline_value を持つものは実行時の値のため、型付きラッパーの内部で取得する。
CONTEXT_ARGUMENTS = (
    {
        "name": "source_file_path",
        "kind": "STRING",
        "description": "呼び出し位置のソース ファイル。コンパイラへ渡した表記のままです。",
        "macro_value": "__FILE__",
    },
    {
        "name": "source_file_name",
        "kind": "STRING",
        "description": "呼び出し位置のソース ファイル名。ディレクトリを除いた表記です。",
        "macro_value": "cplat_path_basename(__FILE__)",
    },
    {
        "name": "source_line",
        "kind": "INT32",
        "description": "呼び出し位置の行番号。",
        "macro_value": "__LINE__",
    },
    {
        "name": "function_name",
        "kind": "STRING",
        "description": "呼び出し位置の関数名。",
        "macro_value": "__func__",
    },
    {
        "name": "process_id",
        "kind": "UINT32",
        "description": "出力を要求したプロセスの ID。",
        "inline_value": "cplat_process_get_pid()",
    },
    {
        "name": "thread_id",
        "kind": "UINT32",
        "description": "出力を要求したスレッドの ID。",
        "inline_value": "cplat_process_get_tid()",
    },
)

# app が定める文脈引数を置き始める位置指定。cplat が定める文脈引数の直後に固定する。
# ここを動かすと、app が書式へ記載した位置指定の意味が変わるため、非互換の変更として扱う。
# cplat が定める文脈引数は、この番号の手前までの 6 個が上限となる。
EXTENSION_ARGUMENT_BASE = 46

# app が定める文脈引数の個数の上限。
EXTENSION_ARGUMENT_MAX = ARGUMENT_MAX - EXTENSION_ARGUMENT_BASE

# cplat が定めるコンテキスト引数が拡張の基底へ届くと、app が定める番号と衝突する。
# 位置指定は定義の並び順から決まるため、衝突しても書式の検査では気付けない。
# cplat 側を増やす場合は、基底の移動を伴う非互換の変更としてここで止める。
assert CONTEXT_ARGUMENT_BASE + len(CONTEXT_ARGUMENTS) <= EXTENSION_ARGUMENT_BASE, (
    "cplat が定めるコンテキスト引数が EXTENSION_ARGUMENT_BASE に達しています。"
    "基底の移動を伴う非互換の変更として扱ってください。"
)

# app 単位の設定ファイルで、文脈引数の拡張を書く節。
CONTEXT_SECTION = "context"

# カタログ定義で、全文字列の書式へ共通に前置、後置する文字列を書く節。
# 言語をキーとして書き、生成の時点で各文字列の書式へ結合する。
TEXT_PREFIX_SECTION = "text_prefix"
TEXT_SUFFIX_SECTION = "text_suffix"
TEXT_AFFIX_SECTIONS = (TEXT_PREFIX_SECTION, TEXT_SUFFIX_SECTION)

# トレース種別の生成物が追加で参照する公開ヘッダー。
TRACE_HEADERS = (
    "cplat/trace/tracer.h",
    "cplat/crt/path.h",
    "cplat/runtime/process.h",
    "cplat/string_catalog/filter.h",
)


class DefinitionError(Exception):
    """カタログ定義の内容が不正であることを表す。"""


def strip_jsonc(text: str) -> str:
    """JSONC から行コメント、ブロック コメント、末尾コンマを取り除く。

    文字列リテラルの中は書き換えない。エスケープも解釈する。
    """
    out = []
    index = 0
    length = len(text)
    in_string = False

    while index < length:
        char = text[index]

        if in_string:
            out.append(char)
            if char == "\\" and (index + 1) < length:
                out.append(text[index + 1])
                index += 2
                continue
            if char == '"':
                in_string = False
            index += 1
            continue

        if char == '"':
            in_string = True
            out.append(char)
            index += 1
            continue

        if text.startswith("//", index):
            while index < length and text[index] != "\n":
                index += 1
            continue

        if text.startswith("/*", index):
            end = text.find("*/", index + 2)
            index = length if end < 0 else end + 2
            continue

        if char == ",":
            # 末尾コンマなら落とす。次の非空白が閉じ括弧のとき。
            probe = index + 1
            while probe < length and text[probe] in " \t\r\n":
                probe += 1
            if probe < length and text[probe] in "}]":
                index += 1
                continue

        out.append(char)
        index += 1

    return "".join(out)


def load_definition(path: Path) -> dict:
    """定義ファイルを読み込む。"""
    text = path.read_text(encoding="utf-8")
    try:
        return json.loads(strip_jsonc(text))
    except json.JSONDecodeError as error:
        raise DefinitionError(f"{path}: JSON として解釈できません: {error}") from error


def settings_path(definition: Path, document: dict) -> Path | None:
    """カタログ定義が指す設定ファイルのパスを返す。参照がない場合は None を返す。"""
    reference = document.get(SETTINGS_REFERENCE)
    if reference is None:
        return None
    if not isinstance(reference, str):
        raise DefinitionError(f"{SETTINGS_REFERENCE} は定義ファイルからの相対パスを文字列で指定してください。")
    return definition.parent / reference


def load_settings(definition: Path, document: dict) -> None:
    """app 単位の設定ファイルを読み込み、カタログ定義へ取り込む。

    設定は app 内の複数のカタログが共有するため、カタログ定義とは別のファイルに置く。
    参照はカタログ定義からの相対パスで書く。
    """
    path = settings_path(definition, document)
    if path is None:
        return
    if not path.is_file():
        raise DefinitionError(f"{SETTINGS_REFERENCE} が指す設定ファイルがありません: {path}")
    document[SETTINGS_KEY] = load_definition(path)


def join_text(value) -> str:
    """文字列、または文字列の配列を 1 つの文字列にする。"""
    if isinstance(value, str):
        return value
    if isinstance(value, list):
        if not all(isinstance(item, str) for item in value):
            raise DefinitionError("文字列の配列に文字列以外が含まれています。")
        return " ".join(value)
    raise DefinitionError(f"文字列または文字列の配列である必要があります: {value!r}")


def placeholder_indices(text: str) -> list[int]:
    """書式に現れる位置指定のインデックスを返します。構文が不正な場合は例外を発生させます。

    受け付ける構文は string_catalog_render.c の render_scan_text と同一です。
    """
    found = []
    index = 0
    length = len(text)

    while index < length:
        char = text[index]

        if char == "}":
            if not text.startswith("}}", index):
                raise DefinitionError(f"対を成さない }} があります: {text!r}")
            index += 2
            continue

        if char != "{":
            index += 1
            continue

        if text.startswith("{{", index):
            index += 2
            continue

        digits = 0
        while (
            digits < INDEX_DIGITS_MAX
            and (index + 1 + digits) < length
            and text[index + 1 + digits].isdigit()
            and text[index + 1 + digits].isascii()
        ):
            digits += 1

        if digits == 0 or (index + 1 + digits) >= length or text[index + 1 + digits] != "}":
            raise DefinitionError(f"位置指定の構文が不正です: {text!r}")
        if digits > 1 and text[index + 1] == "0":
            raise DefinitionError(f"位置指定の添字に先頭のゼロは書けません: {text!r}")

        found.append(int(text[index + 1 : index + 1 + digits]))
        index += 2 + digits

    return found


def text_affix(document: dict, section: str, language: str) -> str:
    """全文字列の書式へ共通に前置、または後置する文字列を、言語別に返す。

    節そのものがなければ空文字列を返す。節はあるが指定の言語がない場合は、
    書式と同じくニュートラル言語へフォールバックする。
    """
    affixes = document.get(section)
    if affixes is None:
        return ""
    if language in affixes:
        return join_text(affixes[language])
    return join_text(affixes.get("neutral", ""))


def entry_text(document: dict, entry: dict, language: str) -> str:
    """言語別の書式へ、カタログ共通の前置と後置を結合した文字列を返す。

    区切りは挿入せず単純に連結する。必要な区切りは、前置と後置の文字列へ定義側が含める。
    """
    return (
        text_affix(document, TEXT_PREFIX_SECTION, language)
        + join_text(entry["texts"][language])
        + text_affix(document, TEXT_SUFFIX_SECTION, language)
    )


def has_text_affix(document: dict) -> bool:
    """カタログ共通の前置または後置が定義されているかを返す。"""
    return any(document.get(section) is not None for section in TEXT_AFFIX_SECTIONS)


def validate_export(document: dict) -> None:
    """カタログを外部へ公開する設定を検査する。"""
    scope = document.get("export")
    if scope is None:
        return

    if scope not in EXPORT_SCOPES:
        raise DefinitionError(
            f"未知の公開範囲です: {scope}。{' または '.join(EXPORT_SCOPES)} を指定してください。"
        )

    settings = document.get(SETTINGS_KEY, {}).get("export")
    if settings is None:
        raise DefinitionError(
            f"export を指定する場合は、{SETTINGS_REFERENCE} が指す設定ファイルへ export を記載してください。"
        )

    for key in ("prefix", "header"):
        if key not in settings:
            raise DefinitionError(f"設定ファイルの export に {key} がありません。")
        if not isinstance(settings[key], str) or not settings[key]:
            raise DefinitionError(f"設定ファイルの export の {key} は空でない文字列で指定してください。")

    # 接頭辞からマクロ名を導く。cplat/base/dll_exports.h が定める規約と同じ形にする。
    prefix = settings["prefix"]
    if not prefix.isupper() or not prefix.replace("_", "").isalnum():
        raise DefinitionError(f"設定ファイルの export の prefix は英大文字と数字で指定してください: {prefix}")


def validate_context(document: dict) -> None:
    """app が定める文脈引数の設定を検査する。"""
    section = document.get(SETTINGS_KEY, {}).get(CONTEXT_SECTION)
    if section is None:
        return

    if not isinstance(section, dict):
        raise DefinitionError(f"設定ファイルの {CONTEXT_SECTION} はオブジェクトで指定してください。")

    headers = section.get("headers", [])
    if not isinstance(headers, list) or not all(isinstance(header, str) for header in headers):
        raise DefinitionError(f"設定ファイルの {CONTEXT_SECTION} の headers は文字列の配列で指定してください。")

    # 文脈引数を持つのはトレース種別だけ。設定ファイルは app 単位で種別をまたいで共有するため、
    # ほかの種別から参照された場合は誤りとせず、ここで読み飛ばす。
    if not is_trace(document):
        return

    # 予約した番号空間は常に確保するため、記載が 0 個でも誤りとしない。
    # app が文脈引数を持たない段階で headers だけを書いておく使い方を許す。
    arguments = section.get("arguments", [])
    if not isinstance(arguments, list):
        raise DefinitionError(f"設定ファイルの {CONTEXT_SECTION} の arguments は配列で指定してください。")

    if len(arguments) > EXTENSION_ARGUMENT_MAX:
        raise DefinitionError(
            f"app が定める文脈引数が上限 {EXTENSION_ARGUMENT_MAX} 個を超えています: {len(arguments)} 個。"
        )

    library_names = {argument["name"] for argument in CONTEXT_ARGUMENTS}
    seen: set[str] = set()

    for argument in arguments:
        if not isinstance(argument, dict):
            raise DefinitionError(f"設定ファイルの {CONTEXT_SECTION} の arguments はオブジェクトの配列です。")


        for key in ("name", "kind", "description", "inline_value"):
            if key not in argument:
                raise DefinitionError(f"app が定義するコンテキスト引数に {key} がありません。")
            if not isinstance(argument[key], str) or not argument[key]:
                raise DefinitionError(f"app が定義するコンテキスト引数の {key} は空でない文字列で指定してください。")

        # 値は実行時に取得する式だけを許す。マクロ経由にすると _with_source の仮引数が
        # app の設定によって変わり、生成物の関数シグネチャが読み取りにくくなる。
        if "macro_value" in argument:
            raise DefinitionError(
                f"app が定義するコンテキスト引数では macro_value を指定できません: {argument['name']}。"
                "inline_value を使用してください。"
            )

        if argument["kind"] not in ARGUMENT_TYPES:
            raise DefinitionError(f"未知の引数種別です: {argument['kind']}")

        if argument["name"] in library_names:
            raise DefinitionError(
                f"app が定義するコンテキスト引数の名前 {argument['name']} が、cplat が定義するコンテキスト引数と重複しています。"
            )
        if argument["name"] in seen:
            raise DefinitionError(f"app が定義するコンテキスト引数の名前が重複しています: {argument['name']}")
        seen.add(argument["name"])



def validate_text_affixes(document: dict) -> None:
    """全文字列の書式へ共通に前置、後置する文字列の設定を検査する。"""
    for section in TEXT_AFFIX_SECTIONS:
        affixes = document.get(section)
        if affixes is None:
            continue

        if not isinstance(affixes, dict):
            raise DefinitionError(f"{section} は言語をキーとしたオブジェクトで指定してください。")

        # 書式と同じ規則とする。指定のない言語は、ニュートラル言語へフォールバックする。
        if "neutral" not in affixes:
            raise DefinitionError(f"{section} に neutral が必要です。")

        for language, value in affixes.items():
            if language not in LANGUAGES:
                raise DefinitionError(f"{section}: ライブラリが扱わない言語です: {language}")
            if not isinstance(value, (str, list)) or (
                isinstance(value, list) and not all(isinstance(item, str) for item in value)
            ):
                raise DefinitionError(f"{section} の {language} は文字列または文字列の配列で指定してください。")

            # 位置指定は構文だけを検査し、添字が値を受け取るかどうかは検査しない。
            # 共通の前置と後置は、引数定義が異なる複数の文字列へ結合されるため。
            placeholder_indices(join_text(value))


def validate(document: dict) -> list[dict]:
    """定義の内容を検査し、文字列の一覧を返す。"""
    for key in ("strings",):
        if key not in document:
            raise DefinitionError(f"必須の項目がありません: {key}")

    if catalog_kind(document) not in CATALOG_KINDS:
        raise DefinitionError(
            f"未知のカタログ種別です: {document['kind']}。{' または '.join(CATALOG_KINDS)} を指定してください。"
        )

    validate_export(document)
    validate_context(document)
    validate_text_affixes(document)

    strings = document["strings"]
    if not strings:
        raise DefinitionError("strings が空です。")

    trace = is_trace(document)
    context_names = {argument["name"] for argument in context_arguments(document)}
    seen_keys: set[str] = set()
    suffixes = MODULE_FUNCTION_SUFFIXES + (TRACE_FUNCTION_SUFFIXES if trace else ())
    reserved_names = {f"{document['module_prefix']}_{suffix}" for suffix in suffixes}

    for entry in strings:
        # 分類値の書き方は種別で異なる。トレース種別はトレース レベルの名前で記載する。
        classifier = "level" if trace else "category"

        # id は処理では意味を持たない補足の文字列のため、message 種別では必須にしない。
        # トレース種別では、言語の設定によらず項目を識別する文字列として必須とする。
        required = ("key", classifier, "brief", "arguments", "texts", "notes")
        for key in (required + ("id",)) if trace else required:
            if key not in entry:
                raise DefinitionError(f"{entry.get('key', '?')}: 必須の項目がありません: {key}")

        unexpected = "category" if trace else "level"
        if unexpected in entry:
            raise DefinitionError(
                f"{entry['key']}: {catalog_kind(document)} 種別では {unexpected} を指定できません。"
                f"{classifier} を使用してください。"
            )

        if trace:
            if entry["level"] not in TRACE_LEVELS:
                raise DefinitionError(
                    f"{entry['key']}: 未知のトレース レベルです: {entry['level']}。"
                    f"{'、'.join(TRACE_LEVELS)} のいずれかを指定してください。"
                )
        # 分類値は直接の値とする。生成物を特定の app の列挙から独立させるため。
        elif not isinstance(entry["category"], int) or isinstance(entry["category"], bool):
            raise DefinitionError(f"{entry['key']}: category は整数で指定してください。")

        for key in ("key", "brief"):
            if not isinstance(entry[key], str):
                raise DefinitionError(f"{entry['key']}: {key} は文字列で指定してください。")

        if "id" in entry and not isinstance(entry["id"], str):
            raise DefinitionError(f"{entry['key']}: id は文字列で指定してください。")


        # 長文は 1 行が長くなるため、文字列の配列でも書けるようにする。連結は join_text が行う。
        for key in ("details", "remarks"):
            if key in entry and (
                not isinstance(entry[key], (str, list))
                or (isinstance(entry[key], list) and not all(isinstance(item, str) for item in entry[key]))
            ):
                raise DefinitionError(f"{entry['key']}: {key} は文字列または文字列の配列で指定してください。")

        # id の重複は検査しない。id は処理で項目を識別しないため。
        if entry["key"] in seen_keys:
            raise DefinitionError(f"文字列キーが重複しています: {entry['key']}")
        seen_keys.add(entry["key"])

        # 型付きラッパーは接頭辞を持たずモジュール接頭辞の名前空間に収まるため、
        # 同じ生成物が出す簡易関数 (@MODULE@_category など) と名前が衝突しうる。
        wrapper = wrapper_name(entry["key"])
        if wrapper in reserved_names:
            raise DefinitionError(
                f"{entry['key']}: 型付きラッパー名 {wrapper} が同じ生成物の簡易関数名と衝突しています。"
            )

        arguments = entry["arguments"]
        argument_max = user_argument_max(document)
        if len(arguments) > argument_max:
            raise DefinitionError(f"{entry['key']}: 引数が上限 {argument_max} 個を超えています。")

        for argument in arguments:
            for key in ("kind", "name", "description"):
                if key not in argument:
                    raise DefinitionError(f"{entry['key']}: 引数に {key} がありません。")
            if argument["kind"] not in ARGUMENT_TYPES:
                raise DefinitionError(f"{entry['key']}: 未知の引数種別です: {argument['kind']}")
            if not isinstance(argument["name"], str) or not isinstance(argument["description"], str):
                raise DefinitionError(f"{entry['key']}: 引数の name と description は文字列で指定してください。")
            # コンテキスト引数は生成器が付け加えるため、同じ名前の引数を利用者が定義すると仮引数が重複する。
            if trace and argument["name"] in context_names:
                raise DefinitionError(
                    f"{entry['key']}: 引数名 {argument['name']} は生成器が付け加えるコンテキスト引数と重複しています。"
                )

        for section in ("texts", "notes"):
            if "neutral" not in entry[section]:
                raise DefinitionError(f"{entry['key']}: {section} に neutral が必要です。")
            for language in entry[section]:
                if language not in LANGUAGES:
                    raise DefinitionError(f"{entry['key']}: ライブラリが扱わない言語です: {language}")

        allowed = allowed_placeholder_indices(document, entry)
        for language, text in entry["texts"].items():
            indices = placeholder_indices(join_text(text))
            for found in indices:
                if found not in allowed:
                    if not trace:
                        raise DefinitionError(
                            f"{entry['key']}: {language} の位置指定 {{{found}}} が"
                            f"引数個数 {len(arguments)} を超えています。"
                        )
                    raise DefinitionError(
                        f"{entry['key']}: {language} の位置指定 {{{found}}} に引数がありません。"
                        f"利用者の引数は 0 から {len(arguments) - 1 if arguments else -1}、"
                        f"コンテキスト引数は {'、'.join(str(index) for index in context_argument_indices(document))} です。"
                    )


    return strings


GENERATED_NOTE =""" *  本ヘッダーと `{source}` は、カタログ定義 `{definition}` から自動生成されたファイルです。\\n
 *  列挙型とテーブルは 1 組の生成単位のため、常に同時に生成してください。\\n
 *  手作業で直接編集せず、生成元の定義を変更してから `app/cplat/bin_internal/string_catalog_gen.py` を実行してください。"""


def c_string(text: str) -> str:
    """C の文字列リテラルへ変換する。"""
    escaped = text.replace("\\", "\\\\").replace('"', '\\"')
    return f'"{escaped}"'


def language_constant(document: dict, language: str) -> str:
    """言語のキーを、ライブラリの列挙定数へ変換する。"""
    return f"{LIBRARY_PREFIX.upper()}_LANGUAGE_{language.upper()}"


def kind_constant(document: dict, kind: str) -> str:
    """引数種別のキーを、ライブラリの列挙定数へ変換する。"""
    return f"{LIBRARY_PREFIX.upper()}_ARGUMENT_KIND_{kind}"


def catalog_kind(document: dict) -> str:
    """カタログ定義の種別を返す。記載がない場合は message として扱う。"""
    return document.get("kind", CATALOG_KIND_MESSAGE)


def is_trace(document: dict) -> bool:
    """カタログ定義がトレース種別かどうかを返す。"""
    return catalog_kind(document) == CATALOG_KIND_TRACE


def trace_level_value(level: str) -> int:
    """トレース レベルの名前を、分類値として保持する整数へ変換する。"""
    return TRACE_LEVELS.index(level)


def extension_arguments(document: dict) -> list[dict]:
    """app が定義するコンテキスト引数の一覧を返す。設定がない場合は空を返す。

    設定ファイルは app 単位で、種別の異なるカタログが共有する。
    コンテキスト引数を持つのはトレース種別だけのため、それ以外では読み飛ばす。
    """
    if not is_trace(document):
        return []
    return list(document.get(SETTINGS_KEY, {}).get(CONTEXT_SECTION, {}).get("arguments", []))


def context_argument_slots(document: dict) -> list[tuple[int, dict]]:
    """(位置指定, 引数) の組を、位置指定の昇順で返す。

    cplat が定義する組は基底から連続し、app が定義する組は拡張の基底から記述順に並ぶ。
    可変長引数はこの並びの順に渡すため、出力処理はいずれもこの関数を経由する。
    """
    if not is_trace(document):
        return []

    slots = [(CONTEXT_ARGUMENT_BASE + offset, argument) for offset, argument in enumerate(CONTEXT_ARGUMENTS)]
    slots.extend(
        (EXTENSION_ARGUMENT_BASE + position, argument)
        for position, argument in enumerate(extension_arguments(document))
    )
    return sorted(slots, key=lambda slot: slot[0])


def context_arguments(document: dict) -> list[dict]:
    """すべてのコンテキスト引数を、位置指定の昇順に平坦化して返す。"""
    return [argument for _, argument in context_argument_slots(document)]


def context_argument_indices(document: dict) -> list[int]:
    """すべてのコンテキスト引数の位置指定を、昇順に返す。"""
    return [index for index, _ in context_argument_slots(document)]


def context_argument_count(document: dict) -> int:
    """生成器が付け加えるコンテキスト引数の個数を返す。"""
    return len(context_arguments(document))


def user_argument_max(document: dict) -> int:
    """利用者がカタログ定義へ記載できる引数の上限を返す。"""
    return CONTEXT_ARGUMENT_BASE if is_trace(document) else ARGUMENT_MAX


def argument_array_length(document: dict, entry: dict) -> int:
    """生成物の引数配列の要素数を返す。

    トレース種別では、コンテキスト引数のために予約した番号空間の全体を確保する。
    app が定義するコンテキスト引数を増減しても要素数が変わらず、未指定の番号は
    値を受け取らないインデックスとして残る。
    """
    if not is_trace(document):
        return len(entry["arguments"])
    return ARGUMENT_MAX


def allowed_placeholder_indices(document: dict, entry: dict) -> set[int]:
    """書式の位置指定が指してよいインデックスを返す。"""
    return set(range(len(entry["arguments"]))) | set(context_argument_indices(document))


# ACCESSOR_DECLARATIONS (および SOURCE_TAIL) が @MODULE@_ に続けて出力する簡易関数の名前一覧。
# 型付きラッパーの関数名が、これらの簡易関数と衝突していないかの検査に使う。
# 一覧はテンプレートの実際の出力に合わせるため、テンプレートを変更した場合はここも見直す。
MODULE_FUNCTION_SUFFIXES = (
    "entries",
    "entry_count",
    "entry",
    "key_index",
    "key_index_count",
    "catalog",
    "format",
    "vformat",
    "verify",
    "category",
    "id",
    "note",
)

# トレース種別の生成物だけが追加で定義する関数の接尾辞。
TRACE_FUNCTION_SUFFIXES = (
    "write",
    "set_tracer",
    "get_tracer",
    "key_names",
    "key_name_count",
    "create_filter",
    "set_filter",
    "get_filter",
)


def wrapper_name(string_key: str) -> str:
    """文字列キーから型付きラッパーの関数名を導出する。

    ライブラリ側の接頭辞は前置せず、文字列キーの定数名をそのまま小文字化した名前にする。
    文字列キーにはカタログ定義ごとのモジュール接頭辞が含まれるため、
    追加の導出規則なしに利用側の名前空間へ収まる。
    ライブラリ側の接頭辞を前置しないのは、利用者が定義した関数がライブラリのリンカー名前空間を
    名乗ってしまうのを避けるため。
    """
    return string_key.lower()


def doc_lines(text: str, indent: str, width: int = 112) -> list[str]:
    """Doxygen 本文の 1 段落を、指定幅で折り返した行にする。"""
    words = text.split()
    lines: list[str] = []
    current = indent

    for word in words:
        candidate = f"{current}{word}" if current == indent else f"{current} {word}"
        if current != indent and len(candidate) > width:
            lines.append(current)
            current = f"{indent}{word}"
        else:
            current = candidate

    if current != indent:
        lines.append(current)
    return lines


def parameter_declaration(argument: dict) -> str:
    """引数定義から、型付きラッパーの仮引数の宣言を組み立てる。"""
    c_type = ARGUMENT_TYPES[argument["kind"]]
    if c_type.endswith("*"):
        return f"{c_type}{argument['name']}"
    return f"const {c_type} {argument['name']}"


def format_par_lines(document: dict, entry: dict) -> list[str]:
    """言語別の書式を @par として並べる。

    カタログ共通の前置と後置は結合した形で示す。生成物が保持する書式と一致させるため。
    """
    lines = ["     *  @par            書式"]
    texts = entry["texts"]
    languages = [language for language in LANGUAGES if language in texts]
    for position, language in enumerate(languages):
        suffix = "\\n" if position < (len(languages) - 1) else ""
        lines.append(f"     *  `{entry_text(document, entry, language)}`{suffix}")
    return lines


def remark_doc_lines(entry: dict) -> list[str]:
    """定義の remarks を @remark の行にする。"""
    remark_text = join_text(entry["remarks"]) if entry.get("remarks") else ""
    if not remark_text:
        return []

    remark_indent = "     *" + " " * 18
    remark_lines = doc_lines(remark_text, remark_indent)
    return [f"     *  @remark         {remark_lines[0][len(remark_indent):]}"] + remark_lines[1:]


def trace_context_doc_lines(document: dict) -> list[str]:
    """コンテキスト引数の位置指定と内容の対応表を、ヘッダーのファイル コメント用に組み立てます。

    定義作成者が書式からコンテキスト値を参照するには、どの番号が何かを把握する必要があります。
    対応表は解決済みのコンテキスト引数から組み立て、生成器の定義と不整合が生じないようにします。
    """
    indices = context_argument_indices(document)
    lines = [
        " *  本カタログはトレース種別です。呼び出し位置と実行コンテキストを、生成器が引数として付与します。\\n",
        f" *  利用者が記載した引数は `{{0}}` から順に並び、"
        f"`{{{CONTEXT_ARGUMENT_BASE}}}` から次のコンテキスト引数が並びます。",
        " *",
        " *  | 位置指定 | 引数名 | 引数種別 | 値 |",
        " *  | --- | --- | --- | --- |",
    ]

    for index, argument in zip(indices, context_arguments(document)):
        kind = kind_constant({}, argument["kind"])
        lines.append(f" *  | `{{{index}}}` | {argument['name']} | {kind} | {argument['description']} |")

    lines.extend(
        [
            " *",
            " *  言語別の書式へこれらの位置指定を書くと、組み立てた文字列へコンテキスト値が現れます。\\n",
            " *  書かない場合は現れません。実装を変えずに、定義の変更だけで切り替えられます。",
            " *",
            f" *  記載した引数の個数から `{{{CONTEXT_ARGUMENT_BASE - 1}}}` までは、値を受け取らないインデックスです。\\n",
            " *  書式から参照すると定義の誤りになります。使用できるのは利用者の引数と、上の表の位置指定です。",
        ]
    )

    # 公開するカタログでは、app が定義するコンテキスト引数の取得式が利用側の翻訳単位で評価される。
    # 取得関数の公開漏れはリンク時まで現れないため、生成物の側でも注意を促す。
    if (document.get("export") is not None) and extension_arguments(document):
        names = "、".join(f"`{argument['name']}`" for argument in extension_arguments(document))
        lines.extend(
            [
                " *",
                f" *  {names} の取得式は、本ヘッダーの `static inline` の中で展開されます。\\n",
                " *  利用側の翻訳単位から呼び出されるため、取得関数はライブラリの外部へ公開する必要があります。\\n",
                " *  公開しない場合、利用側のリンクが失敗します。",
            ]
        )

    return lines


def emit_trace_wrapper(document: dict, entry: dict) -> str:
    """1 件分のトレース出力ラッパーを、マクロとともに書き出す。"""
    name = wrapper_name(entry["key"])
    module = document["module_prefix"]
    arguments = entry["arguments"]
    context = context_arguments(document)
    # マクロから受け取る引数だけが _with_source の仮引数になる。実行時に取る引数は内部で評価する。
    macro_context = [argument for argument in context if "macro_value" in argument]
    user_names = [argument["name"] for argument in arguments]

    names = user_names + [argument["name"] for argument in macro_context]
    name_width = max(len(name_item) for name_item in names) + 1
    # `     *  ` の 8 文字と、`@param[in]      ` の 16 文字のあとに名前欄が並ぶ
    continuation = "     *" + " " * (8 + 16 + name_width - 6)

    lines = ["    /**"]
    lines.append(f"     *  @brief          {entry['brief']}")
    lines.append("     *")
    # 1 文ずつ改行する。長い 1 行にすると、整形時に @c と対象の間で折り返される。
    lines.append(f"     *  関数形式マクロ @c {name} の実体です。\\n")
    lines.append("     *  呼び出し位置は展開の位置で確定する必要があるため、マクロから受け取ります。\\n")
    lines.append("     *  呼び出し側はマクロを使用してください。")
    lines.append("     *")

    for argument in arguments + macro_context:
        padded = argument["name"].ljust(name_width)
        lines.append(f"     *  @param[in]      {padded}{argument['description']}")
        lines.append(f"{continuation}引数種別は @c {kind_constant(document, argument['kind'])} です。")

    lines.append(f"     *  @return         戻り値は @c {module}_write と同じです。")
    lines.append("     */")

    parameters = [parameter_declaration(argument) for argument in arguments + macro_context]

    # 可変長引数は引数配列のインデックス順に並べる必要があるため、文脈引数を 1 本の並びのまま回す。
    # マクロ経由の引数と実行時に取る引数を別々に連結すると、両者が交互に並んだ場合に順序が狂う。
    call = [entry["key"]]
    call.extend(user_names)
    call.extend(
        argument["name"] if "macro_value" in argument else argument["inline_value"] for argument in context
    )

    lines.append(f"    static inline int {name}_with_source({', '.join(parameters)})")
    lines.append("    {")
    lines.append(f"        return {module}_write({', '.join(call)});")
    lines.append("    }")
    lines.append("")

    macro_names = list(user_names)
    macro_width = (max(len(name_item) for name_item in macro_names) + 1) if macro_names else 1
    macro_continuation = "     *" + " " * (8 + 16 + macro_width - 6)

    lines.append("    /**")
    lines.append(f"     *  @brief          {entry['brief']}")
    lines.append("     *")
    details_text = join_text(entry["details"]) if entry.get("details") else ""
    if details_text:
        lines.extend(doc_lines(details_text, "     *  "))
        lines.append("     *")
    lines.append("     *  ソース ファイル、行番号、関数名、プロセス ID、スレッド ID を呼び出しごとに付けて出力します。")
    lines.append(f"     *  出力先は @c {module}_set_tracer で設定したトレーサーです。")
    lines.append("     *")
    for argument in arguments:
        padded = argument["name"].ljust(macro_width)
        lines.append(f"     *  @param[in]      {padded}{argument['description']}")
        lines.append(f"{macro_continuation}引数種別は @c {kind_constant(document, argument['kind'])} です。")
    lines.append(f"     *  @return         戻り値は @c {module}_write と同じです。")
    lines.extend(remark_doc_lines(entry))
    lines.extend(format_par_lines(document, entry))
    lines.append("     */")

    macro_tail = ", ".join(argument["macro_value"] for argument in macro_context)
    macro_arguments = "".join(f"({name_item}), " for name_item in macro_names) + macro_tail
    lines.append(f"#define {name}({', '.join(macro_names)}) {name}_with_source({macro_arguments})")

    return "\n".join(lines)


def emit_wrapper(document: dict, entry: dict) -> str:
    """1 件分の型付きラッパーを、Doxygen コメントとともに書き出す。"""
    if is_trace(document):
        return emit_trace_wrapper(document, entry)

    name = wrapper_name(entry["key"])
    arguments = entry["arguments"]

    names = ["dest", "dest_size"] + [argument["name"] for argument in arguments]
    name_width = max(len(name) for name in names) + 1
    # `     *  ` の 8 文字と、`@param[out]     ` の 16 文字のあとに名前欄が並ぶ
    continuation = "     *" + " " * (8 + 16 + name_width - 6)

    lines = ["    /**"]
    lines.append(f"     *  @brief          {entry['brief']}")
    lines.append("     *")
    details_text = join_text(entry["details"]) if entry.get("details") else ""
    if details_text:
        lines.extend(doc_lines(details_text, "     *  "))
    if not arguments:
        lines.extend(doc_lines("この文字列は引数を必要としません。", "     *  "))
    if details_text or not arguments:
        lines.append("     *")
    lines.append(
        f"     *  @param[out]     {'dest'.ljust(name_width)}文字列の格納先バッファー。NULL を渡してはなりません。"
    )
    lines.append(
        f"     *  @param[in]      {'dest_size'.ljust(name_width)}@p dest のバイト数。1 以上を指定してください。"
    )

    for argument in arguments:
        padded = argument["name"].ljust(name_width)
        lines.append(f"     *  @param[in]      {padded}{argument['description']}")
        lines.append(f"{continuation}引数種別は @c {kind_constant(document, argument['kind'])} です。")

    module = document["module_prefix"]
    lines.append(f"     *  @return         戻り値は @c {module}_format と同じです。")

    lines.extend(remark_doc_lines(entry))
    lines.extend(format_par_lines(document, entry))
    lines.append("     */")

    parameters = ["char *dest", "const size_t dest_size"]
    parameters.extend(parameter_declaration(argument) for argument in arguments)

    # カタログ オブジェクトを補う簡易関数を経由する。ラッパーを展開する呼び出し側が、
    # cplat の関数と cplat_string_catalog のレイアウトへ依存しないようにするため。
    call = ["dest", "dest_size", entry["key"]]
    call.extend(argument["name"] for argument in arguments)

    lines.append(f"    static inline int {name}({', '.join(parameters)})")
    lines.append("    {")
    lines.append(f"        return {module}_format({', '.join(call)});")
    lines.append("    }")

    return "\n".join(lines)


def export_macros(document: dict) -> dict[str, str]:
    """エクスポート装飾の目印に対する置換文字列を返す。

    目印は空でない場合に末尾の空白を含む。公開しない場合は空文字列となり、
    装飾のない宣言がそのまま残る。
    """
    empty = {"@EXPORT@": "", "@API@": "", "@EXPORT_FULL@": "", "@API_FULL@": ""}
    scope = document.get("export")
    if scope is None:
        return empty

    prefix = document[SETTINGS_KEY]["export"]["prefix"]
    macros = dict(empty)
    macros["@EXPORT@"] = f"{prefix}_EXPORT "
    macros["@API@"] = f"{prefix}_API "
    if scope == EXPORT_SCOPE_FULL:
        macros["@EXPORT_FULL@"] = macros["@EXPORT@"]
        macros["@API_FULL@"] = macros["@API@"]
    return macros


def expand(template: str, module: str, library: str, macros: dict[str, str] | None = None) -> str:
    """テンプレート中の接頭辞の目印を置き換える。

    テンプレートは C のコードを含み波括弧が現れるため、str.format は使わない。
    """
    text = (
        template.replace("@MODULE_UPPER@", module.upper())
        .replace("@MODULE@", module)
        .replace("@LIBRARY@", library)
    )
    for marker, replacement in (macros or export_macros({})).items():
        text = text.replace(marker, replacement)
    return text


def derive_module_prefix(definition: Path) -> str:
    """定義ファイルの名前から、モジュール接頭辞を導出する。

    生成物のファイル名と、カタログを省略する口の名前がここから決まる。
    C の識別子の一部になるため、英小文字で始まる snake_case だけを認める。
    """
    prefix = definition.stem
    if re.fullmatch(r"[a-z][a-z0-9_]*", prefix) is None:
        raise DefinitionError(
            "定義ファイルの名前をモジュール接頭辞に使います。"
            f"英小文字で始まり、英小文字、数字、下線だけで綴ってください: {definition.name}"
        )
    return prefix


def key_enum_name(document: dict) -> str:
    """文字列キーの列挙名を導出する。

    モジュール接頭辞に `_key` を続ける。定義ファイルには書かない。
    """
    return f"{document['module_prefix']}_key"


def derive_module_dir(definition: Path) -> str:
    """定義ファイルの位置から、app 直下を起点としたディレクトリを導出する。

    app の配下は `prod/` と `test/` に分かれる。定義ファイルの絶対パスから、
    最も近い `prod` または `test` を探し、そこからの部分をディレクトリとする。
    どちらも無い場所に置いた場合は `.` とする。
    """
    parts = definition.resolve().parent.parts
    for anchor in ("prod", "test"):
        if anchor in parts:
            index = len(parts) - 1 - parts[::-1].index(anchor)
            return "/".join(parts[index:])
    return "."


def header_include_path(header_dir: Path, module: str) -> str | None:
    """公開ヘッダーとして取り込む場合の include パスを返す。

    出力先が公開ヘッダーの置き場所 (prod/include/ 配下) である場合、そこからの相対パスが
    利用側の `#include <...>` に現れる。置き場所から導けない場合は None を返す。
    """
    parts = header_dir.resolve().parts
    for marker in ("include", "include_internal"):
        if marker in parts:
            index = len(parts) - 1 - parts[::-1].index(marker)
            return "/".join(parts[index + 1 :] + (f"{module}.h",))
    return None


def output_dir_display(document: dict, out_relative: str) -> str:
    """生成物の置き場所を、リポジトリ相対のディレクトリとして表す。

    out_relative は、定義ファイルの置き場所から見た出力先の相対パス。
    Doxygen の @file は実際のパスと一致している必要がある。
    """
    module_dir = document.get("module_dir", ".")
    if out_relative in ("", "."):
        return module_dir
    return f"{module_dir}/{out_relative}"


def emit_header(document: dict, strings: list[dict], definition_name: str, out_relative: str = ".") -> str:
    """ヘッダー側の生成物を組み立てます。"""
    module = document["module_prefix"]
    library = LIBRARY_PREFIX
    macros = export_macros(document)
    guard = f"{module.upper()}_H"
    header_name = f"{module}.h"
    source_name = f"{module}.c"
    module_dir = document.get("module_dir", ".")
    output_dir = output_dir_display(document, out_relative)
    include_path = header_name if output_dir == module_dir else f"{out_relative}/{header_name}"
    public_include = document.get(PUBLIC_INCLUDE_KEY)

    out = [
        "/**",
        " " + "*" * 79,
        f" *  @file           {header_name}",
        f" *  @brief          利用者が定義する文字列キーの列挙型と、カタログ取得関数を宣言します。",
        f" *  @author         {document.get('author', '')}",
        f" *  @date           {document.get('date', '')}",
        f" *  @version        {document.get('version', '')}",
        " *",
        *(
            [
                f" *  本ヘッダーは `{output_dir}/` の公開ヘッダーです。\\n",
                f" *  利用側は `#include <{public_include}>` でインクルードします。",
            ]
            if public_include is not None
            else [
                f" *  本ヘッダーは `{output_dir}/` のモジュール私有ヘッダーです。\\n",
                f' *  `{module_dir}/` の実装ファイルからのみ `#include "{include_path}"` でインクルードします。',
            ]
        ),
        " *",
        GENERATED_NOTE.format(source=source_name, definition=definition_name),
        " *",
        " *  この 2 ファイルは、文字列カタログを利用するアプリケーション側で用意するファイルです。\\n",
        " *  言語、引数種別、書式構文はライブラリ側で規定されます。\\n",
        " *  分類値の意味付けは利用側の取り決めであり、別ヘッダーで個別に定義します。",
        " *",
        *(
            [
                " *  本カタログは、全文字列の書式へ共通の前置および後置を結合しています。\\n",
                " *  本ファイルが示す書式は、結合後の文字列です。",
                " *",
            ]
            if has_text_affix(document)
            else []
        ),
    ]

    if is_trace(document):
        out.extend(trace_context_doc_lines(document))
        out.append(" *")

    out += [
        " *  列挙名や関数名はカタログ定義に基づいて決まり、ライブラリの接頭辞とは異なる名前空間に属します。\\n",
        " *  ライブラリ側ではこれらの名前を定義せず、文字列キーを `int` 型として受け取ります。",
        " *",
        f" *  @copyright      Copyright (C) {document.get('author', '')}. 2026. All rights reserved.",
        " *",
        " " + "*" * 79,
        " */",
        "",
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
    ]

    group_id = module.upper()
    group_title = f"文字列カタログ ({module})"

    includes = [f"#include <{LIBRARY_HEADER}>"]
    # トレース種別は、出力先のハンドルと、文脈引数の値を取得する API を参照する。
    includes.extend(f"#include <{header}>" for header in (TRACE_HEADERS if is_trace(document) else ()))
    # 公開するカタログは、app のエクスポート マクロを定義するヘッダーを参照する。
    if document.get("export") is not None:
        includes.append(f"#include <{document[SETTINGS_KEY]['export']['header']}>")
    # app が定める文脈引数は、その取得式が必要とするヘッダーを参照する。
    if extension_arguments(document):
        section = document[SETTINGS_KEY][CONTEXT_SECTION]
        includes.extend(f"#include <{header}>" for header in section.get("headers", []))

    out.extend(
        includes
        + [
            "#include <stdarg.h>",
            "#include <stddef.h>",
            "#include <stdint.h>",
            "",
            "/**",
            f" *  @defgroup       {group_id} {group_title}",
            f" *  @brief          カタログ定義 `{definition_name}` から自動生成された文字列カタログです。",
            " *  @{",
            " */",
            "",
            "#ifdef __cplusplus",
            'extern "C"',
            "{",
            "#endif /* __cplusplus */",
            "",
            "    /**",
            "     *  @brief          カタログに登録された文字列を識別する列挙型です。",
            "     *",
            "     *  各 ID の引数定義、分類値、説明文、言語別の書式および備考は、同一の生成単位のテーブルで保持します。\\n",
            "     *  列挙定数の名前はカタログ定義の key で、処理から文字列を参照する識別子です。\\n",
            "     *  列挙値は、定義の並び順から 1 始まりで生成器が決めます。\\n",
            "     *  定義の id は処理では意味を持たないため、列挙には現れません。",
            "     */",
            f"    typedef enum {key_enum_name(document)}",
            "    {",
        ]
    )

    for position, entry in enumerate(strings):
        comma = "," if position < (len(strings) - 1) else ""
        out.append(f"        {entry['key']} = {position + 1}{comma} /**< {entry['brief']} */")

    out.append(f"    }} {key_enum_name(document)};")
    out.append("")
    out.append(expand(ACCESSOR_DECLARATIONS, module, library, macros))
    if is_trace(document):
        out.append(expand(TRACE_WRITE_DECLARATION, module, library, macros))
    out.extend(
        [
            "#ifdef __cplusplus",
            "}",
            "#endif /* __cplusplus */",
            "",
        ]
    )

    wrapper_group_id = f"{group_id}_TYPED_FORMATTERS"
    wrapper_group_title = (
        "文字列キーごとの型付きトレース出力関数" if is_trace(document) else "文字列キーごとの型付き組み立て関数"
    )
    wrapper_group_brief = (
        "文字列キーごとに引数の型を固定したトレース出力関数です。"
        if is_trace(document)
        else "文字列キーごとに引数の型を固定した組み立て関数です。"
    )
    out.extend(
        [
            "/**",
            f" *  @defgroup       {wrapper_group_id} {wrapper_group_title}",
            f" *  @brief          {wrapper_group_brief}",
            f" *  @ingroup        {group_id}",
            " *  @{",
            " */",
            "",
            "#ifdef __cplusplus",
            'extern "C"',
            "{",
            "#endif /* __cplusplus */",
            "",
        ]
    )

    for entry in strings:
        out.append(emit_wrapper(document, entry))
        out.append("")

    out.extend(
        [
            "#ifdef __cplusplus",
            "}",
            "#endif /* __cplusplus */",
            "",
            "/** @} */",
            "",
            "/** @} */",
            "",
            f"#endif /* {guard} */",
            "",
        ]
    )

    return "\n".join(out)


ACCESSOR_DECLARATIONS = """\
    /**
     *  @brief          カタログ配列の先頭を取得します。
     *  @return         カタログ配列の先頭ポインターです。NULL は返しません。
     *
     *  返されるポインターは静的領域を指しているため、呼び出し側で解放してはなりません。\\n
     *  @c @MODULE@_entry_count とともに @c @LIBRARY@ を構築するための構成要素です。\\n
     *  構築済みのカタログ オブジェクトを取得する場合は @c @MODULE@_catalog を使用してください。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    @EXPORT_FULL@const @LIBRARY@_entry *@API_FULL@@MODULE@_entries(void);

    /**
     *  @brief          カタログの登録件数を取得します。
     *  @return         文字列の登録件数です。1 以上を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    @EXPORT@int @API@@MODULE@_entry_count(void);

    /**
     *  @brief          文字列キーからカタログ配列のインデックスを引くテーブルを取得します。
     *  @return         インデックス テーブルへのポインターです。NULL は返しません。
     *
     *  文字列キーをインデックスとして、カタログ配列のインデックスを格納しています。\\n
     *  未登録の文字列キーに対応する要素には負の値を格納します。\\n
     *  このテーブルを使用することで、文字列キーからカタログを引く探索を線形探索からインデックス参照へ置き換えます。
     *
     *  返されるポインターは静的領域を指しているため、呼び出し側で解放してはなりません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    @EXPORT_FULL@const int *@API_FULL@@MODULE@_key_index(void);

    /**
     *  @brief          インデックス テーブルの要素数を取得します。
     *  @return         インデックス テーブルの要素数です。最大の文字列キーに 1 を加えた値となります。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    @EXPORT_FULL@int @API_FULL@@MODULE@_key_index_count(void);

    /**
     *  @brief          本カタログ定義のカタログ識別オブジェクトを取得します。
     *  @return         カタログ識別オブジェクトへのポインターです。NULL は返しません。
     *
     *  配列とインデックス テーブルを 1 つのカタログ構造体にまとめたオブジェクトです。\\n
     *  ライブラリ側ではカタログを保持しないため、文字列組み立て API へはこのオブジェクトへのポインターを渡します。
     *
     *  返されるポインターは静的領域を指しているため、呼び出し側で解放してはなりません。\\n
     *  他のカタログ定義と組み合わせる場合は、対象に応じたカタログ オブジェクトを使い分けます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    @EXPORT_FULL@const @LIBRARY@ *@API_FULL@@MODULE@_catalog(void);

    /**
     *  @brief          文字列キーに対応するカタログ項目を取得します。
     *  @param[in]      string_key 参照する文字列のキー。
     *  @return         カタログ項目へのポインターです。見つからない場合は NULL を返します。
     *
     *  本カタログ定義から文字列キーに対応する項目を取得するための簡易関数です。\n
     *  内部で @c @MODULE@_catalog と @c @LIBRARY@_get_entry を使用します。
     *  項目の識別には @c key を使用します。@c id は処理では意味を持たない補足の文字列です。
     *  @c id、@c details、@c remarks は、定義で省略されている場合に NULL です。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    @EXPORT_FULL@const @LIBRARY@_entry *@API_FULL@@MODULE@_entry(int string_key);

    /**
     *  @brief          本カタログ定義を使用して、文字列を組み立てます。
     *  @param[out]     dest       文字列の格納先バッファー。NULL を渡してはなりません。
     *  @param[in]      dest_size  @p dest のバイト数。1 以上を指定してください。
     *  @param[in]      string_key 組み立てる文字列のキー。
     *  @param[in]      ...        引数スキーマが定める順序と型の引数リスト。
     *  @return         戻り値は @c @LIBRARY@_format と同じです。
     *
     *  カタログの指定を省略して呼び出すための簡易関数です。\\n
     *  内部で @c @MODULE@_catalog を補って @c @LIBRARY@_format を呼び出します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\\n
     *  言語設定を同時に変更しない場合は、同時に実行できます。\\n
     *  他スレッドが言語設定を変更する場合は、呼び出し側で同期してください。\\n
     *  @c @LIBRARY@_set_language で言語を設定しておらず、言語がまだ決まっていない場合は、
     *  環境変数から言語を決定します。\\n
     *  このとき、他スレッドが環境変数を同時に変更する場合は、呼び出し側で同期してください。
     */
    @EXPORT@int @API@@MODULE@_format(char *dest, size_t dest_size, int string_key, ...);

    /**
     *  @brief          本カタログ定義を使用して、@c va_list から文字列を組み立てます。
     *  @param[out]     dest       文字列の格納先バッファー。NULL を渡してはなりません。
     *  @param[in]      dest_size  @p dest のバイト数。1 以上を指定してください。
     *  @param[in]      string_key 組み立てる文字列のキー。
     *  @param[in]      args       引数スキーマが定める順序と型の値を保持する引数リスト。
     *  @return         戻り値は @c @LIBRARY@_vformat と同じです。
     *
     *  カタログの指定を省略して呼び出すための簡易関数です。\\n
     *  内部で @c @MODULE@_catalog を補って @c @LIBRARY@_vformat を呼び出します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\\n
     *  言語設定を同時に変更しない場合は、同時に実行できます。\\n
     *  他スレッドが言語設定を変更する場合は、呼び出し側で同期してください。\\n
     *  @c @LIBRARY@_set_language で言語を設定しておらず、言語がまだ決まっていない場合は、
     *  環境変数から言語を決定します。\\n
     *  このとき、他スレッドが環境変数を同時に変更する場合は、呼び出し側で同期してください。
     */
    @EXPORT@int @API@@MODULE@_vformat(char *dest, size_t dest_size, int string_key, va_list args);

    /**
     *  @brief          本カタログ定義の内容を確認します。
     *  @param[out]     string_key_out 不正を検出した文字列キーの格納先。不要な場合は NULL を指定できます。
     *  @param[out]     language_out   不正を検出した言語の格納先。不要な場合は NULL を指定できます。
     *  @return         戻り値は @c @LIBRARY@_verify と同じです。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    @EXPORT_FULL@int @API_FULL@@MODULE@_verify(int *string_key_out, @LIBRARY@_language *language_out);

    /**
     *  @brief          本カタログ定義から、文字列の分類値を取得します。
     *  @param[in]      string_key 参照する文字列のキー。
     *  @return         戻り値は @c @LIBRARY@_get_category と同じです。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    @EXPORT@int @API@@MODULE@_category(int string_key);

    /**
     *  @brief          本カタログ定義から、文字列キーに対応する ID を取得します。
     *  @param[in]      string_key 参照する文字列のキー。
     *  @return         戻り値は @c @LIBRARY@_get_id と同じです。ID が未設定の項目では NULL を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    @EXPORT@const char *@API@@MODULE@_id(int string_key);

    /**
     *  @brief          本カタログ定義から、現在の言語設定における文字列の備考を取得します。
     *  @param[in]      string_key 参照する文字列のキー。
     *  @return         戻り値は @c @LIBRARY@_get_note と同じです。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\\n
     *  言語設定を同時に変更しない場合は、同時に実行できます。\\n
     *  他スレッドが言語設定を変更する場合は、呼び出し側で同期してください。\\n
     *  @c @LIBRARY@_set_language で言語を設定しておらず、言語がまだ決まっていない場合は、
     *  環境変数から言語を決定します。\\n
     *  このとき、他スレッドが環境変数を同時に変更する場合は、呼び出し側で同期してください。
     */
    @EXPORT@const char *@API@@MODULE@_note(int string_key);

    /*
     *  ここから下は、文字列キーごとに引数の型を固定したラッパーです。
     *
     *  可変長引数を取る関数では、コンパイラが引数の個数および型を検査できません。
     *  書式および引数スキーマがカタログ内に保持されており、書式文字列が関数呼び出しの実引数ではないためです。
     *  型付きラッパーを経由することで、通常のプロトタイプ検査が働き、引数の個数や型の不整合をビルド時に検出できます。
     *
     *  実体を持つ翻訳単位を増やさないよう、`static inline` 関数として提供します。
     *  これにより、文字列キーの増加に伴って公開シンボルが増加するのを防ぎます。
     *
     *  関数名は文字列キーから機械的に導出します。
     *  ライブラリ側の接頭辞は前置せず、文字列キーの定数名を小文字化した名前をそのまま使用します。
     *  文字列キーにはカタログ定義ごとのモジュール接頭辞が含まれるため、
     *  この関数がどのカタログ定義に属するかは呼び出し側の名前から判別できます。
     *  語句の削除や順序の入れ替えは行わないため、導出規則に例外はありません。
     *  導出規則の全体は docs/architecture.md を参照してください。
     */
"""


TRACE_WRITE_DECLARATION = """\
    /**
     *  @brief          本カタログの出力先となるトレーサーを設定します。
     *  @param[in]      tracer 出力先のトレーサー ハンドル。NULL を指定すると出力しない状態へ戻します。
     *
     *  設定したトレーサーは、本カタログのすべての出力で使用します。\\n
     *  呼び出しごとにトレーサーを指定する必要をなくすため、カタログ単位で保持します。
     *
     *  トレーサーの所有権は移りません。\\n
     *  設定を外す呼び出しは、原則として不要です。トレーサーはプロセスの終了時に cplat が破棄し、
     *  その後に出力を要求する経路がないためです。
     *
     *  トレーサーを明示的に破棄し、そのあとに出力を要求しうる場合に限り、破棄の前に NULL を設定してください。\\n
     *  解放済みのハンドルを参照しないようにするためです。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\\n
     *  プロセス全体で本関数の呼び出しを直列化してください。
     */
    @EXPORT@void @API@@MODULE@_set_tracer(cplat_tracer *tracer);

    /**
     *  @brief          本カタログの出力先に設定されているトレーサーを取得します。
     *  @return         設定されているトレーサー ハンドルです。未設定の場合は NULL を返します。
     *
     *  設定を一時的に差し替える場合に、元のハンドルを保存するために使用します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\\n
     *  他スレッドが出力先の設定を同時に変更しない場合は、同時に実行できます。\\n
     *  他スレッドが出力先の設定を同時に変更する場合は、呼び出し側で同期してください。
     */
    @EXPORT@cplat_tracer *@API@@MODULE@_get_tracer(void);

    /**
     *  @brief          文字列キーの名前解決表の先頭を取得します。
     *  @return         名前解決表の先頭ポインターです。NULL は返しません。
     *
     *  列挙定数名と文字列キーの組を、カタログ定義の並び順で保持します。\\n
     *  条件式フィルターが、条件式に書いた列挙定数名を文字列キーへ解決するために使用します。\\n
     *  通常は @c @MODULE@_create_filter がこの表を使用するため、直接参照する必要はありません。
     *
     *  返されるポインターは静的領域を指しているため、呼び出し側で解放してはなりません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    @EXPORT_FULL@const @LIBRARY@_filter_key_name *@API_FULL@@MODULE@_key_names(void);

    /**
     *  @brief          文字列キーの名前解決表の要素数を取得します。
     *  @return         名前解決表の要素数です。カタログの登録件数と同じ値を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    @EXPORT_FULL@size_t @API_FULL@@MODULE@_key_name_count(void);

    /**
     *  @brief          本カタログ定義と名前解決表で、条件式フィルターのスロットを作成します。
     *  @param[in]      category_names 分類値の名前。分類値を数値だけで扱う場合は NULL を指定します。
     *                                 スロットを破棄するまで有効である必要があります。
     *  @param[in]      line_capacity  適用するフィルター オブジェクトの行数の上限。
     *  @param[in]      line_width     適用するフィルター オブジェクトの行幅。
     *  @param[out]     slot_out       作成したスロットの格納先。
     *  @return         戻り値は @c @LIBRARY@_filter_slot_create と同じです。
     *
     *  カタログ構造体と名前解決表を指定せずにスロットを作成するための簡易関数です。\\n
     *  作成したスロットは @c @MODULE@_set_filter で出力へ接続し、不要になったら
     *  @c @LIBRARY@_filter_slot_dispose で破棄します。
     *
     *  分類値はトレース レベルです。名前を付ける場合は、トレース レベルの値をインデックスとする名前を指定します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    @EXPORT@int @API@@MODULE@_create_filter(const @LIBRARY@_filter_category_names *category_names,
                                            size_t line_capacity, size_t line_width,
                                            @LIBRARY@_filter_slot **slot_out);

    /**
     *  @brief          本カタログの出力に使用する条件式フィルターのスロットを設定します。
     *  @param[in]      slot 本カタログで作成したスロット。NULL を指定すると接続を解除します。
     *  @return         成功時は @c CPLAT_OK を返します。
     *  @return         @p slot が本カタログ以外のカタログで作成されている場合は、設定を変えずに
     *                  @c CPLAT_ERR_INVALID_ARGUMENT を返します。
     *
     *  接続すると、出力のたびに条件式で判定し、いずれかの行に一致したトレースを強制出力のレベルで出力します。\\n
     *  一致しないトレースは、定義のレベルのまま出力先のしきい値で選別されます。\\n
     *  スロットにソース領域を結び付けている場合は、出力のたびに公開内容の変化を確認して取り込みます。
     *
     *  スロットの所有権は移りません。スロットを破棄する前に、NULL を設定して接続を解除してください。\\n
     *  条件の差し替えは、接続したまま @c @LIBRARY@_filter_slot_apply などで行えます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\\n
     *  @c @MODULE@_set_tracer と同じく、プロセス全体で呼び出しを直列化し、出力を開始する前に設定してください。
     */
    @EXPORT@int @API@@MODULE@_set_filter(@LIBRARY@_filter_slot *slot);

    /**
     *  @brief          本カタログの出力に設定されている条件式フィルターのスロットを取得します。
     *  @return         設定されているスロットです。未設定の場合は NULL を返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\\n
     *  他スレッドが接続を同時に変更しない場合は、同時に実行できます。
     */
    @EXPORT@@LIBRARY@_filter_slot *@API@@MODULE@_get_filter(void);

    /**
     *  @brief          本カタログ定義を使用して、組み立てた文字列をトレースへ出力します。
     *  @param[in]      string_key 出力する文字列のキー。
     *  @param[in]      ...        引数スキーマが定める順序と型の引数リスト。
     *  @return         トレーサーが未設定の場合は @c CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  @return         組み立てに失敗した場合は @c @LIBRARY@_format と同じ値を返します。
     *  @return         条件式フィルターを接続している場合、判定と組み立てに失敗すると
     *                  @c @LIBRARY@_filter_slot_vformat と同じ値を返し、出力しません。
     *  @return         組み立てに成功した場合は @c cplat_tracer_write_at と同じ値を返します。
     *
     *  文字列キーごとの型付きラッパーが呼び出す関数です。\\n
     *  引数の個数と型の検査を働かせるため、呼び出し側は型付きラッパーのマクロを使用してください。
     *
     *  出力先は @c @MODULE@_set_tracer で設定したトレーサーです。\\n
     *  未設定の場合は、文字列の組み立ても出力も行わずに失敗を返します。
     *  設定の漏れが成功として隠れないようにするためです。
     *
     *  トレース レベルは、カタログ定義の level から変換した分類値を使用します。\\n
     *  @c @MODULE@_set_filter で接続した条件式に一致した場合は、@c CPLAT_TRACE_LEVEL_TO_FORCE で強制出力のレベルへ変更します。\\n
     *  呼び出し位置は引数として受け取るため、トレース側で重ねて付与しません。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\\n
     *  出力先の設定、条件式フィルターの接続、および言語設定を同時に変更しない場合は、同時に実行できます。\\n
     *  他スレッドがそれらを同時に変更する場合は、呼び出し側で同期してください。\\n
     *  @c @LIBRARY@_set_language で言語を設定しておらず、言語がまだ決まっていない場合は、
     *  環境変数から言語を決定します。\\n
     *  このとき、他スレッドが環境変数を同時に変更する場合は、呼び出し側で同期してください。\\n
     *  接続したスロットへの @c @LIBRARY@_filter_slot_attach_source とは、同時に呼び出さないでください。\\n
     *  出力先のトレーサーを @c CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED で生成した場合は、
     *  同じトレーサーへの呼び出しを呼び出し側で直列化してください。
     */
    @EXPORT@int @API@@MODULE@_write(int string_key, ...);
"""


TRACE_SOURCE_TAIL = """\
/** 本カタログの出力先です。@ref @MODULE@_set_tracer で設定します。 */
static cplat_tracer *s_tracer = NULL;

/** 本カタログの出力に使用する条件式フィルターです。@ref @MODULE@_set_filter で設定します。 */
static @LIBRARY@_filter_slot *s_filter = NULL;

/* Doxygen コメントは、ヘッダーに記載 */

void @MODULE@_set_tracer(cplat_tracer *tracer)
{
    s_tracer = tracer;
}

/* Doxygen コメントは、ヘッダーに記載 */

cplat_tracer *@MODULE@_get_tracer(void)
{
    return s_tracer;
}

/* Doxygen コメントは、ヘッダーに記載 */

const @LIBRARY@_filter_key_name *@MODULE@_key_names(void)
{
    return s_key_names;
}

/* Doxygen コメントは、ヘッダーに記載 */

size_t @MODULE@_key_name_count(void)
{
    return sizeof(s_key_names) / sizeof(s_key_names[0]);
}

/* Doxygen コメントは、ヘッダーに記載 */

int @MODULE@_create_filter(const @LIBRARY@_filter_category_names *category_names, const size_t line_capacity,
                           const size_t line_width, @LIBRARY@_filter_slot **slot_out)
{
    return @LIBRARY@_filter_slot_create(&s_catalog, s_key_names, @MODULE@_key_name_count(), category_names,
                                        line_capacity, line_width, slot_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int @MODULE@_set_filter(@LIBRARY@_filter_slot *slot)
{
    /* 別のカタログで作成したスロットは、文字列キーと引数定義が一致しないため接続しません。 */
    if ((slot != NULL) && (@LIBRARY@_filter_slot_get_catalog(slot) != &s_catalog))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    s_filter = slot;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

@LIBRARY@_filter_slot *@MODULE@_get_filter(void)
{
    return s_filter;
}

/* Doxygen コメントは、ヘッダーに記載 */

int @MODULE@_write(const int string_key, ...)
{
    char text[CPLAT_STRING_CATALOG_TEXT_MAX];
    cplat_trace_level level;
    va_list args;
    int is_matched = 0;
    int ret;

    /* 出力先が未設定の場合は、組み立てを行わずに失敗を返します。設定の漏れを成功として隠蔽しないためです。 */
    if (s_tracer == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    va_start(args, string_key);
    if (s_filter != NULL)
    {
        ret = @LIBRARY@_filter_slot_vformat(s_filter, text, sizeof(text), &is_matched, string_key, args);
    }
    else
    {
        ret = @LIBRARY@_vformat(&s_catalog, text, sizeof(text), string_key, args);
    }
    va_end(args);

    if (ret != CPLAT_OK)
    {
        return ret;
    }

    /* 条件式に一致したトレースは、出力先のしきい値によらず出力します。 */
    level = (cplat_trace_level)@MODULE@_category(string_key);
    if (is_matched != 0)
    {
        level = CPLAT_TRACE_LEVEL_TO_FORCE(level);
    }

    /* 呼び出し位置は引数として渡しているため、呼び出し位置を付与しない API を使用します。 */
    return cplat_tracer_write_at(s_tracer, level, NULL, text);
}
"""


SOURCE_TAIL = """\
/* Doxygen コメントは、ヘッダーに記載 */

const @LIBRARY@_entry *@MODULE@_entries(void)
{
    return s_entries;
}

/* Doxygen コメントは、ヘッダーに記載 */

int @MODULE@_entry_count(void)
{
    return @MODULE_UPPER@_ENTRY_COUNT;
}

/* Doxygen コメントは、ヘッダーに記載 */

const int *@MODULE@_key_index(void)
{
    return s_key_index;
}

/* Doxygen コメントは、ヘッダーに記載 */

int @MODULE@_key_index_count(void)
{
    return @MODULE_UPPER@_KEY_INDEX_COUNT;
}

/**
 *  @brief          本カタログ定義のカタログ識別オブジェクトです。
 *
 *  配列とインデックス テーブルを 1 つのカタログ構造体にまとめます。\\n
 *  すべてのメンバーを初期化子で設定可能なため `const` とし、初期化関数は提供しません。
 */
static const @LIBRARY@ s_catalog = {
    s_entries, s_key_index, @MODULE_UPPER@_ENTRY_COUNT, @MODULE_UPPER@_KEY_INDEX_COUNT};

/* Doxygen コメントは、ヘッダーに記載 */

const @LIBRARY@ *@MODULE@_catalog(void)
{
    return &s_catalog;
}

/* Doxygen コメントは、ヘッダーに記載 */

const @LIBRARY@_entry *@MODULE@_entry(const int string_key)
{
    return @LIBRARY@_get_entry(&s_catalog, string_key);
}

/* Doxygen コメントは、ヘッダーに記載 */

int @MODULE@_vformat(char *dest, const size_t dest_size, const int string_key, va_list args)
{
    return @LIBRARY@_vformat(&s_catalog, dest, dest_size, string_key, args);
}

/* Doxygen コメントは、ヘッダーに記載 */

int @MODULE@_format(char *dest, const size_t dest_size, const int string_key, ...)
{
    va_list args;
    int ret;

    va_start(args, string_key);
    ret = @LIBRARY@_vformat(&s_catalog, dest, dest_size, string_key, args);
    va_end(args);

    return ret;
}

/* Doxygen コメントは、ヘッダーに記載 */

int @MODULE@_verify(int *string_key_out, @LIBRARY@_language *language_out)
{
    return @LIBRARY@_verify(&s_catalog, string_key_out, language_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int @MODULE@_category(const int string_key)
{
    return @LIBRARY@_get_category(&s_catalog, string_key);
}

/* Doxygen コメントは、ヘッダーに記載 */

const char *@MODULE@_id(const int string_key)
{
    return @LIBRARY@_get_id(&s_catalog, string_key);
}

/* Doxygen コメントは、ヘッダーに記載 */

const char *@MODULE@_note(const int string_key)
{
    return @LIBRARY@_get_note(&s_catalog, string_key);
}
"""


def emit_source(document: dict, strings: list[dict], definition_name: str, out_relative: str = ".") -> str:
    """実装側の生成物を組み立てます。"""
    module = document["module_prefix"]
    module_upper = module.upper()
    library = LIBRARY_PREFIX
    header_name = f"{module}.h"
    source_name = f"{module}.c"
    # @file はリポジトリの慣習に合わせ、prod/ を除いた相対パスで示す
    output_dir = output_dir_display(document, out_relative)
    source_display = output_dir[len("prod/") :] if output_dir.startswith("prod/") else output_dir
    last_key = strings[-1]["key"]

    out = [
        "/**",
        " " + "*" * 79,
        f" *  @file           {source_display}/{source_name}",
        " *  @brief          文字列キーごとの引数スキーマ、分類値、メタデータ、および言語別リソースを保持します。",
        f" *  @author         {document.get('author', '')}",
        f" *  @date           {document.get('date', '')}",
        f" *  @version        {document.get('version', '')}",
        " *",
        f" *  本ファイルは、カタログ定義 `{definition_name}` から自動生成されたファイルです。\\n",
        f" *  同じ生成元から作成される `{header_name}` と合わせて 1 組の生成単位です。\\n",
        " *  手作業で直接編集せず、生成元の定義を変更してから `app/cplat/bin_internal/string_catalog_gen.py` を実行してください。",
        " *",
        " *  このテーブルは利用側で用意する定義情報であり、ライブラリ側では保持しません。\\n",
        " *  配列とインデックス テーブルを @ref s_catalog へまとめ、組み立て API の呼び出しごとに渡します。\\n",
        " *  カタログの指定を省略して呼び出すための簡易関数も、本生成物で提供します。",
        " *",
        " *  分類値はライブラリ側では解釈しない補足情報です。\\n",
        " *  意味や有効範囲は利用側で定義します。本生成物では直接の値のまま保持し、特定の列挙型には依存しません。",
        " *",
        " *  カタログ配列に加えて、文字列キーをインデックスとするインデックス テーブルを保持します。\\n",
        " *  ライブラリはこのテーブルを参照して文字列キーからカタログ エントリを直接引き、線形探索を回避します。",
        " *",
        " *  各要素は、定義間で一意な文字列キー、分類値、引数の個数、明示的なアラインメント、引数定義、",
        " *  補足の ID、説明文、補足説明、言語別の書式、言語別の備考の順に配置します。\\n",
        f" *  `texts` と `notes` は、@c {library}_language をキーとした指示付き初期化子で記述します。\\n",
        " *  記述を省略した言語の要素は暗黙的にヌル ポインターとなり、ニュートラル言語の要素へ代替（フォールバック）されます。",
        " *",
        " *  引数の型と文字列表現はこのテーブルで定義し、言語別リソースでは語順のみを管理します。\\n",
        " *  書式中の `{0}` から `{49}` は引数の位置を表します。\\n",
        " *  `{` や `}` そのものを出力する場合は `{{` および `}}` と記述します。",
        " *",
        *(
            [
                " *  本カタログは、全文字列の書式へ共通の前置および後置を結合しています。\\n",
                " *  本ファイルが示す書式は、結合後の文字列です。",
                " *",
            ]
            if has_text_affix(document)
            else []
        ),
        " *  ニュートラル言語の書式は、英語と同一の表現とします。\\n",
        " *  そのため英語の要素は個別に記載せず、ニュートラル言語の書式へフォールバックされます。\\n",
        " *  英語とニュートラル言語で表現を分ける必要が生じた時点で、英語の要素を追加してください。",
        " *",
        " *  ソース ファイルの文字コードおよび出力する文字列は UTF-8 です。",
        " *",
        f" *  @copyright      Copyright (C) {document.get('author', '')}. 2026. All rights reserved.",
        " *",
        " " + "*" * 79,
        " */",
        "",
        (
            f"#include <{document[PUBLIC_INCLUDE_KEY]}>"
            if document.get(PUBLIC_INCLUDE_KEY) is not None
            else f'#include "{header_name}"'
        ),
        "",
        "#include <assert.h>",
        "#include <stdarg.h>",
        "#include <stddef.h>",
        "",
    ]

    trace = is_trace(document)

    for position, entry in enumerate(strings):
        arguments = entry["arguments"]
        if not arguments and not trace:
            continue

        brief = f"/** {entry['key']} の引数定義です。 */"
        declared_length = ""
        if trace:
            brief = (
                f"/** {entry['key']} の引数定義です。"
                f"{CONTEXT_ARGUMENT_BASE} 番から先は生成器が付け加えるコンテキスト引数です。 */"
            )
            # 予約した番号空間の全体を確保する。app が定義するコンテキスト引数を増減しても
            # 要素数が変わらないようにするため、要素数を明示して宣言する。
            declared_length = f"{library.upper()}_ARGUMENT_MAX"

        out.extend(
            [
                "",
                brief,
                f"static const {library}_argument s_arguments_{position}[{declared_length}] = {{",
            ]
        )
        for argument in arguments:
            out.append(
                f"    {{{kind_constant(document, argument['kind'])}, 0, {c_string(argument['name'])}, "
                f"{c_string(argument['description'])}}},"
            )
        if trace:
            if len(arguments) < CONTEXT_ARGUMENT_BASE:
                out.append(
                    f"    /* {len(arguments)} 番から {CONTEXT_ARGUMENT_BASE - 1} 番は、"
                    "要素を明示しないことで値を受け取らないインデックスになります。 */"
                )
            for index, argument in zip(context_argument_indices(document), context_arguments(document)):
                out.append(
                    f"    [{index}] = "
                    f"{{{kind_constant(document, argument['kind'])}, 0, {c_string(argument['name'])}, "
                    f"{c_string(argument['description'])}}},"
                )
            reserved = sorted(set(range(EXTENSION_ARGUMENT_BASE, ARGUMENT_MAX)) - set(context_argument_indices(document)))
            if reserved:
                out.append(
                    f"    /* {reserved[0]} 番から {reserved[-1]} 番は、app が定義するコンテキスト引数のために"
                    "予約した空きです。値を受け取らないインデックスとして残ります。 */"
                )
        out.append("};")

    out.extend(
        [
            "",
            "/** 文字列キーごとのカタログ テーブルです。文字列キーの昇順に定義します。 */",
            f"static const {library}_entry s_entries[] = {{",
        ]
    )

    rows = []
    for position, entry in enumerate(strings):
        arguments = entry["arguments"]
        arguments_line = f"s_arguments_{position}" if (arguments or trace) else "NULL"
        remarks_line = c_string(join_text(entry["remarks"])) if "remarks" in entry else "NULL"
        category = trace_level_value(entry["level"]) if trace else entry["category"]

        row = [
            f"    {{{entry['key']},",
            f"     {category},",
            f"     {argument_array_length(document, entry)},",
            "     0, /* 明示的アラインメント */",
            f"     {arguments_line},",
            f"     {c_string(entry['id']) if 'id' in entry else 'NULL'},",
            f"     {c_string(entry['brief'])},",
            f"     {c_string(join_text(entry['details'])) if entry.get('details') is not None else 'NULL'},",
            f"     {remarks_line},",
        ]

        for section in ("texts", "notes"):
            items = []
            for language in LANGUAGES:
                if language in entry[section]:
                    constant = language_constant(document, language)
                    # 書式だけがカタログ共通の前置と後置を持つ。備考は定義のままとする。
                    value = (
                        entry_text(document, entry, language)
                        if section == "texts"
                        else join_text(entry[section][language])
                    )
                    items.append(f"[{constant}] = {c_string(value)}")
            separator = ",\n      "
            terminator = "}," if section == "texts" else "}}"
            row.append(f"     {{{separator.join(items)}{terminator}")

        rows.append("\n".join(row))

    out.append(",\n".join(rows) + "};")
    out.extend(
        [
            "",
            "/** `s_entries` の要素数です。 */",
            f"#define {module_upper}_ENTRY_COUNT ((int)(sizeof(s_entries) / sizeof(s_entries[0])))",
            "",
            "/** インデックス テーブルにおいて、文字列キーが未登録であることを表す値です。 */",
            f"#define {module_upper}_KEY_INDEX_ABSENT (-1)",
            "",
            "/**",
            " *  @brief          文字列キーをインデックスとして、`s_entries` のインデックスを引くためのテーブルです。",
            " *",
            " *  文字列キーは 1 から始まるため、インデックス 0 は使用しません。\\n",
            f" *  文字列キーが連続せず欠番となる場合は、該当するインデックスへ @ref {module_upper}_KEY_INDEX_ABSENT を格納します。",
            " */",
            "static const int s_key_index[] = {",
            f"    {module_upper}_KEY_INDEX_ABSENT, /* 0: 未使用 */",
        ]
    )

    for position, entry in enumerate(strings):
        comma = "," if position < (len(strings) - 1) else ""
        out.append(f"    {position}{comma} /* {entry['key']} */")

    out.extend(
        [
            "};",
            "",
            "/** `s_key_index` の要素数です。 */",
            f"#define {module_upper}_KEY_INDEX_COUNT ((int)(sizeof(s_key_index) / sizeof(s_key_index[0])))",
            "",
            "/*",
            " *  インデックス テーブルが最大の文字列キーまでを網羅していることを、ビルド時に検証します。",
            " *  網羅されていない文字列キーは線形探索にフォールバックするため動作自体は可能ですが、インデックス テーブルの拡張漏れとなります。",
            " *  対象は、値が最大の文字列キーの定数です。",
            " */",
            f'static_assert({module_upper}_KEY_INDEX_COUNT > {last_key}, "key_index must cover every string key");',
            "",
            expand(SOURCE_TAIL, module, library).rstrip("\n"),
            "",
        ]
    )

    if trace:
        out.extend(
            [
                "",
                "/** 文字列キーの名前解決表です。列挙定数名と文字列キーを、定義の並び順で保持します。 */",
                f"static const {library}_filter_key_name s_key_names[] = {{",
            ]
        )
        out.extend(f"    {{{c_string(entry['key'])}, {entry['key']}, 0U}}," for entry in strings)
        out.append("};")
        out.extend(["", expand(TRACE_SOURCE_TAIL, module, library).rstrip("\n"), ""])

    return "\n".join(out)


def find_clang_format_style(start: Path) -> Path | None:
    """出力先から親ディレクトリを探索して .clang-format を検索します。"""
    for directory in [start.resolve()] + list(start.resolve().parents):
        candidate = directory / ".clang-format"
        if candidate.is_file():
            return candidate
    return None


def format_source(text: str, filename: str, style: Path | None) -> str:
    """生成した内容を clang-format で整形します。

    生成物はリポジトリの整形規則に従う必要があり、整形までを生成器の責務とします。
    整形しない場合、--check が常に差分を報告することになります。
    """
    if style is None or shutil.which("clang-format") is None:
        print("警告: clang-format が見つからないため、整形せずに出力します。", file=sys.stderr)
        return text

    completed = subprocess.run(
        ["clang-format", f"--style=file:{style}", f"--assume-filename={filename}"],
        input=text,
        capture_output=True,
        text=True,
        encoding="utf-8",
        check=True,
    )
    return completed.stdout


def write_text_lf(path: Path, content: str) -> None:
    """UTF-8 のテキストを LF 改行で書き込みます。"""
    with path.open("w", encoding="utf-8", newline="\n") as output:
        output.write(content)


def main(argv: list[str] | None = None) -> int:
    """コマンドのエントリ ポイントです。"""
    parser = argparse.ArgumentParser(description="カタログ定義から cplat 文字列カタログの生成物を出力します。")
    parser.add_argument("definition", type=Path, help="カタログ定義 (JSONC) のパス")
    parser.add_argument("--out-dir", type=Path, default=None, help="出力先ディレクトリ。既定は定義ファイルと同一の場所です。")
    parser.add_argument(
        "--header-dir",
        type=Path,
        default=None,
        help="ヘッダーの出力先ディレクトリ。既定は --out-dir と同一の場所です。公開ヘッダーを分けて配置する場合に指定します。",
    )
    parser.add_argument("--check", action="store_true", help="ファイルを出力せず、既存の生成物と一致するかのみを確認します。")
    parser.add_argument(
        "--if-newer",
        action="store_true",
        help="生成物が定義ファイルおよび生成器より新しい場合は何もしません。make のパース時に呼び出す用途です。",
    )
    args = parser.parse_args(argv)

    try:
        document = load_definition(args.definition)
        # 名前と配置場所は定義ファイル自身から決定します。定義の内部には記述しません。
        document["module_prefix"] = derive_module_prefix(args.definition)
        document["module_dir"] = derive_module_dir(args.definition)
        load_settings(args.definition, document)
        strings = validate(document)
    except DefinitionError as error:
        print(f"エラー: {error}", file=sys.stderr)
        return 1

    out_dir = args.out_dir if args.out_dir is not None else args.definition.parent
    out_dir.mkdir(parents=True, exist_ok=True)
    header_dir = args.header_dir if args.header_dir is not None else out_dir
    header_dir.mkdir(parents=True, exist_ok=True)
    # ヘッダーを別の場所へ出力する場合、利用側は公開ヘッダーの配置場所からの相対パスで取り込みます。
    if header_dir.resolve() != out_dir.resolve():
        document[PUBLIC_INCLUDE_KEY] = header_include_path(header_dir, document["module_prefix"])
    definition_name = args.definition.name
    module = document["module_prefix"]

    if args.if_newer and not args.check:
        targets = [header_dir / f"{module}.h", out_dir / f"{module}.c"]
        # 設定ファイルを変更した場合も再生成します。app 内の複数のカタログが同一の設定を共有するためです。
        settings = settings_path(args.definition, document)
        sources = [args.definition, Path(__file__)] + ([settings] if settings is not None else [])
        if all(target.exists() for target in targets):
            newest_source = max(source.stat().st_mtime for source in sources)
            oldest_target = min(target.stat().st_mtime for target in targets)
            if oldest_target >= newest_source:
                return 0

    # 出力先が定義ファイルと異なるディレクトリの場合は、Doxygen の @file もその位置を指します。
    out_relative = os.path.relpath(out_dir, args.definition.parent).replace("\\", "/")
    header_relative = os.path.relpath(header_dir, args.definition.parent).replace("\\", "/")

    style = find_clang_format_style(out_dir)
    outputs = {
        header_dir / f"{module}.h": format_source(
            emit_header(document, strings, definition_name, header_relative), f"{module}.h", style
        ),
        out_dir / f"{module}.c": format_source(
            emit_source(document, strings, definition_name, out_relative), f"{module}.c", style
        ),
    }

    differs = False
    for path, content in outputs.items():
        if args.check:
            current = path.read_text(encoding="utf-8") if path.exists() else ""
            if current != content:
                print(f"差分あり: {path}", file=sys.stderr)
                differs = True
        else:
            write_text_lf(path, content)
            print(f"生成: {path}")

    return 1 if differs else 0


if __name__ == "__main__":
    sys.exit(main())
