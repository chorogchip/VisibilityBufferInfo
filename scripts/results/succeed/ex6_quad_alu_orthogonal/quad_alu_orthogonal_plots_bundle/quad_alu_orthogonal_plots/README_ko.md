# Quad × ALU 실험 결과 요약

## 입력과 검증

- 성공 행: 363개 = waste-quad 11단계 × ALU 11단계 × 렌더러 3종
- 실패 행: 0개
- 각 격자점의 runner repeat: 1회
- total과 활성 pass 합의 최대 오차: `1.0e-5 ms` 이하
- 비교 렌더러: Forward(1), ForwardPrepass(2), VisBuf(4)

## 가장 빠른 렌더러

121개 측정점 중:

- Forward: 77개
- ForwardPrepass: 0개
- VisBuf: 44개

ForwardPrepass는 `Q=64, ALU≥96`의 세 점에서 Forward보다 약간 빨랐지만, 같은 점에서 VisBuf가 더 빨라 최종 winner가 되지는 못했다.

## Forward → VisBuf crossover

아래 값은 각 waste-quad 샘플에서 VisBuf가 Forward보다 처음 빨라진 **측정된 최소 ALU 값**이다. 샘플 사이의 정확한 경계는 contour 보간값이므로 별도 측정으로 재확인해야 한다.

| Waste quad Q | 최소 ALU |
|---:|---:|
| 8 | 192에서도 미발생 |
| 12 | 192에서도 미발생 |
| 16 | 192에서도 미발생 |
| 24 | 128 |
| 32 | 64 |
| 48 | 32 |
| 64 | 32 |
| 96 | 16 |
| 128 | 8 |
| 192 | 8 |
| 384 | 4 |

즉 이 데이터에서는 Q가 커질수록 VisBuf가 유리해지는 데 필요한 material/ALU 비용이 빠르게 낮아진다.

## 대표 수치

- 최대 VisBuf speedup vs Forward: `2.270×` at `Q=384, ALU=192`
  - Forward: `4.46064 ms`
  - VisBuf: `1.96467 ms`
- 가장 근소한 winner: `Q=64, ALU=16`
  - Forward: `0.24879 ms`
  - VisBuf: `0.24901 ms`
  - 차이: 약 `0.088%`; 사실상 crossover 인접점이므로 반복 측정이 필요하다.
- 최대 winner margin: `2.270×`

## Q축 해석 주의

CSV의 `variable-waste-quad-count`는 실제로 `geometry-div` 값과 동일하다. 동시에 object count가 역제곱으로 변하며 모든 샘플에서 다음 값이 일정하다.

`object_count × geometry_div² = 147456`

예를 들어 Q=8에서는 object count가 2304이고, Q=384에서는 object count가 1이다. 따라서 이 축은 순수한 “폐기 quad 개수 하나만” 바꾸는 축이라기보다, **총 geometry를 고정한 상태에서 subdivision/object 배치를 바꾸는 quad-waste proxy**로 해석해야 한다.

## 경험적 모델

raw Q에 대한 단순 선형식은 이 실험 설계의 역제곱 object-count 변화를 잘 표현하지 못한다. 따라서 다음 reciprocal basis를 사용했다.

`T = β0 + βQ1/Q + βQ2/Q² + βA·A + βQA1·A/Q + βQA2·A/Q²`

적합 결과:

| Variant | R² | RMSE (ms) | MAPE |
|---|---:|---:|---:|
| Forward | 0.956939 | 0.176124 | 9.800% |
| ForwardPrepass | 0.960583 | 0.184261 | 9.708% |
| VisBuf | 0.999775 | 0.009819 | 2.972% |

Forward/ForwardPrepass의 오차가 VisBuf보다 큰 것은 해당 두 경로가 Q 변화에 따라 단순 reciprocal 항만으로 포착되지 않는 비용 변화를 더 포함한다는 신호다. 이 식은 현재 HW·해상도·렌더러·씬 구성에 대한 경험식이며 외삽용 공식으로 사용하면 안 된다.

## 보고서에서 사용할 핵심 결론

1. 낮은 ALU와 낮은 Q에서는 Forward가 우세하다.
2. Q와 ALU가 증가할수록 crossover가 발생하며 VisBuf의 우세 영역이 넓어진다.
3. ForwardPrepass는 이번 격자에서는 winner가 아니었다. quad 낭비를 줄이기 위해 두 번째 geometry/raster pass를 지불하는 비용이 상쇄되지 않았다.
4. `Q=64, ALU=16`처럼 차이가 0.1% 미만인 점이 있으므로 crossover 경계 부근은 반복 측정과 더 촘촘한 ALU 샘플링이 필요하다.
5. Q축은 object count와 함께 변하므로, 결과를 “quad waste만의 독립 효과”로 단정하지 말고 현재 실험 설계의 proxy 효과로 기술해야 한다.
