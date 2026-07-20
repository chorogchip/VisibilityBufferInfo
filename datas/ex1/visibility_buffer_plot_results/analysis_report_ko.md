# VisibilityBufferInfo 카메라 경로 GPU 시간 분석

## 데이터 검증

- 6개 renderer variant를 읽었다.
- 각 파일은 133개 측정 지점을 가진다.
- frame은 0부터 1320까지 10 간격이며 모든 파일에서 정확히 일치한다.
- 결측 timing 값은 없다.
- CSV에 total 열이 없으므로 각 파일의 pass 시간을 합산해 total GPU time을 계산했다.
- CSV 자체에는 시간 단위 metadata가 없다. 프로젝트 맥락을 근거로 그래프에는 ms를 사용했다. 실제 logger 단위가 다르면 그래프 label만 수정해야 한다.

## 평균 total GPU time

| 순위 | Renderer | 평균 | Forward 대비 |
|---:|---|---:|---:|
| 1 | Forward | 0.27749 | 기준 |
| 2 | Deferred | 0.29064 | +4.74% |
| 3 | TVB | 0.35775 | +28.93% |
| 4 | TVB + G-buffer | 0.37469 | +35.03% |
| 5 | Forward + Prepass | 0.44742 | +61.24% |
| 6 | Deferred + Prepass | 0.49074 | +76.85% |

Forward가 133개 측정 지점 중 127개에서 가장 빨랐다. Deferred가 더 빠른 지점은 frame 100, 310, 570, 790, 1070, 1270의 6개다. 이 6개 지점에서 Deferred의 Forward 대비 우위는 약 1.2%에서 5.9%다.

TVB는 이 실행에서 Forward 또는 Deferred보다 빠른 측정 지점이 없었다. 평균 기준으로 Forward보다 28.93%, Deferred보다 23.13% 느리다.

## Pass 수준 결과

- TVB: visibility 0.31535, resolve 0.04240
- TVB total 중 visibility 비중: 88.15%
- TVB + G-buffer: visibility 0.32642, G-buffer 0.04102, lighting 0.00723
- Deferred: geometry 0.28152, lighting 0.00912
- Forward + Prepass: depth prepass 0.19859, 이후 Forward pass 0.24882
- Deferred + Prepass: depth prepass 0.22363, geometry 0.25808, lighting 0.00904

현재 경로에서 TVB의 핵심 병목은 resolve가 아니라 visibility pass다. TVB resolve는 평균 0.04240이지만 visibility 기록이 0.31535이므로, final visible sample 중심의 shading 구조가 얻는 이점보다 visibility pass 자체의 비용이 더 크게 나타났다.

Prepass는 두 renderer 모두 후속 shading/geometry pass를 줄였지만 prepass 비용을 회수하지 못했다.

- Forward pass 감소: 0.27749 → 0.24882, 약 0.02867 절감
- 추가 depth prepass: 0.19859
- Deferred geometry 감소: 0.28152 → 0.25808, 약 0.02344 절감
- 추가 depth prepass: 0.22363

따라서 이 카메라 경로에서는 overdraw 절감량이 prepass의 추가 geometry와 rasterization 비용보다 훨씬 작다.

TVB direct와 TVB + G-buffer의 total 평균 차이는 0.01694지만, 두 실행의 visibility pass 평균도 0.01107 차이 난다. 그러므로 total 차이 전체를 G-buffer 생성·lighting 비용으로 해석하면 안 된다. 공통 pass의 실행 간 편차를 제거하려면 repeat와 run-order 교차 실행이 필요하다.

## 프레임 변화와 상관관계

- TVB와 TVB + G-buffer의 frame별 total 상관계수는 약 0.965로 매우 높다. 카메라 경로에 따른 geometry/visibility 변화에 비슷하게 반응한다.
- Forward와 Deferred의 상관계수는 약 0.726이다.
- 일부 renderer에서 짧은 spike가 관찰되지만 repeat가 없으므로 scene-dependent spike인지 GPU clock·background load·timestamp noise인지 구분할 수 없다.

## 해석 범위

이 결과는 현재 scene, camera path, resolution, material workload, renderer 구현에 대한 결과다. Synthetic 실험의 crossover 결론을 부정하지 않으며, 현재 실제 장면 경로가 Forward가 유리한 영역에 있다는 것을 보여준다.

신뢰도 높은 실제 scene 비교를 위해 다음 측정에서는 같은 경로를 renderer별로 여러 번 반복하고 실행 순서를 섞는 것이 중요하다. 평균만 비교하지 말고 frame별 median, P95, repeat 간 분산, 동일 frame의 paired difference를 함께 사용해야 한다.
