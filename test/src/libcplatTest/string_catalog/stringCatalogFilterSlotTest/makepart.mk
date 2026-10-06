# このテストのディレクトリです。
_FILTER_TEST_DIR := $(MYAPP_DIR)/test/src/libcplatTest/string_catalog/stringCatalogFilterSlotTest

# テスト用カタログの定義 (filter_test_trace.jsonc、filter_test_mixed.jsonc) から gen/ 配下の生成物を書き出す。
# framework はソースの実在を makefile のパース時に検査するため、ビルド規則ではなくパース時に生成する。
# string-catalog-sample の app 直下 makepart.mk と同じ方式。
# --if-newer により、定義ファイル、設定ファイル、生成器のいずれも更新されていなければ何もしない。
ifndef MAKEFW_SYNC_EVAL
    _FILTER_TEST_STATUS := $(shell python3 "$(MYAPP_DIR)/bin_internal/string_catalog_gen.py" \
        "$(_FILTER_TEST_DIR)/filter_test_trace.jsonc" --out-dir "$(_FILTER_TEST_DIR)/gen" --if-newer >&2; echo $$?)
    ifneq ($(_FILTER_TEST_STATUS),0)
        $(error カタログ定義からの生成に失敗しました。上記のメッセージを確認してください)
    endif
    _FILTER_TEST_MIXED_STATUS := $(shell python3 "$(MYAPP_DIR)/bin_internal/string_catalog_gen.py" \
        "$(_FILTER_TEST_DIR)/filter_test_mixed.jsonc" --out-dir "$(_FILTER_TEST_DIR)/gen" --if-newer >&2; echo $$?)
    ifneq ($(_FILTER_TEST_MIXED_STATUS),0)
        $(error カタログ定義からの生成に失敗しました。上記のメッセージを確認してください)
    endif
endif

# テスト対象のソース ファイル
# 判定と差し替えを担うスロット、条件の説明文、ソース領域の公開と読み取りを対象とする
TEST_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/string_catalog/filter/filter_slot.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/string_catalog/filter/filter_describe.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/string_catalog/filter/filter_source.c

# テスト対象がリンクのために必要とする依存実装
# スロットはコンパイル、検証と、文字列カタログの書式展開を呼び出す。
# テスト用カタログの生成物 (gen/filter_test_trace.c、gen/filter_test_mixed.c) も引き込む
ADD_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/string_catalog/filter/filter_compile.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/string_catalog/filter/filter_image.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/string_catalog/string_catalog_argument.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/string_catalog/string_catalog_catalog.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/string_catalog/string_catalog_format.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/string_catalog/string_catalog_language.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/string_catalog/string_catalog_render.c \
	$(_FILTER_TEST_DIR)/gen/filter_test_trace.c \
	$(_FILTER_TEST_DIR)/gen/filter_test_mixed.c

# モジュール私有ヘッダー filter.h と string_catalog.h、条件式フィルターのテストが共有する filterTestSupport.h、
# 生成物のヘッダー (gen/filter_test_trace.h) と、生成物が山かっこ形式で取り込む filter_test_context.h の探索パス。
# テスト ディレクトリへ引き込んだソースからは、元ディレクトリを基準に解決できないため指定する
INCDIR += \
	$(MYAPP_DIR)/prod/libsrc/cplat/string_catalog/filter \
	$(MYAPP_DIR)/prod/libsrc/cplat/string_catalog \
	$(MYAPP_DIR)/test/src/libcplatTest/string_catalog \
	$(_FILTER_TEST_DIR) \
	$(_FILTER_TEST_DIR)/gen

# スロットが呼び出す同期関数の失敗を注入するため、mock_cplat が必要
# テスト対象が呼び出す標準ライブラリ関数は、include_override によって mock_libc へ差し替わる
LIBS += mock_libc mock_cplat
