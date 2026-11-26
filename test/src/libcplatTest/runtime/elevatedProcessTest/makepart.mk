# テスト対象のソース ファイル
TEST_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/runtime/elevated_process.c

# 結果コード変換。テスト側で戻り値を検証するため実装を取り込む
# 一時ディレクトリの取得と一時ファイルの作成は実装を取り込み、実ディレクトリで作成と削除を確かめる
ADD_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/base/error.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/base/result.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/crt/temp_dir_windows.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/crt/temp_file_windows.c

# ライブラリの指定
LIBS += mock_libc mock_cplat
