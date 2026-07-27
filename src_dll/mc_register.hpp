// mc_register.hpp  –  Register all Minecraft JNI mappings in one place.
//
// Call MC_Register::all() once before MC::resolve().
// Adding a new module's mappings = just add registerField / registerMethod calls here.
//
// Format:
//   MC::registerField("key", {"canonical/class/Name", "notch"}, "Ltype;sig;", {"SRGname","deobfName",...})
//   MC::registerMethod("key", {"class",...}, "(args)ret", {"SRGname","deobfName"})

#pragma once
#include "mc.hpp"

namespace MC_Register {

    // Resolved class path stored in MC::g_classes for use as signatures
    // These are resolved dynamically and used to build signatures below.
    // They must be registered before fields that reference them.
    static const char* MC_CLS      = "net/minecraft/client/Minecraft";
    static const char* PLAYER_SP   = "net/minecraft/client/entity/EntityPlayerSP";
    static const char* PLAYER_CLS  = "net/minecraft/entity/player/EntityPlayer";
    static const char* WORLD_CLI   = "net/minecraft/client/multiplayer/WorldClient";
    static const char* WORLD_BASE  = "net/minecraft/world/World";
    static const char* SCREEN_CLS  = "net/minecraft/client/gui/GuiScreen";
    static const char* TIMER_CLS   = "net/minecraft/util/Timer";
    static const char* SETTINGS    = "net/minecraft/client/settings/GameSettings";
    static const char* RENDERER    = "net/minecraft/client/renderer/EntityRenderer";
    static const char* CAP_CLS     = "net/minecraft/entity/player/PlayerCapabilities";
    static const char* KB_CLS      = "net/minecraft/client/settings/KeyBinding";
    static const char* ENTITY      = "net/minecraft/entity/Entity";
    static const char* LIVING      = "net/minecraft/entity/EntityLivingBase";
    static const char* MOP_CLS     = "net/minecraft/util/MovingObjectPosition";
    static const char* INV_CLS     = "net/minecraft/entity/player/InventoryPlayer";
    static const char* NET_HANDLER = "net/minecraft/client/network/NetHandlerPlayClient";
    static const char* NET_PLAYER_INFO = "net/minecraft/client/network/NetworkPlayerInfo";

    inline void all() {

        // ════════════════════════════════════════════════════════════════════
        //  Minecraft fields
        // ════════════════════════════════════════════════════════════════════
        MC::registerField("Minecraft.thePlayer",
            {MC_CLS, "ave"},
            ("L" + std::string(PLAYER_SP) + ";").c_str(),
            {"field_71439_g", "thePlayer"});

        MC::registerField("Minecraft.theWorld",
            {MC_CLS, "ave"},
            ("L" + std::string(WORLD_CLI) + ";").c_str(),
            {"field_71441_e", "theWorld"});

        MC::registerField("Minecraft.currentScreen",
            {MC_CLS, "ave"},
            ("L" + std::string(SCREEN_CLS) + ";").c_str(),
            {"field_71462_r", "currentScreen"});

        MC::registerField("Minecraft.timer",
            {MC_CLS, "ave"},
            ("L" + std::string(TIMER_CLS) + ";").c_str(),
            {"field_71428_T", "timer"});

        MC::registerField("Minecraft.gameSettings",
            {MC_CLS, "ave"},
            ("L" + std::string(SETTINGS) + ";").c_str(),
            {"field_71474_y", "gameSettings"});

        MC::registerField("Minecraft.playerController",
            {MC_CLS, "ave"},
            "Lnet/minecraft/client/multiplayer/PlayerControllerMP;",
            {"field_71442_b", "playerController"});

        MC::registerField("Minecraft.entityRenderer",
            {MC_CLS, "ave"},
            ("L" + std::string(RENDERER) + ";").c_str(),
            {"field_71460_t", "entityRenderer"});

        MC::registerField("Minecraft.objectMouseOver",
            {MC_CLS, "ave"},
            ("L" + std::string(MOP_CLS) + ";").c_str(),
            {"field_71476_x", "objectMouseOver", "s"});

        MC::registerField("Minecraft.rightClickDelayTimer",
            {MC_CLS, "ave"}, "I",
            {"field_71467_ac", "rightClickDelayTimer", "ap"});

        // ════════════════════════════════════════════════════════════════════
        //  Minecraft methods
        // ════════════════════════════════════════════════════════════════════
        MC::registerMethod("Minecraft.getMinecraft",
            {MC_CLS, "ave"},
            ("()" + std::string("L") + MC_CLS + ";").c_str(),
            {"func_71410_x", "getMinecraft", "A", "B", "C"}, true);

        MC::registerMethod("Minecraft.clickMouse",
            {MC_CLS, "ave"}, "()V",
            {"func_147116_af", "clickMouse"});

        MC::registerMethod("Minecraft.rightClickMouse",
            {MC_CLS, "ave"}, "()V",
            {"func_147121_ag", "rightClickMouse"});

        // ════════════════════════════════════════════════════════════════════
        //  Timer
        // ════════════════════════════════════════════════════════════════════
        MC::registerField("Timer.renderPartialTicks",
            {TIMER_CLS, "bas"}, "F",
            {"field_74281_c", "renderPartialTicks"});

        // ════════════════════════════════════════════════════════════════════
        //  GameSettings
        // ════════════════════════════════════════════════════════════════════
        MC::registerField("GameSettings.mouseSensitivity",
            {SETTINGS, "avs"}, "F",
            {"field_74341_c", "mouseSensitivity"});

        MC::registerField("GameSettings.keyBindSneak",
            {SETTINGS, "avs"},
            ("L" + std::string(KB_CLS) + ";").c_str(),
            {"field_74311_E", "keyBindSneak"});

        MC::registerField("GameSettings.keyBindJump",
            {SETTINGS, "avs"},
            ("L" + std::string(KB_CLS) + ";").c_str(),
            {"field_74314_A", "keyBindJump"});

        // ════════════════════════════════════════════════════════════════════
        //  KeyBinding
        // ════════════════════════════════════════════════════════════════════
        MC::registerField("KeyBinding.pressed",
            {KB_CLS, "avt"}, "Z",
            {"field_74513_e", "pressed", "e"});

        // ════════════════════════════════════════════════════════════════════
        //  EntityPlayer (capabilities + inventory declared on superclass)
        // ════════════════════════════════════════════════════════════════════
        MC::registerField("EntityPlayer.capabilities",
            {PLAYER_CLS, "qn"},
            ("L" + std::string(CAP_CLS) + ";").c_str(),
            {"field_71075_bZ", "capabilities"});

        MC::registerField("EntityPlayer.inventory",
            {PLAYER_CLS, "qn"},
            ("L" + std::string(INV_CLS) + ";").c_str(),
            {"field_71071_by", "inventory"});

        // ════════════════════════════════════════════════════════════════════
        //  PlayerCapabilities
        // ════════════════════════════════════════════════════════════════════
        MC::registerField("PlayerCapabilities.isFlying",
            {CAP_CLS, "ob"}, "Z",
            {"field_75100_b", "isFlying", "b"});

        MC::registerField("PlayerCapabilities.allowFlying",
            {CAP_CLS, "ob"}, "Z",
            {"field_75101_c", "allowFlying", "c"});

        MC::registerField("PlayerCapabilities.flySpeed",
            {CAP_CLS, "ob"}, "F",
            {"field_75096_f", "flySpeed", "f"});

        // ════════════════════════════════════════════════════════════════════
        //  Entity
        // ════════════════════════════════════════════════════════════════════
        MC::registerField("Entity.onGround",
            {ENTITY, "pk"}, "Z",
            {"field_70122_E", "onGround"});

        MC::registerField("Entity.motionX",
            {ENTITY, "pk"}, "D",
            {"field_70159_w", "motionX"});

        MC::registerField("Entity.motionY",
            {ENTITY, "pk"}, "D",
            {"field_70181_x", "motionY"});

        MC::registerField("Entity.motionZ",
            {ENTITY, "pk"}, "D",
            {"field_70179_y", "motionZ"});

        MC::registerField("Entity.posX",
            {ENTITY, "pk"}, "D",
            {"field_70165_t", "posX"});

        MC::registerField("Entity.posY",
            {ENTITY, "pk"}, "D",
            {"field_70163_u", "posY"});

        MC::registerField("Entity.posZ",
            {ENTITY, "pk"}, "D",
            {"field_70161_v", "posZ"});

        MC::registerField("Entity.rotationYaw",
            {ENTITY, "pk"}, "F",
            {"field_70177_z", "rotationYaw"});

        MC::registerField("Entity.rotationPitch",
            {ENTITY, "pk"}, "F",
            {"field_70125_A", "rotationPitch"});

        MC::registerField("Entity.prevRotationYaw",
            {ENTITY, "pk"}, "F",
            {"field_70176_d", "prevRotationYaw"});

        MC::registerField("Entity.prevRotationPitch",
            {ENTITY, "pk"}, "F",
            {"field_70173_aa", "prevRotationPitch"});

        MC::registerField("Entity.width",
            {ENTITY, "pk"}, "F",
            {"field_70130_N", "width"});

        MC::registerField("Entity.height",
            {ENTITY, "pk"}, "F",
            {"field_70131_O", "height"});

        MC::registerMethod("Entity.setSprinting",
            {ENTITY, "pk"}, "(Z)V",
            {"func_70031_b", "setSprinting"});

        MC::registerMethod("Entity.getName",
            {ENTITY, "pk"}, "()Ljava/lang/String;",
            {"func_70005_c_", "getName"});

        // ════════════════════════════════════════════════════════════════════
        //  EntityLivingBase
        // ════════════════════════════════════════════════════════════════════
        MC::registerField("EntityLivingBase.hurtTime",
            {LIVING, "pr"}, "I",
            {"field_70737_aN", "hurtTime"});

        MC::registerMethod("EntityLivingBase.getHealth",
            {LIVING, "pr"}, "()F",
            {"func_110143_aJ", "getHealth"});

        MC::registerMethod("EntityLivingBase.getMaxHealth",
            {LIVING, "pr"}, "()F",
            {"func_110138_aP", "getMaxHealth"});

        MC::registerMethod("EntityLivingBase.canEntityBeSeen",
            {LIVING, "pr"},
            ("(L" + std::string(ENTITY) + ";)Z").c_str(),
            {"func_70685_l", "canEntityBeSeen"});

        MC::registerMethod("EntityLivingBase.swingItem",
            {LIVING, "pr"}, "()V",
            {"func_71038_i", "swingItem"});

        MC::registerMethod("PlayerControllerMP.attackEntity",
            {"net/minecraft/client/multiplayer/PlayerControllerMP", "bda"},
            ("(L" + std::string(PLAYER_CLS) + ";L" + std::string(ENTITY) + ";)V").c_str(),
            {"func_78764_a", "attackEntity"});

        MC::registerMethod("EntityPlayer.attackTargetEntityWithCurrentItem",
            {PLAYER_CLS, "wn"}, ("(L" + std::string(ENTITY) + ";)V").c_str(),
            {"func_71059_n", "attackTargetEntityWithCurrentItem"});

        MC::registerMethod("EntityLivingBase.jump",
            {LIVING, "pr"}, "()V",
            {"func_70664_aZ", "jump"});

        MC::registerMethod("EntityLivingBase.getHeldItem",
            {LIVING, "pr"}, "()Lnet/minecraft/item/ItemStack;",
            {"func_70694_bm", "getHeldItem"});

        MC::registerMethod("EntityPlayer.getTotalArmorValue",
            {PLAYER_CLS, "wn"}, "()I",
            {"func_82243_bO", "getTotalArmorValue"});

        MC::registerMethod("ItemStack.getDisplayName",
            {"net/minecraft/item/ItemStack", "ayz"},
            "()Ljava/lang/String;",
            {"func_145748_c_", "getDisplayName"});

        MC::registerMethod("EntityLivingBase.getActivePotionEffects",
            {LIVING, "pr"}, "()Ljava/util/Collection;",
            {"func_70651_bq", "getActivePotionEffects"});

        MC::registerMethod("Minecraft.getNetHandler",
            {"net/minecraft/client/Minecraft", "ave"}, ("()L" + std::string(NET_HANDLER) + ";").c_str(),
            {"func_147114_u", "getNetHandler", "u"});

        MC::registerMethod("NetHandlerPlayClient.getPlayerInfo",
            {"net/minecraft/client/network/NetHandlerPlayClient", "bcy"}, ("(Ljava/lang/String;)L" + std::string(NET_PLAYER_INFO) + ";").c_str(),
            {"func_175104_a", "getPlayerInfo", "a"});

        MC::registerMethod("NetworkPlayerInfo.getResponseTime",
            {"net/minecraft/client/network/NetworkPlayerInfo", "bdc"}, "()I",
            {"func_178853_c", "getResponseTime", "c"});

        // ════════════════════════════════════════════════════════════════════
        //  World
        // ════════════════════════════════════════════════════════════════════
        MC::registerField("World.loadedEntityList",
            {WORLD_BASE, "adm"},
            "Ljava/util/List;",
            {"field_72996_f", "loadedEntityList"});

        MC::registerMethod("World.isAirBlock",
            {WORLD_BASE, "adm"},
            "(Lnet/minecraft/util/BlockPos;)Z",
            {"func_175623_d", "isAirBlock"});

        // ════════════════════════════════════════════════════════════════════
        //  MovingObjectPosition
        // ════════════════════════════════════════════════════════════════════
        MC::registerField("MovingObjectPosition.entityHit",
            {MOP_CLS, "auh"},
            ("L" + std::string(ENTITY) + ";").c_str(),
            {"field_72308_g", "entityHit"});

        MC::registerField("MovingObjectPosition.typeOfHit",
            {MOP_CLS, "auh"},
            "Lnet/minecraft/util/MovingObjectPosition$MovingObjectType;",
            {"field_72313_a", "typeOfHit"});

        // ════════════════════════════════════════════════════════════════════
        //  InventoryPlayer
        // ════════════════════════════════════════════════════════════════════
        MC::registerField("InventoryPlayer.currentItem",
            {INV_CLS, "bft"}, "I",
            {"field_70461_c", "currentItem"});

        // ════════════════════════════════════════════════════════════════════
        //  Session & Auth
        // ════════════════════════════════════════════════════════════════════
        MC::registerMethod("Minecraft.getSession",
            {MC_CLS, "ave"},
            "()Lnet/minecraft/util/Session;",
            {"func_110432_I", "getSession"});

        MC::registerMethod("Session.getUsername",
            {"net/minecraft/util/Session", "avm"},
            "()Ljava/lang/String;",
            {"func_111285_a", "getUsername"});

        // ════════════════════════════════════════════════════════════════════
        //  Textures & Skins
        // ════════════════════════════════════════════════════════════════════
        MC::registerMethod("Minecraft.getTextureManager",
            {MC_CLS, "ave"},
            "()Lnet/minecraft/client/renderer/texture/TextureManager;",
            {"func_110434_K", "getTextureManager"});

        MC::registerMethod("TextureManager.getTexture",
            {"net/minecraft/client/renderer/texture/TextureManager", "bni"},
            "(Lnet/minecraft/util/ResourceLocation;)Lnet/minecraft/client/renderer/texture/ITextureObject;",
            {"func_110581_b", "getTexture"});

        MC::registerMethod("ITextureObject.getGlTextureId",
            {"net/minecraft/client/renderer/texture/ITextureObject", "bmt"},
            "()I",
            {"func_110552_b", "getGlTextureId"});

        MC::registerMethod("AbstractClientPlayer.getLocationSkin",
            {"net/minecraft/client/entity/AbstractClientPlayer", "bet"},
            "()Lnet/minecraft/util/ResourceLocation;",
            {"func_110306_p", "getLocationSkin"});

        // ════════════════════════════════════════════════════════════════════
        //  List
        // ════════════════════════════════════════════════════════════════════
        MC::registerMethod("List.size",
            {"java/util/List", "java/util/List"},
            "()I",
            {"size"});

        MC::registerMethod("List.get",
            {"java/util/List", "java/util/List"},
            "(I)Ljava/lang/Object;",
            {"get"});

        // ════════════════════════════════════════════════════════════════════
        //  ActiveRenderInfo
        // ════════════════════════════════════════════════════════════════════
        MC::registerField("ActiveRenderInfo.MODELVIEW",
            {"net/minecraft/client/renderer/ActiveRenderInfo", "auz"},
            "Ljava/nio/FloatBuffer;",
            {"field_178812_b", "MODELVIEW"});

        MC::registerField("ActiveRenderInfo.PROJECTION",
            {"net/minecraft/client/renderer/ActiveRenderInfo", "auz"},
            "Ljava/nio/FloatBuffer;",
            {"field_178813_c", "PROJECTION"});

        // ════════════════════════════════════════════════════════════════════
        //  Entity (Tracking & Mobs)
        // ════════════════════════════════════════════════════════════════════
        MC::registerField("Entity.lastTickPosX",
            {ENTITY, "pk"}, "D",
            {"field_70142_S", "lastTickPosX"});
        MC::registerField("Entity.lastTickPosY",
            {ENTITY, "pk"}, "D",
            {"field_70137_T", "lastTickPosY"});
        MC::registerField("Entity.lastTickPosZ",
            {ENTITY, "pk"}, "D",
            {"field_70136_U", "lastTickPosZ"});

        MC::registerField("Entity.renderYawOffset",
            {ENTITY, "pk"}, "F",
            {"field_70126_B", "renderYawOffset"});

    } // all()

    // Standalone resolution for Mobs/Animals without fields
    static const char* MOB_CLS    = "net/minecraft/entity/monster/EntityMob";
    static const char* ANIMAL_CLS = "net/minecraft/entity/passive/EntityAnimal";

} // namespace MC_Register
