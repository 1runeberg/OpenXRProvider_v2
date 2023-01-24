# Sample 10 - OpenXR Workshop

**WIP** – Early demo of the openxr provider library’s application template (generated via …./ .py script).

[Video here]

Main mechanics (to date)
(1)	XR Portal Level Transitions: Uses “in-world” portals  - think Star Trek Holodeck or Dr Strange portals. Pretty compelling in passthrough imo :)

(2)	Smooth locomotion with floor grid guide: Guides are beneficial moving around your virtual space, especially in passthrough where traditionally you’re stuck with roomscale space you have in the physical world.

|Action|Controller Input|Handtracking|
|:----|:----|:----|
|Summon Portal|Primary Button (A/X)|Clap|
|Shear (Open) Portal|Press and hold Trigger on both hands and spread hands apart|Index and Thumb tips together on both hands, then spread hands apart.|
|Enable Locomotion guides (smooth, hmd oriented)|Touch primary axis control (e.g. thumbstick, trackpad)|Middle and Thumb tips together on both hands|
|Smooth locomotion|Primary axis control (e.g. thumbstick, trackpad)|Middle and Thumb tips together on both hands. Movement will start immediately towards hmd orientation.|
|Toggle between color & mono passthrough modes|Secondary button (e.g. B/Y)|None|

