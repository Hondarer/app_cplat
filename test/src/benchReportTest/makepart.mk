# Windows では /proc/cpuinfo を読むテストをビルドしない
SKIP_BUILD = WINDOWS

# テスト対象のソース ファイル
TEST_SRCS := $(MYAPP_DIR)/prod/src/cmd/bench-io/bench_report.c

# テスト対象ソースのローカル ヘッダーを参照する
INCDIR += $(MYAPP_DIR)/prod/src/cmd/bench-io

# ライブラリの指定
LIBS += mock_cplat mock_libc
