# REvenge Installer — Инструкция по сборке

## Структура проекта

```
REvenge_Installer/
├── main.cpp                    — весь исходный код (Win32 + C++17)
├── resource.h                  — константы ресурсов
├── installer.rc                — ресурсы (иконка, манифест)
├── logo_data.h                 — логотип, встроенный как байтовый массив
├── flags_data.h                — флаги языков (ENG, RU, TR, BR), встроенные как байтовые массивы
├── REvenge_Installer.vcxproj   — файл проекта VS 2022
├── app.ico                     — иконка приложения
└── README.md                   — эта инструкция
```

---

## Требования

| Компонент | Версия |
|-----------|--------|
| Visual Studio | 2022 (Community / Professional / Enterprise) |
| C++ Workload | «Desktop development with C++» |
| Windows SDK | 10.0.19041.0 или новее |
| Целевая ОС | Windows 10 / 11 (x64) |

---

## Шаг 1 — Открыть проект

1. Запустить **Visual Studio 2022**
2. `File → Open → Project/Solution`
3. Выбрать `REvenge_Installer.vcxproj`

---

## Шаг 2 — Сборка Release

1. Переключить конфигурацию: **Release | x64**
2. `Build → Build Solution` (или `Ctrl+Shift+B`)
3. Готовый файл:
   ```
   x64\Release\REvenge_Installer.exe
   ```

> ⚡ Release собирается со статической CRT (`/MT`) — `.exe` работает **без Visual C++ Redistributable**.

---

## Шаг 3 — Сборка через командную строку (альтернатива)

Открыть **x64 Native Tools Command Prompt for VS 2022**:

```bat
cd путь\к\REvenge_Installer

:: Компиляция ресурсов
rc /fo installer.res installer.rc

:: Компиляция и линковка
cl /nologo /std:c++17 /O2 /MT /W3 /DUNICODE /D_UNICODE ^
   /DNDEBUG /D_WINDOWS ^
   main.cpp installer.res ^
   /link /SUBSYSTEM:WINDOWS /OUT:REvenge_Installer.exe ^
   wininet.lib shell32.lib dwmapi.lib ^
   comctl32.lib msimg32.lib user32.lib gdi32.lib kernel32.lib
```

---

## Как работает установщик

```
[Запуск]
    │
    ▼
[Главное окно] ── 780×500 px, тёмный фон (#121212)
    │
    ├── Левая панель — логотип + название + версия
    │
    └── Правая панель:
         ├── Выбор языка (EN / RU / TR / PT) с флагами
         ├── Прогресс-бар загрузки + статус
         ├── [Скачать и Установить]
         └── [Настройки]
    │
    ▼
[Поток загрузки] ── 4 параллельных потока, WinInet HTTPS ──► GitHub Releases
    │   (WM_DL_PROGRESS → обновление прогресс-бара)
    ▼
[Файл сохранён] ──► %USERPROFILE%\Downloads\REvenge_Setup_1.5.0.exe
    │
    ▼
[ShellExecute] ──── автоматический запуск установщика
```

---

## Особенности реализации

### Интерфейс
- Одно окно — выбор языка и загрузка в одном месте, без отдельного экрана темы
- Левая боковая панель с логотипом и названием приложения
- Флаги языков загружаются из встроенных PNG-байтовых массивов (`flags_data.h`, `logo_data.h`) через GDI+
- Нижняя строка статуса: результат проверки железа, качество соединения, число активных потоков

### High DPI
- `SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)` — до создания окон
- Функция `S(int v)` масштабирует все размеры через `MulDiv(v, g_dpi, 96)`

### Отрисовка без артефактов
- Двойная буферизация (off-screen bitmap + `BitBlt`)
- `WM_ERASEBKGND` возвращает 1
- Все дочерние элементы subclass-ированы и рисуются вручную

### Скруглённые углы
- Windows 11: `DwmSetWindowAttribute(DWMWA_WINDOW_CORNER_PREFERENCE, DWMWCP_ROUND)`
- Карточки и кнопки: `RoundRect` из GDI

### Многопоточная загрузка
- `NUM_THREADS = 4` параллельных потока (`std::thread`)
- Файл делится на диапазоны байт, каждый поток загружает свой кусок
- Прогресс передаётся через `PostMessage(WM_DL_PROGRESS)` — потокобезопасно
- Завершение через `PostMessage(WM_DL_DONE)`

### Путь сохранения
Установщик сохраняет файл в папку `Downloads` пользователя. При неудаче — в `%TEMP%`.

---

## Локализация

Поддерживается 4 языка, переключаются кнопками с флагами прямо в главном окне:

| Язык | Флаг |
|------|------|
| English | 🇺🇸 |
| Русский | 🇷🇺 |
| Türkçe | 🇹🇷 |
| Português | 🇧🇷 |

Все строки интерфейса определены в структурах `kLangEN`, `kLangRU`, `kLangTR`, `kLangPT` в `main.cpp`.

---

## Изменение ссылки для скачивания

В `main.cpp`:

```cpp
constexpr wchar_t DL_URL[]  = L"https://github.com/.../REvenge_Setup_1.5.0.exe";
constexpr wchar_t DL_NAME[] = L"REvenge_Setup_1.5.0.exe";
```

Замените на нужный URL и имя файла.

---

## Зависимости (только системные DLL)

| Библиотека | Назначение |
|------------|-----------|
| `wininet.lib` | HTTPS-загрузка |
| `shell32.lib` | `ShellExecute`, `SHGetFolderPath` |
| `dwmapi.lib` | скруглённые углы, тёмная строка заголовка (Win 11) |
| `comctl32.lib` | `SetWindowSubclass`, `DefSubclassProc` |
| `msimg32.lib` | `GradientFill` |
| `user32`, `gdi32`, `kernel32` | базовые Win32 |

Все статически слинкованы — `.exe` не требует установки дополнительных компонентов.
