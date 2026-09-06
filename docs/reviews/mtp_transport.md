# Direct USB/MTP transport checkpoint

## Status

```text
USB_MTP_DETECTION=PASS
MTP_STORAGE_ACCESS=PASS
MTP_ROOT_FOLDER_ACCESS=PASS
MTP_WRITE=PASS
MTP_LIST_FOLDER=PASS
MTP_READ=PASS
MTP_ROUNDTRIP=PASS
MTP_TRANSPORT_FOUNDATION=PASS
JSON_V1_MTP_TRANSFER=NEXT
TRAINLOG_FORMAT_V1=FROZEN
```

Trainlog now accesses Android file-transfer mode directly through Linux USB/MTP.
No GVFS mount, FUSE mount, file-manager mount, or manual mount/unmount cycle is
required.

## Detection

The discovery layer uses `libudev`.

Only physical USB devices with:

```text
DEVTYPE=usb_device
ID_MTP_DEVICE=1
```

are returned.

USB interface children such as `6-1:1.0` are deliberately ignored so one
physical phone is never reported several times.

The device record contains:

- USB bus number;
- USB device number;
- vendor/product IDs;
- vendor/model strings when available;
- serial number when available;
- sysfs path.

## MTP access

The transport layer uses `libmtp`.

The udev bus/device pair is matched against libmtp raw devices so Trainlog opens
the exact physical phone selected by discovery.

Implemented operations:

- enumerate MTP storage areas;
- expose capacity and free-space information;
- find or create a root folder;
- upload a local text file;
- list direct children of one MTP folder;
- download one MTP file to a local path.

Cached and uncached libmtp opening modes are kept separate because folder-tree
operations and direct `LIBMTP_Get_Files_And_Folders()` traversal have different
backend requirements.

## Physical validation

The checkpoint was validated against an Android Samsung device in USB
"file transfer" / MTP mode.

Observed storage:

```text
description = Stockage interne
storage_id  = 0x00010001
access      = read/write
```

Trainlog created:

```text
Stockage interne/
└── Trainlog/
    └── trainlog-probe.txt
```

The probe was then listed, downloaded back to the Linux host, and compared
byte-for-byte with the original local content.

Final result:

```text
ROUNDTRIP=PASS Trainlog/trainlog-probe.txt
```

## Boundary

This checkpoint validates the transport foundation only.

It does **not** yet claim that a real Trainlog JSON v1 session has been
transferred or imported.

Next:

```text
1. define the Trainlog MTP exchange directory layout
2. transfer a real valid JSON v1 document
3. verify byte-for-byte readback
4. feed the downloaded document to the frozen v1 validator/import path
5. build the minimal Android recorder on top of that contract
```
