# -*- coding: utf-8 -*-
# 에디터에서만 읽을 수 있는 값(DataTable 전량 + Blueprint CDO 프로퍼티)을 JSON 한 파일로 덤프한다.
#
# 부르는 방법 3가지 (reimport_project_dt.py 와 같은 규약):
#   1) Balance Studio — runner 가 BALV_CONFIG 를 주입하고 exec
#   2) 손으로         — remote_cmd.py 에 exec(open(...).read()) 한 줄. 기본 경로로 자동 실행된다.
#   3) 테스트/도구    — runpy 로 함수만 꺼내 쓴다. 최상위에서 아무 일도 하지 않는다.
#
# 읽기 전용 계약: load_object / get_default_object / get_editor_property / export_data_table_to_json_string 만 쓴다.
# save 나 fill 은 이 파일에 없다 — "값을 읽기만 했다" 는 보고가 거짓이 되면 안 된다.
import unreal, json, os, traceback

MARKER_OK = "[BALV-EXTRACT] OK"
MARKER_FAIL = "[BALV-EXTRACT] FAIL "


def project_dir(unreal_api=unreal):
    return unreal_api.Paths.convert_relative_path_to_full(unreal_api.Paths.project_dir())


def default_config(unreal_api=unreal):
    root = project_dir(unreal_api)
    return {
        "manifestPath": os.path.join(root, "Tools", "Balance", "extract", "manifest_editor.json"),
        "outputPath": "C:/tmp/balv_editor.json",
    }


def _extract_to_file(manifest_path, output_path, unreal_api):
    with open(manifest_path, "r", encoding="utf-8") as f:
        manifest = json.load(f)

    result = {"dt": {}, "cdo": {}}

    for name, dt_path in manifest["dtPaths"].items():
        dt = unreal_api.load_object(None, dt_path)
        if dt is None:
            raise RuntimeError("DT load failed: %s (%s)" % (dt_path, name))
        # 5.4 실측: export_data_table_to_json_string 은 row 객체 배열(각 row 에 Name 포함)을 준다.
        s = unreal_api.DataTableFunctionLibrary.export_data_table_to_json_string(dt)
        result["dt"][name] = json.loads(s)

    for entry in manifest["cdoProps"]:
        key, cls_path, prop = entry["key"], entry["class"], entry["prop"]
        cls = unreal_api.load_object(None, cls_path)
        if cls is None:
            raise RuntimeError("class load failed: %s (key=%s)" % (cls_path, key))
        cdo = unreal_api.get_default_object(cls)
        result["cdo"][key] = {"v": float(cdo.get_editor_property(prop)), "loc": cls_path + "." + prop}

    out_dir = os.path.dirname(output_path)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(result, f)

    line = "%s -> %s dt=%d cdo=%d" % (MARKER_OK, output_path, len(result["dt"]), len(result["cdo"]))
    print(line)
    unreal_api.log(line)
    return {"ok": True, "outputPath": output_path, "dt": len(result["dt"]), "cdo": len(result["cdo"])}


def extract_editor(config, unreal_api=unreal):
    manifest_path = config["manifestPath"]
    output_path = config["outputPath"]
    try:
        return _extract_to_file(manifest_path, output_path, unreal_api)
    except Exception as ex:
        # 원격은 traceback 을 안 돌려준다 — 실패 사유를 ASCII 한 줄로 실어 보내야 호출부가 원인을 안다.
        print(MARKER_FAIL + json.dumps("%s: %s" % (type(ex).__name__, ex), ensure_ascii=True))
        unreal_api.log_error(traceback.format_exc())
        return {"ok": False, "error": str(ex)}


if "BALV_CONFIG" in globals():
    extract_editor(BALV_CONFIG)  # noqa: F821 — Studio runner 가 주입한다
elif __name__ == "__main__" and "BALV_NO_AUTORUN" not in globals():
    extract_editor(default_config())
