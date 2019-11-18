package com.mapzen.tangram;

import android.support.annotation.Keep;
import android.support.annotation.Nullable;

import java.util.Map;

/**
 * Callback to receive information about features picked from the map
 * Triggered after a call of {@link MapController#pickFeature(float, float)}
 * Listener should be set with {@link MapController#setFeaturePickListener(FeaturePickListener)}
 * The callback will be run on the main (UI) thread.
 */
@Keep
public interface FeaturePickListener {
    /**
     * Called when a feature pick query is complete, whether or not a feature was found.
     * @param result Info about the selected feature, or null if no feature was found.
     */
    void onFeaturePickComplete(@Nullable final FeaturePickResult result);
}
