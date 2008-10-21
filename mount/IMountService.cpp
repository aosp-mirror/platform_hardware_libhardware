/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include <stdint.h>
#include <sys/types.h>

#include <utils/Parcel.h>

#include <hardware/IMountService.h>

namespace android {

enum {
    GET_MASS_STORAGE_ENABLED_TRANSACTION = IBinder::FIRST_CALL_TRANSACTION,
    SET_MASS_STORAGE_ENABLED_TRANSACTION,
    GET_MASS_STORAGE_CONNECTED_TRANSACTION,
    MOUNT_MEDIA_TRANSACTION,
    UNMOUNT_MEDIA_TRANSACTION
};    

class BpMountService : public BpInterface<IMountService>
{
public:
    BpMountService(const sp<IBinder>& impl)
        : BpInterface<IMountService>(impl)
    {
    }

    virtual bool getMassStorageEnabled()
    {
        uint32_t n;
        Parcel data, reply;
        data.writeInterfaceToken(IMountService::getInterfaceDescriptor());
        remote()->transact(GET_MASS_STORAGE_ENABLED_TRANSACTION, data, &reply);
        return reply.readInt32();
    }

    virtual void setMassStorageEnabled(bool enabled)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IMountService::getInterfaceDescriptor());
        data.writeInt32(enabled ? 1 : 0);
        remote()->transact(SET_MASS_STORAGE_ENABLED_TRANSACTION, data, &reply);
    }

    virtual bool getMassStorageConnected()
    {
        uint32_t n;
        Parcel data, reply;
        data.writeInterfaceToken(IMountService::getInterfaceDescriptor());
        remote()->transact(GET_MASS_STORAGE_CONNECTED_TRANSACTION, data, &reply);
        return reply.readInt32();
    }

    virtual void mountMedia(String16 mountPoint)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IMountService::getInterfaceDescriptor());
        data.writeString16(mountPoint);
        remote()->transact(MOUNT_MEDIA_TRANSACTION, data, &reply);
    }

    virtual void unmountMedia(String16 mountPoint)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IMountService::getInterfaceDescriptor());
        data.writeString16(mountPoint);
        remote()->transact(UNMOUNT_MEDIA_TRANSACTION, data, &reply);
    }
};

IMPLEMENT_META_INTERFACE(MountService, "android.os.IMountService");


};
