# MTA shared OLE initialization demo (Wine, Windows)
This program demonstrates the difference in behavior between Windows and Wine regarding OLE initialization in a multi-threaded environment.

`OleInitialize()` sets the thread (Thread A) to STA, so OLE initialization is usually required for each thread that uses OLE. However, you can make a thread MTA by calling `CoUninitialize()` and `CoInitializeEx(NULL, COINIT_MULTITHREADED)` after `OleInitialize()`.

(This implies the following call: `/*fake*/ OleInitializeEx(NULL, COINIT_MULTITHREADED);`)

When another thread (Thread B) is implicitly MTA, Windows: Since the two threads belong to the same apartment, you can use an already initialized OLE thread without initializing it in Thread B. Wine: `OleInitialize()` is required in Thread B as well.

Windows programs that use this trick to make use of OLE while keeping all threads MTA to simplify things may have problems using Wine.

## Build

### Tiny C Compiler (TCC)
```sh
wine tcc.exe mta_shared_ole.c -lole32 -luser32 -o mta_shared_ole.exe
```

### MinGW-w64
#### 32-bit:
```sh
i686-w64-mingw32-gcc mta_shared_ole.c -lole32 -luser32 -o mta_shared_ole.exe
```
#### 64-bit:
```sh
x86_64-w64-mingw32-gcc mta_shared_ole.c -lole32 -luser32 -o mta_shared_ole.exe
```

## Usage
### On Wine:
```sh
wine mta_shared_ole.exe
```

### On Windows:
```sh
./mta_shared_ole.exe
```

## Results
### Wine (homebrew wine-stable 10.0/macOS Tahoe 26.1)
```
~~~ Step 1. OleInitialize makes Thread A STA ~~~
[Thread A] STARTED
[Thread A] =====< State: UNINITIALIZED / UNKNOWN (hr=0x800401F0) >=====
[Thread A] OleInitialize        -> SUCCESS [0x00000000 : S_OK]
[Thread A] =====< Type: Main STA (0x3), Qual: None (0x0) >=====

~~~ Step 2. COM reinitialization forces Thread A to become MTA ~~~
[Thread A] CoUninitialize
[Thread A] CoInitializeEx(MTA)  -> SUCCESS [0x00000000 : S_OK]
[Thread A] =====< Type: MTA (0x1), Qual: None (0x0) >=====

~~~ Step 3. New Thread B is implicitly MTA ~~~
[Thread A] Window Created
[Thread A] Spawning Worker Thread...
[Thread B] STARTED
[Thread B] =====< Type: MTA (0x1), Qual: Implicit MTA (0x1) >=====

~~~ Step 4. Can Thread B use OLE? ~~~
	Windows:	YES.
	Wine:		NO.
[Thread B] Attempting RegisterDragDrop (No Init)...
[Thread B] RegisterDragDrop     -> FAILED [0x8007000E : E_OUTOFMEMORY/'COM not initialized'(wine)]

~~~ Extra Step. OLE is not initialized in thread B, so it can be initialized now.  ~~~
[Thread B] OleInitialize        -> SUCCESS [0x00000000 : S_OK]
[Thread B] =====< Type: Main STA (0x3), Qual: None (0x0) >=====
[Thread B] OleInitialize        -> SUCCESS [0x00000001 : S_FALSE]
[Thread B] =====< Type: Main STA (0x3), Qual: None (0x0) >=====
[Thread B] Attempting RegisterDragDrop (After Init)...
[Thread B] RegisterDragDrop     -> SUCCESS [0x00000000 : S_OK]
[Thread A] FINISHED
```

### Windows11 25H2
```
~~~ Step 1. OleInitialize makes Thread A STA ~~~
[Thread A] STARTED
[Thread A] =====< State: UNINITIALIZED / UNKNOWN (hr=0x800401F0) >=====
[Thread A] OleInitialize        -> SUCCESS [0x00000000 : S_OK]
[Thread A] =====< Type: Main STA (0x3), Qual: None (0x0) >=====

~~~ Step 2. COM reinitialization forces Thread A to become MTA ~~~
[Thread A] CoUninitialize
[Thread A] CoInitializeEx(MTA)  -> SUCCESS [0x00000000 : S_OK]
[Thread A] =====< Type: MTA (0x1), Qual: None (0x0) >=====

~~~ Step 3. New Thread B is implicitly MTA ~~~
[Thread A] Window Created
[Thread A] Spawning Worker Thread...
[Thread B] STARTED
[Thread B] =====< Type: MTA (0x1), Qual: Implicit MTA (0x1) >=====

~~~ Step 4. Can Thread B use OLE? ~~~
	Windows:	YES.
	Wine:		NO.
[Thread B] Attempting RegisterDragDrop (No Init)...
[Thread B] RegisterDragDrop     -> SUCCESS [0x00000000 : S_OK]

~~~ Extra Step. OLE is not initialized in thread B, so it can be initialized now.  ~~~
[Thread B] OleInitialize        -> SUCCESS [0x00000000 : S_OK]
[Thread B] =====< Type: Main STA (0x3), Qual: None (0x0) >=====
[Thread B] OleInitialize        -> SUCCESS [0x00000001 : S_FALSE]
[Thread B] =====< Type: Main STA (0x3), Qual: None (0x0) >=====
[Thread B] Attempting RegisterDragDrop (After Init)...
[Thread B] RegisterDragDrop     -> FAILED [0x80040101 : DRAGDROP_E_ALREADYREGISTERED]
[Thread A] FINISHED
```
