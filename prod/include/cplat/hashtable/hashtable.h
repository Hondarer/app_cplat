/**
 *******************************************************************************
 *  @file           hashtable.h
 *  @brief          固定レコード数と可変長文字列ストレージを持つハッシュ テーブル API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/08/18
 *  @version        1.0.0
 *
 *  キーと値は個別に固定長バイナリ、固定長文字列、可変長文字列を選択できます。\n
 *  @ref cplat_hashtable_create で構築したテーブルのレコード数と可変長文字列の
 *  ストレージ容量は固定です。\n
 *  断片化で連続領域が不足した場合は @ref CPLAT_ERR_STORAGE_FULL を返すため、
 *  必要に応じて @ref cplat_hashtable_compact を明示的に呼び出してください。\n
 *  @ref cplat_hashtable_create_growable で構築した内部確保テーブルは、通常の追加と
 *  更新で不足した領域を自動的に再構築または拡張します。\n
 *  格納先の確保は、空きブロックの個数を H として O(H) です。断片化がなければ H は 1 のため、
 *  実質 O(1) で完了します。確保は既存のブロックを移動しないため、取得済みの可変長参照は
 *  後述の契機以外で無効になりません。\n
 *  空きリストはストレージごとに capacity + 1 要素を持ち、必要バイト数に含まれます。
 *  必要バイト数は @ref cplat_hashtable_required_size で求めてください。\n
 *  衝突はチェイン法で扱い、削除は寿命付きの加齢により空きへ戻します。\n
 *  lifetime が @ref CPLAT_HASHTABLE_LIFETIME_INFINITE のとき、
 *  削除済みは加齢の末に 255 で止まり、push では空へ戻りません。\n
 *  呼び出し側が用意した領域へ構築する、または構築済み領域を再接続できます。
 *
 *  レコード番号は 1 相対で、内部スロットと 1 対 1 です。\n
 *  1 から capacity までを `get_status` / `get_key_*` / `get_value_*` /
 *  `get_timestamp_*` / `get_generation` で走査できます。\n
 *  使用中のレコードだけを辿るなど、状態で絞って走査する場合は
 *  @ref cplat_hashtable_next_record を使うと、空のスロットを読み飛ばせます。\n
 *  レコード番号は、有効期間を持つ内部キーです。永続化した番号を別の機会に
 *  突き合わせる用途は対象外です。次の操作をまたぐと無効になりえます。\n
 *  - @ref cplat_hashtable_delete / @ref cplat_hashtable_delete_rec :
 *    lifetime が 2 のときは直ちに空へ戻るため、そのレコード。\n
 *  - @ref cplat_hashtable_push_deleted : 寿命に到達して空へ戻ったレコード。\n
 *  - @ref cplat_hashtable_purge_deleted : 削除済みレコードのすべて。\n
 *  - @ref cplat_hashtable_clear : すべて。\n
 *  - @ref cplat_hashtable_add / @ref cplat_hashtable_upsert :
 *    reuse_deleted が非 0 のときに追い出された削除済みレコード。\n
 *  - @ref cplat_hashtable_resize / @ref cplat_hashtable_rebuild_into :
 *    縮小のときはすべて。拡大では保存されます。\n
 *  - @ref cplat_hashtable_dispose : すべて。\n
 *  @ref cplat_hashtable_find_recno で得た番号は、上記に該当する操作を
 *  挟まない範囲で使ってください。\n
 *  空へ戻ったスロットと、追い出された削除済みスロットは、別のキーへ再利用されます。\n
 *  テーブルは常に横断の変更時刻と世代カウンターを持ちます。変更時刻は最後にキーまたは
 *  値が変わった実時刻、世代カウンターは変更のたびに 1 ずつ増える単調な値です。\n
 *  実時刻は時計の巻き戻しで逆行しうるため、変更の前後関係を判定する用途には
 *  世代カウンターを使ってください。実時刻は表示と外部システムとの相関に使います。\n
 *  @ref CPLAT_HASHTABLE_TIMESTAMP_SCOPE_RECORD のときは、各レコードも
 *  実時刻 (`cplat_timespec`) の変更時刻と世代カウンターを持ち、status が
 *  0 以外のときだけ有効です。
 *
 *  テーブルは、識別子・ヘッダー・バケット配列・エントリ配列からなる「管理領域」と、
 *  値配列からなる「データ領域」の 2 領域で構成されます。いずれも永続化して
 *  意味のある状態だけで構成し、アドレス情報や一時的なフラグの類は含みません。\n
 *  呼び出し側が両方省略すれば内部で 1 回の確保にまとめて構築し、解放も
 *  @ref cplat_hashtable_dispose の 1 回で両方が片付きます。\n
 *  両方を明示的に指定すれば、独立した 2 領域(データ領域はメモリマップド
 *  ファイルなど管理領域と連続しない領域を想定)へ構築・再接続できます。\n
 *  片方だけを指定することはできません。
 *
 *  固定長バイナリ値は value_size バイトのバイト列として memcpy で複製します。\n
 *  固定長文字列は NUL までを複製し、保存領域の未使用部分を内部で 0 埋めします。\n
 *  可変長文字列への参照は、そのフィールドの更新・回収・再利用、または
 *  @ref cplat_hashtable_compact / @ref cplat_hashtable_clear /
 *  @ref cplat_hashtable_resize / @ref cplat_hashtable_rebuild_into /
 *  自動拡張版の書き込みに伴う再構築 /
 *  @ref cplat_hashtable_dispose まで有効です。\n
 *  @ref cplat_hashtable_resize 、@ref cplat_hashtable_rebuild_into 、自動再構築は、
 *  残すレコードを移行先の領域へ再配置するため、配置が変わります。\n
 *  値の入力ポインターに型のアラインメントは要求しません。\n
 *  値の格納境界は config の value_align で決まります。0 (既定) では値を隙間なく
 *  並べ、`find_value_ref` / `get_value_ref` が返すポインターに型のアラインメントを
 *  保証しないため、型付きポインターとして直接参照せず memcpy で取り出してください。\n
 *  value_align に非 0 を指定した場合に限り、返るポインターがその境界に整列している
 *  ことを保証するため、境界を満たす型のポインターとして直接参照できます。
 *
 *  @ref cplat_hashtable_create と @ref cplat_hashtable_attach は、
 *  呼び出しのたびに管理領域・データ領域とは別の「内部管理データ」を確保して
 *  不透明ハンドル (@ref cplat_hashtable) として返します。内部管理データは
 *  実行時のみ有効な参照であり、管理領域・データ領域を外部指定した場合でも、
 *  ハンドル自体の解放のため呼び出し方によらず必ず @ref cplat_hashtable_dispose
 *  を呼び出してください。
 *
 *  永続化の入出力とエンディアン変換は本 API の対象外です。\n
 *  読み戻しは同一環境 (同一ビット幅・同一アラインメント規則) を前提とします。\n
 *  本ライブラリは、単体型と構造体に同じアラインメント規則が適用される環境を
 *  前提とします。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#ifndef CPLAT_HASHTABLE_HASHTABLE_H
#define CPLAT_HASHTABLE_HASHTABLE_H

#include <stddef.h>
#include <stdint.h>

#include <cplat/base/result.h>
#include <cplat/clock/timespec.h>
#include <cplat/cplat_export.h>

/**
 *  @ingroup        CPLAT_HASHTABLE
 *  @{
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          キーまたは値の格納形式です。
     */
    typedef enum cplat_hashtable_field_type
    {
        CPLAT_HASHTABLE_FIELD_FIXED_BINARY = 0,   /**< 固定長バイナリ。全バイト 0 も有効。 */
        CPLAT_HASHTABLE_FIELD_FIXED_STRING = 1,   /**< 固定長領域の NUL 終端文字列。 */
        CPLAT_HASHTABLE_FIELD_VARIABLE_STRING = 2 /**< 固定容量ストレージ上の可変長 NUL 終端文字列。 */
    } cplat_hashtable_field_type;

#define CPLAT_HASHTABLE_KEY_STRING CPLAT_HASHTABLE_FIELD_FIXED_STRING
#define CPLAT_HASHTABLE_KEY_BINARY CPLAT_HASHTABLE_FIELD_FIXED_BINARY
    typedef cplat_hashtable_field_type cplat_hashtable_key_type;

#define CPLAT_HASHTABLE_LIFETIME_INFINITE 255 /**< 削除済みを空へ戻さない寿命です。 */

#define CPLAT_HASHTABLE_VALUE_ALIGN_MAX 16 /**< value_align に指定できる最大境界です。 */

    /**
     *  @brief          変更時刻をどの粒度で持つかです。
     *
     *  ゼロ値はテーブル横断のみです。\n
     *  @ref CPLAT_HASHTABLE_TIMESTAMP_SCOPE_RECORD はテーブル横断に加え、
     *  レコードごとの変更時刻も持ちます。
     */
    typedef enum cplat_hashtable_timestamp_scope
    {
        CPLAT_HASHTABLE_TIMESTAMP_SCOPE_TABLE = 0, /**< テーブル横断の変更時刻だけを持つ。 */
        CPLAT_HASHTABLE_TIMESTAMP_SCOPE_RECORD = 1 /**< テーブル横断に加え、レコードごとの変更時刻も持つ。 */
    } cplat_hashtable_timestamp_scope;

    /**
     *  @brief          ハッシュ テーブルの生成設定です。
     *
     *  pad1 と pad2 は明示パディングを兼ねる予約です。\n
     *  @p value_align は固定長値の格納境界です。レイアウト入力を兼ねます。\n
     *  呼び出し側はゼロ初期化してから必要なフィールドだけを設定してください。\n
     *  @p timestamp_scope はエントリ配置と管理領域サイズを決めます。\n
     *  ゼロ値は @ref CPLAT_HASHTABLE_TIMESTAMP_SCOPE_TABLE です。
     */
    typedef struct cplat_hashtable_config
    {
        size_t capacity;                                 /**< バケット数兼エントリ数。0 は不正。 */
        cplat_hashtable_field_type key_type;             /**< キーの格納形式。 */
        cplat_hashtable_field_type value_type;           /**< 値の格納形式。 */
        cplat_hashtable_timestamp_scope timestamp_scope; /**< 変更時刻の粒度。レイアウト入力。 */
        unsigned char pad1[4];                           /**< key_size の直前の予約。常に 0。 */
        size_t key_size;                                 /**< 固定長キーのバイト数。可変長では 0。 */
        size_t value_size;                               /**< 固定長値のバイト数。可変長では 0。 */
        size_t key_storage_size;                         /**< 可変長キーのストレージ容量バイト数。固定長では 0。 */
        size_t value_storage_size;                       /**< 可変長値のストレージ容量バイト数。固定長では 0。 */
        size_t value_align;     /**< 固定長値の格納境界。0 は詰めて配置。非 0 は 2 の冪かつ
                                 @ref CPLAT_HASHTABLE_VALUE_ALIGN_MAX 以下。可変長値では 0。 */
        unsigned char lifetime; /**< 削除済みの寿命。2 から 254 は有限、255 は無限。 */
        unsigned char pad2[3];  /**< reuse_deleted の直前の予約。常に 0。 */
        int reuse_deleted; /**< 0 以外なら、status=0 のレコードが無いとき最古の削除中レコードを再利用する。既定は 0。 */
    } cplat_hashtable_config;

    /**
     *  @brief          自動拡張するハッシュ テーブルの上限設定です。
     *
     *  各フィールドの 0 は上限なしを表します。非 0 の値は、生成時の
     *  @ref cplat_hashtable_config にある対応する初期値以上である必要があります。\n
     *  固定長のキーまたは値に対応するストレージ上限には 0 を指定してください。
     */
    typedef struct cplat_hashtable_growth_config
    {
        size_t max_capacity;           /**< 自動拡張後の最大レコード数。0 は上限なし。 */
        size_t max_key_storage_size;   /**< 可変長キー ストレージの最大バイト数。0 は上限なし。 */
        size_t max_value_storage_size; /**< 可変長値ストレージの最大バイト数。0 は上限なし。 */
    } cplat_hashtable_growth_config;

    /**
     *  @brief          ハッシュ テーブルの不透明ハンドルです。
     *
     *  メンバーへ直接アクセスしてはなりません。\n
     *  @ref cplat_hashtable_create / @ref cplat_hashtable_attach が
     *  呼び出しのたびに内部管理データとして新規確保し、
     *  @ref cplat_hashtable_dispose が解放します。管理領域・データ領域とは
     *  別の割り当てであり、永続化してはなりません。
     */
    typedef struct cplat_hashtable cplat_hashtable;

    /**
     *  @brief          外部バッファーで構築する場合に必要な最小バイト数を求めます。
     *  @param[in]      config         設定。NULL を渡してはなりません。
     *  @param[out]     mgmt_size_out  管理領域の必要バイト数の格納先。不要なら NULL を渡せます。
     *  @param[out]     data_size_out  データ領域の必要バイト数の格納先。不要なら NULL を渡せます。
     *  @return         @ref CPLAT_OK 、または @ref CPLAT_ERR_INVALID_ARGUMENT 。
     *
     *  @p mgmt_size_out と @p data_size_out の両方に NULL を渡してはなりません。\n
     *  @ref cplat_hashtable_create と同じ基準で @p config を検証します。\n
     *  @p timestamp_scope は管理領域サイズに使います。\n
     *  フィールド型、固定長サイズ、可変長ストレージ容量はレイアウトに使います。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_required_size(const cplat_hashtable_config *config,
                                                             size_t *mgmt_size_out, size_t *data_size_out);

    /**
     *  @brief          ハッシュ テーブルを構築します。
     *  @param[in]      config         設定。NULL を渡してはなりません。
     *  @param[in]      buf_mgmt       管理領域。NULL のときは内部確保します。
     *  @param[in]      buf_mgmt_size  @p buf_mgmt のバイト数。@p buf_mgmt が NULL のときは無視します。
     *  @param[in]      buf_data       データ領域。NULL のときは内部確保します。
     *  @param[in]      buf_data_size  @p buf_data のバイト数。@p buf_data が NULL のときは無視します。
     *  @param[out]     ht_out         ハンドルの格納先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_BUFFER_TOO_SMALL 、@ref CPLAT_ERR_OUT_OF_MEMORY 。
     *
     *  失敗時は *@p ht_out を NULL にします。\n
     *  @p buf_mgmt と @p buf_data は、ともに NULL かともに非 NULL である必要があります。\n
     *  片方だけ NULL は @ref CPLAT_ERR_INVALID_ARGUMENT です。\n
     *  ともに NULL のときは、内部で 1 回の確保にまとめて構築し、
     *  解放も @ref cplat_hashtable_dispose の 1 回で両方が片付きます。\n
     *  ともに非 NULL のとき、容量不足やアラインメント不正なら領域へアクセスせずに失敗します。\n
     *  容量不足は @ref CPLAT_ERR_BUFFER_TOO_SMALL 、アラインメント不正は
     *  @ref CPLAT_ERR_INVALID_ARGUMENT です。\n
     *  @p timestamp_scope は @ref CPLAT_HASHTABLE_TIMESTAMP_SCOPE_TABLE または
     *  @ref CPLAT_HASHTABLE_TIMESTAMP_SCOPE_RECORD である必要があります。\n
     *  @p buf_mgmt は内部ヘッダーのアラインメント境界が必要です。\n
     *  @p buf_data に必要な境界は値の形式で決まります。固定長値で value_align が 0 の
     *  ときは要件がなく、非 0 のときはその境界が必要です。\n
     *  可変長値では永続 descriptor を持つため、@p buf_data は uint64_t 境界が必要です。\n
     *  外部領域の所有権は呼び出し側に残りますが、内部管理データ(返るハンドル自体)は
     *  本関数が新規確保するため、外部指定の場合でも @ref cplat_hashtable_dispose の
     *  呼び出しは必要です。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_create(const cplat_hashtable_config *config, void *buf_mgmt,
                                                      size_t buf_mgmt_size, void *buf_data, size_t buf_data_size,
                                                      cplat_hashtable **ht_out);

    /**
     *  @brief          内部領域を必要に応じて自動拡張するハッシュ テーブルを構築します。
     *  @param[in]      initial_config  初期設定。NULL を渡してはなりません。
     *  @param[in]      growth_config   自動拡張の上限設定。NULL を渡してはなりません。
     *  @param[out]     ht_out          ハンドルの格納先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY 。
     *
     *  @ref cplat_hashtable_add と @ref cplat_hashtable_upsert は、空きレコード、
     *  可変長キー ストレージ、または可変長値ストレージが不足したときに自動拡張します。\n
     *  @ref cplat_hashtable_update と @ref cplat_hashtable_update_rec は、可変長値
     *  ストレージが不足したときに自動拡張します。\n
     *  総空き容量が足りていて断片化だけが原因の場合は、容量を増やさず再構築します。\n
     *  自動再構築が発生した書き込みに成功すると、このテーブルから取得済みの
     *  設定、キー、値、管理領域、およびデータ領域への参照はすべて無効になります。\n
     *  自動再構築または要求された書き込みに失敗した場合、テーブルは変更しません。\n
     *  最大レコード数へ到達した場合は @ref CPLAT_ERR_LIMIT_EXCEEDED 、ストレージ上限へ
     *  到達した場合は @ref CPLAT_ERR_STORAGE_FULL を、対象の書き込み API が返します。\n
     *  本関数で生成した領域を永続化して @ref cplat_hashtable_attach で再接続した場合、
     *  自動拡張の上限設定は復元されず、固定長テーブルとして動作します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_create_growable(const cplat_hashtable_config *initial_config,
                                                               const cplat_hashtable_growth_config *growth_config,
                                                               cplat_hashtable **ht_out);

    /**
     *  @brief          構築済み領域へ、既存内容を保ったまま再接続します。
     *  @param[in,out]  buf_mgmt       初期化済み管理領域。NULL を渡してはなりません。
     *  @param[in]      buf_mgmt_size  @p buf_mgmt のバイト数。
     *  @param[in]      buf_data       データ領域。NULL を渡してはなりません。
     *  @param[in]      buf_data_size  @p buf_data のバイト数。
     *  @param[out]     ht_out         ハンドルの格納先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_BUFFER_TOO_SMALL 、@ref CPLAT_ERR_OUT_OF_MEMORY 、
     *                  @ref CPLAT_ERR_CORRUPT_DESCRIPTOR 。
     *
     *  マジックと版番号、ヘッダー範囲、保存されている設定と件数を検証します。\n
     *  保存ヘッダーが不正な場合は @ref CPLAT_ERR_CORRUPT_DESCRIPTOR 、版番号が異なる場合は
     *  @ref CPLAT_ERR_VERSION_MISMATCH です。\n
     *  管理領域のアラインメント不正は @ref CPLAT_ERR_INVALID_ARGUMENT 、
     *  管理領域またはデータ領域の容量不足は @ref CPLAT_ERR_BUFFER_TOO_SMALL です。\n
     *  チェインの整合性は検証しないため、必要なら直後に
     *  @ref cplat_hashtable_validate を呼び出してください。\n
     *  管理領域にはデータ領域アドレスを持たないため、@p buf_data は常に呼び出し側が
     *  渡した値をそのまま使います (プロセスをまたいだ再接続や再マップに対応するためです)。\n
     *  成功時は管理領域とデータ領域の所有権を呼び出し側のままにしますが、本関数は内部管理データ
     *  (返るハンドル自体)を新規確保するため、使用後は必ず @ref cplat_hashtable_dispose を
     *  呼び出してください。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  同一 @p buf_mgmt を他スレッドが使っていないことを呼び出し側で保証してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_attach(void *buf_mgmt, size_t buf_mgmt_size, void *buf_data,
                                                      size_t buf_data_size, cplat_hashtable **ht_out);

    /**
     *  @brief          内部整合性を検証します。
     *  @param[in]      ht  対象。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY 、@ref CPLAT_ERR_CORRUPT_DESCRIPTOR 、@ref CPLAT_ERR_VERSION_MISMATCH 。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  同一テーブルへの同時呼び出しは、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_validate(const cplat_hashtable *ht);

    /**
     *  @brief          設定への参照を返します。
     *  @param[in]      ht          対象。NULL を渡してはなりません。
     *  @param[out]     config_out  参照の格納先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、または @ref CPLAT_ERR_INVALID_ARGUMENT 。
     *
     *  返るポインターは @p ht の寿命に依存します。書き換えたり解放してはなりません。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるテーブルへの同時呼び出しはできます。\n
     *  同一テーブルへの書き込みと同時に呼び出してはなりません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_get_config_ref(const cplat_hashtable *ht,
                                                              const cplat_hashtable_config **config_out);

    /**
     *  @brief          設定を呼び出し側へ複製します。
     *  @param[in]      ht          対象。NULL を渡してはなりません。
     *  @param[out]     config_out  複製先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、または @ref CPLAT_ERR_INVALID_ARGUMENT 。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるテーブルへの同時呼び出しはできます。\n
     *  同一テーブルへの書き込みと同時に呼び出してはなりません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_get_config_val(const cplat_hashtable *ht,
                                                              cplat_hashtable_config *config_out);

    /**
     *  @brief          テーブルが占める管理領域とデータ領域のバイト数を返します。
     *  @param[in]      ht             対象。NULL を渡してはなりません。
     *  @param[out]     mgmt_size_out  管理領域のバイト数の格納先。不要なら NULL を渡せます。
     *  @param[out]     data_size_out  データ領域のバイト数の格納先。不要なら NULL を渡せます。
     *  @return         @ref CPLAT_OK 、または @ref CPLAT_ERR_INVALID_ARGUMENT 。
     *
     *  @p mgmt_size_out と @p data_size_out の両方に NULL を渡してはなりません。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるテーブルへの同時呼び出しはできます。\n
     *  同一テーブルへの書き込みと同時に呼び出してはなりません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_buffer_size(const cplat_hashtable *ht, size_t *mgmt_size_out,
                                                           size_t *data_size_out);

    /**
     *  @brief          テーブルが現在管理している管理領域とデータ領域の先頭を返します。
     *  @param[in]      ht        対象。NULL を渡してはなりません。
     *  @param[out]     mgmt_out  管理領域の先頭の格納先。不要なら NULL を渡せます。
     *  @param[out]     data_out  データ領域の先頭の格納先。不要なら NULL を渡せます。
     *  @return         @ref CPLAT_OK 、または @ref CPLAT_ERR_INVALID_ARGUMENT 。
     *
     *  @p mgmt_out と @p data_out の両方に NULL を渡してはなりません。\n
     *  内部確保と外部指定のどちらで構築したかに関わらず、現在管理している領域を返します。\n
     *  各領域のバイト数は @ref cplat_hashtable_buffer_size で求めてください。\n
     *  返るポインターは @p ht の寿命に依存します。書き換えたり解放してはなりません。
     *
     *  内部確保したテーブルを永続化する用途を想定しています。\n
     *  管理領域とデータ領域は連続とは限らないため、それぞれの先頭とバイト数で個別に
     *  書き出してください。\n
     *  読み戻すときは、呼び出し側が用意した 2 領域へ読み込み、
     *  @ref cplat_hashtable_attach で再接続します。\n
     *  管理領域とデータ領域は、いずれも永続化して意味のある状態だけで構成しており、
     *  アドレス情報や一時的なフラグの類を含まないため、そのまま永続化して差し支えありません。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるテーブルへの同時呼び出しはできます。\n
     *  同一テーブルへの書き込みと同時に呼び出してはなりません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_buffer_ref(const cplat_hashtable *ht, const void **mgmt_out,
                                                          const void **data_out);

    /**
     *  @brief          add で削除済みの同一キーが見つかった場合の振る舞いです。
     */
    typedef enum cplat_hashtable_add_deleted_policy
    {
        CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE = 0, /**< 既定。value で上書きして復活させます。 */
        CPLAT_HASHTABLE_ADD_DELETED_REVIVE = 1     /**< value を無視し、削除前の値のまま復活させます。 */
    } cplat_hashtable_add_deleted_policy;

    /**
     *  @brief          キーを新規追加します。
     *  @param[in,out]  ht              対象。NULL を渡してはなりません。
     *  @param[in]      key             キー。NULL を渡してはなりません。
     *  @param[in]      value           設定した形式の値。NULL を渡してはなりません。
     *  @param[in]      deleted_policy  削除済みの同一キーが見つかった場合の振る舞い。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_OUT_OF_RANGE 、@ref CPLAT_ERR_DUPLICATE_KEY 、
     *                  @ref CPLAT_ERR_LIMIT_EXCEEDED 、@ref CPLAT_ERR_STORAGE_FULL 。
     *
     *  文字列キーは key_size バイト以内に NUL が無いと
     *  @ref CPLAT_ERR_OUT_OF_RANGE です。\n
     *  固定長文字列の未使用部分は内部で 0 埋めするため、呼び出し側で
     *  key_size または value_size バイトへ拡張して渡す必要はありません。\n
     *  使用中の同一キーは、@p deleted_policy に関わらず常に
     *  @ref CPLAT_ERR_DUPLICATE_KEY です。\n
     *  該当キーが存在しない場合の新規追加も、@p deleted_policy の影響を受けません。\n
     *  削除済みの同一キーが見つかった場合は、同じレコードを再利用したうえで
     *  @p deleted_policy に従います。\n
     *  - @ref CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE : @p value で上書きして復活させます。\n
     *  - @ref CPLAT_HASHTABLE_ADD_DELETED_REVIVE : @p value を無視し、削除前の値のまま
     *    復活させます。\n
     *  いずれの場合も成功時は、テーブルの変更時刻を @ref cplat_clock_get_realtime で刻み、
     *  テーブルの世代カウンターを 1 増やします。\n
     *  @ref CPLAT_HASHTABLE_TIMESTAMP_SCOPE_RECORD のときは、レコードにも変更時刻を刻み、
     *  増やしたあとのテーブルの世代カウンターをレコードの世代カウンターにします。\n
     *  該当キーが存在せず、空き(status=0)のレコードも無い場合、
     *  @p reuse_deleted が有効(非 0)なら削除中(status>=2)のレコードを再利用します。\n
     *  status が最大、同点なら変更時刻が最も古い(@ref CPLAT_HASHTABLE_TIMESTAMP_SCOPE_TABLE
     *  のときはレコード番号が最も小さい)レコードを選び、そこにあった削除中のキーを
     *  追い出して新しいキーを追加します。\n
     *  該当する削除中のレコードも無ければ @ref CPLAT_ERR_LIMIT_EXCEEDED です。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  同一テーブルへの同時呼び出しは、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_add(cplat_hashtable *ht, const void *key, const void *value,
                                                   cplat_hashtable_add_deleted_policy deleted_policy);

    /**
     *  @brief          キーが無ければ追加し、あれば値を書き換えます。
     *  @param[in,out]  ht            対象。NULL を渡してはなりません。
     *  @param[in]      key           キー。NULL を渡してはなりません。
     *  @param[in]      value         設定した形式の値。NULL を渡してはなりません。
     *  @param[out]     inserted_out  新規追加なら 1、既存更新なら 0。不要なら NULL を渡せます。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_OUT_OF_RANGE 、@ref CPLAT_ERR_LIMIT_EXCEEDED 、
     *                  @ref CPLAT_ERR_STORAGE_FULL 。
     *
     *  @p inserted_out は @ref CPLAT_OK のときだけ書きます。\n
     *  使用中の同一キーは値を書き換え、@p inserted_out に 0 を格納します。
     *  @ref CPLAT_ERR_DUPLICATE_KEY は返しません。\n
     *  削除済みの同一キーは @p value で上書きして復活させ、@p inserted_out に 1 を格納します。
     *  @ref cplat_hashtable_add の
     *  @ref CPLAT_HASHTABLE_ADD_DELETED_REVIVE に相当する振る舞いは選べません。
     *  値を無視する意味になり、本関数の意味と両立しないためです。\n
     *  該当キーが存在しない場合の空きの選び方と @ref CPLAT_ERR_LIMIT_EXCEEDED の条件は、
     *  @ref cplat_hashtable_add と同じです。@p reuse_deleted が有効なら削除中のレコードを
     *  再利用し、そのレコードにあったキーを追い出します。\n
     *  成功時の変更時刻と世代カウンターの刻み方も @ref cplat_hashtable_add と同じです。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  同一テーブルへの同時呼び出しは、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_upsert(cplat_hashtable *ht, const void *key, const void *value,
                                                      int *inserted_out);

    /**
     *  @brief          レコード番号を指定してキーと値と変更時刻を直接書き込みます。
     *  @param[in,out]  ht      対象。NULL を渡してはなりません。
     *  @param[in]      record  1 相対のレコード番号。
     *  @param[in]      key     キー。NULL を渡してはなりません。
     *  @param[in]      status  書き込む使用状況。1 は使用中、2 以上は削除済みの加齢です。
     *  @param[in]      value   設定した形式の値。NULL を渡してはなりません。
     *  @param[in]      timestamp  書き込む変更時刻。粒度と対応する要否があります。
     *  @param[in]      generation 書き込む世代カウンター。粒度と対応する要否があります。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_SKIPPED 、
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_OUT_OF_RANGE 、
     *                  @ref CPLAT_ERR_DUPLICATE_DEFINITION 、@ref CPLAT_ERR_DUPLICATE_KEY 、
     *                  @ref CPLAT_ERR_STORAGE_FULL 。
     *
     *  マイグレーションや再構築で、番号を保ったままレコードを置くための入口です。\n
     *  レコード番号が 0 または capacity を超える場合は
     *  @ref CPLAT_ERR_INVALID_ARGUMENT です。\n
     *  `status >= 2` かつ先の lifetime が有限で `status` が lifetime 以上なら
     *  @ref CPLAT_SKIPPED を返し、テーブルは変更しません。\n
     *  先の lifetime が @ref CPLAT_HASHTABLE_LIFETIME_INFINITE なら
     *  status 255 も書き込めます。\n
     *  先のスロットが空でない場合は @ref CPLAT_ERR_DUPLICATE_DEFINITION 、
     *  同一キーが既にある場合は @ref CPLAT_ERR_DUPLICATE_KEY です。\n
     *  @ref CPLAT_HASHTABLE_TIMESTAMP_SCOPE_RECORD では @p timestamp は必須で、
     *  渡した時刻がテーブルの変更時刻より新しいときだけテーブル時刻を更新します。\n
     *  @p generation も同様に、渡した値がテーブルの世代カウンターより大きいときだけ
     *  テーブルの世代カウンターを更新します。\n
     *  @ref CPLAT_HASHTABLE_TIMESTAMP_SCOPE_TABLE では @p timestamp に NULL 以外、
     *  または @p generation に 0 以外を渡すと @ref CPLAT_ERR_INVALID_ARGUMENT です。\n
     *  この経路ではテーブル時刻とテーブルの世代カウンターを進めません。
     *  固定長文字列の未使用部分は内部で 0 埋めするため、呼び出し側で
     *  key_size または value_size バイトへ拡張して渡す必要はありません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  同一テーブルへの同時呼び出しは、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_insert_direct(cplat_hashtable *ht, uint64_t record, const void *key,
                                                             int status, const void *value,
                                                             const cplat_timespec *timestamp, uint64_t generation);

    /**
     *  @brief          使用中の既存キーの値を書き換えます。
     *  @param[in,out]  ht     対象。NULL を渡してはなりません。
     *  @param[in]      key    キー。NULL を渡してはなりません。
     *  @param[in]      value  設定した形式の値。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_OUT_OF_RANGE 、@ref CPLAT_ERR_NOT_FOUND 、
     *                  @ref CPLAT_ERR_STORAGE_FULL 。
     *
     *  成功時は、テーブルの変更時刻を @ref cplat_clock_get_realtime で刻み、テーブルの
     *  世代カウンターを 1 増やします。\n
     *  @ref CPLAT_HASHTABLE_TIMESTAMP_SCOPE_RECORD のときは、レコードにも変更時刻を刻み、
     *  増やしたあとのテーブルの世代カウンターをレコードの世代カウンターにします。
     *  固定長文字列の未使用部分は内部で 0 埋めするため、呼び出し側で
     *  value_size バイトへ拡張して渡す必要はありません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  同一テーブルへの同時呼び出しは、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_update(cplat_hashtable *ht, const void *key, const void *value);

    /**
     *  @brief          レコード番号で値を書き換えます。
     *  @param[in,out]  ht      対象。NULL を渡してはなりません。
     *  @param[in]      record  1 相対のレコード番号。
     *  @param[in]      value   設定した形式の値。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_OUT_OF_RANGE 、@ref CPLAT_ERR_NOT_FOUND 、
     *                  @ref CPLAT_ERR_STORAGE_FULL 。
     *
     *  成功時は、テーブルの変更時刻を @ref cplat_clock_get_realtime で刻み、テーブルの
     *  世代カウンターを 1 増やします。\n
     *  @ref CPLAT_HASHTABLE_TIMESTAMP_SCOPE_RECORD のときは、レコードにも変更時刻を刻み、
     *  増やしたあとのテーブルの世代カウンターをレコードの世代カウンターにします。
     *  固定長文字列の未使用部分は内部で 0 埋めするため、呼び出し側で
     *  value_size バイトへ拡張して渡す必要はありません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  同一テーブルへの同時呼び出しは、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_update_rec(cplat_hashtable *ht, uint64_t record, const void *value);

    /**
     *  @brief          キーで値への参照を取得します。
     *  @param[in]      ht         対象。NULL を渡してはなりません。
     *  @param[in]      key        キー。NULL を渡してはなりません。
     *  @param[out]     value_out  参照の格納先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_OUT_OF_RANGE 、@ref CPLAT_ERR_NOT_FOUND 。
     *
     *  削除済みは見つからない扱いです。返るポインターは解放してはなりません。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるテーブルへの同時呼び出しはできます。\n
     *  同一テーブルへの書き込みと同時に呼び出してはなりません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_find_value_ref(const cplat_hashtable *ht, const void *key,
                                                              const void **value_out);

    /**
     *  @brief          キーで値を容量検査付きで複製します。
     *  @param[in]      ht                 対象。NULL を渡してはなりません。
     *  @param[in]      key                キー。NULL を渡してはなりません。
     *  @param[out]     dest               複製先。必要量照会では NULL。
     *  @param[in]      dest_size          複製先容量。必要量照会では 0。
     *  @param[out]     required_size_out  必要バイト数。文字列では NUL を含みます。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_BUFFER_TOO_SMALL 、@ref CPLAT_ERR_OUT_OF_RANGE 、
     *                  @ref CPLAT_ERR_NOT_FOUND 。
     *
     *  @p dest が NULL かつ @p dest_size が 0 のときは、複製せず必要量だけを返します。
     *  文字列の必要量は実際の文字列長と終端 NUL の合計です。\n
     *  固定長バイナリの必要量は value_size です。\n
     *  @ref CPLAT_ERR_BUFFER_TOO_SMALL では @p dest を変更しません。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるテーブルへの同時呼び出しはできます。\n
     *  同一テーブルへの書き込みと同時に呼び出してはなりません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_find_value_copy(const cplat_hashtable *ht, const void *key, void *dest,
                                                               size_t dest_size, size_t *required_size_out);

    /**
     *  @brief          キーからレコード番号を取得します。
     *  @param[in]      ht          対象。NULL を渡してはなりません。
     *  @param[in]      key         キー。NULL を渡してはなりません。
     *  @param[out]     record_out  1 相対番号の格納先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_OUT_OF_RANGE 、@ref CPLAT_ERR_NOT_FOUND 。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるテーブルへの同時呼び出しはできます。\n
     *  同一テーブルへの書き込みと同時に呼び出してはなりません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_find_recno(const cplat_hashtable *ht, const void *key,
                                                          uint64_t *record_out);

    /**
     *  @brief          キーで変更時刻への参照を取得します。
     *  @param[in]      ht              対象。NULL を渡してはなりません。
     *  @param[in]      key             キー。NULL を渡してはなりません。
     *  @param[out]     timestamp_out   参照の格納先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNSUPPORTED 、@ref CPLAT_ERR_OUT_OF_RANGE 、
     *                  @ref CPLAT_ERR_NOT_FOUND 。
     *
     *  @ref CPLAT_HASHTABLE_TIMESTAMP_SCOPE_TABLE では
     *  @ref CPLAT_ERR_UNSUPPORTED です。\n
     *  削除済みは見つからない扱いです。返るポインターは解放してはなりません。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるテーブルへの同時呼び出しはできます。\n
     *  同一テーブルへの書き込みと同時に呼び出してはなりません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_find_timestamp_ref(const cplat_hashtable *ht, const void *key,
                                                                  const cplat_timespec **timestamp_out);

    /**
     *  @brief          キーで変更時刻を呼び出し側へ複製します。
     *  @param[in]      ht              対象。NULL を渡してはなりません。
     *  @param[in]      key             キー。NULL を渡してはなりません。
     *  @param[out]     timestamp_out   複製先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNSUPPORTED 、@ref CPLAT_ERR_OUT_OF_RANGE 、
     *                  @ref CPLAT_ERR_NOT_FOUND 。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるテーブルへの同時呼び出しはできます。\n
     *  同一テーブルへの書き込みと同時に呼び出してはなりません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_find_timestamp_val(const cplat_hashtable *ht, const void *key,
                                                                  cplat_timespec *timestamp_out);

    /**
     *  @brief          キーで世代カウンターを取得します。
     *  @param[in]      ht              対象。NULL を渡してはなりません。
     *  @param[in]      key             キー。NULL を渡してはなりません。
     *  @param[out]     generation_out  格納先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNSUPPORTED 、@ref CPLAT_ERR_OUT_OF_RANGE 、
     *                  @ref CPLAT_ERR_NOT_FOUND 。
     *
     *  @ref CPLAT_HASHTABLE_TIMESTAMP_SCOPE_TABLE では
     *  @ref CPLAT_ERR_UNSUPPORTED です。\n
     *  削除済みは見つからない扱いです。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるテーブルへの同時呼び出しはできます。\n
     *  同一テーブルへの書き込みと同時に呼び出してはなりません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_find_generation(const cplat_hashtable *ht, const void *key,
                                                               uint64_t *generation_out);

    /**
     *  @brief          レコード番号からキーへの参照を取得します。
     *  @param[in]      ht       対象。NULL を渡してはなりません。
     *  @param[in]      record   1 相対のレコード番号。
     *  @param[out]     key_out  参照の格納先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_NOT_FOUND 。
     *
     *  空は失敗、削除済みはキーを返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるテーブルへの同時呼び出しはできます。\n
     *  同一テーブルへの書き込みと同時に呼び出してはなりません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_get_key_ref(const cplat_hashtable *ht, uint64_t record,
                                                           const void **key_out);

    /**
     *  @brief          レコード番号からキーを容量検査付きで複製します。
     *  @param[in]      ht                 対象。NULL を渡してはなりません。
     *  @param[in]      record             1 相対のレコード番号。
     *  @param[out]     dest               複製先。必要量照会では NULL。
     *  @param[in]      dest_size          複製先容量。必要量照会では 0。
     *  @param[out]     required_size_out  必要バイト数。文字列では NUL を含みます。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_BUFFER_TOO_SMALL 、@ref CPLAT_ERR_NOT_FOUND 。
     *
     *  @p dest が NULL かつ @p dest_size が 0 のときは、複製せず必要量だけを返します。\n
     *  文字列の必要量は実際の文字列長と終端 NUL の合計です。\n
     *  固定長バイナリの必要量は key_size です。\n
     *  @ref CPLAT_ERR_BUFFER_TOO_SMALL では @p dest を変更しません。\n
     *  空は失敗、削除済みは削除直前のキーを返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるテーブルへの同時呼び出しはできます。\n
     *  同一テーブルへの書き込みと同時に呼び出してはなりません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_get_key_copy(const cplat_hashtable *ht, uint64_t record, void *dest,
                                                            size_t dest_size, size_t *required_size_out);

    /**
     *  @brief          レコード番号から値への参照を取得します。
     *  @param[in]      ht         対象。NULL を渡してはなりません。
     *  @param[in]      record     1 相対のレコード番号。
     *  @param[out]     value_out  参照の格納先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_NOT_FOUND 。
     *
     *  空は失敗、削除済みは削除直前の値を返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるテーブルへの同時呼び出しはできます。\n
     *  同一テーブルへの書き込みと同時に呼び出してはなりません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_get_value_ref(const cplat_hashtable *ht, uint64_t record,
                                                             const void **value_out);

    /**
     *  @brief          レコード番号から値を容量検査付きで複製します。
     *  @param[in]      ht                 対象。NULL を渡してはなりません。
     *  @param[in]      record             1 相対のレコード番号。
     *  @param[out]     dest               複製先。必要量照会では NULL。
     *  @param[in]      dest_size          複製先容量。必要量照会では 0。
     *  @param[out]     required_size_out  必要バイト数。文字列では NUL を含みます。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_BUFFER_TOO_SMALL 、@ref CPLAT_ERR_NOT_FOUND 。
     *
     *  @p dest が NULL かつ @p dest_size が 0 のときは、複製せず必要量だけを返します。\n
     *  文字列の必要量は実際の文字列長と終端 NUL の合計です。\n
     *  固定長バイナリの必要量は value_size です。\n
     *  @ref CPLAT_ERR_BUFFER_TOO_SMALL では @p dest を変更しません。\n
     *  空は失敗、削除済みは削除直前の値を返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるテーブルへの同時呼び出しはできます。\n
     *  同一テーブルへの書き込みと同時に呼び出してはなりません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_get_value_copy(const cplat_hashtable *ht, uint64_t record, void *dest,
                                                              size_t dest_size, size_t *required_size_out);

    /**
     *  @brief          レコード番号の使用状況を取得します。
     *  @param[in]      ht          対象。NULL を渡してはなりません。
     *  @param[in]      record      1 相対のレコード番号。
     *  @param[out]     status_out  状態の格納先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、または @ref CPLAT_ERR_INVALID_ARGUMENT 。
     *
     *  0 は空、1 は使用中、2 以上は削除済みの加齢カウンタです。\n
     *  255 は lifetime が @ref CPLAT_HASHTABLE_LIFETIME_INFINITE のときの終端です。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるテーブルへの同時呼び出しはできます。\n
     *  同一テーブルへの書き込みと同時に呼び出してはなりません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_get_status(const cplat_hashtable *ht, uint64_t record, int *status_out);

    /**
     *  @brief          走査で対象とする使用状況のビットです。
     *
     *  @ref cplat_hashtable_next_record の status_mask へ、和を取って渡します。
     */
#define CPLAT_HASHTABLE_SCAN_IN_USE  0x1u /**< status が 1 のレコードを対象にします。 */
#define CPLAT_HASHTABLE_SCAN_DELETED 0x2u /**< status が 2 以上のレコードを対象にします。 */
#define CPLAT_HASHTABLE_SCAN_EMPTY   0x4u /**< status が 0 のレコードを対象にします。 */

    /**
     *  @brief          指定した使用状況の、次のレコード番号を返します。
     *  @param[in]      ht              対象。NULL を渡してはなりません。
     *  @param[in]      from            直前に返ったレコード番号。開始時は 0 を渡します。
     *  @param[in]      status_mask     対象とする使用状況のビット和。0 を渡してはなりません。
     *  @param[out]     record_out      1 相対番号の格納先。NULL を渡してはなりません。
     *  @param[out]     has_record_out  該当があれば 1、無ければ 0。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、または @ref CPLAT_ERR_INVALID_ARGUMENT 。
     *
     *  @p from に 0 を渡して開始し、以降は直前の *@p record_out をそのまま渡します。\n
     *  列挙が終わったことはエラーではありません。戻り値は @ref CPLAT_OK となり、
     *  *@p has_record_out に 0 を格納します。*@p record_out は書きません。\n
     *  *@p record_out は、*@p has_record_out が 1 のときだけ有効です。\n
     *  @p from が capacity を超える場合、および @p status_mask が 0 または未定義のビットを
     *  含む場合は @ref CPLAT_ERR_INVALID_ARGUMENT です。\n
     *  内部に状態を持たないため、破棄すべきイテレーター ハンドルはありません。\n
     *  走査中にテーブルを変更すると、レコード番号の有効期間が終わり、カーソルは意味を
     *  失います。本ファイル冒頭のレコード番号の説明を参照してください。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるテーブルへの同時呼び出しはできます。\n
     *  同一テーブルへの書き込みと同時に呼び出してはなりません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_next_record(const cplat_hashtable *ht, uint64_t from,
                                                           unsigned int status_mask, uint64_t *record_out,
                                                           int *has_record_out);

    /**
     *  @brief          レコード番号から変更時刻への参照を取得します。
     *  @param[in]      ht              対象。NULL を渡してはなりません。
     *  @param[in]      record          1 相対のレコード番号。
     *  @param[out]     timestamp_out   参照の格納先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNSUPPORTED 、@ref CPLAT_ERR_NOT_FOUND 。
     *
     *  @ref CPLAT_HASHTABLE_TIMESTAMP_SCOPE_TABLE では
     *  @ref CPLAT_ERR_UNSUPPORTED です。\n
     *  空は失敗、削除済みは削除時の時刻を返します。返るポインターは解放してはなりません。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるテーブルへの同時呼び出しはできます。\n
     *  同一テーブルへの書き込みと同時に呼び出してはなりません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_get_timestamp_ref(const cplat_hashtable *ht, uint64_t record,
                                                                 const cplat_timespec **timestamp_out);

    /**
     *  @brief          レコード番号から変更時刻を呼び出し側へ複製します。
     *  @param[in]      ht              対象。NULL を渡してはなりません。
     *  @param[in]      record          1 相対のレコード番号。
     *  @param[out]     timestamp_out   複製先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNSUPPORTED 、@ref CPLAT_ERR_NOT_FOUND 。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるテーブルへの同時呼び出しはできます。\n
     *  同一テーブルへの書き込みと同時に呼び出してはなりません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_get_timestamp_val(const cplat_hashtable *ht, uint64_t record,
                                                                 cplat_timespec *timestamp_out);

    /**
     *  @brief          レコード番号から世代カウンターを取得します。
     *  @param[in]      ht              対象。NULL を渡してはなりません。
     *  @param[in]      record          1 相対のレコード番号。
     *  @param[out]     generation_out  格納先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNSUPPORTED 、@ref CPLAT_ERR_NOT_FOUND 。
     *
     *  @ref CPLAT_HASHTABLE_TIMESTAMP_SCOPE_TABLE では
     *  @ref CPLAT_ERR_UNSUPPORTED です。\n
     *  空は失敗、削除済みは削除時の世代カウンターを返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるテーブルへの同時呼び出しはできます。\n
     *  同一テーブルへの書き込みと同時に呼び出してはなりません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_get_generation(const cplat_hashtable *ht, uint64_t record,
                                                              uint64_t *generation_out);

    /**
     *  @brief          テーブル横断の変更時刻への参照を取得します。
     *  @param[in]      ht              対象。NULL を渡してはなりません。
     *  @param[out]     timestamp_out   参照の格納先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、または @ref CPLAT_ERR_INVALID_ARGUMENT 。
     *
     *  最後にキーまたは値が変わった実時刻です。粒度に関わらず常に有効です。\n
     *  構築直後は 0 です。空テーブルでも成功します。\n
     *  返るポインターは解放してはなりません。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるテーブルへの同時呼び出しはできます。\n
     *  同一テーブルへの書き込みと同時に呼び出してはなりません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_get_table_timestamp_ref(const cplat_hashtable *ht,
                                                                       const cplat_timespec **timestamp_out);

    /**
     *  @brief          テーブル横断の変更時刻を呼び出し側へ複製します。
     *  @param[in]      ht              対象。NULL を渡してはなりません。
     *  @param[out]     timestamp_out   複製先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、または @ref CPLAT_ERR_INVALID_ARGUMENT 。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるテーブルへの同時呼び出しはできます。\n
     *  同一テーブルへの書き込みと同時に呼び出してはなりません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_get_table_timestamp_val(const cplat_hashtable *ht,
                                                                       cplat_timespec *timestamp_out);

    /**
     *  @brief          テーブル横断の世代カウンターを取得します。
     *  @param[in]      ht              対象。NULL を渡してはなりません。
     *  @param[out]     generation_out  格納先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、または @ref CPLAT_ERR_INVALID_ARGUMENT 。
     *
     *  キーまたは値が変わるたびに 1 ずつ増える単調な値です。粒度に関わらず常に有効です。\n
     *  構築直後は 0 です。空テーブルでも成功します。\n
     *  変更の前後関係を判定する場合は、実時刻ではなく本値を比較してください。実時刻は
     *  時計の巻き戻しで逆行しますが、本値は逆行しません。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるテーブルへの同時呼び出しはできます。\n
     *  同一テーブルへの書き込みと同時に呼び出してはなりません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_get_table_generation(const cplat_hashtable *ht,
                                                                    uint64_t *generation_out);

    /**
     *  @brief          使用中・削除済み・空の件数を返します。
     *  @param[in]      ht           対象。NULL を渡してはなりません。
     *  @param[out]     in_use_out   使用中件数。不要なら NULL を渡せます。
     *  @param[out]     deleted_out  削除済み件数。不要なら NULL を渡せます。
     *  @param[out]     empty_out    空件数。不要なら NULL を渡せます。
     *  @return         @ref CPLAT_OK 、または @ref CPLAT_ERR_INVALID_ARGUMENT 。
     *
     *  3 値の合計は capacity と一致します。\n
     *  永続化領域が保持する走査済みのカウンタをそのまま返すため、capacity に
     *  依存しない一定時間で完了します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるテーブルへの同時呼び出しはできます。\n
     *  同一テーブルへの書き込みと同時に呼び出してはなりません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_count_status(const cplat_hashtable *ht, size_t *in_use_out,
                                                            size_t *deleted_out, size_t *empty_out);

    /**
     *  @brief          使用中の件数を返します。
     *  @param[in]      ht         対象。NULL を渡してはなりません。
     *  @param[out]     count_out  件数の格納先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、または @ref CPLAT_ERR_INVALID_ARGUMENT 。
     *
     *  物理満杯は @ref cplat_hashtable_empty_count が 0 であることで判定します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるテーブルへの同時呼び出しはできます。\n
     *  同一テーブルへの書き込みと同時に呼び出してはなりません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_count(const cplat_hashtable *ht, size_t *count_out);

    /**
     *  @brief          削除済みの件数を返します。
     *  @param[in]      ht         対象。NULL を渡してはなりません。
     *  @param[out]     count_out  件数の格納先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、または @ref CPLAT_ERR_INVALID_ARGUMENT 。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるテーブルへの同時呼び出しはできます。\n
     *  同一テーブルへの書き込みと同時に呼び出してはなりません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_deleted_count(const cplat_hashtable *ht, size_t *count_out);

    /**
     *  @brief          空の件数を返します。
     *  @param[in]      ht         対象。NULL を渡してはなりません。
     *  @param[out]     count_out  件数の格納先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、または @ref CPLAT_ERR_INVALID_ARGUMENT 。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるテーブルへの同時呼び出しはできます。\n
     *  同一テーブルへの書き込みと同時に呼び出してはなりません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_empty_count(const cplat_hashtable *ht, size_t *count_out);

    /**
     *  @brief          キーでレコードを削除します。
     *  @param[in,out]  ht   対象。NULL を渡してはなりません。
     *  @param[in]      key  キー。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_OUT_OF_RANGE 、@ref CPLAT_ERR_NOT_FOUND 。
     *
     *  通常は削除済み (状態 2) にします。lifetime が 2 のときは直ちに空へ戻します。\n
     *  テーブルの変更時刻を刻み、テーブルの世代カウンターを 1 増やします。\n
     *  @ref CPLAT_HASHTABLE_TIMESTAMP_SCOPE_RECORD では、削除済みとして残す場合に
     *  レコードの変更時刻と世代カウンターも刻み、直ちに空へ戻す場合はレコードの
     *  変更時刻と世代カウンターを 0 埋めします。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  同一テーブルへの同時呼び出しは、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_delete(cplat_hashtable *ht, const void *key);

    /**
     *  @brief          レコード番号で削除します。加齢ルールは
     *                  @ref cplat_hashtable_delete と同じです。
     *  @param[in,out]  ht      対象。NULL を渡してはなりません。
     *  @param[in]      record  1 相対のレコード番号。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_NOT_FOUND 。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  同一テーブルへの同時呼び出しは、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_delete_rec(cplat_hashtable *ht, uint64_t record);

    /**
     *  @brief          削除済みレコードを 1 段階加齢し、寿命到達分を空へ戻します。
     *  @param[in,out]  ht  対象。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、または @ref CPLAT_ERR_INVALID_ARGUMENT 。
     *
     *  lifetime が @ref CPLAT_HASHTABLE_LIFETIME_INFINITE のとき、
     *  status 254 の次は 255 になり、255 はそれ以上増えず空へ戻りません。\n
     *  レコードとテーブルの変更時刻および世代カウンターは更新しません。\n
     *  @ref CPLAT_HASHTABLE_TIMESTAMP_SCOPE_RECORD では、空へ戻したスロットの
     *  変更時刻と世代カウンターを 0 埋めします。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  同一テーブルへの同時呼び出しは、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_push_deleted(cplat_hashtable *ht);

    /**
     *  @brief          削除済みレコードを加齢せずすべて空へ戻します。
     *  @param[in,out]  ht  対象。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、または @ref CPLAT_ERR_INVALID_ARGUMENT 。
     *
     *  残したレコードとテーブルの変更時刻および世代カウンターは変えません。\n
     *  @ref CPLAT_HASHTABLE_TIMESTAMP_SCOPE_RECORD では、空へ戻したスロットの
     *  変更時刻と世代カウンターを 0 埋めします。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  同一テーブルへの同時呼び出しは、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_purge_deleted(cplat_hashtable *ht);

    /**
     *  @brief          可変長キーおよび値のストレージを圧縮します。
     *  @param[in,out]  ht  対象。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、または @ref CPLAT_ERR_INVALID_ARGUMENT 。
     *
     *  可変長に設定されたキーと値をそれぞれストレージ先頭へ詰め、未使用領域を
     *  0 埋めします。固定長フィールドだけのテーブルでは何も変更しません。\n
     *  成功すると、このテーブルから取得済みの可変長キーおよび値への参照は
     *  すべて無効になります。論理的なキーと値は変わらないため、テーブルおよび
     *  レコードの変更時刻と世代カウンターは更新しません。\n
     *  計算量は、capacity を N、空きブロックの個数を H、使用バイト数を U として
     *  O(N log H + U) です。\n
     *  格納済みブロックの配置が変わるのは、本関数、@ref cplat_hashtable_resize 、
     *  @ref cplat_hashtable_rebuild_into 、自動拡張版の書き込みに伴う再構築です。\n
     *  本関数は同一のストレージ内で再配置し、ほかの操作は移行先の領域へ再配置します。\n
     *  @ref cplat_hashtable_add などの確保は空き領域だけを使うため、
     *  対象レコード以外のブロックを動かしません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  同一テーブルへの同時呼び出しは、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_compact(cplat_hashtable *ht);

    /**
     *  @brief          内部確保のテーブルを、レコード数とストレージ容量だけ変えて作り直します。
     *  @param[in,out]  ht          対象。NULL を渡してはなりません。
     *  @param[in]      new_config  移行後の設定。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNSUPPORTED 、@ref CPLAT_ERR_OUT_OF_MEMORY 、
     *                  @ref CPLAT_ERR_LIMIT_EXCEEDED 、@ref CPLAT_ERR_STORAGE_FULL 。
     *
     *  @ref cplat_hashtable_create に管理領域とデータ領域の両方を NULL で渡して構築した
     *  テーブルだけが対象です。外部領域のテーブルは @ref CPLAT_ERR_UNSUPPORTED となるため、
     *  @ref cplat_hashtable_rebuild_into を使ってください。\n
     *  @p new_config で変えてよいのは capacity、key_storage_size、value_storage_size の
     *  3 つだけです。ほかの項目が現在の設定と異なる場合は
     *  @ref CPLAT_ERR_INVALID_ARGUMENT です。\n
     *  @ref cplat_hashtable_create_growable で構築したテーブルでは、自動拡張設定の
     *  非 0 の最大値を超える指定も @ref CPLAT_ERR_INVALID_ARGUMENT です。\n
     *  拡大 (capacity が現在以上) では、レコード番号をすべて保存します。\n
     *  縮小ではレコード番号を保存しません。移行後に
     *  @ref cplat_hashtable_find_recno で取り直してください。\n
     *  レコードを 1 件も破棄しません。使用中のレコードが収まらない場合、および
     *  reuse_deleted が 0 で削除済みのレコードが収まらない場合は
     *  @ref CPLAT_ERR_LIMIT_EXCEEDED です。reuse_deleted が非 0 のときだけ、
     *  @ref cplat_hashtable_add の追い出しと同じ規則で削除済みを古い順に空へ戻します。\n
     *  可変長ストレージは移行時に再配置します。再配置後の使用量が新しい容量を超える場合は
     *  @ref CPLAT_ERR_STORAGE_FULL です。\n
     *  失敗した場合、テーブルは一切変更しません。\n
     *  変更時刻と世代カウンターは、テーブルもレコードも移行前の値を引き継ぎます。\n
     *  成功すると、このテーブルから取得済みのすべての参照が無効になります。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  同一テーブルへの同時呼び出しは、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_resize(cplat_hashtable *ht, const cplat_hashtable_config *new_config);

    /**
     *  @brief          呼び出し側が用意した新しい領域へ、内容を保ったまま作り直します。
     *  @param[in]      src            移行元。NULL を渡してはなりません。
     *  @param[in]      new_config     移行後の設定。NULL を渡してはなりません。
     *  @param[in]      buf_mgmt       新しい管理領域。NULL を渡してはなりません。
     *  @param[in]      buf_mgmt_size  @p buf_mgmt のバイト数。
     *  @param[in]      buf_data       新しいデータ領域。NULL を渡してはなりません。
     *  @param[in]      buf_data_size  @p buf_data のバイト数。
     *  @param[out]     ht_out         移行先ハンドルの格納先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_BUFFER_TOO_SMALL 、@ref CPLAT_ERR_OUT_OF_MEMORY 、
     *                  @ref CPLAT_ERR_LIMIT_EXCEEDED 、@ref CPLAT_ERR_STORAGE_FULL 。
     *
     *  メモリ マップド ファイルなど、外部領域のテーブルを伸長または縮小するための入口です。\n
     *  @p src は変更しません。成功後も @p src は有効で、不要になったら
     *  @ref cplat_hashtable_dispose を呼び出してください。\n
     *  必要な領域サイズは @p new_config を @ref cplat_hashtable_required_size へ渡して
     *  求めてください。@p buf_mgmt と @p buf_data の要件は
     *  @ref cplat_hashtable_create と同じです。\n
     *  変えてよい設定、レコード番号の扱い、レコードを破棄しない規則、可変長ストレージの
     *  再配置、変更時刻と世代カウンターの引き継ぎは、いずれも
     *  @ref cplat_hashtable_resize と同じです。\n
     *  失敗時は *@p ht_out を NULL にします。容量やストレージが足りない場合は、
     *  渡された領域へアクセスせずに失敗します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  @p src への書き込みと同時に呼び出してはなりません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_rebuild_into(const cplat_hashtable *src,
                                                            const cplat_hashtable_config *new_config, void *buf_mgmt,
                                                            size_t buf_mgmt_size, void *buf_data, size_t buf_data_size,
                                                            cplat_hashtable **ht_out);

    /**
     *  @brief          使用中と削除済みを含めてテーブルを空にします。
     *  @param[in,out]  ht  対象。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、または @ref CPLAT_ERR_INVALID_ARGUMENT 。
     *
     *  設定と所有権は変えません。\n
     *  空にしたスロットは 0 埋めします。\n
     *  テーブルの変更時刻を @ref cplat_clock_get_realtime で刻み、テーブルの世代カウンターを
     *  1 増やします。空にしたスロットの変更時刻と世代カウンターは 0 です。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  同一テーブルへの同時呼び出しは、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_hashtable_clear(cplat_hashtable *ht);

    /**
     *  @brief          ハンドルを解放します。
     *  @param[in]      ht  対象。NULL のときは何もしません。
     *
     *  内部管理データ(ハンドル自体)は、@ref cplat_hashtable_create /
     *  @ref cplat_hashtable_attach のどちらで得たかに関わらず常に解放します。\n
     *  管理領域とデータ領域を外部指定した場合や @ref cplat_hashtable_attach で
     *  再接続した場合でも、ハンドル自体の解放のため本関数を必ず呼び出してください。\n
     *  @ref cplat_hashtable_create に両方 NULL を渡して内部確保した場合のみ、
     *  管理領域とデータ領域もあわせて解放します。外部指定時は、@p buf_mgmt と
     *  @p buf_data それぞれの解放(メモリマップド ファイルであれば
     *  @c cplat_mmap_detach 等)が呼び出し側の責務です。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  破棄対象を他スレッドが使っていないことを呼び出し側で保証してください。
     */
    CPLAT_EXPORT void CPLAT_API cplat_hashtable_dispose(cplat_hashtable *ht);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_HASHTABLE_HASHTABLE_H */
