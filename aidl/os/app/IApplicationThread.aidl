/*
 * Copyright (C) 2023 Xiaomi Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package os.app;

import os.app.Intent;
import os.app.IServiceConnection;

oneway interface IApplicationThread {
    void scheduleLaunchActivity(@utf8InCpp String activityName, in IBinder token, in Intent intent);
    void scheduleStartActivity(in IBinder token, in Intent intent);
    void scheduleResumeActivity(in IBinder token, in Intent intent);
    void schedulePauseActivity(in IBinder token);
    void scheduleStopActivity(in IBinder token);
    void scheduleDestroyActivity(in IBinder token);
    void onActivityResult(in IBinder token, int requestCode, int resultCode, in Intent resultData);

    void scheduleStartService(@utf8InCpp String serviceName, in IBinder token, in Intent intent);
    void scheduleStopService(in IBinder token);
    void scheduleBindService(@utf8InCpp String serviceName, in IBinder token, in Intent intent,
                             in IServiceConnection connection);
    void scheduleUnbindService(in IBinder token);
    void scheduleReceiveIntent(in IBinder token, in Intent intent);

    void setForegroundApplication(boolean isForeground);
    void terminateApplication();
}
