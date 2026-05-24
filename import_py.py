import ezdxf
import json
import sys
import os
import math

# 辅助函数：计算多边形面积
def get_area(pts):
    area = 0.0
    n = len(pts)
    if n < 3: return 0.0
    for i in range(n):
        j = (i + 1) % n
        area += pts[i][0] * pts[j][1] - pts[j][0] * pts[i][1]
    return abs(area) / 2.0

# 辅助函数：计算两点间距离
def dist(p1, p2):
    return math.hypot(p1[0]-p2[0], p1[1]-p2[1])

def parse_dxf(file_path):
    if not os.path.exists(file_path):
        return {}
    try:
        doc = ezdxf.readfile(file_path)
    except Exception:
        return {}

    data = {"holes": [], "contours": [], "display_paths": []}

    if not doc.modelspace():
        return data

    msp = doc.modelspace()

    # 1. 提取独立孔洞
    temp_holes = []
    for entity in msp.query('CIRCLE'):
        temp_holes.append({
            'center': [entity.dxf.center.x, entity.dxf.center.y],
            'radius': entity.dxf.radius
        })
    temp_holes.sort(key=lambda h: (-round(h['center'][1], 1), round(h['center'][0])))
    data['holes'] = temp_holes

    # 2. 提取所有的独立实体
    elements = []

    def add_line(p1, p2):
        elements.append({
            'type': '直线',
            'points': [p1, p2],
            'start': p1, 'end': p2,
            'is_closed': False
        })

    def process_entity(entity):
        dxftype = entity.dxftype()
        if dxftype == 'LINE':
            add_line([entity.dxf.start.x, entity.dxf.start.y], [entity.dxf.end.x, entity.dxf.end.y])
        elif dxftype == 'ARC':
            try:
                pts = [[p.x, p.y] for p in entity.flattening(0.1)]
                if len(pts) >= 2:
                    elements.append({'type': '圆弧', 'points': pts, 'start': pts[0], 'end': pts[-1], 'is_closed': False})
            except: pass
        elif dxftype == 'CIRCLE':
            try:
                pts = [[p.x, p.y] for p in entity.flattening(0.1)]
                if len(pts) >= 2:
                    if dist(pts[0], pts[-1]) > 1e-3:
                        pts.append(pts[0][:])
                    elements.append({'type': '圆', 'points': pts, 'start': pts[0], 'end': pts[-1], 'is_closed': True})
            except: pass
        elif dxftype in ['LWPOLYLINE', 'POLYLINE']:
            # 【彩蛋】：利用底层的 virtual_entities，将多段线完美炸开为纯粹的“直线”和“圆弧”！
            try:
                for v_ent in entity.virtual_entities():
                    process_entity(v_ent)
            except: pass
        elif dxftype in ['SPLINE', 'ELLIPSE']:
            try:
                pts = [[p.x, p.y] for p in entity.flattening(0.1)]
                if len(pts) >= 2:
                    is_c = False
                    if dxftype == 'ELLIPSE' and entity.dxf.start_param == 0 and abs(entity.dxf.end_param - 2*math.pi) < 1e-3:
                        is_c = True
                    elif hasattr(entity, 'closed') and entity.closed:
                        is_c = True
                    elif dist(pts[0], pts[-1]) < 1e-3:
                        is_c = True

                    if is_c and dist(pts[0], pts[-1]) > 1e-3:
                        pts.append(pts[0][:])

                    elements.append({'type': '样条曲线', 'points': pts, 'start': pts[0], 'end': pts[-1], 'is_closed': is_c})
            except: pass

    # 将图纸内所有元素全部交给处理机
    for entity in msp:
        process_entity(entity)

    # 3. 贪心连接算法
    TOL = 0.5 # 👇【关键修复】：将闭合容差放宽到 0.5 毫米！强行吸附那些画得不规范的断头线！
    used = [False] * len(elements)
    loops = []

    for i in range(len(elements)):
        if used[i]: continue

        comp_elements = [elements[i]]
        used[i] = True

        if elements[i]['is_closed'] or dist(elements[i]['start'], elements[i]['end']) < TOL:
            loops.append(comp_elements)
            continue

        current_start = elements[i]['start']
        current_end = elements[i]['end']

        while True:
            changed = False
            for j in range(len(elements)):
                if used[j]: continue
                seg = elements[j]

                # 动态调转方向以首尾相接，并强行将端点对齐缝合
                if dist(current_end, seg['start']) < TOL:
                    seg['points'][0] = current_end
                    seg['start'] = current_end
                    comp_elements.append(seg)
                    current_end = seg['end']
                    used[j] = True
                    changed = True
                elif dist(current_end, seg['end']) < TOL:
                    seg['points'].reverse()
                    seg['start'], seg['end'] = seg['end'], seg['start']
                    seg['points'][0] = current_end
                    seg['start'] = current_end
                    comp_elements.append(seg)
                    current_end = seg['end']
                    used[j] = True
                    changed = True
                elif dist(current_start, seg['end']) < TOL:
                    seg['points'][-1] = current_start
                    seg['end'] = current_start
                    comp_elements.insert(0, seg)
                    current_start = seg['start']
                    used[j] = True
                    changed = True
                elif dist(current_start, seg['start']) < TOL:
                    seg['points'].reverse()
                    seg['start'], seg['end'] = seg['end'], seg['start']
                    seg['points'][-1] = current_start
                    seg['end'] = current_start
                    comp_elements.insert(0, seg)
                    current_start = seg['start']
                    used[j] = True
                    changed = True
            if not changed: break

        # 4. 判断闭合（只要端点差距在 0.5mm 内就算做闭合！）
        if dist(current_start, current_end) < TOL:
            comp_elements[-1]['points'][-1] = comp_elements[0]['points'][0]
            loops.append(comp_elements)
        else:
            # 只有那些敞口超过 0.5mm 的死胡同线，才会被当做废线删除
            pass

    # 5. 计算面积与排序
    valid_loops = []
    for comp in loops:
        min_xy = float('inf')
        best_element_idx = 0
        best_point_idx = 0

        for i, el in enumerate(comp):
            for j, pt in enumerate(el['points'][:-1]): # 遍历该线段上的点
                val = pt[0] + pt[1]
                if val < min_xy:
                    min_xy = val
                    best_element_idx = i
                    best_point_idx = j

        # 旋转 comp 列表，让 best_element_idx 变成第一个元素
        comp = comp[best_element_idx:] + comp[:best_element_idx]

        # 旋转该线段内的点数组，让 best_point_idx 变成第一个点
        first_el = comp[0]
        first_el['points'] = first_el['points'][best_point_idx:] + first_el['points'][:best_point_idx]
        first_el['start'] = first_el['points'][0]
        first_el['end'] = first_el['points'][-1]

        area = get_area([p for el in comp for p in el['points'][:-1]])
        valid_loops.append({
            'area': area,
            'elements': comp,
            'poly_pts': [p for el in comp for p in el['points'][:-1]]
        })

    # 按面积从大到小排列
    valid_loops.sort(key=lambda x: x['area'], reverse=True)

    # 6. 组装数据并输出
    display_paths = []
    for loop in valid_loops:
        contour_pts = loop['poly_pts']
        if len(contour_pts) > 0:
            contour_pts.append(contour_pts[0])
            data['contours'].append(contour_pts)

        for el in loop['elements']:
            display_paths.append({
                "type": el['type'],
                "points": el['points']
            })

    data['display_paths'] = display_paths
    return data

def save_to_json(dxf_path, output_path):
    data = parse_dxf(dxf_path)
    with open(output_path, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=4, ensure_ascii=False)

if __name__ == '__main__':
    if len(sys.argv) < 3:
        sys.exit(1)
    save_to_json(sys.argv[1], sys.argv[2])
