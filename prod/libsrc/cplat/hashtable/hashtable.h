/**
 *******************************************************************************
 *  @file           hashtable.h
 *  @brief          hashtable の実装ファイル間で共有する型、配置、内部関数を宣言します。
 *  @author         Tetsuo Honda
 *  @date           2026/08/18
 *  @version        1.0.0
 *
 *  本ヘッダーは `prod/libsrc/cplat/hashtable/` のモジュール私有ヘッダーです。\n
 *  同ディレクトリの実装ファイルからだけ `#include "hashtable.h"` で取り込みます。\n
 *  公開契約は公開ヘッダー `<cplat/hashtable/hashtable.h>` を正とします。\n
 *  次の配置は実装の説明であり、公開 ABI ではありません。
 *
 *  @section        hashtable_buffer_layout 管理領域とデータ領域の構成
 *
 *  テーブルは、識別子・ヘッダー・バケット配列・エントリ配列からなる「管理領域」と、
 *  値配列 (data[N]) からなる「データ領域」の 2 ブロックで構成されます。\n
 *  両者は連続している必要がありません。データ領域は、
 *  メモリマップド ファイル (@c cplat_mmap) など管理領域と独立した領域を
 *  想定しています。\n
 *  管理領域とデータ領域は、いずれも永続化して意味のある状態だけで構成し、
 *  アドレス情報や一時的なフラグの類は含みません。@ref cplat_hashtable_create /
 *  @ref cplat_hashtable_attach は、呼び出しのたびに管理領域・データ領域とは別の
 *  「内部管理データ」を確保して不透明ハンドルとして返します。内部管理データは
 *  実行時のみ有効な参照であり、永続化してはならず、また呼び出し方によらず
 *  @ref cplat_hashtable_dispose で必ず解放してください。\n
 *  呼び出し側が両方 NULL を渡した場合 (@ref cplat_hashtable_create) は、
 *  内部で 1 回の確保にまとめ、データ領域は管理領域の直後に連続配置します。
 *  この場合に限り、解放も @ref cplat_hashtable_dispose の 1 回で両方が
 *  片付きます。\n
 *  呼び出し側が両方を明示的に渡した場合は、それぞれ独立した領域として扱い、
 *  解放は呼び出し側の責務です。片方だけ NULL の指定は
 *  @ref CPLAT_ERR_INVALID_ARGUMENT です。\n
 *  各領域の開始は、直前領域の末尾を指定アラインメントへ切り上げた位置です。\n
 *  @p timestamp_scope はエントリ配置と管理領域サイズに使います。\n
 *  @p key_type と @p lifetime はサイズ計算に使いません。\n
 *  必要バイト数の計算入口は @ref cplat_hashtable_required_size です。\n
 *  構築済みテーブルの各領域の先頭は @ref cplat_hashtable_buffer_ref 、
 *  バイト数は @ref cplat_hashtable_buffer_size で取得できます。
 *
    @code
    管理領域 (永続化してよい。アドレス・一時フラグを含まない):
    +----------------------+
    | magic (4B)           |  // CPLAT_HASHTABLE_MAGIC (0x48544142)
    +----------------------+
    | version (4B)         |  // CPLAT_HASHTABLE_VERSION (4)
    +----------------------+
    | header               |  // config, next_empty, in_use_count, deleted_count, table_timestamp,
    |                      |  // key_storage_used, value_storage_used, key_free_count, value_free_count
    +----------------------+
    | pad to uint64        |  // sizeof(struct) が未整列のときだけ
    +----------------------+
    | bucket_head[N]       |  // N x sizeof(uint64_t)
    +----------------------+
    | pad to uint64        |
    +----------------------+
    | entries[N]           |  // N x entry_stride
    +----------------------+
    | key free list[N+1]   |  // 可変長キー時だけ (N+1) x (offset + length)
    +----------------------+
    | key storage          |  // 可変長キー時だけ key_storage_size バイト
    +----------------------+

    データ領域 (管理領域と連続しない、独立したブロック):
    +----------------------+
    | value refs[N]        |  // 可変長値時だけ offset + length
    +----------------------+
    | value free list[N+1] |  // 可変長値時だけ (N+1) x (offset + length)
    +----------------------+
    | value storage        |  // 可変長値時だけ value_storage_size バイト
    +----------------------+
    | data[N]              |  // 固定長値時だけ N x value_size
    +----------------------+

    内部管理データ (create/attach のたびに別途確保し、dispose で解放する。永続化しない):
    +----------------------+
    | hdr(ポインター)       |  // 管理領域先頭への参照
    +----------------------+
    | data(ポインター)      |  // データ領域先頭への参照
    +----------------------+
    | owns_buffer           |  // 1 なら dispose が管理領域とデータ領域を解放する
    +----------------------+
    @endcode
 *
 *  N は @p capacity です。レコード番号は 1 相対で、内部インデックスは record - 1 です。
 *
 *  @subsection     hashtable_ident マジックと版番号
 *
 *  管理領域先頭 8 バイトは識別子です。ヘッダーより前に置きます。\n
 *  マジックは 0x48544142、版番号は 4 です。\n
 *  @ref cplat_hashtable_attach は両者を検証し、不一致なら失敗します。
 *
 *  @subsection     hashtable_header ヘッダー
 *
 *  版番号の直後から、設定の複製、@p next_empty 、実装中件数、削除済み件数、
 *  可変長ストレージの使用バイト数、空きリストの要素数、
 *  テーブル横断の変更時刻が続きます。\n
 *  いずれも永続化して意味のある状態です。アドレスや一時フラグの類は管理領域に
 *  含みません(それらは内部管理データ側が持ちます)。\n
 *  @p next_empty は 1 相対の最小空きスロットです。満杯のときは 0 です。\n
 *  実装中件数・削除済み件数は @ref cplat_hashtable_count_status 等が
 *  そのまま返す走査済みの値で、各更新 API が差分更新します。\n
 *  テーブル時刻は最後にキーまたは値が変わった実時刻です。構築直後は 0 です。\n
 *  テーブル時刻は @p timestamp_scope に関わらず常に持ちます。\n
 *  ヘッダー全体の大きさ、および暗黙パディングは処理系依存です。\n
 *  管理領域バッファーは永続化ヘッダーのアラインメント境界が必要です。\n
 *  データ領域バッファーは値配列への @c memcpy 経由アクセスのみのため、
 *  アラインメント要件はありません。
 *
 *  @subsection     hashtable_buckets バケット先頭
 *
 *  @p capacity 個の @c uint64_t です。\n
 *  値は 1 相対のレコード番号です。0 は後続が無いことを示します。
 *
 *  @subsection     hashtable_entries エントリ
 *
 *  キーのオフセットとストライドは @p timestamp_scope と @p key_type で変わります。\n
 *  可変長キーは 16 バイトの永続 descriptor を置くため、キーのオフセットを
 *  uint64_t 境界へ切り上げます。固定長キーでは切り上げません。\n
 *  ストライドは、キーの終端を uint64_t 境界へ切り上げた値です。
 *
    @code
    SCOPE_RECORD:
    +--------+--------+-------+------------+------------+-------------+-------+
    | next   | status | pad   | timestamp  | generation | key         | pad   |
    | 8B     | 1B     | 7B    | timespec   | 8B         | key_size    | to 8B |
    +--------+--------+-------+------------+------------+-------------+-------+

    SCOPE_TABLE (固定長キー):
    +--------+--------+-------------+-------+
    | next   | status | key         | pad   |
    | 8B     | 1B     | key_size    | to 8B |
    +--------+--------+-------------+-------+

    SCOPE_TABLE (可変長キー):
    +--------+--------+-------+------------------+
    | next   | status | pad   | key descriptor   |
    | 8B     | 1B     | 7B    | 16B              |
    +--------+--------+-------+------------------+
    @endcode
 *
 *  @p next は同一バケットの次レコード (1 相対) です。0 は終端です。\n
 *  @p status は 0 が空、1 が実装中、2 以上が削除済みの加齢です。\n
 *  @p timestamp は実時刻の変更時刻です。status が 0 のときは 0 埋めです。\n
 *  @p generation は変更のたびに 1 ずつ増える単調な値です。status が 0 のときは 0 です。\n
 *  文字列キーは NUL までを格納し、残りを 0 埋めします。
 *
 *  @subsection     hashtable_free_list 空きリスト
 *
 *  可変長ストレージ 1 個につき、空き領域を表す descriptor の配列を 1 本持ちます。\n
 *  キー側はキー ストレージの直前、値側は値ストレージの直前に置きます。\n
 *  要素は @c offset と @c length の対で、オフセットの昇順に並べます。\n
 *  隣接する空きブロックは必ず結合し、空きブロックが常に極大であるように保ちます。\n
 *  空きブロックと使用中ブロックは、ストレージ全体を過不足なく分割します。\n
 *  空きブロックは使用中ブロックで区切られるため、要素数の上限は capacity + 1 です。\n
 *  構築直後は要素数 1 で、ストレージ全体が 1 個の空きブロックです。\n
 *  確保は先頭からの先着適合で、要素数を H として O(H) です。断片化がなければ
 *  H は 1 のため、実質 O(1) で完了します。\n
 *  要素数は永続化ヘッダーの @c key_free_count と @c value_free_count が持ちます。
 *
 *  @subsection     hashtable_data 値配列 (データ領域)
 *
 *  各値は、@p value_size を @p value_align へ切り上げた幅で並べます。\n
 *  @p value_align が 0 のときは切り上げず、@p value_size バイトを隙間なく並べます。
 *  この場合、2 件目以降のアラインメントは @p value_size 次第です。\n
 *  内部確保 (両方 NULL) のときは、管理領域の直後をデータ領域が必要とする境界へ
 *  切り上げた位置にデータ領域を置きます。\n
 *  外部指定のときは、データ領域が必要とする境界を呼び出し側が満たします。\n
 *  データ領域が必要とする境界は、可変長値では descriptor の境界、固定長値では
 *  @p value_align です。@p value_align が 0 なら境界の制約はありません。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#ifndef HASHTABLE_PRIVATE_H
#define HASHTABLE_PRIVATE_H

#include <cplat/hashtable/hashtable.h>

#include <cplat/clock/timespec.h>
#include <stddef.h>
#include <stdint.h>

/**
 *  @brief          エントリの実装状況です。
 *
 *  2 以上は削除済みの加齢カウンターです。255 は無限寿命の終端です。
 */
enum
{
    REC_EMPTY = 0,  /**< 空きスロットです。 */
    REC_IN_USE = 1, /**< 実装中です。 */
    REC_DELETED = 2 /**< 削除直後の加齢開始値です。 */
};

enum
{
    ENTRY_TIMESTAMP_PAD = 7 /**< status の直後から timespec を 8 境界へ置く詰め物です。 */
};

_Static_assert(((sizeof(uint64_t) + sizeof(unsigned char) + 7u) % _Alignof(cplat_timespec)) == 0,
               "cplat: entry time offset must satisfy alignof(cplat_timespec)");

_Static_assert(((sizeof(uint64_t) + sizeof(unsigned char) + 7u + sizeof(cplat_timespec)) % _Alignof(uint64_t)) == 0,
               "cplat: entry generation offset must satisfy alignof(uint64_t)");

#define CPLAT_HASHTABLE_MAGIC   0x48544142u /**< 管理領域先頭の識別子です。 */
#define CPLAT_HASHTABLE_VERSION 4u          /**< 配置の版番号です。 */

struct hashtable_string_ref
{
    uint64_t offset;
    uint64_t length;
};

/**
 *  @brief          可変長ストレージの空き領域 1 個を表します。
 *
 *  空きリストの要素です。オフセットの昇順に隙間なく並べ、隣接する空きブロックは
 *  必ず結合します。使用中ブロックと空きブロックは、ストレージ全体を過不足なく分割します。
 */
struct hashtable_free_block
{
    uint64_t offset; /**< 空き領域の先頭オフセットです。 */
    uint64_t length; /**< 空き領域のバイト数です。0 にはなりません。 */
};

/**
 *  @brief          永続化領域先頭のヘッダーです。
 *
 *  識別子、設定の複製、空きヒント、実装中・削除済み件数、テーブル時刻を
 *  まとめています。\n
 *  永続化して意味を持つ情報だけで構成し、実行時のみ有効なアドレスや
 *  一時的なフラグは含みません。\n
 *  配置の図は本ファイル先頭の説明を参照してください。
 */
struct hashtable_persist_header
{
    uint32_t magic;                 /**< 識別子。@c CPLAT_HASHTABLE_MAGIC 。 */
    uint32_t version;               /**< 配置の版。@c CPLAT_HASHTABLE_VERSION 。 */
    cplat_hashtable_config config;  /**< 構築時の設定の複製です。 */
    uint64_t next_empty;            /**< 1 相対の最小空きです。満杯のときは 0 です。 */
    uint64_t in_use_count;          /**< 実装中の件数です。 */
    uint64_t deleted_count;         /**< 削除済み(加齢中および終端 255 を含む)の件数です。 */
    uint64_t key_storage_used;      /**< 可変長キー ストレージの使用バイト数です。 */
    uint64_t value_storage_used;    /**< 可変長値ストレージの使用バイト数です。 */
    uint64_t key_free_count;        /**< 可変長キー ストレージの空きリストの要素数です。 */
    uint64_t value_free_count;      /**< 可変長値ストレージの空きリストの要素数です。 */
    cplat_timespec table_timestamp; /**< 最後にキーまたは値が変わった実時刻です。 */
    uint64_t table_generation;      /**< 変更のたびに 1 ずつ増える単調な値です。 */
};

/* ヘッダー サイズは hashtable_mgmt_layout の整列前提(uint64_t 境界)を満たす。 */
_Static_assert(sizeof(struct hashtable_persist_header) % _Alignof(uint64_t) == 0,
               "cplat: hashtable_persist_header size must be a multiple of _Alignof(uint64_t)");

/**
 *  @brief          ハッシュ テーブルの内部管理データです。
 *
 *  @ref cplat_hashtable_create と @ref cplat_hashtable_attach が
 *  呼び出しのたびに新規確保し、@ref cplat_hashtable_dispose が解放します。\n
 *  永続化領域・データ領域とは別の割り当てで、実行時のみ有効な参照だけを持ちます。\n
 *  領域オフセットとストライドは @p hdr->config から導出できますが、アクセサーが
 *  呼ばれるたびに再計算すると探索 1 段ごとの費用になるため、構築時に一度だけ
 *  求めて保持します。@ref hashtable_refresh_layout が設定と再設定を担います。
 */
struct cplat_hashtable
{
    struct hashtable_persist_header *hdr; /**< 永続化領域の先頭です。実行時のみ有効な内部参照です。 */
    unsigned char *data;                  /**< データ領域(値配列)の先頭です。実行時のみ有効です。 */

    /* 次は hdr->config から導出したレイアウトの控えです。永続化しません。 */
    size_t off_bucket_head; /**< 管理領域先頭からバケット配列までのバイト オフセットです。 */
    size_t off_entries;     /**< 管理領域先頭からエントリ配列までのバイト オフセットです。 */
    size_t mgmt_size;       /**< 管理領域の総バイト数です。 */
    size_t entry_stride;    /**< エントリ 1 件のバイト数です。 */
    size_t key_offset;      /**< エントリ先頭からキーまでのバイト オフセットです。 */
    size_t value_stride;    /**< 固定長値 1 件のバイト数です。 */
    size_t free_list_size;  /**< 空きリスト 1 本のバイト数です。 */

    unsigned char owns_buffer;            /**< 1 なら @ref cplat_hashtable_dispose が永続化領域と
                                         データ領域をあわせて解放します。 */
    unsigned char growable;               /**< 1 なら通常の追加・更新で自動拡張します。 */
    unsigned char key_is_variable;        /**< 1 ならキーが可変長文字列です。 */
    unsigned char value_is_variable;      /**< 1 なら値が可変長文字列です。 */
    unsigned char pad[4];                 /**< value_is_variable のあとの明示パディングです。 */
    cplat_hashtable_growth_config growth; /**< 自動拡張の上限です。永続化しません。 */
};

/**
 *  @brief          descriptor 取得関数の型です。
 *
 *  キーと値で descriptor の並びが異なるため、共通処理へ取得手段を渡します。
 */
typedef struct hashtable_string_ref *(*hashtable_ref_fn)(const cplat_hashtable *ht, size_t rec);

/**
 *  @brief          可変長ストレージ 1 個分の操作対象をまとめた一時的な束ねです。
 *
 *  キーと値では、ストレージ先頭、空きリスト先頭、容量だけが異なります。\n
 *  実行時のみ有効な参照であり、永続化しません。
 */
struct hashtable_arena
{
    unsigned char *storage;                 /**< ストレージ先頭です。 */
    struct hashtable_free_block *free_list; /**< 空きリスト先頭です。 */
    uint64_t *free_count;                   /**< 空きブロックの個数への参照です。永続化ヘッダー内を指します。 */
    size_t storage_size;                    /**< ストレージのバイト数です。 */
};

/**
 *  @brief          自動拡張を要する不足の種類です。
 *
 *  複数が同時に成立しうるため、ビット和で保持します。
 */
enum hashtable_growth_pressure
{
    HASHTABLE_GROWTH_NONE = 0,         /**< 不足はありません。 */
    HASHTABLE_GROWTH_CAPACITY = 1,     /**< 空きレコードが不足しています。 */
    HASHTABLE_GROWTH_KEY_STORAGE = 2,  /**< 可変長キー ストレージが不足しています。 */
    HASHTABLE_GROWTH_VALUE_STORAGE = 4 /**< 可変長値ストレージが不足しています。 */
};

/**
 *  @brief          1 回の更新操作が要求する拡張の内容です。
 *
 *  更新系の内部実装が不足を検出したときに書き、拡張の段取りへ渡します。
 */
struct hashtable_growth_request
{
    size_t key_storage_min;   /**< 可変長キー ストレージに必要な最小バイト数です。 */
    size_t value_storage_min; /**< 可変長値ストレージに必要な最小バイト数です。 */
    size_t pressure;          /**< @ref hashtable_growth_pressure のビット和です。 */
};

/**
 *  @brief          フィールド形式が可変長かを返します。
 *  @param[in]      type  フィールド形式。
 *  @return         可変長なら 1、固定長なら 0 です。
 */
static inline int hashtable_field_is_variable(cplat_hashtable_field_type type)
{
    return type == CPLAT_HASHTABLE_FIELD_VARIABLE_STRING;
}

/**
 *  @brief          エントリ先頭から世代カウンターまでのオフセットを返します。
 *  @return         バイト オフセットです。
 *
 *  SCOPE_RECORD のときだけ意味を持ちます。
 */
static inline size_t hashtable_entry_generation_offset(void)
{
    return sizeof(uint64_t) + sizeof(unsigned char) + (size_t)ENTRY_TIMESTAMP_PAD + sizeof(cplat_timespec);
}

/**
 *  @brief          レコードごとの変更時刻を持つかを返します。
 *  @param[in]      ht  対象。NULL を渡してはなりません。
 *  @return         持つなら 1、持たないなら 0 です。
 */
static inline int hashtable_has_record_timestamp(const cplat_hashtable *ht)
{
    return ht->hdr->config.timestamp_scope == CPLAT_HASHTABLE_TIMESTAMP_SCOPE_RECORD;
}

/**
 *  @brief          永続化領域をバイト列として見ます。
 *  @param[in]      ht  対象。NULL を渡してはなりません。
 *  @return         永続化領域先頭です。
 *
 *  const の @p ht から領域ポインターを共用するため、const を外します。
 */
static inline unsigned char *hashtable_bytes(const cplat_hashtable *ht)
{
    return (unsigned char *)(uintptr_t)ht->hdr;
}

/**
 *  @brief          バケット先頭配列を返します。
 *  @param[in]      ht  対象。NULL を渡してはなりません。
 *  @return         @p capacity 個の 1 相対レコード番号です。0 は空です。
 *
 *  範囲検査はしません。構築済みの @p ht を渡してください。
 */
static inline uint64_t *hashtable_bucket_head(const cplat_hashtable *ht)
{
    return (uint64_t *)(void *)(hashtable_bytes(ht) + ht->off_bucket_head);
}

/**
 *  @brief          エントリ配列の先頭を返します。
 *  @param[in]      ht  対象。NULL を渡してはなりません。
 *  @return         エントリ配列の先頭です。
 *
 *  範囲検査はしません。構築済みの @p ht を渡してください。
 */
static inline unsigned char *hashtable_entries(const cplat_hashtable *ht)
{
    return hashtable_bytes(ht) + ht->off_entries;
}

/**
 *  @brief          指定スロットの next へのポインターを返します。
 *  @param[in]      ht   対象。NULL を渡してはなりません。
 *  @param[in]      rec  0 相対のスロット インデックス。capacity 未満であること。
 *  @return         1 相対の次レコード番号です。0 は終端です。
 */
static inline uint64_t *hashtable_entry_next(const cplat_hashtable *ht, size_t rec)
{
    return (uint64_t *)(void *)(hashtable_entries(ht) + rec * ht->entry_stride);
}

/**
 *  @brief          指定スロットの status へのポインターを返します。
 *  @param[in]      ht   対象。NULL を渡してはなりません。
 *  @param[in]      rec  0 相対のスロット インデックス。capacity 未満であること。
 *  @return         実装状況へのポインターです。
 */
static inline unsigned char *hashtable_entry_status(const cplat_hashtable *ht, size_t rec)
{
    return hashtable_entries(ht) + rec * ht->entry_stride + sizeof(uint64_t);
}

/**
 *  @brief          指定スロットの変更時刻へのポインターを返します。
 *  @param[in]      ht   対象。NULL を渡してはなりません。
 *  @param[in]      rec  0 相対のスロット インデックス。capacity 未満であること。
 *  @return         変更時刻へのポインターです。
 *
 *  @p timestamp_scope が @c SCOPE_RECORD のときだけ呼べます。
 */
static inline cplat_timespec *hashtable_entry_timestamp(const cplat_hashtable *ht, size_t rec)
{
    return (cplat_timespec *)(void *)(hashtable_entries(ht) + rec * ht->entry_stride + sizeof(uint64_t) +
                                      sizeof(unsigned char) + (size_t)ENTRY_TIMESTAMP_PAD);
}

/**
 *  @brief          指定スロットの世代カウンターへのポインターを返します。
 *  @param[in]      ht   対象。NULL を渡してはなりません。
 *  @param[in]      rec  0 相対のスロット インデックス。capacity 未満であること。
 *  @return         世代カウンターへのポインターです。
 *
 *  @p timestamp_scope が @c SCOPE_RECORD のときだけ呼べます。
 */
static inline uint64_t *hashtable_entry_generation(const cplat_hashtable *ht, size_t rec)
{
    return (uint64_t *)(void *)(hashtable_entries(ht) + rec * ht->entry_stride + hashtable_entry_generation_offset());
}

/**
 *  @brief          指定スロットのキー descriptor へのポインターを返します。
 *  @param[in]      ht   対象。NULL を渡してはなりません。
 *  @param[in]      rec  0 相対のスロット インデックス。capacity 未満であること。
 *  @return         キーの永続 descriptor です。
 *
 *  可変長キーのときだけ意味を持ちます。
 */
static inline struct hashtable_string_ref *hashtable_key_ref_at(const cplat_hashtable *ht, size_t rec)
{
    return (struct hashtable_string_ref *)(void *)(hashtable_entries(ht) + rec * ht->entry_stride + ht->key_offset);
}

/**
 *  @brief          可変長キー ストレージの先頭を返します。
 *  @param[in]      ht  対象。NULL を渡してはなりません。
 *  @return         キー ストレージ先頭です。
 *
 *  キー ストレージは管理領域の末尾に置きます。
 */
static inline unsigned char *hashtable_key_storage(const cplat_hashtable *ht)
{
    return hashtable_bytes(ht) + ht->mgmt_size - ht->hdr->config.key_storage_size;
}

/**
 *  @brief          指定スロットのキーへのポインターを返します。
 *  @param[in]      ht   対象。NULL を渡してはなりません。
 *  @param[in]      rec  0 相対のスロット インデックス。capacity 未満であること。
 *  @return         key_size バイトのキーです。
 */
static inline char *hashtable_entry_key(const cplat_hashtable *ht, size_t rec)
{
    unsigned char *slot = hashtable_entries(ht) + rec * ht->entry_stride + ht->key_offset;

    if (ht->key_is_variable != 0)
    {
        const struct hashtable_string_ref *ref = (const struct hashtable_string_ref *)(const void *)slot;

        return (char *)(hashtable_key_storage(ht) + (size_t)ref->offset);
    }
    return (char *)slot;
}

/**
 *  @brief          値配列の先頭を返します。
 *  @param[in]      ht  対象。NULL を渡してはなりません。
 *  @return         値配列の先頭です。
 *
 *  データ領域は管理領域と独立したブロックのため、@p ht->data をそのまま返します。
 */
static inline unsigned char *hashtable_data(const cplat_hashtable *ht)
{
    return ht->data;
}

/**
 *  @brief          可変長値ストレージの先頭を返します。
 *  @param[in]      ht  対象。NULL を渡してはなりません。
 *  @return         値ストレージ先頭です。
 *
 *  データ領域は、値 descriptor 配列、空きリスト、値ストレージの順に並びます。
 */
static inline unsigned char *hashtable_value_storage(const cplat_hashtable *ht)
{
    return hashtable_data(ht) + ht->hdr->config.capacity * sizeof(struct hashtable_string_ref) + ht->free_list_size;
}

/**
 *  @brief          指定スロットの値 descriptor へのポインターを返します。
 *  @param[in]      ht   対象。NULL を渡してはなりません。
 *  @param[in]      rec  0 相対のスロット インデックス。capacity 未満であること。
 *  @return         値の永続 descriptor です。
 *
 *  可変長値のときだけ意味を持ちます。
 */
static inline struct hashtable_string_ref *hashtable_value_ref_at(const cplat_hashtable *ht, size_t rec)
{
    return &((struct hashtable_string_ref *)(void *)hashtable_data(ht))[rec];
}

/**
 *  @brief          指定スロットの値へのポインターを返します。
 *  @param[in]      ht   対象。NULL を渡してはなりません。
 *  @param[in]      rec  0 相対のスロット インデックス。capacity 未満であること。
 *  @return         value_size バイトの値です。
 */
static inline unsigned char *hashtable_data_at(const cplat_hashtable *ht, size_t rec)
{
    if (ht->value_is_variable != 0)
    {
        return hashtable_value_storage(ht) + (size_t)hashtable_value_ref_at(ht, rec)->offset;
    }
    return hashtable_data(ht) + rec * ht->value_stride;
}

/**
 *  @brief          可変長キーの空きリスト先頭を返します。
 *  @param[in]      ht  対象。NULL を渡してはなりません。
 *  @return         空きリスト先頭です。
 *
 *  空きリストはキー ストレージの直前に置きます。
 */
static inline struct hashtable_free_block *hashtable_key_free_list(const cplat_hashtable *ht)
{
    return (struct hashtable_free_block *)(void *)(hashtable_key_storage(ht) - ht->free_list_size);
}

/**
 *  @brief          可変長値の空きリスト先頭を返します。
 *  @param[in]      ht  対象。NULL を渡してはなりません。
 *  @return         空きリスト先頭です。
 *
 *  空きリストは値ストレージの直前に置きます。
 */
static inline struct hashtable_free_block *hashtable_value_free_list(const cplat_hashtable *ht)
{
    return (struct hashtable_free_block *)(void *)(hashtable_value_storage(ht) - ht->free_list_size);
}

/* ---- レイアウト算出 (hashtable_layout.c) ---- */

/**
 *  @brief          加算のあふれを検出します。
 *  @param[in]      a    加数。
 *  @param[in]      b    加数。
 *  @param[out]     sum_out  和の格納先。あふれ時は書きません。
 *  @return         成功なら 0、あふれなら 1 です。
 */
int hashtable_add_checked(size_t a, size_t b, size_t *sum_out);

/**
 *  @brief          オフセットを指定境界へ切り上げます。
 *  @param[in]      offset     元のオフセット。
 *  @param[in]      alignment  1 以上の境界。
 *  @param[out]     aligned_out  切り上げ後の格納先。あふれ時は書きません。
 *  @return         成功なら 0、あふれなら 1 です。
 */
int hashtable_align_up_checked(size_t offset, size_t alignment, size_t *aligned_out);

/**
 *  @brief          データ領域の先頭に必要なアラインメント境界を返します。
 *  @param[in]      config  設定。NULL を渡してはなりません。
 *  @return         1 以上の境界です。1 は制約なしと同じです。
 *
 *  可変長値では永続 descriptor を置くためその境界、固定長値では value_align です。
 */
size_t hashtable_data_region_align(const cplat_hashtable_config *config);

/**
 *  @brief          管理領域内の領域オフセットと総バイト数を求めます。
 *  @param[in]      config           設定。NULL を渡してはなりません。
 *  @param[out]     off_bucket_head  バケット先頭のオフセット。不要なら NULL を渡せます。
 *  @param[out]     off_entries      エントリ配列のオフセット。不要なら NULL を渡せます。
 *  @param[out]     mgmt_size_out    管理領域の総バイト数。不要なら NULL を渡せます。
 *  @return         成功なら 0、あふれなら -1 です。
 *
 *  管理領域は識別子・ヘッダー・バケット配列・エントリ配列からなり、
 *  値配列(データ領域)は含みません。\n
 *  @p timestamp_scope はエントリ配置に使います。@p lifetime は使いません。\n
 *  @p key_type は、可変長のとき空きリストを置くために使います。\n
 *  構築済みテーブルではあふれません。呼び出し側は戻り値を捨てて構いません。
 */
int hashtable_mgmt_layout(const cplat_hashtable_config *config, size_t *off_bucket_head, size_t *off_entries,
                          size_t *mgmt_size_out);

/**
 *  @brief          データ領域(値配列)の総バイト数を求めます。
 *  @param[in]      config         設定。NULL を渡してはなりません。
 *  @param[out]     data_size_out  データ領域の総バイト数。NULL を渡してはなりません。
 *  @return         成功なら 0、あふれなら -1 です。
 *
 *  データ領域は管理領域と独立したブロックのため、オフセット加算は発生しません。
 */
int hashtable_data_region_size(const cplat_hashtable_config *config, size_t *data_size_out);

/**
 *  @brief          @p hdr->config からレイアウトの控えを求め直します。
 *  @param[in,out]  ht  対象。NULL を渡してはなりません。
 *
 *  @ref cplat_hashtable の off_bucket_head 、off_entries 、mgmt_size 、entry_stride 、
 *  key_offset 、value_stride 、free_list_size と、可変長かどうかの旗を設定します。\n
 *  構築済みテーブルではあふれないため、算出の失敗は起こりません。\n
 *  @p hdr を差し替えたとき、および @p hdr->config を書き換えたときに呼びます。
 */
void hashtable_refresh_layout(cplat_hashtable *ht);

/* ---- ハッシュとキー (hashtable_key.c) ---- */

/**
 *  @brief          キーのバケット番号を求めます。
 *  @param[in]      ht   対象。NULL を渡してはなりません。
 *  @param[in]      key  キー。NULL を渡してはなりません。
 *  @return         0 以上 capacity 未満のバケット番号です。
 *
 *  djb2 を使い、最後に capacity で割った余りを返します。\n
 *  アキュムレータは幅を uint64_t に固定しています。`unsigned long` は
 *  Linux/GCC (LP64, 64bit) と Windows/MSVC (LLP64, 32bit) で幅が異なり、
 *  同じキーでも環境によってバケット番号に差異が生じるためです。\n
 *  capacity は両対象環境で size_t (64bit) のため、幅を size_t に合わせています。
 */
size_t hashtable_hash_key(const cplat_hashtable *ht, const void *key);

/**
 *  @brief          格納済みキーと探索キーが同じかを判定します。
 *  @param[in]      ht          対象。NULL を渡してはなりません。
 *  @param[in]      stored_key  スロット上のキー。
 *  @param[in]      key         探索キー。
 *  @return         一致なら 1、不一致なら 0 です。
 */
int hashtable_key_equal(const cplat_hashtable *ht, const char *stored_key, const void *key);

/**
 *  @brief          キーが key_size に収まるかを判定します。
 *  @param[in]      ht   対象。NULL を渡してはなりません。
 *  @param[in]      key  キー。NULL を渡してはなりません。
 *  @return         収まれば 1、収まらなければ 0 です。
 */
int hashtable_key_fits(const cplat_hashtable *ht, const void *key);

/**
 *  @brief          入力フィールドの保存バイト数を返します。
 *  @param[in]      type        フィールド形式。
 *  @param[in]      fixed_size  固定長フィールドの保存バイト数。
 *  @param[in]      value       入力値。NULL を渡してはなりません。
 *  @return         可変長文字列では NUL を含む長さ、それ以外では @p fixed_size です。
 */
size_t hashtable_field_input_size(cplat_hashtable_field_type type, size_t fixed_size, const void *value);

/**
 *  @brief          固定長文字列フィールドに NUL 終端まで収まるかを判定します。
 *  @param[in]      type        フィールド形式。
 *  @param[in]      fixed_size  固定長フィールドの保存バイト数。
 *  @param[in]      value       入力値。NULL を渡してはなりません。
 *  @return         収まる、または固定長文字列以外なら 1、収まらなければ 0 です。
 */
int hashtable_fixed_string_fits(cplat_hashtable_field_type type, size_t fixed_size, const void *value);

/* ---- 可変長ストレージ (hashtable_arena.c) ---- */

/**
 *  @brief          可変長キーの操作対象を組み立てます。
 *  @param[in]      ht     対象。NULL を渡してはなりません。
 *  @param[out]     arena  組み立て先。NULL を渡してはなりません。
 *
 *  可変長キーのときだけ呼べます。
 */
void hashtable_key_arena(const cplat_hashtable *ht, struct hashtable_arena *arena);

/**
 *  @brief          可変長値の操作対象を組み立てます。
 *  @param[in]      ht     対象。NULL を渡してはなりません。
 *  @param[out]     arena  組み立て先。NULL を渡してはなりません。
 *
 *  可変長値のときだけ呼べます。
 */
void hashtable_value_arena(const cplat_hashtable *ht, struct hashtable_arena *arena);

/**
 *  @brief          使用中ブロックを先頭へ詰め直します。
 *  @param[in,out]  ht       対象。NULL を渡してはなりません。
 *  @param[in,out]  arena    操作対象。NULL を渡してはなりません。
 *  @param[in]      get_ref  descriptor の取得手段。NULL を渡してはなりません。
 *  @param[in]      used     使用中バイト数の合計。
 *
 *  空きブロックを飛ばしながら左詰めするため、移動はオフセットの昇順に起き、上書きは生じません。\n
 *  descriptor の新しいオフセットは、自分より前にある空きブロックのバイト数の累積で決まります。
 *  累積和を空きリスト上へ一時的に作り、二分探索で引きます。
 */
void hashtable_arena_compact(cplat_hashtable *ht, struct hashtable_arena *arena, hashtable_ref_fn get_ref,
                             uint64_t used);

/**
 *  @brief          指定スロットのキーを解放します。
 *  @param[in,out]  ht   対象。NULL を渡してはなりません。
 *  @param[in]      rec  0 相対のスロット インデックス。capacity 未満であること。
 *
 *  可変長キーでは、ストレージを 0 埋めして空きリストへ返し、descriptor を未使用へ戻します。\n
 *  固定長キーでは、キー領域を 0 埋めします。
 */
void hashtable_release_key(cplat_hashtable *ht, size_t rec);

/**
 *  @brief          指定スロットの値を解放します。
 *  @param[in,out]  ht   対象。NULL を渡してはなりません。
 *  @param[in]      rec  0 相対のスロット インデックス。capacity 未満であること。
 *
 *  可変長値では、ストレージを 0 埋めして空きリストへ返し、descriptor を未使用へ戻します。\n
 *  固定長値では、値領域を 0 埋めします。
 */
void hashtable_release_value(cplat_hashtable *ht, size_t rec);

/**
 *  @brief          指定スロットへキーを格納します。
 *  @param[in,out]  ht              対象。NULL を渡してはなりません。
 *  @param[in]      rec             0 相対のスロット インデックス。capacity 未満であること。
 *  @param[in]      key             キー。NULL を渡してはなりません。
 *  @param[in]      storage_offset  可変長キーの格納先オフセット。固定長キーでは使いません。
 *
 *  @p storage_offset は @ref hashtable_key_storage_find_free が返した位置です。\n
 *  可変長キーでは、空きリストから領域を取り、descriptor を更新します。
 */
void hashtable_key_store(cplat_hashtable *ht, size_t rec, const void *key, size_t storage_offset);

/**
 *  @brief          指定スロットへ値を格納します。
 *  @param[in,out]  ht              対象。NULL を渡してはなりません。
 *  @param[in]      rec             0 相対のスロット インデックス。capacity 未満であること。
 *  @param[in]      value           設定した形式の値。NULL を渡してはなりません。
 *  @param[in]      storage_offset  可変長値の格納先オフセット。固定長値では使いません。
 *
 *  @p storage_offset は @ref hashtable_value_storage_find_free が返した位置です。\n
 *  可変長値では、空きリストから領域を取り、descriptor を更新します。
 */
void hashtable_value_store(cplat_hashtable *ht, size_t rec, const void *value, size_t storage_offset);

/**
 *  @brief          空きリストの構造を検査します。
 *  @param[in]      arena     操作対象。NULL を渡してはなりません。
 *  @param[in]      used      使用中バイト数の合計。
 *  @param[in]      capacity  スロット数。
 *  @return         妥当なら 0、壊れているなら -1 です。
 *
 *  個数の上限、長さの非 0、範囲、オフセットの昇順、隣接する空きブロックが結合済みであること、
 *  および空きブロックの合計が未使用バイト数と一致することを確かめます。
 */
int hashtable_arena_validate(const struct hashtable_arena *arena, uint64_t used, size_t capacity);

/**
 *  @brief          使用中ブロックが空きブロックと重ならないことを確かめます。
 *  @param[in]      arena   操作対象。NULL を渡してはなりません。
 *  @param[in]      offset  使用中ブロックの先頭オフセット。
 *  @param[in]      length  使用中ブロックのバイト数。
 *  @return         重ならないなら 0、重なるなら -1 です。
 *
 *  空きブロックは昇順かつ互いに素のため、直前と直後の空きブロックだけを見れば足ります。\n
 *  @ref hashtable_arena_validate が昇順を確かめた後に呼んでください。
 */
int hashtable_arena_validate_used_block(const struct hashtable_arena *arena, uint64_t offset, uint64_t length);

/**
 *  @brief          可変長ストレージの空きリストを未使用の初期状態へ戻します。
 *  @param[in,out]  ht  対象。NULL を渡してはなりません。
 *
 *  可変長フィールドを持つ側だけを対象にします。
 */
void hashtable_reset_arenas(cplat_hashtable *ht);

/**
 *  @brief          可変長キーの格納先を探します。
 *  @param[in]      ht          対象。NULL を渡してはなりません。
 *  @param[in]      rec         0 相対のスロット インデックス。capacity 未満であること。
 *  @param[in]      replace     非 0 なら、@p rec が現在使っているブロックも空きとみなします。
 *  @param[in]      key         格納するキー。NULL を渡してはなりません。
 *  @param[out]     offset_out  格納先オフセットの格納先。成功時だけ書きます。
 *  @return         見つかれば 1、見つからなければ 0 です。
 *
 *  固定長キーでは 0 を返して常に成功します。\n
 *  空きリストを変更しません。実際の確保は @ref hashtable_key_store が行います。\n
 *  @p replace が非 0 のときは、呼び出し側が @ref hashtable_key_store の前に
 *  @ref hashtable_release_key を呼ぶ前提です。
 */
int hashtable_key_storage_find_free(const cplat_hashtable *ht, size_t rec, int replace, const void *key,
                                    size_t *offset_out);

/**
 *  @brief          可変長値の格納先を探します。
 *  @param[in]      ht          対象。NULL を渡してはなりません。
 *  @param[in]      rec         0 相対のスロット インデックス。capacity 未満であること。
 *  @param[in]      replace     非 0 なら、@p rec が現在使っているブロックも空きとみなします。
 *  @param[in]      value       格納する値。NULL を渡してはなりません。
 *  @param[out]     offset_out  格納先オフセットの格納先。成功時だけ書きます。
 *  @return         見つかれば 1、見つからなければ 0 です。
 *
 *  固定長値では 0 を返して常に成功します。\n
 *  空きリストを変更しません。実際の確保は @ref hashtable_value_store が行います。\n
 *  @p replace が非 0 のときは、呼び出し側が @ref hashtable_value_store の前に
 *  @ref hashtable_release_value を呼ぶ前提です。
 */
int hashtable_value_storage_find_free(const cplat_hashtable *ht, size_t rec, int replace, const void *value,
                                      size_t *offset_out);

/* ---- 構築と接続 (hashtable_create.c) ---- */

/**
 *  @brief          設定の意味的な妥当性を検査します。
 *  @param[in]      config  検査対象。NULL を渡してはなりません。
 *  @return         妥当なら 0、不正なら -1 です。
 *
 *  @ref cplat_hashtable_required_size と @ref cplat_hashtable_create が
 *  同じ基準で検査するための共通実装です。レイアウト計算に使わないフィールド
 *  (@p key_type 、 @p lifetime) も含めて検査します。
 */
int hashtable_validate_config(const cplat_hashtable_config *config);

/* ---- 追加・更新・削除 (hashtable_modify.c) ---- */

/**
 *  @brief          テーブル横断の変更時刻へ現在の実時刻を刻みます。
 *  @param[in]      ht  対象。NULL を渡してはなりません。
 */
void hashtable_stamp_table(cplat_hashtable *ht);

/**
 *  @brief          reuse_deleted 用に、再利用する削除中レコードを選びます。
 *
 *  status が最大、同点なら変更時刻が最も古いレコードを選びます。\n
 *  SCOPE_TABLE でレコード世代が無い場合は、走査順 (インデックスが小さい方を優先) により
 *  自然にレコード番号が最も小さいものが残ります。\n
 *  実時刻ではなく世代カウンターで比較します。実時刻は時計の巻き戻しで逆行し、
 *  最も古いレコードを選べなくなるためです。\n
 *  チェーンからの切り離しはしません。呼び出し側が外します。
 *
 *  @param[in]      ht          対象のハンドルです。
 *  @param[in]      keep        capacity 個の候補マスク。NULL なら全レコードを候補にします。
 *                              非 NULL なら、値が 0 以外のレコードだけを候補にします。
 *  @return         見つかった場合は 1 起点のレコード番号、無ければ 0 です。
 */
uint64_t hashtable_find_best_deleted_record(const cplat_hashtable *ht, const unsigned char *keep);

/**
 *  @brief          キーを追加、または既存キーの値を書き換えます。
 *  @param[in,out]  ht              対象。NULL を渡してはなりません。
 *  @param[in]      key             キー。NULL を渡してはなりません。
 *  @param[in]      value           設定した形式の値。NULL を渡してはなりません。
 *  @param[in]      deleted_policy  削除済みの同一キーが見つかった場合の振る舞い。
 *  @param[in]      allow_update    0 なら使用中の同一キーを @ref CPLAT_ERR_DUPLICATE_KEY 、
 *                                  0 以外なら値を書き換えます。
 *  @param[out]     inserted_out    新規追加なら 1、既存更新なら 0。NULL を渡せます。
 *  @param[out]     growth_out      領域不足時の拡張要求。NULL を渡せます。
 *  @return         @ref cplat_hashtable_add と同じ結果コードです。
 *
 *  @ref cplat_hashtable_add と @ref cplat_hashtable_upsert の共通実装です。\n
 *  @p inserted_out は @ref CPLAT_OK のときだけ書きます。
 */
int hashtable_put(cplat_hashtable *ht, const void *key, const void *value,
                  cplat_hashtable_add_deleted_policy deleted_policy, int allow_update, int *inserted_out,
                  struct hashtable_growth_request *growth_out);

/**
 *  @brief          キーで指定したレコードの値を更新します。
 *  @param[in,out]  ht          対象。
 *  @param[in]      key         キー。
 *  @param[in]      value       新しい値。
 *  @param[out]     growth_out  容量不足の詳細。NULL を渡せます。
 *  @return         @ref cplat_hashtable_update と同じ結果コードです。
 */
int hashtable_update(cplat_hashtable *ht, const void *key, const void *value,
                     struct hashtable_growth_request *growth_out);

/**
 *  @brief          レコード番号で指定した値を更新します。
 *  @param[in,out]  ht          対象。
 *  @param[in]      record      1 相対のレコード番号。
 *  @param[in]      value       新しい値。
 *  @param[out]     growth_out  容量不足の詳細。NULL を渡せます。
 *  @return         @ref cplat_hashtable_update_rec と同じ結果コードです。
 */
int hashtable_update_rec(cplat_hashtable *ht, uint64_t record, const void *value,
                         struct hashtable_growth_request *growth_out);

/* ---- 自動拡張と再構築 (hashtable_grow.c) ---- */

/**
 *  @brief          @ref hashtable_put を、不足時の自動拡張付きで実行します。
 *  @param[in,out]  ht              対象。NULL を渡してはなりません。
 *  @param[in]      key             キー。NULL を渡してはなりません。
 *  @param[in]      value           設定した形式の値。NULL を渡してはなりません。
 *  @param[in]      deleted_policy  削除済みの同一キーが見つかった場合の振る舞い。
 *  @param[in]      allow_update    0 以外なら使用中の同一キーの値を書き換えます。
 *  @param[out]     inserted_out    新規追加なら 1、既存更新なら 0。NULL を渡せます。
 *  @return         @ref cplat_hashtable_add と同じ結果コードです。
 *
 *  自動拡張が無効なテーブルでは、@ref hashtable_put の結果をそのまま返します。
 */
int hashtable_put_with_growth(cplat_hashtable *ht, const void *key, const void *value,
                              cplat_hashtable_add_deleted_policy deleted_policy, int allow_update, int *inserted_out);

/**
 *  @brief          @ref hashtable_update を、不足時の自動拡張付きで実行します。
 *  @param[in,out]  ht     対象。NULL を渡してはなりません。
 *  @param[in]      key    キー。NULL を渡してはなりません。
 *  @param[in]      value  設定した形式の値。NULL を渡してはなりません。
 *  @return         @ref cplat_hashtable_update と同じ結果コードです。
 */
int hashtable_update_with_growth(cplat_hashtable *ht, const void *key, const void *value);

/**
 *  @brief          @ref hashtable_update_rec を、不足時の自動拡張付きで実行します。
 *  @param[in,out]  ht      対象。NULL を渡してはなりません。
 *  @param[in]      record  1 相対のレコード番号。
 *  @param[in]      value   設定した形式の値。NULL を渡してはなりません。
 *  @return         @ref cplat_hashtable_update_rec と同じ結果コードです。
 */
int hashtable_update_rec_with_growth(cplat_hashtable *ht, uint64_t record, const void *value);

#endif /* HASHTABLE_PRIVATE_H */
