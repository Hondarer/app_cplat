# テスト対象のソース ファイル
TEST_SRCS := \
	$(MYAPP_DIR)/prod/src/cmd/pinned-prompt/pinned-prompt.c

# エントリ ポイントの変更
# テスト対象のソース ファイルにある main() は直接実行されず、
# テスト コード内から __real_main() 経由で実行される
USE_WRAP_MAIN := 1

# ライブラリの指定
LIBS += mock_cplat mock_libc
