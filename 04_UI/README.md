## 処理の流れ

1. **UiManager** — [`UiManager.h`](01_UIManager/UiManager.h) / [`UiManager.cpp`](01_UIManager/UiManager.cpp)  
   UI用のDescriptorHeap・RTV・PSOを生成し、IUIScreen単位で画面を管理

2. **UI / IUIScreen** — [`UI.h`](02_UIFoundation/UI.h) / [`UI.cpp`](02_UIFoundation/UI.cpp) / [`IUIScreen.h`](02_UIFoundation/IUIScreen.h)  
   UIパーツの生成・描画・入力判定・スクロール・テキストなどの共通機能を提供

3. **UIGamePlayScreen** — [`UIGamePlayScreen.h`](03_GamePlayScreen/UIGamePlayScreen.h) / [`UIGamePlayScreen.cpp`](03_GamePlayScreen/UIGamePlayScreen.cpp)  
   TopBar / UnderBar / ResidentPanel / BuildPanelなどのUIパーツをまとめて管理

4. **UIGamePlayScreenResidentPanel** — [`UIGamePlayScreenResidentPanel.h`](04_ResidentPanel/UIGamePlayScreenResidentPanel.h) / [`UIGamePlayScreenResidentPanel.cpp`](04_ResidentPanel/UIGamePlayScreenResidentPanel.cpp)  
   CharacterSystemと同期して住人UIを動的生成し、選択・スクロール・職業変更を処理 
