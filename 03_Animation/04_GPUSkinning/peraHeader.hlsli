
//バーテックスバッファーの入力
struct VSSkinIn
{
    float4 Pos : POSITION; //位置   
    float4 Norm : NORMAL; //頂点法線
    float2 Tex : TEXCOORD; //テクスチャー座標
    float3 tangent : TANGENT; //接ベクトル
    float3 biNormal : BINORMAL; //従ベクトル
    float4 Bones : BONE_INDEX; //ボーンのインデックス
    float4 Weights : BONE_WEIGHT; //ボーンの重み
    uint InstanceID : SV_InstanceID; //インスタンス描画用
};



struct Output
{
    float4 svpos : SV_POSITION;
    float2 uv : TEXCOORD;
};

struct PixelOutput  //G-Buffer書き出しの順番指定
{
    float4 col : SV_TARGET0;        //gBuffer1つ目(通常のレンダリング結果)
    float4 normal : SV_TARGET1;     //gBuffer2つ目(法線)
    float4 specular : SV_TARGET2;   //gBuffer スペキュラ情報
    float4 highLum : SV_TARGET3;    //gBuffer3つ目(高輝度)
};

struct BlurOutput
{
    float4 highLum : SV_TARGET0;    //gBuffer高輝度
    float4 col : SV_TARGET1;        //gBuffer通常のレンダリング    
};
