# JungleRuins / Donut 로딩 감사

기준일: 2026-07-31

대상: `assets/scenes/unpacked/JungleRuins`
목표: Jungle 전용 visibility-buffer renderer를 설계하기 전에 원본 Blender 자산, USD, 현재 C++ Donut 경로가 보존하는 정보와 손실하는 정보를 분리해 기록한다.

## 결론

현재 구현은 **Blender `.blend`를 직접 로드하지 않는다.** 런타임 입력은
`USD/JungleRuins_Karma.usda`가 compose하는 USD stage이다. 따라서 아래 두 문장을 구분해야 한다.

| 질문 | 판정 | 근거 |
|---|---|---|
| USD stage의 구조·원시 의미 정보를 C++에서 추적 가능한가? | 대체로 예 | `SceneRawJungle`이 로드된 USD stage를 소유하고, semantic `SceneSourceData`가 mesh polygon topology, primvar, material graph, native instance, PointInstancer, camera/light/asset reference와 diagnostic을 보존한다. |
| Blender 원본의 모든 authoring 정보를 현재 Donut GPU renderer가 그대로 재현하는가? | 아니오 | Blender-only Geometry Nodes/collection authoring 및 USD exporter의 표현 범위는 런타임 입력 밖이다. 또한 runtime vertex/material/instance ABI는 의도적으로 PBR subset으로 축소된다. |
| 8,674,676개 scatter transform을 무음 삭제하거나 duplicate mesh로 확장하는가? | 아니오 | compact Donut 경로가 prototype geometry를 공유하고 `PointInstance`/prototype-range stream으로 보낸다. 32-bit 한계 초과는 silent cap 대신 fatal diagnostic이다. |
| foliage의 dithered/blend 외관, transmission, 모든 material graph를 현 renderer가 동일하게 처리하는가? | 아니오 | Blender material 317개 모두 `DITHERED`이고, runtime은 non-opaque를 alpha-tested domain으로 합친다. 완전한 blend sorting, Blender procedural graph, transmission/opacity texture shader 사용은 아직 없다. |

그러므로 현 상태는 **Jungle 전용 renderer의 source/instance 데이터 기반으로는 충분하지만, 최종 화면 품질의 완전한 Blender 동등성은 아직 보장하지 않는다.** 다음 구현은 source semantic을 더 축소하지 말고, foliage alpha/transmission과 material graph subset을 확장해야 한다.

## 검사 기준과 재현성

- 원본 main file: `Blender/JungleRuins_Main.blend` (1,022,071,588 B, SHA-256 `55339E29EFA2382793BA670A365B6F9624695A45B0906158AC4043A0A498D876`)
- USD entry layer: `USD/JungleRuins_Karma.usda` (1,908 B, SHA-256 `26FB58603B7A0695A371B5D7E8D14BFAF2FAD6FBDA235B0E28C449772DC250A8`)
- Blender 4.2.23 LTS background load로 main file과 연결된 12개 foliage library를 실제로 열었다. 링크 해석 실패는 없었다.
- 기존 machine report: `assets/scenes/generated/jungle/reports/source_inventory.json` (Blender 4.2.23 / OpenUSD 0.24.5).
- C++ regression: 2026-07-31에 `JungleSceneCPUBuilderTest`가 Debug 1.46 s, Release 0.46 s로 각각 통과했다.

원본 package는 324 files, 7,435,345,046 B (약 6.93 GiB)이다. `Blender/` 14개 `.blend`는 약 1.62 GiB, USD layers는 약 1.78 GiB, `textures/`는 263 files / 3,717,362,244 B (약 3.46 GiB)이다. 원본은 수정하지 않았다.

## Blender 원본 구조

### 컬렉션과 공간 분할

Blender가 연 main file에는 355 object가 있다: 342 mesh, 12 linked-library empty, 1 camera. 상위 collection은 다음과 같다.

```text
Global                         # Camera, Creek, River, Pyramid shell, Banyan hero
GScatter
  Sources                      # 11 linked prototype collections
  Systems                      # 205 authored scatter systems
Terrain-1/8 Precision
  Main                         # 16 cinematic terrain cells
  Extended                     # 64 extended terrain cells
12 linked foliage collections  # Anthurium, Grass, Moss, Nettle, Shrub, tree families
```

Terrain은 총 80 cell이다.

- cinematic: `M_3x3_01` ~ `M_4x4_04`, 4 x 4, 16 cells. 각 cell은 대략 250 x 250 Blender units이다.
- extended: `E_01` ~ `E_64`, 8 x 8, 64 cells. 각 cell은 대략 1000 x 1000 Blender units이다.
- cinematic cell은 extended 중앙 영역과 겹친다. 지금은 duplicate라고 가정하거나 제거하면 안 된다.

Scatter system은 cinematic 75, extended 128, pyramid 2로 총 205개다. species별 system 수는 Queen Forest 80, River Forest 80, River Sapling 16, River Seedling 16, Grass 5, Anthurium 2, Nettle 2, ShrubSorrel 2, Moss/Shrub 각 1이다. source prototype collection은 11개이며, USD가 참조하는 unique prototype name은 53개다.

### 기하량과 정점 속성

Blender datablock 기준 342 mesh에는 총 9,540,460 vertices, 17,530,719 edges, 8,411,775 polygons, 32,315,870 face corners(loops)가 있다. 이는 instance를 복제하지 않은 authoring/prototype geometry 집계다.

원본 mesh attribute에서 확인된 렌더링 관련 attribute는 다음과 같다.

| 속성 계열 | Blender domain / 형식 | 비고 |
|---|---|---|
| position | point / `FLOAT_VECTOR` | 기본 geometry position |
| normal | mesh normal 및 USD `normals` | semantic loader가 built-in normal을 primvar처럼 보존 |
| UV | corner / `FLOAT2`: `UVMap`, `UV1`, `map1`, `uv` | 여러 명칭과 UV1이 존재 |
| vertex color | corner `BYTE_COLOR` (`Col`), point `FLOAT_COLOR` (`Color`) | 현재 runtime vertex ABI에는 없음 |
| topology/edit metadata | corner/edge/face selection, seam, sharp, crease, sculpt attribute | source authoring 정보이며 renderer 입력이 아님 |

현재 `SceneCPUData::Vertex`와 Donut GPU vertex ABI는 position, normal, tangent, UV0만 사용한다. Semantic stage는 extra primvar를 value/interpolation/index/source reference와 함께 남기며, CPU materialization이 UV1/color 등 비지원 primvar를 발견하면 `unmaterialized_primvar` diagnostic을 낸다. 즉 source 추적은 보존되지만 final runtime shading은 UV1/vertex color를 읽지 않는다.

### 텍스처

수량은 정의를 분리해서 본다.

| 범위 | 수량 | 압축 파일 크기 | 설명 |
|---|---:|---:|---|
| package `textures/` | 263 files | 3,717,362,244 B / 3.46 GiB | HDR과 source package의 미참조/보조 파일까지 포함 |
| Blender image datablock | 275 | - | 270 external resolved, 5 generated/empty image (`Metallic`, `Roughness`, `Render Result`, `River_BaseColor`, `UV_Checker_Terrain_Extended`) |
| unique externally referenced surface image | 248 paths | 3,024,476,372 B / 2.82 GiB | Blender material에서 실제 참조한 unique path |

형식은 Blender image datablock 기준 JPG 158, PNG 100, TIFF 10, EXR 2, filepath 없는 generated/empty 5다. 해상도는 2K 178, 4K 73, 1K 13, 8K 8, 32px 2, image-less 1이다. colorspace는 Non-Color 151, sRGB 123, unset 1이고, alpha mode는 straight 265, channel-packed 8, premultiplied 2다. 따라서 texture upload는 색공간과 alpha를 material slot별로 명시해야 하며, 단순히 모두 sRGB로 올리면 안 된다.

## 머티리얼

Blender datablock에는 material 317개가 있고 111개가 node-based이다. 사용 가능한 mesh material slot의 unique material은 105개다. 전체 material은 `DITHERED`, backface culling off로 기록되어 있다. node graph의 실제 node 수는 Principled BSDF 111, Material Output 111, Image Texture 462, Normal Map 106, Frame 135다. foliage에는 two-sided/translucent 명명과 alpha texture가 많다.

USD compose 결과는 material prim 134, shader prim 674, mesh material subset 37을 가진다. Blender에서 같은 material처럼 보이는 데이터가 USD export에서 prototype/local material로 공유 또는 통합되므로, `317 != 134`는 C++ loader 손실 수치가 아니라 source representation의 identity granularity 차이다. 앞으로 stable source ID와 source prim path를 함께 보존해야 한다.

현재 material 처리 계층은 다음과 같다.

```text
USD generic shader nodes + connections             # semantic: 보존
  -> UsdPreviewSurface / standard_surface PBR subset
  -> SceneCPUData material
  -> Donut material constants / texture descriptor
```

Runtime GPU material은 base-color, metallic-roughness, normal, emissive, occlusion 다섯 texture slot과 scalar PBR 값만 실제 pixel shading에 사용한다. Source `Material` 구조에는 transmission texture/ref도 있으나, legacy `SceneCPUData::Material`에는 transmission/opacity texture가 없고, Donut renderer는 alpha mode `Mask`와 `Blend`를 모두 alpha-tested domain으로 처리한다. 따라서 다음은 후속 구현 항목이다.

- alpha blend/dithered foliage의 정확한 coverage/ordering policy;
- opacity와 transmission texture, leaf translucency/SSS;
- UV1, vertex color, texture transform 및 Blender-specific graph nodes;
- EXR decoder: `palm_bark_nor_gl_4k.exr`는 현재 DirectXTex 경로에서 fallback texture로 대체된 기록이 있다.

## USD와 C++ scene 구조

USD stage는 Z-up, `metersPerUnit = 0.01`이며 3,429 prim으로 compose된다. 주요 타입은 PointInstancer 778, Mesh 121, Material 134, Shader 674, Xform 866, GeomSubset 37, Camera 1, DomeLight 1이다. logical PointInstancer transform은 8,674,676개이며, exact-origin record 197개는 아직 exporter sentinel인지 확정되지 않아 보존한다.

```text
Blender/JungleRuins_Main.blend + linked .blend files
            |  (외부 Blender -> USD export; C++ 범위 밖)
            v
USD/JungleRuins_Karma.usda + referenced layers/assets
            v
SceneRawJungle
  - loaded USD stage, layer/property/asset/variant/diagnostic inventory
            v
SceneRawJungleToSource -> SceneSourceData
  - nodes and transforms, PolygonMesh + all supported primvars
  - material graphs/nodes/connections/bindings/assets
  - native prototypes/instances, PointInstancer arrays, camera/light
            v
JungleSceneCPUBuilder::build_compact
  - shared ordinary geometry + 48 B PointInstance records
  - prototype-local affine and instancer world matrix
  - prototype-grouped logical instance ID ranges
            v
JungleSceneGPUBuilder::build_donut_compact
  - Donut vertex/index/submesh/material buffers
  - PointInstance, prototype matrix, visible-ID buffers
```

`SceneRawJungle`은 raw USD document 자체를 유지하므로, semantic converter가 아직 소비하지 않는 USD property도 stage에서 재질의 가능하다. `SceneSourceData`에는 polygon topology, numeric/serialized primvar, material graph, shader input/output/connection, resolved asset path, native instance, PointInstancer velocity/angular velocity/ID/invisible/inactive ID가 별도로 있다. 즉 **semantic source의 미지원 값은 삭제 대신 diagnostic 또는 raw-stage 경로로 남긴다.**

반면 conversion은 선택한 evaluated time code 한 장면을 materialize한다. time-varying instance/geometry, inactive/invisible ID, unsupported tangent/UV, unsupported PBR graph는 semantic data에 남되 current GPU draw ABI에 모두 반영되지는 않는다.

## 인스턴스와 draw 구조

PointInstancer를 legacy 방식으로 모두 instance/submesh로 확장하면 8,674,818 materialized instance와 22,410,338 draw instance가 된다. 이는 all-visible draw에서 TDR을 일으킨 원인이다. compact 경로의 실제 Jungle 수치는 다음과 같다.

| 항목 | 값 |
|---|---:|
| ordinary/native instance | 142 |
| ordinary generic draw instance | 162 |
| logical point instance | 8,674,676 |
| PointInstancer source / point prototype group | 778 |
| PointInstance stream | 416,384,448 B |
| prototype-grouped ID stream | 34,698,704 B |
| prototype affine matrix stream | 99,584 B |
| compact point GPU data | 451,182,736 B / 약 430 MiB |

각 compact point record는 translation, quaternion rotation, scale, source index를 가지며 48 B이다. prototype root에 shear가 있으면 mesh를 bake/duplicate하지 않고 affine 4x4 matrix를 추가해 `prototype affine -> point TRS -> instancer world` 순서로 적용한다. 이 결정이 vertex duplication 없이 authoring transform을 보존하는 핵심이다.

VFC off는 prototype의 full logical range를 `DrawIndexedInstanced`로 그린다. VFC on은 prototype group별 conservative transformed bounding sphere로 CPU에서 visible ID stream만 새로 만든다. 이 구조는 Jungle 전용 visibility renderer에서도 유지해야 하며, 다음 단계는 CPU traversal을 GPU culling/indirect draw로 옮기는 것이다.

## Jungle 전용 visibility-buffer renderer의 설계 제약

1. source semantic과 compact runtime을 별개 계층으로 유지한다. renderer 최적화를 위해 `SceneSourceData`의 stable ID, material graph, raw source reference를 버리지 않는다.
2. visibility target에는 `prototype/draw ID + triangle ID`가 필요하다. resolve는 draw/prototype -> submesh -> material -> index triple -> shared vertex attribute -> bindless texture 순서로 복원한다.
3. point instance logical ID는 prototype-local contiguous range를 사용하고, per-frame visible-ID indirection을 별도 buffer로 둔다. 8.67M transform을 submesh마다 복제하지 않는다.
4. terrain cell, scatter system, prototype을 culling/streaming 단위로 유지한다. cinematic와 extended terrain 겹침은 시각 검증 전 제거하지 않는다.
5. foliage는 opaque geometry와 분리해 alpha test/dithered coverage를 지원한다. blend/transmission은 unsupported로 조용히 opaque 처리하지 말고 별도 material domain과 diagnostic을 둔다.
6. texture residency는 최소 2.82 GiB의 referenced payload를 전부 고정 resident로 가정하지 않는다. path, colorspace, usage slot, decode status, fallback 여부를 asset registry에서 기록한다.
7. source-to-runtime count는 build마다 report한다: mesh/prototype/material/image, PointInstancer transform, culled visible transform, draw call, triangle, texture upload/fallback 수.

## 근거 코드와 문서

- Raw USD ownership/inventory: `include/scene/raw/SceneRawJungle.h`, `src/scene/raw/SceneRawJungle.cpp`
- USD semantic conversion: `src/scene/builder/source/SceneRawJungleToSource.cpp`
- semantic data contract: `include/scene/data/source/SceneSourceData.h`, `SceneSourceSemantic.h`, `SceneSourceGeometry.h`, `SceneSourceMaterial.h`
- compact materialization: `src/scene/builder/cpu/JungleSceneCPUBuilder.cpp`
- Donut runtime ABI: `include/scene/data/cpu/SceneCPUData.h`, `include/scene/data/gpu/DonutSceneGPUData.h`
- compact GPU upload: `src/scene/builder/gpu/JungleSceneGPUBuilder.cpp`
- canonical source contract and inventory: `docs/jungle/SCENE_CONTRACT.md`, `docs/jungle/SOURCE_INVENTORY.md`, `docs/jungle/CANONICAL_SCENE_MANIFEST.md`
