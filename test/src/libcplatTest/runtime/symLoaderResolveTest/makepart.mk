# テスト対象のソース ファイル
TEST_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/runtime/sym_loader_resolve.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/runtime/sym_loader_dispose.c

# ensure が参照する設定適用と、失敗・明示的既定値の確認。
ADD_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/runtime/sym_loader_init.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/runtime/sym_loader_is_default.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/runtime/sym_loader_is_failed.c

INCDIR += $(MYAPP_DIR)/prod/libsrc/cplat/runtime

# ライブラリの指定
# ロック生成と文字列連結は mock_cplat 経由で実実装へ委譲する。
# 設定の再読込が参照する JSON 解析は mock_cjson へ委譲する。
LIBS += mock_libc mock_cplat mock_cjson
