# テスト対象のソース ファイル
TEST_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/prompt/prompt.c

# 行編集の内部 API は ADD_SRCS。prompt_edit.c のカバレッジは promptEditTest が担う
LIBS += mock_libc mock_cplat

# 共有編集処理はカバレッジ対象から外し、実装の契約を利用する
ADD_SRCS := $(MYAPP_DIR)/prod/libsrc/cplat/prompt/prompt_edit.c
