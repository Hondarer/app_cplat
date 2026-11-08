TEST_SRCS := $(MYAPP_DIR)/prod/src/cmd/bench-io/bench_stdio.c \
    $(MYAPP_DIR)/prod/src/cmd/bench-io/bench_mmap.c
INCDIR += $(MYAPP_DIR)/prod/src/cmd/bench-io
LIBS += mock_cplat mock_libc
