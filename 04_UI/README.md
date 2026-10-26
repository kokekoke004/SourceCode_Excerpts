処理の流れ

1. UiManager  
   UI用のDescriptorHeap・RTV・PSOを生成し、IUIScreen単位で画面を管理

2. UI / IUIScreen  
   UIパーツの生成・描画・入力判定・スクロール・テキストなどの共通機能を提供

3. UIGamePlayScreen  
   TopBar / UnderBar / ResidentPanel / BuildPanelなどのUIパーツをまとめて管理

4. UIGamePlayScreenResidentPanel  
   CharacterSystemと同期して住人UIを動的生成し、選択・スクロール・職業変更を処理 