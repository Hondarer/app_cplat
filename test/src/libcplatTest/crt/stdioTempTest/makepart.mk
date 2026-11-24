# テスト対象のソース ファイル
TEST_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/crt/stdio_temp.c

# テスト対象が依存するソース ファイル
ADD_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/base/error.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/base/result.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/crt/temp_dir_windows.c

# テスト対象ソースのモジュール私有ヘッダーを参照する
INCDIR += \
	$(MYAPP_DIR)/prod/libsrc/cplat/crt

# ライブラリの指定
LIBS += mock_libc mock_cplat
