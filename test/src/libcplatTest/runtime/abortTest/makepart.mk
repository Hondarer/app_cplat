# テスト対象のソース ファイル
TEST_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/runtime/abort.c

# ライブラリの指定
LIBS += mock_libc mock_cplat
