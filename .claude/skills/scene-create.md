# /scene-create

Create a new scene XML file with default entities.

## Usage
`/scene-create <name>`

## Steps
1. Create `demo/scenes/<name>.scene.xml` with template
2. Add default world settings (gravity, time_scale)
3. Add a player spawn point
4. Add a game_system entity with GameManager and HUD scripts
5. Print file path and next steps

## Template
```xml
<?xml version="1.0" encoding="UTF-8"?>
<scene name="<name>" version="1">
  <world>
    <time_scale>1.0</time_scale>
    <gravity>0 -9.81 0</gravity>
  </world>

  <entity id="player_1" archetype="player">
    <transform position="0 1 0"/>
    <stats health="100" max_health="100" ammo="120"/>
    <behavior shader="player_input.nadir"/>
    <script class="PlayerController"/>
  </entity>

  <entity id="game_system" archetype="system">
    <script class="GameManager"/>
    <script class="HUD"/>
  </entity>
</scene>
```
