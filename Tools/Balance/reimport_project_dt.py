# -*- coding: utf-8 -*-
# DT_Project_* 를 DataImport CSV 로 재임포트 + 저장 + 검증.
#
# 부르는 방법 3가지:
#   1) Balance Studio — runner 가 BALV_REQUEST 를 주입하고 exec, 또는 runpy 로 reimport_targets() 직접 호출
#   2) 손으로         — remote_cmd.py 에 exec(open(...).read()) 한 줄. 기본 request(3표 전부)로 자동 실행된다.
#   3) 테스트/도구    — runpy.run_path 로 함수만 꺼내 쓴다. 이때는 최상위에서 아무 일도 하지 않는다.
#
# 원격은 traceback 을 안 돌려주고, 비-ASCII 줄은 remote_cmd 가 통째로 <non-ascii output> 으로 바꾼다.
# 그래서 결과는 ensure_ascii JSON 한 줄(MARKER)로만 내보낸다 — 종료 코드는 성공의 증거가 못 된다.
import unreal, json, os, re, traceback

# 곡선 리시드 대상 = reseed_project_curve.js 의 PROJECT_CURVE_TARGETS 와 1:1. 손으로 실행할 때의 기본 대상이다.
# (2026-07-29 판은 Game 만 밀어서 Finance/IT 는 CSV 와 uasset 이 어긋난 채 남았다 — 그 불일치를 여기서 닫는다.)
CURVE_TARGETS = {
    "DT_Project_Game": "DT_Project_Game_Import.csv",
    "DT_Project_Finance": "DT_Project_Finance_Import.csv",
    "DT_Project_IT": "DT_Project_IT_Import.csv",
}
# 제조업 3종은 Weight_* 컬럼 자체가 저작되지 않아 CSV 가 권위가 아니다 — 리임포트하면 표를 망가뜨린다.
DENIED_TABLES = frozenset(["DT_Project_Semiconductor", "DT_Project_Automobile", "DT_Project_Electronics"])

# 일반 CSV 소스 표(31종)도 대상이 될 수 있다 — 안 그러면 CSV 를 고쳐도 uasset 에 닿지 않아
# tune.js 의 기존 한계("리임포트하면 되돌아감")를 그대로 물려받는다.
# 대신 구조를 엄격히 검사한다: 표 이름 형식 · assetPath 는 /Game 아래이고 leaf 가 표 이름 · CSV 는 csvDir 아래 <표>_Import.csv.
# 진짜 allowlist(manifest dtPaths)는 서버(studio/reimport.js)가 강제한다 — 여기는 2차 방어선이다.
TABLE_NAME_RE = re.compile(r"^DT_[A-Za-z0-9_]+$")
ASSET_PATH_RE = re.compile(r"^/Game/[A-Za-z0-9_]+(?:/[A-Za-z0-9_]+)*$")
DEFAULT_DT_DIR = "/Game/CompanyGrowth/Table/Company"
MARKER = "[BALV-REIMPORT] RESULT "
CELL_EPS = 1e-6


def project_dir(unreal_api=unreal):
    # 사용자 홈을 코드에 박지 않는다 — 어느 머신에서 열든 그 에디터가 연 프로젝트를 따른다.
    return unreal_api.Paths.convert_relative_path_to_full(unreal_api.Paths.project_dir())


def default_request(unreal_api=unreal):
    root = project_dir(unreal_api)
    return {
        "csvDir": os.path.join(root, "DataImport"),
        "targets": [{"table": t, "csvName": c} for t, c in sorted(CURVE_TARGETS.items())],
    }


def _resolve_targets(request, unreal_api):
    csv_dir = request.get("csvDir")
    if not csv_dir or not os.path.isdir(csv_dir):
        raise RuntimeError("csvDir is not a directory: %s" % csv_dir)
    raw_targets = request.get("targets") or []
    if not raw_targets:
        raise RuntimeError("request has no targets")

    root = project_dir(unreal_api)
    seen = set()
    out = []
    for entry in raw_targets:
        table = entry.get("table")
        if not table or not TABLE_NAME_RE.match(table):
            raise RuntimeError("bad table name: %r" % (table,))
        if table in DENIED_TABLES:
            raise RuntimeError("table is denied (CSV is not authoritative): %s" % table)
        if table in seen:
            raise RuntimeError("duplicate target: %s" % table)
        seen.add(table)

        # 요청이 어떤 경로를 실어 보내든 파일명만 취해 csvDir 아래로 고정한다.
        csv_name = os.path.basename(entry.get("csvName") or ("%s_Import.csv" % table))
        if csv_name != "%s_Import.csv" % table:
            raise RuntimeError("csvName must be %s_Import.csv, got %s" % (table, csv_name))
        csv_path = os.path.join(csv_dir, csv_name)
        if not os.path.isfile(csv_path):
            raise RuntimeError("csv not found: %s" % csv_path)

        # assetPath 는 /Game 아래여야 하고 leaf 가 표 이름이어야 한다 — 임의 에셋을 덮어쓸 통로가 되면 안 된다.
        asset_path = entry.get("assetPath") or ("%s/%s" % (DEFAULT_DT_DIR, table))
        if not ASSET_PATH_RE.match(asset_path) or asset_path.rsplit("/", 1)[-1] != table:
            raise RuntimeError("bad assetPath for %s: %s" % (table, asset_path))
        uasset_path = os.path.join(root, "Content", *(asset_path[len("/Game/"):].split("/"))) + ".uasset"
        if not os.path.isfile(uasset_path):
            raise RuntimeError("uasset not found: %s" % uasset_path)

        out.append({
            "table": table,
            "csvPath": csv_path,
            "assetPath": asset_path,
            "uassetPath": uasset_path,
        })
    return out


def _export_rows(dt, unreal_api):
    # (fill 에 다시 먹일 수 있는 JSON 문자열, 정규화된 row 리스트) — 롤백은 이 문자열이 유일한 원본이다.
    raw = unreal_api.DataTableFunctionLibrary.export_data_table_to_json_string(dt)
    data = json.loads(raw)
    if not isinstance(data, list):
        data = [dict(v, Name=k) for k, v in data.items()]
        raw = json.dumps(data)
    return raw, data


def _rollback(done, unreal_api):
    # ⚠ fill 은 임포트 전에 표를 비운다 — 실패한 채 raise 하면 빈/부분 상태가 메모리에 남고
    #    누군가 Save All 하면 그게 디스크에 굳는다. 반드시 원본 JSON 으로 되돌리고 나서 raise.
    notes = []
    for t in done:
        try:
            ok = unreal_api.DataTableFunctionLibrary.fill_data_table_from_json_string(t["dt"], t["beforeJson"])
            notes.append("%s:%s" % (t["table"], "ok" if ok else "fill-false"))
        except Exception as ex:
            notes.append("%s:error(%s)" % (t["table"], ex))
    return notes


def _row_map(rows):
    out = {}
    for r in rows:
        out[str(r.get("Name"))] = r
    return out


def _verify_target(t, expect):
    before, after = t["beforeRows"], t["afterRows"]
    if len(before) != len(after):
        return "row count changed for %s: %d -> %d" % (t["table"], len(before), len(after))
    before_names = sorted(_row_map(before).keys())
    after_names = sorted(_row_map(after).keys())
    if before_names != after_names:
        return "row name set changed for %s" % t["table"]
    if before and after:
        lost = sorted(set(before[0].keys()) - set(after[0].keys()))
        gained = sorted(set(after[0].keys()) - set(before[0].keys()))
        if lost or gained:
            return "column set changed for %s: lost=%s gained=%s" % (t["table"], lost, gained)

    # 서버가 CSV 에 쓴 값이 표에 실제로 들어왔는지 — CSV 만 고치고 리임포트가 헛돈 경우를 여기서 잡는다.
    by_name = _row_map(after)
    for e in expect:
        if e.get("table") != t["table"]:
            continue
        row = by_name.get(str(e.get("row")))
        if row is None:
            return "expected row missing in %s: %s" % (t["table"], e.get("row"))
        got = row.get(e.get("column"))
        try:
            got_f = float(got)
        except (TypeError, ValueError):
            return "expected cell not numeric in %s.%s@%s: %r" % (t["table"], e.get("column"), e.get("row"), got)
        want_f = float(e.get("value"))
        if abs(got_f - want_f) > max(CELL_EPS, abs(want_f) * CELL_EPS):
            return "cell mismatch %s.%s@%s: expected %s got %s" % (
                t["table"], e.get("column"), e.get("row"), want_f, got_f)
    return None


def _readback(t, request, unreal_api):
    columns = request.get("readbackColumns") or []
    if not columns and t["afterRows"]:
        numeric = [k for k, v in t["afterRows"][0].items()
                   if isinstance(v, (int, float)) and not isinstance(v, bool)]
        columns = sorted(numeric)[:2]
    sample = {}
    for c in columns:
        try:
            vals = unreal_api.DataTableFunctionLibrary.get_data_table_column_as_string(t["dt"], c)
        except Exception:
            continue
        if vals:
            sample[c] = [str(v) for v in list(vals)[:3]]
    return {
        "rows": len(t["afterRows"]),
        "columns": len(t["afterRows"][0].keys()) if t["afterRows"] else 0,
        "sample": sample,
    }


def _emit(result, unreal_api):
    # 한 줄 · ASCII — remote_cmd 는 비-ASCII 줄을 <non-ascii output> 으로 바꿔버려 마커가 통째로 사라진다.
    line = MARKER + json.dumps(result, ensure_ascii=True, separators=(",", ":"))
    print(line)
    return line


def reimport_targets(request, unreal_api=unreal):
    result = {"ok": False, "tables": [], "readback": {}}
    loaded = []
    try:
        targets = _resolve_targets(request, unreal_api)
        expect = request.get("expect") or []

        # 1) 전부 먼저 로드 + before 스냅샷. 한 표라도 못 열면 아무것도 건드리지 않는다.
        for t in targets:
            dt = unreal_api.EditorAssetLibrary.load_asset(t["assetPath"])
            if not dt:
                raise RuntimeError("load failed: %s" % t["assetPath"])
            before_json, before_rows = _export_rows(dt, unreal_api)
            t.update({
                "dt": dt,
                "beforeJson": before_json,
                "beforeRows": before_rows,
                "mtime": os.path.getmtime(t["uassetPath"]),
                "size": os.path.getsize(t["uassetPath"]),
            })
            loaded.append(t)

        # 2) fill + 저장 전 검증. 실패는 지금까지 손댄 표 전부를 메모리에서 되돌린 뒤 raise.
        touched = []
        for t in loaded:
            touched.append(t)
            ok = unreal_api.DataTableFunctionLibrary.fill_data_table_from_csv_file(t["dt"], t["csvPath"])
            if not ok:
                result["rollback"] = _rollback(touched, unreal_api)
                raise RuntimeError("fill_data_table_from_csv_file returned false: %s" % t["table"])
            _, t["afterRows"] = _export_rows(t["dt"], unreal_api)
            problem = _verify_target(t, expect)
            if problem:
                result["rollback"] = _rollback(touched, unreal_api)
                raise RuntimeError(problem)

        # 3) 저장. 여기서 실패하면 메모리는 이미 "의도한 값" 이므로 되돌리지 않는다 — 되돌리면 패치를 버리는 것.
        for t in loaded:
            if not unreal_api.EditorAssetLibrary.save_asset(t["assetPath"], only_if_is_dirty=False):
                raise RuntimeError("save_asset returned false: %s" % t["assetPath"])
            new_mtime = os.path.getmtime(t["uassetPath"])
            new_size = os.path.getsize(t["uassetPath"])
            if new_mtime <= t["mtime"] and new_size == t["size"]:
                raise RuntimeError("uasset unchanged on disk (second editor instance?): %s" % t["uassetPath"])
            result["tables"].append(t["table"])

        # 4) 저장 후 리드백 — 디스크가 아니라 표에서 다시 읽어 값이 살아있는지 본다.
        for t in loaded:
            result["readback"][t["table"]] = _readback(t, request, unreal_api)

        result["tables"] = sorted(result["tables"])
        result["ok"] = True
    except Exception as ex:
        result["ok"] = False
        result["error"] = "%s: %s" % (type(ex).__name__, ex)
        result["traceback"] = traceback.format_exc()[-2000:]
        try:
            unreal_api.log_error(traceback.format_exc())
        except Exception:
            pass
    _emit(result, unreal_api)
    return result


if "BALV_REQUEST" in globals():
    reimport_targets(BALV_REQUEST)  # noqa: F821 — Studio runner 가 주입한다
elif __name__ == "__main__" and "BALV_NO_AUTORUN" not in globals():
    # 손으로 exec(open(...).read()) 한 경우 — allowlist 3표 전부가 기본 대상이다.
    reimport_targets(default_request())
