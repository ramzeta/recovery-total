# Recovery Total

Herramienta nativa de recuperación de archivos para Windows, escrita en C++20 con interfaz Win32. Combina lectura del MFT en volúmenes NTFS con *file carving* por firmas, para rescatar archivos borrados, particiones formateadas o unidades en estado RAW.

> ⚠️ Acceso a disco crudo. Debe ejecutarse **como Administrador**.

---

## Características

- **Tres modos de escaneo**
  - `QuickScan` — lectura del MFT (rápido, solo NTFS).
  - `DeepScan` — MFT + carving por firmas (exhaustivo).
  - `CarvingOnly` — escaneo crudo por firmas, ideal para particiones RAW o formateadas.
- **Parser NTFS** propio: lectura de MFT, *data runs* y reconstrucción de archivos.
- **Base de firmas** integrada por categoría: imágenes, documentos, vídeo, audio y archivos comprimidos.
- **GUI Win32 nativa** (ListView, ProgressBar, estilos visuales v6) — sin dependencias externas de UI.
- **Diagnóstico USB** (enumeración de dispositivos con SetupAPI).
- **Multihilo**: el escaneo corre en su propio thread con progreso y cancelación.
- **Privilegios elevados**: habilita `SE_BACKUP_NAME`, `SE_RESTORE_NAME` y `SE_MANAGE_VOLUME_NAME` al arrancar.

## Requisitos

- Windows 10 o superior (`_WIN32_WINNT = 0x0A00`).
- Compilador C++20 — probado con **MinGW-w64 / GCC** sobre MSYS2.
- **CMake ≥ 3.20** y **Ninja**.

Instalación de toolchain en MSYS2:

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
```

## Compilación

### Opción rápida (Windows)

```bat
build.bat
```

El script configura con CMake + Ninja en modo `Release`, compila y copia los runtime DLL necesarios (`libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`) junto al ejecutable.

### Manual

```bash
mkdir build && cd build
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++
ninja
```

Salida: `build/RecoveryTotal.exe`.

## Uso

1. Ejecuta `RecoveryTotal.exe` **como Administrador** (clic derecho → *Ejecutar como administrador*).
2. Selecciona la unidad o volumen a escanear.
3. Elige el modo de escaneo (`Quick`, `Deep` o `Carving`).
4. Espera al fin del escaneo y revisa los archivos encontrados en la lista.
5. Marca los archivos a recuperar y elige una **carpeta de destino** distinta de la unidad escaneada.

> Nunca recuperes archivos sobre la misma unidad de la que estás recuperando: puedes sobrescribir datos aún no rescatados.

## Estructura del proyecto

```
recovery-total/
├── CMakeLists.txt        # Build CMake (C++20, libs Win32)
├── build.bat             # Script de compilación con MinGW + Ninja
├── resources/
│   └── app.rc            # Manifest, iconos y estilos visuales
└── src/
    ├── main.cpp          # Entry point (WinMain / wWinMain)
    ├── gui.[h|cpp]       # Ventana principal y controles Win32
    ├── scanner.[h|cpp]   # Motor de escaneo multihilo
    ├── ntfs_parser.[h|cpp]   # Parser del MFT y data runs
    ├── recovery.[h|cpp]      # Recuperación desde MFT y por carving
    ├── file_signatures.[h|cpp]   # Base de firmas para carving
    ├── usb_diagnostic.[h|cpp]    # Enumeración de dispositivos USB
    └── utils.[h|cpp]     # Privilegios, errores, utilidades comunes
```

## Bibliotecas Win32 enlazadas

`comctl32`, `comdlg32`, `shell32`, `ole32`, `gdi32`, `user32`, `kernel32`, `advapi32`, `shlwapi`, `uxtheme`, `setupapi`.

## Limitaciones conocidas

- Solo Windows: hace uso intensivo de Win32 y de la estructura NTFS.
- `QuickScan` requiere un sistema de archivos NTFS legible; en volúmenes RAW debe usarse `CarvingOnly`.
- El *carving* recupera archivos contiguos; los archivos fragmentados solo se reconstruyen correctamente vía MFT.
- No se garantiza la integridad de archivos sobrescritos parcialmente.

## Aviso legal

Esta herramienta accede a sectores crudos del disco. Úsala únicamente en unidades de tu propiedad o con autorización expresa. El autor no se responsabiliza de la pérdida de datos derivada de un uso incorrecto.

## Licencia

Sin licencia declarada todavía. Hasta que se añada un `LICENSE`, todos los derechos quedan reservados al autor.
