# 変換の実装を取り込み、Win32 API と確保を mock して各失敗経路を検証します。
TEST_SRCS := $(MYAPP_DIR)/prod/libsrc/cplat/crt/wchar_conv.c

ADD_SRCS := $(MYAPP_DIR)/prod/libsrc/cplat/base/result.c

LIBS += mock_libc mock_cplat
