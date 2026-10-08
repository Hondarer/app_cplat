ifdef PLATFORM_LINUX
    # mock_cplat::Mock_cplat() コンストラクターが多数の ON_CALL を持つため、GCC の
    # -fvar-tracking-assignments が内部サイズ制限を超え、次のメッセージが出力されることがある。
    #
    # mock_cplat.cc:6:1: 備考: 変数追跡サイズ制限が -fvar-tracking-assignments を超過しています。
    # -fvar-tracking-assignments 無しで再度試みています
    #
    # var-tracking-assignments-dom-ops-limit に対応している GCC では、上限値を引き上げて備考を抑制する。
    # gcc 8 系のように未対応の環境では、-fno-var-tracking-assignments に切り替えて回避する。
    GCC_VAR_TRACKING_ASSIGNMENTS_DOM_OPS_LIMIT_SUPPORTED := $(shell $(CXX) -Q --help=params 2>/dev/null | grep -q '^[[:space:]]*var-tracking-assignments-dom-ops-limit[[:space:]]' && echo 1)
    ifneq ($(GCC_VAR_TRACKING_ASSIGNMENTS_DOM_OPS_LIMIT_SUPPORTED),)
        CXXFLAGS += --param=var-tracking-assignments-dom-ops-limit=400000
    else
        CXXFLAGS += -fno-var-tracking-assignments
    endif
endif

ifdef PLATFORM_WINDOWS
    # mock_cplat::Mock_cplat() コンストラクターが多数の ON_CALL を持つため、COFF オブジェクトの
    # セクション数上限 (65535) を超え、次のエラーが発生することがある。
    #
    # fatal error C1128: セクションの数がオブジェクト ファイル形式の制限を超えています:
    #                    /bigobj と共にコンパイルしてください
    #
    # /bigobj により、セクション数の上限を 65535 から約 4G に引き上げて回避する。
    CXXFLAGS += /bigobj
endif
