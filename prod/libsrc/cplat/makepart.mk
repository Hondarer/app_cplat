ifdef PLATFORM_WINDOWS
    # DLL エクスポート定義
    # DLL export definition
    CFLAGS   += /DCPLAT_EXPORTS
    CXXFLAGS += /DCPLAT_EXPORTS
endif

# 生成するライブラリは動的ライブラリのみとする。
# Linux の shared/dlopen 対応を維持するため、-ftls-model は指定しない。
LIB_TYPE = shared

ifdef PLATFORM_LINUX
    # 実行ファイルと同じディレクトリへ同梱した libcjson_app.so、libzlib_app.so、libcrypto_app.so.3 を解決する。
    LDFLAGS += -Wl,-z,origin -Wl,-rpath,'$$ORIGIN'
    # regex (C++) が実体化する libstdc++ シンボルを
    # 動的シンボル テーブルへ出力しない。公開 C API (cplat_* / _cplat_*) のみをエクスポートする。
    LDFLAGS += -Wl,--version-script="$(CURDIR)/exports.map"
endif

# ライブラリの指定
#
# cjson は JSON 設定解析、zlib は両 OS の圧縮・展開で動的に利用する。
# crypto_app は Linux の暗号処理と乱数生成で使う app/openssl の共有ライブラリ。
# シンボル バージョン OPENSSL_APP_ によりシステムの OpenSSL と区別する。
# stdc++ は regex モジュールが C++ (std::basic_regex) で実装されているために必要となる。
# 共有ライブラリのリンクは $(CC) -shared (gcc) で行われ、g++ ドライバーを経由しない
# ため、libstdc++ は自動ではリンクされない。
# see: framework/makefw/makefiles/makelibsrc_c_cpp.mk
# see: app/cplat/docs/link-policy.md
LIBS += cjson_app zlib_app
ifdef PLATFORM_LINUX
    LIBS += crypto_app dl stdc++
endif
