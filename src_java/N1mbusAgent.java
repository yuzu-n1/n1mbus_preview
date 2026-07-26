package n1mbus;

import io.netty.channel.Channel;
import io.netty.channel.ChannelDuplexHandler;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.ChannelPromise;

import java.lang.reflect.Field;

public class N1mbusAgent extends ChannelDuplexHandler {
    
    // --- Silent Rotation State ---
    public static volatile float targetYaw = 0.0f;
    public static volatile float targetPitch = 0.0f;
    public static volatile boolean silentActive = false;

    // --- Velocity State ---
    public static volatile boolean velocityEnabled = false;
    // Cached Class instances to bypass obfuscation
    private static Class<?> c03Class = null;
    private static Class<?> c05Class = null;
    private static Class<?> c06Class = null;
    private static Class<?> s12Class = null;
    private static Class<?> s27Class = null;
    public static volatile float velocityH = 0.0f;
    public static volatile float velocityV = 0.0f;

    private static boolean injected = false;
    private static boolean c03FieldsCached = false;
    private static final N1mbusAgent instance = new N1mbusAgent();

    // --- Cached Fields: C03PacketPlayer (Silent Rotation) ---
    private static Field c03YawField;
    private static Field c03PitchField;
    private static Field c03RotatingField;

    // --- Cached Fields: S12PacketEntityVelocity ---
    private static Field s12MotionXField;
    private static Field s12MotionYField;
    private static Field s12MotionZField;
    private static boolean s12FieldsCached = false;

    // --- Cached Fields: S27PacketExplosion ---
    private static Field s27MotionXField;
    private static Field s27MotionYField;
    private static Field s27MotionZField;
    private static boolean s27FieldsCached = false;

    // ================= Public API (called from C++ via JNI) =================

    public static void setSilentRotation(float yaw, float pitch) {
        targetYaw = yaw;
        targetPitch = pitch;
    }

    public static void setSilentActive(boolean enabled) {
        silentActive = enabled;
    }

    public static void setVelocityEnabled(boolean enabled) {
        velocityEnabled = enabled;
    }

    public static void setVelocityParams(float h, float v) {
        velocityH = h;
        velocityV = v;
    }

    public static void resetState() {
        targetYaw = 0.0f;
        targetPitch = 0.0f;
        silentActive = false;
        velocityEnabled = false;
        velocityH = 0.0f;
        velocityV = 0.0f;
    }

    public static boolean isInjected() {
        return injected;
    }

    // ================= Injection =================

    public static void injectFromMinecraft(Object minecraft) {
        if (minecraft == null || injected) return;
        try {
            // Traverse fields of Minecraft to find NetworkManager with a Channel
            for (Field field : minecraft.getClass().getDeclaredFields()) {
                field.setAccessible(true);
                Object value = field.get(minecraft);
                if (value == null) continue;
                inject(value);
                if (injected) return;
            }
        } catch (Exception e) {
            // Ignore silently
        }
    }

    public static void inject(Object networkManager) {
        if (injected || networkManager == null) return;
        try {
            Field channelField = null;
            for (Field f : networkManager.getClass().getDeclaredFields()) {
                if (f.getType().getName().equals("io.netty.channel.Channel")) {
                    f.setAccessible(true);
                    channelField = f;
                    break;
                }
            }
            if (channelField == null) return;
            
            Channel channel = (Channel) channelField.get(networkManager);
            if (channel == null) return;

            if (channel.pipeline().get("n1mbus") == null) {
                if (channel.pipeline().get("packet_handler") != null) {
                    channel.pipeline().addBefore("packet_handler", "n1mbus", instance);
                } else {
                    channel.pipeline().addLast("n1mbus", instance);
                }
                injected = true;
            }
        } catch (Exception e) {
            // Ignore silently
        }
    }

    // ================= Outbound: Silent Rotation =================

    @Override
    public void write(ChannelHandlerContext ctx, Object msg, ChannelPromise promise) throws Exception {
        if (silentActive && msg != null) {
            Class<?> clazz = msg.getClass();
            String className = clazz.getSimpleName();
            
            if (c03Class == null && (className.equals("C03PacketPlayer") || hasField(clazz, "field_149476_e", "yaw", "d", "e"))) { c03Class = clazz; }
            if (c05Class == null && (className.equals("C05PacketPlayerLook") || hasField(clazz, "field_149476_e", "yaw", "d", "e"))) { c05Class = clazz; }
            if (c06Class == null && (className.equals("C06PacketPlayerPosLook") || hasField(clazz, "field_149476_e", "yaw", "d", "e"))) { c06Class = clazz; }

            if (clazz == c03Class || clazz == c05Class || clazz == c06Class) {
                try {
                    // Cache fields once
                    if (!c03FieldsCached) {
                        c03YawField = findFieldInHierarchy(clazz, "field_149476_e", "yaw", "d", "e", "a");
                        c03PitchField = findFieldInHierarchy(clazz, "field_149473_f", "pitch", "e", "f", "b");
                        c03RotatingField = findFieldInHierarchy(clazz, "field_149481_i", "rotating", "h", "g", "i");
                        c03FieldsCached = true;
                    }
                    if (c03YawField != null && c03PitchField != null) {
                        c03YawField.setFloat(msg, targetYaw);
                        c03PitchField.setFloat(msg, targetPitch);
                        if (c03RotatingField != null) {
                            c03RotatingField.setBoolean(msg, true);
                        }
                    }
                } catch (Exception e) { }
            }
        }
        super.write(ctx, msg, promise);
    }

    // ================= Inbound: Velocity =================

    @Override
    public void channelRead(ChannelHandlerContext ctx, Object msg) throws Exception {
        if (velocityEnabled && msg != null) {
            Class<?> clazz = msg.getClass();
            String className = clazz.getSimpleName();

            if (s12Class == null && (className.equals("S12PacketEntityVelocity") || hasField(clazz, "field_149415_b", "motionX", "b"))) { s12Class = clazz; }
            if (s27Class == null && (className.equals("S27PacketExplosion") || hasField(clazz, "field_149152_f", "motionX", "f"))) { s27Class = clazz; }

            if (clazz == s12Class) {
                // Scale velocity by percentages
                try {
                    if (!s12FieldsCached) {
                        s12MotionXField = findFieldInHierarchy(clazz, "field_149415_b", "motionX", "b", "d");
                        s12MotionYField = findFieldInHierarchy(clazz, "field_149416_c", "motionY", "c", "e");
                        s12MotionZField = findFieldInHierarchy(clazz, "field_149414_d", "motionZ", "d", "f");
                        s12FieldsCached = true;
                    }
                    if (s12MotionXField != null) s12MotionXField.setInt(msg, (int)(s12MotionXField.getInt(msg) * velocityH));
                    if (s12MotionYField != null) s12MotionYField.setInt(msg, (int)(s12MotionYField.getInt(msg) * velocityV));
                    if (s12MotionZField != null) s12MotionZField.setInt(msg, (int)(s12MotionZField.getInt(msg) * velocityH));
                } catch (Exception e) { }
                super.channelRead(ctx, msg);
                return;

            } else if (clazz == s27Class) {
                try {
                    if (!s27FieldsCached) {
                        s27MotionXField = findFieldInHierarchy(clazz, "field_149152_f", "motionX", "f", "a");
                        s27MotionYField = findFieldInHierarchy(clazz, "field_149153_g", "motionY", "g", "b");
                        s27MotionZField = findFieldInHierarchy(clazz, "field_149159_h", "motionZ", "h", "c");
                        s27FieldsCached = true;
                    }
                    if (s27MotionXField != null) s27MotionXField.setFloat(msg, s27MotionXField.getFloat(msg) * velocityH);
                    if (s27MotionYField != null) s27MotionYField.setFloat(msg, s27MotionYField.getFloat(msg) * velocityV);
                    if (s27MotionZField != null) s27MotionZField.setFloat(msg, s27MotionZField.getFloat(msg) * velocityH);
                } catch (Exception e) { }
                super.channelRead(ctx, msg);
                return;
            }
        }
        super.channelRead(ctx, msg);
    }

    // ================= Reflection Utils =================

    private static boolean hasField(Class<?> clazz, String... names) {
        return findFieldInHierarchy(clazz, names) != null;
    }

    private static Field findFieldInHierarchy(Class<?> clazz, String... names) {
        Class<?> current = clazz;
        while (current != null && current != Object.class) {
            for (Field f : current.getDeclaredFields()) {
                for (String name : names) {
                    if (f.getName().equals(name)) {
                        f.setAccessible(true);
                        return f;
                    }
                }
            }
            current = current.getSuperclass();
        }
        return null;
    }
}
