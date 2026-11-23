# テスト対象のソース ファイル
TEST_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/crt/sys_stat.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/crt/sys_stat_format.c

# 詳細エラーの記録先と結果コード変換。テスト側で内容を検証するため実装を取り込む
ADD_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/base/error.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/base/result.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/crt/path_format.c

# テスト対象ソースのモジュール私有ヘッダーを参照する
INCDIR += \
	$(MYAPP_DIR)/prod/libsrc/cplat/crt

# ライブラリの指定
LIBS += mock_libc mock_cplat
