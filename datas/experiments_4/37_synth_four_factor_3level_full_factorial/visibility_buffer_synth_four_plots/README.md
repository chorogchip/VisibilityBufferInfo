# VisibilityBufferInfo experiments 37/38 — plot bundle

## 핵심 결과

- 총 318개 성공 run을 사용했습니다: full factorial 162개, dense sweeps 156개.
- 중앙 조건에서 variant 8은 **0.06210 ms**, variant 9는 **0.11641 ms**로, variant 9가 **1.875×**입니다.
- geometry division 8→128에서 variant 8은 **+13.35%**, variant 9는 **+3.17%** 변했습니다. renderer 비율은 **1.980× → 1.802×**로 감소합니다.
- full factorial의 주효과 범위는 geometry division이 가장 큽니다: variant 8 **12.49%**, variant 9 **3.15%**.
- `source_material_count=1`, `active_material_bin_count=1`, `material_bin_compaction_ratio=1.0`이 모든 run에서 고정입니다. 따라서 max-open/locality/diversity의 평탄한 결과는 재료 bin 알고리즘의 무관성을 뜻하기보다, 현재 장면이 해당 축을 실제로 활성화하지 못했다는 진단으로 보는 편이 안전합니다.
- dense sweep에 반복 삽입된 중앙 조건의 범위는 variant 8 **0.403%p**, variant 9 **0.232%p**입니다. 각 비중앙 조건은 repeat=1이므로 작은 요동은 효과보다 측정 잡음일 수 있습니다.
- max-open=2의 variant 8 값은 나머지 max-open 지점 중앙값보다 **+5.86%** 높아 고립된 이상점으로 보입니다. repeat=1이라 재실행 전에는 max-open 효과로 해석하지 않는 편이 안전합니다.

## 파일 구성

- `00_key_results`: 핵심 결과 4-panel 요약
- `01_dense_sweeps_absolute`: 네 축의 절대 total time
- `02_dense_sweeps_normalized`: 중앙 조건 대비 민감도
- `03_dense_sweeps_renderer_ratio`: variant 9 / variant 8 비율
- `04_full_factorial_main_effects`: 3수준 완전요인 주효과
- `05_full_factorial_pairwise_interactions`: pairwise interaction heatmap
- `06_geometry_pass_breakdown`: geometry sweep의 pass별 시간
- `07_full_factorial_effect_sizes`: factor 민감도 순위
- `08_dense_baseline_repeatability`: 네 sweep에 반복된 중앙 조건의 변동
- `*.csv`: plotting에 사용한 정리된 수치
- `plot_synth_four.py`: ZIP/CSV에서 전체 결과를 재생성하는 스크립트

## 실행

```bash
python plot_synth_four.py 37_synth_four_factor_3level_full_factorial.zip 38_synth_four_one_dimensional_dense_sweeps.zip --output-dir synth_four_plots
```

PNG와 SVG를 모두 생성합니다.