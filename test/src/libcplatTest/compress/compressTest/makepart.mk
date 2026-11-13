# 両 OS 共通の圧縮実装を直接取り込む。
TEST_SRCS := $(MYAPP_DIR)/prod/libsrc/cplat/compress/compress.c

# 長さヘッダーのバイトオーダー変換を直接取り込む。
ADD_SRCS := $(MYAPP_DIR)/prod/libsrc/cplat/net/byteorder.c

# zlib の実体へは mock_zlib が動的に委譲する。
LIBS += mock_libc mock_zlib
DEFINES += MOCK_ZLIB_ENABLE_CALLER
