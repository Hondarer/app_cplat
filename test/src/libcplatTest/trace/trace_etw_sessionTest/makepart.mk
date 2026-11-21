# テスト対象のソース ファイル
TEST_SRCS := \
    $(MYAPP_DIR)/prod/libsrc/cplat/trace/backends/etw/trace_etw_session.c

# ライブラリの指定
# trace_etw_session.c が cplat_malloc/cplat_free やスレッド API を呼ぶため、mock_cplat が必要
LIBS += mock_libc mock_cplat
