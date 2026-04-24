import ezdxf
import json
import sys                  # 用于获取命令行参数（比如输入 DXF 路径、输出 JSON 路径），控制程序的命令行运行逻辑
import os                   # 检查文件是否存在，处理文件路径的基础有效性验证

def parse_dxf(file_path):
    if not os.path.exists(file_path):
        print(f"Error: File not found {file_path}")
        return {}

    try:
        doc = ezdxf.readfile(file_path)
    except IOError:                                 # 文件不是DXF格式、文件无法读取
        print(f"Not a DXF file or a generic I/O error.")
        return {}
    except ezdxf.DXFStructureError:                 # DXF文件损坏、格式无效
        print(f"Invalid or corrupted DXF file.")
        return {}

    # 创建存储数据的字典
    data = {
        "holes": [],                                # 存储圆形（孔）的信息：圆心、半径
        "contours": [],                             # 存储多段线（主轮廓）的信息：顶点坐标列表
        "display_paths": []                         # 新增：存储用于在左侧界面全量绘制的线段/曲线
    }

    # 遍历 DXF中的实体
    if doc.modelspace():
        msp = doc.modelspace()                      # 获取DXF模型空间
        temp_holes = []                             # 临时存储孔数据

        # ==================== 1. 提取圆形（孔） ====================
        for entity in msp.query('CIRCLE'):
            center = entity.dxf.center
            radius = entity.dxf.radius
            temp_holes.append({
                'center': [center.x, center.y],
                'radius': radius
            })

        # 排序规则：先按 X轴升序，X相同按 Y轴升序
        temp_holes.sort(key=lambda h: (-round(h['center'][1], 1), round(h['center'][0])))
        data['holes'] = temp_holes

        # ==================== 2. 提取多段线 (主轮廓) ====================
        for entity in msp.query('LWPOLYLINE'):
            points = []
            try:
                with entity.points("xy") as vertices:
                    points = [[p[0], p[1]] for p in vertices]
            except Exception:
                continue
            if len(points) < 2: continue

            if entity.closed:
                if points[0] != points[-1]:
                    points.append(points[0])
            data['contours'].append(points)

        for entity in msp.query('POLYLINE'):
            if entity.is_2d_polyline:
                points = []
                for v in entity.vertices:
                    points.append([v.dxf.location.x, v.dxf.location.y])
                if len(points) < 2: continue
                if entity.is_closed:
                    if points[0] != points[-1]:
                        points.append(points[0])
                data['contours'].append(points)

        # 删除原有的：当没有轮廓时强行用直线计算矩形包围盒的代码

        # ==================== 3. 提取所有的背景绘制元素 (界面显示用) ====================
        # 使用 ezdxf 的 flattening() 方法，将曲线压平为散点构成的多段线
        display_paths = []

        # 3.1 提取独立直线 (LINE)
        for entity in msp.query('LINE'):
            start = entity.dxf.start
            end = entity.dxf.end
            display_paths.append([[start.x, start.y], [end.x, end.y]])

        # 3.2 提取多段线 (将上方的LWPOLYLINE/POLYLINE同样放入显示列表)
        # 注意：如果你的C++代码里已经绘制了contours，这里就不需要再加，防止重叠绘制变粗。
        # 如果C++中打算背景线和主轮廓分离，就把 data['contours'] 直接加进来。
        for contour in data['contours']:
            display_paths.append(contour)

        # 3.3 提取圆弧/曲线 (ARC)
        for entity in msp.query('ARC'):
            try:
                # 0.1为弦高公差，数值越小离散的点越密集，曲线越平滑
                pts = list(entity.flattening(0.1))
                if len(pts) >= 2:
                    display_paths.append([[p.x, p.y] for p in pts])
            except Exception:
                pass

        # 3.4 提取样条曲线 (SPLINE)
        for entity in msp.query('SPLINE'):
            try:
                pts = list(entity.flattening(0.1))
                if len(pts) >= 2:
                    display_paths.append([[p.x, p.y] for p in pts])
            except Exception:
                pass

        # 3.5 提取椭圆 (ELLIPSE)
        for entity in msp.query('ELLIPSE'):
            try:
                pts = list(entity.flattening(0.1))
                if len(pts) >= 2:
                    display_paths.append([[p.x, p.y] for p in pts])
            except Exception:
                pass

        # 3.6 将所有的圆形也离散并提取 (确保所有的孔也能作为背景画出来)
        for entity in msp.query('CIRCLE'):
            try:
                pts = list(entity.flattening(0.1))
                if len(pts) >= 2:
                    # 确保圆形闭合
                    if pts[0] != pts[-1]:
                        pts.append(pts[0])
                    display_paths.append([[p.x, p.y] for p in pts])
            except Exception:
                pass

        # 存入返回的数据字典中
        data['display_paths'] = display_paths

        if len(data['contours']) == 0 and len(display_paths) > 0:
                    # 复制一份所有的线段用于拼接
                    edges = [path.copy() for path in display_paths if len(path) >= 2]

                    joined_contours = []
                    while edges:
                        current_contour = edges.pop(0)
                        changed = True
                        while changed and edges:
                            changed = False
                            for i, edge in enumerate(edges):
                                tol = 1.0 # 缝合容差值，端点距离小于 1.0 认为相连
                                def dist(p1, p2):
                                    return ((p1[0]-p2[0])**2 + (p1[1]-p2[1])**2)**0.5

                                # 尾接首
                                if dist(current_contour[-1], edge[0]) < tol:
                                    current_contour.extend(edge[1:])
                                    edges.pop(i)
                                    changed = True; break
                                # 尾接尾 (edge反向加入)
                                elif dist(current_contour[-1], edge[-1]) < tol:
                                    current_contour.extend(edge[-2::-1])
                                    edges.pop(i)
                                    changed = True; break
                                # 首接尾 (插入到头部)
                                elif dist(current_contour[0], edge[-1]) < tol:
                                    current_contour = edge[:-1] + current_contour
                                    edges.pop(i)
                                    changed = True; break
                                # 首接首 (反转后插入到头部)
                                elif dist(current_contour[0], edge[0]) < tol:
                                    current_contour = edge[::-1][:-1] + current_contour
                                    edges.pop(i)
                                    changed = True; break

                        joined_contours.append(current_contour)

                    # 筛选拼接好的线条，如果首尾闭合，就当作管板主轮廓
                    for contour in joined_contours:
                        if len(contour) > 2:
                            # 检查首尾两端是否闭合
                            if ((contour[0][0]-contour[-1][0])**2 + (contour[0][1]-contour[-1][1])**2)**0.5 < 1.0:
                                # 强制确保首尾坐标完全一致
                                if contour[0] != contour[-1]:
                                    contour.append(contour[0])
                                data['contours'].append(contour)

    return data

def save_to_json(dxf_path, output_path):
    data = parse_dxf(dxf_path)          # 调用parse_dxf获取数据
    with open(output_path, 'w') as f:
        json.dump(data, f, indent=4)    # 将数据写入JSON文件，indent=4（缩进参数，可选）让格式更易读

if __name__ == '__main__':
    # sys.argv是 Python 的sys模块提供的字符串列表，它会自动捕获用户在命令行中输入的所有参数
    # 当在命令行执行 Python 脚本时，操作系统会将输入的所有 “以空格分隔的部分” 按顺序存入sys.argv列表中
    if len(sys.argv) < 3:               # 检查命令行参数数量：脚本名+输入DXF路径+输出JSON路径，至少3个参数
        print("Usage: python import_py.py <input_dxf_path> <output_json_path>")
        sys.exit(1)                     # 参数不足则打印使用说明并退出

    # 第一个元素（sys.argv[0]）：永远是脚本的文件名（包括路径）
    input_dxf = sys.argv[1]             # 第二个参数是 DXF 路径
    output_json = sys.argv[2]           # 第三个参数是 JSON 保存路径

    save_to_json(input_dxf, output_json)
