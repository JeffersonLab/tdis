# (!) This script must be used in Blender 3d
# How to Run in Blender:
#  - Open Blender (version 2.8+ recommended).
#  - Switch to the Scripting workspace (usually a tab at the top).
#  - Click New in the Text Editor area to create a new text block.
#  - Paste the entire script below into the text block.
#  - Click Run Script (on top of the text editor). You should see the geometry appear in the 3D Viewport.
# Alternatively, you can run via the command line:
#    blender --python your_script_name.py

import bpy
import math
import mathutils

# =============================================================================
# 1) Create TPC plane with ring/pad -> face index map
# =============================================================================

def create_readout_plane_with_map(name,
                                  x_loc=-5.0,
                                  inner_radius=5.0,
                                  outer_radius=15.0,
                                  n_rings=21,
                                  pads_per_ring=122):
    """
    Builds an annular TPC plane at x=x_loc, subdivided into (n_rings, pads_per_ring).
    Returns (plane_obj, face_map) where face_map is dict{(ring_idx,pad_idx) : face_index}.

    We also:
      - Add 2 materials (slot0=main grey, slot1=highlight)
      - Add a wireframe modifier with use_replace=False (so faces remain).
    """
    from math import sin, cos, pi

    mesh_verts = []
    mesh_faces = []
    face_map = {}  # (ring_idx, pad_idx) -> face_index
    face_index = 0
    vert_index = 0

    ring_width = (outer_radius - inner_radius) / n_rings
    dphi = 2.0*pi / pads_per_ring

    for ring_idx in range(n_rings):
        r_in  = inner_radius + ring_idx*ring_width
        r_out = inner_radius + (ring_idx+1)*ring_width

        # offset for odd rings
        phi_offset = 0.5*dphi if (ring_idx % 2)==1 else 0.0

        for pad_idx in range(pads_per_ring):
            phi1 = phi_offset + pad_idx*dphi
            phi2 = phi_offset + (pad_idx+1)*dphi

            y1, z1 = r_in*cos(phi1),  r_in*sin(phi1)
            y2, z2 = r_in*cos(phi2),  r_in*sin(phi2)
            y3, z3 = r_out*cos(phi2), r_out*sin(phi2)
            y4, z4 = r_out*cos(phi1), r_out*sin(phi1)

            mesh_verts.extend([
                (x_loc, y1, z1),
                (x_loc, y2, z2),
                (x_loc, y3, z3),
                (x_loc, y4, z4),
            ])
            mesh_faces.append((vert_index, vert_index+1, vert_index+2, vert_index+3))

            face_map[(ring_idx, pad_idx)] = face_index
            face_index  += 1
            vert_index += 4

    # Create and update mesh data
    mesh_data = bpy.data.meshes.new(name + "_mesh")
    mesh_data.from_pydata(mesh_verts, [], mesh_faces)
    mesh_data.update()

    plane_obj = bpy.data.objects.new(name, mesh_data)
    bpy.context.collection.objects.link(plane_obj)

    # Add 2 materials: slot0 = grey, slot1 = highlight
    mat_main = bpy.data.materials.new(name + "_MainMat")
    mat_main.diffuse_color = (0.6, 0.6, 0.6, 1.0)
    plane_obj.data.materials.append(mat_main)

    mat_high = bpy.data.materials.new(name + "_HighlightMat")
    mat_high.diffuse_color = (1.0, 1.0, 0.0, 1.0)  # yellow
    plane_obj.data.materials.append(mat_high)

    # Add wireframe modifier (keep faces)
    bpy.context.view_layer.objects.active = plane_obj
    bpy.ops.object.modifier_add(type='WIREFRAME')
    wire_mod = plane_obj.modifiers["Wireframe"]
    wire_mod.thickness = 0.05
    wire_mod.use_replace = False

    return plane_obj, face_map

def highlight_pad(plane_obj, face_map, ring_idx, pad_idx):
    """
    Highlight a specific pad face by assigning material_index=1 (highlight slot).
    """
    face_index = face_map.get((ring_idx, pad_idx), None)
    if face_index is None:
        print(f"No face for ring={ring_idx}, pad={pad_idx}")
        return
    if face_index >= len(plane_obj.data.polygons):
        print(f"Face index {face_index} out of range!")
        return

    plane_obj.data.polygons[face_index].material_index = 1

# =============================================================================
# 2) Proton trajectory simulation, create neon curve
# =============================================================================

def simulate_proton_trajectory(p_mev_c=200.0, b_tesla=1.5,
                               max_radius=15.0, max_steps=1000, dt=1.0e-11):
    """
    Euler integration for a proton in B=(b_tesla,0,0). Start (0,0,0),
    returns list of (x,y,z) in cm.
    """
    import mathutils
    Q  = 1.602176634e-19
    MP = 1.67262192369e-27
    C  = 3.0e8

    p_si = p_mev_c * 1.602176634e-13 / C
    v_mag = p_si / MP

    r = mathutils.Vector((0.0,0.0,0.0))
    v = mathutils.Vector((0.0, v_mag,0.0))
    B = mathutils.Vector((b_tesla, 0.0,0.0))

    points = []
    for _ in range(max_steps):
        x_cm, y_cm, z_cm = (r.x*100, r.y*100, r.z*100)
        points.append((x_cm,y_cm,z_cm))
        if (y_cm**2 + z_cm**2)**0.5 >= max_radius:
            break
        a = (Q/MP)*v.cross(B)
        v = v + a*dt
        r = r + v*dt

    return points

def create_curve_from_points(name, points):
    """
    Creates a polyline curve from a list of (x,y,z).
    """
    curve_data = bpy.data.curves.new(name+"_curve", type='CURVE')
    curve_data.dimensions = '3D'

    spline = curve_data.splines.new('POLY')
    spline.points.add(len(points)-1)
    for i,(x,y,z) in enumerate(points):
        spline.points[i].co = (x, y, z, 1.0)

    curve_obj = bpy.data.objects.new(name, curve_data)
    bpy.context.collection.objects.link(curve_obj)
    return curve_obj

def make_curve_neon(curve_obj, color=(1.0,0.0,1.0,1.0), strength=20.0, thickness=0.1):
    """
    Gives the curve a 'neon' emission material, a thicker bevel,
    and tries to disable cast shadows on old or new Blender versions.
    """
    # Thicken
    curve_obj.data.bevel_depth = thickness
    curve_obj.data.bevel_resolution = 6

    # Emission material
    mat = bpy.data.materials.new(curve_obj.name + "_NeonMat")
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links

    # remove Principled if present
    for n in nodes:
        if n.type == 'BSDF_PRINCIPLED':
            nodes.remove(n)
    # Add Emission node
    em_node = nodes.new('ShaderNodeEmission')
    em_node.inputs["Color"].default_value = color
    em_node.inputs["Strength"].default_value = strength

    out_node = nodes.get("Material Output")
    if not out_node:
        out_node = nodes.new("ShaderNodeOutputMaterial")
    links.new(em_node.outputs["Emission"], out_node.inputs["Surface"])

    curve_obj.data.materials.append(mat)

    # Try to disable cast shadows in Cycles
    # Some Blender versions may not have 'cycles_visibility'
    if hasattr(curve_obj, 'cycles_visibility'):
        curve_obj.cycles_visibility.shadow = False

    # Also handle Eevee's per-material shadow setting
    for slot in curve_obj.material_slots:
        if hasattr(slot.material, 'shadow_method'):
            slot.material.shadow_method = 'NONE'

# =============================================================================
# 3) Cylinder with no caps + "curved patch"
# =============================================================================

def create_cylindrical_surface_x(name="CylSurf", radius=10.0, length=10.0, center_x=0.0, alpha=0.2):
    """
    Creates a cylinder oriented along X, radius=..., length=..., no caps, semitransparent.
    """
    bpy.ops.mesh.primitive_cylinder_add(
        radius=radius,
        depth=length,
        end_fill_type='NOTHING',
        location=(0,0,0)
    )
    cyl_obj = bpy.context.active_object
    cyl_obj.name = name

    # rotate 90 deg about Y => along X
    cyl_obj.rotation_euler[1] = math.radians(90)
    cyl_obj.location.x = center_x
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    mat = bpy.data.materials.new(name + "_Mat")
    mat.use_nodes = True
    mat.blend_method = 'BLEND'
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Alpha"].default_value = alpha
        bsdf.inputs["Base Color"].default_value = (0.2, 0.8, 1.0, 1.0)
    cyl_obj.data.materials.append(mat)

    return cyl_obj

def find_trajectory_cylinder_intersection(traj_points, radius=10.0, x_min=-5.0, x_max=5.0):
    """
    Returns (x,y,z) of first crossing with cylinder radius about X-axis,
    restricting x in [x_min, x_max], or None if no crossing.
    """
    R2 = radius**2
    for i in range(len(traj_points)-1):
        x1,y1,z1 = traj_points[i]
        x2,y2,z2 = traj_points[i+1]
        r1_sq = y1*y1 + z1*z1
        r2_sq = y2*y2 + z2*z2

        crosses = (r1_sq < R2 and r2_sq > R2) or (r1_sq > R2 and r2_sq < R2)
        if not crosses:
            continue

        denom = (r2_sq - r1_sq)
        if abs(denom)<1e-12:
            continue

        t = (R2 - r1_sq)/denom
        if t<0 or t>1:
            continue

        x_int = x1 + t*(x2 - x1)
        y_int = y1 + t*(y2 - y1)
        z_int = z1 + t*(z2 - z1)

        if x_int>=x_min and x_int<=x_max:
            return (x_int, y_int, z_int)
    return None

def create_cylindrical_patch(name,
                             radius,
                             x_center,
                             phi_center,
                             dx=1.0,
                             dphi=0.05,
                             nphi=16,
                             offset_in_radius=0.01):
    """
    Creates a "curved rectangle" on the cylinder surface about X-axis:
      - extends +/- dx/2 in x
      - extends +/- dphi/2 in phi
      - resolution = nphi steps around the arc
      - offset_in_radius lifts it slightly above the cylinder to avoid z-fighting
    """
    import math

    Nx = 2  # single quad in X direction
    x1 = x_center - 0.5*dx
    x2 = x_center + 0.5*dx
    phi1 = phi_center - 0.5*dphi
    phi2 = phi_center + 0.5*dphi

    r_patch = radius + offset_in_radius

    verts_2d = []
    if nphi<2:
        nphi=2
    dphi_step = (phi2 - phi1)/(nphi-1)

    for i_phi in range(nphi):
        row = []
        cur_phi = phi1 + i_phi*dphi_step
        for i_x in range(Nx):
            x_val = x1 if i_x==0 else x2
            y_val = r_patch*math.cos(cur_phi)
            z_val = r_patch*math.sin(cur_phi)
            row.append((x_val, y_val, z_val))
        verts_2d.append(row)

    flat_verts = []
    for row in verts_2d:
        flat_verts.extend(row)

    faces = []
    def idx(ip, ix):
        return ip*Nx + ix

    for ip in range(nphi-1):
        v0 = idx(ip,0)
        v1 = idx(ip,1)
        v2 = idx(ip+1,1)
        v3 = idx(ip+1,0)
        faces.append((v0,v1,v2,v3))

    mesh_data = bpy.data.meshes.new(name+"_mesh")
    mesh_data.from_pydata(flat_verts, [], faces)
    mesh_data.update()

    patch_obj = bpy.data.objects.new(name, mesh_data)
    bpy.context.collection.objects.link(patch_obj)

    # Red color
    mat = bpy.data.materials.new(name+"_Mat")
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (1.0, 0.0, 0.0, 1.0)
        bsdf.inputs["Alpha"].default_value = 1.0
    patch_obj.data.materials.append(mat)

    return patch_obj

# =============================================================================
# 4) Utility to find ring/pad from r=..., phi=...
# =============================================================================

def find_ring_pad_for_r_phi(r, phi,
                            inner_r=5.0, outer_r=15.0,
                            n_rings=21, pads_per_ring=122):
    from math import floor, pi

    ring_width = (outer_r - inner_r)/n_rings
    ring_idx = int(math.floor((r - inner_r)/ring_width))
    ring_idx = max(min(ring_idx, n_rings-1), 0)

    dphi = 2.0*pi / pads_per_ring
    phi_mod = phi % (2*pi)

    # offset if ring is odd
    offset = 0.5*dphi if (ring_idx%2)==1 else 0.0
    phi_shift = (phi_mod - offset) % (2*pi)

    pad_idx = int(math.floor(phi_shift/dphi))
    pad_idx = max(min(pad_idx, pads_per_ring-1), 0)

    return ring_idx, pad_idx

# =============================================================================
# MAIN
# =============================================================================

def main():
    # Clear
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete()

    # 1) Create TPC plane with face map
    plane_obj, face_map = create_readout_plane_with_map(
        name="TPCPlane",
        x_loc=-5.0,
        inner_radius=5.0,
        outer_radius=15.0,
        n_rings=21,
        pads_per_ring=122
    )

    # 2) Simulate proton + neon curve (no shadows)
    trajectory = simulate_proton_trajectory()
    traj_curve = create_curve_from_points("ProtonTrajectory", trajectory)
    make_curve_neon(traj_curve, color=(1.0,0.0,1.0,1.0), strength=30.0, thickness=0.1)

    # 3) Cylinder surface
    cyl_obj = create_cylindrical_surface_x(
        name="VisualCylSurface",
        radius=10.0,
        length=10.0,
        center_x=0.0,
        alpha=0.2
    )

    # 4) Intersection with cylinder
    intersect = find_trajectory_cylinder_intersection(
        traj_points=trajectory,
        radius=10.0,
        x_min=-5.0,
        x_max=5.0
    )
    if not intersect:
        print("No intersection with cylinder in x=[-5,5].")
        return

    x_int, y_int, z_int = intersect
    print(f"Intersection => x={x_int:.2f}, y={y_int:.2f}, z={z_int:.2f}")

    # 5) Create patch on cylinder near intersection
    #    dphi for 1 pad in TPC => 2*pi / 122
    pad_dphi = 2.0*math.pi / 122
    phi_int = math.atan2(z_int, y_int)
    patch_obj = create_cylindrical_patch(
        name="CylPatch",
        radius=10.0,
        x_center=x_int,
        phi_center=phi_int,
        dx=1.0,          # 1 cm wide in X
        dphi=pad_dphi,   # one pad's angular width
        nphi=16,
        offset_in_radius=0.01
    )

    # 6) Highlight the matching pad
    r_int = math.hypot(y_int, z_int)
    ring_idx, pad_idx = find_ring_pad_for_r_phi(r_int, phi_int,
                                                inner_r=5.0,
                                                outer_r=15.0,
                                                n_rings=21,
                                                pads_per_ring=122)
    print(f"Matching pad => ring={ring_idx}, pad={pad_idx}")
    highlight_pad(plane_obj, face_map, ring_idx, pad_idx)

if __name__ == "__main__":
    main()
