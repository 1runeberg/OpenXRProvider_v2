# OpenXR Provider Library v2
# Sample 06 - Finger Painting

Demonstrates use of multi-vendor extensions (Handtracking) and single vendor (Meta/FB Passthrough).

Designed for the Quest Pro on standalone mode - this shows how to unlock the potential of new devices with OpenXR. 
However, also demonstrates how your app's device support can be made too narrow.

In the next demo app, we'll build this sample further and introduce the OpenXR Input system. We'll backup interaction
paths with the input system to expand our list of supported devices.

To build and find pre-built apks, visit the main project readme: https://github.com/1runeberg/OpenXRProvider_v2

GESTURES:
- *Either hand*: Index tip and thumb tibs together to draw


- *Two-handed*: Middle finger tip and thumb tips together on BOTH hands, spread apart to scale skybox up, inwards to scale skybox down. Scale is commensurate to distance of thumb tips.


- *Two-handed*: Ring finger tip and thumb tips together on BOTH hands, spread apart to add saturation, inwards for less. Saturation is commensurate to distance of thumb tips.

https://user-images.githubusercontent.com/17371351/208169673-5f6e63f2-b7fc-4a38-b1dc-7d7e5c55f4cb.mp4

