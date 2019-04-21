# Android Turret Voice Controller

## Description

Control a toy turret using your voice. Commands are interpreted 
through captured speech and are sent over Bluetooth to a connected toy turret.

## Commands

### Payload commands

- __prime__: Prepare payload for firing
- __fire__: Fire payload (*Note: payload must be primed before this will succed*)


### Adjustment commands
Note that a __tick__ is the smallest unit of movement the turret can support. Currently a tick must be in the range of (0 to 10) inclusive.
- __up X ticks__: Adjust turret pitch upward
- __down X ticks__: Adjust turret pitch downward
- __left X ticks__: Adjust turret yaw left
- __right X ticks__: Adjust turret yaw right

##  Demo

![video](assets/img/voice_demo.gif)


