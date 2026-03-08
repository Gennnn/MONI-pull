bl_info = {
	"name": "Dropper",
	"description": "Saves info about objects in the scene.",
	"author": "Joseph Hocking www.newarteest.com",
	"blender": (2, 80, 0),
	"location": "File > Export > Dropper",
	"category": "Import-Export",
}

'''
install in Edit > Preferences > Add-ons
once installed: File > Export > Dropper
GPL license https://www.blender.org/support/faq/

Saves info about objects in the scene:
name, position, rotation, scale, custom properties
(ignore objects with ~[ somewhere in the name)
Select either XML or JSON for the data format.
'''


import bpy
import os
import json
import mathutils
import subprocess

# favor the accelerated C implementation, fallback to Python implementation
try:
	import xml.etree.cElementTree as et
except ImportError:
	import xml.etree.ElementTree as et
# http://eli.thegreenplace.net/2012/03/15/processing-xml-in-python-with-elementtree


def write_data(context, filepath, format):
    print("writing scene data...")
    
    data = {
        'OPT_JSON': build_json(),
        'OPT_XML': build_json(),
    }[format]
    
    f = open(filepath, 'w', encoding='utf-8')
    f.write(data)
    f.close()

    return {'FINISHED'}
#----- end func -----


def build_json():
    dict = {}
    dict["entities"] = []
    
    for obj in bpy.data.objects:
        if "~[" not in obj.name:
            entity = {"name":obj.name}

            row_world = obj.matrix_world.transposed()
            converted = mathutils.Matrix.Identity(4)
            converted[0][0:3] = row_world[0][0], row_world[0][2], row_world[0][1]
            converted[1][0:3] = row_world[1][0], row_world[1][2], row_world[1][1]
            converted[2][0:3] = row_world[2][0], row_world[2][2], row_world[2][1]
            converted[3][0:3] = row_world[3][0], row_world[3][2], row_world[3][1]
            scaleZ = mathutils.Matrix.Scale(-1.0, 4, (0.0, 0.0, 1.0))
            converted = scaleZ.transposed() @ converted  

            entity["matrix"] = []

            for i in converted:
                entity["matrix"].append(i[0])
                entity["matrix"].append(i[1])
                entity["matrix"].append(i[2])
                entity["matrix"].append(i[3])

            entity["is_mesh"] = False

            if obj.type == "MESH":
                entity["is_mesh"] = True
                entity["bounds"] = []
                bbox_corners = [mathutils.Vector(corner) for corner in obj.bound_box]
                for corner in bbox_corners:
                    # convert corners to vulkan/d3d coordinates
                    #corner[1], corner[2] = corner[2], corner[1]
                    cornerArrayForm = [corner[0], corner[1], corner[2]] # Makes parsing in the solution easier (maybe)
                    entity["bounds"].append(cornerArrayForm)
                
        
            # custom properties
            for k in obj.keys():
                if k not in "_RNA_UI":
                    prop = obj[k]
                    if isinstance(prop, int) or isinstance(prop, float) or isinstance(prop, str):
                        entity[k] = obj[k]
            dict["entities"].append(entity)

    model_export()
        
    return json.dumps(dict, indent=4, sort_keys=True)
#----- end func -----


def build_xml():
    root = et.Element("entities")
    
    for obj in bpy.data.objects:
        if "~[" not in obj.name:
            e = et.Element("entity")
            et.SubElement(e, "name").text = obj.name
            et.SubElement(e, "position").attrib = {"x":str(obj.location.x), "y":str(obj.location.y), "z":str(obj.location.z)}
            et.SubElement(e, "rotation").attrib = {"x":str(obj.rotation_euler.x), "y":str(obj.rotation_euler.y), "z":str(obj.rotation_euler.z)}
            et.SubElement(e, "scale").attrib = {"x":str(obj.scale.x), "y":str(obj.scale.y), "z":str(obj.scale.z)}
            # custom properties
            for k in obj.keys():
                if k not in "_RNA_UI":
                    prop = obj[k]
                    if isinstance(prop, int) or isinstance(prop, float) or isinstance(prop, str):
                        et.SubElement(e, k).text = str(obj[k])
            root.append(e)
    
    indent(root)
    return et.tostring(root).decode("utf-8")

#http://effbot.org/zone/element-lib.htm#prettyprint
def indent(elem, level=0):
    i = "\n" + level*"    "
    if len(elem):
        if not elem.text or not elem.text.strip():
            elem.text = i + "    "
        if not elem.tail or not elem.tail.strip():
            elem.tail = i
        for elem in elem:
            indent(elem, level+1)
        if not elem.tail or not elem.tail.strip():
            elem.tail = i
    else:
        if level and (not elem.tail or not elem.tail.strip()):
            elem.tail = i
#----- end func -----

def patch_mtl_normal_map(mtl_path):
    if not os.path.isfile(mtl_path):
        return

    patched = []
    with open(mtl_path, "r", encoding="utf-8") as f:
        for line in f:
            s = line.strip()
            lower = s.lower()

            
            if lower.startswith("map_bump") or lower.startswith("map_bump"):
                toks = s.split()
                filename = toks[-1]  # last token is the texture path
                patched.append(f"bump {filename}\n")
            else:
                patched.append(line)

    with open(mtl_path, "w", encoding="utf-8") as f:
        f.writelines(patched)

def model_export():
    # *NEW* save any unique MESH data as .obj and .mtl files 
    print("----------Begin Model Export----------")

    path = os.path.dirname(bpy.data.filepath)

    # Set the directory to save the .obj files
    export_dir = path + "\\Models"

    # Create the export directory if it does not exist
    if not os.path.exists(export_dir):
        os.makedirs(export_dir)

    # Create a set to store unique object names
    unique_names = set()

    # Iterate over all scene objects
    for obj in bpy.context.scene.objects:
    # Check if the object has mesh data
        if obj.type == 'MESH':
            # remove duplicate naming from the model
            base_name, extension = os.path.splitext(obj.name)
        # Check if the object is a unique instance
        if base_name not in unique_names:
            # Add the object name to the set of unique names
            unique_names.add(base_name)
            # IMPORTANT: This script filters duplicates therefore
            # modified duplicate geometry/materials will be ignored
            
            # Select the object and set it as the active object
            bpy.context.view_layer.objects.active = obj
            obj.select_set(True)

            # save the object's marix
            save = obj.matrix_world.copy()

            # Set the object's matrix to the identity matrix
            obj.matrix_world.identity()

            # Export the object as an .obj file
            obj_file = os.path.join(export_dir, f"{base_name}.obj")
            bpy.ops.wm.obj_export(filepath=obj_file,
                export_selected_objects=True,
                export_materials=True,
                export_normals=True,
                export_triangulated_mesh=True,
                apply_modifiers=True,
                # need to look into why these had to be set this way
                forward_axis='Y',
                up_axis='Z',
                path_mode='STRIP'
                )

            mtl_file = os.path.join(export_dir, f"{base_name}.mtl")
            patch_mtl_normal_map(mtl_file)

            # Restore the object's original matrix
            obj.matrix_world = save
            
            # Deselect the object
            obj.select_set(False)

    print("----------End Model Export----------")

    # *NEW* Convert .obj and .mtl files to .h2b files if tool is available
    print("----------Begin Model Conversion----------")

    # Look for my_executable.exe in the export directory
    exe_path = os.path.join(export_dir, "Obj2Header v1.9d.exe")
    if os.path.isfile(exe_path):
        # Run the executable if it is present
        os.chdir(export_dir)
        process = subprocess.Popen([exe_path], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        output, errors = process.communicate()
    # delete any generated header files
    for header in unique_names:
        # os.remove(os.path.join(export_dir, f"{header}.obj"))
        os.remove(os.path.join(export_dir, f"{header}.h"))

    print("----------End Model Conversion----------")
#----- end func -----

#----- begin gui -----

# ExportHelper is a helper class, defines filename and
# invoke() function which calls the file selector.
from bpy_extras.io_utils import ExportHelper
from bpy.props import StringProperty, BoolProperty, EnumProperty
from bpy.types import Operator


class Dropper(Operator, ExportHelper):
    """Dropper - Saves info about objects in the scene."""
    bl_idname = "dropper.scene_text"
    bl_label = "Export Scene Data"

    # ExportHelper mixin class uses this
    filename_ext = ".json"

    filter_glob: StringProperty(
        default="*.json",
        options={'HIDDEN'},
        maxlen=255,  # Max internal buffer length, longer would be clamped.
    )

    # options menu next to the file selector
    data_format: EnumProperty(
        name="Data Format",
        description="Choose the data format",
        items=(('OPT_JSON', "JSON", "JavaScript Object Notation"),
               ('OPT_XML', "XML", "eXtensible Markup Language")),
        default='OPT_JSON',
    )

    def execute(self, context):
        return write_data(context, self.filepath, self.data_format)


def menu_func_export(self, context):
    self.layout.operator(Dropper.bl_idname, text="Dropper")


def register():
    bpy.utils.register_class(Dropper)
    bpy.types.TOPBAR_MT_file_export.append(menu_func_export)


def unregister():
    bpy.utils.unregister_class(Dropper)
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)


if __name__ == "__main__":
    register()

    # test call
    #bpy.ops.dropper.scene_text('INVOKE_DEFAULT')
