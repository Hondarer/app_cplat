# テスト対象のソース ファイル
# プラットフォーム別実装のため 2 本を指定する。実際にコンパイルされるのは実行環境側だけ
TEST_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/crypto/random_linux.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/crypto/random_windows.c

# 結果コード変換。テスト側で戻り値を検証するため実装を取り込む
ADD_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/base/result.c

# ライブラリの指定
LIBS += mock_libc
ifdef PLATFORM_LINUX
    # RAND_bytes の失敗を注入するため、app/openssl が提供する mock_openssl を使う。
    # 実関数へは mock_openssl が共有ライブラリ経由で委譲する。
    LIBS += mock_openssl
endif
