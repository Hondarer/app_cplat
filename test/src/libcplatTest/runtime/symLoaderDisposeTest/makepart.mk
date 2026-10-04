# テスト対象のソース ファイル
TEST_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/runtime/sym_loader_dispose.c

# ライブラリの指定
LIBS += mock_libc
