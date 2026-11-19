# テスト対象のソース ファイル
TEST_SRCS := \
	$(MYAPP_DIR)/prod/src/cmd/eventlog-register/eventlog-register.c

# エントリ ポイントの変更
# テスト対象のソース ファイルにある main() は直接実行されず、
# テスト コード内から __real_main() 経由で実行される
USE_WRAP_MAIN := 1

# テスト対象ソースのローカル ヘッダーを参照する
INCDIR += \
	$(MYAPP_DIR)/prod/src/cmd/eventlog-register

# ライブラリの指定
LIBS += mock_cplat mock_libc
