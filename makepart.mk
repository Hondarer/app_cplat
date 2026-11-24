# glibc の GNU 拡張 (dladdr、Dl_info、mkostemp など) を、Linux の全翻訳単位で、最初の標準ヘッダーより前から有効にする。
# glibc は、最初に取り込んだ標準ヘッダーの時点で機能検査マクロを確定するため、ソースやヘッダーの中の
# #define では、先に <assert.h> などが取り込まれた翻訳単位で効かない。
# see: https://man7.org/linux/man-pages/man7/feature_test_macros.7.html
ifdef PLATFORM_LINUX
    DEFINES += _GNU_SOURCE
endif
