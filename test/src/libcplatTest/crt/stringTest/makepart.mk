# テスト対象のソース ファイル
TEST_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/crt/string.c

# ライブラリの指定
LIBS += mock_libc
