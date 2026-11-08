# テスト対象のソース ファイル
TEST_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/runtime/sym_loader_info.c

# 表示前の解決。カバレッジ対象は info のみ。
ADD_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/runtime/sym_loader_resolve.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/runtime/sym_loader_init.c

INCDIR += $(MYAPP_DIR)/prod/libsrc/cplat/runtime

# ライブラリの指定
LIBS += mock_libc mock_cplat mock_cjson
