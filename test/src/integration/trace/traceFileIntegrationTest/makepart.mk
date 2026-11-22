
# 統合テストはカバレッジの充足を目的としないため TEST_SRCS を宣言しない
# 各ソースのカバレッジは対応する単体テストが担う
# see: framework/testfw/docs/how-to-test.md の「統合テストは TEST_SRCS を宣言しない」
ADD_SRCS := \
    $(MYAPP_DIR)/prod/libsrc/cplat/trace/tracer.c \
    $(MYAPP_DIR)/prod/libsrc/cplat/runtime/process.c \
    $(MYAPP_DIR)/prod/libsrc/cplat/clock/clock.c \
    $(MYAPP_DIR)/prod/libsrc/cplat/crt/time.c \
    $(MYAPP_DIR)/prod/libsrc/cplat/crt/file.c \
    $(MYAPP_DIR)/prod/libsrc/cplat/crt/stdio.c \
    $(MYAPP_DIR)/prod/libsrc/cplat/trace/backends/file/trace_file.c \
    $(MYAPP_DIR)/prod/libsrc/cplat/trace/backends/syslog/trace_syslog.c \
    $(MYAPP_DIR)/prod/libsrc/cplat/trace/backends/etw/trace_etw.c \
    $(MYAPP_DIR)/prod/libsrc/cplat/trace/trace_common.c \
    $(MYAPP_DIR)/prod/libsrc/cplat/base/error.c \
    $(MYAPP_DIR)/prod/libsrc/cplat/base/result.c \
    $(MYAPP_DIR)/test/libsrc/mock_cplat/trace/mock_cplat_eventlog_sink_dispose_on_unload.cc

# 引き込んだ crt/file.c と clock/clock.c のモジュール私有ヘッダーを参照する
INCDIR += \
    $(MYAPP_DIR)/prod/libsrc/cplat/clock \
    $(MYAPP_DIR)/prod/libsrc/cplat/crt

# ライブラリの指定
LIBS += mock_libc cplat
