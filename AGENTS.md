# AGENTS.md

이 파일은 이 저장소에서 AI 코딩 에이전트가 작업할 때 따라야 할 프로젝트별 지침이다. 저장소 루트 기준으로 적용한다.

## 프로젝트 목표

VisibilityBufferInfo는 Triangle Visibility Buffer(TVB)가 Forward / Deferred 렌더링보다 유리하거나 불리해지는 조건을 실험적으로 규명하는 Windows Direct3D 12 연구용 벤치마크다.

핵심 관심사는 다음이다.

- Forward의 overdraw 및 quad waste로 인한 불필요한 material/shading 비용
- Forward + depth pre-pass의 두 번째 geometry/rasterization 비용
- Deferred의 G-buffer read/write bandwidth 비용
- TVB visibility pass, fullscreen resolve, vertex/index/material indirect access, cache locality, barycentric/derivative 재구성 비용
- synthetic scene에서 얻은 모델이 Sponza/Bistro 같은 실제 scene에도 통하는지
- 남은 방향: 단순 비교보다 실제 scene 기반 visibility 고도화, 최소 albedo texture 포함, triangle-id 시각화, 발표용 그래프/영상 품질

## 먼저 읽을 문서

작업 시작 전 의도 파악이 필요하면 아래 순서로 읽는다.

1. [README.md](README.md) - 프로젝트 목적과 비교 알고리즘 개요
2. [docs/프롬프트.txt](docs/프롬프트.txt) - 연구 질문, 실험 설계, 성능 모델 관점
3. [docs/프롬프트_진행상황.txt](docs/프롬프트_진행상황.txt) - 누적 진행상황과 남은 항목
4. [docs/4주차.txt](docs/4주차.txt) - 최신 방향 메모. 실제 scene, visibility 고도화, 발표/영상 요구가 강함
5. [docs/TODO_pending.txt](docs/TODO_pending.txt) - 현재 짧은 TODO
6. Donut renderer나 실제 scene 구조를 건드릴 때는 [docs/donut deferred buffers.txt](<docs/donut deferred buffers.txt>)도 읽는다.

`docs/prev_*`는 과거 메모다. 연구 배경을 확인할 때는 유용하지만, 현재 구현 지침보다 우선하지 않는다.

## 코드 구조

- `src/`, `include/`: C++20 Direct3D 12 코드
- `assets/shaders/`: HLSL 셰이더. CMake가 빌드 출력의 `assets/shaders`로 복사한다. 셰이더 자체를 CMake에서 컴파일하지는 않는다.
- `assets/shaders/donut/`: NVIDIA Donut에서 가져온/수정한 셰이더. 라이선스 고지는 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)를 유지한다.
- `scripts/run.py`: 현재 실험 실행 진입점. 이전 `script_libs` 기능이 이 파일로 합쳐져 있다.
- `scripts/*.json`: 실험 spec. `base`, `sweep`, `samples`를 사용한다.
- `scripts/results/succeed`, `scripts/results/failed`: 보존된 연구 결과 번들. 임의 삭제/정리 금지.
- `datas/`: 과거/보조 실험 데이터와 리포트.
- `external/assimp`: 로컬 Assimp 소스. `.gitignore`상 `external/`은 보통 로컬 대용량/외부 의존성으로 취급된다.
- `out/`: CMake build output. 재생성 가능.
- `assets/scenes/`: 로컬 실제 scene asset. git에는 들어가지 않는 전제다.

## 빌드 환경

- Windows 전용 프로젝트다. Direct3D 12, DXGI, D3DCompiler, Win32 window를 사용한다.
- CMake 최소 버전은 3.20, C++ 표준은 C++20이다.
- Visual Studio/Ninja 흐름을 기본으로 본다. `CMakeSettings.json`에는 `x64-Debug`, `x64-Release`가 있다.
- DirectXTex는 CMake `FetchContent`로 `may2026` 태그를 받는다.
- Assimp는 `external/assimp`를 `add_subdirectory`로 빌드한다.

대표 명령:

```powershell
cmake --build out/build/x64-Debug --config Debug
cmake --build out/build/x64-Release --config Release
```

새로 configure가 필요한 경우 Visual Studio 개발자 환경 또는 MSVC 환경이 잡힌 shell에서 진행한다. 기존 `out/build/x64-*`가 있으면 우선 그 빌드 디렉터리를 사용한다.

## 실행 및 실험

단일 실행 파일은 보통 다음 위치에 있다.

```text
out/build/x64-Release/bin/Release/TVBPerf.exe
out/build/x64-Debug/bin/Debug/TVBPerf.exe
```

실험 sweep은 루트가 아니라 `scripts` 기준 상대 경로를 많이 쓴다.

```powershell
cd scripts
python run.py scene_geom_bistro_ex1.json
```

`scripts/run.py`의 정책:

- `ProgramArgument` dataclass를 기준으로 C++ 실행 인자를 만든다.
- JSON key에서 `-`는 `_`로 정규화한다.
- `_`로 시작하는 JSON key는 무시된다. 예: `_renderer_variant`
- `sweep`과 `samples`는 동시에 쓰지 않는다.
- 각 run은 임시 CSV를 만들고, 결과 CSV와 sidecar CSV를 `scripts/results/<experiment>/...`로 복사한다.
- stderr나 로그 경로가 있으면 return code가 0이어도 `salvaged`로 기록될 수 있다. 이는 데이터가 없는 실패와 다르다.
- playback mode에서는 C++가 `output_filepath_<run_id>_result.csv` 같은 windowed sidecar CSV를 추가로 만든다.

실험 spec을 수정할 때는 `executable`, `camera_filepath`, `scene_path`, `measure_frames`, `warmup_frames`, `profile_window_frames`, `use_vfc`, `to_load_texture`, `renderer_variant`를 특히 확인한다.

## Renderer Variant

현재 `src/render/renderer/RendererFactory.cpp` 기준 매핑:

- `1`: `RendererForward(false)` -> Forward
- `2`: `RendererForward(true)` -> ForwardPrepass
- `3`: `RendererDeferred(false)` -> Deferred
- `4`: `RendererVisBuf()` -> VisBuf
- `5`: `RendererDeferred(true)` -> DeferredPrepass
- `6`: `RendererVisBufGBuffer()` -> VisBuf + G-buffer
- `7`: `RendererDonutDeferred(false)` -> DonutDeferred
- `8`: `RendererDonutDeferred(true)` -> DonutDeferredPrepass

새 variant를 추가하면 다음을 함께 맞춘다.

- `RendererFactory.cpp`
- `ProgramArgumentValidator`의 `renderer_variant` 범위
- 실험 JSON의 sweep 목록
- 분석 스크립트/리포트에서 variant 이름 가정
- pass slot 이름과 CSV 컬럼 해석

## ProgramArgument 동기화 규칙

C++ 실행 인자는 `include/ProgramArgument.h`의 `ProgramArgument_MAC`가 기준이다. 인자를 추가/삭제/이름 변경할 때는 최소한 아래를 같이 확인한다.

- `include/ProgramArgument.h`
- `src/ProgramArgument.cpp`
- `src/util/ProgramArgumentValidator.cpp`
- `scripts/run.py` 안의 `ProgramArgument` dataclass와 `VALID_ARGUMENTS`
- 아직 남아 있는 `scripts/script_libs/models.py`가 실제로 쓰이는지 확인
- 기존 `scripts/*.json`과 `scripts/results/**/<experiment>.json`
- CSV header를 읽는 `PROGRAM_RESULT_FIELDS`

주의: 현재 C++ 기본값과 `scripts/run.py` 기본값이 일부 다르다. 예를 들어 scene 기본 경로, camera 기본값, output filename 등이 다를 수 있다. 기본값에 기대지 말고 실험 spec에서 의도한 값을 명시하는 편이 안전하다.

## 측정과 CSV

`RendererBase`가 전체 GPU timestamp를 slot 0에 기록하고, 각 renderer가 slot 1부터 pass별 timestamp를 수동으로 기록한다. `util::Constants::MAX_PASS_COUNT`는 31이고 결과 pass field는 총 32개다.

pass를 추가하거나 순서를 바꾸면 다음을 같이 맞춘다.

- `program_result_.pass_names[...]`
- `frame_time_.start_timestamp/end_timestamp` slot
- 분석 코드의 pass 이름 필터
- 기존 결과와 새 결과가 섞일 때 CSV schema가 어떻게 보일지

`FrameCounter`는 warm-up 이후 measurement frame만 모은다. playback mode에서는 windowed profile CSV도 생성하며, 최신 결과에는 frame별 index count profile도 들어갈 수 있다.

## Camera와 실제 Scene

`camera_mode`:

- `0`: free
- `1`: record
- `2`: playback

반복 가능한 벤치마크는 playback mode와 고정 camera CSV를 사용한다. 현재 대표 파일은 `scripts/standard_camera.csv`, `scripts/standard_camera_bistro.csv`다.

`to_set_start_frame`은 validator상 `camera_mode == 0`일 때만 허용된다. playback 실험에서 임의로 켜지 않는다.

`use_vfc`는 CPU view-frustum culling을 켜는 플래그다. `RendererBenchmark::render_prepare_`에서 카메라 frustum으로 batch를 다시 만들고 index count profile을 갱신한다. 실제 scene 비교에서 이 값이 바뀌면 geometry workload 자체가 달라진다.

## Benchmark Renderer 계층

Synthetic/benchmark 계열:

- Base: `RendererBase`
- Common benchmark setup: `RendererBenchmark`
- Scene loading: `scene::SceneLoader`
- CPU scene: `scene::SceneDataCPU`
- GPU upload: `scene::SceneResourceBuilder`
- Passes: `include/render/pass/benchmark`, `src/render/pass/benchmark`

Benchmark pass들은 `ResourceManagerFrame`, `ResourceManagerShader`, `ResourceManagerSampler`, `GPUResource`를 통해 descriptor와 resource state를 관리한다.

## Donut Renderer 계층

실제 scene/Donut 계열:

- Base: `RendererDonut`
- Current wired renderer: `RendererDonutDeferred`
- CPU scene: `scene::DonutSceneDataCPU`
- GPU scene: `scene::DonutSceneDataGPU`
- Importer: `DonutSceneAssimpImporter`
- GPU upload/layout: `DonutSceneResourceBuilder`
- Passes: `include/render/pass/donut`, `src/render/pass/donut`

Donut CPU/GPU scene 계약의 핵심:

- Node tree와 Instance/Submesh/GeometryInstance를 분리한다.
- global vertex/index/material/texture buffer를 유지한다.
- vertex data는 SoA/ByteAddressBuffer 성격이고 shader struct layout과 `static_assert`가 중요하다.
- visibility future contract는 draw/geometry instance id + triangle id 기반 resolve 방향이다.
- 현재 제외 범위: alpha blend, transmission, shadow renderer, IBL pipeline, SSAO, streaming, residency, GPU-driven culling, material binning.

`PassDonutVisibility`, `PassDonutVisGbuffer` 파일은 존재하지만 factory에 연결된 완성 renderer인지 먼저 확인하고 작업한다. 존재만 보고 실험 가능한 variant라고 가정하지 않는다.

## Descriptor와 Resource State 규칙

- resource state 전이는 `eng::GPUResource::transition`을 우선 사용한다.
- SRV/UAV desc 생성은 가능한 `eng::ResourceViewBuilder`를 사용한다.
- shader-visible descriptor 위치는 `ResourceManagerShader::EnumDescPos`에 고정되어 있다. 새 descriptor를 추가할 때 Donut 영역과 benchmark 영역이 충돌하지 않게 enum 값과 heap count를 확인한다.
- RTV/DSV 위치는 `ResourceManagerFrame::EnumRTV`, `EnumDSV`를 사용한다.
- sampler 위치는 `ResourceManagerSampler`를 사용한다.
- root signature는 `RootSignatureBuilder` 패턴을 따른다.
- pass 내부 root parameter enum 순서와 실제 root signature build 순서는 반드시 일치해야 한다.
- descriptor heap binding을 pass 밖으로 옮기는 TODO가 있다. 관련 수정은 모든 pass의 `SetDescriptorHeaps` 호출과 root descriptor table 사용을 함께 검토한다.

## HLSL 규칙

- benchmark shader는 `assets/shaders/*.hlsl`, 공통 include는 `assets/shaders/common_*`.
- Donut shader는 `assets/shaders/donut/` 아래에 둔다.
- C++ struct와 HLSL layout이 맞아야 한다. `sizeof`, `offsetof`, register space, descriptor table register를 같이 확인한다.
- shader path는 runtime working directory 기준 `assets/shaders/...`다. CMake가 `assets/shaders`만 output으로 복사하므로 다른 asset 자동 복사를 기대하지 않는다.

## Scene/Texture 규칙

- `assets/scenes/`는 gitignore 대상이다. scene asset이 없을 수 있으므로 실제 scene 실행 실패를 코드 실패로 단정하지 않는다.
- `to_load_texture=false`이면 실제 material texture 대신 dummy/fallback texture 흐름으로 간다.
- texture loading을 건드릴 때는 sRGB/linear 색공간, embedded texture, fallback texture, unsupported format logging을 함께 본다.
- TODO에 embedded texture loading이 있었고 일부 로직은 아직 fallback에 의존할 수 있다.

## 실험 데이터 보존

`scripts/results/succeed`, `scripts/results/failed`, `datas/`의 CSV/PNG/HTML/JSON/리포트는 연구 증거물이다. 요청 없이 삭제, 압축 해제 재배치, 포맷 변경, 대량 재생성하지 않는다.

재생성 가능한 로컬 산출물:

- `out/`
- `.vs/`
- `logs/`
- `scripts/results/default/`
- Python `__pycache__/`

위 산출물도 사용자가 방금 실행 중일 수 있으므로 정리 작업은 명시 요청이 있을 때만 한다.

## 코딩 스타일

- 기존 C++ 스타일을 따른다: 작은 helper class, 명시적 `init`, `close`, `render_prepare_`, `render_record_`, `init2_` 흐름.
- `Microsoft::WRL::ComPtr`와 RAII wrapper를 우선 사용한다.
- D3D12 실패는 `util::Utils::throw_if_failed`, 논리 검증은 `util::Logger::g_logger.assert_with_log` 패턴을 따른다.
- Windows `min`/`max` macro 충돌은 `util/minmax_remover.h`나 기존 패턴을 따른다.
- 새 abstraction은 pass/renderer/resource manager 경계를 흐리지 않을 때만 만든다.
- 대규모 refactor보다 실험 목적에 필요한 좁은 변경을 선호한다.
- 외부 Donut-derived shader/code를 옮기거나 크게 수정하면 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)를 훼손하지 않는다.

## 검증 체크리스트

코드 변경 후 가능한 범위에서 다음을 수행한다.

1. `git status --short`로 의도하지 않은 파일 변경 확인
2. C++ 변경: 관련 config 빌드
   ```powershell
   cmake --build out/build/x64-Debug --config Debug
   ```
3. Release 성능 관련 변경: 가능하면 Release도 빌드
   ```powershell
   cmake --build out/build/x64-Release --config Release
   ```
4. 실험 runner 변경: 작은 JSON 또는 기존 spec의 축소판으로 `python scripts/run.py <spec>` 실행
5. ProgramArgument 변경: unknown key 검증, C++ header CSV, Python `PROGRAM_RESULT_FIELDS` 확인
6. Renderer/pass 변경: pass name, timestamp slot, resource transition, descriptor heap size 확인
7. 실제 scene 변경: scene asset 부재 가능성을 분리해서 보고하고, asset이 있을 때 camera playback으로 최소 실행 확인

검증을 실행하지 못했으면 최종 응답에 이유를 짧게 남긴다.

## AI 작업 원칙

- 연구 질문과 측정 공정성을 코드 편의보다 우선한다.
- benchmark 결과를 해석할 때는 pass별 시간, total 시간, warm-up/measure frame, VSync, resolution, camera path, VFC, texture loading 여부를 함께 언급한다.
- 성능 결론을 새로 쓰려면 기존 결과 CSV/리포트를 먼저 확인한다. 그래프만 보고 결론을 만들지 않는다.
- 실제 scene 실험은 "보이는 화면/영상"과 연결될 필요가 있다. 발표용 작업에서는 capture, albedo texture, triangle-id visualization 같은 시각 자료를 우선순위에 둔다.
- 불확실한 하드웨어 병목은 가설로 표시하고, Nsight/PIX/hardware counter가 없으면 단정하지 않는다.
