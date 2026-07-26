return {
    name        = "Example",
    description = "Example script module",
    category    = "Scripts",
    keybind     = key.f,  -- or "f", or 0x46, or key.right_shift

    onEnable = function(self)
        print("[Example] enabled")
    end,

    onDisable = function(self)
        print("[Example] disabled")
    end,

    onUpdate = function(self)
        -- called every render frame while enabled
    end,
}
