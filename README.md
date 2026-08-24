# RPG Camera

A modular top-down / ARPG camera toolkit for **Unreal Engine 5.8**. Core logic is C++, but
everything is exposed to Blueprints — you never have to write C++ to use it.

| Class | Type | Purpose |
|---|---|---|
| `URPGCameraComponent` | `USpringArmComponent` | Camera control: follow, pan, rotate, zoom, pitch, FOV, material parameters |
| `ARPGCameraManager` | `APlayerCameraManager` | Single Blueprint entry point to the active camera |
| `UOcclusionFadeComponent` | `UActorComponent` | Fades meshes that block the view of your character |
| `AOcclusionFadeGroup` | `AVolume` | Makes everything inside it fade as one group |
| `UOcclusionSubsystem` | `UWorldSubsystem` | Actor→group index the fade sweep queries. Automatic |

The camera and the occlusion fade are independent of each other — use either, or both.

---

## Installation

1. Copy the `RPGCamera` folder into `YourProject/Plugins/`.
2. Right-click your `.uproject` → **Generate Visual Studio project files**.
3. Build, then launch. Enable the plugin in **Edit → Plugins → Camera** if it isn't already.

The plugin requires the **Enhanced Input** plugin, which ships enabled by default in 5.8.

---

## Camera setup

On your character Blueprint:

1. Add a **RPG Camera** component to the root.
2. Add a **Camera** component as a child of it.

That's it — the arm finds the camera automatically and takes over its FOV.

A good starting configuration for a Diablo-style feel:

| Setting | Value |
|---|---|
| Min / Max Arm Length | 600 / 1600 |
| Default Arm Length | 1100 |
| Link Pitch To Zoom | true |
| Pitch At Min Zoom | -40 |
| Pitch At Max Zoom | -60 |
| Yaw Mode | Stepped |
| Yaw Step Angle | 45 |
| Follow Interp Speed | 12 |

### Why a spring arm subclass?

`URPGCameraComponent` switches itself to **absolute location and rotation** at BeginPlay.
That means it ignores its parent's transform and positions itself every frame instead. You get
camera lag, target leading, and fully detached free roam without spawning a second actor, and
the component still behaves like a normal spring arm in the editor viewport.

### Zoom

Two modes:

- **Continuous** (default) — `Add Zoom Input` slides the arm by `Zoom Step` per notch.
- **Discrete** — set `Use Discrete Zoom Levels`. One notch moves one level. Levels are spread
  evenly between min and max, or you can list exact distances in `Custom Zoom Levels`.

### Rotation

`Yaw Mode` controls how rotation input is handled:

- **Locked** — rotation disabled, yaw pinned to `Default Yaw`.
- **Continuous** — `Add Yaw Input` scales by `Yaw Speed`, for click-drag or gamepad stick.
- **Stepped** — `Add Yaw Steps(1)` / `(-1)` for quarter-turn style snapping. Set
  `Yaw Step Angle` to 90 for true quarter turns, 45 for eighths.

`Auto Recenter Yaw` drifts back to `Default Yaw` after `Yaw Recenter Delay` seconds.

### Pitch and FOV

Both can be driven by the zoom level. `Pitch At Min Zoom` is the angle when fully zoomed in
(flatter, more dramatic), `Pitch At Max Zoom` when fully out (steeper, more tactical). The
optional `Pitch Blend Curve` remaps normalized zoom (0–1) before the blend if you want
non-linear falloff. `Link FOV To Zoom` works the same way.

Leave `Link Pitch To Zoom` off to use a fixed `Default Pitch`.

### Free roam

Calling `Add Pan Input` while `Pan Switches To Free Roam` is on detaches the camera from the
character. `Auto Return To Target` brings it back after a delay, or call `Snap To Target`.
`Enable Edge Pan` adds RTS-style cursor-at-the-screen-edge panning.

`Clamp To Bounds` keeps the focus point inside an axis-aligned box.

---

## Camera manager setup

1. Create a Blueprint child of `ARPGCameraManager` (or use the class directly).
2. On your PlayerController, set **Player Camera Manager Class** to it.

Now any Blueprint can do `Get Player Camera Manager → Cast To RPG Camera Manager →
Add Zoom Input`, without needing a pawn reference. The manager re-resolves its cached
components automatically whenever the view target changes, so respawning and possession
changes are handled for you.

All passthrough functions no-op safely when no camera is resolved.

---

## Material parameters

The camera can publish its own state into a **Material Parameter Collection** every frame.
That lets materials do their own view-obstruction work — cylinder cutouts around the player,
height clipping, distance falloff — with no traces, no dynamic material instances, and no
per-mesh component. One global vector is usually all a cutout shader needs.

This is independent of the occlusion fade component below. Use either, or both.

### Setup

1. Create a Material Parameter Collection asset with the scalar and vector parameters your
   materials read.
2. On the RPG Camera component, set **Parameter Collection** to it.
3. Fill in **Vector Parameters** / **Scalar Parameters**. Each entry is a parameter name plus
   the camera value that feeds it.

The plugin never assumes parameter names — you map every one yourself, so the collection can
follow whatever naming your project already uses.

### Vector sources

| Source | Value |
|---|---|
| `Camera To Target` | Target location minus camera location, in world units |
| `Camera To Target (Normalized)` | The same, unit length |
| `Camera To Target (Horizontal)` | As above with Z zeroed, for cutouts that ignore height |
| `Camera To Target (Horizontal, Normalized)` | Horizontal, unit length |
| `Target To Camera` | The reverse direction |
| `Target To Camera (Normalized)` | The reverse direction, unit length |
| `Camera Location` | World location of the camera |
| `Target Location` | World location of the follow target |
| `Focus Location` | What the camera is looking at — differs from the target during free roam |
| `Camera Forward` | Camera forward vector |
| `Constant` | A fixed value you type in |

### Scalar sources

| Source | Value |
|---|---|
| `Arm Length` | Current arm length in world units |
| `Normalized Zoom` | 0 at min arm length, 1 at max |
| `Distance To Target` | Straight-line camera-to-target distance |
| `Horizontal Distance To Target` | The same, ignoring height |
| `Target Z` | World Z of the follow target, handy as a clip plane height |
| `Camera Z` | World Z of the camera |
| `Pitch` / `Yaw` | Current camera angles in degrees |
| `Field Of View` | Current FOV in degrees |
| `Constant` | A fixed value you type in |

`Constant` exists so the tuning values your shader needs can live in the same list as the
driven ones, rather than being split between the collection's defaults and the camera. Scalars
also have a **Square Value** flag, which feeds the squared parameters shaders use to compare
distances without a `sqrt`.

### Notes

Parameters are pushed at the end of the camera's tick, after its transform has been applied,
so materials always read the current frame's position. They're seeded once at `BeginPlay` too,
so the first rendered frame isn't using stale defaults. Call `Update Material Parameters`
manually if you move the camera outside the normal tick and need a mid-frame refresh.

A name that doesn't exist in the collection logs a warning once, not once per frame.

Whether your shader wants `Camera To Target` or `Target To Camera` depends on how it builds
its cutout. If the effect lands on the wrong side of the character, flip the source.

---

## Occlusion fade setup

Add an **Occlusion Fade** component to your character. It sweeps a sphere from the character
back toward the camera and fades whatever it hits.

This is the trace-driven alternative to the material parameter approach above: it picks out
individual occluding meshes and fades each one, rather than letting every material decide for
itself. It costs traces but needs no material authoring beyond a single opacity input.

### Trace channel

By default the sweep uses **Visibility**. Because walls *block* that channel, detecting walls
stacked behind other walls takes extra sweep passes (the component automatically re-sweeps past
each blocker, up to 8 passes). For single-pass detection and tighter control over what can fade,
create a dedicated channel:

1. **Project Settings → Engine → Collision → Trace Channels → New Trace Channel** — name it
   e.g. `CameraFade`, default response **Overlap**.
2. Set the component's `Trace Channel` to `CameraFade`.
3. Optionally set floors and landscape to **Ignore** `CameraFade` so they never show up at all.

On an overlap channel nothing blocks the sweep, so every occluder comes back in one pass.
Plugins can't ship collision channels (the `ECC_GameTraceChannel` slots belong to your
project), which is why this is a recommended setup step rather than the default.

### Choosing a fade method

**Custom Primitive Data** (default, recommended) — writes the alpha into a float slot on the
primitive. No dynamic material instances, works with instanced static meshes, cheapest option.
Requires a small material change, see below.

**Material Parameter** — creates dynamic material instances and drives a named scalar. Costs
more memory and creates a MID per material slot, but needs no Custom Primitive Data setup.
Set `Fade Parameter Name` to match your material's scalar parameter.

**Hide Component** — no material work at all. The mesh pops out rather than fading.
`Keep Shadows When Hidden` preserves its shadow so the lighting still reads correctly.

**Interface Only** — the plugin does no rendering work and just fires the `IFadeableTarget`
events so you can do whatever you like.

### Material setup for Custom Primitive Data

In the material your walls use:

1. Set **Blend Mode** to `Masked` and enable **Dither Opacity Mask**, or set it to
   `Translucent` if you prefer true transparency.
2. Add a **Custom Primitive Data** node, set its index to match `Custom Primitive Data Index`
   (default `0`).
3. Wire it into **Opacity Mask** (or **Opacity** for translucent).

Dithered masked is usually the better choice — it's far cheaper than translucency and sorts
correctly, which matters when several walls overlap.

> One caveat: a primitive with no custom primitive data set yet reads `0` from that slot, which
> would make it invisible on load. Set the default value to `1` on the mesh component's
> **Custom Primitive Data Defaults** array in the details panel (or on the material's
> **Material Property Overrides**). The component writes opacity directly — `1` is opaque,
> `0` is faded — so do **not** invert the value in the material. Whatever value the slot held
> before a fade is restored when the fade ends.

### Filtering

By default everything that blocks the trace fades. To narrow it down:

- `Ignored Actor Classes` — add your landscape or floor class here first.
- `Required Actor Tags` — only fade actors tagged e.g. `Fadeable`.
- `Required Component Tags` — only fade individual mesh components carrying one of these
  tags, for opting in specific meshes on a multi-mesh actor. Combines with
  `Required Actor Tags` when both are set (both must match).
- `Ignored Actor Tags` / `Ignored Component Tags` — targeted exclusions.
- `Require Fadeable Interface` — strictest option, only fades actors implementing
  `IFadeableTarget`.

### Occlusion fade groups

By default each mesh fades on its own, so a sweep into a building punches a hole through
whichever wall happens to be in the way while the roof and the rest of the walls stay solid.
An **Occlusion Fade Group** volume fixes that: drop one over the building and when the sweep
crosses the volume - or hits any single member - every member fades together.

1. Place an **Occlusion Fade Group** actor from the Place Actors panel and shape the brush
   around the geometry you want treated as one object.
2. Press **Refresh Members** in the details panel to see the `Member Count` it resolves to.
3. Play. Anything inside now fades and returns as a unit.

**The volume is itself a trigger.** `Volume Triggers Fade` is on by default: the group fades
whenever the sweep passes through the brush, whether or not it touches a member mesh. That
covers the cases where the sweep slips through the building without hitting anything solid - a
doorway, a gap between pillars, a window, or a wall on a channel it ignores - and it fires the
group events at the volume boundary rather than at the first wall. Turn it off for the old
behaviour, where only a hit on a member starts the fade.

Two things follow from that. The sweep starts at the character, so *standing inside the volume
counts as crossing it* and the group fades while you are indoors - usually what you want for a
roof, so scope the brush to the part that should vanish. And the brush needs query collision to
be seen: the actor sets that up itself (query-only, overlapping every channel), so don't switch
the volume to `NoCollision`. The brush is never faded itself.

Membership is *volume overlap + `Additional Members` − `Excluded Actors`*:

- `Capture Overlapping Actors` — on by default. Turn it off to use the volume purely as a
  hand-picked list.
- `Capture Actor Tags` / `Capture Actor Classes` — narrow what the volume swallows, so props
  and NPCs inside a room don't join the building group.
- `Capture Tolerance` — how far outside the brush a mesh may sit and still count (default
  50cm). Covers eaves and trim that poke through the wall you drew.
- `Additional Members` — actors that live outside the volume entirely, e.g. a detached
  balcony.
- `Excluded Actors` — the floor, or anything else the volume would otherwise swallow.

An actor is matched by its own origin *or* by any of its mesh components' bounds centres, so
modular pieces whose pivot sits at a corner or on the floor below still get picked up.

Membership resolves once on **Begin Play**. Call `Refresh Members` after spawning or
streaming in geometry that should join, or `Add Member` / `Remove Member` for one-offs. An
actor may belong to more than one group; all of them fade.

**Per-group appearance.** Turn on `Override Fade Settings` and the group's own `Fade Settings`
replace the fade component's for its members — method, faded alpha, and both speeds. A roof
group can hard-hide with **Hide Component** while a tree group ghosts at 0.2, all driven by
one fade component on the character.

**Events.** The group actor exposes `On Group Began Occluding` and `On Group Stopped
Occluding`, plus an `Is Occluding` getter — handy for swapping an interior lighting setup or
enabling a minimap overlay when the player walks behind a building.

The fade component's `Use Occlusion Groups` toggle turns the whole mechanism off if you want
the old per-mesh behaviour. The component's filters still apply to group members, so an actor
tagged in `Ignored Actor Tags` stays solid even inside a group.

### The IFadeableTarget interface

Optional. Implement it on an actor to get:

- `On Fade Out Begin` / `On Fade In Begin` — fired once per transition.
- `On Fade Alpha Changed(Primitive, Alpha)` — every frame while the alpha moves. `Alpha` is
  1 for opaque, 0 for fully faded.
- `Can Be Faded` — return false to have the fade component skip this actor entirely.

### Performance

`Trace Interval` throttles detection (default 0.05s = 20 sweeps/sec) while alphas still
interpolate every frame, so it stays smooth. Raise it to 0.1 on dense scenes. Detection is a
`SweepMultiByChannel` — a single pass on an overlap channel (see *Trace channel* above), or
repeated past each blocking wall (capped at 8 passes) on blocking channels like Visibility so
stacked occluders all fade. Cost scales with how much geometry sits between the camera and the
character, not with world size.

Groups add a hash lookup per hit actor and then walk the triggered group's members, so cost
scales with group size, not group count — the actor→group index is built once at `Begin Play`.
Volume triggering is free: the brush already comes back as an overlap in the same sweep.
The one expensive moment is `Refresh Members`, which iterates every actor in the world; that's
fine on Begin Play or on a level-streaming callback, not every frame.

---

## Blueprint quick reference

```
// Mouse wheel
Camera Manager → Add Zoom Input (Axis Value)

// Q / E quarter turns
Camera Manager → Add Yaw Steps (1) / (-1)

// WASD free-roam panning
Camera Manager → Add Pan Input (Vector2D)

// Recenter on the character
Camera Manager → Snap To Target

// Cutscene: look at something else, then come back
Camera Manager → Set Follow Target (TargetActor, true)
Camera Manager → Set Follow Target (PlayerPawn, false)

// Read a camera value directly, without going through the collection
RPG Camera → Resolve Vector Source (Camera To Target)
RPG Camera → Get Camera Location / Get Target Location
```

---

## Notes

This is an independent implementation written from a feature description. It shares no code
with any previously published plugin. Use it however you like in your own projects.
