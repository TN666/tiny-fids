# 機場看板（側面看是倒 T 字形）：黑色面板＋底座。組好的「前框＋背蓋」從面板正面嵌進去，面板下緣插進底座的槽。
# 座標沿用 build_enclosure.py（板子座標）：X = 螢幕高度方向，Y = 寬度方向（USB 在 -Y），Z = 厚度（螢幕朝 -Z）。
# 列印：面板正面朝下，不用支撐（背面擋邊有 1.5 mm 小懸空）；底座平放。
exec(open('/Users/tn/fids-mobile/enclosure/fusion_helpers.py', encoding='utf-8').read())

# 沿用前框尺寸
OUT_X = 17.8
OUT_Y0, OUT_Y1 = -3.0, 62.8
FRONT = -7.99
BACK = 6.6

BLK_W, BLK_H, BLK_D = 100.0, 60.0, 20.0   # 面板 寬(Y) × 高(X) × 深(Z)
BASE_D, BASE_T = 70.0, 8.0                 # 底座 前後深度、厚度
SLOT = 5.0                                 # 面板插進底座的深度
SLOT_FIT = 0.2
RECESS = 1.0       # 前框比方塊正面內縮
FIT = 0.2          # 嵌槽單邊間隙
WALL = 2.4         # 方塊外牆
FACE = 3.0         # 方塊正面板厚
TUBE = 2.0         # 嵌槽外圍的牆
LIP = 1.5          # 背面擋邊往內伸出量
LIP_T = 1.6

YC = (OUT_Y0 + OUT_Y1) / 2
ZF = FRONT - RECESS
ZB = ZF + BLK_D
BX, BY0, BY1 = BLK_H / 2, YC - BLK_W / 2, YC + BLK_W / 2


def build_block():
    d = adsk.fusion.Design.cast(app.activeProduct)
    root = d.rootComponent
    for o in list(root.occurrences):
        if o.component.name in ('Block', 'Base'):
            o.deleteMe()
    for o in root.occurrences:  # 舊的立柱和招牌用不到了
        if o.component.name in ('Stand', 'Topper'):
            o.isLightBulbOn = False
    bc = new_component(root, 'Block').component

    # 外形：圓角方塊，中間掏空，背面開口
    sk = bc.sketches.add(bc.xYConstructionPlane)
    rounded_rect_xy(sk, -BX, BY0, BX, BY1, 4.0)
    blk = extrude_range(bc, sk.profiles.item(0), sk, ZF, ZB, NEW).bodies.item(0)
    blk.name = 'Block'
    box(bc, -BX, -BX + 4.1, BY0, BY1, ZF, ZB, JOIN, [blk])  # 下緣做成直角，才能插進底座
    fillet(bc, edges_on_plane(blk, 'z', ZF), 1.5)
    sk = bc.sketches.add(bc.xYConstructionPlane)
    rounded_rect_xy(sk, -BX + WALL, BY0 + WALL, BX - WALL, BY1 - WALL, 4.0 - WALL)
    extrude_range(bc, sk.profiles.item(0), sk, ZF + FACE, ZB + 1, CUT, [blk])

    # 嵌槽外圍的牆＋背面擋邊
    o = OUT_X + FIT
    sk = bc.sketches.add(bc.xYConstructionPlane)
    rounded_rect_xy(sk, -o - TUBE, OUT_Y0 - FIT - TUBE, o + TUBE, OUT_Y1 + FIT + TUBE, 3.0 + TUBE)
    extrude_range(bc, sk.profiles.item(0), sk, ZF + FACE - 0.01, BACK + LIP_T, JOIN, [blk])
    # 嵌槽（前框外形＋間隙）
    sk = bc.sketches.add(bc.xYConstructionPlane)
    rounded_rect_xy(sk, -o, OUT_Y0 - FIT, o, OUT_Y1 + FIT, 3.0 + FIT)
    extrude_range(bc, sk.profiles.item(0), sk, ZF - 1, BACK, CUT, [blk])
    # 擋邊開口
    sk = bc.sketches.add(bc.xYConstructionPlane)
    rounded_rect_xy(sk, -o + LIP, OUT_Y0 - FIT + LIP, o - LIP, OUT_Y1 + FIT - LIP, 3.0)
    extrude_range(bc, sk.profiles.item(0), sk, BACK - 0.1, BACK + LIP_T + 1, CUT, [blk])
    pocket_edges = adsk.core.ObjectCollection.create()  # 嵌槽正面邊緣倒角
    for e in edges_on_plane(blk, 'z', ZF):
        b = e.boundingBox
        if abs(b.minPoint.x * 10) <= o + 0.01 and abs(b.maxPoint.x * 10) <= o + 0.01:
            pocket_edges.add(e)
    chamfer(bc, pocket_edges, 0.6)

    # 嵌槽長邊上的小凸點，增加摩擦讓前框不會自己滑出來
    for sx in (1, -1):
        for y in (15, 33):  # 避開側壁的 RST 孔和卡鉤方孔
            cyl_z(bc, sx * (o + 0.1), y, 0.4, ZF + RECESS + 2, BACK - 1, JOIN, [blk])

    # USB 線孔：穿過嵌槽牆和方塊左側外牆
    box(bc, -7, 7, BY0 - 1, OUT_Y0 + 0.1, -8.0, 2.4, CUT, [blk])

    # 底座：平板，中間一條槽讓面板下緣插進去
    sc = new_component(root, 'Base').component
    zc = (ZF + ZB) / 2
    xb0, xb1 = -BX - (BASE_T - SLOT), -BX + SLOT
    base = box(sc, xb0, xb1, BY0 - 5, BY1 + 5, zc - BASE_D / 2, zc + BASE_D / 2, NEW, name='Base').bodies.item(0)
    vert = adsk.core.ObjectCollection.create()
    for e in base.edges:
        b = e.boundingBox
        if abs(b.maxPoint.x - b.minPoint.x) * 10 > BASE_T - 0.1:
            vert.add(e)
    fillet(sc, vert, 6.0)
    chamfer(sc, edges_on_plane(base, 'x', xb1), 1.0)
    box(sc, -BX - 0.01, xb1 + 1, BY0 - SLOT_FIT, BY1 + SLOT_FIT, ZF - SLOT_FIT, ZB + SLOT_FIT, CUT, [base])

    print('Block', bb_mm(blk), 'Base', bb_mm(base))
    return blk


build_block()
