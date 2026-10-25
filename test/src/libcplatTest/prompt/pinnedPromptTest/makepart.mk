# テスト対象のソース ファイル
TEST_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/prompt/pinned_prompt.c

# 結果コード変換。行編集の内部 API は ADD_SRCS。prompt_edit.c のカバレッジは promptEditTest が担う
ADD_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/prompt/prompt_edit.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/base/result.c

# ライブラリの指定
LIBS += mock_libc mock_cplat

INCDIR += $(MYAPP_DIR)/prod/libsrc/cplat/prompt
