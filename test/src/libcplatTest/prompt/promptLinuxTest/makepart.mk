# テスト対象のソース ファイル
# Linux 専用実装のため、Windows では中身が空になる
TEST_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/prompt/prompt_linux.c

# ライブラリの指定
# read の失敗と EINTR を注入するため mock_libc を使用する
LIBS += mock_libc
