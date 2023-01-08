# OpenXR Provider Library v2
# Sample 09 - Hidden Worlds

Demonstrates eye tracking and custom graphics pipelines (using additional stencil passes in this demo and switchign between graphics pipelines).

Mini-game: Find all hidden worlds in play area using xray scanner (recommended at least 3m x 3m, 5xm x 5m ideal). 
Enter hidden world and it will be made fully visible. Once found, hidden worlds will remain visible, game can be reset by turning on xray mode when all found hidden worlds are visible.

There are also alternative input backups for lack of eye tracking to control the xray area/viewfinder - see table below.

https://user-images.githubusercontent.com/17371351/211217747-527375bd-2a08-4ff7-979a-bc6a063dca70.mp4

| **Action**                    | **Handtracking gesture**                                                                                                                                         | **Input system (controllers)**                                                                                                                                                                                                                                                                                          |
|-------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Xray Mode On                  | Clap (best with eye tracking) or Pinch (index and thumb tips together) if eye tracking isn't available.                                                          | Press either left or right primary button (most controllers - a/x) or menu button (vive, wmr)                                                                                                                                                                                                                           |
| Move Xray area                | Follows gaze if eye tracking is available. Use pinch gesture and move hand as needed otherwise                                                                    | Follows gaze if eye trackign is available. Press primary button on controller and move controller around to aim otherwise.                                                                                                                                                                                              |
| Adjust passthrough saturation | Ring finger tip and thumb tips together on BOTH hands,  spread apart to add saturation, inwards for less.  Saturation is commensurate to distance of thumb tips. | For android only - left/right thumbstick up or down will adjust saturation. As of the time of coding this,  Oculus Desktop's passthrough is unreliable and no OpenXR ext support for passthrough exists in SteamVR.  For PC, see *adjust gamma* and *adjust exposure*                                                   |

To build and find pre-built apks, visit the main project readme: https://github.com/1runeberg/OpenXRProvider_v2
