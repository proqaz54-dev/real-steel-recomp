typedef unsigned long long u64;

/* Entry point of the recompiled title code (vaddr 0x82088ab8). */
extern void L_82088ab8(void);

__attribute__((visibility("default")))
int rs_native_entry(uint64_t a, uint64_t b) {
    (void)a;
    (void)b;
    L_82088ab8();
    return 0;
}

/* Sentinel real-entry: proves the XEX export thunk dispatches here. */
__attribute__((visibility("default")))
void rs_real_entry_reached(void) {
    /* deliberate fault at a known PC so the harness can detect dispatch */
    *(volatile unsigned*)0 = 0x12345678u;
}