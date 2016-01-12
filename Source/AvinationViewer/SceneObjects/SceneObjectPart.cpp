// Fill out your copyright notice in the Description page of Project Settings.

#include "AvinationViewer.h"
#include "../Utils/AvinationUtils.h"
#include "SceneObjectPart.h"
#include "SceneObjectGroup.h"
#include "../AssetSubsystem/MeshAsset.h"
#include "../AssetSubsystem/SculptAsset.h"
#include "../AssetSubsystem/AssetCache.h"
#include "../AssetSubsystem/AssetDecode.h"
#include "../AssetSubsystem/LLSDMeshDecode.h"
#include "../AssetSubsystem/J2KDecode.h"
#include <cstdlib>
#include "Base64.h"
#include "../Meshing/PrimMesher.h"
#include "openjpeg.h"

#define CULLDIST 12800.0

SceneObjectPart::SceneObjectPart()
{
    isPrim = isMesh = isSculpt = false;
    haveAllAssets = false;
    numFaces = 0;
    primData = 0;
    sculptData = 0;
}

SceneObjectPart::~SceneObjectPart()
{
    if (primData)
        delete primData;
    if (sculptData)
        delete sculptData;
}

float SceneObjectPart::ReadFloatValue(rapidxml::xml_node<> *parent, const char *name, float def)
{
    rapidxml::xml_node<> *node = parent->first_node(name);
    if (!node)
        return def;
    if (!node->value())
        return def;
    return atof(node->value());
}

int SceneObjectPart::ReadIntValue(rapidxml::xml_node<> *parent, const char *name, int def)
{
    rapidxml::xml_node<> *node = parent->first_node(name);
    if (!node)
        return def;
    if (!node->value())
        return def;
    return atoi(node->value());
}

FString SceneObjectPart::ReadStringValue(rapidxml::xml_node<> *parent, const char *name, FString def)
{
    rapidxml::xml_node<> *node = parent->first_node(name);
    if (!node)
        return def;
    if (!node->value())
        return def;
    return FString(node->value());
}

bool SceneObjectPart::Load(rapidxml::xml_node<> *xml)
{
    rapidxml::xml_node<> *uuidNode = xml->first_node("UUID");
    rapidxml::xml_node<> *innerUuidNode = uuidNode->first_node();

    FGuid::Parse(FString(innerUuidNode->value()), uuid);

    rapidxml::xml_node<> *shapeNode = xml->first_node("Shape");
    if (!shapeNode)
        return false;
    rapidxml::xml_node<> *sculptTypeNode = shapeNode->first_node("SculptType");
    rapidxml::xml_node<> *sculptTextureNode = shapeNode->first_node("SculptTexture");

    if (!sculptTypeNode || !sculptTextureNode)
        return false;

    innerUuidNode = sculptTextureNode->first_node();
    if (!innerUuidNode)
        return false;

    rapidxml::xml_node<> *textureEntryNode = shapeNode->first_node("TextureEntry");
    if (!textureEntryNode)
        return false;
    FString txe(textureEntryNode->value());

    FBase64::Decode(txe, textureEntry);

    TextureEntry::Parse(textureEntry, defaultTexture, textures);

    pathBegin = ReadFloatValue(shapeNode, "PathBegin", 0);
    pathEnd = ReadFloatValue(shapeNode, "PathEnd", 0);
    pathRadiusOffset = ReadFloatValue(shapeNode, "PathRadiusOffset", 0);
    pathRevolutions = ReadFloatValue(shapeNode, "PathRevolutions", 0);
    pathScaleX = ReadFloatValue(shapeNode, "PathScaleX", 100);
    pathScaleY = ReadFloatValue(shapeNode, "PathScaleY", 100);
    pathShearX = ReadFloatValue(shapeNode, "PathShearX", 0);
    pathShearY = ReadFloatValue(shapeNode, "PathShearY", 0);
    pathSkew = ReadFloatValue(shapeNode, "PathSkew", 0);
    pathTaperX = ReadFloatValue(shapeNode, "PathTaperX", 0);
    pathTaperY = ReadFloatValue(shapeNode, "PathTaperY", 0);
    pathTwist = ReadFloatValue(shapeNode, "PathTiwst", 0);
    pathTwistBegin = ReadFloatValue(shapeNode, "PathTwistBegin", 0);
    profileBegin = ReadFloatValue(shapeNode, "ProfileBegin", 0);
    profileEnd = ReadFloatValue(shapeNode, "ProfileEnd", 0);
    profileHollow = ReadFloatValue(shapeNode, "ProfileHollow", 0);
    pathCurve = ReadIntValue(shapeNode, "PathCurve", 16);
    primFlags = ReadIntValue(shapeNode, "PathCurve", 0);


    physicsShapeType = ReadIntValue(shapeNode, "PathCurve", 0);

    ShouldColide = (physicsShapeType != 1);
    isPhantom = (((PrimFlags)primFlags & Phantom) != None);
    isPhysical = (((PrimFlags)primFlags & Physics) != None);

    profileShape = pstCircle;

    FString txt = ReadStringValue(shapeNode, "ProfileShape", "Square");
    if (txt == "Circle")
        profileShape = pstCircle;
    else if (txt == "Square")
        profileShape = pstSquare;
    else if (txt == "IsometricTriangle" || txt == "IsoTriangle")
        profileShape = pstIsometricTriangle;
    else if (txt == "EquilateralTriangle" || txt == "EqualTriangle")
        profileShape = pstEquilateralTriangle;
    else if (txt == "RightTriangle")
        profileShape = pstRightTriangle;
    else if (txt == "HalfCircle")
        profileShape = pstHalfCircle;

    hollowShape = hstSame;

    txt = ReadStringValue(shapeNode, "HollowShape", "Square");
    if (txt == "Same")
        hollowShape = hstSame;
    else if (txt == "Circle")
        hollowShape = hstCircle;
    else if (txt == "Square")
        hollowShape = hstSquare;
    else if (txt == "Triangle")
        hollowShape = hstTriangle;

    rapidxml::xml_node<> *scaleNode = xml->first_node("Scale");
    if (!scaleNode)
        return false;

    rapidxml::xml_node<> *x, *y, *z, *w;
    x = scaleNode->first_node("X");
    y = scaleNode->first_node("Y");
    z = scaleNode->first_node("Z");

    if (!x || !y || !z)
        return false;

    scale.X = atof(x->value());
    scale.Y = atof(y->value());
    scale.Z = atof(z->value());

    if (scale.X == 0)
        scale.X = 0.001;
    if (scale.Y == 0)
        scale.Y = 0.001;
    if (scale.Z == 0)
        scale.Z = 0.001;

    scale *= 100.0f;

    rapidxml::xml_node<> *rotationNode = xml->first_node("RotationOffset");

    if (!rotationNode)
        return false;

    x = rotationNode->first_node("X");
    y = rotationNode->first_node("Y");
    z = rotationNode->first_node("Z");
    w = rotationNode->first_node("W");

    if (!x || !y || !z || !w)
        return false;

    rotation.X = atof(x->value());
    rotation.Y = -atof(y->value());
    rotation.Z = atof(z->value());
    rotation.W = -atof(w->value());

    rotation.Normalize();

    rapidxml::xml_node<> *positionNode = xml->first_node("OffsetPosition");
    if (!positionNode)
        return false;

    x = positionNode->first_node("X");
    y = positionNode->first_node("Y");
    z = positionNode->first_node("Z");

    if (!x || !y || !z)
        return false;

    position.X = atof(x->value());
    position.Y = -atof(y->value());
    position.Z = atof(z->value());

    rapidxml::xml_node<> *groupPositionNode = xml->first_node("GroupPosition");
    if (!groupPositionNode)
        return false;

    x = groupPositionNode->first_node("X");
    y = groupPositionNode->first_node("Y");
    z = groupPositionNode->first_node("Z");

    if (!x || !y || !z)
        return false;

    groupPosition.X = atof(x->value());
    groupPosition.Y = -atof(y->value());
    groupPosition.Z = atof(z->value());

    sculptType = atoi(sculptTypeNode->value());

    if (sculptType == 0)
    {
        isPrim = true;
    }
    else if ((sculptType & 0x37) == 5) // Mesh
    {
        isMesh = true;
        meshAssetId = FString(innerUuidNode->value());
    }
    else
    {
        isSculpt = true;
        meshAssetId = FString(innerUuidNode->value());
    }

    pathShearX = pathShearX < 128 ? (float)pathShearX * 0.01f : (float)(pathShearX - 256) * 0.01f;
    pathShearY = pathShearY < 128 ? (float)pathShearY * 0.01f : (float)(pathShearY - 256) * 0.01f;
    pathBegin = (float)pathBegin * 2.0e-5f;
    pathEnd = 1.0f - pathEnd * 2.0e-5f;
    pathScaleX = (float)(pathScaleX - 100) * 0.01f;
    pathScaleY = (float)(pathScaleY - 100) * 0.01f;

    profileBegin = (float)profileBegin * 2.0e-5f;
    profileEnd = 1.0f - (float)profileEnd * 2.0e-5f;

    if (profileBegin < 0.0f)
        profileBegin = 0.0f;

    if (profileEnd < 0.02f)
        profileEnd = 0.02f;
    else if (profileEnd > 1.0f)
        profileEnd = 1.0f;

    if (profileBegin >= profileEnd)
        profileBegin = profileEnd - 0.02f;

    profileHollow = (float)profileHollow * 2.0e-5f;
    if (profileHollow > 0.95f)
        profileHollow = 0.95f;


    lodWanted = 2;
    maxLod = High;

    float a = scale.Size();
    float b = a;
    a *= 0.02;
    a = FMath::Atan(a) / 0.785;

    b *= 250.0;
    if (b > CULLDIST)
        b = CULLDIST;

    cullDistance = b;

    //adjust visual mesh maxlod
    if (isPrim && profileShape != pstCircle && profileShape != pstHalfCircle &&
        (ExtrusionType)pathCurve == etStraight && pathTwist == pathTwistBegin
        && (profileHollow == 0 || hollowShape != hstCircle))
    {
        // prims without lod
        lodWanted = 0;
        maxLod = Lowest;
    }
    else
    {
        if (a > .9)
            maxLod = Highest;
        else if (a > 0.4)
            maxLod = High;
        else if (a > 0.15)
            maxLod = Low;
        else
            maxLod = Lowest;
    }

//    maxLod = Highest;
    lodWanted = maxLod;
    return true;
}

void SceneObjectPart::FetchAssets()
{
    FGuid id;
    
    if (isPrim) // Prim
    {
        MeshPrim();
        // Prims need no shape assets
        group->CheckAssetsDone();
    }

    // TODO: Renable assets
    else if (isMesh) // Mesh
    {
        FGuid::Parse(meshAssetId, id);
        AssetFetchedDelegate d;
        d.BindRaw(this, &SceneObjectPart::MeshReceived);
        group->fetchLock.Lock();
        ++group->activeFetches;
        //UE_LOG(LogTemp, Warning, TEXT("Fetches now %d"), group->activeFetches);
        group->fetchLock.Unlock();
        AssetCache::Get().Fetch<MeshAsset>(id, d);
    }
    else // Sculpt
    {
        // The mesh asset is here the sculpt texture
        FGuid::Parse(meshAssetId, id);
        AssetFetchedDelegate d;
        d.BindRaw(this, &SceneObjectPart::SculptReceived);
        group->fetchLock.Lock();
        ++group->activeFetches;
        //UE_LOG(LogTemp, Warning, TEXT("Fetches now %d"), group->activeFetches);
        group->fetchLock.Unlock();
        AssetCache::Get().Fetch<SculptAsset>(id, d);
    }
}

void SceneObjectPart::MeshReceived(FGuid id, TSharedAssetRef asset)
{
    TSharedRef<MeshAsset, ESPMode::ThreadSafe> mesh = StaticCastSharedRef<MeshAsset>(asset);
    
    if (asset->state == AssetBase::Failed)
        return;

    MeshMesh(mesh);
    
    haveAllAssets = true;
    
    group->fetchLock.Lock();
    --group->activeFetches;
    group->fetchLock.Unlock();
    
    group->CheckAssetsDone();
}


LLSDItem *SceneObjectPart::GetMeshData(TSharedRef<MeshAsset, ESPMode::ThreadSafe> assetdata, int lod)
{
    if (isMesh) // Mesh
    {
        if (lod > maxLod)
            lod = maxLod;

        FString lodLevel;
        switch ((LevelDetail)lodWanted)
        {
            case Highest:
                lodLevel = TEXT("high_lod");
                break;
            case High:
                lodLevel = TEXT("medium_lod");
                break;
            case Low:
                lodLevel = TEXT("low_lod");
                break;
            default:
                lodLevel = TEXT("lowest_lod");
                break;
        }
  
        
//        LLSDItem *lodData = LLSDMeshDecode::Decode(data.GetData(), lodLevel);
        LLSDItem *lodData = LLSDMeshDecode::Decode(assetdata->meshData.GetData(), TEXT("high_lod"));
        
        numFaces = lodData->arrayData.Num();
        
        return lodData;
    }
    
    return 0;
}

bool SceneObjectPart::MeshMesh(TSharedRef<MeshAsset, ESPMode::ThreadSafe> assetdata)
{
    LLSDItem * data = GetMeshData(assetdata, 0);
    if (!data)
        return false;

    int textureIndex = 0;
    numFaces = 0;

    primMeshData.Empty();

    for (auto face = data->arrayData.CreateConstIterator(); face; ++face)
    {
        PrimFaceMeshData pm;

        float minX = -0.5f, minY = -0.5f, minZ = -0.5f, maxX = 0.5f, maxY = 0.5f, maxZ = 0.5f;
        float minU = 0.0f, minV = 0.0f, maxU = 0.0f, maxV = 0.0f;


        if (!(*face)->mapData.Contains(TEXT("NoGeometry")) || !(*face)->mapData[TEXT("NoGeometry")]->data.booleanData)
        {
            if ((*face)->mapData.Contains(TEXT("PositionDomain")))
            {
                //LLSDDecode::DumpItem((*face)->mapData[TEXT("PositionDomain")]);
                LLSDItem *positionDomain = (*face)->mapData[TEXT("PositionDomain")];

                LLSDItem *min = positionDomain->mapData[TEXT("Min")];
                LLSDItem *max = positionDomain->mapData[TEXT("Max")];

                minX = (float)(min->arrayData[0]->data.doubleData);
                minY = (float)(min->arrayData[1]->data.doubleData);
                minZ = (float)(min->arrayData[2]->data.doubleData);

                maxX = (float)(max->arrayData[0]->data.doubleData);
                maxY = (float)(max->arrayData[1]->data.doubleData);
                maxZ = (float)(max->arrayData[2]->data.doubleData);
            }

            if ((*face)->mapData.Contains(TEXT("TexCoord0Domain")))
            {
                LLSDItem *texDomain = (*face)->mapData[TEXT("TexCoord0Domain")];

                LLSDItem *min = texDomain->mapData[TEXT("Min")];
                LLSDItem *max = texDomain->mapData[TEXT("Max")];

                minU = (float)(min->arrayData[0]->data.doubleData);
                minV = (float)(min->arrayData[1]->data.doubleData);

                maxU = (float)(max->arrayData[0]->data.doubleData);
                maxV = (float)(max->arrayData[1]->data.doubleData);
            }

            //UE_LOG(LogTemp, Warning, TEXT("Face %d PositionDomain %f, %f, %f - %f, %f, %f"), textureIndex, minX, minY, minZ, maxX, maxY, maxZ);

            int binaryLength = (*face)->mapData[TEXT("Position")]->binaryLength;

            int numVertices = binaryLength / 6; // 3 x uint16_t
            uint16_t *vertexData = (uint16_t *)((*face)->mapData[TEXT("Position")]->data.binaryData);


            //UE_LOG(LogTemp, Warning, TEXT("Vertex count %d Normals count %d"), numVertices, numNormals);

            for (int idx = 0; idx < numVertices; ++idx)
            {
                uint16_t posX = *vertexData++;
                uint16_t posY = *vertexData++;
                uint16_t posZ = *vertexData++;

                FVector pos(AvinationUtils::uint16tofloat(posX, minX, maxX),
                    -AvinationUtils::uint16tofloat(posY, minY, maxY),
                    AvinationUtils::uint16tofloat(posZ, minZ, maxZ));
                pm.vertices.Add(pos);

                //UE_LOG(LogTemp, Warning, TEXT("Vertex %s"), *pos.ToString());

                pm.tangents.Add(FProcMeshTangent(1, 1, 1));
                pm.vertexColors.Add(FColor(255, 255, 255, 255));
            }
            if ((*face)->mapData.Contains(TEXT("Normal")))
            {
                int normalsLength = (*face)->mapData[TEXT("Normal")]->binaryLength;
                int numNormals = binaryLength / 6; // 3 x uint16_t
                if (numNormals > numVertices)
                    numNormals = numVertices;
                uint16_t *normalsData = (uint16_t *)((*face)->mapData[TEXT("Normal")]->data.binaryData);
                for (int idx = 0; idx < numNormals; ++idx)
                {
                    uint16_t norX = *normalsData++;
                    uint16_t norY = *normalsData++;
                    uint16_t norZ = *normalsData++;
                    FVector nor(AvinationUtils::uint16tofloat(norX, -1.0f, 1.0f),
                        -AvinationUtils::uint16tofloat(norY, -1.0f, 1.0f),
                        AvinationUtils::uint16tofloat(norZ, -1.0f, 1.0f));
                    pm.normals.Add(nor);
                }
            }

            uint16_t numTriangles = (*face)->mapData[TEXT("TriangleList")]->binaryLength / 6; // 3 * uint16_t
            uint16_t *trianglesData = (uint16_t *)((*face)->mapData[TEXT("TriangleList")]->data.binaryData);

            //UE_LOG(LogTemp, Warning, TEXT("Triangles count %d"), numTriangles);
            for (int idx = 0; idx < numTriangles; ++idx)
            {
                uint16_t t1 = *trianglesData++;
                uint16_t t2 = *trianglesData++;
                uint16_t t3 = *trianglesData++;
                pm.triangles.Add(t1);
                pm.triangles.Add(t2);
                pm.triangles.Add(t3);
            }

            uint16_t numUvs = (*face)->mapData[TEXT("TexCoord0")]->binaryLength / 4; // 3 * uint16_t
            uint16_t *uvData = (uint16_t *)((*face)->mapData[TEXT("TexCoord0")]->data.binaryData);

            //UE_LOG(LogTemp, Warning, TEXT("UV count %d"), numUvs);

            for (int idx = 0; idx < numUvs; ++idx)
            {
                uint16_t u = *uvData++;
                uint16_t v = *uvData++;

                pm.uv0.Add(FVector2D(AvinationUtils::uint16tofloat(u, minU, maxU),
                    1.0 - AvinationUtils::uint16tofloat(v, minV, maxV)));
            }
        }
        calcVertsTangents(pm);
        primMeshData.Add(pm);
        numFaces++;
    }
    
    return true;
}

bool SceneObjectPart::MeshPrim()
{
    if (lodWanted > maxLod)
        lodWanted = maxLod;
    
    if (meshed)
        return true;
    
    try
    {
        GeneratePrimMesh(lodWanted);
    }
//    catch (std::exception& ex)
    catch (...)
    {
        return false;
    }

    primMeshData.Empty();
    for (int i = 0; i < 10; i++)
    {
        PrimFaceMeshData pm;
        primMeshData.Add(pm);
    }

    int primFace;
    numFaces = 0;
    int i1;
    int i2;
    int i3;
    for (int face = 0; face < primData->viewerFaces.Num(); face++)
    {
        primFace = primData->viewerFaces[face].primFaceNumber;
        if (primFace >= 16 || primData < 0)
            continue;
        
        PrimFaceMeshData *pm = &primMeshData[primFace];

        // vertices
        FVector v1(primData->viewerFaces[face].v1.X,
                -primData->viewerFaces[face].v1.Y,
                primData->viewerFaces[face].v1.Z);
        FVector v2(primData->viewerFaces[face].v2.X,
                -primData->viewerFaces[face].v2.Y,
                primData->viewerFaces[face].v2.Z);
        FVector v3(primData->viewerFaces[face].v3.X,
                -primData->viewerFaces[face].v3.Y,
                primData->viewerFaces[face].v3.Z);

        if (v1 == v2 || v1 == v3 || v2 == v3)
        {
            // This is not a rational triangle
            continue;
        }

        if (!(pm->vertices).Find(v1, i1))
        {
            i1 = pm->vertices.Num();
            pm->vertices.Add(v1);
            pm->uv0.Add(FVector2D(primData->viewerFaces[face].uv1.U,
                1.0f - primData->viewerFaces[face].uv1.V));
            pm->tangents.Add(FProcMeshTangent(1, 1, 1));
        }
        else
        {
            FVector2D uv(primData->viewerFaces[face].uv1.U, 1.0f - primData->viewerFaces[face].uv1.V);
            if (uv != pm->uv0[i1])
            {
                i1 = pm->vertices.Num();
                v1.X -= 1e-6;
                pm->vertices.Add(v1);
                pm->uv0.Add(uv);
                pm->tangents.Add(FProcMeshTangent(1, 1, 1));
            }
        }

        if (!(pm->vertices).Find(v2, i2))
        {
            i2 = pm->vertices.Num();        
            pm->vertices.Add(v2);
            pm->uv0.Add(FVector2D(primData->viewerFaces[face].uv2.U,
                1.0f - primData->viewerFaces[face].uv2.V));
            pm->tangents.Add(FProcMeshTangent(1, 1, 1));

        }
        else
        {
            FVector2D uv(primData->viewerFaces[face].uv2.U, 1.0f - primData->viewerFaces[face].uv2.V);
            if (uv != pm->uv0[i2])
            {
                i2 = pm->vertices.Num();
                v2.X -= 1e-6;
                pm->vertices.Add(v2);
                pm->uv0.Add(uv);
                pm->tangents.Add(FProcMeshTangent(1, 1, 1));
            }
        }

        if (!(pm->vertices).Find(v3, i3))
        {
            i3 = pm->vertices.Num();
            pm->vertices.Add(v3);
            pm->uv0.Add(FVector2D(primData->viewerFaces[face].uv3.U,
                1.0f - primData->viewerFaces[face].uv3.V));
            pm->tangents.Add(FProcMeshTangent(1, 1, 1));

        }
        else
        {
            FVector2D uv(primData->viewerFaces[face].uv3.U, 1.0f - primData->viewerFaces[face].uv3.V);
            if (uv != pm->uv0[i3])
            {
                i3 = pm->vertices.Num();
                v3.X -= 1e-6;
                pm->vertices.Add(v3);
                pm->uv0.Add(uv);
                pm->tangents.Add(FProcMeshTangent(1, 1, 1));
            }
        }

        // triangles
        pm->triangles.Add(i1);
        pm->triangles.Add(i2);
        pm->triangles.Add(i3);
    }

    delete primData;
    primData = 0;

    for (int i = 0; i < 10; i++)
    {
        calcVertsNormals(primMeshData[i]);
        calcVertsTangents(primMeshData[i]);
    }

    meshed = true;
    return true;
}


void SceneObjectPart::calcVertsNormals(PrimFaceMeshData& pm)
{
    int numVertices = pm.vertices.Num();
    if (numVertices == 0)
        return;
    int numTris = pm.triangles.Num();
    if (numTris == 0)
        return;
    pm.normals.Empty();
    pm.normals.AddZeroed(numVertices);

    int i1, i2, i3;
    FVector v1, e1, e2;

    for (int i = 0; i < numTris;)
    {
        i1 = pm.triangles[i++];
        i2 = pm.triangles[i++];
        i3 = pm.triangles[i++];
        v1 = pm.vertices[i1];
        e1 = pm.vertices[i2] - v1;
        e2 = pm.vertices[i3] - v1;

        FVector surfaceNormal = FVector::CrossProduct(e1,e2);
        pm.normals[i1] -= surfaceNormal;
        pm.normals[i2] -= surfaceNormal;
        pm.normals[i3] -= surfaceNormal;
    }
    for (int i = 0; i < numVertices;i++)
        pm.normals[i].Normalize();
}

void SceneObjectPart::calcVertsTangents(PrimFaceMeshData& pm)
{
    int numVerts = pm.vertices.Num();
    if (numVerts == 0)
        return;
    int numFaces = pm.triangles.Num();
    if (numFaces == 0)
        return;

    TArray<FVector> tan1;
    TArray<FVector> tan2;
    tan1.AddZeroed(numVerts);
    tan2.AddZeroed(numVerts);
    pm.tangents.Empty();
    pm.tangents.AddZeroed(numVerts);


    for (int a = 0; a < numFaces;)
    {
        int i1 = pm.triangles[a++];
        int i2 = pm.triangles[a++];
        int i3 = pm.triangles[a++];

        const FVector v1 = pm.vertices[i1];
        const FVector v2 = pm.vertices[i2];
        const FVector v3 = pm.vertices[i3];

        const FVector2D w1 = pm.uv0[i1];
        const FVector2D w2 = pm.uv0[i2];
        const FVector2D w3 = pm.uv0[i3];

        float x1 = v2.X - v1.X;
        float x2 = v3.X - v1.X;
        float y1 = v2.Y - v1.Y;
        float y2 = v3.Y - v1.Y;
        float z1 = v2.Y - v1.Z;
        float z2 = v3.Z - v1.Z;

        float s1 = w2.X - w1.X;
        float s2 = w3.X - w1.X;
        float t1 = w2.Y - w1.Y;
        float t2 = w3.Y - w1.Y;

        float r = 1.0F / (s1 * t2 - s2 * t1);
        FVector sdir((t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r,
            (t2 * z1 - t1 * z2) * r);
        FVector tdir((s1 * x2 - s2 * x1) * r, (s1 * y2 - s2 * y1) * r,
            (s1 * z2 - s2 * z1) * r);

        tan1[i1] += sdir;
        tan1[i2] += sdir;
        tan1[i3] += sdir;

        tan2[i1] += tdir;
        tan2[i2] += tdir;
        tan2[i3] += tdir;
    }

    for (int a = 0; a < numVerts; a++)
    {
        FVector n = pm.normals[a];
        FVector t = tan1[a];

        float dotnt = FVector::DotProduct(n, t);
        FVector crossnt = FVector::CrossProduct(n, t);

        FVector tsubn = t - n * crossnt;

        // Gram-Schmidt orthogonalize
        tsubn.Normalize();
        float dotCrossT2 = FVector::DotProduct(crossnt, tan2[a]);

        pm.tangents[a] = FProcMeshTangent(FVector(-tsubn.X, tsubn.Y, -tsubn.Z), (dotCrossT2 < 0.0F));
    }
}


void SceneObjectPart::GeneratePrimMesh(int lod)
{
    int sides = 4;
    bool isSphere = false;

    float gprofileBegin = profileBegin;
    float gprofileEnd = profileEnd;

    if (profileShape == pstEquilateralTriangle
        || profileShape == pstIsometricTriangle
        || profileShape == pstRightTriangle)
    {
        sides = 3;
    }
    else if (profileShape == pstCircle)
    {
        switch ((LevelDetail)lod)
        {
            case Highest:
                sides = 24;
                break;
            case High:
                sides = 24;
                break;
            case Low:
                sides = 12;
                break;
            default:
                sides = 6;
                break;
        }
    }
    else if (profileShape == pstHalfCircle)
    {
        // half circle, prim is a sphere
        isSphere = true;
        switch ((LevelDetail)lod)
        {
        case Highest:
            sides = 24;
            break;
        case High:
            sides = 24;
            break;
        case Low:
            sides = 12;
            break;
        default:
            sides = 6;
            break;
        }
        
        gprofileBegin = 0.5f * profileBegin + 0.5f;
        gprofileEnd = 0.5f * profileEnd + 0.5f;
    }

    // fallback to same hole type
    int hollowsides = sides;

    if (hollowShape == hstCircle)
    {
        switch ((LevelDetail)lod)
        {
            case Highest:
                hollowsides = 24;
                break;
            case High:
                hollowsides = 24;
                break;
            case Low:
                hollowsides = 12;
                break;
            default:
                hollowsides = 6;
                break;
        }
    }
    else if (hollowShape == hstSquare)
        hollowsides = 4;
    else if (hollowShape == hstTriangle)
    {
        if (profileShape == pstHalfCircle)
            hollowsides = 6;
        else
            hollowsides = 3;
    }
    
    PrimMesh *newPrim = new PrimMesh(sides, gprofileBegin, gprofileEnd, profileHollow, hollowsides);
    newPrim->viewerMode = true;
    newPrim->sphereMode = isSphere;
    newPrim->holeSizeX = pathScaleX;
    newPrim->holeSizeY = pathScaleY;
    newPrim->pathCutBegin = pathBegin;
    newPrim->pathCutEnd = pathEnd;
    newPrim->topShearX = pathShearX;
    newPrim->topShearY = pathShearY;
    newPrim->radius = pathRadiusOffset;
    newPrim->revolutions = pathRevolutions;
    newPrim->skew = pathSkew;
      
    if (((ExtrusionType)pathCurve == etStraight) || ((ExtrusionType)pathCurve == etFlexible))
    {
        newPrim->twistBegin = pathTwistBegin * 18 / 10;
        newPrim->twistEnd =pathTwist * 18 / 10;
        newPrim->taperX = pathScaleX;
        newPrim->taperY = pathScaleY;
 
        newPrim->ExtrudeLinear();
    }
    else
    {
        newPrim->holeSizeX = 1.0f - pathScaleX;
        newPrim->holeSizeY = 1.0f - pathScaleY;
        newPrim->radius = 0.01f * pathRadiusOffset;
        newPrim->revolutions = 1.0f + 0.015f * pathRevolutions;
        newPrim->skew = 0.01f * pathSkew;
        newPrim->twistBegin = pathTwistBegin * 36 / 10;
        newPrim->twistEnd = pathTwist * 36 / 10;
        newPrim->taperX = pathTaperX * 0.01f;
        newPrim->taperY = pathTaperY * 0.01f;
   
        newPrim->ExtrudeCircular();
    }
    
    primData = newPrim;
    
    /*
    for (auto it = primData->viewerFaces.CreateConstIterator() ; it ; ++it)
    {
        UE_LOG(LogTemp, Warning, TEXT("Face %d (%f, %f, %f) - (%f, %f, %f) - (%f, %f, %f)"), (*it).primFaceNumber,
               (*it).v1.X,
               (*it).v1.Y,
               (*it).v1.Z,
               (*it).v2.X,
               (*it).v2.Y,
               (*it).v2.Z,
               (*it).v3.X,
               (*it).v3.Y,
               (*it).v3.Z
               );
    }
    */
}

void SceneObjectPart::SculptReceived(FGuid id, TSharedAssetRef asset)
{
    TSharedRef<SculptAsset, ESPMode::ThreadSafe> sculpt = StaticCastSharedRef<SculptAsset>(asset);
    
    if (asset->state == AssetBase::Failed)
        return;
    
    MeshSculpt(sculpt);
    
    haveAllAssets = true;
    
    group->fetchLock.Lock();
    --group->activeFetches;
    group->fetchLock.Unlock();
    
    group->CheckAssetsDone();
}

bool SceneObjectPart::MeshSculpt(TSharedRef<SculptAsset, ESPMode::ThreadSafe> data)
{
    if (lodWanted > maxLod)
        lodWanted = maxLod;
    
   if (meshed)
        return true;
    
    try
    {
        GenerateSculptMesh(data, lodWanted);
    }
    catch (...)
    {
        return false;
    }

    if (!sculptData)
        return false;

    SculptMesh *prim = sculptData;

    PrimFaceMeshData pm;
    primMeshData.Empty();
    
    FVector* ptr;
    UVCoord* uptr;

    int ncoord = 0;
    for (; ncoord < prim->coords.Num(); ncoord++)
    {
        ptr = &prim->coords[ncoord];
        FVector v(ptr->X, -ptr->Y, ptr->Z);
        pm.vertices.Add(v);

        ptr = &prim->normals[ncoord];
        FVector n(-ptr->X, ptr->Y, -ptr->Z);
        pm.normals.Add(n);

        uptr = &prim->uvs[ncoord];
        pm.uv0.Add(FVector2D(uptr->U, 1.0 - uptr->V));
    }

    for (int i = 0; i < prim->faces.Num(); i++)
    {
        int i1 = prim->faces[i].v1;
        int i2 = prim->faces[i].v2;
        int i3 = prim->faces[i].v3;

        if (i1 >= ncoord || i2 >= ncoord || i3 >= ncoord)
                continue;

        pm.triangles.Add(i3);
        pm.triangles.Add(i2);
        pm.triangles.Add(i1);
    }
    calcVertsTangents(pm);
    primMeshData.Add(pm);

    numFaces = 1;
    
    meshed = true;
    return true;
}

void SceneObjectPart::GenerateSculptMesh(TSharedRef<SculptAsset, ESPMode::ThreadSafe> indata, int lod)
{
    bool mirror = (sculptType & 0x80) ? true : false;
    bool invert = (sculptType & 0x40) ? true : false;
    float pixScale = 1.0f / 256.0f;
    
    opj_image_t *tex = indata->image;
    
    if (!tex)
        throw std::exception();
    
    if (tex->numcomps < 3)
        throw std::exception();
    
    int w = tex->comps[0].w;
    int h = tex->comps[0].h;

    TArray<TArray<FVector>> rows;
    OPJ_INT32 *r = tex->comps[0].data, *g = tex->comps[1].data, *b = tex->comps[2].data;
    for (int i = 0; i < h; i++)
        rows.AddDefaulted();

    for (int i = 0; i < h; i++)
    {
        for (int j = 0; j < w; j++)
        {
            FVector c = FVector((float)(*r++ & 0xff) * pixScale - 0.5f,
                            (float)(*g++ & 0xff) * pixScale - 0.5f,
                            (float)(*b++ & 0xff) * pixScale - 0.5f);
            if (mirror)
                c.X = -c.X;
            rows[h - 1 - i].Add(c);
        }
    }
    
    sculptData = new SculptMesh(rows, (SculptType)sculptType, true, mirror, invert, lod);
}

void SceneObjectPart::DeleteMeshData()
{
    if (primData)
        delete primData;
    if (sculptData)
        delete sculptData;
    primData = 0;
    sculptData = 0;
    meshAssetData.Empty();
}

void SceneObjectPart::GatherTextures()
{
    for (int i = 0 ; i < numFaces ; ++i)
        group->AddTexture(textures[i].textureId);
}