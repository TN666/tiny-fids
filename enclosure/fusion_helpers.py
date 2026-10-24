# Fusion 360 建模輔助函式（單位 mm，內部轉成 cm）
# 在 fusion_execute 中用 exec(open(path).read()) 載入
import math
import adsk.core, adsk.fusion

NEW = adsk.fusion.FeatureOperations.NewBodyFeatureOperation
JOIN = adsk.fusion.FeatureOperations.JoinFeatureOperation
CUT = adsk.fusion.FeatureOperations.CutFeatureOperation


def P(x, y, z=0.0):
    return adsk.core.Point3D.create(x / 10.0, y / 10.0, z / 10.0)


def V(mm):
    return adsk.core.ValueInput.createByReal(mm / 10.0)


def sketch_normal(sk):
    return sk.xDirection.crossProduct(sk.yDirection)


def SP(sk, x, y, z):
    """model (component) 座標 mm -> 該草圖座標點"""
    return sk.modelToSketchSpace(P(x, y, z))


def poly(sk, pts3d):
    lines = sk.sketchCurves.sketchLines
    sp = [SP(sk, *p) for p in pts3d]
    for i in range(len(sp)):
        lines.addByTwoPoints(sp[i], sp[(i + 1) % len(sp)])


def rounded_rect_xy(sk, x0, y0, x1, y1, r, z=0.0):
    """XY 平面圓角矩形"""
    L = sk.sketchCurves.sketchLines
    A = sk.sketchCurves.sketchArcs
    q = lambda x, y: SP(sk, x, y, z)
    L.addByTwoPoints(q(x0 + r, y0), q(x1 - r, y0))
    L.addByTwoPoints(q(x1, y0 + r), q(x1, y1 - r))
    L.addByTwoPoints(q(x1 - r, y1), q(x0 + r, y1))
    L.addByTwoPoints(q(x0, y1 - r), q(x0, y0 + r))
    k = r * (1 - math.cos(math.pi / 4))
    A.addByThreePoints(q(x1 - r, y0), q(x1 - k, y0 + k), q(x1, y0 + r))
    A.addByThreePoints(q(x1, y1 - r), q(x1 - k, y1 - k), q(x1 - r, y1))
    A.addByThreePoints(q(x0 + r, y1), q(x0 + k, y1 - k), q(x0, y1 - r))
    A.addByThreePoints(q(x0, y0 + r), q(x0 + k, y0 + k), q(x0 + r, y0))


def extrude_range(comp, profiles, sk, a0, a1, op, bodies=None):
    """沿草圖法線方向，從 model 座標 a0 擠出到 a1（法線軸上的座標，mm）。
    草圖平面需通過原點且法線為 ±X/±Y/±Z。"""
    n = sketch_normal(sk)
    sign = 1.0 if (n.x + n.y + n.z) > 0 else -1.0
    s0, s1 = sign * a0, sign * a1
    if s1 < s0:
        s0, s1 = s1, s0
    if isinstance(profiles, (list, tuple)):
        coll = adsk.core.ObjectCollection.create()
        for p in profiles:
            coll.add(p)
        prof = coll
    else:
        prof = profiles
    ext = comp.features.extrudeFeatures
    inp = ext.createInput(prof, op)
    inp.startExtent = adsk.fusion.OffsetStartDefinition.create(V(s0))
    inp.setOneSideExtent(adsk.fusion.DistanceExtentDefinition.create(V(s1 - s0)),
                         adsk.fusion.ExtentDirections.PositiveExtentDirection)
    if bodies is not None and op in (CUT, JOIN):
        inp.participantBodies = bodies
    return ext.add(inp)


def all_profiles(sk):
    return [sk.profiles.item(i) for i in range(sk.profiles.count)]


def box(comp, x0, x1, y0, y1, z0, z1, op, bodies=None, name=None):
    sk = comp.sketches.add(comp.xYConstructionPlane)
    poly(sk, [(x0, y0, 0), (x1, y0, 0), (x1, y1, 0), (x0, y1, 0)])
    f = extrude_range(comp, sk.profiles.item(0), sk, z0, z1, op, bodies)
    if name and op == NEW:
        f.bodies.item(0).name = name
    return f


def cyl_z(comp, cx, cy, r, z0, z1, op, bodies=None):
    sk = comp.sketches.add(comp.xYConstructionPlane)
    sk.sketchCurves.sketchCircles.addByCenterRadius(SP(sk, cx, cy, 0), r / 10.0)
    return extrude_range(comp, sk.profiles.item(0), sk, z0, z1, op, bodies)


def cyl_y(comp, cx, cz, r, y0, y1, op, bodies=None):
    sk = comp.sketches.add(comp.xZConstructionPlane)
    sk.sketchCurves.sketchCircles.addByCenterRadius(SP(sk, cx, 0, cz), r / 10.0)
    return extrude_range(comp, sk.profiles.item(0), sk, y0, y1, op, bodies)


def cyl_x(comp, cy, cz, r, x0, x1, op, bodies=None):
    sk = comp.sketches.add(comp.yZConstructionPlane)
    sk.sketchCurves.sketchCircles.addByCenterRadius(SP(sk, 0, cy, cz), r / 10.0)
    return extrude_range(comp, sk.profiles.item(0), sk, x0, x1, op, bodies)


def prism_x(comp, pts_yz, x0, x1, op, bodies=None):
    """在 YZ 平面畫多邊形 [(y,z),...]，沿 X 擠出"""
    sk = comp.sketches.add(comp.yZConstructionPlane)
    poly(sk, [(0, y, z) for (y, z) in pts_yz])
    return extrude_range(comp, sk.profiles.item(0), sk, x0, x1, op, bodies)


def prism_y(comp, pts_xz, y0, y1, op, bodies=None):
    sk = comp.sketches.add(comp.xZConstructionPlane)
    poly(sk, [(x, 0, z) for (x, z) in pts_xz])
    return extrude_range(comp, sk.profiles.item(0), sk, y0, y1, op, bodies)


def bb_mm(o):
    b = o.boundingBox
    return [round(v * 10, 2) for v in (b.minPoint.x, b.minPoint.y, b.minPoint.z,
                                        b.maxPoint.x, b.maxPoint.y, b.maxPoint.z)]


def edges_on_plane(body, axis, value, tol=0.01):
    """回傳完全落在 axis=value 平面上的邊 (mm)"""
    out = adsk.core.ObjectCollection.create()
    for e in body.edges:
        b = e.boundingBox
        lo = getattr(b.minPoint, axis) * 10
        hi = getattr(b.maxPoint, axis) * 10
        if abs(lo - value) < tol and abs(hi - value) < tol:
            out.add(e)
    return out


def fillet(comp, edges, r):
    if edges.count == 0:
        return None
    fi = comp.features.filletFeatures.createInput()
    fi.addConstantRadiusEdgeSet(edges, V(r), True)
    return comp.features.filletFeatures.add(fi)


def chamfer(comp, edges, d):
    if edges.count == 0:
        return None
    ci = comp.features.chamferFeatures.createInput2()
    ci.chamferEdgeSets.addEqualDistanceChamferEdgeSet(edges, V(d), True)
    return comp.features.chamferFeatures.add(ci)


def new_component(root, name, matrix=None):
    m = matrix or adsk.core.Matrix3D.create()
    occ = root.occurrences.addNewComponent(m)
    occ.component.name = name
    return occ


def save_view(path, eye, target, up, w=1000, h=700, fit=True):
    vp = app.activeViewport
    cam = vp.camera
    cam.isSmoothTransition = False
    cam.target = P(*target)
    cam.eye = P(*eye)
    cam.upVector = adsk.core.Vector3D.create(*up)
    vp.camera = cam
    if fit:
        vp.fit()
    vp.refresh()
    vp.saveAsImageFile(path, w, h)
