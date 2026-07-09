from pathlib import Path
import csv
import math
import re

import matplotlib.pyplot as plt


# CSV에서 실제 프레임/패스 시간이 들어있는 컬럼
Y_COLUMN = "time_avg_ms"

# 각 pass 이름이 들어있는 컬럼
PASS_NAME_COLUMN = "pass_name"

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
            f"{file_name}의 {row_index}번째 데이터 행, "
            f"'{column}' 값을 float로 변환할 수 없습니다: {value}"
        ) from e


def read_csv_rows(csv_path: Path) -> list[dict[str, str]]:
    with csv_path.open("r", encoding="utf-8-sig", newline="") as f:
        reader = csv.DictReader(f)

        if reader.fieldnames is None:
            raise ValueError(f"{csv_path.name}에 헤더가 없습니다.")

        return list(reader)


def find_result_csv_files(script_dir: Path) -> list[Path]:
    """
    res1-1.csv, res1-2.csv, ... 형태의 파일을 찾아 숫자 순서대로 정렬한다.
    중간 번호가 비어 있어도 존재하는 파일은 모두 읽는다.
    """
    pattern = re.compile(r"^res1-(\d+)\.csv$")

    numbered_files: list[tuple[int, Path]] = []

    for path in script_dir.glob("res1-*.csv"):
        match = pattern.match(path.name)
        if match:
            numbered_files.append((int(match.group(1)), path))

    numbered_files.sort(key=lambda item: item[0])

    return [path for _, path in numbered_files]


def main():
    script_dir = Path(__file__).resolve().parent
    x_column = read_x_column(script_dir)

    csv_files = find_result_csv_files(script_dir)

    if not csv_files:
        raise FileNotFoundError("res1-1.csv 형태의 CSV 파일을 찾지 못했습니다.")

    x_labels: list[str] = []

    # pass 이름 순서 유지
    pass_labels: list[str] = []

    # pass 이름 -> y값 리스트
    y_values_by_pass: dict[str, list[float]] = {}

    for csv_path in csv_files:
        rows = read_csv_rows(csv_path)

        if not rows:
            raise ValueError(f"{csv_path.name}에 데이터 행이 없습니다.")

        first_row = rows[0]

        if x_column not in first_row:
            raise KeyError(f"{csv_path.name}에 x축 컬럼 '{x_column}'이 없습니다.")

        if Y_COLUMN not in first_row:
            raise KeyError(f"{csv_path.name}에 y축 컬럼 '{Y_COLUMN}'이 없습니다.")

        if PASS_NAME_COLUMN not in first_row:
            raise KeyError(f"{csv_path.name}에 pass 이름 컬럼 '{PASS_NAME_COLUMN}'이 없습니다.")

        x_labels.append(str(first_row[x_column]))

        # 새 CSV 파일을 읽기 전에 기존 pass들에는 기본값 NaN을 추가
        for pass_label in pass_labels:
            y_values_by_pass[pass_label].append(math.nan)

        current_file_passes: set[str] = set()

        for row_index, row in enumerate(rows):
            pass_label = row[PASS_NAME_COLUMN].strip()

            if not pass_label:
                pass_label = f"pass row {row_index + 1}"

            if pass_label in current_file_passes:
                raise ValueError(
                    f"{csv_path.name}의 {row_index + 1}번째 데이터 행에서 "
                    f"중복된 pass_name이 발견되었습니다: {pass_label}"
                )

            current_file_passes.add(pass_label)

            y_value = parse_float(
                row[Y_COLUMN],
                csv_path.name,
                row_index + 1,
                Y_COLUMN,
            )

            if pass_label not in y_values_by_pass:
                pass_labels.append(pass_label)

                # 이전 CSV 파일 개수만큼 NaN을 채운 뒤 현재 파일 값 추가
                y_values_by_pass[pass_label] = [math.nan] * (len(x_labels) - 1)
                y_values_by_pass[pass_label].append(y_value)
            else:
                # 현재 CSV 파일 위치의 NaN을 실제 값으로 교체
                y_values_by_pass[pass_label][-1] = y_value

    x_positions = list(range(len(x_labels)))
    pass_count = len(pass_labels)

    plt.figure(figsize=(max(8, len(x_labels) * 1.2), 6))

    if pass_count == 1:
        pass_label = pass_labels[0]

        plt.bar(
            x_positions,
            y_values_by_pass[pass_label],
            label=pass_label,
        )
    else:
        total_width = 0.8
        bar_width = total_width / pass_count

        for pass_index, pass_label in enumerate(pass_labels):
            offset = (pass_index - (pass_count - 1) / 2) * bar_width
            bar_positions = [x + offset for x in x_positions]

            plt.bar(
                bar_positions,
                y_values_by_pass[pass_label],
                width=bar_width,
                label=pass_label,
            )

    plt.xlabel(x_column)
    plt.ylabel("frame time avg (ms)")
    plt.title(f"Frame time by {x_column}")
    plt.xticks(x_positions, x_labels, rotation=45, ha="right")
    plt.legend(title="pass")
    plt.grid(axis="y", alpha=0.3)
    plt.tight_layout()

    output_path = script_dir / OUTPUT_FILE
    plt.savefig(output_path, dpi=200)
    plt.close()

    print(f"Saved plot: {output_path}")


if __name__ == "__main__":
    main()