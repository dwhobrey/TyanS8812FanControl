/*++

Module Name:

    public.h

Abstract:

    This module contains the common declarations shared by driver
    and user applications.

Environment:

    user and kernel

--*/

//
// Define an Interface Guid so that app can find the device and talk to it.
//

DEFINE_GUID (GUID_DEVINTERFACE_W83795GDriver,
    0xd384e0a5,0x7390,0x497a,0x84,0xe0,0x54,0x3f,0x88,0x4b,0xea,0x57);
// {d384e0a5-7390-497a-84e0-543f884bea57}
