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
     * @brief Activity finish self and send result.
     * @param token: the Activity will be finish.
     * @param resultCode: Description of the result state.
     * @param resultData: Data of result.
     */
    boolean finishActivity(in IBinder token, int resultCode, in @nullable Intent resultData);

    /**
     * @param token: Identify of the Activity.
     * @param status: ON_CREATED/ON_STARTED/ON_RESUMED/ON_PAUSED/ON_STOPED/ON_DESTORYED
     */
    oneway void reportActivityStatus(in IBinder token, int status);

    /**
     * @param token: The Requestor
     * @param intent: Target data
     */
    int startService(in IBinder token, in Intent intent);
}