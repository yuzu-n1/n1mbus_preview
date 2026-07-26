#pragma once

namespace Mappings {
    // 1.8.9 SRG Mappings (Forge / Feather / FML environment)
    // FMLDeobfTweaker remaps Notch -> SRG at runtime
    
    inline const char* Minecraft_Class = "net/minecraft/client/Minecraft";
    inline const char* Minecraft_getMinecraft_Name = "func_71410_x";
    inline const char* Minecraft_getMinecraft_Sig = "()Lnet/minecraft/client/Minecraft;";
    
    inline const char* Minecraft_thePlayer_Name = "field_71439_g";
    inline const char* Minecraft_thePlayer_Sig = "Lnet/minecraft/client/entity/EntityPlayerSP;";
    
    // Minecraft.currentScreen
    inline const char* Minecraft_currentScreen_Name = "field_71462_r";
    inline const char* Minecraft_currentScreen_Sig = "Lnet/minecraft/client/gui/GuiScreen;";
    
    // Minecraft.objectMouseOver
    inline const char* Minecraft_objectMouseOver_Name = "field_71476_x";
    inline const char* Minecraft_objectMouseOver_Sig = "Lnet/minecraft/util/MovingObjectPosition;";

    // MovingObjectPosition
    inline const char* MovingObjectPosition_Class = "net/minecraft/util/MovingObjectPosition";
    inline const char* MovingObjectPosition_typeOfHit_Name = "field_72313_a";
    inline const char* MovingObjectPosition_typeOfHit_Sig = "Lnet/minecraft/util/MovingObjectPosition$MovingObjectType;";
    inline const char* MovingObjectPosition_entityHit_Name = "field_72308_g";
    inline const char* MovingObjectPosition_entityHit_Sig = "Lnet/minecraft/entity/Entity;";
    inline const char* MovingObjectPosition_sideHit_Name = "field_178784_b";
    inline const char* MovingObjectPosition_sideHit_Sig = "Lnet/minecraft/util/EnumFacing;";

    // EnumFacing
    inline const char* EnumFacing_Class = "net/minecraft/util/EnumFacing";
    inline const char* EnumFacing_UP_Name = "UP";
    inline const char* EnumFacing_UP_Sig = "Lnet/minecraft/util/EnumFacing;";
    
    // MovingObjectType Enum
    inline const char* MovingObjectType_Class = "net/minecraft/util/MovingObjectPosition$MovingObjectType";
    inline const char* MovingObjectType_BLOCK_Name = "BLOCK";
    inline const char* MovingObjectType_BLOCK_Sig = "Lnet/minecraft/util/MovingObjectPosition$MovingObjectType;";

    // Entity.setSprinting(boolean)
    inline const char* Entity_setSprinting_Name = "func_70031_b";
    inline const char* Entity_setSprinting_Sig = "(Z)V";
    
    // EntityPlayer.capabilities
    inline const char* EntityPlayer_capabilities_Name = "field_71075_bZ";
    inline const char* EntityPlayer_capabilities_Sig = "Lnet/minecraft/entity/player/PlayerCapabilities;";
    
    // PlayerCapabilities
    inline const char* PlayerCapabilities_isFlying_Name = "field_75100_b";
    inline const char* PlayerCapabilities_isFlying_Sig = "Z";
    inline const char* PlayerCapabilities_allowFlying_Name = "field_75101_c";
    inline const char* PlayerCapabilities_allowFlying_Sig = "Z";
    inline const char* PlayerCapabilities_flySpeed_Name = "field_75096_f";
    inline const char* PlayerCapabilities_flySpeed_Sig = "F";
    
    // Entity motion
    inline const char* Entity_Class = "net/minecraft/entity/Entity";
    inline const char* Entity_motionX_Name = "field_70159_w";
    inline const char* Entity_motionX_Sig = "D";
    inline const char* Entity_motionY_Name = "field_70181_x";
    inline const char* Entity_motionY_Sig = "D";
    inline const char* Entity_motionZ_Name = "field_70179_y";
    inline const char* Entity_motionZ_Sig = "D";
    inline const char* Entity_onGround_Name = "field_70122_E";
    inline const char* Entity_onGround_Sig = "Z";
    inline const char* Entity_rotationYaw_Name = "field_70177_z";
    inline const char* Entity_rotationYaw_Sig = "F";
    inline const char* Entity_rotationPitch_Name = "field_70125_A";
    inline const char* Entity_rotationPitch_Sig = "F";

    // Entity positions
    inline const char* Entity_lastTickPosX_Name = "field_70169_q";
    inline const char* Entity_lastTickPosX_Sig = "D";
    inline const char* Entity_lastTickPosY_Name = "field_70167_r";
    inline const char* Entity_lastTickPosY_Sig = "D";
    inline const char* Entity_lastTickPosZ_Name = "field_70166_s";
    inline const char* Entity_lastTickPosZ_Sig = "D";
    inline const char* Entity_posX_Name = "field_70165_t";
    inline const char* Entity_posX_Sig = "D";
    inline const char* Entity_posY_Name = "field_70163_u";
    inline const char* Entity_posY_Sig = "D";
    inline const char* Entity_posZ_Name = "field_70161_v";
    inline const char* Entity_posZ_Sig = "D";
    inline const char* Entity_width_Name = "field_70130_N";
    inline const char* Entity_width_Sig = "F";
    inline const char* Entity_height_Name = "field_70131_O";
    inline const char* Entity_height_Sig = "F";
    inline const char* Entity_getName_Name = "func_70005_c_";
    inline const char* Entity_getName_Sig = "()Ljava/lang/String;";

    // Session
    inline const char* Minecraft_getSession_Name = "func_110432_I";
    inline const char* Minecraft_getSession_Sig = "()Lnet/minecraft/util/Session;";
    inline const char* Session_getUsername_Name = "func_111285_a";
    inline const char* Session_getUsername_Sig = "()Ljava/lang/String;";

    // Minecraft
    inline const char* Minecraft_getRenderManager_Name = "func_175599_af";
    inline const char* Minecraft_getRenderManager_Sig = "()Lnet/minecraft/client/renderer/entity/RenderManager;";
    inline const char* Minecraft_timer_Name = "field_71428_T";
    inline const char* Minecraft_timer_Sig = "Lnet/minecraft/util/Timer;";
    inline const char* Minecraft_theWorld_Name = "field_71441_e";
    inline const char* Minecraft_theWorld_Sig = "Lnet/minecraft/client/multiplayer/WorldClient;";
    inline const char* Minecraft_getTextureManager_Name = "func_110434_K";
    inline const char* Minecraft_getTextureManager_Sig = "()Lnet/minecraft/client/renderer/texture/TextureManager;";

    // TextureManager
    inline const char* TextureManager_getTexture_Name = "func_110581_b";
    inline const char* TextureManager_getTexture_Sig = "(Lnet/minecraft/util/ResourceLocation;)Lnet/minecraft/client/renderer/texture/ITextureObject;";

    // ITextureObject
    inline const char* ITextureObject_getGlTextureId_Name = "func_110552_b";
    inline const char* ITextureObject_getGlTextureId_Sig = "()I";
    // Timer
    inline const char* Timer_Class = "net/minecraft/util/Timer";
    inline const char* Timer_renderPartialTicks_Name = "field_74281_c";
    inline const char* Timer_renderPartialTicks_Sig = "F";

    // World
    inline const char* World_Class = "net/minecraft/world/World";
    inline const char* World_loadedEntityList_Name = "field_72996_f";
    inline const char* World_loadedEntityList_Sig = "Ljava/util/List;";
    inline const char* World_isAirBlock_Name = "func_175623_d";
    inline const char* World_isAirBlock_Sig = "(Lnet/minecraft/util/BlockPos;)Z";

    // List
    inline const char* List_Class = "java/util/List";
    inline const char* List_size_Name = "size";
    inline const char* List_size_Sig = "()I";
    inline const char* List_get_Name = "get";
    inline const char* List_get_Sig = "(I)Ljava/lang/Object;";

    // RenderManager
    inline const char* RenderManager_Class = "net/minecraft/client/renderer/entity/RenderManager";
    inline const char* RenderManager_viewerPosX_Name = "field_78730_l";
    inline const char* RenderManager_viewerPosX_Sig = "D";
    inline const char* RenderManager_viewerPosY_Name = "field_78731_m";
    inline const char* RenderManager_viewerPosY_Sig = "D";
    inline const char* RenderManager_viewerPosZ_Name = "field_78728_n";
    inline const char* RenderManager_viewerPosZ_Sig = "D";

    // ActiveRenderInfo
    inline const char* ActiveRenderInfo_Class = "net/minecraft/client/renderer/ActiveRenderInfo";
    inline const char* ActiveRenderInfo_MODELVIEW_Name = "field_178812_b";
    inline const char* ActiveRenderInfo_MODELVIEW_Sig = "Ljava/nio/FloatBuffer;";
    inline const char* ActiveRenderInfo_PROJECTION_Name = "field_178813_c";
    inline const char* ActiveRenderInfo_PROJECTION_Sig = "Ljava/nio/FloatBuffer;";
    // Entity Types
    inline const char* EntityPlayer_Class = "net/minecraft/entity/player/EntityPlayer";
    inline const char* AbstractClientPlayer_Class = "net/minecraft/client/entity/AbstractClientPlayer";
    inline const char* AbstractClientPlayer_getLocationSkin_Name = "func_110306_p";
    inline const char* AbstractClientPlayer_getLocationSkin_Sig = "()Lnet/minecraft/util/ResourceLocation;";
    inline const char* EntityMob_Class = "net/minecraft/entity/monster/EntityMob";
    inline const char* EntityAnimal_Class = "net/minecraft/entity/passive/EntityAnimal";
    inline const char* EntityLivingBase_Class = "net/minecraft/entity/EntityLivingBase";
    
    inline const char* EntityLivingBase_getHealth_Name = "func_110143_aJ";
    inline const char* EntityLivingBase_getHealth_Sig = "()F";
    inline const char* EntityLivingBase_getMaxHealth_Name = "func_110138_aP";
    inline const char* EntityLivingBase_getMaxHealth_Sig = "()F";

    // EntityLivingBase.canEntityBeSeen(Entity) -> bool (line-of-sight check)
    inline const char* EntityLivingBase_canEntityBeSeen_Name = "func_70685_l";
    inline const char* EntityLivingBase_canEntityBeSeen_Sig = "(Lnet/minecraft/entity/Entity;)Z";

    // Minecraft.gameSettings
    inline const char* Minecraft_gameSettings_Name = "field_71474_y";
    inline const char* Minecraft_gameSettings_Sig = "Lnet/minecraft/client/settings/GameSettings;";

    // GameSettings.keyBindSneak
    inline const char* GameSettings_keyBindSneak_Name = "field_74311_E";
    inline const char* GameSettings_keyBindSneak_Sig = "Lnet/minecraft/client/settings/KeyBinding;";

    // GameSettings.keyBindJump
    inline const char* GameSettings_keyBindJump_Name = "field_74314_A";
    inline const char* GameSettings_keyBindJump_Sig = "Lnet/minecraft/client/settings/KeyBinding;";

    // KeyBinding.pressed
    inline const char* KeyBinding_Class = "net/minecraft/client/settings/KeyBinding";
    inline const char* KeyBinding_pressed_Name = "field_74513_e";
    inline const char* KeyBinding_pressed_Sig = "Z";

    // EntityLivingBase.hurtTime
    inline const char* EntityLivingBase_hurtTime_Name = "field_70737_aN";
    inline const char* EntityLivingBase_hurtTime_Sig = "I";

    // Minecraft.clickMouse() / rightClickMouse()
    inline const char* Minecraft_clickMouse_Name = "func_147116_af";
    inline const char* Minecraft_clickMouse_Sig = "()V";
    inline const char* Minecraft_rightClickMouse_Name = "func_147121_ag";
    inline const char* Minecraft_rightClickMouse_Sig = "()V";
    inline const char* Minecraft_rightClickDelayTimer_Name = "field_71467_ac";
    inline const char* Minecraft_rightClickDelayTimer_Sig = "I";

    // EntityRenderer
    inline const char* Minecraft_entityRenderer_Name = "field_71460_t";
    inline const char* Minecraft_entityRenderer_Sig = "Lnet/minecraft/client/renderer/EntityRenderer;";
    inline const char* EntityRenderer_getMouseOver_Name = "func_78473_a";
    inline const char* EntityRenderer_getMouseOver_Sig = "(F)V";

    // EntityLivingBase.swingItem()
    inline const char* EntityLivingBase_swingItem_Name = "func_71038_i";
    inline const char* EntityLivingBase_swingItem_Sig = "()V";

    // GameSettings.mouseSensitivity
    inline const char* GameSettings_mouseSensitivity_Name = "field_74341_c";
    inline const char* GameSettings_mouseSensitivity_Sig = "F";

    // EntityLivingBase.jump()
    inline const char* EntityLivingBase_jump_Name = "func_70664_aZ";
    inline const char* EntityLivingBase_jump_Sig = "()V";

    // Entity.hurtResistantTime (counts down from 20 to 0 after damage)
    inline const char* Entity_hurtResistantTime_Name = "field_70172_ad";
    inline const char* Entity_hurtResistantTime_Sig = "I";

    // EntityPlayer.inventory
    inline const char* EntityPlayer_inventory_Name = "field_71071_by";
    inline const char* EntityPlayer_inventory_Sig = "Lnet/minecraft/entity/player/InventoryPlayer;";

    // InventoryPlayer
    inline const char* InventoryPlayer_Class = "net/minecraft/entity/player/InventoryPlayer";
    inline const char* InventoryPlayer_currentItem_Name = "field_70461_c";
    inline const char* InventoryPlayer_currentItem_Sig = "I";
    inline const char* InventoryPlayer_getStackInSlot_Name = "func_70301_a";
    inline const char* InventoryPlayer_getStackInSlot_Sig = "(I)Lnet/minecraft/item/ItemStack;";

    // ItemStack
    inline const char* ItemStack_Class = "net/minecraft/item/ItemStack";
    inline const char* ItemStack_getItem_Name = "func_77973_b";
    inline const char* ItemStack_getItem_Sig = "()Lnet/minecraft/item/Item;";

    // ItemBlock
    inline const char* ItemBlock_Class = "net/minecraft/item/ItemBlock";

    // BlockPos
    inline const char* BlockPos_Class = "net/minecraft/util/BlockPos";
    inline const char* BlockPos_Init_Sig = "(DDD)V";
}
