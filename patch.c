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
launcherXX.exeによって注入されることで、対象実行可能ファイルの起動時に実行されるパッチプログラム
RegisterDragDrop()にフックして、その実行前にOLEを初期化する。
OLEの初期化が行われていないのにRegisterDragDrop()がコールされることで不具合を起こすプログラム[1]に有効。

[1] v0.6.14以前のSiv3Dを用いてビルドされたWindowsプログラムをWineで実行する場合、など
*/

#include <windows.h>
#include <ole2.h>
#include "MinHook.h"

typedef HRESULT (WINAPI *RegisterDragDrop_t)(HWND, IDropTarget*);
RegisterDragDrop_t fpRegisterDragDrop = NULL;

#ifndef APTTYPEQUALIFIER_DEFINED
typedef enum _APTTYPEQUALIFIER
{
  APTTYPEQUALIFIER_NONE = 0,
  APTTYPEQUALIFIER_IMPLICIT_MTA = 1,
  APTTYPEQUALIFIER_NA_ON_MTA = 2,
  APTTYPEQUALIFIER_NA_ON_STA = 3,
  APTTYPEQUALIFIER_NA_ON_IMPLICIT_MTA = 4,
  APTTYPEQUALIFIER_NA_ON_MAINSTA = 5,
  APTTYPEQUALIFIER_APPLICATION_STA = 6
} APTTYPEQUALIFIER;
#define APTTYPEQUALIFIER_DEFINED
#endif
WINOLEAPI CoGetApartmentType(APTTYPE *pAptType, APTTYPEQUALIFIER *pAptQualifier);

// フック関数 (Detour)
HRESULT WINAPI DetourRegisterDragDrop(HWND hwnd, IDropTarget* pDropTarget)
{
  // スレッドのアパートメントタイプを取得する STAかMTAか
  APTTYPE aptType;
  APTTYPEQUALIFIER aptQual;
  HRESULT hrAptType = CoGetApartmentType(&aptType, &aptQual);

  // OLEの初期化 スレッドはSTAになる
  HRESULT hrOleInit = OleInitialize(NULL);

  // 元のアパートメントタイプがMTAなら、MTAに復元する（Siv3Dと同じ方法）
  if ( hrAptType == S_OK && aptType == APTTYPE_MTA && (hrOleInit == S_OK || hrOleInit == S_FALSE) )
  {
    CoUninitialize();
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
  }

  // オリジナルの関数へ
  return fpRegisterDragDrop(hwnd, pDropTarget);
}

// エントリポイント
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
  if (fdwReason == DLL_PROCESS_ATTACH)
  {
    // MinHook初期化
    if (MH_Initialize() != MH_OK) return FALSE;

    // ole32.dll の RegisterDragDrop をフック
    if (MH_CreateHookApi(L"ole32", "RegisterDragDrop", &DetourRegisterDragDrop, (LPVOID*)&fpRegisterDragDrop) == MH_OK)
    {
      MH_EnableHook(MH_ALL_HOOKS);
    }
  }
  else if (fdwReason == DLL_PROCESS_DETACH)
  {
    MH_Uninitialize();
  }
  return TRUE;
}
