# テスト対象のソース ファイル
TEST_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/runtime/stack_limit_linux.c

# 詳細エラーの記録と結果コード変換。内部 API のため実装を取り込む
ADD_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/base/error.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/base/result.c

# ライブラリの指定
LIBS += mock_libc mock_cplat
