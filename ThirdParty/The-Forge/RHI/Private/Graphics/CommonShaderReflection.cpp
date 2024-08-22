/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "Nothings/stb_ds.h"

#include "ILog.h"
#include "Interfaces/IGraphics.h"

#include "IMemory.h"

// This file contains shader reflection code that is the same for all platforms.
// We know it's the same for all platforms since it only interacts with the
//  platform abstractions we created.

#define RESOURCE_NAME_CHECK
static bool ShaderResourceCmp(ShaderResource* a, ShaderResource* b)
{
    bool isSame = true;

    isSame = isSame && (a->type == b->type);
    isSame = isSame && (a->set == b->set);
    isSame = isSame && (a->reg == b->reg);

#ifdef METAL
    isSame = isSame && (a->mArgumentDescriptor.mArgumentIndex == b->mArgumentDescriptor.mArgumentIndex);
#endif

#ifdef RESOURCE_NAME_CHECK
    // we may not need this, the rest is enough but if we want to be super sure we can do this check
    isSame = isSame && (a->name_size == b->name_size);
    // early exit before string cmp
    if (isSame == false)
        return isSame;

    isSame = (strcmp(a->name, b->name) == 0);
#endif
    return isSame;
}

static bool ShaderVariableCmp(ShaderVariable* a, ShaderVariable* b)
{
    bool isSame = true;

    isSame = isSame && (a->offset == b->offset);
    isSame = isSame && (a->size == b->size);
    isSame = isSame && (a->name_size == b->name_size);

    // early exit before string cmp
    if (isSame == false)
        return isSame;

    isSame = (strcmp(a->name, b->name) == 0);

    return isSame;
}

void destroyShaderReflection(ShaderReflection* pReflection)
{
    if (pReflection == NULL)
        return;

    tf_free(pReflection->pNamePool);
    tf_free(pReflection->pVertexInputs);
    tf_free(pReflection->pShaderResources);
    tf_free(pReflection->pVariables);
}

void createPipelineReflection(ShaderReflection* pReflection, uint32_t stageCount, PipelineReflection* pOutReflection)
{
    // Parameter checks
    if (pReflection == NULL)
    {
        LOGF(LogLevel::eERROR, "Parameter 'pReflection' is NULL.");
        return;
    }
    if (stageCount == 0)
    {
        LOGF(LogLevel::eERROR, "Parameter 'stageCount' is 0.");
        return;
    }
    if (pOutReflection == NULL)
    {
        LOGF(LogLevel::eERROR, "Parameter 'pOutShaderReflection' is NULL.");
        return;
    }

    // Sanity check to make sure we don't have repeated stages.
    ShaderStage combinedShaderStages = (ShaderStage)0;
    for (uint32_t i = 0; i < stageCount; ++i)
    {
        if ((combinedShaderStages & pReflection[i].mShaderStage) != 0)
        {
            LOGF(LogLevel::eERROR, "Duplicate shader stage was detected in shader reflection array.");
            return;
        }
        combinedShaderStages = (ShaderStage)(combinedShaderStages | pReflection[i].mShaderStage);
    }

    // Combine all shaders
    // this will have a large amount of looping
    // 1. count number of resources
    uint32_t        vertexStageIndex = ~0u;
    uint32_t        hullStageIndex = ~0u;
    uint32_t        domainStageIndex = ~0u;
    uint32_t        geometryStageIndex = ~0u;
    uint32_t        pixelStageIndex = ~0u;
    ShaderResource* pResources = NULL;
    ShaderVariable* pVariables = NULL;

    ShaderResource** pUniqueResources = NULL;
    ShaderStage*     pShaderUsage = NULL;
    ShaderVariable** pUniqueVariable = NULL;
    ShaderResource** pUniqueVariableParent = NULL;
    for (uint32_t i = 0; i < stageCount; ++i)
    {
        ShaderReflection* pSrcRef = pReflection + i;
        pOutReflection->mStageReflections[i] = *pSrcRef;

        if (pSrcRef->mShaderStage == SHADER_STAGE_VERT)
        {
            vertexStageIndex = i;
        }
#if !defined(METAL)
        else if (pSrcRef->mShaderStage == SHADER_STAGE_HULL)
        {
            hullStageIndex = i;
        }
        else if (pSrcRef->mShaderStage == SHADER_STAGE_DOMN)
        {
            domainStageIndex = i;
        }
        else if (pSrcRef->mShaderStage == SHADER_STAGE_GEOM)
        {
            geometryStageIndex = i;
        }
#endif
        else if (pSrcRef->mShaderStage == SHADER_STAGE_FRAG)
        {
            pixelStageIndex = i;
        }

        // Loop through all shader resources
        for (uint32_t j = 0; j < pSrcRef->mShaderResourceCount; ++j)
        {
            bool unique = true;

            // Go through all already added shader resources to see if this shader
            //  resource was already added from a different shader stage. If we find a
            //  duplicate shader resource, we add the shader stage to the shader stage
            //  mask of that resource instead.
            for (uint32_t k = 0; k < (uint32_t)arrlen(pUniqueResources); ++k)
            {
                unique = !ShaderResourceCmp(&pSrcRef->pShaderResources[j], pUniqueResources[k]);
                if (unique == false)
                {
                    // update shader usage
                    // NOT SURE
                    // shaderUsage[k] = (ShaderStage)(shaderUsage[k] & pSrcRef->pShaderResources[j].used_stages);
                    ASSERT(pShaderUsage);
                    ASSERT(arrlen(pShaderUsage) > k);
                    if (arrlen(pShaderUsage) > k)
                    {
                        pShaderUsage[k] |= pSrcRef->pShaderResources[j].used_stages;
                        break;
                    }
                }
            }

            // If it's unique, we add it to the list of shader resourceas
            if (unique == true)
            {
                arrpush(pShaderUsage, pSrcRef->pShaderResources[j].used_stages);
                arrpush(pUniqueResources, &pSrcRef->pShaderResources[j]);
            }
        }

        // Loop through all shader variables (constant/uniform buffer members)
        for (uint32_t j = 0; j < pSrcRef->mVariableCount; ++j)
        {
            bool unique = true;
            // Go through all already added shader variables to see if this shader
            //  variable was already added from a different shader stage. If we find a
            //  duplicate shader variables, we don't add it.
            for (uint32_t k = 0; k < (uint32_t)arrlen(pUniqueVariable); ++k)
            {
                unique = !ShaderVariableCmp(&pSrcRef->pVariables[j], pUniqueVariable[k]);
                if (unique == false)
                    break;
            }

            // If it's unique we add it to the list of shader variables
            if (unique)
            {
                arrpush(pUniqueVariableParent, &pSrcRef->pShaderResources[pSrcRef->pVariables[j].parent_index]);
                arrpush(pUniqueVariable, &pSrcRef->pVariables[j]);
            }
        }
    }

    // Copy over the shader resources in a dynamic array of the correct size
    if (arrlen(pUniqueResources))
    {
        pResources = (ShaderResource*)tf_calloc(arrlen(pUniqueResources), sizeof(ShaderResource));

        for (uint32_t i = 0; i < (uint32_t)arrlen(pUniqueResources); ++i)
        {
            pResources[i] = *pUniqueResources[i];
            pResources[i].used_stages = pShaderUsage[i];
        }
    }

    // Copy over the shader variables in a dynamic array of the correct size
    if (arrlen(pUniqueVariable))
    {
        pVariables = (ShaderVariable*)tf_malloc(sizeof(ShaderVariable) * arrlen(pUniqueVariable));

        for (uint32_t i = 0; i < (uint32_t)arrlen(pUniqueVariable); ++i)
        {
            pVariables[i] = *pUniqueVariable[i];
            ShaderResource* parentResource = pUniqueVariableParent[i];
            // look for parent
            for (uint32_t j = 0; j < (uint32_t)arrlen(pUniqueResources); ++j)
            {
                if (ShaderResourceCmp(&pResources[j], parentResource)) //-V522
                {
                    pVariables[i].parent_index = j;
                    break;
                }
            }
        }
    }

    // all refection structs should be built now
    pOutReflection->mShaderStages = combinedShaderStages;

    pOutReflection->mStageReflectionCount = stageCount;

    pOutReflection->mVertexStageIndex = vertexStageIndex;
    pOutReflection->mHullStageIndex = hullStageIndex;
    pOutReflection->mDomainStageIndex = domainStageIndex;
    pOutReflection->mGeometryStageIndex = geometryStageIndex;
    pOutReflection->mPixelStageIndex = pixelStageIndex;

    pOutReflection->pShaderResources = pResources;
    pOutReflection->mShaderResourceCount = (uint32_t)arrlen(pUniqueResources);

    pOutReflection->pVariables = pVariables;
    pOutReflection->mVariableCount = (uint32_t)arrlen(pUniqueVariable);

    arrfree(pUniqueResources);
    arrfree(pShaderUsage);
    arrfree(pUniqueVariable);
    arrfree(pUniqueVariableParent);
}

void destroyPipelineReflection(PipelineReflection* pReflection)
{
    if (pReflection == NULL)
        return;

    for (uint32_t i = 0; i < pReflection->mStageReflectionCount; ++i)
        destroyShaderReflection(&pReflection->mStageReflections[i]);

    tf_free(pReflection->pShaderResources);
    tf_free(pReflection->pVariables);
}
