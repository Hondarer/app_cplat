# テスト対象のソース ファイル
TEST_SRCS := $(MYAPP_DIR)/prod/libsrc/cplat/win32/service_api.c

ADD_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/base/result.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/crt/wchar_conv.c

# 実サービスへ接続せず、W 版へ渡す構造体を検査する
ifeq ($(OS),Windows_NT)
CFLAGS += /FIservice_api_fake.h
CXXFLAGS += /FIservice_api_fake.h
endif

LIBS += mock_libc mock_cplat
