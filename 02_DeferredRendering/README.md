処理の流れ

1. Skin.hlsl  
   モデル描画時にAlbedo / Normal / Specular等をG-Bufferへ出力

2. PostProcessPass  
   G-Bufferの生成・RTV/SRV管理・リソースステート遷移

3. PostProcessBuilder  
   ディファードレンダリング用PSO / RootSignatureを構築

4. DeferredRenderingPixelShader.hlsl  
   G-Bufferを参照してライティング・シャドウ計算を実行 