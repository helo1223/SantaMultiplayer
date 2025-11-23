# 🎅 SantaNet

**SantaNet** is a multiplayer mod for **Santa Claus in Trouble (HD)**.  
It is currently in early development — so far only basic multiplayer functionality has been implemented.

---

## Components
- **SantaClient** — Builds the `.ASI` (DLL) file that you inject into `SantaClausInTrouble.exe`
- **SantaNET** — Dedicated server launcher that handles networking & data sync between clients

---

## Build Requirements
- **Visual Studio 2022**
- **DirectX 9 SDK (June 2010)** — https://www.microsoft.com/en-us/download/details.aspx?id=6812

  Add this in *VC++ Directories → Include / Library directories*:  
  `C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Lib\x86`
- **C++14**
- **External Libraries**
  - `enet.lib` — http://enet.bespin.org/Downloads.html
  - `libMinHook.x86.lib` — https://github.com/TsudaKageyu/minhook
  - `steam_api.lib` — https://partner.steamgames.com/downloads/list

---

## How to Use
SantaClient is injected into the game using **Ultimate ASI Loader**:  
https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases

1. Build **SantaClient** (`Release / x86`)
2. Place **`dinput8.dll`** and **`SantaClient.dll`** next to `SantaClausInTrouble.exe`
3. Build and run **SantaNET** to host a multiplayer server
4. Launch the game 
