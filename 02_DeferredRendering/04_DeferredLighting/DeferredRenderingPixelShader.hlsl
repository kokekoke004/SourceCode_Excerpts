#include"peraHeader.hlsli"

#define kMaxBones 255
#define LIGHT_TYPE_POINT 0
#define LIGHT_TYPE_SPOT  1

//G-Buffer///////////////
Texture2D<float4> tex : register(t0); //通常テクスチャ
Texture2D<float4> normalTex : register(t1); //法線テクスチャ
Texture2D<float4> specular : register(t2); //スペキュラ
Texture2D<float4> texHighLum : register(t4); //高輝度テクスチャ
Texture2D<float4> texShrinkHighLum : register(t5); //縮小バッファー高輝度
Texture2D<float4> texShrink : register(t6); //縮小バッファー通常

/////////////////////////
Texture2D<float4> distTex : register(t7); //ノーマルマップを用いたポストエフェクト用
Texture2D<float> depthTex : register(t8); //深度情報
Texture2D<float> lightDepthTex : register(t9); //深度情報ライトから見たもの
Texture2D<float> texSSAO : register(t10); //アンビエントオクルージョン

SamplerState smp : register(s0); //サンプラー
SamplerComparisonState shadowSmp : register(s1);

struct LightDesc
{
    //x, y, z : ライト位置
    //w       : ライトタイプ
    float4 positionType; //ライトの位置
    
    //x, y, z : ライト色
    //w       : 光の強さ
    float4 colorIntensity; //ライトの色と強度
    
    //x : 影響範囲
    //y, z, w : 予備
    float4 rangePadding; //ライトの範囲とパディング
};

//StructuredBufferこれを用いることで可変長になっている
StructuredBuffer<LightDesc> g_InstanceLightDescData : register(t13);


cbuffer global : register(b2)
{
    float4 g_DirLight; //ライトの方向ベクトル
    float4 g_lightColor; //ライトの色
    float4 g_vEye; //カメラ位置	
    
    
    float4 g_groundColor; //地面の色
    float4 g_skyColor; //天球の色
    float4 g_groundNormal; //地面の法線
    
    matrix g_lightCamera; //ライトからみたビュー
    matrix g_invProj; //逆プロジェクション行列
    
    int lightCount = 0; //設置しているポイントライトなどのライトのカウント
    int3 pad; //パディング


};

struct LightingOutput  //G-Buffer書き出しの順番指定
{
    float4 sceneColor : SV_TARGET0; //gBuffer1つ目(通常のレンダリング結果)
    float4 highLum : SV_TARGET1; //gBuffer2つ目(高輝度)
};

//////////////////////////////////////////
//関数宣言
//Lambert拡散反射光を計算する
float3 calcLambertDiffuse(float3 lightDirection, float3 lightColor, float3 normaal);
//Phong鏡面反射光
float3 calcPhongSpecular(float3 lightDirection, float3 lightColor, float3 worldPos, float3 normal);
//半球ライト
float3 hemisphereLight(float3 normal, float3 groundNormal);
//ポイントライト計算
float3 calcPointLight(LightDesc light,float3 worldPos,float3 normal,float3 specularColor,float specuPower);
//影計算
float CalcShadow(float4 worldPos);
//////////////////////////////////////////

LightingOutput DeferredRendering(Output input) : SV_TARGET
{
    LightingOutput output;
    
    float dp = depthTex.Sample(smp, input.uv); //現在のUVの深度
    
    //背景や見描画ピクセルであればライティングをしない
    if (dp >= 0.9999999f)
    {
        output.sceneColor = float4(g_skyColor.rgb, 1.0f);
        output.highLum = float4(0.0f, 0.0f, 0.0f, 1.0f);
        return output;
    }
    
    //元の座標を復元する
    //逆プロジェクション行列を、input.uv * float2(2, -2) + float2(-1, 1)これと、dp, 1にかける事で、もとの座標が求められる
    float4 worldPos = mul(g_invProj, float4(input.uv * float2(2, -2) + float2(-1, 1), dp, 1));
    //もとの座標を求めたら、必ずwでxyzを割る
    worldPos.xyz = worldPos.xyz / worldPos.w;

    worldPos.w = 1.0f;
    
    float4 tpos = mul(worldPos, g_lightCamera);

    
    //アルベド（色情報）
    //1.5は明るくなるように補正してる
    float4 albedo = tex.Sample(smp, input.uv);
    
    
    //法線（G-Bufferから復元）
    float4 normalTexVal = normalTex.Sample(smp, input.uv);
    float3 normal = normalize(normalTexVal.rgb * 2.0f - 1.0f);

    //ライト方向とライティング計算
    float3 LightDir = normalize(-g_DirLight); //ライトの方向ベクトル正規化してる
    //ピクセルが受けている光を求める
    //拡散反射光
    float3 diffuseLig = calcLambertDiffuse(LightDir, g_lightColor.xyz, normal);
    
    //鏡面反射光を求める
    float3 specularLig = calcPhongSpecular(LightDir, g_lightColor.xyz, worldPos.xyz, normal);

    //半球ライトの計算------------------------------
    float3 hemiLight = hemisphereLight(normal, g_groundNormal.xyz);
    //-----------------------------------------------

    
    //スペキュラマップからスペキュラ反射の強さをサンプリング
    float specIntensity= specular.Sample(smp, input.uv).r;
    
    //鏡面反射の強さを鏡面反射光に乗算する
    specularLig *= specIntensity * 5.0f;
    
    //影係数を計算
    float shadow = CalcShadow(worldPos);
    
    //拡散反射光と鏡面反射光を足し算して、最終的な光を求める
    float3 lig = (diffuseLig + specularLig) * shadow;

    //最終的な光を求める
    float3 finalLig = lig + hemiLight;

    
    for (uint i = 0; i < lightCount; i++)
    {
        LightDesc light = g_InstanceLightDescData[i];
        //ライトのタイプを表すwに対応するものを取得する
        uint lightType = (uint) light.positionType.w;
        if (lightType == LIGHT_TYPE_POINT)
        {
            finalLig += calcPointLight(light, worldPos.xyz, normal, g_lightColor.xyz, specularLig);
        }
    }
        
    float3 litColor = albedo.rgb * finalLig;

    output.sceneColor = float4(litColor, albedo.a);

    //litColorを数値の割合で掛けて足す
    float brightness = dot(litColor, float3(0.2126f, 0.7152f, 0.0722f));
    //輝度抽出の閾値
    float threshold = 1.0f;
    float bloomStrength = 1.0f;
    //ブルームの量
    float bloomAmount = saturate((brightness - threshold) / threshold);

    if (brightness > threshold)
    {
        //輝度が高いほどブルーム強くなる
        output.highLum = float4(litColor * bloomAmount * bloomStrength, 1.0f);
        
    }
    else
    {
        output.highLum = float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    
    
    
    return output;
}

//Lambert拡散反射光を計算する
float3 calcLambertDiffuse(float3 lightDirection, float3 lightColor, float3 normaal)
{
    //ピクセルの法線とライトの方向ベクトルの内積計算
    //内積の結果に -１を乗算する
    float t = dot(normaal, lightDirection) * (-1.0f);
    //内積の結果が0以下なら0にする
    t = max(0.0f, t);
    //ピクセルが受けている光を求める
    //拡散反射光
    return lightColor * t;

}

//Phong鏡面反射光
//ライトの方向
//ライトのカラー
//サーフェイスの位置
//サーフェイスの法線
float3 calcPhongSpecular(float3 lightDirection, float3 lightColor, float3 worldPos, float3 normal)
{
        //反射ベクトルを求める
    float3 refVec = reflect(lightDirection, normal);
    
    //光が当たったサーフェースから、視点に伸びるベクトルを計算する
    float3 toEye = g_vEye.xyz - worldPos;
    
    //正規化する
    toEye = normalize(toEye);
    
    //鏡面反射の強さを求める
    //dot関数を利用して、refVecとtoEyeの内積を求める
    float t = dot(refVec, toEye);
    
    //内積の結果は、マイナスになるので、マイナスの場合は０にする
    //0.0以下であれば0.0にして、0.0以上であればtを取る
    t = max(0.0f, t);
    
    //鏡面反射の強さを絞る
    t = pow(t, 5.0f);
    
    //鏡面反射光を求める
    return lightColor * t;

}

//半球ライト
float3 hemisphereLight(float3 normal, float3 groundNormal)
{
    //半球ライトの計算------------------------------
    //サーフェイスの法線と地面の法線との内積計算
    float t = dot(normal, groundNormal);
    
    //内積の結果を0～1の範囲に変換する
    t = (t + 1.0f) / 2.0f;
    
    //地面の色と天球色を補完率tで線形補完する
    return lerp(g_groundColor.xyz, g_skyColor.xyz, t);

}

float3 calcPointLight(LightDesc light, float3 worldPos, float3 normal, float3 specularColor, float specularLig)
{
    float3 lightPos = light.positionType.xyz;
    float3 lightColor = light.colorIntensity.xyz;
    float intensity = light.colorIntensity.w;
    float range = light.rangePadding.x;

    //ピクセルからライトへのベクトル
    float3 toLightVec = lightPos - worldPos;
    float distanceToLight = length(toLightVec);

    //範囲外なら光らせない
    if (distanceToLight >= range)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    //ライトの方向を正規化する
    float3 toLight = normalize(toLightVec);

    //距離減衰
    //ライトに近いほど1、rangeに近づくほど0
    float attenuation = saturate(1.0f - distanceToLight / range);

    //少し自然に落ちるように二乗
    attenuation *= attenuation;

    //拡散反射
    float NdotL = max(dot(normal, toLight), 0.0f);
    float3 diffuse = lightColor * NdotL * intensity * attenuation;

    //鏡面反射
    float3 toEye = normalize(g_vEye.xyz - worldPos);

    //Blinn-Phong
    float3 halfVec = normalize(toLight + toEye);
    float spec = max(dot(normal, halfVec), 0.0f);
    spec = pow(spec, 16.0f);

    float3 specularLight = lightColor * spec * intensity * attenuation;

    //すぺきゅらマップがあればそれに合わせて反射強度を変える
    specularLight *= specularLig * 5.0f;

    return diffuse + specularLight;
}

float CalcShadow(float4 worldPos)
{
    //現在のピクセルのワールド座標をライト空間に変換
    float4 lightClipPos = mul(worldPos, g_lightCamera);
    //wで割って-1から1　/  0から1の座標にする
    float3 posFromLightVP = lightClipPos.xyz / lightClipPos.w;
    
    //シャドーマップ用のUVに変換
    float2 shadowUV;
    //uvは０から１で扱うからここで-1は0、0は0.5、1は1という感じ
    shadowUV.x = posFromLightVP.x * 0.5f + 0.5f;
    shadowUV.y = posFromLightVP.y * -0.5f + 0.5f;

    //ライトから見たカメラ横と縦担当（影を生成する範囲にスクリーンのピクセルからワールド座標にした座標が含まれるかを見る）
    bool isUVOutOfRange = shadowUV.x < 0.0f || shadowUV.x > 1.0f ||
                          shadowUV.y < 0.0f || shadowUV.y > 1.0f;
    //ライトから見たカメラ奥行担当（影を生成する範囲にスクリーンのピクセルからワールド座標にした座標が含まれるかを見る）
    bool isDepthOutOfRange = posFromLightVP.z < 0.0f || posFromLightVP.z > 1.0f;
    
    if (isUVOutOfRange || isDepthOutOfRange)
    {
        return 1.0f;
    }
    
    float bias = 0.0003f;

    //現在の深度よりシャドーマップの深度が大きければ、ライトから見えている/影ではない
    //この逆であれば手前に別の物体があるため陰になる
    float depthFromLight = lightDepthTex.SampleCmp(
    shadowSmp,
    shadowUV,
    posFromLightVP.z - bias);
    
    //影の濃さとして返却
    return lerp(0.5f, 1.0f, depthFromLight);
}