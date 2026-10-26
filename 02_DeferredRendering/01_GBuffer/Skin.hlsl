#include"peraHeader.hlsli"
//定義
#define kMaxBones 256
#define instanceMax 100


//グローバル
//テクスチャーは レジスターt(n)
Texture2D g_texColor: register(t5);     //色情報のテクスチャ
Texture2D<float4> g_TexNormal : register(t6); //ノーマルマップのテクスチャ
Texture2D<float4> g_Texspecular : register(t7);
Texture2D<float> lighatDepthTex : register(t8); //シャドウマップ用ライト深度テクスチャ


SamplerState g_samLinear : register(s0);//サンプラーはレジスターs(n)
SamplerComparisonState shadowSmp : register(s1);

cbuffer global : register(b0)
{
    matrix g_mW; //ワールド行列
	matrix g_view; //ビュー
    matrix g_proj; //プロジェクション
    matrix g_invProj; //逆プロジェクション行列
    matrix g_lightCamera; //ライトからみたビュー
	float4 g_DirLight;  //ライトの方向ベクトル
    float4 g_lightColor;   //ライトの色
	float4 g_vEye;//カメラ位置	
    
    
    float4 g_groundColor; //地面の色
    float4 g_skyColor; //天球の色
    float4 g_groundNormal; //地面の法線

	float4 g_Ambient;//アンビエント光
	float4 g_Diffuse; //拡散反射(色）
	float4 g_Specular;//鏡面反射
	matrix g_mConstBoneWorld[kMaxBones];
    
    //1=true , 0=false
    int g_texDiffuseFlag;  //diffuseのテクスチャがあるかのフラグ
    float3 pad1; //パディング1
    int g_texNormalFlag; //normalのテクスチャがあるかのフラグ
    float3 pad2; //パディング2
    int g_texSpecularFlag; //specularのテクスチャがあるかのフラグ
    float3 pad3; //パディング3

};

//FBXのアニメーションは、ストラクチャーバッファでGPUに全フレームでボーン毎のアニメーションデータを一括で一度渡す
//InstanceDataこれや、AnimationInfoこれの要素を基に、再生するアニメーションのキーフレームにアクセスして
//アニメーションを再生していく

struct InstanceData
{
    float4x4 world; //インスタンス描画の座標
    uint idxInsGPU; //GPUでのインデックス
    uint animationId; //どのアニメーションを再生するかのアニメーションID
    uint frameIndex;  //アニメーションのフレーム番号
    uint pad1;
};

struct AnimationInfo
{
    uint offsetAnim; //アニメーションのオフセット
    uint frameCount; //そのアニメーションのフレーム数
    uint pad1;
    uint pad2;
};

struct InstanceDrawUpdateData
{
    int updateidx;
    uint animationId; //どのアニメーションを再生するかのアニメーションID
    uint frameIndex; //アニメーションのフレーム番号
    int pad3;

    matrix world; //ワールド行列
};

struct InstanceDrawIdxGPU
{
    int drawIdxGPU;
    int pad1;
    int pad2;
    int pad3;
};

//StructuredBufferこれを用いることで可変長になっている
StructuredBuffer<InstanceData> g_InstanceData : register(t0);

StructuredBuffer<InstanceDrawUpdateData> g_InstanceDrawUpdateData : register(t1);

StructuredBuffer<InstanceDrawIdxGPU> g_InstanceDrawIdxGPU : register(t2);


//アニメーションのボーン・キーフレーム毎情報を持つ
StructuredBuffer<float4x4> gAllBonePalettes : register(t3);

//アニメーションにアクセスするための、キーフレームなどを持つ
StructuredBuffer<AnimationInfo> gAnimationInfo : register(t4);


cbuffer g_LightInfo : register(b1)
{
    float4 g_ptPosLight; //ポイントライトの位置
    float4 g_ptColor; //ポイントライトの色
    float g_range;  //ポイントライトの影響範囲
}


//スキニング後の頂点・法線が入る
struct Skin
{
	float4 Pos;
	float3 Norm;
    float3 tangent; //接ベクトル
    float3 biNormal; //従ベクトル

};
//ピクセルシェーダーの入力（バーテックスバッファーの出力）　

struct PSSkinIn
{
    float4 ClipPos : SV_Position; //クリップ空間座標（レンダリング用）(カメラのこと)
    float4 WorldPos : TEXCOORD1;  //ワールド空間座標（ライティング計算用）
    float4 tpos : TPOS;
    float3 Norm : NORMAL;//頂点法線
	float2 Tex	: TEXCOORD0;//テクスチャー座標(UV)
    float3 tangent : TANGENT; //接ベクトル
    float3 biNormal : BINORMAL; //従ベクトル
	float4 Color : COLOR0;//最終カラー（頂点シェーダーにおいての）
    uint InstanceIndex : TEXCOORD2;
};




//
//指定した番号のボーンのポーズ行列を返す　サブ関数（バーテックスシェーダーで使用）
matrix FetchBoneMatrix(uint animId, uint frameIdx,uint iBone)
{
    if (iBone >= kMaxBones)
    {
        matrix Identity =
        {
            { 1, 0, 0, 0 },
            { 0, 1, 0, 0 },
            { 0, 0, 1, 0 },
            { 0, 0, 0, 1 }
        };
        return Identity;
    }
	
    AnimationInfo animInfo = gAnimationInfo[animId];
    //アニメーションフレームをマイナス１する
    uint fi = (animInfo.frameCount > 0) ? min(frameIdx, animInfo.frameCount - 1) : 0;
    uint frameOffset = animInfo.offsetAnim + fi;
    //１フレーム分に並んでいるボーン行列数（kMaxBones）を掛ける
    uint baseIdx = frameOffset * kMaxBones;
    return gAllBonePalettes[baseIdx + iBone];
}

//
//頂点をスキニング（ボーンにより移動）する。サブ関数（バーテックスシェーダーで使用）
//頂点がボーンから受ける影響を計算し、頂点座標とUVに反映してる
Skin SkinVert(VSSkinIn Input)
{
    uint index = Input.InstanceID;
    InstanceDrawIdxGPU insIdx = g_InstanceDrawIdxGPU[index];
    index = insIdx.drawIdxGPU;

    InstanceData inst = g_InstanceData[index];
    AnimationInfo animInfo = gAnimationInfo[inst.animationId];

    if (animInfo.frameCount == 0)
    {
        Skin Out;
        Out.Pos = Input.Pos;
        Out.Norm = Input.Norm;
        Out.tangent = Input.tangent;
        Out.biNormal = Input.biNormal;
        return Out;
    }
    
    Skin Output = (Skin) 0;
    Input.Norm.w = 0;
    float4 Pos = Input.Pos;
    float3 Norm = Input.Norm;
    float3 tangent = Input.tangent;
    float3 biNormal = Input.biNormal;
	//ボーン0
    uint iBone = Input.Bones.x;
    float fWeight = Input.Weights.x;
    matrix m = FetchBoneMatrix(inst.animationId, inst.frameIndex, iBone);
    Output.Pos += fWeight * mul(Pos, m);
    Output.Norm += fWeight * mul(Norm, (float3x3) m); 
    Output.tangent += fWeight * mul(tangent, (float3x3) m);
    Output.biNormal += fWeight * mul(biNormal, (float3x3) m);

	//ボーン1
    iBone = Input.Bones.y;
    fWeight = Input.Weights.y;
    m = FetchBoneMatrix(inst.animationId, inst.frameIndex, iBone);
    Output.Pos += fWeight * mul(Pos, m);
    Output.Norm += fWeight * mul(Norm, (float3x3) m);
    Output.tangent += fWeight * mul(tangent, (float3x3) m);
    Output.biNormal += fWeight * mul(biNormal, (float3x3) m);

	//ボーン2
    iBone = Input.Bones.z;
    fWeight = Input.Weights.z;
    m = FetchBoneMatrix(inst.animationId, inst.frameIndex, iBone);
    Output.Pos += fWeight * mul(Pos, m);
    Output.Norm += fWeight * mul(Norm, (float3x3) m);
    Output.tangent += fWeight * mul(tangent, (float3x3) m);
    Output.biNormal += fWeight * mul(biNormal, (float3x3) m);

	//ボーン3
    iBone = Input.Bones.w;
    fWeight = Input.Weights.w;
    m = FetchBoneMatrix(inst.animationId, inst.frameIndex, iBone);
    Output.Pos += fWeight * mul(Pos, m);
    Output.Norm += fWeight * mul(Norm, (float3x3) m);
    Output.tangent += fWeight * mul(tangent, (float3x3) m);
    Output.biNormal += fWeight * mul(biNormal, (float3x3) m);


    return Output;
}
//
//バーテックスシェーダーPSSkinIn output
PSSkinIn VS(VSSkinIn input)
{
	PSSkinIn output;
	Skin vSkinned = SkinVert(input);
    
    
    uint index = input.InstanceID;
    
    InstanceDrawIdxGPU insIdx= g_InstanceDrawIdxGPU[index];
    index = insIdx.drawIdxGPU;
    //int f = insIdx.drawIdxGPU;


   // float dummyUsage = f * 0.0;
    
    output.InstanceIndex = index;
    InstanceData instance = g_InstanceData[index];
    float4x4 matBone = gAllBonePalettes[0];
    
    output.WorldPos = mul(vSkinned.Pos, instance.world); // ワールド行列で位置を変換
    //output.WorldPos.x += dummyUsage * 0.0f;
    //インスタンスインデックスの影描画フラグが真ならば、影の描画
    //if (instance.flagShadow)
    //{
        //output.WorldPos = mul(instance.shadow, output.WorldPos); // ワールド行列で位置を変換
    //}
    
    output.ClipPos = mul(output.WorldPos, mul(g_view, g_proj)); // カメラ、射影変換（ワールド座標×ビュー×プロジェクション）
    //output.ClipPos = mul(output.WorldPos, mul(g_lightCamera, g_proj)); // カメラ、ライト目線、平行投影なし
    //output.ClipPos = mul(output.WorldPos, g_lightCamera); // カメラ、ライト目線
    output.tpos = mul(output.WorldPos, g_lightCamera);
    output.Norm = normalize(mul(vSkinned.Norm, (float3x3) instance.world)); //Normalをワールド座標を用いて回転させてる
	output.Tex = input.Tex;    
    
    //接ベクトルと従ベクトルをワールド座標に変換する
    output.tangent = normalize(mul((float3x3) instance.world, vSkinned.tangent));
    output.biNormal = normalize(mul((float3x3) instance.world, vSkinned.biNormal));

    output.Color = g_Diffuse; //頂点が持っている色
	return output;
}


//影用頂点座標変換
//lightDepthBufferに書き込み
float4 ShadowVS(VSSkinIn Input) : SV_POSITION
{
    Skin vSkinned = SkinVert(Input);
    
    uint index = Input.InstanceID;
    InstanceDrawIdxGPU insIdx = g_InstanceDrawIdxGPU[index];
    index = insIdx.drawIdxGPU;

    InstanceData instance = g_InstanceData[index];

    
    float4 worldPos = mul(vSkinned.Pos, instance.world); // ワールド行列で位置を変換
    
    float4 shadow = mul(worldPos, g_lightCamera);
    //float4 shadow = mul(worldPos, mul(g_view, g_proj));
    return shadow;
    //return mul(g_lightCamera,Output.Pos);
}


//
// float4 PSSkin(PSSkinIn input) : SV_Target
//ピクセルシェーダー
PixelOutput PS(PSSkinIn input) : SV_Target
{
    uint index = input.InstanceIndex;
    InstanceData instance = g_InstanceData[index];
    //if (instance.flagShadow)
    //{
        //インスタンスインデックスの影描画フラグが真ならば、影の描画
        //return float4(0, 0, 0, 1);
    //}
    
    float3 normal = input.Norm;
    
    //ノーマルマップがあれば
    if (g_texNormalFlag)
    {
         //法線マップからタンジェントスペースの法線をサンプリングする
        float3 localNormal = g_TexNormal.Sample(g_samLinear, input.Tex).xyz;
        //タンジェントスペースの法線を０～１の範囲から、ー１～１の範囲に復元する
        localNormal = (localNormal - 0.5f) * 2.0f;
    
        localNormal = normalize(localNormal);
        
        //タンジェントスペースの法線をワールドスペースに変換する
        normal = input.tangent * localNormal.x
        + input.biNormal * localNormal.y
        + normal * localNormal.z;
        
        //normal = normalize(normal);
        
        //return float4(normal.xyz * 0.5 + 0.5, 1);

    }
    
        
    
    //アンビエントライト（環境光）
    //lig.x += 0.3f;
    //lig.y += 0.3f;
    //lig.z += 0.3f;
    
    // スペキュラマップ
    float3 specularValue = (0.0, 0.0, 0.0);
    if (g_texSpecularFlag)
    {
        specularValue = g_Texspecular.Sample(g_samLinear, input.Tex).xyz;
    }
    else
    {
        //マップが無ければ、マテリアルのスペキュラ定数の輝度をフォールバックに
        specularValue = dot(g_Specular.rgb, float3(0.299f, 0.587f, 0.114f));
    }
    

	float4 TexDiffuse = g_texColor.Sample(g_samLinear, input.Tex);
    float4 color = input.Color * TexDiffuse;    //頂点が持っている色＋テクスチャから得た色
        
    PixelOutput output;
    //output.col = float4(color.rgb * shadowWeight, color.a); //gBuffer1つ目に色情報渡す
    //output.col = float4(color.rgb , color.a); //gBuffer1つ目に色情報渡す

    //ディファードレンダリングで使う
    output.col = color;
    output.normal.rgb = float3((normal + 1.0f)/2.0f); //gBuffer2つ目に法線情報を渡す
    output.normal.a = 1;
    
    output.specular = float4(specularValue,1.0f);

    //色を白黒に近い色にして返却する
    float y = dot(float3(0.299f, 0.587f, 0.114f), output.col.xyz);
    output.highLum = y > 0.99f ? output.col : 0.0f;
    
    //深度がライトから見た座標より後ろなら影にする
    //return (color);
	return  output;
    //return float4(input.Norm * 0.5 + 0.5, 1);
}
