# JungleRuins Scene Decisions

This file is append-only. Superseded decisions remain visible.

## JR-0001 — Scene contract is renderer independent

- Status: Accepted
- Date: 2026-07-27

The canonical scene preserves spatial, semantic, geometric, material, and
provenance information. It contains no renderer-specific batching, descriptor,
PSO, visibility-algorithm, or shader-layout data.

## JR-0002 — Blender and USD are complementary authorities

- Status: Accepted
- Date: 2026-07-27

Blender supplies authoring hierarchy and material/prototype context. USD
supplies the complete baked point-instancer assembly. A converter must report
conflicts instead of silently preferring one representation.

## JR-0003 — Preserve source cells

- Status: Accepted
- Date: 2026-07-27

The 16 `M_*` cinematic cells, 64 `E_*` extended cells, and Pyramid region are
stable scene entities. Their bounds are computed metadata, not renderer culling
instructions.

## JR-0004 — Keep prototypes shared and instances unrealized

- Status: Accepted
- Date: 2026-07-27

The canonical scene stores each unique prototype mesh once and stores scatter
placement as instance transforms. Realized geometry may exist only as an
explicitly documented derived artifact.

## JR-0005 — Separate fact, calculation, and inference

- Status: Accepted
- Date: 2026-07-27

Metadata is labeled `source`, `computed`, or `inferred`. Inferred semantic
labels require review and may be kept outside the master GLB until approved.

## JR-0006 — Logical split precedes physical split

- Status: Accepted
- Date: 2026-07-27

The master artifact is one logical scene so prototypes and materials are not
duplicated. Cinematic, streaming, and benchmark packages are derived later.

## JR-0007 — Spatial normalization must be reversible

- Status: Accepted
- Date: 2026-07-27

USD PointInstancers may aggregate several cinematic authoring systems. Instance
transforms may be assigned to source terrain cells using source cell bounds,
but every normalized record retains its USD layer, prim path, and source array
index.

## JR-0008 — Do not silently remove exact-origin records

- Status: Accepted
- Date: 2026-07-27

Exact-origin PointInstancer records are counted and reported separately.
Although repeated origin records may be exporter sentinels, the canonical
pipeline preserves them until that interpretation is verified.

## JR-0009 — Keep human documentation under the working-root docs directory

- Status: Superseded by JR-0015
- Date: 2026-07-27

Human-readable contracts, decisions, inventories, and validation summaries live
under the repository's `docs/jungle`. Generators, schemas, and their usage
notes live under `assets/scripts/jungle`. This decision originally placed
generated artifacts and a copied source scene in an external working directory;
JR-0015 replaced that part of the workflow.

## JR-0010 — Prove the contract with a scoped GLB before full-scene export

- Status: Accepted
- Date: 2026-07-27

The first GLB is a contract probe containing one complete source terrain cell
and one complete scatter system. It must use shared prototype meshes,
`EXT_mesh_gpu_instancing`, stable scene metadata, and source-array indices.
Root metadata declares that the scene and cell are incomplete, so the probe
cannot be mistaken for the final cinematic package.

## JR-0011 — Treat Blender GLB re-import expansion as consumer behavior

- Status: Accepted
- Date: 2026-07-27

Blender 4.2 expands `EXT_mesh_gpu_instancing` records into editable objects on
import. The renderer-facing scene contract remains the compact extension
accessors. A re-import test verifies transform counts and bounds, but its
expanded object hierarchy does not redefine the canonical GLB layout.

## JR-0012 — Test WebP before committing to the final texture encoding

- Status: Accepted for probe
- Date: 2026-07-27

The uncompressed-source inventory contains about 3.59 GiB of image files, and
the first six-mesh probe GLB was about 295 MB with automatic PNG/JPEG export.
The next probe uses embedded WebP at quality 85 without fallback to measure
size reduction. This does not approve lossy texture fidelity; alpha, normals,
and side-by-side appearance remain validation gates.

## JR-0013 — Use deterministic half-open transform ownership bounds

- Status: Accepted
- Date: 2026-07-27

Terrain mesh bounds have small floating-point gaps or overlaps at shared edges.
Transform assignment therefore uses the midpoint of neighboring bounds, with
inclusive lower edges and exclusive upper edges except at the outer region
boundary. This creates one owner for every transform without modifying its
source position or source-array identity.

## JR-0014 — The complete-cell probe validates transform-level splitting

- Status: Accepted
- Date: 2026-07-27

The `M_3x4_01` complete-cell probe contains its terrain and all five authored
scatter systems. It stores 74,206 transforms in 21
`EXT_mesh_gpu_instancing` sets and re-imports with the same count and cell
bounds. This validates the direct-cell and transform-level spatial-split paths
together. It does not yet validate final material appearance or define the
physical packaging of the full scene.

## JR-0015 — Build directly from the repository source package

- Status: Accepted
- Date: 2026-07-27

The immutable input is `assets/scenes/unpacked/JungleRuins`. The former
`visbufscene` copy is no longer part of the active workflow. Generated GLBs
and machine reports are written only below the ignored
`assets/scenes/generated/jungle` tree.

## JR-0016 — Use four region packages with logical cell boundaries

- Status: Accepted
- Date: 2026-07-27

The physical output is `global`, `cinematic`, `extended`, and `pyramid`.
Creating one standalone GLB per cell would repeatedly embed prototype
geometry and textures. A monolithic GLB approaches the 32-bit GLB container
limit and prevents region-level loading. Each region GLB therefore keeps its
authored cell and system nodes and shares prototype meshes inside the file.

## JR-0017 — Source data retains compact instance streams

- Status: Accepted
- Date: 2026-07-27

`SceneSourceData` stores one 44-byte TRS/source-index record per instance and
nodes reference contiguous instance ranges. It does not realize instances
into render objects or store per-instance AABBs. Cell and system boundaries
remain hierarchy data so a renderer can choose culling and batching later.

## JR-0018 — Embedded images are file ranges

- Status: Accepted
- Date: 2026-07-27

GLB images are represented by source file, byte offset, byte size, and encoded
format. This preserves the exact embedded WebP payload without duplicating it
in source-scene memory. Decoding and GPU upload belong to later layers.

## JR-0019 — Preserve unresolved exact-origin records

- Status: Accepted
- Date: 2026-07-27

Exact-origin records in a direct-cell source stay in that cell. If a spatially
split source has no deterministic cell owner, its exact-origin records are
stored below the region's unresolved-origin node with source array indices
intact.

## JR-0020 — Preserve every unowned spatial-split record

- Status: Accepted
- Date: 2026-07-27

Package validation found one non-origin Grass B record outside all 16
cinematic ownership bounds. Spatial splitting now computes the union of all
cell selections and places every unselected record below the region's
unresolved node. Metadata distinguishes `exact_origin` from
`outside_cell_ownership`, and complete-package coverage must equal the source
instance and exact-origin counts.

## JR-0021 — Make Jungle conversion an explicit source builder

- Status: Accepted
- Date: 2026-07-27

`JungleSceneSourceBuilder` owns the canonical Jungle GLB-to-`SceneSourceData`
conversion below `scene/builder/source`. It is not a generic glTF importer and
does not adapt Jungle to a renderer-owned structure. The source structs retain
the non-texture attributes and `extras.jr` identity needed by downstream
culling, material selection, and instance tracing.

## JR-0022 — Prove SourceData conversion by calling it on all packages

- Status: Accepted
- Date: 2026-07-27

`JungleSceneSourceValidate` loads the four generated region GLBs through the
public builder. It verifies complete source-index coverage, canonical totals,
optional geometry attributes, hierarchy metadata, unresolved records, camera
data, and the River specular color. The first complete run passed for all
8,674,676 instances and 778 source streams. This executable is an offline
contract check and is excluded from the default build.
