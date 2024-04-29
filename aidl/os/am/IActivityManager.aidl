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

package os.am;

import os.app.Intent;
import os.app.IApplicationThread;
import os.app.IServiceConnection;
import os.app.IBroadcastReceiver;

interface IActivityManager {
    /**
     * @param app: The application bind self to ams
     */
    int attachApplication(IApplicationThread app);

    /**
     * @param token: The caller who want to start Activity.
     * @param intent: Transferring data.
     * @param requestCode: Request code for target activity.
     */
    int startActivity(in IBinder token, in Intent intent, int requestCode);

    /**
     * @brief Stop Activity by Intent, Usually used for debugging.
     * @param intent: Target activity and data
     * @param requestCode: Request code for target activity.
     */
    int stopActivity(in Intent intent, int resultCode);

    /**
     * @brief Stop Application by Token. Token is Activity or Service.
     */
    int stopApplication(in IBinder token);

    /**
     * @brief Activity finish self and send result.
     * @param token: the Activity will be finish.
     * @param resultCode: Description of the result state.
     * @param resultData: Data of result.
     */
    boolean finishActivity(in IBinder token, int resultCode, in @nullable Intent resultData);

    /**
     * @brief Attempts to move a task backwards
     * @param token: the activity we wish to move
     * @param nonRoot: If false then this only works if the activity is the root
     *                of a task; if true it will work for any activity in a task
     */
    boolean moveActivityTaskToBackground(in IBinder token, boolean nonRoot);

    /**
     * @param token: Identify of the Activity.
     * @param status: ON_CREATED/ON_STARTED/ON_RESUMED/ON_PAUSED/ON_STOPED/ON_DESTORYED
     */
    void reportActivityStatus(in IBinder token, int status);

    /**
     * @param intent: Target service
     */
    int startService(in Intent intent);

    /**
     * @param intent: describe the services that need to be stopped
     */
    int stopService(in Intent intent);

    /**
     * @param token: service token, use to stop self
     */
    int stopServiceByToken(in IBinder token);

    /**
     * @param token: service token
     * @param status: onCreated/onStarted/onDestoryed
     */
    void reportServiceStatus(in IBinder token, int status);

    /**
     * @brief bindService, conn->onServiceConnected() will be called
     * when serivce onbinded success.
     */
    int bindService(IBinder caller, in Intent intent, in IServiceConnection conn);

    /**
     * @brief unbindService.
     */
    void unbindService(in IServiceConnection conn);

    /**
     * @brief publish the service that returned by onbinded().
     * @param token: Service key
     * @param serviceBinder: service IBinder object
     */
    void publishService(in IBinder token, in IBinder serviceBinder);

    /**
     * @brief postIntent, postIntent to intent.target.
     * @param intent
     */
    int postIntent(in Intent intent);

    /**
     * @brief sendBroadcast
     * @param intent, intent.action the broadcast title.
     */
    int sendBroadcast(in Intent intent);

    /**
     * @brief registerReceiver
     */
    int registerReceiver(@utf8InCpp String action, in IBroadcastReceiver receiver);

    /**
     * @brief unregisterReceiver
     */
    void unregisterReceiver(in IBroadcastReceiver receiver);
}