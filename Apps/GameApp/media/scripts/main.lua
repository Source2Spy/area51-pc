-- main.lua
-- Entry point. Engine runs this file on startup.

-- ============================================================
--  Hook dispatcher
--  Must be declared BEFORE loading scripts.
--  Scripts call register_update / register_level_load /
--  register_level_unload instead of declaring global functions.
-- ============================================================

local _on_update       = {}
local _on_level_load   = {}
local _on_level_unload = {}

function register_update      ( fn ) _on_update      [#_on_update      +1] = fn end
function register_level_load  ( fn ) _on_level_load  [#_on_level_load  +1] = fn end
function register_level_unload( fn ) _on_level_unload[#_on_level_unload+1] = fn end

-- Engine calls these three functions:
function OnUpdate( dt )
    for _, fn in ipairs( _on_update ) do fn( dt ) end
end

function OnLevelLoad()
    for _, fn in ipairs( _on_level_load ) do fn() end
end

function OnLevelUnload()
    for _, fn in ipairs( _on_level_unload ) do fn() end
end

-- ============================================================
--  Script loading
--  Uncomment what you need (both can be active simultaneously).
-- ============================================================

-- dofile("scripts\\hazmat_spawner.lua")
-- dofile("scripts\\dynamic_light_spawner.lua")
-- dofile("scripts\\volumetric_spawner.lua")
-- dofile("scripts\\del_surf.lua")
