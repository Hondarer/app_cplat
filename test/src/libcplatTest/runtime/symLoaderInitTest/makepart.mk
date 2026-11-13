# テスト対象のソース ファイル
TEST_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/runtime/sym_loader_init.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/runtime/sym_loader_dispose.c

INCDIR += $(MYAPP_DIR)/prod/libsrc/cplat/runtime

# ライブラリの指定
# fopen / fread 等は mock_cplat、JSON 解析は mock_cjson 経由で実実装へ委譲する。
LIBS += mock_libc mock_cplat mock_cjson
DEFINES += MOCK_CJSON_ENABLE_CALLER
