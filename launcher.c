/*
BSD 2-Clause License

Copyright (c) 2025, ponzu840w

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
パッチDLLをロードさせた上で対象の実行可能ファイルを起動します
パッチDLLはlauncherXX.exeと同じフォルダに存在するpatchXX.dll（XXは32/64）がデフォルトで使用されますが、
  --dll オプションで指定することもできます。
対象実行可能ファイルはコマンドライン引数として与えることができるほか、
  launcherXX.exeと同じフォルダに存在するもう一つの（唯一の）exeが選ばれます。

NOTE: ほとんどのコードはgeminiが生成したままの未検証コードであり、バグが含まれる可能性が大いにあります
*/

#include <windows.h>
#include <stdio.h>
#include <string.h>

// Makefileから渡されない場合のデフォルトDLL名
#ifndef DEFAULT_DLL_NAME
#define DEFAULT_DLL_NAME "patch.dll"
#endif

// -----------------------------------------------------------------------------
// ユーティリティ関数群
// -----------------------------------------------------------------------------

// ファイルの存在確認
// 戻り値: 1 = 存在する, 0 = 存在しない
int FileExists(const char* path)
{
  DWORD attrib = GetFileAttributesA(path);
  return (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
}

// パス区切り文字が含まれているか判定
// 戻り値: 1 = 含まれている, 0 = 含まれていない
int HasPathSeparator(const char* path)
{
  return (strchr(path, '\\') != NULL || strchr(path, '/') != NULL);
}

// パスを結合するヘルパー (dest = dir + filename)
// dirの末尾に区切り文字がなければ自動追加します
void JoinPath(char* dest, const char* dir, const char* filename)
{
  strcpy(dest, dir);
  size_t len = strlen(dest);
  if (len > 0 && dest[len - 1] != '\\' && dest[len - 1] != '/')
  {
    strcat(dest, "\\");
  }
  strcat(dest, filename);
}

// フルパスからディレクトリ部分を抽出
// "C:\Path\To\File.exe" -> "C:\Path\To\"
void GetDirFromFullPath(const char* fullPath, char* outDir)
{
  strcpy(outDir, fullPath);
  char* lastSlash = strrchr(outDir, '\\');
  char* lastSlash2 = strrchr(outDir, '/');
  if (lastSlash2 > lastSlash) lastSlash = lastSlash2;

  if (lastSlash)
  {
    *(lastSlash + 1) = '\0'; // 区切り文字の直後で終端させる
  }
  else
  {
    strcpy(outDir, ".\\");
  }
}

// -----------------------------------------------------------------------------
// 自動検出ロジック
// -----------------------------------------------------------------------------

// ディレクトリ内のEXEファイルを探索し、自動検出を試みる
// outTargetExe: 見つかったEXEのフルパスを格納するバッファ
// 戻り値: 1 = 成功, 0 = 失敗 (見つからない、または複数ある)
int AutoDetectTargetExe(char* outTargetExe)
{
  char launcherPath[MAX_PATH];
  char searchDir[MAX_PATH];
  char searchPattern[MAX_PATH];
  WIN32_FIND_DATA findData;
  HANDLE hFind;

  // 自分のフルパスを取得 (自分自身を除外するため)
  GetModuleFileNameA(NULL, launcherPath, MAX_PATH);
  GetDirFromFullPath(launcherPath, searchDir);

  // 検索パターン作成: "CurrentDir\*.exe"
  JoinPath(searchPattern, searchDir, "*.exe");

  printf("Auto-detecting target in: %s\n", searchDir);

  hFind = FindFirstFileA(searchPattern, &findData);
  if (hFind == INVALID_HANDLE_VALUE)
  {
    printf("Error: No .exe files found in directory.\n");
    return 0;
  }

  int count = 0;
  char candidate[MAX_PATH] = { 0 };

  do
  {
    // ディレクトリはスキップ
    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

    // 見つかったファイルのフルパスを作成
    char currentPath[MAX_PATH];
    JoinPath(currentPath, searchDir, findData.cFileName);

    // 自分自身 (ランチャー) はスキップ
    // ※ フルパス同士で比較 (大文字小文字の違いを吸収するため _stricmp 推奨だが、標準的な strcmp で簡易実装)
    if (strcasecmp(currentPath, launcherPath) == 0) continue;

    // 候補発見
    count++;
    if (count == 1) strcpy(candidate, currentPath);
    else            break; // 2つ以上見つかった場合はエラーにするためループを抜ける

  }
  while (FindNextFileA(hFind, &findData));

  FindClose(hFind);

  if (count == 0)
  {
    printf("Error: No other .exe files found in this directory.\n");
    return 0;
  }
  else if (count > 1)
  {
    printf("Error: Multiple .exe files found. Please specify target explicitly.\n");
    return 0;
  }

  // 唯一の候補を確定
  strcpy(outTargetExe, candidate);
  return 1;
}


// -----------------------------------------------------------------------------
// メイン処理
// -----------------------------------------------------------------------------

void print_usage(const char* progName)
{
  printf("Usage: %s [options] [target_exe]\n", progName);
  printf("Options:\n");
  printf("  --dll <path|filename>  Specify DLL to inject (Default: %s)\n", DEFAULT_DLL_NAME);
  printf("Note: If target_exe is omitted, the launcher looks for the only other .exe in the directory.\n");
}

int main(int argc, char* argv[])
{
  const char* targetExeArg = NULL;
  const char* dllInput = DEFAULT_DLL_NAME;

  // --- 引数解析 ---
  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "--dll") == 0)
    {
      if (i + 1 < argc)
      {
        dllInput = argv[++i];
      }
      else
      {
        printf("Error: --dll option requires an argument.\n");
        return 1;
      }
    }
    else
    {
      // オプション以外はターゲットEXEパスとみなす
      targetExeArg = argv[i];
    }
  }

  // --- ターゲットEXEの決定 ---
  char finalTargetExe[MAX_PATH] = { 0 };

  if (targetExeArg != NULL)
  {
    // 引数で指定された場合
    strcpy(finalTargetExe, targetExeArg);
  }
  else
  {
    // 引数がない場合、自動検出を試みる
    if (!AutoDetectTargetExe(finalTargetExe))
    {
      print_usage(argv[0]);
      return 1;
    }
    printf("Auto-detected target: %s\n", finalTargetExe);
  }

  char finalDllPath[MAX_PATH] = { 0 };

  // --- DLLパスの探索ロジック ---
  if (HasPathSeparator(dllInput))
  {
    // A. パスが含まれる場合 (絶対パス/相対パス指定) -> その場所を直接確認
    GetFullPathNameA(dllInput, MAX_PATH, finalDllPath, NULL);

    if (!FileExists(finalDllPath))
    {
      printf("Error: DLL not found at specified path: %s\n", finalDllPath);
      return 1;
    }
  }
  else
  {
    // B. ファイル名のみの場合 -> 探索
    printf("Search mode for DLL: %s\n", dllInput);
    char checkPath[MAX_PATH];

    // 1. カレントディレクトリ
    GetCurrentDirectoryA(MAX_PATH, checkPath);
    JoinPath(checkPath, checkPath, dllInput);

    if (FileExists(checkPath))
    {
      strcpy(finalDllPath, checkPath);
      printf("Found in Current Dir: %s\n", finalDllPath);
      goto FOUND_DLL;
    }

    // 2. EXEのあるディレクトリ
    char targetFull[MAX_PATH];
    char targetDir[MAX_PATH];
    GetFullPathNameA(finalTargetExe, MAX_PATH, targetFull, NULL);
    GetDirFromFullPath(targetFull, targetDir);

    JoinPath(checkPath, targetDir, dllInput);
    if (FileExists(checkPath))
    {
      strcpy(finalDllPath, checkPath);
      printf("Found in Target Dir: %s\n", finalDllPath);
      goto FOUND_DLL;
    }

    // 3. ランチャーのあるディレクトリ
    char launcherFull[MAX_PATH];
    char launcherDir[MAX_PATH];
    GetModuleFileNameA(NULL, launcherFull, MAX_PATH);
    GetDirFromFullPath(launcherFull, launcherDir);

    JoinPath(checkPath, launcherDir, dllInput);
    if (FileExists(checkPath))
    {
      strcpy(finalDllPath, checkPath);
      printf("Found in Launcher Dir: %s\n", finalDllPath);
      goto FOUND_DLL;
    }

    printf("Error: DLL '%s' not found in search paths.\n", dllInput);
    return 1;
  }

FOUND_DLL:

  // --- プロセスの起動とDLL注入 ---
  STARTUPINFOA si = { sizeof(si) };
  PROCESS_INFORMATION pi = { 0 };

  printf("Launching: %s\n", finalTargetExe);
  printf("Injecting: %s\n", finalDllPath);

  // 1. プロセスをサスペンド(一時停止)状態で起動
  if (!CreateProcessA(NULL, finalTargetExe, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi))
  {
    printf("Error: Failed to create process. (Code: %lu)\n", GetLastError());
    return 1;
  }

  // 2. ターゲットプロセス内にメモリを確保 (DLLパス文字列用)
  void* pRemoteMem = VirtualAllocEx(pi.hProcess, NULL, MAX_PATH, MEM_COMMIT, PAGE_READWRITE);
  if (!pRemoteMem)
  {
    printf("Error: VirtualAllocEx failed.\n");
    TerminateProcess(pi.hProcess, 1);
    return 1;
  }

  // 3. 確保したメモリにDLLパスを書き込む
  if (!WriteProcessMemory(pi.hProcess, pRemoteMem, finalDllPath, strlen(finalDllPath) + 1, NULL))
  {
    printf("Error: WriteProcessMemory failed.\n");
    TerminateProcess(pi.hProcess, 1);
    return 1;
  }

  // 4. LoadLibraryAのアドレスを取得 (Kernel32.dllのアドレスは全プロセス共通)
  LPVOID pLoadLibrary = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");

  // 5. リモートスレッドを作成して LoadLibraryA を実行させる
  HANDLE hThread = CreateRemoteThread(pi.hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pLoadLibrary, pRemoteMem, 0, NULL);
  if (!hThread)
  {
    printf("Error: CreateRemoteThread failed.\n");
    TerminateProcess(pi.hProcess, 1);
    return 1;
  }

  // 6. 注入スレッドの終了待ち
  WaitForSingleObject(hThread, INFINITE);
  DWORD exitCode = 0;
  GetExitCodeThread(hThread, &exitCode);
  CloseHandle(hThread);

  // パス用メモリの開放
  VirtualFreeEx(pi.hProcess, pRemoteMem, 0, MEM_RELEASE);

  if (exitCode == 0)
  {
    printf("Warning: LoadLibrary in remote process might have failed (ExitCode: 0).\n");
    // 失敗してもプロセスは再開してみる
  }
  else
  {
    printf("Injection successful! Resuming process...\n");
  }

  // 7. メインスレッドを再開 (ゲーム開始)
  ResumeThread(pi.hThread);

  // ハンドル開放
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  return 0;
}
