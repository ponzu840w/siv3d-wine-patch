#include <windows.h>
#include <stdio.h>
#include <string.h>

// Makefileから渡されない場合のフォールバック
#ifndef DEFAULT_DLL_NAME
#define DEFAULT_DLL_NAME "patch.dll"
#endif

// ファイルの存在確認
int FileExists(const char* path) {
    DWORD attrib = GetFileAttributesA(path);
    return (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
}

// パス区切り文字が含まれているか判定
int HasPathSeparator(const char* path) {
    return (strchr(path, '\\') != NULL || strchr(path, '/') != NULL);
}

// パスを結合するヘルパー (dir + filename)
void JoinPath(char* dest, const char* dir, const char* filename) {
    strcpy(dest, dir);
    // ディレクトリ末尾に区切り文字がなければ追加
    size_t len = strlen(dest);
    if (len > 0 && dest[len - 1] != '\\' && dest[len - 1] != '/') {
        strcat(dest, "\\");
    }
    strcat(dest, filename);
}

// フルパスからディレクトリ部分を抽出 (末尾の区切り文字は残すか、親切に追加して返す)
void GetDirFromFullPath(const char* fullPath, char* outDir) {
    strcpy(outDir, fullPath);
    char* lastSlash = strrchr(outDir, '\\');
    char* lastSlash2 = strrchr(outDir, '/');
    if (lastSlash2 > lastSlash) lastSlash = lastSlash2;

    if (lastSlash) {
        *(lastSlash + 1) = '\0'; // 区切り文字の直後で切る (例: "C:\Path\" になる)
    } else {
        // 区切り文字がない（通常GetFullPathNameを通せばありえないが念のため）
        strcpy(outDir, ".\\");
    }
}

void print_usage(const char* progName) {
    printf("Usage: %s [options] <target_exe>\n", progName);
    printf("Options:\n");
    printf("  --dll <path|filename>  Specify DLL (Default: %s)\n", DEFAULT_DLL_NAME);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char* targetExe = NULL;
    const char* dllInput = DEFAULT_DLL_NAME;

    // --- 引数解析 ---
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--dll") == 0) {
            if (i + 1 < argc) {
                dllInput = argv[++i];
            } else {
                printf("Error: --dll option requires an argument.\n");
                return 1;
            }
        } else {
            targetExe = argv[i];
        }
    }

    if (targetExe == NULL) {
        printf("Error: Target executable not specified.\n");
        return 1;
    }

    char finalDllPath[MAX_PATH] = { 0 };

    // --- DLL探索ロジック ---
    if (HasPathSeparator(dllInput)) {
        // パターンA: パス指定あり (絶対パス or 相対パス) -> 最優先・一発勝負
        GetFullPathNameA(dllInput, MAX_PATH, finalDllPath, NULL);
        
        if (!FileExists(finalDllPath)) {
            printf("Error: DLL not found at specified path: %s\n", finalDllPath);
            return 1;
        }
    } else {
        // パターンB: ファイル名のみ指定 -> 探索開始
        printf("Search mode for DLL: %s\n", dllInput);
        char checkPath[MAX_PATH];

        // 1. カレントディレクトリ
        GetCurrentDirectoryA(MAX_PATH, checkPath);
        JoinPath(checkPath, checkPath, dllInput);
        
        if (FileExists(checkPath)) {
            strcpy(finalDllPath, checkPath);
            printf("Found in Current Dir: %s\n", finalDllPath);
            goto FOUND;
        }

        // 2. EXEのあるディレクトリ
        char targetFull[MAX_PATH];
        char targetDir[MAX_PATH];
        GetFullPathNameA(targetExe, MAX_PATH, targetFull, NULL); // 相対パスかもしれないので絶対パス化
        GetDirFromFullPath(targetFull, targetDir);
        
        JoinPath(checkPath, targetDir, dllInput);
        if (FileExists(checkPath)) {
            strcpy(finalDllPath, checkPath);
            printf("Found in Target Dir: %s\n", finalDllPath);
            goto FOUND;
        }

        // 3. ランチャーのあるディレクトリ
        char launcherFull[MAX_PATH];
        char launcherDir[MAX_PATH];
        GetModuleFileNameA(NULL, launcherFull, MAX_PATH);
        GetDirFromFullPath(launcherFull, launcherDir);

        JoinPath(checkPath, launcherDir, dllInput);
        if (FileExists(checkPath)) {
            strcpy(finalDllPath, checkPath);
            printf("Found in Launcher Dir: %s\n", finalDllPath);
            goto FOUND;
        }

        // 見つからなかった場合
        printf("Error: DLL '%s' not found in search paths.\n", dllInput);
        printf("Checked:\n 1. Current Dir\n 2. Target Dir (%s)\n 3. Launcher Dir (%s)\n", targetDir, launcherDir);
        return 1;
    }

FOUND:
    // --- 注入と起動 ---
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };

    printf("Launching: %s\n", targetExe);
    printf("Injecting: %s\n", finalDllPath);

    if (!CreateProcessA(NULL, (char*)targetExe, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        printf("Error: Failed to create process. (Code: %lu)\n", GetLastError());
        return 1;
    }

    void* pRemoteMem = VirtualAllocEx(pi.hProcess, NULL, MAX_PATH, MEM_COMMIT, PAGE_READWRITE);
    if (!pRemoteMem) {
        printf("Error: VirtualAllocEx failed.\n");
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }

    if (!WriteProcessMemory(pi.hProcess, pRemoteMem, finalDllPath, strlen(finalDllPath) + 1, NULL)) {
        printf("Error: WriteProcessMemory failed.\n");
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }

    LPVOID pLoadLibrary = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");

    HANDLE hThread = CreateRemoteThread(pi.hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pLoadLibrary, pRemoteMem, 0, NULL);
    if (!hThread) {
        printf("Error: CreateRemoteThread failed.\n");
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }

    WaitForSingleObject(hThread, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);
    VirtualFreeEx(pi.hProcess, pRemoteMem, 0, MEM_RELEASE);

    if (exitCode == 0) {
        printf("Warning: LoadLibrary in remote process might have failed (ExitCode: 0).\n");
    } else {
        printf("Injection successful! Resuming process...\n");
    }

    ResumeThread(pi.hThread);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return 0;
}
