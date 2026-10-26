
// 読み取り・書き込み用（インデックス付き）
//出力データ
RWStructuredBuffer<uint> outPut : register(u0);
//要素数カウント（こっちにも必ず値追加して正しい要素数になるようにする）
AppendStructuredBuffer<uint> OutBuffer : register(u1);


struct ObstacleData
{
    float3 position;
    float padding;
    float3 halfExtents; //[0]:width,[1]:height,[2]:depth
    float padding2;
    float4 rotQuat; //クォータニオン

};

// 読み取り・書き込み用
//障害物になりえるFBX
RWStructuredBuffer<ObstacleData> ObstaclesData : register(u2);

// 読み取り専用
//ランドスケープの高さ格納されてる
RWStructuredBuffer<float> heightData : register(u3);


//構造体
cbuffer Constant : register(b0)
{
    int flagCalcHeight;
    int widthPix; // 高さテクスチャ横ピクセル数
    int heightPix; //        〃          縦ピクセル数
    float scaleXZ; //1テクセル何メートルか
    float scaleY;
    float offsetX; // 地形の幅の半分
    float offsetZ; // 地形の奥行きの半分
    int gridNum; //パッチ数
    float patchSizeXZ; //１パッチのサイズ
    int obstacleCount; //障害物になりえる物合計数
    
    int chunkSize; //チャンクサイズ
    int cX; //チャンク座標＿X
    int cZ; //チャンク座標＿Z

};

float3 rotateByQuat(float3 v, float4 q)
{
    float3 t = 2.0 * cross(q.xyz, v);
    return v + q.w * t + cross(q.xyz, t);
}

float3 rotateInvByQuat(float3 v, float4 q)
{
    float4 qc = float4(-q.xyz, q.w); //conjugate
    return rotateByQuat(v, qc); //逆回転=共役で正方向回転
}

float bilerp(float a, float b, float c, float d, float fx, float fz)
{
    float u = lerp(a, b, fx);
    float v = lerp(c, d, fx);
    return lerp(u, v, fz);
}

float HeightAt(float3 worldPos)
{
    float worldW = gridNum * patchSizeXZ;
    float worldD = gridNum * patchSizeXZ;
    float u = (worldPos.x + offsetX) / worldW;
    float v = (worldPos.z + offsetZ) / worldD;

    float s = u * widthPix - 0.5;
    float t = v * heightPix - 0.5;
    int i = clamp((int) floor(s), 0, widthPix - 1);
    int j = clamp((int) floor(t), 0, heightPix - 1);
    float fx = s - floor(s);
    float fz = t - floor(t);

    int i1 = clamp(i + 1, 0, widthPix - 1);
    int j1 = clamp(j + 1, 0, heightPix - 1);

    float a = heightData[j * widthPix + i];
    float b = heightData[j * widthPix + i1];
    float c = heightData[j1 * widthPix + i];
    float d = heightData[j1 * widthPix + i1];

    return bilerp(a, b, c, d, fx, fz);
}


//配列outPutで０の部分が侵入不可能
//1の部分が侵入可能

[numthreads(16, 16, 1)]
void CSObstacleMapPerChunk(uint3 DTid : SV_DispatchThreadID)
{
    
    if (DTid.x >= widthPix || DTid.y >= heightPix)
        return;

    int pixOffsetW = widthPix / 2;
    int pixOffsetH = heightPix / 2;
        
    //ワールド座標をセル番号に変換
    float2 worldXZ = float2(cX * chunkSize, cZ * chunkSize);
    float2 gridF = worldXZ / scaleXZ + float2(pixOffsetW, pixOffsetH);

    int gX = int(round(gridF.x) + DTid.x);
    int gZ = int(round(gridF.y) + DTid.y);
    
    
    //チャンク座標を求めてハイトマップに対応するピクセルインデックス取得
    uint gIdx = gZ * widthPix + gX;
    if (gIdx >= heightPix * widthPix)
        return;

    
    float terrainY = heightData[gIdx];
    //float3 heightIdx = 0;
    //heightIdx.x = DTid.x*scaleXZ-offsetX;
    //heightIdx.z = DTid.y*scaleXZ-offsetZ;
    //float terrainY = HeightAt(heightIdx);
    
    //高さ計算とりあえずテクスチャピクセルの0,0に合わせたいから、純粋な幅でオフセット計算
    float offX = widthPix * 0.5;
    float offZ = heightPix * 0.5;
    
    float worldW = gridNum * patchSizeXZ; // 3200
    
    //ピクセル座標から、ワールド座標へ
    //ワールド座標をピクセル座標にした手順とそのまま反対にワールド座標に直す
    //ピクセル座標にするとき、ピクセル座標の半分の大きさをオフセットとして足していたので引いてワールド座標に直す
    const float xWorld = ( (float)gX - (float)pixOffsetW ) * scaleXZ;
    const float zWorld = ( (float)gZ - (float)pixOffsetH ) * scaleXZ;
    uint result = 1;

    //セル中心
    float3 cellCenter = float3(xWorld, terrainY, zWorld);
    
    // セル半辺長
    float halfCell = scaleXZ * 0.5;

    for (int i = 0; i < obstacleCount; i++)
    {
        ObstacleData obs = ObstaclesData[i];

        float3 p = cellCenter - obs.position;
        
        //クォータニオンを適用
        float3 pLocal = rotateInvByQuat(p, obs.rotQuat);
        
        
        float3 heInflated = obs.halfExtents;

        //セル中心をOBBのローカル空間へ変換し、XZ平面で侵入判定
        bool insideXZ = (abs(pLocal.x) <= heInflated.x) && (abs(pLocal.z) <= heInflated.z);

        float minY = obs.position.y - obs.halfExtents.y;
        float maxY = obs.position.y + obs.halfExtents.y;
       
        float climbLimitMax = terrainY + 3.5f;
        float climbLimitMin = terrainY;

        //高さ判定：判定結果厳しすぎてすこしでも地面から浮いていれば侵入可能になるから、とりあえずXZ軸を見てコリジョンあれば侵入不可にする
        bool overlapY = (climbLimitMax >= minY && climbLimitMin <= maxY);
        
        if (insideXZ)
        {
            //コリジョンマッピングする
            result = 0;
            break;
        }
    }

    //配列に格納する専用インデックス
    int idx = DTid.y * chunkSize + DTid.x;
    if (idx >= chunkSize * chunkSize)
        return;
    
    outPut[idx] = result;
    OutBuffer.Append(idx);
}