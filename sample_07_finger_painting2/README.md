# OpenXR Provider Library v2
# Sample 07 - Finger Painting 2

*"Use reality(ies) as your canvas!"*

Introduces the OpenXR Input System - this demo builds up on the single form factor and heavily vendor extension dependent previous sample (sample_06_finger_painting).

This demo uses the openxr input system to provide alternative input to the hand tracking gestures and provide platform-dependent hand proxy models:

| **Action**                    | **Handtracking gesture**                                                                                                                                                   | **Input system (controllers)**                                                                                                                                                                                                                                                                                     |
|-------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Paint                         | Index tip and thumb tibs together to draw (either hand)                                                                                                                    | Press either left or right trigger                                                                                                                                                                                                                                                                                 |
| Scale skybox                  | Middle finger tip and thumb tips together on BOTH hands,  spread apart to scale skybox up, inwards to scale skybox down.  Scale is commensurate to distance of thumb tips. | Left thumbstick up to scale up, down to scale down. For PC, right thumbstick will do the same                                                                                                                                                                                                                      |
| Cycle passthrough fx          | Clap                                                                                                                                                                       | None - see *Cycle panoramas* instead                                                                                                                                                                                                                                                                               |
| Cycle panoramas               | None - see *Cycle passthrough fx* instead                                                                                                                                  | Left or right primary button (e.g. A or X button depending on hand and controller)  For controllers without a A or X button, press the Menu button instead.                                                                                                                                                        |
| Adjust passthrough saturation | Ring finger tip and thumb tips together on BOTH hands,  spread apart to add saturation, inwards for less.  Saturation is commensurate to distance of thumb tips.           | For android only - right thumbstick up or down will adjust saturation. As of the time of coding this,  Oculus Desktop's passthrough is unreliable and no OpenXR ext support for passthrough exists in SteamVR.  So for PC, the right thumbstick will instead scale the skybox up or down, similar ot the left hand |
| Easter (Christmas?) egg       | With your left hand palm up, touch palm (somewhere near wrist) with your right index finger to generate a surprise!                                                        | Tip both controllers together towards the index fingers of the hand proxy meshes to generate a surprise! Haptics will also be generated.                                                                                                                                                                           |

To build and find pre-built apks, visit the main project readme: https://github.com/1runeberg/OpenXRProvider_v2