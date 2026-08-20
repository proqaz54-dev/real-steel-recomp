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

## Статус (фундамент)

- [x] Завантажувач XEX2 (big-endian), заголовки + секції + entry:
      `src/xex.{h,cpp}`
- [x] Мінімальний декодер PPC32 (integer/load-store/branch subset):
      `src/ppc32_decode.{h,cpp}`
- [x] Емітер ARM64 (текст): `src/arm64_emit.{h,cpp}`
- [x] CLI: `real-steel-recomp default.xex -o out.s`
- [ ] Повне покриття PPC32 (включно FPU/Xenos), XEX1
- [ ] Аналіз контролю потоку + меж функцій (з `bl`-таргетів)
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