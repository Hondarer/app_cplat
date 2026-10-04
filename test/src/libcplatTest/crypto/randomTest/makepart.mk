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
    # RAND_bytes の失敗を注入するため mock_openssl を使う
    LIBS += mock_openssl crypto
endif
