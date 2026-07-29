# Jungle USD 전용 CPU/GPU builder 구현 기록

작성일: 2026-07-29

## 범위

이 작업은 다음 두 기반 커밋 위에서 Jungle USD가 semantic
`SceneSourceData`에서 멈추지 않고 실제 renderer GPU upload까지 도달하게 만든다.

- `0f81997 Add raw OpenUSD Jungle scene inspection`
- `8619f26 Add Jungle USD SceneSource conversion`

기존 GLB Jungle builder를 복구하지 않았다. OpenUSD 의존성은 raw/converter/source
builder 구현에만 남기고 일반 `SceneSourceData`, `SceneCPUData`, renderer 헤더에는
노출하지 않았다.

최종 경로는 다음과 같다.

```text
SceneRawJungle
  -> SceneRawJungleToSource
  -> semantic SceneSourceData
  -> JungleSceneCPUBuilder의 명시적 legacy materialization
  -> SceneCPUBuilder
  -> JungleSceneGPUBuilder 전용 진입점
  -> BenchmarkSceneGPUBuilder 또는 DonutSceneGPUBuilder
  -> renderer GPU resource
```

semantic `SceneSourceData`는 USD 의미 정보의 보존 계층이고, triangle 기반 legacy
scene과 `SceneCPUData`는 Jungle 전용 materialization 단계의 별도 산출물이다.

## 구현 내용

### Source builder와 factory

`JungleSceneSourceBuilder`는 USD stage open과 `SceneRawJungleToSource` 변환을 묶는
Jungle 전용 source entry point다. public header에는 OpenUSD 타입이 없다.

`SceneSourceFactory` 선택 규칙은 다음과 같다.

- `scene_importer=jungle`: `JungleSceneSourceBuilder` 사용
- `scene_importer=auto`이며 확장자가 `.usd` 또는 `.usda`: Jungle builder 사용
- 그 외: 기존 Assimp 경로 유지

`TVBPerf`는 `SceneRawJungleToSource` target을 링크하도록 CMake를 연결했다.

### 명시적 CPU materialization

`JungleSceneCPUBuilder`는 semantic scene을 수정하거나 대체하지 않고, renderer가
소비할 수 있는 legacy triangle scene을 별도로 만든다.

Geometry 정책:

- USD polygon topology는 이 단계에서만 deterministic fan triangulation한다.
- face/vertex index의 32-bit 범위를 사전 검증하고 넘으면 fatal diagnostic으로
  실패한다.
- built-in normals와 `primvars:normals`를 semantic primvar로 보존한다.
- normals, tangents, UV0는 `constant`, `uniform`, `vertex`, `varying`,
  `faceVarying` interpolation을 처리한다.
- indexed primvar도 이 단계에서 명시적으로 resolve한다.
- 값 개수, index 또는 지원 범위가 맞지 않으면 silent fallback하지 않고
  `conversion_diagnostics`에 남긴다.

Material/texture 정책:

- generic USD shader node와 connection graph는 semantic scene에 그대로 보존한다.
- legacy renderer가 소비 가능한 PBR subset만 `source::Material`로 파생한다.
- authored asset path와 resolved asset path는 semantic data에 유지한다.
- legacy GPU texture path에는 실제로 존재하는 absolute resolved path만 넣는다.
- 지원하지 못한 shader 입력과 material 표현은 diagnostic으로 남긴다.

### Native instance와 PointInstancer

Native instance는 prototype geometry를 공유한다. PointInstancer prototype이 native
instance wrapper를 가리키는 중첩 구조도 최종 prototype까지 resolve하고 동일 mesh를
alias한다. 같은 material의 prototype primitive는 하나로 합쳐 submesh 수와
`geometry instance` 수의 불필요한 곱셈을 줄인다.

PointInstancer 정책:

- semantic `SceneSourceData`에서는 절대 확장하지 않는다.
- legacy/CPU 계약으로 내릴 때만 prototype별 instance range로 전부 확장한다.
- JungleRuins의 8,674,676 logical instance를 silent skip하거나 임의 cap하지 않는다.
- 전체 instance 수, draw instance 수, range offset과 32-bit 계약을 실제 할당 전에
  정확하게 preflight한다.
- overflow 또는 계약 위반은 명시적 fatal diagnostic으로 처리한다.
- inactive/invisible ID와 time sample처럼 legacy 계약이 완전히 표현하지 못하는
  의미는 diagnostic을 남기며 원본 semantic data는 유지한다.

PointInstancer prototype root transform에는 TRS로 분해할 수 없는 shear가 존재할 수
있다. Geometry에 root transform을 bake하면 정확하지만 native wrapper마다 geometry가
복제되어 실제 Jungle에서 vertex stream이 4 GiB를 넘었다. 이를 피하기 위해
`source::InstanceTransform`에 선택적 affine 4x4 matrix를 추가했다.

- 일반 instance는 기존 compact TRS를 사용한다.
- prototype root가 identity가 아니면 `prototype root * point TRS`를 정확한 affine
  matrix로 저장한다.
- `SceneCPUBuilder`는 matrix가 있으면 TRS 대신 matrix를 사용한다.
- 따라서 shear를 손실 없이 보존하면서 prototype mesh 공유도 유지한다.

### GPU entry와 renderer 연결

`JungleSceneGPUBuilder`는 Benchmark와 Donut 양쪽에 Jungle 전용 GPU 진입점을
제공한다. 현재 GPU buffer layout과 upload 구현은 기존 generic builder를 재사용하지만,
renderer는 반드시 이 전용 진입점을 거친다. 진입 전후에 vertex/index/instance/
draw-instance와 실제 upload 결과를 기록한다.

`RendererBenchmark`와 `RendererDonut`에는 Jungle source인 경우에만 적용되는 좁은
분기를 추가했다. pass와 shader에는 대규모 변경을 하지 않았다.

CPU cache는 현재 의도적으로 비활성 상태를 유지한다. 매 실행마다 source를 다시
생성하고 Jungle materialization을 수행한다.

## 실제 crash 분석과 수정

사용자 지시에 따라 crash에서 중단하지 않고 원인을 추적해 다음 순서로 해결했다.

1. 최초 Release 실행이 `0xc0000409`로 종료됐다. dump와 Windows `dbgeng` stack을
   확인해 `JungleSceneCPUBuilder::materialize`의 C++ 예외가 uncaught 상태로
   `terminate`된 것을 확인했다. `main`에 top-level exception logging을 추가해 이후
   materialization 오류가 즉시 flush되고 정상적인 실패 코드로 반환되게 했다.
2. 첫 실제 오류는 RiverSeedling PointInstancer prototype이 geometry prim이 아니라
   native-instance wrapper를 가리키는 경우였다. 중첩 native prototype resolve를
   구현해 최종 geometry를 공유하도록 수정했다.
3. prototype의 submesh 수와 8.67M instance가 곱해져 generic CPU/GPU instance
   메모리가 폭증했다. 같은 material의 prototype primitive compaction과 전체
   preflight count를 추가했다.
4. native wrapper geometry 중복으로 Donut vertex stream이 4 GiB를 넘었다. identity
   wrapper만이 아니라 모든 wrapper가 underlying prototype mesh를 alias하도록
   수정했다.
5. VFC를 끄고 전체 scene을 한 frame에 제출한 실행은 `Present`에서
   `0x887A0005`, device removed reason `0x887A0006`으로 TDR/device hung가 발생했다.
   이는 upload 실패가 아니라 8.67M instance/22.4M draw-instance 전체 제출 조건이다.
6. prototype root TRS를 단순 합성하는 과정에서 shear가 검출됐다. 정확한 geometry
   bake 실험은 `meshes=670`, `vertices=362,301,783`, `indices=512,886,399`까지
   중복되어 다시 4 GiB 제한에 도달했다. 최종적으로 선택적 affine instance matrix를
   도입해 정확성과 공유를 동시에 유지했다.

## 검증

### Build와 fixture

다음 build가 성공했다.

```powershell
cmake --build out/build/x64-Debug --config Debug
cmake --build out/build/x64-Release --config Release
```

작은 USD fixture는 quad/triangle triangulation, normals/UV, material graph 보존,
native instance 공유, nested prototype alias, PointInstancer 전체 확장, prototype
compaction과 affine matrix 경로를 검증한다.

```powershell
out/build/x64-Debug/JungleSceneCPUBuilderTest.exe
out/build/x64-Release/JungleSceneCPUBuilderTest.exe
```

두 configuration 모두 exit code 0으로 통과했다.

### 실제 JungleRuins CPU -> GPU 실행

최종 검증 명령은 다음 조건으로 실행했다.

```powershell
out/build/x64-Release/bin/Release/TVBPerf.exe `
  --run-id 9911 `
  --run-name jungle-gpu-final `
  --output-filepath out/jungle_gpu_smoke_9911.csv `
  --renderer-variant 7 `
  --to-use-scene true `
  --to-load-texture false `
  --use-vfc true `
  --scene-importer jungle `
  --scene-path assets/scenes/unpacked/JungleRuins/USD/JungleRuins_Karma.usda `
  --warmup-frames 1 `
  --measure-frames 1 `
  --auto-terminate true `
  --vsync false `
  --camera-mode 0 `
  --camera-filepath scripts/standard_camera.csv `
  --profile-window-frames 1 `
  --window-width 640 `
  --window-height 360 `
  --camera-near-z 0.1 `
  --camera-far-z 1000 `
  --camera-pos-x 0 --camera-pos-y 0 --camera-pos-z -10 `
  --camera-lookat-x 0 --camera-lookat-y 0 --camera-lookat-z 0
```

최종 materialization 결과:

```text
meshes=195
native_instances=741
expanded_point_instances=8674676
materialized_instances=8674818
draw_instances=22410338
shared_prototype_meshes=53
```

최종 Donut GPU upload 결과:

```text
vertices=46067121
indices=55924755
instances=8674818
draw_instances=22410338
vertex_bytes=1842684868
geometry_instances=22410338
```

GPU upload, warm-up, 측정 frame과 CSV 저장까지 완료됐고 로그에서 exception,
assert, device removed, shader/PSO compile 오류가 발견되지 않았다.

한 frame smoke 결과는 total `8.69226 ms`, geometry `8.65718 ms`, lighting
`0.01510 ms`, tonemap `0.00605 ms`였다. 이는 `(0, 0, -10)` 고정 free camera와
VFC를 사용해 실제 CPU->GPU 경로 도달만 확인한 값이며 Jungle 전체 성능 결론으로
사용하면 안 된다. Texture upload는 이 실행에서 `to_load_texture=false`였다.

## 변경 파일 역할

- `JungleSceneSourceBuilder.*`: OpenUSD Jungle source 전용 entry
- `JungleSceneCPUBuilder.*`: explicit polygon/material/instance materialization
- `JungleSceneGPUBuilder.*`: Benchmark/Donut 전용 GPU entry와 upload 계측
- `SceneSourceFactory.*`: explicit/auto Jungle 선택
- `SceneCPUCache.cpp`: Jungle CPU builder routing
- `SceneSourceHierarchy.h`, `SceneCPUBuilder.cpp`: optional affine instance matrix
- `RendererBenchmark.cpp`, `RendererDonut.cpp`: Jungle GPU entry routing
- `SceneRawJungleToSource.cpp`: normals와 `TexCoord2fArray` semantic 변환 보강
- `main.cpp`: uncaught exception의 확실한 기록과 종료 코드
- `JungleSceneCPUBuilderTest.cpp`, USD fixture: legacy materialization regression
- `CMakeLists.txt`: target/link/test wiring

## 남은 확장 과제

- 현재 GPU entry는 generic Benchmark/Donut layout을 재사용한다. 전용 GPU
  PointInstancer/prototype layout을 추가하면 8.67M CPU instance expansion과
  22.4M geometry-instance materialization을 피할 수 있다.
- 선택적 affine matrix로 정확성을 유지했지만 legacy instance 하나의 CPU 크기가
  커졌다. dedicated GPU compact affine/TRS stream이 필요하다.
- VFC를 끈 전체 Jungle 제출은 현 GPU에서 TDR을 일으킨다. GPU-driven culling,
  residency/streaming 또는 prototype-native draw 구조가 필요하다.
- Benchmark renderer의 Jungle 전용 entry는 build/fixture로 검증했지만 실제
  JungleRuins 실행은 Donut variant 7에서 수행했다.
- 실제 resolved texture upload, 더 넓은 USD shader graph subset, alpha/transmission
  등은 후속 과제다.
