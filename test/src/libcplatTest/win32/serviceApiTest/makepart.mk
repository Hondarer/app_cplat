# テスト対象のソース ファイル
TEST_SRCS := $(MYAPP_DIR)/prod/libsrc/cplat/win32/service_api.c

# 実サービスへ接続せず、W 版へ渡す構造体を検査する
ifeq ($(OS),Windows_NT)
CFLAGS += /FIservice_api_fake.h
CXXFLAGS += /FIservice_api_fake.h
endif

LIBS += mock_cplat
