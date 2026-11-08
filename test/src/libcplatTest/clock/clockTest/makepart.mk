# テスト対象のソース ファイル
TEST_SRCS := \
    $(MYAPP_DIR)/prod/libsrc/cplat/clock/clock.c

# ライブラリの指定
# timespec 演算は mock_cplat から実ライブラリーへ委譲する
LIBS += mock_libc mock_cplat

# 時刻取得の OS 失敗を共通結果へ変換する依存実装
ADD_SRCS += $(MYAPP_DIR)/prod/libsrc/cplat/base/error.c $(MYAPP_DIR)/prod/libsrc/cplat/base/result.c
