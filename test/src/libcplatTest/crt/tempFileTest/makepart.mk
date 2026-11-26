# テスト対象のソース ファイル
TEST_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/crt/temp_file_windows.c

# 詳細エラーの記録先と結果コード変換。テスト側で内容を検証するため実装を取り込む
# 一時ディレクトリの取得は、テスト用の差し替えで任意のパスを返すため実装を取り込む
ADD_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/base/error.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/base/result.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/crt/temp_dir_windows.c

# ライブラリの指定
LIBS += mock_libc mock_cplat
