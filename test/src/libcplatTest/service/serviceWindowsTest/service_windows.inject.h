#ifndef SERVICE_WINDOWS_INJECT_H
#define SERVICE_WINDOWS_INJECT_H

#ifdef __cplusplus
extern "C"
{
#endif
    /* サービス ステータス ハンドルを設定する (NULL を渡すと SCM 未接続の状態へ戻す)。 */
    void test_service_windows_set_status_handle(void *handle);

    /* テストで使うサービス ステータス ロックを設定する。 */
    void test_service_windows_set_status_lock(void *lock);
#ifdef __cplusplus
}
#endif

#endif /* SERVICE_WINDOWS_INJECT_H */
