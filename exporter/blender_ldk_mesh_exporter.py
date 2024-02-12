import bpy
import os
import math
from bpy.props import BoolProperty, FloatProperty, IntProperty, EnumProperty


class Surface:
    def __init__(self, materialName):
        self.materialName = materialName
        self.firstIndex = -1
        self.indexCount = 0


class Material:
    def __init__(self, id, name):
        self.id = id
        self.name = name
        # In case we decide to embed material information here ..
        # self.properties = {}


def computeBoundingBox(vertices):
    if not vertices:
        return None

    min_x, min_y, min_z = vertices[0]
    max_x, max_y, max_z = vertices[0]

    for vertex in vertices:
        x, y, z = vertex
        min_x = min(min_x, x)
        min_y = min(min_y, y)
        min_z = min(min_z, z)
        max_x = max(max_x, x)
        max_y = max(max_y, y)
        max_z = max(max_z, z)

    return (min_x, min_y, min_z, max_x, max_y, max_z)


def computeBoundingSphere(vertices):
    """
    Computes a bounding sphere for the mesh
    # Step 1: Find the Center
    # Step 2: Calculate the Radius
    """
    if not vertices:
        return None

    # Find the Center
    center = vertices[0]  # Initialize to the first vertex
    num_vertices = 1

    for vertex in vertices[1:]:
        num_vertices += 1
        center = [c1 + c2 for c1, c2 in zip(center, vertex)]

    # Calculate the average center
    center = [c / num_vertices for c in center]

    # Calculate the Radius.
    #Initialize to the distance to the first vertex
    radius = math.dist(center, vertices[0])

    for vertex in vertices:
        distance = math.dist(center, vertex)
        radius = max(radius, distance)

    return (center, radius)



def objFileToLDKMesh(objFileName, ldkMeshFileName):
    v_list = []
    vt_list = []
    vn_list = []
    index_list = []
    surface_list = []
    vertex_dict = {}
    material_lib = set()
    materials = {}

    with open(objFileName, "r") as file:
        # Pass one, collect vertex data
        for line in file:
            if line.startswith("v "):  # Vertex line
                vertex = line.split()[1:]
                v_list.append(tuple(vertex))
            elif line.startswith("vn "):  # Normal line
                normal = line.split()[1:]
                vn_list.append(tuple(normal))
            elif line.startswith("vt "):  # UV line
                uv = line.split()[1:]
                vt_list.append(tuple(uv))
            elif line.startswith("mtllib "):  # mtllib line (materials)
                mtllib = line.split()[1]
                material_lib.add(mtllib)
            elif line.startswith("usemtl "):  # usemtl line (materials)
                materialName = line.split()[1]
                if materialName not in materials:
                    materialId = len(materials)
                    materials[materialName] = Material(materialId, materialName)

        # Pass two, process face and material groups
        file.seek(0)

        currentSurface = None

        currentVertexPos = 0

        for line in file:
            if line.startswith("usemtl "):
                materialName = line.split()[1]
                if materialName not in materials:
                    materialId = len(materials)
                    materials[materialName] = Material(materialId, materialName)
                currentSurface = Surface(materialName)
                surface_list.append(currentSurface)
            elif line.startswith("f "):  # Face line
                face = line.split()[1:]
                for vertex_spec in face:
                    v, vt, vn = vertex_spec.split("/")
                    vertex_index = (int(v), int(vt), int(vn))
                    if vertex_index not in vertex_dict:
                        vertex_dict[vertex_index] = len(vertex_dict)

                    # We add this 'index' to the array of indices
                    index_list.append(vertex_dict[vertex_index])

                    # No usemtl was issued before this vertex definition
                    # We must add a 'default' surface group
                    if currentSurface is None:
                        materials["default"] = Material(0, "default")
                        surface_list.append(Surface("default"))
                        currentSurface = surface_list[0]

                    if currentSurface.firstIndex == -1:
                        #currentSurface.firstIndex = vertex_dict[vertex_index]
                        currentSurface.firstIndex = currentVertexPos

                    currentSurface.indexCount = currentSurface.indexCount + 1
                    currentVertexPos += 1


        lines = []
        lines.append("# ldk mesh file")
        lines.append("# Format for complex types")
        lines.append("# vertex           pos_x pos_y pos_z normal_x normal_y normal_z uv_x uv_y")
        lines.append("# material         id name")
        lines.append("# surface          material_id index_start index_count")
        lines.append("# bounding_box     min_x min_y min_z max_x max_y max_z")
        lines.append("# bounding_sphere  center.x center.y center.z radius")
        lines.append("")

        lines.append("version 1.0")
        lines.append("vertex_format PNU")
        lines.append(f"vertex_count {len(vertex_dict)}")
        lines.append(f"index_count {len(index_list)}")
        lines.append(f"material_count {len(materials)}")
        lines.append(f"surface_count {len(surface_list)}")
        lines.append("")

        for vertex_id, vertex in vertex_dict.items():
            v, vt, vn = vertex_id
            pos = v_list[v-1]
            normal = vn_list[vn-1]
            uv = vt_list[vt-1]
            lines.append(f"vertex {pos[0]} {pos[1]} {pos[2]} {normal[0]} {normal[1]} {normal[2]} {uv[0]} {uv[1]}")

        lines.append("")
        lines.append(f"index_list {' '.join([str(x) for x in index_list])}")
        lines.append("")

        for material in materials.values():
            if not material.name.endswith(".material"):
                material.name += ".material"
            lines.append(f"material {material.id} {material.name}")
        lines.append("")

        for surface in surface_list:
            lines.append(f"surface {materials[surface.materialName].id} {surface.firstIndex} {surface.indexCount}")
        lines.append("")

        floatVertices = [tuple(float(item) for item in tpl) for tpl in v_list]
        boundingSphereCenter, boundingSphereRadius = computeBoundingSphere(floatVertices)
        lines.append(f"bounding_sphere {boundingSphereCenter[0]} {boundingSphereCenter[1]} {boundingSphereCenter[2]} {boundingSphereRadius}")

        boundingBox = computeBoundingBox(floatVertices)
        lines.append(f"bounding_box {boundingBox[0]} {boundingBox[1]} {boundingBox[2]} {boundingBox[3]} {boundingBox[4]} {boundingBox[5]}")

    with open(ldkMeshFileName, "w") as ldkMeshFile:
        ldkMeshFile.writelines('\n'.join(lines))


class ExportOBJWithOptionsOperator(bpy.types.Operator):
    bl_idname = "export.ldk_mesh_from_obj"
    bl_label = "LDK mesh"

    #export_materials: BoolProperty(
    #    name="Material color as vertex attribute",
    #    description="Export material colors as vertex attributes",
    #    default=True
    #)

    global_scale: FloatProperty(
            name="Global Scale",
            description="Scale factor for the exported objects",
            default=1.0,
            min=0.001,
            max=100.0
            )

    filepath: bpy.props.StringProperty(subtype='FILE_PATH')

    def execute(self, context):
        temp_file_path = self.filepath + ".temp"

        # Set the export options
        bpy.ops.export_scene.obj(
                filepath=temp_file_path,
                use_selection=False,  # Export selected objects
                use_materials=True,  # Write Materials
                use_triangles=True,  # Triangulate Faces
                use_normals=True,  # Include Normals
                use_uvs=True,  # Include UVs,
                use_mesh_modifiers=True,
                keep_vertex_order=True,
                use_animation=False,
                use_blen_objects=False,
                use_edges=False,
                use_smooth_groups=False,
                global_scale=self.global_scale,
                path_mode='AUTO',
                axis_forward='-Z',
                axis_up='Y'
                )

        objFileToLDKMesh(temp_file_path, self.filepath)

        # Delete the obj file
        os.remove(temp_file_path)

        return {'FINISHED'}

    def invoke(self, context, event):
        self.filepath = "object.mesh"
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}


def menu_func(self, context):
    self.layout.operator(ExportOBJWithOptionsOperator.bl_idname)


def register():
    bpy.utils.register_class(ExportOBJWithOptionsOperator)
    bpy.types.TOPBAR_MT_file_export.append(menu_func)


def unregister():
    bpy.utils.unregister_class(ExportOBJWithOptionsOperator)
    bpy.types.TOPBAR_MT_file_export.remove(menu_func)


if __name__ == "__main__":
    register()
