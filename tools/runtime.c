#include <stdint.h>

/* Entry point of the recompiled title code (vaddr 0x82088ab8). */
extern void L_82088ab8(void);

__attribute__((visibility("default")))
int rs_native_entry(uint64_t a, uint64_t b) {
    (void)a;
    (void)b;
    L_82088ab8();
    return 0;
}