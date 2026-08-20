# real-steel-recomp

Статичний рекомпілятор для **Xbox 360** версії гри **Real Steel**
(PowerPC32 -> ARM64), задача — висока продуктивність на Android (aarch64).

## Чому Xbox 360

- Референс-віртуальна машина — Xenia (відкрита, підтримує Xbox 360).
- Guest-код — PowerPC32 (XEX2 `default.xex`), на відміну від PS3 (PPU64/ELF).
  Це інший формат і архітектура, тому під обидві консолі потрібні різні
  пайплайни — вибрали Xbox.
- Готовий бенчмарк для швидкості: Xenia-версія гри і наш нативний ARM64-код.

## Підхід

Замість емуляції кожної інструкції (як Xenia) — статично переписуємо готів
машинний код гри в нативний ARM64, лишаючи тільки тонкий runtime
(пам'ять, виклики XAM/ядро, графіка). Це знімає emulation overhead і дає
високий FPS на ARM-чипах.

Етапи в `docs/plan.md`. Код гри (XEX) не включено в репозиторій — тільки
інструменти; XEX постачає користувач.

## Статус

- [x] Завантажувач XEX2 (big-endian), заголовки + секції + entry:
      `src/xex.{h,cpp}`
- [x] Декодер PPC32: інтеджери/АЛУ з carry, всі load/store (D/DS/X-форми,
      byte-reverse, multi/string), гілки (b/bl/bc,bdnz/bdz/bclr/bctr за
      повною BO-таблицею), FPU (fadd..fnmsub, fcmpu, lfs/lfd/stfs/stfd),
      paired-single (Xenon), RLWINM/RLWIMI/RLWNM (ror+mask/bfi), mflr/mtlr/ctr,
      cmp, CR-special, dcbz/dcbst/dcbf/eieio, tw/sc:
      `src/ppc32_decode.{h,cpp}`
- [x] Емітер ARM64 (текст) з проміжним мапінгом: `src/arm64_emit.{h,cpp}`
- [x] Функційні мітки по `bl`-таргетах + статистика:
      `src/main.cpp`
- [x] CLI: `real-steel-recomp default.xex -o out.s`
- [ ] CR-емulation точна (флаги в shadow-регістр; зараз best-effort from cmp)
- [ ] Регістровий алокатор + ABI (Xenon ABI: r1=SP, r3..r10 аргументи)
- [ ] Заглушки XAM/ядра (syscall), імпорти
- [ ] Графіка Xenos (D3D-подібний) -> Vulkan/GLES
- [ ] Завантаження ігрових даних (dvd:/, content/)
- [ ] Запуск + валідація проти Xenia
- [ ] Perf-прохід: LTO, hot-path syscall ring, JIT для непрямих викликів

## Build

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Usage

```
# Користувач надає default.xex зі своєї Astrophysics-копії гри
./build/real-steel-recomp default.xex -o out.s
```