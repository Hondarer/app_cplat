# テスト対象のソース ファイル
TEST_SRCS := \
    $(MYAPP_DIR)/prod/libsrc/cplat/clock/timespec.c

# テスト対象ソースのモジュール私有ヘッダーを参照する
INCDIR += \
    $(MYAPP_DIR)/prod/libsrc/cplat/clock
