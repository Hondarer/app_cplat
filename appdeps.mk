# 利用側の定義: JSON 設定解析に cJSON、圧縮・展開に zlib、Linux の暗号処理と乱数生成に OpenSSL を利用する。
# OpenSSL は app/openssl の静的ライブラリを使う。Windows は CNG を使うが、ビルド順序をそろえるため両 OS で依存する。
APP_DEPS := cjson zlib openssl
