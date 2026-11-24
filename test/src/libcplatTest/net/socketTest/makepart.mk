# テスト対象のソース ファイル
# プラットフォーム別実装のため 2 本を指定する。実際にコンパイルされるのは実行環境側だけ
TEST_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/net/socket_linux.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/net/socket_windows.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/net/socket_watch_linux.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/net/socket_watch_windows.c

# 詳細エラーの記録先と結果コード変換。テスト側で内容を検証するため実装を取り込む
ADD_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/base/error.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/base/result.c

# ライブラリの指定
LIBS += mock_libc mock_cplat
