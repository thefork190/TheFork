#include <array>
#include <vector>
#include <random>
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <SDL3/SDL_timer.h>
#include <IResourceLoader.h>
#include "ILog.h"
#include "Low/Engine.h"
#include "Low/Inputs.h"
#include "Low/RHI.h"
#include "Low/Window.h"
#include "Medium/FontRendering.h"
#include "FlappyClone.h"

namespace FlappyClone
{
    // Game related constants (TODO: drive these via debug UI)
    float const             OBSTACLE_WIDTH = 0.15f;                                 // Width of an obstacle
    float const             OBSTACLE_GAME_START_X_OFFSET = 1.f;                     // X offset of first obstacle when starting a new game
    float const             OBSTACLE_GAP_HEIGHT = OBSTACLE_WIDTH * 1.75f;           // Obstacle gap (in between top and bottom "pipes")
    float const             DIST_BETWEEN_OBSTACLES = 0.5f;                          // Dist to next obstacle
    unsigned int const      TOTAL_OBSTACLES = 20u;                                  // Should have enough to cover ultra wide resolution
    float const             PLAYER_SIZE = OBSTACLE_GAP_HEIGHT * 0.35f;              // Player size (relative to gap height)
    float const             SCROLL_SPEED = 0.33f;                                   // How fast obstacles translate towards the player
    float const             GRAVITY = -2.33f;
    float const             IMPULSE_FORCE = 0.75f;                                  // Impulse force to apply upwards to simulate flapping
    float const             PLAYER_START_COLOR[4] = { 0.45f, 0.45f, 0.45f, 1.f };   // Player default color when starting a game
    float const             PLAYER_X_OFFSET = 0.1f;                                 // X offset that the player is positioned at

    // Rendering constants
#define MAX_QUADS 64 // this needs to match the same define in DrawQuad.h.fsl
    size_t const            TOTALS_QUADS_TO_DRAW = (TOTAL_OBSTACLES * 2) + 1;
    size_t const            UNIFORMS_PLAYER_INDEX = (TOTAL_OBSTACLES * 2);

    // COMPONENT /////////////
    struct RenderPassData
    {
        Shader* pTriShader = nullptr;
        RootSignature* pRootSignature = nullptr;
        DescriptorSet* pDescriptorSetUniforms = nullptr;
        Pipeline* pPipeline = nullptr;
        VertexLayout vertexLayout = {};
        Buffer* pVertexBuffer = nullptr;
        Buffer* pIndexBuffer = nullptr;
        std::vector<Buffer*> uniformsBuffers;

        // Uniforms data
        struct UniformsData
        {
            glm::mat4 proj = {};
            glm::mat4 mv[MAX_QUADS] = {};
            glm::vec4 color[MAX_QUADS] = {};
        } uniformsData;
        size_t curUniformIndex = 0;

        void Reset()
        {
            pTriShader = nullptr;
            pRootSignature = nullptr;
            pDescriptorSetUniforms = nullptr;
            pPipeline = nullptr;
            vertexLayout = {};
            pVertexBuffer = nullptr;
            pIndexBuffer = nullptr;
            uniformsBuffers.clear();
            uniformsData = {};
        }

        // Caching resolution which is useful to have (eg. positioning score text when updating the player entity)
        unsigned int resX = 0;
        unsigned int resY = 0;
    };

    struct Position
    {
        float x = 0.f;
        float y = 0.f;
        float z = 0.1f;
    };

    struct Scale
    {
        float x = 1.f;
        float y = 1.f;
    };

    struct Color
    {
        float r = 1.f;
        float g = 1.f;
        float b = 1.f;
        float a = 1.f;
    };

    struct Obstacle {};

    struct Player 
    {
        float distanceTravelled = 0.f; // used to calculate score
    };

    struct Velocity
    {
        float x = 0.f;
        float y = 0.f;
        float z = 0.f;
    };

    struct GameContext
    {
        flecs::query<Position const, Scale const> obstacleChildrenQuery; // used for collision detection with player

        enum eSTATE
        {
            START,
            IN_PLAY,
            GAME_OVER,
            RESET_WORLD,
        } state = RESET_WORLD;

        unsigned int obstaclesCreated = 0; // used to create unique names for obstacles
    };
    //////////////////////////

    // Rendering /////////////
    static void AddShaders(Renderer* const pRenderer, RenderPassData& passDataInOut)
    {
        ShaderLoadDesc basicShader = {};
        basicShader.mStages[0].pFileName = "DrawQuad.vert";
        basicShader.mStages[1].pFileName = "DrawQuad.frag";
        addShader(pRenderer, &basicShader, &passDataInOut.pTriShader);
    }

    static void RemoveShaders(Renderer* const pRenderer, RenderPassData& passDataInOut)
    {
        removeShader(pRenderer, passDataInOut.pTriShader);
    }

    static void AddRootSignature(Renderer* const pRenderer, RenderPassData& passDataInOut)
    {
        Shader* shaders[1];
        uint32_t shadersCount = 0;
        shaders[shadersCount++] = passDataInOut.pTriShader;

        RootSignatureDesc rootDesc = {};
        rootDesc.mShaderCount = shadersCount;
        rootDesc.ppShaders = shaders;
        addRootSignature(pRenderer, &rootDesc, &passDataInOut.pRootSignature);
    }

    static void RemoveRootSignature(Renderer* const pRenderer, RenderPassData& passDataInOut)
    {
        removeRootSignature(pRenderer, passDataInOut.pRootSignature);
    }

    static void AddDescriptorSet(RHI::RHI const* pRHI, RenderPassData& passDataInOut)
    {
        DescriptorSetDesc desc = { passDataInOut.pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, pRHI->dataBufferCount };
        addDescriptorSet(pRHI->pRenderer, &desc, &passDataInOut.pDescriptorSetUniforms);
    }

    static void RemoveDescriptorSet(Renderer* const pRenderer, RenderPassData& passDataInOut)
    {
        removeDescriptorSet(pRenderer, passDataInOut.pDescriptorSetUniforms);
    }

    static void AddPipeline(
        RHI::RHI const* pRHI,
        Window::SDLWindow const* pWindow,
        RenderPassData& passDataInOut)
    {
        RasterizerStateDesc rasterizerStateDesc = {};
        rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

        DepthStateDesc depthStateDesc = {};

        PipelineDesc desc = {};
        desc.mType = PIPELINE_TYPE_GRAPHICS;
        GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
        pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineSettings.mRenderTargetCount = 1;
        pipelineSettings.pDepthState = &depthStateDesc;
        pipelineSettings.pColorFormats = &pWindow->pSwapChain->ppRenderTargets[0]->mFormat;
        pipelineSettings.mSampleCount = pWindow->pSwapChain->ppRenderTargets[0]->mSampleCount;
        pipelineSettings.mSampleQuality = pWindow->pSwapChain->ppRenderTargets[0]->mSampleQuality;
        pipelineSettings.pRootSignature = passDataInOut.pRootSignature;
        pipelineSettings.pShaderProgram = passDataInOut.pTriShader;
        pipelineSettings.pVertexLayout = &passDataInOut.vertexLayout;
        pipelineSettings.pRasterizerState = &rasterizerStateDesc;
        addPipeline(pRHI->pRenderer, &desc, &passDataInOut.pPipeline);
    }

    static void RemovePipeline(Renderer* const pRenderer, RenderPassData& passDataInOut)
    {
        removePipeline(pRenderer, passDataInOut.pPipeline);
    }
    //////////////////////////

    // Game Utils ////////////
    static void ResetPlayer(Position& position, Scale& scale, Color& color, Velocity& velocity, float const xOffset)
    {
        scale = { PLAYER_SIZE, PLAYER_SIZE };
        color = { PLAYER_START_COLOR[0], PLAYER_START_COLOR[1], PLAYER_START_COLOR[2], PLAYER_START_COLOR[3] };
        position.x = xOffset + PLAYER_SIZE / 2.f;
        position.y = 0.5f;
        position.z = 0.1f;
        velocity = {};
    }

    static void ResetObstacle(
        std::array<Position, 2>& position,
        std::array<Scale, 2>& scale,
        std::array<Color, 2>& color,
        float const xOffset0,
        float const xOffset1)
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        float const minVerticalOffset = OBSTACLE_WIDTH + OBSTACLE_WIDTH * 0.5f;
        std::uniform_real_distribution<float> dis(minVerticalOffset, 1.f - minVerticalOffset);
        float const gapPosY = dis(gen);

        for (unsigned int i = 0; i < 2; ++i)
        {
            color[i] = {0.f, 0.0, 1.f, 1.f};

            scale[i].x = OBSTACLE_WIDTH;
            scale[i].y = OBSTACLE_WIDTH;

            if (i == 0)
            {
                float const topPos = gapPosY + OBSTACLE_GAP_HEIGHT / 2.f;
                scale[i].y = 1.f - topPos;
            }
            else
            {
                float const bottomPos = gapPosY - OBSTACLE_GAP_HEIGHT / 2.f;
                scale[i].y = bottomPos;
            }

            position[i].x = xOffset0 + xOffset1;
            position[i].y = (i == 0) ? 1.f - scale[i].y / 2.f : scale[i].y / 2.f;
            position[i].z = 0.1f;
        }
    }
    //////////////////////////

    module::module(flecs::world& ecs)
    {
        ecs.import<RHI::module>();
        ecs.import<Window::module>();
        ecs.import<Engine::module>();

        ecs.module<module>();

        ecs.component<RenderPassData>();
        ecs.component<Scale>();
        ecs.component<Position>();
        ecs.component<Color>();
        ecs.component<FontRendering::FontText>();
        
        RHI::RHI const* pRHI = ecs.has<RHI::RHI>() ? ecs.get<RHI::RHI>() : nullptr;
        ASSERTMSG(pRHI, "RHI singleton doesn't exist.");

        RenderPassData renderPassData = {};
        
        AddShaders(pRHI->pRenderer, renderPassData);
        AddRootSignature(pRHI->pRenderer, renderPassData);
        AddDescriptorSet(pRHI, renderPassData);

        renderPassData.uniformsBuffers.resize(pRHI->dataBufferCount);
        BufferLoadDesc ubDesc = {};
        ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        ubDesc.pData = nullptr;
        for (uint32_t i = 0; i < pRHI->dataBufferCount; ++i)
        {
            ubDesc.mDesc.pName = "FlappyClone_UniformBuffer";
            ubDesc.mDesc.mSize = sizeof(RenderPassData::UniformsData);
            ubDesc.ppBuffer = &renderPassData.uniformsBuffers[i];
            addResource(&ubDesc, nullptr);
                        
            DescriptorData uParams[1] = {};
            uParams[0].pName = "UniformBlock";
            uParams[0].ppBuffers = &renderPassData.uniformsBuffers[i];
            updateDescriptorSet(pRHI->pRenderer, i, renderPassData.pDescriptorSetUniforms, 1, uParams);
        }

        renderPassData.vertexLayout.mBindingCount = 1;
        renderPassData.vertexLayout.mBindings[0].mStride = 12; // xyz pos
        renderPassData.vertexLayout.mAttribCount = 1;
        renderPassData.vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        renderPassData.vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
        renderPassData.vertexLayout.mAttribs[0].mBinding = 0;
        renderPassData.vertexLayout.mAttribs[0].mLocation = 0;
        renderPassData.vertexLayout.mAttribs[0].mOffset = 0;

        std::vector<glm::vec3> triPositions(4);
        triPositions[0] = { -0.5f, -0.5f , 0.f };
        triPositions[1] = { 0.5f, -0.5f , 0.f };
        triPositions[2] = { 0.5f, 0.5f , 0.f };
        triPositions[3] = { -0.5f, 0.5f , 0.f };

        BufferLoadDesc vbDesc = {};
        vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
        vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        vbDesc.mDesc.mSize = triPositions.size() * 12;
        vbDesc.pData = triPositions.data();
        vbDesc.ppBuffer = &renderPassData.pVertexBuffer;
        addResource(&vbDesc, nullptr);

        std::vector<uint16_t> triIndices(8);
        triIndices[0] = 0;
        triIndices[1] = 1;
        triIndices[2] = 2;
        triIndices[3] = 2;
        triIndices[4] = 3;
        triIndices[5] = 0;

        BufferLoadDesc ibDesc = {};
        ibDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
        ibDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        ibDesc.mDesc.mSize = triIndices.size() * sizeof(uint16_t);
        ibDesc.pData = triIndices.data();
        ibDesc.ppBuffer = &renderPassData.pIndexBuffer;
        addResource(&ibDesc, nullptr);

        flecs::query<Window::SDLWindow> windowQuery = ecs.query_builder<Window::SDLWindow>().build();
        windowQuery.each([pRHI, &renderPassData](flecs::iter& it, size_t i, Window::SDLWindow& window)
            {
                ASSERTMSG(i == 0, "Drawing to more than one window not implemented.");
                AddPipeline(pRHI, &window, renderPassData);

                // While we're at it, cap the min window size
                SDL_SetWindowMinimumSize(window.pWindow, 800, 600);

                // Cache res
                renderPassData.resX = window.pSwapChain->ppRenderTargets[0]->mWidth;
                renderPassData.resY = window.pSwapChain->ppRenderTargets[0]->mHeight;
            });


        waitForAllResourceLoads();

        ecs.set<RenderPassData>(renderPassData);

        // Create the player entity
        {
            auto player = ecs.entity((std::string("Player")).c_str());
            player.add<Player>();

            Color color = {};
            Scale scale = {};
            Position pos = {};
            Velocity vel = {};

            ResetPlayer(pos, scale, color, vel, PLAYER_X_OFFSET);

            player.set<Color>(color);
            player.set<Scale>(scale);
            player.set<Position>(pos);
            player.set<Velocity>(vel);

            // FontText to show current score
            FontRendering::FontText fontText {};
            player.set<FontRendering::FontText>(fontText);
        }

        // Create the game context
        GameContext gameContext = {};
        gameContext.obstacleChildrenQuery = ecs.query_builder<Position const, Scale const>().
            with<Obstacle>().up(flecs::ChildOf).
            cached().
            build();
        ecs.set<GameContext>(gameContext);

        // Following are all systems (note the decl' order is important for systems within the same flecs phase)

        // State Transitioning
        // - Handles game context state transitions
        // - Checks for ESC key press to exit app
        ecs.system("FlappyClone::StateTransitioning")
                .kind(flecs::PreUpdate)
                .run([](flecs::iter& it)
                {
                    Inputs::RawKeboardStates const* pKeyboard = it.world().has<Inputs::RawKeboardStates>() ? it.world().get<Inputs::RawKeboardStates>() : nullptr;
                    Engine::Context* pEngineContext = it.world().has<Engine::Context>() ? it.world().get_mut<Engine::Context>() : nullptr;
                    if (pKeyboard && pEngineContext)
                    {
                        // Exit if ESC is pressed
                        if (pKeyboard->WasPressed(SDLK_ESCAPE))
                        {
                            LOGF(eDEBUG, "ESC pressed, requesting to exit the app.");
                            pEngineContext->RequestExit();
                        }

                        // State transitions
                        GameContext* pGameCtx = it.world().has<GameContext>() ? it.world().get_mut<GameContext>() : nullptr;
                        if (pGameCtx)
                        {
                            if (pKeyboard->WasPressed(SDLK_SPACE))
                            {
                                if (GameContext::START == pGameCtx->state)
                                    pGameCtx->state = GameContext::IN_PLAY;

                                if (GameContext::GAME_OVER == pGameCtx->state)
                                    pGameCtx->state = GameContext::RESET_WORLD;
                            }

                            if (GameContext::RESET_WORLD == pGameCtx->state)
                            {
                                // Reset players
                                auto playerQuery = it.world().query_builder<Player, Position, Scale, Color, Velocity>();
                                playerQuery.each([](Player& player, Position& pos, Scale& scale, Color& color, Velocity& vel) 
                                    {
                                        player.distanceTravelled = 0.f;
                                        ResetPlayer(pos, scale, color, vel, PLAYER_X_OFFSET);
                                    });

                                // Delete all obstacle entities (we'll just recreate them)
                                flecs::query<Obstacle> obstacleQuery = it.world().query_builder<Obstacle>().build();
                                obstacleQuery.each([&](flecs::entity e, Obstacle) 
                                    {
                                        e.destruct();
                                    });

                                // Create obstacle entities
                                // An obstacle entity will have 2 children: a top and bottom "pipe".  In flappy bird, a bottom and top pipe are always on the same Y axis.                                
                                for (unsigned int i = 0; i < TOTAL_OBSTACLES; ++i)
                                {
                                    auto obstacleEnt = it.world().entity((std::string("FlappyClone::Obstacle") + std::to_string(pGameCtx->obstaclesCreated)).c_str());
                                    obstacleEnt.add<Obstacle>();

                                    std::array<Position, 2> positions = {};
                                    std::array<Scale, 2> scales = {};
                                    std::array<Color, 2> colors = {};

                                    flecs::entity child[2] =
                                    {
                                        it.world().entity((std::string("FlappyClone::Obstacle") + std::to_string(pGameCtx->obstaclesCreated) + "::TOP").c_str()),
                                        it.world().entity((std::string("FlappyClone::Obstacle") + std::to_string(pGameCtx->obstaclesCreated) + "::BOTTOM").c_str())
                                    };

                                    ResetObstacle(positions, scales, colors, OBSTACLE_GAME_START_X_OFFSET, (i * DIST_BETWEEN_OBSTACLES));

                                    for (unsigned int j = 0; j < 2; ++j)
                                    {
                                        child[j].set<Position>(positions[j]);
                                        child[j].set<Scale>(scales[j]);
                                        child[j].set<Color>(colors[j]);
                                        child[j].add(flecs::ChildOf, obstacleEnt);
                                    }

                                    pGameCtx->obstaclesCreated += 1;
                                }

                                pGameCtx->state = GameContext::START;
                            }
                        }
                    }
                }
            );


        // Update Obstacles:
        // - Scrolls obstacles
        // - Resets them in position (and randomizes gap) once they go past the left side of the screen
        // - Updates uniforms data so we can render them
        ecs.system<Position, Scale, Color>("FlappyClone::UpdateObstacles")
            .kind(flecs::OnUpdate)
            .with<Obstacle>().up(flecs::ChildOf)
            .each([](flecs::iter& it, size_t i, Position& position, Scale& scale, Color const& color)
                {
                    GameContext const* pGameCtx = it.world().has<GameContext>() ? it.world().get<GameContext>() : nullptr;

                    if (pGameCtx && GameContext::IN_PLAY == pGameCtx->state)
                    {
                        // Translate obstacle
                        position.x -= SCROLL_SPEED * it.delta_system_time();

                        // Has it gone out of view?  If so reset the position to the other end
                        if (position.x < -scale.x)
                        {
                            position.x += DIST_BETWEEN_OBSTACLES * TOTAL_OBSTACLES;
                        }
                    }

                    // Update rendering data
                    RenderPassData* pRPD = it.world().has<RenderPassData>() ? it.world().get_mut<RenderPassData>() : nullptr;
                    
                    if (pRPD)
                    {
                        RenderPassData::UniformsData& updatedData = pRPD->uniformsData;

                        glm::mat4 modelMat(1.f);
                        modelMat = glm::translate(modelMat, glm::vec3(position.x, position.y, position.z));
                        modelMat = glm::scale(modelMat, glm::vec3(scale.x, scale.y, 1.f));
                        updatedData.mv[pRPD->curUniformIndex] = modelMat;
                        updatedData.color[pRPD->curUniformIndex] = glm::vec4(color.r, color.g, color.b, color.a);

                        pRPD->curUniformIndex++;
                    }
                }
            );

        // Apply Gravity
        // - Simulates gravity on entities with a velocity and position component
        ecs.system<Velocity, Position>("FlappyClone::ApplyGravity")
            .kind(flecs::OnUpdate)
            .each([](flecs::iter& it, size_t i, Velocity& vel, Position& pos)
                {
                    GameContext const* pGameCtx = it.world().has<GameContext>() ? it.world().get<GameContext>() : nullptr;

                    if (pGameCtx && GameContext::IN_PLAY == pGameCtx->state)
                    {
                        vel.y += GRAVITY * it.delta_system_time();
                        pos.y += vel.y * it.delta_system_time();

                        if (pos.y < 0.f)
                        {
                            pos.y = 0.f;
                            vel.y = 0.f;
                        }
                    }
                }
            );

        // Update Player
        // - Updates uniforms data for rendering
        // - Handles player inputs 
        ecs.system<Player, Position, Scale, Color, Velocity, FontRendering::FontText>("FlappyClone::UpdatePlayer")
            .kind(flecs::OnUpdate)
            .each([](flecs::iter& it, size_t i, Player& player, Position& position, Scale& scale, Color const& color, Velocity& vel, FontRendering::FontText& fontText)
                {
                    ASSERTMSG(i == 0, "More than 1 player not supported.");
                    
                    // Update rendering data
                    RenderPassData* pRPD = it.world().has<RenderPassData>() ? it.world().get_mut<RenderPassData>() : nullptr;
                    
                    if (pRPD)
                    {
                        RenderPassData::UniformsData& updatedData = pRPD->uniformsData;

                        glm::mat4 modelMat(1.f);
                        modelMat = glm::translate(modelMat, glm::vec3(position.x, position.y, position.z));
                        modelMat = glm::scale(modelMat, glm::vec3(scale.x, scale.y, 1.f));
                        updatedData.mv[UNIFORMS_PLAYER_INDEX] = modelMat;
                        updatedData.color[UNIFORMS_PLAYER_INDEX] = glm::vec4(color.r, color.g, color.b, color.a);
                    }

                    // Exit if ESC is pressed
                    Inputs::RawKeboardStates const* pKeyboard = it.world().has<Inputs::RawKeboardStates>() ? it.world().get<Inputs::RawKeboardStates>() : nullptr;                   
                    GameContext const* pGameCtx = it.world().has<GameContext>() ? it.world().get<GameContext>() : nullptr;
                    if (pKeyboard)
                    {
                        if (pGameCtx &&
                            pKeyboard->WasPressed(SDLK_SPACE))
                        {                            
                            if (GameContext::IN_PLAY == pGameCtx->state)
                                vel.y = IMPULSE_FORCE;
                        }
                    }

                    // Update score and text
                    float score = 0;
                    if (pGameCtx && GameContext::IN_PLAY == pGameCtx->state)
                    {
                        player.distanceTravelled += SCROLL_SPEED * it.delta_system_time();
                    }
                    score = ((player.distanceTravelled - OBSTACLE_GAME_START_X_OFFSET + PLAYER_X_OFFSET + PLAYER_SIZE * 0.5f) / (DIST_BETWEEN_OBSTACLES)) + 1.f;
                    if (score < 0.f)
                        score = 0.f;

                    fontText.text = std::to_string(static_cast<unsigned int>(score));
                    fontText.fontSize = 85.f;

                    float textSize[2] = {};
                    FontRendering::MeasureText(it.world(), fontText, textSize[0], textSize[1]);

                    if (pRPD)
                    {
                        fontText.posX = (pRPD->resX * 0.5f) - (textSize[0] * 0.5f);
                        fontText.posY = (pRPD->resY * 0.15f) - (textSize[1] * 0.5f);
                    }
                }
            );

        // Validate Player
        // - Checks if player collided with obstacles
        ecs.system<Player, Position, Scale, Color>("FlappyClone::ValidatePlayer")
            .kind(flecs::OnValidate)
            .each([](flecs::iter& it, size_t i, Player, Position const& playerPos, Scale const& playerScale, Color& playerColor)
                {
                    ASSERTMSG(i == 0, "More than 1 player not supported.");

                    GameContext* pGameCtx = it.world().has<GameContext>() ? it.world().get_mut<GameContext>() : nullptr;

                    bool intersected = false;
                    
                    if (pGameCtx && GameContext::IN_PLAY == pGameCtx->state)
                    {
                        glm::vec2 const minPlayer = { playerPos.x - playerScale.x * 0.5f,  playerPos.y - playerScale.y * 0.5f };
                        glm::vec2 const maxPlayer = { playerPos.x + playerScale.x * 0.5f,  playerPos.y + playerScale.y * 0.5f };

                        pGameCtx->obstacleChildrenQuery.run([&intersected, &minPlayer, &maxPlayer](flecs::iter& it) {
                            while (it.next()) 
                            {
                                auto obsPositions = it.field<Position const>(0);
                                auto obsScales = it.field<Scale const>(1);

                                for (auto i : it)
                                {
                                    auto obsPos = obsPositions[i];
                                    auto obsScale = obsScales[i];

                                    glm::vec2 const minObs = { obsPos.x - obsScale.x * 0.5f,  obsPos.y - obsScale.y * 0.5f };
                                    glm::vec2 const maxObs = { obsPos.x + obsScale.x * 0.5f,  obsPos.y + obsScale.y * 0.5f };
                                    
                                    if (maxPlayer.x < minObs.x || minPlayer.x > maxObs.x)
                                        continue;
                                    
                                    if (maxPlayer.y < minObs.y || minPlayer.y > maxObs.y)
                                        continue;
                                     
                                    intersected = true;

                                    it.fini();
                                    return;
                                }
                            }
                        });

                        // Check if player went too far down and hit the ground
                        if (playerPos.y - playerScale.y * 0.5f < 0.f)
                            intersected = true;

                        if (intersected)
                        {
                            playerColor.r = 1.f;
                            playerColor.g = 0.f;
                            playerColor.b = 0.f;
                            playerColor.a = 1.f;

                            pGameCtx->state = GameContext::GAME_OVER;
                        }
                        else
                        {
                            playerColor.r = 0.f;
                            playerColor.g = 1.f;
                            playerColor.b = 0.f;
                            playerColor.a = 1.f;
                        }
                    }
                }
            );
        
        // Update Uniforms
        // - Updates gpu unif buffer
        ecs.system<Engine::Canvas, Window::SDLWindow>("FlappyClone::UpdateUniforms")
            .kind(flecs::PreStore)
            .each([](flecs::iter& it, size_t i, Engine::Canvas const& canvas, Window::SDLWindow const& sdlWin)
                {
                    ASSERTMSG(i == 0, "Drawing to more than one window not implemented.");

                    RHI::RHI const* pRHI = it.world().has<RHI::RHI>() ? it.world().get<RHI::RHI>() : nullptr;
                    RenderPassData* pRPD = it.world().has<RenderPassData>() ? it.world().get_mut<RenderPassData>() : nullptr;

                    // Rendering update
                    if (pRHI && pRPD)
                    {
                        float const aspect = canvas.width / static_cast<float>(canvas.height);
                        pRPD->uniformsData.proj = glm::orthoLH_ZO(0.f, aspect, 0.f, 1.f, 0.1f, 1.f);
                        
                        // Update uniform buffers
                        BufferUpdateDesc updateDesc = { pRPD->uniformsBuffers[pRHI->frameIndex] };
                        beginUpdateResource(&updateDesc);
                        memcpy(updateDesc.pMappedData, &pRPD->uniformsData, sizeof(RenderPassData::UniformsData));
                        endUpdateResource(&updateDesc);

                        // Reset uniform index for next frame
                        pRPD->curUniformIndex = 0;
                    }

                    
                }
            );

        // Draw
        // - Records GPU cmds
        ecs.system<Engine::Canvas, Window::SDLWindow>("FlappyClone::Draw")
            .kind(flecs::OnStore)
            .each([](flecs::iter& it, size_t i, Engine::Canvas& canvas, Window::SDLWindow& sdlWin)
                {
                    ASSERTMSG(i == 0, "Drawing to more than one window not implemented.");

                    RHI::RHI const* pRHI = it.world().has<RHI::RHI>() ? it.world().get<RHI::RHI>() : nullptr;
                    RenderPassData* pRPD = it.world().has<RenderPassData>() ? it.world().get_mut<RenderPassData>() : nullptr;

                    // Updated latest res so that it can be used if needed during next frame's update
                    pRPD->resX = sdlWin.pSwapChain->ppRenderTargets[0]->mWidth;
                    pRPD->resY = sdlWin.pSwapChain->ppRenderTargets[0]->mHeight;

                    if (pRHI && pRPD && sdlWin.pCurRT)
                    {
                        Cmd* pCmd = pRHI->curCmdRingElem.pCmds[0];
                        ASSERT(pCmd);

                        cmdBeginDebugMarker(pCmd, 1, 0, 1, "FlappyClone::ClearScreen");

                        RenderTargetBarrier barriers[] = {
                                 { sdlWin.pCurRT, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
                        };
                        cmdResourceBarrier(pCmd, 0, nullptr, 0, nullptr, 1, barriers);

                        BindRenderTargetsDesc bindRenderTargets = {};
                        bindRenderTargets.mRenderTargetCount = 1;
                        bindRenderTargets.mRenderTargets[0] = { sdlWin.pCurRT, LOAD_ACTION_CLEAR };
                        cmdBindRenderTargets(pCmd, &bindRenderTargets);
                        cmdSetViewport(pCmd, 0.0f, 0.0f, static_cast<float>(sdlWin.pCurRT->mWidth), static_cast<float>(sdlWin.pCurRT->mHeight), 0.0f, 1.0f);
                        cmdSetScissor(pCmd, 0, 0, sdlWin.pCurRT->mWidth, sdlWin.pCurRT->mHeight);

                        cmdBindRenderTargets(pCmd, nullptr);

                        barriers[0] = { sdlWin.pCurRT, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
                        cmdResourceBarrier(pCmd, 0, nullptr, 0, nullptr, 1, barriers);

                        cmdEndDebugMarker(pCmd);

                        cmdBeginDebugMarker(pCmd, 1, 0, 1, "FlappyClone::DrawObstacles");
                        
                        cmdBindPipeline(pCmd, pRPD->pPipeline);
                        cmdBindDescriptorSet(pCmd, pRHI->frameIndex, pRPD->pDescriptorSetUniforms);
                        cmdBindVertexBuffer(pCmd, 1, &pRPD->pVertexBuffer, &pRPD->vertexLayout.mBindings[0].mStride, nullptr);
                        cmdBindIndexBuffer(pCmd, pRPD->pIndexBuffer, INDEX_TYPE_UINT16, 0);
                        cmdDrawIndexedInstanced(pCmd, 6, 0, TOTALS_QUADS_TO_DRAW, 0, 0);

                        cmdBindRenderTargets(pCmd, nullptr);

                        cmdEndDebugMarker(pCmd);
                    }
                }
            );
    }

    void module::OnExit(flecs::world& ecs)
    {
        if (ecs.has<RHI::RHI>() && ecs.has<RenderPassData>())
        {
            RHI::RHI const* pRHI = ecs.get<RHI::RHI>();

            waitQueueIdle(pRHI->pGfxQueue);

            Renderer* pRenderer = pRHI->pRenderer;
            RenderPassData* pRenderPassData = ecs.get_mut<RenderPassData>();
            RemoveDescriptorSet(pRenderer, *pRenderPassData);
            RemoveRootSignature(pRenderer, *pRenderPassData);
            RemoveShaders(pRenderer, *pRenderPassData);
            
            for (uint32_t i = 0; i < pRHI->dataBufferCount; ++i)
            {
                removeResource(pRenderPassData->uniformsBuffers[i]);
            }

            removeResource(pRenderPassData->pVertexBuffer);
            removeResource(pRenderPassData->pIndexBuffer);

            RemovePipeline(pRenderer, *pRenderPassData);

            pRenderPassData->Reset();
        }
    }
}