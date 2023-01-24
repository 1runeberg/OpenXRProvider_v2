# Sample 10 - OpenXR Workshop

**WIP** – Early demo of the openxr provider library’s application template (generated via ../app_template/openxr_template/app_template/openxr_template.py script).

- Pertinent portal source code can be found at: `src/Workshop.cpp, src/Workshop.hpp`

- Pertinent portal assets (blender, affinity photo files, etc) can be found at: `/assets_src/...`


https://user-images.githubusercontent.com/17371351/214259239-14cb7ddf-ac27-43b9-b302-a210fd018dca.mp4

**Main mechanics (to date)**
1. XR Portal Level Transitions: Uses “in-world” portals  - think Star Trek Holodeck or Dr Strange portals. Pretty compelling in passthrough imo :)

2. Smooth locomotion (other options and locomotion types soon) with floor grid guide: Guides are beneficial moving around your virtual space, especially in passthrough where traditionally you’re stuck with roomscale space you have in the physical world.

|Action|Controller Input|Handtracking|
|:----|:----|:----|
|Summon Portal|Primary Button (A/X)|Clap|
|Shear (Open) Portal|Press and hold Trigger on both hands and spread hands apart|Index and Thumb tips together on both hands, then spread hands apart.|
|Enable Locomotion guides (smooth, hmd oriented)|Touch primary axis control (e.g. thumbstick, trackpad)|Middle and Thumb tips together on both hands|
|Smooth locomotion|Primary axis control (e.g. thumbstick, trackpad)|Middle and Thumb tips together on both hands. Movement will start immediately towards hmd orientation.|
|Toggle between color & mono passthrough modes|Secondary button (e.g. B/Y)|None|

