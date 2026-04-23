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

// ─────────────────────────────────────────────────────────────────────────────
// CONFIGURACIÓN
// ─────────────────────────────────────────────────────────────────────────────

// Volumen máximo permitido (0.0f – 1.0f). 0.20f = 20 %.
static constexpr float  MAX_VOLUME       = 0.20f;

// Intervalo de comprobación en milisegundos.
static constexpr DWORD  TICK_INTERVAL_MS = 500; // cada 500 ms

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

    if (fLevel > MAX_VOLUME)
        g_pEndpointVolume->SetMasterVolumeLevelScalar(MAX_VOLUME, nullptr);
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

