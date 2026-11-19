# テスト対象のソース ファイル
TEST_SRCS := \
	$(MYAPP_DIR)/prod/src/cmd/bench-io/bench-io.c

# テスト対象が依存するソース ファイル
# 測定の処理と表の出力は実体で動かし、bench-io.c の終了の直前の検査だけに失敗を注入する。
# 各ソースのカバレッジは benchIoFailureTest と benchTimerTest が担う
ADD_SRCS := \
	$(MYAPP_DIR)/prod/src/cmd/bench-io/bench_mmap.c \
	$(MYAPP_DIR)/prod/src/cmd/bench-io/bench_report.c \
	$(MYAPP_DIR)/prod/src/cmd/bench-io/bench_stdio.c \
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
