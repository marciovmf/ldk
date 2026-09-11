bl_info = {
    "name": "LDK Mesh Exporter",
    "author": "Marcio VMF",
    "version": (3, 0, 0),
    "blender": (3, 0, 0),
    "location": "File > Export > LDK mesh (.mesh)",
    "description": "Export Blender mesh objects to the LDK STATIC mesh format",
    "category": "Import-Export",
}

import bpy
from bpy.props import FloatProperty, StringProperty
from bpy_extras.io_utils import axis_conversion
from mathutils import Matrix


LDK_MESH_VERSION = "3.0"
LDK_VERTEX_FORMAT = "STATIC"
LDK_DEFAULT_VERTEX_COLOR = 0xFFFFFFFF
LDK_INDEXES_PER_LINE = 48


def _float_text(value):
    return f"{float(value):.9g}"


def _quoted(text):
    text = str(text)
    text = text.replace("\\", "\\\\")
    text = text.replace("\"", "\\\"")
    text = text.replace("\n", "\\n")
    text = text.replace("\r", "\\r")
    text = text.replace("\t", "\\t")
    return f'"{text}"'


def _clamp_byte(value):
    return max(0, min(255, int(round(float(value) * 255.0))))


def _pack_rgba(color):
    r = _clamp_byte(color[0])
    g = _clamp_byte(color[1])
    b = _clamp_byte(color[2])
    a = _clamp_byte(color[3] if len(color) > 3 else 1.0)
    return (r << 24) | (g << 16) | (b << 8) | a


def _active_color_attribute(mesh):
    color_attributes = getattr(mesh, "color_attributes", None)
    if color_attributes and len(color_attributes) > 0:
        attribute = getattr(color_attributes, "active_color", None)
        if attribute is None:
            attribute = getattr(color_attributes, "active", None)
        if attribute is None:
            attribute = color_attributes[0]
        return attribute

    vertex_colors = getattr(mesh, "vertex_colors", None)
    if vertex_colors and len(vertex_colors) > 0:
        return vertex_colors.active or vertex_colors[0]

    return None


def _loop_color(mesh, color_attribute, loop_index, vertex_index):
    if color_attribute is None:
        return LDK_DEFAULT_VERTEX_COLOR

    domain = getattr(color_attribute, "domain", "CORNER")
    data_index = vertex_index if domain == "POINT" else loop_index

    if data_index < 0 or data_index >= len(color_attribute.data):
        return LDK_DEFAULT_VERTEX_COLOR

    color = color_attribute.data[data_index].color
    return _pack_rgba(color)


def _loop_normal(mesh, loop_index):
    corner_normals = getattr(mesh, "corner_normals", None)
    if corner_normals is not None and len(corner_normals) > loop_index:
        return corner_normals[loop_index].vector.copy()

    loop = mesh.loops[loop_index]
    normal = getattr(loop, "normal", None)
    if normal is not None:
        return normal.copy()

    return mesh.vertices[loop.vertex_index].normal.copy()


def _loop_uv(mesh, loop_index):
    uv_layer = mesh.uv_layers.active
    if uv_layer is None or loop_index >= len(uv_layer.data):
        return (0.0, 0.0)

    uv = uv_layer.data[loop_index].uv
    return (float(uv.x), float(uv.y))


def _vertex_key(position, normal, uv, color):
    return (
        float(position.x),
        float(position.y),
        float(position.z),
        float(normal.x),
        float(normal.y),
        float(normal.z),
        float(uv[0]),
        float(uv[1]),
        int(color),
    )


def _material_name(mesh, material_index):
    if material_index < len(mesh.materials):
        material = mesh.materials[material_index]
        if material is not None and material.name:
            return material.name
    return f"Material {material_index}"


def _collect_object_mesh(obj, depsgraph, global_matrix):
    evaluated = obj.evaluated_get(depsgraph)
    mesh = evaluated.to_mesh(
        preserve_all_data_layers=True, depsgraph=depsgraph)
    if mesh is None:
        return None

    try:
        mesh.calc_loop_triangles()
        if not mesh.loop_triangles:
            return None

        calc_normals_split = getattr(mesh, "calc_normals_split", None)
        if calc_normals_split is not None:
            calc_normals_split()

        # Geometry remains object-local. The scene graph owns obj.matrix_world;
        # only the Blender->LDK axis conversion and exporter scale are baked.
        object_matrix = global_matrix
        normal_matrix = object_matrix.to_3x3().inverted_safe().transposed()
        mirrored = object_matrix.to_3x3().determinant() < 0.0
        color_attribute = _active_color_attribute(mesh)

        source_material_indices = sorted({
            triangle.material_index
            if 0 <= triangle.material_index < len(mesh.materials)
            else 0
            for triangle in mesh.loop_triangles
        })
        if not source_material_indices:
            source_material_indices = [0]

        slot_map = {
            source_index: slot
            for slot, source_index in enumerate(source_material_indices)
        }
        material_slots = [
            _material_name(mesh, source_index)
            for source_index in source_material_indices
        ]
        triangles_by_slot = [[] for _ in material_slots]

        for triangle in mesh.loop_triangles:
            source_index = triangle.material_index
            if source_index not in slot_map:
                source_index = 0
            triangles_by_slot[slot_map[source_index]].append(triangle)

        vertices = []
        vertex_lookup = {}
        indices = []
        submeshes = []

        for material_slot, triangles in enumerate(triangles_by_slot):
            first_index = len(indices)

            for triangle in triangles:
                loop_indices = list(triangle.loops)
                if mirrored:
                    loop_indices[1], loop_indices[2] = (
                        loop_indices[2], loop_indices[1])

                for loop_index in loop_indices:
                    loop = mesh.loops[loop_index]
                    source_vertex = mesh.vertices[loop.vertex_index]

                    position = object_matrix @ source_vertex.co
                    normal = normal_matrix @ _loop_normal(mesh, loop_index)
                    if normal.length_squared > 0.0:
                        normal.normalize()

                    uv = _loop_uv(mesh, loop_index)
                    color = _loop_color(
                        mesh, color_attribute, loop_index, loop.vertex_index)
                    key = _vertex_key(position, normal, uv, color)

                    vertex_index = vertex_lookup.get(key)
                    if vertex_index is None:
                        vertex_index = len(vertices)
                        vertex_lookup[key] = vertex_index
                        vertices.append(key)

                    indices.append(vertex_index)

            index_count = len(indices) - first_index
            if index_count:
                submeshes.append(
                    (first_index, index_count, material_slot))

        if not vertices or not indices or len(indices) % 3 != 0:
            return None

        return {
            "name": obj.name,
            "vertices": vertices,
            "indices": indices,
            "material_slots": material_slots,
            "submeshes": submeshes,
        }
    finally:
        evaluated.to_mesh_clear()


def _collect_meshes(context, global_scale):
    depsgraph = context.evaluated_depsgraph_get()
    axis_matrix = axis_conversion(
        from_forward="Y",
        from_up="Z",
        to_forward="-Z",
        to_up="Y",
    ).to_4x4()
    global_matrix = Matrix.Scale(global_scale, 4) @ axis_matrix

    meshes = []
    for obj in context.scene.objects:
        if obj.type != "MESH":
            continue
        mesh = _collect_object_mesh(obj, depsgraph, global_matrix)
        if mesh is not None:
            meshes.append(mesh)

    return meshes


def _write_mesh(path, meshes):
    if not meshes:
        raise ValueError("No renderable mesh objects were found in the scene.")

    lines = [
        "# LDK mesh file",
        "# STATIC vertex: position.xyz normal.xyz uv.xy color(RRGGBBAA)",
        f"version {LDK_MESH_VERSION}",
        f"vertex_format {LDK_VERTEX_FORMAT}",
        f"mesh_count {len(meshes)}",
        "",
    ]

    for mesh in meshes:
        vertices = mesh["vertices"]
        indices = mesh["indices"]
        material_slots = mesh["material_slots"]
        submeshes = mesh["submeshes"]

        lines.append(f"mesh {_quoted(mesh['name'])}")
        lines.append(f"vertex_count {len(vertices)}")
        lines.append(f"index_count {len(indices)}")
        lines.append(f"material_slot_count {len(material_slots)}")
        lines.append(f"submesh_count {len(submeshes)}")

        for slot, name in enumerate(material_slots):
            lines.append(f"material_slot {slot} {_quoted(name)}")

        lines.append("")
        for vertex in vertices:
            px, py, pz, nx, ny, nz, u, v, color = vertex
            lines.append(
                "vertex "
                f"{_float_text(px)} {_float_text(py)} {_float_text(pz)} "
                f"{_float_text(nx)} {_float_text(ny)} {_float_text(nz)} "
                f"{_float_text(u)} {_float_text(v)} 0x{color:08X}"
            )

        lines.append("")
        for start in range(0, len(indices), LDK_INDEXES_PER_LINE):
            chunk = indices[start:start + LDK_INDEXES_PER_LINE]
            lines.append(
                "index_list " + " ".join(str(index) for index in chunk))

        lines.append("")
        for first_index, index_count, material_slot in submeshes:
            lines.append(
                f"submesh {first_index} {index_count} {material_slot}")

        lines.append("end_mesh")
        lines.append("")

    with open(path, "w", encoding="utf-8", newline="\n") as mesh_file:
        mesh_file.write("\n".join(lines))


def export_ldk_mesh(context, path, global_scale):
    meshes = _collect_meshes(context, global_scale)
    _write_mesh(path, meshes)


class LDKMeshExportOperator(bpy.types.Operator):
    bl_idname = "export_scene.ldk_mesh"
    bl_label = "LDK mesh"
    bl_options = {"PRESET"}

    filename_ext = ".mesh"

    filter_glob: StringProperty(
        default="*.mesh",
        options={"HIDDEN"},
        maxlen=255,
    )

    global_scale: FloatProperty(
        name="Global Scale",
        description="Scale factor applied to exported geometry",
        default=1.0,
        min=0.001,
        max=100.0,
    )

    filepath: StringProperty(subtype="FILE_PATH")

    def execute(self, context):
        path = self.filepath
        if not path.lower().endswith(self.filename_ext):
            path += self.filename_ext

        try:
            export_ldk_mesh(context, path, self.global_scale)
        except (OSError, ValueError) as error:
            self.report({"ERROR"}, str(error))
            return {"CANCELLED"}

        self.filepath = path
        return {"FINISHED"}

    def invoke(self, context, event):
        if not self.filepath:
            self.filepath = "object.mesh"
        context.window_manager.fileselect_add(self)
        return {"RUNNING_MODAL"}


def menu_func_export(self, context):
    self.layout.operator(LDKMeshExportOperator.bl_idname, text="LDK mesh (.mesh)")


def register():
    bpy.utils.register_class(LDKMeshExportOperator)
    bpy.types.TOPBAR_MT_file_export.append(menu_func_export)


def unregister():
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)
    bpy.utils.unregister_class(LDKMeshExportOperator)


if __name__ == "__main__":
    register()
