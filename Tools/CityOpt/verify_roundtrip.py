"""HISM 왕복 무손실 증명 — 개별 -> HISM -> 개별 이 트랜스폼을 보존하는지 실측.

4,000개를 변환하기 전에 작은 그룹 하나로 증명한다. "되돌릴 수 있다"는 주장은
측정 없이는 주장일 뿐이고, 되돌려 보니 틀어졌다면 그때는 원본이 이미 없다.

설정 = C:/tmp/cityopt_rt.json  {"mesh": "SM_Deck_Chair"}
단계는 phase 로 나눠 각각 따로 실행한다(한 스크립트에서 spawn+destroy 를 연달아 하면
에디터가 액터 목록을 갱신하기 전에 다음 단계가 읽을 수 있다).
   phase=snapshot_before -> phase=convert -> phase=explode -> phase=compare
"""
import unreal, json, os

CFG = json.load(open("C:/tmp/cityopt_rt.json", encoding="utf-8"))
MESH = CFG["mesh"]
PHASE = CFG["phase"]
SNAP = "C:/tmp/cityopt_rt_%s.json"
TAG = "CityOpt_HISM"
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def guard():
    acts = EAS.get_all_level_actors()
    if not any(a.get_class().get_name() == "Landscape" for a in acts):
        raise RuntimeError("ABORT: 랜드스케이프 0 — PIE 중이거나 셀 미로드 (actors=%d)" % len(acts))
    return acts


def snapshot(acts):
    rows = []
    for a in acts:
        if a.get_class().get_name() != "StaticMeshActor":
            continue
        c = a.static_mesh_component
        m = c.static_mesh
        if m is None or m.get_name() != MESH:
            continue
        t = a.get_actor_transform()
        rows.append({
            "loc": [round(t.translation.x, 4), round(t.translation.y, 4), round(t.translation.z, 4)],
            "rot": [round(t.rotation.rotator().pitch, 4), round(t.rotation.rotator().yaw, 4),
                    round(t.rotation.rotator().roll, 4)],
            "scale": [round(t.scale3d.x, 5), round(t.scale3d.y, 5), round(t.scale3d.z, 5)],
            "mats": [(c.get_material(i).get_path_name() if c.get_material(i) else "None")
                     for i in range(c.get_num_materials())],
            "cast_shadow": bool(c.cast_shadow),
        })
    rows.sort(key=lambda r: (r["loc"][0], r["loc"][1], r["loc"][2]))
    return rows


def run_mode(mode):
    json.dump({"mode": mode, "apply": True, "meshes": [MESH], "min_group": 4},
              open("C:/tmp/cityopt_cfg.json", "w", encoding="utf-8"))
    path = os.path.join(unreal.Paths.project_dir(), "Tools", "CityOpt", "hism_convert.py")
    exec(compile(open(path, encoding="utf-8").read(), path, "exec"), {"__name__": "__cityopt__"})


rep = {"phase": PHASE, "mesh": MESH}
if PHASE == "snapshot_before":
    rows = snapshot(guard())
    json.dump(rows, open(SNAP % "before", "w", encoding="utf-8"))
    rep["count"] = len(rows)
elif PHASE in ("convert", "explode"):
    guard()
    run_mode(PHASE)
    rep["ran"] = PHASE
    rep["result"] = json.load(open("C:/tmp/cityopt_%s.json" % PHASE, encoding="utf-8"))
elif PHASE == "compare":
    after = snapshot(guard())
    json.dump(after, open(SNAP % "after", "w", encoding="utf-8"))
    before = json.load(open(SNAP % "before", encoding="utf-8"))
    rep["count_before"], rep["count_after"] = len(before), len(after)
    if len(before) != len(after):
        rep["verdict"] = "FAIL 개수 불일치"
    else:
        worst = {"loc": 0.0, "rot": 0.0, "scale": 0.0}
        mism = 0
        for b, a in zip(before, after):
            for k in ("loc", "rot", "scale"):
                for i in range(3):
                    d = abs(b[k][i] - a[k][i])
                    if k == "rot":
                        d = min(d, abs(360.0 - d))   # 회전은 ±180 랩어라운드를 같은 값으로 본다
                    worst[k] = max(worst[k], d)
            if b["mats"] != a["mats"] or b["cast_shadow"] != a["cast_shadow"]:
                mism += 1
        rep["worst_delta"] = worst
        rep["material_or_shadow_mismatch"] = mism
        ok = worst["loc"] < 0.01 and worst["rot"] < 0.01 and worst["scale"] < 1e-4 and mism == 0
        rep["verdict"] = "PASS 왕복 무손실" if ok else "FAIL 값 변동"

unreal.log("[RT] " + json.dumps(rep, ensure_ascii=False, default=str)[:1200])
json.dump(rep, open("C:/tmp/cityopt_rt_rep_%s.json" % PHASE, "w", encoding="utf-8"),
          ensure_ascii=False, indent=1, default=str)
