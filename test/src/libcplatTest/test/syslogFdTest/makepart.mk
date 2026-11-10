# テスト対象のソース ファイル
TEST_SRCS := $(MYAPP_DIR)/prod/libsrc/cplat/test/syslog_test.c

# ライブラリの指定
LIBS += mock_cplat mock_libc
