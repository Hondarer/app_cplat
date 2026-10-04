/**
 *******************************************************************************
 *  @file           filter.h
 *  @brief          文字列カタログの条件式フィルター API を宣言します。
 *  @author         Tetsuo Honda
 *  @date           2026/09/26
 *  @version        0.1.0
 *
 *  設計の正本は `app/cplat/docs/proposals/string-catalog-filter-design.md` です。
 *
 *  API は 2 つの層に分かれます。
 *
 *  - コンパイルの層は、条件式リストとフィルター オブジェクトだけを扱い、カタログ定義を参照しません。
 *  - フィルター スロットの層は、フィルター オブジェクトをカタログ定義へ関連付け、判定と書式展開を行います。
 *
 *  フィルター オブジェクトは、ポインターを含まない固定長の領域です。\n
 *  大きさは @ref CPLAT_STRING_CATALOG_FILTER_IMAGE_SIZE で行数の上限と行幅から求めます。\n
 *  領域のアラインメントは問いません。
 *
 *  フィルター オブジェクトは形式版とバイト順序の目印を持ちます。\n
 *  検証を行うすべての関数は、本ライブラリと形式版またはバイト順序が異なるフィルター オブジェクトを拒否し、変換しません。
 *
 *  フィルター オブジェクトを別のスレッドやプロセスから受け取る場合は、ソース領域を使用できます。\n
 *  ソース領域のメモリと、その排他の実装は利用側が受け持ちます。
 *  本ライブラリは、先頭アドレスとバイト数、および排他を取得、解放する関数 (@ref cplat_string_catalog_filter_source_lock)
 *  を受け取るだけで、領域の確保や共有、排他の方式を知りません。\n
 *  ソース領域は、ヘッダーとフィルター オブジェクトを並べた領域です。共有メモリやプロセス内の静的領域など、
 *  先頭アドレスとバイト数で示せる領域であれば種類を問いません。\n
 *  書き込み側は @ref cplat_string_catalog_filter_source_publish で公開し、
 *  読み取り側は @ref cplat_string_catalog_filter_slot_attach_source でスロットへ結び付けます。\n
 *  スロットは判定付きの組み立てのたびにヘッダーの版番号を 1 回だけ読み、変化した場合に取り込みます。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *******************************************************************************
 */

#ifndef CPLAT_STRING_CATALOG_FILTER_H
#define CPLAT_STRING_CATALOG_FILTER_H

#include <cplat/clock/timespec.h>
#include <cplat/cplat_export.h>
#include <cplat/string_catalog/string_catalog.h>

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

/**
 *  @ingroup        CPLAT_STRING_CATALOG
 *  @{
 */

/**
 *  @brief          フィルター オブジェクトが格納できる行数の上限です。
 *
 *  行幅の上限とそろえています。\n
 *  スロットは、作成時に指定した行数の上限に合わせて固定長の領域を確保します。
 *  事前計算の領域は行数と項目数の積に比例するため、行数の上限は必要な分だけ指定してください。
 */
#define CPLAT_STRING_CATALOG_FILTER_LINE_MAX 1024U

/** 行幅の下限です。最も短い判定要素 `key<1` と NUL 終端を格納できる幅です。 */
#define CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MIN 8U

/** 行幅の上限です。定数のオフセットを 16 ビットで表すためです。 */
#define CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MAX 1024U

/** 1 行に記述できる判定要素の上限です。 */
#define CPLAT_STRING_CATALOG_FILTER_PREDICATE_MAX 32U

/** 1 行に記述できる括弧と `!` のネストの上限です。構文解析の再帰の深さを抑えます。 */
#define CPLAT_STRING_CATALOG_FILTER_NESTING_MAX 16U

/** 1 行が名前で参照できる引数の種類の上限です。 */
#define CPLAT_STRING_CATALOG_FILTER_ARGUMENT_REFERENCE_MAX 8U

/** 1 行に記述できる、文字列キーの名前 (識別子の定数) の上限です。 */
#define CPLAT_STRING_CATALOG_FILTER_IDENTIFIER_REFERENCE_MAX 16U

/** 1 行に記述できる正規表現のパターン (`matches` と `matches_i`) の上限です。 */
#define CPLAT_STRING_CATALOG_FILTER_PATTERN_REFERENCE_MAX 4U

/**
 *  @brief          正規表現で照合する文字列のバイト数の上限です。
 *
 *  照合はバックトラッキングで行うため、長い文字列と複雑なパターンの組み合わせで時間やスタックを消費します。\n
 *  これを超える文字列は照合せず、`matches` と `matches_i` を偽とします。
 *  トレースの組み立て結果の上限 (`CPLAT_STRING_CATALOG_TEXT_MAX`) と同じ値です。
 */
#define CPLAT_STRING_CATALOG_FILTER_PATTERN_SUBJECT_MAX 512U

/** フィルター オブジェクトのヘッダーのバイト数です。 */
#define CPLAT_STRING_CATALOG_FILTER_HEADER_SIZE 64U

/** 行レコードの見出しのバイト数です。 */
#define CPLAT_STRING_CATALOG_FILTER_RECORD_HEADER_SIZE 16U

/**
 *  @brief          行幅から行レコードのバイト数を求めます。
 *  @param[in]      width 行幅。
 *
 *  命令領域と定数領域は、いずれも行幅 1 文字あたり 8 バイトです。
 */
#define CPLAT_STRING_CATALOG_FILTER_RECORD_SIZE(width) \
    (CPLAT_STRING_CATALOG_FILTER_RECORD_HEADER_SIZE + (16U * (size_t)(width)))

/**
 *  @brief          行数の上限と行幅から、フィルター オブジェクトのバイト数を求めます。
 *  @param[in]      lines 行数の上限。
 *  @param[in]      width 行幅。
 */
#define CPLAT_STRING_CATALOG_FILTER_IMAGE_SIZE(lines, width) \
    (CPLAT_STRING_CATALOG_FILTER_HEADER_SIZE + ((size_t)(lines) * CPLAT_STRING_CATALOG_FILTER_RECORD_SIZE(width)))

/** ソース領域のヘッダーのバイト数です。フィルター オブジェクトはこの位置から始まります。 */
#define CPLAT_STRING_CATALOG_FILTER_SOURCE_HEADER_SIZE 64U

/** ソース領域の先頭アドレスに求めるアラインメントです。版番号をアトミックに読み書きするためです。 */
#define CPLAT_STRING_CATALOG_FILTER_SOURCE_ALIGNMENT 8U

/**
 *  @brief          行数の上限と行幅から、ソース領域のバイト数を求めます。
 *  @param[in]      lines 行数の上限。
 *  @param[in]      width 行幅。
 */
#define CPLAT_STRING_CATALOG_FILTER_SOURCE_SIZE(lines, width) \
    (CPLAT_STRING_CATALOG_FILTER_SOURCE_HEADER_SIZE + CPLAT_STRING_CATALOG_FILTER_IMAGE_SIZE(lines, width))

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          無効にした行の原因です。
     *
     *  関数の結果コード (`CPLAT_OK` や `CPLAT_ERR_*`) とは別の値です。\n
     *  関数が成功した場合も、行ごとの原因を @ref cplat_string_catalog_filter_diagnostic で通知します。
     */
    typedef enum cplat_string_catalog_filter_line_error
    {
        CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE = 0,    /**< 原因なし。 */
        CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_LEXICAL = 1, /**< 字句の誤り。閉じていない引用符や範囲外の数値など。 */
        CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_SYNTAX = 2,  /**< 構文の誤り。 */
        CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_TYPE_MISMATCH =
            3, /**< フィールド、演算子、定数の型の組み合わせの誤り。 */
        CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_LIMIT_EXCEEDED =
            4,                                                    /**< 判定要素数、ネスト、参照数、行幅の上限の超過。 */
        CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_LINE_CAPACITY = 5, /**< フィルター オブジェクトの行数の上限の超過。 */
        CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_UNRESOLVED_KEY_NAME = 6, /**< 名前解決テーブルにない文字列キーの名前。 */
        CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_UNRESOLVED_ARGUMENT_NAME = 7, /**< カタログのどの項目にもない引数名。 */
        CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_UNRESOLVED_CATEGORY_NAME =
            8, /**< 分類値の名前にない識別子を、分類値と比較した。 */
        CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_CATEGORY_OUT_OF_RANGE =
            9, /**< 分類値と比較する定数が、分類値の名前の範囲外。 */
        CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_INVALID_PATTERN =
            10, /**< 正規表現のパターンをコンパイルできない。構文の誤りや、照合器の制限の超過など。 */
        CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NEVER_SATISFIABLE =
            11 /**< カタログのどの項目に対しても成立し得ない条件式。型の合わない比較や、互いに矛盾する条件など。 */
    } cplat_string_catalog_filter_line_error;

    /**
     *  @brief          無効にした行の診断情報です。
     *
     *  コンパイルでは、@ref cplat_string_catalog_filter_diagnostic::line_index は入力した条件式リストの行 (0 起点) です。\n
     *  適用では、フィルター オブジェクトが格納している行 (0 起点) です。
     */
    typedef struct cplat_string_catalog_filter_diagnostic
    {
        uint32_t line_index;                     /**< 行の位置 (0 起点)。 */
        uint32_t column;                         /**< 行内のバイト位置 (0 起点)。適用で検出した場合は 0。 */
        cplat_string_catalog_filter_line_error error; /**< 原因。 */
    } cplat_string_catalog_filter_diagnostic;

    /**
     *  @brief          フィルター オブジェクトのヘッダーから読み取った情報です。
     */
    typedef struct cplat_string_catalog_filter_info
    {
        uint64_t image_size;     /**< 全体のバイト数。 */
        uint64_t content_hash;   /**< 有効な行の内容から算出したハッシュ値。 */
        uint32_t line_capacity;  /**< 行数の上限。 */
        uint32_t line_width;     /**< 行幅。 */
        uint32_t line_count;     /**< 格納している条件式の数。 */
        uint32_t format_version; /**< 形式版。 */
    } cplat_string_catalog_filter_info;

    /**
     *  @brief          ソース領域のヘッダーから読み取った公開の情報です。
     */
    typedef struct cplat_string_catalog_filter_source_info
    {
        uint64_t published_revision;       /**< 版番号。公開のたびに 2 ずつ増える偶数で、0 は未公開です。 */
        cplat_timespec published_realtime; /**< 公開した実時刻。未公開の場合は 0 です。 */
        uint32_t publisher_process_id;     /**< 公開したプロセスの ID。未公開の場合は 0 です。 */
        uint32_t line_capacity;            /**< 公開したフィルター オブジェクトの行数の上限。未公開の場合は 0 です。 */
        uint32_t line_width;               /**< 公開したフィルター オブジェクトの行幅。未公開の場合は 0 です。 */
        uint32_t pad;                      /**< 明示的アラインメントです。 */
        uint64_t catalog_id; /**< 公開時に指定したカタログの識別値。未公開の場合は 0 です。 */
    } cplat_string_catalog_filter_source_info;

    /**
     *  @brief          ソース領域の排他を取得する関数です。
     *  @param[in]      context @ref cplat_string_catalog_filter_source_lock::context に指定した値。
     *  @return         取得できた場合は `CPLAT_OK`、取得できない場合はその結果コードを返します。
     */
    typedef int (*cplat_string_catalog_filter_source_lock_fn)(void *context);

    /**
     *  @brief          ソース領域の排他を解放する関数です。
     *  @param[in]      context @ref cplat_string_catalog_filter_source_lock::context に指定した値。
     */
    typedef void (*cplat_string_catalog_filter_source_unlock_fn)(void *context);

    /**
     *  @brief          ソース領域の排他を取得、解放する関数の組です。
     *
     *  排他の実装は利用側が受け持ちます。本ライブラリは、公開と取り込みの間だけ関数を呼び出します。\n
     *  同じソース領域を使う書き込み側と読み取り側には、同じ排他を指定します。
     *
     *  - 公開 (@ref cplat_string_catalog_filter_source_publish) は、書き込みの間だけ排他を取ります。
     *  - スロット (@ref cplat_string_catalog_filter_slot_attach_source) は、版番号の変化を検知した場合だけ排他を取り、
     *    版番号を読み直してから複製し、複製を終えた時点で解放します。
     *
     *  排他の実装は、次の条件を満たす必要があります。
     *  - 同じプロセスのスレッドの間と、ソース領域を共有するプロセスの間の両方で排他になること。
     *  - 排他を保持したプロセスが異常終了した場合も、ほかのプロセスが取得できるようになること。
     *  - 本ライブラリは排他を再入して取得しません。再入に対応する必要はありません。
     */
    typedef struct cplat_string_catalog_filter_source_lock
    {
        cplat_string_catalog_filter_source_lock_fn lock;     /**< 排他を取得する関数。NULL にできません。 */
        cplat_string_catalog_filter_source_unlock_fn unlock; /**< 排他を解放する関数。NULL にできません。 */
        void *context;                                       /**< 2 つの関数へ渡す値。 */
    } cplat_string_catalog_filter_source_lock;

    /**
     *  @brief          スロットがソース領域から取り込んだ状態です。
     */
    typedef struct cplat_string_catalog_filter_source_status
    {
        uint64_t taken_revision;   /**< 取り込みを試みた版番号。未取り込みの場合は 0 です。 */
        size_t last_invalid_count; /**< 直近の取り込みで無効にした行の数。 */
        int last_result;           /**< 直近の取り込みで適用した結果コード。未取り込みの場合は `CPLAT_OK` です。 */
        unsigned int pad;          /**< 明示的アラインメントです。 */
    } cplat_string_catalog_filter_source_status;

    /**
     *  @brief          文字列キーの名前と値の対応です。
     *
     *  条件式に記述した列挙定数名を、適用の時点で整数へ解決するために使用します。
     */
    typedef struct cplat_string_catalog_filter_key_name
    {
        const char *name; /**< 列挙定数名。NULL にできません。 */
        int key;          /**< 文字列キー。 */
        unsigned int pad; /**< 明示的アラインメントです。0 を指定します。 */
    } cplat_string_catalog_filter_key_name;

    /**
     *  @brief          項目ごとの事前計算の状態です。
     *
     *  値 0 を「常に不一致」とし、条件式リストが空の状態と一致させています。
     */
    typedef enum cplat_string_catalog_filter_state
    {
        CPLAT_STRING_CATALOG_FILTER_STATE_NEVER_MATCH = 0,       /**< 引数の値によらず、どの行にも一致しない。 */
        CPLAT_STRING_CATALOG_FILTER_STATE_ALWAYS_MATCH = 1,      /**< 引数の値によらず、いずれかの行に一致する。 */
        CPLAT_STRING_CATALOG_FILTER_STATE_ARGUMENT_DEPENDENT = 2 /**< 引数の値を評価するまで一致が確定しない。 */
    } cplat_string_catalog_filter_state;

    /**
     *  @brief          分類値の名前です。自然文での表現で、分類値を名前で表すために使用します。
     *
     *  フィルターは分類値の意味を解釈しません。意味を決める利用側 (例: 分類値をトレース レベルとして扱う app) が、スロットの作成時に指定します。\n
     *  分類値 i の名前は @ref cplat_string_catalog_filter_category_names::names の i 番目です。
     */
    typedef struct cplat_string_catalog_filter_category_names
    {
        const char *const *names; /**< 分類値をインデックスとする名前の配列。NULL にできません。 */
        size_t count;             /**< @ref cplat_string_catalog_filter_category_names::names の要素数。1 以上です。 */
        const char *subject_japanese; /**< 日本語の文型で主語にする名前 (例: "レベル")。NULL にできません。 */
        const char
            *subject_neutral; /**< ニュートラル言語の文型で主語にする名前 (例: "the level")。NULL にできません。 */
    } cplat_string_catalog_filter_category_names;

    /** フィルター スロット (不透明型)。 */
    typedef struct cplat_string_catalog_filter_slot cplat_string_catalog_filter_slot;

    /* ===== コンパイルの層 (カタログ定義を参照しない) ===== */

    /**
     *  @brief          条件式リストをコンパイルし、フィルター オブジェクトを生成します。
     *  @param[in]      lines               条件式リストの先頭。`char lines[N][M]` の `&lines[0][0]`。
     *                                      @p line_count が 0 の場合は NULL を指定できます。
     *  @param[in]      line_count          条件式リストの行数 `N`。
     *  @param[in]      line_width          条件式リストの行幅 `M`。フィルター オブジェクトの行幅になります。
     *  @param[in]      line_capacity       フィルター オブジェクトの行数の上限。
     *  @param[out]     image_out           フィルター オブジェクトの格納先。
     *  @param[in]      image_size          @p image_out のバイト数。
     *  @param[out]     diagnostics         無効にした行の診断情報の格納先。NULL を指定できます。
     *  @param[in]      diagnostic_capacity @p diagnostics の要素数。
     *  @param[out]     invalid_count_out   無効にした行の総数の格納先。NULL を指定できます。
     *  @return         成功時は `CPLAT_OK` を返します。無効にした行があっても成功です。
     *  @return         引数が不正な場合、または行幅と行数の上限が範囲外の場合は
     *                  `CPLAT_ERR_INVALID_ARGUMENT` を返します。
     *  @return         @p image_size が @ref CPLAT_STRING_CATALOG_FILTER_IMAGE_SIZE に満たない場合は
     *                  `CPLAT_ERR_BUFFER_TOO_SMALL` を返します。
     *
     *  空行、空白だけの行、先頭が `#` の行は格納しません。\n
     *  診断情報は @p diagnostic_capacity 個までを格納し、総数は @p invalid_count_out へ格納します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p image_out に対する呼び出しは同時に実行できます。\n
     *  同一 @p image_out に対する並行操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_string_catalog_filter_compile(const char *lines, size_t line_count,
                                                                   size_t line_width, size_t line_capacity,
                                                                   void *image_out, size_t image_size,
                                                                   cplat_string_catalog_filter_diagnostic *diagnostics,
                                                                   size_t diagnostic_capacity,
                                                                   size_t *invalid_count_out);

    /**
     *  @brief          指定した行の条件式を置き換えます。
     *  @param[in,out]  image          フィルター オブジェクト。
     *  @param[in]      image_size     @p image のバイト数。
     *  @param[in]      line_index     置き換える行 (0 起点)。格納している条件式の数より小さい値です。
     *  @param[in]      text           NUL 終端の条件式。
     *  @param[out]     diagnostic_out コンパイルに失敗した場合の診断情報の格納先。NULL を指定できます。
     *  @return         成功時は `CPLAT_OK` を返します。
     *  @return         引数が不正な場合は `CPLAT_ERR_INVALID_ARGUMENT` を返します。
     *  @return         @p image の形式版が異なる場合は `CPLAT_ERR_VERSION_MISMATCH`、
     *                  それ以外の検証の失敗は `CPLAT_ERR_CORRUPT_DESCRIPTOR` を返します。
     *  @return         条件式が不正な場合、および空行やコメントの場合は `CPLAT_ERR_MALFORMED_DEFINITION` を返します。
     *
     *  失敗した場合、@p image は変更しません。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p image に対する呼び出しは同時に実行できます。\n
     *  同一 @p image に対する並行操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API
    cplat_string_catalog_filter_compile_line(void *image, size_t image_size, size_t line_index, const char *text,
                                             cplat_string_catalog_filter_diagnostic *diagnostic_out);

    /**
     *  @brief          指定した位置へ条件式を挿入します。
     *  @param[in,out]  image          フィルター オブジェクト。
     *  @param[in]      image_size     @p image のバイト数。
     *  @param[in]      line_index     挿入する位置 (0 起点)。格納している条件式の数と等しい場合は末尾へ追加します。
     *  @param[in]      text           NUL 終端の条件式。
     *  @param[out]     diagnostic_out コンパイルに失敗した場合の診断情報の格納先。NULL を指定できます。
     *  @return         成功時は `CPLAT_OK` を返します。
     *  @return         引数が不正な場合は `CPLAT_ERR_INVALID_ARGUMENT` を返します。
     *  @return         @p image の形式版が異なる場合は `CPLAT_ERR_VERSION_MISMATCH`、
     *                  それ以外の検証の失敗は `CPLAT_ERR_CORRUPT_DESCRIPTOR` を返します。
     *  @return         行数の上限に達している場合は `CPLAT_ERR_STORAGE_FULL` を返します。
     *  @return         条件式が不正な場合、および空行やコメントの場合は `CPLAT_ERR_MALFORMED_DEFINITION` を返します。
     *
     *  失敗した場合、@p image は変更しません。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p image に対する呼び出しは同時に実行できます。\n
     *  同一 @p image に対する並行操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API
    cplat_string_catalog_filter_insert_line(void *image, size_t image_size, size_t line_index, const char *text,
                                            cplat_string_catalog_filter_diagnostic *diagnostic_out);

    /**
     *  @brief          指定した行の条件式を削除します。
     *  @param[in,out]  image      フィルター オブジェクト。
     *  @param[in]      image_size @p image のバイト数。
     *  @param[in]      line_index 削除する行 (0 起点)。
     *  @return         成功時は `CPLAT_OK` を返します。
     *  @return         引数が不正な場合は `CPLAT_ERR_INVALID_ARGUMENT` を返します。
     *  @return         @p image の形式版が異なる場合は `CPLAT_ERR_VERSION_MISMATCH`、
     *                  それ以外の検証の失敗は `CPLAT_ERR_CORRUPT_DESCRIPTOR` を返します。
     *
     *  後続の行は 1 つ前へ移動します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p image に対する呼び出しは同時に実行できます。\n
     *  同一 @p image に対する並行操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_string_catalog_filter_remove_line(void *image, size_t image_size,
                                                                       size_t line_index);

    /**
     *  @brief          フィルター オブジェクトの形式と内容の整合を確認します。
     *  @param[in]      image      フィルター オブジェクト。
     *  @param[in]      image_size @p image のバイト数。
     *  @return         整合している場合は `CPLAT_OK` を返します。
     *  @return         引数が NULL の場合は `CPLAT_ERR_INVALID_ARGUMENT` を返します。
     *  @return         形式版の不一致を検出した場合は `CPLAT_ERR_VERSION_MISMATCH` を返します。
     *  @return         それ以外の不整合を検出した場合は `CPLAT_ERR_CORRUPT_DESCRIPTOR` を返します。
     *
     *  署名、形式版、バイト順序の目印、行数と行幅、全体のバイト数、内容のハッシュ値、
     *  および各行の命令と定数の参照先が領域内に収まることを確認します。
     *
     *  形式版とバイト順序の目印は、本ライブラリが生成する値と一致する必要があります。\n
     *  異なる形式版のフィルター オブジェクトは `CPLAT_ERR_VERSION_MISMATCH`、
     *  バイト順序が異なる環境で生成したものは `CPLAT_ERR_CORRUPT_DESCRIPTOR` で拒否し、変換しません。\n
     *  形式版は @ref cplat_string_catalog_filter_get_info で読み取れます。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  他スレッドが @p image を同時に変更しない場合は、同時に実行できます。\n
     *  他スレッドが同時に @p image を変更する場合は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_string_catalog_filter_validate(const void *image, size_t image_size);

    /**
     *  @brief          フィルター オブジェクトのヘッダーの情報を取得します。
     *  @param[in]      image      フィルター オブジェクト。
     *  @param[in]      image_size @p image のバイト数。
     *  @param[out]     info_out   情報の格納先。
     *  @return         成功時は `CPLAT_OK` を返します。
     *  @return         引数が NULL の場合は `CPLAT_ERR_INVALID_ARGUMENT` を返します。
     *  @return         @p image の形式版が異なる場合は `CPLAT_ERR_VERSION_MISMATCH`、
     *                  それ以外の検証の失敗は `CPLAT_ERR_CORRUPT_DESCRIPTOR` を返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  他スレッドが @p image を同時に変更しない場合は、同時に実行できます。\n
     *  他スレッドが同時に @p image を変更する場合は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_string_catalog_filter_get_info(const void *image, size_t image_size,
                                                                    cplat_string_catalog_filter_info *info_out);

    /**
     *  @brief          指定した行の条件式を復元します。
     *  @param[in]      image      フィルター オブジェクト。
     *  @param[in]      image_size @p image のバイト数。
     *  @param[in]      line_index 復元する行 (0 起点)。
     *  @param[out]     dest       復元した条件式の格納先。常に NUL 終端します。
     *  @param[in]      dest_size  @p dest のバイト数。1 以上です。
     *  @return         成功時は `CPLAT_OK` を返します。
     *  @return         引数が不正な場合は `CPLAT_ERR_INVALID_ARGUMENT` を返します。
     *  @return         @p image の形式版が異なる場合は `CPLAT_ERR_VERSION_MISMATCH`、
     *                  それ以外の検証の失敗は `CPLAT_ERR_CORRUPT_DESCRIPTOR` を返します。
     *  @return         @p dest に収まらない場合は、切り詰めたうえで `CPLAT_ERR_BUFFER_TOO_SMALL` を返します。
     *
     *  括弧は演算子の優先順位から必要な位置にだけ付与します。\n
     *  復元した条件式を再びコンパイルすると、同じ命令列になります。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  他スレッドが @p image を同時に変更しない場合は、同時に実行できます。\n
     *  他スレッドが同時に @p image を変更する場合は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_string_catalog_filter_decompile_line(const void *image, size_t image_size,
                                                                          size_t line_index, char *dest,
                                                                          size_t dest_size);

    /**
     *  @brief          カタログの定義から、カタログの識別値を求めます。
     *  @param[in]      catalog        カタログ。
     *  @param[out]     catalog_id_out 識別値の格納先。0 にはなりません。
     *  @return         成功時は `CPLAT_OK` を返します。
     *  @return         引数が NULL の場合、または @p catalog の項目数が負、または項目を持つのに配列が NULL の場合は
     *                  `CPLAT_ERR_INVALID_ARGUMENT` を返します。
     *
     *  識別値は、判定に関わる定義から求めるハッシュ値です。
     *  項目数と、項目ごとの文字列キー、分類値、ID、引数の種別と名前を、定義の並び順に使います。\n
     *  書式、説明文、備考は判定に関わらないため含めません。
     *  これらだけを変えたカタログは、同じ識別値になります。
     *
     *  ソース領域へ公開するときに指定し、取り込む側のスロットのカタログと一致することを確かめるために使います。\n
     *  同じ定義からは、プロセスや実行環境によらず同じ値を求めます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_string_catalog_filter_get_catalog_id(const cplat_string_catalog *catalog,
                                                                         uint64_t *catalog_id_out);

    /**
     *  @brief          ソース領域へフィルター オブジェクトを公開します。
     *  @param[in,out]  source        ソース領域の先頭アドレス。
     *                                @ref CPLAT_STRING_CATALOG_FILTER_SOURCE_ALIGNMENT の倍数のアドレスである必要があります。
     *  @param[in]      source_size   @p source のバイト数。
     *  @param[in]      image         公開するフィルター オブジェクト。
     *  @param[in]      image_size    @p image のバイト数。
     *  @param[in]      catalog_id    @p image を判定に使うカタログの識別値 (@ref cplat_string_catalog_filter_get_catalog_id)。
     *                                取り込む側は、自分のカタログの識別値と一致する公開内容だけを取り込みます。
     *  @param[in]      lock          ソース領域の排他を取得、解放する関数の組。書き込みの間だけ取得します。
     *                                書き込み側が 1 つしかないなど、呼び出し側で直列化する場合は NULL を指定できます。
     *  @param[out]     revision_out 版番号の格納先。不要な場合は NULL を指定できます。
     *  @return         成功時は `CPLAT_OK` を返します。
     *  @return         @p source または @p image が NULL の場合、@p source のアラインメントが合わない場合、
     *                  @p source_size がヘッダーとフィルター オブジェクトを格納できない場合、
     *                  @p catalog_id が 0 の場合、または @p lock の関数が NULL の場合は `CPLAT_ERR_INVALID_ARGUMENT` を返します。
     *  @return         @p image が `cplat_string_catalog_filter_validate` の確認を通らない場合は、その結果コードを返します。
     *  @return         @p source が 0 で埋まっておらず、ソース領域の署名が一致して形式版が異なる場合は
     *                  `CPLAT_ERR_VERSION_MISMATCH` を返します。
     *  @return         @p source が 0 で埋まっておらず、ソース領域の署名またはヘッダー長が異なる場合は
     *                  `CPLAT_ERR_CORRUPT_DESCRIPTOR` を返します。
     *  @return         @p lock の排他を取得できない場合は、取得する関数の結果コードを返します。
     *
     *  失敗した場合は、ソース領域を変更しません。\n
     *  引数と @p image の検証は、排他を取得する前に行います。ソース領域のヘッダーの確認と書き込みは、排他の下で行います。
     *
     *  版番号は、前回の版番号に 2 を加えた値です。時刻には依存しません。\n
     *  書き込み側どうしは直列化するため (スレッド セーフの項を参照)、同じ版番号を 2 回公開しません。\n
     *  ファイルをマップした領域は OS の再起動を越えて残りますが、その場合も前回の値から増やし続けます。\n
     *  前回の値に 2 を加えると 64 ビットを超える場合は、2 へ戻します。
     *  読み取り側は大小ではなく不一致で変化を判定するため、戻った版番号も取り込みます。\n
     *  書き込みの間は版番号を奇数にし、書き終えてから新しい偶数の値を書き込みます。\n
     *  読み取り側は、奇数の版番号と、読み取りの前後で変わった版番号の内容を取り込みません。\n
     *  書き込みの途中で処理が中断した場合は版番号が奇数のまま残り、読み取り側は適用済みの条件を使い続けます。\n
     *  次の公開で、この状態から回復します。
     *
     *  0 で埋まった領域は、未公開のソース領域として扱います。\n
     *  公開済みの領域を 0 で埋め直すと、版番号は 2 から数え直します。
     *  取り込み済みの版番号と一致して変化を検知できないことがあるため、
     *  埋め直した場合は、読み取り側のスロットへ @ref cplat_string_catalog_filter_slot_attach_source で結び付け直してください。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  読み取り側とは同時に実行できます。\n
     *  @p lock を指定した場合、同じソース領域への公開はその排他で直列化します。
     *  NULL の場合は、プロセスをまたぐ場合も含め、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_string_catalog_filter_source_publish(
        void *source, size_t source_size, const void *image, size_t image_size, uint64_t catalog_id,
        const cplat_string_catalog_filter_source_lock *lock, uint64_t *revision_out);

    /**
     *  @brief          ソース領域のヘッダーから公開の情報を読み取ります。
     *  @param[in]      source      ソース領域の先頭アドレス。
     *                              @ref CPLAT_STRING_CATALOG_FILTER_SOURCE_ALIGNMENT の倍数のアドレスである必要があります。
     *  @param[in]      source_size @p source のバイト数。
     *  @param[out]     info_out    情報の格納先。
     *  @return         成功時は `CPLAT_OK` を返します。未公開の場合も `CPLAT_OK` を返し、各メンバーへ 0 を格納します。
     *  @return         引数が NULL の場合、@p source のアラインメントが合わない場合、
     *                  または @p source_size がヘッダーに満たない場合は `CPLAT_ERR_INVALID_ARGUMENT` を返します。
     *  @return         ソース領域の署名が一致して形式版が異なる場合は `CPLAT_ERR_VERSION_MISMATCH` を返します。
     *  @return         ソース領域の署名またはヘッダー長が異なる場合は `CPLAT_ERR_CORRUPT_DESCRIPTOR` を返します。
     *  @return         書き込み中のため一貫した内容を読み取れない場合は `CPLAT_ERR_BUSY` を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。書き込み側と同時に実行できます。
     */
    CPLAT_EXPORT int CPLAT_API cplat_string_catalog_filter_source_get_info(
        const void *source, size_t source_size, cplat_string_catalog_filter_source_info *info_out);

    /* ===== フィルター スロットの層 (カタログ定義と結び付く) ===== */

    /**
     *  @brief          フィルター スロットを作成します。
     *  @param[in]      catalog        判定の対象とするカタログ。スロットを破棄するまで有効である必要があります。
     *  @param[in]      key_names      文字列キーの名前解決テーブル。@p key_name_count が 0 の場合は NULL を指定できます。
     *                                 スロットを破棄するまで有効である必要があります。
     *  @param[in]      key_name_count @p key_names の要素数。
     *  @param[in]      category_names 分類値の名前。分類値を数値だけで扱う場合は NULL を指定します。
     *                                 スロットを破棄するまで有効である必要があります。
     *  @param[in]      line_capacity  適用するフィルター オブジェクトの行数の上限。
     *  @param[in]      line_width     適用するフィルター オブジェクトの行幅。
     *  @param[out]     slot_out       作成したスロットの格納先。
     *  @return         成功時は `CPLAT_OK` を返します。
     *  @return         引数が不正な場合は `CPLAT_ERR_INVALID_ARGUMENT` を返します。
     *  @return         項目を持つ @p catalog が `cplat_string_catalog_verify` の確認を通らない場合は、その結果コードを返します。
     *  @return         @p key_names に NULL の名前、重複する名前、カタログに存在しない文字列キーがある場合は
     *                  `CPLAT_ERR_INVALID_ARGUMENT` を返します。
     *  @return         @p category_names の名前の配列、要素、主語のいずれかが NULL の場合、または要素数が 0 の場合は
     *                  `CPLAT_ERR_INVALID_ARGUMENT` を返します。
     *  @return         メモリを確保できない場合は `CPLAT_ERR_OUT_OF_MEMORY` を返します。
     *  @return         同期オブジェクトを作成できない場合は、作成関数の結果コードを返します。
     *
     *  @p slot_out が NULL でない場合、失敗時は @p slot_out へ NULL を格納します。\n
     *  判定と説明文はカタログの引数定義を直接参照するため、作成時にカタログ全体を `cplat_string_catalog_verify` で確認します。\n
     *  項目数が 0 のカタログは、@ref cplat_string_catalog::entries が NULL でも確認せずに受け付けます。どの文字列キーにも一致しない、何もしないスロットになります。\n
     *  異なる名前が同じ文字列キーを指すことは許可します。\n
     *  作成直後のスロットは、行を持たないフィルター オブジェクトを適用した状態です。\n
     *  事前計算の結果を格納する 2 面のバッファーは、この時点で確保します。
     *
     *  @p category_names を指定した場合、次の 3 点が変わります。
     *  - 条件式で分類値と比較する識別子 (例: `category <= WARNING`) を、文字列キーの名前ではなく
     *    分類値の名前で解決します。名前にない識別子は @ref CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_UNRESOLVED_CATEGORY_NAME として行を無効にします。
     *  - 分類値と比較する定数は、0 以上 @ref cplat_string_catalog_filter_category_names::count 未満の整数に限ります。
     *    範囲外の値や整数でない値は @ref CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_CATEGORY_OUT_OF_RANGE として行を無効にします。
     *  - 自然文での表現では、条件を満たす分類値の名前を列挙して表します。
     *
     *  分類値の名前は作成時に固定し、変更できません。名前の解決と範囲の確認は、適用の時点で行います。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_string_catalog_filter_slot_create(
        const cplat_string_catalog *catalog, const cplat_string_catalog_filter_key_name *key_names,
        size_t key_name_count, const cplat_string_catalog_filter_category_names *category_names, size_t line_capacity,
        size_t line_width, cplat_string_catalog_filter_slot **slot_out);

    /**
     *  @brief          フィルター スロットを破棄します。
     *  @param[in,out]  slot 破棄するスロットを保持する変数のアドレス。破棄後は NULL を設定します。
     *                       NULL または *slot が NULL の場合は何もしません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  同じスロットへの他の呼び出しが完了していることを、呼び出し側で保証してください。
     */
    CPLAT_EXPORT void CPLAT_API cplat_string_catalog_filter_slot_dispose(cplat_string_catalog_filter_slot **slot);

    /**
     *  @brief          フィルター スロットの作成に使用したカタログを取得します。
     *  @param[in]      slot フィルター スロット。
     *  @return         作成時に指定したカタログを返します。
     *  @return         @p slot が NULL の場合は NULL を返します。
     *
     *  スロットを特定のカタログ用の出力へ接続する際に、別のカタログで作成したスロットを拒否するために使用します。\n
     *  カタログは作成時に固定され、スロットを破棄するまで変わりません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT const cplat_string_catalog *CPLAT_API
    cplat_string_catalog_filter_slot_get_catalog(const cplat_string_catalog_filter_slot *slot);

    /**
     *  @brief          フィルター スロットへソース領域を結び付けます。
     *  @param[in]      slot        フィルター スロット。
     *  @param[in]      source      ソース領域の先頭アドレス。NULL を指定すると結び付けを解除します。
     *                              @ref CPLAT_STRING_CATALOG_FILTER_SOURCE_ALIGNMENT の倍数のアドレスである必要があります。
     *                              結び付けを解除するまで有効である必要があります。
     *  @param[in]      source_size @p source のバイト数。@p source が NULL の場合は無視します。
     *  @param[in]      lock        書き込み側の排他を読み取り側でも取得するための関数の組。
     *                              ロックを取らずに読み取る場合は NULL を指定します。
     *                              内容は複製して保持します。@ref cplat_string_catalog_filter_source_lock::context は
     *                              結び付けを解除するまで有効である必要があります。
     *  @return         成功時は `CPLAT_OK` を返します。
     *  @return         @p slot が NULL の場合、@p source のアラインメントが合わない場合、
     *                  @p source_size がスロットの行数の上限と行幅に対する
     *                  @ref CPLAT_STRING_CATALOG_FILTER_SOURCE_SIZE に満たない場合、
     *                  または @p lock の関数が NULL の場合は `CPLAT_ERR_INVALID_ARGUMENT` を返します。
     *
     *  結び付けた時点では取り込みません。次の判定付きの組み立てで版番号を確認し、公開済みであれば取り込みます。\n
     *  結び付けと解除のたびに、取り込みの状態を未取り込みへ戻します。適用済みの条件は変わりません。\n
     *  そのため、ファイルをマップした領域のように以前の公開内容が残っている場合は、最初の判定付きの組み立てで取り込みます。
     *
     *  ヘッダーがスロットと一致しない公開内容は取り込まず、@ref cplat_string_catalog_filter_slot_get_source_status へ記録します。
     *  ソース領域の形式版が異なる場合は `CPLAT_ERR_VERSION_MISMATCH`、
     *  署名、ヘッダー長、行数の上限、行幅が異なる場合は `CPLAT_ERR_CORRUPT_DESCRIPTOR` です。\n
     *  公開時に指定したカタログの識別値が、スロットのカタログの識別値 (@ref cplat_string_catalog_filter_get_catalog_id)
     *  と一致しない公開内容も取り込まず、`CPLAT_ERR_IDENTITY_MISMATCH` として記録します。
     *  以前の版のライブラリが公開した領域は識別値を持たないため、新しい版で公開し直すまで取り込みません。
     *
     *  版番号の確認は、@p lock の有無にかかわらず、ロックを取らない 1 回のアトミックな読み取りです。\n
     *  変化を検知した場合の取り込みは、@p lock の有無で次のように変わります。
     *  - @p lock を指定した場合は、二重確認で取り込みます。書き込み側の排他を取り、版番号を読み直して、
     *    取り込み済みでなければ複製します。複製を終えた時点で排他を解放し、検証と適用は排他の外で行います。
     *    排他を取得できない場合は取り込まず、結果コードを記録して次の判定で改めて試みます。
     *  - @p lock が NULL の場合は、ロックを取らずに複製し、複製の後に版番号を読み直します。
     *    書き込みと重なった複製は捨てます。この場合、書き込み中の領域を読むことがあります。
     *
     *  どちらの場合も、版番号が奇数の間 (書き込み中、または書き込みの途中で書き込み側が停止した状態) は取り込みません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  同じスロットで判定付きの組み立てを行っていないときに呼び出してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_string_catalog_filter_slot_attach_source(
        cplat_string_catalog_filter_slot *slot, const void *source, size_t source_size,
        const cplat_string_catalog_filter_source_lock *lock);

    /**
     *  @brief          フィルター スロットがソース領域から取り込んだ状態を取得します。
     *  @param[in]      slot       フィルター スロット。
     *  @param[out]     status_out 状態の格納先。
     *  @return         成功時は `CPLAT_OK` を返します。
     *  @return         引数が NULL の場合は `CPLAT_ERR_INVALID_ARGUMENT` を返します。
     *  @return         同期に失敗した場合は、同期関数の結果コードを返します。
     *
     *  適用に失敗した公開内容も取り込み済みとして記録し、同じ版番号の取り込みを繰り返しません。\n
     *  失敗の原因は @ref cplat_string_catalog_filter_source_status::last_result で確認します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_string_catalog_filter_slot_get_source_status(
        cplat_string_catalog_filter_slot *slot, cplat_string_catalog_filter_source_status *status_out);

    /**
     *  @brief          フィルター オブジェクトをスロットへ適用します。
     *  @param[in,out]  slot                フィルター スロット。
     *  @param[in]      image               フィルター オブジェクトの先頭。
     *  @param[in]      image_size          @p image のバイト数。
     *  @param[out]     diagnostics         無効にした行の診断情報の格納先。NULL を指定できます。
     *  @param[in]      diagnostic_capacity @p diagnostics の要素数。
     *  @param[out]     invalid_count_out   無効にした行の総数の格納先。NULL を指定できます。
     *  @return         成功時は `CPLAT_OK` を返します。無効にした行があっても成功です。
     *  @return         引数が不正な場合は `CPLAT_ERR_INVALID_ARGUMENT` を返します。
     *  @return         @p image の形式版が異なる場合は `CPLAT_ERR_VERSION_MISMATCH` を返します。現在の内容を維持します。
     *  @return         それ以外の検証の失敗、または行数の上限と行幅がスロットと一致しない場合は
     *                  `CPLAT_ERR_CORRUPT_DESCRIPTOR` を返します。現在の内容を維持します。
     *
     *  内容はスロットの内部へ複製します。\n
     *  本関数が戻った時点で @p image は参照されず、呼び出し側は直ちに再利用または解放できます。
     *
     *  名前の解決と事前計算は、判定が使用していない面へ行い、最後に参照する面を切り替えます。\n
     *  内容が同一の行は、現在の面の事前計算の結果を再利用します。
     *
     *  次の行は、その行だけを無効にして @p diagnostics へ原因を返します。
     *  - 名前を解決できない文字列キー、引数、分類値を含む行
     *  - 正規表現のパターンをコンパイルできない行
     *  - カタログのどの項目に対しても成立し得ない行 (@ref CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NEVER_SATISFIABLE)。
     *    文字列の引数を数値と比べるような型の合わない比較や、`key == A && key == B` のような矛盾する条件が該当します。
     *    決して一致しない行のため、無効にしても判定の結果は変わりません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_string_catalog_filter_slot_apply(
        cplat_string_catalog_filter_slot *slot, const void *image, size_t image_size,
        cplat_string_catalog_filter_diagnostic *diagnostics, size_t diagnostic_capacity, size_t *invalid_count_out);

    /**
     *  @brief          適用中のフィルター オブジェクトを複製して取り出します。
     *  @param[in]      slot       フィルター スロット。
     *  @param[out]     image_out  複製の格納先。
     *  @param[in]      image_size @p image_out のバイト数。
     *  @return         成功時は `CPLAT_OK` を返します。
     *  @return         引数が NULL の場合は `CPLAT_ERR_INVALID_ARGUMENT` を返します。
     *  @return         @p image_size がスロットのフィルター オブジェクトの大きさに満たない場合は
     *                  `CPLAT_ERR_BUFFER_TOO_SMALL` を返します。
     *
     *  名前を解決できずに無効とした行も、フィルター オブジェクトには残ります。\n
     *  有効かどうかは @ref cplat_string_catalog_filter_slot_get_line_error で判別します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_string_catalog_filter_slot_snapshot(cplat_string_catalog_filter_slot *slot,
                                                                         void *image_out, size_t image_size);

    /**
     *  @brief          適用中の条件式の 1 行について、有効かどうかと無効にした原因を取得します。
     *  @param[in]      slot       フィルター スロット。
     *  @param[in]      line_index 行の位置 (0 起点)。
     *  @param[out]     error_out  原因の格納先。有効な行では @ref CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE を格納します。
     *  @return         成功時は `CPLAT_OK` を返します。
     *  @return         引数が NULL の場合、または @p line_index が適用中のフィルター オブジェクトの行数以上の場合は
     *                  `CPLAT_ERR_INVALID_ARGUMENT` を返します。
     *  @return         同期に失敗した場合は、同期関数の結果コードを返します。
     *
     *  適用の時点で名前を解決できなかった行や、正規表現をコンパイルできなかった行は、無効として原因を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_string_catalog_filter_slot_get_line_error(
        cplat_string_catalog_filter_slot *slot, size_t line_index, cplat_string_catalog_filter_line_error *error_out);

    /**
     *  @brief          適用中の条件式の 1 行を、カタログのメタ情報を用いた自然文で表現します。
     *  @param[in]      slot       フィルター スロット。
     *  @param[in]      line_index 適用中のフィルター オブジェクトの行 (0 起点)。
     *  @param[out]     dest       自然文の格納先。常に NUL 終端します。
     *  @param[in]      dest_size  @p dest のバイト数。1 以上です。
     *  @return         成功時は `CPLAT_OK` を返します。
     *  @return         引数が不正な場合、または行が範囲外の場合は `CPLAT_ERR_INVALID_ARGUMENT` を返します。
     *  @return         適用で無効とした行の場合は `CPLAT_ERR_MALFORMED_DEFINITION` を返します。
     *  @return         @p dest に収まらない場合は、切り詰めたうえで `CPLAT_ERR_BUFFER_TOO_SMALL` を返します。
     *
     *  文字列キーの比較は項目の `brief` と `id`、引数の比較は引数の名前と説明で表します。\n
     *  分類値は値そのもので表し、意味を解釈しません。
     *  作成時に分類値の名前を指定した場合は、条件を満たす分類値の名前を列挙して表します。\n
     *  行が 1 つの項目に限定される場合は、その項目の引数の説明を使います。\n
     *  複数の項目が対象の場合は、引数を持つすべての項目で説明が一致するときに限り、その説明を使います。
     *
     *  文型は、`cplat_string_catalog_get_language` が日本語を返す場合は日本語、それ以外はニュートラル言語です。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_string_catalog_filter_slot_describe_line(cplat_string_catalog_filter_slot *slot,
                                                                              size_t line_index, char *dest,
                                                                              size_t dest_size);

    /**
     *  @brief          文字列キーに対する事前計算の状態を取得します。
     *  @param[in]      slot       フィルター スロット。
     *  @param[in]      string_key 文字列キー。
     *  @param[out]     state_out  状態の格納先。
     *  @return         成功時は `CPLAT_OK` を返します。
     *  @return         引数が NULL の場合は `CPLAT_ERR_INVALID_ARGUMENT` を返します。
     *  @return         カタログに存在しない文字列キーの場合は `CPLAT_ERR_NOT_FOUND` を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_string_catalog_filter_slot_test(cplat_string_catalog_filter_slot *slot,
                                                                     int string_key,
                                                                     cplat_string_catalog_filter_state *state_out);

    /**
     *  @brief          条件式で判定したうえで、文字列を組み立てます (va_list 版)。
     *  @param[in]      slot        フィルター スロット。
     *  @param[out]     dest        組み立てた文字列の格納先。
     *  @param[in]      dest_size   @p dest のバイト数。
     *  @param[out]     matched_out いずれかの行に一致した場合は 0 以外、一致しない場合は 0 の格納先。
     *  @param[in]      string_key  文字列キー。
     *  @param[in]      args        文字列キーの引数スキーマに従う可変長引数。
     *  @return         成功時は `CPLAT_OK` を返します。
     *  @return         @p slot、@p matched_out、@p dest のいずれかが NULL の場合、または @p dest_size が 0 の場合は
     *                  `CPLAT_ERR_INVALID_ARGUMENT` を返します。
     *  @return         文字列キーの項目の検索、引数の取り出し、文字列の組み立てに失敗した場合は、
     *                  `cplat_string_catalog_vformat` と同じ結果コードを返します。
     *  @return         判定のための同期に失敗した場合は、同期関数の結果コードを返します。
     *
     *  可変長引数は 1 回だけ取り出し、判定と文字列の組み立てで同じ値を使用します。\n
     *  判定できた場合は、一致の有無にかかわらず文字列を組み立てます。一致した文字列の扱いは呼び出し側が決めます。\n
     *  カタログに存在しない文字列キーは判定せず、`CPLAT_ERR_NOT_FOUND` を返します。
     *
     *  判定のための同期に失敗した場合は、不一致と区別するため文字列を組み立てません。\n
     *  @p matched_out が NULL でなければ、失敗時は 0 を格納します。\n
     *  @p dest が NULL でなく @p dest_size が 1 以上であれば、組み立てより前に失敗した場合は空文字列を格納します。\n
     *  組み立てで失敗した場合の @p dest の内容は、`cplat_string_catalog_vformat` と同じです。
     *
     *  ソース領域を結び付けている場合は、判定の前に版番号を 1 回だけ読みます。\n
     *  版番号が変わっていれば、フィルター オブジェクトを取り込んでから判定します。\n
     *  ほかのスレッドが取り込み中または適用中の場合は待たずに、適用済みの条件で判定します。\n
     *  書き込み側の排他を結び付けている場合は、変化を検知したときに限り、その排他を待ちます。\n
     *  取り込みの結果は本関数の戻り値に含めず、@ref cplat_string_catalog_filter_slot_get_source_status で確認します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_string_catalog_filter_slot_vformat(cplat_string_catalog_filter_slot *slot,
                                                                        char *dest, size_t dest_size, int *matched_out,
                                                                        int string_key, va_list args);

    /**
     *  @brief          条件式で判定したうえで、文字列を組み立てます。
     *
     *  引数と戻り値は @ref cplat_string_catalog_filter_slot_vformat と同じです。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_string_catalog_filter_slot_format(cplat_string_catalog_filter_slot *slot,
                                                                       char *dest, size_t dest_size, int *matched_out,
                                                                       int string_key, ...);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_STRING_CATALOG_FILTER_H */
