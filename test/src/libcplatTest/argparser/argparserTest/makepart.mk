# テスト対象のソース ファイル
TEST_SRCS := \
    $(MYAPP_DIR)/prod/libsrc/cplat/argparser/argparser.c

# UTF-8 境界切り詰めの内部 API。utf8.c のカバレッジは utf8Test が担う
ADD_SRCS := \
    $(MYAPP_DIR)/prod/libsrc/cplat/crt/utf8.c

# ライブラリの指定
LIBS += mock_libc mock_cplat
