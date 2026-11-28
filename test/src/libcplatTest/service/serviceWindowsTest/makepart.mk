# LINUX ではビルドをスキップ (Windows SCM 専用テスト)
SKIP_BUILD = LINUX

# テスト対象のソース ファイル
TEST_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/service/service_windows.c

# 共通層と内部エラー報告の実装を取り込む
ADD_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/service/service.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/base/error.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/base/result.c

# ライブラリの指定
LIBS += mock_libc mock_cplat
