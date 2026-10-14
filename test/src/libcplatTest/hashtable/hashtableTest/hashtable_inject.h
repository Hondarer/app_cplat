#ifndef HASHTABLE_INJECT_H
#define HASHTABLE_INJECT_H

#include <cplat/hashtable/hashtable.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    extern uint64_t *test_hashtable_bucket_head(const cplat_hashtable *ht);
    extern unsigned char *test_hashtable_entry_status(const cplat_hashtable *ht, size_t rec);
    extern uint64_t *test_hashtable_entry_next(const cplat_hashtable *ht, size_t rec);
    extern char *test_hashtable_entry_key(const cplat_hashtable *ht, size_t rec);
    extern uint32_t *test_hashtable_entry_key_length(const cplat_hashtable *ht, size_t rec);
    extern void *test_hashtable_key_ref_at(const cplat_hashtable *ht, size_t rec);
    extern void *test_hashtable_value_ref_at(const cplat_hashtable *ht, size_t rec);
    extern uint64_t *test_hashtable_entry_generation(const cplat_hashtable *ht, size_t rec);
    extern void test_hashtable_set_next_empty(cplat_hashtable *ht, uint64_t value);
    extern void test_hashtable_set_config(cplat_hashtable *ht, const cplat_hashtable_config *config);
    extern void test_hashtable_set_counts(cplat_hashtable *ht, uint64_t in_use_count, uint64_t deleted_count);
    extern void *test_hashtable_value_free_list(const cplat_hashtable *ht);
    extern uint64_t test_hashtable_value_free_count(const cplat_hashtable *ht);
    extern void test_hashtable_set_value_free_count(cplat_hashtable *ht, uint64_t count);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* HASHTABLE_INJECT_H */
