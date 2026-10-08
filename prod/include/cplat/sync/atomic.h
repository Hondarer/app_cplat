/**
 *******************************************************************************
 *  @file           atomic.h
 *  @brief          プラットフォーム共通のアトミック操作を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/09/27
 *  @version        1.0.0
 *
 *  ロックを取らずに複数のスレッド、または共有メモリを介して複数のプロセスが読み書きする値のための API です。\n
 *  値は専用型 (@ref cplat_atomic_u8 、@ref cplat_atomic_i32 など) に格納し、本ヘッダーの関数だけで読み書きします。
 *  通常の代入や参照で読み書きすると、データ競合となり動作は未定義です。
 *
 *  各操作はメモリ順序 (@ref cplat_memory_order) を受け取ります。
 *  - 順序を守る操作: @ref CPLAT_MEMORY_ORDER_ACQUIRE 、@ref CPLAT_MEMORY_ORDER_RELEASE 、
 *    @ref CPLAT_MEMORY_ORDER_ACQ_REL 、@ref CPLAT_MEMORY_ORDER_SEQ_CST 。
 *    ほかの値の読み書きとの前後関係を保証します。
 *  - 順序を守らない軽量な操作: @ref CPLAT_MEMORY_ORDER_RELAXED 。
 *    値そのものの不可分性 (分割アクセスされないこと) と、最適化で読み書きが省略されないことだけを保証します。
 *    統計カウンターや、ほかの同期で順序が保証される場面の目印に使います。
 *
 *  意味は C11 の `<stdatomic.h>` と同じです。
 *  C11 の `_Atomic` と `<stdatomic.h>` は MSVC の C17 モードで利用できないため、本ヘッダーが代替します。\n
 *  `volatile` はスレッド間の同期を保証しないため、同期には使わず本ヘッダーを使います
 *  (cplat のコーディング規範「アトミック操作」)。
 *
 *  実装はコンパイラの組み込み関数です。GCC は `__atomic` 組み込み関数、MSVC は `<intrin.h>` の intrinsic を使います。
 *  呼び出しの負担を避けるため `static inline` 関数として提供します (cplat のコーディング規範の例外)。\n
 *  専用型はすべてロック不要 (lock-free) の実装であり、大きさと配置は格納する整数型と同じです。
 *  そのため、共有メモリ上に置いて複数のプロセスから操作できます。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_SYNC_ATOMIC_H
#define CPLAT_SYNC_ATOMIC_H

#include <stdint.h>

#include <cplat/base/compiler.h>
#include <cplat/base/platform.h>

#if defined(COMPILER_MSVC)
    #include <intrin.h>
#endif /* COMPILER_MSVC */

/**
 *  @ingroup        CPLAT_SYNC
 *  @{
 */

/**
 *  @brief          アトミック型の静的初期化子です。
 *  @param[in]      initial_value 初期値。
 *
 *  静的記憶域期間を持つ変数と、構造体の初期化子で使用します。
 *  0 で初期化する場合は、`{0}` やゼロ初期化でもかまいません。
 *
 *  @code{.c}
    static cplat_atomic_u64 s_counter = CPLAT_ATOMIC_INIT(0);
    @endcode
 */
#define CPLAT_ATOMIC_INIT(initial_value) {(initial_value)}

#if defined(COMPILER_MSVC)
    /* _ReadWriteBarrier はコンパイラの並べ替えだけを止める。MSVC の標準ライブラリと同じく、非推奨の警告を抑止して使う。
       see: https://learn.microsoft.com/cpp/intrinsics/readwritebarrier */
    #define CPLAT_ATOMIC_COMPILER_BARRIER() \
        __pragma(warning(push)) __pragma(warning(disable : 4996)) _ReadWriteBarrier() __pragma(warning(pop))
#endif /* COMPILER_MSVC */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          アトミック操作のメモリ順序です。意味は C11 の memory_order と同じです。
     */
    typedef enum cplat_memory_order
    {
        CPLAT_MEMORY_ORDER_RELAXED = 0, /**< 順序を保証しない。値の不可分性だけを保証する。 */
        CPLAT_MEMORY_ORDER_ACQUIRE = 1, /**< 後続の読み書きが、この読み取りより前へ移らない。読み取りに使う。 */
        CPLAT_MEMORY_ORDER_RELEASE = 2, /**< 先行の読み書きが、この書き込みより後へ移らない。書き込みに使う。 */
        CPLAT_MEMORY_ORDER_ACQ_REL = 3, /**< ACQUIRE と RELEASE の両方。読み書きを伴う操作に使う。 */
        CPLAT_MEMORY_ORDER_SEQ_CST = 4  /**< すべてのスレッドから同じ順序に見える、最も強い順序。 */
    } cplat_memory_order;

    /** アトミックに操作する符号なし 8 ビット整数です。状態値の配列など、小さな値に使います。 */
    typedef struct cplat_atomic_u8
    {
        uint8_t value; /**< 値。本ヘッダーの関数以外から読み書きしないでください。 */
    } cplat_atomic_u8;

    /** アトミックに操作する符号付き 32 ビット整数です。 */
    typedef struct cplat_atomic_i32
    {
        int32_t value; /**< 値。本ヘッダーの関数以外から読み書きしないでください。 */
    } cplat_atomic_i32;

    /** アトミックに操作する符号なし 32 ビット整数です。 */
    typedef struct cplat_atomic_u32
    {
        uint32_t value; /**< 値。本ヘッダーの関数以外から読み書きしないでください。 */
    } cplat_atomic_u32;

    /** アトミックに操作する符号付き 64 ビット整数です。 */
    typedef struct cplat_atomic_i64
    {
        int64_t value; /**< 値。本ヘッダーの関数以外から読み書きしないでください。 */
    } cplat_atomic_i64;

    /** アトミックに操作する符号なし 64 ビット整数です。 */
    typedef struct cplat_atomic_u64
    {
        uint64_t value; /**< 値。本ヘッダーの関数以外から読み書きしないでください。 */
    } cplat_atomic_u64;

    /** アトミックに操作するポインターです。 */
    typedef struct cplat_atomic_ptr
    {
        void *value; /**< 値。本ヘッダーの関数以外から読み書きしないでください。 */
    } cplat_atomic_ptr;

#if defined(COMPILER_GCC)

    /* ===== GCC: __atomic 組み込み関数 =====
     * 組み込み関数はメモリ順序に定数を要求するため、switch で定数へ写す。
     * 引数が定数なら、インライン展開後に分岐は消える。
     * see: https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html */

    /** 読み取りに使えない順序 (RELEASE と ACQ_REL) は SEQ_CST として扱います。 */
    #define CPLAT_ATOMIC_GCC_LOAD(type, pointer, order) \
        __extension__({ \
            type cplat_atomic_loaded_; \
            switch (order) \
            { \
            case CPLAT_MEMORY_ORDER_RELAXED: \
                cplat_atomic_loaded_ = __atomic_load_n((pointer), __ATOMIC_RELAXED); \
                break; \
            case CPLAT_MEMORY_ORDER_ACQUIRE: \
                cplat_atomic_loaded_ = __atomic_load_n((pointer), __ATOMIC_ACQUIRE); \
                break; \
            case CPLAT_MEMORY_ORDER_RELEASE: \
            case CPLAT_MEMORY_ORDER_ACQ_REL: \
            case CPLAT_MEMORY_ORDER_SEQ_CST: \
            default: \
                cplat_atomic_loaded_ = __atomic_load_n((pointer), __ATOMIC_SEQ_CST); \
                break; \
            } \
            cplat_atomic_loaded_; \
        })

    /** 書き込みに使えない順序 (ACQUIRE と ACQ_REL) は SEQ_CST として扱います。 */
    #define CPLAT_ATOMIC_GCC_STORE(pointer, desired, order) \
        do \
        { \
            switch (order) \
            { \
            case CPLAT_MEMORY_ORDER_RELAXED: \
                __atomic_store_n((pointer), (desired), __ATOMIC_RELAXED); \
                break; \
            case CPLAT_MEMORY_ORDER_RELEASE: \
                __atomic_store_n((pointer), (desired), __ATOMIC_RELEASE); \
                break; \
            case CPLAT_MEMORY_ORDER_ACQUIRE: \
            case CPLAT_MEMORY_ORDER_ACQ_REL: \
            case CPLAT_MEMORY_ORDER_SEQ_CST: \
            default: \
                __atomic_store_n((pointer), (desired), __ATOMIC_SEQ_CST); \
                break; \
            } \
        } while (0)

    /** 読み書きを伴う操作は、すべての順序を指定できます。 */
    #define CPLAT_ATOMIC_GCC_RMW(type, builtin, pointer, operand, order) \
        __extension__({ \
            type cplat_atomic_previous_; \
            switch (order) \
            { \
            case CPLAT_MEMORY_ORDER_RELAXED: \
                cplat_atomic_previous_ = builtin((pointer), (operand), __ATOMIC_RELAXED); \
                break; \
            case CPLAT_MEMORY_ORDER_ACQUIRE: \
                cplat_atomic_previous_ = builtin((pointer), (operand), __ATOMIC_ACQUIRE); \
                break; \
            case CPLAT_MEMORY_ORDER_RELEASE: \
                cplat_atomic_previous_ = builtin((pointer), (operand), __ATOMIC_RELEASE); \
                break; \
            case CPLAT_MEMORY_ORDER_ACQ_REL: \
                cplat_atomic_previous_ = builtin((pointer), (operand), __ATOMIC_ACQ_REL); \
                break; \
            case CPLAT_MEMORY_ORDER_SEQ_CST: \
            default: \
                cplat_atomic_previous_ = builtin((pointer), (operand), __ATOMIC_SEQ_CST); \
                break; \
            } \
            cplat_atomic_previous_; \
        })

    /** 比較交換の失敗時の順序は、成功時の順序から C11 の既定と同じ規則で導きます。 */
    #define CPLAT_ATOMIC_GCC_CAS(pointer, expected, desired, order) \
        __extension__({ \
            int cplat_atomic_exchanged_; \
            switch (order) \
            { \
            case CPLAT_MEMORY_ORDER_RELAXED: \
                cplat_atomic_exchanged_ = (int)__atomic_compare_exchange_n((pointer), (expected), (desired), 0, \
                                                                           __ATOMIC_RELAXED, __ATOMIC_RELAXED); \
                break; \
            case CPLAT_MEMORY_ORDER_ACQUIRE: \
                cplat_atomic_exchanged_ = (int)__atomic_compare_exchange_n((pointer), (expected), (desired), 0, \
                                                                           __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE); \
                break; \
            case CPLAT_MEMORY_ORDER_RELEASE: \
                cplat_atomic_exchanged_ = (int)__atomic_compare_exchange_n((pointer), (expected), (desired), 0, \
                                                                           __ATOMIC_RELEASE, __ATOMIC_RELAXED); \
                break; \
            case CPLAT_MEMORY_ORDER_ACQ_REL: \
                cplat_atomic_exchanged_ = (int)__atomic_compare_exchange_n((pointer), (expected), (desired), 0, \
                                                                           __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE); \
                break; \
            case CPLAT_MEMORY_ORDER_SEQ_CST: \
            default: \
                cplat_atomic_exchanged_ = (int)__atomic_compare_exchange_n((pointer), (expected), (desired), 0, \
                                                                           __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); \
                break; \
            } \
            cplat_atomic_exchanged_; \
        })

#elif defined(COMPILER_MSVC)

    /* ===== MSVC: <intrin.h> の intrinsic =====
     * MSVC の標準ライブラリ <atomic> と同じ方式とする。
     * x64 のメモリ モデル (TSO) では、通常の読み取りは acquire、通常の書き込みは release の性質を持つ。
     * そのため読み取りと release の書き込みはコンパイラの並べ替えだけを止め、
     * seq_cst の書き込みと読み書きを伴う操作は Interlocked (完全なメモリ バリアを伴う) で行う。
     * see: https://learn.microsoft.com/cpp/intrinsics/iso-volatile-load-store
     * see: https://learn.microsoft.com/cpp/intrinsics/interlockedexchange-intrinsic-functions */

#else
    #error "cplat/sync/atomic.h: 対応していないコンパイラです。"
#endif /* COMPILER_ */

    /* ===== 8 ビット ===== */

    /**
     *  @brief          符号なし 8 ビット整数をアトミックに読みます。
     *  @param[in]      atomic 読み取る値。NULL を渡してはなりません。
     *  @param[in]      order  メモリ順序。RELAXED、ACQUIRE、SEQ_CST のいずれかを指定します。
     *                         それ以外は SEQ_CST として扱います。
     *  @return         読み取った値。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline uint8_t cplat_atomic_load_u8(const cplat_atomic_u8 *atomic, const cplat_memory_order order)
    {
#if defined(COMPILER_GCC)
        return CPLAT_ATOMIC_GCC_LOAD(uint8_t, &atomic->value, order);
#else
    const uint8_t loaded = (uint8_t)__iso_volatile_load8((const volatile char *)&atomic->value);

    if (order != CPLAT_MEMORY_ORDER_RELAXED)
    {
        CPLAT_ATOMIC_COMPILER_BARRIER();
    }
    return loaded;
#endif /* COMPILER_ */
    }

    /**
     *  @brief          符号なし 8 ビット整数をアトミックに書き込みます。
     *  @param[out]     atomic  書き込み先。NULL を渡してはなりません。
     *  @param[in]      desired 書き込む値。
     *  @param[in]      order   メモリ順序。RELAXED、RELEASE、SEQ_CST のいずれかを指定します。
     *                          それ以外は SEQ_CST として扱います。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline void cplat_atomic_store_u8(cplat_atomic_u8 *atomic, const uint8_t desired,
                                             const cplat_memory_order order)
    {
#if defined(COMPILER_GCC)
        CPLAT_ATOMIC_GCC_STORE(&atomic->value, desired, order);
#else
    switch (order)
    {
    case CPLAT_MEMORY_ORDER_RELAXED:
        __iso_volatile_store8((volatile char *)&atomic->value, (char)desired);
        break;
    case CPLAT_MEMORY_ORDER_RELEASE:
        CPLAT_ATOMIC_COMPILER_BARRIER();
        __iso_volatile_store8((volatile char *)&atomic->value, (char)desired);
        break;
    case CPLAT_MEMORY_ORDER_ACQUIRE:
    case CPLAT_MEMORY_ORDER_ACQ_REL:
    case CPLAT_MEMORY_ORDER_SEQ_CST:
    default:
        (void)_InterlockedExchange8((volatile char *)&atomic->value, (char)desired);
        break;
    }
#endif /* COMPILER_ */
    }

    /**
     *  @brief          符号なし 8 ビット整数を交換し、交換前の値を返します。
     *
     *  引数と戻り値は @ref cplat_atomic_exchange_i32 と同じです。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline uint8_t cplat_atomic_exchange_u8(cplat_atomic_u8 *atomic, const uint8_t desired,
                                                   const cplat_memory_order order)
    {
#if defined(COMPILER_GCC)
        return CPLAT_ATOMIC_GCC_RMW(uint8_t, __atomic_exchange_n, &atomic->value, desired, order);
#else
    (void)order;
    return (uint8_t)_InterlockedExchange8((volatile char *)&atomic->value, (char)desired);
#endif /* COMPILER_ */
    }

    /**
     *  @brief          符号なし 8 ビット整数の強い比較交換です。
     *
     *  引数と戻り値は @ref cplat_atomic_compare_exchange_i32 と同じです。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline int cplat_atomic_compare_exchange_u8(cplat_atomic_u8 *atomic, uint8_t *expected,
                                                       const uint8_t desired, const cplat_memory_order order)
    {
#if defined(COMPILER_GCC)
        return CPLAT_ATOMIC_GCC_CAS(&atomic->value, expected, desired, order);
#else
    const uint8_t previous =
        (uint8_t)_InterlockedCompareExchange8((volatile char *)&atomic->value, (char)desired, (char)*expected);

    (void)order;
    if (previous == *expected)
    {
        return 1;
    }
    *expected = previous;
    return 0;
#endif /* COMPILER_ */
    }

    /* ===== 32 ビット ===== */

    /**
     *  @brief          符号付き 32 ビット整数をアトミックに読みます。
     *  @param[in]      atomic 読み取る値。NULL を渡してはなりません。
     *  @param[in]      order  メモリ順序。RELAXED、ACQUIRE、SEQ_CST のいずれかを指定します。
     *                         それ以外は SEQ_CST として扱います。
     *  @return         読み取った値。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline int32_t cplat_atomic_load_i32(const cplat_atomic_i32 *atomic, const cplat_memory_order order)
    {
#if defined(COMPILER_GCC)
        return CPLAT_ATOMIC_GCC_LOAD(int32_t, &atomic->value, order);
#else
    const int32_t loaded = (int32_t)__iso_volatile_load32((const volatile int *)&atomic->value);

    if (order != CPLAT_MEMORY_ORDER_RELAXED)
    {
        CPLAT_ATOMIC_COMPILER_BARRIER();
    }
    return loaded;
#endif /* COMPILER_ */
    }

    /**
     *  @brief          符号付き 32 ビット整数をアトミックに書き込みます。
     *  @param[out]     atomic  書き込み先。NULL を渡してはなりません。
     *  @param[in]      desired 書き込む値。
     *  @param[in]      order   メモリ順序。RELAXED、RELEASE、SEQ_CST のいずれかを指定します。
     *                          それ以外は SEQ_CST として扱います。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline void cplat_atomic_store_i32(cplat_atomic_i32 *atomic, const int32_t desired,
                                              const cplat_memory_order order)
    {
#if defined(COMPILER_GCC)
        CPLAT_ATOMIC_GCC_STORE(&atomic->value, desired, order);
#else
    switch (order)
    {
    case CPLAT_MEMORY_ORDER_RELAXED:
        __iso_volatile_store32((volatile int *)&atomic->value, (int)desired);
        break;
    case CPLAT_MEMORY_ORDER_RELEASE:
        CPLAT_ATOMIC_COMPILER_BARRIER();
        __iso_volatile_store32((volatile int *)&atomic->value, (int)desired);
        break;
    case CPLAT_MEMORY_ORDER_ACQUIRE:
    case CPLAT_MEMORY_ORDER_ACQ_REL:
    case CPLAT_MEMORY_ORDER_SEQ_CST:
    default:
        (void)_InterlockedExchange((volatile long *)&atomic->value, (long)desired);
        break;
    }
#endif /* COMPILER_ */
    }

    /**
     *  @brief          符号付き 32 ビット整数を交換し、交換前の値を返します。
     *  @param[in,out]  atomic  操作する値。NULL を渡してはなりません。
     *  @param[in]      desired 新しい値。
     *  @param[in]      order   メモリ順序。
     *  @return         交換前の値。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline int32_t cplat_atomic_exchange_i32(cplat_atomic_i32 *atomic, const int32_t desired,
                                                    const cplat_memory_order order)
    {
#if defined(COMPILER_GCC)
        return CPLAT_ATOMIC_GCC_RMW(int32_t, __atomic_exchange_n, &atomic->value, desired, order);
#else
    (void)order;
    return (int32_t)_InterlockedExchange((volatile long *)&atomic->value, (long)desired);
#endif /* COMPILER_ */
    }

    /**
     *  @brief          値が期待値と等しい場合に限り、新しい値へ交換します (強い比較交換)。
     *  @param[in,out]  atomic   操作する値。NULL を渡してはなりません。
     *  @param[in,out]  expected 期待値。交換しなかった場合は、読み取った現在の値を格納します。NULL を渡してはなりません。
     *  @param[in]      desired  新しい値。
     *  @param[in]      order    交換した場合のメモリ順序。交換しなかった場合の順序は C11 の既定と同じ規則で導きます
     *                           (RELEASE は RELAXED、ACQ_REL は ACQUIRE)。
     *  @return         交換した場合は 0 以外、交換しなかった場合は 0。
     *
     *  見かけ上の失敗 (値が等しいのに失敗すること) は起きません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline int cplat_atomic_compare_exchange_i32(cplat_atomic_i32 *atomic, int32_t *expected,
                                                        const int32_t desired, const cplat_memory_order order)
    {
#if defined(COMPILER_GCC)
        return CPLAT_ATOMIC_GCC_CAS(&atomic->value, expected, desired, order);
#else
    const int32_t previous =
        (int32_t)_InterlockedCompareExchange((volatile long *)&atomic->value, (long)desired, (long)*expected);

    (void)order;
    if (previous == *expected)
    {
        return 1;
    }
    *expected = previous;
    return 0;
#endif /* COMPILER_ */
    }

    /**
     *  @brief          値に加算し、加算前の値を返します。
     *  @param[in,out]  atomic  操作する値。NULL を渡してはなりません。
     *  @param[in]      operand 加算する値。
     *  @param[in]      order   メモリ順序。
     *  @return         加算前の値。
     *
     *  桁あふれした場合は、2 の補数表現で折り返します (C の符号付き加算と異なり、未定義動作になりません)。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline int32_t cplat_atomic_fetch_add_i32(cplat_atomic_i32 *atomic, const int32_t operand,
                                                     const cplat_memory_order order)
    {
#if defined(COMPILER_GCC)
        return CPLAT_ATOMIC_GCC_RMW(int32_t, __atomic_fetch_add, &atomic->value, operand, order);
#else
    (void)order;
    return (int32_t)_InterlockedExchangeAdd((volatile long *)&atomic->value, (long)operand);
#endif /* COMPILER_ */
    }

    /**
     *  @brief          値から減算し、減算前の値を返します。
     *
     *  引数、戻り値、折り返しは @ref cplat_atomic_fetch_add_i32 と同じです。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline int32_t cplat_atomic_fetch_sub_i32(cplat_atomic_i32 *atomic, const int32_t operand,
                                                     const cplat_memory_order order)
    {
#if defined(COMPILER_GCC)
        return CPLAT_ATOMIC_GCC_RMW(int32_t, __atomic_fetch_sub, &atomic->value, operand, order);
#else
    (void)order;
    /* 2 の補数の否定は符号なしで計算し、INT32_MIN でも未定義動作にしない */
    return (int32_t)_InterlockedExchangeAdd((volatile long *)&atomic->value, (long)(int32_t)(0U - (uint32_t)operand));
#endif /* COMPILER_ */
    }

    /**
     *  @brief          符号なし 32 ビット整数をアトミックに読みます。
     *
     *  引数と戻り値は @ref cplat_atomic_load_i32 と同じです。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline uint32_t cplat_atomic_load_u32(const cplat_atomic_u32 *atomic, const cplat_memory_order order)
    {
        return (uint32_t)cplat_atomic_load_i32((const cplat_atomic_i32 *)(const void *)atomic, order);
    }

    /**
     *  @brief          符号なし 32 ビット整数をアトミックに書き込みます。
     *
     *  引数は @ref cplat_atomic_store_i32 と同じです。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline void cplat_atomic_store_u32(cplat_atomic_u32 *atomic, const uint32_t desired,
                                              const cplat_memory_order order)
    {
        cplat_atomic_store_i32((cplat_atomic_i32 *)(void *)atomic, (int32_t)desired, order);
    }

    /**
     *  @brief          符号なし 32 ビット整数を交換し、交換前の値を返します。
     *
     *  引数と戻り値は @ref cplat_atomic_exchange_i32 と同じです。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline uint32_t cplat_atomic_exchange_u32(cplat_atomic_u32 *atomic, const uint32_t desired,
                                                     const cplat_memory_order order)
    {
        return (uint32_t)cplat_atomic_exchange_i32((cplat_atomic_i32 *)(void *)atomic, (int32_t)desired, order);
    }

    /**
     *  @brief          符号なし 32 ビット整数の強い比較交換です。
     *
     *  引数と戻り値は @ref cplat_atomic_compare_exchange_i32 と同じです。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline int cplat_atomic_compare_exchange_u32(cplat_atomic_u32 *atomic, uint32_t *expected,
                                                        const uint32_t desired, const cplat_memory_order order)
    {
        int32_t signed_expected = (int32_t)*expected;
        const int is_exchanged = cplat_atomic_compare_exchange_i32((cplat_atomic_i32 *)(void *)atomic, &signed_expected,
                                                                   (int32_t)desired, order);

        *expected = (uint32_t)signed_expected;
        return is_exchanged;
    }

    /**
     *  @brief          符号なし 32 ビット整数に加算し、加算前の値を返します。2^32 を法として折り返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline uint32_t cplat_atomic_fetch_add_u32(cplat_atomic_u32 *atomic, const uint32_t operand,
                                                      const cplat_memory_order order)
    {
        return (uint32_t)cplat_atomic_fetch_add_i32((cplat_atomic_i32 *)(void *)atomic, (int32_t)operand, order);
    }

    /**
     *  @brief          符号なし 32 ビット整数から減算し、減算前の値を返します。2^32 を法として折り返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline uint32_t cplat_atomic_fetch_sub_u32(cplat_atomic_u32 *atomic, const uint32_t operand,
                                                      const cplat_memory_order order)
    {
        return (uint32_t)cplat_atomic_fetch_sub_i32((cplat_atomic_i32 *)(void *)atomic, (int32_t)operand, order);
    }

    /* ===== 64 ビット ===== */

    /**
     *  @brief          符号付き 64 ビット整数をアトミックに読みます。
     *
     *  引数と戻り値は @ref cplat_atomic_load_i32 と同じです。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline int64_t cplat_atomic_load_i64(const cplat_atomic_i64 *atomic, const cplat_memory_order order)
    {
#if defined(COMPILER_GCC)
        return CPLAT_ATOMIC_GCC_LOAD(int64_t, &atomic->value, order);
#elif defined(COMPILER_MSVC)
    const int64_t loaded = (int64_t)__iso_volatile_load64((const volatile __int64 *)&atomic->value);

    if (order != CPLAT_MEMORY_ORDER_RELAXED)
    {
        CPLAT_ATOMIC_COMPILER_BARRIER();
    }
    return loaded;
#endif /* COMPILER_ */
    }

    /**
     *  @brief          符号付き 64 ビット整数をアトミックに書き込みます。
     *
     *  引数は @ref cplat_atomic_store_i32 と同じです。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline void cplat_atomic_store_i64(cplat_atomic_i64 *atomic, const int64_t desired,
                                              const cplat_memory_order order)
    {
#if defined(COMPILER_GCC)
        CPLAT_ATOMIC_GCC_STORE(&atomic->value, desired, order);
#elif defined(COMPILER_MSVC)
    switch (order)
    {
    case CPLAT_MEMORY_ORDER_RELAXED:
        __iso_volatile_store64((volatile __int64 *)&atomic->value, desired);
        break;
    case CPLAT_MEMORY_ORDER_RELEASE:
        CPLAT_ATOMIC_COMPILER_BARRIER();
        __iso_volatile_store64((volatile __int64 *)&atomic->value, desired);
        break;
    case CPLAT_MEMORY_ORDER_ACQUIRE:
    case CPLAT_MEMORY_ORDER_ACQ_REL:
    case CPLAT_MEMORY_ORDER_SEQ_CST:
    default:
        (void)_InterlockedExchange64((volatile __int64 *)&atomic->value, desired);
        break;
    }
#endif /* COMPILER_ */
    }

    /**
     *  @brief          符号付き 64 ビット整数を交換し、交換前の値を返します。
     *
     *  引数と戻り値は @ref cplat_atomic_exchange_i32 と同じです。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline int64_t cplat_atomic_exchange_i64(cplat_atomic_i64 *atomic, const int64_t desired,
                                                    const cplat_memory_order order)
    {
#if defined(COMPILER_GCC)
        return CPLAT_ATOMIC_GCC_RMW(int64_t, __atomic_exchange_n, &atomic->value, desired, order);
#elif defined(COMPILER_MSVC)
    (void)order;
    return (int64_t)_InterlockedExchange64((volatile __int64 *)&atomic->value, desired);
#endif /* COMPILER_ */
    }

    /**
     *  @brief          符号付き 64 ビット整数の強い比較交換です。
     *
     *  引数と戻り値は @ref cplat_atomic_compare_exchange_i32 と同じです。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline int cplat_atomic_compare_exchange_i64(cplat_atomic_i64 *atomic, int64_t *expected,
                                                        const int64_t desired, const cplat_memory_order order)
    {
#if defined(COMPILER_GCC)
        return CPLAT_ATOMIC_GCC_CAS(&atomic->value, expected, desired, order);
#else
    const int64_t previous =
        (int64_t)_InterlockedCompareExchange64((volatile __int64 *)&atomic->value, desired, *expected);

    (void)order;
    if (previous == *expected)
    {
        return 1;
    }
    *expected = previous;
    return 0;
#endif /* COMPILER_ */
    }

    /**
     *  @brief          符号付き 64 ビット整数に加算し、加算前の値を返します。
     *
     *  引数、戻り値、折り返しは @ref cplat_atomic_fetch_add_i32 と同じです。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline int64_t cplat_atomic_fetch_add_i64(cplat_atomic_i64 *atomic, const int64_t operand,
                                                     const cplat_memory_order order)
    {
#if defined(COMPILER_GCC)
        return CPLAT_ATOMIC_GCC_RMW(int64_t, __atomic_fetch_add, &atomic->value, operand, order);
#elif defined(COMPILER_MSVC)
    (void)order;
    return (int64_t)_InterlockedExchangeAdd64((volatile __int64 *)&atomic->value, operand);
#endif /* COMPILER_ */
    }

    /**
     *  @brief          符号付き 64 ビット整数から減算し、減算前の値を返します。
     *
     *  引数、戻り値、折り返しは @ref cplat_atomic_fetch_add_i32 と同じです。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline int64_t cplat_atomic_fetch_sub_i64(cplat_atomic_i64 *atomic, const int64_t operand,
                                                     const cplat_memory_order order)
    {
#if defined(COMPILER_GCC)
        return CPLAT_ATOMIC_GCC_RMW(int64_t, __atomic_fetch_sub, &atomic->value, operand, order);
#else
    /* 2 の補数の否定は符号なしで計算し、INT64_MIN でも未定義動作にしない */
    return cplat_atomic_fetch_add_i64(atomic, (int64_t)(0U - (uint64_t)operand), order);
#endif /* COMPILER_ */
    }

    /**
     *  @brief          符号なし 64 ビット整数をアトミックに読みます。
     *
     *  引数と戻り値は @ref cplat_atomic_load_i32 と同じです。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline uint64_t cplat_atomic_load_u64(const cplat_atomic_u64 *atomic, const cplat_memory_order order)
    {
        return (uint64_t)cplat_atomic_load_i64((const cplat_atomic_i64 *)(const void *)atomic, order);
    }

    /**
     *  @brief          符号なし 64 ビット整数をアトミックに書き込みます。
     *
     *  引数は @ref cplat_atomic_store_i32 と同じです。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline void cplat_atomic_store_u64(cplat_atomic_u64 *atomic, const uint64_t desired,
                                              const cplat_memory_order order)
    {
        cplat_atomic_store_i64((cplat_atomic_i64 *)(void *)atomic, (int64_t)desired, order);
    }

    /**
     *  @brief          符号なし 64 ビット整数を交換し、交換前の値を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline uint64_t cplat_atomic_exchange_u64(cplat_atomic_u64 *atomic, const uint64_t desired,
                                                     const cplat_memory_order order)
    {
        return (uint64_t)cplat_atomic_exchange_i64((cplat_atomic_i64 *)(void *)atomic, (int64_t)desired, order);
    }

    /**
     *  @brief          符号なし 64 ビット整数の強い比較交換です。
     *
     *  引数と戻り値は @ref cplat_atomic_compare_exchange_i32 と同じです。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline int cplat_atomic_compare_exchange_u64(cplat_atomic_u64 *atomic, uint64_t *expected,
                                                        const uint64_t desired, const cplat_memory_order order)
    {
        int64_t signed_expected = (int64_t)*expected;
        const int is_exchanged = cplat_atomic_compare_exchange_i64((cplat_atomic_i64 *)(void *)atomic, &signed_expected,
                                                                   (int64_t)desired, order);

        *expected = (uint64_t)signed_expected;
        return is_exchanged;
    }

    /**
     *  @brief          符号なし 64 ビット整数に加算し、加算前の値を返します。2^64 を法として折り返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline uint64_t cplat_atomic_fetch_add_u64(cplat_atomic_u64 *atomic, const uint64_t operand,
                                                      const cplat_memory_order order)
    {
        return (uint64_t)cplat_atomic_fetch_add_i64((cplat_atomic_i64 *)(void *)atomic, (int64_t)operand, order);
    }

    /**
     *  @brief          符号なし 64 ビット整数から減算し、減算前の値を返します。2^64 を法として折り返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline uint64_t cplat_atomic_fetch_sub_u64(cplat_atomic_u64 *atomic, const uint64_t operand,
                                                      const cplat_memory_order order)
    {
        return (uint64_t)cplat_atomic_fetch_sub_i64((cplat_atomic_i64 *)(void *)atomic, (int64_t)operand, order);
    }

    /* ===== ポインター ===== */

    /**
     *  @brief          ポインターをアトミックに読みます。
     *
     *  引数は @ref cplat_atomic_load_i32 と同じです。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline void *cplat_atomic_load_ptr(const cplat_atomic_ptr *atomic, const cplat_memory_order order)
    {
#if defined(COMPILER_GCC)
        return CPLAT_ATOMIC_GCC_LOAD(void *, &atomic->value, order);
#elif defined(COMPILER_MSVC)
    return (void *)cplat_atomic_load_i64((const cplat_atomic_i64 *)(const void *)atomic, order);
#endif /* COMPILER_ */
    }

    /**
     *  @brief          ポインターをアトミックに書き込みます。
     *
     *  引数は @ref cplat_atomic_store_i32 と同じです。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline void cplat_atomic_store_ptr(cplat_atomic_ptr *atomic, void *desired, const cplat_memory_order order)
    {
#if defined(COMPILER_GCC)
        CPLAT_ATOMIC_GCC_STORE(&atomic->value, desired, order);
#elif defined(COMPILER_MSVC)
    cplat_atomic_store_i64((cplat_atomic_i64 *)(void *)atomic, (int64_t)(intptr_t)desired, order);
#endif /* COMPILER_ */
    }

    /**
     *  @brief          ポインターを交換し、交換前の値を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline void *cplat_atomic_exchange_ptr(cplat_atomic_ptr *atomic, void *desired,
                                                  const cplat_memory_order order)
    {
#if defined(COMPILER_GCC)
        return CPLAT_ATOMIC_GCC_RMW(void *, __atomic_exchange_n, &atomic->value, desired, order);
#elif defined(COMPILER_MSVC)
    (void)order;
    return _InterlockedExchangePointer((void *volatile *)&atomic->value, desired);
#endif /* COMPILER_ */
    }

    /**
     *  @brief          ポインターの強い比較交換です。
     *
     *  引数と戻り値は @ref cplat_atomic_compare_exchange_i32 と同じです。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline int cplat_atomic_compare_exchange_ptr(cplat_atomic_ptr *atomic, void **expected, void *desired,
                                                        const cplat_memory_order order)
    {
#if defined(COMPILER_GCC)
        return CPLAT_ATOMIC_GCC_CAS(&atomic->value, expected, desired, order);
#elif defined(COMPILER_MSVC)
    void *const previous = _InterlockedCompareExchangePointer((void *volatile *)&atomic->value, desired, *expected);

    (void)order;
    if (previous == *expected)
    {
        return 1;
    }
    *expected = previous;
    return 0;
#endif /* COMPILER_ */
    }

    /* ===== フェンス ===== */

    /**
     *  @brief          指定した順序のメモリ フェンスを置きます。
     *  @param[in]      order メモリ順序。RELAXED の場合は何もしません。
     *
     *  RELAXED のアトミック操作と組み合わせ、順序が必要な位置だけに順序を与える場合に使用します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    static inline void cplat_atomic_thread_fence(const cplat_memory_order order)
    {
#if defined(COMPILER_GCC)
        switch (order)
        {
        case CPLAT_MEMORY_ORDER_RELAXED:
            break;
        case CPLAT_MEMORY_ORDER_ACQUIRE:
            __atomic_thread_fence(__ATOMIC_ACQUIRE);
            break;
        case CPLAT_MEMORY_ORDER_RELEASE:
            __atomic_thread_fence(__ATOMIC_RELEASE);
            break;
        case CPLAT_MEMORY_ORDER_ACQ_REL:
            __atomic_thread_fence(__ATOMIC_ACQ_REL);
            break;
        case CPLAT_MEMORY_ORDER_SEQ_CST:
        default:
            __atomic_thread_fence(__ATOMIC_SEQ_CST);
            break;
        }
#else
    switch (order)
    {
    case CPLAT_MEMORY_ORDER_RELAXED:
        break;
    case CPLAT_MEMORY_ORDER_ACQUIRE:
    case CPLAT_MEMORY_ORDER_RELEASE:
    case CPLAT_MEMORY_ORDER_ACQ_REL:
        /* x64 では、acquire と release の順序はハードウェアが保証する。コンパイラの並べ替えだけを止める */
        CPLAT_ATOMIC_COMPILER_BARRIER();
        break;
    case CPLAT_MEMORY_ORDER_SEQ_CST:
    default:
    {
        /* 書き込みとその後の読み取りの並べ替えを止めるため、完全なメモリ バリアを伴う操作を行う */
        long guard = 0;

        (void)_InterlockedExchange(&guard, 0);
        break;
    }
    }
#endif /* COMPILER_ */
    }

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_SYNC_ATOMIC_H */
