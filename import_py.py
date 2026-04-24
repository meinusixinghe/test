import ezdxf
import json
import sys
import os

def parse_dxf(file_path):
    if not os.path.exists(file_path):
        print(f"Error: File not found {file_path}")
        return {}

    try:
        doc = ezdxf.readfile(file_path)
    except IOError:
        print(f"Not a DXF file or a generic I/O error.")
        return {}
    except ezdxf.DXFStructureError:
        print(f"Invalid or corrupted DXF file.")
        return {}

    data = {
        "holes": [],
        "contours": [],
        "display_paths": []
    }

    if doc.modelspace():
        msp = doc.modelspace()
        temp_holes = []

        # ==================== 1. 提取圆形（孔） ====================
        for entity in msp.query('CIRCLE'):
            center = entity.dxf.center
            radius = entity.dxf.radius
            temp_holes.append({
                'center': [center.x, center.y],
                'radius': radius
            })

        temp_holes.sort(key=lambda h: (-round(h['center'][1], 1), round(h['center'][0])))
        data['holes'] = temp_holes

        # ==================== 2. 提取所有的线段 (界面显示用 + 拼接用) ====================
        display_paths = []
        edges = []

        # 提取独立直线 (LINE)
        for entity in msp.query('LINE'):
            start = entity.dxf.start
            end = entity.dxf.end
            pts = [[start.x, start.y], [end.x, end.y]]
            display_paths.append({"type": "直线", "points": pts}) # 标记类型
            edges.append(pts.copy())

        # 提取多段线 (LWPOLYLINE / POLYLINE) - 拆分为独立直线
        for entity in msp.query('LWPOLYLINE'):
            try:
                with entity.points("xy") as vertices:
                    pts = [[p[0], p[1]] for p in vertices]
                    if len(pts) >= 2:
                        if entity.closed and pts[0] != pts[-1]:pts.append([pts[0][0], pts[0][1]])
                        # 将多段线拆分成单独的直线段放入显示列表
                        for i in range(len(pts)-1):
                            display_paths.append({"type": "直线", "points": [pts[i], pts[i+1]]})
                        edges.append(pts.copy())
            except Exception:
                pass

        for entity in msp.query('POLYLINE'):
            if entity.is_2d_polyline:
                pts = []
                for v in entity.vertices:
                    pts.append([v.dxf.location.x, v.dxf.location.y])
                if len(pts) >= 2:
                    if entity.is_closed and pts[0] != pts[-1]:pts.append([pts[0][0], pts[0][1]])
                    for i in range(len(pts)-1):
                        display_paths.append({"type": "直线", "points": [pts[i], pts[i+1]]})
                    edges.append(pts.copy())

        # 提取圆弧、椭圆、圆 (统一标记为圆弧)
        for entity in msp.query('ARC ELLIPSE CIRCLE'):
            try:
                pts = list(entity.flattening(0.1))
                if len(pts) >= 2:
                    p_list = [[p.x, p.y] for p in pts]
                    if entity.dxftype() == 'CIRCLE' and p_list[0] != p_list[-1]:
                        p_list.append(p_list[0])
                    display_paths.append({"type": "圆弧", "points": p_list})
                    if entity.dxftype() != 'CIRCLE':
                        edges.append(p_list.copy())
            except Exception:
                pass

        # 提取样条曲线
        for entity in msp.query('SPLINE'):
            try:
                pts = list(entity.flattening(0.1))
                if len(pts) >= 2:
                    p_list = [[p.x, p.y] for p in pts]
                    display_paths.append({"type": "样条曲线", "points": p_list})
                    edges.append(p_list.copy())
            except Exception:
                pass

        data['display_paths'] = display_paths

        # ==================== 3. 自动拼接散线为闭合轮廓 ====================
        joined_contours = []
        while edges:
            current_contour = edges.pop(0)
            changed = True
            while changed and edges:
                changed = False
                for i, edge in enumerate(edges):
                    tol = 1.0
                    def dist(p1, p2):
                        return ((p1[0]-p2[0])**2 + (p1[1]-p2[1])**2)**0.5

                    if dist(current_contour[-1], edge[0]) < tol:
                        current_contour.extend(edge[1:])
                        edges.pop(i)
                        changed = True; break
                    elif dist(current_contour[-1], edge[-1]) < tol:
                        current_contour.extend(edge[-2::-1])
                        edges.pop(i)
                        changed = True; break
                    elif dist(current_contour[0], edge[-1]) < tol:
                        current_contour = edge[:-1] + current_contour
                        edges.pop(i)
                        changed = True; break
                    elif dist(current_contour[0], edge[0]) < tol:
                        current_contour = edge[::-1][:-1] + current_contour
                        edges.pop(i)
                        changed = True; break

            joined_contours.append(current_contour)

        valid_contours = []
        for contour in joined_contours:
            if len(contour) > 2:
                if ((contour[0][0]-contour[-1][0])**2 + (contour[0][1]-contour[-1][1])**2)**0.5 < 1.0:
                    if contour[0] != contour[-1]:
                        contour.append(contour[0])
                    valid_contours.append(contour)

        valid_contours.sort(key=len, reverse=True)
        data['contours'] = valid_contours

    return data

def save_to_json(dxf_path, output_path):
    data = parse_dxf(dxf_path)
    with open(output_path, 'w') as f:
        json.dump(data, f, indent=4)

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: python import_py.py <input_dxf_path> <output_json_path>")
        sys.exit(1)

    input_dxf = sys.argv[1]
    output_json = sys.argv[2]

    save_to_json(input_dxf, output_json)
