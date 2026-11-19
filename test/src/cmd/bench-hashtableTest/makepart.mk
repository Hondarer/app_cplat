# テスト対象のソース ファイル
TEST_SRCS := \
	$(MYAPP_DIR)/prod/src/cmd/bench-hashtable/bench-hashtable.c

# 反復回数の自動調整は bench-io と同じ実装を共有する。bench_timer.c のカバレッジは benchTimerTest が担う
ADD_SRCS := \
	$(MYAPP_DIR)/prod/src/cmd/bench-io/bench_timer.c

# エントリ ポイントの変更
# テスト対象のソース ファイルにある main() は直接実行されず、
# テスト コード内から __real_main() 経由で実行される
USE_WRAP_MAIN := 1

# テスト対象ソースのローカル ヘッダーを参照する
INCDIR += \
	$(MYAPP_DIR)/prod/src/cmd/bench-io

# ライブラリの指定
LIBS += mock_cplat mock_libc
