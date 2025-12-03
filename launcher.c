#include <windows.h>
#include <stdio.h>
#include <string.h>

// 使い方: launcher.exe "対象アプリ.exe"
int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <target_exe>\n", argv[0]);
        return 1;
    }

    const char* targetExe = argv[1];
    // 注入するDLLの名前（同じフォルダにある前提）
    const char* dllName = "patch.dll";
    
    char fullDllPath[MAX_PATH];
    // DLLのフルパスを取得（LoadLibraryはフルパス推奨）
    if (GetFullPathNameA(dllName, MAX_PATH, fullDllPath, NULL) == 0) {
        printf("Error: Cannot find absolute path for %s\n", dllName);
        return 1;
    }

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };

    printf("Launching: %s\n", targetExe);
    printf("Injecting: %s\n", fullDllPath);

    // 1. 対象プロセスを「サスペンド（一時停止）状態」で起動
    //    これならDLL注入前に勝手に動き出すことがない
    if (!CreateProcessA(NULL, argv[1], NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        printf("Error: Failed to create process. (Code: %lu)\n", GetLastError());
        return 1;
    }

    // 2. 対象プロセスのメモリ空間に、DLLパスの文字列を書き込む場所を確保
    void* pRemoteMem = VirtualAllocEx(pi.hProcess, NULL, MAX_PATH, MEM_COMMIT, PAGE_READWRITE);
    if (!pRemoteMem) {
        printf("Error: VirtualAllocEx failed.\n");
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }

    // 3. DLLパスを対象プロセスに書き込む
    if (!WriteProcessMemory(pi.hProcess, pRemoteMem, fullDllPath, strlen(fullDllPath) + 1, NULL)) {
        printf("Error: WriteProcessMemory failed.\n");
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }

    // 4. LoadLibraryAのアドレスを取得（kernel32.dllのロードアドレスは全プロセス共通という特性を利用）
    LPVOID pLoadLibrary = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");

    // 5. 対象プロセス内で「LoadLibraryA("fix.dll")」を実行するスレッドを作成
    HANDLE hThread = CreateRemoteThread(pi.hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pLoadLibrary, pRemoteMem, 0, NULL);
    if (!hThread) {
        printf("Error: CreateRemoteThread failed.\n");
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }

    // 6. 注入完了を待つ
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    VirtualFreeEx(pi.hProcess, pRemoteMem, 0, MEM_RELEASE);

    printf("Injection successful! Resuming process...\n");

    // 7. 止めていたメインスレッドを再開（ゲーム開始）
    ResumeThread(pi.hThread);

    // 後始末
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return 0;
}
