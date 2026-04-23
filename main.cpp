/**
 * volume-limiter
 * Proceso de fondo invisible para Windows (sin consola, sin ventana, sin taskbar).
 * Limita el volumen maestro del dispositivo de audio predeterminado al 20 %.
 * Solo visible en la pestaña "Detalles" del Administrador de Tareas.
 *
 * Compilar (MSYS2 UCRT64):
 *   g++ main.cpp -o volume-limiter.exe -mwindows -static -O2 -s -std=c++17 -lole32 -loleaut32 -luuid
 *
 * Flags:
 *   -mwindows        -> Subsistema WINDOWS; sin consola
 *   -static          -> Vincula CRT y libstdc++ estáticamente
 *   -O2 -s           -> Optimización + elimina símbolos (binario más pequeño)
 *   -lole32 -loleaut32 -luuid -> COM + Core Audio APIs
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <fstream>
#include <string>
#include <cstdio>

// ─────────────────────────────────────────────────────────────────────────────
// CONFIGURACIÓN
// ─────────────────────────────────────────────────────────────────────────────

// Volumen máximo permitido por defecto (0.0f – 1.0f). 0.20f = 20 %.
static constexpr float  MAX_VOLUME_DEFAULT = 0.20f;

// Intervalo de comprobación en milisegundos.
static constexpr DWORD  TICK_INTERVAL_MS   = 500; // cada 500 ms

// Volumen máximo efectivo; se carga desde el archivo de configuración al inicio.
static float g_maxVolume = MAX_VOLUME_DEFAULT;

// ─────────────────────────────────────────────────────────────────────────────
// Lee el volumen máximo desde «volume-limiter.cfg» ubicado junto al ejecutable.
// Formato del archivo (líneas de comentario comienzan con # o ;):
//   max_volume=0.20
// Si el archivo no existe o el valor está fuera de rango, usa MAX_VOLUME_DEFAULT.
// ─────────────────────────────────────────────────────────────────────────────
static float LoadMaxVolume()
{
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    // Truncar tras la última barra para obtener el directorio del ejecutable.
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash)
        *(lastSlash + 1) = L'\0';
    else
        exePath[0] = L'\0';

    wchar_t cfgPath[MAX_PATH] = {};
    wcscpy_s(cfgPath, exePath);
    wcscat_s(cfgPath, L"volume-limiter.cfg");

    std::ifstream file(cfgPath);
    if (!file.is_open())
    {
        // El archivo no existe: crearlo con la configuración por defecto.
        std::ofstream pNew(cfgPath);
        if (pNew.is_open())
        {
            pNew << "# volume-limiter configuration\n"
                    "# Volumen maximo permitido: valor entre 0.0 (silencio) y 1.0 (100 %).\n"
                    "# Ejemplos: 0.20 = 20 %   |   0.50 = 50 %   |   1.0 = sin limite\n"
                    "max_volume=0.20\n";
        }
        return MAX_VOLUME_DEFAULT;
    }

    const std::string key = "max_volume=";
    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;

        if (line.rfind(key, 0) == 0)
        {
            try
            {
                float val = std::stof(line.substr(key.size()));
                if (val >= 0.0f && val <= 1.0f)
                    return val;
            }
            catch (...) {}
        }
    }

    return MAX_VOLUME_DEFAULT;
}

// ─────────────────────────────────────────────────────────────────────────────
// Estado global del endpoint de audio (inicializado una sola vez en WinMain)
// ─────────────────────────────────────────────────────────────────────────────
static IAudioEndpointVolume* g_pEndpointVolume = nullptr;

// ─────────────────────────────────────────────────────────────────────────────
// Inicializa COM y obtiene el IAudioEndpointVolume del dispositivo predeterminado.
// Devuelve true si tuvo éxito.
// ─────────────────────────────────────────────────────────────────────────────
static bool InitAudio()
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return false;

    IMMDeviceEnumerator* pEnumerator = nullptr;
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr,
        CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&pEnumerator));
    if (FAILED(hr))
        return false;

    IMMDevice* pDevice = nullptr;
    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    pEnumerator->Release();
    if (FAILED(hr))
        return false;

    hr = pDevice->Activate(
        __uuidof(IAudioEndpointVolume), CLSCTX_ALL,
        nullptr, reinterpret_cast<void**>(&g_pEndpointVolume));
    pDevice->Release();

    return SUCCEEDED(hr) && g_pEndpointVolume != nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// Lógica principal: si el volumen supera MAX_VOLUME, lo recorta.
// ─────────────────────────────────────────────────────────────────────────────
static void DoWork()
{
    if (!g_pEndpointVolume)
        return;

    float fLevel = 0.0f;
    if (FAILED(g_pEndpointVolume->GetMasterVolumeLevelScalar(&fLevel)))
        return;

    if (fLevel > g_maxVolume)
        g_pEndpointVolume->SetMasterVolumeLevelScalar(g_maxVolume, nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// Punto de entrada de la aplicación Windows (sin consola)
// ─────────────────────────────────────────────────────────────────────────────
int WINAPI WinMain(
    _In_     HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_     LPSTR     lpCmdLine,
    _In_     int       nCmdShow)
{
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    g_maxVolume = LoadMaxVolume();

    if (!InitAudio())
    {
        // Sin audio disponible: esperar y reintentar en el siguiente ciclo
        // (dispositivo aún no listo al arrancar, p. ej. en inicio de sesión).
        for (;;) Sleep(TICK_INTERVAL_MS);
    }

    // Waitable Timer periódico para CPU ≈ 0 % entre ciclos.
    HANDLE hTimer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    if (!hTimer)
    {
        for (;;)
        {
            DoWork();
            Sleep(TICK_INTERVAL_MS);
        }
    }

    LARGE_INTEGER liDueTime;
    liDueTime.QuadPart = -static_cast<LONGLONG>(TICK_INTERVAL_MS) * 10000LL;
    SetWaitableTimer(hTimer, &liDueTime, TICK_INTERVAL_MS, nullptr, nullptr, FALSE);

    for (;;)
    {
        if (WaitForSingleObject(hTimer, INFINITE) == WAIT_OBJECT_0)
            DoWork();
    }

    // Inalcanzable; limpieza formal.
    g_pEndpointVolume->Release();
    CloseHandle(hTimer);
    CoUninitialize();
    return 0;
}

