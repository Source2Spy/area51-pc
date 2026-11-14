-- volumetric_spawner.lua
--
-- F5              - spawn / replace one volume at the camera ray hit
-- F6              - select the next parameter (logged to the console)
-- Numpad - / +    - decrease / increase the selected parameter
-- F7 / F8         - toggle shadows / visibility
-- F9 / F10        - destroy the volume / print current settings
-- Arrow keys      - move on world X/Z; Numpad 1 / 3 moves down / up
-- Numpad 4 / 6    - yaw; Numpad 8 / 2 pitches the volume

local settings =
{
    texture       = "Volumetric_light_000.xbmp",
    intensity     = 0.015,
    start_size    = 100.0,
    end_size      = 300.0,
    length        = 800.0,
    edge_softness = 0.2,
    start_fade    = 0.02,
    end_fade      = 0.15,
    cast_shadows  = true,
    enabled       = true,
    start_color   = { 255, 240, 210, 255 },
    end_color     = { 255, 240, 210, 180 },
}

-- Adjustment rates are per second while holding a key.
local parameters =
{
    { "intensity",     "Intensity",    0.02, 0.0,  5.0 },
    { "start_size",    "StartSize",  150.0,  1.0, 1000.0 },
    { "end_size",      "EndSize",    150.0,  1.0, 1000.0 },
    { "length",        "Length",     400.0,  1.0, 3000.0 },
    { "edge_softness", "EdgeSoftness", 0.3,  0.0, 1.0 },
    { "start_fade",    "StartFade",    0.3,  0.0, 1.0 },
    { "end_fade",      "EndFade",      0.3,  0.0, 1.0 },
}

local volume_guid = 0
local selected = 1
local pitch = 0.0
local yaw = 0.0
local move_speed = 400.0
local rotation_speed = 60.0

local function exists()
    return volume_guid ~= 0 and obj_isalive(volume_guid)
end

local function destroy()
    if exists() then
        obj_destroy(volume_guid)
    end
    volume_guid = 0
end

local function print_settings()
    log(string.format(
        "[volumetric] selected=%s intensity=%.4f size=%.1f->%.1f length=%.1f edge=%.3f fade=%.3f/%.3f shadows=%s enabled=%s pitch=%.1f yaw=%.1f",
        parameters[selected][2], settings.intensity,
        settings.start_size, settings.end_size, settings.length,
        settings.edge_softness, settings.start_fade, settings.end_fade,
        tostring(settings.cast_shadows), tostring(settings.enabled), pitch, yaw))
end

local function apply_settings()
    for _, parameter in ipairs(parameters) do
        obj_prop_set(volume_guid, "VolumetricLight\\" .. parameter[2], settings[parameter[1]])
    end

    obj_prop_set(volume_guid, "VolumetricLight\\Texture", settings.texture)
    obj_prop_set(volume_guid, "VolumetricLight\\CastShadows", settings.cast_shadows)
    obj_prop_set(volume_guid, "VolumetricLight\\IsTurnedOn", settings.enabled)
    obj_prop_set(volume_guid, "VolumetricLight\\StartColor", table.unpack(settings.start_color))
    obj_prop_set(volume_guid, "VolumetricLight\\EndColor", table.unpack(settings.end_color))
end

local function spawn()
    local hx, hy, hz, nx, ny, nz = raycast_from_cam_pos(110000)
    if not hx then
        log("[volumetric] no surface hit; aim at a wall or ceiling")
        return
    end

    local created = obj_create("VolumetricLight")
    if created == 0 then
        log("[volumetric] obj_create failed")
        return
    end

    destroy()
    volume_guid = created
    pitch = math.deg(-math.atan(ny, math.sqrt(nx * nx + nz * nz)))
    yaw = math.deg(math.atan(nx, nz))

    local player = obj_get_player()
    if player ~= 0 and obj_isalive(player) then
        local zone1, zone2 = obj_getzone(player)
        obj_setzones(volume_guid, zone1, zone2)
    end

    obj_setpos(volume_guid, hx + nx * 10.0, hy + ny * 10.0, hz + nz * 10.0)
    obj_setrotation(volume_guid, pitch, yaw, 0.0)
    apply_settings()
    print_settings()
end

local function axis(negative, positive)
    return (input_ispressed(positive) and 1 or 0) - (input_ispressed(negative) and 1 or 0)
end

register_update(function(dt)
    if input_waspressed("INPUT_KBD_F5") then
        spawn()
    end
    if input_waspressed("INPUT_KBD_F6") then
        selected = selected % #parameters + 1
        print_settings()
    end
    if input_waspressed("INPUT_KBD_F9") then
        destroy()
    end

    local toggled = false
    if input_waspressed("INPUT_KBD_F7") then
        settings.cast_shadows = not settings.cast_shadows
        toggled = true
    end
    if input_waspressed("INPUT_KBD_F8") then
        settings.enabled = not settings.enabled
        toggled = true
    end
    if toggled then
        if exists() then
            obj_prop_set(volume_guid, "VolumetricLight\\CastShadows", settings.cast_shadows)
            obj_prop_set(volume_guid, "VolumetricLight\\IsTurnedOn", settings.enabled)
        end
        print_settings()
    end
    if input_waspressed("INPUT_KBD_F10") then
        print_settings()
    end

    if not exists() then
        return
    end

    local delta = axis("INPUT_KBD_SUBTRACT", "INPUT_KBD_ADD")
    if delta ~= 0 then
        local parameter = parameters[selected]
        local value = settings[parameter[1]] + delta * parameter[3] * dt
        settings[parameter[1]] = math.max(parameter[4], math.min(parameter[5], value))
        obj_prop_set(volume_guid, "VolumetricLight\\" .. parameter[2], settings[parameter[1]])
    end

    local dx = axis("INPUT_KBD_LEFT", "INPUT_KBD_RIGHT")
    local dy = axis("INPUT_KBD_NUMPAD1", "INPUT_KBD_NUMPAD3")
    local dz = axis("INPUT_KBD_DOWN", "INPUT_KBD_UP")
    if dx ~= 0 or dy ~= 0 or dz ~= 0 then
        obj_moverel(volume_guid, dx * move_speed * dt, dy * move_speed * dt, dz * move_speed * dt)
    end

    local dp = axis("INPUT_KBD_NUMPAD8", "INPUT_KBD_NUMPAD2")
    local dyaw = axis("INPUT_KBD_NUMPAD4", "INPUT_KBD_NUMPAD6")
    if dp ~= 0 or dyaw ~= 0 then
        pitch = math.max(-90.0, math.min(90.0, pitch + dp * rotation_speed * dt))
        yaw = (yaw + dyaw * rotation_speed * dt) % 360.0
        obj_setrotation(volume_guid, pitch, yaw, 0.0)
    end
end)

register_level_unload(destroy)
