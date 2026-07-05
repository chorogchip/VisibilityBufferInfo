# Visibility Buffer 및 GPU 렌더링 참고 링크

Visibility Buffer, 소프트웨어 래스터라이제이션, The Forge TVB, 실제 엔진 구현, 그래픽 API 명세를 주제별로 정리한 자료 모음입니다.

## 목차

- 1. 핵심 논문과 연구 자료
- 2. The Forge Triangle Visibility Buffer
- 3. GPU Zen 서적과 목차 자료
- 4. DOOM: The Dark Ages와 컨퍼런스 자료
- 5. 다른 렌더링 프레임워크와 엔진 구현
- 6. 그래픽 API와 셰이더 기능 명세
- 7. 차분 렌더링과 관련 실시간 렌더링 자료

---

## 1. 핵심 논문과 연구 자료

### 1.1 Visibility Buffer와 지연 속성 보간

**The Visibility Buffer 논문 페이지, PDF·BibTeX·부속 HLSL 코드 제공**  
<https://jcgt.org/published/0002/02/04/>

**Burns와 Hunt의 2013년 The Visibility Buffer 논문 원문**  
<https://jcgt.org/published/0002/02/04/paper.pdf>

**The Visibility Buffer가 수록된 JCGT 전체 이슈**  
<https://jcgt.org/published/0002/02/>

**Deferred Attribute Interpolation for Memory-Efficient Deferred Shading 논문**  
<https://cg.ivd.kit.edu/publications/2015/dais/DAIS.pdf>

**Eurographics Digital Library의 DAIS 논문 페이지**  
<https://diglib.eg.org/items/f74abc84-78bd-4be9-a300-61820d92c204>

**GPU Pro 7의 Deferred Attribute Interpolation Shading 챕터**  
<https://www.taylorfrancis.com/chapters/edit/10.1201/b21261-10/deferred-attribute-interpolation-shading-christoph-schied-carsten-dachsbacher>

### 1.2 프로그래머블·소프트웨어 래스터라이제이션

**Piko: A Framework for Authoring Programmable Graphics Pipelines 논문**  
<https://arxiv.org/abs/1404.6293>

**LucidRaster: Fully Programmable Rasterization for Order-Independent Transparency 논문**  
<https://arxiv.org/abs/2405.13364>

**CuRast: CUDA-Based Software Rasterization for Billions of Triangles 논문**  
<https://arxiv.org/abs/2604.21749>

**CuRast 소프트웨어 래스터라이저 소스 코드**  
<https://github.com/m-schuetz/CuRast>

**Rendering Point Clouds with Compute Shaders 논문**  
<https://arxiv.org/abs/2104.07526>

**Software Rasterization of 2 Billion Points in Real Time 논문**  
<https://arxiv.org/abs/2204.01287>

---

## 2. The Forge Triangle Visibility Buffer

### 2.1 개념 설명과 초기 발표

**Triangle Visibility Buffer 1.0 구조 설명 글**  
<https://diaryofagraphicsprogrammer.blogspot.com/2018/03/triangle-visibility-buffer.html>

**The Forge Triangle Visibility Buffer 위키**  
<https://github.com/ConfettiFX/The-Forge/wiki/Triangle-Visibility-Buffer>

**GDC Europe 2016 Filtered Triangle Visibility Buffer 발표**  
<https://www.gdcvault.com/play/1023792/4K-Rendering-Breakthrough-The-Filtered>

**TVB의 pre-skinned animation 처리 설명**  
<https://github.com/ConfettiFX/The-Forge/wiki/Triangle-Visibility-Buffer-Pre-Skinned-Animations>

### 2.2 TVB 1.0 예제와 공용 구현

**The Forge TVB 1.0 예제 전체**  
<https://github.com/ConfettiFX/The-Forge/tree/master/Examples_3/Visibility_Buffer>

**TVB 1.0 메인 애플리케이션 코드**  
<https://github.com/ConfettiFX/The-Forge/blob/master/Examples_3/Visibility_Buffer/src/Visibility_Buffer.cpp>

**TVB 1.0 셰이더 전체**  
<https://github.com/ConfettiFX/The-Forge/tree/master/Examples_3/Visibility_Buffer/src/Shaders/FSL>

**TVB 1.0 셰이더 목록**  
<https://github.com/ConfettiFX/The-Forge/blob/master/Examples_3/Visibility_Buffer/src/Shaders/FSL/shaders.list>

**The Forge 공용 VisibilityBuffer 모듈**  
<https://github.com/ConfettiFX/The-Forge/blob/master/Common_3/Renderer/VisibilityBuffer/VisibilityBuffer.cpp>

**The Forge VisibilityBuffer 인터페이스**  
<https://github.com/ConfettiFX/The-Forge/blob/master/Common_3/Renderer/Interfaces/IVisibilityBuffer.h>

### 2.3 The Forge 저장소·릴리스·라이선스

**The Forge 공식 저장소**  
<https://github.com/ConfettiFX/The-Forge>

**The Forge 릴리스와 TVB 2.0 변경 이력**  
<https://github.com/ConfettiFX/The-Forge/releases>

**The Forge 라이선스**  
<https://github.com/ConfettiFX/The-Forge/blob/master/LICENSE>

### 2.4 TVB 2.0 애플리케이션과 공용 모듈

**The Forge TVB 2.0 예제 전체**  
<https://github.com/ConfettiFX/The-Forge/tree/master/Examples_3/Visibility_Buffer2>

**TVB 2.0 메인 애플리케이션 코드**  
<https://github.com/ConfettiFX/The-Forge/blob/master/Examples_3/Visibility_Buffer2/src/Visibility_Buffer.cpp>

**공용 VisibilityBuffer2 모듈 구현**  
<https://github.com/ConfettiFX/The-Forge/blob/master/Common_3/Renderer/VisibilityBuffer2/VisibilityBuffer2.cpp>

**VisibilityBuffer2 인터페이스**  
<https://github.com/ConfettiFX/The-Forge/blob/master/Common_3/Renderer/Interfaces/IVisibilityBuffer2.h>

### 2.5 TVB 2.0 셰이더와 Compute Rasterizer

**TVB 2.0 FSL 셰이더 전체**  
<https://github.com/ConfettiFX/The-Forge/tree/master/Examples_3/Visibility_Buffer2/src/Shaders/FSL>

**TVB 2.0 셰이더 빌드 목록**  
<https://github.com/ConfettiFX/The-Forge/blob/master/Examples_3/Visibility_Buffer2/src/Shaders/FSL/shaders.list>

**TVB 2.0 셰이더 공통 정의**  
<https://github.com/ConfettiFX/The-Forge/blob/master/Examples_3/Visibility_Buffer2/src/Shaders/FSL/ShaderDefs.h.fsl>

**TVB 2.0 리소스와 버퍼 정의**  
<https://github.com/ConfettiFX/The-Forge/blob/master/Examples_3/Visibility_Buffer2/src/Shaders/FSL/VisibilityBufferResources.h.fsl>

**TVB 2.0 compute rasterizer 공통 코드**  
<https://github.com/ConfettiFX/The-Forge/blob/master/Examples_3/Visibility_Buffer2/src/Shaders/FSL/rasterizer.h.fsl>

**삼각형 필터링 compute shader**  
<https://github.com/ConfettiFX/The-Forge/blob/master/Examples_3/Visibility_Buffer2/src/Shaders/FSL/TriangleFiltering.comp.fsl>

**Binning과 compute rasterization 핵심 셰이더**  
<https://github.com/ConfettiFX/The-Forge/blob/master/Examples_3/Visibility_Buffer2/src/Shaders/FSL/BinRasterizer.comp.fsl>

**TVB 2.0 버퍼 초기화 셰이더**  
<https://github.com/ConfettiFX/The-Forge/blob/master/Examples_3/Visibility_Buffer2/src/Shaders/FSL/ClearBuffers.comp.fsl>

**Compute render target 초기화 셰이더**  
<https://github.com/ConfettiFX/The-Forge/blob/master/Examples_3/Visibility_Buffer2/src/Shaders/FSL/ClearRenderTarget.comp.fsl>

**Visibility depth 결과 복사 셰이더**  
<https://github.com/ConfettiFX/The-Forge/blob/master/Examples_3/Visibility_Buffer2/src/Shaders/FSL/VisibilityBufferBlitDepth.frag.fsl>

**Visibility Buffer 최종 shading 셰이더**  
<https://github.com/ConfettiFX/The-Forge/blob/master/Examples_3/Visibility_Buffer2/src/Shaders/FSL/VisibilityBufferShade.frag.fsl>

### 2.6 관련 예제와 FSL·에셋 문서

**Visibility Buffer OIT 예제**  
<https://github.com/ConfettiFX/The-Forge/tree/master/Examples_3/Unit_Tests/src/15a_VisibilityBufferOIT>

**Visibility Buffer OIT 메인 코드**  
<https://github.com/ConfettiFX/The-Forge/blob/master/Examples_3/Unit_Tests/src/15a_VisibilityBufferOIT/15a_VisibilityBufferOIT.cpp>

**Visibility Buffer 기반 조명·그림자 예제**  
<https://github.com/ConfettiFX/The-Forge/tree/master/Examples_3/Unit_Tests/src/09_LightShadowPlayground>

**Visibility Buffer 리소스 테이블**  
<https://github.com/ConfettiFX/The-Forge/blob/master/Examples_3/Unit_Tests/src/09_LightShadowPlayground/Shaders/FSL/VisibilityBuffer.srt.h>

**Visibility depth pass 리소스 테이블**  
<https://github.com/ConfettiFX/The-Forge/blob/master/Examples_3/Unit_Tests/src/09_LightShadowPlayground/Shaders/FSL/VisibilityBufferDepthPass.srt.h>

**The Forge Shader Language 프로그래밍 가이드**  
<https://github.com/ConfettiFX/The-Forge/wiki/FSL-Programming-Guide>

**FSL 통합 가이드**  
<https://github.com/ConfettiFX/The-Forge/wiki/FSL-Integration-Guide>

**The Forge 에셋 파이프라인 설명**  
<https://github.com/ConfettiFX/The-Forge/wiki/AssetPipeline>

**The Forge 전체 위키**  
<https://github.com/ConfettiFX/The-Forge/wiki>

### 2.7 I3D 2024 TVB 2.0 발표

**I3D 2024 Triangle Visibility Buffer 2.0 발표 페이지**  
<https://i3dsymposium.org/2024/sponsored-talks.html>

**I3D 2024 TVB 2.0 발표 영상**  
<https://youtu.be/kWLev9CoQdg>

**I3D 2024 TVB 2.0 발표 슬라이드**  
<http://www.conffx.com/I3D-VisibilityBuffer2.pptx>

**I3D 2024 컨퍼런스 메인 페이지**  
<https://i3dsymposium.org/2024/>

---

## 3. GPU Zen 서적과 목차 자료

### 3.1 GPU Zen 3·4 및 참고 목차

**GPU Zen 3 페이퍼백, Triangle Visibility Buffer 2.0 챕터 수록**  
<https://www.amazon.com/GPU-Zen-Advanced-Rendering-Techniques/dp/B0DNXNM14K>

**GPU Zen 3 Kindle판**  
<https://www.amazon.com/GPU-Zen-Advanced-Rendering-Techniques-ebook/dp/B0DNWXT24X>

**GPU Zen 시리즈 관련 블로그**  
<https://gpuzen.blogspot.com/>

**GPU 그래픽 서적 비공식 목차 모음**  
<https://github.com/Gforcex/GPU-Book>

**GPU Zen 4 페이퍼백**  
<https://www.amazon.com/GPU-Zen-Advanced-Rendering-Techniques/dp/B0GNZJPVZ4>

**GPU Zen 4 Kindle판**  
<https://www.amazon.com/GPU-Zen-Advanced-Rendering-Techniques-ebook/dp/B0GP9TJXYM>

**GPU Zen 4 비공식 목차**  
<https://github.com/Gforcex/GPU-Book#gpu-zen-4>

**GPU Zen 1 비공식 목차**  
<https://github.com/Gforcex/GPU-Book#gpu-zen-1>

---

## 4. DOOM: The Dark Ages와 컨퍼런스 자료

### 4.1 Graphics Programming Conference 2025 발표

**Graphics Programming Conference 2025 발표 목록**  
<https://graphicsprogrammingconference.com/2025/>

**DOOM: The Dark Ages Visibility Buffer 발표 PDF**  
<https://static.graphicsprogrammingconference.com/public/2025/slides/visibility-buffer-and-deferred-rendering-in-doom/Lazarek-Hammer-visibility-buffer-and-deferred-rendering-in-doom-the-dark-ages.pdf>

**DOOM Visibility Buffer 발표 PPTX**  
<https://static.graphicsprogrammingconference.com/public/2025/slides/visibility-buffer-and-deferred-rendering-in-doom/Lazarek-Hammer-visibility-buffer-and-deferred-rendering-in-doom-the-dark-ages.pptx>

**DOOM Variable-Rate Compute Shaders 발표 PDF**  
<https://static.graphicsprogrammingconference.com/public/2025/slides/variable-rate-compute-shaders-in-doom/Fuller-Hammer-variable-rate-compute-shaders-in-doom-the-dark-ages.pdf>

**DOOM Variable-Rate Compute Shaders 발표 PPTX**  
<https://static.graphicsprogrammingconference.com/public/2025/slides/variable-rate-compute-shaders-in-doom/Fuller-Hammer-variable-rate-compute-shaders-in-doom-the-dark-ages.pptx>

**Microsoft의 DOOM VRCS 성능 최적화 글**  
<https://developer.microsoft.com/en-us/games/articles/2026/04/variable-rate-compute-shaders-doom-the-dark-ages/>

### 4.2 GDC 세션과 발표 아카이브

**GDC DOOM: The Dark Ages 렌더링 분석 세션**  
<https://schedule.gdconf.com/session/rip-tear-breaking-down-the-renderingofdoom-the-dark-ages/915274>

**GDC 발표 자료 아카이브**  
<https://www.gdcvault.com/>

---

## 5. 다른 렌더링 프레임워크와 엔진 구현

### 5.1 AMD GeometryFX

**AMD GPUOpen GeometryFX 아카이브**  
<https://gpuopen.com/archived/geometryfx/>

**AMD GeometryFX 공식 저장소**  
<https://github.com/GPUOpen-Effects/GeometryFX>

**GeometryFX 전체 소스**  
<https://github.com/GPUOpen-Effects/GeometryFX/tree/master>

**GeometryFX 라이선스**  
<https://github.com/GPUOpen-Effects/GeometryFX/blob/master/LICENSE.txt>

### 5.2 NVIDIA Falcor VBuffer

**NVIDIA Falcor 렌더링 프레임워크**  
<https://github.com/NVIDIAGameWorks/Falcor>

**Falcor VBuffer 구현 디렉터리**  
<https://github.com/NVIDIAGameWorks/Falcor/tree/master/Source/RenderPasses/GBuffer/VBuffer>

**Falcor raster V-buffer 패스**  
<https://github.com/NVIDIAGameWorks/Falcor/blob/master/Source/RenderPasses/GBuffer/VBuffer/VBufferRaster.cpp>

**Falcor raster V-buffer 인터페이스**  
<https://github.com/NVIDIAGameWorks/Falcor/blob/master/Source/RenderPasses/GBuffer/VBuffer/VBufferRaster.h>

**Falcor raster V-buffer 셰이더**  
<https://github.com/NVIDIAGameWorks/Falcor/blob/master/Source/RenderPasses/GBuffer/VBuffer/VBufferRaster.3d.slang>

**Falcor ray-traced V-buffer 패스**  
<https://github.com/NVIDIAGameWorks/Falcor/blob/master/Source/RenderPasses/GBuffer/VBuffer/VBufferRT.cpp>

**Falcor ray-traced V-buffer 공통 셰이더**  
<https://github.com/NVIDIAGameWorks/Falcor/blob/master/Source/RenderPasses/GBuffer/VBuffer/VBufferRT.slang>

**Falcor compute V-buffer 셰이더**  
<https://github.com/NVIDIAGameWorks/Falcor/blob/master/Source/RenderPasses/GBuffer/VBuffer/VBufferRT.cs.slang>

**Falcor ray tracing V-buffer 셰이더**  
<https://github.com/NVIDIAGameWorks/Falcor/blob/master/Source/RenderPasses/GBuffer/VBuffer/VBufferRT.rt.slang>

**Falcor VBufferRaster 테스트 그래프**  
<https://github.com/NVIDIAGameWorks/Falcor/blob/master/tests/image_tests/renderpasses/graphs/VBufferRaster.py>

**Falcor alpha-tested VBufferRaster 테스트**  
<https://github.com/NVIDIAGameWorks/Falcor/blob/master/tests/image_tests/renderpasses/graphs/VBufferRasterAlpha.py>

### 5.3 Wicked Engine Visibility Buffer

**Wicked Engine 공식 저장소**  
<https://github.com/turanszkij/WickedEngine>

**Visibility surface attribute 재구성 셰이더**  
<https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/shaders/visibility_surfaceCS.hlsl>

**Visibility Buffer shading 셰이더**  
<https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/shaders/visibility_shadeCS.hlsl>

**Visibility 결과 resolve 셰이더**  
<https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/shaders/visibility_resolveCS.hlsl>

**Visibility 기반 motion vector 셰이더**  
<https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/shaders/visibility_velocityCS.hlsl>

**Visibility pipeline sky 셰이더**  
<https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/shaders/visibility_skyCS.hlsl>

**Wicked Engine CPU·GPU 공용 렌더링 구조**  
<https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/shaders/ShaderInterop_Renderer.h>

**Wicked Engine 렌더러 구현**  
<https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/wiRenderer.cpp>

**Wicked Engine 라이선스**  
<https://github.com/turanszkij/WickedEngine/blob/master/LICENSE>

---

## 6. 그래픽 API와 셰이더 기능 명세

### 6.1 Barycentric 좌표·원자 연산·Wave/Subgroup·Conservative Rasterization

**HLSL 시스템 시맨틱 공식 문서**  
<https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-semantics>

**DirectX barycentric coordinate 명세**  
<https://microsoft.github.io/DirectX-Specs/d3d/Barycentrics.html>

**HLSL InterlockedMin 문서**  
<https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/interlockedmin>

**Shader Model 6 wave intrinsic 설명**  
<https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/hlsl-shader-model-6-0-features-for-direct3d-12#wave-intrinsics>

**Direct3D 12 conservative rasterization 문서**  
<https://learn.microsoft.com/en-us/windows/win32/direct3d12/conservative-rasterization>

**Vulkan fragment shader barycentric 확장**  
<https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_fragment_shader_barycentric.html>

**SPIR-V barycentric 확장 명세**  
<https://github.khronos.org/SPIRV-Registry/extensions/KHR/SPV_KHR_fragment_shader_barycentric.html>

**Vulkan conservative rasterization 확장**  
<https://registry.khronos.org/vulkan/specs/latest/man/html/VK_EXT_conservative_rasterization.html>

**Vulkan subgroup 가이드**  
<https://docs.vulkan.org/guide/latest/subgroups.html>

**Metal Shading Language 공식 명세**  
<https://developer.apple.com/metal/Metal-Shading-Language-Specification.pdf>

**Metal 기능 지원 표**  
<https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf>

**Apple Metal 공식 문서**  
<https://developer.apple.com/documentation/metal>

### 6.2 Indirect Draw·Dispatch와 Work Graphs

**Direct3D 12 ExecuteIndirect 문서**  
<https://learn.microsoft.com/en-us/windows/win32/direct3d12/indirect-drawing>

**Vulkan indirect draw 가이드**  
<https://docs.vulkan.org/guide/latest/draw_indirect.html>

**Vulkan indexed indirect draw 명세**  
<https://registry.khronos.org/vulkan/specs/latest/man/html/vkCmdDrawIndexedIndirect.html>

**Vulkan indirect draw count 명세**  
<https://registry.khronos.org/vulkan/specs/latest/man/html/vkCmdDrawIndexedIndirectCount.html>

**Vulkan indirect compute dispatch 명세**  
<https://registry.khronos.org/vulkan/specs/latest/man/html/vkCmdDispatchIndirect.html>

**Direct3D 12 Work Graphs 명세**  
<https://microsoft.github.io/DirectX-Specs/d3d/WorkGraphs.html>

### 6.3 Mesh Shader·Meshlet·Geometry Processing

**DirectX Mesh Shader 명세**  
<https://microsoft.github.io/DirectX-Specs/d3d/MeshShader.html>

**Vulkan Mesh Shader 확장**  
<https://registry.khronos.org/vulkan/specs/latest/man/html/VK_EXT_mesh_shader.html>

**Microsoft D3D12 Mesh Shader 샘플**  
<https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12MeshShaders>

**NVIDIA Vulkan·OpenGL meshlet 샘플**  
<https://github.com/nvpro-samples/gl_vk_meshlet_cadscene>

**Meshoptimizer 공식 문서**  
<https://meshoptimizer.org/>

**Meshoptimizer 소스 코드**  
<https://github.com/zeux/meshoptimizer>

**Meshoptimizer meshlet API 설명**  
<https://github.com/zeux/meshoptimizer#mesh-shading>

**Meshoptimizer simplification과 LOD 설명**  
<https://github.com/zeux/meshoptimizer#simplification>

**Microsoft DirectXMesh 라이브러리**  
<https://github.com/microsoft/DirectXMesh>

### 6.4 Programmable Sample Positions와 Variable Rate Shading

**Direct3D 12 programmable sample positions 명세**  
<https://microsoft.github.io/DirectX-Specs/d3d/ProgrammableSamplePositions.html>

**Vulkan programmable sample locations 확장**  
<https://registry.khronos.org/vulkan/specs/latest/man/html/VK_EXT_sample_locations.html>

**Call of Duty software-based VRS 발표**  
<https://research.activision.com/publications/2020-09/software-based-variable-rate-shading-in-call-of-duty--modern-war>

**Direct3D 12 Variable Rate Shading 문서**  
<https://learn.microsoft.com/en-us/windows/win32/direct3d12/vrs>

**Vulkan fragment shading rate 확장**  
<https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_fragment_shading_rate.html>

---

## 7. 차분 렌더링과 관련 실시간 렌더링 자료

### 7.1 nvdiffrast와 차분 렌더링

**NVIDIA nvdiffrast 소스 코드**  
<https://github.com/NVlabs/nvdiffrast>

**Modular Primitives for High-Performance Differentiable Rendering 논문**  
<https://arxiv.org/abs/2011.03277>

**nvdiffrast 공식 문서**  
<https://nvlabs.github.io/nvdiffrast/>

### 7.2 SIGGRAPH 실시간 렌더링 자료와 Nanite

**SIGGRAPH 2021 Advances in Real-Time Rendering 자료**  
<https://advances.realtimerendering.com/s2021/index.html>

**Unreal Engine Nanite 공식 문서**  
<https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-virtualized-geometry-in-unreal-engine>
