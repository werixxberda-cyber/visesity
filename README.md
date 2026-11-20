# Vice Vox — вокальная цепочка для FL Studio (VST3)

Кнопки:
- UNDERGROUND — все стадии 2–7: Auto-Tune, EQ, компрессор, де-эссер, сатурация, реверб + дилэй (sends)
- FLINT — только 3–6: EQ, компрессор, де-эссер, сатурация
- повторный клик по активной кнопке — плагин выключается (bypass)
- ручки: REVERB / DELAY (уровень отправки), OUTPUT (громкость)

## Как получить .vst3 без установки компилятора
1. Создай репозиторий на github.com и залей туда ВСЕ файлы из этой папки (включая скрытую папку .github).
2. Вкладка Actions -> "Build VST3" -> дождись зелёной галочки (5–10 мин).
3. Внутри запуска скачай артефакт ViceVox-VST3 (zip), внутри папка `Vice Vox.vst3`.
4. Скопируй `Vice Vox.vst3` в C:\Program Files\Common Files\VST3
5. FL Studio: Options -> Manage plugins -> Find installed plugins -> Vice Vox.

## Или собрать на ПК
Нужны Visual Studio 2022 (C++) и CMake:
    cmake -B build
    cmake --build build --config Release --target ViceVox_VST3

VST2 не собирается: Steinberg закрыл VST2 SDK. FL Studio 25 спокойно работает с VST3.
