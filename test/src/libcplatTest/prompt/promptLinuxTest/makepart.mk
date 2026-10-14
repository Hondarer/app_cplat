# テスト対象のソース ファイル
# Linux 専用実装のため、Windows では中身が空になる
TEST_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/prompt/prompt_linux.c

# ライブラリの指定
# read の失敗と EINTR を注入するため mock_libc を使用する
# CPLAT_FD_SET() が展開する cplat_abort_at() は mock_cplat から実 libcplat へ委譲する
LIBS += mock_libc mock_cplat
