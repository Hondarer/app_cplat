# テスト対象のソース ファイル
TEST_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/runtime/sym_loader_is_default.c

# 非公開の解決処理。カバレッジ対象は is_default のみ。
ADD_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/runtime/sym_loader_resolve.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/runtime/sym_loader_init.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/runtime/sym_loader_is_failed.c

INCDIR += $(MYAPP_DIR)/prod/libsrc/cplat/runtime

# ライブラリの指定
LIBS += mock_libc mock_cplat mock_cjson
