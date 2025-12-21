package dev.danielc.libpak;

import android.content.Context;

import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import org.junit.Test;
import org.junit.runner.RunWith;
import static org.junit.Assert.*;

@RunWith(AndroidJUnit4.class)
public class WiFiTest {
    @Test
    public void useAppContext() {
        Context ctx = InstrumentationRegistry.getInstrumentation().getTargetContext();
        assertEquals("dev.danielc.libpak", ctx.getPackageName());
        WiFi.isWiFiModuleCapableOfHandlingTwoConnections(ctx);
        WiFi.isHotSpotEnabled(ctx);

        WiFi.openHotSpotSettings(ctx);
    }
}