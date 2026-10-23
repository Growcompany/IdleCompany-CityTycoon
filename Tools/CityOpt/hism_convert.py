"""개별 StaticMeshActor <-> HISM 양방향 변환 (드로우콜 감축).

핵심 설계 = **단방향이 아니다.** 1회성 수동 머지는 맵을 복제하면 전부 유실되고(2026-10-02
Demonstration_WP_Office 에서 170그룹 날아감) 편집도 막는다. explode 로 되돌릴 수 있으면
"편집 끝날 때까지 최적화 대기" 자체가 불필요해진다.

사용 (UE 에디터 원격 실행) — 설정은 C:/tmp/cityopt_cfg.json:
    {"mode":"inventory"}                                  무엇이 몇 개 묶이는지만 (맵 무변경)
    {"mode":"convert","apply":true,"meshes":[...]}         개별 -> HISM
    {"mode":"explode","apply":true,"cells":[[-3,0]]}       HISM -> 개별 (구역만 가능)
"""
import unreal, json, collections, os

CFG = {}
if os.path.exists("C:/tmp/cityopt_cfg.json"):
    CFG = json.load(open("C:/tmp/cityopt_cfg.json", encoding="utf-8"))
MODE        = CFG.get("mode", "inventory")
APPLY       = bool(CFG.get("apply", False))
GRID        = float(CFG.get("grid", 12800.0))   # WP 셀과 같은 격자로 묶어야 스트리밍이 셀 단위로 유지된다
MIN_GROUP   = int(CFG.get("min_group", 4))      # 이보다 작으면 HISM 이 오히려 손해(액터 1개 추가)
MIN_ACTORS  = int(CFG.get("min_actors", 0))     # 로딩 미완에서 0건 처리하는 거짓 통과를 막는다
MESH_FILTER = CFG.get("meshes") or None
CELL_FILTER = set(tuple(c) for c in CFG.get("cells", [])) or None
EXPECT      = CFG.get("expect_total")   # 대상 메시가 몇 개 보여야 하는가 — 숫자 총수 하한보다 낫다
AT          = CFG.get("at")             # remove 모드: 월드 좌표 [x,y,z]
RADIUS      = float(CFG.get("radius", 300.0))
BOX         = CFG.get("box")            # [[xmin,ymin,zmin],[xmax,ymax,zmax]] — 구역 조회/삭제
TAG  = "CityOpt_HISM"
OUT  = "C:/tmp/cityopt_%s.json" % MODE

# ★ 복사하지 않은 속성은 "Δ 0" 으로 안 잡힌다 — 아예 검증되지 않는다. 그래서 목록으로 고정한다.
#   collision = 밟는 면(도로/보도), navigation = NPC 보행, visible/hidden = 숨긴 액터가 되살아남 방지.
COPY_PROPS = ("cast_shadow", "can_ever_affect_navigation", "visible",
              "hidden_in_game", "ld_max_draw_distance")

EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
rep = {"mode": MODE, "apply": APPLY, "errors": []}


def guard():
    acts = EAS.get_all_level_actors()
    # WP 맵은 PIE 중 에디터 셀을 해제한다 — 0 을 결과로 읽으면 거짓 판정.
    if not any(a.get_class().get_name() == "Landscape" for a in acts):
        raise RuntimeError("ABORT: 랜드스케이프 0 — PIE 중이거나 셀 미로드 (actors=%d)" % len(acts))
    # ★ 랜드스케이프는 WP 스트리밍에서 먼저 올라온다 — 그것만 보면 로딩 미완에도 통과한다(실측 9,568/11,210).
    if MIN_ACTORS and len(acts) < MIN_ACTORS:
        raise RuntimeError("ABORT: 액터 %d개 < 하한 %d — 로딩 미완" % (len(acts), MIN_ACTORS))
    return acts


def mat_key(comp):
    return tuple((comp.get_material(i).get_path_name() if comp.get_material(i) else "None")
                 for i in range(comp.get_num_materials()))


def prop_sig(comp):
    sig = {}
    for p in COPY_PROPS:
        sig[p] = comp.get_editor_property(p)
    sig["collision_profile"] = str(comp.get_collision_profile_name())
    sig["collision_enabled"] = comp.get_collision_enabled()
    return sig


def sig_key(sig):
    return tuple(sorted((k, round(v, 3) if isinstance(v, float) else v) for k, v in sig.items()))


def apply_sig(comp, sig):
    for p in COPY_PROPS:
        comp.set_editor_property(p, sig[p])
    comp.set_collision_profile_name(sig["collision_profile"])
    comp.set_collision_enabled(sig["collision_enabled"])


def cell_of(loc):
    return (int(loc.x // GRID), int(loc.y // GRID))


def expect_guard(acts, found):
    """숫자 총수 하한은 내 작업으로 계속 움직여 매번 손대야 했다(대상 기준 검증). 대상 기준으로 본다."""
    if not EXPECT or not MESH_FILTER:
        return None
    if found == 0:
        wanted = set(MESH_FILTER)
        for a in acts:
            for c in a.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
                m = c.static_mesh
                if m and m.get_name() in wanted:
                    return "ALREADY_CONVERTED: 대상 0개인데 같은 메시의 HISM 이 있다 — 이미 변환됨"
        raise RuntimeError("ABORT: 대상 0개, 해당 HISM 도 없음 — 로딩 미완이거나 메시 이름 오류")
    if found < EXPECT * 0.95:
        raise RuntimeError("ABORT: 대상 %d개 < 기대 %d의 95%% — 로딩 미완" % (found, EXPECT))
    return None


def grouped_actor_set(acts):
    """GroupActor 멤버는 변환하지 않는다 — destroy_actor 가 그룹의 멤버 목록을 정리해 주지 않아서,
    지우면 그룹 패키지에 dangling 참조가 남고 맵 로드마다 'Failed import' 가 찍힌다(2026-10-03 실측).
    그룹은 저자가 한 덩어리로 다루려고 묶은 것이므로 건드리지 않는 쪽이 맞다."""
    out = set()
    for g in acts:
        if g.get_class().get_name() != "GroupActor":
            continue
        try:
            members = g.get_editor_property("group_actors")
        except Exception:
            # 프로퍼티가 파이썬에 안 열린다(실측) — 그룹이 있으면 바운드로 걸러낼 수 없으니 보수적으로 경고만.
            rep.setdefault("warnings", []).append(
                "GroupActor %s 의 멤버 목록을 읽을 수 없다 — 그룹 멤버가 변환될 수 있다" % g.get_name())
            continue
        for m in members or []:
            if m:
                out.add(m.get_name())
    return out


def collect_groups(acts):
    groups = collections.defaultdict(list)
    sigs = {}
    in_group = grouped_actor_set(acts)
    skipped_grouped = 0
    for a in acts:
        if a.get_class().get_name() != "StaticMeshActor":
            continue
        if a.get_name() in in_group:
            skipped_grouped += 1
            continue
        c = a.static_mesh_component
        mesh = c.static_mesh
        if mesh is None:
            continue
        if MESH_FILTER and mesh.get_name() not in MESH_FILTER:
            continue
        sig = prop_sig(c)
        actor_scale = a.get_actor_scale3d()
        handedness = actor_scale.x * actor_scale.y * actor_scale.z < 0.0
        key = (mesh.get_path_name(), mat_key(c), sig_key(sig) + (("mirrored", handedness),), cell_of(a.get_actor_location()))
        groups[key].append(a)
        sigs[key] = sig
    if skipped_grouped:
        rep["skipped_grouped_actors"] = skipped_grouped
    return groups, sigs


def do_inventory():
    groups, _ = collect_groups(guard())
    rows, saved = [], 0
    for key, actors in groups.items():
        if len(actors) < MIN_GROUP:
            continue
        rows.append({"mesh": key[0].split(".")[-1], "cell": list(key[3]),
                     "count": len(actors), "slots": len(key[1])})
        saved += len(actors) - 1
    rows.sort(key=lambda r: -r["count"])
    per_mesh = collections.Counter()
    for r in rows:
        per_mesh[r["mesh"]] += r["count"]
    rep.update({"groups": len(rows), "actors_in_groups": sum(r["count"] for r in rows),
                "primitive_reduction": saved, "top_groups": rows[:30],
                "per_mesh": dict(per_mesh.most_common(60))})


def save(pkgs):
    # 삭제 반영에는 삭제된 액터의 패키지가 목록에 있어야 한다(destroy 전에 담아야 얻을 수 있다).
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    pkgs.add(world.get_outermost())
    rep["packages_saved"] = len(pkgs)
    rep["saved"] = unreal.EditorLoadingAndSavingUtils.save_packages(list(pkgs), True)
    rep["dirty_map_after"] = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]


def do_convert():
    acts = guard()
    groups, sigs = collect_groups(acts)
    found = sum(len(v) for v in groups.values())
    note = expect_guard(acts, found)
    if note:
        rep["note"] = note
        return
    rep["target_found"] = found
    made, removed, pkgs, manifest = 0, 0, set(), []
    for key, actors in groups.items():
        if len(actors) < MIN_GROUP:
            continue
        mesh_path, mats, _, cell = key
        sig = sigs[key]
        if not APPLY:
            made += 1; removed += len(actors); continue
        holder = EAS.spawn_actor_from_class(unreal.Actor, actors[0].get_actor_location(), unreal.Rotator())
        # 5.4 파이썬에 Actor.add_component_by_class 가 없다 — 에디터에서 영속되는 경로는 SubobjectDataSubsystem 뿐.
        sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
        handles = sds.k2_gather_subobject_data_for_instance(holder)
        params = unreal.AddNewSubobjectParams(
            parent_handle=handles[0],
            new_class=unreal.HierarchicalInstancedStaticMeshComponent,
            blueprint_context=None)
        new_handle, fail = sds.add_new_subobject(params)
        if fail and str(fail):
            raise RuntimeError("add_new_subobject 실패: %s" % fail)
        hism = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(
            sds.k2_find_subobject_data_from_handle(new_handle))
        if hism is None:
            raise RuntimeError("add_new_subobject 가 컴포넌트를 반환하지 않음")
        hism.set_static_mesh(unreal.load_asset(mesh_path))
        hism.set_mobility(unreal.ComponentMobility.STATIC)
        apply_sig(hism, sig)
        if dict(key[2])["mirrored"]:
            hism.set_world_scale3d(unreal.Vector(-1.0, 1.0, 1.0))
        for i, mp in enumerate(mats):
            if mp != "None":
                hism.set_material(i, unreal.load_asset(mp))
        for a in actors:
            hism.add_instance(a.get_actor_transform(), True)
        holder.set_actor_label("HISM_%s_%d_%d" % (mesh_path.split(".")[-1], cell[0], cell[1]))
        holder.tags = [TAG]
        manifest.append({"label": holder.get_actor_label(), "mesh": mesh_path, "mats": list(mats),
                         "cell": list(cell), "instances": len(actors),
                         "sig": {k: str(v) for k, v in sig.items()}})
        pkgs.add(holder.get_outermost())
        for a in actors:
            pkgs.add(a.get_outermost())   # ★ destroy 뒤에는 패키지를 못 얻는다
            EAS.destroy_actor(a)
            removed += 1
        made += 1
    rep.update({"hism_actors_made": made, "actors_removed": removed, "manifest": manifest})
    if APPLY:
        save(pkgs)


def do_explode():
    acts = guard()
    targets, skipped = [], 0
    for a in acts:
        if TAG not in [str(t) for t in a.tags]:
            continue
        comps = a.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent)
        names = [c.static_mesh.get_name() for c in comps if c.static_mesh]
        if MESH_FILTER and not any(n in MESH_FILTER for n in names):
            skipped += 1; continue
        if CELL_FILTER and cell_of(a.get_actor_location()) not in CELL_FILTER:
            skipped += 1; continue
        targets.append(a)
    spawned, pkgs = 0, set()
    for holder in targets:
        for hism in holder.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
            mesh = hism.static_mesh
            sig = prop_sig(hism)
            mats = [hism.get_material(i) for i in range(hism.get_num_materials())]
            for i in range(hism.get_instance_count()):
                xf = hism.get_instance_transform(i, True)
                if not APPLY:
                    spawned += 1; continue
                a = EAS.spawn_actor_from_class(unreal.StaticMeshActor, xf.translation, xf.rotation.rotator())
                a.set_actor_scale3d(xf.scale3d)
                c = a.static_mesh_component
                c.set_mobility(unreal.ComponentMobility.STATIC)
                c.set_static_mesh(mesh)
                apply_sig(c, sig)
                for mi, m in enumerate(mats):
                    if m:
                        c.set_material(mi, m)
                pkgs.add(a.get_outermost())
                spawned += 1
        if APPLY:
            pkgs.add(holder.get_outermost())
            EAS.destroy_actor(holder)
    rep.update({"hism_actors_found": len(targets), "skipped_by_filter": skipped, "actors_spawned": spawned})
    if APPLY:
        save(pkgs)


def in_box(v):
    (x0, y0, z0), (x1, y1, z1) = BOX
    return x0 <= v.x <= x1 and y0 <= v.y <= y1 and z0 <= v.z <= z1


def in_scope(v):
    if BOX:
        return in_box(v)
    return (v - unreal.Vector(float(AT[0]), float(AT[1]), float(AT[2]))).length() <= RADIUS


def do_survey():
    """구역 안에 무엇이 있는지 메시별로 센다 — HISM 인스턴스와 개별 액터를 같이 본다. 맵 무변경."""
    acts = guard()
    if not (BOX or AT):
        raise RuntimeError("ABORT: 'box' 또는 'at'+'radius' 가 필요하다")
    hism_hit = collections.Counter(); sma_hit = collections.Counter()
    zmin, zmax = 1e18, -1e18
    for a in acts:
        if TAG in [str(t) for t in a.tags]:
            for c in a.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
                m = c.static_mesh
                if m is None: continue
                for i in range(c.get_instance_count()):
                    v = c.get_instance_transform(i, True).translation
                    if in_scope(v):
                        hism_hit[m.get_name()] += 1
                        zmin = min(zmin, v.z); zmax = max(zmax, v.z)
        elif a.get_class().get_name() == "StaticMeshActor":
            m = a.static_mesh_component.static_mesh
            if m is None: continue
            v = a.get_actor_location()
            if in_scope(v):
                sma_hit[m.get_name()] += 1
                zmin = min(zmin, v.z); zmax = max(zmax, v.z)
    rep.update({"hism_instances": dict(hism_hit.most_common()),
                "individual_actors": dict(sma_hit.most_common()),
                "z_range": [None if zmin > 1e17 else round(zmin, 1), None if zmax < -1e17 else round(zmax, 1)],
                "total": sum(hism_hit.values()) + sum(sma_hit.values())})


def do_remove():
    """HISM 인스턴스를 좌표+반경(+메시 필터)으로 지운다 — explode/재convert 없이.
    apply=False 면 무엇이 지워질지만 보고한다(거리 포함). 좌표는 사용자가 마커를 두고 알려주면 된다."""
    acts = guard()
    if not (BOX or AT):
        raise RuntimeError("ABORT: 'box' 또는 'at'+'radius' 가 필요하다")
    hits, pkgs, removed = [], set(), 0
    for a in acts:
        if TAG not in [str(t) for t in a.tags]:
            continue
        for c in a.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
            m = c.static_mesh
            if m is None or (MESH_FILTER and m.get_name() not in MESH_FILTER):
                continue
            # 뒤에서부터 지운다 — 앞에서 지우면 인덱스가 밀려 다음 대상을 놓친다.
            idxs = []
            for i in range(c.get_instance_count()):
                v = c.get_instance_transform(i, True).translation
                if in_scope(v):
                    idxs.append((i, [round(v.x), round(v.y), round(v.z)]))
            if not idxs:
                continue
            hits.append({"actor": a.get_actor_label(), "mesh": m.get_name(),
                         "count": len(idxs), "locs": [d for _, d in idxs][:6]})
            if APPLY:
                for i, _ in sorted(idxs, reverse=True):
                    c.remove_instance(i)
                c.modify(True); a.modify(True)
                pkgs.add(a.get_outermost())
                removed += len(idxs)
    rep.update({"hits": hits, "instances_removed": removed,
                "note": "dry-run — apply=true 로 실제 삭제" if not APPLY else None})
    if APPLY and pkgs:
        save(pkgs)


try:
    {"inventory": do_inventory, "convert": do_convert, "explode": do_explode,
     "remove": do_remove, "survey": do_survey}[MODE]()
except Exception as e:
    rep["errors"].append("%s: %s" % (type(e).__name__, e))

open(OUT, "w", encoding="utf-8").write(json.dumps(rep, ensure_ascii=False, indent=1, default=str))
unreal.log("[CITYOPT %s apply=%s] %s" % (MODE, APPLY, json.dumps(
    {k: v for k, v in rep.items() if k not in ("top_groups", "per_mesh", "manifest")}, default=str)))
