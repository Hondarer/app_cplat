# テスト対象のソース ファイル
TEST_SRCS := $(MYAPP_DIR)/prod/src/cmd/bench-tracer/bench-tracer.c

# テストから __real_main() を呼ぶ
USE_WRAP_MAIN := 1

# ライブラリの指定
LIBS += mock_cplat mock_libc
