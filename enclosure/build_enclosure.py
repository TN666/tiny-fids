# 機場看板風格外殼 — LilyGO T-Display S3 Touch
# 在 Fusion 360 中執行（fusion_execute）。座標沿用官方 STEP 模型 (mm)：
#   X = 板子寬度方向（對稱），Y = 長度方向（USB-C 在 Y≈-1.2），Z = 厚度（螢幕朝 -Z）
# 外殼上下對稱，螢幕 rotation 1 或 3 都可以裝。
exec(open('/Users/tn/fids-mobile/enclosure/fusion_helpers.py', encoding='utf-8').read())

# ---------------- 參數 ----------------
GLASS_Z = -7.79          # 觸控玻璃正面
RIM_Z = -6.79            # 32x55 玻璃外框邊正面（前框內面壓在這裡）
SKIN = 1.2               # 前框厚度
FRONT = RIM_Z - SKIN     # 外殼正面 Z
BACK_IN = 5.0            # 背蓋內面（背面零件最高 3.51，留 1.5 mm 給實物誤差）
BACK_T = 1.6
BACK = BACK_IN + BACK_T
WALL = 1.6
CAV_X = 16.2             # 內腔半寬（玻璃外框 ±16.0）
CAV_Y0, CAV_Y1 = -1.4, 61.2
OUT_X = CAV_X + WALL
OUT_Y0, OUT_Y1 = CAV_Y0 - WALL, CAV_Y1 + WALL
WIN_X = 13.9             # 玻璃 ±13.5 + 0.4
WIN_Y0, WIN_Y1 = 6.26, 57.06
BTN_X, BTN_Y = 9.05, 2.26
C = 0.25                 # 支架/頂牌 卡槽間隙
HOOK_Y = ((20, 26), (40, 46))   # 背蓋卡鉤 Y 範圍
HOOK_Z = -3.6            # 卡鉤倒鉤的扣合面（方孔後緣）
TILT = 10.0              # 螢幕後仰角度


def build():
    d = adsk.fusion.Design.cast(app.activeProduct)
    root = d.rootComponent
    for o in list(root.occurrences):
        if o.component.name in ('Shell', 'BackCover', 'Stand', 'Topper', 'Ref_TouchGlass'):
            o.deleteMe()

    # ---------- 觸控玻璃參考模型（官方 CapacitiveTouchScreen.stp 量測） ----------
    ref = new_component(root, 'Ref_TouchGlass').component
    box(ref, -16.0, 16.0, 4.16, 59.16, RIM_Z, -6.39, NEW, name='rim')
    box(ref, -13.5, 13.5, 6.66, 56.66, GLASS_Z, RIM_Z, NEW, name='glass')

    # ---------- 前框 Shell ----------
    sc = new_component(root, 'Shell').component
    sk = sc.sketches.add(sc.xYConstructionPlane)
    rounded_rect_xy(sk, -OUT_X, OUT_Y0, OUT_X, OUT_Y1, 3.0)
    shell = extrude_range(sc, sk.profiles.item(0), sk, FRONT, BACK, NEW).bodies.item(0)
    shell.name = 'Shell'
    fillet(sc, edges_on_plane(shell, 'z', FRONT), 1.0)
    # 內腔
    box(sc, -CAV_X, CAV_X, CAV_Y0, CAV_Y1, RIM_Z, BACK + 1, CUT, [shell])
    # 螢幕開口（圓角）+ 外緣倒角
    sk = sc.sketches.add(sc.xYConstructionPlane)
    rounded_rect_xy(sk, -WIN_X, WIN_Y0, WIN_X, WIN_Y1, 2.0)
    extrude_range(sc, sk.profiles.item(0), sk, FRONT - 1, RIM_Z + 0.1, CUT, [shell])
    win_edges = adsk.core.ObjectCollection.create()
    for e in edges_on_plane(shell, 'z', FRONT):
        b = e.boundingBox
        if abs(b.minPoint.x * 10) <= WIN_X + 0.01 and abs(b.maxPoint.x * 10) <= WIN_X + 0.01 \
                and b.minPoint.y * 10 >= WIN_Y0 - 0.01 and b.maxPoint.y * 10 <= WIN_Y1 + 0.01:
            win_edges.add(e)
    chamfer(sc, win_edges, 0.6)

    # 背蓋擋塊：只放在四個角落。板子是從背面、螢幕先放進去，
    # 32mm 寬的玻璃外框邊 (Y 4.16~59.16) 必須能整段通過內腔，所以兩側不能有任何凸起。
    # 板子左右靠玻璃外框邊在 ±16.2 內腔裡定位，前後靠 PCB 兩端定位。
    for sx in (1, -1):
        for (y0, y1) in ((CAV_Y0 - 0.1, 3.6), (59.7, CAV_Y1 + 0.1)):
            box(sc, *sorted((sx * 13.6, sx * (CAV_X + 0.1))), y0, y1, BACK_IN - 1.0, BACK_IN, JOIN, [shell])

    # 按鍵：U 形切槽彈片 + 頂桿 + 指示凹點
    for sx in (1, -1):
        x_hinge, x_free, x_slot = 4.5 * sx, 11.5 * sx, 12.1 * sx
        lo = lambda a, b: (min(a, b), max(a, b))
        z0, z1 = FRONT - 1, RIM_Z + 0.1
        box(sc, *lo(x_free, x_slot), BTN_Y - 2.8, BTN_Y + 2.8, z0, z1, CUT, [shell])
        box(sc, *lo(x_hinge, x_slot), BTN_Y - 2.8, BTN_Y - 2.2, z0, z1, CUT, [shell])
        box(sc, *lo(x_hinge, x_slot), BTN_Y + 2.2, BTN_Y + 2.8, z0, z1, CUT, [shell])
        box(sc, *lo(x_hinge, x_free), BTN_Y - 2.2, BTN_Y + 2.2, RIM_Z - 0.4, RIM_Z + 0.1, CUT, [shell])  # 削薄彈片
        cyl_z(sc, BTN_X * sx, BTN_Y, 1.1, RIM_Z - 0.41, -3.1, JOIN, [shell])
        cyl_z(sc, BTN_X * sx, BTN_Y, 1.6, FRONT - 1, FRONT + 0.3, CUT, [shell])

    # USB-C 開口、RST 針孔（上下兩側）、背蓋卡槽、取下背蓋的缺口
    box(sc, -6.3, 6.3, OUT_Y0 - 1, CAV_Y0 + 0.1, -6.4, 0.8, CUT, [shell])
    cyl_x(sc, 10.3, 1.0, 1.0, -OUT_X - 1, OUT_X + 1, CUT, [shell])
    for sx in (1, -1):  # 背蓋卡鉤的方孔（貫穿側壁，拆背蓋時用牙籤從這裡把卡鉤往內推）
        for (y0, y1) in HOOK_Y:
            box(sc, *sorted((sx * (CAV_X - 0.1), sx * (OUT_X + 0.1))), y0 - 0.3, y1 + 0.3,
                HOOK_Z - 1.7, HOOK_Z, CUT, [shell])
    box(sc, -4, 4, CAV_Y1 - 0.1, OUT_Y1 + 0.5, BACK - 0.7, BACK + 0.5, CUT, [shell])

    # ---------- 背蓋 BackCover ----------
    bc = new_component(root, 'BackCover').component
    cover = box(bc, -(CAV_X - 0.25), CAV_X - 0.25, CAV_Y0 + 0.25, CAV_Y1 - 0.25, BACK_IN, BACK, NEW,
                name='BackCover').bodies.item(0)
    # 壓 PCB 背面兩側邊緣：兩端立柱撐起一條橫樑（列印時是架橋，不用支撐）。
    # 靠近立柱的凸點幾乎不會彎，負責撐住按鍵那端；中間的凸點在橫樑中央，會彎，負責消除晃動。
    for sx in (1, -1):
        bx = sorted((sx * 11.7, sx * 12.55))
        for (y0, y1) in ((14, 17), (55, 58)):
            box(bc, *bx, y0, y1, 1.6, BACK_IN + 0.01, JOIN, [cover])
        box(bc, *bx, 14, 58, 1.6, 3.0, JOIN, [cover])
        box(bc, *bx, 17, 20, 0.6, 1.61, JOIN, [cover])
        box(bc, *bx, 34, 38, 0.2, 1.61, JOIN, [cover])
        # 卡鉤：從背蓋內面往前伸的懸臂，前端外側有倒鉤，卡進側壁方孔
        xo = sx * (CAV_X - 0.25)
        for (y0, y1) in HOOK_Y:
            box(bc, *sorted((xo, xo - sx * 1.2)), y0, y1, HOOK_Z - 1.4, BACK_IN + 0.01, JOIN, [cover])
            prism_y(bc, [(xo - sx * 0.01, HOOK_Z - 1.4), (xo + sx * 0.45, HOOK_Z - 1.4),
                         (xo + sx * 0.95, HOOK_Z - 0.6), (xo + sx * 0.95, HOOK_Z - 0.05),
                         (xo - sx * 0.01, HOOK_Z - 0.05)], y0, y1, JOIN, [cover])
    for vx in (-10, -5, 0, 5, 10):  # 散熱孔
        box(bc, vx - 0.8, vx + 0.8, 16, 46, BACK_IN - 0.1, BACK + 0.1, CUT, [cover])

    # ---------- 頂部招牌 Topper（套在上緣） ----------
    tc = new_component(root, 'Topper').component
    zf, zb = FRONT - C, BACK + C
    xt = OUT_X + C
    prof = [(xt + 2.0, zf - 1.8), (OUT_X - 2.8, zf - 1.8), (OUT_X - 2.8, zf), (xt, zf), (xt, zb),
            (OUT_X - 4.0, zb), (OUT_X - 4.0, zb + 1.8), (xt + 2.0, zb + 1.8)]
    topper = prism_y(tc, prof, 8, 52, NEW).bodies.item(0)
    topper.name = 'Topper'
    box(tc, xt + 1.9, xt + 14.0, OUT_Y0, OUT_Y1, zf - 1.8, zf + 1.2, JOIN, [topper])
    sign_face_z = zf - 1.8
    # 招牌浮雕文字（PingFang 字形會讓布林運算失敗，改用 Heiti TC）
    face = [f for f in topper.faces if isinstance(f.geometry, adsk.core.Plane)
            and f.geometry.normal.z < -0.99 and f.boundingBox.maxPoint.x * 10 > xt + 10][0]
    sk = tc.sketches.add(face)
    path = sk.sketchCurves.sketchLines.addByTwoPoints(SP(sk, 24.8, OUT_Y0 + 1.5, sign_face_z),
                                                      SP(sk, 24.8, OUT_Y1 - 1.5, sign_face_z))
    path.isConstruction = True
    ti = sk.sketchTexts.createInput2('航班資訊  FLIGHT INFO', 0.40)
    ti.setAsAlongPath(path, True, adsk.core.HorizontalAlignments.CenterHorizontalAlignment, 0)
    ti.fontName = 'Heiti TC'
    txt = sk.sketchTexts.add(ti)
    glyphs = tc.features.extrudeFeatures.addSimple(txt, V(0.6), NEW)
    tools = adsk.core.ObjectCollection.create()
    for b in glyphs.bodies:
        tools.add(b)
    ci = tc.features.combineFeatures.createInput(topper, tools)
    ci.operation = JOIN
    tc.features.combineFeatures.add(ci)

    # ---------- 立柱底座 Stand（自己的座標：Z 朝上，Y 朝後） ----------
    th = math.radians(TILT)
    s, c = math.sin(th), math.cos(th)
    base_t, pole_h = 5.0, 40.0
    hp = base_t + pole_h
    fz0, fz1 = FRONT - C - 1.8, BACK + C + 2.0       # 托架 Z 範圍（板子座標）
    fx_bot = -OUT_X - C - 2.0                        # 托架底面 X
    ymid = (OUT_Y0 + OUT_Y1) / 2
    B = (fx_bot, ymid, (fz0 + fz1) / 2)
    P0 = (B[0] - hp * c, B[1], B[2] + hp * s)

    def to_local(xb, yb, zb_):
        dx, dz = xb - P0[0], zb_ - P0[2]
        return (yb - P0[1], s * dx + c * dz, c * dx - s * dz)

    m = adsk.core.Matrix3D.create()
    m.setWithCoordinateSystem(P(*P0), adsk.core.Vector3D.create(0, 1, 0),
                              adsk.core.Vector3D.create(s, 0, c), adsk.core.Vector3D.create(c, 0, -s))
    stc = new_component(root, 'Stand').component
    _, ym, _ = to_local(0, ymid, (FRONT + BACK) / 2)
    sk = stc.sketches.add(stc.xYConstructionPlane)
    rounded_rect_xy(sk, -35, ym - 25, 35, ym + 25, 8.0)
    stand = extrude_range(stc, sk.profiles.item(0), sk, 0, base_t, NEW).bodies.item(0)
    stand.name = 'Stand'
    chamfer(stc, edges_on_plane(stand, 'z', base_t), 1.5)
    box(stc, -7, 7, -4, 4, base_t - 0.1, hp + 1.0, JOIN, [stand])
    zfc, zbc = FRONT - C, BACK + C
    xin = -OUT_X - C
    cradle_b = [(fx_bot, fz0), (-OUT_X + 2.8, fz0), (-OUT_X + 2.8, zfc), (xin, zfc), (xin, zbc),
                (-8.0, zbc), (-8.0, fz1), (fx_bot, fz1)]
    cradle_l = [to_local(xb, 0, zb_)[1:] for (xb, zb_) in cradle_b]
    prism_x(stc, cradle_l, -18, 18, JOIN, [stand])

    # 放到正確位置
    occ = [o for o in root.occurrences if o.component.name == 'Stand'][0]
    occ.transform = m
    if d.snapshots.hasPendingSnapshot:  # 參數式設計需記錄位置，否則重算會還原
        d.snapshots.add()

    # 配色：深灰外殼、黃色招牌、黑色玻璃
    lib = app.materialLibraries.itemByName('Fusion Appearance Library')

    def appearance(src, name, rgb=None):
        a = d.appearances.itemByName(name)
        if not a:
            a = d.appearances.addByCopy(lib.appearances.itemByName(src), name)
            if rgb:
                for p in a.appearanceProperties:
                    if p.id in ('opaque_albedo', 'surface_albedo'):
                        try:
                            p.value = adsk.core.Color.create(*rgb, 255)
                        except Exception:
                            pass
        return a
    colors = {'Shell': appearance('Plastic - Matte (Black)', 'FIDS Charcoal', (38, 40, 44)),
              'BackCover': appearance('Plastic - Matte (Black)', 'FIDS Charcoal', (38, 40, 44)),
              'Topper': appearance('Plastic - Matte (Black)', 'FIDS Yellow', (255, 194, 14)),
              'Stand': appearance('Plastic - Matte (Black)', 'FIDS Stand', (70, 74, 82)),
              'Ref_TouchGlass': appearance('Plastic - Glossy (Black)', 'Plastic - Glossy (Black)')}
    for o in root.occurrences:
        if o.component.name in colors:
            for b in o.component.bRepBodies:
                b.appearance = colors[o.component.name]

    for o in root.occurrences:
        if o.component.name in ('Shell', 'BackCover', 'Stand', 'Topper', 'Ref_TouchGlass'):
            print(o.component.name, [bb_mm(b) for b in o.bRepBodies])
    print('sign_face_z', sign_face_z)


build()
