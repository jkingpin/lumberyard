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

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include "BlendTreeVector2DecomposeNode.h"


namespace EMotionFX
{
    AZ_CLASS_ALLOCATOR_IMPL(BlendTreeVector2DecomposeNode, AnimGraphAllocator, 0)


    BlendTreeVector2DecomposeNode::BlendTreeVector2DecomposeNode()
        : AnimGraphNode()
    {
        // setup the input ports
        InitInputPorts(1);
        SetupInputPort("Vector", INPUTPORT_VECTOR, MCore::AttributeVector2::TYPE_ID, PORTID_INPUT_VECTOR);

        // setup the output ports
        InitOutputPorts(2);
        SetupOutputPort("x", OUTPUTPORT_X, MCore::AttributeFloat::TYPE_ID, PORTID_OUTPUT_X);
        SetupOutputPort("y", OUTPUTPORT_Y, MCore::AttributeFloat::TYPE_ID, PORTID_OUTPUT_Y);
    }

        
    BlendTreeVector2DecomposeNode::~BlendTreeVector2DecomposeNode()
    {
    }


    bool BlendTreeVector2DecomposeNode::InitAfterLoading(AnimGraph* animGraph)
    {
        if (!AnimGraphNode::InitAfterLoading(animGraph))
        {
            return false;
        }

        InitInternalAttributesForAllInstances();

        Reinit();
        return true;
    }


    // get the palette name
    const char* BlendTreeVector2DecomposeNode::GetPaletteName() const
    {
        return "Vector2 Decompose";
    }


    // get the category
    AnimGraphObject::ECategory BlendTreeVector2DecomposeNode::GetPaletteCategory() const
    {
        return AnimGraphObject::CATEGORY_MATH;
    }


    // the update function
    void BlendTreeVector2DecomposeNode::Update(AnimGraphInstance* animGraphInstance, float timePassedInSeconds)
    {
        // update all inputs
        UpdateAllIncomingNodes(animGraphInstance, timePassedInSeconds);

        // if there are no incoming connections, there is nothing to do
        if (mConnections.size() == 0)
        {
            return;
        }

        AZ::Vector2 value = GetInputVector2(animGraphInstance, INPUTPORT_VECTOR)->GetValue();
        GetOutputFloat(animGraphInstance, OUTPUTPORT_X)->SetValue(value.GetX());
        GetOutputFloat(animGraphInstance, OUTPUTPORT_Y)->SetValue(value.GetY());
    }


    void BlendTreeVector2DecomposeNode::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (!serializeContext)
        {
            return;
        }

        serializeContext->Class<BlendTreeVector2DecomposeNode, AnimGraphNode>()
            ->Version(1);


        AZ::EditContext* editContext = serializeContext->GetEditContext();
        if (!editContext)
        {
            return;
        }

        editContext->Class<BlendTreeVector2DecomposeNode>("Vector2 Decompose", "Vector2 decompose attributes")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::AutoExpand, "")
            ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
            ;
    }
} // namespace EMotionFX