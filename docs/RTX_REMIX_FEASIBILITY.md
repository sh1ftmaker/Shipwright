# RTX Support Feasibility for Ship of Harkinian

**Date:** 2026-03-17
**Scope:** Integration of NVIDIA RTX Remix (or equivalent ray-tracing) into the Ship of Harkinian codebase

---

## Executive Summary

Adding RTX ray tracing to Ship of Harkinian is **technically feasible but non-trivial**. RTX Remix cannot be dropped in directly — it requires a new graphics backend and light exposure work. Two integration paths exist: **(A) RTX Remix via a D3D9 fixed-function backend** (most compatible with the Remix ecosystem and its tooling) and **(B) a native Vulkan ray tracing backend** (more control, more work, no external dependency). Path B is the cleaner long-term option; Path A unlocks RTX Remix's community tooling (material editor, asset replacement) faster.

---

## Understanding the Current Rendering Architecture

### Graphics Stack

```
N64 Display List (Gfx*)
       │
       ▼
Interpreter::Run()          ← interpreter.cpp (4000+ lines)
  - RSP vertex processing
  - RDP state machine
  - CPU-side Gouraud lighting
  - Color combine evaluation
       │
       ▼
Multi-backend abstraction   ← gfx_rendering_api.h
       │
  ┌────┼────────┐
  │    │        │
OpenGL D3D11  Metal
```

### Critical Rendering Facts

1. **No GPU-side lighting.** All lighting — directional, positional, and ambient — is computed *on the CPU* in `GfxSpVertex` (interpreter.cpp:1258–1328) and baked into vertex colors before submission to the GPU. The GPU receives pre-lit `RGBA` vertex attributes; it has no concept of a light source.

2. **Programmable shaders throughout.** Every backend uses programmable vertex + fragment shaders generated dynamically from color combiner modes. There is no fixed-function pipeline path.

3. **Light data is available in the interpreter.** The N64 light structs — positions, directions, colors, attenuation coefficients — are live in `mRsp->current_lights[]` during display list processing. This is the extraction point for any path-tracing integration.

4. **Backend API spread.** The existing backends are OpenGL, Direct3D 11, and Metal. There is **no D3D9 backend**.

5. **Vertex normals exist.** `vn->n[0..2]` per-vertex normals are processed by the interpreter for lighting. These survive until `GfxSpVertex` writes them — they could be forwarded to a RT-aware backend if intercepted at the right point.

---

## RTX Remix Requirements vs. SoH Reality

| Requirement | RTX Remix Needs | SoH Reality | Gap |
|---|---|---|---|
| Graphics API | DirectX 8 or 9 | D3D11, OpenGL, Metal | **Hard mismatch** |
| Pipeline type | Fixed-function only | Programmable shaders | **Hard mismatch** |
| Light exposure | SetLight() D3D9 calls | CPU vertex shading | Needs new code |
| Geometry format | D3D9 FVF buffers | Custom float VBOs | Format translation needed |
| Scene reconstruction | Inferred from D3D9 FF calls | N/A — no D3D9 | Requires new backend |
| Platform | Windows 10/11 | Windows, Linux, macOS, WASM | Remix is Windows-only |

**Bottom line:** RTX Remix cannot hook into SoH as-is. It intercepts `d3d9.dll` API calls that SoH never makes. Bridging the gap requires either creating a D3D9 fixed-function backend or bypassing Remix entirely.

---

## Integration Path A: RTX Remix via D3D9 Fixed-Function Backend

### How It Works

RTX Remix works by intercepting D3D9 fixed-function API calls. The fixed-function pipeline is the key — RTX Remix infers material properties, light positions, and geometry from the D3D9 state machine. If SoH emits D3D9 fixed-function calls, Remix takes over from there.

### What Would Need to Be Built

**1. New `gfx_d3d9_ff.cpp` backend** implementing the `GfxRenderingAPI` interface but using D3D9 fixed-function calls:

```
GfxRenderingAPI::draw_triangles()
  → D3D9 SetFVF(D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1)
  → D3D9 DrawPrimitive()
```

The intentional goal is to look like a 2001-era D3D9 game. RTX Remix's scene reconstructor expects exactly this.

**2. Light injection** — before each `DrawPrimitive()` call, emit the current light state:

```
// interpreter.cpp already has: mRsp->current_lights[i].p.pos[x/y/z]
//                               mRsp->current_lights[i].l.col[r/g/b]
→ D3D9 SetLight(i, &d3dLight)
→ D3D9 LightEnable(i, TRUE)
```

This is the critical step. Without proper D3D9 `SetLight()` calls, RTX Remix has no scene lights to path-trace from, and the result would be only ambient illumination.

**3. Material/texture mapping** — N64 color combiner modes must approximate D3D9 texture stage states. This is lossy but workable for most cases:

```
G_CC_MODULATEI → D3DTSS_COLOROP = D3DTOP_MODULATE
G_CC_DECALRGBA → D3DTSS_COLOROP = D3DTOP_SELECTARG1
```

**4. Geometry de-projection.** The current interpreter works in clip space. The D3D9 FF backend needs world-space positions (pre-projection) so RTX Remix can reconstruct the scene geometry correctly. The world-space position `world_pos[]` is already available in `GfxSpVertex` at line ~1257.

**5. Normal forwarding.** Vertex normals (`vn->n[]`) must be passed through to D3D9 FVF instead of being consumed by the CPU lighting path. RTX Remix uses normals for surface shading and reflection direction.

### What You Get

- Full RTX Remix ecosystem: path tracing, DLSS 4, Neural Radiance Cache
- RTX Remix Creator Toolkit for material replacement (PBR textures, emissive surfaces)
- Community-driven asset remastering workflow compatible with 237+ established mod projects
- NVIDIA-maintained ray tracing implementation

### Challenges and Limitations

| Challenge | Severity | Notes |
|---|---|---|
| D3D9 FF pipeline approximation of N64 color combiners | Medium | ~80% of modes mappable; edge cases will need fallback |
| World-space position reconstruction | Medium | Data is available in interpreter; needs routing |
| Lighting fidelity | Medium-High | N64 has up to 32 lights; D3D9 FF supports 8. Need prioritization heuristic |
| Windows-only | High | Remix runtime only runs on Windows with RTX GPU. Other platforms unaffected |
| Two rendering modes | Low | D3D9 FF backend runs alongside existing backends; no need to remove them |
| 2-cycle N64 render modes | Medium | Complex alpha/color combiner passes won't have D3D9 FF equivalents |

### Architecture Diagram

```
interpreter.cpp (unchanged)
       │
       ├── mRsp->current_lights[]  ──────────────────────┐
       │                                                  │
       ├── world_pos[] (per vertex)  ────────────────┐   │
       │                                             │   │
       └── vn->n[] (vertex normals)  ───────────┐   │   │
                                                │   │   │
                              gfx_d3d9_ff.cpp   │   │   │
                              ┌─────────────────▼───▼───▼────┐
                              │  SetFVF(XYZ+NORMAL+TEX)      │
                              │  SetLight() x N               │
                              │  DrawPrimitive()              │
                              └───────────────────────────────┘
                                           │
                                     d3d9.dll (RTX Remix interposer)
                                           │
                                     dxvk-remix → Vulkan RT
                                           │
                                     Path-traced output with DLSS
```

### Estimated Effort

- **D3D9 FF backend skeleton:** 2–3 weeks (model after existing `gfx_direct3d11.cpp`)
- **Light injection + world-space geometry:** 1 week
- **Color combiner → texture stage mapping:** 2–3 weeks
- **Testing and visual debugging:** 2–4 weeks
- **Total:** ~2–3 months of focused engineering

---

## Integration Path B: Native Vulkan Ray Tracing

This path skips RTX Remix entirely and adds a Vulkan backend using `VK_KHR_ray_tracing_pipeline` (supported on all RTX GPUs and many AMD GPUs).

### Architecture

```
interpreter.cpp
  (modified to emit scene light data)
       │
       ▼
gfx_vulkan_rt.cpp (new backend)
  - Rasterize geometry normally via Vulkan graphics pipeline
  - Build BLAS (Bottom Level Acceleration Structure) from triangle data
  - Build TLAS (Top Level AS) per frame
  - Run ray tracing passes:
      1. Shadow rays (hard shadows from scene lights)
      2. Ambient occlusion
      3. (Optional) One-bounce indirect illumination via screen-space probes
       │
       ▼
Composite RT result with rasterized frame
```

### Advantages over RTX Remix

- Cross-platform: Vulkan RT runs on Windows and Linux
- Full control over what gets ray-traced and how
- No approximation of D3D9 FF semantics
- Can implement denoising (AMD FSR or NVIDIA's open RTXDI)
- Can be tuned specifically for N64 geometry (low-poly, large flat surfaces)

### Challenges

- Significantly more engineering: BLAS/TLAS management, shader binding tables, ray generation shaders
- No Remix asset tooling — material replacement would need separate implementation
- Denoising implementation or integration required for acceptable frame rates
- Acceleration structure rebuilds every frame (N64 geometry is fully dynamic)

### Estimated Effort

- **Vulkan backend for rasterization:** 4–6 weeks
- **BLAS/TLAS construction from interpreter data:** 3–4 weeks
- **RT shadow/AO passes:** 3–4 weeks
- **Denoising integration:** 2–3 weeks
- **Total:** ~4–5 months of focused engineering

---

## The Lighting Problem (Both Paths)

This is the most architecturally interesting challenge: **the N64 has no scene lights** from a GPU perspective. All illumination is pre-computed as Gouraud vertex colors. From the GPU's perspective, a lit N64 mesh and an unlit N64 mesh look identical — both are just colored vertices.

For path tracing to work meaningfully, you need:
1. Unlit geometry (vertex colors or albedo textures only)
2. Explicit scene lights the path tracer can cast rays from/to

**The interpreter already has both pieces:**
- Vertex normals (`vn->n[]`) before lighting is applied
- Light positions, directions, colors, and attenuation coefficients (`mRsp->current_lights[]`)

The solution for either path:

1. **Disable CPU lighting** in the D3D9/Vulkan RT backend (skip the `G_LIGHTING` vertex coloring pass)
2. **Forward vertex normals** to the GPU in world space
3. **Forward scene lights** as GPU scene lights or into the RT acceleration structure as area/directional lights
4. Let the path tracer re-compute illumination with ray-traced shadows and indirect lighting

This is a clean separation — existing rasterized backends are unaffected; the RT backend takes a different code path through `GfxSpVertex`.

---

## Specific N64/OOT Lighting Considerations

Ocarina of Time uses several lighting primitives:

| N64 Light Type | RTX Equivalent | Notes |
|---|---|---|
| Directional light (sun/moon) | Distant/directional light | Maps cleanly |
| Positional light (torches, fires) | Point/sphere light | Use attenuation coefficients from `p.unk7` / `p.unkE` |
| Ambient light | Sky light / ambient occlusion baseline | Every room has one ambient color |
| Lookat vectors | Used for env-mapping only | Not a light; irrelevant to RT |

OOT's indoor environments set a dominant ambient color (dungeon rooms are very dark amber, Forest Temple is green-tinted). The path tracer can respect this as a sky color or SH-encoded ambient term.

OOT uses **up to 7 simultaneous lights** per actor (well within RTX scene limits). The scene has many short-range positional lights for ambiance, ideal for point light path tracing.

---

## Recommendation

### If the goal is RTX Remix compatibility (Remix ecosystem, community tooling):

**Pursue Path A.** Build a `gfx_d3d9_ff.cpp` backend. The key insight is to intentionally use the D3D9 fixed-function pipeline — not because it's better, but because RTX Remix was designed to intercept exactly those calls. The color combiner approximation will have rough edges but the scene reconstruction and path-traced lighting will work well given OOT's relatively simple geometry.

Priority implementation order:
1. Basic D3D9 FF backend with world-space vertex positions and normals
2. Light injection from `mRsp->current_lights[]` via `SetLight()`
3. Color combiner → texture stage approximation
4. Test with RTX Remix runtime

### If the goal is maximum visual quality and cross-platform support:

**Pursue Path B.** A native Vulkan RT backend gives full control, works on Linux, and can be tuned specifically for OOT's art style. The investment is higher but so is the ceiling.

### If the goal is a quick proof of concept:

**Prototype with RTX Remix first.** Create a minimal D3D9 FF backend that emits just world-space geometry and lights. RTX Remix will immediately add path-traced shadows and indirect lighting, proving the concept before committing to the full implementation.

---

## Files to Modify / Create

### New Files
- `libultraship/src/fast/backends/gfx_direct3d9_ff.cpp` — D3D9 fixed-function backend
- `libultraship/src/fast/backends/gfx_direct3d9_ff.h`
- `libultraship/src/fast/backends/gfx_vulkan_rt.cpp` — (Path B) Vulkan RT backend

### Modified Files
- `libultraship/src/fast/interpreter.cpp` — New code path in `GfxSpVertex` to skip CPU lighting and forward normals/lights when RT backend is active
- `libultraship/src/fast/Fast3dWindow.cpp` — Backend selection logic
- `libultraship/CMakeLists.txt` — Build system additions

### No Changes Required
- All existing backends (OpenGL, D3D11, Metal) — fully preserved
- Game logic, OTR asset loading, controller input — untouched

---

## References

- RTX Remix Runtime: https://github.com/NVIDIAGameWorks/dxvk-remix
- RTX Remix Compatibility Requirements: https://github.com/NVIDIAGameWorks/rtx-remix/wiki/Compatibility
- SoH Lighting implementation: `libultraship/src/fast/interpreter.cpp:1258–1328`
- SoH Backend interface: `libultraship/include/fast/backends/gfx_rendering_api.h`
- SoH D3D11 reference backend: `libultraship/src/fast/backends/gfx_direct3d11.cpp`
