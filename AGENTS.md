# MC-CPP architecture quickmap

- Build/entry: `CMakeLists.txt` wires GLFW (from `include/glfw`), GLAD, stb, glm, miniaudio, zlib, and compiles all sources; `compile.bat` is a clean rebuild helper. `MC-CPP.desktop` is a Linux launcher pointing to the built binary/assets.
- Game loop: `src/main.cpp` bootstraps GLFW/GLAD, loads window icon/font/shaders, instantiates `World`, `TextureManager`, and `Player`, registers input callbacks (mouse/key, block placement/breaking, saving, fullscreen toggle), loads block definitions via `load_blocks` from `data/blocks.mccpp`, initializes audio, and runs a per-frame update + render loop (player input, world tick, rendering, crosshair, debug text). Startup menu for music volume is shown before entering the game, mouse is captured when gameplay begins.
- World & chunks: `src/world.h/cpp` owns shaders, texture manager, player reference, block type registry, and a chunk map keyed by chunk coords. Handles block set/get with collision checks, daylight/block-light propagation queues, skylight init, chunk update scheduling, and rendering order (visible chunks sorted by distance, opaque then translucent passes).
- Chunk meshing: `src/chunk/chunk.h/cpp` stores block/light arrays for a 16x128x16 chunk, manages subchunks, mesh buffers, and GPU upload; `update_at_position` queues neighboring subchunks when a block changes. `src/chunk/subchunk.h/cpp` builds per-subchunk meshes, computing face visibility, per-face AO/smooth lighting, and selecting translucent vs opaque buffers based on `BlockType`.
- Block types & geometry: `src/renderer/block_type.*` turns `ModelData` + texture bindings into renderable block definitions (colliders, UVs, transparency flags). `src/models/*` provides model templates (cube, plant, liquid, slab, torch, door, ladder, etc.) with vertex data/UVs/lighting defaults; `data/blocks.mccpp` maps numeric IDs to model/texture assignments and names used at load.
- Rendering helpers: `src/renderer/shader.*` is a minimal GL shader wrapper; `src/renderer/texture_manager.*` builds a texture array atlas via stb_image and hands out indices; `src/text_renderer.*` implements bitmap text rendering with stb_truetype for UI/debug overlays.
- Simulation/entities: `src/entity/entity.*` defines physics integration (gravity, drag, collision resolution with block colliders) and movement state; `src/entity/player.*` extends it with input-driven acceleration, sprint logic, view matrices, and interpolation for rendering. `src/physics/collider.h` is the AABB + sweep test helper; `src/physics/hit.*` implements ray casting for block targeting.
- Persistence & world init: `src/save.*` loads/saves chunks to `save/` using NBT gzip, generates flat terrain fallback, seeds lighting, and queues meshing; `src/nbt_utils.*` handles minimal NBT ByteArray read/write with zlib.
- Audio: `src/audio.*` initializes miniaudio, scans `assets/audio/music/`, and schedules randomized music playback with simple timing logic.
- Config/utils: `src/options.h` holds tunables (render distance, FOV, lighting flags); `src/util.h` collects direction constants and a hash for `glm::ivec3` used in chunk maps. `compile.bat` rebuilds from a fresh `build/` folder via `cmake -S . -B build`.

Note: Third-party libraries live under `include/` (glfw, glad, glm, stb, zlib, miniaudio) and were not inspected per request.

## Real-time shadow mapping (CSM) – notes & pitfalls

- High-level:
  - Shadows are classic directional shadow maps with cascades (CSM), driven by the sun direction from `World::get_light_direction()` and the camera matrices from `Player`.
  - All toggles and tunables live in `src/options.h` under the `Options::SHADOW_*` fields; `SHADOWS_ENABLED` defaults to `true` in this repo snapshot but can be flipped off to hard-disable the whole system.

- Core ownership:
  - `World` owns all shadow resources:
    - `shadow_shader` – dedicated depth-only shader, see `assets/shaders/shadow/*`.
    - `shadow_fbo` – framebuffer for rendering into the depth array.
    - `shadow_map` – `GL_TEXTURE_2D_ARRAY` holding one depth slice per cascade.
    - `shadow_matrices` / `shadow_splits` – per-cascade light-space matrices and split distances.
    - Cached uniform locations in the main world shader for `u_ShadowMap`, `u_LightSpaceMatrices`, `u_CascadeSplits`, `u_ShadowCascadeCount`, `u_ShadowTexelSize`, `u_ShadowMinBias`, `u_ShadowSlopeBias`, `u_ShadowPCFRadius`.
  - Initialization is in `World::init_shadow_resources()` (called from `World` ctor if `SHADOWS_ENABLED` and the main shader/texture manager are valid). On any failure (bad shader, incomplete FBO, etc.) shadows are auto-disabled and the rest of the pipeline behaves as pre-shadow.

- GL state interactions (important “pain points”):
  - Shadow rendering (`World::render_shadows()`):
    - Binds `shadow_fbo`, switches `glDrawBuffer(GL_NONE)` / `glReadBuffer(GL_NONE)` and restores previous draw/read buffers and viewport after the pass.
    - Uses `glBindTexture(GL_TEXTURE_2D_ARRAY, texture_manager->texture_array)` on `GL_TEXTURE0` for alpha-testing in the shadow pass.
    - It is crucial that any code which uploads block textures re-binds the correct texture object before `glTexSubImage3D` or `glGenerateMipmap` (see `TextureManager` notes below) – otherwise you will accidentally write into the shadow map instead of the atlas (this was the main source of “all black/odd colors” bugs).
  - Main world draw (`World::draw()`):
    - Always calls `shader->use()` and sets `u_Daylight` each frame.
    - Explicitly forces `u_TextureArraySampler = 0` to guarantee that the main shader samples from `GL_TEXTURE0` regardless of any earlier GL state.
    - When shadows are enabled and valid, binds `shadow_map` on `GL_TEXTURE1` and pushes all shadow-related uniforms; if something is missing, it falls back to `u_ShadowCascadeCount = 0` so the shader can skip shadows safely.

- TextureManager & shadow map interaction (the big gotcha):
  - `TextureManager` owns `texture_array` – the block atlas used by the world shader (`u_TextureArraySampler`).
  - Before the shadow system was added, it relied on the global binding state; after shadows introduced another `GL_TEXTURE_2D_ARRAY`, it became easy to accidentally upload block textures into the wrong texture (the shadow map) if something else had been bound last.
  - To prevent this, `TextureManager` now **always** does:
    - `glBindTexture(GL_TEXTURE_2D_ARRAY, texture_array);` in:
      - `TextureManager::TextureManager` (before `glTexImage3D`),
      - `TextureManager::generate_mipmaps` (before `glGenerateMipmap`),
      - `TextureManager::add_texture` (before `glTexSubImage3D`).
  - Symptom of forgetting this: blocks render as pure black / diagnostic magenta, and test sampling from `vec3(0.5, 0.5, 0.0)` in the world shader returns ~0, meaning layer 0 of the atlas is empty.

- Player & camera matrices:
  - `Player` exposes `get_current_fov()`; it reproduces the sprint-based FOV change used in `update_matrices`, but in a reusable form so shadows and CSM can use the same FOV.
  - `Player::update_matrices` now:
    - Computes `p_matrix` via `glm::perspective(glm::radians(get_current_fov()), view_width/view_height, near_plane, far_plane)`.
    - Builds `mv_matrix` with rotations (`rotation.y` pitch, `rotation.x` yaw + 1.57) and translation by `-(interpolated_position + (0, eyelevel + step_offset, 0))`.
    - Fills `vp_matrix = p_matrix * mv_matrix` and pushes:
      - `u_MVPMatrix` = `vp_matrix` (as before),
      - `u_ViewMatrix` = `mv_matrix` (new, used by shadow code and for view-space depth).
  - **Important**: `Player::update_matrices` assumes that the correct program is bound; `main.cpp` explicitly calls `shader.use(); player.update_matrices(1.0f);` to guarantee uniforms go into the world shader, not into any other GL program.

- Shadow cascades (CSM) specifics:
  - Parameters (see `Options`):
    - `SHADOW_MAP_RESOLUTION` – atlas resolution per-cascade (square); defaults to 2048.
    - `SHADOW_CASCADES` – number of cascades (1–4 supported); defaults to 4.
    - `SHADOW_LOG_WEIGHT` – mix factor between logarithmic and linear split distribution.
    - `SHADOW_MIN_BIAS` / `SHADOW_SLOPE_BIAS` – currently only `SHADOW_MIN_BIAS` is used; slope bias is reserved for future tuning.
    - `SHADOW_PCF_RADIUS` – PCF radius (1 → 3×3, 2 → 5×5).
  - `World::update_shadow_cascades()`:
    - Uses `near_plane` / `far_plane` from `Player`, FoV from `get_current_fov()`, and aspect ratio from `view_width / view_height`.
    - Computes split distances in view-space (`u_CascadeSplits`) as a log/lin mix (`lambda = Options::SHADOW_LOG_WEIGHT`).
    - For each cascade: constructs 8 frustum corner points in view-space (near/far corners), transforms them to world-space via `inverse(mv_matrix)`, finds bounding sphere radius, then builds a light-space view/projection:
      - Light direction from `get_light_direction()` (normalized).
      - Light position = frustum center − lightDir * (radius * 2).
      - `lightView = lookAt(lightPos, center, (0,1,0))`.
      - `lightProj = ortho(-radius, radius, -radius, radius, zNear=0.1, zFar=radius*4)`.
    - The resulting `shadow_matrices[i] = lightProj * lightView` are uploaded to the world shader when drawing.

- Shadow pass & main pass ordering:
  - In `main.cpp` game loop, the order is:
    1. `player.update(dt);`
    2. `world.tick(dt);`
    3. `shader.use();`
    4. `player.update_matrices(1.0f);` (updates `u_MVPMatrix`, `u_ViewMatrix`)
    5. `world.prepare_rendering();` (sorts chunks by distance)
    6. `world.render_shadows();` (fills depth array for each cascade)
    7. Compute `daylight_factor`, set clear color.
    8. `post_processor->beginRender();`
    9. `shader.use(); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D_ARRAY, tm.texture_array);`
    10. `world.draw();` (uploads shadow uniforms, draws opaque chunks, then translucent).
    11. Post-process, crosshair, UI, swap buffers.
  - Key invariant: **shadow pass uses the same sorted `visible_chunks` as the main pass**, and both share the same camera matrices from the immediately preceding `update_matrices` call.

- World shader (colored_lighting) integration:
  - Vertex shader (`assets/shaders/colored_lighting/vert.glsl`):
    - Takes packed `a_Data0/1/2` (pos, UV, light) from chunk VBO, decodes to `v_Position` (world-space), `v_TexCoords` (u, v, layer), and `v_Light` (colored lighting with day/night factor).
    - Uses `u_ViewMatrix` to compute `v_ViewDepth` = `- (u_ViewMatrix * vec4(v_Position,1)).z`, used to choose cascade in the fragment shader.
  - Fragment shader (`assets/shaders/colored_lighting/frag.glsl`):
    - Samples block atlas via `u_TextureArraySampler` at `v_TexCoords`.
    - Early discards transparent texels (`textureColor.a <= 0.5`).
    - Computes soft shadow factor via `calculateShadowFactor(v_Position, v_ViewDepth)` using:
      - `u_ShadowMap` (depth array),
      - `u_LightSpaceMatrices[MAX_CASCADES]`,
      - `u_CascadeSplits[MAX_CASCADES]`,
      - `u_ShadowCascadeCount`,
      - `u_ShadowTexelSize`,
      - `u_ShadowMinBias` / `u_ShadowPCFRadius`.
    - Final color: `fragColor = textureColor * vec4(v_Light * shadowFactor, 1.0);`.
    - Shadow factor is intentionally clamped to a very mild range (`[0.9, 1.0]`) to avoid “all black” regressions while the system is tuned.

- Debugging tips (what went wrong before):
  - If blocks become **pure black** when enabling shadows:
    - First suspect: block atlas writes are going into the wrong texture. Ensure `TextureManager` always binds `texture_array` before `glTexSubImage3D` / `glGenerateMipmap`.
    - Second suspect: world shader sampling the wrong texture unit – check that `u_TextureArraySampler` is set to 0 in both `main.cpp` (initial setup) and `World::draw`.
  - If blocks become **diagnostic magenta**:
    - That means `texture(u_TextureArraySampler, v_TexCoords)` is ~0, i.e., atlas layer/UV are invalid for that fragment (or that particular texture file is missing/solid black). Use this to spot broken block definitions or missing PNGs.
  - If the whole frame goes black/empty right after shadow rendering:
    - Check that `World::render_shadows` restores `glDrawBuffer`/`glReadBuffer` and `glViewport` after the pass; leaving them as `GL_NONE`/shadow viewport will make the subsequent passes appear blank.

- Safe defaults & rollback:
  - Set `Options::SHADOWS_ENABLED = false` to completely bypass shadow init and `render_shadows` while preserving the original lighting model.
  - If tweaking shaders or GL state, prefer to:
    - First validate that the world renders correctly with shadows disabled.
    - Then re-enable shadows and only adjust bias / PCF / cascade parameters, not the texture upload path or sampler units.
