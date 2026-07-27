# VisibilityBufferInfo 그래프 스크립트

대화에서 사용한 CSV 병합 및 그래프 생성 코드를 독립 실행형으로 정리한 묶음이다.
모든 스크립트는 같은 디렉터리의 `common.py`를 사용한다.

## 설치

```powershell
python -m pip install -r requirements.txt
```

## 포함 파일

- `01_merge_result_csvs.py`: `run_*.csv_*_result.csv` 병합
- `02_sponza_full_analysis.py`: Sponza 기본 분석 및 요약 plot/CSV
- `03_pass_by_frame_plots.py`: renderer/ALU별 pass 시간
- `04_median_pass_breakdown.py`: 선택 ALU median pass breakdown
- `05_selected_pass_lines.py`: 요청한 pass만 renderer/ALU별 출력
- `06_selected_pass_contact_sheet.py`: PNG contact sheet 생성
- `07_selected_pass_single_plot.py`: 선택 pass를 하나의 plot에 통합
- `08_selected_11_plots.py`: ALU 1/2/4 및 10/20 조합의 11장 생성
- `09_bistro_total_time.py`: Bistro ALU 1/5/10/20 total time 4장
- `10_bistro_alu20_raster_stats.py`: ALU 20 GPU time + 주요 raster stat
- `11_bistro_all_raster_stat_figures.py`: 모든 raster stat별 3-panel figure
- `12_bistro_top5_gap_stat_similarity.py`: 성능 차이와 stat의 상위 5개 Pearson 유사도

## 기본 sweep 순서

```text
renderer_variant: 바깥 루프
alu_calc_count:   안쪽 루프
```

## 예시

```powershell
python 01_merge_result_csvs.py --directory . --output merged_results.csv
```

```powershell
python 09_bistro_total_time.py merged_results.csv
```

```powershell
python 12_bistro_top5_gap_stat_similarity.py `
    merged_results.csv `
    out_result_0_raster_stats.csv
```

역상관까지 모양 유사도로 포함하려면:

```powershell
python 12_bistro_top5_gap_stat_similarity.py `
    merged_results.csv `
    out_result_0_raster_stats.csv `
    --rank-absolute
```
