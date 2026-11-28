# テスト対象のソース ファイル
TEST_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/service/service.c

# ライブラリの指定
LIBS += mock_libc mock_cplat
