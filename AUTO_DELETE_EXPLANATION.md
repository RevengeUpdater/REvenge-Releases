# REvenge Installer — Инструкция по сборке

## Структура проекта

```
REvenge_Installer/
├── main.cpp              — весь исходный код (Win32 + C++17)
├── resource.h            — константы ресурсов
├── installer.rc          — ресурсы (иконка, манифест)
├── REvenge_Installer.vcxproj  — файл проекта VS 2022
├── app.ico               — (опционально) иконка приложения
└── README.md             — эта инструкция
```

---

## Требования

| Компонент | Версия |
|-----------|--------|
| Visual Studio | 2022 (любой edition — Community / Professional / Enterprise) |
| C++ Workload | "Desktop development with C++" |
| Windows SDK | 10.0.19041.0 или новее |
| Целевая ОС | Windows 10 / 11 (x64) |

---

## Шаг 1 — Открыть проект

1. Запустить **Visual Studio 2022**
2. Открыть `File → Open → Project/Solution`
3. Выбрать файл `REvenge_Installer.vcxproj`

---

## Шаг 2 — (опционально) Добавить иконку

Если хотите кастомную иконку:

1. Создайте или скачайте файл `app.ico` (рекомендуемый размер: 256×256 + 48×48 + 32×32 + 16×16)
2. Положите `app.ico` в папку проекта рядом с `installer.rc`
3. В файле `installer.rc` раскомментируйте строку:
   ```rc
   IDI_MAIN_ICON  ICON  "app.ico"
   ```
4. В `main.cpp` замените:
   ```cpp
   wcTheme.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
   wcMain.hIcon  = LoadIcon(nullptr, IDI_APPLICATION);
   ```
   на:
   ```cpp
   wcTheme.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_MAIN_ICON));
   wcMain.hIcon  = LoadIcon(hInst, MAKEINTRESOURCE(IDI_MAIN_ICON));
   ```

---

## Шаг 3 — Сборка Release

В Visual Studio:

1. Переключить конфигурацию: `Release` | `x64` (выпадающие списки на панели инструментов)
2. Нажать `Build → Build Solution` (или `Ctrl+Shift+B`)
3. Готовый файл появится в:
   ```
   x64\Release\REvenge_Installer.exe
   ```

> ⚡ **Release** компилируется со статической CRT (`/MT`),  
> поэтому `.exe` работает **без установки Visual C++ Redistributable**.

---

## Шаг 4 — Сборка через командную строку (альтернатива)

Открыть **x64 Native Tools Command Prompt for VS 2022** и выполнить:

```bat
cd путь\к\REvenge_Installer

:: Компиляция ресурсов
rc /fo installer.res installer.rc

:: Компиляция и линковка одной командой
cl /nologo /std:c++17 /O2 /MT /W3 /DUNICODE /D_UNICODE ^
   /DNDEBUG /D_WINDOWS ^
   main.cpp installer.res ^
   /link /SUBSYSTEM:WINDOWS /OUT:REvenge_Installer.exe ^
   wininet.lib shell32.lib uxtheme.lib dwmapi.lib ^
   comctl32.lib user32.lib gdi32.lib kernel32.lib
```

---

## Как работает установщик

```
[Запуск]
    │
    ▼
[Окно выбора темы]  ←── 480×300 px, тёмный фон
    │
    ├─── [Dark Mode] ──► тема #1E1E1E + #BB86FC
    │
    └─── [Pink Mode] ──► тема #FFF0F5 + #FF69B4
    │
    ▼
[Главное окно] ──── 520×340 px
    │
    [Кнопка "Download & Install"]
    │
    ▼
[Поток загрузки] ── WinInet HTTPS ──► GitHub Releases
    │   (WM_PROGRESS → обновление прогресс-бара)
    ▼
[Файл сохранён] ──► %USERPROFILE%\Downloads\REvenge_Setup_1.5.0.exe
    │
    ▼
[ShellExecute] ──── автоматический запуск установщика
```

---

## Особенности реализации

### High DPI
- `SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)` — устанавливается до создания любых окон
- Функция `Scale(int v)` масштабирует все размеры через `MulDiv(v, dpi, 96)`
- Шрифты создаются с отрицательным размером (`CreateFont(-Scale(size), ...)`) — GDI корректно обрабатывает DPI

### Отрисовка без артефактов
- Двойная буферизация (off-screen bitmap + `BitBlt`) в `PaintMain` и `DrawThemeWindow`
- `WM_ERASEBKGND` возвращает 1 (не стираем фон лишний раз)
- Все дочерние элементы (статик-текст, кнопка) subclass-ированы и рисуются вручную

### Скруглённые углы
- Windows 11: через `DwmSetWindowAttribute(DWMWA_WINDOW_CORNER_PREFERENCE, DWMWCP_ROUND)`
- Карточки / кнопки: `RoundRect` из GDI с `r = Scale(8)`

### Загрузка
- Выполняется в `std::thread` — UI не блокируется
- Прогресс передаётся через `PostMessage(WM_PROGRESS)` — потокобезопасно
- Завершение через `PostMessage(WM_DONE)` с кодом 1 (успех) / 0 (ошибка)

### Путь сохранения
Установщик пытается сохранить файл в папку `Downloads` пользователя.  
Если не получается — падает обратно в `%TEMP%`.

---

## Изменение ссылки для скачивания

В `main.cpp`, строка `DOWNLOAD_URL`:

```cpp
constexpr wchar_t DOWNLOAD_URL[] =
    L"https://github.com/RevengeUpdater/REvenge-Releases/releases/download/v1.5.0/REvenge_Setup_1.5.0.exe";
```

Замените URL на нужный. Аналогично `SAVE_NAME` — имя сохраняемого файла.

---

## Зависимости (только системные DLL)

| Библиотека | Назначение |
|------------|-----------|
| `wininet.lib` | HTTPS-загрузка через WinInet |
| `shell32.lib` | `ShellExecute`, `SHGetFolderPath` |
| `uxtheme.lib` | темизация (отключение стандартных тем для кастомной отрисовки) |
| `dwmapi.lib` | скруглённые углы, тёмная строка заголовка (Windows 11) |
| `comctl32.lib` | `SetWindowSubclass`, `DefSubclassProc` |
| `user32`, `gdi32`, `kernel32` | базовые Win32 |

Все статически слинкованы — итоговый `.exe` не требует установки дополнительных компонентов.
