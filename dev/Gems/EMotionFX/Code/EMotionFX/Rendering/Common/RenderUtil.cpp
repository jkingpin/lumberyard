/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include "RenderUtil.h"
#include <MCore/Source/Compare.h>
#include <MCore/Source/Ray.h>
#include <MCore/Source/PlaneEq.h>
#include <EMotionFX/Source/SkinningInfoVertexAttributeLayer.h>
#include <EMotionFX/Source/TransformData.h>
#include <EMotionFX/Source/ActorManager.h>
#include <EMotionFX/Source/EMotionFXManager.h>
#include <EMotionFX/Source/Node.h>
#include "OrthographicCamera.h"
#include "OrbitCamera.h"


namespace MCommon
{
    // gizmo colors
    MCore::RGBAColor ManipulatorColors::mSelectionColor         = MCore::RGBAColor(1.0f, 1.0f, 0.0f);
    MCore::RGBAColor ManipulatorColors::mSelectionColorDarker   = MCore::RGBAColor(0.5f, 0.5f, 0.0f, 0.5f);
    MCore::RGBAColor ManipulatorColors::mRed                    = MCore::RGBAColor(0.781f, 0.0f, 0.0f);
    MCore::RGBAColor ManipulatorColors::mGreen                  = MCore::RGBAColor(0.0f, 0.609f, 0.0f);
    MCore::RGBAColor ManipulatorColors::mBlue                   = MCore::RGBAColor(0.0f, 0.0f, 0.762f);


    // static variables
    uint32 RenderUtil::mNumMaxLineVertices          = 8192 * 16;// 8096 * 16 * sizeof(LineVertex) = 3,5 MB
    uint32 RenderUtil::mNumMaxMeshVertices          = 1024;
    uint32 RenderUtil::mNumMaxMeshIndices           = 1024 * 3;
    uint32 RenderUtil::mNumMax2DLines               = 8192;
    uint32 RenderUtil::mNumMaxTriangleVertices      = 8192 * 16;// 8096 * 16 * sizeof(LineVertex) = 3,5 MB
    float RenderUtil::m_wireframeSphereSegmentCount = 16.0f;


    // constructor
    RenderUtil::RenderUtil()
        : m_devicePixelRatio(1.0f)
    {
        mVertexBuffer   = new LineVertex[mNumMaxLineVertices];
        m2DLines        = new Line2D[mNumMax2DLines];
        mNumVertices    = 0;
        mNum2DLines     = 0;
        mUnitSphereMesh = CreateSphere(1.0f);
        mCylinderMesh   = CreateCylinder(2.0f, 1.0f, 2.0f);
        mArrowHeadMesh  = CreateArrowHead(1.0f, 0.5f);
        mUnitCubeMesh   = CreateCube(1.0f);
        mFont           = new VectorFont(this);

        mTriangleVertices.SetMemoryCategory(MEMCATEGORY_MCOMMON);
    }


    // destructor
    RenderUtil::~RenderUtil()
    {
        delete[]    mVertexBuffer;
        delete[]    m2DLines;
        delete      mUnitSphereMesh;
        delete      mCylinderMesh;
        delete      mUnitCubeMesh;
        delete      mArrowHeadMesh;
        delete      mFont;

        // get rid of the global space positions
        mGlobalPositions.Clear();
    }


    // render lines from the local vertex buffer
    void RenderUtil::RenderLines()
    {
        // check if we have to render anything and skip directly in case the line vertex buffer is empty
        if (mNumVertices == 0)
        {
            return;
        }

        // render the lines and reset the number of vertices
        RenderLines(mVertexBuffer, mNumVertices);
        mNumVertices = 0;
    }


    // render 2D lines from the local line buffer
    void RenderUtil::Render2DLines()
    {
        // check if we have to render anything and skip directly in case the line buffer is empty
        if (mNum2DLines == 0)
        {
            return;
        }

        // render the lines and reset the number of lines
        Render2DLines(m2DLines, mNum2DLines);
        mNum2DLines = 0;
    }


    // render triangles from the local vertex buffer
    void RenderUtil::RenderTriangles()
    {
        // check if we have to render anything and skip directly in case there are no triangles
        if (mTriangleVertices.GetIsEmpty())
        {
            return;
        }

        // render the triangles and clear the array
        RenderTriangles(mTriangleVertices);
        mTriangleVertices.Clear(false);
    }


    // render a grid
    void RenderUtil::RenderGrid(AZ::Vector2 start, AZ::Vector2 end, const AZ::Vector3& normal, float scale, const MCore::RGBAColor& mainAxisColor, const MCore::RGBAColor& gridColor, const MCore::RGBAColor& subStepColor, bool directlyRender)
    {
        start.SetX(start.GetX() - MCore::Math::FMod(start.GetX(), scale));
        start.SetY(start.GetY() - MCore::Math::FMod(start.GetY(), scale));
        end.SetX(end.GetX() - MCore::Math::FMod(end.GetX(), scale));
        end.SetY(end.GetY() - MCore::Math::FMod(end.GetY(), scale));

        AZ::Vector3 gridLineStart, gridLineEnd;
        MCore::RGBAColor color;

        MCore::Matrix matRotate;
        matRotate.Identity();
        matRotate.SetRotationMatrixTwoVectors(AZ::Vector3(0.0f, 1.0f, 0.0f), normal);

        const uint32    gridBlockSize       = 5;
        const uint32    numVerticalLines    = static_cast<uint32>((end.GetX() - start.GetX()) / scale);// x component
        const uint32    numHorizontalLines  = static_cast<uint32>((end.GetY() - start.GetY()) / scale);// y component
        const float     scaledGridBlockSize = gridBlockSize * scale;
        const float     maxFmodError        = scale * 0.1f;

        // render all vertical grid lines
        for (uint32 x = 0; x <= numVerticalLines; ++x)
        {
            gridLineStart.Set(start.GetX() + x * scale, 0.0f, start.GetY());
            gridLineEnd.Set(gridLineStart.GetX(), 0.0f, end.GetY());

            const float fmodStartValue = MCore::Math::FMod(MCore::Math::Abs(gridLineStart.GetX()), scaledGridBlockSize);

            // coordinate axis
            if (MCore::Compare<float>::CheckIfIsClose(gridLineStart.GetX(), 0.0f, maxFmodError))
            {
                color = mainAxisColor;
            }
            // substep lines
            else if (MCore::Compare<float>::CheckIfIsClose(fmodStartValue, 0.0f, maxFmodError) || MCore::Compare<float>::CheckIfIsClose(fmodStartValue, scaledGridBlockSize, maxFmodError))
            {
                color = subStepColor;
            }
            // block lines
            else
            {
                color = gridColor;
            }

            gridLineStart = matRotate.Mul3x3(gridLineStart);
            gridLineEnd   = matRotate.Mul3x3(gridLineEnd);
            RenderLine(gridLineStart, gridLineEnd, color);
        }

        // render all horizontal grid lines
        for (uint32 y = 0; y <= numHorizontalLines; ++y)
        {
            gridLineStart.SetZ(start.GetY() + y * scale);
            gridLineStart.SetX(start.GetX());
            gridLineEnd.SetZ(gridLineStart.GetZ());
            gridLineEnd.SetX(end.GetX());

            gridLineStart.Set(start.GetX(), 0.0f, start.GetY() + y * scale);
            gridLineEnd.Set(end.GetX(), 0.0f, gridLineStart.GetZ());

            const float fmodStartValue = MCore::Math::FMod(MCore::Math::Abs(gridLineStart.GetZ()), scaledGridBlockSize);

            // coordinate axis
            if (MCore::Compare<float>::CheckIfIsClose(gridLineStart.GetZ(), 0.0f, maxFmodError))
            {
                color = mainAxisColor;
            }
            // substep lines
            else if (MCore::Compare<float>::CheckIfIsClose(fmodStartValue, 0.0f, maxFmodError) || MCore::Compare<float>::CheckIfIsClose(fmodStartValue, scaledGridBlockSize, maxFmodError))
            {
                color = subStepColor;
            }
            // block lines
            else
            {
                color = gridColor;
            }

            gridLineStart = matRotate.Mul3x3(gridLineStart);
            gridLineEnd   = matRotate.Mul3x3(gridLineEnd);

            RenderLine(gridLineStart, gridLineEnd, color);
        }

        if (directlyRender)
        {
            RenderLines();
        }
    }


    // render the current bounding box of the given actor instance
    void RenderUtil::RenderAABB(const MCore::AABB& box, const MCore::RGBAColor& color, bool directlyRender)
    {
        AZ::Vector3 min = box.GetMin();
        AZ::Vector3 max = box.GetMax();

        // generate our vertices
        AZ::Vector3 p[8];
        p[0].Set(min.GetX(), min.GetY(), min.GetZ());
        p[1].Set(max.GetX(), min.GetY(), min.GetZ());
        p[2].Set(max.GetX(), min.GetY(), max.GetZ());
        p[3].Set(min.GetX(), min.GetY(), max.GetZ());
        p[4].Set(min.GetX(), max.GetY(), min.GetZ());
        p[5].Set(max.GetX(), max.GetY(), min.GetZ());
        p[6].Set(max.GetX(), max.GetY(), max.GetZ());
        p[7].Set(min.GetX(), max.GetY(), max.GetZ());

        // render the box
        RenderLine(p[0], p[1], color);
        RenderLine(p[1], p[2], color);
        RenderLine(p[2], p[3], color);
        RenderLine(p[3], p[0], color);

        RenderLine(p[4], p[5], color);
        RenderLine(p[5], p[6], color);
        RenderLine(p[6], p[7], color);
        RenderLine(p[7], p[4], color);

        RenderLine(p[0], p[4], color);
        RenderLine(p[1], p[5], color);
        RenderLine(p[2], p[6], color);
        RenderLine(p[3], p[7], color);

        if (directlyRender)
        {
            RenderLines();
        }
    }


    // render selection gizmo around the given AABB
    void RenderUtil::RenderSelection(const MCore::AABB& box, const MCore::RGBAColor& color, bool directlyRender)
    {
        //const Vector3 center  = box.CalcMiddle();
        const AZ::Vector3       min     = box.GetMin();// + (box.GetMin()-center).Normalized()*0.005f;
        const AZ::Vector3       max     = box.GetMax();// + (box.GetMax()-center).Normalized()*0.005f;
        const float             scale   = box.CalcRadius() * 0.1f;
        const AZ::Vector3       up      = AZ::Vector3(0.0f, 1.0f, 0.0f) * scale;
        const AZ::Vector3       right   = AZ::Vector3(1.0f, 0.0f, 0.0f) * scale;
        const AZ::Vector3       front   = AZ::Vector3(0.0f, 0.0f, 1.0f) * scale;

        // generate our vertices
        AZ::Vector3 p[8];
        p[0].Set(min.GetX(), min.GetY(), min.GetZ());
        p[1].Set(max.GetX(), min.GetY(), min.GetZ());
        p[2].Set(max.GetX(), min.GetY(), max.GetZ());
        p[3].Set(min.GetX(), min.GetY(), max.GetZ());
        p[4].Set(min.GetX(), max.GetY(), min.GetZ());
        p[5].Set(max.GetX(), max.GetY(), min.GetZ());
        p[6].Set(max.GetX(), max.GetY(), max.GetZ());
        p[7].Set(min.GetX(), max.GetY(), max.GetZ());

        // render the box
        RenderLine(p[0], p[0] + up,    color);
        RenderLine(p[0], p[0] + right, color);
        RenderLine(p[0], p[0] + front, color);

        RenderLine(p[1], p[1] + up,    color);
        RenderLine(p[1], p[1] - right, color);
        RenderLine(p[1], p[1] + front, color);

        RenderLine(p[2], p[2] + up,    color);
        RenderLine(p[2], p[2] - right, color);
        RenderLine(p[2], p[2] - front, color);

        RenderLine(p[3], p[3] + up,    color);
        RenderLine(p[3], p[3] + right, color);
        RenderLine(p[3], p[3] - front, color);

        RenderLine(p[4], p[4] - up,    color);
        RenderLine(p[4], p[4] + right, color);
        RenderLine(p[4], p[4] + front, color);

        RenderLine(p[5], p[5] - up,    color);
        RenderLine(p[5], p[5] - right, color);
        RenderLine(p[5], p[5] + front, color);

        RenderLine(p[6], p[6] - up,    color);
        RenderLine(p[6], p[6] - right, color);
        RenderLine(p[6], p[6] - front, color);

        RenderLine(p[7], p[7] - up,    color);
        RenderLine(p[7], p[7] + right, color);
        RenderLine(p[7], p[7] - front, color);

        if (directlyRender)
        {
            RenderLines();
        }
    }


    // constructor
    RenderUtil::AABBRenderSettings::AABBRenderSettings()
    {
        mNodeBasedAABB              = true;
        mMeshBasedAABB              = true;
        mCollisionMeshBasedAABB     = true;
        mStaticBasedAABB            = true;
        mStaticBasedColor           = MCore::RGBAColor(0.0f, 0.7f, 0.7f);
        mNodeBasedColor             = MCore::RGBAColor(1.0f, 0.0f, 0.0f);
        mCollisionMeshBasedColor    = MCore::RGBAColor(0.0f, 0.7f, 0.0f);
        mMeshBasedColor             = MCore::RGBAColor(0.0f, 0.0f, 0.7f);
    }


    // render the given types of AABBs of a actor instance
    void RenderUtil::RenderAABBs(EMotionFX::ActorInstance* actorInstance, const AABBRenderSettings& renderSettings, bool directlyRender)
    {
        // get the current LOD level
        const uint32 lodLevel = actorInstance->GetLODLevel();

        // handle the collision mesh based AABB
        if (renderSettings.mCollisionMeshBasedAABB)
        {
            // calculate the collision mesh based AABB
            MCore::AABB box;
            actorInstance->CalcCollisionMeshBasedAABB(lodLevel, &box);

            // render the aabb
            if (box.CheckIfIsValid())
            {
                RenderAABB(box, renderSettings.mCollisionMeshBasedColor);
            }
        }

        // handle the node based AABB
        if (renderSettings.mNodeBasedAABB)
        {
            // calculate the node based AABB
            MCore::AABB box;
            actorInstance->CalcNodeBasedAABB(&box);

            // render the aabb
            if (box.CheckIfIsValid())
            {
                RenderAABB(box, renderSettings.mNodeBasedColor);
            }
        }

        // handle the mesh based AABB
        if (renderSettings.mMeshBasedAABB)
        {
            // calculate the mesh based AABB
            MCore::AABB box;
            actorInstance->CalcMeshBasedAABB(lodLevel, &box);

            // render the aabb
            if (box.CheckIfIsValid())
            {
                RenderAABB(box, renderSettings.mMeshBasedColor);
            }
        }

        if (renderSettings.mStaticBasedAABB)
        {
            // calculate the static based AABB
            MCore::AABB box;
            actorInstance->CalcStaticBasedAABB(&box);

            // render the aabb
            if (box.CheckIfIsValid())
            {
                RenderAABB(box, renderSettings.mStaticBasedColor);
            }
        }

        if (directlyRender)
        {
            RenderLines();
        }
    }


    // render a simple line based skeleton
    void RenderUtil::RenderSimpleSkeleton(EMotionFX::ActorInstance* actorInstance, MCore::Array<uint32>* visibleNodeIndices, MCore::Array<uint32>* selectedNodeIndices, const MCore::RGBAColor& color, const MCore::RGBAColor& selectedColor, bool directlyRender)
    {
        // get the actor it is an instance from
        EMotionFX::Actor* actor = actorInstance->GetActor();

        // get the global space matrices
        MCore::Matrix* globalMatrices = actorInstance->GetTransformData()->GetGlobalInclusiveMatrices();

        EMotionFX::Skeleton* skeleton = actor->GetSkeleton();

        // for all nodes in the actor
        const uint32 numNodes = actorInstance->GetNumEnabledNodes();
        for (uint32 n = 0; n < numNodes; ++n)
        {
            EMotionFX::Node*    curNode     = skeleton->GetNode(actorInstance->GetEnabledNode(n));
            const uint32        nodeIndex   = curNode->GetNodeIndex();

            // skip root nodes, you could also use curNode->IsRootNode()
            // but we use the parent index here, as we will reuse it
            uint32 parentIndex = curNode->GetParentIndex();
            if (parentIndex == MCORE_INVALIDINDEX32)
            {
                continue;
            }

            // check if the node is visible
            if (visibleNodeIndices == NULL ||
                (visibleNodeIndices->GetIsEmpty()) ||
                (visibleNodeIndices->GetIsEmpty() == false && visibleNodeIndices->Find(nodeIndex) != MCORE_INVALIDINDEX32))
            {
                // draw a line between the current node and its parent node
                // here we first calculate the start and end position of the line, which are just the global space positions of the nodes
                AZ::Vector3 startPos = globalMatrices[ curNode->GetNodeIndex() ].GetTranslation();
                AZ::Vector3 endPos    = globalMatrices[ parentIndex ].GetTranslation();

                // finally draw the line
                if (selectedNodeIndices != NULL && selectedNodeIndices->GetIsEmpty() == false && selectedNodeIndices->Find(parentIndex) != MCORE_INVALIDINDEX32)
                {
                    RenderLine(startPos, endPos, selectedColor);
                }
                else
                {
                    RenderLine(startPos, endPos, color);
                }
            }
        }

        if (directlyRender)
        {
            RenderLines();
        }
    }


    // render object orientated bounding  boxes for all enabled nodes inside the actor instance
    void RenderUtil::RenderOBBs(EMotionFX::ActorInstance* actorInstance, MCore::Array<uint32>* visibleNodeIndices, MCore::Array<uint32>* selectedNodeIndices, const MCore::RGBAColor& color, const MCore::RGBAColor& selectedColor, bool directlyRender)
    {
        AZ::Vector3 p[8];

        // get the actor it is an instance from
        EMotionFX::Actor* actor = actorInstance->GetActor();
        EMotionFX::Skeleton* skeleton = actor->GetSkeleton();

        // get the global space matrices
        MCore::Matrix* globalMatrices = actorInstance->GetTransformData()->GetGlobalInclusiveMatrices();

        // iterate through all enabled nodes
        MCore::RGBAColor tempColor;
        const uint32 numEnabled = actorInstance->GetNumEnabledNodes();
        for (uint32 i = 0; i < numEnabled; ++i)
        {
            EMotionFX::Node*    node        = skeleton->GetNode(actorInstance->GetEnabledNode(i));
            const uint32        nodeIndex   = node->GetNodeIndex();

            // check if the node is visible
            if ((visibleNodeIndices == NULL) ||
                (visibleNodeIndices->GetIsEmpty()) ||
                (visibleNodeIndices->GetIsEmpty() == false && visibleNodeIndices->Find(nodeIndex) != MCORE_INVALIDINDEX32))
            {
                const MCore::OBB&   obb         = actor->GetNodeOBB(node->GetNodeIndex());
                MCore::Matrix       globalTM    = globalMatrices[ node->GetNodeIndex() ];

                // skip the OBB if it isn't valid
                if (obb.CheckIfIsValid() == false)
                {
                    continue;
                }

                // check if the current bone is selected and set the color according to it
                if (selectedNodeIndices != NULL && selectedNodeIndices->GetIsEmpty() == false && selectedNodeIndices->Find(nodeIndex) != MCORE_INVALIDINDEX32)
                {
                    tempColor = selectedColor;
                }
                else
                {
                    tempColor = color;
                }

                obb.CalcCornerPoints(p);
                p[0] *= globalTM;
                p[1] *= globalTM;
                p[2] *= globalTM;
                p[3] *= globalTM;
                p[4] *= globalTM;
                p[5] *= globalTM;
                p[6] *= globalTM;
                p[7] *= globalTM;

                // render
                RenderLine(p[0], p[1], tempColor);
                RenderLine(p[1], p[2], tempColor);
                RenderLine(p[2], p[3], tempColor);
                RenderLine(p[0], p[3], tempColor);

                RenderLine(p[1], p[5], tempColor);
                RenderLine(p[3], p[7], tempColor);
                RenderLine(p[2], p[6], tempColor);
                RenderLine(p[0], p[4], tempColor);

                RenderLine(p[4], p[5], tempColor);
                RenderLine(p[4], p[7], tempColor);
                RenderLine(p[6], p[7], tempColor);
                RenderLine(p[6], p[5], tempColor);
            }
        }

        if (directlyRender)
        {
            RenderLines();
        }
    }


    // render wireframe mesh
    void RenderUtil::RenderWireframe(EMotionFX::Mesh* mesh, const MCore::Matrix& globalTM, const MCore::RGBAColor& color, bool directlyRender, float offsetScale)
    {
        // check if the mesh is valid and skip the node in case it's not
        if (mesh == NULL)
        {
            return;
        }

        const float scale = 0.01f * offsetScale;

        AZ::PackedVector3f* normals     = (AZ::PackedVector3f*)mesh->FindVertexData(EMotionFX::Mesh::ATTRIB_NORMALS);
        AZ::PackedVector3f* positions   = (AZ::PackedVector3f*)mesh->FindVertexData(EMotionFX::Mesh::ATTRIB_POSITIONS);
        uint32*         indices     = (uint32*)mesh->GetIndices();
        uint8*          vtxCounts   = (uint8*)mesh->GetPolygonVertexCounts();

        uint32 indexA;
        uint32 indexB;
        uint32 curIndex = 0;
        AZ::Vector3 posA;
        AZ::Vector3 posB;
        const uint32 numPolygons = mesh->GetNumPolygons();
        for (uint32 p = 0; p < numPolygons; ++p)
        {
            const uint32 numPolyVerts = vtxCounts[p];
            for (uint32 i = 0; i < numPolyVerts; ++i)
            {
                indexA = indices[curIndex + i];
                indexB = (i < numPolyVerts - 1) ? indices[curIndex + i + 1] : indices[curIndex];
                posA = (AZ::Vector3(positions[indexA]) + AZ::Vector3(normals[indexA]) * scale) * globalTM;
                posB = (AZ::Vector3(positions[indexB]) + AZ::Vector3(normals[indexB]) * scale) * globalTM;
                RenderLine(posA, posB, color);
            }

            curIndex += numPolyVerts;
        }
        /*
            int32 a, b, c;
            Vector3 posA, posB, posC;
            for (uint32 j=0; j<numIndices; j+=3)
            {
                a       = indices[j+0];
                b       = indices[j+1];
                c       = indices[j+2];

                posA    = (positions[a] + normals[a] * 0.005f) * globalTM;
                posB    = (positions[b] + normals[b] * 0.005f) * globalTM;
                posC    = (positions[c] + normals[c] * 0.005f) * globalTM;

                RenderLine( posA, posB, color );
                RenderLine( posB, posC, color );
                RenderLine( posA, posC, color );
            }*/

        if (directlyRender)
        {
            RenderLines();
        }
    }


    // render vertex and face normals
    void RenderUtil::RenderNormals(EMotionFX::Mesh* mesh, const MCore::Matrix& globalTM, bool vertexNormals, bool faceNormals, float vertexNormalsScale, float faceNormalsScale, const MCore::RGBAColor& colorVertexNormals, const MCore::RGBAColor& colorFaceNormals, bool directlyRender)
    {
        // check if the mesh is valid and skip the node in case it's not
        if (mesh == NULL)
        {
            return;
        }

        // check if we need to render anything at all
        if (vertexNormals == false && faceNormals == false)
        {
            return;
        }

        PrepareForMesh(mesh, globalTM);

        AZ::PackedVector3f* normals     = (AZ::PackedVector3f*)mesh->FindVertexData(EMotionFX::Mesh::ATTRIB_NORMALS);
        uint32*         indices     = (uint32*)mesh->GetIndices();
        const uint32    numVertices = mesh->GetNumVertices();
        uint8*          vertCounts  = mesh->GetPolygonVertexCounts();

        // render face normals
        if (faceNormals)
        {
            AZ::Vector3 posA, posB, posC;
            AZ::Vector3 normalDir, normalPos;
            uint32 indexA, indexB, indexC;
            uint32 polyStartIndex = 0;
            const uint32 numPolygons = mesh->GetNumPolygons();
            for (uint32 p = 0; p < numPolygons; ++p)
            {
                const uint32 numPolyVerts = vertCounts[p];

                // get the first 3 indices
                indexA = indices[ polyStartIndex + 0 ];
                indexB = indices[ polyStartIndex + 1 ];
                indexC = indices[ polyStartIndex + 2 ];

                posA = mGlobalPositions[ indexA ];
                posB = mGlobalPositions[ indexB ];
                posC = mGlobalPositions[ indexC ];

                normalDir   = (posB - posA).Cross(posC - posA).GetNormalized();

                // calculate the center pos
                normalPos = AZ::Vector3::CreateZero();
                for (uint32 v = 0; v < numPolyVerts; ++v)
                {
                    normalPos += mGlobalPositions[ indices[polyStartIndex + v] ];
                }
                normalPos /= (float)numPolyVerts;

                RenderLine(normalPos, normalPos + (normalDir * faceNormalsScale), colorFaceNormals);

                polyStartIndex += numPolyVerts;
            }
        }

        // render vertex normals
        if (vertexNormals)
        {
            AZ::Vector3 normal;
            for (uint32 j = 0; j < numVertices; ++j)
            {
                normal = globalTM.Mul3x3(AZ::Vector3(normals[j])) * vertexNormalsScale;
                RenderLine(mGlobalPositions[j], mGlobalPositions[j] + normal, colorVertexNormals);
            }
        }

        if (directlyRender)
        {
            RenderLines();
        }
    }


    // render tangents and bitangents of the mesh
    void RenderUtil::RenderTangents(EMotionFX::Mesh* mesh, const MCore::Matrix& globalTM, float scale, const MCore::RGBAColor& colorTangents, const MCore::RGBAColor& mirroredBitangentColor, const MCore::RGBAColor& colorBitangent, bool directlyRender)
    {
        // check if the mesh is valid and skip the node in case it's not
        if (mesh == NULL)
        {
            return;
        }

        // get the tangents and check if this mesh actually has tangents
        AZ::Vector4* tangents = static_cast<AZ::Vector4*>(mesh->FindVertexData(EMotionFX::Mesh::ATTRIB_TANGENTS));
        if (tangents == NULL)
        {
            return;
        }

        AZ::PackedVector3f* bitangents = static_cast<AZ::PackedVector3f*>(mesh->FindVertexData(EMotionFX::Mesh::ATTRIB_BITANGENTS));

        PrepareForMesh(mesh, globalTM);

        AZ::PackedVector3f* normals = (AZ::PackedVector3f*)mesh->FindVertexData(EMotionFX::Mesh::ATTRIB_NORMALS);
        const uint32 numVertices = mesh->GetNumVertices();

        // render the tangents and bitangents
        AZ::Vector3 position, orgTangent, tangent, bitangent;
        for (uint32 i = 0; i < numVertices; ++i)
        {
            orgTangent.Set(tangents[i].GetX(), tangents[i].GetY(), tangents[i].GetZ());
            tangent = globalTM.Mul3x3(orgTangent).GetNormalized();

            if (bitangents)
            {
                bitangent= AZ::Vector3(bitangents[i]);
            }
            else
            {
                bitangent = tangents[i].GetW() * (AZ::Vector3(normals[i]).Cross(orgTangent));
            }
            bitangent = globalTM.Mul3x3(bitangent).GetNormalized();

            RenderLine(mGlobalPositions[i], mGlobalPositions[i] + (tangent * scale), colorTangents);

            if (tangents[i].GetW() < 0.0f)
            {
                RenderLine(mGlobalPositions[i], mGlobalPositions[i] + (bitangent * scale), mirroredBitangentColor);
            }
            else
            {
                RenderLine(mGlobalPositions[i], mGlobalPositions[i] + (bitangent * scale), colorBitangent);
            }
        }

        if (directlyRender)
        {
            RenderLines();
        }
    }


    // precalculate data for rendering for the given mesh
    void RenderUtil::PrepareForMesh(EMotionFX::Mesh* mesh, const MCore::Matrix& globalTM)
    {
        // check if we have already prepared for the given mesh
        if (mCurrentMesh == mesh)
        {
            return;
        }

        // set our new current mesh
        mCurrentMesh = mesh;

        // get the number of vertices and the data
        const uint32    numVertices = mCurrentMesh->GetNumVertices();
        AZ::PackedVector3f* positions   = (AZ::PackedVector3f*)mCurrentMesh->FindVertexData(EMotionFX::Mesh::ATTRIB_POSITIONS);

        // check if the vertices fits in our buffer
        if (mGlobalPositions.GetLength() < numVertices)
        {
            mGlobalPositions.Resize(numVertices);
        }

        // pre-calculate the global space positions
        for (uint32 i = 0; i < numVertices; ++i)
        {
            mGlobalPositions[i] = AZ::Vector3(positions[i]) * globalTM;
        }
    }


    // calculate the size of the joint sphere
    float RenderUtil::GetBoneScale(EMotionFX::ActorInstance* actorInstance, EMotionFX::Node* node)
    {
        // get the transform data
        EMotionFX::TransformData* transformData = actorInstance->GetTransformData();

        const uint32            nodeIndex       = node->GetNodeIndex();
        const uint32            parentIndex     = node->GetParentIndex();
        const AZ::Vector3       nodeGlobalPos   = transformData->GetGlobalInclusiveMatrix(nodeIndex).GetTranslation();

        if (parentIndex != MCORE_INVALIDINDEX32)
        {
            const AZ::Vector3       parentGlobalPos = transformData->GetGlobalInclusiveMatrix(parentIndex).GetTranslation();
            const AZ::Vector3       bone            = parentGlobalPos - nodeGlobalPos;
            const float             boneLength      = MCore::SafeLength(bone);

            // 10% of the bone length is the sphere size
            return boneLength * 0.1f;
        }

        return 0.0f;
    }


    // render the advanced skeleton
    void RenderUtil::RenderSkeleton(EMotionFX::ActorInstance* actorInstance, const MCore::Array<uint32>& boneList, MCore::Array<uint32>* visibleNodeIndices, MCore::Array<uint32>* selectedNodeIndices, const MCore::RGBAColor& color, const MCore::RGBAColor& selectedColor)
    {
        // check if our render util supports rendering meshes, if not render the fallback skeleton using lines only
        if (GetIsMeshRenderingSupported() == false)
        {
            RenderSimpleSkeleton(actorInstance, visibleNodeIndices, selectedNodeIndices, color, selectedColor, true);
            return;
        }

        // get the actor it is an instance from
        EMotionFX::Actor* actor = actorInstance->GetActor();
        EMotionFX::Skeleton* skeleton = actor->GetSkeleton();

        // get the transform data
        EMotionFX::TransformData* transformData = actorInstance->GetTransformData();

        // iterate through all enabled nodes
        MCore::RGBAColor tempColor;
        const uint32 numEnabled = actorInstance->GetNumEnabledNodes();
        for (uint32 i = 0; i < numEnabled; ++i)
        {
            EMotionFX::Node*    node            = skeleton->GetNode(actorInstance->GetEnabledNode(i));
            const uint32        nodeIndex       = node->GetNodeIndex();
            const uint32        parentIndex     = node->GetParentIndex();

            // check if this node has a parent and is a bone, if not skip it
            if (parentIndex == MCORE_INVALIDINDEX32 || boneList.Find(nodeIndex) == MCORE_INVALIDINDEX32)
            {
                continue;
            }

            // check if the node is visible
            if ((visibleNodeIndices == NULL) ||
                (visibleNodeIndices->GetIsEmpty()) ||
                (visibleNodeIndices->GetIsEmpty() == false && visibleNodeIndices->Find(nodeIndex) != MCORE_INVALIDINDEX32))
            {
                const AZ::Vector3       nodeGlobalPos       = transformData->GetGlobalInclusiveMatrix(nodeIndex).GetTranslation();
                const AZ::Vector3       parentGlobalPos     = transformData->GetGlobalInclusiveMatrix(parentIndex).GetTranslation();
                const AZ::Vector3       bone                = parentGlobalPos - nodeGlobalPos;
                const AZ::Vector3       boneDirection       = MCore::SafeNormalize(bone);
                const float             boneLength          = MCore::SafeLength(bone);
                const float             boneScale           = GetBoneScale(actorInstance, node);
                const float             parentBoneScale     = GetBoneScale(actorInstance, skeleton->GetNode(parentIndex));
                const float             cylinderSize        = boneLength - boneScale - parentBoneScale;
                const AZ::Vector3       boneStartPosition   = nodeGlobalPos + boneDirection * boneScale;

                // check if the current bone is selected and set the color according to it
                if (selectedNodeIndices != NULL && selectedNodeIndices->GetIsEmpty() == false && selectedNodeIndices->Find(parentIndex) != MCORE_INVALIDINDEX32)
                {
                    tempColor = selectedColor;
                }
                else
                {
                    tempColor = color;
                }

                // render the bone cylinder, the cylinder will be directed towards the node's parent and must fit between the spheres
                RenderCylinder(boneScale, parentBoneScale, cylinderSize, boneStartPosition, boneDirection, tempColor);

                // check if the current bone is selected and set the color according to it
                if (selectedNodeIndices != NULL && selectedNodeIndices->GetIsEmpty() == false && selectedNodeIndices->Find(nodeIndex) != MCORE_INVALIDINDEX32)
                {
                    tempColor = selectedColor;
                }
                else
                {
                    tempColor = color;
                }

                // render the joint sphere
                RenderSphere(nodeGlobalPos, boneScale, tempColor);
            }
        }
    }


    // render node orientations
    void RenderUtil::RenderNodeOrientations(EMotionFX::ActorInstance* actorInstance, const MCore::Array<uint32>& boneList, MCore::Array<uint32>* visibleNodeIndices, MCore::Array<uint32>* selectedNodeIndices, float scale, bool scaleBonesOnLength)
    {
        // get the actor and the transform data
        const float unitScale = 1.0f / (float)MCore::Distance::ConvertValue(1.0f, MCore::Distance::UNITTYPE_METERS, EMotionFX::GetEMotionFX().GetUnitType());
        EMotionFX::Actor*           actor           = actorInstance->GetActor();
        EMotionFX::Skeleton*        skeleton        = actor->GetSkeleton();
        EMotionFX::TransformData*   transformData   = actorInstance->GetTransformData();
        const float                 constPreScale   = scale * unitScale * 3.0f;
        AxisRenderingSettings axisRenderingSettings;


        // iterate through all enabled nodes
        const uint32 numEnabled = actorInstance->GetNumEnabledNodes();
        for (uint32 i = 0; i < numEnabled; ++i)
        {
            EMotionFX::Node*    node            = skeleton->GetNode(actorInstance->GetEnabledNode(i));
            const uint32        nodeIndex       = node->GetNodeIndex();
            const uint32        parentIndex     = node->GetParentIndex();

            // check if the node is visible
            if ((visibleNodeIndices == NULL) ||
                (visibleNodeIndices->GetIsEmpty()) ||
                (visibleNodeIndices->GetIsEmpty() == false && visibleNodeIndices->Find(nodeIndex) != MCORE_INVALIDINDEX32))
            {
                // either scale the bones based on their length or use the normal size
                if (scaleBonesOnLength && parentIndex != MCORE_INVALIDINDEX32 && boneList.Find(nodeIndex) != MCORE_INVALIDINDEX32)
                {
                    axisRenderingSettings.mSize = GetBoneScale(actorInstance, node) * constPreScale * 50.0f;
                }
                else
                {
                    axisRenderingSettings.mSize = constPreScale;
                }

                // check if the current bone is selected and set the color according to it
                if (selectedNodeIndices != NULL && selectedNodeIndices->GetIsEmpty() == false && selectedNodeIndices->Find(nodeIndex) != MCORE_INVALIDINDEX32)
                {
                    axisRenderingSettings.mSelected = true;
                }
                else
                {
                    axisRenderingSettings.mSelected = false;
                }

                // set the global space matrix
                //axisRenderingSettings.mGlobalTM   = transformData->GetLocalMatrices()[nodeIndex];
                //axisRenderingSettings.mGlobalTM.SetTranslation( transformData->GetGlobalMatrices()[nodeIndex].GetTranslation() );
                axisRenderingSettings.mGlobalTM = transformData->GetGlobalInclusiveMatrices()[nodeIndex];

                // axis rendering
                RenderLineAxis(axisRenderingSettings);
            }
        }
    }


    // visualize the actor bind pose
    void RenderUtil::RenderBindPose(EMotionFX::ActorInstance* actorInstance, const MCore::Array<MCore::Matrix>& globalSpaceBindPoseMatrices, const MCore::RGBAColor& color, bool directlyRender)
    {
        // get the actor it is an instance from
        EMotionFX::Actor*       actor       = actorInstance->GetActor();
        EMotionFX::Skeleton*    skeleton    = actor->GetSkeleton();

        AxisRenderingSettings axisRenderingSettings;

        // iterate through all enabled nodes
        const uint32 numEnabled = actorInstance->GetNumEnabledNodes();
        for (uint32 i = 0; i < numEnabled; ++i)
        {
            EMotionFX::Node*    node            = skeleton->GetNode(actorInstance->GetEnabledNode(i));
            const uint32        nodeIndex       = node->GetNodeIndex();

            // render node orientation
            axisRenderingSettings.mSize     = GetBoneScale(actorInstance, node) * 5.0f;
            axisRenderingSettings.mGlobalTM = globalSpaceBindPoseMatrices[nodeIndex];
            RenderLineAxis(axisRenderingSettings);// line based axis rendering

            // skip root nodes for the line based skeleton rendering, you could also use curNode->IsRootNode()
            // but we use the parent index here, as we will reuse it
            uint32 parentIndex = node->GetParentIndex();
            if (parentIndex != MCORE_INVALIDINDEX32)
            {
                // draw a line between the current node and its parent node
                // here we first calculate the start and end position of the line, which are just the global space positions of the nodes
                AZ::Vector3 startPos     = globalSpaceBindPoseMatrices[ nodeIndex ].GetTranslation();
                AZ::Vector3 endPos       = globalSpaceBindPoseMatrices[ parentIndex ].GetTranslation();

                // finally draw the line
                RenderLine(startPos, endPos, color);
            }
        }

        if (directlyRender)
        {
            RenderLines();
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////

    // constructor
    RenderUtil::UtilMesh::UtilMesh()
    {
        mNumVertices    = 0;
        mNumIndices     = 0;
        mPositions      = NULL;
        mNormals        = NULL;
        mIndices        = NULL;
    }


    // destructor
    RenderUtil::UtilMesh::~UtilMesh()
    {
        delete[] mPositions;
        delete[] mNormals;
        delete[] mIndices;
    }


    // recalculate the normals of the mesh
    void RenderUtil::UtilMesh::CalculateNormals(bool counterClockWise)
    {
        // check if the normals actually got allocated
        if (mNormals == NULL)
        {
            return;
        }

        // reset all normals to the zero vector
        MCore::MemSet(mNormals, 0, sizeof(AZ::PackedVector3f) * mNumVertices);

        // iterate through all vertices and sum up the face normals
        uint32 i;
        uint32 indexA, indexB, indexC;
        AZ::Vector3 v1, v2, normal;
        for (i = 0; i < mNumIndices; i += 3)
        {
            indexA = mIndices[i];
            indexB = mIndices[i + (counterClockWise ? 1 : 2)];
            indexC = mIndices[i + (counterClockWise ? 2 : 1)];

            v1 = AZ::Vector3(mPositions[indexB]) - AZ::Vector3(mPositions[indexA]);
            v2 = AZ::Vector3(mPositions[indexC]) - AZ::Vector3(mPositions[indexA]);

            normal = v1.Cross(v2);

            mNormals[indexA] = AZ::PackedVector3f(AZ::Vector3(mNormals[indexA]) + normal);
            mNormals[indexB] = AZ::PackedVector3f(AZ::Vector3(mNormals[indexB]) + normal);
            mNormals[indexC] = AZ::PackedVector3f(AZ::Vector3(mNormals[indexC]) + normal);
        }

        // normalize all the normals
        for (i = 0; i < mNumVertices; ++i)
        {
            mNormals[i] = AZ::PackedVector3f(AZ::Vector3(mNormals[i]).GetNormalized());
        }
    }


    // allocate memory for the vertices, indices and normals
    void RenderUtil::UtilMesh::Allocate(uint32 numVertices, uint32 numIndices, bool hasNormals)
    {
        assert(numVertices > 0 && numIndices % 3 == 0);
        assert(mPositions == NULL && mIndices == NULL && mNormals == NULL);

        // copy over the number of vertices and indices
        mNumVertices = numVertices;
        mNumIndices = numIndices;

        // allocate the buffers
        mPositions  = new AZ::PackedVector3f[numVertices];
        mIndices    = new uint32[numIndices];
        if (hasNormals)
        {
            mNormals = new AZ::PackedVector3f[numVertices];
        }
    }


    // create a cylinder mesh
    RenderUtil::UtilMesh* RenderUtil::CreateCylinder(float baseRadius, float topRadius, float length, uint32 numSegments)
    {
        // create the util mesh for our cylinder
        UtilMesh* cylinderMesh = new UtilMesh();

        // allocate memory for the vertices, normals and indices
        const uint32    numVertices = numSegments * 2;
        const uint32    numIndices  = numSegments * 2 * 3;
        cylinderMesh->Allocate(numVertices, numIndices, true);

        // construct the actual mesh
        FillCylinder(cylinderMesh, baseRadius, topRadius, length, true);

        return cylinderMesh;
    }


    // fill the cylinder vertex and index buffers
    void RenderUtil::FillCylinder(UtilMesh* mesh, float baseRadius, float topRadius, float length, bool calculateNormals)
    {
        // check if the positions and the indices have been allocated already by the CreateCylinder() function
        if (mesh->mPositions == NULL || mesh->mIndices == NULL)
        {
            return;
        }

        // number of segments/sides of the cylinder
        const uint32 numSegments = mesh->mNumVertices / 2;

        // fill in the vertices
        uint32 i;
        for (i = 0; i < numSegments; ++i)
        {
            const float p = i / (float)numSegments * 2.0f * MCore::Math::pi;
            const float z = MCore::Math::Sin(p);
            const float y = MCore::Math::Cos(p);

            mesh->mPositions[i]               = AZ::PackedVector3f(0.0f,    y * baseRadius, z * baseRadius);
            mesh->mPositions[i + numSegments] = AZ::PackedVector3f(-length, y * topRadius,  z * topRadius);
        }

        // fill in the indices
        uint32 c = 0;
        for (i = 0; i < numSegments; ++i)
        {
            mesh->mIndices[c++] = i;
            mesh->mIndices[c++] = ((i + 1) % numSegments);
            mesh->mIndices[c++] = i + numSegments;
        }

        for (i = 0; i < numSegments; ++i)
        {
            mesh->mIndices[c++] = i + numSegments;
            mesh->mIndices[c++] = ((i + 1) % numSegments);
            mesh->mIndices[c++] = ((i + 1) % numSegments) + numSegments;
        }

        // recalculate normals if desired
        if (calculateNormals)
        {
            mesh->CalculateNormals();
        }
    }


    // render the given cylinder
    void RenderUtil::RenderCylinder(float baseRadius, float topRadius, float length, const AZ::Vector3& position, const AZ::Vector3& direction, const MCore::RGBAColor& color)
    {
        MCore::Matrix globalTM;

        // rotate the cylinder to the desired direction
        if (MCore::Compare<AZ::Vector3>::CheckIfIsClose(direction, AZ::Vector3(1.0f, 0.0f, 0.0f), MCore::Math::epsilon) == false)
        {
            globalTM.SetRotationMatrixAxisAngle(AZ::Vector3(-1.0f, 0.0f, 0.0f).Cross(direction), MCore::Math::ACos(direction.Dot(AZ::Vector3(-1.0f, 0.0f, 0.0f))));
        }
        else
        {
            globalTM.SetRotationMatrix(MCore::Quaternion(0.0f, 0.0f, MCore::Math::DegreesToRadians(180.0f)));
        }

        // set the cylinder to the given position
        globalTM.SetTranslation(position);

        // render the cylinder
        RenderCylinder(baseRadius, topRadius, length, color, globalTM);
    }


    // construct a sphere mesh
    RenderUtil::UtilMesh* RenderUtil::CreateSphere(float radius, uint32 numSegments)
    {
        uint32 i;

        // create our util mesh for the sphere
        UtilMesh* sphereMesh = new UtilMesh();

        // calculate the number of vertices and indices
        const uint32 numVertices =  (numSegments - 2) * numSegments + 2;
        uint32 numIndices       =  (numSegments - 3) * 6;
        numIndices              += (numSegments - 3) * (numSegments - 1) * 6;
        numIndices              += (numSegments - 1) * 3;
        numIndices              += (numSegments - 1) * 3;
        numIndices              += 6;

        // allocate memory for the vertices, normals and indices
        sphereMesh->Allocate(numVertices, numIndices, true);

        // fill the vertices
        for (i = 1; i < numSegments - 1; ++i)
        {
            const float z = (1.0f - (i / (float)(numSegments - 1)) * 2.0f);
            const float r = MCore::Math::Sin(MCore::Math::ACos(z)) * radius;

            for (uint32 j = 0; j < numSegments; j++)
            {
                const float p = (j / (float)numSegments) * MCore::Math::pi * 2.0f;
                const float x = r * MCore::Math::Sin(p);
                const float y = r * MCore::Math::Cos(p);

                sphereMesh->mPositions[(i - 1) * numSegments + j] = AZ::PackedVector3f(x, y, z * radius);
            }
        }

        // the highest and lowest vertices
        sphereMesh->mPositions[(numSegments - 2) * numSegments + 0]   = AZ::PackedVector3f(0.0f, 0.0f,  radius);
        sphereMesh->mPositions[(numSegments - 2) * numSegments + 1]   = AZ::PackedVector3f(0.0f, 0.0f, -radius);

        // calculate normals
        for (i = 0; i < sphereMesh->mNumVertices; ++i)
        {
            sphereMesh->mNormals[i] = AZ::PackedVector3f(-AZ::Vector3(sphereMesh->mPositions[i]).GetNormalized());
        }

        // fill the indices
        uint32 c = 0;
        for (i = 1; i < numSegments - 2; ++i)
        {
            for (uint32 j = 0; j < numSegments - 1; j++)
            {
                sphereMesh->mIndices[c++] = (i - 1) * numSegments + j;
                sphereMesh->mIndices[c++] = (i - 1) * numSegments + j + 1;
                sphereMesh->mIndices[c++] = i * numSegments + j;

                sphereMesh->mIndices[c++] = (i - 1) * numSegments + j + 1;
                sphereMesh->mIndices[c++] = i * numSegments + j + 1;
                sphereMesh->mIndices[c++] = i * numSegments + j;
            }

            sphereMesh->mIndices[c++] = (i - 1) * numSegments + numSegments - 1;
            sphereMesh->mIndices[c++] = (i - 1) * numSegments;
            sphereMesh->mIndices[c++] = i * numSegments + numSegments - 1;

            sphereMesh->mIndices[c++] = i * numSegments;
            sphereMesh->mIndices[c++] = (i - 1) * numSegments;
            sphereMesh->mIndices[c++] = i * numSegments + numSegments - 1;
        }

        // highest and deepest indices
        for (i = 0; i < numSegments - 1; ++i)
        {
            sphereMesh->mIndices[c++] = i;
            sphereMesh->mIndices[c++] = i + 1;
            sphereMesh->mIndices[c++] = (numSegments - 2) * numSegments;
        }

        sphereMesh->mIndices[c++] = numSegments - 1;
        sphereMesh->mIndices[c++] = 0;
        sphereMesh->mIndices[c++] = (numSegments - 2) * numSegments;

        for (i = 0; i < numSegments - 1; ++i)
        {
            sphereMesh->mIndices[c++] = (numSegments - 3) * numSegments + i;
            sphereMesh->mIndices[c++] = (numSegments - 3) * numSegments + i + 1;
            sphereMesh->mIndices[c++] = (numSegments - 2) * numSegments + 1;
        }

        sphereMesh->mIndices[c++] = (numSegments - 3) * numSegments + (numSegments - 1);
        sphereMesh->mIndices[c++] = (numSegments - 3) * numSegments;
        sphereMesh->mIndices[c++] = (numSegments - 2) * numSegments + 1;

        return sphereMesh;
    }


    // render the given sphere
    void RenderUtil::RenderSphere(const AZ::Vector3& position, float radius, const MCore::RGBAColor& color)
    {
        // setup the global space matrix of the sphere
        MCore::Matrix sphereMatrix;
        sphereMatrix.SetScaleMatrix(AZ::Vector3(radius, radius, radius));
        sphereMatrix.SetTranslation(position);

        // render the sphere
        RenderSphere(color, sphereMatrix);
    }


    // render a circle using the RenderLine and RenderTriangles function
    void RenderUtil::RenderCircle(const MCore::Matrix& globalTM, float radius, uint32 numSegments, const MCore::RGBAColor& color, float startAngle, float endAngle, bool fillCircle, const MCore::RGBAColor& fillColor, bool cullFaces, const AZ::Vector3& camRollAxis)
    {
        // if culling is enabled but cam roll axis has not been set return without rendering
        if (cullFaces && camRollAxis == AZ::Vector3::CreateZero())
        {
            return;
        }

        // calculate the angle and step size
        const float angleRange = endAngle - startAngle;

        // if the angle range is too small to see, return directly
        if (angleRange - MCore::Math::epsilon < 0.0f)
        {
            return;
        }

        const float stepSize    = angleRange / (float)(numSegments * (angleRange / MCore::Math::twoPi));

        // render circle segments within the calculated range
        for (float i = startAngle; i < endAngle - stepSize; i += stepSize)
        {
            // calculate the origin relative position
            const float p1 = i;
            const float x1 = radius * MCore::Math::Sin(p1);
            const float y1 = radius * MCore::Math::Cos(p1);
            const float p2 = i + stepSize;
            const float x2 = radius * MCore::Math::Sin(p2);
            const float y2 = radius * MCore::Math::Cos(p2);

            // convert position values into 3D vectors
            AZ::Vector3 pos1(x1, y1, 0.0f);
            AZ::Vector3 pos2(x2, y2, 0.0f);

            // perform global transformation
            pos1 = globalTM.Mul3x3(pos1) + globalTM.GetTranslation();
            pos2 = globalTM.Mul3x3(pos2) + globalTM.GetTranslation();

            // render line segment
            if (cullFaces == false ||
                MCore::InRange(MCore::Math::ACos((pos2 - globalTM.GetTranslation()).GetNormalized().Dot(camRollAxis)), MCore::Math::halfPi - (MCore::Math::halfPi / 18.0f), MCore::Math::pi))
            {
                RenderLine(pos1, pos2, color);
            }

            if (fillCircle)
            {
                RenderTriangle(globalTM.GetTranslation(), pos2, pos1, fillColor);
            }
        }
    }


    // construct a cube mesh
    RenderUtil::UtilMesh* RenderUtil::CreateCube(float size)
    {
        // set number of triangles and vertices
        const uint32 numVertices    = 8;
        const uint32 numTriangles   = 12;

        // create the mesh
        UtilMesh* mesh = new UtilMesh();
        mesh->Allocate(numVertices, numTriangles * 3, true);

        // define the vertices
        mesh->mPositions[0] = AZ::PackedVector3f(AZ::Vector3(-0.5f, -0.5f, -0.5f) * size);
        mesh->mPositions[1] = AZ::PackedVector3f(AZ::Vector3(0.5f, -0.5f, -0.5f) * size);
        mesh->mPositions[2] = AZ::PackedVector3f(AZ::Vector3(0.5f, 0.5f, -0.5f) * size);
        mesh->mPositions[3] = AZ::PackedVector3f(AZ::Vector3(-0.5f, 0.5f, -0.5f) * size);
        mesh->mPositions[4] = AZ::PackedVector3f(AZ::Vector3(-0.5f, -0.5f, 0.5f) * size);
        mesh->mPositions[5] = AZ::PackedVector3f(AZ::Vector3(0.5f, -0.5f, 0.5f) * size);
        mesh->mPositions[6] = AZ::PackedVector3f(AZ::Vector3(0.5f, 0.5f, 0.5f) * size);
        mesh->mPositions[7] = AZ::PackedVector3f(AZ::Vector3(-0.5f, 0.5f, 0.5f) * size);

        // define the indices
        mesh->mIndices[0]   = 0;
        mesh->mIndices[1]  = 1;
        mesh->mIndices[2]  = 2;
        mesh->mIndices[3]   = 0;
        mesh->mIndices[4]  = 2;
        mesh->mIndices[5]  = 3;
        mesh->mIndices[6]   = 1;
        mesh->mIndices[7]  = 5;
        mesh->mIndices[8]  = 6;
        mesh->mIndices[9]   = 1;
        mesh->mIndices[10] = 6;
        mesh->mIndices[11] = 2;
        mesh->mIndices[12]  = 5;
        mesh->mIndices[13] = 4;
        mesh->mIndices[14] = 7;
        mesh->mIndices[15]  = 5;
        mesh->mIndices[16] = 7;
        mesh->mIndices[17] = 6;
        mesh->mIndices[18]  = 4;
        mesh->mIndices[19] = 0;
        mesh->mIndices[20] = 3;
        mesh->mIndices[21]  = 4;
        mesh->mIndices[22] = 3;
        mesh->mIndices[23] = 7;
        mesh->mIndices[24]  = 1;
        mesh->mIndices[25] = 0;
        mesh->mIndices[26] = 4;
        mesh->mIndices[27]  = 1;
        mesh->mIndices[28] = 4;
        mesh->mIndices[29] = 5;
        mesh->mIndices[30]  = 3;
        mesh->mIndices[31] = 2;
        mesh->mIndices[32] = 6;
        mesh->mIndices[33]  = 3;
        mesh->mIndices[34] = 6;
        mesh->mIndices[35] = 7;

        // calculate the normals
        mesh->CalculateNormals();

        // return the mesh
        return mesh;
    }


    // render a cube
    void RenderUtil::RenderCube(const AZ::Vector3& size, const AZ::Vector3& position, const MCore::RGBAColor& color)
    {
        // setup the global space matrix of the cube
        MCore::Matrix cubeMatrix;
        cubeMatrix.SetScaleMatrix(size);
        cubeMatrix.SetTranslation(position);

        // render the cube
        RenderCube(color, cubeMatrix);
    }


    // construct the arrow head mesh used for rendering
    RenderUtil::UtilMesh* RenderUtil::CreateArrowHead(float height, float radius)
    {
        const uint32 numSegments    = 12;
        const uint32 numTriangles   = numSegments * 2;
        const uint32 numVertices    = numTriangles * 3;

        // construct the arrow head mesh and allocate memory for the vertices, indices and normals
        UtilMesh* mesh = new UtilMesh();
        mesh->Allocate(numVertices, numVertices, true);

        // fill in the indices
        for (uint32 i = 0; i < numVertices; ++i)
        {
            mesh->mIndices[i] = i;
        }

        // fill in the vertices and recalculate the normals
        FillArrowHead(mesh, height, radius, true);

        return mesh;
    }


    // fill the arrow head mesh vertices, indices and recalculate the normals if desired
    void RenderUtil::FillArrowHead(UtilMesh* mesh, float height, float radius, bool calculateNormals)
    {
        static AZ::Vector3      points[12];
        uint32                  pointNr     = 0;
        const uint32            numVertices = mesh->mNumVertices;
        const uint32            numTriangles = numVertices / 3;
        assert(numTriangles * 3 == numVertices);
        const uint32            numSegments = numTriangles / 2;
        assert(numSegments * 2 == numTriangles);
        const uint32            angleStep   = 30;
        assert(360 / angleStep == numSegments);

        // check and prevent the radius being greater than 30% of the height
        if (radius > (height * 0.3f))
        {
            radius = height * 0.3f;
        }

        // construct the segment points
        for (uint32 angle = angleStep; angle <= 360; angle += angleStep)
        {
            float theta     = MCore::Math::DegreesToRadians(static_cast<float>(angle));
            float x         = MCore::Math::Cos(theta) * radius;
            float z         = MCore::Math::Sin(theta) * radius;
            points[pointNr] = AZ::Vector3(x, 0.0f, z);
            pointNr++;
        }

        // get some data used for constructing the arrow head mesh
        uint32  vertexNr;
        AZ::Vector3 segmentPoint;
        //AZ::Vector3 center     = AZ::Vector3(0.0f, height * 0.25f, 0.0f);   // real arrow head
        AZ::Vector3 center           = AZ::Vector3::CreateZero();     // normal cone
        AZ::Vector3 top              = AZ::Vector3(0.0f, height, 0.0f);
        AZ::Vector3 previousPoint    = points[(numSegments - 1)];

        for (uint32 i = 0; i < numSegments; ++i)
        {
            // preprocess data
            segmentPoint    = points[i];
            vertexNr        = i * 6;

            // triangle 1
            mesh->mPositions[vertexNr + 0] = AZ::PackedVector3f(segmentPoint);
            mesh->mPositions[vertexNr + 1] = AZ::PackedVector3f(previousPoint);
            mesh->mPositions[vertexNr + 2] = AZ::PackedVector3f(center);

            // triangle 2
            mesh->mPositions[vertexNr + 3] = AZ::PackedVector3f(previousPoint);
            mesh->mPositions[vertexNr + 4] = AZ::PackedVector3f(segmentPoint);
            mesh->mPositions[vertexNr + 5] = AZ::PackedVector3f(top);

            // postprocess data
            previousPoint = segmentPoint;
        }

        // recalculate the normals if desired
        if (calculateNormals)
        {
            mesh->CalculateNormals();
        }
    }


    // render the given arrow head
    void RenderUtil::RenderArrowHead(float height, float radius, const AZ::Vector3& position, const AZ::Vector3& direction, const MCore::RGBAColor& color)
    {
        MCore::Matrix globalTM;

        // rotate the arrow head to the desired direction
        if (MCore::Compare<AZ::Vector3>::CheckIfIsClose(direction, AZ::Vector3(0.0f, -1.0f, 0.0f), MCore::Math::epsilon) == false)
        {
            globalTM.SetRotationMatrixAxisAngle(AZ::Vector3(0.0f, 1.0f, 0.0f).Cross(direction), MCore::Math::ACos(direction.Dot(AZ::Vector3(0.0f, 1.0f, 0.0f))));
        }
        else
        {
            globalTM.SetRotationMatrix(MCore::Quaternion(MCore::Math::DegreesToRadians(180.0f), 0.0f, 0.0f));
        }

        // translate the arrow head to the given position
        globalTM.SetTranslation(position);

        // render the arrow head
        RenderArrowHead(height, radius, color, globalTM);
    }


    // render mesh based axis
    void RenderUtil::RenderAxis(float size, const AZ::Vector3& position, const AZ::Vector3& right, const AZ::Vector3& up, const AZ::Vector3& forward)
    {
        const float                     zeroSphereRadius    = size * 0.075f;
        const float                     arrowHeadRadius     = size * 0.1f;
        const float                     arrowHeadHeight     = size * 0.3f;
        const float                     axisCylinderRadius  = size * 0.02f;
        const float                     axisCylinderHeight  = size * 0.7f + arrowHeadHeight * 0.25f;
        static const MCore::RGBAColor   xAxisColor(1.0f, 0.0f, 0.0f);
        static const MCore::RGBAColor   yAxisColor(0.0f, 1.0f, 0.0f);
        static const MCore::RGBAColor   zAxisColor(0.0f, 0.0f, 1.0f);
        static const MCore::RGBAColor   centerColor(0.5f, 0.5f, 0.5f);

        // render zero/center sphere
        RenderSphere(position, zeroSphereRadius, centerColor);

        // render cylinder axis
        RenderCylinder(axisCylinderRadius, axisCylinderRadius, axisCylinderHeight, position, right, xAxisColor);
        RenderCylinder(axisCylinderRadius, axisCylinderRadius, axisCylinderHeight, position, up, yAxisColor);
        RenderCylinder(axisCylinderRadius, axisCylinderRadius, axisCylinderHeight, position, forward, zAxisColor);

        // render axis arrow heads
        RenderArrowHead(arrowHeadHeight, arrowHeadRadius, position + right * (axisCylinderHeight - 0.25f * arrowHeadHeight), right, xAxisColor);
        RenderArrowHead(arrowHeadHeight, arrowHeadRadius, position + up * (axisCylinderHeight - 0.25f * arrowHeadHeight), up, yAxisColor);
        RenderArrowHead(arrowHeadHeight, arrowHeadRadius, position + forward * (axisCylinderHeight - 0.25f * arrowHeadHeight), forward, zAxisColor);
    }


    // constructor
    RenderUtil::AxisRenderingSettings::AxisRenderingSettings()
    {
        mSize           = 1.0f;
        mRenderXAxis    = true;
        mRenderYAxis    = true;
        mRenderZAxis    = true;
        mRenderXAxisName = false;
        mRenderYAxisName = false;
        mRenderZAxisName = false;
        mSelected       = false;
    }


    // render line based axis
    void RenderUtil::RenderLineAxis(const AxisRenderingSettings& settings)
    {
        const float             size                = settings.mSize;
        const MCore::Matrix&    globalTM            = settings.mGlobalTM;
        const AZ::Vector3&      cameraRight         = settings.mCameraRight;
        const AZ::Vector3&      cameraUp            = settings.mCameraUp;
        const float             arrowHeadRadius     = size * 0.1f;
        const float             arrowHeadHeight     = size * 0.3f;
        const float             axisHeight          = size * 0.7f;
        const AZ::Vector3       position            = globalTM.GetTranslation();

        if (settings.mRenderXAxis)
        {
            // set the color
            MCore::RGBAColor xAxisColor = MCore::RGBAColor(1.0f, 0.0f, 0.0f);

            MCore::RGBAColor xSelectedColor;
            if (settings.mSelected)
            {
                xSelectedColor = MCore::RGBAColor(1.0f, 0.647f, 0.0f);
            }
            else
            {
                xSelectedColor = xAxisColor;
            }

            const AZ::Vector3       xAxisDir = (AZ::Vector3(size, 0.0f, 0.0f) * globalTM - position).GetNormalized();
            const AZ::Vector3       xAxisArrowStart = position + xAxisDir * axisHeight;
            RenderArrowHead(arrowHeadHeight, arrowHeadRadius, xAxisArrowStart, xAxisDir, xSelectedColor);
            RenderLine(position, xAxisArrowStart, xAxisColor);

            if (settings.mRenderXAxisName)
            {
                const AZ::Vector3 xNamePos = position + xAxisDir * (size * 1.15f);
                RenderLine(xNamePos + cameraUp * (-0.15f * size) + cameraRight * (0.1f * size),    xNamePos + cameraUp * (0.15f * size) + cameraRight * (-0.1f * size),    xAxisColor);
                RenderLine(xNamePos + cameraUp * (-0.15f * size) + cameraRight * (-0.1f * size),   xNamePos + cameraUp * (0.15f * size) + cameraRight * (0.1f * size),     xAxisColor);
            }
        }

        if (settings.mRenderYAxis)
        {
            // set the color
            MCore::RGBAColor yAxisColor = MCore::RGBAColor(0.0f, 1.0f, 0.0f);

            MCore::RGBAColor ySelectedColor;
            if (settings.mSelected)
            {
                ySelectedColor = MCore::RGBAColor(1.0f, 0.647f, 0.0f);
            }
            else
            {
                ySelectedColor = yAxisColor;
            }

            const AZ::Vector3       yAxisDir = (AZ::Vector3(0.0f, size, 0.0f) * globalTM - position).GetNormalized();
            const AZ::Vector3       yAxisArrowStart = position + yAxisDir * axisHeight;
            RenderArrowHead(arrowHeadHeight, arrowHeadRadius, yAxisArrowStart, yAxisDir, ySelectedColor);
            RenderLine(position, yAxisArrowStart, yAxisColor);

            if (settings.mRenderYAxisName)
            {
                const AZ::Vector3 yNamePos = position + yAxisDir * (size * 1.15f);
                RenderLine(yNamePos,   yNamePos + cameraRight * (-0.1f * size) + cameraUp * (0.15f * size),    yAxisColor);
                RenderLine(yNamePos,   yNamePos + cameraRight * (0.1f * size) + cameraUp * (0.15f * size),     yAxisColor);
                RenderLine(yNamePos,   yNamePos + cameraUp * (-0.15f * size),                              yAxisColor);
            }
        }

        if (settings.mRenderZAxis)
        {
            // set the color
            MCore::RGBAColor zAxisColor = MCore::RGBAColor(0.0f, 0.0f, 1.0f);

            MCore::RGBAColor zSelectedColor;
            if (settings.mSelected)
            {
                zSelectedColor = MCore::RGBAColor(1.0f, 0.647f, 0.0f);
            }
            else
            {
                zSelectedColor = zAxisColor;
            }

            const AZ::Vector3       zAxisDir = (AZ::Vector3(0.0f, 0.0f, size) * globalTM - position).GetNormalized();
            const AZ::Vector3       zAxisArrowStart = position + zAxisDir * axisHeight;
            RenderArrowHead(arrowHeadHeight, arrowHeadRadius, zAxisArrowStart, zAxisDir, zSelectedColor);
            RenderLine(position, zAxisArrowStart, zAxisColor);

            if (settings.mRenderZAxisName)
            {
                const AZ::Vector3 zNamePos = position + zAxisDir * (size * 1.15f);
                RenderLine(zNamePos + cameraRight * (-0.1f * size) + cameraUp * (0.15f * size),    zNamePos + cameraRight * (0.1f * size) + cameraUp * (0.15f * size),            zAxisColor);
                RenderLine(zNamePos + cameraRight * (0.1f * size) + cameraUp * (0.15f * size),    zNamePos + cameraRight * (-0.1f * size) + cameraUp * (-0.15f * size),           zAxisColor);
                RenderLine(zNamePos + cameraRight * (-0.1f * size) + cameraUp * (-0.15f * size),   zNamePos + cameraRight * (0.1f * size) + cameraUp * (-0.15f * size),           zAxisColor);
            }
        }
    }


    // calculate the visible area of the grid for the given camera
    void RenderUtil::CalcVisibleGridArea(Camera* camera, uint32 screenWidth, uint32 screenHeight, float unitSize, AZ::Vector2* outGridStart, AZ::Vector2* outGridEnd)
    {
        // render the grid
        AZ::Vector2 gridStart(0.0f, 0.0f);
        AZ::Vector2 gridEnd(0.0f, 0.0f);
        if (camera->GetType() == MCommon::OrthographicCamera::TYPE_ID)
        {
            MCore::Matrix invProj = camera->GetProjectionMatrix();
            MCore::Matrix invView = camera->GetViewMatrix();

            AZ::Vector3 a = UnprojectOrtho(0.0f, 0.0f, static_cast<float>(screenWidth), static_cast<float>(screenHeight), -1.0f, invProj, invView);
            AZ::Vector3 b = UnprojectOrtho(static_cast<float>(screenWidth), static_cast<float>(screenHeight), static_cast<float>(screenWidth), static_cast<float>(screenHeight), 1.0f, invProj, invView);

            OrthographicCamera* orthoCamera = static_cast<OrthographicCamera*>(camera);
            switch (orthoCamera->GetMode())
            {
            case OrthographicCamera::VIEWMODE_FRONT:
            {
                gridStart.SetX(MCore::Min(a.GetX(), b.GetX()) - unitSize);
                gridStart.SetY(MCore::Min(a.GetY(), b.GetY()) - unitSize);
                gridEnd.SetX(MCore::Max(a.GetX(), b.GetX()) + unitSize);
                gridEnd.SetY(MCore::Max(a.GetY(), b.GetY()) + unitSize);
                break;
            }
            case OrthographicCamera::VIEWMODE_BACK:
            {
                a = AZ::Vector3(a.GetX(), -a.GetY(), a.GetZ());
                b = AZ::Vector3(b.GetX(), -b.GetY(), b.GetZ());
                gridStart.SetX(MCore::Min(a.GetX(), b.GetX()) - unitSize);
                gridStart.SetY(MCore::Min(a.GetY(), b.GetY()) - unitSize);
                gridEnd.SetX(MCore::Max(a.GetX(), b.GetX()) + unitSize);
                gridEnd.SetY(MCore::Max(a.GetY(), b.GetY()) + unitSize);
                break;
            }
            case OrthographicCamera::VIEWMODE_LEFT:
            {
                a = AZ::Vector3(a.GetX(), -a.GetY(), a.GetZ());
                b = AZ::Vector3(b.GetX(), -b.GetY(), b.GetZ());
                gridStart.SetX(MCore::Min(a.GetY(), b.GetY()) - unitSize);
                gridStart.SetY(MCore::Min(a.GetZ(), b.GetZ()) - unitSize);
                gridEnd.SetX(MCore::Max(a.GetY(), b.GetY()) + unitSize);
                gridEnd.SetY(MCore::Max(a.GetZ(), b.GetZ()) + unitSize);
                break;
            }
            case OrthographicCamera::VIEWMODE_RIGHT:
            {
                gridStart.SetX(MCore::Min(a.GetY(), b.GetY()) - unitSize);
                gridStart.SetY(MCore::Min(a.GetZ(), b.GetZ()) - unitSize);
                gridEnd.SetX(MCore::Max(a.GetY(), b.GetY()) + unitSize);
                gridEnd.SetY(MCore::Max(a.GetZ(), b.GetZ()) + unitSize);
                break;
            }
            case OrthographicCamera::VIEWMODE_TOP:
            {
                gridStart.SetX(MCore::Min(a.GetX(), b.GetX()) - unitSize);
                gridStart.SetY(MCore::Min(a.GetZ(), b.GetZ()) - unitSize);
                gridEnd.SetX(MCore::Max(a.GetX(), b.GetX()) + unitSize);
                gridEnd.SetY(MCore::Max(a.GetZ(), b.GetZ()) + unitSize);
                break;
            }
            case OrthographicCamera::VIEWMODE_BOTTOM:
            {
                gridStart.SetX(MCore::Min(a.GetX(), b.GetX()) - unitSize);
                gridStart.SetY(MCore::Min(a.GetZ(), b.GetZ()) - unitSize);
                gridEnd.SetX(MCore::Max(a.GetX(), b.GetX()) + unitSize);
                gridEnd.SetY(MCore::Max(a.GetZ(), b.GetZ()) + unitSize);
                break;
            }
            }
        }
        else
        {
            const float screenWidth     = static_cast<float>(camera->GetScreenWidth());
            const float screenHeight    = static_cast<float>(camera->GetScreenHeight());

            // find the 4 corners of the frustum
            AZ::Vector3 corners[4];
            corners[0] = MCore::Unproject(0.0f, 0.0f, screenWidth, screenHeight, camera->GetFarClipDistance(), camera->GetProjectionMatrix().Inversed(), camera->GetViewMatrix().Inversed());
            corners[1] = MCore::Unproject(screenWidth, 0.0f, screenWidth, screenHeight, camera->GetFarClipDistance(), camera->GetProjectionMatrix().Inversed(), camera->GetViewMatrix().Inversed());
            corners[2] = MCore::Unproject(screenWidth, screenHeight, screenWidth, screenHeight, camera->GetFarClipDistance(), camera->GetProjectionMatrix().Inversed(), camera->GetViewMatrix().Inversed());
            corners[3] = MCore::Unproject(0.0f, screenHeight, screenWidth, screenHeight, camera->GetFarClipDistance(), camera->GetProjectionMatrix().Inversed(), camera->GetViewMatrix().Inversed());

            // calculate the intersection points with the ground plane and create an AABB around those
            // if there is no intersection point then use the ray target as point, which is the projection onto the far plane basically
            MCore::AABB aabb;
            AZ::Vector3 intersectionPoint;
            MCore::PlaneEq groundPlane(AZ::Vector3(0.0f, 0.0f, 1.0f), AZ::Vector3::CreateZero());
            for (uint32 i = 0; i < 4; ++i)
            {
                if (MCore::Ray(camera->GetPosition(), corners[i]).Intersects(groundPlane, &intersectionPoint))
                {
                    corners[i] = intersectionPoint;
                }

                aabb.Encapsulate(corners[i]);
            }

            // set the grid start and end values
            gridStart.SetX(aabb.GetMin().GetX() - unitSize);
            gridStart.SetY(aabb.GetMin().GetY() - unitSize);
            gridEnd.SetX(aabb.GetMax().GetX() + unitSize);
            gridEnd.SetY(aabb.GetMax().GetY() + unitSize);
        }

        *outGridStart = gridStart;
        *outGridEnd = gridEnd;
    }


    // get aabb which includes all actor instances
    MCore::AABB RenderUtil::CalcSceneAABB()
    {
        MCore::AABB finalAABB;

        // get the number of actor instances and iterate through them
        const uint32 numActorInstances = EMotionFX::GetActorManager().GetNumActorInstances();
        for (uint32 i = 0; i < numActorInstances; ++i)
        {
            // get the actor instance and update its transformations and meshes
            EMotionFX::ActorInstance* actorInstance = EMotionFX::GetActorManager().GetActorInstance(i);

            if (actorInstance->GetIsOwnedByRuntime())
            {
                continue;
            }

            actorInstance->UpdateTransformations(0.0f, true);
            actorInstance->UpdateMeshDeformers(0.0f);

            // get the mesh based bounding box
            MCore::AABB boundingBox;
            actorInstance->CalcMeshBasedAABB(actorInstance->GetLODLevel(), &boundingBox);

            // in case there aren't any meshes, use the node based bounding box
            if (boundingBox.CheckIfIsValid() == false)
            {
                actorInstance->CalcNodeBasedAABB(&boundingBox);
            }

            // make sure the actor instance is covered in our global bounding box
            finalAABB.Encapsulate(boundingBox);
        }

        return finalAABB;
    }


    // visualize the trajectory
    void RenderUtil::RenderTrajectory(const MCore::Matrix& globalTM, const MCore::RGBAColor& innerColor, const MCore::RGBAColor& borderColor, float scale)
    {
        // get the position and some direction vectors of the trajectory node matrix
        const AZ::Vector3 center        = globalTM.GetTranslation();
        const AZ::Vector3 forward       = globalTM.GetRight().GetNormalized();
        const AZ::Vector3 right         = globalTM.GetForward().GetNormalized();
        const float trailWidthHalf      = 0.5f;
        const float trailLengh          = 2.0f;
        const float arrowWidthHalf      = 1.5f;
        const float arrowLength         = 2.0f;

        AZ::Vector3 vertices[7];
        /*
            //              4
            //             / \
            //            /   \
            //          /       \
            //        /           \
            //      /               \
            //    5-----6       2-----3
            //          |       |
            //          |       |
            //          |       |
            //          |       |
            //          |       |
            //         0---------1
        */
        // construct the arrow vertices
        vertices[0] = center + AZ::Vector3(-right * trailWidthHalf - forward * trailLengh) * scale;
        vertices[1] = center + AZ::Vector3(right * trailWidthHalf - forward * trailLengh) * scale;
        vertices[2] = center + AZ::Vector3(right * trailWidthHalf) * scale;
        vertices[3] = center + AZ::Vector3(right * arrowWidthHalf) * scale;
        vertices[4] = center + AZ::Vector3(forward * arrowLength) * scale;
        vertices[5] = center + AZ::Vector3(-right * arrowWidthHalf) * scale;
        vertices[6] = center + AZ::Vector3(-right * trailWidthHalf) * scale;

        // render the solid arrow
        RenderTriangle(vertices[0], vertices[1], vertices[2], innerColor);
        RenderTriangle(vertices[2], vertices[6], vertices[0], innerColor);
        RenderTriangle(vertices[3], vertices[4], vertices[2], innerColor);
        RenderTriangle(vertices[2], vertices[4], vertices[6], innerColor);
        RenderTriangle(vertices[6], vertices[4], vertices[5], innerColor);

        // render the border
        RenderLine(vertices[0], vertices[1], borderColor);
        RenderLine(vertices[1], vertices[2], borderColor);
        RenderLine(vertices[2], vertices[3], borderColor);
        RenderLine(vertices[3], vertices[4], borderColor);
        RenderLine(vertices[4], vertices[5], borderColor);
        RenderLine(vertices[5], vertices[6], borderColor);
        RenderLine(vertices[6], vertices[0], borderColor);
    }


    // visualize the trajectory
    void RenderUtil::RenderTrajectory(EMotionFX::ActorInstance* actorInstance, const MCore::RGBAColor& innerColor, const MCore::RGBAColor& borderColor, float scale)
    {
        EMotionFX::Actor*   actor     = actorInstance->GetActor();
        const uint32        nodeIndex = actor->GetMotionExtractionNodeIndex();

        // in case the motion extraction node is not set, return directly
        if (nodeIndex == MCORE_INVALIDINDEX32)
        {
            return;
        }

        // get the global TM for the trajectory
        EMotionFX::Transform transform = actorInstance->GetTransformData()->GetCurrentPose()->GetGlobalTransformInclusive(nodeIndex).ProjectedToGroundPlane();
        MCore::Matrix globalTM = transform.ToMatrix();

        // pass it down to the real rendering function
        RenderTrajectory(globalTM, innerColor, borderColor, scale);
    }


    // render the trajectory trace particles as one big curved arrow
    void RenderUtil::RenderTrajectoryPath(TrajectoryTracePath* trajectoryPath, const MCore::RGBAColor& innerColor, float scale)
    {
        // make sure the incoming trajectory trace path is valid
        if (trajectoryPath == NULL)
        {
            return;
        }

        // get some helper variables and check if there is a motion extraction node set
        EMotionFX::ActorInstance*   actorInstance   = trajectoryPath->mActorInstance;
        EMotionFX::Actor*           actor           = actorInstance->GetActor();
        EMotionFX::Node*            extractionNode  = actor->GetMotionExtractionNode();
        if (extractionNode == NULL)
        {
            return;
        }

        // fast access to the trajectory trace particles
        const MCore::Array<MCommon::RenderUtil::TrajectoryPathParticle>& traceParticles = trajectoryPath->mTraceParticles;
        const int32 numTraceParticles = traceParticles.GetLength();
        if (traceParticles.GetIsEmpty())
        {
            return;
        }

        const float trailWidthHalf          = 0.25f;
        const float trailLength             = 2.0f;
        const float arrowWidthHalf          = 0.75f;
        const float arrowLength             = 1.5f;
        const AZ::Vector3 liftFromGround(0.0f, 0.0f, 0.0001f);

        const MCore::Matrix trajectoryGlobalTM = actorInstance->GetGlobalTransform().ToMatrix();

        //////////////////////////////////////////////////////////////////////////////////////////////////////
        // Render arrow head
        //////////////////////////////////////////////////////////////////////////////////////////////////////
        // get the position and some direction vectors of the trajectory node matrix
        MCore::Matrix  globalTM = traceParticles[numTraceParticles - 1].mGlobalTM;
        AZ::Vector3 right = trajectoryGlobalTM.GetRight().GetNormalized();
        AZ::Vector3 center = trajectoryGlobalTM.GetTranslation();
        AZ::Vector3 forward = trajectoryGlobalTM.GetForward().GetNormalized();
        AZ::Vector3 up(0.0f, 0.0f, 1.0f);

        AZ::Vector3 vertices[7];
        AZ::Vector3 oldLeft, oldRight;

        /*
            //              4
            //             / \
            //            /   \
            //          /       \
            //        /           \
            //      /               \
            //    5-----6       2-----3
            //          |       |
            //          |       |
            //          |       |
            //          |       |
            //          |       |
            //          0-------1
        */
        // construct the arrow vertices
        vertices[0] = center + (-right * trailWidthHalf - forward * trailLength) * scale;
        vertices[1] = center + (right * trailWidthHalf - forward * trailLength) * scale;
        vertices[2] = center + (right * trailWidthHalf) * scale;
        vertices[3] = center + (right * arrowWidthHalf) * scale;
        vertices[4] = center + (forward * arrowLength) * scale;
        vertices[5] = center + (-right * arrowWidthHalf) * scale;
        vertices[6] = center + (-right * trailWidthHalf) * scale;

        oldLeft     = vertices[6];
        oldRight    = vertices[2];

        AZ::Vector3 arrowOldLeft = oldLeft;
        AZ::Vector3 arrowOldRight = oldRight;

        // render the solid arrow
        MCore::RGBAColor arrowColor = innerColor * 1.2f;
        arrowColor.Clamp();
        RenderTriangle(vertices[3] + liftFromGround, vertices[4] + liftFromGround, vertices[2] + liftFromGround, arrowColor);
        RenderTriangle(vertices[2] + liftFromGround, vertices[4] + liftFromGround, vertices[6] + liftFromGround, arrowColor);
        RenderTriangle(vertices[6] + liftFromGround, vertices[4] + liftFromGround, vertices[5] + liftFromGround, arrowColor);

        //////////////////////////////////////////////////////////////////////////////////////////////////////
        // Render arrow tail (actual path)
        //////////////////////////////////////////////////////////////////////////////////////////////////////
        AZ::Vector3 a, b;
        MCore::RGBAColor color = innerColor;

        // render the path from the arrow head towards the tail
        for (int32 i = numTraceParticles - 1; i > 0; i--)
        {
            // calculate the normalized distance to the head, this value also represents the alpha value as it fades away while getting closer to the end
            float normalizedDistance = (float)i / numTraceParticles;

            // get the start and end point of the line segment and calculate the delta between them
            globalTM    = traceParticles[i].mGlobalTM;
            a           = globalTM.GetTranslation();
            b           = traceParticles[i - 1].mGlobalTM.GetTranslation();
            right       = globalTM.GetRight().GetNormalized();

            if (i > 1 && i < numTraceParticles - 3)
            {
                const AZ::Vector3 deltaA = traceParticles[i - 2].mGlobalTM.GetTranslation() - traceParticles[i - 1].mGlobalTM.GetTranslation();
                const AZ::Vector3 deltaB = traceParticles[i - 1].mGlobalTM.GetTranslation() - traceParticles[i    ].mGlobalTM.GetTranslation();
                const AZ::Vector3 deltaC = traceParticles[i    ].mGlobalTM.GetTranslation() - traceParticles[i + 1].mGlobalTM.GetTranslation();
                const AZ::Vector3 deltaD = traceParticles[i + 1].mGlobalTM.GetTranslation() - traceParticles[i + 2].mGlobalTM.GetTranslation();
                AZ::Vector3 delta = deltaA + deltaB + deltaC + deltaD;
                delta = MCore::SafeNormalize(delta);

                right = up.Cross(delta);
            }

            /*
                    //              .
                    //              .
                    //              .
                    //(oldLeft) 0   a   1 (oldRight)
                    //          |       |
                    //          |       |
                    //          |       |
                    //          |       |
                    //          |       |
                    //          2---b---3
            */

            // construct the arrow vertices
            vertices[0] = oldLeft;
            vertices[1] = oldRight;
            vertices[2] = b + AZ::Vector3(-right * trailWidthHalf) * scale;
            vertices[3] = b + AZ::Vector3(right * trailWidthHalf) * scale;

            // make sure we perfectly align with the arrow head
            if (i == numTraceParticles - 1)
            {
                normalizedDistance = 1.0f;
                vertices[0] = arrowOldLeft;
                vertices[1] = arrowOldRight;
            }

            // render the solid arrow
            color.a = normalizedDistance;
            RenderTriangle(vertices[0] + liftFromGround, vertices[2] + liftFromGround, vertices[1] + liftFromGround, color);
            RenderTriangle(vertices[1] + liftFromGround, vertices[2] + liftFromGround, vertices[3] + liftFromGround, color);

            // overwrite the old left and right values so that they can be used for the next trace particle
            oldLeft  = vertices[2];
            oldRight = vertices[3];
        }

        // Debug info.
        //RenderLine( projectedTransform.mPosition, projectedTransform.mPosition + trajectoryGlobalTM.GetRow(1), MCore::RGBAColor(1.0f, 1.0f, 0.0f) );
        //RenderLine( actorInstance->GetGlobalPosition(), actorInstance->GetGlobalPosition() + actorInstance->GetGlobalTransform().mRotation.CalcForwardAxis(), MCore::RGBAColor(0.0f, 1.0f, 0.0f) );

        // make sure we render all lines within one call and with the correct render flags set
        RenderLines();
    }


    // reset the trajectory path
    void RenderUtil::ResetTrajectoryPath(TrajectoryTracePath* trajectoryPath)
    {
        // make sure the incoming trajectory trace path is valid
        if (trajectoryPath == NULL)
        {
            return;
        }

        // remove all particles while keeping the data in memory
        trajectoryPath->mTraceParticles.Clear(false);
    }


    // render the name of the given node at the node position
    void RenderUtil::RenderText(const char* text, uint32 textSize, const AZ::Vector3& globalPos, MCommon::Camera* camera, uint32 screenWidth, uint32 screenHeight, const MCore::RGBAColor& color)
    {
        // project the node global position to the screen space
        const AZ::Vector3 projectedPoint = Project(globalPos, camera->GetViewProjMatrix(), screenWidth, screenHeight);

        // perform clipping and make sure the node is actually visible, if not skip rendering its name
        if (projectedPoint.GetX() < 0.0f || projectedPoint.GetX() > screenWidth || projectedPoint.GetY() < 0.0f || projectedPoint.GetY() > screenHeight)
        {
            return;
        }

        if (camera->GetType() != MCommon::OrthographicCamera::TYPE_ID && projectedPoint.GetZ() < 0.0f)
        {
            return;
        }

        // render the text
        RenderText(projectedPoint.GetX() * m_devicePixelRatio, projectedPoint.GetY() * m_devicePixelRatio, text, color, static_cast<float>(textSize), true);
    }


    // render node names for all enabled nodes
    void RenderUtil::RenderNodeNames(EMotionFX::ActorInstance* actorInstance, Camera* camera, uint32 screenWidth, uint32 screenHeight, const MCore::RGBAColor& color, const MCore::RGBAColor& selectedColor, const MCore::Array<uint32>& visibleNodeIndices, const MCore::Array<uint32>& selectedNodeIndices)
    {
        EMotionFX::Actor*           actor           = actorInstance->GetActor();
        EMotionFX::Skeleton*        skeleton        = actor->GetSkeleton();
        EMotionFX::TransformData*   transformData   = actorInstance->GetTransformData();
        const uint32    numEnabledNodes = actorInstance->GetNumEnabledNodes();

        // get the number of enabled nodes and iterate through them
        for (uint32 i = 0; i < numEnabledNodes; ++i)
        {
            EMotionFX::Node*    node        = skeleton->GetNode(actorInstance->GetEnabledNode(i));
            const uint32        nodeIndex   = node->GetNodeIndex();
            AZ::Vector3         globalPos   = transformData->GetGlobalInclusiveMatrix(nodeIndex).GetTranslation();

            // check if the current enabled node is along the visible nodes and render it if that is the case
            if ((visibleNodeIndices.GetIsEmpty()) ||
                (visibleNodeIndices.GetIsEmpty() == false && visibleNodeIndices.Find(nodeIndex) != MCORE_INVALIDINDEX32))
            {
                // is the current node selected?
                if (selectedNodeIndices.GetIsEmpty() == false && selectedNodeIndices.Find(nodeIndex) != MCORE_INVALIDINDEX32)
                {
                    RenderText(node->GetName(), 11, globalPos, camera, screenWidth, screenHeight, selectedColor);
                }
                else
                {
                    RenderText(node->GetName(), 11, globalPos, camera, screenWidth, screenHeight, color);
                }
            }
        }
    }


    void RenderUtil::RenderWireframeBox(const AZ::Vector3& dimensions, const MCore::Matrix& globalTM, const MCore::RGBAColor& color, bool directlyRender)
    {
        AZ::Vector3 min = AZ::Vector3(-dimensions.GetX()*0.5f, -dimensions.GetY()*0.5f, -dimensions.GetZ()*0.5f);
        AZ::Vector3 max = AZ::Vector3( dimensions.GetX()*0.5f,  dimensions.GetY()*0.5f,  dimensions.GetZ()*0.5f);

        AZ::Vector3 p[8];
        p[0].Set(min.GetX(), min.GetY(), min.GetZ());
        p[1].Set(max.GetX(), min.GetY(), min.GetZ());
        p[2].Set(max.GetX(), min.GetY(), max.GetZ());
        p[3].Set(min.GetX(), min.GetY(), max.GetZ());
        p[4].Set(min.GetX(), max.GetY(), min.GetZ());
        p[5].Set(max.GetX(), max.GetY(), min.GetZ());
        p[6].Set(max.GetX(), max.GetY(), max.GetZ());
        p[7].Set(min.GetX(), max.GetY(), max.GetZ());

        for (int i = 0; i < 8; ++i)
        {
            p[i] = p[i] * globalTM;
        }

        RenderLine(p[0], p[1], color);
        RenderLine(p[1], p[2], color);
        RenderLine(p[2], p[3], color);
        RenderLine(p[3], p[0], color);

        RenderLine(p[4], p[5], color);
        RenderLine(p[5], p[6], color);
        RenderLine(p[6], p[7], color);
        RenderLine(p[7], p[4], color);

        RenderLine(p[0], p[4], color);
        RenderLine(p[1], p[5], color);
        RenderLine(p[2], p[6], color);
        RenderLine(p[3], p[7], color);

        if (directlyRender)
        {
            RenderLines();
        }
    }


    void RenderUtil::RenderWireframeSphere(float radius, const MCore::Matrix& globalTM, const MCore::RGBAColor& color, bool directlyRender)
    {
        const float stepSize = AZ::Constants::TwoPi / m_wireframeSphereSegmentCount;

        AZ::Vector3 pos1, pos2;
        float x1, y1, x2, y2;
        const float endAngle = AZ::Constants::TwoPi + std::numeric_limits<float>::epsilon();
        for (float i = 0.0f; i < endAngle; i += stepSize)
        {
            x1 = radius * cosf(i);
            y1 = radius * sinf(i);
            x2 = radius * cosf(i + stepSize);
            y2 = radius * sinf(i + stepSize);

            pos1 = AZ::Vector3(x1, y1, 0.0f) * globalTM;
            pos2 = AZ::Vector3(x2, y2, 0.0f) * globalTM;
            RenderLine(pos1, pos2, color);

            pos1 = AZ::Vector3(x1, 0.0f, y1) * globalTM;
            pos2 = AZ::Vector3(x2, 0.0f, y2) * globalTM;
            RenderLine(pos1, pos2, color);

            pos1 = AZ::Vector3(0.0f, x1, y1) * globalTM;
            pos2 = AZ::Vector3(0.0f, x2, y2) * globalTM;
            RenderLine(pos1, pos2, color);
        }

        if (directlyRender)
        {
            RenderLines();
        }
    }


    // The capsule caps (for one aligned vertically) are rendered as one horizontal full circle (around y) and two vertically aligned half circles around the x and z axes.
    // The end points of these half circles connect the bottom cap to the top cap (the cylinder part in the middle).
    void RenderUtil::RenderWireframeCapsule(float radius, float height, const MCore::Matrix& globalTM, const MCore::RGBAColor& color, bool directlyRender)
    {
        float stepSize = AZ::Constants::TwoPi / m_wireframeSphereSegmentCount;
        const float cylinderHeight = height - 2.0f * radius;
        const float halfCylinderHeight = cylinderHeight * 0.5f;

        AZ::Vector3 pos1, pos2;
        float x1, y1, x2, y2;

        // Draw the full circles for both caps
        float startAngle = 0.0f;
        float endAngle = AZ::Constants::TwoPi + std::numeric_limits<float>::epsilon();
        for (float i = startAngle; i < endAngle; i += stepSize)
        {
            x1 = radius * cosf(i);
            y1 = radius * sinf(i);
            x2 = radius * cosf(i + stepSize);
            y2 = radius * sinf(i + stepSize);

            pos1 = AZ::Vector3(x1, y1,  halfCylinderHeight) * globalTM;
            pos2 = AZ::Vector3(x2, y2,  halfCylinderHeight) * globalTM;
            RenderLine(pos1, pos2, color);

            pos1 = AZ::Vector3(x1, y1, -halfCylinderHeight) * globalTM;
            pos2 = AZ::Vector3(x2, y2, -halfCylinderHeight) * globalTM;
            RenderLine(pos1, pos2, color);
        }

        // Draw half circles for caps
        startAngle = 0.0f;
        endAngle = AZ::Constants::Pi - std::numeric_limits<float>::epsilon();
        for (float i = startAngle; i < endAngle; i += stepSize)
        {
            x1 = radius * cosf(i);
            y1 = radius * sinf(i);
            x2 = radius * cosf(i + stepSize);
            y2 = radius * sinf(i + stepSize);

            // Upper cap
            pos1 = AZ::Vector3(x1, 0.0f, y1 + halfCylinderHeight) * globalTM;
            pos2 = AZ::Vector3(x2, 0.0f, y2 + halfCylinderHeight) * globalTM;
            RenderLine(pos1, pos2, color);

            pos1 = AZ::Vector3(0.0f, x1, y1 + halfCylinderHeight) * globalTM;
            pos2 = AZ::Vector3(0.0f, x2, y2 + halfCylinderHeight) * globalTM;
            RenderLine(pos1, pos2, color);

            // Lower cap
            pos1 = AZ::Vector3(x1, 0.0f, -y1 - halfCylinderHeight) * globalTM;
            pos2 = AZ::Vector3(x2, 0.0f, -y2 - halfCylinderHeight) * globalTM;
            RenderLine(pos1, pos2, color);

            pos1 = AZ::Vector3(0.0f, x1, -y1 - halfCylinderHeight) * globalTM;
            pos2 = AZ::Vector3(0.0f, x2, -y2 - halfCylinderHeight) * globalTM;
            RenderLine(pos1, pos2, color);
        }

        // Draw cap connectors (cylinder height)
        startAngle = 0.0f;
        endAngle = AZ::Constants::TwoPi + std::numeric_limits<float>::epsilon();
        stepSize = AZ::Constants::Pi * 0.5f;
        for (float i = startAngle; i < endAngle; i += stepSize)
        {
            x1 = radius * cosf(i);
            y1 = radius * sinf(i);

            pos1 = AZ::Vector3(x1, y1,  halfCylinderHeight) * globalTM;
            pos2 = AZ::Vector3(x1, y1, -halfCylinderHeight) * globalTM;
            RenderLine(pos1, pos2, color);
        }

        if (directlyRender)
        {
            RenderLines();
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Vector Font
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    //   font info:
    //
    //Peter Holzmann, Octopus Enterprises
    //USPS: 19611 La Mar Court, Cupertino, CA 95014
    //UUCP: {hplabs!hpdsd,pyramid}!octopus!pete
    //Phone: 408/996-7746
    //
    //This distribution is made possible through the collective encouragement
    //of the Usenet Font Consortium, a mailing list that sprang to life to get
    //this accomplished and that will now most likely disappear into the mists
    //of time... Thanks are especially due to Jim Hurt, who provided the packed
    //font data for the distribution, along with a lot of other help.
    //
    //This file describes the Hershey Fonts in general, along with a description of
    //the other files in this distribution and a simple re-distribution restriction.
    //
    //USE RESTRICTION:
    //        This distribution of the Hershey Fonts may be used by anyone for
    //        any purpose, commercial or otherwise, providing that:
    //                1. The following acknowledgements must be distributed with
    //                        the font data:
    //                        - The Hershey Fonts were originally created by Dr.
    //                                A. V. Hershey while working at the U. S.
    //                                National Bureau of Standards.
    //                        - The format of the Font data in this distribution
    //                                was originally created by
    //                                        James Hurt
    //                                        Cognition, Inc.
    //                                        900 Technology Park Drive
    //                                        Billerica, MA 01821
    //                                        (mit-eddie!ci-dandelion!hurt)
    //                2. The font data in this distribution may be converted into
    //                        any other format *EXCEPT* the format distributed by
    //                        the U.S. NTIS (which organization holds the rights
    //                        to the distribution and use of the font data in that
    //                        particular format). Not that anybody would really
    //                        *want* to use their format... each point is described
    //                        in eight bytes as "xxx yyy:", where xxx and yyy are
    //                        the coordinate values as ASCII numbers.

#define FONT_VERSION 1

    unsigned char gFontData[6350] = {
        0x46, 0x4F, 0x4E, 0x54, 0x01, 0x00, 0x00, 0x00, 0x43, 0x01, 0x00, 0x00, 0x5E, 0x00, 0x00, 0x00, 0xC4, 0x06, 0x00, 0x00, 0x0A, 0xD7, 0x23, 0x3C, 0x3D, 0x0A, 0x57, 0x3E, 0x0A, 0xD7, 0x23, 0x3C,
        0x28, 0x5C, 0x8F, 0x3D, 0x0A, 0xD7, 0x23, 0x3C, 0x08, 0xD7, 0xA3, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x08, 0xD7, 0x23, 0x3C, 0x0A, 0xD7, 0x23, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x0A, 0xD7, 0xA3, 0x3C,
        0x08, 0xD7, 0x23, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x3D, 0x0A, 0x57, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x28, 0x5C, 0x0F, 0x3E, 0x0A, 0xD7, 0xA3, 0x3D, 0x3D, 0x0A, 0x57, 0x3E, 0x0A, 0xD7, 0xA3, 0x3D,
        0x28, 0x5C, 0x0F, 0x3E, 0x0A, 0xD7, 0xA3, 0x3D, 0x00, 0x00, 0x80, 0x3E, 0x0C, 0xD7, 0x23, 0x3C, 0x29, 0x5C, 0x8F, 0xBD, 0x29, 0x5C, 0x0F, 0x3E, 0x00, 0x00, 0x80, 0x3E, 0x29, 0x5C, 0x8F, 0x3D,
        0x29, 0x5C, 0x8F, 0xBD, 0x0C, 0xD7, 0x23, 0x3C, 0x8F, 0xC2, 0xF5, 0x3D, 0x9A, 0x99, 0x19, 0x3E, 0x8F, 0xC2, 0xF5, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x8E, 0xC2, 0x75, 0x3D, 0x29, 0x5C, 0x0F, 0x3E,
        0x8E, 0xC2, 0x75, 0x3D, 0xCD, 0xCC, 0x4C, 0x3D, 0x00, 0x00, 0x80, 0x3E, 0xCD, 0xCC, 0x4C, 0x3D, 0x0A, 0xD7, 0x23, 0xBD, 0xEC, 0x51, 0xB8, 0x3D, 0x00, 0x00, 0x80, 0x3E, 0xEC, 0x51, 0xB8, 0x3D,
        0x0A, 0xD7, 0x23, 0xBD, 0x29, 0x5C, 0x0F, 0x3E, 0xEB, 0x51, 0x38, 0x3E, 0x8F, 0xC2, 0xF5, 0x3D, 0xCC, 0xCC, 0x4C, 0x3E, 0xEC, 0x51, 0xB8, 0x3D, 0x3D, 0x0A, 0x57, 0x3E, 0xCD, 0xCC, 0x4C, 0x3D,
        0x3D, 0x0A, 0x57, 0x3E, 0x0C, 0xD7, 0xA3, 0x3C, 0xCC, 0xCC, 0x4C, 0x3E, 0x00, 0x00, 0x00, 0x00, 0xEB, 0x51, 0x38, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x0A, 0xD7, 0x23, 0x3E, 0x0C, 0xD7, 0x23, 0x3C,
        0x28, 0x5C, 0x0F, 0x3E, 0x0C, 0xD7, 0xA3, 0x3C, 0xB8, 0x1E, 0x05, 0x3E, 0x0A, 0xD7, 0x23, 0x3D, 0x8F, 0xC2, 0xF5, 0x3D, 0xCD, 0xCC, 0xCC, 0x3D, 0xCC, 0xCC, 0xCC, 0x3D, 0x8F, 0xC2, 0xF5, 0x3D,
        0xEB, 0x51, 0xB8, 0x3D, 0xB8, 0x1E, 0x05, 0x3E, 0x0A, 0xD7, 0xA3, 0x3D, 0x29, 0x5C, 0x0F, 0x3E, 0x8E, 0xC2, 0xF5, 0x3C, 0x8F, 0xC2, 0xF5, 0x3D, 0x08, 0xD7, 0x23, 0x3C, 0xEC, 0x51, 0xB8, 0x3D,
        0x00, 0x00, 0x00, 0x00, 0xCD, 0xCC, 0x4C, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8E, 0xC2, 0xF5, 0x3C, 0xEB, 0x51, 0x38, 0x3E, 0x3D, 0x0A, 0x57, 0x3E, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x28, 0x5C, 0x8F, 0x3D, 0x5C, 0x8F, 0x42, 0x3E, 0x28, 0x5C, 0x8F, 0x3D, 0x7A, 0x14, 0x2E, 0x3E, 0x8E, 0xC2, 0x75, 0x3D, 0x99, 0x99, 0x19, 0x3E, 0x0A, 0xD7, 0x23, 0x3D,
        0x28, 0x5C, 0x0F, 0x3E, 0x08, 0xD7, 0xA3, 0x3C, 0x28, 0x5C, 0x0F, 0x3E, 0x08, 0xD7, 0x23, 0x3C, 0xCC, 0xCC, 0x4C, 0x3E, 0x8E, 0xC2, 0xF5, 0x3C, 0x3D, 0x0A, 0x57, 0x3E, 0x28, 0x5C, 0x8F, 0x3D,
        0xCC, 0xCC, 0x4C, 0x3E, 0xCC, 0xCC, 0xCC, 0x3D, 0x5C, 0x8F, 0x42, 0x3E, 0xB8, 0x1E, 0x05, 0x3E, 0x5C, 0x8F, 0x42, 0x3E, 0x0A, 0xD7, 0x23, 0x3E, 0xCC, 0xCC, 0x4C, 0x3E, 0x28, 0x5C, 0x0F, 0x3E,
        0x28, 0x5C, 0x8F, 0x3D, 0x8F, 0xC2, 0xF5, 0x3D, 0x8E, 0xC2, 0x75, 0x3D, 0xAE, 0x47, 0xE1, 0x3D, 0x0A, 0xD7, 0x23, 0x3D, 0xAE, 0x47, 0xE1, 0x3D, 0x08, 0xD7, 0xA3, 0x3C, 0xB8, 0x1E, 0x05, 0x3E,
        0x00, 0x00, 0x00, 0x00, 0x99, 0x99, 0x19, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x7A, 0x14, 0x2E, 0x3E, 0x08, 0xD7, 0x23, 0x3C, 0xEB, 0x51, 0x38, 0x3E, 0x8E, 0xC2, 0xF5, 0x3C, 0xEB, 0x51, 0x38, 0x3E,
        0xCC, 0xCC, 0x4C, 0x3D, 0x0A, 0xD7, 0x23, 0x3E, 0x28, 0x5C, 0x8F, 0x3D, 0xCC, 0xCC, 0x4C, 0x3E, 0x8F, 0xC2, 0xF5, 0x3D, 0xCC, 0xCC, 0x4C, 0x3E, 0xB8, 0x1E, 0x05, 0x3E, 0x5C, 0x8F, 0x42, 0x3E,
        0x28, 0x5C, 0x0F, 0x3E, 0xEB, 0x51, 0x38, 0x3E, 0x28, 0x5C, 0x0F, 0x3E, 0x7A, 0x14, 0x2E, 0x3E, 0xB8, 0x1E, 0x05, 0x3E, 0x0A, 0xD7, 0x23, 0x3E, 0xAE, 0x47, 0xE1, 0x3D, 0x8E, 0xC2, 0xF5, 0x3D,
        0x8E, 0xC2, 0xF5, 0x3C, 0xCC, 0xCC, 0xCC, 0x3D, 0x08, 0xD7, 0x23, 0x3C, 0x0A, 0xD7, 0xA3, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x09, 0xD7, 0x23, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x0A, 0xD7, 0x23, 0x3D, 0x08, 0xD7, 0x23, 0x3C, 0x0A, 0xD7, 0xA3, 0x3D, 0x08, 0xD7, 0xA3, 0x3C, 0xEB, 0x51, 0xB8, 0x3D, 0xEB, 0x51, 0xB8, 0x3D, 0xB8, 0x1E, 0x05, 0x3E, 0xCC, 0xCC, 0xCC, 0x3D,
        0x28, 0x5C, 0x0F, 0x3E, 0xAD, 0x47, 0xE1, 0x3D, 0x0A, 0xD7, 0x23, 0x3E, 0xAD, 0x47, 0xE1, 0x3D, 0xEB, 0x51, 0x38, 0x3E, 0xCC, 0xCC, 0xCC, 0x3D, 0xCC, 0xCC, 0x4C, 0x3E, 0x8E, 0xC2, 0x75, 0x3D,
        0xCC, 0xCC, 0x4C, 0x3E, 0xCC, 0xCC, 0x4C, 0x3D, 0xEB, 0x51, 0x38, 0x3E, 0xCC, 0xCC, 0x4C, 0x3D, 0x0A, 0xD7, 0x23, 0x3E, 0x8E, 0xC2, 0x75, 0x3D, 0xB8, 0x1E, 0x05, 0x3E, 0x0A, 0xD7, 0xA3, 0x3D,
        0xCC, 0xCC, 0xCC, 0x3D, 0xB8, 0x1E, 0x05, 0x3E, 0x8E, 0xC2, 0xF5, 0x3C, 0x99, 0x99, 0x19, 0x3E, 0x08, 0xD7, 0x23, 0x3C, 0x7A, 0x14, 0x2E, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x0A, 0xD7, 0x23, 0x3C,
        0x5C, 0x8F, 0x42, 0x3E, 0x00, 0x00, 0x00, 0x00, 0xCC, 0xCC, 0x4C, 0x3E, 0x0A, 0xD7, 0xA3, 0x3C, 0xEB, 0x51, 0x38, 0x3E, 0x0A, 0xD7, 0x23, 0x3C, 0x0A, 0xD7, 0x23, 0x3E, 0x00, 0x00, 0x00, 0x00,
        0x99, 0x99, 0x19, 0x3E, 0x29, 0x5C, 0x8F, 0x3D, 0x00, 0x00, 0x80, 0x3E, 0xCC, 0xCC, 0x4C, 0x3D, 0x1E, 0x85, 0x6B, 0x3E, 0x8F, 0xC2, 0xF5, 0x3C, 0xCC, 0xCC, 0x4C, 0x3E, 0x00, 0x00, 0x00, 0x00,
        0xAE, 0x47, 0xE1, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x28, 0x5C, 0x8F, 0x3D, 0x8F, 0xC2, 0xF5, 0x3C, 0x0C, 0xD7, 0xA3, 0xBC, 0xCC, 0xCC, 0x4C, 0x3D, 0xCE, 0xCC, 0x4C, 0xBD, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x80, 0x3E, 0x0A, 0xD7, 0xA3, 0x3C, 0x1E, 0x85, 0x6B, 0x3E, 0x0A, 0xD7, 0x23, 0x3D, 0xCC, 0xCC, 0x4C, 0x3E, 0x8F, 0xC2, 0x75, 0x3D, 0x0A, 0xD7, 0x23, 0x3E, 0x29, 0x5C, 0x8F, 0x3D,
        0xAE, 0x47, 0xE1, 0x3D, 0x29, 0x5C, 0x8F, 0x3D, 0x28, 0x5C, 0x8F, 0x3D, 0x8F, 0xC2, 0x75, 0x3D, 0x08, 0xD7, 0xA3, 0x3C, 0x0A, 0xD7, 0x23, 0x3D, 0x0C, 0xD7, 0xA3, 0xBC, 0x0A, 0xD7, 0xA3, 0x3C,
        0xCE, 0xCC, 0x4C, 0xBD, 0x00, 0x00, 0x00, 0x00, 0x29, 0x5C, 0x8F, 0xBD, 0xCC, 0xCC, 0x4C, 0x3D, 0x99, 0x99, 0x19, 0x3E, 0xCC, 0xCC, 0x4C, 0x3D, 0x8E, 0xC2, 0xF5, 0x3C, 0x00, 0x00, 0x00, 0x00,
        0x8F, 0xC2, 0xF5, 0x3D, 0xCC, 0xCC, 0xCC, 0x3D, 0x8E, 0xC2, 0x75, 0x3D, 0xCC, 0xCC, 0xCC, 0x3D, 0x8F, 0xC2, 0xF5, 0x3D, 0xEB, 0x51, 0xB8, 0x3D, 0xEB, 0x51, 0x38, 0x3E, 0x00, 0x00, 0x00, 0x00,
        0xEB, 0x51, 0xB8, 0x3D, 0xEB, 0x51, 0x38, 0x3E, 0xEB, 0x51, 0xB8, 0x3D, 0x0A, 0xD7, 0xA3, 0x3C, 0x0A, 0xD7, 0x23, 0x3D, 0x0A, 0xD7, 0x23, 0x3C, 0x8E, 0xC2, 0xF5, 0x3C, 0x0A, 0xD7, 0x23, 0x3C,
        0xCC, 0xCC, 0x4C, 0x3D, 0x0A, 0xD7, 0xA3, 0x3C, 0x08, 0xD7, 0xA3, 0x3C, 0xEB, 0x51, 0x38, 0x3E, 0x00, 0x00, 0x80, 0x3E, 0x90, 0xC2, 0x75, 0x3D, 0x3D, 0x0A, 0x57, 0x3E, 0x0C, 0xD7, 0x23, 0x3C,
        0x7A, 0x14, 0x2E, 0x3E, 0x0C, 0xD7, 0x23, 0x3C, 0x0A, 0xD7, 0x23, 0x3D, 0x90, 0xC2, 0xF5, 0x3C, 0x08, 0xD7, 0x23, 0x3C, 0x90, 0xC2, 0x75, 0x3D, 0x00, 0x00, 0x00, 0x00, 0xAE, 0x47, 0xE1, 0x3D,
        0x08, 0xD7, 0x23, 0x3C, 0xB8, 0x1E, 0x05, 0x3E, 0x0A, 0xD7, 0x23, 0x3D, 0x29, 0x5C, 0x0F, 0x3E, 0xEB, 0x51, 0xB8, 0x3D, 0x29, 0x5C, 0x0F, 0x3E, 0x8F, 0xC2, 0xF5, 0x3D, 0xB8, 0x1E, 0x05, 0x3E,
        0x7A, 0x14, 0x2E, 0x3E, 0xAE, 0x47, 0xE1, 0x3D, 0xCC, 0xCC, 0x4C, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x7A, 0x14, 0x2E, 0x3E, 0x0C, 0xD7, 0xA3, 0x3C, 0x5C, 0x8F, 0x42, 0x3E, 0x8F, 0xC2, 0xF5, 0x3D,
        0x5C, 0x8F, 0x42, 0x3E, 0xB8, 0x1E, 0x05, 0x3E, 0x99, 0x99, 0x19, 0x3E, 0x8F, 0xC2, 0xF5, 0x3D, 0xB8, 0x1E, 0x05, 0x3E, 0x29, 0x5C, 0x0F, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x0C, 0xD7, 0xA3, 0x3C,
        0x3D, 0x0A, 0x57, 0x3E, 0xB8, 0x1E, 0x05, 0x3E, 0x3D, 0x0A, 0x57, 0x3E, 0x29, 0x5C, 0x8F, 0x3D, 0xB8, 0x1E, 0x05, 0x3E, 0xCD, 0xCC, 0xCC, 0x3D, 0xB8, 0x1E, 0x05, 0x3E, 0x8F, 0xC2, 0xF5, 0x3D,
        0x8F, 0xC2, 0xF5, 0x3D, 0xB8, 0x1E, 0x05, 0x3E, 0xAE, 0x47, 0xE1, 0x3D, 0x29, 0x5C, 0x0F, 0x3E, 0x0A, 0xD7, 0xA3, 0x3D, 0xCD, 0xCC, 0xCC, 0x3D, 0x3D, 0x0A, 0x57, 0x3E, 0x9A, 0x99, 0x19, 0x3E,
        0x28, 0x5C, 0x8F, 0x3D, 0xCD, 0xCC, 0xCC, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x8F, 0xC2, 0xF5, 0x3D, 0x3D, 0x0A, 0x57, 0x3E, 0xCD, 0xCC, 0x4C, 0x3D, 0x28, 0x5C, 0x0F, 0x3E, 0xAE, 0x47, 0xE1, 0x3D,
        0xB8, 0x1E, 0x05, 0x3E, 0x8F, 0xC2, 0xF5, 0x3D, 0xEB, 0x51, 0x38, 0x3E, 0x29, 0x5C, 0x8F, 0x3D, 0x00, 0x00, 0x00, 0x00, 0xB8, 0x1E, 0x05, 0x3E, 0x8E, 0xC2, 0x75, 0x3D, 0xB8, 0x1E, 0x05, 0x3E,
        0x28, 0x5C, 0x8F, 0x3D, 0x8F, 0xC2, 0xF5, 0x3D, 0xCC, 0xCC, 0xCC, 0x3D, 0x8F, 0xC2, 0xF5, 0x3C, 0x8F, 0xC2, 0xF5, 0x3D, 0x0C, 0xD7, 0x23, 0x3C, 0xCC, 0xCC, 0xCC, 0x3D, 0x29, 0x5C, 0x0F, 0x3E,
        0x3D, 0x0A, 0x57, 0x3E, 0x0C, 0xD7, 0x23, 0x3C, 0xEB, 0x51, 0x38, 0x3E, 0x0A, 0xD7, 0x23, 0x3D, 0xB8, 0x1E, 0x05, 0x3E, 0x0A, 0xD7, 0xA3, 0x3D, 0x8F, 0xC2, 0xF5, 0x3D, 0xAE, 0x47, 0xE1, 0x3D,
        0xAE, 0x47, 0xE1, 0x3D, 0xB8, 0x1E, 0x05, 0x3E, 0xEB, 0x51, 0xB8, 0x3D, 0x29, 0x5C, 0x0F, 0x3E, 0x0A, 0xD7, 0x23, 0x3D, 0xB8, 0x1E, 0x05, 0x3E, 0x08, 0xD7, 0xA3, 0x3C, 0x0C, 0xD7, 0x23, 0x3C,
        0xEB, 0x51, 0xB8, 0x3D, 0x90, 0xC2, 0xF5, 0x3C, 0xAE, 0x47, 0xE1, 0x3D, 0x90, 0xC2, 0x75, 0x3D, 0x8F, 0xC2, 0xF5, 0x3D, 0x8F, 0xC2, 0xF5, 0x3D, 0x28, 0x5C, 0x0F, 0x3E, 0xB8, 0x1E, 0x05, 0x3E,
        0x0A, 0xD7, 0x23, 0x3E, 0xB8, 0x1E, 0x05, 0x3E, 0xEB, 0x51, 0x38, 0x3E, 0xB8, 0x1E, 0x05, 0x3E, 0x28, 0x5C, 0x0F, 0x3E, 0x8F, 0xC2, 0xF5, 0x3D, 0xAE, 0x47, 0xE1, 0x3D, 0xCD, 0xCC, 0xCC, 0x3D,
        0xEB, 0x51, 0xB8, 0x3D, 0x29, 0x5C, 0x8F, 0x3D, 0x0A, 0xD7, 0xA3, 0x3D, 0x90, 0xC2, 0x75, 0x3D, 0x0A, 0xD7, 0xA3, 0x3D, 0x90, 0xC2, 0xF5, 0x3C, 0xEB, 0x51, 0xB8, 0x3D, 0x0C, 0xD7, 0x23, 0x3C,
        0xAE, 0x47, 0xE1, 0x3D, 0x29, 0x5C, 0x8F, 0x3D, 0x3D, 0x0A, 0x57, 0x3E, 0x8F, 0xC2, 0xF5, 0x3D, 0x0A, 0xD7, 0x23, 0x3D, 0x0A, 0xD7, 0xA3, 0x3C, 0xAE, 0x47, 0xE1, 0x3D, 0x0A, 0xD7, 0x23, 0x3E,
        0xEB, 0x51, 0x38, 0x3E, 0x0A, 0xD7, 0x23, 0x3E, 0x00, 0x00, 0x00, 0x00, 0xEB, 0x51, 0x38, 0x3E, 0x8F, 0xC2, 0xF5, 0x3D, 0xEB, 0x51, 0x38, 0x3E, 0x8E, 0xC2, 0x75, 0x3D, 0x0A, 0xD7, 0x23, 0x3E,
        0xEB, 0x51, 0xB8, 0x3D, 0x0A, 0xD7, 0x23, 0x3D, 0x3D, 0x0A, 0x57, 0x3E, 0xAE, 0x47, 0xE1, 0x3D, 0x5C, 0x8F, 0x42, 0x3E, 0x8F, 0xC2, 0xF5, 0x3D, 0x7A, 0x14, 0x2E, 0x3E, 0x8F, 0xC2, 0xF5, 0x3D,
        0x99, 0x99, 0x19, 0x3E, 0x8F, 0xC2, 0x75, 0x3D, 0xCC, 0xCC, 0xCC, 0x3D, 0x8F, 0xC2, 0x75, 0x3D, 0x28, 0x5C, 0x8F, 0x3D, 0xCC, 0xCC, 0x4C, 0x3D, 0x08, 0xD7, 0x23, 0x3C, 0x29, 0x5C, 0x8F, 0x3D,
        0x08, 0xD7, 0x23, 0x3C, 0xEB, 0x51, 0xB8, 0x3D, 0x99, 0x99, 0x19, 0x3E, 0x28, 0x5C, 0x8F, 0x3D, 0x0A, 0xD7, 0x23, 0x3E, 0x0A, 0xD7, 0x23, 0x3D, 0x0A, 0xD7, 0x23, 0x3E, 0x09, 0xD7, 0xA3, 0x3C,
        0x99, 0x99, 0x19, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x0A, 0xD7, 0xA3, 0x3D, 0x08, 0xD7, 0x23, 0x3C, 0x8E, 0xC2, 0x75, 0x3D, 0x8E, 0xC2, 0xF5, 0x3C, 0xCC, 0xCC, 0x4C, 0x3D, 0x8E, 0xC2, 0x75, 0x3D,
        0xCC, 0xCC, 0x4C, 0x3D, 0x0A, 0xD7, 0xA3, 0x3D, 0x8E, 0xC2, 0x75, 0x3D, 0xEB, 0x51, 0xB8, 0x3D, 0x0A, 0xD7, 0xA3, 0x3D, 0x09, 0xD7, 0xA3, 0x3C, 0x8E, 0xC2, 0x75, 0x3D, 0xCC, 0xCC, 0xCC, 0x3D,
        0x0A, 0xD7, 0x23, 0x3E, 0xEB, 0x51, 0xB8, 0x3D, 0x8E, 0xC2, 0x75, 0x3D, 0xAE, 0x47, 0xE1, 0x3D, 0xCC, 0xCC, 0x4C, 0x3D, 0xB8, 0x1E, 0x05, 0x3E, 0xCC, 0xCC, 0x4C, 0x3D, 0x0A, 0xD7, 0x23, 0x3E,
        0xCC, 0xCC, 0xCC, 0x3D, 0x0A, 0xD7, 0x23, 0x3E, 0x8F, 0xC2, 0xF5, 0x3D, 0x99, 0x99, 0x19, 0x3E, 0x99, 0x99, 0x19, 0x3E, 0x28, 0x5C, 0x0F, 0x3E, 0x7A, 0x14, 0x2E, 0x3E, 0x90, 0xC2, 0xF5, 0x3C,
        0x28, 0x5C, 0x8F, 0x3D, 0x29, 0x5C, 0x0F, 0x3E, 0x99, 0x99, 0x19, 0x3E, 0xB8, 0x1E, 0x05, 0x3E, 0xB8, 0x1E, 0x05, 0x3E, 0xEC, 0x51, 0xB8, 0x3D, 0xAE, 0x47, 0xE1, 0x3D, 0x9A, 0x99, 0x19, 0x3E,
        0x0A, 0xD7, 0x23, 0x3E, 0x00, 0x00, 0x00, 0x00, 0xB8, 0x1E, 0x05, 0x3E, 0x0C, 0xD7, 0xA3, 0x3C, 0x8E, 0xC2, 0xF5, 0x3C, 0x0A, 0xD7, 0x23, 0x3D, 0x08, 0xD7, 0x23, 0x3C, 0x9A, 0x99, 0x19, 0x3E,
        0xCC, 0xCC, 0x4C, 0x3D, 0x29, 0x5C, 0x0F, 0x3E, 0xB8, 0x1E, 0x05, 0x3E, 0x0A, 0xD7, 0xA3, 0x3D, 0xAE, 0x47, 0xE1, 0x3D, 0x9A, 0x99, 0x19, 0x3E, 0x0A, 0xD7, 0xA3, 0x3D, 0xCD, 0xCC, 0xCC, 0x3D,
        0x0A, 0xD7, 0xA3, 0x3D, 0x29, 0x5C, 0x0F, 0x3E, 0xAE, 0x47, 0xE1, 0x3D, 0xCC, 0xCC, 0xCC, 0x3D, 0xCC, 0xCC, 0x4C, 0x3D, 0xEB, 0x51, 0xB8, 0x3D, 0x08, 0xD7, 0xA3, 0x3C, 0x0A, 0xD7, 0xA3, 0x3D,
        0x08, 0xD7, 0x23, 0x3C, 0x00, 0x00, 0x00, 0x00, 0xCC, 0xCC, 0x4C, 0x3D, 0xCD, 0xCC, 0x4C, 0x3D, 0x8F, 0xC2, 0xF5, 0x3D, 0x8F, 0xC2, 0xF5, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x0A, 0xD7, 0x23, 0x3E,
        0x3D, 0x0A, 0x57, 0x3E, 0x0A, 0xD7, 0x23, 0x3E, 0x0A, 0xD7, 0xA3, 0x3D, 0x0A, 0xD7, 0x23, 0x3E, 0xB8, 0x1E, 0x05, 0x3E, 0x29, 0x5C, 0x0F, 0x3E, 0x28, 0x5C, 0x0F, 0x3E, 0xB8, 0x1E, 0x05, 0x3E,
        0x8F, 0xC2, 0xF5, 0x3D, 0xEC, 0x51, 0xB8, 0x3D, 0xCC, 0xCC, 0xCC, 0x3D, 0x00, 0x00, 0x00, 0x00, 0xCC, 0xCC, 0xCC, 0x3D, 0xEB, 0x51, 0xB8, 0x3D, 0x0A, 0xD7, 0x23, 0x3D, 0x9A, 0x99, 0x19, 0x3E,
        0x0C, 0xD7, 0xA3, 0xBC, 0xCC, 0xCC, 0x4C, 0x3E, 0x3D, 0x0A, 0x57, 0x3E, 0x0A, 0xD7, 0x23, 0x3C, 0x00, 0x00, 0x80, 0x3E, 0x29, 0x5C, 0x0F, 0x3E, 0x90, 0xC2, 0xF5, 0xBC, 0x8F, 0xC2, 0x75, 0x3D,
        0x00, 0x00, 0x80, 0x3E, 0x8F, 0xC2, 0x75, 0x3D, 0x29, 0x5C, 0x8F, 0xBD, 0x0A, 0xD7, 0xA3, 0x3D, 0x1E, 0x85, 0x6B, 0x3E, 0xEB, 0x51, 0x38, 0x3E, 0x29, 0x5C, 0x8F, 0xBD, 0x0A, 0xD7, 0xA3, 0x3C,
        0x0A, 0xD7, 0x23, 0x3E, 0x0A, 0xD7, 0xA3, 0x3C, 0x8F, 0xC2, 0xF5, 0x3D, 0x0A, 0xD7, 0x23, 0x3C, 0xB8, 0x1E, 0x05, 0x3E, 0x8F, 0xC2, 0xF5, 0x3C, 0xB8, 0x1E, 0x05, 0x3E, 0x29, 0x5C, 0x8F, 0x3D,
        0x28, 0x5C, 0x0F, 0x3E, 0x8F, 0xC2, 0xF5, 0x3D, 0x0A, 0xD7, 0xA3, 0x3D, 0xAE, 0x47, 0xE1, 0x3D, 0x8E, 0xC2, 0xF5, 0x3C, 0xEB, 0x51, 0xB8, 0x3D, 0x08, 0xD7, 0x23, 0x3C, 0xAE, 0x47, 0xE1, 0x3D,
        0x8F, 0xC2, 0xF5, 0x3D, 0x8F, 0xC2, 0xF5, 0x3C, 0x7A, 0x14, 0x2E, 0x3E, 0x8F, 0xC2, 0xF5, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x8F, 0xC2, 0xF5, 0x3D, 0x0C, 0xD7, 0xA3, 0xBC, 0xAE, 0x47, 0xE1, 0x3D,
        0xCE, 0xCC, 0x4C, 0xBD, 0xCC, 0xCC, 0xCC, 0x3D, 0x8E, 0xC2, 0x75, 0xBD, 0x0A, 0xD7, 0xA3, 0x3D, 0x29, 0x5C, 0x8F, 0xBD, 0xCC, 0xCC, 0x4C, 0x3D, 0x29, 0x5C, 0x8F, 0xBD, 0x8F, 0xC2, 0xF5, 0x3C,
        0x8E, 0xC2, 0x75, 0xBD, 0xAE, 0x47, 0xE1, 0x3D, 0xCC, 0xCC, 0xCC, 0x3D, 0xAE, 0x47, 0xE1, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x0A, 0xD7, 0x23, 0x3C, 0xAE, 0x47, 0x61, 0x3E, 0xCC, 0xCC, 0x4C, 0x3D,
        0xCC, 0xCC, 0x4C, 0x3E, 0xCC, 0xCC, 0x4C, 0x3D, 0xAE, 0x47, 0x61, 0x3E, 0xCC, 0xCC, 0x4C, 0x3D, 0x90, 0xC2, 0xF5, 0xBC, 0x0A, 0xD7, 0x23, 0x3D, 0x8E, 0xC2, 0x75, 0xBD, 0x0A, 0xD7, 0xA3, 0x3C,
        0x29, 0x5C, 0x8F, 0xBD, 0x0A, 0xD7, 0x23, 0x3D, 0x0A, 0xD7, 0xA3, 0x3D, 0x0A, 0xD7, 0x23, 0x3E, 0x28, 0x5C, 0x0F, 0x3E, 0x3D, 0x0A, 0x57, 0x3E, 0xB8, 0x1E, 0x05, 0x3E, 0xAE, 0x47, 0x61, 0x3E,
        0xCC, 0xCC, 0xCC, 0x3D, 0xAE, 0x47, 0x61, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x8F, 0xC2, 0xF5, 0x3D, 0x29, 0x5C, 0x8F, 0xBD, 0x8E, 0xC2, 0xF5, 0x3C, 0x0A, 0xD7, 0xA3, 0x3D, 0x0A, 0xD7, 0xA3, 0x3D,
        0x28, 0x5C, 0x8F, 0x3D, 0x08, 0xD7, 0x23, 0x3C, 0x08, 0xD7, 0x23, 0x3C, 0x8F, 0xC2, 0xF5, 0x3C, 0x0A, 0xD7, 0x23, 0x3D, 0xAE, 0x47, 0xE1, 0x3D, 0x28, 0x5C, 0x0F, 0x3E, 0x8F, 0xC2, 0xF5, 0x3C,
        0x8E, 0xC2, 0x75, 0x3E, 0x0A, 0xD7, 0xA3, 0x3C, 0x7A, 0x14, 0x2E, 0x3E, 0x8F, 0xC2, 0xF5, 0x3C, 0x0A, 0xD7, 0x23, 0x3E, 0x0A, 0xD7, 0xA3, 0x3C, 0xCC, 0xCC, 0xCC, 0x3D, 0x0A, 0xD7, 0xA3, 0x3C,
        0xAE, 0x47, 0x61, 0x3E, 0x8F, 0xC2, 0xF5, 0x3C, 0xEB, 0x51, 0x38, 0x3E, 0x0A, 0xD7, 0x23, 0x3D, 0x7A, 0x14, 0x2E, 0x3E, 0xCC, 0xCC, 0x4C, 0x3D, 0xB8, 0x1E, 0x05, 0x3E, 0x0A, 0xD7, 0x23, 0x3D,
        0xAE, 0x47, 0xE1, 0x3D, 0x0A, 0xD7, 0x23, 0x3D, 0x28, 0x5C, 0x8F, 0x3D, 0xCC, 0xCC, 0x4C, 0x3D, 0xCC, 0xCC, 0x4C, 0x3D, 0x0A, 0xD7, 0xA3, 0x3C, 0x0C, 0xD7, 0xA3, 0xBC, 0x0A, 0xD7, 0xA3, 0x3C,
        0x0A, 0xD7, 0x23, 0xBD, 0x0A, 0xD7, 0xA3, 0x3C, 0x0A, 0xD7, 0xA3, 0x3D, 0x0A, 0xD7, 0x23, 0x3D, 0x8E, 0xC2, 0x75, 0x3D, 0x0A, 0xD7, 0xA3, 0x3C, 0x8E, 0xC2, 0x75, 0x3E, 0x8F, 0xC2, 0xF5, 0x3C,
        0x1E, 0x85, 0x6B, 0x3E, 0x0A, 0xD7, 0x23, 0x3D, 0x5C, 0x8F, 0x42, 0x3E, 0x8F, 0xC2, 0xF5, 0x3C, 0xCC, 0xCC, 0xCC, 0x3D, 0x8F, 0xC2, 0xF5, 0x3C, 0xAE, 0x47, 0x61, 0x3E, 0xCC, 0xCC, 0x4C, 0x3D,
        0xEB, 0x51, 0xB8, 0x3D, 0x0A, 0xD7, 0xA3, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x8F, 0xC2, 0xF5, 0x3C, 0x0A, 0xD7, 0x23, 0xBD, 0x0A, 0xD7, 0xA3, 0x3C, 0x8E, 0xC2, 0x75, 0xBD, 0xAE, 0x47, 0xE1, 0x3D,
        0x0A, 0xD7, 0xA3, 0x3D, 0x7A, 0x14, 0x2E, 0x3E, 0x0A, 0xD7, 0xA3, 0x3D, 0xEB, 0x51, 0x38, 0x3E, 0xCC, 0xCC, 0xCC, 0x3D, 0xCC, 0xCC, 0x4C, 0x3D, 0xAE, 0x47, 0xE1, 0x3D, 0x28, 0x5C, 0x8F, 0x3D,
        0xCC, 0xCC, 0xCC, 0x3D, 0xAE, 0x47, 0xE1, 0x3D, 0x28, 0x5C, 0x8F, 0x3D, 0x99, 0x99, 0x19, 0x3E, 0x8E, 0xC2, 0x75, 0x3D, 0x7A, 0x14, 0x2E, 0x3E, 0x28, 0x5C, 0x8F, 0x3D, 0xAE, 0x47, 0xE1, 0x3D,
        0x3D, 0x0A, 0x57, 0x3E, 0x9A, 0x99, 0x19, 0x3E, 0x3D, 0x0A, 0x57, 0x3E, 0x21, 0x0A, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x03, 0x00, 0x04, 0x00, 0x04, 0x00, 0x05, 0x00, 0x05,
        0x00, 0x02, 0x00, 0x22, 0x04, 0x00, 0x06, 0x00, 0x07, 0x00, 0x08, 0x00, 0x09, 0x00, 0x23, 0x08, 0x00, 0x0A, 0x00, 0x0B, 0x00, 0x0C, 0x00, 0x0D, 0x00, 0x0E, 0x00, 0x0F, 0x00, 0x10, 0x00, 0x11,
        0x00, 0x24, 0x2A, 0x00, 0x12, 0x00, 0x13, 0x00, 0x14, 0x00, 0x15, 0x00, 0x16, 0x00, 0x17, 0x00, 0x17, 0x00, 0x18, 0x00, 0x18, 0x00, 0x19, 0x00, 0x19, 0x00, 0x1A, 0x00, 0x1A, 0x00, 0x1B, 0x00,
        0x1B, 0x00, 0x1C, 0x00, 0x1C, 0x00, 0x1D, 0x00, 0x1D, 0x00, 0x1E, 0x00, 0x1E, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x20, 0x00, 0x20, 0x00, 0x21, 0x00, 0x21, 0x00, 0x22, 0x00, 0x22, 0x00, 0x11, 0x00,
        0x11, 0x00, 0x23, 0x00, 0x23, 0x00, 0x24, 0x00, 0x24, 0x00, 0x25, 0x00, 0x25, 0x00, 0x26, 0x00, 0x26, 0x00, 0x05, 0x00, 0x05, 0x00, 0x27, 0x00, 0x25, 0x34, 0x00, 0x28, 0x00, 0x29, 0x00, 0x19,
        0x00, 0x2A, 0x00, 0x2A, 0x00, 0x2B, 0x00, 0x2B, 0x00, 0x2C, 0x00, 0x2C, 0x00, 0x2D, 0x00, 0x2D, 0x00, 0x2E, 0x00, 0x2E, 0x00, 0x1C, 0x00, 0x1C, 0x00, 0x1B, 0x00, 0x1B, 0x00, 0x2F, 0x00, 0x2F,
        0x00, 0x30, 0x00, 0x30, 0x00, 0x19, 0x00, 0x19, 0x00, 0x31, 0x00, 0x31, 0x00, 0x32, 0x00, 0x32, 0x00, 0x33, 0x00, 0x33, 0x00, 0x34, 0x00, 0x34, 0x00, 0x28, 0x00, 0x35, 0x00, 0x36, 0x00, 0x36,
        0x00, 0x37, 0x00, 0x37, 0x00, 0x38, 0x00, 0x38, 0x00, 0x39, 0x00, 0x39, 0x00, 0x3A, 0x00, 0x3A, 0x00, 0x3B, 0x00, 0x3B, 0x00, 0x3C, 0x00, 0x3C, 0x00, 0x3D, 0x00, 0x3D, 0x00, 0x3E, 0x00, 0x3E,
        0x00, 0x35, 0x00, 0x26, 0x3C, 0x00, 0x3F, 0x00, 0x40, 0x00, 0x40, 0x00, 0x41, 0x00, 0x41, 0x00, 0x42, 0x00, 0x42, 0x00, 0x43, 0x00, 0x43, 0x00, 0x44, 0x00, 0x44, 0x00, 0x11, 0x00, 0x11, 0x00,
        0x45, 0x00, 0x45, 0x00, 0x46, 0x00, 0x46, 0x00, 0x47, 0x00, 0x47, 0x00, 0x48, 0x00, 0x48, 0x00, 0x05, 0x00, 0x05, 0x00, 0x02, 0x00, 0x02, 0x00, 0x49, 0x00, 0x49, 0x00, 0x10, 0x00, 0x10, 0x00,
        0x4A, 0x00, 0x4A, 0x00, 0x4B, 0x00, 0x4B, 0x00, 0x4C, 0x00, 0x4C, 0x00, 0x4D, 0x00, 0x4D, 0x00, 0x4E, 0x00, 0x4E, 0x00, 0x4F, 0x00, 0x4F, 0x00, 0x50, 0x00, 0x50, 0x00, 0x08, 0x00, 0x08, 0x00,
        0x51, 0x00, 0x51, 0x00, 0x52, 0x00, 0x52, 0x00, 0x53, 0x00, 0x53, 0x00, 0x54, 0x00, 0x54, 0x00, 0x55, 0x00, 0x55, 0x00, 0x56, 0x00, 0x56, 0x00, 0x57, 0x00, 0x57, 0x00, 0x58, 0x00, 0x27, 0x0C,
        0x00, 0x59, 0x00, 0x5A, 0x00, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1A, 0x00, 0x1A, 0x00, 0x5B, 0x00, 0x5B, 0x00, 0x5C, 0x00, 0x5C, 0x00, 0x5D, 0x00, 0x28, 0x12, 0x00, 0x5E, 0x00, 0x5F, 0x00,
        0x5F, 0x00, 0x60, 0x00, 0x60, 0x00, 0x5C, 0x00, 0x5C, 0x00, 0x61, 0x00, 0x61, 0x00, 0x62, 0x00, 0x62, 0x00, 0x02, 0x00, 0x02, 0x00, 0x63, 0x00, 0x63, 0x00, 0x64, 0x00, 0x64, 0x00, 0x0D, 0x00,
        0x29, 0x12, 0x00, 0x65, 0x00, 0x66, 0x00, 0x66, 0x00, 0x67, 0x00, 0x67, 0x00, 0x68, 0x00, 0x68, 0x00, 0x69, 0x00, 0x69, 0x00, 0x6A, 0x00, 0x6A, 0x00, 0x6B, 0x00, 0x6B, 0x00, 0x6C, 0x00, 0x6C,
        0x00, 0x6D, 0x00, 0x6D, 0x00, 0x6E, 0x00, 0x2A, 0x06, 0x00, 0x6F, 0x00, 0x70, 0x00, 0x71, 0x00, 0x72, 0x00, 0x73, 0x00, 0x10, 0x00, 0x2B, 0x04, 0x00, 0x74, 0x00, 0x25, 0x00, 0x75, 0x00, 0x76,
        0x00, 0x2C, 0x0C, 0x00, 0x77, 0x00, 0x78, 0x00, 0x78, 0x00, 0x49, 0x00, 0x49, 0x00, 0x79, 0x00, 0x79, 0x00, 0x77, 0x00, 0x77, 0x00, 0x7A, 0x00, 0x7A, 0x00, 0x29, 0x00, 0x2D, 0x02, 0x00, 0x75,
        0x00, 0x76, 0x00, 0x2E, 0x08, 0x00, 0x79, 0x00, 0x49, 0x00, 0x49, 0x00, 0x78, 0x00, 0x78, 0x00, 0x77, 0x00, 0x77, 0x00, 0x79, 0x00, 0x2F, 0x02, 0x00, 0x7B, 0x00, 0x6E, 0x00, 0x30, 0x20, 0x00,
        0x7C, 0x00, 0x60, 0x00, 0x60, 0x00, 0x7D, 0x00, 0x7D, 0x00, 0x71, 0x00, 0x71, 0x00, 0x75, 0x00, 0x75, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x7F, 0x00, 0x7F, 0x00, 0x80, 0x00, 0x80, 0x00, 0x47, 0x00,
        0x47, 0x00, 0x81, 0x00, 0x81, 0x00, 0x82, 0x00, 0x82, 0x00, 0x83, 0x00, 0x83, 0x00, 0x84, 0x00, 0x84, 0x00, 0x85, 0x00, 0x85, 0x00, 0x86, 0x00, 0x86, 0x00, 0x08, 0x00, 0x08, 0x00, 0x7C, 0x00,
        0x31, 0x06, 0x00, 0x87, 0x00, 0x5B, 0x00, 0x5B, 0x00, 0x19, 0x00, 0x19, 0x00, 0x26, 0x00, 0x32, 0x1A, 0x00, 0x5C, 0x00, 0x7D, 0x00, 0x7D, 0x00, 0x88, 0x00, 0x88, 0x00, 0x60, 0x00, 0x60, 0x00,
        0x19, 0x00, 0x19, 0x00, 0x18, 0x00, 0x18, 0x00, 0x86, 0x00, 0x86, 0x00, 0x89, 0x00, 0x89, 0x00, 0x85, 0x00, 0x85, 0x00, 0x8A, 0x00, 0x8A, 0x00, 0x8B, 0x00, 0x8B, 0x00, 0x20, 0x00, 0x20, 0x00,
        0x29, 0x00, 0x29, 0x00, 0x8C, 0x00, 0x33, 0x1C, 0x00, 0x8D, 0x00, 0x8E, 0x00, 0x8E, 0x00, 0x8F, 0x00, 0x8F, 0x00, 0x90, 0x00, 0x90, 0x00, 0x91, 0x00, 0x91, 0x00, 0x92, 0x00, 0x92, 0x00, 0x93,
        0x00, 0x93, 0x00, 0x11, 0x00, 0x11, 0x00, 0x56, 0x00, 0x56, 0x00, 0x81, 0x00, 0x81, 0x00, 0x47, 0x00, 0x47, 0x00, 0x26, 0x00, 0x26, 0x00, 0x05, 0x00, 0x05, 0x00, 0x02, 0x00, 0x02, 0x00, 0x49,
        0x00, 0x34, 0x06, 0x00, 0x94, 0x00, 0x62, 0x00, 0x62, 0x00, 0x95, 0x00, 0x94, 0x00, 0x96, 0x00, 0x35, 0x20, 0x00, 0x97, 0x00, 0x8D, 0x00, 0x8D, 0x00, 0x0E, 0x00, 0x0E, 0x00, 0x1E, 0x00, 0x1E,
        0x00, 0x98, 0x00, 0x98, 0x00, 0x09, 0x00, 0x09, 0x00, 0x99, 0x00, 0x99, 0x00, 0x92, 0x00, 0x92, 0x00, 0x93, 0x00, 0x93, 0x00, 0x11, 0x00, 0x11, 0x00, 0x56, 0x00, 0x56, 0x00, 0x81, 0x00, 0x81,
        0x00, 0x47, 0x00, 0x47, 0x00, 0x26, 0x00, 0x26, 0x00, 0x05, 0x00, 0x05, 0x00, 0x02, 0x00, 0x02, 0x00, 0x49, 0x00, 0x36, 0x2C, 0x00, 0x9A, 0x00, 0x86, 0x00, 0x86, 0x00, 0x08, 0x00, 0x08, 0x00,
        0x7C, 0x00, 0x7C, 0x00, 0x60, 0x00, 0x60, 0x00, 0x7D, 0x00, 0x7D, 0x00, 0x71, 0x00, 0x71, 0x00, 0x62, 0x00, 0x62, 0x00, 0x78, 0x00, 0x78, 0x00, 0x7F, 0x00, 0x7F, 0x00, 0x80, 0x00, 0x80, 0x00,
        0x9B, 0x00, 0x9B, 0x00, 0x46, 0x00, 0x46, 0x00, 0x45, 0x00, 0x45, 0x00, 0x9C, 0x00, 0x9C, 0x00, 0x9D, 0x00, 0x9D, 0x00, 0x9E, 0x00, 0x9E, 0x00, 0x73, 0x00, 0x73, 0x00, 0x8F, 0x00, 0x8F, 0x00,
        0x54, 0x00, 0x54, 0x00, 0x9F, 0x00, 0x9F, 0x00, 0xA0, 0x00, 0xA0, 0x00, 0x62, 0x00, 0x37, 0x04, 0x00, 0xA1, 0x00, 0x48, 0x00, 0x06, 0x00, 0xA1, 0x00, 0x38, 0x38, 0x00, 0x19, 0x00, 0x1A, 0x00,
        0x1A, 0x00, 0xA2, 0x00, 0xA2, 0x00, 0x5C, 0x00, 0x5C, 0x00, 0x2E, 0x00, 0x2E, 0x00, 0xA3, 0x00, 0xA3, 0x00, 0xA4, 0x00, 0xA4, 0x00, 0xA5, 0x00, 0xA5, 0x00, 0xA6, 0x00, 0xA6, 0x00, 0x35, 0x00,
        0x35, 0x00, 0xA7, 0x00, 0xA7, 0x00, 0xA8, 0x00, 0xA8, 0x00, 0x24, 0x00, 0x24, 0x00, 0x25, 0x00, 0x25, 0x00, 0x26, 0x00, 0x26, 0x00, 0x05, 0x00, 0x05, 0x00, 0x02, 0x00, 0x02, 0x00, 0x49, 0x00,
        0x49, 0x00, 0x62, 0x00, 0x62, 0x00, 0xA9, 0x00, 0xA9, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAB, 0x00, 0xAB, 0x00, 0x90, 0x00, 0x90, 0x00, 0xAC, 0x00, 0xAC, 0x00, 0xAD, 0x00, 0xAD, 0x00, 0xAE, 0x00,
        0xAE, 0x00, 0x17, 0x00, 0x17, 0x00, 0x18, 0x00, 0x18, 0x00, 0x19, 0x00, 0x39, 0x2C, 0x00, 0xAF, 0x00, 0xB0, 0x00, 0xB0, 0x00, 0xB1, 0x00, 0xB1, 0x00, 0xB2, 0x00, 0xB2, 0x00, 0xB3, 0x00, 0xB3,
        0x00, 0xB4, 0x00, 0xB4, 0x00, 0xB5, 0x00, 0xB5, 0x00, 0x07, 0x00, 0x07, 0x00, 0x5D, 0x00, 0x5D, 0x00, 0xA2, 0x00, 0xA2, 0x00, 0x60, 0x00, 0x60, 0x00, 0x7C, 0x00, 0x7C, 0x00, 0xB6, 0x00, 0xB6,
        0x00, 0x50, 0x00, 0x50, 0x00, 0x9A, 0x00, 0x9A, 0x00, 0xAF, 0x00, 0xAF, 0x00, 0xA6, 0x00, 0xA6, 0x00, 0xB7, 0x00, 0xB7, 0x00, 0x46, 0x00, 0x46, 0x00, 0x9B, 0x00, 0x9B, 0x00, 0x26, 0x00, 0x26,
        0x00, 0x05, 0x00, 0x05, 0x00, 0x78, 0x00, 0x3A, 0x10, 0x00, 0x0E, 0x00, 0x61, 0x00, 0x61, 0x00, 0xA0, 0x00, 0xA0, 0x00, 0xB8, 0x00, 0xB8, 0x00, 0x0E, 0x00, 0x79, 0x00, 0x49, 0x00, 0x49, 0x00,
        0x78, 0x00, 0x78, 0x00, 0x77, 0x00, 0x77, 0x00, 0x79, 0x00, 0x3B, 0x14, 0x00, 0x0E, 0x00, 0x61, 0x00, 0x61, 0x00, 0xA0, 0x00, 0xA0, 0x00, 0xB8, 0x00, 0xB8, 0x00, 0x0E, 0x00, 0x77, 0x00, 0x78,
        0x00, 0x78, 0x00, 0x49, 0x00, 0x49, 0x00, 0x79, 0x00, 0x79, 0x00, 0x77, 0x00, 0x77, 0x00, 0x7A, 0x00, 0x7A, 0x00, 0x29, 0x00, 0x3C, 0x04, 0x00, 0xB9, 0x00, 0x75, 0x00, 0x75, 0x00, 0xBA, 0x00,
        0x3D, 0x04, 0x00, 0x71, 0x00, 0xBB, 0x00, 0x10, 0x00, 0xBC, 0x00, 0x3E, 0x04, 0x00, 0x1B, 0x00, 0xBD, 0x00, 0xBD, 0x00, 0x29, 0x00, 0x3F, 0x22, 0x00, 0x1C, 0x00, 0x87, 0x00, 0x87, 0x00, 0x59,
        0x00, 0x59, 0x00, 0x1A, 0x00, 0x1A, 0x00, 0xBE, 0x00, 0xBE, 0x00, 0x08, 0x00, 0x08, 0x00, 0x50, 0x00, 0x50, 0x00, 0xBF, 0x00, 0xBF, 0x00, 0xC0, 0x00, 0xC0, 0x00, 0xC1, 0x00, 0xC1, 0x00, 0x99,
        0x00, 0x99, 0x00, 0x73, 0x00, 0x73, 0x00, 0xC2, 0x00, 0xC2, 0x00, 0xC3, 0x00, 0x6B, 0x00, 0xC4, 0x00, 0xC4, 0x00, 0x80, 0x00, 0x80, 0x00, 0xC5, 0x00, 0xC5, 0x00, 0x6B, 0x00, 0x40, 0x34, 0x00,
        0x90, 0x00, 0xC6, 0x00, 0xC6, 0x00, 0xC7, 0x00, 0xC7, 0x00, 0xC8, 0x00, 0xC8, 0x00, 0xC9, 0x00, 0xC9, 0x00, 0x1D, 0x00, 0x1D, 0x00, 0x61, 0x00, 0x61, 0x00, 0xCA, 0x00, 0xCA, 0x00, 0xCB, 0x00,
        0xCB, 0x00, 0xCC, 0x00, 0xCC, 0x00, 0xCD, 0x00, 0xCD, 0x00, 0xCE, 0x00, 0xCE, 0x00, 0xCF, 0x00, 0xC8, 0x00, 0x2E, 0x00, 0x2E, 0x00, 0xB5, 0x00, 0xB5, 0x00, 0x4A, 0x00, 0x4A, 0x00, 0xD0, 0x00,
        0xD0, 0x00, 0xCC, 0x00, 0xD1, 0x00, 0xCF, 0x00, 0xCF, 0x00, 0xD2, 0x00, 0xD2, 0x00, 0xD3, 0x00, 0xD3, 0x00, 0xD4, 0x00, 0xD4, 0x00, 0x95, 0x00, 0x95, 0x00, 0xD5, 0x00, 0xD5, 0x00, 0xD6, 0x00,
        0xD6, 0x00, 0xD7, 0x00, 0xD7, 0x00, 0xD8, 0x00, 0x41, 0x06, 0x00, 0x08, 0x00, 0x29, 0x00, 0x08, 0x00, 0xBA, 0x00, 0xD9, 0x00, 0x9D, 0x00, 0x42, 0x24, 0x00, 0x06, 0x00, 0x29, 0x00, 0x06, 0x00,
        0x18, 0x00, 0x18, 0x00, 0x17, 0x00, 0x17, 0x00, 0x33, 0x00, 0x33, 0x00, 0xD8, 0x00, 0xD8, 0x00, 0xDA, 0x00, 0xDA, 0x00, 0xDB, 0x00, 0xDB, 0x00, 0x91, 0x00, 0x91, 0x00, 0xDC, 0x00, 0x61, 0x00,
        0xDC, 0x00, 0xDC, 0x00, 0x9E, 0x00, 0x9E, 0x00, 0xA6, 0x00, 0xA6, 0x00, 0x35, 0x00, 0x35, 0x00, 0xA7, 0x00, 0xA7, 0x00, 0xA8, 0x00, 0xA8, 0x00, 0x24, 0x00, 0x24, 0x00, 0x25, 0x00, 0x25, 0x00,
        0x29, 0x00, 0x43, 0x22, 0x00, 0xDD, 0x00, 0x16, 0x00, 0x16, 0x00, 0x17, 0x00, 0x17, 0x00, 0x94, 0x00, 0x94, 0x00, 0x7C, 0x00, 0x7C, 0x00, 0x67, 0x00, 0x67, 0x00, 0x5B, 0x00, 0x5B, 0x00, 0x5C,
        0x00, 0x5C, 0x00, 0xDE, 0x00, 0xDE, 0x00, 0xCA, 0x00, 0xCA, 0x00, 0x79, 0x00, 0x79, 0x00, 0xDF, 0x00, 0xDF, 0x00, 0xE0, 0x00, 0xE0, 0x00, 0x80, 0x00, 0x80, 0x00, 0x96, 0x00, 0x96, 0x00, 0x24,
        0x00, 0x24, 0x00, 0x23, 0x00, 0x23, 0x00, 0xE1, 0x00, 0x44, 0x18, 0x00, 0x06, 0x00, 0x29, 0x00, 0x06, 0x00, 0xB6, 0x00, 0xB6, 0x00, 0x50, 0x00, 0x50, 0x00, 0x9A, 0x00, 0x9A, 0x00, 0xAD, 0x00,
        0xAD, 0x00, 0xE2, 0x00, 0xE2, 0x00, 0x93, 0x00, 0x93, 0x00, 0xD4, 0x00, 0xD4, 0x00, 0x45, 0x00, 0x45, 0x00, 0x46, 0x00, 0x46, 0x00, 0x9B, 0x00, 0x9B, 0x00, 0x29, 0x00, 0x45, 0x08, 0x00, 0x06,
        0x00, 0x29, 0x00, 0x06, 0x00, 0x8E, 0x00, 0x61, 0x00, 0xE3, 0x00, 0x29, 0x00, 0x39, 0x00, 0x46, 0x06, 0x00, 0x06, 0x00, 0x29, 0x00, 0x06, 0x00, 0x8E, 0x00, 0x61, 0x00, 0xE3, 0x00, 0x47, 0x26,
        0x00, 0xDD, 0x00, 0x16, 0x00, 0x16, 0x00, 0x17, 0x00, 0x17, 0x00, 0x94, 0x00, 0x94, 0x00, 0x7C, 0x00, 0x7C, 0x00, 0x67, 0x00, 0x67, 0x00, 0x5B, 0x00, 0x5B, 0x00, 0x5C, 0x00, 0x5C, 0x00, 0xDE,
        0x00, 0xDE, 0x00, 0xCA, 0x00, 0xCA, 0x00, 0x79, 0x00, 0x79, 0x00, 0xDF, 0x00, 0xDF, 0x00, 0xE0, 0x00, 0xE0, 0x00, 0x80, 0x00, 0x80, 0x00, 0x96, 0x00, 0x96, 0x00, 0x24, 0x00, 0x24, 0x00, 0x23,
        0x00, 0x23, 0x00, 0xE1, 0x00, 0xE1, 0x00, 0xE4, 0x00, 0xE5, 0x00, 0xE4, 0x00, 0x48, 0x06, 0x00, 0x06, 0x00, 0x29, 0x00, 0xA1, 0x00, 0x8C, 0x00, 0x61, 0x00, 0xE6, 0x00, 0x49, 0x02, 0x00, 0x06,
        0x00, 0x29, 0x00, 0x4A, 0x12, 0x00, 0x94, 0x00, 0xE7, 0x00, 0xE7, 0x00, 0xE8, 0x00, 0xE8, 0x00, 0xE9, 0x00, 0xE9, 0x00, 0x80, 0x00, 0x80, 0x00, 0x48, 0x00, 0x48, 0x00, 0x05, 0x00, 0x05, 0x00,
        0x02, 0x00, 0x02, 0x00, 0xEA, 0x00, 0xEA, 0x00, 0x62, 0x00, 0x4B, 0x06, 0x00, 0x06, 0x00, 0x29, 0x00, 0xA1, 0x00, 0x62, 0x00, 0xEB, 0x00, 0x8C, 0x00, 0x4C, 0x04, 0x00, 0x06, 0x00, 0x29, 0x00,
        0x29, 0x00, 0xEC, 0x00, 0x4D, 0x08, 0x00, 0x06, 0x00, 0x29, 0x00, 0x06, 0x00, 0x47, 0x00, 0xED, 0x00, 0x47, 0x00, 0xED, 0x00, 0xBA, 0x00, 0x4E, 0x06, 0x00, 0x06, 0x00, 0x29, 0x00, 0x06, 0x00,
        0x8C, 0x00, 0xA1, 0x00, 0x8C, 0x00, 0x4F, 0x28, 0x00, 0x7C, 0x00, 0x67, 0x00, 0x67, 0x00, 0x5B, 0x00, 0x5B, 0x00, 0x5C, 0x00, 0x5C, 0x00, 0xDE, 0x00, 0xDE, 0x00, 0xCA, 0x00, 0xCA, 0x00, 0x79,
        0x00, 0x79, 0x00, 0xDF, 0x00, 0xDF, 0x00, 0xE0, 0x00, 0xE0, 0x00, 0x80, 0x00, 0x80, 0x00, 0x96, 0x00, 0x96, 0x00, 0x24, 0x00, 0x24, 0x00, 0x23, 0x00, 0x23, 0x00, 0xE1, 0x00, 0xE1, 0x00, 0xEE,
        0x00, 0xEE, 0x00, 0xEF, 0x00, 0xEF, 0x00, 0xDD, 0x00, 0xDD, 0x00, 0x16, 0x00, 0x16, 0x00, 0x17, 0x00, 0x17, 0x00, 0x94, 0x00, 0x94, 0x00, 0x7C, 0x00, 0x50, 0x14, 0x00, 0x06, 0x00, 0x29, 0x00,
        0x06, 0x00, 0x18, 0x00, 0x18, 0x00, 0x17, 0x00, 0x17, 0x00, 0x33, 0x00, 0x33, 0x00, 0xD8, 0x00, 0xD8, 0x00, 0xF0, 0x00, 0xF0, 0x00, 0xF1, 0x00, 0xF1, 0x00, 0xB0, 0x00, 0xB0, 0x00, 0xF2, 0x00,
        0xF2, 0x00, 0xF3, 0x00, 0x51, 0x2A, 0x00, 0x7C, 0x00, 0x67, 0x00, 0x67, 0x00, 0x5B, 0x00, 0x5B, 0x00, 0x5C, 0x00, 0x5C, 0x00, 0xDE, 0x00, 0xDE, 0x00, 0xCA, 0x00, 0xCA, 0x00, 0x79, 0x00, 0x79,
        0x00, 0xDF, 0x00, 0xDF, 0x00, 0xE0, 0x00, 0xE0, 0x00, 0x80, 0x00, 0x80, 0x00, 0x96, 0x00, 0x96, 0x00, 0x24, 0x00, 0x24, 0x00, 0x23, 0x00, 0x23, 0x00, 0xE1, 0x00, 0xE1, 0x00, 0xEE, 0x00, 0xEE,
        0x00, 0xEF, 0x00, 0xEF, 0x00, 0xDD, 0x00, 0xDD, 0x00, 0x16, 0x00, 0x16, 0x00, 0x17, 0x00, 0x17, 0x00, 0x94, 0x00, 0x94, 0x00, 0x7C, 0x00, 0xF4, 0x00, 0xF5, 0x00, 0x52, 0x16, 0x00, 0x06, 0x00,
        0x29, 0x00, 0x06, 0x00, 0x18, 0x00, 0x18, 0x00, 0x17, 0x00, 0x17, 0x00, 0x33, 0x00, 0x33, 0x00, 0xD8, 0x00, 0xD8, 0x00, 0xDA, 0x00, 0xDA, 0x00, 0xDB, 0x00, 0xDB, 0x00, 0x91, 0x00, 0x91, 0x00,
        0xDC, 0x00, 0xDC, 0x00, 0x61, 0x00, 0x69, 0x00, 0x8C, 0x00, 0x53, 0x26, 0x00, 0x16, 0x00, 0x17, 0x00, 0x17, 0x00, 0x18, 0x00, 0x18, 0x00, 0x19, 0x00, 0x19, 0x00, 0x1A, 0x00, 0x1A, 0x00, 0x1B,
        0x00, 0x1B, 0x00, 0x1C, 0x00, 0x1C, 0x00, 0x1D, 0x00, 0x1D, 0x00, 0x1E, 0x00, 0x1E, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x20, 0x00, 0x20, 0x00, 0x21, 0x00, 0x21, 0x00, 0x22, 0x00, 0x22, 0x00, 0x11,
        0x00, 0x11, 0x00, 0x23, 0x00, 0x23, 0x00, 0x24, 0x00, 0x24, 0x00, 0x25, 0x00, 0x25, 0x00, 0x26, 0x00, 0x26, 0x00, 0x05, 0x00, 0x05, 0x00, 0x27, 0x00, 0x54, 0x04, 0x00, 0xB6, 0x00, 0x9B, 0x00,
        0x06, 0x00, 0xA1, 0x00, 0x55, 0x12, 0x00, 0x06, 0x00, 0x10, 0x00, 0x10, 0x00, 0x78, 0x00, 0x78, 0x00, 0x7F, 0x00, 0x7F, 0x00, 0x80, 0x00, 0x80, 0x00, 0x47, 0x00, 0x47, 0x00, 0x81, 0x00, 0x81,
        0x00, 0x56, 0x00, 0x56, 0x00, 0x11, 0x00, 0x11, 0x00, 0xA1, 0x00, 0x56, 0x04, 0x00, 0x06, 0x00, 0x47, 0x00, 0xED, 0x00, 0x47, 0x00, 0x57, 0x08, 0x00, 0x06, 0x00, 0x26, 0x00, 0x94, 0x00, 0x26,
        0x00, 0x94, 0x00, 0x3A, 0x00, 0xF6, 0x00, 0x3A, 0x00, 0x58, 0x04, 0x00, 0x06, 0x00, 0x8C, 0x00, 0xA1, 0x00, 0x29, 0x00, 0x59, 0x06, 0x00, 0x06, 0x00, 0xE3, 0x00, 0xE3, 0x00, 0x47, 0x00, 0xED,
        0x00, 0xE3, 0x00, 0x5A, 0x06, 0x00, 0xA1, 0x00, 0x29, 0x00, 0x06, 0x00, 0xA1, 0x00, 0x29, 0x00, 0x8C, 0x00, 0x5B, 0x08, 0x00, 0x65, 0x00, 0x6E, 0x00, 0xF7, 0x00, 0x0B, 0x00, 0x65, 0x00, 0x5E,
        0x00, 0x6E, 0x00, 0x0D, 0x00, 0x5C, 0x02, 0x00, 0x06, 0x00, 0xF8, 0x00, 0x5D, 0x08, 0x00, 0xF9, 0x00, 0xFA, 0x00, 0x5E, 0x00, 0x0D, 0x00, 0x65, 0x00, 0x5E, 0x00, 0x6E, 0x00, 0x0D, 0x00, 0x5E,
        0x04, 0x00, 0xFB, 0x00, 0x75, 0x00, 0xFB, 0x00, 0xBD, 0x00, 0x5F, 0x02, 0x00, 0x6E, 0x00, 0xFC, 0x00, 0x60, 0x0C, 0x00, 0xFD, 0x00, 0x07, 0x00, 0x07, 0x00, 0x71, 0x00, 0x71, 0x00, 0xB5, 0x00,
        0xB5, 0x00, 0xFE, 0x00, 0xFE, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x71, 0x00, 0x61, 0x1C, 0x00, 0xAC, 0x00, 0xEC, 0x00, 0xB0, 0x00, 0x90, 0x00, 0x90, 0x00, 0x09, 0x00, 0x09, 0x00, 0x98, 0x00, 0x98,
        0x00, 0x00, 0x01, 0x00, 0x01, 0xB5, 0x00, 0xB5, 0x00, 0xCA, 0x00, 0xCA, 0x00, 0x10, 0x00, 0x10, 0x00, 0x78, 0x00, 0x78, 0x00, 0x7F, 0x00, 0x7F, 0x00, 0x26, 0x00, 0x26, 0x00, 0x47, 0x00, 0x47,
        0x00, 0x46, 0x00, 0x46, 0x00, 0x45, 0x00, 0x62, 0x1C, 0x00, 0x06, 0x00, 0x29, 0x00, 0x61, 0x00, 0x1E, 0x00, 0x1E, 0x00, 0x2D, 0x00, 0x2D, 0x00, 0x01, 0x01, 0x01, 0x01, 0x4C, 0x00, 0x4C, 0x00,
        0xA5, 0x00, 0xA5, 0x00, 0x02, 0x01, 0x02, 0x01, 0x36, 0x00, 0x36, 0x00, 0x03, 0x01, 0x03, 0x01, 0x04, 0x01, 0x04, 0x01, 0x9B, 0x00, 0x9B, 0x00, 0x48, 0x00, 0x48, 0x00, 0x05, 0x00, 0x05, 0x00,
        0x27, 0x00, 0x63, 0x1A, 0x00, 0xB0, 0x00, 0x90, 0x00, 0x90, 0x00, 0x09, 0x00, 0x09, 0x00, 0x98, 0x00, 0x98, 0x00, 0x00, 0x01, 0x00, 0x01, 0xB5, 0x00, 0xB5, 0x00, 0xCA, 0x00, 0xCA, 0x00, 0x10,
        0x00, 0x10, 0x00, 0x78, 0x00, 0x78, 0x00, 0x7F, 0x00, 0x7F, 0x00, 0x26, 0x00, 0x26, 0x00, 0x47, 0x00, 0x47, 0x00, 0x46, 0x00, 0x46, 0x00, 0x45, 0x00, 0x64, 0x1C, 0x00, 0x97, 0x00, 0xEC, 0x00,
        0xB0, 0x00, 0x90, 0x00, 0x90, 0x00, 0x09, 0x00, 0x09, 0x00, 0x98, 0x00, 0x98, 0x00, 0x00, 0x01, 0x00, 0x01, 0xB5, 0x00, 0xB5, 0x00, 0xCA, 0x00, 0xCA, 0x00, 0x10, 0x00, 0x10, 0x00, 0x78, 0x00,
        0x78, 0x00, 0x7F, 0x00, 0x7F, 0x00, 0x26, 0x00, 0x26, 0x00, 0x47, 0x00, 0x47, 0x00, 0x46, 0x00, 0x46, 0x00, 0x45, 0x00, 0x65, 0x20, 0x00, 0xCA, 0x00, 0x02, 0x01, 0x02, 0x01, 0x9E, 0x00, 0x9E,
        0x00, 0x05, 0x01, 0x05, 0x01, 0x90, 0x00, 0x90, 0x00, 0x09, 0x00, 0x09, 0x00, 0x98, 0x00, 0x98, 0x00, 0x00, 0x01, 0x00, 0x01, 0xB5, 0x00, 0xB5, 0x00, 0xCA, 0x00, 0xCA, 0x00, 0x10, 0x00, 0x10,
        0x00, 0x78, 0x00, 0x78, 0x00, 0x7F, 0x00, 0x7F, 0x00, 0x26, 0x00, 0x26, 0x00, 0x47, 0x00, 0x47, 0x00, 0x46, 0x00, 0x46, 0x00, 0x45, 0x00, 0x66, 0x0A, 0x00, 0x08, 0x00, 0x7C, 0x00, 0x7C, 0x00,
        0x67, 0x00, 0x67, 0x00, 0x06, 0x01, 0x06, 0x01, 0x07, 0x01, 0x07, 0x00, 0x01, 0x01, 0x67, 0x26, 0x00, 0xAC, 0x00, 0x08, 0x01, 0x08, 0x01, 0x09, 0x01, 0x09, 0x01, 0x0A, 0x01, 0x0A, 0x01, 0x0B,
        0x01, 0x0B, 0x01, 0x0C, 0x01, 0x0C, 0x01, 0x0D, 0x01, 0xB0, 0x00, 0x90, 0x00, 0x90, 0x00, 0x09, 0x00, 0x09, 0x00, 0x98, 0x00, 0x98, 0x00, 0x00, 0x01, 0x00, 0x01, 0xB5, 0x00, 0xB5, 0x00, 0xCA,
        0x00, 0xCA, 0x00, 0x10, 0x00, 0x10, 0x00, 0x78, 0x00, 0x78, 0x00, 0x7F, 0x00, 0x7F, 0x00, 0x26, 0x00, 0x26, 0x00, 0x47, 0x00, 0x47, 0x00, 0x46, 0x00, 0x46, 0x00, 0x45, 0x00, 0x68, 0x0E, 0x00,
        0x06, 0x00, 0x29, 0x00, 0xF3, 0x00, 0x00, 0x01, 0x00, 0x01, 0x98, 0x00, 0x98, 0x00, 0x09, 0x00, 0x09, 0x00, 0x90, 0x00, 0x90, 0x00, 0x0E, 0x01, 0x0E, 0x01, 0x0F, 0x01, 0x69, 0x0A, 0x00, 0x06,
        0x00, 0x2F, 0x00, 0x2F, 0x00, 0x8D, 0x00, 0x8D, 0x00, 0x10, 0x01, 0x10, 0x01, 0x06, 0x00, 0x1D, 0x00, 0x04, 0x00, 0x6A, 0x10, 0x00, 0xBE, 0x00, 0x11, 0x01, 0x11, 0x01, 0x7C, 0x00, 0x7C, 0x00,
        0x12, 0x01, 0x12, 0x01, 0xBE, 0x00, 0x98, 0x00, 0x13, 0x01, 0x13, 0x01, 0x14, 0x01, 0x14, 0x01, 0x15, 0x01, 0x15, 0x01, 0x6E, 0x00, 0x6B, 0x06, 0x00, 0x06, 0x00, 0x29, 0x00, 0x4D, 0x00, 0x49,
        0x00, 0x16, 0x01, 0x0F, 0x01, 0x6C, 0x02, 0x00, 0x06, 0x00, 0x29, 0x00, 0x6D, 0x1A, 0x00, 0x07, 0x00, 0x29, 0x00, 0xF3, 0x00, 0x00, 0x01, 0x00, 0x01, 0x98, 0x00, 0x98, 0x00, 0x09, 0x00, 0x09,
        0x00, 0x90, 0x00, 0x90, 0x00, 0x0E, 0x01, 0x0E, 0x01, 0x0F, 0x01, 0x0E, 0x01, 0xE2, 0x00, 0xE2, 0x00, 0x17, 0x01, 0x17, 0x01, 0x41, 0x00, 0x41, 0x00, 0x18, 0x01, 0x18, 0x01, 0x19, 0x01, 0x19,
        0x01, 0x1A, 0x01, 0x6E, 0x0E, 0x00, 0x07, 0x00, 0x29, 0x00, 0xF3, 0x00, 0x00, 0x01, 0x00, 0x01, 0x98, 0x00, 0x98, 0x00, 0x09, 0x00, 0x09, 0x00, 0x90, 0x00, 0x90, 0x00, 0x0E, 0x01, 0x0E, 0x01,
        0x0F, 0x01, 0x6F, 0x20, 0x00, 0x98, 0x00, 0x00, 0x01, 0x00, 0x01, 0xB5, 0x00, 0xB5, 0x00, 0xCA, 0x00, 0xCA, 0x00, 0x10, 0x00, 0x10, 0x00, 0x78, 0x00, 0x78, 0x00, 0x7F, 0x00, 0x7F, 0x00, 0x26,
        0x00, 0x26, 0x00, 0x47, 0x00, 0x47, 0x00, 0x46, 0x00, 0x46, 0x00, 0x45, 0x00, 0x45, 0x00, 0x9C, 0x00, 0x9C, 0x00, 0x22, 0x00, 0x22, 0x00, 0xB0, 0x00, 0xB0, 0x00, 0x90, 0x00, 0x90, 0x00, 0x09,
        0x00, 0x09, 0x00, 0x98, 0x00, 0x70, 0x1C, 0x00, 0x07, 0x00, 0x6E, 0x00, 0x61, 0x00, 0x1E, 0x00, 0x1E, 0x00, 0x2D, 0x00, 0x2D, 0x00, 0x01, 0x01, 0x01, 0x01, 0x4C, 0x00, 0x4C, 0x00, 0xA5, 0x00,
        0xA5, 0x00, 0x02, 0x01, 0x02, 0x01, 0x36, 0x00, 0x36, 0x00, 0x03, 0x01, 0x03, 0x01, 0x04, 0x01, 0x04, 0x01, 0x9B, 0x00, 0x9B, 0x00, 0x48, 0x00, 0x48, 0x00, 0x05, 0x00, 0x05, 0x00, 0x27, 0x00,
        0x71, 0x1C, 0x00, 0xAC, 0x00, 0x1B, 0x01, 0xB0, 0x00, 0x90, 0x00, 0x90, 0x00, 0x09, 0x00, 0x09, 0x00, 0x98, 0x00, 0x98, 0x00, 0x00, 0x01, 0x00, 0x01, 0xB5, 0x00, 0xB5, 0x00, 0xCA, 0x00, 0xCA,
        0x00, 0x10, 0x00, 0x10, 0x00, 0x78, 0x00, 0x78, 0x00, 0x7F, 0x00, 0x7F, 0x00, 0x26, 0x00, 0x26, 0x00, 0x47, 0x00, 0x47, 0x00, 0x46, 0x00, 0x46, 0x00, 0x45, 0x00, 0x72, 0x0A, 0x00, 0x07, 0x00,
        0x29, 0x00, 0xCA, 0x00, 0xB5, 0x00, 0xB5, 0x00, 0x00, 0x01, 0x00, 0x01, 0x98, 0x00, 0x98, 0x00, 0x09, 0x00, 0x73, 0x20, 0x00, 0xA5, 0x00, 0x90, 0x00, 0x90, 0x00, 0x01, 0x01, 0x01, 0x01, 0x2D,
        0x00, 0x2D, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x61, 0x00, 0x61, 0x00, 0xA9, 0x00, 0xA9, 0x00, 0x1C, 0x01, 0x1C, 0x01, 0x1D, 0x01, 0x1D, 0x01, 0x72, 0x00, 0x72, 0x00, 0x37, 0x00, 0x37, 0x00, 0x03,
        0x01, 0x03, 0x01, 0x46, 0x00, 0x46, 0x00, 0x9B, 0x00, 0x9B, 0x00, 0x48, 0x00, 0x48, 0x00, 0x1E, 0x01, 0x1E, 0x01, 0x27, 0x00, 0x74, 0x0A, 0x00, 0x30, 0x00, 0x1F, 0x01, 0x1F, 0x01, 0xE0, 0x00,
        0xE0, 0x00, 0x80, 0x00, 0x80, 0x00, 0x47, 0x00, 0x07, 0x00, 0x01, 0x01, 0x75, 0x0E, 0x00, 0x07, 0x00, 0x49, 0x00, 0x49, 0x00, 0x1E, 0x01, 0x1E, 0x01, 0x07, 0x01, 0x07, 0x01, 0x80, 0x00, 0x80,
        0x00, 0xE9, 0x00, 0xE9, 0x00, 0x37, 0x00, 0x20, 0x01, 0x0F, 0x01, 0x76, 0x04, 0x00, 0x07, 0x00, 0x80, 0x00, 0xAC, 0x00, 0x80, 0x00, 0x77, 0x08, 0x00, 0x07, 0x00, 0x48, 0x00, 0x09, 0x00, 0x48,
        0x00, 0x09, 0x00, 0xEC, 0x00, 0x17, 0x01, 0xEC, 0x00, 0x78, 0x04, 0x00, 0x07, 0x00, 0x0F, 0x01, 0x20, 0x01, 0x29, 0x00, 0x79, 0x0C, 0x00, 0x1D, 0x00, 0x9B, 0x00, 0xAF, 0x00, 0x9B, 0x00, 0x9B,
        0x00, 0x13, 0x00, 0x13, 0x00, 0x0D, 0x01, 0x0D, 0x01, 0x0B, 0x00, 0x0B, 0x00, 0x6E, 0x00, 0x7A, 0x06, 0x00, 0x20, 0x01, 0x29, 0x00, 0x07, 0x00, 0x20, 0x01, 0x29, 0x00, 0x0F, 0x01, 0x7B, 0x34,
        0x00, 0x12, 0x00, 0x21, 0x01, 0x21, 0x01, 0x66, 0x00, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x59, 0x00, 0x59, 0x00, 0x22, 0x01, 0x22, 0x01, 0x23, 0x01, 0x23, 0x01, 0x2D, 0x00, 0x2D, 0x00, 0x1F,
        0x00, 0x1F, 0x00, 0x24, 0x01, 0x21, 0x01, 0x25, 0x01, 0x25, 0x01, 0x1A, 0x00, 0x1A, 0x00, 0x26, 0x01, 0x26, 0x01, 0x27, 0x01, 0x27, 0x01, 0x6F, 0x00, 0x6F, 0x00, 0x28, 0x01, 0x28, 0x01, 0x29,
        0x01, 0x29, 0x01, 0x75, 0x00, 0x75, 0x00, 0x2A, 0x01, 0x2A, 0x01, 0x2B, 0x01, 0x2B, 0x01, 0x70, 0x00, 0x70, 0x00, 0xE0, 0x00, 0xE0, 0x00, 0x07, 0x01, 0x07, 0x01, 0x2C, 0x01, 0x2C, 0x01, 0x2D,
        0x01, 0x2D, 0x01, 0x0D, 0x01, 0x2E, 0x01, 0x2F, 0x01, 0x7C, 0x02, 0x00, 0x65, 0x00, 0x6E, 0x00, 0x7D, 0x34, 0x00, 0x65, 0x00, 0x30, 0x01, 0x30, 0x01, 0x31, 0x01, 0x31, 0x01, 0xBE, 0x00, 0xBE,
        0x00, 0x32, 0x01, 0x32, 0x01, 0x06, 0x01, 0x06, 0x01, 0xFD, 0x00, 0xFD, 0x00, 0x1D, 0x00, 0x1D, 0x00, 0x0E, 0x00, 0x0E, 0x00, 0x33, 0x01, 0x30, 0x01, 0x34, 0x01, 0x34, 0x01, 0x60, 0x00, 0x60,
        0x00, 0x5B, 0x00, 0x5B, 0x00, 0x7D, 0x00, 0x7D, 0x00, 0x5D, 0x00, 0x5D, 0x00, 0xDE, 0x00, 0xDE, 0x00, 0xB5, 0x00, 0xB5, 0x00, 0x35, 0x01, 0x35, 0x01, 0x01, 0x00, 0x01, 0x00, 0xEA, 0x00, 0xEA,
        0x00, 0x27, 0x00, 0x27, 0x00, 0x1E, 0x01, 0x1E, 0x01, 0x36, 0x01, 0x36, 0x01, 0x63, 0x00, 0x63, 0x00, 0x37, 0x01, 0x37, 0x01, 0x38, 0x01, 0x1C, 0x01, 0xCB, 0x00, 0x7E, 0x28, 0x00, 0x10, 0x00,
        0xCA, 0x00, 0xCA, 0x00, 0xB5, 0x00, 0xB5, 0x00, 0x9F, 0x00, 0x9F, 0x00, 0xEB, 0x00, 0xEB, 0x00, 0x69, 0x00, 0x69, 0x00, 0x39, 0x01, 0x39, 0x01, 0x9D, 0x00, 0x9D, 0x00, 0x95, 0x00, 0x95, 0x00,
        0x3A, 0x01, 0x3A, 0x01, 0x3B, 0x01, 0xCA, 0x00, 0xA0, 0x00, 0xA0, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0x3C, 0x01, 0x3C, 0x01, 0x3D, 0x01, 0x3D, 0x01, 0x3E, 0x01, 0x3E, 0x01, 0x9C, 0x00, 0x9C, 0x00,
        0x3F, 0x01, 0x3F, 0x01, 0x40, 0x01, 0x40, 0x01, 0x3B, 0x01, 0x3B, 0x01, 0xBB, 0x00
    };


    static inline const unsigned char*  getUShort(const unsigned char* data, unsigned short& v)
    {
        const unsigned short* src = (const unsigned short*)data;
        v = src[0];
        data += sizeof(unsigned short);
        return data;
    }


    static inline const unsigned char*  getUint(const unsigned char* data, uint32& v)
    {
        const uint32* src = (const uint32*)data;
        v = src[0];
        data += sizeof(uint32);
        return data;
    }


    RenderUtil::FontChar::FontChar()
    {
        mIndexCount = 0;
        mIndices    = 0;
        mX1 = mY1 = mX2 = mY2 = 0;
    }


    const unsigned char* RenderUtil::FontChar::Init(const unsigned char* data, const float* vertices)
    {
        data        = getUShort(data, mIndexCount);
        mIndices    = (const unsigned short*)data;
        data        += mIndexCount * sizeof(unsigned short);

        for (uint32 i = 0; i < mIndexCount; ++i)
        {
            uint32          index   = mIndices[i];
            const float*    vertex  = &vertices[index * 2];
            //assert( _finite(vertex[0]) );
            //assert( _finite(vertex[1]) );

            if (i == 0)
            {
                mX1 = mX2 = vertex[0];
                mY1 = mY2 = vertex[1];
            }
            else
            {
                if (vertex[0] < mX1)
                {
                    mX1 = vertex[0];
                }
                if (vertex[1] < mY1)
                {
                    mY1 = vertex[1];
                }
                if (vertex[0] > mX2)
                {
                    mX2 = vertex[0];
                }
                if (vertex[1] > mY2)
                {
                    mY2 = vertex[1];
                }
            }
        }

        //assert( _finite(mX1) );
        //assert( _finite(mX2) );
        //assert( _finite(mY1) );
        //assert( _finite(mY2) );

        return data;
    }


    void RenderUtil::FontChar::Render(const float* vertices, RenderUtil* renderUtil, float textScale, float& x, float& y, float posX, float posY, const MCore::RGBAColor& color)
    {
        if (mIndices)
        {
            const uint32    lineCount   = mIndexCount / 2;
            const float     spacing     = (mX2 - mX1) + 0.05f;

            AZ::Vector2 p1;
            AZ::Vector2 p2;
            for (uint32 i = 0; i < lineCount; ++i)
            {
                const float* v1 = &vertices[ mIndices[i * 2 + 0] * 2 ];
                const float* v2 = &vertices[ mIndices[i * 2 + 1] * 2 ];
                p1.SetX((v1[0] + x) * textScale + posX);
                p1.SetY((v1[1] + y) * textScale);
                p2.SetX((v2[0] + x) * textScale + posX);
                p2.SetY((v2[1] + y) * textScale);

                renderUtil->Render2DLine(p1.GetX(), -p1.GetY() + posY, p2.GetX(), -p2.GetY() + posY, color);
            }

            x += spacing;
        }
        else
        {
            x += 0.1f;
        }
    }


    RenderUtil::VectorFont::VectorFont(RenderUtil* renderUtil)
    {
        mRenderUtil = renderUtil;
        mVersion    = 0;
        mVcount     = 0;
        mCount      = 0;
        mVertices   = NULL;

        Init(gFontData);
    }


    RenderUtil::VectorFont::~VectorFont()
    {
        Release();
    }


    void RenderUtil::VectorFont::Release()
    {
        mVersion    = 0;
        mVcount     = 0;
        mCount      = 0;
        mVertices   = NULL;
    }


    void RenderUtil::VectorFont::Init(const unsigned char* fontData)
    {
        Release();
        if (fontData[0] == 'F' && fontData[1] == 'O' && fontData[2] == 'N' && fontData[3] == 'T')
        {
            fontData += 4;
            fontData = getUint(fontData, mVersion);

            if (mVersion == FONT_VERSION)
            {
                fontData = getUint(fontData, mVcount);
                fontData = getUint(fontData, mCount);
                fontData = getUint(fontData, mIcount);

                uint32 vsize    = sizeof(float) * mVcount * 2;
                mVertices       = (float*)fontData;
                fontData        += vsize;

                for (uint32 i = 0; i < mCount; ++i)
                {
                    unsigned char c = *fontData++;
                    fontData = mCharacters[c].Init(fontData, mVertices);
                }
            }
        }
    }


    float RenderUtil::VectorFont::CalculateTextWidth(const char* text)
    {
        float textWidth = 0.0f;

        while (*text)
        {
            char codeUnit = *text++;
            //if (codeUnit <= 255)
            textWidth += mCharacters[(int)codeUnit].GetWidth();
            //else
            //  textWidth += mCharacters['?'].GetWidth();
        }

        return textWidth;
    }


    void RenderUtil::VectorFont::Render(float posX, float posY, float textScale, bool centered, const char* text, const MCore::RGBAColor& color)
    {
        float   x           = 0;
        float   y           = 0;
        float   fontScale   = textScale * 4.0f;// scale the text scale so that we are pixel perfect

        if (centered)
        {
            x = -CalculateTextWidth(text) * 0.5f;
        }

        while (*text)
        {
            const char c = *text++;
            //      if (c <= 255)
            mCharacters[(int)c].Render(mVertices, mRenderUtil, fontScale, x, y, posX, posY, color);
            //      else
            //          mCharacters['?'].Render( mVertices, mRenderUtil, fontScale, x, y, posX, posY, color );
        }
    }
} // namespace MCommon
