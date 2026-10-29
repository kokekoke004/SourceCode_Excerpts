## 処理の流れ

1. **Skin.hlsl** — [`Skin.hlsl`](01_GBuffer/Skin.hlsl) / [`peraHeader.hlsli`](01_GBuffer/peraHeader.hlsli)  
   モデル描画時にAlbedo / Normal / Specular等をG-Bufferへ出力

2. **PostProcessPass** — [`PostProcessPass.h`](02_PostProcess/PostProcessPass.h) / [`PostProcessPass.cpp`](02_PostProcess/PostProcessPass.cpp)  
   G-Bufferの生成・RTV/SRV管理・リソースステート遷移

3. **PostProcessBuilder** — [`PostProcessBuilder.h`](03_Pipeline/PostProcessBuilder.h) / [`PostProcessBuilder.cpp`](03_Pipeline/PostProcessBuilder.cpp)  
   ディファードレンダリング用PSO / RootSignatureを構築

4. **DeferredRenderingPixelShader.hlsl** — [`DeferredRenderingPixelShader.hlsl`](04_DeferredLighting/DeferredRenderingPixelShader.hlsl)  
   G-Bufferを参照してライティング・シャドウ計算を実行 
