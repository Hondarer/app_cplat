// promptTest 専用のプラットフォーム層 fake
// prompt.c が呼ぶ prompt_platform_* を差し替え、実機の端末なしにキー入力を再現します
#ifndef PROMPT_PLATFORM_FAKE_H
#define PROMPT_PLATFORM_FAKE_H

#include <string>
#include <vector>

/* 後続の cplat_internal_prompt_platform_read_char / _nb が返すバイト列を設定する。
   列を消費し切った後は EOF (-1) を返す。 */
void promptFakeSetInput(const std::string &bytes);

/* 後続の cplat_internal_prompt_platform_read_char / _nb が返す整数列を設定する。
   -1 は EOF、-2 は端末サイズ変更通知として使用できる。 */
void promptFakeSetResults(const std::vector<int> &results);

/* cplat_internal_prompt_platform_enter_raw / leave_raw の呼び出し回数を取得する。 */
int promptFakeEnterRawCount(void);
int promptFakeLeaveRawCount(void);

/* 呼び出し回数と入力列を初期化する。 */
void promptFakeReset(void);

/* 再描画で返す端末幅を指定する。 */
void promptFakeSetColumns(size_t columns);

#endif /* PROMPT_PLATFORM_FAKE_H */
