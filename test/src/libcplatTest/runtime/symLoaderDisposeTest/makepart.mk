# テスト対象のソース ファイル
TEST_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/runtime/sym_loader_dispose.c

INCDIR += $(MYAPP_DIR)/prod/libsrc/cplat/runtime

# ライブラリの指定
# テストが利用側の列を cplat_calloc で用意し、dispose が cplat_free で解放する。
LIBS += mock_libc cplat
