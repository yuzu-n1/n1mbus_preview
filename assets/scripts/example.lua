return {
    name        = "Example",
    description = "Example script module",
    category    = "Scripts",
    keybind     = 0x51, -- F key

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
