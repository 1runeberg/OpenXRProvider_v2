package net.beyondreality.hiddenworlds;

import android.annotation.TargetApi;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;

public class MainActivity extends android.app.NativeActivity
{
  private static final String PERMISSION_EYE_TRACKING_META = "com.oculus.permission.EYE_TRACKING";
  private static final int PERMISSION_EYE_TRACKING_META_CODE = 1;

  static
  {
    System.loadLibrary("openxr_loader");
    System.loadLibrary("openxr_provider");
    System.loadLibrary("sample_09_hidden_worlds");
  }

   private void requestEyeTracking()
  {
    if (checkSelfPermission(PERMISSION_EYE_TRACKING_META) != PackageManager.PERMISSION_GRANTED)
    {
     requestPermissions(
              new String[] {PERMISSION_EYE_TRACKING_META}, PERMISSION_EYE_TRACKING_META_CODE);
    }
  }

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);

    requestEyeTracking();
  }

}
