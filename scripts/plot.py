from pathlib import Path
import csv
import math

import matplotlib.pyplot as plt


Y_COLUMN = "frame_time_avg"
SETTINGS_FILE = "plot_settings.txt"
OUTPUT_FILE = "plot.png"


def read_x_column(script_dir: Path) -> str:
    settings_path = script_dir / SETTINGS_FILE

    if not settings_path.exists():
        raise FileNotFoundError(f"설정 파일이 없습니다: {settings_path}")

    with settings_path.open("r", encoding="utf-8-sig") as f:
        x_column = f.readline().strip()

    if not x_column:
        raise ValueError(f"{SETTINGS_FILE} 첫 줄에 x축 컬럼명을 써야 합니다.")

    return x_column


def parse_float(value: str, file_name: str, row_index: int, column: str) -> float:
    try:
        return float(value)
    except ValueError as e:
        raise ValueError(
            f"{file_name}의 {row_index}번째 데이터 행, '{column}' 값을 float로 변환할 수 없습니다: {value}"
        ) from e


def read_csv_rows(csv_path: Path) -> list[dict[str, str]]:
    with csv_path.open("r", encoding="utf-8-sig", newline="") as f:
        reader = csv.DictReader(f)
        return list(reader)


def main():
    script_dir = Path(__file__).resolve().parent
    x_column = read_x_column(script_dir)

    file_labels = []
    x_labels = []
    y_series_by_row_index = []

    index = 1

    while True:
        csv_path = script_dir / f"res1-{index}.csv"

        if not csv_path.exists():
            break

        rows = read_csv_rows(csv_path)

        if not rows:
            raise ValueError(f"{csv_path.name}에 데이터 행이 없습니다.")

        first_row = rows[0]

        if x_column not in first_row:
            raise KeyError(f"{csv_path.name}에 x축 컬럼 '{x_column}'이 없습니다.")

        if Y_COLUMN not in first_row:
            raise KeyError(f"{csv_path.name}에 y축 컬럼 '{Y_COLUMN}'이 없습니다.")

        file_labels.append(f"res1-{index}")
        x_labels.append(str(first_row[x_column]))

        while len(y_series_by_row_index) < len(rows):
            y_series_by_row_index.append([])

        for row_index, row in enumerate(rows):
            if Y_COLUMN not in row:
                raise KeyError(f"{csv_path.name}에 y축 컬럼 '{Y_COLUMN}'이 없습니다.")

            y_value = parse_float(
                row[Y_COLUMN],
                csv_path.name,
                row_index + 1,
                Y_COLUMN,
            )

            y_series_by_row_index[row_index].append(y_value)

        # 이전 파일보다 데이터 행 수가 적은 경우를 대비해서 빈 값 채우기
        for row_index in range(len(rows), len(y_series_by_row_index)):
            y_series_by_row_index[row_index].append(math.nan)

        index += 1

    if not file_labels:
        raise FileNotFoundError("res1-1.csv부터 시작하는 CSV 파일을 찾지 못했습니다.")

    x_positions = list(range(len(x_labels)))
    row_count = len(y_series_by_row_index)

    plt.figure(figsize=(max(8, len(x_labels) * 1.2), 6))

    if row_count == 1:
        plt.bar(
            x_positions,
            y_series_by_row_index[0],
            label="data row 1",
        )
    else:
        total_width = 0.8
        bar_width = total_width / row_count

        for row_index, y_values in enumerate(y_series_by_row_index):
            offset = (row_index - (row_count - 1) / 2) * bar_width
            bar_positions = [x + offset for x in x_positions]

            plt.bar(
                bar_positions,
                y_values,
                width=bar_width,
                label=f"data row {row_index + 1}",
            )

    plt.xlabel(x_column)
    plt.ylabel(Y_COLUMN)
    plt.title(f"{Y_COLUMN} by {x_column}")
    plt.xticks(x_positions, x_labels, rotation=45, ha="right")
    plt.legend(title="CSV data row")
    plt.grid(axis="y", alpha=0.3)
    plt.tight_layout()

    output_path = script_dir / OUTPUT_FILE
    plt.savefig(output_path, dpi=200)
    plt.close()

    print(f"Saved plot: {output_path}")


if __name__ == "__main__":
    main()