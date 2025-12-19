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
Demonstrate the difference in behavior between Windows 11 and wine-stable 10.0 (macOS).
This difference in behavior explains why older Siv3D Windows apps fail to launch under Wine.
*/

// Uncomment this if targetting older than Windows 7 or ReactOS
// #define OLDER_THAN_WIN7_OR_REACTOS

#include <windows.h>
#include <ole2.h>
#include <stdio.h>

// Missing definition in .h for TCC/MinGW
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

// Prototype
void LogInfo(const char* tName, const char* msg);
void LogHr(const char* tName, const char* action, HRESULT hr);
void ShowApartmentState(const char* tName);
const char* GetAptTypeStr(APTTYPE t);
const char* GetAptQualStr(APTTYPEQUALIFIER q);
const char* GetHrStr(HRESULT hr);
extern IDropTargetVtbl vtbl;
static IDropTarget dropTarget = { &vtbl };

// ---------------------------------------------------------
//               WorkerThread (Thread B)
// ---------------------------------------------------------
DWORD WINAPI WorkerThread(LPVOID lpParam)
{
  HWND hwnd = (HWND)lpParam;
  const char* tName = "Thread B";
  HRESULT hr;

  LogInfo(tName, "STARTED");

  // Checking the initial state (expected: implicit MTA)
  ShowApartmentState(tName);

  printf("\n~~~ Step 4. Can Thread B use OLE? ~~~\n");
  printf("\tWindows:\tYES.\n\tWine:\t\tNO.\n");
  LogInfo(tName, "Attempting RegisterDragDrop (No Init)...");
  hr = RegisterDragDrop(hwnd, &dropTarget);
  LogHr(tName, "RegisterDragDrop", hr);

  printf("\n~~~ Extra Step. OLE is not initialized in thread B, so it can be initialized now.  ~~~\n");
  // 1: The initialization will be successful
  hr = OleInitialize(NULL);
  LogHr(tName, "OleInitialize", hr);
  ShowApartmentState(tName);
  // 2: Multiple initializations will fail
  hr = OleInitialize(NULL);
  LogHr(tName, "OleInitialize", hr);
  ShowApartmentState(tName);

  LogInfo(tName, "Attempting RegisterDragDrop (After Init)...");
  hr = RegisterDragDrop(hwnd, &dropTarget);
  LogHr(tName, "RegisterDragDrop", hr);

  OleUninitialize();
  return 0;
}

// ---------------------------------------------------------
//                MainThread (Thread A)
// ---------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  if (msg == WM_DESTROY) PostQuitMessage(0);
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

int main()
{
  const char* tName = "Thread A";
  setvbuf(stdout, NULL, _IONBF, 0);
  HRESULT hr;

  printf("~~~ Step 1. OleInitialize makes Thread A STA ~~~\n");
  LogInfo(tName, "STARTED");

  // Checking the initial state (expected: UNINITIALIZED)
  ShowApartmentState(tName);

  // OleInitialize -> STA
  hr = OleInitialize(NULL);
  LogHr(tName, "OleInitialize", hr);
  ShowApartmentState(tName);

  printf("\n~~~ Step 2. COM reinitialization forces Thread A to become MTA ~~~\n");

  // COM reinitialization -> MTA
  if ( hr >= 0 /* hr !=  RPC_E_CHANGED_MODE */ )
  {
    CoUninitialize();
    LogInfo(tName, "CoUninitialize");
    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    LogHr(tName, "CoInitializeEx(MTA)", hr);
  }
  ShowApartmentState(tName);

  printf("\n~~~ Step 3. New Thread B is implicitly MTA ~~~\n");

  // Create Window
  WNDCLASSA wc = {0};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = GetModuleHandle(NULL);
  wc.lpszClassName = "ThreadTestClass";
  RegisterClassA(&wc);

  HWND hwnd = CreateWindowA("ThreadTestClass", "Thread Test",
                            WS_OVERLAPPEDWINDOW, 100, 100, 200, 200,
                            NULL, NULL, wc.hInstance, NULL);

  LogInfo(tName, "Window Created");

  // CreateThread
  LogInfo(tName, "Spawning Worker Thread...");
  CreateThread(NULL, 0, WorkerThread, hwnd, 0, NULL);

  // Message Loop
  MSG msg;
  DWORD startTime = GetTickCount();
  while (GetTickCount() - startTime < 500)
  {
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    Sleep(5);
  }

  CoUninitialize();
  LogInfo(tName, "FINISHED");
  return 0;
}

// ---------------------------------------------------------
//                        Log Helper
// ---------------------------------------------------------

void ShowApartmentState(const char* tName)
{
#ifndef OLDER_THAN_WIN7_OR_REACTOS
  APTTYPE aptType;
  APTTYPEQUALIFIER aptQual;
  HRESULT hr = CoGetApartmentType(&aptType, &aptQual);

  //printf("\n   === [%s] APT STATE===\n", tName);
  printf("[%s] =====< ", tName);
  if (hr == S_OK)
  {
    printf("Type: %s (0x%X), ", GetAptTypeStr(aptType), aptType);
    printf("Qual: %s (0x%X)", GetAptQualStr(aptQual), aptQual);
  }
  else
  {
    printf("State: UNINITIALIZED / UNKNOWN (hr=0x%08X)", (unsigned int)hr);
  }
  printf(" >=====\n");
#endif
}

void LogInfo(const char* tName, const char* msg)
{
  printf("[%s] %s\n", tName, msg);
}

void LogHr(const char* tName, const char* action, HRESULT hr)
{
  const char* resultStr = SUCCEEDED(hr) ? "SUCCESS" : "FAILED";
  printf("[%s] %-20s -> %s [0x%08X : %s]\n", tName, action, resultStr, (unsigned int)hr, GetHrStr(hr));
}

const char* GetHrStr(HRESULT hr)
{
  switch (hr)
  {
  case S_OK:                          return "S_OK";
  case S_FALSE:                       return "S_FALSE";
  case E_INVALIDARG:                  return "E_INVALIDARG";
  case E_OUTOFMEMORY:                 return "E_OUTOFMEMORY/'COM not initialized'(wine)";
  case E_NOINTERFACE:                 return "E_NOINTERFACE";
  case E_NOTIMPL:                     return "E_NOTIMPL";
  case E_FAIL:                        return "E_FAIL";
  case CO_E_NOTINITIALIZED:           return "CO_E_NOTINITIALIZED";
  case CO_E_ALREADYINITIALIZED:       return "CO_E_ALREADYINITIALIZED";
  case RPC_E_CHANGED_MODE:            return "RPC_E_CHANGED_MODE";
  case DRAGDROP_E_INVALIDHWND:        return "DRAGDROP_E_INVALIDHWND";
  case DRAGDROP_E_ALREADYREGISTERED:  return "DRAGDROP_E_ALREADYREGISTERED";
  default:                            return "Unknown HRESULT";
  }
}

const char* GetAptTypeStr(APTTYPE t)
{
  switch (t)
  {
  case APTTYPE_CURRENT: return "CURRENT";
  case APTTYPE_STA:     return "STA";
  case APTTYPE_MTA:     return "MTA";
  case APTTYPE_NA:      return "NA (Neutral)";
  case APTTYPE_MAINSTA: return "Main STA";
  default:              return "Unknown Type";
  }
}

const char* GetAptQualStr(APTTYPEQUALIFIER q)
{
  switch (q)
  {
  case APTTYPEQUALIFIER_NONE:               return "None";
  case APTTYPEQUALIFIER_IMPLICIT_MTA:       return "Implicit MTA";
  case APTTYPEQUALIFIER_NA_ON_MTA:          return "NA on MTA";
  case APTTYPEQUALIFIER_NA_ON_STA:          return "NA on STA";
  case APTTYPEQUALIFIER_NA_ON_IMPLICIT_MTA: return "NA on Implicit MTA";
  case APTTYPEQUALIFIER_NA_ON_MAINSTA:      return "NA on Main STA";
  case APTTYPEQUALIFIER_APPLICATION_STA:    return "Application STA";
  default:                                  return "Unknown Qualifier";
  }
}

// --- Dummy IDropTarget ---
HRESULT STDMETHODCALLTYPE DT_QueryInterface(IDropTarget *this, REFIID riid, void **ppv) { *ppv = NULL; return E_NOINTERFACE; }
ULONG STDMETHODCALLTYPE DT_AddRef(IDropTarget *this) { return 1; }
ULONG STDMETHODCALLTYPE DT_Release(IDropTarget *this) { return 1; }
HRESULT STDMETHODCALLTYPE DT_DragEnter(IDropTarget *this, IDataObject *d, DWORD k, POINTL p, DWORD *e) { return S_OK; }
HRESULT STDMETHODCALLTYPE DT_DragOver(IDropTarget *this, DWORD k, POINTL p, DWORD *e) { return S_OK; }
HRESULT STDMETHODCALLTYPE DT_DragLeave(IDropTarget *this) { return S_OK; }
HRESULT STDMETHODCALLTYPE DT_Drop(IDropTarget *this, IDataObject *d, DWORD k, POINTL p, DWORD *e) { return S_OK; }
IDropTargetVtbl vtbl = { DT_QueryInterface, DT_AddRef, DT_Release, DT_DragEnter, DT_DragOver, DT_DragLeave, DT_Drop };
