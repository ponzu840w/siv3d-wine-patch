#include <windows.h>
#include <stdio.h>
#include "MinHook.h" // MinHookライブラリ (前回と同じ)

// 関数ポインタの定義
typedef HRESULT (WINAPI *RegisterDragDrop_t)(HWND, IDropTarget*);
RegisterDragDrop_t fpRegisterDragDrop = NULL;

// フック関数 (Detour)
HRESULT WINAPI DetourRegisterDragDrop(HWND hwnd, IDropTarget* pDropTarget) {
    // ★ここで強制初期化 (このスレッドが未初期化ならS_OKで初期化される)
    OleInitialize(NULL);
    
    // オリジナルの関数へ
    return fpRegisterDragDrop(hwnd, pDropTarget);
}

// エントリーポイント
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        // MinHook初期化
        if (MH_Initialize() != MH_OK) return FALSE;

        // ole32.dll の RegisterDragDrop をフック
        // (EXE起動時にole32もロードされるので見つかるはず)
        if (MH_CreateHookApi(L"ole32", "RegisterDragDrop", &DetourRegisterDragDrop, (LPVOID*)&fpRegisterDragDrop) == MH_OK) {
            MH_EnableHook(MH_ALL_HOOKS);
        }
    }
    else if (fdwReason == DLL_PROCESS_DETACH) {
        MH_Uninitialize();
    }
    return TRUE;
}
